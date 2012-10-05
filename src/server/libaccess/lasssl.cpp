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

#include <netsite.h>
#include <base/util.h>
#include <base/plist.h>
#include <base/ereport.h>
#include "frame/http_ext.h"
#include <libaccess/nserror.h>
#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclerror.h>
#include <plstr.h>

/*	SSL LAS driver
 *	Note that everything is case-insensitive.
 *	INPUT
 *	attr		must be the string "ssl".
 *	comparator	can only be "=" or "!=".
 *	pattern		"on", "off", "yes", "no", "true", "false", "1", "0"
 *	OUTPUT
 *	cachable	Will be set to ACL_NOT_CACHABLE.
 *	return code	set to LAS_EVAL_*
 */
int
LASSSLEval(NSErr_t *errp, char *attr, CmpOp_t comparator, char *pattern, 
		 ACLCachable_t *cachable, void **las_cookie, PList_t subject, 
		 PList_t resource, PList_t auth_info, PList_t global_auth)
{
    Session *sn = NULL;
    PRBool sslrequired, sslstate;
    int rv;
    
    /*	Sanity checking				*/
    if (strcmp(attr, "ssl") != 0) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6300, ACL_Program, 2, XP_GetAdminStr(DBT_sslLasUnexpectedAttribute), attr);
        return LAS_EVAL_INVALID;
    }
    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6310, ACL_Program, 2, XP_GetAdminStr(DBT_sslLasIllegalComparator), comparator_string(comparator));
        return LAS_EVAL_INVALID;
    }
    *cachable = ACL_NOT_CACHABLE;       // ????

    if (PL_strcasecmp(pattern, "on") == 0 ||
        PL_strcasecmp(pattern, "true") == 0 ||
        PL_strcasecmp(pattern, "yes") == 0 ||
        PL_strcasecmp(pattern, "1") == 0)
    {
        sslrequired = PR_TRUE;
    }
    else
    if (PL_strcasecmp(pattern, "off") == 0 ||
        PL_strcasecmp(pattern, "false") == 0 ||
        PL_strcasecmp(pattern, "no") == 0 ||
        PL_strcasecmp(pattern, "0") == 0)
    {
        sslrequired = PR_FALSE;
    }
    else {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6320, ACL_Program, 2, XP_GetAdminStr(DBT_sslLasIllegalValue), pattern);
        return LAS_EVAL_INVALID;
    }

    // Now look whether our session is over SSL
    if (PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL) < 0) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6330, ACL_Program, 2, XP_GetAdminStr(DBT_sslLasUnableToGetSessionAddr));
        return LAS_EVAL_FAIL;
    }

    sslstate = GetSecurity(sn);

    if ((sslstate == sslrequired) && (comparator == CMP_OP_EQ)) {
        rv = LAS_EVAL_TRUE;
    }
    else if ((sslstate != sslrequired) && (comparator == CMP_OP_NE)) {
        rv = LAS_EVAL_TRUE;
    } else {
        rv = LAS_EVAL_FALSE;
    }

    ereport(LOG_VERBOSE, "acl ssl: %s on ssl %s (%s)",
            (rv == LAS_EVAL_FALSE) ? "no match" : "match",
            (comparator == CMP_OP_EQ) ? "=" : "!=",
            pattern);

    return rv;
}

void
LASSSLFlush(void **cookie)
{
    return;
}
