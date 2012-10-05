/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 
 *
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * httpact.c: Defines the actions which compose the NSAPI for the HTTP
 * servers
 *
 * Rob McCool
 */


#include "netsite.h"
#include "base/pblock.h"    /* various */
#include "base/session.h"   /* Session struct */
#include "base/util.h"
#include <base/plist.h>
#include "base/shexp.h"     /* shexp_casecmp */
#include "base/vs.h"
#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include <libaccess/aclcache.h>
#include <libaccess/genacl.h>
#include "frame/httpact.h"
#include "frame/http_ext.h"
#include "frame/httpdir.h"
#include "frame/httpfilter.h"
#include "frame/req.h"      /* Request structure, objset, object, etc */
#include "frame/log.h"      /* error logging */
#include "frame/func.h"     /* func_exec */
#include "frame/protocol.h" /* protocol_finish_request */
#include "frame/conf.h"     /* std_os global */
#include "frame/error.h"
#include <frame/acl.h>
#include "httpdaemon/httprequest.h"
#include "httpdaemon/statsmanager.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/HttpMethodRegistry.h"
#include "httpdaemon/vsconf.h"
#include "support/NSString.h"
#include "support/SimpleHash.h"

#ifdef PUMPKIN_HOUR
#include <signal.h>
#ifdef XP_WIN32
#include "nt/regparms.h"
#endif /* XP_WIN32 */
#endif

#include "frame/dbtframe.h"
#include "ares/arapi.h"

#define ACL_HTTP_RIGHT_PREFIX "http_"
#define ACL_HTTP_RIGHT_PREFIX_LEN (sizeof(ACL_HTTP_RIGHT_PREFIX) - 1)

#define PROCESS_URI_OBJECTS "process-uri-objects"

/*
 * objlist is a container for a bunch of objects. Unlike httpd_objset, the
 * contents of an objlist aren't sorted.
 */
struct objlist {
    int pos;
    httpd_object **obj;
};

static inline PRBool rights_always_allowed(Session *sn, Request *rq, const VirtualServer *vs, ACLListHandle_t *aclroot, char **rights);
static int _perform_pathchecks(Session *sn, Request *rq, httpd_object *obj, int doacl);
static PRBool ParamMatch(const char* expr, const char* str, PRBool noicmp);
static PRBool FindApplicableMethods(Session* sn, Request* rq, 
                                    NSString& methodStr);


/* ------------------------------------------------------------------------ */
/* ----------------------- Directive execution code ----------------------- */
/* ------------------------------------------------------------------------ */


/* ------------------------- _directive_applyone -------------------------- */

/* Apply the first applicable instance of a directive and return */

static inline int _directive_applyone(NSAPIPhase ph, dtable *d, Session *sn, Request *rq) 
{
    register int x;
    int rv;

    for(x = 0; x < d->ni; x++) {
        rv = object_execute(&d->inst[x], sn, rq);
        if (rv != REQ_NOACTION)
            return rv;
    }
    return REQ_NOACTION;
}


/* ------------------------- _directive_applyall -------------------------- */

static inline int _directive_applyall(NSAPIPhase ph, dtable *d, Session *sn, Request *rq)
{
    register int x;
    int rv;

    for(x = 0; x < d->ni; x++) {
        rv = object_execute(&d->inst[x], sn, rq);
        if (rv != REQ_NOACTION && rv != REQ_PROCEED)
            return rv;
    }
    return REQ_PROCEED;
}


/* ------------------------------------------------------------------------ */
/* ----------------------- AuthTrans and NameTrans ------------------------ */
/* ------------------------------------------------------------------------ */


/* ----------------------------- add_objects ------------------------------ */

static inline int add_objects(Session *sn, Request *rq, httpd_objset *os, char *name, char *ppath, objlist *objlist)
{
    /*
     * Named objects take precedence over ppath-based objects.
     *
     * For historical reasons, name values encountered while processing a
     * given object are handled in the opposite order from which they were
     * inserted. The order isn't usually an issue in practice because a) the
     * assign-name SAF won't insert a second name parameter if one is already
     * present and b) the pfx2dir SAF returns REQ_PROCEED, preventing a
     * subsequent NameTrans directive in that object from inserting a second
     * name parameter. ntrans-j2ee does things its own way, though, and will
     * happily insert a second name parameter.
     */
    while (name) {
        httpd_object *obj = objset_findbyname(name, rq->os, os);
        if (!obj) {
            log_error(LOG_MISCONFIG, PROCESS_URI_OBJECTS, sn, rq, 
                      XP_GetAdminStr(DBT_cannotFindTemplateS_), name);
            return REQ_ABORTED;
        }

        PR_ASSERT(objlist->pos < os->pos);
        objlist->obj[objlist->pos++] = obj;
        objset_add_object(obj, rq->os);

        pblock_removekey(pb_key_name, rq->vars);
        name = pblock_findkeyval(pb_key_name, rq->vars);
    }

    /* Look for objects with matching ppath parameters */
    if (ppath) {
        while (httpd_object *obj = objset_findbyppath(ppath, rq->os, os)) {
            PR_ASSERT(objlist->pos < os->pos);
            objlist->obj[objlist->pos++] = obj;
            objset_add_object(obj, rq->os);
        }
    }

    return REQ_PROCEED;
}


/* --------------------------- request_uri2path --------------------------- */

NSAPI_PUBLIC int servact_uri2path(Session *sn, Request *rq)
{
    httpd_objset *os = NULL;
    if (((NSAPIRequest*)rq)->hrq) {
        os = ((NSAPIRequest*)rq)->hrq->getObjset();
    } else if (const VirtualServer *vs = conf_get_vs()) {
        os = vs->getObjset();
    } else {
        return REQ_ABORTED;
    }

    return servact_objset_uri2path(sn, rq, os);
}

