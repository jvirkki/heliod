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
 * pcheck.c: PathCheck functions
 * 
 * Rob McCool
 */


#include "netsite.h"

#ifdef NS_OLDES3X
#include "sslio/sslio.h"
#else
#include "ssl.h"
#define SSL_INVALIDATESESSION SSL_InvalidateSession
#endif /* NS_OLDES3X */

#include "safs/pcheck.h"
#include "safs/dbtsafs.h"
#include "base/shexp.h"
#include "base/session.h"
#include "frame/req.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/http_ext.h"
#include "frame/httpdir.h"
#include "frame/object.h"
#include "base/util.h"  /* util_uri_is_evil */
#include "base/language.h"

#include "base/pblock.h"
#include "base/pool.h"
#include "frame/protocol.h"
#include "frame/conf.h"
#include "httpdaemon/vsconf.h"
#include "support/NSString.h"
#include "NsprWrap/NsprError.h"

#include <sys/stat.h>
#include <ctype.h>


/* --------------------------- pcheck_find_path --------------------------- */


/*
 * Takes a given path, and figures out if there is any path_info attached.
 * If no explicit path_info was provided by nametrans, and the file doesn't 
 * exist as specified, it tries to find it by groping through the filesystem.
 * 
 * This type of implicit path_info cannot be attached to directories. Such
 * a request will be flagged as not found.
 * 
 * pb unused.
 */

static char* find_param(char *uri, char *param)
{
    // Find ;parameters on the end of uri
    if (param > uri) {
        if (*param == FILE_PATHSEP) {
            --param;
        }
        while (param > uri && *param != FILE_PATHSEP && *param != ';') {
            --param;
        }
        if (*param == ';') {
            return param;
        }
    }

    return NULL;
}

static PRBool set_path_info(Request *rq, char *uri, int path_info_depth)
{
    // Find trailing path components in uri, e.g. the "/baz;qux/quux" in
    // "/cgi-bin/foo.pl;bar/baz;qux/quux" (here path_info_depth would be 2)
    char *path_info = &uri[strlen(uri)];
    while (path_info > uri && path_info_depth) {
        --path_info;
        if (*path_info == FILE_PATHSEP) {
            --path_info_depth;
        }
    }

    if (*path_info) {
        pblock_nvinsert("path-info", path_info, rq->vars);
        return PR_TRUE;
    }

    return PR_FALSE;
}

