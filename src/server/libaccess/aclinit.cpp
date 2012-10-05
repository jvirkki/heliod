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

#include <stdio.h>
#include <string.h>
#include <netsite.h>
#include <base/nsassert.h>
#include <base/util.h>
#include <libaccess/acl.h>
#include "aclpriv.h"
#include "usrcache.h"
#include <safs/init.h>
#include <libaccess/aclproto.h>
#include <libaccess/aclerror.h>
#include <libaccess/aclglobal.h>
#include <libaccess/ldapacl.h>
#include <libaccess/nullacl.h>
#include <libaccess/genacl.h>
#include <libaccess/fileacl.h>
#include <libaccess/pamacl.h>
#include <libaccess/dacl.h>
#include <libaccess/cryptwrapper.h>
#include <libaccess/gssapi.h>

#define HTTP_GENERIC_SIZE 7
char   *http_generic[HTTP_GENERIC_SIZE]; 
/* 
 * This method appends new rights in abstract rights array http_generic
 * This is called from dav_init() method hence we don't need to make it thread safe. 
 * Input Parameters : 
 * char *newPrivilege - New abstact privilege to be introduced in http_generic
 * char *coreRight - CORE aggregate right it corresponds to 
 * Return Value :  integer
 *     -1 for error 
 *     0 for success
 */
NSAPI_PUBLIC int ACL_RegisterNewRights(char *newPrivilege, char *coreRight)
{
    int idx = -1;
    if ((newPrivilege == NULL) || (coreRight == NULL))
        return -1; 

    for (int i=0; i < (HTTP_GENERIC_SIZE - 1) && generic_rights[i] != NULL; i++) {
        if (!strcmp(generic_rights[i], coreRight)) {
            idx = i; 
            break;
        }
    }
    if ((idx < 0) || (idx >= HTTP_GENERIC_SIZE) || (http_generic[idx] == NULL))
      return -1; 

    // Test if it already exists in this line

    char *p = STRDUP(http_generic[idx]);
    char *next = NULL;
    char *token = util_strtok(p,", ", &next);
    while (token) {
        if (!strcasecmp(token, newPrivilege)) 
            return -1;
        token = util_strtok(NULL, ", " , &next);
    }
    FREE(p);

    int orig_len = strlen(http_generic[idx]);
    int len = strlen(newPrivilege);
    size_t new_len = orig_len + 2 + len + 1;
    http_generic[idx] = (char *)realloc(http_generic[idx], new_len);
    memcpy(http_generic[idx] + orig_len, ", ", 2);
    memcpy(http_generic[idx] + orig_len + 2, newPrivilege, len);
    http_generic[idx][new_len-1] = '\0';
    return 0;
}

