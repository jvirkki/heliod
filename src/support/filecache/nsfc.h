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

#ifndef __nsfc_h_
#define __nsfc_h_

/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#include "nspr.h"

PR_BEGIN_EXTERN_C

/* CONSTANTS */

/* File cache API version numbers */
#define NSFC_API_VERSION        72 /* Current API version number */
#define NSFC_API_VERSION_MIN    71 /* Minimum version number supported */
#define NSFC_API_VERSION_MAX    72 /* Maximum version number supported */

/* TYPES */

typedef enum NSFCStatus {
    NSFC_OK        =  0,          /* successful completion */
    NSFC_NSPRERROR = -1,          /* NSPR error */
    NSFC_NOTFOUND  = -2,          /* matching entry not found */
    NSFC_NOFINFO   = -3,          /* matching entry has no finfo */ 
    NSFC_DELETED   = -4,          /* referenced entry is marked for delete */
    NSFC_NOSPACE   = -5,          /* insufficient space in cache */
    NSFC_BUSY      = -6,          /* write access busy on entry */
    NSFC_DEADCACHE = -8,          /* cache shutdown or destroyed */
    NSFC_STATFAIL  = -9           /* can't stat to get finfo */
} NSFCStatus;

typedef enum NSFCStatusInfo {
    NSFC_STATUSINFO_NONE               = 0,
    NSFC_STATUSINFO_CREATE             = 1,  /* new entry created */
    NSFC_STATUSINFO_STAT               = 2,  /* stat returned */
    NSFC_STATUSINFO_DELETED            = 3,  /* fDelete set */
    NSFC_STATUSINFO_BADCALL            = 4,  /* invalid call argument */

    /* some status info for transmit file */
    NSFC_STATUSINFO_BUSY               = 6,
    NSFC_STATUSINFO_FILESIZE           = 7,
    NSFC_STATUSINFO_OPENFAIL           = 8,
    NSFC_STATUSINFO_STATFAIL           = 9,
    NSFC_STATUSINFO_DELETETMPFAIL      = 10,
    NSFC_STATUSINFO_TMPUSEREALFILE     = 11, 
    NSFC_STATUSINFO_CREATETMPFAIL      = 12 

} NSFCStatusInfo;

#define NSFCSTATUSINFO_INIT(_si) \
{ \
    if (_si) { \
        (*(_si)) = NSFC_STATUSINFO_NONE; \
    } \
}
#define NSFCSTATUSINFO_SET(_si, _v) \
{\
        if (_si) { \
            (*(_si)) = (_v); \
        } \
}

typedef enum NSFCAsyncStatus {
    NSFC_ASYNCSTATUS_DONE = 0,        /* transmission complete */
    NSFC_ASYNCSTATUS_AGAIN = -1,      /* transmission incomplete, call again */
    NSFC_ASYNCSTATUS_IOERROR = -2,    /* network IO error */
    NSFC_ASYNCSTATUS_WOULDBLOCK = -3  /* can't be transmitted asynchronously */
} NSFCAsyncStatus;

/*
 * TYPE: NSFCEntryEnumState - entry enumeration state
 *
 * This type preserves entry enumeration state across calls to
 * NSFC_EnumEntries().  It must be initialized to NSFCENTRYENUMSTATE_INIT.
 * It must be destroyed by a call to NSFC_EndEntryEnum().
 */
typedef struct NSFCEntryEnumState_s *NSFCEntryEnumState;

/* Initializer for NSFCEntryEnumState */
#define NSFCENTRYENUMSTATE_INIT 0

/*
 * TYPE: NSFCCacheSignature - token for file cache instance state
 *
 * This type represents the state of a file cache instance.  The state
 * changes whenever entries and private data are added or removed.
 */
typedef PRUint64 NSFCCacheSignature;