int pcheck_find_path(pblock *pb, Session *sn, Request *rq) 
{
    char *path = pblock_findkeyval (pb_key_path, rq -> vars);
    char *path_info   = pblock_findkeyval (pb_key_path_info  , rq -> vars);
    char *script_name = pblock_findkeyval (pb_key_script_name, rq -> vars);
    NSFCFileInfo *finfo = NULL;

    rq->directive_is_cacheable = 1;

    if (path_info != NULL || script_name != NULL || path == NULL)
	return REQ_NOACTION;// ruslan: bail out right away for performance (no need to go into file cache)

    if (!*path)
        return REQ_NOACTION;

    if ((INTrequest_info_path(path, rq, NULL) == PR_SUCCESS))
        return REQ_NOACTION;

    rq->directive_is_cacheable = 0;

    path_info = &path[strlen(path) - 1];

    char *forward = pblock_findkeyval (pb_key_find_pathinfo_forward, rq -> vars);
    char *base = NULL;
    if (forward) {
        base   = pblock_findval("ntrans-base"  , rq -> vars);
        if (!base) forward = NULL;
    }

    int path_info_depth = 0;

    if (forward) 
        goto find_forward;

    while (1) {
        /* Change all occurrences of '/' to FILE_PATHSEP for WIN32 */
        for(  ; path_info != path; --path_info)
            if (*path_info == FILE_PATHSEP)
                break;
        for(  ; path_info != path; --path_info) {
            ++path_info_depth;
            if (*(path_info - 1) != FILE_PATHSEP)
                break;
        }

        if (path_info == path)
            break;

        *path_info = '\0';
        if ((INTrequest_info_path(path, rq, &finfo) == PR_SUCCESS) && finfo) {
            if (finfo->pr.type != PR_FILE_FILE) {
                *path_info = FILE_PATHSEP;
                if (set_path_info(rq, pblock_findkeyval(pb_key_uri, rq->reqpb), 0)) {
                    return REQ_PROCEED;
                }
                break;
            } else {
                set_path_info(rq, pblock_findkeyval(pb_key_uri, rq->reqpb), path_info_depth);
                return REQ_PROCEED;
            }
        }
        else *path_info-- = FILE_PATHSEP;
    }
    /* This was changed to support virtual documents */
    return REQ_NOACTION;

find_forward:

    PR_ASSERT(base);
    int baselen = strlen(base);
    if (strncmp(path, base, baselen))
        return REQ_NOACTION;

    path_info = &path[baselen];
    if (*path_info == '/')
        path_info++;

    while (1) {
        for(  ; *path_info; ++path_info)
            if (*path_info == FILE_PATHSEP)
                break;

        if (!*path_info) {
            if (set_path_info(rq, pblock_findkeyval(pb_key_uri, rq->reqpb), 0)) {
                return REQ_PROCEED;
            }
            break;
        }

        *path_info = '\0';
        if ((INTrequest_info_path(path, rq, &finfo) == PR_SUCCESS) && finfo) {
            if (finfo->pr.type != PR_FILE_FILE) {
                *path_info++ = FILE_PATHSEP;
            } else {
	        char *uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
                *path_info = FILE_PATHSEP;
#ifdef XP_WIN32
                char *unmpath = pblock_findval("unmuri", rq->vars);
                if (unmpath) {
                    char *nm = &path_info[strlen(path_info)-1];
                    char *unm = &unmpath[strlen(unmpath)-1];
                    while(nm != path_info && unm != unmpath) {
                        if (*nm != *unm) {
                            if (*unm == '\\' && *nm == '/')
                                *nm = *unm;
                            else
                                PR_ASSERT(0);
                        }
                        nm--;
                        unm--;
                    }
                    uri = unmpath;
                }
#endif
                char *t = path_info;
                for(; *t; ++t)
                    if (*t == FILE_PATHSEP)
                        ++path_info_depth;
                *path_info = '\0';
                set_path_info(rq, uri, path_info_depth);
                return REQ_PROCEED;
            }
        }
        else {
           *path_info = FILE_PATHSEP;
            break;
        }
    }

    /* This was changed to support virtual documents */
    return REQ_NOACTION;
}



/* ------------------------ pcheck_deny_existence ------------------------- */


int pcheck_deny_existence(pblock *pb, Session *sn, Request *rq)
{
    pb_param *path = pblock_findkey(pb_key_path, rq->vars);
    char *check_path = pblock_findkeyval(pb_key_path, pb);

    rq->directive_is_cacheable = 1;

    if((!check_path) || (!WILDPAT_CMP(path->value, check_path))) {
        char *bong_file = pblock_findval("bong-file", pb);
        log_error(LOG_SECURITY, "deny-existence", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError1),
                  path->value);
        if(bong_file) {
            FREE(path->value);
            path->value = STRDUP(bong_file);
            return REQ_PROCEED;
        }
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        return REQ_ABORTED;
    }
    else
        return REQ_NOACTION;
}


/* -------------------------- pcheck_find_index --------------------------- */


/* Helper: Take dir and append path */

static char *_make_path(char *dir, char *file, char *t)
{
    char *p;

    for(p = t; (*p = *dir); ++p, ++dir);
    if (p > t && p[-1] != FILE_PATHSEP)
        *p++ = FILE_PATHSEP;
    while( (*p = *file) )
        (++p, ++file);
    return t;
}

static char *
_setup_newuri(pool_handle_t* pool, Request* rq, const char *uri, int urilen, char *path, const char *param)
{
    if (!param) param = "";
    int pathlen = strlen(path);
    int paramlen = strlen(param);
    char* luri = (char *) pool_malloc(pool, urilen + pathlen + paramlen + 1);
    memcpy(luri, uri, urilen);
    memcpy(&luri[urilen], path, pathlen);
    memcpy(&luri[urilen + pathlen], param, paramlen + 1);
    return luri;
}

