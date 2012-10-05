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
 * auth.c: Handle AuthTrans related things. Currently, only stupid HTTP
 * basic authentication.
 *
 * Rob McCool
 */


#include "netsite.h"

// Stupid client stuff...
#ifndef XP_CPLUSPLUS
#define XP_CPLUSPLUS
#endif
#include "ssl.h"
#include "base64.h"
#include "secitem.h"
#include "base/daemon.h"
#include "base/session.h"
#include "frame/req.h"

#include "frame/protocol.h"
#include "frame/log.h"
#include "frame/func.h"
#include "frame/http.h"
#include "frame/http_ext.h"
#include "frame/conf.h"
#include "frame/conf_api.h"

#include "base/buffer.h"
#include "base/util.h"

#include "libaccess/acl.h"
#include "libaccess/aclproto.h"
#include "libaccess/digest.h"

#include "libadmin/libadmin.h"

#include "safs/auth.h"
#include "safs/init.h"
#include "safs/digest.h"

#include "safs/dbtsafs.h"

#include "base/crit.h"
#include "libaccess/cryptwrapper.h"

static CRITICAL auth_crit = NULL;

extern "C" void
auth_init_crits(void)
{
    auth_crit = crit_init();
}

int _cmpSHA1Passwd(const char *encrypted, const char* cleartext)
{
    int rv;
    crit_enter(auth_crit);
    /* If using SHA, do not pass prefix {SHA} of the encrypted password */
    rv = https_sha1_pw_cmp((char *)cleartext, (char *)encrypted+5); 
    crit_exit(auth_crit);
    return rv;
}  /* _cmpSHA1Passwd */

#define AUTH_MAX_LINE 1024


/* ------------------------------ _uudecode ------------------------------- */

const unsigned char pr2six[256]={
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,0,1,2,3,4,5,6,7,8,9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,64,26,27,
    28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64
};

