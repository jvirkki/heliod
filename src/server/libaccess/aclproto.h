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

#ifndef ACL_PROTO_HEADER
#define ACL_PROTO_HEADER

#ifndef NOINTNSACL
#define INTNSACL
#endif /* !NOINTNSACL */

#ifndef PUBLIC_NSACL_ACLDEF_H
#include "public/nsacl/acldef.h"
#endif /* !PUBLIC_NSACL_ACLDEF_H */

#ifdef INTNSACL

NSPR_BEGIN_EXTERN_C

/*********************************************************************
 *  ACL language and file interfaces
 *********************************************************************/

NSAPI_PUBLIC ACLListHandle_t * ACL_ParseFile(NSErr_t *errp, char *filename);
NSAPI_PUBLIC ACLListHandle_t * ACL_ParseString(NSErr_t *errp, char *buffer);
NSAPI_PUBLIC int ACL_Decompose(NSErr_t *errp, char **acl, ACLListHandle_t *acl_list);
NSAPI_PUBLIC int ACL_WriteString(NSErr_t *errp, char **acl, ACLListHandle_t *acllist);
NSAPI_PUBLIC int ACL_WriteFile(NSErr_t *errp, char *filename, ACLListHandle_t *acllist);
NSAPI_PUBLIC int ACL_FileRenameAcl(NSErr_t *errp, char *filename, char *acl_name, char *new_acl_name, int flags);
NSAPI_PUBLIC int ACL_FileDeleteAcl(NSErr_t *errp, char *filename, char *acl_name, int flags);
NSAPI_PUBLIC int ACL_FileGetAcl(NSErr_t *errp, char *filename, char *acl_name, char **acl_text, int flags);
NSAPI_PUBLIC int ACL_FileSetAcl(NSErr_t *errp, char *filename, char *acl_text, int flags);
NSAPI_PUBLIC int ACL_FileMergeAcl(NSErr_t *errp, char *filename, char **acl_name_list, char *new_acl_name, int  flags);
NSAPI_PUBLIC int ACL_FileMergeFile(NSErr_t *errp, char *filename, char **file_list, int  flags);


/*********************************************************************
 *  ACL Expression construction interfaces
 *********************************************************************/
NSAPI_PUBLIC ACLExprHandle_t *ACL_ExprNew(const ACLExprType_t expr_type);
NSAPI_PUBLIC void ACL_ExprDestroy(ACLExprHandle_t *expr);
NSAPI_PUBLIC int ACL_ExprSetPFlags(NSErr_t *errp, ACLExprHandle_t *expr, PFlags_t flags);
NSAPI_PUBLIC int ACL_ExprClearPFlags(NSErr_t *errp, ACLExprHandle_t *expr);
NSAPI_PUBLIC int ACL_ExprTerm(NSErr_t *errp, ACLExprHandle_t *acl_expr, char *attr_name, CmpOp_t cmp, char *attr_pattern);
NSAPI_PUBLIC int ACL_ExprNot(NSErr_t *errp, ACLExprHandle_t *acl_expr);
NSAPI_PUBLIC int ACL_ExprAnd(NSErr_t *errp, ACLExprHandle_t *acl_expr);
NSAPI_PUBLIC int ACL_ExprOr(NSErr_t *errp, ACLExprHandle_t *acl_expr);
NSAPI_PUBLIC int ACL_ExprAddAuthInfo(ACLExprHandle_t *expr, PList_t auth_info);
NSAPI_PUBLIC int ACL_ExprAddArg(NSErr_t *errp, ACLExprHandle_t *expr, char *arg);
NSAPI_PUBLIC int ACL_ExprSetDenyWith(NSErr_t *errp, ACLExprHandle_t *expr, char *deny_type, char *deny_response);
NSAPI_PUBLIC int ACL_ExprGetDenyWith(NSErr_t *errp, ACLExprHandle_t *expr, char **deny_type, char **deny_response);
extern int ACL_ExprDisplay( ACLExprHandle_t *acl_expr );

/*********************************************************************
 * ACL manipulation
 *********************************************************************/

