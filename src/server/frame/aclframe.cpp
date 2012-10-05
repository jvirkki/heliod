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

/********************************************************************

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 1996 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

*********************************************************************/

#include	"sys/types.h"
#include	<limits.h>
#include	"malloc.h" 
#include	"netsite.h"
#ifdef	XP_WIN32
#include	"winsock.h"
#else
#include	<arpa/inet.h>
#endif
#include	<base/util.h>
#include	<base/file.h>
#include	<base/session.h>
#include	<base/plist.h>
#include	<base/ereport.h>
#include	<base/rwlock.h>

#include 	<libaccess/acl.h>
#include	<libaccess/aclproto.h>
#include 	<libaccess/nserror.h>
#include	<libaccess/nsauth.h>
#include	<libaccess/aclglobal.h>
#include 	<libaccess/dbtlibaccess.h>
#include	<libaccess/aclerror.h>
#include	<libaccess/digest.h>

#include	<frame/req.h>
#include	<frame/acl.h>
#include	<frame/aclinit.h>
#include	<frame/log.h>
#include	<frame/httpact.h>
#include	<frame/http.h>
#include	<frame/conf.h>

#include        <base/vs.h>
#include        <httpdaemon/vsconf.h>

#include	<safs/init.h>		// for authentication types

static struct program_groups	none = { NULL, NULL, NULL };
NSAPI_PUBLIC struct program_groups	*ADM_programGroups = &none;


int ACL_SetDigestPrompt(char const * prompt, char * buf, int len, PList_t resource) 
{
    char *nonce = 0;
    char *stale = 0;

    // we create a new nonce every time
    Digest_create_nonce(GetDigestPrivatekey(), &nonce, "");
    // NOTE: we do not support the "domain" attribute yet.
    util_snprintf(buf, len, "Digest realm=\"%s\", nonce=\"%s\", algorithm=\"MD5\", qop=\"auth\"", prompt, nonce);
    PORT_Free(nonce);

    PListGetValue(resource, ACL_ATTR_STALE_DIGEST_INDEX, (void **)&stale, NULL);
    if ((stale) && (strcmp(stale, "true")==0))
        PL_strcatn(buf, len, ", stale=\"true\"");

    return REQ_PROCEED;
}

int ACL_SetBasicPrompt(char const * prompt, char * buf, int len) 
{
    util_snprintf(buf, len, "Basic realm=\"%s\"", prompt);
    return REQ_PROCEED;
}

int ACL_SetGSSAPIPrompt(char const * prompt, char * buf, int len)
{
    util_snprintf(buf, len, "Negotiate %s", prompt);
    return REQ_PROCEED;
}

//
//   If the authdb has set the pb_key_auth_expiring attribute it means
//   the user's password is about to expire or has expired. If this
//   authdb is configured with a auth-expiring-url element, redirect
//   this request there instead of serving the original request.
//
// Parameters :
//   NSErr_t  - NSPR Error structure
//   Request * - request itself
//   Session * - request's session 
//   const VirtualServer * - virtual server which servers this request
//   int rv - value to return in case this function does nothing.
//
// Returns :
//   int -  ACL_RES_DENY in case request is redirected; otherwise rv
//
static int handle_expiration_redirect(NSErr_t err, Request *rq, Session *sn, 
                                      const VirtualServer *vs, int rv) 
{
    char * expiration = pblock_findkeyval(pb_key_auth_expiring, rq->vars);

    if (expiration != NULL) {
        const char *url = NULL;
        ACLVirtualDb_t *virtdb = NULL;
        const char *dbname = NULL;

        if ((dbname = pblock_findval("auth-db", rq->vars)) != NULL) {
            if (ACL_VirtualDbLookup(NULL, vs, 
                                    dbname, &virtdb) == LAS_EVAL_TRUE) {
                ACL_VirtualDbReadLock(virtdb);
                int rv1 = ACL_VirtualDbGetAttr(&err, virtdb,
                                               "auth-expiring-url",
                                               &url);
                ACL_VirtualDbUnlock(virtdb);
                if (rv1 >= 0 && url) {
                    protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
                    pblock_nvinsert("url", url, rq->vars);
                    ereport(LOG_VERBOSE,
                               "acl: auth-db indicates password "
                                "is expiring or has expired, "
                                "redirecting request to [%s]", url);
                    return ACL_RES_DENY;
                } // if rv1 > 0 && url
            } // if ACL_VirtualDbLookup ...
        } // if dbname ...
    }
    return rv;
}

