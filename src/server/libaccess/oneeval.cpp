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

/*

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 1996 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

*/

/*
 * Description (acleval.c)
 *
 *	This module provides functions for evaluating Access Control List
 *	(ACL) structures in memory.
 *
 */

#include <string.h>
#include <sys/types.h>
#include <assert.h>

#include <netsite.h>
#include <base/systems.h>
#include <base/crit.h>
#include <base/session.h>
#include <base/util.h>

#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include "symbols.h"
#include <libaccess/aclglobal.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclerror.h>
#include <libaccess/nserror.h>

static int acl_default_result = ACL_RES_DENY;

//
// ACL_CacheEvalInfo - try to put information about DBTYPE, DBNAME and dbhandle into resource
// 
// this needs to be called whenever ACL_GetAttribute or any function calling it is called.
//
void
ACL_CacheEvalInfo(NSErr_t *errp, PList_t resource, PList_t auth_info)
{
    //
    // please note that ACL_ATTR_DATABASE in auth_info holds the database name
    // for the ACL, and ACL_ATTR_DATABASE in resource holds the database handle
    //
    char *dbname;
    if (ACL_AuthInfoGetDbname(auth_info, &dbname) >= 0) {
        ACLDbType_t dbtype;
        void *db;
        if (ACL_DatabaseFind(errp, dbname, &dbtype, &db) != LAS_EVAL_TRUE) {
            nserrGenerate(errp, ACLERRINVAL, ACLERR6000, ACL_Program, 2,
                          XP_GetAdminStr(DBT_notARegisteredDatabase), dbname);
            return;
        }

        PListInitProp(resource, ACL_ATTR_DBTYPE_INDEX, ACL_ATTR_DBTYPE, dbtype, NULL);
        PListInitProp(resource, ACL_ATTR_DBNAME_INDEX, ACL_ATTR_DBNAME, dbname, NULL);
        PListInitProp(resource, ACL_ATTR_DATABASE_INDEX, ACL_ATTR_DATABASE, db, NULL);
    }
}

//
// ACL_FlushEvalInfo - flush the cached database information out of the resource plist
// 
void
ACL_FlushEvalInfo(NSErr_t *errp, PList_t resource)
{
    PListDeleteProp(resource, ACL_ATTR_DBTYPE_INDEX, ACL_ATTR_DBTYPE);
    PListDeleteProp(resource, ACL_ATTR_DBNAME_INDEX, ACL_ATTR_DBNAME);
    PListDeleteProp(resource, ACL_ATTR_DATABASE_INDEX, ACL_ATTR_DATABASE);
}

//
// ACLEvalAce - evaluate an ACE (access control expression) within an ACL
//
// walk the parse tree, calling the LAS functions
//
// params:
//  IN  errp - error stack
//  IN  acleval - acl evaluation context
//  IN  ace - the expression to evaluate
//  OUT cachable - whether the result is cacheable
//  IN  autharray - array of auth_info's for every expression in the ace
//  IN  global_auth - list of auth_info plists, keyed by auth attr ("user", "group")
//
int
ACLEvalAce(NSErr_t *errp, ACLEvalHandle_t *acleval, ACLExprHandle_t *ace, 
            ACLCachable_t *cachable, PList_t autharray[], PList_t global_auth)
{
    ACLCachable_t      local_cachable;
    int                result;
    ACLExprEntry_t    *expr;
    int	               expr_index;
    PList_t            auth_info = NULL, old_auth_info;

    *cachable = ACL_INDEF_CACHABLE;

    // we always start at expression #0
    expr_index = 0;

    // clean out potential leftovers.
    ACL_FlushEvalInfo(errp, acleval->resource);

    while (TRUE) {
        local_cachable    = ACL_NOT_CACHABLE;

        // if expr_index is less than zero, evaluation
        // has finished. -1 is LAS_EVAL_TRUE, -2 is LAS_EVAL_FALSE etc.
        if (expr_index < 0) 
            return (expr_index); // we're done
        expr = &ace->expr_arry[expr_index];

        // 
        // optimization: set dbtype, dbname and dbhandle in the resource plist
        // so that we spare us the expensive lookups in each and every call to ACL_GetAttribute.
        // we do not need to lock here, because the resource plist is only for this
        // thread and ACL evaluation.
        //
        // we need to clear out old values and redo it for every pass, as the expression
        // we process might have different "authenticate" statements for a LAS (or none at all).
        // example:
        //  acl "wrdlbrmpft";
        //  authenticate(user) { database = "db1"; };
        //  authenticate(group) { database = "db2"; };
        //  allow (read, write) user = "joe" or group = "admin";
        //
        // XXX we should restrict ACLs to have only one authenticate statement, as I don't
        // XXX think anyone in the whole wide world is using this feature. at that point,
        // XXX we could pull all of the following magic out of the loop.
        //
        if (autharray) {
            //
            // if the auth_info is the same as last time through - which is the common case
            // for ACLs that involve more than one LAS evaluation
            // (ex: user = "joe" or user = "frank"), we just leave the values as they are.
            //
            // if old_auth_info == auth_info == NULL, there are no properties to remove or add
            // if old_auth_info == auth_info != NULL, leave everything as it is
            // if old_auth_info == NULL && auth_info != NULL, just add the new stuff
            // if old_auth_info != NULL, remove old stuff and add new
            //
            old_auth_info = auth_info;
            auth_info = autharray[expr_index];
            if (old_auth_info != auth_info) {
                if (old_auth_info != NULL) {
                    // if we had something from the previous round, remove it...
                    ACL_FlushEvalInfo(errp, acleval->resource);
                }
                // update with data from the cache
                ACL_CacheEvalInfo(errp, acleval->resource, auth_info);
            }
        }

        // now find the evaluation function for the LAS
	if (!expr->las_eval_func) {
            ACL_CritEnter();    // global lock for ACLEvalAce
	    if (!expr->las_eval_func) {	/* Must check again after locking */
	        ACL_LasFindEval(errp, expr->attr_name, &expr->las_eval_func);
	        if (!expr->las_eval_func) {	/* Couldn't find it */
                    ACL_CritExit();     // global lock for ACLEvalAce
		    return LAS_EVAL_INVALID;
		}
	    }
	    ACL_CritExit();     // global lock for ACLEvalAce
	}

        // then call it.
        result = (*expr->las_eval_func)(
			  errp, 
                          expr->attr_name, 
                          expr->comparator, 
                          expr->attr_pattern, 
                          &local_cachable, 
                          &expr->las_cookie,
			  acleval->subject,
			  acleval->resource,
			  auth_info,
			  global_auth);

        if (local_cachable < *cachable) {
            /* Take the minimum value */
            *cachable = local_cachable;
        }

        //
        // the expression parser generates a true_idx and a false_idx
        // for every expression that indicate whether to return or
        // at which subexpression to continue
        //
        switch (result) {
        case LAS_EVAL_TRUE:
            expr_index = expr->true_idx;
            break;
        case LAS_EVAL_FALSE:
            expr_index = expr->false_idx;
            break;
        default:
            return (result);
        }
    }
}

