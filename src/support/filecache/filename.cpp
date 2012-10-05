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

#include "prpdce.h"
#include "nsfc_pvt.h"
#include "time/nstime.h"

static NSFCStatus
_NSFC_DeleteEntry(NSFCCache cache, NSFCEntryImpl *nep, 
                     PRBool hasBucketLock, PRBool delCntIt, PRBool outdCntIt);

static NSFCStatus
_NSFC_ReleaseEntry(NSFCCache cache, NSFCEntryImpl *nep, PRBool *destroyed);

static PRBool
_NSFC_IsTimeToReplace(NSFCCache cip);

static void
_NSFC_DestroyEntry(NSFCCache cip, NSFCEntryImpl *nep, PRBool delCntIt);

static NSFCStatus
_NSFC_RejuvenateEntry(NSFCCache cache, NSFCEntryImpl *nep);

#ifdef DEBUG

/*
 * NSFC_AcquireBucket - debug implementation of NSFC_ACQUIREBUCKET
 */
PR_IMPLEMENT(void)
NSFC_AcquireBucket(NSFCCache cache, int bucket)
{
    PR_Lock(cache->bucketLock[bucket]);
    ++cache->bucketHeld[bucket];
    PR_ASSERT(cache->bucketHeld[bucket] == 1);
}

/*
 * NSFC_ReleaseBucket - debug implementation of NSFC_RELEASEBUCKET
 */
PR_IMPLEMENT(void)
NSFC_ReleaseBucket(NSFCCache cache, int bucket)
{
    PR_ASSERT(cache->bucketHeld[bucket] == 1);
    --cache->bucketHeld[bucket];
    PR_Unlock(cache->bucketLock[bucket]);
}

#endif

/*
 * NSFC_CheckEntry - checks whether an entry is still useable
 *
 * This function checks whether an entry returned by NSFC_LookupFilename() or
 * NSFC_AccessFilename() is current.  If the entry has exceeded its maximum
 * age, NSFC_CheckEntry() checks whether the file has changed on disk.  If
 * the maximum age has not been reached or the file has not changed on disk,
 * the file cache has not been shutdown, and the entry has not been marked for
 * deletion, NSFC_CheckEntry() returns NSFC_OK.
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_CheckEntry(NSFCEntry entry, NSFCCache cache)
{
    if (cache->state != NSFCCache_Active) {
        return NSFC_DEADCACHE;
    }

    if (entry->fDelete) {
        return NSFC_DELETED;
    }

    if (cache->cfg.maxAge != PR_INTERVAL_NO_TIMEOUT) {
        PRIntervalTime age = ft_timeIntervalNow() - entry->finfo.lastUpdate;
        if (age > cache->cfg.maxAge) {
            if (_NSFC_RejuvenateEntry(cache, entry) != NSFC_OK) {
                return NSFC_DELETED;
            }
        }
    }

    return NSFC_OK;
}

/*
 * NSFC_RefreshFilename - ensure cache consistency for file
 *
 * This function ensures that any file cache entry for the named file is
 * consistent with what's on disk.  If an existing entry contains outdated
 * information, the entry is marked for deletion.
 */
PR_IMPLEMENT(void)
NSFC_RefreshFilename(const char *filename, NSFCCache cache)
{
    NSFCEntry entry = NSFCENTRY_INIT;
    if (NSFC_LookupFilename(filename, &entry, cache) == NSFC_OK) {
        _NSFC_RejuvenateEntry(cache, entry);
        NSFC_ReleaseEntry(cache, &entry);
    }
}

/*
 * NSFC_InvalidateFilename - mark file cache entry for deletion
 *
 * This function marks any file cache entry for the named file for deletion.
 */
PR_IMPLEMENT(void)
NSFC_InvalidateFilename(const char *filename, NSFCCache cache)
{
    NSFCEntry entry = NSFCENTRY_INIT;
    if (NSFC_LookupFilename(filename, &entry, cache) == NSFC_OK) {
        NSFC_DeleteEntry(cache, entry, PR_FALSE);
        NSFC_ReleaseEntry(cache, &entry);
    }
}