/*
 * TYPE: NSFCCache - handle for a file cache instance
 *
 * This type is a pointer to a structure that represents a cache instance.
 * The contents of the structure are private to the cache implementation,
 * and are defined in nsfc_pvt.h.
 *
 * A handle for a cache is obtained when a cache instance is created,
 * using NSFC_CreateCache().
 *
 * The handle continues to be valid when the cache instance is shut
 * down, using NSFC_ShutdownCache(), and reactivated, using
 * NSFC_StartupCache().  A client of a cache can request notifications
 * of shutdown and startup events, using NSFC_MonitorCache().  The
 * current state of the cache can be retrieved using NSFC_GetCacheState().
 *
 * A null cache handle is permitted on operations which can be performed
 * even when caching is disabled, and simply indicates the the operation
 * should be performed without using a cache.  Examples of this are
 * NSFC_GetFileInfo() and NSFC_TransmitFile().
 */
typedef struct NSFCCache_s *NSFCCache;

/*
 * TYPE: NSFCEntry - handle for a cache entry
 *
 * This type is an opaque structure that contains a reference to a cache
 * entry.  The structure is allocated by the client, and initialized via
 * the initializer defined by the NSFCENTRY_INIT macro.  The NSFCEntry
 * structure must be initialized before being used for the first time in
 * NSFC calls.
 *
 * A reference to a cache entry can be in one of two states:
 *
 *     1. Invalid
 *
 *        This is the state achieved by the NSFCENTRY_INIT
 *        initializer.
 *
 *     2. Valid
 *
 *        This state is entered when the handle is set up to
 *        reference an existing cache entry, for example, using
 *        NSFC_LookupFilename() or NSFC_AccessFilename().  A
 *        handle in this state represents an increment to the
 *        use count of the corresponding cache entry.  A valid
 *        handle should ultimately be passed to NSFC_ReleaseEntry(),
 *        which will decrement the use count of the cache entry,
 *        and set the handle state to Invalid.
 */
typedef struct NSFCEntryImpl *NSFCEntry;

/* Initializer for NSFCEntry */
#define NSFCENTRY_INIT ((NSFCEntry)0)

#define NSFCENTRY_ISVALID(entry) (*(entry) != (NSFCEntry)0)

/*
 * TYPE: NSFCCacheConfig - cache instance configuration parameters
 *
 * This type defines a structure that is used to pass configuration
 * parameters for creation of a cache instance to NSFC_CreateCache().
 * It is allocated by the client, and can be freed after calling
 * NSFC_CreateCache(), since the information is copied into internal
 * data structures.
 */
typedef struct NSFCCacheConfig NSFCCacheConfig;
struct NSFCCacheConfig {
    PRIntn version;       /* NSFC API version number */

    PRBool cacheEnable;   /* PR_FALSE: all caching disabled */
                          /* PR_TRUE: enable caching subject to other flags */
    PRBool dirmonEnable;  /* PR_FALSE: do not run dirmon thread */
                          /* PR_TRUE: run dirmon thread */
    PRBool contentCache;  /* PR_FALSE: disable caching of file contents/fds */
                          /* PR_TRUE: enable caching of file contents/fds */
    PRBool copyFiles;     /* PR_FALSE: do not copy files to tempDir */
                          /* PR_TRUE: copy cached files to tempDir */
    PRBool useSendFile;   /* PR_FALSE: do not use PR_SendFile() or cache fds */
                          /* PR_TRUE: use PR_SendFile(), cache fds */
    PRBool replaceFiles;  /* PR_FALSE: file entries are never replaced */
                          /* PR_TRUE: reuse existing entries for new files */
    PRBool hitOrder;      /* PR_FALSE: replace LRU entry whenever possible */
                          /* PR_TRUE: replace least hit entry */

    PRIntervalTime maxAge;      /* maximum age of a valid cache entry */
    PRIntervalTime dirmonPoll;  /* dirmon polling interval */
    PRIntervalTime minReplace;  /* minimum time between entry replacements */