NSAPI_PUBLIC int servact_objset_uri2path(Session *sn, Request *rq, httpd_objset *vs_os)
{
    PRBool checked_server_name = PR_FALSE;
    pb_param *pp;
    int rv;

    NSAPIRequest *nrq = (NSAPIRequest *)rq;
    const HttpRequest *hrq = GetHrq(rq);
    const VirtualServer *vs;
    if (hrq) {
        // this was a real HTTP request
        vs = hrq->getVS();
        PR_ASSERT(vs);
    } else {
        // no HTTP request : we are in a VSInit
        vs = conf_get_vs();
        PR_ASSERT(vs);
    }

    /* Initialize two separate, empty lists of objects */
    objlist objectlists[2];
    objectlists[0].pos = 0;
    objectlists[0].obj = (httpd_object **) pool_malloc(sn->pool, sizeof(httpd_object *) * vs_os->pos);
    objectlists[1].pos = 0;
    objectlists[1].obj = (httpd_object **) pool_malloc(sn->pool, sizeof(httpd_object *) * vs_os->pos);

servact_objset_uri2path_restart:
    int nametrans_rv = REQ_NOACTION;

    /* Initially, look for default object. */
    const NSString& defname = vs->getDefaultObjectName();
    pp = pblock_kvinsert(pb_key_name, defname.data(), defname.length(), rq->vars);
    char *name = pp->value;

    /* Start with ppath set to the uri */
    char *ppath = pblock_findkeyval(pb_key_uri, rq->reqpb);
    pp = pblock_kvinsert(pb_key_ppath, ppath, strlen(ppath), rq->vars);
    ppath = pp->value;

    rq->os = objset_create_pool(sn->pool);

    /*
     * To prevent 3rd party SAFs from causing restart loops, we track the
     * number of times servact_uri2path has been called for this request.
     *
     * (Note that the SAFs we ship call request_restart to restart requests.
     * request_restart enforces the MAX_REQUEST_RESTARTS limit on its own.)
     */
    if (nrq->nuri2path > MAX_REQUEST_RESTARTS) {
        log_error(LOG_FAILURE, PROCESS_URI_OBJECTS, sn, rq,
                  XP_GetAdminStr(DBT_httpRestartUriXExceedsDepthY),
                  ppath, MAX_REQUEST_RESTARTS);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }

    nrq->nuri2path++;

    PRBool log_finest = ereport_can_log(LOG_FINEST);

    if (log_finest) {
        log_error(LOG_FINEST, PROCESS_URI_OBJECTS, sn, rq,
                  "processing objects for URI %s",
                  ppath);
    }

    /* Start with the default object and any objects that apply to the URI */
    objectlists[0].pos = 0;
    rv = add_objects(sn, rq, vs_os, name, ppath, &objectlists[0]);
    if (rv != REQ_PROCEED)
        return rv;

    for (int pass = 0; ; pass++) {
        /*
         * At any given time, we are processing directives from one list of
         * objects and simultaneously constructing a list of additional
         * objects that we should process on the next pass.
         */
        const objlist& current = objectlists[pass & 1];
        objlist& additional = objectlists[!(pass & 1)];
        additional.pos = 0;

        /* If we don't have any more objects to process... */
        if (!current.pos) {
            /* If nobody translated ppath to a local file system path... */
            if (nametrans_rv == REQ_NOACTION && *ppath == '/') {
                /* Use the <virtual-server> <document-root> */
                const NSString& root = vs->getNormalizedDocumentRoot();
                rv = request_set_path(sn, rq, root.data(), root.length(),
                                      ppath, strlen(ppath));
                if (rv != REQ_PROCEED)
                    return rv;
                nametrans_rv = REQ_PROCEED;

                /*
                 * Check if any additional objects apply as a result of
                 * prepending the document root
                 */
                name = NULL;
                ppath = pblock_findkeyval(pb_key_ppath, rq->vars);
                rv = add_objects(sn, rq, vs_os, name, ppath, &additional);
                if (rv != REQ_PROCEED)
                    return rv;
                continue;
            }

            /* We're done when there are no more objects to process */
            break;
        }

        /*
         * Process the objects in the order we encountered them. Note that
         * because objset_add_object() sorts objects, this order might not be
         * the same as the order in rq->os. In other words, the order in which
         * objects are processed during the Input, AuthTrans, and NameTrans
         * stages may vary from the order in which they are processed in
         * subsequent stages.
         */
        for (int x = 0; x < current.pos; x++) {
            httpd_object *obj = current.obj[x];

            if (log_finest) {
                char *obj_name = pblock_pblock2str(obj->name, NULL);
                log_error(LOG_FINEST, PROCESS_URI_OBJECTS, sn, rq,
                          "processing object %s",
                          obj_name);
            }

            /* Run Input stage for the new object */
            if (!session_input_done(sn) && obj->dt[NSAPIInput].ni)
                servact_input(sn, rq);

            /* Run AuthTrans stage for the new object */
            rv = _directive_applyone(NSAPIAuthTrans,
                                     &obj->dt[NSAPIAuthTrans],
                                     sn, rq);
            if (rv == REQ_RESTART)
                goto servact_objset_uri2path_restart;
            if (rv != REQ_NOACTION && rv != REQ_PROCEED)
                return rv;

            /*
             * Check the <virtual-server> <canonical-server-name>. Note that
             * we do this only after executing the default object's AuthTrans
             * directives. This gives the administrator an opportunity to
             * modify protocol, hostname, and port values during AuthTrans.
             */
            if (!checked_server_name) {
                checked_server_name = PR_TRUE;
                rv = http_canonical_redirect(sn, rq);
                if (rv != REQ_NOACTION)
                    return rv;
            }

            /* Run NameTrans stage for the new object */
            rv = _directive_applyone(NSAPINameTrans,
                                     &obj->dt[NSAPINameTrans],
                                     sn, rq);
            if (rv == REQ_RESTART)
                goto servact_objset_uri2path_restart;
            if (rv != REQ_NOACTION && rv != REQ_PROCEED)
                return rv;
            if (rv == REQ_PROCEED)
                nametrans_rv = REQ_PROCEED;

            /*
             * Check if any additional objects apply as a result of executing
             * the directives in this object
             */
            name = pblock_findkeyval(pb_key_name, rq->vars);
            ppath = pblock_findkeyval(pb_key_ppath, rq->vars);
            rv = add_objects(sn, rq, vs_os, name, ppath, &additional);
            if (rv != REQ_PROCEED)
                return rv;
        }
    }

    /* Have path, will travel. */
    if( (pp = pblock_removekey(pb_key_ppath, rq->vars)) ) {
        strcpy(pp->name, "path");
        pblock_kpinsert(pb_key_path, pp, rq->vars);
    }
    else {
        log_error(LOG_MISCONFIG, PROCESS_URI_OBJECTS, sn, rq, 
                  XP_GetAdminStr(DBT_noPartialPathAfterObjectProcessi_));
        return REQ_ABORTED;
    }

    return REQ_PROCEED;
}


/* ------------------------ servact_translate_uri ------------------------- */

static inline char* _servact_translate_uri(Session* sn, Request* rq)
{
    char *path;

    if(servact_uri2path(sn, rq) == REQ_ABORTED)
        path = NULL;
    else
        path = pool_strdup(pblock_pool(rq->vars), pblock_findkeyval(pb_key_path, rq->vars));

    return path;
}

NSAPI_PUBLIC char *servact_translate_uri(char *uri, Session *sn)
{
    Request *rq = request_restart_internal(uri, NULL);
    char* res = _servact_translate_uri(sn, rq);
    request_free(rq);
    return res;
}

NSAPI_PUBLIC char* INTservact_translate_uri2(char* uri, Session* sn, Request* rq)
{
    char* res = 0;
    Request *newrq = request_create_child(sn, rq, NULL, uri, NULL);
    if (newrq) {
        res = _servact_translate_uri(sn, newrq);
        request_free(newrq);
    }
    return res;
}

 
/* ------------------------ servact_require_right ------------------------- */

NSAPI_PUBLIC int servact_require_right(Request *rq, const char *right, int len)
{
    pb_param *pp = pblock_findkey(pb_key_required_rights, rq->vars);
    if (pp) {
        int old_len = strlen(pp->value);

        char *value = (char *) REALLOC(pp->value, old_len + 1 + len + 1);
        if (!value)
            return -1;

        pp->value = value;

        value[old_len] = ',';
        memcpy(value + old_len + 1, right, len);
        value[old_len + 1 + len] = '\0';
    } else {
        pp = pblock_kvinsert(pb_key_required_rights, right, len, rq->vars);
        if (!pp)
            return -1;
    }

    return 0;
}


/* --------------------- servact_get_required_rights ---------------------- */

static char **servact_get_required_rights(Request *rq)
{
    const char *custom = pblock_findkeyval(pb_key_required_rights, rq->vars);

    // Count the number of custom access rights
    int ncustom = 0;
    if (custom) {
        for (const char *p = custom; *p != '\0'; p++) {
            if (*p == ',')
                ncustom++;
        }
        ncustom++;
    }

    // Allocate an array that can accomodate both the HTTP method access right
    // and any custom access rights
    char **rights = (char **) MALLOC((1 + ncustom + 1) * sizeof(char *));
    if (!rights)
        return NULL;

    // Set the HTTP method access right at the front of the rights array
    const char *method = pblock_findkeyval(pb_key_method, rq->reqpb);
    if (!method)
        return NULL;
    int method_len = strlen(method);
    rights[0] = (char *) MALLOC(5 + method_len + 1);
    if (!rights[0])
        return NULL;
    memcpy(rights[0], ACL_HTTP_RIGHT_PREFIX, ACL_HTTP_RIGHT_PREFIX_LEN);
    memcpy(rights[0] + 5, method, method_len);
    rights[0][5 + method_len] = '\0';

    // Append any custom access rights to the rights array
    int i = 1;
    if (custom) {
        char *copy = STRDUP(custom);
        if (!copy)
            return NULL;
        char *next = NULL;
        char *token = util_strtok(copy, ",", &next);
        while (token) {
            rights[i++] = STRDUP(token);
            token = util_strtok(NULL, "," , &next);
        }
    }

    // Terminate the rights array with a NULL
    rights[i] = NULL;

    return rights;
}


/* ------------------------------------------------------------------------ */
/* ------------------------------ PathCheck ------------------------------- */
/* ------------------------------------------------------------------------ */


static inline PRBool build_acl_list(char *path, const char *luri, ACLListHandle **acllist, ACLListHandle *aclroot)
{
    PRBool rv = PR_TRUE;

    // Strip ;-delimited parameters from luri so /protected;sneaky/index.html
    // will match a /protected/index.html ACL.
    char* stripped = STRDUP(luri);
    util_uri_strip_params(stripped);

    // Each ACL_Get*Acls() call must succeed for us to return PR_TRUE
    rv &= ACL_GetPathAcls(stripped, acllist, "uri=", aclroot);
    rv &= ACL_GetPathAcls(path, acllist, "path=", aclroot);

    FREE(stripped);

    return rv;
}