//
//    ACLEvalBuildContext - build acleval->acllist->cache
//
//    Builds three structures:
//      acleval->acllist->cache->Table
//              A hash table of all access rights referenced by any ACE in any
//              of the ACLs in this list.  Each hash entry then has a list of
//              the relevant ACEs, in the form of indexes to the ACE linked
//              list.
//      acleval->acllist->cache->acelist
//              A linked list of all the ACEs in the proper evaluation order.
//      ace->autharray[expr_index]
//              Contains plists of attributes that need to be authenticated when evaluating
//              an expression.
//
//    This is done only ONCE per ACLList lifetime.
//    For concurrency control, the caller must hold ACL_Crit
//
int
ACLEvalBuildContext(NSErr_t *errp, ACLEvalHandle_t *acleval)
{
    ACLHandle_t        *acl;
    ACLExprHandle_t    *ace;
    int                ace_cnt = -1;
    ACLAceEntry_t      *acelast, *new_ace;
    ACLAceNumEntry_t   *entry, *temp_entry;
    char               **argp;
    ACLListCache_t     *cache;
    ACLWrapper_t       *wrapper;
    PList_t	       curauthplist = NULL, absauthplist = NULL;
    int		       i, rv;
    ACLExprEntry_t     *expr;
    PList_t	       authplist;

    // Allocate the cache context
    cache = (ACLListCache_t *)PERM_CALLOC(sizeof(ACLListCache_t));
    if (cache == NULL) {
	nserrGenerate(errp, ACLERRNOMEM, ACLERR4010, ACL_Program, 0);
	goto error;
    }

    // allocate the access rights hash table
    cache->Table = PR_NewHashTable(0, PR_HashString, PR_CompareStrings, PR_CompareValues, &ACLPermAllocOps, NULL);
    if (cache->Table == NULL) {
	nserrGenerate(errp, ACLERRNOMEM, ACLERR4000, ACL_Program, 1,
	              XP_GetAdminStr(DBT_EvalBuildContextUnableToCreateHash));
	goto error;
    }

    // create list of absoluted auth attributes
    if ((absauthplist = PListNew(NULL)) == NULL) {
        nserrGenerate(errp, ACLERRNOMEM, ACLERR4050, ACL_Program, 1, XP_GetAdminStr(DBT_EvalBuildContextUnableToAllocAuthPlist));
        goto error;
    }

    // loop through all the ACLs in the list
    for (wrapper = acleval->acllist->acl_list_head; wrapper != NULL; wrapper = wrapper->wrap_next) {
	acl = wrapper->acl;
        ace = acl->expr_list_head;

        /* Loop through all the ACEs in this ACL    */
        for (ace = acl->expr_list_head; ace != NULL; ace = ace->expr_next) {

            // allocate a new ace list entry
            new_ace = (ACLAceEntry_t *)PERM_CALLOC(sizeof(ACLAceEntry_t));
            if (new_ace == (ACLAceEntry_t *)NULL) {
		nserrGenerate(errp, ACLERRNOMEM, ACLERR4020, ACL_Program, 1,
		XP_GetAdminStr(DBT_EvalBuildContextUnableToAllocAceEntry));
		goto error;
            }
            new_ace->acep = ace;
            new_ace->next = NULL;

            // now link it into the cache's acelist
            if (cache->acelist == NULL)
                cache->acelist = new_ace;
            else {
                acelast->next = new_ace;
            }
            acelast = new_ace;
            ace_cnt++;

	    switch (ace->expr_type) {
	    case ACL_EXPR_TYPE_ALLOW:     // an "allow(right1, right2, ...) { expr }" expression
	    case ACL_EXPR_TYPE_DENY:      //  a "deny(right1, right2, ...) { expr }" expression

                // Add this ACE to the appropriate entries in the access rights hash table
                // The result is a hash that maps from a "right" to a list of expressions that apply for this right
                //  in this particular ACL list.
                //  (only these expressions need to be evaluated at request time - a request only needs permission
                //   for one specific (i.e. http_get) and one generic (i.e. read) right.)
                for (argp = ace->expr_argv; *argp; argp++) {                            //  for all the rights..
                    entry = (ACLAceNumEntry_t *)PERM_CALLOC(sizeof(ACLAceNumEntry_t));
                    if (entry == (ACLAceNumEntry_t *)NULL) {
		         nserrGenerate(errp, ACLERRNOMEM, ACLERR4030, ACL_Program, 1,
    	                               XP_GetAdminStr(DBT_EvalBuildContextUnableToAllocAceEntry));
			goto error;
                    }
                    // put entry on the chain of ACENumEntry_t's
                    if (cache->chain_head == NULL) 
                        cache->chain_head = cache->chain_tail = entry;
                    else {
                        cache->chain_tail->chain = entry;
                        cache->chain_tail = entry;
                    }
                    // point to ACE by ACE number
                    entry->acenum = ace_cnt;
    
		    temp_entry = (ACLAceNumEntry_t *)PR_HashTableLookup(cache->Table, *argp); 
                    if (temp_entry) {
			// find the end of the rights chain and link it in
                        // (XXX chrisk - why not at the beginning? is order important?)
			while (temp_entry->next)
			    temp_entry = temp_entry->next;
			temp_entry->next = entry;
                    } else
                        // the first ACE for this right...
		        PR_HashTableAdd(cache->Table, *argp, entry);
                }

		// if this ACL does not require authentication, we're done early
		if (!curauthplist)
                    break;

                // else go through all the terms, look if this expression contains an attribute we need
                // to authenticate. If so, put their authplist into ace->autharray.
                for (i = 0; i < ace->expr_term_index; i++) {
                    expr = &ace->expr_arry[i];
                    rv = PListFindValue(curauthplist, expr->attr_name, NULL, &authplist);
                    if (rv <= 0)
                        continue;       // not found, next...

                    // if we found something, authplist now contains a pointer to the property list
                    // of the authenticate statement belonging to this allow/deny expression
                    if (!new_ace->autharray) {
                        new_ace->autharray = (PList_t *)PERM_CALLOC(sizeof(PList_t *) * ace->expr_term_index);
                        if (!new_ace->autharray) {
                            nserrGenerate(errp, ACLERRNOMEM, ACLERR4040, ACL_Program, 1, XP_GetAdminStr(DBT_EvalBuildContextUnableToAllocAuthPointerArray));
                            goto error;
                        }
                    }
                    // set autharray[i] to it.
                    // this way, we can later easily find the authentication properties belonging to a particular
                    // variable.
                    new_ace->autharray[i] = authplist;
                }
		break;

	    case ACL_EXPR_TYPE_AUTH:    // an "authenticate(attr1, attr2, ...) { properties }" expression

                // build current list of authenticated attributes and the associated authentication properties
		if (!curauthplist) {
                    // first ACL with an "authenticate" in this list...
		    curauthplist = PListNew(NULL);
		    if (!curauthplist) {
			nserrGenerate(errp, ACLERRNOMEM, ACLERR4050, ACL_Program, 1, XP_GetAdminStr(DBT_EvalBuildContextUnableToAllocAuthPlist));
			goto error;
		    }
                } else {
                    // we can have multiple "authenticate" statements in an ACL.
                    //  and they can be additive, like:
                    //
                    // authenticate(user) { database = "db1"; }
                    // authenticate(group) { database = "db2"; }
                    // allow(read) user = "joe" or group = "admin"
                    // 
                    // we cannot just use the current curauthplist because we already assigned it
                    //  to new_acl->global_auth so to avoid having to use refcounts we just duplicate the old list.
                    // we DO reset the curauthplist between ACLs (this changed for 6.0 - it was
                    //  broken for 3.x and 4.x)
		    curauthplist = PListDuplicate(curauthplist, NULL, 0);
		    if (!curauthplist) {
			nserrGenerate(errp, ACLERRNOMEM, ACLERR4050, ACL_Program, 1, XP_GetAdminStr(DBT_EvalBuildContextUnableToAllocAuthPlist));
			goto error;
		    }
		}

		// for each attribute, enter the auth properties into curauthplist
                // example:
                //  authenticate (user,group) has "user" and "group"
                for (argp = ace->expr_argv; *argp; argp++) {
		    //  skip any attributes that were absoluted in previously processed ACEs
		    if (PListFindValue(absauthplist, *argp, NULL, NULL) >= 0)
                        continue;

                    // save pointer to the expression property list in curauthplist
                    PListInitProp(curauthplist, NULL, *argp, ace->expr_auth, ace->expr_auth);
                    if (IS_ABSOLUTE(ace->expr_flags))
                        PListInitProp(absauthplist, NULL, *argp, NULL, NULL);
		}
		break;

	    case ACL_EXPR_TYPE_RESPONSE:
                // extract deny_type & deny response out of the ACE & set it for the whole ACL list (?!)
		(void) ACL_ExprGetDenyWith(NULL, ace, &cache->deny_type, &cache->deny_response); 
		break;

	    default:
		NS_ASSERT(0);
	    }

            // new_ace->global_auth contains a list of attributes to authenticate
            //  for this ACE, for instance "user" (== ACL_ATTR_USER)
            //  and pointers to the corresponding "authenticate" expression properties.
            new_ace->global_auth = curauthplist;

        } // for (all ACEs in ACL)

#if 0
        // end of the current ACL: we need to reset the authentication property list
        //  (this was added as of iWS6.0)
        if (curauthplist)
            PListDestroy(curauthplist);
        curauthplist = NULL;
#endif

    } // for (all ACLs in acllist)

    // this was just a temporary. get rid of it.
    if (absauthplist)
	PListDestroy(absauthplist);

    /* This must be done last to avoid a race in initialization */
    acleval->acllist->cache = (void *)cache;

    return 0;

error:
    if (absauthplist)
	PListDestroy(absauthplist);
    if (cache)
        ACL_EvalDestroyContext(cache);
    acleval->acllist->cache = NULL;
    return ACL_RES_ERROR;
}