/*
 * This function will first determine if the given path is a directory.
 * If so, it will try to find each of the files in the index-names list
 * in order. If not, it will return REQ_NOACTION.
 * 
 * If the file is found, and there was no trailing slash, this function will
 * force the client to redirect.
 */

int pcheck_find_index(pblock *pb, Session *sn, Request *rq)
{
    pb_param *pp;
    char *path;
    char *flist;
    char *t, *u, *v;
    char *uri;
    NSFCFileInfo *finfo = NULL;
    int lp;
    char *method;
    char *langpath;
    char *qs;

    rq->directive_is_cacheable = 1;

    /* If method is GET, POST  append "index.html" to the uri */
    /* Added HEAD; bug# 368359 */ 
    if (rq->method_num >= 0) {
      if (!ISMGET(rq) && !ISMPOST(rq) && !ISMHEAD(rq))
          return REQ_NOACTION;
    }
    else {
        if ((method = pblock_findkeyval( pb_key_method, rq->reqpb ))) {
	          if (strcmp( method, "GET" ) && 
                strcmp( method, "POST" ) &&
                strcmp(method, "HEAD")) {
	              return REQ_NOACTION;
            }
        }
    }

    pp = pblock_findkey(pb_key_path, rq->vars);
    path = pp->value;

    if (!path)
	return REQ_NOACTION;
 
    lp = strlen(path);

    if ((INTrequest_info_path(path, rq, &finfo) != PR_SUCCESS) ||
        (finfo && (finfo->pr.type != PR_FILE_DIRECTORY)))
        return REQ_NOACTION;

    rq->directive_is_cacheable = 0;

    char *fl = pblock_findval("index-names", pb);
    if(!fl) {
        log_error(LOG_WARN, "find-index", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError2));
        return REQ_ABORTED;
    }

    uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
    char *param = NULL;
    int urilen = strlen(uri);
    if (uri[urilen - 1] != '/') {
        // We have a URI without a trailing slash
        param = find_param(uri, &uri[urilen]);
        if (param > uri && param[-1] == '/') {
            // Drop parameters from the trailing path segment
            urilen = (param - uri);
        } else {
            // Redirect to the URI with a trailing slash
            pb_param *url = pblock_key_param_create(rq->vars, pb_key_url, NULL, 0);
            url->value = protocol_uri2url_dynamic(uri, "/", sn, rq);
            pblock_kpinsert(pb_key_url, url, rq->vars);
            protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
            return REQ_ABORTED;
        }
    }

    flist = pool_strdup(sn->pool, fl);
    t = flist;
    u = flist;

    /* We overallocate v a bit but shouldn't be much */
    v = (char *) pool_malloc(sn->pool, strlen(t) + lp + 1 + 1);

    while(*t) {
        while((*u) && (*u != ',')) ++u;
        if(u == t) {
            /* grumble grumble what a dope */
            t = ++u;
            continue;
        }
        if(*u)
            *u++ = '\0';

        /* additonal language lookup if accept-language feature is on */
        langpath = _make_path(path, t, v);
        const VirtualServer *vs = request_get_vs(rq);
        if (vs->localization.negotiateClientLanguage) {
            char *newpath;
            if (newpath = lang_acceptlang_file(v))
                langpath = newpath;
        }

        finfo = NULL;
        if ((INTrequest_info_path(langpath, rq, &finfo) == PR_SUCCESS) &&
            finfo) {
            if (finfo && finfo->pr.type == PR_FILE_FILE) {
                pool_free(sn->pool, pp->value);
                pp->value = langpath;

                char *luri = _setup_newuri(sn->pool, rq, uri, urilen, t, param);
                char *query = pblock_findkeyval(pb_key_query, rq->reqpb);

                if (v != langpath)
                    pool_free(sn->pool, v);
                pool_free(sn->pool, flist);

                return request_restart(sn, rq, NULL, luri, query);
            }
        }
        t = u;
    }
    pool_free(sn->pool, v);
    pool_free(sn->pool, flist);
    return REQ_NOACTION;
}


/* --------------------------- pcheck_uri_clean --------------------------- */


