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
 * req.c: Request-specific stuff
 * 
 * Rob McCool
 * 
 */

#include "time/nstime.h"
#include "NsprWrap/NsprError.h"
#include "base/util.h"
#include "base/plist.h"
#include "base/language.h"
#include "frame/log.h"
#include "frame/req.h"
#include "frame/conf.h"
#include "frame/http.h"
#include "frame/http_ext.h"
#include "frame/httpdir.h"
#include "frame/filter.h"
#include "frame/dbtframe.h"
#include "safs/nsfcsafs.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httprequest.h"


#define ACCEPT_LANGUAGE "Accept-language"
#define ACCEPT_LANGUAGE_LEN (sizeof(ACCEPT_LANGUAGE) - 1)

/*
 * _request_server_active is set after the server has started its request
 * processing threads.
 */
static PRBool _request_server_active;

/*
 * _request_slots is the number of request_alloc_slot() slots that have been
 * registered.
 */
static PRInt32 _request_slots;

/*
 * _request_slot_destructors is an array of _request_slots elements containing
 * the SlotDestructorFuncPtr for each registered request_alloc_slot() slot.
 * Slots without destructors have NULL SlotDestructorFuncPtrs.
 */
static SlotDestructorFuncPtr* _request_slot_destructors;


/* ----------------------------- request_pool ----------------------------- */

NSAPI_PUBLIC pool_handle_t *request_pool(Request *rq)
{
    NSAPIRequest *nrq = (NSAPIRequest *)rq;

    pool_handle_t *pool = NULL;
    if (nrq && nrq->hrq)
        pool = nrq->hrq->GetDaemonSession().GetThreadPool();
    if (!pool)
        pool = system_pool();

    return pool;
}


/* ------------------------- request_reset_slots -------------------------- */

static inline void request_reset_slots(NSAPIRequest *nrq)
{
    // Give code with non-NULL per-Request data a chance to clean up
    for (int i = nrq->data.count - 1; i >= 0; i--) {
        if (nrq->data.slots[i] && _request_slot_destructors[i])
            (*_request_slot_destructors[i])(nrq->data.slots[i]);
        nrq->data.slots[i] = NULL;
    }
}


/* ----------------------------- request_free ----------------------------- */

NSAPI_PUBLIC void request_free(Request *rq)
{
    NSAPIRequest *nrq = (NSAPIRequest *) rq;

    // No references to the ACL list should exist outside of PathCheck
    // XXX elving CR 6322567 PR_ASSERT(nrq->rq.acllist == NULL);

    // Remove any filters that were installed during this request
    if (nrq->filter_sn) {
        filter_finish_request(nrq->filter_sn, &nrq->rq, NULL);
        nrq->filter_sn = NULL;
    }

    // Any accelerator cache data should already have been cleaned up by
    // accel_store()
    PR_ASSERT(nrq->accel_nsfc_entry == NSFCENTRY_INIT);
    PR_ASSERT(nrq->accel_ssl_unclean_shutdown_browser == NULL);

    request_reset_slots(nrq);
}


/* -------------------------- request_initialize -------------------------- */

