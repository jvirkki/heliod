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
 * nsconfig.c: Handle .nsconfig files 
 * 
 * Rob McCool
 */


#include "base/pblock.h"
#include "base/session.h"
#include "frame/req.h"

#include "base/util.h"      /* sprintf */
#include "frame/log.h"      /* log_error */
#include "base/shexp.h"     /* shexp_cmp */
#include "frame/protocol.h" /* protocol_status */
#include "frame/object.h"
#include "frame/objset.h"
#include "frame/httpdir.h"  /* NUM_DIRECTIVES */
#include "frame/httpact.h"  /* various things */
#include "safs/dbtsafs.h"

#define NSCONFIG_MAXLEN 1024
#define NSCONFIG_HASHSIZE 2

#define ALLOW 1
#define DENY 0
#define DUNNO -1


/* ------------------------- nsconfig_add_object -------------------------- */

/*
 * Adds an object to the tmpos set of the Request structure. If no set
 * has been allocated yet, does it. This also adds the object to the
 * std_os set so that it is used for later configuration information
 */

void _nsconfig_add_object(Request *rq, httpd_object *obj)
{
    if(!rq->tmpos)
        rq->tmpos = objset_create();

    objset_add_object(obj, rq->tmpos);
    objset_add_object(obj, rq->os);
}


/* ------------------------ nsconfig_add_directive ------------------------ */

/*
 * Add a directive of type dtype to the object obj. If obj is NULL, 
 * allocates an object. Returns either obj or the new object.
 */

httpd_object *_nsconfig_add_directive(char *dtype, char *ppath, 
                                      pblock *dpb, httpd_object *obj)
{
    int dc = directive_name2num(dtype);

    if(!obj) {
        pblock *npb = pblock_create(1);

        pblock_nvinsert("ppath", ppath, npb);
        obj = object_create(NUM_DIRECTIVES, npb);
    }
    object_add_directive(dc, dpb, NULL, obj);
    return obj;
}


/* ---------------------------- nsconfig_parse ---------------------------- */

/*
 * Parses the file given by dir/file, for the request of path rpath
 */

int auth_basic1(pblock *pb, Session *sn, Request *rq);