//
//    ACLEvalDestroyContext - destroy ACL evaluation cache
//
int
ACL_EvalDestroyContext(ACLListCache_t *cache)
{
    ACLAceEntry_t	*cur_ace, *next_ace;
    ACLAceNumEntry_t    *cur_num_p, *next_num_p;
    ACLExprHandle_t	*acep;

    if (!cache)
        return 0;

    PR_HashTableDestroy(cache->Table);
    cache->Table = NULL;

    cur_ace = cache->acelist;
    cache->acelist = NULL;
    while (cur_ace) {
	if (cur_ace->autharray)
	    PERM_FREE(cur_ace->autharray);
	if ((cur_ace->global_auth) && (cur_ace->acep->expr_type == ACL_EXPR_TYPE_AUTH))
            // XXX chrisk - shouldn't we get rid of the global_auth even for non-auth expressions?
	    PListDestroy(cur_ace->global_auth);
        next_ace = cur_ace->next;
        acep = cur_ace->acep;    /* The ACE structure itself */
        PERM_FREE(cur_ace);
        cur_ace = next_ace;
    }

    cur_num_p = cache->chain_head;
    cache->chain_head = NULL;
    while (cur_num_p) {
        next_num_p = cur_num_p->chain;
        PERM_FREE(cur_num_p);
        cur_num_p = next_num_p;
    }

    PERM_FREE(cache);

    return 0;
}

