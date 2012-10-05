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
 * Provide intra-process and inter-process RW locking for
 * DB_HASH implementation of the database.
 * Locking is enabled by specifying DB_LOCK field as one of the flags
 * during dbopen(). This will automatically enable inter-process locking.
 * For intra-process locking, one should call dbinit() prior to the first
 * dbopen() and dbdestroy() after closing the last database file.
 * $revision$
 * $author$
 */

#ifdef WIN32
#include <Windows.h>
#include <io.h>
#endif

#include <string.h>
#include <errno.h>
#include "nspr.h"              /* NSPR threads declarations */
#include "plhash.h"
#include "mcom_db.h"
#include "hash.h"

#define DEFAULT_SLEEP_TIMEOUT 10 /* 10ms to sleep */
#define DEFAULT_RETRY_COUNT 6000  /* 6000 * 10 = 1 minute */

/* Global lock for accessing the hashtable */
static PRLock* hashlock = NULL;

/* Hashtable {pathname, hashlock} to access a database file */
/* The entry is not removed when a lockdb goes away */
static PRHashTable* filehash = NULL;

/* HashPermAllocOps - allocation ops passed to the PLHash routines for 
 * pool allocation
 */

void *
Hash_PermAllocTable(void *pool, PRSize size)
{
    return malloc(size);
}

void
Hash_PermFreeTable(void *pool, void *item)
{
    free(item);
}

PRHashEntry *
Hash_PermAllocEntry(void *pool, const void *unused)
{
    return (PRHashEntry *)malloc(sizeof(PRHashEntry));
}

void
Hash_PermFreeEntry(void *pool, PRHashEntry *he, uintn flag)
{
    if (flag == HT_FREE_ENTRY) {
        free(he->key);
        PR_DestroyRWLock((PRRWLock*)he->value);
	free(he);
    }
}

static const PRHashAllocOps HashPermAllocOps = {
    Hash_PermAllocTable,
    Hash_PermFreeTable,
    Hash_PermAllocEntry,
    Hash_PermFreeEntry
};

int 
__hash_init()
{
    hashlock = PR_NewLock();
    PR_ASSERT(hashlock != NULL);
    filehash = PR_NewHashTable(0, PR_HashString, PR_CompareStrings, 
                               PR_CompareValues, &HashPermAllocOps, NULL);

    return 0;
}

int
__hash_destroy()
{
    if (filehash) {
        PR_HashTableDestroy(filehash);
        filehash = NULL;
    }
    if (hashlock) {
        PR_DestroyLock(hashlock);
        hashlock = NULL;
    }

    return 0;
}