/*
 * NSFC_LookupFilename - find cache entry handle for a filename
 *
 * This function attempts to find a cache entry for a specified filename
 * in a specified cache instance.  If a handle for this cache entry is
 * returned successfully, the calling thread will have a read lock on
 * the entry, and the entry cannot be deleted until NSFC_ReleaseEntry()
 * is called to release the lock.  Another function, NSFC_AccessFilename(),
 * also returns a handle for a cache entry, but will attempt to create
 * a new entry if one does not exist.
 *
 *      filename - filename for which a cache entry handle is needed
 *      entry - pointer to where cache entry handle is returned
 *      cache - file cache instance handle, or NULL for default
 *
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_LookupFilename(const char *filename, NSFCEntry *entry,
                    NSFCCache cache)
{
    NSFCEntryImpl *nep;
    PRUint32 hval;
    NSFCStatus rfc;

    PR_ASSERT(entry);

    /* Find cache entry and acquire a read lock */
    nep = NSFC_ActivateEntry(filename, *entry, hval, rfc, cache);
    if (nep == NULL) {
        switch (rfc) {
        case NSFC_NOTFOUND:
            /* Couldn't find entry */
            PR_ASSERT(*entry == NULL);
            break;

        case NSFC_NOFINFO:
        case NSFC_DELETED:
        case NSFC_DEADCACHE:
            /* Entry wasn't usable */
            break;

        default:
            /* Unexpected return code from NSFC_ActivateEntry() */
            PR_ASSERT(0);
            break;
        }
    }
    else {
        PR_ASSERT(rfc == NSFC_OK);
        PR_ASSERT((*entry == NULL) || (nep == *entry));
        PR_ASSERT(nep->flags & NSFCENTRY_HASINFO);
        PR_ASSERT(nep->refcnt >= 1);

        *entry = nep;
    }

    return rfc;
}

/*
 * NSFC_AccessFilename - get cache entry handle for a filename
 *
 * This function finds or creates a cache entry for a specified filename
 * in a specified cache instance.  If a valid cache entry handle is
 * specified, that handle will not be changed on return, and the return
 * status will reflect the status of that particular entry.  If an
 * invalid cache entry handle is specified, an attempt will be made
 * to find or create a new cache entry for the specified filename, and
 * if either is successful, a handle for the resulting entry is returned,
 * and the caller is responsible for eventually releasing it.
 *
 *      filename - filename for which a cache entry handle is needed
 *      entry - input or output handle for cache entry
 *      cache - file cache instance handle, or NULL for default
 *
 * >>New Comment>> on NOTFOUND, new entries are created with finfo.  
 *
 * On successful return, caller should always use the returned entry's
 * finfo.
 *
 * if returns NSFC_STATFAIL, error finfo will be returned in efinfo if
 * it's passed in not NULL.
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_AccessFilename(const char *filename, NSFCEntry *entry,
                       NSFCFileInfo *efinfo,
                       NSFCCache cache,
                       NSFCStatusInfo *statusInfo)
{
    NSFCEntryImpl *nep;
    PRUint32 hval;
    PRUint32 bucket;
    NSFCStatus rfc;

    NSFCSTATUSINFO_INIT(statusInfo);

    PR_ASSERT(entry);

    nep = NSFC_ActivateEntry(filename, *entry, hval, rfc, cache);
    if (nep == NULL) {
        switch (rfc) {
        case NSFC_NOTFOUND:
            /* Couldn't find entry */
            PR_ASSERT(*entry == NULL);

            /* hval was computed by NSFC_ActivateEntry() */
            bucket = hval % cache->hsize;
            NSFC_ACQUIREBUCKET(cache, bucket);

            /* Looks like we're going to have to create an entry */
            nep = NSFC_NewFilenameEntry(cache, filename, hval, rfc);
            if (nep) {
                PR_ASSERT(rfc == NSFC_OK);

                /* 
                 * At this point, NSFC_NewFilenameEntry() has added nep to the 
                 * table.  However, since we hold the bucket lock for nep, 
                 * we're the only ones who can see it.
                 */

                /* Get finfo for this entry */
                NSFCFileInfo f_info;
                PRStatus rv = NSFC_GetNonCacheFileInfo(filename, &f_info);
                if (rv == PR_FAILURE) {
                    PR_ASSERT(nep->fWriting == 0);
                    --nep->refcnt;
                    PR_ASSERT(nep->refcnt == 0);

                    _NSFC_DeleteEntry(cache, nep, PR_TRUE, PR_FALSE, PR_FALSE);

                    NSFC_RELEASEBUCKET(cache, bucket);

                    if (efinfo) {
                        memcpy(efinfo, &f_info, sizeof(*efinfo));
                        NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_STAT);
                    }

                    rfc = NSFC_STATFAIL;
                    break;
                }
                nep->finfo.pr.type = f_info.pr.type;
                nep->finfo.pr.size = f_info.pr.size;
                nep->finfo.pr.creationTime = f_info.pr.creationTime;
                nep->finfo.pr.modifyTime = f_info.pr.modifyTime;
                nep->finfo.lastUpdate = f_info.lastUpdate;
                nep->finfo.prerr = f_info.prerr;
                nep->finfo.oserr = f_info.oserr;
                PR_ASSERT(nep->refcnt == 1);
                PR_ASSERT(nep->flags == 0);
                if (f_info.prerr != 0 || f_info.oserr != 0)
                    nep->flags |= NSFCENTRY_ERRORINFO;
                nep->flags |= NSFCENTRY_HASINFO;
            }

            /* After this, everyone will be able to see nep (if it exists) */
            NSFC_RELEASEBUCKET(cache, bucket);

            if (nep) {
                PR_ASSERT(rfc == NSFC_OK);
                PR_ASSERT(nep->flags & NSFCENTRY_HASINFO);
                PR_ASSERT(nep->refcnt >= 1);

                NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_CREATE);

                /* Return handle for new entry */
                *entry = nep;
            }
            break;
            
        case NSFC_NOFINFO:
        case NSFC_DELETED:
        case NSFC_DEADCACHE:
            /* Entry wasn't usable */
            break;

        default:
            /* Unexpected return code from NSFC_ActivateEntry() */
            PR_ASSERT(0);
            break;
        }
    }
    else {
        PR_ASSERT(rfc == NSFC_OK);
        PR_ASSERT((*entry == NULL) || (nep == *entry));
        PR_ASSERT(nep->flags & NSFCENTRY_HASINFO);
        PR_ASSERT(nep->refcnt >= 1);

        *entry = nep;
    }

    return rfc;
}

