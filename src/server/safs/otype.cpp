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
 * otype.c: ObjectType directive related functions
 * 
 * Rob McCool
 * 
 */


#include "netsite.h"       /* FREE, STRDUP */
#include "safs/otype.h"
#include "safs/dbtsafs.h"
#include "base/cinfo.h"     /* cinfo_find */
#include "frame/log.h"
#include "frame/cookie.h"
#include "frame/otype_helper.h"

#include "base/shexp.h"     /* shexp_cmp */
#include "frame/protocol.h" /* protocol_status */

#include "base/util.h"      /* is_mozilla, sprintf */
#include "httpdaemon/vsconf.h"

#include <sys/types.h>
#include <sys/stat.h>


/* --------------------------- otype_forcetype ---------------------------- */


static inline void otype_add(pblock *pb, const pb_key *config, pblock *srvhdrs, const pb_key *header)
{
    char *value = pblock_findkeyval(config, pb);
    if (value) {
        if (!pblock_findkey(header, srvhdrs))
            pblock_kvinsert(header, value, strlen(value), srvhdrs);
    }
}

static inline void otype_add(pblock *pb, const pb_key *config, pblock *srvhdrs, const char *header)
{
    char *value = pblock_findkeyval(config, pb);
    if (value) {
        if (!pblock_find(header, srvhdrs))
            pblock_nvinsert(header, value, srvhdrs);
    }
}

int otype_forcetype(pblock *pb, Session *sn, Request *rq)
{
    char *ps;
    pb_param *pp;

    rq->directive_is_cacheable = 1;

    otype_add(pb, pb_key_type, rq->srvhdrs, pb_key_content_type);
    otype_add(pb, pb_key_enc,  rq->srvhdrs, "content-encoding");
    otype_add(pb, pb_key_lang, rq->srvhdrs, "content-language");
    otype_add(pb, pb_key_charset, rq->srvhdrs, pb_key_magnus_charset);

    return REQ_PROCEED;
}

/**
 * otype_changetype
 *
 * Change the content-type to a new specified value possibly conditionally, only when the certain
 * type is already there
 */

int
otype_changetype (pblock *pb, Session *sn, Request *rq)
{
    char * type = pblock_findval ("type", pb);

    rq->directive_is_cacheable = 1;

    if (type != NULL)
    {
        const char *if_type = pblock_findval ("if-type", pb);	

        if (if_type != NULL)
        {
            const char *ct_type = pblock_findval ("content-type", rq -> srvhdrs);

            if (ct_type != NULL 
                && !strcmp (if_type, ct_type))
            {
                pblock_nvreplace ("content-type", type, rq -> srvhdrs);
                return REQ_PROCEED;
            }
        }
        else
        {
            pblock_nvreplace ("content-type", type, rq -> srvhdrs);
            return REQ_PROCEED;
        }
    }

    return REQ_NOACTION;
}

/* --------------------------- otype_typebyexp ---------------------------- */


int otype_typebyexp(pblock *pb, Session *sn, Request *rq) {
    char *exp = pblock_findval("exp", pb);
    char *path = pblock_findval("path", rq->vars);

    rq->directive_is_cacheable = 1;

    if(!exp) {
        log_error(LOG_MISCONFIG, "type-by-exp", sn, rq, 
                  XP_GetAdminStr(DBT_ntwincgiError14));
        return REQ_ABORTED;
    }

    if(!(WILDPAT_CMP(path, exp)))
        return otype_forcetype(pb, sn, rq);
    else {
        return REQ_NOACTION;
    }
}


/* --------------------------- otype_exec_shtml --------------------------- */


/* Aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaack hack barrf */

int otype_shtmlhacks(pblock *pb, Session *sn, Request *rq)
{
    char *path = pblock_findval("path", rq->vars);
    char *do_exec = pblock_findval("exec-hack", pb);
    int l;

    /* This is cachable- we always do the same thing based SOLELY
     * on the URI...
     */
    rq->directive_is_cacheable = 1;

    l = strlen(path);
    if((!strcasecmp(&path[l-4],".htm")) || (!strcasecmp(&path[l-5],".html"))) {
#ifdef XP_UNIX
        if(do_exec) {
            struct stat *fi = request_stat_path(NULL, rq);

            if(fi && (!(fi->st_mode & S_IXUSR)))
                return REQ_NOACTION;
        }
#endif /* XP_UNIX */
        param_free(pblock_remove("content-type", rq->srvhdrs));
        pblock_nvinsert("content-type", "magnus-internal/parsed-html",
                        rq->srvhdrs);
        return REQ_PROCEED;
    }
    return REQ_NOACTION;
}


