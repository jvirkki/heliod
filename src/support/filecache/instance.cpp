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
 * Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#include "nsfc_pvt.h"
#include "prtypes.h"
#ifdef XP_UNIX
#include <unistd.h>
#endif
#ifdef XP_WIN32
#include <process.h>
#endif

static PRIntn NSFCInstanceCount = 0;
static NSFCCache NSFCInstanceList = NULL;

static void 
_NSFC_UninitializeCache(NSFCCache cache);

/*
 * NSFC_CreateCache - create a new file cache instance
 */

PR_IMPLEMENT(PRStatus)
NSFC_CreateCache(NSFCCacheConfig *ccfg,
                 NSFCMemAllocFns *memfns, void *memptr,
                 NSFCLocalMemAllocFns *local_memfns,
                 NSFCCache *cache)
{
    NSFCCache cip = NULL;

    /* Do default module initialization if it hasn't been done yet */
    if (!NSFCMonitor) {
        NSFC_Initialize(NULL, NULL, NULL);
    }

    /* Create the file cache instance object */
    PR_ASSERT(!memfns || (memfns->alloc && memfns->free));
    cip = (NSFCCache)((memfns) ? memfns->alloc(memptr, sizeof(*cip)) : PR_MALLOC(sizeof(*cip)));
    if (!cip) {
        /* Failed to allocate cache instance structure */
        return PR_FAILURE;
    }

    memset(cip, 0, sizeof(*cip));

    /* Create a monitor for this instance */
    cip->monitor = PR_NewMonitor();
    if (!cip->monitor) {
        if (memfns)  
            memfns->free(memptr, cip);
        else
            PR_DELETE(cip);
        return PR_FAILURE;
    }
    /* Create the individual locks for this instance */
    cip->hitLock = PR_NewLock();
    cip->namefLock = PR_NewLock();
    cip->keyLock = PR_NewLock();
    if (!cip->monitor || !cip->hitLock || !cip->namefLock || !cip->keyLock) {
        if (cip->monitor) PR_DestroyMonitor(cip->monitor);
        if (cip->hitLock) PR_DestroyLock(cip->hitLock);
        if (cip->namefLock) PR_DestroyLock(cip->namefLock);
        if (cip->keyLock) PR_DestroyLock(cip->keyLock);
        if (memfns)
            memfns->free(memptr, cip);
        else
            PR_DELETE(cip);
        return PR_FAILURE;
    }

    /* Initialize list of free file name entries */
    cip->namefl = NULL;

    cip->hname = NULL;
    cip->keys = NULL;

    /* Copy pointers to memory allocation functions, if any */
    if (memfns && memfns->alloc && memfns->free) {
        cip->memfns = memfns;
        cip->memptr = memptr;
    }
    if (local_memfns && 
        local_memfns->alloc && local_memfns->free) {
        cip->local_memfns = local_memfns;
    }

    PR_ASSERT(ccfg);
    cip->cfg = *ccfg;
    cip->state = NSFCCache_Uninitialized;

    /* Add this instance to the global list of instances */
    NSFC_EnterGlobalMonitor();
    cip->next = NSFCInstanceList;
    NSFCInstanceList = cip;
    ++NSFCInstanceCount;
    NSFC_ExitGlobalMonitor();

    if (ccfg && (ccfg->cacheEnable == PR_TRUE)) {
        (void)NSFC_InitializeCache(ccfg, cip);
    }

    if (cache)
        *cache = cip;

    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
NSFC_UpdateCacheMaxOpen(NSFCCacheConfig *ccfg, NSFCCache cache) 
{
    NSFCCache cip = NULL;
    PRStatus rv = PR_FAILURE;
    if (!cache || !cache->monitor) {
        return PR_FAILURE;
    }

    NSFC_EnterGlobalMonitor();
    for (cip = NSFCInstanceList; cip; cip = cip->next) {
        if (cip == cache) {
            break;
        }
    }
    NSFC_ExitGlobalMonitor();

    PR_ASSERT(cip);
    if (!cip)
        return PR_FAILURE;
    
    PR_EnterMonitor(cache->monitor);
    if (cache->state == NSFCCache_Uninitialized) {
        PR_ExitMonitor(cache->monitor);
        return PR_FAILURE;
    }

    if (ccfg && (ccfg->cacheEnable == PR_TRUE) &&
        cache->curOpen <= ccfg->maxOpen) {
        // Set the new value only if number of currently open fds is 
        // less than or equal to the new value
        cache->cfg.maxOpen = ccfg->maxOpen;
        rv = PR_SUCCESS;
    }
    PR_ExitMonitor(cache->monitor);

    return rv;
}

PR_IMPLEMENT(PRStatus)
NSFC_InitializeCache(NSFCCacheConfig *ccfg, NSFCCache cache)
{
    NSFCCache cip = NULL;
    PRStatus rv = PR_FAILURE;

    if (!cache || !cache->monitor || 
        cache->state != NSFCCache_Uninitialized) {
        return PR_FAILURE;
    }

    NSFC_EnterGlobalMonitor();
    for (cip = NSFCInstanceList; cip; cip = cip->next) {
         if (cip == cache) {
            break;
        }
    }
    NSFC_ExitGlobalMonitor();

    PR_ASSERT(cip);
    if (!cip)
        return PR_FAILURE;

    PR_EnterMonitor(cache->monitor);
    if (cache->state != NSFCCache_Uninitialized) {
        PR_ExitMonitor(cache->monitor);
        return PR_FAILURE;
    }

    if (ccfg) {
        cache->cfg = *ccfg;
        cache->cfg.instanceId = NULL;
        cache->cfg.tempDir = NULL;
    }

    rv = PR_FAILURE;
    if (ccfg && (ccfg->cacheEnable == PR_TRUE)) {

        do {
            cache->state = NSFCCache_Initializing;

            PR_ASSERT(cache->hname == NULL);
            PR_ASSERT(cache->namefl == NULL);
            PR_ASSERT(cache->keys == NULL);
            cache->namefl = NULL;
            cache->keys = NULL;
            cache->cfg.instanceId = NULL;
            cache->cfg.tempDir = NULL;

            /* Allocate hash tables */
            cache->hsize = ccfg->hashInit;
            if (cache->hsize == 0) {
                /* HashInit == 0 means use MaxFiles heuristic */
                /* XXX Shouldn't we make this divisor prime? */
                cache->hsize = ccfg->maxFiles + ccfg->maxFiles + 1;
            }

            /* Hash table currently does not grow after initialization */
            cache->cfg.hashInit = cache->hsize;
            cache->cfg.hashMax = cache->hsize;
            cache->hname = (NSFCEntryImpl **)NSFC_Calloc(cache->hsize,
                                                         sizeof(NSFCEntryImpl *),
                                                         cache);
            if (!cache->hname) break;

            /* Initialize Per-bucket locks */
            cache->bucketLock = (PRLock **)PR_Calloc(cache->hsize, 
                                                     sizeof(PRLock *));
            if (!cache->bucketLock) break;
            cache->bucketHeld = (PRInt32 *)PR_Calloc(cache->hsize, 
                                                     sizeof(PRInt32));
            if (!cache->bucketHeld) break;
            PRUint32 iLock;
            for (iLock = 0; iLock < cache->hsize; iLock++)
            {
                cache->bucketLock[iLock] = PR_NewLock();
                if (!cache->bucketLock[iLock]) break;
            }
            if (iLock != cache->hsize) break;

            /* Set up instance id string */
            if (ccfg && ccfg->instanceId) {
                cache->cfg.instanceId = NSFC_Strdup(ccfg->instanceId, cache);
            }
            else {
                char idstr[12];
                int pid = getpid();

                /* Use pid as the instance id if none supplied */
                PR_snprintf(idstr, sizeof(idstr), "%08x", pid);
                cache->cfg.instanceId = NSFC_Strdup(idstr, cache);
            }

            /* Set up directory for temporary files */
            int tdlen = strlen(ccfg->tempDir);
            if ((ccfg->tempDir[tdlen-1] == '/') && (tdlen > 1)) {
                /* Remove trailing '/' */
                ccfg->tempDir[tdlen-1] = 0;
            }
            cache->cfg.tempDir = NSFC_Strdup(ccfg->tempDir, cache);
            if (!cache->cfg.tempDir) break;

            /* Initialize hit list to be empty */
            PR_Lock(cache->hitLock);
            PR_INIT_CLIST(&cache->hit_list);
            PR_Unlock(cache->hitLock);

            cache->state = NSFCCache_Active;
            rv = PR_SUCCESS;
        } while (0);

        if (rv == PR_FAILURE) { 
            _NSFC_UninitializeCache(cache);
            PR_ASSERT(cache->state == NSFCCache_Uninitialized);
        }
    }

    PR_ASSERT((rv == PR_FAILURE && cache->state == NSFCCache_Uninitialized)
              || (rv == PR_SUCCESS && cache->state == NSFCCache_Active));
    PR_ExitMonitor(cache->monitor);
    return rv;
}

/*
 * caller should be inside cache->montior
 */
static void
_NSFC_UninitializeCache(NSFCCache cache)
{
    if (cache && cache->state == NSFCCache_Initializing) {
         
        /* Free all of the private data key structures */
        for (NSFCPrivDataKey pdk = cache->keys;
             pdk; pdk = cache->keys) {
            cache->keys = pdk->next;

            NSFC_Free(pdk, sizeof(*pdk), cache);
        }
        cache->keys = NULL;

        /* Free the hash table */
        if (cache->hname) {
            NSFC_Free(cache->hname,
                      (cache->hsize * sizeof(NSFCEntryImpl *)), cache);
        }
        cache->hname = NULL;
        if (cache->bucketLock) {
            for (PRUint32 iLoop = 0; iLoop < cache->hsize; iLoop++) {
                if (cache->bucketLock[iLoop]) {
                    PR_DestroyLock(cache->bucketLock[iLoop]);
                }
            }
            PR_Free(cache->bucketLock);
        }
        cache->bucketLock = NULL;
        if (cache->bucketHeld) {
            PR_Free(cache->bucketHeld);
        }
        cache->bucketHeld = NULL;
        if (cache->cfg.instanceId) {
            NSFC_FreeStr(cache->cfg.instanceId, cache);
        }
        cache->cfg.instanceId = NULL;

        /* Free the tempDir name string */
        if (cache->cfg.tempDir) {
            NSFC_FreeStr(cache->cfg.tempDir, cache);
        }
        cache->cfg.tempDir = NULL;

        /* Get rid of any free NSFCEntryImpls */
        while (cache->namefl != NULL) {
            NSFCEntryImpl *pe = cache->namefl;
            cache->namefl = pe->next;
            NSFC_Free(pe, sizeof(*pe), cache);
        }

        cache->state = NSFCCache_Uninitialized;
    }
}

/*
 * NSFC_EnterCacheMonitor - enter the monitor for a cache instance
 */
PR_IMPLEMENT(void)
NSFC_EnterCacheMonitor(NSFCCache cache)
{
    if (cache && cache->monitor) {
        PR_EnterMonitor(cache->monitor);
    }
}

/*
 * NSFC_ExitCacheMonitor - exit the monitor for a cache instance
 *
 * Returns NSFC_OK if normal exit, NSFC_DEADCACHE if cache was destroyed.
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_ExitCacheMonitor(NSFCCache cip)
{
    NSFCEntryImpl *nep;
    NSFCPrivateData *pdelast = NULL;
    NSFCPrivateData *pde;
    NSFCStatus rfc = NSFC_OK;

    if (cip && cip->monitor) {
        if (cip->curFiles <= 0) {
            /* The thread that did the shutdown may be waiting */
            PR_NotifyAll(cip->monitor);
        }
        (void)PR_ExitMonitor(cip->monitor);
    }

    return rfc;
}

