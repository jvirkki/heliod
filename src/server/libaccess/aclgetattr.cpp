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
#include <base/ereport.h>
#include <libaccess/acl.h>              // generic ACL definitions
#include <libaccess/aclproto.h>         // internal prototypes
#include <libaccess/aclglobal.h>        // global data
#include <libaccess/aclerror.h>         // error codes
#include "aclpriv.h"                    // internal data structure definitions
#include <libaccess/dbtlibaccess.h>     // strings
#include "plhash.h"
#include <base/util.h>
#include "libaccess/dacl.h"

//
// attr_getter_is_matching - see if the current getter matches method and dbtype
//
static int
attr_getter_is_matching(NSErr_t *errp, ACLAttrGetter_t *getter, ACLMethod_t method, ACLDbType_t dbtype)
{
    return ((ACL_MethodIsEqual(errp, getter->method, method) || ACL_MethodIsEqual(errp, getter->method, ACL_METHOD_ANY)) &&
	    (ACL_DbTypeIsEqual(errp, getter->dbtype, dbtype) || ACL_DbTypeIsEqual(errp, getter->dbtype, ACL_DBTYPE_ANY)));
}
				   
//
// ACL_GetAttribute - find and call one or more matching attribute getter functions
//
NSAPI_PUBLIC int
ACL_GetAttribute(NSErr_t *errp, const char *attr, void **val,
		     		  PList_t subject, PList_t resource, 
				  PList_t auth_info, PList_t global_auth) 
{ 
    int rv; 
    void *attrval;
    ACLAttrGetterFn_t func;
    ACLAttrGetterList_t getters;
    ACLAttrGetter_t *getter;
    ACLMethod_t method;
    char *dbname;
    ACLDbType_t dbtype;

    /* If subject PList is NULL, we will fail anyway */
    if (!subject)
        return LAS_EVAL_FAIL;

    /* Is the attribute already present in the subject property list? */
    rv = PListFindValue(subject, attr, &attrval, NULL);
    if (rv >= 0) {
        /* Yes, take it from there */
	*val = attrval;
	return LAS_EVAL_TRUE;
    }

    /* Get the authentication method and database type */
    // XXX umm... for ACLs that do not depend on user databases and authentication
    // methods (like cipher, dns, ip, tod!), we do not need method and database type.
    // so there's no reason to fail if we don't find anything here.
    // I think setting method to ACL_METHOD_ANY and dbtype to ACL_DBTYPE_ANY would
    // do the job in attr_getter_is_matching - this way, we would find only attr
    // getters that do not care about method and dbtype.

    if (ACL_AuthInfoGetMethod(errp, auth_info, &method) < 0) {
	nserrGenerate(errp, ACLERRFAIL, ACLERR4300, ACL_Program, 2,
            XP_GetAdminStr(DBT_GetAttributeCouldntDetermineMethod), attr);
        return LAS_EVAL_FAIL;
    }

    // dbtype is cached by our friendly ACLEvalAce caller (it's constant for the ACE)
    // XXX what if we don't get called by ACLEvalAce?
    if (PListGetValue(resource, ACL_ATTR_DBTYPE_INDEX, &dbtype, NULL) < 0) {
        dbtype = ACL_DBTYPE_INVALID;
    }

    /* Get the list of attribute getters */
    if ((ACL_AttrGetterFind(errp, attr, &getters) < 0) || (getters == 0)) {
	nserrGenerate(errp, ACLERRFAIL, ACLERR4310, ACL_Program, 2,
                      XP_GetAdminStr(DBT_GetAttributeCouldntLocateGetter), attr);
        return LAS_EVAL_DECLINE;
    }

    // Iterate over each getter and see if it should be called
    // Call each matching getter until a getter which doesn't decline is
    // found.
    char * method_name = NULL;
    char * dbtype_name = NULL;

    for (getter = ACL_AttrGetterFirst(&getters); getter != 0; getter = ACL_AttrGetterNext(&getters, getter)) {

        /* Require matching method and database type */

        if (!attr_getter_is_matching(errp, getter, method, dbtype))
            continue;

        if (ereport_can_log(LOG_VERBOSE)) {
            method_name = acl_get_name(ACLMethodHash, method);
            dbtype_name = acl_get_name(ACLDbTypeHash, dbtype);
            ereport(LOG_VERBOSE, "acl: calling getter for (attr=%s; "
                    "method=%s, dbtype=%s)", attr, method_name, dbtype_name);
        }

        /* Call the getter function */
        func = getter->fn;
        rv = (*func)(errp, subject, resource, auth_info, global_auth, getter->arg);

        if (method_name) {
            ereport(LOG_VERBOSE, "acl: getter for (attr=%s; "
                    "method=%s, dbtype=%s) returns %d", 
                    attr, method_name, dbtype_name, rv);
            FREE(method_name);
            FREE(dbtype_name);
        }

        // if the getter declined, let's try to find another one
        if (rv == LAS_EVAL_DECLINE)
            continue;

        /* Did the getter succeed? */
        if (rv == LAS_EVAL_TRUE) {
            /*
             * Yes, it should leave the attribute on the subject
             * property list.
             */
            if (PListFindValue(subject, attr, (void **)&attrval, NULL) < 0) {
                nserrGenerate(errp, ACLERRFAIL, ACLERR4320, ACL_Program, 2,
                              XP_GetAdminStr(DBT_GetAttributeDidntSetAttr), attr);
                return LAS_EVAL_FAIL;
            }

            /* Got it */
            *val = attrval;
            return LAS_EVAL_TRUE;
        } else {
            /* No, did it fail to get the attribute */
            if (rv == LAS_EVAL_FAIL || rv == LAS_EVAL_INVALID) {
                nserrGenerate(errp, ACLERRFAIL, ACLERR4330, ACL_Program, 2,
                              XP_GetAdminStr(DBT_GetAttributeDidntGetAttr), attr);
            }
            return rv;
        }
    }

    // If we fall out of the loop, all the getters declined

    if (ereport_can_log(LOG_VERBOSE)) {
        method_name = acl_get_name(ACLMethodHash, method);
        dbtype_name = acl_get_name(ACLDbTypeHash, dbtype);
        ereport(LOG_VERBOSE, "acl: unable to obtain an attribute getter for "
                "[%s] with method [%s] (%d), dbtype [%s] (%d)", 
                attr, method_name, method, dbtype_name, dbtype);
        FREE(method_name);
        FREE(dbtype_name);
    }

    nserrGenerate(errp, ACLERRFAIL, ACLERR4340, ACL_Program, 2,
		  XP_GetAdminStr(DBT_GetAttributeAllGettersDeclined), attr);
    return LAS_EVAL_DECLINE;
}