int 
__hash_lock(HTAB* hashp, int mode)
{

    PRRWLock* threadlock = NULL;
    int status = PR_FAILURE;

    /* If it was not opened with DB_LOCK, there is nothing to do */
    if (!(hashp->flags & DB_LOCK))
        return 0;

    if (!((mode == HASH_SHARED_LOCK) || 
          (mode == HASH_EXCLUSIVE_LOCK)))
        return EINVAL;

    /* If already locked with Exclusive, nothing to do */
    if (hashp->lockmode == HASH_EXCLUSIVE_LOCK) {
        hashp->lockcnt++;
        return 0;
       
    }
    else if (hashp->lockmode == HASH_SHARED_LOCK) {
        if (mode == HASH_EXCLUSIVE_LOCK)
            return EINVAL;
        hashp->lockcnt++;
        return 0;
    }
    
    if (hashlock) {
        PR_Lock(hashlock);
        threadlock = (PRRWLock*)PR_HashTableLookup(filehash, 
                                                   (char*)hashp->filename);
        if (!threadlock) {
            char* fname = strdup(hashp->filename);
            threadlock = PR_NewRWLock(0, "ReadWriteLock");
            PR_HashTableAdd(filehash, fname, threadlock);
        }
        PR_Unlock(hashlock);        
    }

    /* Lock the other threads/processes out before proceeding further */
#ifdef WIN32
    {
        DWORD flags; 
        const DWORD len = 0xffffffff;
        OVERLAPPED offset;
        HANDLE handle;

        /* synchronize against all threads in this process */
        if (threadlock) {
            if (mode == HASH_SHARED_LOCK)
                PR_RWLock_Rlock(threadlock);
            else
                PR_RWLock_Wlock(threadlock);
        }

        flags = (mode == HASH_SHARED_LOCK) ? 0 : LOCKFILE_EXCLUSIVE_LOCK;
        /* Syntax is correct, len is passed for LengthLow and LengthHigh*/
        memset (&offset, 0, sizeof(offset));

        /* Retrieve the OS specific handle from the fd */
        handle = _get_osfhandle(hashp->fp);
        if (handle == -1 || 
            !LockFileEx(handle, flags, 0, len, len, &offset)) {
            /* Release the threadlock */
            if (threadlock) 
                PR_RWLock_Unlock(threadlock);
            return -1;
        }
        status = PR_SUCCESS; /* success */
    }
#else
    {
        struct flock l = { 0 };
        int fc, rc;
        int i;

        for (i = 0; i < DEFAULT_RETRY_COUNT; i++) {

            /* synchronize against all threads in this process */
            if (threadlock) {
                if (mode == HASH_SHARED_LOCK)
                    PR_RWLock_Rlock(threadlock);
                else
                    PR_RWLock_Wlock(threadlock);
            }

            l.l_whence = SEEK_SET;  /* lock from current point */
            l.l_start = 0;          /* begin lock at this offset */
            l.l_len = 0;            /* lock to end of file */
            if (mode == HASH_SHARED_LOCK)
                l.l_type = F_RDLCK;
            else
                l.l_type = F_WRLCK;

            /* Block until a lock can be obtained */
            fc = F_SETLKW;

            /* keep trying if fcntl() gets interrupted (by a signal) */
            while ((rc = fcntl(hashp->fp, fc, &l)) < 0 && (errno == EINTR))
                continue;

            if (rc == -1) {
                if (errno == EDEADLK) {
                    PRIntervalTime sleeptime =PR_MillisecondsToInterval(DEFAULT_SLEEP_TIMEOUT);
                    /* Release the threadlock */
                    if (threadlock) 
                        PR_RWLock_Unlock(threadlock);

                    /* Let other threads acquire the lock */
                    PR_Sleep(sleeptime);
                }
                /* Other errors are considered fatal, release the thread 
                 *  lock 
                 */
                else {
                    if (threadlock) 
                        PR_RWLock_Unlock(threadlock);
                    return errno;
                }
            }
            else {
                status = PR_SUCCESS;
                break;
            }
        }
    }
#endif        
    if (status == PR_SUCCESS) {
        /* Initialize the values appropriately */
        hashp->lockcnt++;
        hashp->lockmode = mode;
        return 0;
    }

    return -1;

}

int 
__hash_unlock(HTAB* hashp)
{

    /* If it was not opened with DB_LOCK, there is nothing to do */
    if (!(hashp->flags & DB_LOCK))
        return 0;

    if (--hashp->lockcnt != 0)
        return 0;
#ifdef WIN32
    {
        DWORD len = 0xffffffff;
        /* Syntax is correct, len is passed for LengthLow and LengthHigh*/
        OVERLAPPED offset;
        HANDLE handle;

        memset (&offset, 0, sizeof(offset));
        /* Retrieve the OS specific handle from the fd */
        handle = _get_osfhandle(hashp->fp);
        if (handle == -1 || 
            !UnlockFileEx(handle, 0, len, len, &offset)) {
            return -1;
        }
    }
#else
    {
        struct flock l = { 0 };
        int rc;

        l.l_whence = SEEK_SET;  /* lock from current point */
        l.l_start = 0;          /* begin lock at this offset */
        l.l_len = 0;            /* lock to end of file */
        l.l_type = F_UNLCK;
    
        /* keep trying if fcntl() gets interrupted (by a signal) */
        while ((rc = fcntl(hashp->fp, F_SETLKW, &l)) < 0
               && errno == EINTR)
            continue;
        
        if (rc == -1)
            return errno;
    }
#endif

    /* synchronize against all threads in this process */
    if (hashlock) {
        PRRWLock* threadlock = NULL;

        PR_Lock(hashlock);
        threadlock = (PRRWLock*)PR_HashTableLookup(filehash, 
                                                   (char*)hashp->filename);
        PR_Unlock(hashlock);

        if (threadlock) 
            PR_RWLock_Unlock(threadlock);
    }
    hashp->lockmode = 0;
    return 0;
}