int pcheck_uri_clean(pblock *pb, Session *sn, Request *rq)
{ 
    static int tilde_ok = -1;
    static int dotdir_ok = -1;
    char *t = pblock_findkeyval(pb_key_path, rq->vars);

    /*
     * This directive either returns REQ_ABORTED, or has no effect, based
     * solely on the path.
     */
    rq->directive_is_cacheable = 1;

    /* lazy initialization */
    if (tilde_ok == -1) {
        tilde_ok = pblock_findval("tildeok", pb)?1:0;
    }
    if (dotdir_ok == -1) {
        dotdir_ok = pblock_findval("dotdirok", pb)?1:0;
    }

    if(!t || !t[0])
        return REQ_NOACTION;

    /* let "*" (e.g. for OPTIONS) through */
    if(t[0] == '*' && t[1] == '\0')
        return REQ_NOACTION;

#ifdef XP_UNIX
    if((t[0]) != '/') {
#else /* WIN32 */
    if((t[0] != '/') &&  (!(isalpha(t[0]) && (t[1] == ':')))) {
#endif /* XP_UNIX */

        /* let proxied requests through */
        if(util_is_url(t))
            return REQ_NOACTION;

        /* chastise stupid client */
        log_error(LOG_WARN, "uri-clean", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError3), t);
        return REQ_ABORTED;
    }

    if(util_uri_is_evil_internal(t, tilde_ok, dotdir_ok)) {
        log_error(LOG_WARN, "uri-clean", sn, rq, 
                  XP_GetAdminStr(DBT_MalformedPathX), t);
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        return REQ_ABORTED;
    }

    return REQ_PROCEED;
}


/* -------------------------- pcheck_ntcgicheck --------------------------- */


#ifdef XP_WIN32
int pcheck_ntcgicheck(pblock *pb, Session *sn, Request *rq)
{ 
    char *extension = pblock_findval("extension", pb);
    pb_param *pp = pblock_find("path", rq->vars);
    char *cgiPath, *newPath ;
	struct stat statBuffer;

    int pathLength;

    if((!extension)) {
        log_error(LOG_MISCONFIG, "ntcgicheck", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError4));
        return REQ_ABORTED;
    }

    if(!pp)
        return REQ_NOACTION;

    cgiPath = pp->value;
	pathLength = strlen(cgiPath);

	if (cgiPath[pathLength - 1] == '/') {
		// if the path is not that of a plain file, dont add to it...
		return REQ_NOACTION;
	} else {
		// check if the file is a directory...
		// I think that the file handle is cached.
		// If not, to do this differently will make the code ugly...

		// stat returns 0 if it succeeds, if it fails, let PathCheck
		// catch it after we append the extension...

		if (!(stat(cgiPath, &statBuffer))) {
			if ( _S_IFDIR & statBuffer.st_mode ) {
				return REQ_NOACTION;
			}
		}
	}

	// If the last three letters end in .cgi change to .exe
	// If the path does not have an extension, add the default extension...
	// The <= is there in case it has neither a '.' nor a '/'.

	if (strrchr(cgiPath, '.') <= strrchr(cgiPath,'/')) {
		pathLength = strlen(cgiPath) + strlen(extension) + 1;
		newPath = (PCHAR)MALLOC(pathLength);
		sprintf(newPath, "%s%s\0", cgiPath, extension);
		FREE( pp->value );
		pp -> value  =  newPath;
	} else if (!strcmp( (cgiPath + pathLength - 4), ".cgi")) {
		// We are removing .cgi and adding in the default extension...
		pathLength = strlen(cgiPath) - 4 + strlen(extension) + 1;
		newPath = (PCHAR)MALLOC(pathLength);
		strncpy( newPath, cgiPath, pathLength - 4);
		strcpy( newPath + pathLength - 5 , extension);
		newPath[pathLength - 1] = '\0';
		FREE(pp->value);
		pp -> value = newPath;
	}

    return REQ_PROCEED;
}
#endif /* XP_WIN32 */


/* ------------------------- pcheck_require_auth -------------------------- */