NSAPI_PUBLIC void
ACL_AttrGetterRegisterInit()
{
    ACLAttrGetterHash = PR_NewHashTable(256,
				     ACLPR_HashCaseString,
				     ACLPR_CompareCaseStrings,
				     PR_CompareValues,
				     &ACLPermAllocOps,
				     NULL);
    NS_ASSERT(ACLAttrGetterHash);
}

/*  The hash table is keyed by attribute name, and contains pointers to the
 *  PRCList headers.  These in turn, circularly link a set of AttrGetter_s
 *  structures.
 */
NSAPI_PUBLIC int
ACL_AttrGetterRegister(NSErr_t *errp, const char *attr, ACLAttrGetterFn_t fn,
                       ACLMethod_t m, ACLDbType_t d, int position, void *arg)
{
    ACLAttrGetter_t	*getter;
    PRHashEntry         **hep;

    if (position != ACL_AT_FRONT  &&  position != ACL_AT_END) {
	return -1;
    }

    ACL_CritEnter();
    
    hep = PR_HashTableRawLookup(ACLAttrGetterHash, ACLPR_HashCaseString(attr), attr);

    /*  Now, allocate the current entry  */
    getter = (ACLAttrGetter_t *)CALLOC(sizeof(ACLAttrGetter_t));
    if (getter == NULL) {
        ACL_CritExit();
        return -1;
    }
    getter->method	= m;
    getter->dbtype	= d;
    getter->fn	= fn;
    getter->arg = arg;

    if (*hep == 0) {	/* New entry */

	PR_INIT_CLIST(&getter->list);
        PR_HashTableAdd(ACLAttrGetterHash, attr, (void *)getter);
    }
    else {

        ACLAttrGetter_t *head = (ACLAttrGetter_t *)((*hep)->value);

        PR_INSERT_BEFORE(&getter->list, &head->list);

        if (position == ACL_AT_FRONT) {

            /* Set new head of list */
            (*hep)->value = (void *)getter;
        }
    }

    ACL_CritExit();
    return 0;
}

NSAPI_PUBLIC int
ACL_AttrGetterFind(NSErr_t *errp, const char *attr,
                   ACLAttrGetterList_t *getters)
{
    *getters = PR_HashTableLookup(ACLAttrGetterHash, attr);
    if (*getters)
	return 0;
    else
        return -1;
}

NSAPI_PUBLIC ACLAttrGetter_t *
ACL_AttrGetterFirst(ACLAttrGetterList_t *getters)
{
    ACLAttrGetter_t * first = 0;

    if (getters && *getters) {

        first = (ACLAttrGetter_t *)(*getters);
    }

    return first;
}

NSAPI_PUBLIC ACLAttrGetter_t *
ACL_AttrGetterNext(ACLAttrGetterList_t *getters, ACLAttrGetter_t *last)
{
    ACLAttrGetter_t *head;
    ACLAttrGetter_t *next = 0;

    if (getters && *getters && last) {

        head = (ACLAttrGetter_t *)(*getters);
        if (head) {

            /* End of list? */
            if (last != (ACLAttrGetter_t *)PR_LIST_TAIL(&head->list)) {

                /* No, get next entry */
                next = (ACLAttrGetter_t *)PR_NEXT_LINK(&last->list);
            }
        }
    }

    return next;
}