    PRUint32 maxFiles;      /* maximum number of files allowed in cache */
    PRUint64 maxHeap;       /* maximum heap space to use for file contents */
    PRUint64 maxMmap;       /* maximum VM to use for mapped file contents */
    PRUint32 maxOpen;       /* maximum cached open file descriptors */
    PRUint32 limSmall;      /* maximum size of a "small" sized file */
    PRUint32 limMedium;     /* maximum size of a "medium" sized file */
    PRUint32 hashInit;      /* initial number of hash buckets */
    PRUint32 hashMax;       /* maximum number of hash buckets */
    PRUint32 bufferSize;    /* size of file IO buffers */
    PRUint32 sendfileSize;  /* Max bytes for content in PR_SendFile call */

    char *instanceId;       /* server instance id */
    char *tempDir;          /* pathname of directory for temporary files */
};

/*
 * TYPE: NSFCCacheEnumerator - cache instance enumeration state
 */
typedef struct NSFCCacheEnumerator NSFCCacheEnumerator;
struct NSFCCacheEnumerator {
    NSFCCache next;        /* pointer to next in list */
};

/*
 * TYPE: NSFCFileInfo - file information structure
 *
 * This structure contains information about a file, returned by
 * NSFC_GetFileInfo().  This information also may be passed back into various
 * file cache routines, which may accelerate their operation.
 */
typedef struct NSFCFileInfo NSFCFileInfo;
struct NSFCFileInfo {
    PRFileInfo64 pr;           /* file information from NSPR */
    PRIntervalTime lastUpdate; /* time of last update of cached information */
    PRUint32 fileid[2];        /* file id value */
    PRErrorCode prerr;         /* NSPR error code */
    PRInt32 oserr;             /* native OS error code */
};

/*
 * TYPE: NSFCGlobalConfig - module configuration parameters
 *
 * This structure is used to pass NSFC module initialization
 * parameters to NSFC_Initialize().
 */
typedef struct NSFCGlobalConfig NSFCGlobalConfig;
struct NSFCGlobalConfig {
    PRIntn version;       /* NSFC API version number */

};


/*
 * TYPE: NSFCLocalMemAllocFns - memory allocation functions
 *
 * Memory allocation functions for per thread use may be set for
 * each file cache instance.
 */
typedef void *(PR_CALLBACK *LocalMemoryAllocateFN)(int size);
typedef void (PR_CALLBACK *LocalMemoryFreeFN)(void *ptr);
typedef struct NSFCLocalMemAllocFns NSFCLocalMemAllocFns;
struct NSFCLocalMemAllocFns {
    LocalMemoryAllocateFN alloc;
    LocalMemoryFreeFN free;
};

/*
 * TYPE: NSFCMemAllocFns - memory allocation functions
 *
 * Memory allocation functions may be set for each file cache instance.
 * If none are specified, malloc() and free() are used.
 */
typedef void *(PR_CALLBACK *MemoryAllocateFN)(void *handle, PRUint32 nbytes);
typedef void (PR_CALLBACK *MemoryFreeFN)(void *handle, void *ptr);
typedef struct NSFCMemAllocFns NSFCMemAllocFns;
struct NSFCMemAllocFns {
    MemoryAllocateFN alloc;
    MemoryFreeFN free;
};

/*
 * TYPE:  NSFCPrivDataKey - private data key value
 *
 * Private data keys are used to associate client information with a
 * cache entry.  This type is a pointer to an internal structure that
 * contains information about a particular type of private data.
 * This information is provided when a private data key is created
 * via a call to NSFC_NewPrivateDataKey().  Private data keys are
 * not destroyed when a cache is shutdown (via NSFC_ShutdownCache()).
 */
typedef struct NSFCPrivDataKey_s *NSFCPrivDataKey;

