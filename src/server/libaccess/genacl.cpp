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

#include <netsite.h>
#include <base/ereport.h>
#include <base/util.h>                  // util_strtok
#include <libaccess/acl.h>              // generic ACL definitions
#include <libaccess/aclproto.h>         // internal prototypes
#include <libaccess/aclglobal.h>        // global data
#include <libaccess/aclerror.h>         // error codes
#include <libaccess/genacl.h>           // prototypes for local functions
#include "aclpriv.h"                    // internal data structure definitions
#include <libaccess/dbtlibaccess.h>     // strings
#include <frame/acl.h>                  // for ACL_EreportError
#include <frame/conf.h>
#include "plhash.h"
#include "plstr.h"

#define BIG_LINE 1024

//
// get_is_owner_default - default attribute getter for ACL_ATTR_IS_OWNER
//
// always sets ACL_ATTR_IS_OWNER to "true"
//
NSAPI_PUBLIC int
get_is_owner_default (NSErr_t *errp, PList_t subject,
			         PList_t resource, PList_t auth_info,
                                PList_t global_auth, void *unused)
{
    /* Make sure we don't generate error "all getters declined" message from
     * ACL_GetAttribute.
     */
    PListInitProp(subject, ACL_ATTR_IS_OWNER_INDEX, ACL_ATTR_IS_OWNER, "true", 0);

    return LAS_EVAL_TRUE;
}

//
// acl_groupcheck - check if user is member in a group.
//
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// this is a legacy function - it will not work correctly anymore because the VS is needed
// to find out the correct database
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
//
// works with all db types
// consults user and group cache
//
// parameters:
// user    - username
// cert    - user certificate or NULL (for groupOfCertificates matching with ssl method)
// group   - group to check
// dbname  - name of database to check or NULL, defaults to the default database name
// method  - method to check with or NULL, defaults to "basic"
// logerr  - 1 if errors should be logged in the error log, 0 otherwise
//
NSAPI_PUBLIC int
acl_groupcheck(const char *user, const void *cert, const char *group, const char *dbname, const char *method, const int logerr)
{
    NSErr_t err = NSERRINIT;
    NSErr_t *errp = &err;
    pool_handle_t *aclgcpool = 0;
    ACLCachable_t cachable;
    void *LAS_cookie;
    int rv, ret = PR_FALSE;
    PList_t subject = 0, resource = 0, auth_info = 0, global_auth = 0;
    ACLMethod_t aclmeth;
    char *pGroup;

    // sanity - cert, dbname and method can be NULL
    if (!user || !group)
	return PR_FALSE;

    // set up a fake ACL environment
    aclgcpool = pool_create();
    subject = PListCreate(aclgcpool, ACL_ATTR_INDEX_MAX, 0, 0);
    resource = PListCreate(aclgcpool, ACL_ATTR_INDEX_MAX, 0, 0);
    auth_info = PListCreate(aclgcpool, ACL_ATTR_INDEX_MAX, 0, 0);
    global_auth = 0;
    if ((pool_enabled() && !aclgcpool) || !subject || !resource || !auth_info) {
	goto err;
    }

    // set up the USER and CERT properties - the only thing we know now
    rv = PListInitProp(subject, ACL_ATTR_USER_INDEX, ACL_ATTR_USER, user, 0);
    if (rv < 0) { goto err; }

    if (cert) {
	rv = PListInitProp(subject, ACL_ATTR_USER_CERT_INDEX, ACL_ATTR_USER_CERT, cert, 0);
	if (rv < 0) { goto err; }
    }

    // set DBNAME and DBTYPE attributes in auth_info
    rv = ACL_AuthInfoSetDbname(errp, auth_info, dbname);
    if (rv < 0) { goto err; }

    // set method
    rv = ACL_MethodFind(errp, method ? method : "basic", &aclmeth);
    if (rv < 0) { goto err; }

    rv = ACL_AuthInfoSetMethod(errp, auth_info, aclmeth);
    if (rv < 0) { goto err; }

    // now see if user exists at all
    // on ldap, this gets you the userdn, too
    rv = ACL_GetAttribute(errp, ACL_ATTR_USER_EXISTS, (void **)&user,
	subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) { goto err; }

    // LASGroupEval has a char * in the prototype, so we need to dup it
    pGroup = pool_strdup(aclgcpool, group);

    // and here's the full group check
    rv = LASGroupEval(errp, ACL_ATTR_GROUP, CMP_OP_EQ, pGroup,
		&cachable, &LAS_cookie,
		subject, resource, auth_info, global_auth);

    if (rv == LAS_EVAL_TRUE) { ret = PR_TRUE; }

err:
    if (logerr) {
	char buf[BIG_LINE];
	aclErrorFmt(errp, buf, BIG_LINE, 6);
	ereport(LOG_SECURITY, "Error while checking group membership of %s in %s: %s", user, group, buf);
    }
    nserrDispose(errp);

    // free the stuff
    if (subject) PListDestroy(subject);
    if (resource) PListDestroy(resource);
    if (auth_info) PListDestroy(auth_info);
    if (aclgcpool) pool_destroy(aclgcpool);

    return (ret);
}