NSAPI_PUBLIC ACLHandle_t * ACL_AclNew(NSErr_t *errp, char *tag);
NSAPI_PUBLIC void ACL_AclDestroy(NSErr_t *errp, ACLHandle_t *acl);
NSAPI_PUBLIC int ACL_ExprAppend(NSErr_t *errp, ACLHandle_t *acl, ACLExprHandle_t *expr);
NSAPI_PUBLIC const char *ACL_AclGetTag(ACLHandle_t *acl);

/*********************************************************************
 * ACL list manipulation
 *********************************************************************/

NSAPI_PUBLIC ACLListHandle_t * ACL_ListNew(NSErr_t *errp);
NSAPI_PUBLIC int ACL_ListConcat(NSErr_t *errp, ACLListHandle_t *acl_list1, ACLListHandle_t *acl_list2, int flags);
NSAPI_PUBLIC int ACL_ListAppend(NSErr_t *errp, ACLListHandle_t *acllist, ACLHandle_t *acl, int flags);
NSAPI_PUBLIC void ACL_ListDestroy(NSErr_t *errp, ACLListHandle_t *acllist);
NSAPI_PUBLIC ACLHandle_t * ACL_ListFind(NSErr_t *errp, ACLListHandle_t *acllist, char *aclname, int flags);
NSAPI_PUBLIC int ACL_ListAclDelete(NSErr_t *errp, ACLListHandle_t *acl_list, char *acl_name, int flags);
NSAPI_PUBLIC int ACL_ListGetNameList(NSErr_t *errp, ACLListHandle_t *acl_list, char ***name_list);
NSAPI_PUBLIC int ACL_FileGetNameList(NSErr_t *errp, char * filename, char ***name_list);
NSAPI_PUBLIC int ACL_NameListDestroy(NSErr_t *errp, char **name_list);
NSAPI_PUBLIC ACLHandle_t *ACL_ListGetFirst(ACLListHandle_t *acl_list,
                                           ACLListEnum_t *acl_enum);
NSAPI_PUBLIC ACLHandle_t *ACL_ListGetNext(ACLListHandle_t *acl_list,
                                           ACLListEnum_t *acl_enum);

/* Need to be ACL_LIB_INTERNAL */
NSAPI_PUBLIC int ACL_ListPostParseForAuth(NSErr_t *errp, ACLListHandle_t *acl_list);

/*********************************************************************
 * ACL evaluation 
 *********************************************************************/

NSAPI_PUBLIC int ACL_EvalTestRights(NSErr_t *errp, ACLEvalHandle_t *acleval, char **rights, char **map_generic, char **deny_type, char **deny_response, char **acl_tag, int *expr_num);
NSAPI_PUBLIC int ACL_CachableAclList(ACLListHandle_t *acllist);
NSAPI_PUBLIC int ACL_AlwaysAllows(ACLListHandle_t *acllist, char **rights, char **map_generic);
NSAPI_PUBLIC int ACL_CanDeny(ACLListHandle_t *acllist, char **rights, char **map_generic);
NSAPI_PUBLIC ACLEvalHandle_t * ACL_EvalNew(NSErr_t *errp, pool_handle_t *pool);
NSAPI_PUBLIC void ACL_EvalDestroy(NSErr_t *errp, pool_handle_t *pool, ACLEvalHandle_t *acleval);
NSAPI_PUBLIC void ACL_EvalDestroyNoDecrement(NSErr_t *errp, pool_handle_t *pool, ACLEvalHandle_t *acleval);
NSAPI_PUBLIC int ACL_ListDecrement(NSErr_t *errp, ACLListHandle_t *acllist);
NSAPI_PUBLIC void ACL_ListIncrement(NSErr_t *errp, ACLListHandle_t *acllist);
NSAPI_PUBLIC int ACL_EvalSetACL(NSErr_t *errp, ACLEvalHandle_t *acleval, ACLListHandle_t *acllist);
NSAPI_PUBLIC PList_t ACL_EvalGetSubject(NSErr_t *errp, ACLEvalHandle_t *acleval);
NSAPI_PUBLIC int ACL_EvalSetSubject(NSErr_t *errp, ACLEvalHandle_t *acleval, PList_t subject);
NSAPI_PUBLIC PList_t ACL_EvalGetResource(NSErr_t *errp, ACLEvalHandle_t *acleval);
NSAPI_PUBLIC int ACL_EvalSetResource(NSErr_t *errp, ACLEvalHandle_t *acleval, PList_t resource);