/*
 * NSFC_FirstCache - begin an enumeration of all file cache instances
 */
PR_IMPLEMENT(NSFCCache)
NSFC_FirstCache(NSFCCacheEnumerator *cenum)
{
    NSFCCache cip = NULL;

    if (cenum) {
        cip = NSFCInstanceList;
        cenum->next = (cip) ? cip->next : NULL;
    }

    return cip;
}

/*
 * NSFC_NextCache - continue an enumeration of all file cache instances
 */
PR_IMPLEMENT(NSFCCache)
NSFC_NextCache(NSFCCacheEnumerator *cenum)
{
    NSFCCache cip = NULL;

    if (cenum) {
        cip = cenum->next;
        cenum->next = (cip) ? cip->next : NULL;
    }

    return cip;
}

/*
 * NSFC_IsCacheActive - check whether a specified cache is active
 *
 * This function searches the list of known cache instances for a
 * specified instance, and checks whether the instance is active.
 */
PR_IMPLEMENT(PRBool)
NSFC_IsCacheActive(NSFCCache cache)
{
    NSFCCache cip;
    PRBool isActive = PR_FALSE;

    for (cip = NSFCInstanceList; cip; cip = cip->next) {
        if (cip == cache) {
            if (cip->state == NSFCCache_Active) {
                isActive = PR_TRUE;
            }
            break;
        }
    }

    return isActive;
}