/* ---------------------------- otype_ext2type ---------------------------- */


static inline void otype_ciadd(const char *c, pblock *srvhdrs, const pb_key *key)
{
    if (c) {
        if (!pblock_findkey(key, srvhdrs))
            pblock_kvinsert(key, c, strlen(c), srvhdrs);
    }
}

static inline void otype_ciadd(const char *c, pblock *srvhdrs, const char *name)
{
    if (c) {
        if (!pblock_find(name, srvhdrs))
            pblock_nvinsert(name, c, srvhdrs);
    }
}

NSAPI_PUBLIC int otype_ext2type(pblock *param, Session *sn, Request *rq)
{
    cinfo *ci;
    pb_param *pp;
    char *path = pblock_findkeyval(pb_key_path, rq->vars);
    NSFCFileInfo *finfo = NULL;

    rq->directive_is_cacheable = 1;

    /* Check for "*" and set type appropriately */
    if(path[0] == '*' && path[1] == '\0') {
        pblock_kvinsert(pb_key_content_type,
                        "magnus-internal/asterisk",
                        sizeof("magnus-internal/asterisk") - 1,
                        rq->srvhdrs);
        return REQ_PROCEED;
    }

    /* Check for directory and set type appropriately */
    /* If not found, ignore error */
    if( (INTrequest_info_path(path, rq, &finfo) == PR_SUCCESS) && finfo ) {
        if(finfo->pr.type == PR_FILE_DIRECTORY) {
            if(!(pp = pblock_findkey(pb_key_content_type, rq->srvhdrs)))
                pblock_kvinsert(pb_key_content_type,
                                "magnus-internal/directory", 
                                sizeof("magnus-internal/directory") - 1,
                                rq->srvhdrs);
            return REQ_PROCEED;
        }
    }

    // Get content info
    const VirtualServer* vs = request_get_vs(rq);
    ci = vs->getMime().getContentInfo(sn->pool, path);
    if(ci) {
        otype_ciadd(ci->type, rq->srvhdrs, pb_key_content_type);
        otype_ciadd(ci->encoding, rq->srvhdrs, "content-encoding");
        otype_ciadd(ci->language, rq->srvhdrs, "content-language");

        pool_free(sn->pool, ci->type);
        pool_free(sn->pool, ci->encoding);
        pool_free(sn->pool, ci->language);
        pool_free(sn->pool, ci);
    }

    return REQ_PROCEED;
}


/* --------------------------- otype_imgswitch ---------------------------- */


int otype_imgswitch(pblock *pb, Session *sn, Request *rq)
{
    char *ct, *ua, *pa, *npath, *t;
    struct stat fi;
    pb_param *pp;

    /* This routine might be cacheable.  Lets check the objtype so
     * far to see if we are talking about an image file.  If this path
     * is not an image, we can go ahead and cache.
     */
    rq->directive_is_cacheable = 1;

    if(!(ct = pblock_findval("content-type", rq->srvhdrs)))
        return REQ_NOACTION;
    if(strncasecmp(ct, "image/", 6))
        return REQ_NOACTION;

    /* This routine is still running, so it must be an image we are
     * dealing with.  This is not cacheable since we need the user-agent
     * info to determine which file we return.
     */
    rq->directive_is_cacheable=0;

    /* In the absence of a capabilities header, use user-agent */
    if(request_header("user-agent", &ua, sn, rq) == REQ_ABORTED)
        return REQ_ABORTED;
    /* We want to be nice to proxies */
    if(request_header("proxy-agent", &pa, sn, rq) == REQ_ABORTED)
        return REQ_ABORTED;

    if((!ua) || pa || strstr(ua, "roxy"))
        return REQ_NOACTION;

    /* Look for jpeg if we're talking to mozilla and image is .gif */
    if((!strncasecmp(ua, "mozilla", 7)) && (!strcasecmp(ct, "image/gif"))) {
        npath = STRDUP(pblock_findval("path", rq->vars));
        if(!(t = strstr(npath, ".gif")))
            return REQ_NOACTION;
        t[1] = 'j'; t[2] = 'p'; t[3] = 'g';
        if(stat(npath, &fi) == -1) {
            FREE(npath);
            return REQ_NOACTION;
        }
        pp = pblock_find("path", rq->vars);
        FREE(pp->value);
        pp->value = npath;

        /* don't check return; it should work. */
        request_stat_path(npath, rq);

        pp = pblock_find("content-type", rq->srvhdrs);
        FREE(pp->value);
        pp->value = STRDUP("image/jpeg");

        return REQ_PROCEED;
    }
    return REQ_NOACTION;
}


