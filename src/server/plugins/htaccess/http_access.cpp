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
 * http_access: Security options etc.
 *
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 * 
 * Based on NCSA HTTPd 1.3 by Rob McCool
 *
 *  03-12-95 blong
 *     Added patch to fix ALLOW_THEN_DENY
 * 
 */

#include "frame/protocol.h"  /* protocol_status */

#include "htaccess.h"


int htaccess_in_domain(char *domain, char *what) {
    int dl=strlen(domain);
    int wl=strlen(what);

    if((wl-dl) >= 0) {
        if (strcasecmp(domain,&what[wl-dl]) != 0)
            return 0;

        /* Ensure that we do the right thing regarding subdomains.  We
         * don't want to match ihateiplanet.com with iplanet.com.
         */

        if (wl == dl)
            return 1;  /* the whole thing matches */
        else
            return (domain[0] == '.' || what[wl - dl - 1] == '.');
    } else
        return 0;
}

/* We support the following methods of specifying IP addresses:
 *   a.b.c.d
 *   a.b.c.     ==> a.b.c.0/24
 *   a.b.c.0/mask where mask is the form /nnn or /aaa.bbb.ccc.ddd
 *
 * Returns 1 upon success (authentication allowed)
 * Returns 0 upon failure
 *
 * NOTE: All sanity checking of the octets, mask, etc are done in
 *       check_allow()
 */
int htaccess_in_ip(char *allowfrom, char *where) {
    char * s;
    unsigned long mask = 0; /* allow all bits */

    unsigned long allowedip = 0;
    unsigned long remoteip = 0;

    remoteip = inet_addr(where);

    if ((s = strchr(allowfrom, '/'))) { 

        *s++ = '\0';

        /* mask is the form /a.b.c.d */
        if (strchr(s, '.')) {
            mask = inet_addr(s);
        } else {
            /* assume it is /nnn */
            mask = atoi(s);
            mask = 0xFFFFFFFFUL << (32 - mask);
            mask = htonl(mask);
        }
        allowedip = (allowedip & mask);
        allowedip = inet_addr(allowfrom);
    } else {
        /* it is either an IP address or the form a.b.c. */
        int shift = 24;
        char *s, *t;
        int octet;

        s = allowfrom;
        mask = 0;

        while (*s) {
            t = s;

            /* This should never match, but just in case, this will prevent
             * a server hang because the loop will never exit otherwise.
             */
            if ((!isdigit(*t)) && (*t != '.'))
                return 0;

            while (isdigit(*t))
                ++t;

            if (*t == '.')
                *t++ = 0;

            octet = atoi(s);

            allowedip |= octet << shift;
            mask |= 0xFFUL << shift;
            s = t;
            shift -= 8;
        }
        allowedip = ntohl(allowedip);
        mask = ntohl(mask);
    }

    if ((remoteip & mask) == allowedip)
        return 1;
    else
        return 0;
}

int htaccess_check_method(int method_mask, int method) {
    if (method_mask & (1 << method))
        return 1;
    if (method_mask == 0)
        return 1;
    else
        return 0;
}

int htaccess_find_allow(int x, int method, htaccess_context_s *ctxt) {
    register int y;
    security_data * sec = htaccess_getsec(ctxt->sec, x);

    if(sec->num_allow < 0)
        return 0;

    for(y=0;y<sec->num_allow;y++) {
	if (htaccess_check_method(sec->allow_methods[y], method)) {
            if(!strcmp("all",sec->allow[y])) 
                return 1;

            /* check to see if this domain is allowed */
            if(ctxt->remote_host && !htaccess_is_ip(ctxt->remote_host)) 
                if(htaccess_in_domain(sec->allow[y],ctxt->remote_host))
                    return 1;

            /* check to see if this IP is allowed */
            if(htaccess_in_ip(sec->allow[y],ctxt->remote_ip))
                return 1;
        }
    }
    return 0;
}

int htaccess_find_deny(int x, int method, htaccess_context_s *ctxt) {
    register int y;
    security_data * sec = htaccess_getsec(ctxt->sec, x);

    if(sec->num_deny < 0)
        return 1;

    for(y=0;y<sec->num_deny;y++) {
	if (htaccess_check_method(sec->deny_methods[y], method)) {
            if(!strcmp("all",sec->deny[y])) 
                return 1;

            /* check to see if this domain is denied */
            if(ctxt->remote_host && !htaccess_is_ip(ctxt->remote_host))
                if(htaccess_in_domain(sec->deny[y],ctxt->remote_host))
                    return 1;

            /* check to see if this IP is denied */
            if(htaccess_in_ip(sec->deny[y],ctxt->remote_ip))
                return 1;
        }
    }
    return 0;
}

