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

#ifndef PUBLIC_NSACL_ACLDEF_H
#define PUBLIC_NSACL_ACLDEF_H

/*
 * File:        acldef.h
 *
 * Description:
 *
 *      This file contains constant and type definitions for the ACL API.
 */

#ifndef PUBLIC_NSAPI_H
#include "nsapi.h"
#endif /* !PUBLIC_NSAPI_H */

#ifndef PUBLIC_NSACL_NSERRDEF_H
#include "nserrdef.h"
#endif /* !PUBLIC_NSACL_NSERRDEF_H */

#ifndef PUBLIC_NSACL_PLISTDEF_H
#include "plistdef.h"
#endif /* !PUBLIC_NSACL_PLISTDEF_H */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Type:        ACLCachable_t
 *
 * Description:
 *
 *      This type is used to specify whether and how long something
 *      may be safely cached.  A value of zero (ACL_NOT_CACHABLE)
 *      indicates that the item is not cachable.  Any other value is
 *      a time, in seconds since 00:00:00 UTC, January 1, 1970, after
 *      which the cached information should be discarded.
 */

typedef unsigned long ACLCachable_t;

#define ACL_NOT_CACHABLE        0
#define ACL_INDEF_CACHABLE      ((unsigned long)(-1))

/*
 * Type:        ACLListHandle_t
 *
 * Description:
 *
 *      This type represents a list of ACLs in their in-memory form.
 */

typedef struct ACLListHandle ACLListHandle_t;

/* The object has been checked for ACLs and has none attached */
#define	ACL_LIST_NO_ACLS	((ACLListHandle_t *)-1)

/*
 * Type:        ACLHandle_t
 *
 * Description:
 *
 *      This type represents the in-memory form of an ACL.
 */

typedef struct ACLHandle ACLHandle_t;

/*
 * Type:        ACLListEnum_t
 *
 * Description:
 *
 *      This type contains the state of an ACL list enumeration.
 */

typedef void *ACLListEnum_t;

/*
 * Type:        ACLExprHandle_t
 *
 * Description:
 *
 *      This type represents a single ACL entry, e.g. allow, deny, etc.
 */

typedef struct ACLExprHandle ACLExprHandle_t;

/*
 * Type:        ACLEvalHandle_t
 *
 * Description:
 *
 *      This type represents an ACL evaluation context, which includes
 *      an ACL list and property lists for the subject and resource.
 */

typedef struct ACLEvalHandle ACLEvalHandle_t;

/*
 * Type:        PFlags_t
 *
 * Description:
 *
 *      This type represents a set of processing flags for an ACL entry.
 */
typedef int PFlags_t;

#define ACL_PFLAG_ABSOLUTE  	0x1
#define ACL_PFLAG_TERMINAL  	0x2
#define ACL_PFLAG_CONTENT  	0x4

#define IS_ABSOLUTE(x)		((x) & ACL_PFLAG_ABSOLUTE)
#define IS_STATIC(x)		((x) & ACL_PFLAG_STATIC)
#define IS_CONTENT(x)		((x) & ACL_PFLAG_CONTENT)

/*
 * Type:        CmpOp_t
 *
 * Description:
 *
 *      This type represents a comparison operator in an ACL attribute
 *      expression.
 */
typedef enum	{
		CMP_OP_EQ,
		CMP_OP_NE,
		CMP_OP_GT,
		CMP_OP_LT,
		CMP_OP_GE,
		CMP_OP_LE
		} CmpOp_t;

/*
 * Type:        ACLExprType_t
 *
 * Description:
 *
 *      This type represents the type of an ACL entry.
 */
typedef enum 	{
		ACL_EXPR_TYPE_ALLOW,
		ACL_EXPR_TYPE_DENY,
		ACL_EXPR_TYPE_AUTH,
		ACL_EXPR_TYPE_RESPONSE
		} ACLExprType_t;

/*
 * Type:        ACLEvalRes_t
 *
 * Description:
 *
 *      This type represents the result of ACL evaluation.
 */
typedef enum	{
		ACL_RES_ALLOW,
		ACL_RES_DENY,
		ACL_RES_FAIL,
		ACL_RES_INVALID,
		ACL_RES_NONE
		} ACLEvalRes_t;

/*
 * Type:        ACLMethod_t
 *
 * Description:
 *
 *      This type represents a reference to an authentication method.
 */
typedef	void * ACLMethod_t;