/*
 * NSFC_NewPrivateDataKey - allocate a private data key
 *
 * This function allocates a private data key for a specified cache
 * instance.  Each private data key can be used to associate a pointer
 * to a particular type of caller private data with any cache entry,
 * using the NSFC_SetEntryPrivateData() function.  If the caller needs to
 * be notified when the cache entry is deleted, for example to free the
 * private data, a callback function can be specified for that purpose
 * when the private data key is created.  This function is called when
 * either the cache entry is deleted or NSFC_SetEntryPrivateData() is
 * called to replace a non-null private data pointer with a null pointer.
 * Cache private data keys persist until the associated cache instance
 * is destroyed.
 *
 *      cache - file cache instance handle
 *      delfn - a callback function as described above
 */
PR_IMPLEMENT(NSFCPrivDataKey)
NSFC_NewPrivateDataKey(NSFCCache cache, NSFCPrivDataDeleteFN delfn)
{
    NSFCPrivDataKey key = NULL;

    if (cache) {
        key = (NSFCPrivDataKey)NSFC_Malloc(sizeof(struct NSFCPrivDataKey_s),
                                           cache);
        if (key) {
            key->delfn = delfn;
            PR_Lock(cache->keyLock);
            key->next = cache->keys;
            cache->keys = key;
            PR_Unlock(cache->keyLock);
        }
    }

    return key;
}