int _nsconfig_parse(char *dir, char *file, char *rpath,
                    Session *sn, Request *rq)
{
    SYS_FILE fd;
    filebuf_t *buf;
    char *t, *u, *d, *v, *dns, *filepat, *fpath, line[NSCONFIG_MAXLEN];
    int ln, n, ignoring = 0, match, pass;
    pblock *vpb, *dirpb;
    httpd_object *obj;

    int failcode = PROTOCOL_FORBIDDEN;

    fpath = (char *) MALLOC(strlen(dir) + strlen(file) + 2);
    util_sprintf(fpath, "%s/%s", dir, file);

    fd = system_fopenRO(fpath);
    FREE(fpath);
    if(fd == SYS_ERROR_FD)
        return REQ_NOACTION;

    buf = filebuf_open(fd, FILE_BUFFERSIZE);

    vpb = NULL; filepat = NULL; 
    n = 0; pass = DUNNO;
    obj = NULL;
    for(ln = 1; 1; ++ln) {
        if(vpb) {
            pblock_free(vpb);
            vpb = NULL;
        }
        if(n)
            break;
        switch( (n = util_getline(buf, ln, NSCONFIG_MAXLEN, line)) ) {
          case -1:
            log_error(LOG_FAILURE, "load-config", sn, rq, 
                      XP_GetAdminStr(DBT_nsconfigerror1), dir, FILE_PATHSEP, 
                      file, 
                      (line[0] == '\0' ? system_errmsg() : line)); /* bug 362522 */
            goto bong;
          default:
            if(line[0] == '#')
                continue;
            for(d = line; *d && (isspace(*d)); ++d);
            if(!*d)
                continue;

            for(v = d; *v && (!isspace(*v)); ++v);
            *v++ = '\0';
            while( (*v && isspace(*v)) ) ++v;
            if(!strcasecmp(d, "<files")) {
                /* Imply </files> where necessary */
                if(filepat)
                    FREE(filepat);

                /* If closing > is not there try to be flexible. */
                if( (t = strrchr(v, '>')) ) {
                    *t = '\0';
                }

#ifdef XP_WIN32
		{
		    char * u;
		    for (u = v; *u; ++u) {
			if (*u == '\\') {
			    *u = '/';
			}
		    }
		}
#endif /* XP_WIN32 */

                /* 1: / between dir and v */
                filepat = (char *) MALLOC(strlen(dir) + strlen(v) + 1 + 1);
                util_sprintf(filepat, "%s/%s", dir, v);

                ignoring = (shexp_cmp(rpath, filepat) ? 1 : 0);
                continue;
            }
            if(!filepat) {
                log_error(LOG_MISCONFIG, "load-config", sn, rq, 
                          XP_GetAdminStr(DBT_nsconfigerror2), dir, file, ln);
                goto bong;
            }
            if(!strcasecmp(d, "</files>")) {
                if(obj) {
                    _nsconfig_add_object(rq, obj);
                    obj = NULL;
                }
                FREE(filepat);
                filepat = NULL;
            }
            else if(!ignoring) {
                vpb = pblock_create(NSCONFIG_HASHSIZE);
                pblock_str2pblock(v, vpb);
                if(!strcasecmp(d, "restrictaccess")) {
                    if( (t = pblock_findval("method", vpb)) )
                        if(shexp_cmp(pblock_findval("method", rq->reqpb), t))
                            continue;

                    if(!(u = pblock_findval("type", vpb))) {
                        log_error(LOG_MISCONFIG, "load-config", sn, rq,
                                  XP_GetAdminStr(DBT_nsconfigerror3), dir, file, ln);
                        goto bong;
                    }
                    if(pass == DUNNO)
                        pass = DENY;
                    match = 0;
                    if( (t = pblock_findval("ip", vpb)) ) 
                        if(!shexp_cmp(pblock_findval("ip", sn->client), t))
                            match = 1;
                    if( (t = pblock_findval("dns", vpb)) ) {
                        if( (dns = session_maxdns(sn)) ) {
                            if(!shexp_cmp(dns, t))
                                match = 1;
                        }
                    }
                    if ( (t = pblock_findval("return-code", vpb)) ) {
                        failcode = atoi(t);
                        switch (failcode) {
                          case 404:
                          case 500:
                          case 403:
                          case 401:
                            break;
                          default:
                            failcode = PROTOCOL_FORBIDDEN;
                        }
                    }
                    if(match) {
                        if(!strcasecmp(u, "allow"))
                            pass = ALLOW;
                        else 
                            pass = DENY;
                    }
                }
                else if(!strcasecmp(d, "requireauth")) {
                    char *dbm = pblock_findval("dbm", vpb);
                    char *userfile = pblock_findval("userfile", vpb);
                    char *realm = pblock_findval("realm", vpb);
                    char *userpat = pblock_findval("userpat", vpb);

                    if(((!dbm) && (!userfile)) && (!realm))
                    {
                        log_error(LOG_MISCONFIG, "load-config", sn, rq, 
                                  XP_GetAdminStr(DBT_nsconfigerror4), dir, file, ln);
                        goto bong;
                    }
                    if(dbm || userfile) {
                        /* This is a bit more low-level than I'd like */
                        dirpb = pblock_create(NSCONFIG_HASHSIZE);
                        pblock_nvinsert("fn", "basic-ncsa", dirpb);
                        pblock_nvinsert("auth-type", "basic", dirpb);
                        if(dbm)
                            pblock_nvinsert("dbm", dbm, dirpb);
                        else
                            pblock_nvinsert("userfile", userfile, dirpb);
                        if(auth_basic1(dirpb, sn, rq) == REQ_ABORTED) {
                            pblock_free(dirpb);
                            goto bong;
                        }
                        pblock_free(dirpb);
                    }
                    if(realm) {
                        dirpb = pblock_create(NSCONFIG_HASHSIZE);
                        pblock_nvinsert("fn", "require-auth", dirpb);
                        pblock_nvinsert("auth-type", "basic", dirpb);
                        pblock_nvinsert("realm", realm, dirpb);
                        if(userpat)
                            pblock_nvinsert("auth-user", userpat, dirpb);
                        obj = _nsconfig_add_directive("pathcheck", filepat,
                                                      dirpb, obj);
                    }
                }
                else if(!strcasecmp(d, "addtype")) {
                    dirpb = pblock_create(NSCONFIG_HASHSIZE);
                    pblock_copy(vpb, dirpb);
                    /* Stop some smart aleck from changing fn */
                    param_free(pblock_remove("fn", dirpb));
                    pblock_nvinsert("fn", "type-by-exp", dirpb);
                    obj = _nsconfig_add_directive("objecttype", filepat, 
                                                  dirpb, obj);
                }
                else if(!strcasecmp(d, "errorfile")) {
                    pb_param *pp = pblock_remove("path", vpb);
                    char *t;

                    dirpb = pblock_create(NSCONFIG_HASHSIZE);
                    pblock_copy(vpb, dirpb);
                    /* Stop some smart aleck from changing fn */
                    param_free(pblock_remove("fn", dirpb));
                    pblock_nvinsert("fn", "send-error", dirpb);
                    if(pp && (!util_uri_is_evil(pp->value))) {
                        if( (t = request_translate_uri(pp->value, sn)) ) {
                            pblock_nvinsert("path", t, dirpb);
                            FREE(t);
                        }
                    }
                    param_free(pp);
                    obj = _nsconfig_add_directive("error", filepat, 
                                                  dirpb, obj);
                }
                else {
                    log_error(LOG_MISCONFIG, "load-config", sn, rq, 
                            XP_GetAdminStr(DBT_nsconfigerror5), dir, file, ln, line);
                    goto bong;
                }
            }
            break;
        }
    }
    if(pass == DENY) {
        log_error(LOG_SECURITY, "load-config", sn, rq,
                  XP_GetAdminStr(DBT_nsconfigerror6), rpath);
        protocol_status(sn, rq, failcode, NULL);
        goto bong;
    }
    filebuf_close(buf);
    return REQ_PROCEED;

  bong:
    if(vpb) {
        pblock_free(vpb);
        vpb = NULL;
    }
    filebuf_close(buf);
    return REQ_ABORTED;
}