static inline PRBool path_requires_list_right(Request *rq, const char *path)
{
    NSFCFileInfo *finfo = NULL;

    return (ISMGET(rq) &&
            INTrequest_info_path(path, rq, &finfo) == PR_SUCCESS &&
            finfo &&
            finfo->pr.type == PR_FILE_DIRECTORY);
}

static inline PRBool method_always_allowed(Session *sn, Request *rq, const VirtualServer *vs, ACLListHandle_t *aclroot, const char *method)
{
    char http_right[30];
    memcpy(http_right, ACL_HTTP_RIGHT_PREFIX, ACL_HTTP_RIGHT_PREFIX_LEN);
    util_strlcpy(http_right + ACL_HTTP_RIGHT_PREFIX_LEN, method,
                 sizeof(http_right) - ACL_HTTP_RIGHT_PREFIX_LEN);

    char *rights[2];
    rights[0] = http_right;
    rights[1] = NULL;

    return rights_always_allowed(sn, rq, vs, aclroot, rights);
}

static inline PRBool rights_always_allowed(Session *sn, Request *rq, const VirtualServer *vs, ACLListHandle_t *aclroot, char **rights)
{
    // Bail if an ACE can deny the right
    if (ACL_CanDeny(aclroot, rights, http_generic))
        return PR_FALSE;

    // Get the default object from obj.conf
    httpd_objset *objset = vs->getObjset();
    if (!objset)
        return PR_FALSE;
    httpd_object *obj = objset_findbyname(vs->defaultObjectName, 0, objset);
    if (!obj)
        return PR_FALSE;

    PR_ASSERT(rq->acllist == NULL);

    // Invoke the default object's check-acl directives to build a list of
    // ACLs that apply to all paths (i.e. add the default ACLs to rq->acllist)
    //
    // XXX like the ACL cache code, this code assumes PathCheck fn=check-acl
    // directives won't be wrapped by <Client> tags (i.e. it assumes that the
    // list of relevant ACLs varies only by path)
    pblock_kvinsert(pb_key_magnus_internal_default_acls_only, "1", 1, rq->vars);
    int res = _perform_pathchecks(sn, rq, obj, 1);
    param_free(pblock_removekey(pb_key_magnus_internal_default_acls_only, rq->vars));

    // Do the default ACLs always allow this right?
    PRBool always_allowed = PR_FALSE;
    if (res == REQ_PROCEED)
        always_allowed = ACL_AlwaysAllows(rq->acllist, rights, http_generic);

    // Discard the default ACL list
    if (rq->acllist != NULL) {
        ACL_ListDecrement(NULL, rq->acllist);
        rq->acllist = NULL;
    }

    return always_allowed;
}

NSAPI_PUBLIC int servact_pathchecks(Session *sn, Request *rq)
{
    httpd_objset *os = rq->os;
    register int x;
    httpd_object *obj;
    char *uri=0, *path=0;
    int not_found_in_acl_cache = 0;
    NSErr_t *errp = 0;
    const VirtualServer *vs = request_get_vs(rq);
    ACLListHandle_t *aclroot;
    ACLCache *aclcache;
    const char *vsid;

    PR_ASSERT(rq->acllist == NULL);

    if (vs == 0)
        return REQ_ABORTED;     // nothing we can do...
    
    vsid = vs_get_id(vs);
    aclroot = vs->getACLList();
    aclcache = vs->getConfiguration()->getACLCache();

    if (aclroot) {
        uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
        path = pblock_findkeyval(pb_key_path, rq->vars);

        ACLShortcut_t shortcut = ACL_SHORTCUT_UNKNOWN;

        if (path_requires_list_right(rq, path)) {
            // This is a request to list the contents of a directory
            servact_require_right(rq, ACL_GENERIC_RIGHT_LIST, ACL_GENERIC_RIGHT_LIST_LEN);
        } else if (!pblock_findkeyval(pb_key_required_rights, rq->vars)) {
            // This isn't a request to list the contents of a directory and
            // there are no custom required rights, so maybe we can skip ACL
            // checks for this HTTP method altogether
            shortcut = vs->getACLMethodShortcut(rq->method_num);
            if (shortcut == ACL_SHORTCUT_UNKNOWN) {
                char *method = pblock_findkeyval(pb_key_method, rq->reqpb);
                if (method_always_allowed(sn, rq, vs, aclroot, method)) {
                    ereport(LOG_VERBOSE, "%s requests for virtual server %s can safely bypass ACL checks", method, vsid);
                    shortcut = ACL_SHORTCUT_ALWAYS_ALLOW;
                } else {
                    ereport(LOG_VERBOSE, "%s requests for virtual server %s cannot bypass ACL checks", method, vsid);
                    shortcut = ACL_SHORTCUT_NONE;
                }
                vs->setACLMethodShortcut(rq->method_num, shortcut);
            }
        }

        if (shortcut == ACL_SHORTCUT_ALWAYS_ALLOW) {
            rq->acllist = ACL_LIST_NO_ACLS;
            aclroot = NULL;
        } else {
            if (aclcache == 0 ||
                !(ISMGET(rq) ? aclcache->CheckGet(vsid, uri, &rq->acllist) :
                  aclcache->Check(vsid, uri, &rq->acllist)))
            {
                not_found_in_acl_cache = 1;
            }
        }
    } else {
        rq->acllist = ACL_LIST_NO_ACLS;
    }

    // XXXrobm PathCheck functions need ACL information. However, the ACL 
    // functions don't necessarily execute before ones that need the info.
    // The "real" solution is to make a new directive between NameTrans and
    // PathCheck that does the ACLs, but we're too far along into 2.0 to
    // make such a change. So I've hard-coded it so that check-acl gets called
    // first. 

    //
    // acl (check-acl) pathchecks go first
    // if we cached the ACL list for this URI, we do not need to do them, though,
    // as all they do is modify rq->acllist, and we have that already
    //
    if (aclroot && not_found_in_acl_cache) {
        for(x = 0; x < os->pos; ++x) {
            obj = os->obj[x];
            int rv = _perform_pathchecks(sn, rq, os->obj[x], 1);
            if (rv != REQ_NOACTION && rv != REQ_PROCEED) {
                ACL_ListDecrement(errp, rq->acllist);
                rq->acllist = NULL;
                return rv;
            }
        }
    }

    //
    // now for the non-acl pathchecks
    //
    for(x = 0; x < os->pos; ++x) {
        obj = os->obj[x];
        int rv = _perform_pathchecks(sn, rq, os->obj[x], 0);
        if (rv != REQ_NOACTION && rv != REQ_PROCEED) {
            ACL_ListDecrement(errp, rq->acllist);
            rq->acllist = NULL;
            return rv;
        }
        // nsconfig stuff modifies os. Make sure we stay on the right object
        for(x = 0; x < os->pos; ++x)
            if(os->obj[x] == obj)
                break;
    }

    //
    // Finally, evaluate any ONE ACLs - must be done *after* all flavors of
    // pathchecks so that the pblock/"path" value is final.
    //
    if (aclroot) {
        /* After getting the PathCheck ACLs, look for any affiliated with any
         * component of the URI.  Finally look for an FS pathname-bound ACLs.
         */
        uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
        path = pblock_findkeyval(pb_key_path, rq->vars);

        if (not_found_in_acl_cache) {
            // we did not get rq->acllist from the ACL cache
            // so we need to build it now.
            //
            // now add any matching path and uri ACLs
            // as we're still operating on our private rq->acllist (it's not
            // coming from the ACL cache yet), we can modify it...
            if (!build_acl_list(path, uri, &rq->acllist, aclroot)) {
                // failed - clean up...
                ACL_ListDecrement(errp, rq->acllist);
                rq->acllist = NULL;
                return REQ_ABORTED;

            }

            // now enter the brand spanking new acllist into the ACL cache
            // (we pass a reference as we might get back a different,
            //  but equivalent list, and in any case, with an incremented refcount)
            if (aclcache) {
                // Bug 461689 don't cache CGI URIs that contain path info
                if (!pblock_findkeyval(pb_key_path_info, rq->vars) && !strchr(uri, ';')) {
                    if (ISMGET(rq))
                        aclcache->EnterGet(vsid, uri, &rq->acllist);
                    else
                        aclcache->Enter(vsid, uri, &rq->acllist);
                } else if (!rq->acllist) {
                    rq->acllist = ACL_LIST_NO_ACLS;
                }
            }
        }
    }

    // from this point on, the acllist is read-only.

    if (aclroot && rq->acllist != ACL_LIST_NO_ACLS) {
        /* Resource has ACLs, so accelerator cache cannot be used */
        rq->request_is_cacheable = 0;

        char **rights = servact_get_required_rights(rq);

        /* Evaluate ACLs */
        int rv = ACL_SetupEval(rq->acllist, sn, rq, rights, http_generic, NULL);

        ACL_ListDecrement(0, rq->acllist);      // we don't use rq->acllist anymore after this
        rq->acllist = NULL;                     // (might have ended up in the ACLCache, though -
                                                //  at which point the refcount would prevent it
                                                //  from being deleted)

        if ( rv != ACL_RES_ALLOW ) {
            if (!pblock_findkeyval(pb_key_status, rq->srvhdrs)) {
                /* Set the status only if it is not already set */
                protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
            }
            return REQ_ABORTED;
        }
    } else {
        ACL_ListDecrement(0, rq->acllist);
        rq->acllist = NULL;
    }
    return REQ_PROCEED;
}

