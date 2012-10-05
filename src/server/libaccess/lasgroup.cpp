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


/*	lasgroup.c
 *	This file contains the Group LAS code.
 */
#include <stdio.h>
#include <string.h>
#include <netsite.h>
#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclerror.h>
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <base/ereport.h>
#include "usrcache.h"

#ifdef UTEST
extern char *LASGroupGetUser();
#endif /* UTEST */


/*
 *  LASGroupEval
 *    INPUT
 *	attr_name	The string "group" - in lower case.
 *	comparator	CMP_OP_EQ or CMP_OP_NE only
 *	attr_pattern	A comma-separated list of groups
 *	*cachable	Always set to ACL_NOT_CACHABLE
 *      subject		Subjust property list
 *	resource	Resource property list
 *	auth_info	Authentication info, if any
 *    RETURNS
 *	retcode	        The usual LAS return codes.
 */
int LASGroupEval(NSErr_t *errp, char *attr_name, CmpOp_t comparator, 
		 char *attr_pattern, ACLCachable_t *cachable,
                 void **LAS_cookie, PList_t subject, PList_t resource,
                 PList_t auth_info, PList_t global_auth)
{
    char	    *groups = attr_pattern;
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

    if (strcmp(attr_name, ACL_ATTR_GROUP) != 0) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR4900, ACL_Program, 2, XP_GetAdminStr(DBT_lasGroupEvalReceivedRequestForAt_), attr_name);
	return LAS_EVAL_INVALID;
    }

    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR4910, ACL_Program, 2, XP_GetAdminStr(DBT_lasgroupevalIllegalComparatorDN_), comparator_string(comparator));
	return LAS_EVAL_INVALID;
    }

    if (!strcmp(attr_pattern, "anyone")) {
        *cachable = ACL_INDEF_CACHABLE;
        /*
         * Remove WWW-Authenticate header if the ACE is of the form :
         * deny or allow (rights) group = "anyone".
         * Keyword "anyone" when used with "deny" means that nobody
         * should be allowed access, hence we should not prompt for
         * username and password and we should return 403 Forbidden
         * error instead of 401 Unauthorized. We are additonally
         * removing WWW-Authenticate header even in case of allow 
         * also as there is no way to know in this function if its 
         * an allow or deny ACE.
         */
        if (comparator == CMP_OP_EQ)
            PListDeleteProp(resource, ACL_ATTR_WWW_AUTH_PROMPT_INDEX, 
                            ACL_ATTR_WWW_AUTH_PROMPT);
        ereport(LOG_VERBOSE, "acl group: match on group %s (anyone)",
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
	    nserrGenerate(errp, ACLERRFAIL, ACLERR4920, ACL_Program, 2, XP_GetAdminStr(DBT_lasGroupEvalUnableToGetDatabaseName), rv_str);
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

            /* check if userdn is also available */
	    rv = PListGetValue(subject, ACL_ATTR_USERDN_INDEX, (void **)&userdn, NULL);

            if (rv >= 0 && userdn) {
                /* user exists -- check the user's cache */
                rv = acl_usr_cache_groups_check(&user, 0, dbname, groups,
                                                ',', userdn, subj_pool);
                if (rv == LAS_EVAL_TRUE) {
                    ereport(LOG_VERBOSE,
                            "acl group: Using cached match on (%s) for [%s] belonging to userdn [%s]",
                            groups, user, userdn);
                    goto done;
                }
            }
	}
    }

    PListDeleteProp(subject, ACL_ATTR_GROUPS_INDEX, ACL_ATTR_GROUPS);
    PListInitProp(subject, ACL_ATTR_GROUPS_INDEX, ACL_ATTR_GROUPS, groups, 0);

    if (!cached_user) {
	/* There won't be a cert if there is a cached user */
	/* Check if cert->group succeeds (groupOfCertificates) */
	/* ignore error messages */
	rv = ACL_GetAttribute(0, ACL_ATTR_USER_CERT, (void **)&cert, subject,
			      resource, auth_info, global_auth);

	if (rv == LAS_EVAL_TRUE) {
	    /* check if cert->group cache exists */
	    if (usr_cache_enabled) {
		rv = acl_usr_cache_groups_check(&user, cert, dbname, groups,
                                                ',', userdn, subj_pool);
		
		if (rv == LAS_EVAL_TRUE) {
                    ereport(LOG_VERBOSE,
                            "acl group: Using cached match on (%s) for [%s] belonging to userdn [%s]",
                            groups, user, userdn);
                    goto done;
                }
	    }
	    
	    PListDeleteProp(subject, ACL_ATTR_CERT2GROUP_INDEX, ACL_ATTR_CERT2GROUP);
	    /* ignore error messages here as well */
	    rv = ACL_GetAttribute(0, ACL_ATTR_CERT2GROUP, (void **)&member_of,
				  subject, resource, auth_info, global_auth);

	    if (rv == LAS_EVAL_TRUE) goto cache;

	    /* our attr getter for cert2group also checks for userdn at the same time,
	     * if our attr getter got called, we are done (& have failed) */
	    if (userdn && rv != LAS_EVAL_DECLINE) {
		rv = LAS_EVAL_FAIL;
		goto done;
	    }
	}
	else if (rv != LAS_EVAL_DECLINE) {
	    return rv;
	}
	
	/* cert->group didn't succeed */
	/* Get the authenticated user */
	rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&user,
			      subject, resource, auth_info, global_auth);
    
	if (rv != LAS_EVAL_TRUE) {
	    return rv;
	}

        /* Presumably auth-type and auth-user rq->vars have been set */
        set_rqvars = 0;

	if (usr_cache_enabled) {

	    if (userdn == NULL)
	        rv = PListGetValue(subject, ACL_ATTR_USERDN_INDEX, (void **)&userdn, NULL);
	    if (rv >= 0 && userdn) {
	        rv = acl_usr_cache_groups_check(&user, 0, dbname, groups,
                                                ',', userdn, subj_pool);
		if (rv == LAS_EVAL_TRUE) {
			ereport(LOG_VERBOSE,
			        "acl group: Using cached match on (%s) for [%s] belonging to userdn [%s]",
			        groups, user, userdn);
			goto done;
		}
            }
	}
    }
    
    /* not found in the cache or not one of the groups we want */
    PListDeleteProp(subject, ACL_ATTR_USER_ISMEMBER_INDEX, ACL_ATTR_USER_ISMEMBER);
    rv = ACL_GetAttribute(errp, ACL_ATTR_USER_ISMEMBER, (void **)&member_of,
			  subject, resource, auth_info, global_auth);
    
    if (rv != LAS_EVAL_TRUE && rv != LAS_EVAL_FALSE) {
	return rv;
    }
    
cache:
    if (rv == LAS_EVAL_TRUE && usr_cache_enabled) {
	/* User is a member of one of the groups */
	/* update the user's cache */
	acl_usr_cache_set_group(user, cert, dbname, member_of, userdn);
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
	/* User is not a member of any of the groups */
	retcode = (comparator == CMP_OP_EQ ? LAS_EVAL_FALSE : LAS_EVAL_TRUE);
    }

    ereport(LOG_VERBOSE, "acl group: user [%s] belonging to userdn [%s] %s group %s (%s)",
            user,
            userdn ? userdn : "null",
            (retcode == LAS_EVAL_FALSE) ? "does not match"
            : (retcode == LAS_EVAL_TRUE) ? "matched" : "error in",
            (comparator == CMP_OP_EQ) ? "=" : "!=",
            attr_pattern);

    return retcode;
}


/*	LASGroupFlush
 *	Deallocates any memory previously allocated by the LAS
 */
void
LASGroupFlush(void **las_cookie)
{
    /* do nothing */
    return;
}
