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

/*	lasuser.c
 *	This file contains the User LAS code.
 */

#include <netsite.h>
#include <base/shexp.h>
#include <base/util.h>
#include <base/ereport.h>
#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclerror.h>
#include "aclpriv.h"
#include <libaccess/dacl.h>

/*
 *  LASUserEval
 *    INPUT
 *	attr_name	The string "user" - in lower case.
 *	comparator	CMP_OP_EQ or CMP_OP_NE only
 *	attr_pattern	A comma-separated list of users
 *	*cachable	Always set to ACL_NOT_CACHABLE.
 *      subject		Subject property list
 *      resource        Resource property list
 *      auth_info	Authentication info, if any
 *    RETURNS
 *	retcode	        The usual LAS return codes.
 */
int LASUserEval(NSErr_t *errp, char *attr_name, CmpOp_t comparator, 
		char *attr_pattern, ACLCachable_t *cachable,
                void **LAS_cookie, PList_t subject, PList_t resource,
                PList_t auth_info, PList_t global_auth)
{
    char	    *uid;
    char	    *users;
    char	    *user;
    char	    *comma;
    int		    retcode;
    int		    matched;
    int		    rv;

    *cachable = ACL_NOT_CACHABLE;
    *LAS_cookie = (void *)0;

    if (strcmp(attr_name, ACL_ATTR_USER) != 0) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR5700, ACL_Program, 2, XP_GetAdminStr(DBT_lasUserEvalReceivedRequestForAtt_), attr_name);
	return LAS_EVAL_INVALID;
    }

    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR5710, ACL_Program, 2, XP_GetAdminStr(DBT_lasuserevalIllegalComparatorDN_), comparator_string(comparator));
	return LAS_EVAL_INVALID;
    }

    if (!strcmp(attr_pattern, "anyone")) {
        *cachable = ACL_INDEF_CACHABLE;
        /*
         * Remove WWW-Authenticate header if the ACE is of the form :
         * deny or allow (rights) user = "anyone".
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
        ereport(LOG_VERBOSE, "acl user: match on user %s (anyone)",
                (comparator == CMP_OP_EQ) ? "=" : "!=");
	return comparator == CMP_OP_EQ ? LAS_EVAL_TRUE : LAS_EVAL_FALSE;
    }

    // get the authenticated user name
    rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&uid, subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) {
	return rv;
    }

    // we have an authenticated user, so if the pattern is "all",
    // we're done.
    if (!strcmp(attr_pattern, "all")) {
        ereport(LOG_VERBOSE, "acl user: match on user %s (all)",
                (comparator == CMP_OP_EQ) ? "=" : "!=");
	return comparator == CMP_OP_EQ ? LAS_EVAL_TRUE : LAS_EVAL_FALSE;
    }

    // copy the string so we can mangle it to create null-delimited strings
    // we would not need that if WILDPAT_CASECMP would take a length
    if ((users = STRDUP(attr_pattern)) == NULL) {
	nserrGenerate(errp, ACLERRNOMEM, ACLERR5720, ACL_Program, 1,
	XP_GetAdminStr(DBT_lasuserevalRanOutOfMemoryN_));
	return LAS_EVAL_FAIL;
    }

    // walk the list of users to see if uid is one of them
    // XXX: see 6226967. also whitespace cleanup should be done earlier,once
    user = users;
    matched = 0;
    while (user != 0 && *user != 0 && !matched) {
	if ((comma = strchr(user, ',')) != NULL) {
	    *comma++ = 0;
	}

	/* ignore leading whitespace */
	while(*user == ' ' || *user == '\t') user++;

	if (*user) {
	    /* ignore trailing whitespace */
	    int len = strlen(user);
	    char *ptr = user+len-1;

	    while(*ptr == ' ' || *ptr == '\t') *ptr-- = 0;
	}

	if (!matched) {
            if (!WILDPAT_CASECMP(uid, user)) {
                /* uid is one of the users */
                matched = 1;
            } else {
                /* continue checking for next user */
                user = comma;
            }
	}
    }

    if (comparator == CMP_OP_EQ) {
	retcode = (matched ? LAS_EVAL_TRUE : LAS_EVAL_FALSE);
    } else {
	retcode = (matched ? LAS_EVAL_FALSE : LAS_EVAL_TRUE);
    }

    ereport(LOG_VERBOSE, "acl user: user [%s] %s user %s (%s)",
            uid,
            (retcode == LAS_EVAL_FALSE) ? "does not match"
            : (retcode == LAS_EVAL_TRUE) ? "matched" : "error in",
            (comparator == CMP_OP_EQ) ? "=" : "!=",
            attr_pattern);
        
    FREE(users);
    return retcode;
}


/*	LASUserFlush
 *	Deallocates any memory previously allocated by the LAS
 */
void
LASUserFlush(void **las_cookie)
{
    /* do nothing */
    return;
}
