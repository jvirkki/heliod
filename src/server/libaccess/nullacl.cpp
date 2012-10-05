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


/* #define DBG_PRINT */

#include <netsite.h>
#include <base/ereport.h>
#include <base/util.h>
#include <base/shexp.h>
#include <base/nsassert.h>

#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclglobal.h>
#include <libaccess/aclerror.h>
#include <libaccess/nullacl.h>

#include <ldaputil/errors.h>
#include <ldaputil/certmap.h>
#include <ldaputil/dbconf.h>

#define NULL_URL_PREFIX		"null:"
#define NULL_URL_PREFIX_LEN	5

//
// nullacl.cpp - NULL database specific functions for the ACL subsystem
//
// this file contains:
//
// get_is_valid_password_null       - attribute getter for ACL_ATTR_IS_VALID_PASSWORD
// get_user_exists_null             - attribute getter for ACL_ATTR_USER_EXISTS
// get_user_ismember_null           - attribute getter for ACL_ATTR_USER_ISMEMBER
// get_user_isinrole_null           - attribute getter for ACL_ATTR_USER_ISINROLE
//
// parse_null_url                   - parser for dbswitch.conf null database urls
// 

//
// parse_null_url - parser for dbswitch.conf database urls with the "null" method
//
// there are two possible URLs:
//   null:///all  - requests authentication & authenticates all user/password values
//   null:///none - requests authentication & authenticates NO user/password values
//
NSAPI_PUBLIC int
parse_null_url (NSErr_t *errp, ACLDbType_t, const char *dbname, const char *url,
		 PList_t plist, void **db)
{
    const char *rest;

    *db = 0;

    if (!url || !*url) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR6200, ACL_Program, 1, XP_GetAdminStr(DBT_nullaclDatabaseUrlIsMissing));
	return -1;
    }

    if (!dbname || !*dbname) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR6210, ACL_Program, 1, XP_GetAdminStr(DBT_nullaclDatabaseNameIsMissing));
	return -1;
    }

    // find out if we're null:///all or null:///none
    if (strncmp(url, NULL_URL_PREFIX, NULL_URL_PREFIX_LEN)) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR6220, ACL_Program, 1, XP_GetAdminStr(DBT_nullaclMustHaveNullPrefix));
	return -1;
    }

    rest = url + NULL_URL_PREFIX_LEN;

    if (strncmp(rest, "///", 3) != 0) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR6230, ACL_Program, 1, XP_GetAdminStr(DBT_nullaclMustHaveSlashes));
	return -1;
    }

    rest += 3;

    // we don't need no fancy data structures
    if (strcmp(rest, "all") == 0)
	*db = (void *)NULLDB_ALL;
    else if (strcmp(rest, "none") == 0)
	*db = (void *)NULLDB_NONE;
    else {
	nserrGenerate(errp, ACLERRINVAL, ACLERR6240, ACL_Program, 1, XP_GetAdminStr(DBT_nullaclOnlyAllOrNone));
	return -1;
    }

    return 0;
}

//
// get_is_valid_password_basic_null - attribute getter for ACL_ATTR_IS_VALID_PASSWORD
//
// This subroutine authenticates a user/password pair against the current database
// The user must be present, have a password and the authentication must succeed.
// For "null:" databases, the result is already coded in the URL, but we check for
// correct data anyway.
// 
int
get_is_valid_password_null (NSErr_t *errp, PList_t subject,
				      PList_t resource, PList_t auth_info,
				      PList_t global_auth, void *unused)
{
    char *raw_user = 0;
    char *raw_pw = 0;
    int rv;
    char *dbname = 0;
    ACLDbType_t dbtype;
    size_t answer = 0;          // For 64 bit, must be the sizeof pointer.

    // we don't really care what the raw user and password are
    // as the result of our authentication is determined already,
    // but they have to be present and valid, else the authentication fails
    if ((rv = ACL_GetAttribute(errp, ACL_ATTR_RAW_USER, (void **)&raw_user,
			  subject, resource, auth_info, global_auth)) != LAS_EVAL_TRUE) {
	return rv;
    }

    if ((rv = ACL_GetAttribute(errp, ACL_ATTR_RAW_PASSWORD, (void **)&raw_pw,
			  subject, resource, auth_info, global_auth)) != LAS_EVAL_TRUE) {
	return rv;
    }

    if (!raw_user || !raw_pw) {
	return LAS_EVAL_FALSE;
    }

    // get the user database name out of the ACL
    rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
    if (rv < 0) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_nullACLDBNameErr), rv);
        return LAS_EVAL_FAIL;
    }

    // find the authentication database
    rv = ACL_DatabaseFind(errp, dbname,  &dbtype, (void **)&answer);
    if (rv != LAS_EVAL_TRUE) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_nullUserInfoErr), dbname, rv);
        return rv;
    }
    NS_ASSERT(answer == NULLDB_ALL || answer == NULLDB_NONE);

    switch (answer) {
    case NULLDB_ALL:
	// we accept everyone
	PListInitProp(subject, ACL_ATTR_IS_VALID_PASSWORD_INDEX,
                  ACL_ATTR_IS_VALID_PASSWORD, raw_user, 0);
        ereport(LOG_VERBOSE, "null authdb: Authentication succeeded for [%s]",
                raw_user);
	return LAS_EVAL_TRUE;
        
    case NULLDB_NONE:
    default:
	// we deny everyone
        ereport(LOG_VERBOSE, "null authdb: Authentication failed for [%s]",
                raw_user);
	return LAS_EVAL_FALSE;
    }
}