/*
 * NSFC_GetEntryPrivateData - get private data for entry and key
 *
 * This function retrieves a pointer to caller private data previously
 * set by a call to NSFC_SetEntryPrivateData().  It fails if
 * NSFC_SetEntryPrivateData() has not been called for the entry and key.
 *
 *      entry - handle for cache entry
 *      key - private data key used in call to NSFC_SetEntryPrivateData()
 *      pdata - pointer to where private data pointer is returned
 *      cache - file cache instance handle
 *
 * Notes:
 *     1. The caller is assumed to have incremented the use count
 *        for the target cache entry.
 *
 *     2. If successful, NSFC_OK is returned.
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_GetEntryPrivateData(NSFCEntry entry, NSFCPrivDataKey key,
                         void **pdata, NSFCCache cache)
{
    NSFCEntryImpl *nep = entry;
    NSFCPrivateData *pde = NULL;
    NSFCStatus rfc;

    PR_ASSERT(nep->refcnt >= 1);

    /* Search for private data matching the specified PD key */
    if (nep->pdlist) {
        /* XXX elving reduce pdLock lock contention */
        PR_Lock(nep->pdLock);
        for (pde = nep->pdlist; pde; pde = pde->next) {
            if (pde->key == key) {
                break;
            }
        }
        PR_Unlock(nep->pdLock);
    }

    if (pde) {
        NSFC_RecordEntryHit(cache, entry);
        if (pdata) {
            *pdata = pde->pdata;
        }
        rfc = NSFC_OK;
    } else {
        rfc = NSFC_NOTFOUND;
    }

    return rfc;
}

/*
 * NSFC_SetEntryPrivateData - store private data for a cache entry
 *
 * This function stores a pointer to caller private data under a previously
 * created private data key, and associates it with a specified file.
 *
 *      entry - handle for cache entry
 *      key - private data key
 *      pdata - caller private data pointer
 *      cache - file cache instance handle, or NULL for default
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_SetEntryPrivateData(NSFCEntry entry, NSFCPrivDataKey key, void *pdata,
                         NSFCCache cache)
{
    NSFCEntryImpl *nep = entry;
    NSFCPrivDataKey kdef;
    NSFCPrivateData *opde;
    NSFCPrivateData *npde;

    PR_ASSERT(nep->refcnt >= 1);

    if (nep->fDelete) {
        return NSFC_DELETED;
    }

#ifdef DEBUG
    /* Search the key definitions for a matching key */
    PR_Lock(cache->keyLock);
    for (kdef = cache->keys; kdef; kdef = kdef->next) {
        if (kdef == key) {
            /* specified key is valid */
            break;
        }
    }
    PR_Unlock(cache->keyLock);

    PR_ASSERT(kdef != NULL);