int pcheck_nsconfig(pblock *pb, Session *sn, Request *rq)
{
    char *file = pblock_findval("file", pb);
    char *basedir = pblock_findval("basedir", pb);
    char *dtypes = pblock_findval("disable-types", pb);
    int descend = (pblock_findval("descend", pb) ? 1 : 0);
    char *path = pblock_findval("path", rq->vars);
    char *dir, *wd;
    int result;

    if(!file)
        file = ".nsconfig";
    if(!basedir) {
        if(!(basedir = pblock_findval("ntrans-base", rq->vars))) {
            log_error(LOG_MISCONFIG, "load-config", sn, rq, 
                      XP_GetAdminStr(DBT_nsconfigerror7));
            return REQ_ABORTED;
        }
    }

    /* Make sure we're in the right base directory */
#ifdef XP_UNIX
    result = strncmp(path, basedir, strlen(basedir));
#else /* XP_WIN32 */
    result = strncasecmp(path, basedir, strlen(basedir));
#endif /* XP_WIN32 */

    if (result) 
        return REQ_NOACTION;

    if(dtypes) {
        pblock *tmp, *npb = pblock_create(1);
        httpd_object *obj;

        tmp = pblock_create(NSCONFIG_HASHSIZE);
        pblock_nvinsert("fn", "disable-types", tmp);
        pblock_nvinsert("type", dtypes, tmp);

        pblock_nvinsert("ppath", "*", npb);
        obj = object_create(NUM_DIRECTIVES, npb);
        object_add_directive(directive_name2num("service"), tmp, NULL, obj);

        if(!rq->tmpos)
            rq->tmpos = objset_create();
        objset_add_object(obj, rq->tmpos);
        objset_add_object(obj, rq->os);
    }
    if(_nsconfig_parse(basedir, file, path, sn, rq) == REQ_ABORTED)
        return REQ_ABORTED;
    if(descend) {
        dir = STRDUP(path);
        wd = dir + strlen(basedir);
        if(*wd == '/')
            ++wd;
        while(1) {
            if(!(wd = strchr(wd, FILE_PATHSEP)))
                break;
            *wd = '\0';
            if(_nsconfig_parse(dir, file, path, sn, rq) == REQ_ABORTED) {
                FREE(dir);
                return REQ_ABORTED;
            }
            *wd++ = FILE_PATHSEP;
        }
        FREE(dir);
    }
    return REQ_PROCEED;
}