/*
 * NSFC_AcquireEntryWriteLock - acquire write access to an entry
 *
 * This function acquires write access to an entry.  It limits
 * access to one writer at a time.  It assumes that the caller
 * has incremented the entry's use count.  It will allow other
 * threads to increment the use count and read the entry while
 * the writer is active.
 *
 * If another thread already has write access, it does not wait,
 * but simply returns NSFC_BUSY.  If write access is obtained successfully,
 * it returns NSFC_OK.
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_AcquireEntryWriteLock(NSFCCache cip, NSFCEntryImpl *nep)
{
    PRInt32 rv;

    /*
     * XXX elving this is unsafe on platforms that don't guarantee total store
     * order
     */

    rv = PR_AtomicSet(&nep->fWriting, 1);

    /* We got it if the prior value was zero */
    return (rv) ? NSFC_BUSY : NSFC_OK;
}

PR_IMPLEMENT(void)
NSFC_ReleaseEntryWriteLock(NSFCCache cip, NSFCEntryImpl *nep)
{
    PRInt32 rv;

    PR_ASSERT(nep->refcnt >=1 && nep->fWriting);
    rv = PR_AtomicSet(&nep->fWriting, 0);
    PR_ASSERT(rv == 1);

    /* Cache contents have been modified */
    cip->sig++;
}

/*
 * FUNCTION: NSFC_ActivateEntry - activate an Idle cache entry handle
 *
 * This function contains code that is common to NSFC_AccessFilename()
 * and NSFC_LookupFilename(), and is not intended to be used beyond
 * that.  It takes both a cache entry handle and a filename as arguments.
 * If the cache entry handle is in a Valid state, an attempt is made
 * to reactivate the referenced entry.  If that fails, or if the handle
 * was in the Invalid state, an attempt is made to find a cache entry
 * that matches the specified filename. 
 *
 * >>New Comment>> the above last sentence is incorrect. If a non NULL
 * entry handle passed, if the handle is not in a valid state, this 
 * function returns NULL and rfc indicates the failure; if the handle 
 * failed on reactivating (validate its finfo), the entry is marked for
 * fDelete and return NULL.  This behavior should not be changed otherwise
 * may cause leak references for other functions dependent on it (eg.
 * NSFC_LookupFilename).
 *
 * The bucket lock may be acquired during processing, so the caller must 
 * not hold any bucket locks.  The bucket lock is always released before 
 * we return.
 *
 * In summary:
 *
 *     NULL return value:
 *         NSFC_NOTFOUND
 *             no matching entry
 *             filename hash value has been computed
 *         NSFC_DELETED
 *             entry specified by handle is marked for delete
 *             filename hash value has not necessarily been computed
 *         NSFC_DEADCACHE
 *             cache is dead
 *             no matching entry
 *             filename hash value has not necessarily been computed
 *
 *     non-NULL return value:
 *         matching entry is read locked
 *         filename hash value has not necessarily been computed
 */