#endif

    /*
     * Look for a private data element that has a matching key in the cache
     * entry's list.
     */
    PR_Lock(nep->pdLock);
    for (opde = nep->pdlist; opde; opde = opde->next) {
        if (opde->key == key) {
            break;
        }
    }
    PR_Unlock(nep->pdLock);

    if (opde) {
        /* Can't replace existing element because someone might be using it */
        NSFC_DeleteEntry(cache, nep, PR_FALSE);
        return NSFC_BUSY;
    }

    /* Allocate a new element */
    npde = (NSFCPrivateData *)NSFC_Malloc(sizeof(*npde), cache);
    if (!npde) {
        return NSFC_NOSPACE;
    }

    npde->key = key;
    npde->pdata = pdata;

    /* Add the new element, provided nobody else beat us to it */
    PR_Lock(nep->pdLock);
    for (opde = nep->pdlist; opde; opde = opde->next) {
        if (opde->key == key) {
            break;
        }
    }
    if (!opde) {
        npde->next = nep->pdlist;
        nep->pdlist = npde;
    }
    PR_Unlock(nep->pdLock);

    if (opde) {
        /* Someone else added data with this key first */
        NSFC_Free(npde, sizeof(*npde), cache);
        return NSFC_BUSY;
    }

    /* Cache contents have been modified */
    cache->sig++;

    return NSFC_OK;
}

PR_IMPLEMENT(NSFCCacheSignature)
NSFC_GetCacheSignature(NSFCCache cache)
{
    return cache->sig;
}

/*
 * NSFC_ShutdownCache - shut down a cache instance
 *
 * This function shuts down a cache instance.  This causes all existing
 * cache entries to be destroyed as soon as they are not being used.  It
 * does not cause the cache instance itself to be destroyed.  The handle
 * for the cache instance can continue to be used with some functions,
 * such as NSFC_GetFileInfo() and NSFC_TransmitFile(), and these functions
 * will perform their operations without the benefit of caching.
 *
 *      cache - file cache instance handle
 *      doitnow - PR_TRUE: wait for all cache entries to be destroyed
 *                PR_FALSE: return PR_FAILURE if any cache entries are
 *                          in use and cannot be destroyed
 */
PR_IMPLEMENT(PRStatus)
NSFC_ShutdownCache(NSFCCache cache, PRBool doitnow)
{
    NSFC_EnterCacheMonitor(cache);

    if (cache->state == NSFCCache_Dead) {
        return PR_FAILURE;
    }
    if (cache->state == NSFCCache_Shutdown ||
        cache->state == NSFCCache_Uninitialized ||
        cache->state == NSFCCache_TerminatingWait) {
        NSFC_ExitCacheMonitor(cache);
        return PR_FAILURE;
    }

    cache->state = NSFCCache_Terminating;

    do {
        /* If the hash table exists, mark all entries for deletion */
        NSFCEntryImpl **ht = cache->hname;
        if (ht) {

            /* Scan hash buckets */
            for (PRUint32 i = 0; i < cache->hsize; ++i) {
                NSFCEntryImpl *pe;
                NSFCEntryImpl *npe;

                NSFC_ACQUIREBUCKET(cache, i);

                /* Scan bucket collision list */
                for (pe = ht[i]; pe; pe = npe) {
                    npe = pe->next;

                    /* Mark entry for delete */
                    if (!pe->fDelete)
                        NSFC_DeleteEntry(cache, pe, PR_TRUE);
                }

                NSFC_RELEASEBUCKET(cache, i);
            }
        }

        /* We're all done if caller doesn't want to wait */
        if (!doitnow) break;

        /* Wait for the last entry to be destroyed */
        cache->state = NSFCCache_TerminatingWait;
        PR_Wait(cache->monitor, PR_SecondsToInterval(1));        
    } while (cache->curFiles > 0);

    if (cache->curFiles > 0) {
        /* Cache is terminating and doitnow is not set */
        NSFC_ExitCacheMonitor(cache);
        return PR_FAILURE;
    }

    cache->state = NSFCCache_Shutdown;

    NSFC_ExitCacheMonitor(cache);

    /* 
     * Note that this does not free the cache itself.  In earlier revisions of
     * instance.cpp, there was a function named NSFC_DestroyCache() which tore 
     * down the cache.  When per-bucket locks were added to the cache,
     * NSFC_DestroyCache() was removed as 1. some work was required to 
     * correctly synchronize cache destruction and 2. it wasn't being used.
     *
     * Revision 1.1.2.28 was the last to feature NSFC_DestroyCache().
     */

    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
NSFC_StartupCache(NSFCCache cache)
{
    PRStatus rv = PR_FAILURE;

    NSFC_EnterCacheMonitor(cache);
    if (cache->state == NSFCCache_Shutdown) {
        cache->state = NSFCCache_Active;
        rv = PR_SUCCESS;
    }
    NSFC_ExitCacheMonitor(cache);

    return rv;
}
