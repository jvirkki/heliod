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

/**
 * $Revision: 1.1.2.1 $
 * $author: Meena Vyas $
 */
#include "netsite.h"
#include "frame/req.h"
#include "libaccess/aclproto.h"
#include "aclpriv.h" 
#include "libaccess/dacl.h" 

#include <base/ereport.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclerror.h>
#include <plstr.h>

/*
 *  LASOwnerEval
 *
 *  This eval function is invoked whenever an ACL is set of the form
 *  allow/deny (*) owner =/!= true/false/0/1/on/off/yes/no
 *  It will call attribute getter for ACL_ATTR_IS_OWNER which is 
 *  initialized to get_is_owner_default currently.
 *  For dav subsystem attribute getter for ACL_ATTR_IS_OWNER is get_is_owner_dav.
 *
 *  INPUT
 *    attr_name     The string "owner" - in lower case.
 *    comparator    CMP_OP_EQ or CMP_OP_NE only
 *    attr_pattern  "on", "off", "yes", "no", "true", "false", "1", "0"
 *    cachable      Always set to ACL_NOT_CACHABLE.
 *    subject       Subject property list
 *    resource      Resource property list
 *    auth_info     Authentication info, if any
 *  RETURNS
 *    retcode       The usual LAS return codes.
 */
int LASOwnerEval(NSErr_t *errp, char *attr_name, CmpOp_t comparator,
                 char *attr_pattern, ACLCachable_t *cachable,
                 void **LAS_cookie, PList_t subject, PList_t resource,
                 PList_t auth_info, PList_t global_auth)
{
    int retcode=0;
    // should not try to check the value as the value set is void *
    char *owner=NULL; 
    PRBool pattern=PR_TRUE;
    int rv=0;

    *cachable = ACL_NOT_CACHABLE;
    *LAS_cookie = (void *)0;

    if (strcmp(attr_name, ACL_ATTR_OWNER)) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6700, ACL_Program, 2,
                      XP_GetAdminStr(DBT_lasOwnerEvalReceivedRequestForAtt_),
                      attr_name);
        return LAS_EVAL_INVALID;
    }

    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6710, ACL_Program, 2,
                      XP_GetAdminStr(DBT_lasOwnerEvalIllegalComparatorDN_),
                      comparator_string(comparator));
        return LAS_EVAL_INVALID;
    }

   if (PL_strcasecmp(attr_pattern, "on") == 0 ||
       PL_strcasecmp(attr_pattern, "true") == 0 ||
       PL_strcasecmp(attr_pattern, "yes") == 0 ||
       PL_strcasecmp(attr_pattern, "1") == 0) {
        pattern = PR_TRUE;
    }
    else if (PL_strcasecmp(attr_pattern, "off") == 0 ||
        PL_strcasecmp(attr_pattern, "false") == 0 ||
        PL_strcasecmp(attr_pattern, "no") == 0 ||
        PL_strcasecmp(attr_pattern, "0") == 0) {
        pattern = PR_FALSE;
    } else {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6720, ACL_Program, 2,
                      XP_GetAdminStr(DBT_lasOwnerEvalIllegalAttrPattern_),
                      attr_pattern);
        return LAS_EVAL_INVALID;
    }

    // get the owner
    rv = ACL_GetAttribute(errp, ACL_ATTR_IS_OWNER, (void **)&owner, 
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE && rv != LAS_EVAL_FALSE) {
        return rv;
    }

    if (pattern == PR_TRUE)
        if (comparator == CMP_OP_EQ)
            retcode = rv;
        else
            retcode = ((rv == LAS_EVAL_TRUE) ? LAS_EVAL_FALSE : LAS_EVAL_TRUE);
    else
        if (comparator == CMP_OP_EQ)
            retcode = ((rv == LAS_EVAL_TRUE) ? LAS_EVAL_FALSE : LAS_EVAL_TRUE);
        else
            retcode = rv;

    ereport(LOG_VERBOSE,
            "acl owner : user %s owner %s (%s)",
            (retcode == LAS_EVAL_FALSE) ? "does not match"
            : (retcode == LAS_EVAL_TRUE) ? "matched" : "error in",
            (comparator == CMP_OP_EQ) ? "=" : "!=",
            attr_pattern);

    return retcode;
}

/* LASOwnerFlush
 * Deallocates any memory previously allocated by the LAS
 */
void
LASOwnerFlush(void **las_cookie) 
{
    /* do nothing */
    return;
}
