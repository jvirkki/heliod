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

#ifndef __nsfc_pvt_h_
#define __nsfc_pvt_h_

/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#include <string.h>              /* for memset() */
#include <limits.h>
#include "xp/xpatomic.h"
#include "nsfc.h"

/* Internal handle for a cache entry */
typedef struct NSFCEntryImpl NSFCEntryImpl;

#ifdef XP_UNIX
#include "md_unix.h"
#endif /* XP_UNIX */

#ifdef XP_WIN32
#include "md_win32.h"
#endif /* XP_WIN32 */

PR_BEGIN_EXTERN_C

/* CONSTANTS */

/* default temporary sub-directory prefix */
#define NSFC_DEFAULT_TMP_SUBDIR "file-cache"

/* bit flags in NSFCEntryImpl flags field */
#define NSFCENTRY_HASINFO       0x1  /* file information is present */
#define NSFCENTRY_ERRORINFO     0x2  /* error getting info for filename */
#define NSFCENTRY_HASCONTENT    0x4  /* file content is present */
#define NSFCENTRY_CMAPPED       0x8  /* file content is mapped */
#define NSFCENTRY_OPENFD        0x10 /* open file descriptor is cached */
#define NSFCENTRY_TMPFILE       0x20 /* file has been copied to tmp file */
#define NSFCENTRY_WAITING       0x40 /* file has somebody waiting for ref 
                                        count to be 1 */
#define NSFCENTRY_CREATETMPFAIL 0x80 /* file didn't fit in tempDir */ 

/* number of times to retry NSPR calls on a stale file handle */
#define NSFC_ESTALE_RETRIES 16

/* MACROS */

/* Dynamic initialization of an NSFCEntry */
#define NSFCENTRY(e, c, n) (*e) = (n)

/* Get NSFCEntryImpl pointer from a pointer to its hit_list member */
#define NSFCENTRYIMPL(p) \
    (NSFCEntryImpl *)((char *)(p) - offsetof(NSFCEntryImpl, hit_list))

/* Acquire bucket lock */
#ifdef DEBUG
#define NSFC_ASSERTBUCKETHELD(cache, bucket) PR_ASSERT((cache)->bucketHeld[(bucket)] == 1)
#define NSFC_ACQUIREBUCKET(cache, bucket) NSFC_AcquireBucket((cache), (bucket))
#define NSFC_RELEASEBUCKET(cache, bucket) NSFC_ReleaseBucket((cache), (bucket))
#else
#define NSFC_ASSERTBUCKETHELD
#define NSFC_ACQUIREBUCKET(cache, bucket) PR_Lock((cache)->bucketLock[(bucket)])
#define NSFC_RELEASEBUCKET(cache, bucket) PR_Unlock((cache)->bucketLock[(bucket)])
#endif

/* TYPES */

typedef struct NSFCPrivateData NSFCPrivateData;

typedef enum {
    NSFCCache_Uninitialized,
    NSFCCache_Initializing,
    NSFCCache_Active,
    NSFCCache_Terminating,
    NSFCCache_TerminatingWait,
    NSFCCache_Shutdown,
    NSFCCache_Dead
} NSFCCacheState;

/*
 * NSFCCache - file cache instance
 *
 * This structure describes a file cache instance.
 */