char *_uudecode(char *bufcoded)
{
    register char *bufin = bufcoded;
    register unsigned char *bufout;
    register int nprbytes;
    unsigned char *bufplain;
    int nbytesdecoded;

    /* Find the length */
    while(pr2six[(int)*(bufin++)] <= 63);
    nprbytes = bufin - bufcoded - 1;
    nbytesdecoded = ((nprbytes+3)/4) * 3;

    bufout = (unsigned char *) MALLOC(nbytesdecoded + 1);
    bufplain = bufout;

    bufin = bufcoded;
    
    while (nprbytes > 0) {
        *(bufout++) = (unsigned char) 
            (pr2six[(int)(*bufin)] << 2 | pr2six[(int)bufin[1]] >> 4);
        *(bufout++) = (unsigned char) 
            (pr2six[(int)bufin[1]] << 4 | pr2six[(int)bufin[2]] >> 2);
        *(bufout++) = (unsigned char) 
            (pr2six[(int)bufin[2]] << 6 | pr2six[(int)bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }
    
    if(nprbytes & 03) {
        if(pr2six[(int)bufin[-2]] > 63)
            nbytesdecoded -= 2;
        else
            nbytesdecoded -= 1;
    }
    bufplain[nbytesdecoded] = '\0';

    return (char *)bufplain;
}


/* ------------------------------ auth_basic ------------------------------ */

int auth_basic(pblock *param, Session *sn, Request *rq)
{
    char *pwfile, *grpfile, *type, *auth, *user, *pw;
    char *pwfn, *grpfn;
    pblock *npb;
    pb_param *pp;
    int ret;

    /* Although this is authorization (which is not cacheable) the
     * check is not actually done until we call require-auth.  So 
     * this part is cacheable; require-auth can be cacheable if the
     * user has limited the auth to only affect a certain set of 
     * paths.
     */
    rq->directive_is_cacheable = 1;

    if(request_header("authorization", &auth, sn, rq) == REQ_ABORTED)
        return REQ_ABORTED;

    if(!auth)
        return REQ_NOACTION;

    type = pblock_findval("auth-type", param);
    pwfile = pblock_findval("userdb", param);
    grpfile = pblock_findval("groupdb", param);
    pwfn = pblock_findval("userfn", param);
    grpfn = pblock_findval("groupfn", param);

    if((!type) || (!pwfile) || (!pwfn) || (grpfile && !grpfn)) {
        log_error(LOG_MISCONFIG, "basic-auth", sn, rq,
                  XP_GetAdminStr(DBT_authError1)); 
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }

    /* Skip leading whitespace */
    while(*auth && (*auth == ' '))
        ++auth;
    if(!(*auth)) {
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        return REQ_ABORTED;
    }

    /* Verify correct type */
    if((strlen(auth) < 6) || strncasecmp(auth, "basic ", 6))
        return REQ_NOACTION;

    /* Skip whitespace */
    auth += 6;
    while(*auth && (*auth == ' '))
        ++auth;

    if(!*auth)
        return REQ_NOACTION;

    /* Uuencoded user:password now */
    if(!(user = _uudecode(auth)))
        return REQ_NOACTION;

    if(!(pw = strchr(user, ':'))) {
        FREE(user);
        return REQ_NOACTION;
    }
    *pw++ = '\0';

    npb = pblock_create(4);
    pblock_nvinsert("user", user, npb);
    pblock_nvinsert("pw", pw, npb);
    pblock_nvinsert("userdb", pwfile, npb);
    if(grpfile) 
        pblock_nvinsert("groupdb", grpfile, npb);
    pblock_nvinsert("fn", pwfn, npb);

    if ((ret = func_exec(npb, sn, rq)) != REQ_PROCEED)
    {
        goto bye;
    }

    pblock_nvinsert("auth-type", "basic", rq->vars);
    pblock_nvinsert("auth-user", user, rq->vars);
    pblock_nvinsert("auth-db", pwfile, rq->vars);
#if defined(XP_WIN32) || defined(MCC_ADMSERV)
    /* MLM - the admin server needs this password information,
     *       so I'm putting it back   */
    pblock_nvinsert("auth-password", pw, rq->vars);
#endif /* XP_WIN32 */

    if(grpfile) {
        pblock_nvinsert("groupdb", grpfile, npb);
        pp = pblock_find("fn", npb);
        FREE(pp->value);
        pp->value = STRDUP(grpfn);

        if( (ret = func_exec(npb, sn, rq)) != REQ_PROCEED )
            goto bye;
    }
    ret = REQ_PROCEED;
  bye:
    pblock_free(npb);
    FREE(user);
    return ret;
}


/* ----------------------------- auth_basic1 ------------------------------ */


/*
 * Compatibility function for 1.0 servers, to more powerful 1.1 SAF
 * accessible auth trans function
 */
int auth_basic1(pblock *param, Session *sn, Request *rq)
{
    pblock *npb = pblock_create(4);
    char *pwfile = pblock_findval("userfile", param);
    char *grpfile = pblock_findval("grpfile", param);
    char *type = pblock_findval("auth-type", param);
    char *pwdbm = pblock_findval("dbm", param);
    int ret;

    rq->directive_is_cacheable = 1;

    if(type) pblock_nvinsert("auth-type", type, npb);

    if(grpfile) {
        pblock_nvinsert("groupdb", grpfile, npb);
        pblock_nvinsert("groupfn", "simple-groupdb", npb);
    }
    if(pwfile) {
        pblock_nvinsert("userdb", pwfile, npb);
        pblock_nvinsert("userfn", "simple-userdb", npb);
    }
    else if(pwdbm) {
        pblock_nvinsert("userdb", pwdbm, npb);
        pblock_nvinsert("userfn", "dbm-userdb", npb);
    }
    ret = auth_basic(npb, sn, rq);
    pblock_free(npb);
    return ret;
}


/* ----------------------------- simple_group ----------------------------- */


int simple_group(pblock *param, Session *sn, Request *rq)
{
    SYS_FILE fd;
    filebuf_t *buf;
    int eof, ln = 0;
    char *t, *u;
    char line[AUTH_MAX_LINE];
    char *groupnames = NULL;
    int empty = 1;


    char *gfn = pblock_findval("groupdb", param);
    char *user = pblock_findval("user", param);

    rq->directive_is_cacheable = 1;

    if( (fd = system_fopenRO(gfn)) == SYS_ERROR_FD) {
        log_error(LOG_FAILURE, "basic-ncsa", sn, rq, 
                  XP_GetAdminStr(DBT_authError4), gfn);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }
    if(!(buf = filebuf_open(fd, FILE_BUFFERSIZE))) {
        log_error(LOG_FAILURE, "basic-ncsa", sn, rq, 
                  XP_GetAdminStr(DBT_authError5), gfn);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }

    /* Parse group file. Match username against group names.
     * Create comma seperated list of group names and
     * include it as part of auth-group.
     */

    while(1) {
        eof = util_getline(buf, ++ln, AUTH_MAX_LINE, line);
        if(!line[0]) {
            if(eof) {
                filebuf_close(buf);
                if(empty ==1)
                    return REQ_PROCEED;
                if(groupnames != NULL) {
                    /* Add comma seperated list of matched group
                     * names into auth-group.
                     */
                    pblock_kvinsert(pb_key_auth_group, groupnames,
                                    strlen(groupnames), rq->vars);
                    FREE(groupnames);
                    return REQ_PROCEED;
                }
                else
                    return REQ_NOACTION;
            }
            else
                continue;
        }

        empty = 0;
        for(t = line; *t && (*t != ':'); t++);
        if(!(*t))
            continue;
        *t++ = '\0';

        /* "line" contains the group name and buffer "t" contains
         * the list of users in this group. Parse the list of users
         * to check if the required "user" is present. If the user
         * name is matched, add the group name into "groupnames"
         * buffer.
         */

        while(1) {
            while((*t) && (*t == ' ')) ++t;
            if(!(*t))
                break;
            u = t;

            while((*u) && (*u != ' ')) ++u;
            if(!strncmp(user, t, u - t)) {
                if(groupnames == NULL) {
                    groupnames = (char *)MALLOC(buf->len);
                    strcpy(groupnames, line);
                }
                else {
                    strcat(groupnames, ",");
                    strcat(groupnames, line);
                }
            }
            if(!(*u))
                break;
            else
                t = u;
        }
    }
}

//
// simple_user - implement simple text-file based user authentication
//
// params:
//   userdb - name of password file (text file with colon-separated user/password pairs)
//   user -   username to authenticate against
//   pw -     password
//
int
simple_user(pblock *param, Session *sn, Request *rq)
{
    register char *t;
    char line[AUTH_MAX_LINE];
    SYS_FILE fd;
    filebuf_t *buf;
    int ln, eof, res;
    const char *reason;

    char *pwfile = pblock_findval("userdb", param);
    char *user = pblock_findval("user", param);
    char *pw = pblock_findval("pw", param);

    rq->directive_is_cacheable = 1;

    /* Open user file and verify password */
    if((fd = system_fopenRO(pwfile)) == SYS_ERROR_FD) {
        log_error(LOG_FAILURE, "basic-ncsa", sn, rq, 
                  XP_GetAdminStr(DBT_authError6), pwfile, 
                  system_errmsg());
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }
    if(!(buf = filebuf_open(fd, FILE_BUFFERSIZE))) {
        log_error(LOG_FAILURE, "basic-ncsa", sn, rq, 
                  XP_GetAdminStr(DBT_authError7), pwfile, 
                  system_errmsg());
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }
    ln = 1;
    while(1) {
        eof = util_getline(buf, ln, AUTH_MAX_LINE, line);
        if(!line[0]) {
	    /* empty line */
            if(eof) /* eof after last line? */
		goto nosuchuser;
            else
                continue;
        }

        if(!(t = strchr(line, ':')))
            continue;
        *t++ = '\0';
        if(!strcmp(line, user)) {
#ifdef XP_WIN32
	    /** 
	     * if you forget the password (e.g: admin usr), the suggested 
	     * workaround is to remove everything after * "admin:" in the 
	     * admpw file. but, this does not work on W2K. ACL_CryptCompare()
	     * would return a non-zero value (-ve). the DES cipher test fails
	     * this is a HACK for now !!!
	     */
	    if (!strcmp(t, "")) {
		filebuf_close(buf);
		return REQ_PROCEED;
	    }
#endif /* XP_WIN32 */
	    if (!strncmp(t, "{SHA}",5))
	        res = _cmpSHA1Passwd(t, pw);
	    else
	        res = ACL_CryptCompare(pw, t, t);
	    if (res) {
                log_error(LOG_SECURITY, "basic-ncsa", sn, rq, 
                          XP_GetAdminStr(DBT_authError8),
                          user, pwfile);
		reason = "bad password";
		goto ateof;
            } else {
		/* correct password */
                filebuf_close(buf);
                return REQ_PROCEED;
            }
        }
	/* if user not found yet, keep going */
	/* if user not found at all, bail out */
        if(eof) {
	    /* eof at last line (missing newline at last line) */
	    goto nosuchuser;
        }
    }
nosuchuser:
    log_error(LOG_SECURITY, "basic-ncsa", sn, rq, 
	      XP_GetAdminStr(DBT_authError9),
	      user, pwfile);
    reason = "no such user";
ateof:
    filebuf_close(buf);
    /* MLM - Admin server wants this to stop if the user gives
     *       the wrong password or if they don't exist.  Leave
     *       a message for the admin server and return noaction. */
    return REQ_NOACTION;
}


/*
 * get_auth_user_basic - attribute getter for ACL_ATTR_USER and methods = basic or digest
 *
 *   Description:
 *	This getter calls the getter for ACL_ATTR_IS_VALID_PASSWORD to verify the
 *	password and store authenticated user information on the Session
 *	property list for backward compatibilty.
 */

int get_auth_user_basic (NSErr_t *errp, PList_t subject, PList_t resource,
			 PList_t auth_info, PList_t global_auth, void *unused)
{
    char *raw_user;
    char *raw_pw;
    char *userdn;
    int rv;
    pblock *pb;
    Session *sn;
    Request *rq;
    int accel;			/* accelerated path? - no request struct! */
    char *cached_user = 0, *authreq = 0;
    ACLMethod_t *acl_methodp = NULL, method = ACL_METHOD_INVALID;

    // are we just checking whether we need to authenticate?
    rv = PListGetValue(resource, ACL_ATTR_AUTHREQCHECK_INDEX, (void **)&authreq, NULL);
    if ((rv >= 0) && authreq && *authreq) {
        authreq[0] = '*';       // check the fact that we needed an authenticated user & fail 
        return LAS_EVAL_FAIL;
    }

    rv = PListGetValue(subject, ACL_ATTR_CACHED_USER_INDEX, (void **)&cached_user, NULL);
    if ((rv >= 0) && cached_user && *cached_user) {
        PListInitProp(subject, ACL_ATTR_USER_INDEX, ACL_ATTR_USER, cached_user, 0);
        return LAS_EVAL_TRUE;
    }

    rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, (void **)&rq, NULL);
    if (rv < 0 || !rq) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_authEreport18));
        return LAS_EVAL_FAIL;
    }

    // delete old IS_VALID_PASSWORD property and get new one.
    PListDeleteProp(subject, ACL_ATTR_IS_VALID_PASSWORD_INDEX, ACL_ATTR_IS_VALID_PASSWORD);
    rv = ACL_GetAttribute(errp, ACL_ATTR_IS_VALID_PASSWORD, (void **)&raw_user, subject, resource, auth_info, global_auth);
    if (rv == LAS_EVAL_FALSE || rv == LAS_EVAL_PWEXPIRED) {
        // if the password is invalid or expired
        // we set up basic auth & do the pwexpired hack
	char* realm = 0;

	if (rv == LAS_EVAL_PWEXPIRED)
            // transmit the fact that the password is expired to the upper layers.
	    pblock_nvinsert("password-policy", "expired", rq->vars);

	rv = PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL);
	if (rv < 0 || !sn) {
	    ereport(LOG_SECURITY, XP_GetAdminStr(DBT_authEreport2), rv);
	    return LAS_EVAL_FAIL;
	}
    
        // get the realm out of auth_info, if there...
	if (auth_info)
	    rv = PListGetValue(auth_info, ACL_ATTR_PROMPT_INDEX, (void **)&realm, NULL);

        // mark the resource plist that basic or digest auth is requested.
	// (this may fail if somebody has requested www-auth already...)
        // we set up a new realm, 
        PListDeleteProp(resource, ACL_ATTR_WWW_AUTH_PROMPT_INDEX, ACL_ATTR_WWW_AUTH_PROMPT);
        PListInitProp(resource, ACL_ATTR_WWW_AUTH_PROMPT_INDEX, ACL_ATTR_WWW_AUTH_PROMPT, realm, 0);

        if (PListGetValue(auth_info, ACL_ATTR_METHOD_INDEX, (void **)&acl_methodp, 0) < 0) {
            // no method property in the ACL: check if the ACL's database can do digestauth.
            // if so, we'll later present both challenges
            // set ACL_ATTR_WWW_AUTH_METHOD to ACL_MethodBasic to signal this.
            if (ACL_AuthDBAllowsDigestAuth(errp, resource, auth_info) == LAS_EVAL_TRUE)
                PListInitProp(resource, ACL_ATTR_WWW_AUTH_METHOD_INDEX, ACL_ATTR_WWW_AUTH_METHOD, ACL_MethodBasic, 0);
        } else if (*acl_methodp == ACL_MethodBasic) {
            // delete any remnants of earlier digest auth requests
            PListDeleteProp(resource, ACL_ATTR_WWW_AUTH_METHOD_INDEX, ACL_ATTR_WWW_AUTH_METHOD);
        } else if (*acl_methodp == ACL_MethodDigest) {
            // check if we CAN do digest auth on the auth DB anyway
            if (ACL_AuthDBAllowsDigestAuth(errp, resource, auth_info) != LAS_EVAL_TRUE) {
                ereport(LOG_MISCONFIG, "digest authentication request on a non-digestauth capable database.");
                return LAS_EVAL_FAIL;
            }

            // present only digest auth challenge
            PListDeleteProp(resource, ACL_ATTR_WWW_AUTH_METHOD_INDEX, ACL_ATTR_WWW_AUTH_METHOD);
            PListInitProp(resource, ACL_ATTR_WWW_AUTH_METHOD_INDEX, ACL_ATTR_WWW_AUTH_METHOD, ACL_MethodDigest, 0);
        }
	return LAS_EVAL_NEED_MORE_INFO;
    } else if (rv != LAS_EVAL_TRUE) {
        // in any case, if the password is not correct, there's no authenticated user
	return rv;
    }

    // set various variables in rq->vars
    char *dbname = 0;

    // set ACL_ATTR_AUTH_TYPE and ACL_ATTR_AUTH_USER
    //pblock_nvinsert(ACL_ATTR_AUTH_TYPE, ACL_AUTHTYPE_BASIC, rq->vars);
    if (PListGetValue(auth_info, ACL_ATTR_METHOD_INDEX, (void **)&acl_methodp, 0) < 0) {
        pblock_nvinsert(ACL_ATTR_AUTH_TYPE, ACL_AUTHTYPE_BASIC, rq->vars);
    } else {
        if (*acl_methodp==ACL_MethodDigest)
            pblock_nvinsert(ACL_ATTR_AUTH_TYPE, ACL_AUTHTYPE_DIGEST, rq->vars);
        else
            pblock_nvinsert(ACL_ATTR_AUTH_TYPE, ACL_AUTHTYPE_BASIC, rq->vars);
    }
    pblock_nvinsert(ACL_ATTR_AUTH_USER, raw_user, rq->vars);

    // if we did digest authentication, and we do not have an Authentication-Info
    // server header yet, put one in.
    char *rspauth = 0;
    if (PListGetValue(resource, ACL_ATTR_RSPAUTH_INDEX, (void **)&rspauth, 0) >= 0 && rspauth) {
        char *authentication_info = pblock_findval("Authentication-info", rq->srvhdrs);
        if (!authentication_info) {
            char buf[2048];

            util_snprintf(buf, sizeof(buf), "rspauth=\"%s\"", rspauth);
            pblock_nvinsert("Authentication-info", buf, rq->srvhdrs);
        }
    }

    // for backward compatibility:

    // put raw password into ACL_ATTR_AUTH_PASSWORD if we have it
    rv = PListGetValue(subject, ACL_ATTR_RAW_PASSWORD_INDEX, (void **)&raw_pw, NULL);
    if (rv >= 0 && raw_pw)
        pblock_nvinsert(ACL_ATTR_AUTH_PASSWORD, raw_pw, rq->vars);

    // put user DN into ACL_ATTR_USERDN if we have it
    rv = PListGetValue(subject, ACL_ATTR_USERDN_INDEX, (void **)&userdn, NULL);
    if (rv >= 0 && userdn && *userdn)
        pblock_nvinsert(ACL_ATTR_USERDN, userdn, rq->vars);

    // put dbname into ACL_ATTR_AUTH_DB if we have it
    rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
    if ((rv >= 0) && dbname)
        pblock_nvinsert(ACL_ATTR_AUTH_DB, dbname, rq->vars);

    // finally, do what we came in for - set the ACL_ATTR_USER in the subject plist
    PListInitProp(subject, ACL_ATTR_USER_INDEX, ACL_ATTR_USER, pool_strdup(PListGetPool(subject), raw_user), 0);
    return LAS_EVAL_TRUE;
}