NSAPI_PUBLIC PRStatus request_initialize(pool_handle_t *pool,
                                         HttpRequest *hrq,
                                         const char *hostname,
                                         NSAPIRequest *nrq)
{
    Request *rq = &nrq->rq;

    rq->vars = pblock_create_pool(pool, REQ_HASHSIZE);
    if (!rq->vars)
        return PR_FAILURE;
    rq->reqpb = pblock_create_pool(pool, REQ_HASHSIZE);
    if (!rq->reqpb)
        return PR_FAILURE;
    rq->loadhdrs = 0;
    rq->headers = pblock_create_pool(pool, REQ_HASHSIZE);
    if (!rq->headers)
        return PR_FAILURE;
    rq->senthdrs = 0;
    rq->srvhdrs = pblock_create_pool(pool, REQ_HASHSIZE);
    if (!rq->srvhdrs)
        return PR_FAILURE;
    rq->os = NULL;
    rq->tmpos = NULL;
    rq->statpath = NULL;
    rq->staterr = NULL;
    rq->finfo = NULL;
    rq->aclstate = 0;
    rq->acldirno = 0;
    rq->aclname = NULL;
    rq->aclpb = NULL;
    rq->acllist = NULL;
    rq->request_is_cacheable = 0;
    rq->directive_is_cacheable = 0;
    rq->cached_headers = NULL;
    rq->cached_headers_len = 0;
    rq->unused = NULL;
    rq->req_start = ft_time();
    rq->protv_num = 0;
    rq->method_num = -1;
    PR_ASSERT(sizeof(rq->rq_attr) == sizeof(RQATTR));
    *(RQATTR *) &rq->rq_attr = 0;
    rq->hostname = pool_strdup(pool, hostname);
    rq->allowed = 0;
    rq->byterange = 0;
    rq->status_num = 0;
    rq->staterrno = 0;
    rq->orig_rq = rq;

    nrq->hrq = hrq;
    nrq->nsfcpath = NULL;
    nrq->nsfcinfo = NULL;
    nrq->param = NULL;
    nrq->webModule = NULL;
    nrq->servletResource = NULL;
    nrq->davCollection = NULL;
    nrq->filter_sn = NULL;
    nrq->output_next_rq = NULL;
    nrq->output_done = PR_FALSE;
    nrq->output_rv = REQ_PROCEED;
    nrq->session_clone = PR_FALSE;
    nrq->data.slots = NULL;
    nrq->data.count = 0;
    nrq->nuri2path = 0;
    nrq->accel_nsfc_entry = NSFCENTRY_INIT;
    nrq->accel_flex_log = NULL;
    nrq->accel_ssl_unclean_shutdown_browser = NULL;

    return PR_SUCCESS;
}


/* ---------------------------- request_create ---------------------------- */

static inline NSAPIRequest *request_create(pool_handle_t *pool,
                                           HttpRequest *hrq)
{
    NSAPIRequest *nrq;

    const char *hostname;
    const char *host;
    if (hrq) {
        hostname = hrq->GetServerHostname();
        host = pblock_findkeyval(pb_key_host, hrq->GetNSAPIRequest()->rq.headers);
    } else {
        hostname = conf_get_true_globals()->Vserver_hostname;
        host = NULL;
    }

    nrq = (NSAPIRequest *) pool_malloc(pool, sizeof(NSAPIRequest));
    if (nrq) {
        if (request_initialize(pool, hrq, hostname, nrq) == PR_SUCCESS) {
            if (host)
                pblock_kvinsert(pb_key_host, host, strlen(host), nrq->rq.headers);
            nrq->rq.rq_attr.internal_req = 1;
        } else {
            nrq = NULL;
        }
    }

    return nrq;
}

NSAPI_PUBLIC Request *request_create(void)
{
    pool_handle_t *pool = system_pool();
    HttpRequest *hrq = HttpRequest::CurrentRequest();
    return (Request *) request_create(pool, hrq);
}


/* ---------------------------- request_depth ----------------------------- */

static inline void request_depth(Request *rq, int *prestarted, int *pinternal)
{
    // Calculate the depth of the rq->orig_rq linked list
    while (rq->orig_rq != rq) {
        if (rq->rq_attr.req_restarted)
            (*prestarted)++;
        if (rq->rq_attr.internal_req)
            (*pinternal)++;
        rq = rq->orig_rq;
    }
}


/* ------------------------ request_parse_location ------------------------ */

static inline void request_parse_location(const char *location,
                                          const char **puri, int *purilen,
                                          const char **pquery)
{
    *puri = location;

    if (util_is_url(location)) {
        *purilen = strlen(location);
        *pquery = NULL;
    } else {
        *pquery = strchr(location, '?');
        if (*pquery) {
            *purilen = *pquery - location;
            (*pquery)++;
        } else {
            *purilen = strlen(location);
        }
    }
}


/* ------------------------- request_get_base_uri ------------------------- */

NSAPI_PUBLIC void request_get_base_uri(Request *rq, const char **pbase,
                                       int *plen)
{
    // URI                 Base URI
    // ------------------- ---------
    // /index.html         /
    // /foo/bar/           /foo/bar/
    // /foo/bar/index.html /foo/bar/
    // /cgi-bin/foo.pl/bar /cgi-bin/

    if (rq) {
        const char *uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
        if (uri) {
            int len = strlen(uri);
            const char *path_info = pblock_findkeyval(pb_key_path_info, rq->vars);
            if (path_info) {
                int suffixlen = strlen(path_info);
                if (len > suffixlen)
                    len -= suffixlen;
            }

            do { len--; } while (len > 0 && uri[len] != '/');

            if (len > 0) {
                *pbase = uri;
                *plen = len + 1;
                return;
            }
        }
    }

    *pbase = "/";
    *plen = 1;
}