NSAPI_PUBLIC int ACL_SetDefaultResult (NSErr_t *errp, ACLEvalHandle_t *acleval, int result);
NSAPI_PUBLIC int ACL_GetDefaultResult (ACLEvalHandle_t *acleval);
extern void ACL_CacheEvalInfo(NSErr_t *errp, PList_t resource, PList_t auth_info);
extern void ACL_FlushEvalInfo(NSErr_t *errp, PList_t resource);

/*********************************************************************
 * ACL misc routines 
 *********************************************************************/

NSAPI_PUBLIC int ACL_Init(void);
NSAPI_PUBLIC int ACL_InitPostMagnus(void);
NSAPI_PUBLIC int ACL_LateInitPostMagnus(void);
NSAPI_PUBLIC void ACL_SetUserCacheMaxAge(int timeout);
NSAPI_PUBLIC void ACL_SetUserCacheMaxUsers(int n);
NSAPI_PUBLIC void ACL_SetUserCacheMaxGroupsPerUser(int n);

NSAPI_PUBLIC PRBool ACL_GetPathAcls(char *path, ACLListHandle_t **acllist_p, char *prefix, ACLListHandle_t *masterlist);

NSAPI_PUBLIC int ACL_MethodNamesGet(NSErr_t *errp, char ***names, int *count);
NSAPI_PUBLIC int ACL_MethodNamesFree(NSErr_t *errp, char **names, int count);

NSAPI_PUBLIC int ACL_DatabaseNamesGet(NSErr_t *errp, char ***names, int *count);
NSAPI_PUBLIC int ACL_DatabaseNamesFree(NSErr_t *errp, char **names, int count);

NSAPI_PUBLIC int ACL_InitAttr2Index(void);
NSAPI_PUBLIC int ACL_Attr2Index(const char *attrname);

NSAPI_PUBLIC int ACL_RegisterNewRights(char *newPrivilege, char *coreRight);

#ifdef MCC_DEBUG
NSAPI_PUBLIC int ACL_ListPrint (ACLListHandle_t *acl_list );
#endif /* MCC_DEBUG */

/*********************************************************************
 * ACL cache and flush utility 
 *********************************************************************/

NSAPI_PUBLIC int ACL_CacheFlush(void);
NSAPI_PUBLIC void ACL_Restart(void *clntData);
NSAPI_PUBLIC void ACL_CritEnter(void);
NSAPI_PUBLIC void ACL_CritExit(void);
extern int ACL_CritHeld(void);
extern void ACL_CritInit(void);

/*********************************************************************
 * ACL CGI routines
 *********************************************************************/

NSAPI_PUBLIC void ACL_OutputSelector(char *name, char **item);

/*********************************************************************
 * ACL attribute getter routines
 *********************************************************************/

NSAPI_PUBLIC extern int ACL_GetAttribute(NSErr_t *errp, const char *attr, void **val,
                                    PList_t subject, PList_t resource,
                                    PList_t auth_info, PList_t global_auth);
NSAPI_PUBLIC extern void ACL_AttrGetterRegisterInit();
NSAPI_PUBLIC extern int ACL_AttrGetterRegister(NSErr_t *errp, const char *attr,
                                    ACLAttrGetterFn_t fn, ACLMethod_t m,
                                    ACLDbType_t d, int position, void *arg);
NSAPI_PUBLIC extern int ACL_AttrGetterFind(NSErr_t *errp, const char *attr,
                                    ACLAttrGetterList_t *getters);
NSAPI_PUBLIC extern ACLAttrGetter_t * ACL_AttrGetterFirst(ACLAttrGetterList_t *getters);
NSAPI_PUBLIC ACLAttrGetter_t * ACL_AttrGetterNext(ACLAttrGetterList_t *getters, ACLAttrGetter_t *last);

/*********************************************************************
 * LAS registration interface
 *********************************************************************/

NSAPI_PUBLIC extern int ACL_LasRegister(NSErr_t *errp, char *attr_name, LASEvalFunc_t eval_func, LASFlushFunc_t flush_func);
NSAPI_PUBLIC extern int ACL_LasFindEval(NSErr_t *errp, char *attr_name, LASEvalFunc_t *eval_funcp);
NSAPI_PUBLIC extern int ACL_LasFindFlush(NSErr_t *errp, char *attr_name, LASFlushFunc_t *flush_funcp);
extern void ACL_LasHashInit(void);
extern void ACL_LasHashDestroy(void);