#define	ACL_METHOD_ANY		((ACLMethod_t)-1)
#define	ACL_METHOD_INVALID	((ACLMethod_t)-2)

/*
 * Type:        ACLDbType_t
 *
 * Description:
 *
 *      This type represents a reference to a type of authentication
 *      database.
 */
typedef	void * ACLDbType_t;

#define	ACL_DBTYPE_ANY		((ACLDbType_t)-1)
#define	ACL_DBTYPE_INVALID	((ACLDbType_t)-2)

/*
 * Type:        ACLAttrGetterFn_t
 *
 * Description:
 *
 *      This type describes a kind of callback function that obtains
 *      a value for an ACL attribute and enters the attribute and value
 *      into the subject property list.
 */
typedef int (*ACLAttrGetterFn_t)(NSErr_t *errp, PList_t subject,
                                 PList_t resource, PList_t auth_info,
                                 PList_t global_auth, void *arg);

typedef struct ACLAttrGetter ACLAttrGetter_t;
typedef void *ACLAttrGetterList_t;

/*
 * Type:        AclModuleInitFunc
 *
 * Description:
 *
 *      This type describes a kind of callback function that is
 *      specified to ACL_ModuleRegister() and called from there.
 *	The function should return 0 on success and non-zero on
 *	failure.
 */
typedef int (*AclModuleInitFunc)(NSErr_t *errp);

/*
 * Type:        DbParseFn_t
 *
 * Description:
 *
 *      This type describes a kind of callback function that parses
 *      a reference to an authentication database of a particular
 *      database type.  It is called when ACL_DatabaseRegister() is
 *      called for a database which is that database type.
 *	The function should return 0 on success and non-zero on
 *	failure.
 */
typedef int (*DbParseFn_t)(NSErr_t *errp, ACLDbType_t dbtype,
			   const char *name, const char *url,
			   PList_t plist, void **db);

/*
 * Type:        AclCacheFlushFunc_t
 *
 * Description:
 *
 *      This type describes a kind of callback function that is called
 *      when ACL_CacheFlush() is called.
 */
typedef int (*AclCacheFlushFunc_t)(void);

/*
 * Type:        LASEvalFunc_t
 *
 * Description:
 *
 *      This type describes a kind of callback function that is called
 *      to evaluate an attribute value expression in an ACL statement.
 */
typedef int (*LASEvalFunc_t)(NSErr_t *errp, char *attr_name,
                             CmpOp_t comparator, char *attr_pattern,
                             ACLCachable_t *cachable, void **cookie,
                             PList_t subject, PList_t resource,
                             PList_t auth_info, PList_t global_auth);

/*
 * Type:        LASFlushFunc_t
 *
 * Description:
 *
 *      This type describes a kind of callback function that is called
 *      when a previously cached LAS cookie is being flushed from
 *      the ACL cache.
 */
typedef void (*LASFlushFunc_t)(void **cookie);

/*
 * Type:        LDAP
 *
 * Description:
 *
 *      This is an opaque type that represents an open LDAP connection.
 *      It is used mostly via the LDAP SDK API.
 *	Include the <ldap.h> file before including this file if you wish to
 *	use the function ACL_LDAPDatabaseHandle.
 */
#ifndef _LDAP_H
typedef struct ldap LDAP;
#endif /* _LDAP_H */

/*
 * Type:        ACLVirtualDb_t
 *
 * Description:
 *
 *      This is an opaque type that represents authentication database
 *      information that is specific to a particular virtual server.
 *
 *      In previous releases of the ACL API header files, ACLVirtualDb_t was
 *      named UserDB.
 */
typedef struct ACLVirtualDb ACLVirtualDb_t;

#ifdef ACL_USE_DEPRECATED_USERDB
typedef ACLVirtualDb_t UserDB;
#endif


/*  Flags to ACL_ListFind  */
#define ACL_CASE_INSENSITIVE	 0x1
#define ACL_CASE_SENSITIVE	 0x2

#define	ACL_MAX_TEST_RIGHTS	32
#define	ACL_MAX_GENERIC		32

/*
 * ACLERRFAIL -- Use this as an 'retcode' argument to nserrGenerate.
 */
#define ACLERRFAIL	-11


/*
 *	Command values for the "position" argument to ACL_RegisterGetter
 *	Any positive >0 value is the specific position in the list to insert
 *	the new function.
 */