/* --------------------------- request_set_uri ---------------------------- */

static PRStatus request_set_uri(pool_handle_t *pool, Request *rq,
                                Request *old_rq, const char *uri, int urilen)
{
    // If the passed URI isn't an URL...
    if (!util_is_url(uri)) {
        // Construct an absolute URI if necessary
        if (*uri != '/') {
            const char *base;
            int baselen;
            request_get_base_uri(old_rq, &base, &baselen);

            char *abs = (char *) pool_malloc(pool, baselen + urilen + 1);
            if (!abs)
                return PR_FAILURE;

            memcpy(abs, base, baselen);
            memcpy(abs + baselen, uri, urilen);
            abs[baselen + urilen] = '\0';

            uri = abs;
            urilen = baselen + urilen;
        }

        // Canonicalize the URI (e.g. remove "..")
        int canonlen;
        char *canon = util_canonicalize_uri(pool, uri, urilen, &canonlen);
        if (!canon)
            return PR_FAILURE;

        uri = canon;
        urilen = canonlen;
    }

    // Remove any escaped version of the original URI as it will likely be
    // inconsistent with the new URI
    pblock_removekey(pb_key_escaped, rq->reqpb);

    // Set the new URI
    PR_ASSERT(!pblock_findkey(pb_key_uri, rq->reqpb));
    if (!pblock_kvinsert(pb_key_uri, uri, urilen, rq->reqpb))
        return PR_FAILURE;

    return PR_SUCCESS;
}


/* ------------------------ request_prepare_child ------------------------- */

static NSAPIRequest *request_prepare_child(pool_handle_t *pool,
                                           Session *sn,
                                           Request *parent_rq,
                                           const char *method,
                                           const char *uri, int urilen,
                                           const char *query)
{
    // Prevent infinite recursion
    int nrestarted = 0;
    int ninternal = 0;
    request_depth(parent_rq, &nrestarted, &ninternal);
    if (ninternal >= nrestarted + MAX_REQUEST_CHILDREN) {
        log_error(LOG_FAILURE, NULL, sn, parent_rq,
                  XP_GetAdminStr(DBT_httpChildUriXExceedsDepthY),
                  uri, MAX_REQUEST_CHILDREN);
        return NULL;
    }

    NSAPIRequest *parent_nrq = (NSAPIRequest *) parent_rq;

    // Allocate an empty request
    NSAPIRequest *child_nrq = request_create(pool, parent_nrq->hrq);
    if (!child_nrq)
        return NULL;

    Request *child_rq = &child_nrq->rq;
    PR_ASSERT(child_rq->rq_attr.internal_req == 1);
    child_rq->orig_rq = parent_rq;

    if (method) {
        // Use method specified by caller
        if (!pblock_kvinsert(pb_key_method, method, strlen(method), child_rq->reqpb))
            return NULL;
        child_rq->method_num = http_get_method_num(method);
    } else if (parent_rq->method_num == METHOD_HEAD) {
        // Preserve HEAD method from parent request
        if (!pblock_kvinsert(pb_key_method, "HEAD", 4, child_rq->reqpb))
            return NULL;
        child_rq->method_num = METHOD_HEAD;
    } else {
        // Default to GET
        if (!pblock_kvinsert(pb_key_method, "GET", 3, child_rq->reqpb))
            return NULL;
        child_rq->method_num = METHOD_GET;
    }

    if (request_set_uri(pool, child_rq, parent_rq, uri, urilen) != PR_SUCCESS)
        return NULL;

    if (query) {
        if (!pblock_kvinsert(pb_key_query, query, strlen(query), child_rq->reqpb))
            return NULL;
    }

    // Use the parent protocol for the child
    child_rq->protv_num = parent_rq->protv_num;
    if (const char *p = pblock_findkeyval(pb_key_protocol, parent_rq->reqpb)) {
        if (!pblock_kvinsert(pb_key_protocol, p, strlen(p), child_rq->reqpb))
            return NULL;
    }

    // Give the child a copy of the request headers
    if (pblock_copy(parent_rq->headers, child_rq->headers) == -1)
        return NULL;

    // Remove RFC 2616 9.3 request headers that make GETs conditional
    while (pblock_removekey(pb_key_if_modified_since, child_rq->headers));
    while (pblock_removekey(pb_key_if_unmodified_since, child_rq->headers));
    while (pblock_removekey(pb_key_if_match, child_rq->headers));
    while (pblock_removekey(pb_key_if_none_match, child_rq->headers));
    while (pblock_removekey(pb_key_if_range, child_rq->headers));

    // Propagate authentication information to the child
    for (int hi = 0; hi < parent_rq->vars->hsize; hi++) {
        for (pb_entry *pe = parent_rq->vars->ht[hi]; pe; pe = pe->next) {
            if (*pe->param->name == 'a' && !strncmp(pe->param->name, "auth-", 5)) {
                if (!pblock_nvinsert(pe->param->name, pe->param->value, child_rq->vars))
                    return NULL;
            }
        }
    }

    return child_nrq;
}