PR_IMPLEMENT(NSFCEntryImpl *)
NSFC_ActivateEntry(const char *filename, 
                   NSFCEntry entry,
                   PRUint32 &hval, 
                   NSFCStatus &rfc, 
                   NSFCCache cache)
{
    PRBool fHashComputed = PR_FALSE;
    int lookupInc = 0;    /* lookupInc only when hname is looked up */
    int outdInc = 0;

    rfc = NSFC_OK;
    if (!cache || (cache->state != NSFCCache_Active)) {
        rfc = NSFC_DEADCACHE;
    }

    while (rfc == NSFC_OK) {
        NSFCEntryImpl *nep;

        if (entry) {
            nep = entry;

            /* Don't activate an entry with fDelete set */
            if (nep->fDelete) {
                rfc = NSFC_DELETED;
                break; /* Failure */
            }
        }
        else {

            /* Didn't have an entry handle, so find it by filename */
            PR_ASSERT(filename != NULL);
            nep = NULL;

            /* Hash the filename */
            if (!fHashComputed) {
                hval = NSFC_HashFilename(filename);
                fHashComputed = PR_TRUE;
            }

            /* Look for filename in the linked list at this hash bucket */
            PRUint32 bucket = hval % cache->hsize;
            if (cache->hname[bucket]) {
                NSFCEntryImpl *next = NULL;

                /* Take a quick, optimistic look at the linked list */
                NSFC_ACQUIREBUCKET(cache, bucket);
                for (nep = cache->hname[bucket]; nep; nep = next) {
                    next = nep->next;
                    if (nep->hash == hval) {
                        /* This is probably the entry we're after */
                        ++nep->refcnt;
                        break;
                    }
                }
                NSFC_RELEASEBUCKET(cache, bucket);

                /* Check whether we got the wrong entry by mistake */
                if (nep && NSFC_CmpFilename(filename, nep->filename)) {
                    _NSFC_ReleaseEntry(cache, nep, NULL);
                    nep = NULL;
                }

                /* If we didn't find the entry we were looking for... */
                if (!nep) {
                    /* If there are other entries at this hash bucket... */
                    if (next) {
                        /* Take a closer look at the linked list */
                        NSFC_ACQUIREBUCKET(cache, bucket);
                        for (nep = cache->hname[bucket]; nep; nep = nep->next) {
                            if (nep->hash == hval) {
                                if (!NSFC_CmpFilename(filename, nep->filename)) {
                                    /* Yay, we finally found the entry! */
                                    ++nep->refcnt;
                                    break;
                                }
                            }
                        }
                        NSFC_RELEASEBUCKET(cache, bucket);
                    }
                }
            }

            lookupInc++;

            if (!nep) {
                rfc = NSFC_NOTFOUND;
                break; /* Failure */
            }

            if (!(nep->flags & NSFCENTRY_HASINFO)) {
                _NSFC_ReleaseEntry(cache, nep, NULL);
                rfc = NSFC_NOFINFO;
                break; /* Failure */
            }

            if (nep->fDelete) {
                _NSFC_ReleaseEntry(cache, nep, NULL);
                rfc = NSFC_DELETED;
                break; /* Failure */
            }
        }

        /*
         * At this point we have found an entry that is not marked for delete
         * and its refcnt has been set.
         */

        PR_ASSERT(nep->refcnt >= 1);

        /* Increment the lookup count if not done previously */
        if (lookupInc) {
            PR_AtomicIncrement((PRInt32*)&cache->lookups);
            PR_AtomicIncrement((PRInt32*)&cache->hitcnt);
            lookupInc = 0;
        }

        /*
         * If the entry has expired, try to rejuvenate it.
         * If that fails, we mark it for delete.
         */
        if (cache->cfg.maxAge != PR_INTERVAL_NO_TIMEOUT) {

            PRIntervalTime age = ft_timeIntervalNow() - nep->finfo.lastUpdate;
            if (age > cache->cfg.maxAge) {

                /* If the file hasn't changed, rejuvenate the entry */
                if (_NSFC_RejuvenateEntry(cache, nep) != NSFC_OK) {

                    /* If we found nep by filename, release it */
                    if (nep != entry) {
                        PR_ASSERT(!entry);

                        PRBool destroyed = PR_FALSE;
                        rfc = _NSFC_ReleaseEntry(cache, nep, &destroyed);

                        /* only go back lookup once */
                        if (outdInc == 0 && destroyed == PR_TRUE) {
                            outdInc = 1;
                            continue;
                        }
                    }

                    rfc = NSFC_DELETED;
                    break; /* Failure */
                }
            }
        }

        /* Adjust counts */
        if (lookupInc) PR_AtomicIncrement((PRInt32*)&cache->lookups);

        /* Success */
        return nep;
    }

    /* Adjust counts */
    if (lookupInc) PR_AtomicIncrement((PRInt32*)&cache->lookups);
    if (outdInc) PR_AtomicDecrement((PRInt32*)&cache->hitcnt);

    /* Failure */
    return NULL;
}

PR_IMPLEMENT(void)
NSFC_RecordEntryHit(NSFCCache cache, NSFCEntry entry)
{
    PR_ASSERT(entry->refcnt >= 1);

    entry->hitcnt++;

    /*
     * If existing entries can be recycled for new files, indicate that
     * this entry is active.
     */
    if (cache->cfg.replaceFiles == PR_TRUE) {
        /* Update the hit list order if this entry is in the hit list */
        PR_Lock(cache->hitLock);
        if (PR_LIST_HEAD(&entry->hit_list) != PR_LIST_TAIL(&entry->hit_list)) {
            if (cache->cfg.hitOrder == PR_TRUE) {
                /*
                 * If this entry is not at the head of the hit list,
                 * move it ahead of all entries with the same hitcnt.
                 */
                PRCList *prev;
                NSFCEntryImpl *pnep;

                for (prev = PR_PREV_LINK(&entry->hit_list);
                     prev != &cache->hit_list;
                     prev = PR_PREV_LINK(prev)) {

                    pnep = NSFCENTRYIMPL(prev);
                    if (pnep->hitcnt > entry->hitcnt) {
                        break; /* Our spot in the list */
                    }
                }

                /* Move the element up if necessary */
                if (prev != PR_PREV_LINK(&entry->hit_list)) {
                    PR_REMOVE_LINK(&entry->hit_list);
                    PR_INSERT_AFTER(&entry->hit_list, prev);
                }
            }
            else {
                /* Ignore hitcnt, keep list in strict MRU to LRU order */
                if (&entry->hit_list != PR_LIST_HEAD(&cache->hit_list)) {
                    PR_REMOVE_LINK(&entry->hit_list);
                    PR_INSERT_LINK(&entry->hit_list, &cache->hit_list);
                }
            }
        }
        PR_Unlock(cache->hitLock);
    }
}