/** This method returns ACLList for a request without a restart **/
NSAPI_PUBLIC int 
ACL_BuildAclList(char *path, const char *luri, ACLListHandle **acllist, ACLListHandle *aclroot)
{
    return build_acl_list(path, luri, acllist, aclroot);
}

/* 
   doacl defines if we should call check-acl only. 1 == do only it. 
   0 == do everything else 
 */
static int _perform_pathchecks(Session *sn, Request *rq, httpd_object *obj, int doacl)
{
    register int x, isacl;
    dtable *d = &obj->dt[NSAPIPathCheck];
    int rv;

    static FuncStruct *fs_check_acl = NULL;
    if (!fs_check_acl)
        fs_check_acl = func_find_str("check-acl");

    for(x = 0; x < d->ni; x++) {
#ifdef DEBUG
        /* check-acl should have been resolved to a FuncStruct * by now */
        const char *fn = pblock_findkeyval(pb_key_fn, d->inst[x].param.pb);
        PR_ASSERT(d->inst[x].f != NULL || fn == NULL || strcmp(fn, "check-acl"));
#endif
        isacl = (d->inst[x].f == fs_check_acl);
        if (doacl != isacl)
            continue;
        rv = object_execute(&d->inst[x], sn, rq);
        if (rv != REQ_NOACTION && rv != REQ_PROCEED) {
            return rv;
        }
    }

    return REQ_PROCEED;
}


/* ------------------------------------------------------------------------ */
/* ------------------------------ ObjectType ------------------------------ */
/* ------------------------------------------------------------------------ */


/* -------------------------- _perform_findinfo --------------------------- */

static inline int _perform_findinfo(Session *sn, Request *rq, httpd_object *obj) 
{
    return _directive_applyall(NSAPIObjectType,
                               &obj->dt[NSAPIObjectType], sn, rq);
}


/* --------------------------- request_fileinfo --------------------------- */

NSAPI_PUBLIC int servact_fileinfo(Session *sn, Request *rq) 
{
    httpd_objset *os = rq->os;
    register int x;

    for(x = os->pos - 1; x != -1; --x) {
        /* REQ_PROCEED: multiple instances supercede each other */
        int rv = _perform_findinfo(sn, rq, os->obj[x]);
        if (rv != REQ_NOACTION && rv != REQ_PROCEED)
            return rv;
    }

    return REQ_PROCEED;
}


/* ------------------------------------------------------------------------ */
/* ------------------------------- Service -------------------------------- */
/* ------------------------------------------------------------------------ */


/* ----------------------------- check_method ----------------------------- */

static inline int check_method(const char *mexp, Session *sn, Request *rq)
{
    PR_ASSERT(mexp != NULL);

    // Get method from the request
    const char *m = pblock_findkeyval(pb_key_method, rq->reqpb);
    if (!m) {
        // Invalid request
        return REQ_ABORTED;
    }

    // Compare method from request with method expression from directive
    switch(shexp_noicmp(m, mexp)) {
      case 0:
        // Match, call this SAF
        return REQ_PROCEED;

      case 1:
        // Mismatch, don't call this SAF
        return REQ_NOACTION;
    }

    log_error(LOG_MISCONFIG, XP_GetAdminStr(DBT_checkMethod_), sn, rq,
              XP_GetAdminStr(DBT_invalidShexpS_), mexp);

    // Misconfiguration, abort request
    return REQ_ABORTED;
}


/* ------------------------- check_service_method ------------------------- */

static inline int check_service_method(pblock *pb, Session *sn, Request *rq)
{
    // Get method expression from directive
    const char *mexp = pblock_findkeyval(pb_key_method, pb);
    if (!mexp) {
        // Directive doesn't have a method expression
        if (ISMOPTIONS(rq)) {
            // This is an OPTIONS request, and directive doesn't explicitly
            // indicate the SAF can handle OPTIONS; don't call this SAF
            return REQ_NOACTION;
        } else if (ISMTRACE(rq)) {
            // This is an TRACE request, and directive doesn't explicitly
            // indicate the SAF can handle TRACE; don't call this SAF
            return REQ_NOACTION;
        } else {
            // Call this SAF
            return REQ_PROCEED;
        }
    }

    return check_method(mexp, sn, rq);
}


/* --------------------------- check_io_method ---------------------------- */

static inline int check_io_method(pblock *pb, Session *sn, Request *rq)
{
    // Get method expression from directive
    const char *mexp = pblock_findkeyval(pb_key_method, pb);
    if (!mexp) {
        // Call this SAF
        return REQ_PROCEED;
    }

    return check_method(mexp, sn, rq);
}


/* ------------------------------ check_type ------------------------------ */

static inline int check_type(pblock *pb, Session *sn, Request *rq, pblock *hdrs)
{
    // Get type expression from directive
    const char *texp = pblock_findkeyval(pb_key_type, pb);
    if (!texp) {
        // Directive doesn't have a type expression, call this SAF
        return REQ_PROCEED;
    }

    // Get content-type from request/response headers
    const char *t = pblock_findkeyval(pb_key_content_type, hdrs);
    if (!t)
        t = "";

    // Compare content-type from response with type expression from directive
    switch(shexp_noicmp(t, texp)) {
      case 0:
        // Match, call this SAF
        return REQ_PROCEED;

      case 1:
        // Mismatch, don't call this SAF
        return REQ_NOACTION;
    }

    log_error(LOG_MISCONFIG, XP_GetAdminStr(DBT_checkType_), sn, rq,
              XP_GetAdminStr(DBT_invalidShexpS_), texp);

    // Misconfiguration, abort request
    return REQ_ABORTED;
}


/* ----------------------------- check_query ------------------------------ */

static inline int check_query(pblock *pb, Session *sn, Request *rq)
{
    // Get query expression from directive
    const char *qexp = pblock_findkeyval(pb_key_query, pb);
    if (!qexp) {
        // Directive doesn't have a query expression, call this SAF
        return REQ_PROCEED;
    }

    // Get query string from request
    const char *q = pblock_findkeyval(pb_key_query, rq->reqpb);
    if (!q) {
        // Request didn't contain a query string, don't call this SAF
        return REQ_NOACTION;
    }

    // Compare query string from request with query expression from directive
    switch(shexp_noicmp(q, qexp)) {
      case 0:
        // Match, call this SAF
        return REQ_PROCEED;

      case 1:
        // Mismatch, don't call this SAF
        return REQ_NOACTION;
    }

    log_error(LOG_MISCONFIG, XP_GetAdminStr(DBT_checkQuery_), sn, rq,
              XP_GetAdminStr(DBT_invalidShexpS_), qexp);

    // Misconfiguration, abort request
    return REQ_ABORTED;
}


/* ----------------------------- _CHECK_PARAM ----------------------------- */

#define _CHECK_PARAM(res)        \
    switch (res) {               \
      case REQ_NOACTION:         \
        continue; /* Skip SAF */ \
      case REQ_PROCEED:          \
        break; /* Call SAF */    \
      default:                   \
        return res;              \
    }


/* --------------------------- request_service ---------------------------- */

