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

#ifndef DACL_HEADER
#define DACL_HEADER
#include "libaccess/aclproto.h" 

NSPR_BEGIN_EXTERN_C
struct acl_callback_func_table {
    void (*fn_acl_start)(char *tag, void *data);
    void (*fn_ace_start)(void *data);
    void (*fn_ace_expr_flags)(int expr_flags, void *data);
    void (*fn_ace_expr_type)(ACLExprType_t expr_type, void *data);
    void (*fn_ace_expr_argv)(char *argp, void *data);
    void (*fn_ace_expr_arry)(char *attr_name, CmpOp_t comparator, char *attr_pattern, int false_idx, int true_idx, int start_flag, void *data);
    void (*fn_ace_expr_raw)(char *attr_name, CmpOp_t comparator, char *attr_pattern, int logical, void *data);
    void (*fn_ace_end)(void *data);
    void (*fn_acl_end)(void *data);
    void (*fn_acllist_end)(void *data);
};
typedef struct acl_callback_func_table acl_callback_func_table;

NSAPI_PUBLIC extern int 
ACL_AceWalker(ACLListHandle_t* acllist, 
              const acl_callback_func_table t, void *data);

NSAPI_PUBLIC extern int
get_is_owner_dav(NSErr_t *errp, PList_t subject, PList_t resource,
                 PList_t auth_info, PList_t global_auth, void *unused);

extern int 
LASIsLockOwnerEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
                   char *pattern, ACLCachable_t *cachable, void **las_cookie,
                   PList_t subject, PList_t resource, PList_t auth_info,
                   PList_t global_auth);

extern void LASIsLockOwnerFlush(void **cookie);

NSPR_END_EXTERN_C
#endif