//
//    ACL_InvalidateSubjectPList
//
//    Given a new authentication plist, enumerate the plist and for each
//    key in the plist, search for the matching key in the subject plist
//    and delete any matches.  E.g. "user", "group".
//
void
ACL_InvalidateSubjectPList(char *attr, const void *value, void *user_data)
{
    PList_t subject = (PList_t)user_data;

    PListDeleteProp(subject, 0, attr);
    return;
}

//
// ACL_SetDefaultResult - sets the default result for the evaluation context
//
// params:
//  IN  errp    - error stack
//  IN  acleval - ACL evaluation context
//  IN  result  - default result
// return values:
//   0 - success
//  -1 - failure
//
NSAPI_PUBLIC int
ACL_SetDefaultResult(NSErr_t *errp, ACLEvalHandle_t *acleval, int result)
{
    int rv;

    switch(result) {
    case ACL_RES_ALLOW:
    case ACL_RES_DENY:
    case ACL_RES_FAIL:
    case ACL_RES_INVALID:
	acleval->default_result = result;
	rv = 0;
	break;
    default:
	rv = -1;
    }

    return rv;
}

//
// ACL_GetDefaultResult - get default result out of ACL evaluation context
//
// params:
//  IN  acleval - ACL evaluation context
// return values:
//  ACL_RES_ALLOW, ACL_RES_DENY, ACL_RES_FAIL, ACL_RES_INVALID
//
NSAPI_PUBLIC int
ACL_GetDefaultResult(ACLEvalHandle_t *acleval)
{
    return acleval->default_result;
}

