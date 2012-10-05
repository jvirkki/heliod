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
 * ntrans.c: Functions to be used with the NameTrans directive.
 * 
 * Rob McCool
 */


#include "netsite.h"
#include "safs/ntrans.h"
#include "safs/var.h"
#include "safs/dbtsafs.h"
#include "base/util.h"
#include "base/pool.h"
#include "base/shexp.h"
#include "base/session.h"
#include "base/vs.h"
#include "frame/req.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/http_ext.h"
#include "frame/httpdir.h"
#include "frame/object.h"
#include "frame/conf.h"
#include "httpdaemon/vsconf.h"

#include <sys/stat.h>

#ifdef XP_UNIX
#include <pwd.h>

#define PATH_SIZE 128

/* -------------------------- ntrans_uhome_init --------------------------- */


#include "base/crit.h"

static pblock *pwpb = NULL;
static CRITICAL pwcrit = NULL;
extern "C" void ntrans_init_crits(void)
{
    pwcrit = crit_init();
}

int _passwd_scan(char *fn, char *user, char *l)
{
    SYS_FILE fd;
    filebuf_t *fb;
    char *t, *u, *dir, *uid;
    int eof, ln;

    if((fd = system_fopenRO(fn)) == SYS_ERROR_FD) {
        strcpy(l, system_errmsg());
        return -1;
    }
    if(!(fb = filebuf_open(fd, FILE_BUFFERSIZE))) {
        strcpy(l, system_errmsg());
        return -1;
    }
    for(eof = 0, ln = 0; (!eof); ) {
        if( (eof = util_getline(fb, ln, 256, l)) == -1)
            return -1;

        if(!(t = strchr(l, ':')))
            continue;
        *t++ = '\0';
        if(user && strcmp(user, l))
            continue;
        if(!(t = strchr(t, ':')))
            continue;
        ++t;
        if(!(u = strchr(t, ':')))
            continue;
        *u = '\0';
        uid = t;
        if(!(t = strchr(u+1, ':')))
            continue;
        if(!(t = strchr(t+1, ':')))
            continue;
        dir = ++t;
        if(!(u = strchr(t, ':')))
            continue;
        *u = '\0';
        if(user) {
            filebuf_close(fb);
            util_sprintf(l, "%s:%s", uid, dir); /* overlap should be safe */
            return 1;
        }
        else {
            char l2[256];
            util_sprintf(l2, "%s:%s", uid, dir);
            pblock_nvinsert(l, l2, pwpb);
        }
    }
    filebuf_close(fb);

    return 0;
}


void ntrans_uhome_terminate(void *data)
{
    pblock_free(pwpb);
    pwpb = NULL;
}


int ntrans_uhome_init(pblock *pb, Session *sn, Request *rq)
{
    char *fn = pblock_findval("pwfile", pb), l[256];

    if(!pwpb) {
        pwpb = pblock_create(27);
#if 0
        magnus_atrestart(ntrans_uhome_terminate, NULL);
#endif
    }
    if(fn) {
        if(_passwd_scan(fn, NULL, l)) {
            pblock_nvinsert("errors", l, pb);
            return REQ_ABORTED;
        }
        else
            return REQ_PROCEED;
    }
    else {
        struct passwd *pw;
	int i=0;

        setpwent();
        while( (pw = getpwent()) ) {
            util_sprintf(l, "%d:%s", pw->pw_uid, pw->pw_dir);
            pblock_nvinsert(pw->pw_name, l, pwpb);
	    i++;
        }
        endpwent();
	log_ereport(LOG_VERBOSE, " number of passwords read: %d",i);
    }
    return REQ_PROCEED;
}


char *_name2dir(char *name, char *fn, char *uid)
{
    struct passwd *pw, pw_ret;
    char *t, l[256];
    int ret;

    if(pwpb) {
        if(!(t = pblock_findval(name, pwpb)))
            return NULL;
	char *c = strchr(t, ':');
	memcpy(uid, t, c - t);
	uid[c - t] = 0;
	return STRDUP(c + 1);
    }
    else if(fn) {
        crit_enter(pwcrit);
        ret = _passwd_scan(fn, name, l);
        crit_exit(pwcrit);
        switch(ret) {
        case -1:
            log_error(LOG_WARN, "unix-home", NULL, NULL, 
                      XP_GetAdminStr(DBT_ntransError1), fn, l);
        default: /* 0: no match */
            return NULL;
        case 1:
	    t = strchr(l, ':');
	    memcpy(uid, l, t - l);
	    uid[t - l] = 0;
	    return STRDUP(t + 1);
        }
    }
    else {
        char pwline[DEF_PWBUF];
        pw = util_getpwnam(name, &pw_ret, pwline, DEF_PWBUF);
        if (!pw)
            return NULL;
        util_itoa(pw->pw_uid, uid);
        return STRDUP(pw->pw_dir);
    }
}