/*
 * NSFC_DeleteEntry - delete a specified cache entry
 *
 * This function marks a specified cache entry for deletion
 *
 * Assumptions:
 *    if (hasBucketLock == PR_FALSE) then
 *        The caller does not hold the bucket lock for the entry
 *        The caller holds a read or write lock on the entry, and will
 *        release the lock on return
 *    else
 *        The caller holds the bucket lock for the entry
 */

PR_IMPLEMENT(NSFCStatus)
NSFC_DeleteEntry(NSFCCache cache, NSFCEntryImpl *nep, PRBool hasBucketLock)
{
    return _NSFC_DeleteEntry(cache, nep, hasBucketLock, PR_TRUE, PR_FALSE);
}

static NSFCStatus
_NSFC_DeleteEntry(NSFCCache cache, NSFCEntryImpl *nep, 
                  PRBool hasBucketLock, PRBool delCntIt, PRBool outdCntIt)
{
    NSFCStatus rfc = NSFC_OK;

    PR_ASSERT(nep);

    /* Get access to nep's bucket if caller doesn't have it already */
    PRUint32 bucket;
    if (hasBucketLock == PR_FALSE) {
        if (nep->fDelete) {
            return rfc;
        }
        bucket = nep->hash % cache->hsize;
        NSFC_ACQUIREBUCKET(cache, bucket);
    } else {
        NSFC_ASSERTBUCKETHELD(cache, nep->hash % cache->hsize);
    }

    /* If nep hasn't already been marked for deletion... */
    if (!nep->fDelete) {

        /* Remove nep from the hit list */
        PR_Lock(cache->hitLock);
        PR_REMOVE_AND_INIT_LINK(&nep->hit_list);
        PR_Unlock(cache->hitLock);
        PR_ASSERT(PR_LIST_HEAD(&nep->hit_list) == PR_LIST_TAIL(&nep->hit_list));

        /* Mark it for delete */
        nep->fDelete = 1;

        /* Increment total count of entries deleted */
        if (delCntIt == PR_TRUE) { 
            PR_AtomicIncrement((PRInt32 *)&cache->delCnt);
            PR_AtomicIncrement((PRInt32 *)&cache->busydCnt);
            if (outdCntIt == PR_TRUE)
                PR_AtomicIncrement((PRInt32 *)&cache->outdCnt);
        }
    }

    if (nep->refcnt == 0) {
        /*
         * Given that the entry's use count is zero, the caller had better have
         * the bucket lock (because he clearly doesn't have a refcnt)
         */
        PR_ASSERT(hasBucketLock == PR_TRUE);

        _NSFC_DestroyEntry(cache, nep, delCntIt);
    }

    /* Release the bucket lock if we weren't holding it on entry */
    if (hasBucketLock == PR_FALSE) {
        NSFC_RELEASEBUCKET(cache, bucket);
    }

    /* Cache contents have been modified */
    cache->sig++;

    return rfc;
}

/*
 * NSFC_DestroyEntry - destroy a cache entry
 *
 * Assumptions:
 *
 *      Caller has bucket lock for the entry.
 *      Entry refcnt is zero.
 *      Entry is marked for delete and is in cache->hname hashtable
 *
 */
PR_IMPLEMENT(void)
NSFC_DestroyEntry(NSFCCache cip, NSFCEntryImpl *nep)
{
    _NSFC_DestroyEntry(cip, nep, PR_TRUE);
}