//
// ACL_INTEvalTestRights - evaluate ACL list against requested rights
//
// This is the core routine for ACL evaluation. 
// All the interesting stuff happens here.
// However, no one except the original author understands how it's done.
//
//    INPUT
//    *errp         The usual error context stack
//    *acleval      ACL evaluation context containing a list of ACLs
//    **rights      An array of strings listing the requested rights
//    **map_generic An array of strings listing the specific rights
//                  that map from the generic rights.
//    OUTPUT
//    **deny_type   bong file type passed on the way back out  
//    **deny_response bong file pathname passed on the way back out  
//    **acl_tag	    Name of the ACL that denies access
//    *expr_num     ACE number within the denying ACL
//    *cachable	    Is the result cachable?
//
static int
ACL_INTEvalTestRights(
    NSErr_t          *errp,
    ACLEvalHandle_t  *acleval,
    char    **rights,
    char    **map_generic,
    char    **deny_type,
    char    **deny_response,
    char    **acl_tag,
    int     *expr_num,
    ACLCachable_t *cachable)
{
    struct     rights_ent {
        char right[ACL_MAX_RIGHT_LEN];/* lowercase-ed rights string    */
        int result;                   /* Interim result value          */
        int absolute;                 /* ACE with absolute keyword     */
	int count;		      /* # specific + generic rights   */
        ACLAceNumEntry_t *acelist[ACL_MAX_GENERIC+1];    
				      /* List of relevant ACEs         */
    };
    struct rights_ent *rarray_p;
    struct    rights_ent rights_arry[ACL_MAX_TEST_RIGHTS];
    ACLAceNumEntry_t *alllist;  /* List of ACEs for "all" rights */
    ACLAceEntry_t *cur_ace;
    ACLListCache_t *cache;
    int rights_cnt = 0;
    int prev_acenum, cur_acenum;
    int i, j, right_num, delta;
    ACLCachable_t ace_cachable;
    int result;
    int absolute = 0;
    int skipflag;
    int g_num;    /* index into the generic rights array.  */
    char **g_rights;
    PList_t global_auth = NULL;
    int allow_error = 0;
    int allow_absolute = 0;
    char *allow_tag = NULL;
    int allow_num = 0;
    int default_result = ACL_GetDefaultResult(acleval);

    *acl_tag  = NULL;
    *expr_num = 0;
    *cachable = ACL_INDEF_CACHABLE;

    // no ACLs? that's easy: we're done.
    if (acleval->acllist == NULL ||
        acleval->acllist == ACL_LIST_NO_ACLS)
        return ACL_RES_ALLOW;

    // does the ACL list have a right index already?
    if (acleval->acllist->cache == NULL) {
        ACL_CritEnter();        // --> ACLList Cache lock
        if (acleval->acllist->cache == NULL) {	/* Check again */
            // no. build one.
            if (ACLEvalBuildContext(errp, acleval) == ACL_RES_ERROR) {
		nserrGenerate(errp, ACLERRINTERNAL, ACLERR4110, ACL_Program,
		              1, XP_GetAdminStr(DBT_EvalTestRightsEvalBuildContextFailed));
                ACL_CritExit(); // --> ACLList Cache lock
                return ACL_RES_ERROR;
            }
	}
	ACL_CritExit(); // --> ACLList Cache lock
    }

    // so now we have the cached stuff for the acllist in the evaluation context
    cache = (ACLListCache_t *)acleval->acllist->cache;

    // set the bong information
    *deny_response = cache->deny_response;
    *deny_type = cache->deny_type;

    /*     For the list of rights requested, get back the list of relevant
     *     ACEs. If we want to alter the precedence of allow/deny, this would be a good 
     *     place to do it.
     */

    for (; *rights != NULL; rights++) {
        rarray_p = &rights_arry[rights_cnt];

	/* Initialize the rights array entry */
        strcpy(&rarray_p->right[0], *rights);
        makelower(&rarray_p->right[0]);
        rarray_p->result    = default_result;
        rarray_p->absolute  = 0;

	// Locate the list of ACEs that apply to the right
        // The specific right is always first on the list of ACE lists
        // use Const function to avoid race
	rarray_p->acelist[0] = (ACLAceNumEntry_t *)PL_HashTableLookupConst(cache->Table, rarray_p->right);
        rarray_p->count    = 1;

        /* See if the requested right also maps back to a generic right and
         * if so, locate the acelist for it as well.
         */
        if (map_generic) {
            for (g_rights=map_generic, g_num=0; *g_rights; g_rights++, g_num++) {
                if (strstr(*g_rights, rarray_p->right)) {
		    // found it. add it to our list of ACE lists
		    rarray_p->acelist[rarray_p->count++] = 
		      (ACLAceNumEntry_t *)PL_HashTableLookupConst(cache->Table, (char *)generic_rights[g_num]);
		    NS_ASSERT (rarray_p->count < ACL_MAX_GENERIC);
		}
            }
        }
        rights_cnt++;
	NS_ASSERT (rights_cnt < ACL_MAX_TEST_RIGHTS);
    }

    /*    Special case - look for an entry that applies to "all" rights     */
    alllist = (ACLAceNumEntry_t *)PL_HashTableLookupConst(cache->Table, "all");

    /*    Ok, we've now got a list of relevant ACEs.  Now evaluate things.  */
    prev_acenum    = -1;
    cur_ace        = cache->acelist;

    /* Loop through the relevant ACEs for the requested rights    */
    while (TRUE) {
        // ACL borders don't really apply anymore here
        // we just evaluate all the ACEs that apply for the requested rights
        // we have to maintain order, so we need to go through the list (every time)
        // to find the lowest one

        cur_acenum = 10000;            /* Pick a really high num so we lose */
        for (i=0; i < rights_cnt; i++) {
            rarray_p = &rights_arry[i];
	    if (rarray_p->absolute)
                // if this flag is set, it means an earlier evaluation for an absolute ACE with that right
                // came back successfully. So this right is decided upon already.
                // Don't check for it anymore.
                continue;
	    for (j=0; j < rarray_p->count; j++) {
                if  ((rarray_p->acelist[j] != NULL) && (rarray_p->acelist[j]->acenum < cur_acenum)) {
                    cur_acenum = rarray_p->acelist[j]->acenum;
		}
	    }
        }

        // Special case - look for the "all" rights ace list and see if its the lowest of all.
        if (alllist && (alllist->acenum < cur_acenum))
            cur_acenum = alllist->acenum;

        // If no new ACEs that apply for our rights were found then we're done
        if (cur_acenum == 10000) 
            break;

        // Locate that ACE and evaluate it.  We have to step through the linked list of ACEs to find it.
        if (prev_acenum == -1)
            delta = cur_acenum;
        else
            delta = cur_acenum - prev_acenum;
        for (i=0; i<delta; i++)
            cur_ace = cur_ace->next;

        // cur_ace is now the ACE we're going to evaluate

        //
        // if we're not the first time through here (i.e. we already called ACLEvalAce),
        //   and we have a cur_ace->global_auth list that is different from last time,
        //   remove every property in cur_ace->global_auth from the subject plist.
        //   Typically, that would be anything that is authenticated using the last set
        //   of "authenticate" statements ("user" and/or "group").
        // That means we discard any previous authentication results for every new
        //  "authenticate" statement.
        //
        if (global_auth && global_auth != cur_ace->global_auth) {
	    /* We must enumerate the auth_info plist and remove entries for
	     * each attribute from the subject property list.
	     */
	     PListEnumerate(cur_ace->global_auth, ACL_InvalidateSubjectPList, acleval->subject);
	}
        global_auth = cur_ace->global_auth;

        // finally, evaluate all the expressions in the ACE
        // XXX what if the expression is ACL_EXPR_TYPE_AUTH?
        //     does it have expression terms then?
        result = ACLEvalAce(errp, acleval, cur_ace->acep, &ace_cachable,
			cur_ace->autharray, cur_ace->global_auth);

        if (ace_cachable < *cachable) {
            // Take the minimum value
            *cachable = ace_cachable;
        }

        // Under certain circumstances, no matter what happens later,
        // the current result is not gonna change.
        if ((result != LAS_EVAL_TRUE) && (result != LAS_EVAL_FALSE)) {

            // result is neither TRUE or FALSE, so it must be an error

	    if (cur_ace->acep->expr_type == ACL_EXPR_TYPE_ALLOW) {
		// If the error is on an allow statement, continue processing
		// and see if a subsequent allow works.  If not, remember the
		// error and return it.
		if (!allow_error) {
		    allow_error = EvalToRes(result);
                    allow_tag = cur_ace->acep->acl_tag;
	            allow_num = cur_ace->acep->expr_number;
		}
                if (IS_ABSOLUTE(cur_ace->acep->expr_flags)) {
		    allow_absolute = 1;
		}
	    } else {
		if (allow_error) {
	            *acl_tag = allow_tag;
	            *expr_num = allow_num;
	            return (allow_error);
		} else {
                    *acl_tag = cur_ace->acep->acl_tag;
        	    *expr_num = cur_ace->acep->expr_number;
                    return (EvalToRes(result));
		}
	    }
	}

        /* Now apply the result to the rights array.  Look to see which rights'
         * acelist include the current one, or if the current one is on the
         * "all" rights ace list.
         */
        for (right_num=0; right_num<rights_cnt; right_num++) {
            rarray_p = &rights_arry[right_num];

            /*    Have we fixated on a prior result?    */
            if (rarray_p->absolute) 
                continue;

            skipflag = 1;

            // Did this ace apply to this right?
	    for (i=0; i<rarray_p->count; i++) {
                if ((rarray_p->acelist[i]) && (rarray_p->acelist[i]->acenum == cur_acenum)) {
                    rarray_p->acelist[i] = rarray_p->acelist[i]->next;
                    skipflag = 0;
		}
            }

            /* This ace was on the "all" rights queue */
            if ((alllist) && (alllist->acenum == cur_acenum)) {
                skipflag = 0;
            }

            if (skipflag)
                continue;    /* doesn't apply to this right */

            if (IS_ABSOLUTE(cur_ace->acep->expr_flags) && (result == LAS_EVAL_TRUE)) {
                rarray_p->absolute     = 1;
                absolute    = 1;
            } else 
                absolute    = 0;

            switch (cur_ace->acep->expr_type) {
            case ACL_EXPR_TYPE_ALLOW:
                if (result == LAS_EVAL_TRUE) {
                    rarray_p->result = ACL_RES_ALLOW;
		    if (!allow_absolute) {
			/* A previous ALLOW error was superceded */
		        allow_error = 0;
		    }
		} else if (!*acl_tag) {
	            *acl_tag = cur_ace->acep->acl_tag;
	            *expr_num = cur_ace->acep->expr_number;
		}
                break;
            case ACL_EXPR_TYPE_DENY:
                if (result == LAS_EVAL_TRUE) {
	            *acl_tag = cur_ace->acep->acl_tag;
	            *expr_num = cur_ace->acep->expr_number;
                    if (absolute) {
			if (allow_error) {
	                    *acl_tag = allow_tag;
	                    *expr_num = allow_num;
			    return (allow_error);
			}
                        return (ACL_RES_DENY);
		    }
                    rarray_p->result = ACL_RES_DENY;
                }
                break;
            default:
                /* a non-authorization ACE, just ignore    */
                break;
            }
        }

        /* This ace was on the "all" rights queue */
        if ((alllist) && (alllist->acenum == cur_acenum))
            alllist = alllist->next;

        /* If this is an absolute, check to see if all the rights
         * have already been fixed by this or previous absolute
         * statements.  If so, we can compute the response without
         * evaluating any more of the ACL list.
         */
        if (absolute) {
            for (i=0; i<rights_cnt; i++) {
                /* Non absolute right, so skip this section */
                if (rights_arry[i].absolute == 0)
                    break;
                /* This shouldn't be possible, but check anyway.
                 * Any absolute non-allow result should already 
                 * have been returned earlier.
                 */
                if (rights_arry[i].result != ACL_RES_ALLOW) {
		    char result_str[16];
		    sprintf(result_str, "%d", rights_arry[i].result);
		    nserrGenerate(errp, ACLERRINTERNAL, ACLERR4100, ACL_Program, 3,
                        XP_GetAdminStr(DBT_EvalTestRightsInterimAbsoluteNonAllowValue),
                        rights[i], result_str);
                    break;
                }
                if (i == (rights_cnt - 1))
                    return ACL_RES_ALLOW;
            }
        }
        prev_acenum = cur_acenum;
    }

    // all the evaluations on all the rights must have resulted in ACL_RES_ALLOW
    // to return an ACL_RES_ALLOW
    for (right_num=0; right_num<rights_cnt; right_num++) {
        if (rights_arry[right_num].result != ACL_RES_ALLOW) {
            if (allow_error) {
                // we have an unsuperseded error on an ALLOW ACE
                // so return that
                *acl_tag = allow_tag;
                *expr_num = allow_num;
	        return (allow_error);
	    } else 
                // otherwise, we return what we've memorized
                return (rights_arry[right_num].result);
	}
    }
    return (ACL_RES_ALLOW);
}