#define	ACL_AT_FRONT		0
#define	ACL_AT_END		-1
#define	ACL_REPLACE_ALL 	-2
#define	ACL_REPLACE_MATCHING	-3

#define ACL_ATTR_GROUP          "group"
#define ACL_ATTR_GROUP_INDEX		1
#define ACL_ATTR_RAW_USER_LOGIN "user-login"
#define ACL_ATTR_RAW_USER_LOGIN_INDEX	2
#define ACL_ATTR_AUTH_USER	"auth-user"
#define ACL_ATTR_AUTH_USER_INDEX	3
#define ACL_ATTR_AUTH_TYPE	"auth-type"
#define ACL_ATTR_AUTH_TYPE_INDEX	4
#define ACL_ATTR_AUTH_DB	"auth-db"
#define ACL_ATTR_AUTH_DB_INDEX		5
#define ACL_ATTR_AUTH_PASSWORD  "auth-password"
#define ACL_ATTR_AUTH_PASSWORD_INDEX	6
#define ACL_ATTR_USER	        "user"
#define ACL_ATTR_USER_INDEX		7
#define ACL_ATTR_PASSWORD	"pw"
#define ACL_ATTR_PASSWORD_INDEX		8
#define ACL_ATTR_USERDN	        "userdn"
#define ACL_ATTR_USERDN_INDEX		9
#define ACL_ATTR_RAW_USER	"raw-user"
#define ACL_ATTR_RAW_USER_INDEX		10
#define ACL_ATTR_RAW_PASSWORD   "raw-pw"
#define ACL_ATTR_RAW_PASSWORD_INDEX	11
#define ACL_ATTR_USER_ISMEMBER  "user-ismember"
#define ACL_ATTR_USER_ISMEMBER_INDEX	12
#define ACL_ATTR_DATABASE	"database"
#define ACL_ATTR_DATABASE_INDEX		13
#define ACL_ATTR_DBTYPE	        "dbtype"
#define ACL_ATTR_DBTYPE_INDEX		14
#define ACL_ATTR_DBNAME	        "dbname"
#define ACL_ATTR_DBNAME_INDEX		15
#define ACL_ATTR_DATABASE_URL   "url"
#define ACL_ATTR_DATABASE_URL_INDEX	16
#define ACL_ATTR_METHOD	        "method"
#define ACL_ATTR_METHOD_INDEX		17
#define ACL_ATTR_AUTHTYPE	"authtype"
#define ACL_ATTR_AUTHTYPE_INDEX		18
#define ACL_ATTR_AUTHORIZATION  "authorization"
#define ACL_ATTR_AUTHORIZATION_INDEX	19
#define ACL_ATTR_PARSEFN	"parsefn"
#define ACL_ATTR_PARSEFN_INDEX		20
#define ACL_ATTR_ATTRIBUTE	"attr"
#define ACL_ATTR_ATTRIBUTE_INDEX	21
#define ACL_ATTR_GETTERFN	"getterfunc"
#define ACL_ATTR_GETTERFN_INDEX		22
#define ACL_ATTR_IP		"ip"
#define ACL_ATTR_IP_INDEX		23
#define ACL_ATTR_DNS	        "dns"
#define ACL_ATTR_DNS_INDEX		24
#define ACL_ATTR_MODULE	        "module"
#define ACL_ATTR_MODULE_INDEX		25
#define ACL_ATTR_MODULEFUNC	"func"
#define ACL_ATTR_MODULEFUNC_INDEX	26
#define ACL_ATTR_GROUPS	        "groups"
#define ACL_ATTR_GROUPS_INDEX		27
#define ACL_ATTR_IS_VALID_PASSWORD "isvalid-password"
#define ACL_ATTR_IS_VALID_PASSWORD_INDEX	28
#define ACL_ATTR_CERT2USER	"cert2user"
#define ACL_ATTR_CERT2USER_INDEX	29
#define ACL_ATTR_USER_CERT	"cert"
#define ACL_ATTR_USER_CERT_INDEX	30
#define ACL_ATTR_PROMPT	        "prompt"
#define ACL_ATTR_PROMPT_INDEX		31
#define ACL_ATTR_TIME	        "time"
#define ACL_ATTR_TIME_INDEX		32
#define ACL_ATTR_USERS_GROUP    "users-group"
#define ACL_ATTR_USERS_GROUP_INDEX	33
#define	ACL_ATTR_SESSION		"session"       /* subject property */
#define ACL_ATTR_SESSION_INDEX		34
#define	ACL_ATTR_REQUEST		"request"       /* resource property */
#define ACL_ATTR_REQUEST_INDEX		35
#define ACL_ATTR_ERROR		"error"
#define	ACL_ATTR_ERROR_INDEX		36
#define ACL_ATTR_PROGRAMS		"programs"      /* resource property */
#define	ACL_ATTR_PROGRAMS_INDEX		37
#define ACL_ATTR_ACCEL_AUTH		"accel-authorization"
#define ACL_ATTR_ACCEL_AUTH_INDEX	38
#define ACL_ATTR_WWW_AUTH_PROMPT	"www-auth-prompt"
#define ACL_ATTR_WWW_AUTH_PROMPT_INDEX	39
#define ACL_ATTR_OWNER			"owner"
#define ACL_ATTR_OWNER_INDEX		40
#define ACL_ATTR_IS_OWNER		"is-owner"
#define ACL_ATTR_IS_OWNER_INDEX		41
#define ACL_ATTR_CACHED_USER		"cached-user"
#define ACL_ATTR_CACHED_USER_INDEX	42
#define ACL_ATTR_USER_EXISTS		"user-exists"
#define ACL_ATTR_USER_EXISTS_INDEX	43
#define ACL_ATTR_CERT2GROUP		"cert2group"
#define ACL_ATTR_CERT2GROUP_INDEX	44
#define ACL_ATTR_NSDBUSEROBJ            "nsdb-userobj"
#define ACL_ATTR_NSDBUSEROBJ_INDEX      45
#define ACL_ATTR_CIPHER			"cipher"
#define ACL_ATTR_CIPHER_INDEX		46
#define ACL_ATTR_CERTMAP		"certmap"
#define ACL_ATTR_CERTMAP_INDEX		47
#define ACL_ATTR_VS     		"vs"
#define ACL_ATTR_VS_INDEX		48
#define ACL_ATTR_RSPAUTH                "rspauth"
#define ACL_ATTR_RSPAUTH_INDEX          49
#define ACL_ATTR_STALE_DIGEST           "stale"
#define ACL_ATTR_STALE_DIGEST_INDEX     50
#define ACL_ATTR_WWW_AUTH_METHOD        "www-auth-method"
#define ACL_ATTR_WWW_AUTH_METHOD_INDEX  51
#define ACL_ATTR_ROLE                   "role"
#define ACL_ATTR_ROLE_INDEX             52
#define ACL_ATTR_ROLES                  "roles"
#define ACL_ATTR_ROLES_INDEX            53
#define ACL_ATTR_USER_ISINROLE          "user-isinrole"
#define ACL_ATTR_USER_ISINROLE_INDEX    54
#define ACL_ATTR_AUTHREQCHECK          "authreqcheck"
#define ACL_ATTR_AUTHREQCHECK_INDEX     55
#define ACL_ATTR_SERVERNAME            "servername"
#define ACL_ATTR_SERVERNAME_INDEX       56
#define ACL_ATTR_LOCK_OWNER            "lock-owner"
#define ACL_ATTR_LOCK_OWNER_INDEX       57
#define ACL_ATTR_IS_LOCK_OWNER         "is-lock-owner"
#define ACL_ATTR_IS_LOCK_OWNER_INDEX    58