//
// ACL_Authenticate - check if username and password authenticate correctly
//
// works with all db types
// consults user and group cache
//
// parameters:
// user     - username
// password - user certificate or NULL (for groupOfCertificates matching with ssl method)
// vs       - the virtual server to authenticate against
// dbname   - name of database to check or NULL, defaults to the default database name
// logerr   - 1 if errors should be logged in the error log, 0 otherwise
//
NSAPI_PUBLIC int
ACL_Authenticate(const char *user, const char *password, const VirtualServer *vs, const char *dbname, const int logerr)
{
    NSErr_t err = NSERRINIT;
    NSErr_t *errp = &err;
    pool_handle_t *aclgcpool = 0;
    int rv, ret = PR_FALSE;
    PList_t subject = 0, resource = 0, auth_info = 0, global_auth = 0;
    ACLMethod_t aclmeth;
    char *raw_user;

    // sanity - dbname can be NULL
    if (!user || !password)
	return PR_FALSE;

    // set up a fake ACL environment
    aclgcpool = pool_create();
    subject = PListCreate(aclgcpool, ACL_ATTR_INDEX_MAX, 0, 0);
    resource = PListCreate(aclgcpool, ACL_ATTR_INDEX_MAX, 0, 0);
    auth_info = PListCreate(aclgcpool, ACL_ATTR_INDEX_MAX, 0, 0);
    global_auth = 0;
    if ((pool_enabled() && !aclgcpool) || !subject || !resource || !auth_info) {
	goto err;
    }

    // set up the RAW_USER, RAW_PASSWORD and VS properties - the only things we know now
    if (PListInitProp(subject, ACL_ATTR_RAW_USER_INDEX, ACL_ATTR_RAW_USER, user, 0) < 0 ||
        PListInitProp(subject, ACL_ATTR_RAW_PASSWORD_INDEX, ACL_ATTR_RAW_PASSWORD, password, 0) < 0 ||
        PListInitProp(resource, ACL_ATTR_VS_INDEX, ACL_ATTR_VS, vs, 0) < 0)
        goto err;

    // set DBNAME and DBTYPE attributes in auth_info and resource
    if (ACL_AuthInfoSetDbname(errp, auth_info, dbname) < 0)
        goto err;
    // set up DB information in resource for ACL_GetAttribute etc.
    // ACL_ATTR_VS must be set for this to work
    ACL_CacheEvalInfo(errp, resource, auth_info);

    // set method to "basic"
    if (ACL_MethodFind(errp, "basic", &aclmeth) < 0 ||
        ACL_AuthInfoSetMethod(errp, auth_info, aclmeth) < 0)
        goto err;

    rv = ACL_GetAttribute(errp, ACL_ATTR_IS_VALID_PASSWORD, (void **)&raw_user,
                                    subject, resource, auth_info, global_auth);
    if (rv == LAS_EVAL_TRUE) { ret = PR_TRUE; }

err:
    if (rv != LAS_EVAL_TRUE && rv != LAS_EVAL_FALSE && logerr) {
	char buf[BIG_LINE];
	aclErrorFmt(errp, buf, BIG_LINE, 6);
	ereport(LOG_SECURITY, "Error while checking authentication of %s in %s: %s", user, dbname, buf);
    }
    nserrDispose(errp);

    // free the stuff
    if (subject) PListDestroy(subject);
    if (resource) PListDestroy(resource);
    if (auth_info) PListDestroy(auth_info);
    if (aclgcpool) pool_destroy(aclgcpool);

    return (ret);
}

static inline void append(char* &p, const char *e, const char *s)
{
    while (*s && p < e) *p++ = *s++;
}

static inline void appendc(char* &p, const char *e, char c)
{
    if (p < e) *p++ = c;
}