/* --------------------------- ntrans_unix_home --------------------------- */


int ntrans_unix_home(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;

    pb_param *path = pblock_find("ppath", rq->vars);
    if(!path)
        return REQ_NOACTION;

    char *from = pblock_findval("from", pb);
    if (!from) {
        log_error(LOG_MISCONFIG, "unix-home", sn, rq,
                  XP_GetAdminStr(DBT_ntransError2));
        return REQ_ABORTED;
    }

    int fl = strlen(from);
    if(strncmp(path->value, from, fl))
        return REQ_NOACTION;

    char *t = &path->value[fl];
    char *u = t;
    while(*u && (*u != '/'))
        ++u;
    int c = *u;
    *u = 0;

    char *dir;
    char uid[16];
    if((!*t) || (!(dir = _name2dir(t, pblock_findval("pwfile", pb), uid)))) {
        log_error(LOG_WARN, "unix-home", sn, rq,
                  XP_GetAdminStr(DBT_ntransError3), t);
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        return REQ_ABORTED;
    }
    pblock_nvinsert("user", t, rq->vars);
    pblock_nvinsert("uid", uid, rq->vars);
    *u = c;

    char *new_path = pblock_findval("subdir", pb);

    int dirlen = strlen(dir);
    int ulen = strlen(u);
    if (new_path) {
        /* new_dir = dir + '/' + new_path */
        int nplenp1 = strlen(new_path) + 1;
        char* new_dir = (char *) MALLOC(dirlen + nplenp1 + 1);
        memcpy(new_dir, dir, dirlen);
        new_dir[dirlen] = '/';
        memcpy(&new_dir[dirlen + 1], new_path, nplenp1);
        /* free dir and set dir and dirlen to new value */
        FREE(dir);
        dir = new_dir;
        dirlen += nplenp1; /* 1 extra character for '/' */
    }
    int rv = request_set_path(sn, rq, dir, dirlen, u, ulen);
    FREE(dir);
    if (rv == REQ_PROCEED) {
        const char *name = pblock_findkeyval(pb_key_name, pb);
        if (name) {
            pblock_kvinsert(pb_key_name, name, strlen(name), rq->vars);
        }
    }

    return rv;
}

#endif /* XP_UNIX */


/* ---------------------------- ntrans_pfx2dir ---------------------------- */


int ntrans_pfx2dir(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;

    pb_param *pp = pblock_findkey(pb_key_ppath, rq->vars);
    if(!pp) {
        return REQ_NOACTION;
    }

    char *from = pblock_findkeyval(pb_key_from, pb);
    char *dir = pblock_findkeyval(pb_key_dir, pb);
    if((!from) || (!dir)) {
        log_error(LOG_MISCONFIG, "pfx2dir", sn, rq, 
                  XP_GetAdminStr(DBT_ntransError4));
        return REQ_ABORTED;
    }

    int fl = strlen(from);
    char *u = pp->value;

    /* The second or clause is to prevent partials from shadowing.
       That is, if you have a cgi-bin directory and cgi-bin-sucks.html, 
       cgi-bin-sucks.html is not hidden by the mapping. Side effect is that 
       trailing slashes on from or dir causes problems.
    */
#ifdef XP_WIN32
    if((strncasecmp(from, u, fl)) || (u[fl] && (u[fl] != '/')))
#else
    if((strncmp(from, u, fl)) || (u[fl] && (u[fl] != '/')))
#endif
    {
        return REQ_NOACTION;
    }

    int ulen = strlen(u);
    int dirlen = strlen(dir);

    int rv = request_set_path(sn, rq, dir, dirlen, u + fl, ulen - fl);
    if (rv == REQ_PROCEED) {
        const char *name = pblock_findkeyval(pb_key_name, pb);
        if (name) {
            pblock_kvinsert(pb_key_name, name, strlen(name), rq->vars);
            const char *forward = pblock_findkeyval(pb_key_find_pathinfo_forward, pb);
            if (forward)
                pblock_kvinsert(pb_key_find_pathinfo_forward, forward, strlen(forward), rq->vars);
        }
    }

    return rv;
}