/*
 * TYPE: NSFCPrivDataDeleteFN - private data deletion callback function
 *
 * This type defines the prototype for a function to be called when a
 * private data pointer associated with a cache entry is modified or
 * deleted.  This function is specified when a private data key is
 * created (via NSFC_NewPrivateDataKey()).  The expected use is that
 * the delete callback will free dynamic memory associated with the
 * private data that is being deleted.
 *
 * The callback function receives a handle for the cache, the filename
 * associated with the cache entry, the private data key under which
 * the data is stored, and a pointer to the private data itself.
 */
typedef void (PR_CALLBACK *NSFCPrivDataDeleteFN)(NSFCCache cache,
                                                 const char *filename,
                                                 NSFCPrivDataKey key,
                                                 void *privateData);
/*
 * TYPE: NSFCNativeSocketDesc - operating system socket descriptor
 *
 * This is the operating system's socket descriptor type.  Most NSFC
 * functions operate on platform-independent NSPR file desciptors.  The
 * NSFC_TransmitAsync function, however, requires an operating system
 * socket descriptor.  On Unix, this is int.  NSFC_TransmitAsync is
 * currently a no-op on Windows.
 */
typedef int NSFCNativeSocketDesc;

/* FUNCTION DECLARATIONS */

/*
 * FUNCTION: NSFC_AccessFilename - get cache entry handle for a filename
 *
 * This function finds or creates a cache entry for a specified filename
 * in a specified cache instance.  If a handle for this cache entry is
 * returned successfully, the cache entry cannot be deleted until it is
 * released, using NSFC_ReleaseEntry().  Another function,
 * NSFC_LookupFilename(), will return a handle for an existing cache
 * entry, but not create a new one.
 *
 *      filename - filename for which a cache entry handle is needed
 *      entry - pointer to handle for cache entry
 *      cache - file cache instance handle, or NULL for default
 */
PR_EXTERN(NSFCStatus) NSFC_AccessFilename(const char *filename,
                                          NSFCEntry *entry,
                                          NSFCFileInfo *efinfo,
                                          NSFCCache cache,
                                          NSFCStatusInfo *statusInfo);

/*
 * FUNCTION: NSFC_CreateCache - create a file cache instance
 *
 * This function creates a new file cache instance, and returns a handle
 * for it.  Each file cache instance may have its own caching policies
 * and temporary file directory.  The cache instance created may either 
 * in NSFCCache_Uninitialized or NSFCCache_Active state
 *
 *      ccfg - pointer to configuration parameters for this cache instance
 *      memfns - pointer to memory allocation functions
 *      memptr - handle to be passed to memory allocation functions
 *      local_memfns - pointer to per thread memory allocation functions
 *      cache - returned handle for new file cache instance
 */
PR_EXTERN(PRStatus) NSFC_CreateCache(NSFCCacheConfig *ccfg,
                                     NSFCMemAllocFns *memfns,
                                     void *memptr,
                                     NSFCLocalMemAllocFns *local_memfns,
                                     NSFCCache *cache);

/*
 * FUNCTION: NSFC_InitializeCache - initialize a cache instance created
 * by NSFC_CreateCache which is in NSFCCache_Uninitialized state
 *
 *      ccfg - pointer to configuration parameters for this cache instance
 *      cache - handle for the cache instance
 */
PR_EXTERN(PRStatus) NSFC_InitializeCache(NSFCCacheConfig *ccfg, NSFCCache cache);


/*
 * FUNCTION: NSFC_UpdateCacheMaxOpen - Update the max open of a cache
 * instance created by NSFC_CreateCache which is not in 
 * NSFCCache_Uninitialized state
 *
 *      ccfg - pointer to configuration parameters for this cache instance
 *      cache - handle for the cache instance
 */
PR_EXTERN(PRStatus) NSFC_UpdateCacheMaxOpen(NSFCCacheConfig *ccfg, NSFCCache cache);

/*
 * FUNCTION: NSFC_EnterGlobalMonitor - enter the file cache module monitor
 *
 * This function enters the monitor associated with the file cache
 * module.  The same thread may enter the monitor more than once,
 * but only one thread may enter at a time.
 */
