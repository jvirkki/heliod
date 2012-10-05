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

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 1996 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

*/


#include <stdio.h>
#include <string.h>
#include <netsite.h>
#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclerror.h>
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include "usrcache.h"
#include <base/ereport.h>


/*
 *  LASRoleEval
 *    INPUT
 *	attr_name	The string "role" - in lower case.
 *	comparator	CMP_OP_EQ or CMP_OP_NE only
 *	attr_pattern	A comma-separated list of roles
 *	*cachable	Always set to ACL_NOT_CACHABLE
 *      subject		Subject property list
 *	resource	Resource property list
 *	auth_info	Authentication info, if any
 *    RETURNS
 *	retcode	        The usual LAS return codes.
 */
int LASRoleEval(NSErr_t *errp, char *attr_name, CmpOp_t comparator, 
		 char *attr_pattern, ACLCachable_t *cachable,
                 void **LAS_cookie, PList_t subject, PList_t resource,
                 PList_t auth_info, PList_t global_auth)
{
    char	    *roles = attr_pattern;
    int		    retcode;
    int		    set_rqvars = 1;
    char	    *member_of = 0;
    char	    *user = 0;
    char	    *cached_user = 0;
    char	    *userdn = 0;
    char	    *dbname = 0;
    void	    *cert = 0;
    int		    rv;
    int		    usr_cache_enabled = acl_usr_cache_enabled();
    pool_handle_t  *subj_pool = PListGetPool(subject);

    *cachable = ACL_NOT_CACHABLE;
    *LAS_cookie = (void *)0;

    if (strcmp(attr_name, ACL_ATTR_ROLE) != 0) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR4900, ACL_Program, 2, XP_GetAdminStr(DBT_lasRoleEvalReceivedRequestForAt_), attr_name);
	return LAS_EVAL_INVALID;
    }

    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR4910, ACL_Program, 2, XP_GetAdminStr(DBT_lasRoleEvalIllegalComparatorDN_), comparator_string(comparator));
	return LAS_EVAL_INVALID;
    }

    if (!strcmp(attr_pattern, "anyone")) {
        *cachable = ACL_INDEF_CACHABLE;
        ereport(LOG_VERBOSE, "acl role: match on role %s (anyone)",
                (comparator == CMP_OP_EQ) ? "=" : "!=");
	return comparator == CMP_OP_EQ ? LAS_EVAL_TRUE : LAS_EVAL_FALSE;
    }

    /* Cached user is used by Agents & others to test if a user has certain
     * privileges.  The user may not be accessing the server at this time. */
    rv = PListGetValue(subject, ACL_ATTR_CACHED_USER_INDEX, (void **)&cached_user, NULL);

    if (usr_cache_enabled) {
	rv = PListGetValue(resource, ACL_ATTR_DBNAME_INDEX, (void **)&dbname, NULL);
	if (rv < 0) {
	    char rv_str[16];
	    sprintf(rv_str, "%d", rv);
	    nserrGenerate(errp, ACLERRFAIL, ACLERR4920, ACL_Program, 2, XP_GetAdminStr(DBT_lasRoleEvalUnableToGetDatabaseName), rv_str);
	    return LAS_EVAL_FAIL;
	}
    
	if (cached_user) {
	    user = cached_user;
	} else {
	    /* If the authenticated user already exists, check the cache first */
	    rv = PListGetValue(subject, ACL_ATTR_USER_INDEX, (void **)&user, NULL);
            if (user) {
                /* Presumably auth-type and auth-user rq->vars have been set */
                set_rqvars = 0;
            }
	}
	
	if (user) {
	    /* user exists -- check the user's cache */
	    rv = PListGetValue(subject, ACL_ATTR_USERDN_INDEX, (void **)&userdn, NULL);
	}
    }

    PListDeleteProp(subject, ACL_ATTR_ROLES_INDEX, ACL_ATTR_ROLES);
    PListInitProp(subject, ACL_ATTR_ROLES_INDEX, ACL_ATTR_ROLES, roles, 0);
    if (!cached_user) {
	/* Get the authenticated user */
	rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&user,
			      subject, resource, auth_info, global_auth);
    
	if (rv != LAS_EVAL_TRUE) {
	    return rv;
	}

        /* Presumably auth-type and auth-user rq->vars have been set */
        set_rqvars = 0;
    }
    
    /* role cache not implemented yet, flush and fetch the attribute */
    PListDeleteProp(subject, ACL_ATTR_USER_ISINROLE_INDEX, ACL_ATTR_USER_ISINROLE);
    rv = ACL_GetAttribute(errp, ACL_ATTR_USER_ISINROLE, (void **)&member_of,
			  subject, resource, auth_info, global_auth);
    
    if (rv != LAS_EVAL_TRUE && rv != LAS_EVAL_FALSE) {
	return rv;
    }
    
done:
    if (rv == LAS_EVAL_TRUE) {
        /* Need to set auth-type and auth-user rq->vars here sometimes */
        if (set_rqvars && user) {
            Request *rq = 0;
            rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, (void **)&rq, NULL);
            if (rq) {
                pblock_nvinsert("auth-type", (cert) ? "ssl" : "basic", rq->vars);
                pblock_nvinsert("auth-user", user, rq->vars);
            }
        }
	retcode = (comparator == CMP_OP_EQ ? LAS_EVAL_TRUE : LAS_EVAL_FALSE);
    }
    else {
	/* User is not a member of any of the roles */
	retcode = (comparator == CMP_OP_EQ ? LAS_EVAL_FALSE : LAS_EVAL_TRUE);
    }

    ereport(LOG_VERBOSE, "acl role: user [%s] %s roles %s (%s)",
            user,
            (retcode == LAS_EVAL_FALSE) ? "does not match"
            : (retcode == LAS_EVAL_TRUE) ? "matched" : "error in",
            (comparator == CMP_OP_EQ) ? "=" : "!=",
            attr_pattern);

    return retcode;
}


/*	LASRoleFlush
 *	Deallocates any memory previously allocated by the LAS
 */
void
LASRoleFlush(void **las_cookie)
{
    /* do nothing */
    return;
}