//
// parse_basic_user_login - get username and password from base64-encoded Authorization header
//
int
parse_basic_user_login (const char *auth, char **user, char **password)
{
    char *uid;
    char *pw;

    /* Skip leading whitespace */
    while (*auth && (*auth == ' ')) ++auth;

    if (!(*auth)) return LAS_EVAL_FAIL;

    /* Verify correct type */
    if((strlen(auth) < 6) || strncasecmp(auth, "basic ", 6))
        return LAS_EVAL_FAIL;

    /* Skip whitespace */
    auth += 6;
    while(*auth && (*auth == ' '))
        ++auth;

    if(!*auth)
        return LAS_EVAL_FAIL;

    /* Uuencoded user:password now */
    if(!(uid = _uudecode((char *)auth)))
        return LAS_EVAL_FAIL;

    if(!(pw = strchr(uid, ':'))) {
        FREE(uid);
        return LAS_EVAL_FAIL;
    }
    *pw++ = '\0';

    *user = uid;
    *password = pw;

    return LAS_EVAL_TRUE;
}


PRBool checkUserName(const char* name)
{
    if (!name || *name==NULL)
        return PR_FALSE;
    NSString uName;
    uName.append(name);
    uName.strip(NSString::BOTH,' ');
    if (uName.length() == strlen(name) )
        return PR_TRUE;
    return PR_FALSE;
}