/*  ACL_CachableAclList
 *  Returns 1 if the ACL list will always evaluate to ALLOW for http_get.
 */
NSAPI_PUBLIC int
ACL_CachableAclList(ACLListHandle_t *acllist)
{
    static char *rights[] = { "http_get", NULL };
    return ACL_AlwaysAllows(acllist, rights, http_generic);
}
    
//
// ACL_AlwaysAllows - indicate whether the given rights are always allowed
//
// Evaluate the ACL list.  Returns 1 if the ACL list will always evaluate to
// ALLOW for the given rights.
//
// return values:
//   0 - one or more ACEs could potentially deny access
//   1 - the given rights are always allowed
//
NSAPI_PUBLIC int
ACL_AlwaysAllows(ACLListHandle_t *acllist, char **rights, char **map_generic)
{
    ACLEvalHandle_t *acleval;
    char *bong;
    char *bong_type;
    char *acl_tag;
    int  expr_num;
    int  rv;
    ACLCachable_t cachable = ACL_INDEF_CACHABLE;

    if (!acllist  ||  acllist == ACL_LIST_NO_ACLS) {
        return 1;
    }
    acleval = ACL_EvalNew(NULL, NULL);
    ACL_EvalSetACL(NULL, acleval, acllist);
    rv = ACL_INTEvalTestRights(NULL, acleval, rights, map_generic, 
                               &bong_type, &bong, &acl_tag, &expr_num, &cachable);

    ACL_EvalDestroyNoDecrement(NULL, NULL, acleval);
    if (rv == ACL_RES_ALLOW  &&  cachable == ACL_INDEF_CACHABLE) {
        return 1;
    }

    return 0;
}

