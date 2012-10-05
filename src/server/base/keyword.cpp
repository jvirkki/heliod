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
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

/*
 * ABSTRACT:
 *
 *      This module provides services for mapping string-valued keywords to
 *      unique integer indices.
 */

#include "keyword_pvt.h"
#include "support/prime.h"
#include "base/util.h"

PR_IMPLEMENT(NSKWSpace *)
NSKW_CreateNamespace(int resvcnt)
{
    NSKWSpace *ns;
    int len;

    ns = (NSKWSpace *)pool_malloc(NULL, sizeof(NSKWSpace));

    /* fake loop */
    while (ns) {
        if (resvcnt < 0) {
            resvcnt = 0;
            ns->itsize = NSKW_DEFAULT_ITSIZE;
        }
        else {
            /*
             * Make index table a little bigger than the initial
             * number of indices.
             */
            ns->itsize = resvcnt + (resvcnt >> 2);
        }

        ns->nextkwi = resvcnt;

        /* Try to make the hash table big enough to avoid collisions */
        ns->htsize = (ns->itsize * 2) + 1;

        ns->itable = (NSKWEntry **)pool_calloc(NULL, ns->itsize,
                                               sizeof(NSKWEntry *));
        ns->htable = (NSKWEntry **)pool_calloc(NULL, ns->htsize,
                                               sizeof(NSKWEntry *));
        if (ns->itable && ns->htable) {
            break;
        }

        /* Failed */
        NSKW_DestroyNamespace(ns);
        ns = NULL;
        break;
    }

    return ns;
}

PR_IMPLEMENT(int)
NSKW_DefineKeyword(const char * const keyword, int index, NSKWSpace *ns)
{
    NSKWEntry *kw;
    int bucket;
    unsigned long hash;

    kw = (NSKWEntry *)pool_malloc(NULL, sizeof(NSKWEntry));
    if (!kw) {
        return -1;
    }

    kw->next = NULL;
    kw->keyword = keyword;

    /*
     * If no specific index is requested,
     * or if the requested index is taken ...
     */
    if ((index <= 0) || (ns->itable[index-1] != NULL)) {

        /* Pick the next available index */
        index = ++ns->nextkwi;

        /* Is it time to grow the itable? */
        if (index > ns->itsize) {
            ns->itsize += NSKW_ITABLE_INC;
            ns->itable = (NSKWEntry **)pool_realloc(NULL, ns->itable,
                                       ns->itsize*sizeof(NSKWEntry *));
            if (!ns->itable) {
                --ns->nextkwi;
                pool_free(NULL, kw);
                return -1;
            }
        }
    }

    /* Set the assigned index for this entry */
    kw->index = index;

    /* Add the new entry to the hash chain */
    kw->hash = NSKW_HashKeyword(keyword);
    bucket = kw->hash % ns->htsize;
    kw->next = ns->htable[bucket];
    ns->htable[bucket] = kw;

    /* Add the new entry to the index table */
    ns->itable[index - 1] = kw;
        
    return index;
}

PR_IMPLEMENT(unsigned long)
NSKW_HashKeyword(const char * const keyword)
{
    const char *cp;
    unsigned long hash = 0;

    for (cp = keyword; *cp; ++cp) {
        NSKW_HASHNEXT(hash, *cp);
    }

    return hash;
}

PR_IMPLEMENT(int)
NSKW_LookupKeyword(const char * const keyword, int len, PRBool caseMatters,
                   unsigned long hash, NSKWSpace *ns)
{
    NSKWEntry *kw;
    int bucket;

    /* Consistency check length of keyword */
    if (len <= 0) {
        return -1;
    }

    bucket = hash % ns->htsize;
    for (kw = ns->htable[bucket]; kw; kw = kw->next) {

        /* Match hash value before getting into string compares */
        if (kw->hash == hash) {

            if (caseMatters) {
                if (!strncmp(keyword, kw->keyword, len)) {
                    return kw->index;
                }
            }
            else {
                if (!strncasecmp(keyword, kw->keyword, len)) {
                    return kw->index;
                }
            }
        }
    }

    return -1;
}

static int
_NSKW_ResizeHashTable(NSKWSpace *ns, int nhtsize)
{
    int i;

    PR_ASSERT(nhtsize > 0);

    /* Is the hash table already the requested size? */
    if (ns->htsize == nhtsize) {
        return 0;
    }

    /* Attempt to allocate a new hash table */
    NSKWEntry **nhtable = (NSKWEntry **)pool_calloc(NULL, nhtsize,
                                                    sizeof(NSKWEntry *));
    if (!nhtable) {
        return -1;
    }

    /* Discard the old hash table */
    pool_free(NULL, ns->htable);

    /* Use the new hash table */
    ns->htsize = nhtsize;
    ns->htable = nhtable;

    /* Add each entry to the new hash table */
    for (i = 0; i < ns->itsize; i++) {
        NSKWEntry *kw = ns->itable[i];
        if (kw) {
            int bucket = kw->hash % ns->htsize;
            kw->next = ns->htable[bucket];
            ns->htable[bucket] = kw;
        }
    }

    return 0;
}

static int
_NSKW_GetCollisionCount(NSKWSpace *ns, int htsize)
{
    int collisions = 0;
    int i;

    int *counts = (int *) pool_calloc(NULL, htsize, sizeof(int));
    if (!counts)
        return -1;

    /* Count the number of hash buckets that would contain multiple entries */
    for (i = 0; i < ns->itsize; i++) {
        if (ns->itable[i]) {
            int bucket = ns->itable[i]->hash % htsize;
            if (counts[bucket])
                collisions++;
            counts[bucket]++;
        }
    }

    pool_free(NULL, counts);

    return collisions;
}

PR_IMPLEMENT(int)
NSKW_Optimize(NSKWSpace *ns)
{
    int mincollisions = ns->itsize;
    int besthtsize = ns->htsize;
    int htsize = ns->htsize;
    int i;

    /* Try to find the best hash table size */
    for (i = 0; i < NSKW_OPTIMIZE_PASSES; i++) {

        /* How many collisions would we get with this hash table size? */
        int collisions = _NSKW_GetCollisionCount(ns, htsize);
        if (collisions == -1) {
            return -1;
        }

        /* Is that the lowest number of collisions we've seen so far? */
        if (collisions < mincollisions) {
            mincollisions = collisions;
            besthtsize = htsize;
            if (collisions == 0) {
                /* 0 collisions is the best we can do */
                break;
            }
        }

        /* Try another hash table size */
        htsize = findPrime(htsize + 2);
    }

    /* We want 0 collisions, at least in the lab */
    PR_ASSERT(mincollisions == 0);

    return _NSKW_ResizeHashTable(ns, besthtsize);
}

PR_IMPLEMENT(void)
NSKW_DestroyNamespace(NSKWSpace *ns)
{
    if (ns) {

        if (ns->itable) {
            pool_free(NULL, ns->itable);
        }
        if (ns->htable) {
            pool_free(NULL, ns->htable);
        }
        pool_free(NULL, ns);
    }
}
