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

static PRInt64
__MDEntry_Transmit(PRFileDesc *socket, NSFCMDEntry *md, NSFCFileInfo *finfo,
                   const void *headers, PRInt32 hdrlen,
                   const void *trailers, PRInt32 tlrlen,
                   PRIntervalTime timeout, PRInt32 sendfileSize);

static NSFCMDEntryState
__MDEntry_ContentFill(PRFileDesc *fd, NSFCMDEntry *mdEntry,
                      PRInt64 length, int *fillFlags, NSFCCache cache, 
                      PRErrorCode *prerr, PRInt32 *oserr);
static void
__MDEntry_ContentFree(NSFCCache cip, NSFCMDEntry *md, int *pflags);


PR_IMPLEMENT(PRIntn)
NSFC_CmpFilename(const char *fn1, const char *fn2)
{
    return (PRIntn)strcmp(fn1, fn2);
}

/*
 * NSFC_MD_FreeContent - free resources used by cached content
 *
 * This function frees any resources used by cached file content
 * associated with an entry.  It marks the entry as no longer having
 * cached contents, and updates resource statistics in the cache
 * instance to reflect the freed resources.
 *
 * Assumptions:
 *
 *      Caller has exclusive write access to the entry.
 */
PR_IMPLEMENT(void)
NSFC_MD_FreeContent(NSFCCache cip, NSFCEntryImpl *nep)
{
    /* Establish that the entry has cached content */
    if (nep->flags & NSFCENTRY_HASCONTENT) {
        __MDEntry_ContentFree(cip, &nep->md, &nep->flags);
    }
    nep->flags &= ~NSFCENTRY_TMPFILE;
}