//
// ACL_CanDeny - indicate whether an ACL list can deny any of the given rights
//
// Walk the list of ACLs, looking for ACEs that can deny the given right(s).
//
// return values:
//   0 - the given right(s) are never denied
//   1 - one or more ACEs could potentially deny access
//
NSAPI_PUBLIC int
ACL_CanDeny(ACLListHandle_t *acllist, char **rights, char **map_generic)
{
    if (!acllist || acllist == ACL_LIST_NO_ACLS)
        return 0;

    char required_rights[ACL_MAX_TEST_RIGHTS][ACL_MAX_RIGHT_LEN];
    int n_rights = 0;

    // Canonicalize the rights the caller requested
    while (*rights) {
        if (n_rights >= ACL_MAX_TEST_RIGHTS)
            return 1;

        util_strlcpy(required_rights[n_rights], *rights, ACL_MAX_RIGHT_LEN);
        makelower(required_rights[n_rights]);

        n_rights++;
        rights++;
    }

    // Map caller's specific rights to generic rights
    if (map_generic) {
        int n_specific_rights = n_rights;

        for (int r_num = 0; r_num < n_specific_rights; r_num++) {
            char **g_rights;
            int g_num;

            for (g_rights = map_generic, g_num = 0; *g_rights; g_rights++, g_num++) {
                if (strstr(*g_rights, required_rights[r_num])) {
                    if (n_rights >= ACL_MAX_TEST_RIGHTS)
                        return 1;

                    strcpy(required_rights[n_rights], generic_rights[g_num]);
                    n_rights++;
                }
            }
        }
    }

    // For all the ACLs in the ACL list...
    for (ACLWrapper_t *wrapper = acllist->acl_list_head; wrapper; wrapper = wrapper->wrap_next) {
        ACLHandle_t *acl = wrapper->acl;
        ACLExprHandle_t *ace = acl->expr_list_head;

        // For all the ACEs in the ACL...
        for (ace = acl->expr_list_head; ace; ace = ace->expr_next) {
            // We only care about "deny" ACEs
            if (ace->expr_type != ACL_EXPR_TYPE_DENY)
                continue;

            // For all the rights in the ACE...
            for (char **argp = ace->expr_argv; *argp; argp++) {
                if (!strcmp(*argp, "all"))
                    return 1; // ACE denies all rights

                for (int r_num = 0; r_num < n_rights; r_num++) {
                    if (!strcmp(*argp, required_rights[r_num]))
                        return 1; // ACE denies a required right
                }
            }
        }
    }

    return 0;
}