/*********************************************************************
 * Method registration interface
 *********************************************************************/

extern void ACL_MethodHashInit(void);
NSAPI_PUBLIC extern int ACL_MethodRegister(NSErr_t *errp, const char *name, ACLMethod_t *t);
NSAPI_PUBLIC extern int ACL_MethodIsEqual(NSErr_t *errp, const ACLMethod_t t1, const ACLMethod_t t2);
NSAPI_PUBLIC extern int ACL_MethodNameIsEqual(NSErr_t *errp, const ACLMethod_t t, const char *name);
NSAPI_PUBLIC extern int ACL_MethodFind(NSErr_t *errp, const char *name, ACLMethod_t *t);
NSAPI_PUBLIC extern ACLMethod_t ACL_MethodGetDefault(NSErr_t *errp);
NSAPI_PUBLIC extern int ACL_MethodSetDefault(NSErr_t *errp, const ACLMethod_t t);
NSAPI_PUBLIC extern int ACL_AuthInfoGetMethod(NSErr_t *errp, PList_t auth_info, ACLMethod_t *t);
NSAPI_PUBLIC extern int ACL_AuthInfoSetMethod(NSErr_t *errp, PList_t auth_info, ACLMethod_t t);

/*********************************************************************
 * Dbtype registration interface
 *********************************************************************/

extern ACLDbType_t ACL_DbTypeLdap;
extern ACLDbType_t ACL_DbTypeNull;
extern ACLDbType_t ACL_DbTypeFile;
extern ACLDbType_t ACL_DbTypePAM;
extern ACLDbType_t ACL_DbTypeKerberos;

extern void ACL_DbTypeHashInit(void);
NSAPI_PUBLIC extern int ACL_DbTypeRegister(NSErr_t *errp, const char *name, DbParseFn_t func, ACLDbType_t *t);
NSAPI_PUBLIC extern int ACL_DbTypeIsEqual(NSErr_t *errp, const ACLDbType_t t1, const ACLDbType_t t2);
NSAPI_PUBLIC extern int ACL_DbTypeNameIsEqual(NSErr_t *errp, const ACLDbType_t t, const char *name);
NSAPI_PUBLIC extern int ACL_DbTypeFind(NSErr_t *errp, const char *name, ACLDbType_t *t);
NSAPI_PUBLIC extern ACLDbType_t ACL_DbTypeGetDefault(NSErr_t *errp);
NSAPI_PUBLIC extern int ACL_DbTypeSetDefault(NSErr_t *errp, ACLDbType_t t);
NSAPI_PUBLIC extern int ACL_AuthInfoGetDbType(NSErr_t *errp, PList_t auth_info, ACLDbType_t *t);
NSAPI_PUBLIC extern int ACL_DbTypeIsRegistered(NSErr_t *errp, const ACLDbType_t dbtype);
NSAPI_PUBLIC extern DbParseFn_t ACL_DbTypeParseFn(NSErr_t *errp, const ACLDbType_t dbtype);

/*********************************************************************
 * Module registration interface
 *********************************************************************/

NSAPI_PUBLIC int ACL_ModuleRegister (NSErr_t *errp, const char *moduleName, AclModuleInitFunc func);

/*********************************************************************
 * Database registration interface
 *********************************************************************/

