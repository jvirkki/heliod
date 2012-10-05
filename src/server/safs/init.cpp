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

#include <netsite.h>
#include <base/ereport.h>

#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include <libaccess/ldapacl.h>
#include <libaccess/nullacl.h>
#ifdef FEAT_PAM
#include <libaccess/pamacl.h>
#endif
#include <libaccess/fileacl.h>
#include <libaccess/gssapi.h>

#include <frame/func.h>
#include <frame/conf.h>

#include <safs/auth.h>
#include <safs/digest.h>
#include <safs/clauth.h>
#include <safs/init.h>
#include <safs/dbtsafs.h>

ACLMethod_t ACL_MethodBasic = ACL_METHOD_INVALID;
ACLMethod_t ACL_MethodSSL = ACL_METHOD_INVALID;
ACLMethod_t ACL_MethodDigest = ACL_METHOD_INVALID;
ACLMethod_t ACL_MethodGSSAPI = ACL_METHOD_INVALID;

#define BIG_LINE 1024

#define ACL_REG(x, y, z) \
    { \
	int rv = (x); \
	if (rv < 0) { \
	    ereport(LOG_MISCONFIG, y, z); \
	    return REQ_ABORTED; \
	} \
    }

int register_attribute_getter (pblock *pb, Session *sn, Request *rq)
{
    char *method_str = pblock_findval(ACL_ATTR_METHOD, pb);
    char *attr = pblock_findval(ACL_ATTR_ATTRIBUTE, pb);
    char *funcStr = pblock_findval(ACL_ATTR_GETTERFN, pb);
    char *dbtype_str = pblock_findval(ACL_ATTR_DBTYPE, pb);
    char *position_str = pblock_findval(ACL_ATTR_POSITION, pb);
    ACLDbType_t dbtype = ACL_DBTYPE_ANY;
    ACLMethod_t method = ACL_METHOD_ANY;
    ACLAttrGetterFn_t func;
    char err[BIG_LINE];
    NSErr_t *errp = 0;
    int position = ACL_AT_END;

    if (method_str) {
	ACL_REG(ACL_MethodFind(errp, method_str, &method),
		"Method \"%s\" is not registered", method_str);
    }

    if (dbtype_str) {
	ACL_REG(ACL_DbTypeFind(errp, dbtype_str, &dbtype),
		"Database type \"%s\" is not registered", dbtype_str);
    }

    if (!attr || !*attr) {
	pblock_nvinsert("error", "Attribute name is missing", pb);
	return REQ_ABORTED;
    }

    if (!funcStr || !*funcStr) {
	pblock_nvinsert("error", "Attribute getter function name is missing", pb);
	return REQ_ABORTED;
    }

    if (!position_str) {
	if (!strcmp(position_str, "ACL_AT_FRONT")) position = ACL_AT_FRONT;
	else if (!strcmp(position_str, "ACL_AT_END")) position = ACL_AT_END;
	else if (!strcmp(position_str, "ACL_REPLACE_ALL")) position = ACL_REPLACE_ALL;
	else if (!strcmp(position_str, "ACL_REPLACE_MATCHING")) position = ACL_REPLACE_MATCHING;
	else {
	    sprintf(err, "Position attribute \"%s\" is not valid", position_str);
	    pblock_nvinsert("error", err, pb);
	    return REQ_ABORTED;
	}
    }

    func = (ACLAttrGetterFn_t)func_find((char *)funcStr);

    if (!func) {
	sprintf(err, "Could not map \"%s\" to a function", funcStr);
	pblock_nvinsert("error", err, pb);
	return REQ_ABORTED;
    }

    ACL_REG(ACL_AttrGetterRegister(errp, attr, func, method, dbtype, position,
                                   NULL),
	    "Failed to register attribute getter for %s",
	    attr);

    return REQ_PROCEED;
}

int register_method (pblock *pb, Session *sn, Request *rq)
{
    char *method = pblock_findval(ACL_ATTR_METHOD, pb);
    ACLMethod_t t;
    NSErr_t *errp = 0;

    ACL_REG(ACL_MethodRegister (errp, method, &t),
	    "Failed to register method \"%s\"", method);

    return REQ_PROCEED;
}

int set_default_method (pblock *pb, Session *sn, Request *rq)
{
    char *method = pblock_findval(ACL_ATTR_METHOD, pb);
    NSErr_t *errp = 0;
    ACLMethod_t	t;

    ACL_REG(ACL_MethodFind(errp, method, &t),
	    "Method \"%s\" is not registered", method);

    ACL_MethodSetDefault(errp, t);

    return REQ_PROCEED;
}

int set_default_database (pblock *pb, Session *sn, Request *rq)
{
    char *dbname = pblock_findval(ACL_ATTR_DBNAME, pb);
    NSErr_t *errp = 0;

    ACL_REG(ACL_DatabaseSetDefault(errp, dbname),
	    "Failed to set default method \"%s\"", dbname);

    return REQ_PROCEED;
}