static void
_NSFC_DestroyEntry(NSFCCache cip, NSFCEntryImpl *nep, PRBool delCntIt)
{
    NSFCEntryImpl **list;
    PRUint32 bucket = nep->hash % cip->hsize;
    int found = 0;

    PR_ASSERT(nep->refcnt == 0);
    PR_ASSERT(nep->fDelete);
    PR_ASSERT(nep->fHashed);

    ++nep->seqno;

    /* First remove the name entry from the hname hash table. */
    NSFC_ASSERTBUCKETHELD(cip, bucket);
    for (list = &cip->hname[bucket];
         *list; list = &(*list)->next) {
         if (*list == nep) {
             *list = nep->next;
             found = 1;
             break;
         }
    }
    PR_ASSERT(found == 1);
    nep->fHashed = 0;

    if (delCntIt == PR_TRUE) {
        PR_ASSERT(cip->busydCnt > 0);
        PR_AtomicDecrement((PRInt32 *)&cip->busydCnt);
    }

    /* No one but us should be able to see this entry */
    PR_ASSERT(nep->fDelete == PR_TRUE);
    PR_ASSERT(nep->fHashed == PR_FALSE);
    PR_ASSERT(nep->refcnt == 0);
    PR_ASSERT(PR_LIST_HEAD(&nep->hit_list) == PR_LIST_TAIL(&nep->hit_list));

    /* Free cached content */
    if (nep->flags & NSFCENTRY_HASCONTENT) {
        NSFC_MD_FreeContent(cip, nep);
    }

    /* Handle notifications for any associated private data */
    NSFCPrivateData *pdenext;
    NSFCPrivateData *pde;
    for (pde = nep->pdlist; pde; pde = pdenext) {
        pdenext = pde->next;
        if (pde->key->delfn) {
            pde->key->delfn(cip, nep->filename, pde->key, pde->pdata);
        }
        NSFC_Free(pde, sizeof(*pde), cip);
    }

    /* Destroy private data lock */
    if (nep->pdLock) {
        PR_DestroyLock(nep->pdLock);
        nep->pdLock = NULL;
    }

    /* Free filename string from entry, if any */
    if (nep->filename) {
        NSFC_FreeStr(nep->filename, cip);
        nep->filename = NULL;
    }

    /* Return the file name entry to the free list */
    PR_Lock(cip->namefLock);
    nep->next = cip->namefl;
    cip->namefl = nep;
    PR_Unlock(cip->namefLock);

    /* Finally decrement the number of entries used */
    if (!PR_AtomicDecrement((PRInt32*)&cip->curFiles)) {
        /* Wake anyone waiting for this final entry to go away */
        NSFC_RELEASEBUCKET(cip, bucket);
        NSFC_EnterCacheMonitor(cip);
        NSFC_ExitCacheMonitor(cip);
        NSFC_ACQUIREBUCKET(cip, bucket);
    }

    /* Cache contents have been modified */
    cip->sig++;
}

/*
 * NSFC_HashFilename - compute hash value for a filename
 */
PR_IMPLEMENT(PRUint32)
NSFC_HashFilename(const char *filename)
{
    PRUint32 hval = 0;

    while (*filename) {
        hval = (hval<<5) + hval + *(unsigned char *)filename;
        ++filename;
    }

    return hval;
}

/*
 * Assumptions:
 *
 *      Caller has bucket lock for hvalue.  Note that the bucket lock may be
 *      released and reacquired during processing.
 */