//
// ACL_IsUserInRole - java ACL interface function
//
// works with all db types
// consults user and group cache
//
// parameters:
// user        - username
// matchWith   - type of check - 1 = user exists, 2 = user is member of group, 3 = user is in role
// grouporrole - group or role to check
// vs          - virtual server to check against
// dbname      - name of database to check or NULL, defaults to the default database name
// logerr      - 1 if errors should be logged in the error log, 0 otherwise
//
NSAPI_PUBLIC int
ACL_IsUserInRole(const char *user, int matchWith, const char *grouporrole, const VirtualServer *vs, const char *dbname, const int logerr)
{
    NSErr_t err = NSERRINIT;
    NSErr_t *errp = &err;
    pool_handle_t *temppool = 0;
    ACLCachable_t cachable;
    void *LAS_cookie;
    int rv = LAS_EVAL_FAIL, ret = PR_FALSE;
    PList_t subject = 0, resource = 0, auth_info = 0, global_auth = 0;
    ACLMethod_t aclmeth;
    char *pGroup, *pRole;

    // sanity - dbname and method can be NULL
    if (!user || !grouporrole)
	return PR_FALSE;

    if ((temppool = pool_create()) == NULL && pool_enabled())
        goto err;
    
    // set up a fake ACL environment
    subject = PListCreate(temppool, ACL_ATTR_INDEX_MAX, 0, 0);
    resource = PListCreate(temppool, ACL_ATTR_INDEX_MAX, 0, 0);
    auth_info = PListCreate(temppool, ACL_ATTR_INDEX_MAX, 0, 0);
    global_auth = 0;
    if (!subject || !resource || !auth_info)
	goto err;

    // set up the USER and VS properties - the only things we know now
    if (PListInitProp(subject, ACL_ATTR_USER_INDEX, ACL_ATTR_USER, user, 0) < 0 ||
        PListInitProp(resource, ACL_ATTR_VS_INDEX, ACL_ATTR_VS, vs, 0) < 0)
        goto err;

    // set DBNAME and DBTYPE attributes in auth_info
    if (ACL_AuthInfoSetDbname(errp, auth_info, dbname) < 0)
        goto err;

    // set up DB information in resource for ACL_GetAttribute etc.
    // ACL_ATTR_VS must be set for this to work
    ACL_CacheEvalInfo(errp, resource, auth_info);

    // set method - always use basic
    if (ACL_MethodFind(errp, "basic", &aclmeth) < 0 ||
        ACL_AuthInfoSetMethod(errp, auth_info, aclmeth) < 0)
    {
        goto err;
    }

    // now see if user exists at all
    // on ldap, this sets ACL_ATTR_USERDN, too
    rv = ACL_GetAttribute(errp, ACL_ATTR_USER_EXISTS, (void **)&user,
	subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) { goto err; }
        
    switch (matchWith) {
    case 1:             // just check if user exists, and we did that already, so we're done
        ret = PR_TRUE;
        break;
    case 2:
        // LASGroupEval has a char * in the prototype, so we need to dup it
        pGroup = pool_strdup(temppool, grouporrole);

        // and here's the full group check
        rv = LASGroupEval(errp, ACL_ATTR_GROUP, CMP_OP_EQ, pGroup,
                    &cachable, &LAS_cookie,
                    subject, resource, auth_info, global_auth);

        if (rv == LAS_EVAL_TRUE) { ret = PR_TRUE; }
        break;
    case 3:
        // LASRoleEval has a char * in the prototype, so we need to dup it
        pRole = pool_strdup(temppool, grouporrole);

        // and here's the full role check
        rv = LASRoleEval(errp, ACL_ATTR_ROLE, CMP_OP_EQ, pRole,
                    &cachable, &LAS_cookie,
                    subject, resource, auth_info, global_auth);

        if (rv == LAS_EVAL_TRUE) { ret = PR_TRUE; }
        break;
    
    default:
        break;
    }

err:
    if (logerr && rv != LAS_EVAL_TRUE && rv != LAS_EVAL_FALSE) {
	char buf[BIG_LINE];
	aclErrorFmt(errp, buf, BIG_LINE, 6);
	ereport(LOG_SECURITY, "Error while checking role membership of %s in %s: %s", user, grouporrole, buf);
    }

    nserrDispose(errp);

    // free the stuff
    if (subject) PListDestroy(subject);
    if (resource) PListDestroy(resource);
    if (auth_info) PListDestroy(auth_info);
    if (temppool) pool_destroy(temppool);

    return (ret);
}