PR_EXTERN(void) NSFC_EnterGlobalMonitor(void);

/*
 * FUNCTION: NSFC_ExitGlobalMonitor - exit the file cache module monitor
 *
 * This function exits the monitor associated with the file cache
 * module, previously entered via NSFC_EnterGlobalMonitor().  The
 * thread must call NSFC_ExitGlobalMonitor() the same number of times
 * as it has called NSFC_EnterGlobalMonitor() before another thread can
 * enter the monitor.
 */
PR_EXTERN(PRStatus) NSFC_ExitGlobalMonitor(void);

/*
 * FUNCTION: NSFC_GetEntryPrivateData - get private data for entry and key
 *
 * This function retrieves a pointer to caller private data previously
 * set by a call to NSFC_SetEntryPrivateData().  It fails if
 * NSFC_SetEntryPrivateData() has not been called for the entry and key.
 *
 *      entry - handle for cache entry
 *      key - private data key used in call to NSFC_SetEntryPrivateData()
 *      pdata - pointer to where private data pointer is returned
 *      cache - file cache instance handle
 */
PR_EXTERN(NSFCStatus) NSFC_GetEntryPrivateData(NSFCEntry entry,
                                               NSFCPrivDataKey key,
                                               void **pdata, NSFCCache cache);

/*
 * FUNCTION: NSFC_Initialize - initial file cache module
 *
 * This function should be called once to initialize the file cache module,
 * prior to creating any file cache instances.
 *
 *      config - pointer to module configuration parameters
 *      vmin - returned minimum API version number supported
 *      vmax - returned maximum API version number supported
 */
PR_EXTERN(PRStatus) NSFC_Initialize(const NSFCGlobalConfig *config,
                                    PRIntn *vmin, PRIntn *vmax);

/*
 * FUNCTION: NSFC_IsCacheActive - check whether a cache is active
 *
 * This function searches the list of known cache instances for a
 * specified instance, and checks whether the instance is active.
 */
PR_EXTERN(PRBool) NSFC_IsCacheActive(NSFCCache cache);

PR_EXTERN(NSFCCache) NSFC_FirstCache(NSFCCacheEnumerator *cenum);
PR_EXTERN(NSFCCache) NSFC_NextCache(NSFCCacheEnumerator *cenum);

/*
 * FUNCTION: NSFC_GetFileInfo - get file information through the file cache
 *
 * This function returns information about a named file.  The function will
 * use cached information if possible or cause the retrieved file information
 * to be cached for future use.
 *
 *      name - filename for which information is to be retrieved
 *      finfo - pointer to structure to contain returned information
 *      cache - file cache instance handle
 */
PR_EXTERN(PRStatus) NSFC_GetFileInfo(const char *name, 
                                     NSFCFileInfo *finfo,
                                     NSFCCache cache);

/*
 * FUNCTION: NSFC_GetEntryFileInfo - get a cache entry's finfo
 *
 *      entry - the cache entry handle for which its finfo is to be retrieved
 *      finfo - pointer to structure to contain returned information
 *      cache - file cache instance handle
 */
PR_EXTERN(NSFCStatus) NSFC_GetEntryFileInfo(NSFCEntry entry, 
                                            NSFCFileInfo *finfo,
                                            NSFCCache cache);

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
 * Note: Some care must be taken with the NSFCEntry returned by this
 * function when passing it to other NSFC functions.  Some functions
 * may modify the NSFCEntry, which will make it incorrect to use the
 * same handle when calling NSFC_ReleaseEntry().  In general, it is
 * recommended that a copy of the NSFCEntry be passed to functions
 * other than NSFC_ReleaseEntry(), to avoid problems.
 */
PR_EXTERN(NSFCStatus) NSFC_LookupFilename(const char *filename,
                                          NSFCEntry *handle,
                                          NSFCCache cache);

