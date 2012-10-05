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

/********************************************************************

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 1996 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

*********************************************************************/

#include <string.h>
#include <netsite.h>
#include <base/session.h>
#include <libaccess/nserror.h>
#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include "symbols.h"
#include <libaccess/aclerror.h>
#include <libaccess/aclglobal.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/ldapacl.h>

// moved from obsolete file aclbuild.cpp as of iWS5.0
extern "C"
{
char * ACL_Program = "NSACL";		/* ACL facility name */
};

/* Ordered list of generic rights */
char * generic_rights[] = { ACL_GENERIC_RIGHT_READ,
                            ACL_GENERIC_RIGHT_WRITE,
                            ACL_GENERIC_RIGHT_EXECUTE,
                            ACL_GENERIC_RIGHT_DELETE,
                            ACL_GENERIC_RIGHT_INFO,
                            ACL_GENERIC_RIGHT_LIST,
                            NULL };

/* Pointer to all global ACL data.  This pointer is moved (atomically)
   when a cache flush call is made.
*/
ACLGlobal_p	ACLGlobal;
ACLGlobal_p	oldACLGlobal;


//
// ACL API vector - moved from oneeval.cpp
//
static ACLDispatchVector_t __nsacl_vector = {

    /* Error frame stack support */

    nserrDispose,
    nserrFAlloc,
    nserrFFree,
    nserrGenerate,

    /* Property list support */

    PListAssignValue,
    PListCreate,
    PListDefProp,
    PListDeleteProp,
    PListFindValue,
    PListInitProp,
    PListNew,
    PListDestroy,
    PListGetValue,
    PListNameProp,
    PListSetType,
    PListSetValue,
    PListEnumerate,
    PListDuplicate,
    PListGetPool,

    /* ACL attribute handling */

    ACL_LasRegister,

    /* method/dbtype registration routines */

    ACL_MethodRegister,
    ACL_MethodIsEqual,
    ACL_MethodNameIsEqual,
    ACL_MethodFind,
    ACL_MethodGetDefault,
    ACL_MethodSetDefault,
    ACL_AuthInfoGetMethod,

    ACL_DbTypeRegister,
    ACL_DbTypeIsEqual,
    ACL_DbTypeNameIsEqual,
    ACL_DbTypeFind,
    ACL_DbTypeGetDefault,
    ACL_AuthInfoGetDbType,      // OBSOLETE - cannot get dbtype out of ACL anymore
    ACL_DbTypeIsRegistered,
    ACL_DbTypeParseFn,

    ACL_AttrGetterRegister,

    ACL_ModuleRegister,
    ACL_GetAttribute,
    ACL_DatabaseRegister,
    ACL_DatabaseFind,           // finds low level database - so it's kinda obsolete
    ACL_DatabaseSetDefault,
    ACL_LDAPDatabaseHandle,
    ACL_AuthInfoGetDbname,
    ACL_CacheFlushRegister,
    ACL_CacheFlush,

    /*  ACL language and file interfaces */

    ACL_ParseFile,
    ACL_ParseString,
    ACL_WriteString,
    ACL_WriteFile,
    ACL_FileRenameAcl,
    ACL_FileDeleteAcl,
    ACL_FileGetAcl,
    ACL_FileSetAcl,

    /*  ACL Expression construction interfaces  */

    ACL_ExprNew,
    ACL_ExprDestroy,
    ACL_ExprSetPFlags,
    ACL_ExprClearPFlags,
    ACL_ExprTerm,
    ACL_ExprNot,
    ACL_ExprAnd,
    ACL_ExprOr,
    ACL_ExprAddAuthInfo,
    ACL_ExprAddArg,
    ACL_ExprSetDenyWith,
    ACL_ExprGetDenyWith,
    ACL_ExprAppend,

    /* ACL manipulation */

    ACL_AclNew,
    ACL_AclDestroy,

    /* ACL list manipulation */

    ACL_ListNew,
    ACL_ListConcat,
    ACL_ListAppend,
    ACL_ListDestroy,
    ACL_ListFind,
    ACL_ListAclDelete,
    ACL_ListGetNameList,
    ACL_NameListDestroy,

    /* ACL evaluation */

    ACL_EvalTestRights,
    ACL_EvalNew,
    ACL_EvalDestroy,
    ACL_EvalSetACL,
    ACL_EvalGetSubject,
    ACL_EvalSetSubject,
    ACL_EvalGetResource,
    ACL_EvalSetResource,

    /* Access to critical section for ACL cache */

    ACL_CritEnter,
    ACL_CritExit,

    /* Miscellaneous functions */

    ACL_AclGetTag,
    ACL_ListGetFirst,
    ACL_ListGetNext,

    /* Functions added after ES 3.0 release */
    ACL_DatabaseGetDefault,
    ACL_SetDefaultResult,
    ACL_GetDefaultResult,

    /* Functions added in ES 4.0 release */
    ACL_LDAPSessionAllocate,
    ACL_LDAPSessionFree,

    /* Functions added in iWS 6.0 release */
    ACL_VirtualDbLookup,
    ACL_VirtualDbGetAttr,
    ACL_VirtualDbSetAttr,
    ACL_VirtualDbReadLock,
    ACL_VirtualDbWriteLock,
    ACL_VirtualDbUnlock,
    ACL_VirtualDbGetParsedDb,
    ACL_VirtualDbGetDbType,
    ACL_VirtualDbGetCanonicalDbName
};

NSAPI_PUBLIC ACLDispatchVector_t *__nsacl_table = &__nsacl_vector;