void htaccess_check_dir_access(int x, int m, int *w, int *n, htaccess_context_s *ctxt) 
{
    security_data * sec = htaccess_getsec(ctxt->sec, x);

    if(sec->auth_type)
        ctxt->auth_type = sec->auth_type;
    if(sec->auth_name)
        ctxt->auth_name = sec->auth_name;
    if(sec->auth_pwfile) {
        ctxt->auth_pwfile = sec->auth_pwfile;
#ifdef AUTHNSDBFILE
        ctxt->auth_nsdb = sec->auth_nsdb;
#endif /* AUTHNSDBFILE */
    }
    if(sec->auth_grpfile) {
        ctxt->auth_grpfile = sec->auth_grpfile;
#ifdef AUTHNSDBFILE
        ctxt->auth_nsdb = sec->auth_nsdb;
#endif /* AUTHNSDBFILE */
    }

    if(sec->order[m] == ALLOW_THEN_DENY) {
	*w=0;
        if(htaccess_find_allow(x,m,ctxt))
            *w=1;
        if(htaccess_find_deny(x,m,ctxt))
            *w=0;
    } else if(sec->order[m] == DENY_THEN_ALLOW) {
        *w=1;
        if(htaccess_find_deny(x,m,ctxt))
            *w=0;
        if(htaccess_find_allow(x,m,ctxt))
            *w=1;
    }
    else
        *w = htaccess_find_allow(x,m,ctxt) && (!htaccess_find_deny(x,m,ctxt));

    /* check if any auth (require) statement applies to the current */
     /* method m. if so, set n (need_auth) to the statement number */
     if (sec->num_auth) {
     	for (int y=0; y < sec->num_auth; y++) {
            if (htaccess_check_method(sec->auth_methods[y], m)) {
            	*n=x;
            	break;
             }
         }
     }
}

int htaccess_evaluate_access(char *p, int isdir, int methnum, 
                    pblock *pb, Session *sn, Request *rq)
{
    int will_allow, need_auth, num_dirs;
    register int x;
    char *path,*root_dir,*full_filename;
    int start=0; /* start looking for .htaccess files at ntrans-base */
    int init=0; /* htaccess context allocation flag */
    char *base = pblock_findval("ntrans-base", rq->vars);
    htaccess_context_s * ctxt;
    int rv = ACCESS_OK;
    int len=0;

    len = strlen(p);

    if ((path = (char *)MALLOC(len + 1)) == NULL) {
        log_error(LOG_FAILURE,"htaccess_evaluate_access", sn, rq, "out of memory");
        return REQ_ABORTED;
    }
    if ((root_dir = (char *)MALLOC(len + 1)) == NULL) {
        log_error(LOG_FAILURE,"htaccess_evaluate_access", sn, rq, "out of memory");
        FREE(path);
        return REQ_ABORTED;
    }

    if(isdir) htaccess_strcpy_dir(path,p);
    else htaccess_lim_strcpy(path,p, MAX_STRING_LEN);

    htaccess_no2slash(path);

    num_dirs = htaccess_count_dirs(path);
    will_allow = 1; 
    need_auth = -1;

    char *access_name = pblock_findval("filename", pb);
    if (!access_name)
        access_name = DEFAULT_ACCESS_FNAME;

    /* Allocate space to store "/" and null apart from htaccess file name */
    if ((full_filename = (char *)MALLOC(len + strlen(access_name) + 2)) == NULL) {
        log_error(LOG_FAILURE,"htaccess_evaluate_access", sn, rq, "out of memory");
        FREE(path);
        FREE(root_dir);
        return REQ_ABORTED;
    }

    /* traverse the request path looking for .htaccess files */
    for(x=0;x<num_dirs;x++) {

        htaccess_make_dirstr(path,x+1,root_dir);

        /* start checking for files at ntrans-base */
        if ((strcmp(root_dir, base)) == 0) start=1;

        if (start) {
            filebuffer *f;

            htaccess_make_full_path(root_dir, access_name, full_filename);

            /* don't create a context unless there is a file to parse */
            if ((f=htaccess_cfg_open(full_filename))) {
		if (!init) {
                    ctxt = _htaccess_newctxt(pb, sn, rq);
                    init=1;
                }
                htaccess_parse_access_dir(f,-1,0,root_dir,full_filename,ctxt);
                htaccess_cfg_close(f);
            }
        }
    }

    if (init) {
        for(x=0;x<ctxt->num_sec;x++) {
            /* We already know that all the entries apply */
            htaccess_check_dir_access(x,methnum,&will_allow,&need_auth, ctxt);
        }

        if(!will_allow)
            rv = ACCESS_FORBIDDEN;
        else {
            if(need_auth >= 0) {
                security_data * sec = htaccess_getsec(ctxt->sec, need_auth);
                if(htaccess_check_auth(sec,methnum,ctxt) == REQ_ABORTED)
                    rv = ACCESS_AUTHFAIL;
            }
        }

        _htaccess_freectxt(ctxt);
    }
    FREE(path);
    FREE(root_dir);
    FREE(full_filename);
    return rv;
}