/* ------------------------ trim_trailing_slashes ------------------------- */


static void trim_trailing_slashes(const char *fn, Session *sn, Request *rq, char *root, int rootlen)
{
    int i;

    for (i = rootlen - 1; i >= 0 && (root[i] == '/' || root[i] == '\0'); i--)
        root[i] = '\0';

    if (i < rootlen - 1) {
        log_error(LOG_MISCONFIG, fn, sn, rq,
                  XP_GetAdminStr(DBT_ntransWarning1),
                  root, vs_get_id(request_get_vs(rq)));
    }
}


/* ---------------------------- ntrans_docroot ---------------------------- */


int ntrans_docroot(pblock *pb, Session *sn, Request *rq)
{
    const VirtualServer *vs = request_get_vs(rq);

    rq->directive_is_cacheable = 1;

    pb_param *path = pblock_findkey(pb_key_ppath, rq->vars);
    if(!path) {
        return REQ_NOACTION;
    }

    if(path->value[0] != '/') {
        if(path->value[0] == '*' && path->value[1] == '\0')
            return REQ_NOACTION;

        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        return REQ_ABORTED;
    }

    char *dr = pblock_findkeyval(pb_key_root, pb);
    if(!dr) {
        log_error(LOG_MISCONFIG, "document-root", sn, rq, 
                  XP_GetAdminStr(DBT_ntransError5));
        return REQ_ABORTED;
    }

    int drlen = strlen(dr);
    trim_trailing_slashes("document-root", sn, rq, dr, drlen);

    return request_set_path(sn, rq, dr, drlen, path->value, strlen(path->value));
}


/* ------------------------------ user2name ------------------------------- */


int ntrans_user2name(pblock *pb, Session *sn, Request *rq)
{
    pb_param *user = pblock_find("user", rq->vars);
    pb_param *name = pblock_remove("name", rq->vars);

    rq->directive_is_cacheable = 1;

    if(name) {
        FREE(user->name);
        user->name = name->name;
        FREE(name->value);
        FREE(name);
    }
    else {
        FREE(user->name);
        user->name = STRDUP("name");
    }
    return REQ_PROCEED;
}


/* ------------------------------- redirect ------------------------------- */


static PRBool _redirect_loop(Session *sn, Request *rq, const char *urlpfx, const char *urlsfx)
{
    PRBool loop = PR_FALSE;

    if (const char *uri = pblock_findkeyval(pb_key_uri, rq->reqpb)) {
        char *ou = http_uri2url_dynamic(uri, "", sn, rq);
        char *nu = http_uri2url_dynamic(urlpfx, urlsfx ? urlsfx : "", sn, rq);

        loop = !strcmp(ou, nu);

        pool_free(sn->pool, ou);
        pool_free(sn->pool, nu);
    }

    return loop;
}

static int _redirect(pblock *pb, Session *sn, Request *rq, const char *urlpfx, const char *urlsfx)
{
    // Catch and avoid some of the simplest types of redirect loops
    if (_redirect_loop(sn, rq, urlpfx, urlsfx)) {
        log_error(LOG_VERBOSE, pblock_findkeyval(pb_key_fn, pb), sn, rq,
                  "Not redirecting to %s%s (Possible redirect loop detected)",
                  urlpfx, urlsfx ? urlsfx : "");
        return REQ_NOACTION;
    }

    int rv;

    const char *directive = pblock_findkeyval(pb_key_Directive, pb);
    if (directive && !strcmp(directive, directive_num2name(NSAPIError))) {
        // We're processing an Error directive
        rv = REQ_PROCEED;
    } else {
        // We'll tell the server to start processing Error directives
        rv = REQ_ABORTED;
    }

    if (!urlsfx) {
        pblock_kvinsert(pb_key_location, urlpfx, strlen(urlpfx), rq->srvhdrs);
    } else {
        pb_param *pp = pblock_key_param_create(rq->srvhdrs, pb_key_location, NULL, 0);
        int urlpfxlen = strlen(urlpfx);
        int urlsfxlen = urlsfx ? strlen(urlsfx) : 0;
        const char *query = pblock_findkeyval(pb_key_query, rq->reqpb);
        if (query) {
            int querylen = strlen(query);
            char *v = (char*)MALLOC(urlpfxlen + urlsfxlen + querylen + 2);
            memcpy(v, urlpfx, urlpfxlen);
            memcpy(&v[urlpfxlen], urlsfx, urlsfxlen);
            v[urlpfxlen + urlsfxlen] = '?';
            memcpy(&v[urlpfxlen + urlsfxlen + 1], query, querylen + 1);
            pp->value = v;
        } else {
            char *v = (char*)MALLOC(urlpfxlen + urlsfxlen + 1);
            memcpy(v, urlpfx, urlpfxlen);
            memcpy(&v[urlpfxlen], urlsfx, urlsfxlen + 1);
            pp->value = v;
        }
        pblock_kpinsert(pb_key_location, pp, rq->srvhdrs);
    }

    int code = PROTOCOL_REDIRECT;
    char *reason = NULL;

    if (char *status = pblock_findkeyval(pb_key_status, pb)) {
        int ul = strtoul(status, &reason, 0);
        if (ul != 0)
            code = ul;
        if (*reason == ' ')
            reason++;
        if (*reason == '\0')
            reason = NULL;
    }

    protocol_status(sn, rq, code, reason);

    return rv;
}