/*
 * FUNCTION: NSFC_NewPrivateDataKey - allocate a private data key
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
PR_EXTERN(NSFCPrivDataKey) NSFC_NewPrivateDataKey(NSFCCache cache,
                                                  NSFCPrivDataDeleteFN delfn);

/*
 * FUNCTION: NSFC_ReleaseEntry - release a cache entry handle
 *
 * This function is called to release a cache entry handle that was
 * previously returned by another function.  It causes the use count
 * of the corresponding cache entry to be decremented, which may have
 * a side effect of allowing the entry to be deleted, if it has
 * previously been marked for delete.  All cache entry handles passed
 * to this function should be Valid or Invalid, i.e. always initialized
 * somehow.  This function will return the handle in the Invalid state.
 */
PR_EXTERN(NSFCStatus) NSFC_ReleaseEntry(NSFCCache cache, NSFCEntry *entry);

/*
 * FUNCTION: NSFC_SetEntryPrivateData - store private data for a cache entry
 *
 * This function stores a pointer to caller private data under a previously
 * created private data key, and associates it with a specified file.
 *
 *      entry - handle for cache entry
 *      key - private data key
 *      pdata - caller private data pointer
 *      cache - file cache instance handle, or NULL for default
 */
PR_EXTERN(NSFCStatus) NSFC_SetEntryPrivateData(NSFCEntry entry,
                                               NSFCPrivDataKey key,
                                               void *pdata,
                                               NSFCCache cache);
 
PR_EXTERN(PRStatus) NSFC_ShutdownCache(NSFCCache cache, PRBool doitnow);

PR_EXTERN(PRStatus) NSFC_StartupCache(NSFCCache cache);

/*
 * FUNCTION: NSFC_Terminate - shutdown file cache module
 *
 * This function is called to shutdown the file cache module.  It destroys
 * any existing file cache instances, and terminates threads used internally
 * by the module.
 *
 *      flushtmp - PR_TRUE: remove any temporary files
 *      doitnow - PR_TRUE: quick and dirty termination
 */
PR_EXTERN(void) NSFC_Terminate(PRBool flushtmp, PRBool doitnow);

/*
 * NSFC_TransmitAsync - asynchronously transmit file on socket
 *
 * This function asynchronously transmits a previously cached file on a
 * socket.  Optional headers may be specified.  Because each call only
 * transmits as much data as can be sent without blocking, it may be
 * necessary to call the function multiple times to transmit a single
 * file.  On return, *offset is set to the total number of bytes
 * transmitted so far.  If the function returns NSFC_ASYNCSTATUS_AGAIN,
 * this offset should be specified on a subsequent call to resume
 * transmission after the socket polls ready for writing.
 *
 *      socket - operating system output socket file descriptor
 *      entry - cache entry handle
 *      headers - pointer to header buffer
 *      hdrlen - length of header data
 *      offset - offset from which to resume transmission
 *      cache - file cache instance handle
 */
PR_EXTERN(NSFCAsyncStatus) NSFC_TransmitAsync(NSFCNativeSocketDesc socket,
                                              NSFCEntry entry,
                                              const void *headers,
                                              PRInt32 hdrlen,
                                              PRInt64 *offset,
                                              NSFCCache cache);

/*
 * NSFC_TransmitEntryFile - transmit file on network socket
 *
 * This function transmits, on a specified network socket, a specified
 * file for which a handle for a cache entry was previously obtained
 * from NSFC_AccessFilename() or NSFC_LookupFilename().  The caller
 * may also specify a buffer containing data to be transmitted prior
 * to the file contents.  The timeout argument is used as described
 * for PR_TransmitFile().
 *
 *      socket - output socket file descriptor
 *      entry - cache entry handle
 *      headers - pointer to header buffer
 *      hdrlen - length of header data
 *      trailers - pointer to trailer buffer
 *      tlrlen - length of trailer data
 *      timeout - see PR_TransmitFile()
 *      cache - file cache instance handle
 */