//
// get_user_login_basic - attribute getter for ACL_ATTR_RAW_USER and ACL_ATTR_RAW_PASSWORD
//                        and method == basic or method == digest
//
int
get_user_login_basic (NSErr_t *errp, PList_t subject, 
			  PList_t resource, PList_t auth_info,
			  PList_t global_auth, void *unused)
{
    /* Get the authorization attribute and parse it into raw_user and
     * raw_password.
     */
    char *auth;
    char *raw_user;
    char *raw_pw;
    char *h;
    int rv;

    rv = ACL_GetAttribute(errp, ACL_ATTR_AUTHORIZATION, (void **)&auth, subject,
			  resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) {
	return rv;
    }
    else if (!auth) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_authEreport3));
	return LAS_EVAL_FAIL;
    }

    if (PL_strncasecmp(auth, "basic ", 6) == 0) {
        // Parse the authorization attribute to get raw_user_login and
        // raw_user_password.
        if ((rv = parse_basic_user_login(auth, &raw_user, &raw_pw)) != LAS_EVAL_TRUE)
            return rv;

        h = raw_user;
        if (h==NULL || *h==NULL) {
            return LAS_EVAL_FALSE;
        }
        if (checkUserName(h)==PR_FALSE) {
                ereport(LOG_SECURITY, XP_GetAdminStr(DBT_authEreport6), raw_user);
                FREE(raw_user);           /* also frees raw_pw */
                return LAS_EVAL_FALSE;
        }

        PListInitProp(subject, ACL_ATTR_RAW_USER_INDEX, ACL_ATTR_RAW_USER,
                      pool_strdup(PListGetPool(subject), raw_user), 0);
        PListInitProp(subject, ACL_ATTR_RAW_PASSWORD_INDEX, ACL_ATTR_RAW_PASSWORD,
                      pool_strdup(PListGetPool(subject), raw_pw), 0);
        PListInitProp(subject, ACL_ATTR_AUTHTYPE_INDEX, ACL_ATTR_AUTHTYPE,
                      ACL_MethodBasic, 0);
        FREE(raw_user);		/* also frees raw_pw */

    } else if (PL_strncasecmp(auth, "digest ", 7) == 0) {
        // just get the raw user value out of the authorization header
        if ((rv = parse_digest_user_login(auth, &raw_user, NULL, NULL, NULL, NULL,
                                        NULL, NULL, NULL, NULL, NULL)) != LAS_EVAL_TRUE)
            return rv;

        h = raw_user;
        if (h==NULL || *h==NULL) {
            return LAS_EVAL_FALSE;
        }
        if (checkUserName(h)==PR_FALSE) {
                ereport(LOG_SECURITY, XP_GetAdminStr(DBT_authEreport6), raw_user);
                FREE(raw_user);           /* also frees raw_pw */
                return LAS_EVAL_FALSE;
        }

        // parse_digest_user_login allocates raw_user for us, so no need to strdup
        PListInitProp(subject, ACL_ATTR_RAW_USER_INDEX, ACL_ATTR_RAW_USER,
                      raw_user, 0);
        // in case of digest, ACL_ATTR_RAW_PASSWORD contains the contents of the
        // authorization header for more detailed parsing later on, so we don't have
        // to create lots of attributes for digestauth...
        PListInitProp(subject, ACL_ATTR_RAW_PASSWORD_INDEX, ACL_ATTR_RAW_PASSWORD,
                      pool_strdup(PListGetPool(subject), auth), 0);
        PListInitProp(subject, ACL_ATTR_AUTHTYPE_INDEX, ACL_ATTR_AUTHTYPE,
                      ACL_MethodDigest, 0);
    } else {
        ereport(LOG_SECURITY, "Unknown authentication method in Authorization header.");
        return LAS_EVAL_FAIL;
    }

    return LAS_EVAL_TRUE;
}