int register_module (pblock *pb, Session *sn, Request *rq)
{
    char *module = pblock_findval(ACL_ATTR_MODULE, pb);
    char *funcStr = pblock_findval(ACL_ATTR_MODULEFUNC, pb);
    AclModuleInitFunc func;
    NSErr_t *errp = 0;

    if (!funcStr || !*funcStr) {
	ereport(LOG_SECURITY, XP_GetAdminStr(DBT_initereport1));
	return REQ_ABORTED;
    }

    func = (AclModuleInitFunc)func_find(funcStr);

    if (!func) {
	ereport(LOG_SECURITY, XP_GetAdminStr(DBT_initereport2));
	return REQ_ABORTED;
    }

    ACL_REG(ACL_ModuleRegister (errp, module, func),
	          XP_GetAdminStr(DBT_initereport3), module);

    return REQ_PROCEED;
}

int register_database_type(pblock *pb, Session *sn, Request *rq)
{
    char *dbtype_str = pblock_findval(ACL_ATTR_DBTYPE, pb);
    char *parseFuncStr = pblock_findval(ACL_ATTR_PARSEFN, pb);
    ACLDbType_t dbtype = ACL_DBTYPE_INVALID;
    DbParseFn_t parseFunc;
    char err[BIG_LINE];
    NSErr_t *errp = 0;

    if (!dbtype_str || !*dbtype_str) {
	pblock_nvinsert("error", "dbtype is missing", pb);
	return REQ_ABORTED;
    }

    if (!parseFuncStr) {
	pblock_nvinsert("error", "parse function is missing", pb);
	return REQ_ABORTED;
    }

    parseFunc = (DbParseFn_t)func_find(parseFuncStr);

    if (!parseFunc) {
	sprintf(err, "Could map \"%s\" to a function", parseFunc);
	pblock_nvinsert("error", err, pb);
	return REQ_ABORTED;
    }

    ACL_REG(ACL_DbTypeRegister(errp, dbtype_str, parseFunc, &dbtype),
	    "Failed to register database type \"%s\"", dbtype_str);

    return REQ_PROCEED;
}

int register_database_name(pblock *pb, Session *sn, Request *rq)
{
    char *dbtype_str = pblock_findval(ACL_ATTR_DBTYPE, pb);
    char *dbname = pblock_findval(ACL_ATTR_DBNAME, pb);
    char *url = pblock_findval(ACL_ATTR_DATABASE_URL, pb);
    ACLDbType_t dbtype = ACL_DBTYPE_INVALID;
    NSErr_t *errp = 0;
    PList_t plist = PListCreate(NULL, ACL_ATTR_INDEX_MAX, NULL, NULL);

    if (!dbtype_str || !*dbtype_str) {
	pblock_nvinsert("error", "dbtype is missing", pb);
	return REQ_ABORTED;
    }

    if (!dbname || !*dbname) {
	pblock_nvinsert("error", "database name is missing", pb);
	return REQ_ABORTED;
    }

    if (!url || !*url) {
	pblock_nvinsert("error", "database url is missing", pb);
	return REQ_ABORTED;
    }

    ACL_REG(ACL_DbTypeFind(errp, dbtype_str, &dbtype),
	    "Database type \"%s\" is not registered", dbtype_str);

    ACL_REG(ACL_DatabaseRegister(errp, dbtype, dbname, url, plist),
	    "Failed to register database \"%s\"", dbname);

    return REQ_PROCEED;
}


/*-----------------------------------------------------------------------------
 * Various ACL/authdb initializations. See also libaccess/aclinit.cpp for
 * additional initializations (which run before this one).
 *
 */