/* ------------------------- request_create_child ------------------------- */

NSAPI_PUBLIC Request *request_create_child(Session *sn, Request *parent_rq,
                                           const char *method, const char *uri,
                                           const char *query)
{
    NSAPIRequest *child_nrq = request_prepare_child(sn->pool, sn, parent_rq, method, uri, strlen(uri), query);
    if (!child_nrq)
        return NULL;

    return &child_nrq->rq;
}


/* ------------------------ request_create_virtual ------------------------ */

NSAPI_PUBLIC Request *request_create_virtual(Session *sn, Request *parent_rq,
                                             const char *location,
                                             pblock *param)
{
    // Parse any query string from the location argument
    const char *uri;
    int urilen;
    const char *query;
    request_parse_location(location, &uri, &urilen, &query);

    NSAPIRequest *child_nrq = request_prepare_child(sn->pool, sn, parent_rq, NULL, uri, urilen, query);
    if (!child_nrq)
        return NULL;

    child_nrq->param = param;

    return &child_nrq->rq;
}


/* --------------------- request_prepare_for_restart ---------------------- */

static PRStatus request_prepare_for_restart(pool_handle_t *pool, Session *sn,
                                            Request *rq, const char *method,
                                            const char *uri, int urilen,
                                            const char *query)
{
    log_error(LOG_VERBOSE, NULL, sn, rq,
              "restarting request as %.*s%s%s",
              urilen, uri, query ? "?" : "", query ? query : "");

    // Prevent restart loops
    int nrestarted = 0;
    int ninternal = 0;
    request_depth(rq, &nrestarted, &ninternal);
    if (nrestarted >= MAX_REQUEST_RESTARTS) {
        log_error(LOG_FAILURE, NULL, sn, rq,
                  XP_GetAdminStr(DBT_httpRestartUriXExceedsDepthY),
                  uri, MAX_REQUEST_RESTARTS);
        return PR_FAILURE;
    }

    NSAPIRequest *nrq = (NSAPIRequest *) rq;

    // Allocate an empty request
    NSAPIRequest *saved_nrq = request_create(pool, nrq->hrq);
    if (!saved_nrq)
        return PR_FAILURE;

    Request *saved_rq = &saved_nrq->rq;

    // Save information about the original request for posterity
    if (pblock_copy(rq->reqpb, saved_rq->reqpb) == -1)
        return PR_FAILURE;
    if (pblock_copy(rq->headers, saved_rq->headers) == -1)
        return PR_FAILURE;
    saved_rq->req_start = rq->req_start;
    saved_rq->protv_num = rq->protv_num;
    saved_rq->method_num = rq->method_num;
    saved_rq->rq_attr.req_restarted = rq->rq_attr.req_restarted;
    PR_ASSERT(saved_rq->rq_attr.internal_req);
    PR_ASSERT(!saved_rq->rq_attr.perm_req);

    // Create a dummy objset for the saved request.  Because we don't want the
    // original request's Output, Route, etc. directives to be executed while
    // processing the restarted request, it's important that this dummy objset
    // be empty.
    saved_rq->os = objset_create_pool(pool);
    httpd_object *saved_obj = object_create(NUM_DIRECTIVES, pblock_create(1));
    objset_add_object(saved_obj, saved_rq->os);

    // Reset some request state in preparation for the restart.  Since we're
    // modifying the original Request *, we need to be careful never to leave
    // things in a state where the Request * can't be passed to Error and
    // AddLog SAFs.
    request_reset_slots(nrq);
    pblock *vars = pblock_create_pool(pool, REQ_HASHSIZE);
    if (!vars)
        return PR_FAILURE;
    rq->vars = vars;
    pblock *srvhdrs = pblock_create_pool(pool, REQ_HASHSIZE);
    if (!srvhdrs)
        return PR_FAILURE;
    rq->srvhdrs = srvhdrs;
    rq->os = NULL;
    rq->req_start = ft_time();
    rq->rq_attr.req_restarted = 1;
    nrq->param = NULL;
    nrq->webModule = NULL;
    nrq->servletResource = NULL;
    nrq->davCollection = NULL;

    // Override the request's method only if caller specified one
    if (method) {
        pblock_removekey(pb_key_method, rq->reqpb);
        if (!pblock_kvinsert(pb_key_method, method, strlen(method), rq->reqpb))
            return PR_FAILURE;
        rq->method_num = http_get_method_num(method);
    }

    // Override the request's URI
    pblock_removekey(pb_key_uri, rq->reqpb);
    if (request_set_uri(pool, rq, saved_rq, uri, urilen) != PR_SUCCESS)
        return PR_FAILURE;

    // Override the request's query string
    pblock_removekey(pb_key_query, rq->reqpb);
    if (query) {
        if (!pblock_kvinsert(pb_key_query, query, strlen(query), rq->reqpb))
            return PR_FAILURE;
    }

    // Insert the saved request data into the rq->orig_rq linked list
    if (rq->orig_rq == rq) {
        saved_rq->orig_rq = saved_rq;
    } else {
        saved_rq->orig_rq = rq->orig_rq;
    }
    rq->orig_rq = saved_rq;

    return PR_SUCCESS;
}