PR_IMPLEMENT(PRInt64)
NSFC_MD_TransmitFile(PRFileDesc *socket, NSFCEntryImpl *nep,
                     const void *headers, PRInt32 hdrlen,
                     const void *trailers, PRInt32 tlrlen,
                     PRIntervalTime timeout, NSFCCache cache,
                     NSFCStatusInfo *statusInfo)
{
    PRInt64 rv = -1;

    PR_ASSERT(nep != NULL && (nep->flags & NSFCENTRY_HASINFO) &&
              !(nep->flags & NSFCENTRY_ERRORINFO));

    NSFCSTATUSINFO_INIT(statusInfo);

    // Determine the maximum content size for PR_SendFile
    PRInt32 sendfileSize;
    PRBool sizeOK = NSFC_isSizeOK(cache, &nep->finfo, hdrlen,
                                  tlrlen, sendfileSize);

    /* Is content present for this file? */
    if (!(nep->flags & NSFCENTRY_HASCONTENT)) {
        PRBool useSendFile = cache->cfg.useSendFile;
        NSFCMDEntryState state;
        NSFCMDEntry newEntry;
        int fillFlags = 0; 
        PRFileDesc *fd = NULL;
        PRErrorCode prerr = 0;
        PRInt32 oserr = 0;

        /* Keep count of cached content misses */
        (void)PR_AtomicIncrement((PRInt32 *)&cache->ctntMiss);

        /*
         * XXX elving This is bogus.  If files are being updated, we might end
         * up using finfo from one inode for the response headers and the
         * contents of another for the response body!  Instead of doing stat()
         * then open(), we should open() then fstat().
         */

        /* Get file content */
        NSFCStatusInfo mystatusInfo;
        NSFCSTATUSINFO_INIT(&mystatusInfo);
        fd = NSFC_OpenEntryFile(nep, cache, &mystatusInfo);
        if (!fd) {
            /* Couldn't open file */
            NSFCSTATUSINFO_SET(statusInfo, mystatusInfo);
            state = MDENTRY_FATAL;
        }
        else if (cache->cfg.copyFiles == PR_TRUE &&
                 mystatusInfo == NSFC_STATUSINFO_TMPUSEREALFILE) {
            /*
             * The configuration requires we copy files before using
             * PR_SendFile(), but copying the file failed.  Don't use
             * PR_SendFile().
             */
            useSendFile = PR_FALSE;
            state = MDENTRY_NONCACHEFD;
        }
        else if (!sizeOK) {
            /*
             * NSPR IO interfaces are 31-bit.  Don't bother to cache content
             * we won't be able to use.
             */
            useSendFile = PR_FALSE;
            state = MDENTRY_NONCACHEFD;
        }
        else {
            /*
             * Read content into newEntry.  Note that__MDEntry_ContentFill()
             * either closes fd or caches it in newEntry.
             */
            state = __MDEntry_ContentFill(fd, &newEntry,
                                          nep->finfo.pr.size,
                                          &fillFlags, cache,
                                          &prerr, &oserr);
            if (state != MDENTRY_NONCACHEFD)
                fd = NULL;
        }

        /* Cache file content */
        if (state == MDENTRY_FILLED) {
            /* Get write access to the cache entry */
            if (NSFC_AcquireEntryWriteLock(cache, nep) == NSFC_OK) {

                /* See if content got cached while we acquired the lock */
                if (!(nep->flags & NSFCENTRY_HASCONTENT)) {
                    /* Need to copy from newEntry to nep */
                    nep->md = newEntry;
                    nep->flags |= fillFlags;
                    state = MDENTRY_CACHED;
                }

                NSFC_ReleaseEntryWriteLock(cache, nep);
            }
        }

        /* Transmit file content */
        switch (state) {
        case MDENTRY_DIFFSIZE:
            NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_FILESIZE);
            NSFC_DeleteEntry(cache, nep, PR_FALSE);
            /* fall through */
        case MDENTRY_FATAL: 
            rv = -1;
            if (prerr || oserr) {
                PR_SetError(prerr, oserr);
            }
            break;
        case MDENTRY_CACHED:
            rv = __MDEntry_Transmit(socket, &nep->md, &nep->finfo,
                                    headers, hdrlen, trailers, tlrlen,
                                    timeout, sendfileSize);
            break;
        case MDENTRY_FILLED:
            rv = __MDEntry_Transmit(socket, &newEntry, &nep->finfo,
                                    headers, hdrlen, trailers, tlrlen,
                                    timeout, sendfileSize);
            __MDEntry_ContentFree(cache, &newEntry, &fillFlags);
            break;
        case MDENTRY_NONCACHEFD:
            PR_ASSERT(fd);
            if (useSendFile) {
                rv = NSFC_PR_SendFile(socket, fd, &nep->finfo,
                                      headers, hdrlen, trailers, tlrlen,
                                      timeout, sendfileSize, statusInfo);
            } else {
                rv = NSFC_ReadWriteFile(socket, fd, &nep->finfo,
                                        headers, hdrlen, trailers, tlrlen,
                                        timeout, cache, statusInfo);
            }
            PR_Close(fd);
            fd = NULL;
            if ((rv < 0) && (prerr || oserr)) {
                PR_SetError(prerr, oserr);
            }
            break;
        default: 
            PR_ASSERT(0);
            break;
        }

        PR_ASSERT(fd == NULL);
    }
    else {
        /* Keep count of cached content hits */
        PR_AtomicIncrement((PRInt32 *)&cache->ctntHits);

        if (sizeOK) {

            /* Transmit file content */
            rv = __MDEntry_Transmit(socket, &nep->md, &nep->finfo,
                                    headers, hdrlen, trailers, tlrlen,
                                    timeout, sendfileSize);
        }
        else {
            /* If the file was too big for __MDEntry_Transmit()... */
            rv = NSFC_TransmitFileNonCached(socket, nep->filename, &nep->finfo,
                                            headers, hdrlen, trailers, tlrlen,
                                            timeout, cache, statusInfo);
        }
    }

    if (rv >= 0 && rv != (hdrlen + nep->finfo.pr.size + tlrlen)) {
        NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_FILESIZE);
        NSFC_DeleteEntry(cache, nep, PR_FALSE);
    }

    return rv;
}

PR_IMPLEMENT(NSFCAsyncStatus)
NSFC_MD_TransmitAsync(int socket,
                      NSFCEntryImpl *nep,
                      const void *headers,
                      PRInt32 hdrlen,
                      PRInt64 *offset,
                      NSFCCache cache)
{
    struct iovec iov[2];
    int iovcnt = 0;

    /* Make sure the content is already in memory */
    if (!(nep->flags & NSFCENTRY_HASCONTENT) || nep->md.fd) {
        *offset = 0;
        return NSFC_ASYNCSTATUS_WOULDBLOCK;
    }

    /* Construct the iov[], skipping *offset bytes */
    PRInt64 skip = *offset;
    PR_ASSERT(skip >= 0);
    if (hdrlen > skip) {
        iov[iovcnt].iov_base = (char *)headers + skip;
        iov[iovcnt].iov_len = hdrlen - skip;
        iovcnt++;
        skip = 0;
    } else {
        skip -= hdrlen;
    }
    if (nep->md.length > skip) {
        iov[iovcnt].iov_base = (char *)nep->md.pcontent + skip;
        iov[iovcnt].iov_len = nep->md.length - skip;
        iovcnt++;
        skip = 0;
    } else {
        skip -= nep->md.length;
    }
    PR_ASSERT(skip == 0);

    /* Send the iov[] */
    int rv = writev(socket, iov, iovcnt);
    if (rv == -1) {
        /* Bail out on network error */
        int e = errno;
        if (e != EAGAIN && e != EWOULDBLOCK)
            return NSFC_ASYNCSTATUS_IOERROR;
    } else {
        /* Keep track of how much data has been sent */
        *offset += rv;
    }

    /* Let the caller know if there's more data left to send */
    if (*offset < hdrlen + nep->md.length)
        return NSFC_ASYNCSTATUS_AGAIN;

    /* Keep count of cached content hits */
    PR_AtomicIncrement((PRInt32 *)&cache->ctntHits);

    return NSFC_ASYNCSTATUS_DONE;
}