/*	Must be 1 larger than the highest index used	*/
#define	ACL_ATTR_INDEX_MAX		59

#ifdef	ALLOCATE_ATTR_TABLE
/* Must be in the same order as the index numbers */
char	*ACLAttrTable[] = {
		 NULL,				/*  0 */
/* Don't have one numbered 0 */
		 ACL_ATTR_GROUP,		/*  1 */
		 ACL_ATTR_RAW_USER_LOGIN,	/*  2 */
		 ACL_ATTR_AUTH_USER,		/*  3 */
		 ACL_ATTR_AUTH_TYPE,		/*  4 */
		 ACL_ATTR_AUTH_DB,		/*  5 */
		 ACL_ATTR_AUTH_PASSWORD,	/*  6 */
		 ACL_ATTR_USER,			/*  7 */
		 ACL_ATTR_PASSWORD,		/*  8 */
		 ACL_ATTR_USERDN,		/*  9 */
		 ACL_ATTR_RAW_USER,		/* 10 */
		 ACL_ATTR_RAW_PASSWORD,		/* 11 */
		 ACL_ATTR_USER_ISMEMBER,	/* 12 */
		 ACL_ATTR_DATABASE,		/* 13 */
		 ACL_ATTR_DBTYPE,		/* 14 */
		 ACL_ATTR_DBNAME,		/* 15 */
		 ACL_ATTR_DATABASE_URL,		/* 16 */
		 ACL_ATTR_METHOD,		/* 17 */
		 ACL_ATTR_AUTHTYPE,		/* 18 */
		 ACL_ATTR_AUTHORIZATION,	/* 19 */
		 ACL_ATTR_PARSEFN,		/* 20 */
		 ACL_ATTR_ATTRIBUTE,		/* 21 */
		 ACL_ATTR_GETTERFN,		/* 22 */
		 ACL_ATTR_IP,			/* 23 */
		 ACL_ATTR_DNS,			/* 24 */
		 ACL_ATTR_MODULE,		/* 25 */
		 ACL_ATTR_MODULEFUNC,		/* 26 */
		 ACL_ATTR_GROUPS,		/* 27 */
		 ACL_ATTR_IS_VALID_PASSWORD,	/* 28 */
		 ACL_ATTR_CERT2USER,		/* 29 */
		 ACL_ATTR_USER_CERT,		/* 30 */
		 ACL_ATTR_PROMPT,		/* 31 */
		 ACL_ATTR_TIME,			/* 32 */
		 ACL_ATTR_USERS_GROUP,		/* 33 */
		 ACL_ATTR_SESSION,		/* 34 */
		 ACL_ATTR_REQUEST,		/* 35 */
		 ACL_ATTR_ERROR,		/* 36 */
		 ACL_ATTR_PROGRAMS,		/* 37 */
		 ACL_ATTR_ACCEL_AUTH,		/* 38 */
		 ACL_ATTR_WWW_AUTH_PROMPT,	/* 39 */
		 ACL_ATTR_OWNER,		/* 40 */
		 ACL_ATTR_IS_OWNER,		/* 41 */
		 ACL_ATTR_CACHED_USER,		/* 42 */
		 ACL_ATTR_USER_EXISTS,		/* 43 */
		 ACL_ATTR_CERT2GROUP,		/* 44 */
                 ACL_ATTR_NSDBUSEROBJ,          /* 45 */
		 ACL_ATTR_CIPHER,		/* 46 */
		 ACL_ATTR_CERTMAP,		/* 47 */
		 ACL_ATTR_VS,    		/* 48 */
                 ACL_ATTR_RSPAUTH,              /* 49 */
                 ACL_ATTR_STALE_DIGEST,         /* 50 */
                 ACL_ATTR_WWW_AUTH_METHOD,      /* 51 */
                 ACL_ATTR_ROLE,                 /* 52 */
                 ACL_ATTR_ROLES,                /* 53 */
                 ACL_ATTR_USER_ISINROLE,        /* 54 */
                 ACL_ATTR_AUTHREQCHECK,         /* 55 */
                 ACL_ATTR_SERVERNAME,           /* 56 */
                 ACL_ATTR_LOCK_OWNER,           /* 57 */
                 ACL_ATTR_IS_LOCK_OWNER         /* 58 */
};
#endif


#define ACL_DBTYPE_LDAP         "ldap"
#define ACL_DBTYPE_NULL         "null"
#define ACL_DBTYPE_NSDB         "nsdb"
#define ACL_DBTYPE_FILE         "file"
#define ACL_DBTYPE_PAM          "pam"
#define ACL_DBTYPE_KERBEROS     "kerberos"

#define METHOD_DEFAULT          "default"

/*  Errors must be < 0 */
#define ACL_RES_ERROR      	-1

/* LAS return codes - Must all be negative numbers */
#define	LAS_EVAL_TRUE		-1
#define	LAS_EVAL_FALSE		-2
#define	LAS_EVAL_DECLINE	-3
#define	LAS_EVAL_FAIL		-4
#define	LAS_EVAL_INVALID	-5
#define	LAS_EVAL_NEED_MORE_INFO	-6
#define LAS_EVAL_PWEXPIRED      -7

/* Max pathlength.  Intended to match REQ_MAX_LEN */
#define ACL_PATH_MAX	4096

#ifdef __cplusplus
}
#endif

#endif /* !PUBLIC_NSACL_ACLDEF_H */