int pcheck_require_auth(pblock *pb, Session *sn, Request *rq)
{
    char *t, *u, *type;
    int buflen;
    char buf[256];

    rq->directive_is_cacheable = 1;

    if( (t = pblock_findval("path", pb)) )
        if(WILDPAT_CMP(pblock_findval("path", rq->vars), t))
            return REQ_NOACTION;

    rq->directive_is_cacheable = 0;

    if(!(type = pblock_findval("auth-type", pb))) {
        log_error(LOG_MISCONFIG, "require-auth", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError5));
        return REQ_ABORTED;
    }
    if(!(t = pblock_findval("auth-type", rq->vars)))
        goto punt;

    if(strcasecmp(t, type))
        goto punt;

    if( (t = pblock_findval("auth-user", rq->vars)) ) {
        if( (u = pblock_findval("auth-user", pb))) {
            if(shexp_cmp(t, u))
                goto punt;
        }
    }

    if(!(u = pblock_findkeyval(pb_key_auth_group, pb)))
        return REQ_PROCEED;
    if(!(t = pblock_findkeyval(pb_key_auth_group, rq->vars)))
        goto punt;  /* group required but none given */

    if(!shexp_cmp(t, u))
        return REQ_PROCEED;

  punt:
    if(!(u = pblock_findval("realm", pb))) {
        log_error(LOG_MISCONFIG, "require-auth", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError6));
        return REQ_ABORTED;
    }

    buflen = PR_snprintf(buf, sizeof(buf), "%s realm=\"%s\"", type, u);
    if (buflen < 0) {
        buf[sizeof(buf) - 1] = '\0';
        buflen = strlen(buf);
    }

    /* Ensure a terminating quote on the realm string */
    if (buf[buflen-1] != '\"') {
        buf[buflen-1] = '\"';
    }

    pblock_nvinsert("www-authenticate", buf, rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_UNAUTHORIZED, NULL);

    return REQ_ABORTED;
}


/* -------------------------- pcheck_find_links --------------------------- */

#ifdef XP_UNIX
int pcheck_find_links(pblock *pb, Session *sn, Request *rq)
{
    /** VB : This function does a bit more than required.
        It should not be trying to stat and check for file existence and 
        returning a REQ_ABORTED if the file did not exist since the request 
        could have been for a plugin.
        Bug# 338887 relates to this. 
        The checkFileExistence="Yes" in obj.conf is a sort of insurance 
        to get back to default behaviour for any customer relying on the 
        earlier broken behaviour.
    **/
    const char* checkFile = pblock_findval("checkFileExistence", pb);
    int checkFileFlag = 0;
    char *path = pblock_findval("path", rq->vars);
    char *disable = pblock_findval("disable", pb);
    char c, *t;
    struct stat fi;
    int soft, hard;
    register int l;
    uid_t uid = 0;

    if (checkFile && (toupper(*checkFile) == 'Y'))
        checkFileFlag = 1;

    if(!disable) {
        log_error(LOG_MISCONFIG, "find-links", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError7));
        return REQ_ABORTED;
    }
    for(soft = 0, hard = 0; *disable; ++disable) {
        if(*disable == 'h')
            hard = 1;
        else if(*disable == 's')
            soft = 1;
        else if(*disable == 'o') {
            uid = getuid();
            soft = 2;
        }
    }

    const char *dir = pblock_findkeyval(pb_key_dir, pb);
    if(!dir) {
        dir = pblock_findkeyval(pb_key_ntrans_base, rq->vars);
        if(!dir)
            dir = "/";
    }

    l = strlen(dir);

    //KITTY: Changed from if(l && (... to if((l > 1) && (... so
    //that it won't remove the '/' if l == 1. (bug# 520263)
    if((l > 1) && (dir[l-1] == '/')) --l;

    t = path;

    if(l) {
        if(dir[0] == '/') {
            if(strncmp(path, dir, l))
                return REQ_NOACTION;
            t += l;
        }
        else {
            if(!(t = strstr(path, dir)))
                return REQ_NOACTION;
            t += l;
        }
    }

    while(1) {
        while(*t && (*t != '/')) ++t;
        c = *t;
        *t = '\0';
        if (lstat(path, &fi) == -1) {
            if (checkFileFlag) {
                log_error(LOG_WARN, "find-links", sn, rq, 
                          XP_GetAdminStr(DBT_pcheckError8),
                          pblock_findval("uri", rq->vars), 
                          path, system_errmsg());
                protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
                return REQ_ABORTED;
            }
            else 
                return REQ_NOACTION;
        }
        if(soft && S_ISLNK(fi.st_mode)) {
	    if(soft == 2) {
                struct stat realfi;

                if(stat(path, &realfi) == -1) {
                    log_error(LOG_WARN, "find-links", sn, rq, 
                              XP_GetAdminStr(DBT_pcheckError9),
                              pblock_findval("uri", rq->vars), path, 
                              system_errmsg());
                    protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
                    return REQ_ABORTED;
                }
                if(realfi.st_uid != uid)
                    goto bong;
	    }
	    else goto bong;
        }
        if(hard && S_ISREG(fi.st_mode) && (fi.st_nlink != 1)) {
          bong:
            log_error(LOG_WARN, "find-links", sn, rq,
                      XP_GetAdminStr(DBT_pcheckError10), path);
            protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
            return REQ_ABORTED;
        }
        if(!(*t++ = c))
            break;
    }
    return REQ_PROCEED;
}
#endif /* XP_UNIX */