PR_IMPLEMENT(NSFCEntryImpl *)
NSFC_NewFilenameEntry(NSFCCache cip, const char *filename, 
                      PRUint32 hvalue, NSFCStatus &rfc)
{
    PRUint32 bucket = hvalue % cip->hsize;

    PR_ASSERT(cip);

    if (cip->state != NSFCCache_Active) {
        rfc = NSFC_DEADCACHE;
        return NULL;
    }

    rfc = NSFC_OK;

    /* Replace file cache entries once the cache fills up */
    if (_NSFC_IsTimeToReplace(cip)) {
        PR_Lock(cip->hitLock);
        if (!PR_CLIST_IS_EMPTY(&cip->hit_list)) {
            NSFCEntryImpl* nepDelete;
            PRUint32 bucketDelete;

            /* Get the LRU entry from the hit list and remember its bucket */
            PRCList *lru = PR_LIST_TAIL(&cip->hit_list);
            PR_ASSERT(lru);
            nepDelete = (NSFCEntryImpl*)((char*)lru - offsetof(NSFCEntryImpl,
                                                               hit_list));
            bucketDelete = nepDelete->hash % cip->hsize;
            PR_Unlock(cip->hitLock);

            /* Get access to the LRU entry's bucket */
            if (bucket != bucketDelete) {
                NSFC_RELEASEBUCKET(cip, bucket);
                NSFC_ACQUIREBUCKET(cip, bucketDelete);
            }

            /* Look for the LRU entry in the bucket */
            NSFCEntryImpl *nep;
            for (nep = cip->hname[bucketDelete]; nep; nep = nep->next) {
                if (nep == nepDelete) break;
            }
            if (nep == nepDelete) {
                /* The LRU entry is still around, mark it for deletion */
                NSFC_DeleteEntry(cip, nep, PR_TRUE);

                /* Increment count of replaced entries */
                PR_AtomicIncrement((PRInt32*)&cip->rplcCnt);
            }

            /* Get access to the new entry's bucket */
            if (bucket != bucketDelete) {
                NSFC_RELEASEBUCKET(cip, bucketDelete);
                NSFC_ACQUIREBUCKET(cip, bucket);
            }
        }
        else {
            PR_Unlock(cip->hitLock);
        }
    }

    /* Respect limit on number of cache entries */
    if (cip->curFiles >= cip->cfg.maxFiles) {
        cip->cacheFull = PR_TRUE;
        rfc = NSFC_NOSPACE;
        return NULL;
    }

    /* Get a file name entry */
    PR_Lock(cip->namefLock);
    NSFCEntryImpl *nep = cip->namefl;
    if (nep != NULL) {
        /* Found a file name entry on the free list */
        PR_ASSERT(nep->refcnt == 0);
        PR_ASSERT(!nep->fHashed);
        cip->namefl = nep->next;
    }
    PR_Unlock(cip->namefLock);
    if (nep == NULL) {
        /* Allocate a new file name entry */
        nep = (NSFCEntryImpl *)NSFC_Calloc(1, sizeof(*nep), cip);
        if (nep) {
            nep->seqno = 1;
        }
    }

    if (nep) {
        nep->filename = NSFC_Strdup(filename, cip);
        if (nep->filename) {
            /* Initialize entry */
            nep->next = NULL;
            nep->pdLock = PR_NewLock();
            nep->pdlist = NULL;
            nep->finfo.pr.type = PR_FILE_OTHER;
            nep->finfo.pr.size = 0;
            nep->finfo.pr.creationTime = 0;
            nep->finfo.pr.modifyTime = 0;
            PRIntervalTime now = ft_timeIntervalNow();
            nep->finfo.lastUpdate = now;
            nep->finfo.fileid[0] = hvalue;
            nep->finfo.fileid[1] = nep->seqno;
            nep->finfo.prerr = 0;
            nep->finfo.oserr = 0;
            nep->hash = hvalue;
            nep->hitcnt = 0;
            nep->refcnt = 1;
            nep->flags = 0;
            nep->fHashed = 1;
            nep->fDelete = 0;
            nep->fWriting = 0;

            /* Add entry to cache instance hash table */
            NSFC_ASSERTBUCKETHELD(cip, bucket);
            nep->next = cip->hname[bucket];
            cip->hname[bucket] = nep;
            PR_AtomicIncrement((PRInt32*)&cip->curFiles);

            /* Add entry to the hit list */
            PR_Lock(cip->hitLock);
            PR_INIT_CLIST(&nep->hit_list);
            if (cip->cfg.hitOrder == PR_TRUE) {
                /*
                 * Add this entry towards the end of the hit list,
                 * but ahead of other entries with a zero hit count.
                 */
                PRCList *prev;
                NSFCEntryImpl *pnep;

                for (prev = PR_LIST_TAIL(&cip->hit_list);
                     prev != &cip->hit_list;
                     prev = PR_PREV_LINK(prev)) {

                    pnep = NSFCENTRYIMPL(prev);
                    if (pnep->hitcnt > nep->hitcnt) {
                        break; /* Our spot in the list */
                    }
                }

                PR_INSERT_AFTER(&nep->hit_list, prev);
            }
            else {
                /* Put new entry at head of hit list */
                PR_INSERT_LINK(&nep->hit_list, &cip->hit_list);
            }
            PR_Unlock(cip->hitLock);

            PR_ASSERT(!nep->fDelete);
        }
        else {
            /* Failed, so return the entry to the free list */
            PR_Lock(cip->namefLock);
            nep->next = cip->namefl;
            cip->namefl = nep;
            nep = NULL;
            PR_Unlock(cip->namefLock);

            cip->cacheFull = PR_TRUE; /* file cache is full */
            rfc = NSFC_NOSPACE;
        }
    }
    else {
        cip->cacheFull = PR_TRUE; /* file cache is full */
        rfc = NSFC_NOSPACE;
    }

    /* Cache contents have been modified */
    cip->sig++;

    return nep;
}

/*
 * _NSFC_IsTimeToReplace - indicate if a file cache entry should be replaced
 */
static PRBool
_NSFC_IsTimeToReplace(NSFCCache cip)
{
    /* replaceFiles = PR_FALSE means we never replace entries */
    if (cip->cfg.replaceFiles != PR_TRUE) {
        return PR_FALSE;
    }

    /* Don't replace entries unless the cache is full */
    if (cip->cacheFull != PR_TRUE && cip->curFiles < cip->cfg.maxFiles) {
        return PR_FALSE;
    }

    /* minReplace specifies the minimum time between file entry replacements */
    if (cip->cfg.minReplace != 0) {
        /* Bail if not enough time has elapsed since the last replacement */
        PRIntervalTime now = PR_IntervalNow();
        PRIntervalTime lastReplaced = cip->lastReplaced;
        if ((PRIntervalTime)(now - lastReplaced) < cip->cfg.minReplace) {
            return PR_FALSE;
        }

        /* Enough time has elapsed; were we the first to notice? */
        PRIntervalTime set = PR_AtomicSet((PRInt32 *)&cip->lastReplaced, now);
        if (set != lastReplaced) {
            return PR_FALSE;
        }

        cip->cacheFull = PR_FALSE;
    }

    return PR_TRUE;
}