PR_EXTERN(PRInt64) NSFC_TransmitEntryFile(PRFileDesc *socket,
                                          NSFCEntry entry,
                                          const void *headers, 
                                          PRInt32 hdrlen,
                                          const void *trailers, 
                                          PRInt32 tlrlen,
                                          PRIntervalTime timeout,
                                          NSFCCache cache,
                                          NSFCStatusInfo *statusInfo);

/*
 * NSFC_TransmitFile - transmit file on network socket
 *
 * This function transmits, on a specified network socket, a specified
 * file.  The caller may also specify a buffer containing data to be 
 * transmitted prior to the file contents.  The timeout argument is
 * used as described for PR_TransmitFile().
 *
 *      socket - output socket file descriptor
 *      filename - name of file to transmit
 *      headers - pointer to header buffer
 *      hdrlen - length of header data
 *      trailers - pointer to trailer buffer
 *      tlrlen - length of trailer data
 *      timeout - see PR_TransmitFile()
 *      cache - file cache instance handle, or NULL
 */
PR_EXTERN(PRInt64) NSFC_TransmitFile(PRFileDesc *socket,
                                     const char *filename,
                                     const void *headers,
                                     PRInt32 hdrlen,
                                     const void *trailers,
                                     PRInt32 tlrlen,
                                     PRIntervalTime timeout,
                                     NSFCCache cache,
                                     NSFCStatusInfo *statusInfo);

PR_EXTERN(PRInt64) NSFC_TransmitFileNonCached(PRFileDesc *socket,
                           const char *filename, NSFCFileInfo *finfo,
                           const void *headers, PRInt32 hdrlen,
                           const void *trailers, PRInt32 tlrlen,
                           PRIntervalTime timeout,
                           NSFCCache cache,
                           NSFCStatusInfo *statusInfo);

PR_EXTERN(PRStatus) NSFC_GetNonCacheFileInfo(const char *filename,
                                             NSFCFileInfo *finfo);

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
PR_EXTERN(NSFCStatus) NSFC_CheckEntry(NSFCEntry entry, NSFCCache cache);

/*
 * NSFC_RefreshFilename - ensure cache consistency for file
 *
 * This function ensures that any file cache entry for the named file is
 * consistent with what's on disk.  If an existing entry contains outdated
 * information, the entry is marked for deletion.
 */
PR_EXTERN(void) NSFC_RefreshFilename(const char *filename, NSFCCache cache);

/*
 * NSFC_InvalidateFilename - mark file cache entry for deletion
 *
 * This function marks any file cache entry for the named file for deletion.
 */
PR_EXTERN(void) NSFC_InvalidateFilename(const char *filename, NSFCCache cache);

/*
 * NSFC_EnumEntries - enumerate entries
 *
 * This function enumerates the entries in the file cache.  To enumerate
 * all entries, NSFC_EnumEntries() should be called repeatedly until it
 * return an entry rv for which !NSFCENTRY_ISVALID(&rv).
 *
 * The caller is given a reference to each returned NSFCEntry.  The
 * reference must be released by calling NSFCReleaseEntry().
 *
 * Before beginning enumeration, state must be initialized to
 * NSFCENTRYENUMSTATE_INIT.  When the enumeration has finished,
 * NSFC_EndEntryEnum(state) must be called to free state associated with
 * the enumeration.
 *
 * Note that if the file cache is in use, it is possible but unlikely that
 * 1. some entries may be skipped and 2. a single NSFCEntry may be returned
 * multiple times.
 *
 *      cache - file cache instance handle
 *      state - address of an NSFCEntryEnumState
 */
PR_EXTERN(NSFCEntry) NSFC_EnumEntries(NSFCCache cache, NSFCEntryEnumState *state);

PR_EXTERN(void) NSFC_EndEntryEnum(NSFCEntryEnumState *state);

PR_EXTERN(NSFCCacheSignature) NSFC_GetCacheSignature(NSFCCache cache); 

PR_END_EXTERN_C

#endif /* __nsfc_h_ */