int init_acl_modules (NSErr_t *errp)
{
    int pos = ACL_AT_END;


    /* Register the basic method */
    ACL_REG(ACL_MethodRegister(errp, ACL_AUTHTYPE_BASIC, &ACL_MethodBasic),
	    "Failed to register the method \"%s\"", ACL_AUTHTYPE_BASIC);

    /* Register the ssl method */
    ACL_REG(ACL_MethodRegister(errp, ACL_AUTHTYPE_SSL, &ACL_MethodSSL),
	    "Failed to register the method \"%s\"", ACL_AUTHTYPE_SSL);

    /* Register the digest method */
    ACL_REG(ACL_MethodRegister(errp, ACL_AUTHTYPE_DIGEST, &ACL_MethodDigest),
            "Failed to register the method \"%s\"", ACL_AUTHTYPE_DIGEST);

#ifdef FEAT_GSS
    /* Register the gssapi method */
    ACL_REG(ACL_MethodRegister(errp, ACL_AUTHTYPE_GSSAPI, &ACL_MethodGSSAPI),
            "Failed to register the method \"%s\"", ACL_AUTHTYPE_GSSAPI);
#endif

    //------------------------------------------------------------------------
    // Generic getters for "any" authdb
    
    // method "any" - generic attrs which don't depend on method/authdb
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_AUTHORIZATION,
				   get_authorization_basic,
				   ACL_METHOD_ANY, ACL_DBTYPE_ANY, pos, NULL),
	    "Failed to register attr getter for \"%s\"",
            ACL_ATTR_AUTHORIZATION);

    ACL_REG(ACL_AttrGetterRegister(NULL, ACL_ATTR_IP, LASIpv6Getter,
                                   ACL_METHOD_ANY,
                                   ACL_DBTYPE_ANY, pos, NULL),
            "Failed to register attr getter for \"%s\"",
            ACL_ATTR_IP);
    
    ACL_REG(ACL_AttrGetterRegister(NULL, ACL_ATTR_DNS, LASDnsGetter,
                                   ACL_METHOD_ANY,
                                   ACL_DBTYPE_ANY, pos, NULL),
            "Failed to register attr getter for \"%s\"",
            ACL_ATTR_DNS);

                                // XXX? why ldap fn for any/any?
    ACL_REG(ACL_AttrGetterRegister(NULL, ACL_ATTR_USERDN,
                                   get_userdn_ldap, ACL_METHOD_ANY,
                                   ACL_DBTYPE_ANY, pos, NULL),
            "Failed to register attr getter for \"%s\"",
            ACL_ATTR_USERDN);
        
    // method "basic"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER, get_auth_user_basic,
				   ACL_MethodBasic, ACL_DBTYPE_ANY, pos, NULL),
            "Failed to register attr getter for \"%s\"", ACL_ATTR_USER);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_RAW_USER,
                                   get_user_login_basic,
				   ACL_MethodBasic, ACL_DBTYPE_ANY, pos, NULL),
	    "Failed to register attr getter for \"%s\"", ACL_ATTR_RAW_USER);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_RAW_PASSWORD,
                                   get_user_login_basic,
				   ACL_MethodBasic, ACL_DBTYPE_ANY, pos, NULL),
	    "Failed to register attr getter for \"%s\"",
            ACL_ATTR_RAW_PASSWORD);

    // method "digest"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER, get_auth_user_basic,
                                   ACL_MethodDigest, ACL_DBTYPE_ANY, pos,NULL),
            "Failed to register attr getter for \"%s\"", ACL_ATTR_USER);
    
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_RAW_USER,
                                   get_user_login_basic,
                                   ACL_MethodDigest, ACL_DBTYPE_ANY, pos,NULL),
            "Failed to register attr getter for \"%s\"", ACL_ATTR_RAW_USER);
    
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_RAW_PASSWORD,
                                   get_user_login_basic,
                                   ACL_MethodDigest, ACL_DBTYPE_ANY, pos,NULL),
            "Failed to register attr getter for \"%s\"",ACL_ATTR_RAW_PASSWORD);


    // method "ssl"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_CERT, get_user_cert_ssl,
	 			   ACL_MethodSSL, ACL_DBTYPE_ANY, pos, NULL),
 	    "Failed to register attr getter for \"%s\"", ACL_ATTR_USER_CERT);

#ifdef FEAT_GSS
    // method "gssapi"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER, get_auth_user_gssapi,
                                   ACL_MethodGSSAPI, ACL_DBTYPE_ANY, pos,NULL),
            "Failed to register attr getter for \"%s\"", ACL_ATTR_USER);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_IS_VALID_PASSWORD,
                                   gssapi_authenticate_user, ACL_MethodGSSAPI,
                                   ACL_DBTYPE_ANY, pos, NULL),
            "Failed to register PAM attr getter for \"%s\"",
            ACL_ATTR_IS_VALID_PASSWORD);