/* --------------------------- otype_htmlswitch --------------------------- */


/* Sigh. Another stupid pet trick that will get dropped on the floor */
int otype_htmlswitch(pblock *pb, Session *sn, Request *rq)
{
    char *ct, *ua, *pa, *npath, *t;
    struct stat fi;
    pb_param *pp;

    /* This routine might be cacheable.  Lets check the objtype so
     * far to see if we are talking about a text file.  If this path
     * is not a text, we can go ahead and cache.
     */
    rq->directive_is_cacheable = 1;

    if(!(ct = pblock_findval("content-type", rq->srvhdrs)))
        return REQ_NOACTION;
    if(strncasecmp(ct, "text/", 5))
        return REQ_NOACTION;

    /* This is still running, so it must be a text file we are
     * dealing with.  This is not cacheable since we need the user-agent
     * info to determine which file we return.
     */
    rq->directive_is_cacheable = 0;

    /* In the absence of a capabilities header, use user-agent */
    if(request_header("user-agent", &ua, sn, rq) == REQ_ABORTED)
        return REQ_ABORTED;
    /* We want to be nice to proxies */
    if(request_header("proxy-agent", &pa, sn, rq) == REQ_ABORTED)
        return REQ_ABORTED;

    if((!ua) || pa || strstr(ua, "roxy"))
        return REQ_NOACTION;

    /* Look for html3 if we're talking to mozilla and find HTML */
    if(util_is_mozilla(ua, "1", "1") && (!strcasecmp(ct, "text/html"))) {
        t = pblock_findval("path", rq->vars);
        npath = (char *) MALLOC(strlen(t) + 1 + 1);
        util_sprintf(npath, "%s3", t);
        if(stat(npath, &fi) == -1) {
            FREE(npath);
            return REQ_NOACTION;
        }
        pp = pblock_find("path", rq->vars);
        FREE(pp->value);
        pp->value = npath;

        /* don't check return; it should work. */
        request_stat_path(npath, rq);
        return REQ_PROCEED;
    }
    return REQ_NOACTION;
}


/**********************************************************************
   Function: otype_setdefaulttype
   Returns: always returns REQ_PROCEED
   Allow the user to set a default content-type, content-encoding
   content-language and charset
   These are added into rq->vars under the corresponding internalNames
   After applying all the otypes functions, these are copied from the
   rq->vars into rq->srvhdrs if they do not already exist in rq->srvhdrs.
***********************************************************************/


int
otype_setdefaulttype(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;
    OtypeHelerSetDefaults(rq, pb);    
    return REQ_PROCEED;
}


/* --------------------------- otype_setcookie ---------------------------- */


int otype_setcookie(pblock *pb, Session *sn, Request *rq)
{
    rq->directive_is_cacheable = 1;

    const char *name = pblock_findkeyval(pb_key_name, pb);
    if (!name) {
        log_error(LOG_MISCONFIG, "set-cookie", sn, rq,
                  XP_GetAdminStr(DBT_ntransError7));
        return REQ_ABORTED;
    }

    PRInt64 max_age = COOKIE_MAX_AGE_SESSION;
    if (const char *max_age_str = pblock_findkeyval(pb_key_max_age, pb))
        max_age = util_atoi64(max_age_str);

    PRBool secure = PR_FALSE;
    if (const char *secure_str = pblock_findkeyval(pb_key_secure, pb))
        secure = util_getboolean(secure_str, PR_TRUE);

    int res = cookie_set(sn, rq, name,
                         pblock_findkeyval(pb_key_value, pb),
                         pblock_findkeyval(pb_key_path, pb),
                         pblock_findkeyval(pb_key_domain, pb),
                         pblock_findkeyval(pb_key_expires, pb),
                         max_age, secure);
    if (res != REQ_PROCEED)
        return res;

    return REQ_NOACTION;
}