/* --------------------------- request_restart ---------------------------- */

NSAPI_PUBLIC int request_restart(Session *sn, Request *rq, const char *method,
                                 const char *uri, const char *query)
{
    if (request_prepare_for_restart(sn->pool, sn, rq, method, uri, strlen(uri), query) != PR_SUCCESS)
        return REQ_ABORTED;

    return REQ_RESTART;
}


/* ----------------------- request_restart_location ----------------------- */

NSAPI_PUBLIC int request_restart_location(Session *sn, Request *rq,
                                          const char *location)
{
    // Parse any query string from the location argument
    const char *uri;
    int urilen;
    const char *query;
    request_parse_location(location, &uri, &urilen, &query);

    if (request_prepare_for_restart(sn->pool, sn, rq, NULL, uri, urilen, query) != PR_SUCCESS)
        return REQ_ABORTED;

    return REQ_RESTART;
}


/* ----------------------- request_restart_internal ----------------------- */

NSAPI_PUBLIC Request *request_restart_internal(const char *location,
                                               Request *rq)
{
    pool_handle_t *pool = request_pool(rq);

    // Parse any query string from the location argument
    const char *uri;
    int urilen;
    const char *query;
    request_parse_location(location, &uri, &urilen, &query);

    // If we're restarting an existing request...
    if (rq) {
        if (request_prepare_for_restart(pool, NULL, rq, NULL, uri, urilen, query) != PR_SUCCESS)
            return NULL;

        return rq;
    }

    // Allocate an empty request
    NSAPIRequest *nrq = request_create(pool, GetHrq(rq));
    if (!nrq)
        return NULL;

    rq = &nrq->rq;

    // Build a dummy request (hopefully there's a virtual server associated
    // with this thread...)
    rq->rq_attr.internal_req = 1;
    if (!pblock_kvinsert(pb_key_protocol, "HTTP/0.9", 8, rq->reqpb))
        return NULL;
    rq->protv_num = PROTOCOL_VERSION_HTTP09;
    if (!pblock_kvinsert(pb_key_method, "GET", 3, rq->reqpb))
        return NULL;
    rq->method_num = METHOD_GET;
    if (request_set_uri(pool, rq, NULL, uri, urilen) != PR_SUCCESS)
        return NULL;
    if (query) {
        if (!pblock_kvinsert(pb_key_query, query, strlen(query), rq->reqpb))
            return NULL;
    }

    return rq;
}


