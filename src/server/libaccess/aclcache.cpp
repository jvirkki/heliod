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

#include <base/nsassert.h>
#include <base/ereport.h>
#include "plhash.h"
#include <libaccess/acl.h>              // generic ACL definitions
#include <libaccess/aclproto.h>         // internal prototypes
#include <libaccess/aclglobal.h>        // global data
#include <libaccess/aclcache.h>
#include "aclpriv.h"                    // internal data structure definitions
#include <libaccess/dbtlibaccess.h>     // strings

struct uricachekey {
    const char *vsid;
    const char *uri;
};

static PLHashNumber
hash_uricachekey(const void *k)
{
    PLHashNumber h;
    const PRUint8 *s;
    struct uricachekey *key = (struct uricachekey *)k;

    h = 0;
    for (s = (const PRUint8*)key->vsid; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ *s;
    for (s = (const PRUint8*)key->uri; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ *s;
    return h;
}

static int
compare_uricachekey(const void *k1, const void *k2)
{
    struct uricachekey *key1 = (struct uricachekey *)k1;
    struct uricachekey *key2 = (struct uricachekey *)k2;

    return (strcmp(key1->vsid, key2->vsid) == 0) && (strcmp(key1->uri, key2->uri) == 0);
}

static struct uricachekey *
copy_uricachekey(struct uricachekey *key, pool_handle_t *pool)
{
    struct uricachekey *newkey;

    if ((newkey = (struct uricachekey *)pool_malloc(pool, sizeof(struct uricachekey))) == NULL)
        return NULL;
    if ((newkey->vsid = (const char *)pool_strdup(pool, key->vsid)) == NULL) {
        pool_free(pool, (void *)newkey);
        return NULL;
    }
    if ((newkey->uri = (const char *)pool_strdup(pool, key->uri)) == NULL) {
        pool_free(pool, (void *)newkey->vsid);
        pool_free(pool, (void *)newkey);
        return NULL;
    }
    return newkey;
}


/*	hash_listcachekey
 *	Given an ACL List address, computes a randomized hash value of the
 *	ACL structure pointer addresses by simply adding them up.  Returns
 *	the resultant hash value.
 */
static PRHashNumber
hash_listcachekey(const void *Iacllist)
{
    PRHashNumber hash=0;
    ACLWrapper_t *wrap;
    ACLListHandle_t *acllist=(ACLListHandle_t *)Iacllist;

    for (wrap=acllist->acl_list_head; wrap; wrap=wrap->wrap_next) {
	hash += (PRHashNumber)(size_t)wrap->acl;
    }

    return (hash);
}

/*	compare_listcachekey
 *	Given two acl lists, compares the addresses of the acl pointers within
 *	them to see if theyre identical.  Returns 1 if equal, 0 otherwise.
 */
static int
compare_listcachekey(const void *Iacllist1, const void *Iacllist2)
{
    ACLWrapper_t *wrap1, *wrap2;
    ACLListHandle_t *acllist1=(ACLListHandle_t *)Iacllist1;
    ACLListHandle_t *acllist2=(ACLListHandle_t *)Iacllist2;

    if (acllist1->acl_count != acllist2->acl_count) 
        return 0;

    wrap1 = acllist1->acl_list_head;
    wrap2 = acllist2->acl_list_head;

    while ((wrap1 != NULL) && (wrap2 != NULL)) {
	if (wrap1->acl != wrap2->acl) 
            return 0;
	wrap1 = wrap1->wrap_next;
	wrap2 = wrap2->wrap_next;
    }

    if ((wrap1 != NULL) || (wrap2 != NULL)) 
        return 0;
    else 
        return 1;
}

/*  compare_listcachevalue
 *  Returns 1 if equal, 0 otherwise
 */
static int
compare_listcachevalue(const void *acllist1, const void *acllist2)
{
    return (acllist1 == acllist2);
}

static PRIntn
deletelists(PRHashEntry *he, PRIntn i, void *arg)
{
    NSErr_t *errp = 0;
    ACLListHandle_t *acllist;

    NS_ASSERT(he);
    NS_ASSERT(he->value);
    acllist = (ACLListHandle_t *)he->value;
    // we delete the ACL cache when the configuration is going away, and at
    // that time, no request should be active anymore.
    // therefore, no acllist should be in use. however, they should all have
    // one reference from the listhash itself.
    NS_ASSERT(acllist->ref_count == 1);
    NS_ASSERT(ACL_CritHeld());

    ACL_ListDecrement(errp, acllist);
    return 0;
}


ACLCache::ACLCache()
{
    this->hash = PR_NewHashTable(200,
                                     hash_uricachekey,
                                     compare_uricachekey,
                                     PR_CompareValues,
                                     &ACLPermAllocOps,
                                     NULL);
    this->gethash = PR_NewHashTable(200,
                                     hash_uricachekey,
                                     compare_uricachekey,
                                     PR_CompareValues,
                                     &ACLPermAllocOps,
                                     NULL);
    this->listhash = PR_NewHashTable(200,
                                     hash_listcachekey,
                                     compare_listcachekey,
                                     compare_listcachevalue,
                                     &ACLPermAllocOps,
                                     NULL);
    this->pool = pool_create();
}

ACLCache::~ACLCache()
{
    if (this->hash)
	PR_HashTableDestroy(this->hash);
    if (this->gethash)
	PR_HashTableDestroy(this->gethash);

    // when we destroy the cache, we must destroy all the acllists.
    // the listhash contains all acllists that are in use for this ACL cache
    // (and all of them exactly once).
    if (this->listhash) {
        ACL_CritEnter();
        PR_HashTableEnumerateEntries(this->listhash, deletelists, NULL);
        ACL_CritExit();
	PR_HashTableDestroy(this->listhash);
    }
    pool_destroy((void **)this->pool);
}

//
//  ACLCache::Check
//
//  INPUT
//      uri, vsid  the request URI and the virtual server ID
//	acllistp   pointer to an acllist placeholder.  E.g. &rq->acllist
//  OUTPUT
//      if there's an entry in the cache for this VSID/URI, the cached acllist
//       is put into acllistp and 1 is returned.
//      otherwise, 0 is returned (and *acllistp is unchanged).
//	The reference count on the ACL List is INCREMENTED, and will be
//      decremented when ACL_EvalDestroy or ACL_ListDecrement is called. 
//
//  the ACLCrit lock must be held.
//
int
ACLCache::CheckH(aclcachetype which, const char *vsid, char *uri, ACLListHandle_t **acllistp)
{
    struct uricachekey key;

    NS_ASSERT(uri && this->hash && this->gethash);

    key.vsid = vsid;
    key.uri = uri;

    ACL_CritEnter();
    NS_ASSERT(ACL_CritHeld());
    /*  ACL cache:  If the ACL List is already in the cache, there's no need
     *  to go through the pathcheck directives.
     *  NULL	means that the URI hasn't been accessed before.
     *  ACL_LIST_NO_ACLS	
     *		means that the URI has no ACLs.
     *  Anything else is a pointer to the acllist.
     */
    *acllistp = (ACLListHandle_t *)PR_HashTableLookup((which == ACL_URI_HASH) ? this->hash : this->gethash, &key);
    if (*acllistp == NULL) {
        ACL_CritExit();
        return 0;
    }
    if (*acllistp != ACL_LIST_NO_ACLS) {
        // have a valid ACL list
        NS_ASSERT((*acllistp)->ref_count >= 0);
        // increment refcount
        (*acllistp)->ref_count++;
    }
    NS_ASSERT(ACL_AssertAcllist(*acllistp));
    ACL_CritExit();
    return 1;		/* Normal path */
}

//
//  ACLCache::Enter
//
//  INPUT
//	acllist or 0 if there were no ACLs that applied.
//      the acllist we pass must be one that is newly created (not one coming out
//      of the ACL cache) because we might need to destroy it.
//  OUTPUT
//      The acllist address may be changed if it matches an existing one.
//
// the ACLCrit lock must be held.
//
void
ACLCache::EnterH(aclcachetype which, const char *vsid, char *uri, ACLListHandle_t **acllistp)
{
    ACLListHandle_t *tmpacllist;
    NSErr_t *errp = 0;
    PRHashTable *urihash;
    struct uricachekey key, *newkey;

    NS_ASSERT(uri);

    key.vsid = vsid;
    key.uri = uri;

    ACL_CritEnter();
    NS_ASSERT(ACL_CritHeld());

    /* Get the pointer to the hash table after acquiring the lock */
    urihash = (which == ACL_URI_HASH) ? this->hash : this->gethash;

    // Check again (now that we're in the critical section) to see if
    // someone else created and entered another ACL List for this URI
    // since we decided to Enter this list.
    // If so, discard the list that we made and replace it with the one just found.
    tmpacllist = (ACLListHandle_t *)PR_HashTableLookup(urihash, &key);
    if (tmpacllist != NULL) {
        // we found something for the same VS/URI
        if (tmpacllist != ACL_LIST_NO_ACLS)
            tmpacllist->ref_count++;	// we're going to use it
        ACL_CritExit();
        // destroy the acllist we tried to enter.
	if (*acllistp  &&  *acllistp != ACL_LIST_NO_ACLS) {
	    ACL_ListDecrement(errp, *acllistp);
	}
        // and return the one we found in the cache.
	*acllistp = tmpacllist;
	NS_ASSERT(ACL_AssertAcllist(*acllistp));
	return; // we're done.
    }

    /*  Didn't find another list, so put ours in.  */
    /*  Look for a matching ACL List and use it if we find one.  */
    if (*acllistp)  {
	NS_ASSERT(*acllistp != ACL_LIST_NO_ACLS);
        NS_ASSERT(ACL_AssertAcllist(*acllistp));

        // make sure we don't create a lot of the same lists.
        // this might happen, for example, when a whole directory with
        // a couple of hundred files in it is protected by a single ACL.
        // Every URI in that directory will have the same acllist.
        //
        // we can try to find an existing, equivalent acllist (that contains
        // the same acls) and just return that.
        // (once ACLlists are created and put into the ACL cache, they are r/o)
        //
        // this will also spare us the effort of having to create the
        // evaluation cache info for lots of the same acllists.
        //

        // try to find an existing acllist with the same acls in it
        tmpacllist = (ACLListHandle_t *)PR_HashTableLookup(this->listhash, *acllistp);
        if (tmpacllist) {
            NS_ASSERT(*acllistp  &&  *acllistp != ACL_LIST_NO_ACLS);
            if (tmpacllist != *acllistp) {
                // if there's one, and it's not the one we have
                // destroy the one we have and use the one we already had
                ACL_ListDecrement(errp, *acllistp);
                *acllistp = tmpacllist;
            }
            tmpacllist->ref_count++;	/* we're gonna use it */
        } else {
            // if we did not find one, we'll just put this list
            // into the listhash
            (*acllistp)->ref_count++;   // should set it to 2
            PR_HashTableAdd(this->listhash, *acllistp, *acllistp);
        }
    } else {
	*acllistp = ACL_LIST_NO_ACLS;
    }

    // at this point, *acllistp is either ACL_LIST_NO_ACLS or points
    // to a list with the correct ref_count.
    if ((newkey = copy_uricachekey(&key, this->pool)) != NULL)
        PR_HashTableAdd(urihash, newkey, (void *)*acllistp);

    NS_ASSERT(ACL_AssertAcllist(*acllistp));
    ACL_CritExit();
    return;
}