struct NSFCCache_s {
    NSFCCache        next;       /* next instance on NSFCInstanceList */
    NSFCCacheState   state;      /* current state of this instance */
    NSFCCacheSignature sig;      /* changes when entries and private data are added, deleted, destroyed, or modified */
    PRUint32         hsize;      /* number of buckets in hname */
    PRMonitor       *monitor;    /* monitor for exclusive access */
    PRLock          *hitLock;    /* hit_list protection lock */
    PRLock          *namefLock;  /* namefl protection lock */
    PRLock          *keyLock;    /* key protection lock */
    PRLock         **bucketLock; /* protects hname buckets and the constituent NSFCEntryImpls' next, fDeleted, and refcnt  */
    PRInt32         *bucketHeld; /* set when a bucketLock is held */
    NSFCEntryImpl   *namefl;     /* free list of file name entries */
    NSFCEntryImpl  **hname;      /* hash table of file name entries */
    NSFCMemAllocFns *memfns;     /* pointer to memory allocation functions */
    void            *memptr;     /* handle for memory allocation functions */
    NSFCLocalMemAllocFns *local_memfns; /* pointer to per thread memory allocation functions */
    NSFCPrivDataKey  keys;       /* pointer to list of private data keys */
    PRUint32         lookups;    /* total number of cache lookups */
    PRUint32         hitcnt;     /* number of hits on cache entry lookups */
    PRUint32         infoHits;   /* number of hits on cached file info */
    PRUint32         ctntHits;   /* number of hits on cached file contents */
    PRUint32         infoMiss;   /* number of file info misses */
    PRUint32         ctntMiss;   /* number of content misses */
    PRUint32         rplcCnt;    /* number of entries deleted to make room */
    PRUint32         outdCnt;    /* number of outdated entries deleted */
    PRUint32         delCnt;     /* total number of entries deleted */
    PRUint32         busydCnt;   /* number of entries in use marked fDelete */
    PRUint32         curFiles;   /* current number of cached files */
    XPUint64         curHeap;    /* current heap space used */
    XPUint64         curMmap;    /* current VM mapped */
    PRUint32         curOpen;    /* current number of cached fds */
    NSFCCacheConfig  cfg;        /* cache instance config parameters */
    PRCList          hit_list;   /* hit list: head is mru, tail is lru */
    PRIntervalTime   lastReplaced;  /* last time we tried to replace entry */
    PRBool           cacheFull;  /* set when cache is full */
};

/*
 * NSFCEntryImpl - filename hash table entry
 *
 * This structure describes an entry in a cache instance hnames hash
 * table.
 */
struct NSFCEntryImpl {
    NSFCEntryImpl   *next;        /* next entry on hash collision list */
    PRCList          hit_list;    /* hit list links */
    char            *filename;    /* name of cached file */
    PRLock          *pdLock;      /* pdlist protection lock */
    NSFCPrivateData *pdlist;      /* list of private data items */
    NSFCFileInfo     finfo;       /* file information */
    NSFCMDEntry      md;          /* machine-dependent area */
    PRUint32         hash;        /* hash value for filename */
    PRUint32         seqno;       /* sequence number */
    PRUint32         hitcnt;      /* hit count */
    PRUint32         refcnt;      /* reference count */
    PRIntn           flags;       /* entry content bit flags */
    PRInt32          fWriting;    /* entry is being updated */
    unsigned         fHashed:1;   /* entry is in cache hash table */
    unsigned         fDelete:1;   /* delete requested for entry */
};

/*
 * NSFCPrivDataKey_s - private data key definition
 *
 * This structure defines a private data key for a cache instance.  Each
 * cache instance has a linked list of the private data keys that have
 * been created.
 */
struct NSFCPrivDataKey_s {
    NSFCPrivDataKey       next;     /* pointer to next key on list */
    NSFCPrivDataDeleteFN  delfn;    /* private data deletion callback */
};

/*
 * NSFCPrivateData - private data list element
 *
 * This structure defines an element of a list of private data pointers
 * that can be associated with each filename entry.  The element includes
 * the private data key, which is used to retrieve or set the private
 * data pointer.
 */
struct NSFCPrivateData {
    NSFCPrivateData *next;    /* pointer to next element */
    NSFCPrivDataKey key;      /* pointer to private data key definition */
    void *pdata;              /* pointer to private data */
};

/*
 * NSFCEntryEnumState_s - private entry enumeration state
 */
struct NSFCEntryEnumState_s {
    int bucket;
    int offset;
};

/* GLOBAL VARIABLES */

/* defined in init.cpp */
PR_EXTERN_DATA(PRMonitor *) NSFCMonitor;
PR_EXTERN_DATA(PRBool) NSFCTerminating;

/* defined in instance.cpp */
PR_EXTERN(void) NSFC_EnterCacheMonitor(NSFCCache cache);
PR_EXTERN(NSFCStatus) NSFC_ExitCacheMonitor(NSFCCache cip);

/* FUNCTIONS */

/* defined in alloc.cpp */
PR_EXTERN(void) NSFC_InitializeAlloc(NSFCCache cip);
PR_EXTERN(PRBool) NSFC_ReserveMmap(NSFCCache cip, PRInt32 size);
PR_EXTERN(void) NSFC_ReleaseMmap(NSFCCache cip, PRInt32 size);
PR_EXTERN(void *) NSFC_Calloc(PRUint32 nelem, PRUint32 elsize, NSFCCache cip);
PR_EXTERN(void) NSFC_Free(void *ptr, PRUint32 nbytes, NSFCCache cip);
PR_EXTERN(void) NSFC_FreeStr(char *str, NSFCCache cip);
PR_EXTERN(void *) NSFC_Malloc(PRUint32 nbytes, NSFCCache cip);
PR_EXTERN(char *) NSFC_Strdup(const char *str, NSFCCache cip);

