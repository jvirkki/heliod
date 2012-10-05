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
#include <base/nsassert.h>

#include <libaccess/acl.h>              // generic ACL definitions
#include <libaccess/aclproto.h>         // internal prototypes
#include <libaccess/aclglobal.h>        // global data
#include "aclpriv.h"                    // internal data structure definitions
#include <libaccess/dbtlibaccess.h>     // strings

#define ACL_MAX_DBTYPE          32

// -----------------------------------------------------------------------------
// ACL_DbTypeHash
// -----------------------------------------------------------------------------

// XXX why is this stuff here and not in aclspace.cpp??
ACLDbType_t ACL_DbTypeLdap = ACL_DBTYPE_INVALID;
ACLDbType_t ACL_DbTypeNull = ACL_DBTYPE_INVALID;
ACLDbType_t ACL_DbTypeFile = ACL_DBTYPE_INVALID;
ACLDbType_t ACL_DbTypePAM = ACL_DBTYPE_INVALID;
ACLDbType_t ACL_DbTypeKerberos = ACL_DBTYPE_INVALID;

DbParseFn_t     ACLDbParseFnTable[ACL_MAX_DBTYPE];

void
ACL_DbTypeHashInit()
{
    ACLDbTypeHash = PR_NewHashTable(ACL_MAX_DBTYPE, ACLPR_HashCaseString, ACLPR_CompareCaseStrings,
				     PR_CompareValues, &ACLPermAllocOps, NULL);
    NS_ASSERT(ACLDbTypeHash);

    for (int i = 0; i < ACL_MAX_DBTYPE; i++)
	ACLDbParseFnTable[i] = 0;
}

//
//  ACL_DbTypeRegister
//  INPUT
//	name		DbType name string.  Can be freed after return.
//  OUTPUT
//	&t		Place to return the DbType (>0)
//	retcode		0 on success, non-zero otherwise
//
// This function is exposed in the ACL API
//

int cur_dbtype = 0;	/* Use a static counter to generate the numbers */

NSAPI_PUBLIC int
ACL_DbTypeRegister(NSErr_t *errp, const char *name, DbParseFn_t func, ACLDbType_t *t)
{
    ACLDbType_t rv;

    ACL_CritEnter();

    /*  See if this is already registered  */
    rv = (ACLDbType_t) PR_HashTableLookup(ACLDbTypeHash, name);
    if (rv != NULL) {
	*t = rv;
	ACLDbParseFnTable[(int)(size_t)rv] = func;
        ACL_CritExit();
	return 0;
    }
	
    /*  To prevent the hash table from resizing, don't get to 32 entries  */
    if (cur_dbtype >= (ACL_MAX_DBTYPE-1)) {
	ACL_CritExit();
	return -1;
    }
	
    /*  Put it in the hash table  */
    rv = PR_HashTableAdd(ACLDbTypeHash, name, (void *)++cur_dbtype);
    *t = (ACLDbType_t) cur_dbtype;
    ACLDbParseFnTable[cur_dbtype] = func;

    ACL_CritExit();
    return 0;
}

//
// ACL_DbTypeFind - find a database type based on the name
//
// This function is exposed in the ACL API.
// 
NSAPI_PUBLIC int
ACL_DbTypeFind(NSErr_t *errp, const char *name, ACLDbType_t *t)
{
    ACLDbType_t rv;

    // we need to get the Critical Section lock 'cause PR_Hash actually
    // modifies the hash table on every lookup.
    ACL_CritEnter();
    rv = (ACLDbType_t) PR_HashTableLookup(ACLDbTypeHash, name);
    if (rv != NULL) {
	*t = rv;
        ACL_CritExit();
	return 0;
    }
    ACL_CritExit();
    return -1;
}

//
// ACL_DbTypeIsRegistered - find out if this database type is registered
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_DbTypeIsRegistered (NSErr_t *errp, const ACLDbType_t t)
{
    return (0 < ((int)(size_t)t) && ((int)(size_t)t) <= cur_dbtype);
}

//
//  ACL_DbTypeIsEqual - find out if the argument database types are equal
//
//	RETURNS		non-zero if equal.
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_DbTypeIsEqual(NSErr_t *errp, const ACLDbType_t t1, const ACLDbType_t t2)
{
    return (t1 == t2);
}


//
//  ACL_DbTypeNameIsEqual - takes a dbtype type and a dbtype name and sees if they match.
//
//	Returns non-zero on match.
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_DbTypeNameIsEqual(NSErr_t *errp, const ACLDbType_t t1, const char *name)
{
    int		rv;
    ACLDbType_t	t2;

    rv = ACL_DbTypeFind(errp, name, &t2);
    if (rv) 
        return (rv);
    else
        return (t1 == t2);
}

ACLDbType_t ACLDbTypeDefault = ACL_DBTYPE_INVALID;

//
// ACL_DbTypeGetDefault - get the default dbtype
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC ACLDbType_t
ACL_DbTypeGetDefault(NSErr_t *errp)
{
    // Default dbtype can vary by virtual server
    ACLDbType_t dbtype;
    void *db;
    if (ACL_DatabaseFind(errp, NULL, &dbtype, &db) == LAS_EVAL_TRUE)
        return dbtype;

    return (ACLDbTypeDefault);
}

//
// ACL_DbTypeSetDefault - set the default dbtype
//
// This function is **NOT** exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_DbTypeSetDefault(NSErr_t *errp, ACLDbType_t t)
{
    ACLDbTypeDefault = t;
    return 0;
}

//
// ACL_DbTypeParseFn - return the parse function pointer for this dbtype
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC DbParseFn_t
ACL_DbTypeParseFn(NSErr_t *errp, const ACLDbType_t dbtype)
{
    if (ACL_DbTypeIsRegistered(errp, dbtype))
	return ACLDbParseFnTable[(int)(size_t)dbtype];
    else
	return 0;
}

//
//  ACL_AuthInfoGetDbType - get cached dbtype out of auth_info's ACL_ATTR_DBTYPE
//                          or, if not there, deliver ACLDbTypeDefault
//
//  This function is exposed in the ACL API, but is **OBSOLETE** as of iWS6.0.
//  Reason: To figure out the dbtype, one needs to known the VS, because the actual
//          database might be different for every VS.
//
//	INPUT
//	auth_info	A PList of the authentication name/value pairs as
//			provided by EvalTestRights to the LAS.
//	OUTPUT
//	*t		The DbType number.  This can be the default dbtype
//			number if the auth_info PList doesn't explicitly
//			have a DbType entry.
//	retcode		0 on success.
//
NSAPI_PUBLIC int
ACL_AuthInfoGetDbType(NSErr_t *errp, PList_t auth_info, ACLDbType_t *t)
{
    const char *dbname;
    if (!auth_info || PListGetValue(auth_info, ACL_ATTR_DATABASE_INDEX, (void **)&dbname, NULL) < 0)
        dbname = NULL;

    void *db;
    if (ACL_DatabaseFind(errp, dbname, t, &db) == LAS_EVAL_TRUE)
        return 0;

    return -1;
}