static PRInt64
__MDEntry_Transmit(PRFileDesc *socket, NSFCMDEntry *md, NSFCFileInfo *finfo,
                   const void *headers, PRInt32 hdrlen,
                   const void *trailers, PRInt32 tlrlen,
                   PRIntervalTime timeout, PRInt32 sendfileSize)
{
    PRInt64 rv = -1;

    PR_ASSERT(md != NULL);

    if (md->fd) {
        PR_ASSERT(!md->pcontent);

        /* Only the file descriptor was cached, not its contents */
        rv = NSFC_PR_SendFile(socket, md->fd, finfo, headers, hdrlen,
                              trailers, tlrlen, timeout, sendfileSize, 
                              NULL);
    } else {
        PR_ASSERT(!md->fd);

        /* Content is in memory, so just writev the headers and content */
        PRIOVec iov[3];
        PRInt32 iovcnt = 0;
        if (headers && (hdrlen > 0)) {
            iov[iovcnt].iov_base = (char *)headers;
            iov[iovcnt].iov_len = hdrlen;
            ++iovcnt;
        }
        if (md->length > 0) {
            iov[iovcnt].iov_base = (char *)md->pcontent;
            iov[iovcnt].iov_len = md->length;
            ++iovcnt;
        }
        if (trailers && (tlrlen > 0)) {
            iov[iovcnt].iov_base = (char *)trailers;
            iov[iovcnt].iov_len = tlrlen;
            ++iovcnt;
        }
        if (iovcnt != 0) {
            rv = PR_Writev(socket, iov, iovcnt, timeout);
        }
        else {
            rv = 0;
        }
    }

    return rv;
}

static NSFCMDEntryState 
__MDEntry_ContentFill(PRFileDesc *fd, NSFCMDEntry *mdEntry,
                      PRInt64 length, int *fillFlags, NSFCCache cache, 
                      PRErrorCode *prerr, PRInt32 *oserr)

