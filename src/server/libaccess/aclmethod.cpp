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
#include "plhash.h"

ACLMethod_t ACLMethodDefault = ACL_METHOD_INVALID;

#define ACL_MAX_METHOD          32

// -----------------------------------------------------------------------------
// ACL_MethodHash
// -----------------------------------------------------------------------------

void
ACL_MethodHashInit()
{
    ACLMethodHash = PR_NewHashTable(ACL_MAX_METHOD,
				     ACLPR_HashCaseString,
				     ACLPR_CompareCaseStrings,
				     PR_CompareValues,
				     &ACLPermAllocOps,
				     NULL);
    NS_ASSERT(ACLMethodHash);
}

/*  ACL_MethodRegister
 *  INPUT
 *	name		Method name string.  Can be freed after return.
 *  OUTPUT
 *	&t		Place to return the Method_t (>0)
 *	retcode		0 on success, non-zero otherwise
 */

int cur_method = 0;	/* Use a static counter to generate the numbers */

NSAPI_PUBLIC int
ACL_MethodRegister(NSErr_t *errp, const char *name, ACLMethod_t *t)
{
    ACLMethod_t rv;

    ACL_CritEnter();

    /*  See if this is already registered  */
    rv = (ACLMethod_t) PR_HashTableLookup(ACLMethodHash, name);
    if (rv != NULL) {
	*t = rv;
        ACL_CritExit();
	return 0;
    }

    /*  To prevent the hash table from resizing, don't get to 32 entries  */
    if (cur_method >= (ACL_MAX_METHOD-1)) {
	ACL_CritExit();
	return -1;
    }
	
    /*  Put it in the hash table  */
    rv = PR_HashTableAdd(ACLMethodHash, name, (void *)++cur_method);
    *t = (ACLMethod_t) cur_method;

    ACL_CritExit();
    return 0;
}

NSAPI_PUBLIC int
ACL_MethodFind(NSErr_t *errp, const char *name, ACLMethod_t *t)
{
    ACLMethod_t rv;

    // we need to get the Critical Section lock 'cause PR_Hash actually
    // modifies the hash table on every lookup.
    ACL_CritEnter();
    rv = (ACLMethod_t) PR_HashTableLookup(ACLMethodHash, name);
    if (rv != NULL) {
	*t = rv;
        ACL_CritExit();
	return 0;
    }
    ACL_CritExit();

    return -1;
}

NSAPI_PUBLIC int
ACL_MethodNamesGet(NSErr_t *errp, char ***names, int *count)
{
    *count = cur_method;
    return acl_registered_names (ACLMethodHash, *count, names);
}

NSAPI_PUBLIC int
ACL_MethodNamesFree(NSErr_t *errp, char **names, int count)
{
    int i;

    if (!names) return 0;

    for (i = count-1; i>=0; i--) FREE(names[i]);

    FREE(names);
    return 0;
}

/*  ACL_MethodIsEqual
 *	RETURNS		non-zero if equal.
 */
NSAPI_PUBLIC int
ACL_MethodIsEqual(NSErr_t *errp, const ACLMethod_t t1, const ACLMethod_t t2)
{
    return (t1 == t2);
}

/*  ACL_MethodNameIsEqual
 *	Takes a method type and a method name and sees if they match.
 *	Returns non-zero on match.
 */
NSAPI_PUBLIC int
ACL_MethodNameIsEqual(NSErr_t *errp, const ACLMethod_t t1, const char *name)
{
    int		rv;
    ACLMethod_t	t2;

    rv = ACL_MethodFind(errp, name, &t2);
    if (rv) 
        return (rv);
    else
        return (t1 == t2);
}

/*  ACL_MethodGetDefault
 */
NSAPI_PUBLIC ACLMethod_t
ACL_MethodGetDefault(NSErr_t *errp)
{
    return (ACLMethodDefault);
}

/*  ACL_MethodSetDefault
 */
NSAPI_PUBLIC int
ACL_MethodSetDefault(NSErr_t *errp, const ACLMethod_t t)
{
    ACLMethodDefault = t;
    return 0;
}

//
//  ACL_AuthInfoGetMethod
//	INPUT
//	auth_info	A PList of the authentication name/value pairs as
//			provided by EvalTestRights to the LAS.
//	OUTPUT
//	*t		The Method number.  This can be the default method number if
//                      the auth_info PList doesn't explicitly have a Method entry.
//	retcode		0 on success.
//
NSAPI_PUBLIC int
ACL_AuthInfoGetMethod(NSErr_t *errp, PList_t auth_info, ACLMethod_t *t)
{
    ACLMethod_t *methodp;

    if (!auth_info || PListGetValue(auth_info, ACL_ATTR_METHOD_INDEX, (void **)&methodp, NULL) < 0) {
	// No cached value for "method"
	*t = ACLMethodDefault;
    } else {
	*t = *methodp;
    }

    return 0;
}

//
//  ACL_AuthInfoSetMethod
//    INPUT
//	auth_info	A PList of the authentication name/value pairs as
//			provided by EvalTestRights to the LAS.
//	t		The Method number.
//    OUTPUT
//	retcode		0 on success.
//
NSAPI_PUBLIC int
ACL_AuthInfoSetMethod(NSErr_t *errp, PList_t auth_info, ACLMethod_t t)
{
    ACLMethod_t *methodp;
    int rv;

    if (auth_info) {
	rv = PListGetValue(auth_info, ACL_ATTR_METHOD_INDEX, (void **)&methodp, NULL);
	if (rv < 0) {
	    // No cached value for "method"
	    pool_handle_t *auth_info_pool = PListGetPool(auth_info);
	    methodp = (ACLMethod_t *)pool_malloc(auth_info_pool, sizeof(ACLMethod_t));
	    if (!methodp)
                return -1;
	    *methodp = t;
	    PListInitProp(auth_info, ACL_ATTR_METHOD_INDEX, ACL_ATTR_METHOD, methodp, 0);
	} else {
	    // replace the old entry
	    if (!methodp) return -1;
	    *methodp = t;
	}
    } else {
	return -1;
    }

    return 0;
}