/* MLM - Check for vulnerable (pre-1.12) versions of the navigator.  
 *       Bong them if they try to access you.  Requested by customers.
 *       Was in 1.12, somehow got regressed out of 2.0.
 */
/* --------------------- pcheck_detect_vulnerable ------------------------ */
 
int pcheck_detect_vulnerable(pblock *pb, Session *sn, Request *rq)
{
    char *ua;
    char *whereto;
 
    if(request_header("user-agent", &ua, sn, rq) == REQ_ABORTED)
        return REQ_ABORTED;
 
    if((!ua) || strncasecmp(ua, "Mozilla/", 8))
        return REQ_PROCEED;
 
    /* Detect if it's a major rev <= 1. */
    /* If not, they're using 2.0 and are Happy.*/
    if(ua[8] > '1')
        return REQ_PROCEED;
 
    /* Bong 0.xx releases */
    else if((ua[8] < '1') || (ua[9] != '.'))
        goto deny_request;

    /* Minor version. Check to see if it's patch level "1.12" or "1.22" */
    if(ua[10] > '2')
        return REQ_PROCEED;
    if((!isdigit(ua[11])) || (ua[11] < '2'))
        goto deny_request;
    else
        return REQ_PROCEED;
 
deny_request:
    whereto = pblock_findval("dest", pb);
    if(!whereto)  {
        log_error(LOG_MISCONFIG, "detect_vulnerable", sn, rq,
                  XP_GetAdminStr(DBT_pcheckError11));
        return REQ_ABORTED;
    }
 
    if(util_is_url(whereto))   {
        protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
        pblock_nvinsert("url", whereto, rq->vars);
        return REQ_ABORTED;
    }  else  {
        char *newloc = http_uri2url(whereto, "");
        char *uri = pblock_findval("uri", rq->reqpb);
 
        /* If they only gave a partial path, then allow them to get ONLY
         * that path.  (This means no graphics, but hey.)  If they're not
         * requesting the same path, redirect them. */
        if(!strcmp(uri, whereto))
            return REQ_PROCEED;
 
        protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
        pblock_nvinsert("url", newloc, rq->vars);
        FREE(newloc);
        return REQ_ABORTED;
    }
}

/* Check SSL cipher characteristics on current session */
int pcheck_ssl_check(pblock *pb, Session *sn, Request *rq)
{
    int rv = REQ_NOACTION;

    if (GetSecurity(sn)) {
        char *dir_skey_str = pblock_findval("secret-keysize", pb);
        char *req_skey_str = pblock_findval("secret-keysize", sn->client);

        if (dir_skey_str) {
            if (req_skey_str && (atoi(req_skey_str) >= atoi(dir_skey_str))) {
                rv = REQ_PROCEED;
            }
            else {
                char *bong_file = pblock_findval("bong-file", pb);
                if (bong_file) {
                    pblock *opb = pblock_create(1);
                    httpd_object *obj = object_create(NUM_DIRECTIVES, opb);
                    pblock *dpb = pblock_create(1);
                    pblock_nvinsert("fn", "send-bong-file", dpb);
                    pblock_nvinsert("bong-file", bong_file, dpb);
                    object_add_directive(NSAPIError, dpb, NULL, obj);
                    objset_add_object(obj, rq->os);
                }
                protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
                rv = REQ_ABORTED;
                SSL_INVALIDATESESSION(sn->csd);
            }
        }
    }

    return rv;
}