NSAPI_PUBLIC int servact_service(Session *sn, Request *rq)
{
    httpd_objset *os = rq->os;
    int x;

    // Track session statistics
    HttpRequest *hrq = ((NSAPIRequest *)rq)->hrq;
    if (hrq)
        hrq->GetDaemonSession().setMode(STATS_THREAD_RESPONSE);

    PRBool first_saf = PR_TRUE;

    // Get method from request
    const char *m = pblock_findkeyval(pb_key_method, rq->reqpb);
    if (!m) {
        // Invalid request
        return REQ_ABORTED;
    }


    for(x = os->pos - 1; x != -1; --x) {
        httpd_object* obj = os->obj[x];
        int y;
        int stage_rv = REQ_NOACTION;
        dtable *d;

        d = &obj->dt[NSAPIService];
        for(y = 0; y < d->ni; y++) {
            // _CHECK_PARAM falls through, continues, or returns as appropriate
            _CHECK_PARAM(check_type(d->inst[y].param.pb, sn, rq, rq->srvhdrs))
            _CHECK_PARAM(check_service_method(d->inst[y].param.pb, sn, rq))
            _CHECK_PARAM(check_query(d->inst[y].param.pb, sn, rq))
            _CHECK_PARAM(object_check(&d->inst[y], sn, rq))

            /* Success: execute function */

            // Interpolate parameters
            pblock *param;
            if (d->inst[y].param.model) {
                param = object_interpolate(d->inst[y].param.model, &d->inst[y], sn, rq);
                if (!param)
                    return REQ_ABORTED;
            } else {
                param = d->inst[y].param.pb;
            }

            // If we need to unchunk the request message body...
            if (first_saf && !INTERNAL_REQUEST(rq)  && pblock_findkey(pb_key_transfer_encoding, rq->headers) && !pblock_findkey(pb_key_content_length, rq->headers)) {
                int ibufsize = -1;
                int itimeout = -1;

                char *ibufsize_str = pblock_findkeyval(pb_key_ChunkedRequestBufferSize,
                                                       param);
                if (ibufsize_str)
                    ibufsize = atoi(ibufsize_str);

                char *itimeout_str = pblock_findkeyval(pb_key_ChunkedRequestTimeout,
                                                       param);
                if (itimeout_str)
                    itimeout = atoi(itimeout_str);

                if (http_unchunk_request(sn, rq, ibufsize, itimeout) == REQ_ABORTED) {
                    stage_rv = REQ_ABORTED;
                    break;
                }
            }

            first_saf = PR_FALSE;

            int saf_rv = func_exec_directive(&d->inst[y], param, sn, rq);
            if (saf_rv != REQ_NOACTION) {
                stage_rv = saf_rv;
                break;
            }
        }

        if (stage_rv != REQ_NOACTION) {
            // a SAF got to run
            return stage_rv;
        }
    }

    return REQ_NOACTION;
}


/* ------------------------------------------------------------------------ */
/* -------------------------------- AddLog -------------------------------- */
/* ------------------------------------------------------------------------ */


/* --------------------------- object_findlogs ---------------------------- */

static inline int _perform_logs(Session *sn, Request *rq, httpd_object *obj)
{
    return _directive_applyall(NSAPIAddLog, &obj->dt[NSAPIAddLog], sn, rq);
}


/* ------------------------------ _find_logs ------------------------------ */

NSAPI_PUBLIC int servact_addlogs(Session *sn, Request *rq)
{
    httpd_objset *os = rq->os;
    register int x;

    for(x = os->pos - 1; x != -1; --x) {
        int rv = _perform_logs(sn, rq, os->obj[x]);
        if (rv != REQ_NOACTION && rv != REQ_PROCEED)
            return rv;
    }
    return REQ_PROCEED;
}


/* ------------------------------------------------------------------------ */
/* -------------------------------- Error --------------------------------- */
/* ------------------------------------------------------------------------ */


/* Assumes that status is properly formatted. */
static inline int _find_error(Session *sn, Request *rq, httpd_object *obj)
{
    char *status = pblock_findkeyval(pb_key_status, rq->srvhdrs);
    register int y, r;
    register char *t;
    dtable *d;

    d = &obj->dt[NSAPIError];
    for(y = 0; y < d->ni; y++) {
        if( (t = pblock_findval("reason", d->inst[y].param.pb)) ) {
            if(strcasecmp(t, &status[4]))
                continue;
        }
        else if ( (t = pblock_findval("code", d->inst[y].param.pb)) )
            if(strncmp(t, status, 3))
                continue;
	/* One of those passed. Try to execute.
	 * <Client> restrictions are applied by object_execute(),
	 * which will return NO_ACTION on mismatch.
	 */
        r = object_execute(&d->inst[y], sn, rq);
	if (r != REQ_NOACTION)
	    return r;
    }
    return REQ_NOACTION;
}

NSAPI_PUBLIC int servact_finderror(Session *sn, Request *rq)
{
    register int x;

    PR_ASSERT(rq->os);

    for(x = rq->os->pos - 1; x != -1; --x) {
        int rv = _find_error(sn, rq, rq->os->obj[x]);
        if (rv != REQ_NOACTION)
            return rv;
    }
    return REQ_NOACTION;
}


/* ------------------------------------------------------------------------ */
/* -------------------------------- ROUTE --------------------------------- */
/* ------------------------------------------------------------------------ */


/* ---------------------------- _perform_route ---------------------------- */
/*
 * Performs proxy routing.
 *
 *
 */

int _perform_route(Session *sn, Request *rq, httpd_object *obj)
{
    return _directive_applyall(NSAPIRoute, &obj->dt[NSAPIRoute], sn, rq);
}


/* ---------------------------- servact_route ----------------------------- */

NSAPI_PUBLIC int servact_route(Session *sn, Request *rq)
{
    httpd_objset *os = rq->os;
    register int x;

    for(x = os->pos - 1; x != -1; --x) {
        /* REQ_PROCEED: multiple instances supercede each other */
        int rv = _perform_route(sn, rq, os->obj[x]);
        if (rv != REQ_NOACTION && rv != REQ_PROCEED)
            return rv;
    }
    return REQ_PROCEED;
}


/* ------------------------------------------------------------------------ */
/* --------------------------------- DNS ---------------------------------- */
/* ------------------------------------------------------------------------ */


/* ----------------------------- _perform_dns ----------------------------- */
/*
 * Performs custom DNS resolution.
 *
 *
 */

int _perform_dns(Session *sn, Request *rq, httpd_object *obj)
{
    register int y, r;
    dtable *d;

    d = &obj->dt[8];
    for(y = 0; y < d->ni; y++) {
        if ( (r = object_execute(&d->inst[y], sn, rq)) != REQ_NOACTION)
            return r;
    }
    return REQ_NOACTION;
}


/* ----------------------------- servact_dns ------------------------------ */


int servact_dns(Session *sn, Request *rq)
{
    httpd_objset *os = rq->os;
    register int x, r;

    for(x = os->pos - 1; x != -1; --x)
        if( (r = _perform_dns(sn, rq, os->obj[x])) != REQ_NOACTION)
            return r;

    return REQ_NOACTION;
}


PRHostEnt *optimized_gethostbyname(const char *host, Session *sn, Request *rq)
{
    if (!host || !*host)
        return NULL;

    PRNetAddr addr; 
    int cnt = 0;
    int dots = 1;
    int len = 0;
    char*tmp;

    log_error(LOG_VERBOSE, NULL, sn, rq, "attempting to resolve %s", host);

    // XXX
    PRHostEnt* he = (PRHostEnt*) pool_malloc(sn->pool, sizeof(PRHostEnt));
    if (!he)
        return NULL;

    // XXX
    char* hostbuf = (char*) pool_malloc(sn->pool, PR_NETDB_BUF_SIZE);
    if (!hostbuf)
        return NULL;

    if (!rq->vars || !(tmp = pblock_findval("local-domain-levels", rq->vars)) ||
        (dots = atoi(tmp)) < 0 || PR_StringToNetAddr(host, &addr) == PR_SUCCESS) { /* off by default in Proxy 3.51 */
        if (PR_GetHostByName(host, hostbuf, PR_NETDB_BUF_SIZE, he) != PR_SUCCESS)
            return NULL;
        return he;
    }

    const char* p;
    for(p=host; *p; p++) {
        if (*p == '.')
            cnt++;
    }

    len = (int)(p - host);
    if (cnt > dots && host[len-1] != '.') {
        char *dothost = (char *)pool_malloc(sn->pool, len + 2);
        strcpy(dothost, host);
        dothost[len] = '.';
        dothost[len+1] = '\0';
        if (PR_GetHostByName(dothost, hostbuf, PR_NETDB_BUF_SIZE, he) != PR_SUCCESS)
        {
            pool_free(sn->pool, dothost);
            return NULL;
        }
        pool_free(sn->pool, dothost);
    } else {
        if (PR_GetHostByName(host, hostbuf, PR_NETDB_BUF_SIZE, he) != PR_SUCCESS)
            return NULL;
    }

    return he;
}