/* defined in filecopy.cpp */
PR_EXTERN(PRStatus) NSFC_MakeDirectory(char *dirname, NSFCCache cip);
PR_EXTERN(PRStatus) NSFC_MakeTempCopy(const char *infile, char *outfile,
                                      NSFCCache cip, PRInt64 *size);

PR_EXTERN(PRFileDesc *) NSFC_OpenTempCopy(NSFCCache cip, 
                                          NSFCEntryImpl *nep,
                                          NSFCStatusInfo *statusInfo);

PR_EXTERN(PRFileDesc *) NSFC_OpenEntryFile(NSFCEntryImpl *nep,
                                      NSFCCache cache,
                                      NSFCStatusInfo *statusInfo);


/* defined in filename.cpp */
PR_EXTERN(void) NSFC_AcquireBucket(NSFCCache cache, int bucket);

PR_EXTERN(void) NSFC_ReleaseBucket(NSFCCache cache, int bucket);

PR_EXTERN(NSFCStatus) NSFC_AcquireEntryWriteLock(NSFCCache cip,
                                                 NSFCEntryImpl *nep);

PR_EXTERN(void) NSFC_ReleaseEntryWriteLock(NSFCCache cip, NSFCEntryImpl *nep);

PR_EXTERN(NSFCEntryImpl *) NSFC_ActivateEntry(const char *filename, 
                                              NSFCEntry entry,
                                              PRUint32 &hval, 
                                              NSFCStatus &rfc,
                                              NSFCCache cache);
PR_EXTERN(NSFCStatus) NSFC_DeleteEntry(NSFCCache cache,
                                       NSFCEntryImpl *nep,
                                       PRBool hasBucketLock);
PR_EXTERN(void) NSFC_DestroyEntry(NSFCCache cip, NSFCEntryImpl *nep);
PR_EXTERN(PRUint32) NSFC_HashFilename(const char *filename);

PR_EXTERN(NSFCEntryImpl *) NSFC_NewFilenameEntry(NSFCCache cip, 
                                              const char *filename,
                                              PRUint32 hvalue, 
                                              NSFCStatus &rfc);

PR_IMPLEMENT(void) NSFC_RecordEntryHit(NSFCCache cache, NSFCEntry entry);

/* defined in md_win32.cpp or md_unix.cpp */
PR_EXTERN(PRIntn) NSFC_CmpFilename(const char *fn1, const char *fn2);

/* defined in fileio.cpp */

PR_EXTERN(PRStatus) NSFC_PR_GetFileInfo(const char *filename,
                                        PRFileInfo64 *finfo);

PR_EXTERN(PRFileDesc*) NSFC_PR_Open(const char *name,
                                    PRIntn flags,
                                    PRIntn mode);

PR_EXTERN(PRInt64) NSFC_ReadWriteFile(PRFileDesc *socket,
                                      PRFileDesc *fd,
                                      NSFCFileInfo *finfo,
                                      const void *headers, PRInt32 hdrlen,
                                      const void *trailers, PRInt32 tlrlen,
                                      PRIntervalTime timeout,
                                      NSFCCache cache,
                                      NSFCStatusInfo *statusInfo);

PR_EXTERN(PRInt32) NSFC_PR_SendFile(PRFileDesc *socket,
                                    PRFileDesc *fd,
                                    NSFCFileInfo *finfo,
                                    const void *headers, PRInt32 hdrlen,
                                    const void *trailers, PRInt32 tlrlen,
                                    PRIntervalTime timeout,
                                    PRInt32 sendfileSize,
                                    NSFCStatusInfo *statusInfo);

PR_EXTERN(PRBool)
NSFC_isSizeOK(NSFCCache cache, NSFCFileInfo* finfo, 
              PRInt32 hdrlen, PRInt32 tlrlen, PRInt32& sendfileSize);

PR_END_EXTERN_C

#endif /* __nsfc_pvt_h_ */