/* ---------------------------- request_header ---------------------------- */

NSAPI_PUBLIC int request_header(char *name, char **value, Session *sn,
                                Request *rq)
{
    *value = pblock_findval(name, rq->headers);
    return REQ_PROCEED;
}


/* -------------------------- request_stat_path --------------------------- */

NSAPI_PUBLIC struct stat *request_stat_path(const char *tpath, Request *rq)
{
    const char *path = (tpath ? tpath : pblock_findkeyval(pb_key_path, rq->vars));
    pool_handle_t *pool = request_pool(rq);

    if(!path || !file_is_path_abs(path)) {
        rq->statpath = NULL;
        rq->staterrno = PR_INVALID_ARGUMENT_ERROR;
        rq->staterr = pool_strdup(pool, XP_GetAdminStr(DBT_statNoPathGiven));
        NsprError::setError(rq->staterrno, rq->staterr);
        return NULL;
    }
    if(rq->statpath) {
        if(!strcmp(rq->statpath, path)) {
            NsprError::setError(rq->staterrno, rq->staterr);
            return rq->finfo;
        }
        rq->statpath = NULL;
    }
    if(!rq->finfo)
        rq->finfo = (struct stat *) pool_malloc(pool, sizeof(struct stat));

    if(system_stat(path, rq->finfo) == -1) {
        /* Tell future stats that this was not found */
        rq->statpath = pool_strdup(pool, path);
        rq->staterrno = PR_GetError();
        rq->finfo = NULL; 
        rq->staterr = pool_strdup(pool, system_errmsg());
        return NULL;
    }
    rq->statpath = pool_strdup(pool, path);
    rq->staterrno = 0;
    rq->staterr = NULL;
    return rq->finfo;
}


/* -------------------------- request_info_path --------------------------- */

NSAPI_PUBLIC PRStatus INTrequest_info_path(const char *tpath, Request *rq,
                                           NSFCFileInfo **finfo)
{
    NSAPIRequest *nrq = (NSAPIRequest *)rq;
    pool_handle_t *pool = request_pool(rq);
    const char *path = (tpath ? tpath : pblock_findkeyval(pb_key_path, rq->vars));
    PRStatus rv;

    if (!path || !file_is_path_abs(path)) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    char *nostat = pblock_findkeyval(pb_key_nostat, rq->vars);
    if (nostat) {
	int nostatlen = strlen(nostat);
        char *ntrans_base = pblock_findkeyval(pb_key_ntrans_base, rq->vars);
        if (ntrans_base) {
	    int baselen = strlen(ntrans_base);
            int totalen = baselen+nostatlen;
            if (!strncmp(path, ntrans_base, baselen) && 
                !strncmp(&path[baselen], nostat, nostatlen) &&
                (path[totalen] ==  '/' || path[totalen] == '\0')) {
                PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
                return PR_FAILURE;
            }
        }
        else if (!strncmp(path, nostat, nostatlen) &&  
                 (path[nostatlen] == '/' || path[nostatlen] == '\0')) {
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return PR_FAILURE;
        }
    }
    if (nrq->nsfcpath && !strcmp(nrq->nsfcpath, path)) {
        if (finfo) {
            *finfo = nrq->nsfcinfo;
        }
        if (nrq->nsfcinfo->prerr || nrq->nsfcinfo->oserr) {
            PR_SetError(nrq->nsfcinfo->prerr, nrq->nsfcinfo->oserr);
            return PR_FAILURE;
        }
        return PR_SUCCESS;
    }
    nrq->nsfcpath = pool_strdup(pool, path);

    if (!nrq->nsfcinfo)
        nrq->nsfcinfo = (NSFCFileInfo *) pool_malloc(pool, sizeof(NSFCFileInfo));

    rv = NSFC_GetFileInfo(path, nrq->nsfcinfo, GetServerFileCache());

    if (finfo) {
        *finfo = nrq->nsfcinfo;
    }

    if (rv == PR_FAILURE) {
        PR_SetError(nrq->nsfcinfo->prerr, nrq->nsfcinfo->oserr);
    }

    return rv;
}


/* ------------------------- request_is_internal -------------------------- */

NSAPI_PUBLIC int INTrequest_is_internal(Request *rq)
{
    return (rq->rq_attr.internal_req == 1);
}