/* -------------------------- htaccess_evaluate --------------------------- */


int 
htaccess_evaluate(pblock *pb, Session *sn, Request *rq)
{
    char *p = pblock_findval("path", rq->vars);
    struct stat *finfo = request_stat_path(p, rq);
    int isdir = finfo && S_ISDIR(finfo->st_mode);

    /* some java servlets that are set in rules.properties don't set this */
    if(!pblock_findval("ntrans-base", rq->vars))
        return REQ_NOACTION;

    char *methstr = pblock_findval("method", rq->reqpb);
    int methnum;
    HttpMethodRegistry& registry = HttpMethodRegistry::GetRegistry();

    /* treat HEAD as a GET */
    if (!strcasecmp(methstr, "HEAD"))
        methnum = registry.HttpMethodRegistry::GetMethodIndex("GET");
    else
        methnum = registry.HttpMethodRegistry::GetMethodIndex(methstr);

    /* if it isn't a method we support, don't allow access */
    if (methnum == -1)
    {
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        return REQ_ABORTED;
    }

    switch(htaccess_evaluate_access(p, isdir, methnum, pb, sn, rq)) {
      case ACCESS_OK:
        return REQ_PROCEED;

      case ACCESS_FORBIDDEN:
      default:
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        log_error(LOG_SECURITY, "htaccess-find", sn, rq, 
                  "access of %s denied by server configuration.", p);
        /* fallthrough */

      case ACCESS_AUTHFAIL:
        /* the data structures will be set up by http_auth.c */
        return REQ_ABORTED;
    }
}

static htaccess_context_s *
_htaccess_newctxt(pblock *pb, Session *sn, Request *rq)
{
    htaccess_context_s *ctxt = 
        (htaccess_context_s *) MALLOC(sizeof(htaccess_context_s));
    char *rhst = session_dns(sn);

    ctxt->pb = pb;
    ctxt->sn = sn;
    ctxt->rq = rq;

    ctxt->auth_type = NULL;
    ctxt->auth_name = NULL;
    ctxt->auth_pwfile = NULL;
    ctxt->auth_grpfile = NULL;
    ctxt->auth_authdb = 0;
    ctxt->auth_grplist = 0;
    ctxt->auth_grplfile = 0;
    ctxt->auth_grpllen = 0;
    ctxt->user_check_fn = NULL;
    ctxt->group_check_fn = NULL;

#ifdef AUTHNSDBFILE
    ctxt->auth_uoptr = 0;
    ctxt->auth_nsdb = 0;
#endif /* AUTHNSDBFILE */
    ctxt->auth_line = pblock_findval("authorization", rq->headers);

    ctxt->num_sec = 0;

    ctxt->remote_host = rhst;
    ctxt->remote_ip = pblock_findval("ip", sn->client);
    ctxt->remote_name = rhst ? rhst : ctxt->remote_ip;

    ctxt->access_name = pblock_findval("filename", pb);
    if(!ctxt->access_name)
        ctxt->access_name = DEFAULT_ACCESS_FNAME;

    ctxt->user[0] = '\0';
    ctxt->groupname[0] = '\0';

    ctxt->sec = htaccess_newsec();

    return ctxt;
}

static void
_htaccess_freectxt(htaccess_context_s *ctxt)
{
    int x, y;

    for(x=0;x<ctxt->num_sec;x++) {
        if(ctxt->user_check_fn)
            FREE(ctxt->user_check_fn);
        if(ctxt->group_check_fn)
            FREE(ctxt->group_check_fn);
        if (ctxt->auth_authdb) {
            htaccess_kill_group(ctxt);
        }
        if (ctxt->auth_grplist) {
            FREE(ctxt->auth_grplist);
        }
        if (ctxt->auth_grplfile) {
            FREE(ctxt->auth_grplfile);
        }
    }
    htaccess_secflush(ctxt->sec);
    FREE(ctxt->sec);
    FREE(ctxt);
}