//
// get_authorization_basic - attribute getter for ACL_ATTR_AUTHORIZATION and method == basic | digest
//
int
get_authorization_basic (NSErr_t *errp, PList_t subject, 
			     PList_t resource, PList_t auth_info,
			     PList_t global_auth, void *unused)
{
    /* request basic auth if authorization attr doesn't exist */
    int rv;
    Session *sn = 0;
    Request *rq = 0;
    char *auth = 0;
    ACLMethod_t method = ACL_METHOD_INVALID;

    rv = PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL);
    if (rv < 0 || !sn)
        return LAS_EVAL_FAIL;

    rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, (void **)&rq, NULL);
    if (rv < 0 || !rq) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_authEreport5), rv);
        return LAS_EVAL_FAIL;
    }

    /* request_header always returns REQ_PROCEED! */
    request_header("authorization", &auth, sn, rq);
    if (!auth || !*auth)
	return LAS_EVAL_FALSE;

    PListInitProp(subject, ACL_ATTR_AUTHORIZATION_INDEX, ACL_ATTR_AUTHORIZATION,
                  pool_strdup(PListGetPool(subject), auth), 0);
    return LAS_EVAL_TRUE;
}


//
// auth_get_sslid - get base64 encoding of SSL session id into sn->client pblock
//
int
auth_get_sslid(pblock *param, Session *sn, Request *rq)
{
    char *idstr;
    SECItem *iditem;

    /* This requires SSL to be active */
    if (!GetSecurity(sn))
        return REQ_NOACTION;

    /* Have we already got the session id in the client pblock? */
    if (pblock_findval("ssl-id", sn->client) != NULL)
        return REQ_NOACTION;

    /* No, look it up based on the connection fd */
    if ((iditem = SSL_GetSessionID(sn->csd)) == NULL)
        return REQ_NOACTION;

    /* Convert to base64 ASCII encoding */
    idstr = BTOA_DataToAscii(iditem->data, iditem->len);
    if (idstr) {

        /* Add encoding to client pblock */
        pblock_nvinsert("ssl-id", idstr, sn->client);

        /* Free the encoding buffer (pblock_nvinsert dups it) */
        PORT_Free(idstr);
    }

    SECITEM_FreeItem(iditem, PR_TRUE);

    return REQ_NOACTION;
}