{
    /*
     * N.B. we assume ownership of the fd (either caching it in mdEntry->fd or
     * closing it) *except* when we return MDENTRY_NONCACHEFD
     */

    PR_ASSERT(fd != NULL);
    PR_ASSERT(mdEntry != NULL && fillFlags != NULL);
    PR_ASSERT(prerr != NULL && oserr != NULL);
    PR_ASSERT(length <= PR_INT32_MAX);

    *fillFlags = 0;

    /*
     * Files are classified as small, medium, or large.  We cache small files'
     * contents on the heap, mmap medium files' contents, and cache only the
     * file descriptor for large files.
     */

    if (cache->cfg.contentCache) {
        /*
         * Handle small and medium files
         */

        if (length == 0) { /* mmap can't have length 0 */
            /* 0-byte file, so caching is easy */
            mdEntry->length = length;
            mdEntry->fd = NULL;
            mdEntry->pcontent = NULL;
            mdEntry->fmap = NULL;
            *fillFlags |= NSFCENTRY_HASCONTENT;
            PR_Close(fd);
            return MDENTRY_FILLED;
        }

        if (length <= cache->cfg.limSmall) {
            /* Small file, use the heap */
            mdEntry->pcontent = NSFC_Malloc(length, cache);
            if (mdEntry->pcontent) {
                PRInt32 bytesToRead = length;
                char* buff = (char*)mdEntry->pcontent;
                while (bytesToRead > 0) { 
                    PRInt32 rv = PR_Read(fd, (void*)buff, bytesToRead);
                    if (rv < 0) {
                        *prerr = PR_GetError();
                        *oserr = PR_GetOSError();
                        NSFC_Free(mdEntry->pcontent, length, cache);
                        mdEntry->pcontent = NULL;
                        PR_Close(fd);
                        return MDENTRY_FATAL;
                    }
                    else if (rv == 0) {
                        NSFC_Free(mdEntry->pcontent, length, cache);
                        mdEntry->pcontent = NULL;
                        if (PR_Seek64(fd, 0, PR_SEEK_SET) == -1) {
                            *prerr = PR_GetError();
                            *oserr = PR_GetOSError();
                            PR_Close(fd);
                            return MDENTRY_DIFFSIZE;
                        }
                        return MDENTRY_NONCACHEFD;
                    }
                    bytesToRead = bytesToRead - rv;
                    buff += rv;
                }
                mdEntry->length = length;
                mdEntry->fd = NULL;
                mdEntry->fmap = NULL;
                *fillFlags |= NSFCENTRY_HASCONTENT;
                PR_Close(fd);
                return MDENTRY_FILLED;
            }

            /* No heap space left */
            cache->cacheFull = PR_TRUE;
        }

        if (length <= cache->cfg.limMedium) {
            /* Medimum file, use mmap */
            if (NSFC_ReserveMmap(cache, length)) {
                mdEntry->fmap = PR_CreateFileMap(fd, length, PR_PROT_READONLY);
                if (!mdEntry->fmap) {
                    NSFC_ReleaseMmap(cache, length);
                    *prerr = PR_GetError();
                    *oserr = PR_GetOSError();
                    PR_Close(fd);
                    return MDENTRY_FATAL; 
                }
                mdEntry->pcontent = PR_MemMap(mdEntry->fmap, 0, length);
                if (!(mdEntry->pcontent)) {
                    *prerr = PR_GetError();
                    *oserr = PR_GetOSError();
                    PR_CloseFileMap(mdEntry->fmap);
                    mdEntry->fmap = NULL;
                    NSFC_ReleaseMmap(cache, length);
                    PR_Close(fd);
                    return MDENTRY_FATAL; 
                }
                mdEntry->length = length;
                mdEntry->fd = NULL;
                *fillFlags |= (NSFCENTRY_HASCONTENT|NSFCENTRY_CMAPPED);
                PR_Close(fd);
                return MDENTRY_FILLED;
            }

            /* No mmap address space left */
            cache->cacheFull = PR_TRUE;
        }
    }

    /*
     * This file is too large to put on the heap or to mmap, but hopefully we
     * can still cache its file descriptor
     */

    /* If we can't use PR_SendFile(), there's no point in caching fds */
    if (!cache->cfg.useSendFile) {
        return MDENTRY_NONCACHEFD;
    }

    /* Ensure that the limit on open fds is not exceeded */
    if (cache->curOpen >= cache->cfg.maxOpen) {
        /* Reached maximum number of open fds */
        cache->cacheFull = PR_TRUE;
        return MDENTRY_NONCACHEFD; 
    }
    if (PR_AtomicIncrement((PRInt32 *)&cache->curOpen) > cache->cfg.maxOpen) {
        /* Reached maximum number of open fds */
        PR_AtomicDecrement((PRInt32 *)&cache->curOpen);
        cache->cacheFull = PR_TRUE;
        return MDENTRY_NONCACHEFD; 
    }

    /* Large file, cache only the file descriptor */
    mdEntry->length = length;
    mdEntry->fd = fd;
    mdEntry->pcontent = NULL;
    mdEntry->fmap = NULL;
    *fillFlags |= (NSFCENTRY_HASCONTENT|NSFCENTRY_OPENFD);
    return MDENTRY_FILLED;
}

static void
__MDEntry_ContentFree(NSFCCache cip, NSFCMDEntry *md, int *pflags)
{
    PR_ASSERT(*pflags & NSFCENTRY_HASCONTENT);

    if (*pflags & NSFCENTRY_OPENFD) {
        PR_ASSERT(md->fd);
        PR_ASSERT(!md->pcontent);
        PR_ASSERT(!md->fmap);
        PR_ASSERT(md->length > 0);

        /* Close file descriptor */
        PR_Close(md->fd);
        md->fd = NULL;
        PR_AtomicDecrement((PRInt32 *)&cip->curOpen);
        *pflags &= ~(NSFCENTRY_HASCONTENT|NSFCENTRY_OPENFD);
    }
    else if (*pflags & NSFCENTRY_CMAPPED) {
        PR_ASSERT(!md->fd);
        PR_ASSERT(md->fmap);
        PR_ASSERT(md->length > 0);

        /* Unmap memory mapped content */
        PR_MemUnmap(md->pcontent, md->length);
        PR_CloseFileMap(md->fmap);
        md->pcontent = NULL;
        md->fmap = NULL;
        NSFC_ReleaseMmap(cip, md->length);
        *pflags &= ~(NSFCENTRY_HASCONTENT|NSFCENTRY_CMAPPED);
    }
    else {
        PR_ASSERT(!md->fd);
        PR_ASSERT(!md->fmap);

        /* Free content cached on the heap */
        if (md->length > 0) {
            NSFC_Free(md->pcontent, md->length, cip);
        }
        md->pcontent = NULL;
        *pflags &= ~(NSFCENTRY_HASCONTENT);
    }

    md->length = 0;
}

PR_IMPLEMENT(PRInt32)
NSFC_MD_GetPageSize()
{
    PRInt32 pageSize = sysconf(_SC_PAGESIZE);
    PR_ASSERT(pageSize > 0);
    return pageSize;
}