//
// get_user_exists_null - attribute getter for ACL_ATTR_USER_EXISTS
//
// This subroutine checks if a user is present in the current database
// 
int
get_user_exists_null (NSErr_t *errp, PList_t subject,
			  PList_t resource, PList_t auth_info,
			  PList_t global_auth, void *unused)
{
    int rv;
    char *user;

    /* See if the user name is available */
    rv = PListGetValue(subject, ACL_ATTR_USER_INDEX, (void **)&user, NULL);

    if (rv >= 0) {
	// yes, it is there.
	char *dbname;
	ACLDbType_t dbtype;
	size_t answer = 0;      // For 64 bit, must be the sizeof pointer.

        // get the database name out of the ACL (or default)
	rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
	if (rv < 0) {
            ereport(LOG_SECURITY, XP_GetAdminStr(DBT_nullDBNameErr2), rv);
	    return LAS_EVAL_FAIL;
	}

	// find the authentication database
        rv = ACL_DatabaseFind(errp, dbname,  &dbtype, (void **)&answer);
        if (rv != LAS_EVAL_TRUE) {
            ereport(LOG_SECURITY, XP_GetAdminStr(DBT_nullUserInfoErr2), dbname, rv);
            return rv;
        }

	NS_ASSERT(answer == NULLDB_ALL || answer == NULLDB_NONE);

	switch (answer) {
	case NULLDB_ALL:
            ereport(LOG_VERBOSE, "null authdb: user [%s] exists", user);
	    rv = LAS_EVAL_TRUE;

	case NULLDB_NONE:
            ereport(LOG_VERBOSE, "null authdb: user [%s] does not exist",
                    user);
	    rv = LAS_EVAL_FALSE;
	}
    }

    /* If we can get the user then the user exists */
    if (rv == LAS_EVAL_TRUE) {
	PListInitProp(subject, ACL_ATTR_USER_EXISTS_INDEX, ACL_ATTR_USER_EXISTS, user, 0);
    }

    return rv;
}