#endif

    //------------------------------------------------------------------------
    // LDAP authdb getters

    // method "any"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_ISMEMBER,
				   get_user_ismember_ldap,
				   ACL_METHOD_ANY, ACL_DbTypeLdap, pos, NULL),
	    "Failed to register LDAP attr getter for \"%s\"",
            ACL_ATTR_USER_ISMEMBER);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_ISINROLE,
				   get_user_isinrole_ldap,
				   ACL_METHOD_ANY, ACL_DbTypeLdap, pos, NULL),
	    "Failed to register LDAP attr getter for \"%s\"",
            ACL_ATTR_USER_ISINROLE);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_EXISTS,
				   get_user_exists_ldap,
				   ACL_METHOD_ANY, ACL_DbTypeLdap, pos, NULL),
	    "Failed to register LDAP attr getter for \"%s\"",
            ACL_ATTR_USER_EXISTS);

    // method "basic"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_IS_VALID_PASSWORD,
				   get_is_valid_password_ldap, 
				   ACL_MethodBasic, ACL_DbTypeLdap, pos, NULL),
	    "Failed to register LDAP attr getter for \"%s\"",
	    ACL_ATTR_IS_VALID_PASSWORD);

    // method "digest"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_IS_VALID_PASSWORD,
                                   get_is_valid_password_ldap,
                                   ACL_MethodDigest, ACL_DbTypeLdap, pos,NULL),
            "Failed to register LDAP attr getter for \"%s\"",
            ACL_ATTR_IS_VALID_PASSWORD);

    // method "ssl"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER, get_auth_user_ssl,
				   ACL_MethodSSL, ACL_DbTypeLdap, pos, NULL),
	    "Failed to register SSL LDAP attr getter for \"%s\"",
            ACL_ATTR_USER);
    
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_CERT2GROUP,
                                   get_cert2group_ldap,
				   ACL_MethodSSL, ACL_DbTypeLdap, pos, NULL),
	    "Failed to register attr getter for \"%s\"", ACL_ATTR_CERT2GROUP);

    
    //------------------------------------------------------------------------
    // NULL authdb getters

    // method "any"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_EXISTS,
				   get_user_exists_null,
				   ACL_METHOD_ANY, ACL_DbTypeNull, pos, NULL),
	    "Failed to register NULL attr getter for \"%s\"",
            ACL_ATTR_USER_EXISTS);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_ISINROLE,
                                   get_user_isinrole_null,
				   ACL_METHOD_ANY, ACL_DbTypeNull, pos, NULL),
            "Failed to register NULL attr getter for \"%s\"",
            ACL_ATTR_USER_ISINROLE);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_ISMEMBER,
				   get_user_ismember_null,
				   ACL_METHOD_ANY, ACL_DbTypeNull, pos, NULL),
            "Failed to register NULL attr getter for \"%s\"",
            ACL_ATTR_USER_ISMEMBER);

    // method "basic"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_IS_VALID_PASSWORD,
				   get_is_valid_password_null, 
				   ACL_MethodBasic, ACL_DbTypeNull, pos, NULL),
	    "Failed to register NULL attr getter for \"%s\"",
	    ACL_ATTR_IS_VALID_PASSWORD);

    // method "digest"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_IS_VALID_PASSWORD,
				   get_is_valid_password_null, 
				   ACL_MethodDigest, ACL_DbTypeNull, pos,NULL),
	    "Failed to register NULL attr getter for \"%s\"",
	    ACL_ATTR_IS_VALID_PASSWORD);

    // method "ssl"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER, get_auth_user_ssl,
				   ACL_MethodSSL, ACL_DbTypeNull, pos, NULL),
	    "Failed to register SSL NULL attr getter for \"%s\"",
            ACL_ATTR_USER);


#ifdef FEAT_PAM
    //------------------------------------------------------------------------
    // PAM authdb getters (see p.103 of ACPG)

    // method "basic"
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_IS_VALID_PASSWORD,
                                   pam_authenticate_user, ACL_MethodBasic,
                                   ACL_DbTypePAM, pos, NULL),
            "Failed to register PAM attr getter for \"%s\"",
            ACL_ATTR_IS_VALID_PASSWORD);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_ISMEMBER,
                                   pam_user_ismember_get, ACL_MethodBasic,
                                   ACL_DbTypePAM, pos, NULL),
            "Failed to register PAM attr getter for \"%s\"",
            ACL_ATTR_USER_ISMEMBER);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_EXISTS,
                                   pam_userexists_get, ACL_MethodBasic,
                                   ACL_DbTypePAM, pos, NULL),
            "Failed to register PAM attr getter for \"%s\"",
            ACL_ATTR_USER_EXISTS);
#endif

    //------------------------------------------------------------------------
    // File authdb getters

    // method "any" (file supports both basic+digest)

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_IS_VALID_PASSWORD,
                                   fileacl_user_get,
                                   ACL_METHOD_ANY, ACL_DbTypeFile, pos, NULL),
            "Failed to register attr getter for \"%s\"",
            ACL_ATTR_IS_VALID_PASSWORD);
    
    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_ISMEMBER,
                                   fileacl_user_ismember_get,
                                   ACL_METHOD_ANY, ACL_DbTypeFile, pos, NULL),
            "Failed to register attr getter for \"%s\"",
            ACL_ATTR_USER_ISMEMBER);

    ACL_REG(ACL_AttrGetterRegister(errp, ACL_ATTR_USER_EXISTS,
                                   fileacl_userexists_get,
                                   ACL_METHOD_ANY, ACL_DbTypeFile, pos, NULL),
            "Failed to register attr getter for \"%s\"",
            ACL_ATTR_USER_EXISTS);
    

    return REQ_PROCEED;
}