/* ------------------------- request_is_restarted ------------------------- */

NSAPI_PUBLIC PRBool INTrequest_is_restarted(Request *rq)
{
    return (rq->rq_attr.req_restarted == 1);
}


/* --------------------- request_is_default_type_set ---------------------- */

NSAPI_PUBLIC PRBool INTrequest_is_default_type_set(const Request *rq) 
{
    return (rq->rq_attr.default_type_set == 1);
}


/* ----------------------- request_has_default_type ----------------------- */

NSAPI_PUBLIC void INTrequest_has_default_type(Request *rq) 
{
    rq->rq_attr.default_type_set = 1;
}


/* ---------------------------- request_get_vs ---------------------------- */

NSAPI_PUBLIC const VirtualServer *INTrequest_get_vs(Request *rq) 
{
    if (rq) {
        NSAPIRequest *nrq = (NSAPIRequest *) rq;
        if (nrq && nrq->hrq)
            return nrq->hrq->getVS();
    }
    return conf_get_vs();
}


/* ---------------------- request_get_start_interval ---------------------- */

NSAPI_PUBLIC PRStatus INTrequest_get_start_interval(Request *rq,
                                                    PRIntervalTime *start)
{
    if (StatsManager::isInitialized()) {
        if (rq) {
            HttpRequest *hrq = ((NSAPIRequest*)rq)->hrq;
            if (hrq) {
                DaemonSession *ds = &hrq->GetDaemonSession();
                *start = ds->getRequestStartTime();
                return PR_SUCCESS;
            }
        }
    }

    return PR_FAILURE;
}


/* ------------------------ request_server_active ------------------------- */

NSAPI_PUBLIC void INTrequest_server_active(void)
{
    _request_server_active = PR_TRUE;
}


/* ----------------------- request_is_server_active ----------------------- */

NSAPI_PUBLIC PRBool INTrequest_is_server_active(void)
{
    return _request_server_active;
}


/* -------------------------- request_alloc_slot -------------------------- */

NSAPI_PUBLIC int request_alloc_slot(SlotDestructorFuncPtr destructor)
{
    // Slots must be allocated before we go threaded
    PR_ASSERT(!_request_server_active);
    if (_request_server_active)
        return -1;

    int slot = _request_slots;
    _request_slots++;

    _request_slot_destructors = (SlotDestructorFuncPtr *)
        PERM_REALLOC(_request_slot_destructors,
                     sizeof(_request_slot_destructors[0]) *
                     _request_slots);
    _request_slot_destructors[slot] = destructor;

    return slot;
}


/* --------------------------- request_get_data --------------------------- */

NSAPI_PUBLIC void *INTrequest_get_data(Request *rq, int slot)
{
    NSAPIRequest *nrq = (NSAPIRequest *)rq;

    PR_ASSERT(slot >= 0 && slot < _request_slots);

    if (slot >= 0 && slot < nrq->data.count)
        return nrq->data.slots[slot];

    return NULL;
}


/* --------------------------- request_set_data --------------------------- */

NSAPI_PUBLIC void *INTrequest_set_data(Request *rq, int slot, void *data)
{
    NSAPIRequest *nrq = (NSAPIRequest *)rq;
    void *old = NULL;

    PR_ASSERT(slot >= 0 && slot < _request_slots);

    if (slot >= 0 && slot < _request_slots) {
        // Grow/allocate the slots[] array if necessary
        if (slot >= nrq->data.count) {
            if (nrq->data.count == 0)
                nrq->data.slots = NULL;

            void **slots = (void **)pool_realloc(request_pool(rq),
                nrq->data.slots, sizeof(nrq->data.slots[0]) * _request_slots);
            if (!slots)
                return data;

            for (int i = nrq->data.count; i < _request_slots; i++)
                slots[i] = NULL;

            nrq->data.slots = slots;
            nrq->data.count = _request_slots;
        }

        old = nrq->data.slots[slot];
        nrq->data.slots[slot] = data;
    }

    return old;
}


/* -------------------------- request_lang_path --------------------------- */