PRHostEnt *servact_gethostbyname(const char *host, Session *sn, Request *rq)
{
#ifdef XXX_MCC_PROXY
#ifdef PROXY_DNS_CACHE
    DNSCacheKey key;
#endif
    TSET((THROTTLE_ACTION_DNS));
#endif
    PRHostEnt* rv = NULL;

    if (host) {
#ifdef MCC_PROXY
        if ((rv = host_dns_cache_lookup(host, sn, rq)))
            return rv;
#endif

        if (sn && rq) {
#ifdef XXX_MCC_PROXY
            char *pushed = rq->host;

            rq->host = host;
            rq->hp = NULL;
#endif

            switch (servact_dns(sn, rq)) {
              case REQ_PROCEED:
#ifdef XXX_MCC_PROXY
                rv = rq->hp;
#endif
                break;

              case REQ_NOACTION:
                rv = optimized_gethostbyname(host, sn, rq);
                break;

              default:
                rv = NULL;
                break;
            }
#ifdef XXX_MCC_PROXY
            rq->host = pushed;
#endif
        } else {
            rv = optimized_gethostbyname(host, sn, rq);
        }

#ifdef XXX_MCC_PROXY
#ifdef PROXY_DNS_CACHE
        dns_cache_insert(&key, rv);
#endif
#endif
    }

    return rv;
}


/* ------------------------------------------------------------------------ */
/* ------------------------------- Connect -------------------------------- */
/* ------------------------------------------------------------------------ */


/* --------------------------- _perform_connect --------------------------- */
/*
 * Returns the connected socket descriptor.
 *
 * RELIES HEAVILY ON THE RIGHT VALUES SET FOR REQ_* DEFINES:
 *
 * REQ_PROCEED ==  0 which may also be a valid returned sd (in practice
 * zero is always already in use).  The success can't be
 * tested against REQ_PROCEED, but with condition > -1.
 *
 * REQ_ABORTED == -1 which is the same as when the connection fails, and
 * the only legal negative value for a connect function
 * to return.
 *
 * REQ_NOACTION == -2 when no function was set, and the default should be
 * used.
 *
 * Other negative values will be treated as -1 return value
 * (failure to connect).
 *
 */

int _perform_connect(Session *sn, Request *rq, httpd_object *obj)
{
    register int y, r;
    dtable *d;

    d = &obj->dt[9];
    for(y = 0; y < d->ni; y++) {
        if ( (r = object_execute(&d->inst[y], sn, rq)) != REQ_NOACTION)
            return r;
    }
    return REQ_NOACTION;
}


/* --------------------------- servact_connect ---------------------------- */


int servact_connect(const char *host, int port, Session *sn, Request *rq)
{

    if (host && sn && rq) {
        httpd_objset *os = rq->os;
        register int x, r;

        pblock_nvinsert("connect-host",host,rq->vars);
        pblock_nninsert("connect-port",port,rq->vars);

        for(x = os->pos - 1; x != -1; --x) {
            if( (r = _perform_connect(sn, rq, os->obj[x])) != REQ_NOACTION) {
                param_free(pblock_remove("connect-host",rq->vars));
                param_free(pblock_remove("connect-port",rq->vars));
                return (r > -1) ? r : -1;/* return sd, or SYS_ERROR_FD */
            }
        }

        param_free(pblock_remove("connect-host",rq->vars));
        param_free(pblock_remove("connect-port",rq->vars));
    }

    return REQ_NOACTION;
}


/* ------------------------------------------------------------------------ */
/* -------------------------------- Filter -------------------------------- */
/* ------------------------------------------------------------------------ */


/* --------------------------- servact_filter ----------------------------- */

int _perform_filter(Session *sn, Request *rq, httpd_object *obj)
{
    register int y, r;
    char *t, *u;
    dtable *d;

    d = &obj->dt[10];
    for(y = 0; y < d->ni; y++) {
        /* currently always execute function */
        r = object_execute(&d->inst[y], sn, rq);
        if ( r != REQ_NOACTION && r != REQ_PROCEED )
            return r;
    }
    return REQ_NOACTION;
}


int servact_filter(Session *sn, Request *rq)
{
    if (sn && rq) {
        httpd_objset *os = rq->os;
        register int x, r;

        for(x = os->pos - 1; x != -1; --x) {
            r = _perform_filter(sn, rq, os->obj[x]);
            if( r != REQ_NOACTION && r != REQ_PROCEED )
                return r;
        }
    }

    return REQ_PROCEED;
}


NSAPI_PUBLIC int servact_nowaytohandle(Session *sn, Request *rq)
{
    const char *method = pblock_findkeyval(pb_key_method, rq->reqpb);

    HttpMethodRegistry& registry = HttpMethodRegistry::GetRegistry();
    if (registry.IsKnownHttpMethod(method) == PR_FALSE) {
        // Unknown method
        http_status(sn, rq, PROTOCOL_NOT_IMPLEMENTED, NULL);
        log_error(LOG_MISCONFIG, XP_GetAdminStr(DBT_handleProcessed_), sn, rq, 
                  XP_GetAdminStr(DBT_unknownMethod),
                  pblock_findkeyval(pb_key_uri, rq->reqpb)); 
        return REQ_ABORTED;
    }

    NSString methods;
    pb_param *pp = pblock_remove("allow", rq->srvhdrs);
    if (pp) {
        methods.append(pp->value);
        param_free(pp);
    }
    if (FindApplicableMethods(sn, rq, methods) == PR_TRUE) {
        pblock_nvinsert("allow", methods, rq->srvhdrs);
    }

    if (ISMOPTIONS(rq)) {
        // OPTIONS response
        param_free(pblock_removekey(pb_key_content_type, rq->srvhdrs));
        pblock_nvinsert("content-length", "0", rq->srvhdrs);
        http_status(sn, rq, PROTOCOL_OK, NULL);
        http_start_response(sn, rq);
        return REQ_PROCEED;
    }

    http_status(sn, rq, PROTOCOL_METHOD_NOT_ALLOWED, NULL);
    log_error(LOG_MISCONFIG, XP_GetAdminStr(DBT_handleProcessed_), sn, rq, 
              XP_GetAdminStr(DBT_methodNotAllowed),
              pblock_findkeyval(pb_key_uri, rq->reqpb)); 
    return REQ_ABORTED;
}


/* ----------------------- servact_handle_processed ----------------------- */