int ntrans_redirect(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;

    const char *path = pblock_findkeyval(pb_key_ppath, rq->vars);
    if (!path) {
        path = pblock_findkeyval(pb_key_uri, rq->reqpb);
        if (!path)
            path = "";
    }

    const char *pfx = pblock_findkeyval(pb_key_from, pb);
    if (pfx) {
        int l = strlen(pfx);

#ifdef XP_WIN32
        if (strncasecmp(path, pfx, l))
#else
        if (strncmp(path, pfx, l))
#endif
            return REQ_NOACTION;

        path += l;
    }

    const char *upfx;
    const char *usfx;

    if (const char *p = pblock_findkeyval(pb_key_url, pb)) {
        upfx = p;
        usfx = NULL;
    } else if (const char *p = pblock_findkeyval(pb_key_url_prefix, pb)) {
        upfx = p;
        usfx = util_uri_escape(NULL, path);
    } else {
        log_error(LOG_MISCONFIG, "redirect", sn, rq,
                  XP_GetAdminStr(DBT_httpNeedUrlOrUrlPrefix));
        return REQ_ABORTED;
    }

    /*
     * Check if we need to escape the portion of the URL supplied by the
     * administrator.  Note that we always fully escape any portion of the URL
     * obtained from "ppath" or "uri".
     */
    if (util_getboolean(pblock_findkeyval(pb_key_escape, pb), PR_TRUE))
        upfx = util_url_escape(NULL, upfx);

    return _redirect(pb, sn, rq, upfx, usfx);
}


/* --------------------------- mozilla-redirect --------------------------- */


int ntrans_mozilla_redirect(pblock *pb, Session *sn, Request *rq)
{
    char *ppath = pblock_findval("ppath", rq->vars);
    char *from = pblock_findval("from", pb);
    char *url = pblock_findval("url", pb);
    char *ua;

    if((!from) || (!url)) {
        log_error(LOG_MISCONFIG, "mozilla-redirect", sn, rq,
                  XP_GetAdminStr(DBT_ntransError6));
        return REQ_ABORTED;
    }

    if(WILDPAT_CMP(ppath, from)) {
        rq->directive_is_cacheable = 1;
        return REQ_NOACTION;
    }

    if(request_header("user-agent", &ua, sn, rq) == REQ_ABORTED) {
        return REQ_ABORTED;
    }

    if(util_is_mozilla(ua, "0", "96")) {
        protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
        pblock_nvinsert("url", url, rq->vars);
        return REQ_ABORTED;            
    }

    return REQ_NOACTION;
}


/* ------------------------------- homepage ------------------------------- */