//
// ACL_SetupEval - handle ACL evaluation for the current request
//
// the acllist is evaluated in the request's context against the requested rights
// rq used to be able to be NULL, but not anymore because we need to get the VS from the request...
// if user is non-NULL, just compute the outcome (no request processing), but still log failures.
//
// Parameters:
//  acllist     - the (request's) acllist to check
//  sn          - the request's session
//  rq          - the request itself
//  rights      - array of rights to test against
//  map_generic - pointer to a table mapping protocol specific rights to generic rights
//  user        - already authenticated user or NULL
//
NSAPI_PUBLIC int
ACL_SetupEval(ACLListHandle_t *acllist, Session *sn, Request *rq, char **rights, char **map_generic, const char *user)
{
    int rv = ACL_RES_FAIL;
    NSErr_t err = NSERRINIT;
    NSErr_t *errp = &err;
    ACLEvalHandle_t *acleval;
    char *bong;
    char *bong_type;
    char *acl_tag;
    int  expr_num;
    char *user_str = 0;
    char *cached_user = 0;
    PList_t resource = NULL;
    pb_param *path = NULL;

    // ==================================================================
    // step 1: create and populate the subject and resource plists
    //
    // subject: lifetime = session/request, contains
    //     SESSION - session pointer (if unaccelerated path)
    //     USER (cleared at the end of every request)
    //     CACHED_USER (cleared at the end of every request)
    //     IS_OWNER (cleared at the beginning of every request)
    // resource: lifetime = ACL_SetupEval, contains
    //     REQUEST - request pointer
    //     VS - virtual server

    // subject
    if (!sn->subject) {
        // allocate new subject plist lazily
        if ((sn->subject = PListCreate(sn->pool, ACL_ATTR_INDEX_MAX, NULL, NULL)) == NULL) {
            ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameSubjectPropNotFound));
            return ACL_RES_FAIL;
        }
        // set the session pointer
        rv = PListInitProp(sn->subject, ACL_ATTR_SESSION_INDEX, ACL_ATTR_SESSION, sn, 0);
        if (rv < 0) {
            ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameSubjectPropCannotSet), rv);
            return ACL_RES_FAIL;
        }
    } else {
    	// existing subject list: need to clear the ACL_ATTR_IS_OWNER attribute
        PListDeleteProp(sn->subject, ACL_ATTR_IS_OWNER_INDEX, ACL_ATTR_IS_OWNER);
    }

    // resource
    // this is always created here and deleted at the end of ACL_SetupEval
    if ((resource = PListCreate(sn->pool, ACL_ATTR_INDEX_MAX, NULL, NULL)) == NULL) {
	ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameResourcePropListAllocFailure));
	return ACL_RES_FAIL;
    }

    if (user == (char *)-1) {
        // hackhack
        // we overload the "user" parameter here to bring in one more option:
        // check if authentication is required for this ACL list
        //
        // this is done by setting ACL_ATTR_AUTHREQCHECK_INDEX in resource, but not ACL_ATTR_USER.
        // this way, the attribute getters for ACL_ATTR_USER are going to be called when
        // an authenticated user is required.
        // All attribute getters for ACL_ATTR_USER are supposed to check for ACL_ATTR_AUTHREQCHECK_INDEX,
        // and if present, set its value to "*".
        // XXX move this to ACL_GetAttribute() to avoid having this code in every attr getter?
        // After EvalTestRights, this function will check what the value of ACL_ATTR_AUTHREQCHECK
        // is. If it's still "?", no attribute getter for ACL_ATTR_USER has been called, so no
        // authentication is required. If it's "*", we need authentication.
        //
        PListInitProp(resource, ACL_ATTR_AUTHREQCHECK_INDEX, ACL_ATTR_AUTHREQCHECK, pool_strdup(sn->pool, "?"), 0);
    } else if (user) {
        // might as well set the user ID if we know it
        PListInitProp(sn->subject, ACL_ATTR_USER_INDEX, ACL_ATTR_USER, pool_strdup(sn->pool, user), 0);
        PListInitProp(sn->subject, ACL_ATTR_CACHED_USER_INDEX, ACL_ATTR_CACHED_USER, pool_strdup(sn->pool, user), 0);
    }

    if ((rv = PListInitProp(resource, ACL_ATTR_REQUEST_INDEX, ACL_ATTR_REQUEST, rq, 0)) < 0) {
	ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameResourcePropListCannotAlloc), rv); // XXX errormsg
	PListDestroy(resource);
	return ACL_RES_FAIL;
    }

    // find VS for this rq
    const VirtualServer *vs = request_get_vs(rq);     // argument and return value can be NULL for all we care
    if ((rv = PListInitProp(resource, ACL_ATTR_VS_INDEX, ACL_ATTR_VS, vs, 0)) < 0) {
	ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameResourcePropListCannotAlloc), rv); // XXX errormsg
	PListDestroy(resource);
	return ACL_RES_FAIL;
    }

    // ==================================================================
    // step 2: create an ACL evaluation context on the PERM heap
    // (XXXchrisk - might as well do it on the sn->pool!)
    // (see ACL_EvalDestroy for pool!)
    //
    acleval = ACL_EvalNew(errp, NULL);

    // attach the ACL list
    rv = ACL_EvalSetACL(errp, acleval, acllist);

    // attach the resource and subject property lists to the Evaluation context
    ACL_EvalSetResource(errp, acleval, resource);
    ACL_EvalSetSubject(errp, acleval, sn->subject);

    // ==================================================================
    // step 3: check the acllist against the requested rights
    //
    rv = ACL_EvalTestRights(errp, acleval, rights, map_generic, &bong_type, &bong, &acl_tag, &expr_num);

    // if there's no request, or we authenticate a known user, we're just about done.
    if (rq == NULL || user != NULL)
        goto cleanup;

    // ==================================================================
    // step 4: request result processing
    //
    path = pblock_findkey(pb_key_path, rq->vars);
    
    if (rv == ACL_RES_DENY) {
        // we've been denied, and there's an underlying request
        char *prompt = 0;

        // When this block is all done the prompt will end up in 'buf'
        if (PListGetValue(resource, ACL_ATTR_WWW_AUTH_PROMPT_INDEX, (void **)&prompt, NULL) >= 0) {
            // ==================================================================
            // step 4a: HTTP authentication (basic & digest)
            //

            // basic-auth or digest-auth is requested
            // construct the challenge(s) and generate a 401
            char buf[512];
            char digestbuf[512];
            ACLMethod_t method = ACL_METHOD_INVALID;;
            char *auth;

            if (!prompt) prompt = "";

            // If a method was specified in an ACI, use that method only
            if (PListGetValue(resource, ACL_ATTR_WWW_AUTH_METHOD_INDEX, (void **)&method, NULL) < 0) 
            {
                // not found - just do BasicAuth (backward compatibility...)
                ACL_SetBasicPrompt(prompt, buf, sizeof(buf));
            } else if (ACL_MethodIsEqual(errp, method, ACL_MethodDigest)) {
                // just do DigestAuth
                ACL_SetDigestPrompt(prompt, buf, sizeof(buf), resource);
            } else if (ACL_MethodIsEqual(errp, method, ACL_MethodBasic)) {
                // do both digestauth and basicauth
                ACL_SetBasicPrompt(prompt, buf, sizeof(buf));
                ACL_SetDigestPrompt(prompt, digestbuf, sizeof(digestbuf), resource);
                pblock_nvinsert("www-authenticate", digestbuf, rq->srvhdrs);
            } else if (ACL_MethodIsEqual(errp, method, ACL_MethodGSSAPI)) {
                ACL_SetGSSAPIPrompt(prompt, buf, sizeof(buf));
            } else {
                NS_ASSERT(0);
                strcpy(buf, "invalid");
            }

            // send the "WWW-Authenticate" header with the response
            pblock_nvinsert("www-authenticate", buf, rq->srvhdrs);
            protocol_status(sn, rq, PROTOCOL_UNAUTHORIZED, NULL);
	} else {
            // =================================================================
            // step 4b: plain old denial
            //
            // however, if we have a bong file, things might change
            if (bong) {
                if ( strcmp(bong_type, "url") == 0 ) {
                    // URL bong - redirect the request
                    // We do not have an easy way to check if the bong url and the current url are the same
                    protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
                    pblock_nvinsert("url", bong, rq->vars);
                } else if ( strcmp(bong_type, "uri") == 0 ) {
                    // URI bong - redirect the request after checking for endless loops
                    char* uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
                    if (PL_strcasecmp(uri, bong) == 0) {
                        log_error(LOG_SECURITY, "acl-state", sn, rq, XP_GetAdminStr(DBT_aclFrameAccessDeniedRedirectURL), 
                              bong, acl_tag, expr_num);
                        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
                    } else {
                        char *newloc = http_uri2url_dynamic(bong, "", sn, rq);
                        protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
                        pblock_nvinsert("url", newloc, rq->vars);
                        FREE(newloc);
                   }
                } else { 
                    // unknown bong type
                    ACL_EreportError(errp);
                    log_error(LOG_MISCONFIG, "check-acl", sn, rq, XP_GetAdminStr(DBT_aclFrameAccessDeniedRespTypeUndefined), bong_type);
                    protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
                } 
            } else {
                // no bong file - just plain "forbidden" plus a log entry
                // this is a bit verbose - we log whatever has accumulated in errp, plus the fact that access was denied...
                ACL_EreportError(errp);
                pb_param *path = pblock_findkey(pb_key_path, rq->vars);
                log_error(LOG_SECURITY, "acl-state", sn, rq, XP_GetAdminStr(DBT_aclFrameAccessDenied), path->value, acl_tag, expr_num);
                protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
            }
        }

        // Check if an expiration redirect is applicable, which it might
        // be if authentication was denied due to expired password.
        rv =  handle_expiration_redirect(err, rq, sn, vs, rv);

    } else if (rv != ACL_RES_ALLOW) {
        // Was not a ACL_RES_DENY but neither an ACL_RES_ALLOW, so
        // ACL_RES_FAIL or ACL_RES_INVALID or ACL_RES_NONE
        //
        // ACL evaluation failed, probably due to a misconfiguration.
        // Ex: acls based on dns when DNS is not enabled
        // Treat this as a server error
        ACL_EreportError(errp);
        log_error(LOG_SECURITY, "acl-state", sn, rq, XP_GetAdminStr(DBT_aclFrameAccessDenied1), path->value, acl_tag, expr_num);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);

    } else {
        assert(rv == ACL_RES_ALLOW);
        // While authentication succeeded, an expiration redirect may apply
        // if the password is expiring soon, so check for this condition.
        rv =  handle_expiration_redirect(err, rq, sn, vs, rv);
    }