NSAPI_PUBLIC int servact_handle_processed(Session *sn, Request *rq)
{
    int rv;

servact_handle_processed_restart:
#ifdef MCC_PROXY
    /* Awful hack for the proxy to make it work with netlib */
    glob_rq = rq;
    glob_sn = sn;
#endif
    do {
        /* Figure out what objects it uses */
        rv = servact_uri2path(sn, rq);
        if (rv != REQ_PROCEED)
            continue;

        /* Process PathChecks */
        rv = servact_pathchecks(sn, rq);
        if (rv != REQ_PROCEED)
            continue;

        /* Find that file's typing information */
        rv = servact_fileinfo(sn, rq);
        if (rv != REQ_PROCEED)
            continue;

#ifdef MCC_PROXY
        /* Find the Filters to pipe the data through */
        rv = servact_filter(sn, rq);
        if (rv != REQ_PROCEED)
            continue;
#endif

        /* Service object (actually send it) */
        rv = servact_service(sn, rq);
        if (rv != REQ_PROCEED)
            continue;

    } while (rv == REQ_RESTART);

    if (INTERNAL_REQUEST(rq)) {
        /* We don't generate error pages or log accesses for internal requests */
        return rv;
    }

    /* If nobody could handle the method, list the methods we do support */
    if (rv == REQ_NOACTION)
        rv = servact_nowaytohandle(sn, rq);

    PR_ASSERT(rv == REQ_PROCEED || rv == REQ_ABORTED || rv == REQ_EXIT);

    /* Wrap up the request/response if someone serviced it */
    if (rv == REQ_PROCEED)
        rv = httpfilter_finish_request(sn, rq);

    /* If there was an error handling the request, return an error message */
    if (rv == REQ_ABORTED) {
        /* Try to clear any filter stack error or partial response */
        switch (httpfilter_reset_response(sn, rq)) {
        case REQ_NOACTION:
            /* Response was already committed, so there's nothing we can do */
            rv = REQ_ABORTED;
            break;

        case REQ_PROCEED:
            /* We're clear to send our error response */
            rv = error_report(sn, rq);
            break;

        case REQ_ABORTED:
            /* Error running Input/Output; send 500 Server Error */
            /* Should do this only when we're not blocking a mime type. */
            if (!pblock_findval("block-this-mime-type", rq->vars))
                protocol_status(sn, rq, 500, NULL);
            rv = error_report(sn, rq);
            break;
        }

        if (rv == REQ_RESTART)
            goto servact_handle_processed_restart;

        httpfilter_finish_request(sn, rq);
    }

    /* If things went horribly wrong, make sure the connection dies */
    if (rv == REQ_EXIT)
        KEEP_ALIVE(rq) = 0; 

    /* Log the request */
    servact_addlogs(sn, rq); /* return doesn't matter */

    return rv;
}


/* ----------------------- servact_error_processed ------------------------ */

NSAPI_PUBLIC void servact_error_processed(Session *sn, Request *rq, int status, const char *reason, const char *url)
{
    int rv;

    /* SAFs will expect fully-populated NSAPI structures */
    PR_ASSERT(sn != NULL);
    PR_ASSERT(rq != NULL);
    PR_ASSERT(pblock_findkey(pb_key_method, rq->reqpb));
    PR_ASSERT(pblock_findkey(pb_key_uri, rq->reqpb));
    PR_ASSERT(pblock_findkey(pb_key_protocol, rq->reqpb));

    /* We should only be called for HTTP protocol errors */
    PR_ASSERT(!session_get_httpfilter_context(sn));
    PR_ASSERT((status >= 400 && status <= 599) || (url && status == PROTOCOL_REDIRECT));

    /* Figure out which objects to use */
    do {
        rv = servact_uri2path(sn, rq);
    } while (rv == REQ_RESTART);

    PR_ASSERT(rv == REQ_PROCEED || rv == REQ_NOACTION || rv == REQ_ABORTED || rv == REQ_EXIT);

    /* Send error response */
    if (rv == REQ_PROCEED || rv == REQ_NOACTION || rv == REQ_ABORTED) {
        protocol_status(sn, rq, status, reason);

        if (url && !pblock_find("url", rq->vars)) {
            PR_ASSERT(status == PROTOCOL_REDIRECT);
            pblock_nvinsert("url", url, rq->vars);
        }

        rv = error_report(sn, rq);
    }

    PR_ASSERT(rv == REQ_PROCEED || rv == REQ_EXIT);

    /* If things went horribly wrong, make sure the connection dies */
    if (rv == REQ_EXIT)
        KEEP_ALIVE(rq) = 0;

    /* Log the request */
    servact_addlogs(sn, rq);
}


PRBool
FindApplicableMethods(Session* sn, Request* rq, NSString& methodStr)
{
    PRBool res = PR_FALSE;

    httpd_objset *os = rq->os;
    HttpMethodRegistry& registry = HttpMethodRegistry::GetRegistry();
    PRInt32 numMethods = registry.GetNumMethods();
    PRBool* methodVector = (int*)pool_malloc(sn->pool, numMethods*sizeof(PRBool));
    if (!methodVector) {
        return res;
    }

    for (int i=0; i < numMethods; i++) {
        methodVector[i] = PR_FALSE;
    }

    // Check for methods already in methodStr
    char *methods = STRDUP(methodStr);
    char *lasts = NULL;
    char *method = util_strtok(methods, ", \t", &lasts);
    while (method) {
        int key = registry.GetMethodIndex(method);
        if (key != -1)
            methodVector[key] = PR_TRUE;
        method = util_strtok(NULL, ", \t", &lasts);
    }
    FREE(methods);
    methodStr.clear();

    const char* uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
    if (ISMOPTIONS(rq) && (strcmp(uri, "*") == 0)) {
       res = PR_TRUE;
       methodStr.append(registry.GetAllMethods());  
       pool_free(sn->pool, methodVector);
       return res;
    }
        

    for (int x = os->pos - 1; x != -1; --x) {
        httpd_object* obj = os->obj[x];
        dtable *d = &obj->dt[NSAPIService];
        for (int y = 0; y < d->ni; y++) {
            const char* typeExp = pblock_findkeyval(pb_key_type, d->inst[y].param.pb);
            const char* queryExp = pblock_findkeyval(pb_key_query, d->inst[y].param.pb);
            const char* cntType = pblock_findkeyval(pb_key_content_type, rq->srvhdrs);
            const char* query = pblock_findkeyval(pb_key_query, rq->reqpb);
            if ((ParamMatch(typeExp, cntType, PR_TRUE) == PR_TRUE) && 
                (ParamMatch(queryExp, query, PR_FALSE) == PR_TRUE)) {
                const char* methodExp = pblock_findkeyval(pb_key_method, d->inst[y].param.pb);
                const char* rqMethod = pblock_findkeyval(pb_key_method, rq->reqpb);
                if (ISMOPTIONS(rq) || !methodExp || 
                    ParamMatch(methodExp, rqMethod, PR_TRUE) == PR_FALSE) {
                    // Type matched, but not the method
                    for (int key = 0; key < numMethods; key++) {
                        const char* currMethod = registry.GetMethod(key);
                        PR_ASSERT(currMethod);
                        if (ParamMatch(methodExp, currMethod, PR_TRUE) == PR_TRUE) {
                            // method matches
                            methodVector[key] = PR_TRUE;
                        }
                    } // for every registered method
                }
            }
        } // for evey service function in an object
    } // for every object

    for (int key = 0; key < numMethods; key++) {
        if (methodVector[key]) {
            if (methodStr.length() > 0)
                methodStr.append(", ");
            methodStr.append(registry.GetMethod(key));
        }
    }

    pool_free(sn->pool, methodVector);

    return (methodStr.length() > 0);
}

PRBool
ParamMatch(const char* expr, const char* str, PRBool noicmp)
{
    PRBool res = PR_TRUE;

    if (expr) {
        res = PR_FALSE;
        if (str) {
            int cmpRes = 0;
            if (noicmp == PR_TRUE) {
                cmpRes = shexp_noicmp((char*)str, (char*)expr);  
            }
            else {
                cmpRes = shexp_cmp((char*)str, (char*)expr);
            }
            res = (cmpRes == 0 ? PR_TRUE : PR_FALSE);
        }
    }
    

    return res;
}


/* ------------------------------------------------------------------------ */
/* ----------------------------- Input/Output ----------------------------- */
/* ------------------------------------------------------------------------ */


/* ---------------------------- servact_input ----------------------------- */