/* Invalidate the current SSL session in the SSL session cache */
int pcheck_ssl_logout(pblock *pb, Session *sn, Request *rq)
{
    if (GetSecurity(sn)) {
        SSL_INVALIDATESESSION(sn->csd);
        return REQ_PROCEED;
    }

    return REQ_NOACTION;
}

int pcheck_set_cache_control(pblock *pb, Session *sn, Request *rq)
{
    /* same code as below, just reordered for speed */

    rq->directive_is_cacheable = 1;

    pb_param *pp = pblock_find("path", rq->vars);
    if(!pp)
        return REQ_NOACTION;

    const char *cc_value = pblock_findval("control", pb);
    if(!cc_value) {
        log_error(LOG_MISCONFIG, "cache-control", sn, rq,
                  XP_GetAdminStr(DBT_pcheckError12));
        return REQ_ABORTED;
    }

    char cc_buf[256];
    NSString str;
    str.useStatic(cc_buf, sizeof(cc_buf), 0);
    str.append(cc_value);

    const char* val = (const char*)str;
    pblock_nvreplace("cache-control", (char*)val, rq->srvhdrs);

    return REQ_PROCEED;
}


/*************************************************************************
   Fn: pcheck_set_virtual_index

   Refer bug # 343875

   This is a PathCheck function refereed to in obj.conf as set-virtual-index
   The intent of the function is to be able to set the index of any directory
   to be a virtual index (such as a LW app, a servlet in it's own namespace,
   a NAS applogic etc).
   
   This function takes two parameters:
   virtual-index : The uri of the content gnerator acting as index for this uri
   from : This is optional. If specified, it should be a comma separated list
          of uri's for which this virtual index generator is applicable.

   Return values:
      REQ_NOACTION : If any of the uri's listed in the "from" parameter, do
                     not match the current uri.
      REQ_ABORTED : If virtual-index is missing or current uri cannot be found.
      REQ_RESTART : If the current uri matches any one of the uri's mentioned
                    in the from paramter.
                    If there is no from paramter, then the virtual index will 
                    always apply.
   It is necessary to return REQ_RESTART so that an internal request will get
   generated with the new ppath. This will result in execution of any nametrans
   that may apply to this new ppath and inclusion of any objects/acls in 
   obj.conf based on this new ppath. 
*************************************************************************/
int 
pcheck_set_virtual_index(pblock *pb, Session *sn, Request *rq)
{
    int res = REQ_NOACTION;

    char* uri  = pblock_findval("uri",  rq->reqpb);
    if (!uri) {
        log_error(LOG_MISCONFIG, "set-virtual-index", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError14));
        res = REQ_ABORTED;
    }
    int len = strlen(uri);
   

    char* virtualIndexName = pblock_findval("virtual-index", pb);
    if (!virtualIndexName) {
        log_error(LOG_MISCONFIG, "set-virtual-index", sn, rq, 
                  XP_GetAdminStr(DBT_pcheckError13));
        res = REQ_ABORTED;
    }

    if (res != REQ_ABORTED) {
        char* from = pblock_findval("from", pb);
        PRBool found = PR_TRUE;
        if (from) {
            // Iterate thru the comma separated from uri's for a match
            found = PR_FALSE;
            while (from && (found == PR_FALSE)) {
                char* curr = from;
                from = strchr(curr, ',');
                PRBool changed = PR_FALSE;
                if ((from && (from-curr == len) && strncmp(curr, uri, len) == 0)
                    || (!from && strncmp(curr, uri, len) == 0)) { 
                    found = PR_TRUE;
                }
                if (from) {
                    from++;
                }
            }
        }
        if (found == PR_TRUE) {
            // This virtual index must be applied
            char *luri = _setup_newuri(sn->pool, rq, uri, len, virtualIndexName, NULL);
            char *query = pblock_findkeyval(pb_key_query, rq->reqpb);
            return request_restart(sn, rq, NULL, luri, query);
        }
    }

    return res;
}