static int request_lang_path(Session *sn,
                             Request *rq,
                             const char *root,
                             int rootlen,
                             const char *ppath,
                             int ppathlen)
{
    const VirtualServer *vs = request_get_vs(rq);
    if (!vs->localization.negotiateClientLanguage)
        return REQ_NOACTION;

    // We will make decisions based on the Accept-language header, so this
    // request cannot be cached based on URI alone
    rq->request_is_cacheable = 0;

    // If this request was restarted with a language-specific URI...
    if (VARY_ACCEPT_LANGUAGE(rq)) {
        // Indicate content varies based on the Accept-language header
        if (!pblock_findkey(pb_key_vary, rq->srvhdrs))
            pblock_kvinsert(pb_key_vary, ACCEPT_LANGUAGE, ACCEPT_LANGUAGE_LEN, rq->srvhdrs);

        return REQ_NOACTION;
    }

    // Check for langpath, a language-specific file system path
    char *langpath = lang_acceptlang_file(ppath);
    if (!langpath)
        return REQ_NOACTION;

    PR_ASSERT(!strncmp(ppath, root, rootlen));
    PR_ASSERT(!strncmp(langpath, root, rootlen));
    PR_ASSERT(langpath[rootlen] == '/');

    // Find the first difference between langpath and the "normal" path
    const char *ppathsfx = ppath + rootlen;
    char *langpathsfx = langpath + rootlen;
    while (*ppathsfx && *ppathsfx == *langpathsfx) {
        ppathsfx++;
        langpathsfx++;
    }

    int rv = REQ_NOACTION;

    // If langpath and the "normal" path have different suffixes...
    if (*ppathsfx != *langpathsfx) {
        int ppathsfxlen = ppathlen - (ppathsfx - ppath);
        int langpathsfxlen = strlen(langpathsfx);

        // If the URI currently ends with the "normal" path suffix...
        const char *uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
        int urilen = strlen(uri);
        if (urilen > ppathsfxlen && !strcmp(ppathsfx, uri + urilen - ppathsfxlen)) {
            // Build a language-specific URI by chopping off the "normal" path
            // suffix and replacing it with a language-specific path suffix
            // (e.g. replace "index.html" with "en/index.html" or ".html" with
            // with "_en.html")
            char *languri = (char *)pool_malloc(sn->pool, urilen - ppathsfxlen + langpathsfxlen + 1);
            memcpy(languri, uri, urilen - ppathsfxlen);
            memcpy(languri + urilen - ppathsfxlen, langpathsfx, langpathsfxlen + 1);

            // We'll restart the request with a language-specific URI
            VARY_ACCEPT_LANGUAGE(rq) = 1;
            rv = request_restart(sn, rq, NULL, languri, pblock_findkeyval(pb_key_query, rq->reqpb));
        }
    }

    return rv;
}


/* --------------------------- request_set_path --------------------------- */

NSAPI_PUBLIC int INTrequest_set_path(Session *sn,
                                     Request *rq,
                                     const char *root,
                                     int rootlen,
                                     const char *ppath,
                                     int ppathlen)
{
    pb_param *pp = pblock_findkey(pb_key_ppath, rq->vars);
    if (!pp)
        return REQ_ABORTED;

    int newpathlen = rootlen + ppathlen;
    char *newpath = (char *)pool_malloc(sn->pool, newpathlen + 1);
    if (!newpath)
        return REQ_ABORTED;

    memcpy(newpath, root, rootlen);
    memcpy(newpath + rootlen, ppath, ppathlen);
    newpath[rootlen + ppathlen] = '\0';

    pp->value = newpath;

    pblock_kvinsert(pb_key_ntrans_base, root, rootlen, rq->vars);

    int rv = request_lang_path(sn, rq, root, rootlen, newpath, newpathlen);
    if (rv != REQ_NOACTION)
        return rv;

    return REQ_PROCEED;
}


NSAPI_PUBLIC PRUint32 
GetCurrentRecursionDepth()
{
  HttpRequest *hrq = HttpRequest::CurrentRequest();
  PR_ASSERT(hrq);
  return hrq->GetRecursionDepth();
}

NSAPI_PUBLIC void 
IncrementRecursionDepth()
{
  HttpRequest *hrq = HttpRequest::CurrentRequest();
  PR_ASSERT(hrq);
  hrq->IncrementRecursionDepth();
}

NSAPI_PUBLIC void 
DecrementRecursionDepth()
{
  HttpRequest *hrq = HttpRequest::CurrentRequest();
  PR_ASSERT(hrq);
  hrq->DecrementRecursionDepth();
}