NSAPI_PUBLIC int servact_input(Session *sn, Request *rq)
{
    NSAPISession *nsn = (NSAPISession *)sn;

    // servact_input() only runs on the original request (if nothing else,
    // input_os_pos is an index into the original request's objset so we
    // can't handle the objset changing underneath us)
    if (INTERNAL_REQUEST(rq) || RESTARTED_REQUEST(rq))
        return REQ_NOACTION;

    // servact_input() should not be called once any request body has been read
    // or an Input SAF returns an error.  Note, however, that servact_input()
    // may be called multiple times per request, up to 1 call per object.
    PR_ASSERT(!nsn->input_done);

    int input_rv = REQ_NOACTION;
    if (nsn->input_os_pos == 0) {
        input_rv = REQ_NOACTION;
    } else {
        input_rv = nsn->input_rv;
    }

    // Fail net_read() calls until these Input directives have run.  Because 
    // subsequent Input directives could insert filters below any given filter,
    // we don't let filters do IO from their insert methods.
    nsn->input_rv = REQ_ABORTED;
    nsn->input_done = PR_TRUE;

    PRBool input_done = PR_FALSE;
    int object_rv = REQ_NOACTION;

    /*
     * Unlike most stages, Input/Output starts executing in the default object
     * and works it way up to the most specific object.  This facilitates the
     * correct layering of filters.
     */

    int x;
    httpd_objset *os = rq->os;
    for (x = nsn->input_os_pos; x < os->pos; x++) {
        dtable *d = &os->obj[x]->dt[NSAPIInput];

        for (int y = 0; y < d->ni; y++) {
            // _CHECK_PARAM falls through, continues, or returns as appropriate
            _CHECK_PARAM(check_type(d->inst[y].param.pb, sn, rq, rq->headers))
            _CHECK_PARAM(check_io_method(d->inst[y].param.pb, sn, rq))
            _CHECK_PARAM(check_query(d->inst[y].param.pb, sn, rq))

            int saf_rv = object_execute(&d->inst[y], sn, rq);

            if (saf_rv == REQ_NOACTION)
                continue;

            object_rv = saf_rv;
            input_rv = saf_rv;

            if (saf_rv == REQ_PROCEED)
                continue;

            /*
             * Input SAF error
             */

            if (d->inst[y].f) {
                // If f == NULL, object_execute() already logged an error
                log_error(LOG_WARN, XP_GetAdminStr(DBT_handleProcessed_),
                          sn, rq, XP_GetAdminStr(DBT_StageXFnYError),
                          directive_num2name(NSAPIInput), d->inst[y].f->name);
            }

            input_done = PR_TRUE;

            break;
        }
    }
    nsn->input_os_pos = x;

    nsn->input_rv = input_rv;
    nsn->input_done = input_done;

    return object_rv;
}


/* --------------------------- _perform_output ---------------------------- */

static inline int _perform_output(Session *sn, Request *rq)
{
    NSAPIRequest *nrq = (NSAPIRequest *)rq;

    // Run Output at most once per Request
    if (nrq->output_done)
        return nrq->output_rv;

    // Fail net_write(), etc. calls until all Output directives have run.
    // Because subsequent Output directives could insert filters below any
    // given filter, we don't let filters do IO from their insert methods.
    nrq->output_rv = REQ_ABORTED;
    nrq->output_done = PR_TRUE;

    int stage_rv = REQ_NOACTION;

    /*
     * Unlike most stages, Input/Output starts executing in the default object
     * and works it way up to the most specific object.  This facilitates the
     * correct layering of filters.
     */

    httpd_objset *os = rq->os;
    for (int x = 0; x < os->pos; ++x) {
        dtable *d = &os->obj[x]->dt[NSAPIOutput];

        for (int y = 0; y < d->ni; y++) {
            // _CHECK_PARAM falls through, continues, or returns as appropriate
            _CHECK_PARAM(check_type(d->inst[y].param.pb, sn, rq, rq->srvhdrs))
            _CHECK_PARAM(check_io_method(d->inst[y].param.pb, sn, rq))
            _CHECK_PARAM(check_query(d->inst[y].param.pb, sn, rq))

            int saf_rv = object_execute(&d->inst[y], sn, rq);

            if (saf_rv == REQ_NOACTION)
                continue;

            stage_rv = saf_rv;

            if (saf_rv == REQ_PROCEED)
                continue;

            /*
             * Output SAF error
             */

            if (d->inst[y].f) {
                // If f == NULL, object_execute() already logged an error
                log_error(LOG_WARN, XP_GetAdminStr(DBT_handleProcessed_),
                          sn, rq, XP_GetAdminStr(DBT_StageXFnYError),
                          directive_num2name(NSAPIOutput), d->inst[y].f->name);
            }

            break;
        }
    }

    // If the stage returned REQ_PROCEED, store REQ_NOACTION (i.e. subsequent
    // servact_output() calls will be no ops)
    if (stage_rv == REQ_PROCEED) {
        nrq->output_rv = REQ_NOACTION;
    } else {
        nrq->output_rv = stage_rv;
    }

    return stage_rv;
}


/* ---------------------------- servact_output ---------------------------- */

NSAPI_PUBLIC int servact_output(Session *sn, Request *rq)
{
    NSAPIRequest *nrq = (NSAPIRequest *)rq;

    // servact_output() should only be called once per Request; the caller
    // should inspect nrq->output_done
    PR_ASSERT(!nrq->output_done);
    if (nrq->output_done)
        return nrq->output_rv;

    Request *prev_rq;
    Request *curr_rq;

    // Build a linked list with the original Request (or the first Request
    // that used this filter stack) at the head
    prev_rq = NULL;
    curr_rq = rq;
    for (;;) {
        ((NSAPIRequest *)curr_rq)->output_next_rq = prev_rq;
        prev_rq = curr_rq;

        // Stop when we reach the original Request
        if (curr_rq->orig_rq == curr_rq)
            break;

        // Stop if we see a Request that was given its own filter stack by
        // session_clone()
        if (((NSAPIRequest *)curr_rq)->session_clone)
            break;

        curr_rq = curr_rq->orig_rq;
    }

    // Handle the Output stage for each Request, starting with the original
    PRBool output_done = PR_FALSE;
    int output_rv = REQ_NOACTION;
    while (curr_rq) {
        if (output_done) {
            // A lower Request failed Output
            ((NSAPIRequest *)curr_rq)->output_done = PR_TRUE;
            ((NSAPIRequest *)curr_rq)->output_rv = output_rv;

        } else if (curr_rq->os) {
            // Run the Output stage for this Request
            int curr_rq_rv = _perform_output(sn, curr_rq);
            if (curr_rq_rv != REQ_NOACTION) {
                output_rv = curr_rq_rv;
                if (curr_rq_rv != REQ_PROCEED)
                    output_done = PR_TRUE;
            }
        }

        curr_rq = ((NSAPIRequest *)curr_rq)->output_next_rq;
    }

    PR_ASSERT(sn->csd_open == 1);
    PR_ASSERT(sn->csd->higher == NULL);
    PR_ASSERT(request_output_done(rq));

    return output_rv;
}


/* ---------------------------- servact_lookup ---------------------------- */

NSAPI_PUBLIC int servact_lookup(Session *sn, Request *rq)
{
    int rv;

    do {
        /* Figure out what objects it uses */
        rv = servact_uri2path(sn, rq);
        if (rv != REQ_PROCEED)
            continue;

        /* Process PathChecks */
        rv = servact_pathchecks(sn, rq);
        if (rv != REQ_PROCEED)
            continue;

        /* Find that file's typing information */
        rv = servact_fileinfo(sn, rq);
        if (rv != REQ_PROCEED)
            continue;

    } while (rv == REQ_RESTART);

    return rv;
}


/* ----------------------- servact_include_virtual ------------------------ */

NSAPI_PUBLIC int servact_include_virtual(Session *sn, Request *parent_rq,
                                         const char *location,
                                         pblock *param)
{
    Request *child_rq = request_create_virtual(sn, parent_rq, location, param);
    if (!child_rq)
        return REQ_ABORTED;

    int rv = servact_handle_processed(sn, child_rq);

    request_free(child_rq);

    return rv;
}


/* ------------------------- servact_include_file ------------------------- */

NSAPI_PUBLIC int servact_include_file(Session *sn, Request *o_rq, const char *path)
{
    pb_param *pp;
    Request *rq;

    char *location = pblock_findkeyval(pb_key_uri, o_rq->reqpb);
    if (!location)
        return REQ_ABORTED;

    rq = request_create_virtual(sn, o_rq, location, NULL);

    httpd_objset *os = NULL;
    if (((NSAPIRequest*)rq)->hrq) {
        os = ((NSAPIRequest*)rq)->hrq->getObjset();
    } else if (const VirtualServer *vs = conf_get_vs()) {
        os = vs->getObjset();
    } else {
        return REQ_ABORTED;
    }

    const VirtualServer* vs = request_get_vs(rq);
    httpd_object *obj;

    const char *name = vs->defaultObjectName;

    rq->os = objset_create_pool(sn->pool);

    obj = objset_findbyname(name, rq->os, os);
    if (!obj)
        return REQ_ABORTED;

    objset_add_object(obj, rq->os);

    pblock_nvinsert("path", path, rq->vars);

    int rv = servact_fileinfo(sn, rq);
    if (rv != REQ_PROCEED)
        return rv;
    rv = servact_service(sn, rq);

    return rv;
}