extern void ACL_DbNameHashInit(void);
NSAPI_PUBLIC extern const char * ACL_DatabaseGetDefault(NSErr_t *errp);
NSAPI_PUBLIC extern int ACL_DatabaseSetDefault(NSErr_t *errp, const char *dbname);
NSAPI_PUBLIC int ACL_DatabaseRegister(NSErr_t *errp, ACLDbType_t dbtype, const char *dbname, const char *url, PList_t plist);
NSAPI_PUBLIC int ACL_RegisterDbFromACL(NSErr_t *errp, const char *url, ACLDbType_t *dbtype);
NSAPI_PUBLIC int ACL_DatabaseFind(NSErr_t *errp, const char *dbname, ACLDbType_t *dbtype, void **db);
NSAPI_PUBLIC int ACL_AuthInfoGetDbname (PList_t auth_info, char **dbname);
NSAPI_PUBLIC int ACL_AuthInfoSetDbname (NSErr_t *errp, PList_t auth_info, const char *dbname);
NSAPI_PUBLIC int ACL_VirtualDbRegister(NSErr_t *errp, const VirtualServer *vs, const char *virtdbname, const char *url, PList_t plist, const char **dbname);
NSAPI_PUBLIC int ACL_VirtualDbRef(NSErr_t *errp, const char *dbname, ACLVirtualDb_t **virtdb);
NSAPI_PUBLIC void ACL_VirtualDbUnref(ACLVirtualDb_t *virtdb);
NSAPI_PUBLIC int ACL_VirtualDbFind(NSErr_t *errp, PList_t resource, const char *virtdbname, ACLVirtualDb_t **virtdb, ACLDbType_t *dbtype, void **db, const VirtualServer **vs);
NSAPI_PUBLIC int ACL_VirtualDbLookup(NSErr_t *errp, const VirtualServer *vs, const char *virtdbname, ACLVirtualDb_t **virtdb);
NSAPI_PUBLIC void ACL_VirtualDbReadLock(ACLVirtualDb_t *virtdb);
NSAPI_PUBLIC void ACL_VirtualDbWriteLock(ACLVirtualDb_t *virtdb);
NSAPI_PUBLIC void ACL_VirtualDbUnlock(ACLVirtualDb_t *virtdb);
NSAPI_PUBLIC int ACL_VirtualDbGetAttr(NSErr_t *errp, ACLVirtualDb_t *virtdb, const char *name, const char **value);
NSAPI_PUBLIC int ACL_VirtualDbSetAttr(NSErr_t *errp, ACLVirtualDb_t *virtdb, const char *name, const char *value);
NSAPI_PUBLIC void ACL_VirtualDbGetParsedDb(NSErr_t *errp, ACLVirtualDb_t *virtdb, void **db);
NSAPI_PUBLIC void ACL_VirtualDbGetDbType(NSErr_t *errp, ACLVirtualDb_t *virtdb, ACLDbType_t *dbtype);
NSAPI_PUBLIC void ACL_VirtualDbGetCanonicalDbName(NSErr_t *errp, ACLVirtualDb_t *virtdb, const char **dbname);

/*********************************************************************
 * Miscellaneous other stuff
 *********************************************************************/

NSAPI_PUBLIC int ACL_CacheFlushRegister(AclCacheFlushFunc_t func);

/* Only used for asserts.  Probably shouldn't be publicly advertized */
extern int ACL_AssertAcl( ACLHandle_t *acl );
extern int ACL_AssertAcllist( ACLListHandle_t *acllist );

struct program_groups {
	char *type;
	char **groups;
	char **programs;
};

/*********************************************************************
 * LAS evaluation functions
 *********************************************************************/

extern int LASTimeOfDayEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASDayOfWeekEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASIpEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASDnsEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASSSLEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASRoleEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASGroupEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASUserEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASCipherEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASProgramEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);
extern int LASOwnerEval(NSErr_t *errp, char *attribute, CmpOp_t comparator,
			char *pattern, ACLCachable_t *cachable, void **las_cookie,
			PList_t subject, PList_t resource, PList_t auth_info,
			PList_t global_auth);

extern void LASTimeOfDayFlush(void **cookie);
extern void LASDayOfWeekFlush(void **cookie);
extern void LASIpFlush(void **cookie);
extern void LASDnsFlush(void **cookie);
extern void LASSSLFlush(void **cookie);
extern void LASOwnerFlush(void **cookie);

extern int LASDnsGetter(NSErr_t *errp, PList_t subject, PList_t resource, PList_t
           auth_info, PList_t global_auth, void *arg);
extern int LASIpGetter(NSErr_t *errp, PList_t subject, PList_t resource, PList_t
           auth_info, PList_t global_auth, void *arg);
extern int LASIpv6Getter(NSErr_t *errp, PList_t subject, PList_t resource, PList_t
           auth_info, PList_t global_auth, void *arg);

NSPR_END_EXTERN_C

#endif /* INTNSACL */

#endif