//
// get_user_ismember_null - attribute getter for ACL_ATTR_USER_ISMEMBER
//
int
get_user_ismember_null (NSErr_t *errp, PList_t subject,
			    PList_t resource, PList_t auth_info,
			    PList_t global_auth, void *unused)
{
    int rv;
    char *user = 0;
    char *groups = 0;
    char *member_of = 0;
    char *dbname = 0;
    ACLDbType_t dbtype;
    size_t answer = 0;          // For 64 bit, must be the sizeof pointer.
    const char *token;
    int tlen = 0;

    /* Get the authenticated user name */
    if ((rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&user, subject,
			  resource, auth_info, global_auth)) != LAS_EVAL_TRUE)
    {
	return rv;
    }

    /* Get the list of groups of interest, set up by LASGroupEval() */
    if (ACL_GetAttribute(errp, ACL_ATTR_GROUPS, (void **)&groups, subject,
			  resource, auth_info, global_auth) < 0)
    {
	return LAS_EVAL_FAIL;
    }

    if (PListGetValue(resource, ACL_ATTR_DATABASE_INDEX, (void **)&answer, 0) < 0) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_nullUserInfoErr3), dbname, rv);
        return LAS_EVAL_FAIL;
    }
    // must be either one
    NS_ASSERT(answer == NULLDB_ALL || answer == NULLDB_NONE);

    /* Loop through the list of groups we want to consider */
    token = groups;
    for (;;) {

        /* Get the next group name from the list */
        token = acl_next_token_len(token, ',', &tlen);
        if (!token || (tlen < 0))
            break;

        if (tlen > 0) {
	    /* we take the first token and return that as matching */
	    member_of = (char *)token;
	    break;
        }

        if (token)
            ++token;
    }


    switch (answer) {
    case NULLDB_ALL:
	// ok, user is a member of any available group, so we match the first one
	if (member_of) {
	    PListInitProp(subject, ACL_ATTR_USER_ISMEMBER_INDEX,
			  ACL_ATTR_USER_ISMEMBER,
			  pool_strdup(PListGetPool(subject), member_of), 0);
            ereport(LOG_VERBOSE,
                    "null authdb: User [%s] matched group [%s]",
                    user, member_of);
	    return LAS_EVAL_TRUE;
	} else {
            ereport(LOG_VERBOSE,
                    "null authdb: User [%s] is not in group [%s]",
                    user, groups);            
	    return LAS_EVAL_FALSE;
        }
        
    case NULLDB_NONE:
    default:
        ereport(LOG_VERBOSE,
                "null authdb: User [%s] is not in group [%s]",
                user, groups);            
        return LAS_EVAL_FALSE;
    }
}

//
// get_user_isinrole_null - attribute getter for ACL_ATTR_USER_ISINROLE
//
int
get_user_isinrole_null (NSErr_t *errp, PList_t subject,
			    PList_t resource, PList_t auth_info,
			    PList_t global_auth, void *unused)
{
    int rv;
    char *user = 0;
    char *roles = 0;
    char *in_role = 0;
    size_t answer = 0;          // For 64 bit, must be the sizeof pointer.
    const char *token;
    int tlen = 0;

    /* Get the authenticated user name */
    if ((rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&user, subject,
			  resource, auth_info, global_auth)) != LAS_EVAL_TRUE)
    {
	return rv;
    }

    /* Get the list of roles of interest, set up by LASRoleEval() */
    if (ACL_GetAttribute(errp, ACL_ATTR_ROLES, (void **)&roles, subject,
			  resource, auth_info, global_auth) < 0)
    {
	return LAS_EVAL_FAIL;
    }

    // get dbhandle from resource plist (set up by attr getter)
    if ((rv = PListGetValue(resource, ACL_ATTR_DATABASE_INDEX, (void **)&answer, 0)) < 0) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_nullIsInRoleUnableToGetDbhandle), rv);
        return LAS_EVAL_FAIL;
    }
    // must be either one
    NS_ASSERT(answer == NULLDB_ALL || answer == NULLDB_NONE);

    /* Loop through the list of roles we want to consider */
    token = roles;
    for (;;) {

        /* Get the next group name from the list */
        token = acl_next_token_len(token, ',', &tlen);
        if (!token || (tlen < 0))
            break;

        if (tlen > 0) {
	    /* we take the first token and return that as matching */
	    in_role = (char *)token;
	    break;
        }

        if (token)
            ++token;
    }

    switch (answer) {
    case NULLDB_ALL:
	// ok, user is in any available role, so we match the first one
	if (in_role) {
	    PListInitProp(subject, ACL_ATTR_USER_ISINROLE_INDEX,
			  ACL_ATTR_USER_ISINROLE,
			  pool_strdup(PListGetPool(subject), in_role), 0);
            ereport(LOG_VERBOSE,
                    "null authdb: User [%s] matched role [%s]",
                    user, in_role);
	    return LAS_EVAL_TRUE;
            
	} else {
            ereport(LOG_VERBOSE,
                    "null authdb: User [%s] is not in roles [%s]",
                    user, roles);
	    return LAS_EVAL_FALSE;
        }
        
    case NULLDB_NONE:
    default:
        ereport(LOG_VERBOSE,
                "null authdb: User [%s] is not in roles [%s]",
                user, roles);
        return LAS_EVAL_FALSE;
    }
}