NSAPI_PUBLIC int
ACL_Init(void)
{
    NSErr_t *errp = 0;
    int rv;

    ACL_InitAttr2Index();
    ACLGlobal = (ACLGlobal_p)PERM_CALLOC(sizeof(ACLGlobal_s));
    oldACLGlobal = (ACLGlobal_p)PERM_CALLOC(sizeof(ACLGlobal_s));
    NS_ASSERT(ACLGlobal && oldACLGlobal);
    ACL_DATABASE_POOL = NULL;
    ACL_METHOD_POOL = NULL;
    ACL_CritInit();
    ACL_CryptCritInit();
    ACL_LasHashInit();
    ACL_MethodHashInit();
    ACL_DbTypeHashInit();
    ACL_AttrGetterRegisterInit();
    ACL_DbNameHashInit();

    // "read" equivalent
    http_generic[0] = strdup("http_get, http_head, http_trace, http_options, http_copy, http_bcopy");

    // "write" equivalent
    http_generic[1] = strdup("http_put, http_mkdir, http_lock, http_unlock, http_proppatch, http_mkcol, http_acl, http_version-control, http_checkout, http_uncheckout, http_checkin, http_mkworkspace, http_update, http_label, http_merge, http_baseline-control, http_mkactivity, http_bproppatch");

    // "execute" equivalent
    http_generic[2] = strdup("http_post, http_connect, http_subscribe, http_unsubscribe, http_notify, http_poll");

    // "delete" equivalent
    http_generic[3] = strdup("http_delete, http_rmdir, http_move, http_bdelete, http_bmove");

    // "info" equivalent
    http_generic[4] = strdup("http_head, http_trace, http_options");

    // "list" equivalent
    http_generic[5] = strdup("http_index, http_propfind, http_report, http_search, http_bpropfind");

    http_generic[6] = NULL;

    //-----------------------------------------------------------------------
    // Register the built-in auth-db's and functions.
    // See also p.119 of Access Control Prog.Guide
    //
    // Note: Getter registration done in safs/init.cpp, which runs
    // after this code.
    //
    // - ACL_DBTYPE_* is a string name for the auth-db. See definitions in
    //   include/public/nsacl/acldef.h
    // - ACL_DbType* is a unique numeric identifier assigned to the auth-db
    //   by its corresponding ACL_DbTypeRegister below. These were allocated
    //   in acldbtype.cpp

    // Register the ldap database
    rv = ACL_DbTypeRegister(errp, ACL_DBTYPE_LDAP, parse_ldap_url, &ACL_DbTypeLdap);

    // Register the null database
    rv |= ACL_DbTypeRegister(errp, ACL_DBTYPE_NULL, parse_null_url, &ACL_DbTypeNull);

    // Register the file auth-db
    rv |= ACL_DbTypeRegister(errp, ACL_DBTYPE_FILE, fileacl_parse_url, &ACL_DbTypeFile);

    // Register the PAM auth-db
#ifdef FEAT_PAM
    rv |= ACL_DbTypeRegister(errp, ACL_DBTYPE_PAM,
                             pam_parse_url, &ACL_DbTypePAM);
#endif
    
    // Register the kerberos auth-db
#ifdef FEAT_GSS
    rv |= ACL_DbTypeRegister(errp, ACL_DBTYPE_KERBEROS,
                             kerberos_parse_url, &ACL_DbTypeKerberos);
#endif

    // Register the ACL functions
    rv |= ACL_LasRegister(NULL, "timeofday", LASTimeOfDayEval, LASTimeOfDayFlush);
    rv |= ACL_LasRegister(NULL, "dayofweek", LASDayOfWeekEval, LASDayOfWeekFlush);
    rv |= ACL_LasRegister(NULL, "ip", LASIpEval, LASIpFlush);
    rv |= ACL_LasRegister(NULL, "dns", LASDnsEval, LASDnsFlush);
    rv |= ACL_LasRegister(NULL, "dnsalias", LASDnsEval, LASDnsFlush);
    rv |= ACL_LasRegister(NULL, "group", LASGroupEval, (LASFlushFunc_t)NULL);
    rv |= ACL_LasRegister(NULL, "role", LASRoleEval, (LASFlushFunc_t)NULL);
    rv |= ACL_LasRegister(NULL, "user", LASUserEval, (LASFlushFunc_t)NULL);
    rv |= ACL_LasRegister(NULL, "cipher", LASCipherEval, (LASFlushFunc_t)NULL);
    rv |= ACL_LasRegister(NULL, "ssl", LASSSLEval, LASSSLFlush);
    rv |= ACL_LasRegister(NULL, "is-lock-owner", LASIsLockOwnerEval, LASIsLockOwnerFlush);
    rv |= ACL_LasRegister(NULL, "owner", LASOwnerEval, LASOwnerFlush);

    return rv;
}

NSAPI_PUBLIC int
ACL_InitPostMagnus(void)
{
    int rv;

    rv = ACL_AttrGetterRegister(NULL, ACL_ATTR_IS_OWNER,
                               get_is_owner_default,
                               ACL_METHOD_ANY, ACL_DBTYPE_ANY,
                               ACL_AT_END, NULL);
    return rv;
}

NSAPI_PUBLIC int
ACL_LateInitPostMagnus(void)
{
    return acl_usr_cache_init();
}