int ntrans_homepage(pblock *pb, Session *sn, Request *rq)
{
    pb_param *pp = pblock_find("ppath", rq->vars);

    rq->directive_is_cacheable = 1;

    if (pp->value[0] != '/' || pp->value[1] != 0)
        return REQ_NOACTION;

    char *hp = pblock_findval("path", pb);
    if (!hp) {
        log_error(LOG_MISCONFIG, "homepage", sn, rq, 
                  XP_GetAdminStr(DBT_missingPathParameter));
        return REQ_ABORTED;
    }

    /* Check if this is an absolute path or a relative path */
#ifdef XP_WIN32
    if ((hp[0] == '/' || hp[0] == '\\') ||
        (hp[0] != '\0' && hp[1] == ':')) {
#else
    if(hp[0] == '/') {
#endif
        FREE(pp->value);
        pp->value = STRDUP(hp);
        char *filename = strrchr(hp, '/');
        if (filename > hp)
            pblock_kvinsert(pb_key_ntrans_base, hp, filename - hp, rq->vars);
        return REQ_PROCEED;
    }
    pp->value = (char *) REALLOC(pp->value, 1 + strlen(hp) + 1);
    strcpy(&pp->value[1], hp);
    return REQ_NOACTION;
}


/* ---------------------------- strip-params ------------------------------ */


int ntrans_strip_params(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;

    // As per RFC2396, URI path segments can contain parameters beginning with
    // ';'.  These parameters must be removed from the ppath.  Bug 418271
    char *ppath = pblock_findkeyval(pb_key_ppath, rq->vars);
    if (ppath) {
        util_uri_strip_params(ppath);
    }
    return REQ_NOACTION;
}


/* ----------------------------- assign-name ------------------------------ */


int ntrans_assign_name(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;

    /* Don't override any names previously assigned */
    if(pblock_findkey(pb_key_name, rq->vars)) {
        return REQ_NOACTION;
    }

    char *path = pblock_findkeyval(pb_key_ppath, rq->vars);
    char *from = pblock_findkeyval(pb_key_from, pb);
    char *name = pblock_findkeyval(pb_key_name, pb);

    if(!name) {
        log_error(LOG_MISCONFIG, "assign-name", sn, rq, 
                  XP_GetAdminStr(DBT_ntransError7));
        return REQ_ABORTED;
    }
    if(!from || !WILDPAT_CMP(path, from))  {
        pblock_kvinsert(pb_key_name, name, strlen(name), rq->vars);
        char *nostat = pblock_findkeyval(pb_key_nostat, pb);
        if (nostat) { 
            if (nostat[0] == '/' && nostat[1] != '\0')
                pblock_kvinsert(pb_key_nostat, nostat, strlen(nostat), rq->vars);
        }
        char *forward = pblock_findkeyval(pb_key_find_pathinfo_forward, pb);
        if (forward)
            pblock_kvinsert(pb_key_find_pathinfo_forward, forward, strlen(forward), rq->vars);
        if (pblock_findval("stop", pb))
            return REQ_PROCEED;
    }

    return REQ_NOACTION;
}


/* ----------------------------- set-variable ----------------------------- */


enum CHANGE_VARIABLE { INSERT_VARIABLE, SET_VARIABLE, REMOVE_VARIABLE };

enum SYNTAX_STATUS { SYNTAX_ERROR = -1, SYNTAX_UNKNOWN = 0, SYNTAX_OK = 1, SYNTAX_IGNORED = 2 };

static SYNTAX_STATUS _change_pblock_variable(Session *sn, Request *rq, CHANGE_VARIABLE change, const char *pbname, const char *nv)
{
    SYNTAX_STATUS syntax = SYNTAX_ERROR;

    char *nvcopy = pool_strdup(sn->pool, nv);

    // Get the pblock, name, and value
    pblock *pb;
    const char *name;
    const char *value;
    if (var_get_pblock_name_value(sn, rq, pbname, nvcopy, &pb, &name, &value) == PR_SUCCESS) {
        // Apply the change
        switch (change) {
        case INSERT_VARIABLE:
            pblock_nvinsert(name, value ? value : "", pb);
            break;

        case SET_VARIABLE:
            pblock_nvreplace(name, value ? value : "", pb);
            break;

        case REMOVE_VARIABLE:
            if (value) {
                // Remove name only if its current value matches value
                for (;;) {
                    char *current = pblock_findval(name, pb);
                    if (!current || WILDPAT_CMP(current, (char*)value))
                        break;
                    param_free(pblock_remove(name, pb));
                }
            } else {
                // Remove the current value of name
                param_free(pblock_remove(name, pb));
            }
            break;
        }

        syntax = SYNTAX_OK;
    }

    pool_free(sn->pool, nvcopy);

    return syntax;
}

static SYNTAX_STATUS _http_crossgrade(const char *name, const char *value, Request *rq)
{
    PRBool downgrade = !strcmp(name, "http-downgrade");
    PRBool upgrade = !downgrade && !strcmp(name, "http-upgrade");

    if (!downgrade && !upgrade)
        return SYNTAX_ERROR;

    // Get the canonical version number and string from value
    char protocol[80];
    int protv_num;
    if (!util_format_http_version(value, &protv_num, protocol, sizeof(protocol)))
        return SYNTAX_ERROR;

    // Downgrade/upgrade as appropriate
    if (downgrade && (rq->protv_num > protv_num) ||
        upgrade && (rq->protv_num < protv_num))
    {
        rq->protv_num = protv_num;
    }
    pblock_nvreplace("protocol", protocol, rq->vars);

    return SYNTAX_OK;
}

static int _set_variable(pblock *pb, Session *sn, Request *rq, const pb_key * const *ignorev)
{
    PRBool flagExplicitReturnValue = PR_FALSE;
    int ret = REQ_NOACTION;

    int i;
    for (i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        ignored:
        while (p) {
            SYNTAX_STATUS syntax = SYNTAX_UNKNOWN;
            const pb_key *k = param_key(p->param);
            const char *n = p->param->name;
            const char *v = p->param->value;
            p = p->next;

            // Ignore any name that appears in ignorev[]
            if (ignorev) {
                for (int ignore = 0; ignorev[ignore]; ignore++) {
                    if (k == ignorev[ignore])
                        goto ignored;
                }
            }

            switch (*n) {
            case '$': // Assignment to $variable
                if (var_set_variable(sn, rq, n + 1, v) == PR_SUCCESS) {
                    syntax = SYNTAX_OK;
                } else {
                    syntax = SYNTAX_ERROR;
                }
                break;

            case 'D': // Directive
                if (!strcmp(n, "Directive")) {
                    syntax = SYNTAX_IGNORED;
                }
                break;

            case 'm': // method
                if (!strcmp(n, "method")) {
                    syntax = SYNTAX_IGNORED;
                }
                break;

            case 't': // type
                if (!strcmp(n, "type")) {
                    syntax = SYNTAX_IGNORED;
                }
                break;

            case 'q': // query
                if (!strcmp(n, "query")) {
                    syntax = SYNTAX_IGNORED;
                }
                break;

            case 'c': // code
                if (!strcmp(n, "code")) {
                    syntax = SYNTAX_IGNORED;
                }
                break;

            case 'f': // fn, find-pathinfo-forward
                if (!strcmp(n, "fn")) {
                    syntax = SYNTAX_IGNORED;
                } else if (!strcmp(n, "find-pathinfo-forward")) {
                    pblock_kvinsert(pb_key_find_pathinfo_forward, v, strlen(v), rq->vars);
                    syntax = SYNTAX_OK;
                }
                break;

            case 'n': // name, nostat, noaction
                if (!strcmp(n, "name")) {
                    pblock_kvinsert(pb_key_name, v, strlen(v), rq->vars);
                    syntax = SYNTAX_OK;
                } else if (!strcmp(n, "nostat")) {
                    if (v[0] == '/' && v[1] != '\0') {
                        pblock_kvinsert(pb_key_nostat, v, strlen(v), rq->vars);
                        syntax = SYNTAX_OK;
                    }
                } else if (!strcmp(n, "noaction")) {
                    if (!*v || util_getboolean(v, PR_FALSE)) {
                        ret = REQ_NOACTION;
                        flagExplicitReturnValue = PR_TRUE;
                        syntax = SYNTAX_OK;
                    }
                }
                break;

            case 'i': // insert, insert-*
                if (!strcmp(n, "insert")) {
                    syntax = _change_pblock_variable(sn, rq, INSERT_VARIABLE, NULL, v);
                } else if (!strncmp(n, "insert-", sizeof("insert-")-1)) {
                    syntax = _change_pblock_variable(sn, rq, INSERT_VARIABLE, n + sizeof("insert-")-1, v);
                }
                break;

            case 'r': // remove, remove-*, reason
                if (!strcmp(n, "remove")) {
                    syntax = _change_pblock_variable(sn, rq, REMOVE_VARIABLE, NULL, v);
                } else if (!strncmp(n, "remove-", sizeof("remove-")-1)) {
                    syntax = _change_pblock_variable(sn, rq, REMOVE_VARIABLE, n + sizeof("remove-")-1, v);
                } else if (!strcmp(n, "reason")) {
                    syntax = SYNTAX_IGNORED;
                }
                break;

            case 'k': // keep-alive
                if (!strcmp(n, "keep-alive")) {
                    KEEP_ALIVE(rq) &= util_getboolean(v, PR_TRUE);
                    syntax = SYNTAX_OK;
                }
                break;

            case 'h': // http-downgrade, http-upgrade
                syntax = _http_crossgrade(n, v, rq);
                break;

            case 'a': // abort
                if (!strcmp(n, "abort")) {
                    if (!*v || util_getboolean(v, PR_FALSE)) {
                        ret = REQ_ABORTED;
                        flagExplicitReturnValue = PR_TRUE;
                        syntax = SYNTAX_OK;
                    }
                }
                break;

            case 'e': // exit, error, escape
                if (!strcmp(n, "exit")) {
                    if (!*v || util_getboolean(v, PR_FALSE)) {
                        ret = REQ_EXIT;
                        flagExplicitReturnValue = PR_TRUE;
                        syntax = SYNTAX_OK;
                    }
                } else if (!strcmp(n, "error")) {
                    int status = atoi(v);
                    if (status) {
                        const char *reason = strchr(v, ' ');
                        if (reason) reason++;
                        protocol_status(sn, rq, status, reason);
                        if (!flagExplicitReturnValue)
                            ret = REQ_ABORTED;
                        syntax = SYNTAX_OK;
                    }
                }
                break;

            case 's': // set, set-*, stop, ssl-unclean-shutdown, senthdrs
                if (!strcmp(n, "set")) {
                    syntax = _change_pblock_variable(sn, rq, SET_VARIABLE, NULL, v);
                } else if (!strncmp(n, "set-", sizeof("set-")-1)) {
                    syntax = _change_pblock_variable(sn, rq, SET_VARIABLE, n + sizeof("set-")-1, v);
                } else if (!strcmp(n, "stop")) {
                    if (!*v || util_getboolean(v, PR_FALSE)) {
                        ret = REQ_PROCEED;
                        flagExplicitReturnValue = PR_TRUE;
                        syntax = SYNTAX_OK;
                    }
                } else if (!strcmp(n, "ssl-unclean-shutdown")) {
                    if (!*v || util_getboolean(v, PR_FALSE)) {
                        SSL_UNCLEAN_SHUTDOWN(rq) = PR_TRUE;
                        syntax = SYNTAX_OK;
                    }
                }
                break;

            case 'u': // url
                if (!strcmp(n, "url")) {
                    ret = _redirect(pb, sn, rq, v, NULL);
                    syntax = SYNTAX_OK;
                }
                break;
            }

            if (syntax == SYNTAX_UNKNOWN) {
                if (var_set_internal_variable(sn, rq, n, v) == PR_SUCCESS) {
                    syntax = SYNTAX_OK;
                } else {
                    syntax = SYNTAX_ERROR;
                }
            }

            if (syntax == SYNTAX_ERROR) {
                log_error(LOG_MISCONFIG, pblock_findkeyval(pb_key_fn, pb), sn, rq, XP_GetAdminStr(DBT_InvalidExpressionNameValue), n, v);
                return REQ_ABORTED;
            }
        }
    }

    return ret;
}

int ntrans_set_variable(pblock *pb, Session *sn, Request *rq)
{
    return _set_variable(pb, sn, rq, NULL);
}


/* ----------------- ntrans_match_browser_init_directive ------------------ */


static int ntrans_match_browser_init_directive(const directive *dir, VirtualServer *incoming, const VirtualServer *current)
{
    // XXX The default obj.conf file contains an AuthTrans fn="match-browser"
    // browser="*MSIE*" ssl-unclean-shutdown="true" directive.  The
    // accelerator cache knows how to cache these directives, but we need to
    // tell it whenever we see one.

    int browser = 0;
    int ssl_unclean_shutdown = 0;
    int ignored = 0;
    int unknown = 0;

    for (int i = 0; i < dir->param.pb->hsize; i++) {
        for (pb_entry *p = dir->param.pb->ht[i]; p; p = p->next) {
            const pb_key *key = param_key(p->param);
            if (key == pb_key_browser) {
                browser++;
            } else if (key == pb_key_ssl_unclean_shutdown && !strcmp(p->param->value, "true")) {
                ssl_unclean_shutdown++;
            } else if (key == pb_key_Directive || key == pb_key_fn) {
                ignored++;
            } else {
                unknown++;
            }
        }
    }

    if (browser == 1 && ssl_unclean_shutdown > 0 && unknown == 0) {
        // This is a browser="..." ssl-unclean-shutdown="true" directive
        pblock_kvinsert(pb_key_magnus_internal, "1", 1, dir->param.pb);
    }

    return REQ_PROCEED;
}


/* ---------------------------- match-browser ----------------------------- */


int ntrans_match_browser(pblock *pb, Session *sn, Request *rq)
{
    // Bail if another match-browser directive already matched
    if (pblock_findkeyval(pb_key_matched_browser, rq->vars)) {
        return REQ_NOACTION;
    }

    // XXX The accelerator cache knows how to cache browser="..."
    // ssl-unclean-shutdown="true"
    if (rq->request_is_cacheable) {
        if (pblock_findkey(pb_key_magnus_internal, pb)) {
            NSAPIRequest *nrq = (NSAPIRequest *) rq;
            if (nrq->accel_ssl_unclean_shutdown_browser == NULL) {
                const char *browser = pblock_findkeyval(pb_key_browser, pb);
                nrq->accel_ssl_unclean_shutdown_browser = pool_strdup(sn->pool, browser);
                rq->directive_is_cacheable = 1;
            }
        }
    }

    // Get the client's user-agent header
    const char *agent = pblock_findkeyval(pb_key_user_agent, rq->headers);
    if (!agent)
        agent = "";

    // Iterate over the pblock looking for browser="agent"
    int i;
    for (i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        while (p) {
            if (param_key(p->param) == pb_key_browser) {
                // Compare client's user-agent against the browser pattern
                if (!WILDPAT_CMP(agent, p->param->value)) {
                    // Apply the variables
                    pblock_kvinsert(pb_key_matched_browser, "1", 1, rq->vars);
                    const pb_key *ignorev[3] = { pb_key_browser, pb_key_magnus_internal, NULL };
                    return _set_variable(pb, sn, rq, ignorev);
                }
            }
            p = p->next;
        }
    }

    return REQ_NOACTION;
}


/* ------------------------------- rewrite -------------------------------- */


int ntrans_rewrite(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;

    pb_param *pp = pblock_findkey(pb_key_ppath, rq->vars);
    if (!pp || !pp->value)
        return REQ_ABORTED;

    const char *from = pblock_findkeyval(pb_key_from, pb);
    if (from && WILDPAT_CMP(pp->value, from))
        return REQ_NOACTION;

    int rv = REQ_NOACTION;

    const char *path = pblock_findkeyval(pb_key_path, pb);
    char *root = pblock_findkeyval(pb_key_root, pb);
    if (root) {
        int rootlen = strlen(root);
        trim_trailing_slashes("rewrite", sn, rq, root, rootlen);
        const char *ppath = path ? path : pp->value;
        log_error(LOG_FINER, "rewrite", sn, rq,
                  "Setting absolute path to %s%s",
                  root, ppath);
        rv = request_set_path(sn, rq, root, rootlen, ppath, strlen(ppath));
        if (rv != REQ_PROCEED)
            return rv;
    } else if (path) {
        log_error(LOG_FINER, "rewrite", sn, rq,
                  "Setting partial path to %s",
                  path);
        pool_free(sn->pool, pp->value);
        pp->value = pool_strdup(sn->pool, path);
    }

    const char *name = pblock_findkeyval(pb_key_name, pb);
    if (name)
        pblock_kvinsert(pb_key_name, name, strlen(name), rq->vars);

    return rv;
}


/* ------------------------------- restart -------------------------------- */


int ntrans_restart(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;

    const char *from = pblock_findkeyval(pb_key_from, pb);
    if (from) {
        const char *path = pblock_findkeyval(pb_key_ppath, rq->vars);
        if (!path)
            return REQ_ABORTED;
        if (WILDPAT_CMP(path, from))
            return REQ_NOACTION;
    }

    const char *uri = pblock_findkeyval(pb_key_uri, pb);
    if (!uri) {
        log_error(LOG_MISCONFIG, "restart", sn, rq,
                  XP_GetAdminStr(DBT_httpNeedUri));
        return REQ_ABORTED;
    }

    return request_restart_location(sn, rq, uri);
}


/* ----------------------------- ntrans_init ------------------------------ */


void ntrans_init(void)
{
    // Call ntrans_match_browser_init_directive() to check whether each
    // match-browser directive can be cached by the accelerator cache
    vs_directive_register_cb(ntrans_match_browser, ntrans_match_browser_init_directive, 0);
}