cleanup:

    // ==================================================================
    // step 5: cleanup
    //

    if (user == (char *)-1) {
        // we are just checking whether authentication will be required.
        char *authreq = 0;

        // so now our return value just depends on whether we called an attr getter
        // for ACL_ATTR_AUTH_USER or not - we do not care about the outcome of the evaluation.

        rv = ACL_RES_FAIL;      // assume the worst
        if (PListGetValue(resource, ACL_ATTR_AUTHREQCHECK_INDEX, (void **)&authreq, NULL) >= 0 && authreq != NULL) {
            switch (authreq[0]) {
            case '?': rv = ACL_RES_ALLOW; break;
            case '*': rv = ACL_RES_DENY; break;
            }
            pool_free(sn->pool, (void *)authreq);
        }
    }

    // get rid of the resource plist (it's just a temporary...)
    PListDestroy(resource);

    // the user and cached-user properties in the subject plist need to be free'd, too.
    user_str = (char *)PListDeleteProp(sn->subject, ACL_ATTR_USER_INDEX, ACL_ATTR_USER);
    if (user_str)
        pool_free(sn->pool, (void *)user_str);
    cached_user = (char *)PListDeleteProp(sn->subject, ACL_ATTR_CACHED_USER_INDEX, ACL_ATTR_CACHED_USER);
    if (cached_user)
        pool_free(sn->pool, (void *)cached_user);

    //
    // now destroy the evaluation context, and we're done.
    // we used to call ACL_EvalDestroy here.
    // ACL_EvalDestroy does what ACL_EvalDestroyNoDecrement() does (free the 
    // evaluation context's private pool), plus call ACL_ListDecrement() on the acllist.
    // To avoid leaking acllists across configuration lifetimes, we need to call
    // ACL_ListDecrement outside of here
    //
    ACL_EvalDestroyNoDecrement(errp, NULL, acleval);

    nserrDispose(errp);
 
    return (rv);
}

/** This method is a wrapper around ACL_SetupEval with http_generic **/
NSAPI_PUBLIC int
ACL_SetupEvalWithHttpGeneric(ACLListHandle_t *acllist, Session *sn, Request *rq,
                             char **rights, const char *user)
{
    return ACL_SetupEval(acllist, sn, rq, rights, http_generic, user);
}
 
NSAPI_PUBLIC int
ACL_InitFrame(void)
{
    return 0;
}

NSAPI_PUBLIC int
ACL_InitFramePostMagnus(void)
{
    return 0;
}

//
// ACL_EreportError
// 
// take all the stuff that accumulated in errp and log it in the error log
//
void
ACL_EreportError(NSErr_t *errp)
{
    char errmsg[1024];

    errmsg[0] = '\0';
    aclErrorFmt(errp, errmsg, 1023, 4);
    errmsg[1023] = '\0';
    if (errmsg[0] != '\0')
	ereport(LOG_SECURITY, errmsg);
    nserrDispose(errp);
}