NSAPI_PUBLIC int
ACL_EvalTestRights(
    NSErr_t          *errp,
    ACLEvalHandle_t  *acleval,
    char    **rights,
    char    **map_generic,
    char    **deny_type,
    char    **deny_response,
    char    **acl_tag,
    int     *expr_num)
{
    ACLCachable_t cachable;

    return (ACL_INTEvalTestRights(errp, acleval, rights, map_generic, 
                                  deny_type, deny_response, 
                                  acl_tag, expr_num, &cachable));
}

NSAPI_PUBLIC ACLEvalHandle_t *
ACL_EvalNew(NSErr_t *errp, pool_handle_t *pool)
{
    ACLEvalHandle_t *rv = ((ACLEvalHandle_t *)pool_calloc(pool, sizeof(ACLEvalHandle_t), 1));
    rv->default_result = ACL_RES_DENY;
    return rv;
}

//
// ACL_EvalDestroy - destroy ACL evaluation context
//
// for historical reasons, and because the ACL API is public, we also
// call ACL_ListDecrement() on the eval context's acl list in here
// There's another function, ACL_EvalDestroyNoDecrement(), that doesn't do
// this.
//
NSAPI_PUBLIC void
ACL_EvalDestroy(NSErr_t *errp, pool_handle_t *pool, ACLEvalHandle_t *acleval)
{
    if (!acleval->acllist  ||  acleval->acllist == ACL_LIST_NO_ACLS)
	return;

    // safely decrement list's refcount or destroy it
    ACL_ListDecrement(errp, acleval->acllist);

    pool_free(pool, acleval);
}

NSAPI_PUBLIC void
ACL_EvalDestroyNoDecrement(NSErr_t *errp, pool_handle_t *pool, ACLEvalHandle_t *acleval)
{
    if (!acleval->acllist  ||  acleval->acllist == ACL_LIST_NO_ACLS)
	return;

    pool_free(pool, acleval);
}

NSAPI_PUBLIC int
ACL_EvalSetACL(NSErr_t *errp, ACLEvalHandle_t *acleval, ACLListHandle_t *acllist)
{
    NS_ASSERT(ACL_AssertAcllist(acllist));

    // ref_count of the acllist is not modified.
    // the evaluation context is not a reference of its own because it's
    // transient.
    acleval->acllist = acllist;
    return(0);
}

NSAPI_PUBLIC int
ACL_EvalSetSubject(NSErr_t *errp, ACLEvalHandle_t *acleval, PList_t subject)
{
    acleval->subject = subject;
    return 0;
}

NSAPI_PUBLIC PList_t
ACL_EvalGetSubject(NSErr_t *errp, ACLEvalHandle_t *acleval)
{
    return (acleval->subject);
}

NSAPI_PUBLIC int
ACL_EvalSetResource(NSErr_t *errp, ACLEvalHandle_t *acleval, PList_t resource)
{
    acleval->resource = resource;
    return 0;
}

NSAPI_PUBLIC PList_t
ACL_EvalGetResource(NSErr_t *errp, ACLEvalHandle_t *acleval)
{
    return (acleval->resource);
}
