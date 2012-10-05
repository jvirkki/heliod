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
 * Contains all dacl related APIs
 *
 * $Revision: 1.1.2.2 $
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

NSAPI_PUBLIC int 
ACL_AceWalker(ACLListHandle_t* acllist, const acl_callback_func_table t, 
              void *data)
{
    for (ACLWrapper_t *wrapper = acllist->acl_list_head; wrapper != NULL; 
         wrapper = wrapper->wrap_next) {
            if (wrapper->acl)
                t.fn_acl_start(wrapper->acl->tag, data);
        for (ACLExprHandle_t *ace = wrapper->acl->expr_list_head; ace != NULL; 
             ace = ace->expr_next) {
            t.fn_ace_start(data);

            t.fn_ace_expr_flags(ace->expr_flags, data);

            // ace->expr_type could be ACL_EXPR_TYPE_ALLOW/DENY/AUTH/RESPONSE
            t.fn_ace_expr_type(ace->expr_type, data);

            for (char **argp = ace->expr_argv; *argp; argp++) {
                t.fn_ace_expr_argv(*argp, data);
            } // end for each expr_argv (right)

            for (int i = 0; i < ace->expr_term_index; i++) {
                ACLExprEntry_t *expr = &ace->expr_arry[i];
                t.fn_ace_expr_arry(expr->attr_name, expr->comparator, 
                                   expr->attr_pattern, expr->false_idx, 
                                   expr->true_idx, expr->start_flag, data);
            } // end for each expression array

            for (int j = 0; j < ace->expr_raw_index; j++) {
                 ACLExprRaw_t *expr = &ace->expr_raw[j];
                 t.fn_ace_expr_raw(expr->attr_name, expr->comparator, 
                                   expr->attr_pattern, (int)expr->logical, 
                                   data);
            } // end for each expression raw

            t.fn_ace_end(data);
        } // for (all ACEs in ACL)
        t.fn_acl_end(data);
    } // for (all ACLs in acllist)
    t.fn_acllist_end(data);
    return 0;
}

/*
 *  LASIsLockOwnerEval
 *  INPUT
 *    attr_name     The string "is-lock-owner" - in lower case.
 *    comparator    CMP_OP_EQ or CMP_OP_NE only
 *    attr_pattern  "on", "off", "yes", "no", "true", "false", "1", "0"
 *    cachable      Always set to ACL_NOT_CACHABLE.
 *    subject       Subject property list
 *    resource      Resource property list
 *    auth_info     Authentication info, if any
 *  RETURNS
 *    retcode       The usual LAS return codes.
 */
int LASIsLockOwnerEval(NSErr_t *errp, char *attr_name, CmpOp_t comparator,
                       char *attr_pattern, ACLCachable_t *cachable,
                       void **LAS_cookie, PList_t subject, PList_t resource,
                       PList_t auth_info, PList_t global_auth)
{
    int retcode=0;
    int matched=0;
    char *lock_owner=NULL;
    char *user=NULL;
    PRBool pattern=PR_TRUE;
    int rv=0;

    *cachable = ACL_NOT_CACHABLE;
    *LAS_cookie = (void *)0;

    if (strcmp(attr_name, ACL_ATTR_IS_LOCK_OWNER)) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6700, ACL_Program, 2,
                      XP_GetAdminStr(DBT_lasIsLockOwnerEvalReceivedRequestForAtt_),
                      attr_name);
        return LAS_EVAL_INVALID;
    }

    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6710, ACL_Program, 2,
                      XP_GetAdminStr(DBT_lasIsLockOwnerEvalIllegalComparatorDN_),
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
                      XP_GetAdminStr(DBT_lasIsLockOwnerEvalIllegalAttrPattern_),
                      attr_pattern);
        return LAS_EVAL_INVALID;
    }

    // get the lock owner 
    // assuming lock owner has been set in plist before ACL_Evaluate is called
    rv = ACL_GetAttribute(errp, ACL_ATTR_LOCK_OWNER, (void **)&lock_owner, 
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) {
        return rv;
    }

    if (lock_owner) {
        // get the authenticated user name
        rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&user, 
                              subject, resource, auth_info, global_auth);
        if (rv != LAS_EVAL_TRUE) {
            return rv;
        }
        if (user) {
            // if user is the lock owner
            matched = !strcmp(user, lock_owner);
        } else // user has not been authenticated yet
            matched=0;
    } else // lock owner is null means does not match
        matched=0;

    if (pattern == PR_FALSE)
        matched = !matched;
    if (comparator == CMP_OP_EQ) {
        retcode = (matched ? LAS_EVAL_TRUE : LAS_EVAL_FALSE);
    } else {
        retcode = (matched ? LAS_EVAL_FALSE : LAS_EVAL_TRUE);
    }

    ereport(LOG_VERBOSE,
            "acl is-lock-owner : user [%s] %s is lock owner [%s] %s (%s)",
            user?user:"",
            (retcode == LAS_EVAL_FALSE) ? "does not match"
            : (retcode == LAS_EVAL_TRUE) ? "matched" : "error in",
            lock_owner?lock_owner:"",
            (comparator == CMP_OP_EQ) ? "=" : "!=",
            attr_pattern);

    return retcode;
}

/* LASIsLockOwnerFlush
 * Deallocates any memory previously allocated by the LAS
 */
void
LASIsLockOwnerFlush(void **las_cookie) 
{
    /* do nothing */
    return;
}