static NSFCStatus
_NSFC_RejuvenateEntry(NSFCCache cache, NSFCEntryImpl *nep)
{
    PRFileInfo64 finfo;
    PRIntervalTime now;
    NSFCStatus rfc = NSFC_DELETED;

    if (nep->flags & NSFCENTRY_HASINFO) {

        now = ft_timeIntervalNow();

        if (NSFC_PR_GetFileInfo(nep->filename, &finfo) == PR_FAILURE) {
            if ((nep->flags & NSFCENTRY_ERRORINFO) &&
                (PR_GetError() == nep->finfo.prerr) &&
                (PR_GetOSError() == nep->finfo.oserr)) {
                nep->finfo.lastUpdate = now;
                rfc = NSFC_OK;
            }
        }
        else {
            if ((finfo.size == nep->finfo.pr.size) &&
                (finfo.creationTime == nep->finfo.pr.creationTime) &&
                (finfo.modifyTime == nep->finfo.pr.modifyTime)) {
                nep->finfo.lastUpdate = now;
                rfc = NSFC_OK;
            }
        }
    }

    if (rfc != NSFC_OK) {
        /* Couldn't rejuvenate, so mark this outdated entry for deletion */
        _NSFC_DeleteEntry(cache, nep, PR_FALSE, PR_TRUE, PR_TRUE);
    }

    return rfc;
}

/*
 * Assumptions:
 *
 *     Caller doesn't hold bucket lock for entry.
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_ReleaseEntry(NSFCCache cache, NSFCEntry *entry)
{
    NSFCEntryImpl* nep = *entry;

    /* Invalidate caller's handle */
    *entry = NULL;

    return _NSFC_ReleaseEntry(cache, nep, NULL);
}

static NSFCStatus
_NSFC_ReleaseEntry(NSFCCache cache, NSFCEntryImpl *nep, PRBool *destroyed)
{
    NSFCStatus rfc = NSFC_OK;

    PR_ASSERT(nep);

    if (destroyed)
        *destroyed = PR_FALSE;

    if (nep != NULL) {
        PR_ASSERT(nep->refcnt != 0);

        /* Lock nep's bucket */
        PRUint32 bucket = nep->hash % cache->hsize;
        NSFC_ACQUIREBUCKET(cache, bucket);

        /*
         * If the entry is marked for delete, and the use
         * count is decremented to zero, the entry can be
         * deleted.
         */
        if (--nep->refcnt == 0) {

            PR_ASSERT(nep->fWriting == 0);

            if (nep->fDelete) {
                NSFC_DestroyEntry(cache, nep);
                if (destroyed)
                    *destroyed = PR_TRUE;
            }
        }
        else if ((nep->flags & NSFCENTRY_WAITING) &&
                 (nep->refcnt == 1)) {
            /* required to notify someone who is waiting to 
               set private data */
            PR_CEnterMonitor((void*)nep);
            PR_CNotifyAll((void*)nep);
            PR_CExitMonitor((void*)nep);
        }

        /* Unlock nep's bucket */
        NSFC_RELEASEBUCKET(cache, bucket);

        rfc = NSFC_OK;
    }

    return rfc;
}

PR_IMPLEMENT(NSFCEntry)
NSFC_EnumEntries(NSFCCache cache, NSFCEntryEnumState *state)
{
    // If this is the first call...
    if (*state == NSFCENTRYENUMSTATE_INIT) {
        // Initialize the entry enumeration state
        *state = (NSFCEntryEnumState_s *)malloc(sizeof(NSFCEntryEnumState_s));
        if (!*state)
            return NULL;
        (*state)->bucket = 0;
        (*state)->offset = 0;
    }

    for (;;) {
        // Don't enumerate beyond the end of the file cache
        PR_ASSERT((*state)->bucket >= 0 && (*state)->bucket <= cache->hsize);
        if ((*state)->bucket == cache->hsize) {
            return NULL;
        }

        // Check the current bucket
        if (cache->hname[(*state)->bucket]) {
            NSFCEntryImpl *nep;
            int offset = 0;

            NSFC_ACQUIREBUCKET(cache, (*state)->bucket);

            // Walk the linked list, looking for an entry at the offset
            // where we left off the last time we checked this bucket
            for (nep = cache->hname[(*state)->bucket]; nep; nep = nep->next) {
                if (offset >= (*state)->offset) {
                    // Found a new entry
                    ++(*state)->offset;
                    if (!nep->fDelete) {
                        ++nep->refcnt;
                        break;
                    }
                }
                offset++;
            }

            NSFC_RELEASEBUCKET(cache, (*state)->bucket);

            if (nep) {
                // Return a reference to the entry we found
                return nep;
            }
        }

        // Find the next non-empty bucket
        ++(*state)->bucket;
        (*state)->offset = 0;
        while ((*state)->bucket < cache->hsize) {
            if (cache->hname[(*state)->bucket]) {
                break;
            }
            ++(*state)->bucket;
        }
    }
}

PR_IMPLEMENT(void)
NSFC_EndEntryEnum(NSFCEntryEnumState *state)
{
    if (*state != NSFCENTRYENUMSTATE_INIT) {
        free(*state);
    }
}
