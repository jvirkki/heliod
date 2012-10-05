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

#include <errno.h>
#include "private/pprio.h"
#include "nsfc_pvt.h"
#include "time/nstime.h"

#ifdef SOLARIS_TNF
#include <tnf/probe.h>
#endif

static void
_NSFC_PR_NT_CancelIo(PRFileDesc *fd);

/* 
 * NSFC_GetEntryFileInfo - get the entry's finfo
 *
 * caller needs to have a valid entry reference
 *
 * if return NSFC_OK, the entry's finfo is returned in finfo
 * if return NSFC_STATFAIL, the entry's error finfo is returned in finfo
 *
 */
PR_IMPLEMENT(NSFCStatus)
NSFC_GetEntryFileInfo(NSFCEntry entry, NSFCFileInfo *finfo,
                      NSFCCache cache)
{
    NSFCEntryImpl *nep = entry;
    NSFCStatus rfc = NSFC_OK;

    if (rfc == NSFC_OK) {
        PR_ASSERT(nep->flags & NSFCENTRY_HASINFO);
        memcpy(finfo, &nep->finfo, sizeof(*finfo));
        if (nep->flags & NSFCENTRY_ERRORINFO)
            rfc = NSFC_STATFAIL;
        NSFC_RecordEntryHit(cache, nep);
    } 
    else {
        PR_ASSERT(rfc != NSFC_STATFAIL);
    }
    PR_ASSERT(rfc != NSFC_NOFINFO);
    PR_ASSERT(rfc != NSFC_NOTFOUND);

    return rfc;
}

/*
 * if error info return failure, otherwise return success 
 *
 * get finfo even dead cache
 *
 * if finfo passed NULL, this function canbe used to test
 * whether the file canbe stat.
 *
 */
PR_IMPLEMENT(PRStatus)
NSFC_GetFileInfo(const char *filename, NSFCFileInfo *finfo,
                 NSFCCache cache)
{
    NSFCEntry entry = NSFCENTRY_INIT;
    NSFCEntryImpl *nep = entry;
    NSFCStatus rfc;
    NSFCStatusInfo statusInfo; 
    NSFCFileInfo myfinfo;
    NSFCFileInfo *f_info = (finfo) ? finfo : &myfinfo;
    PRBool hit = PR_FALSE;

    PRStatus rv = PR_SUCCESS;

    NSFCSTATUSINFO_INIT(&statusInfo);

    rfc = NSFC_AccessFilename(filename, &nep, f_info, cache, &statusInfo);
    if (rfc == NSFC_OK) {
        PR_ASSERT(nep->flags & NSFCENTRY_HASINFO);
        if (finfo)
            memcpy(finfo, &nep->finfo, sizeof(*finfo));
        if (nep->flags & NSFCENTRY_ERRORINFO) {
            rv = PR_FAILURE;
        }
        if (statusInfo != NSFC_STATUSINFO_CREATE)
            hit = PR_TRUE;
        NSFC_RecordEntryHit(cache, nep);
        NSFC_ReleaseEntry(cache, &nep);
    }
    else if (rfc == NSFC_STATFAIL) {
            rv = PR_FAILURE;
    }
    else {
        rv = NSFC_GetNonCacheFileInfo(filename, f_info);
    }

    if (hit)
        PR_AtomicIncrement((PRInt32 *)&cache->infoHits);
    else
        PR_AtomicIncrement((PRInt32 *)&cache->infoMiss);

    return rv;
}

PR_IMPLEMENT(PRStatus)
NSFC_GetNonCacheFileInfo(const char *filename, NSFCFileInfo *finfo)
{

    PRStatus rv;
    PRIntervalTime now;

    PR_ASSERT(filename != NULL);
    PR_ASSERT(finfo != NULL);

    now = ft_timeIntervalNow();
    finfo->lastUpdate = now;
    finfo->fileid[0] = 0;
    finfo->fileid[1] = 0;
    finfo->prerr = 0;
    finfo->oserr = 0;

    rv = NSFC_PR_GetFileInfo(filename, &finfo->pr);
    if (rv == PR_FAILURE) {
            /* Save the details of the failure */
        finfo->prerr = PR_GetError();
        finfo->oserr = PR_GetOSError();
        finfo->pr.type = PR_FILE_OTHER;
        finfo->pr.size = 0;
        finfo->pr.creationTime = 0;
        finfo->pr.modifyTime = 0;
   }

   return rv;
}

PR_IMPLEMENT(PRStatus)
NSFC_PR_GetFileInfo(const char *filename,
                    PRFileInfo64 *finfo)
{
    PRStatus rv;

#ifdef ESTALE
    // Retry PR_GetFileInfo64() up to NSFC_ESTALE_RETRIES times if it returns
    // ESTALE.  Bug 545152
    int retries = NSFC_ESTALE_RETRIES;
    do {
        rv = PR_GetFileInfo64(filename, finfo);
    } while (rv == PR_FAILURE && PR_GetOSError() == ESTALE && retries--);
#else
    rv = PR_GetFileInfo64(filename, finfo);
#endif

    // Reject requests for "/index.html/"
    if (rv == PR_SUCCESS && finfo->type == PR_FILE_FILE) {
        int len = strlen(filename);
        if (len > 0) {
#ifdef XP_WIN32
            if (filename[len - 1] == '/' || filename[len - 1] == '\\')
#else
            if (filename[len - 1] == '/')
#endif
            {
                PR_SetError(PR_NOT_DIRECTORY_ERROR, 0);
                rv = PR_FAILURE;
            }
        }
    }

    return rv;
}

PR_IMPLEMENT(PRFileDesc*)
NSFC_PR_Open(const char *name,
             PRIntn flags,
             PRIntn mode)
{
    PRFileDesc *fd;

#ifdef ESTALE
    // Retry PR_Open() up to NSFC_ESTALE_RETRIES times if it returns ESTALE.
    // Bug 545152
    int retries = NSFC_ESTALE_RETRIES;
    do {
        fd = PR_Open(name, flags, mode);
    } while (!fd && PR_GetOSError() == ESTALE && retries--);
#else
    fd = PR_Open(name, flags, mode);
#endif

#ifdef XP_WIN32
    /* Work-around for NSPR error mapping deficiency */
    if ((fd == NULL) && (PR_GetError() == PR_UNKNOWN_ERROR) 
        && (PR_GetOSError() == ERROR_SHARING_VIOLATION))
        PR_SetError(PR_FILE_IS_BUSY_ERROR, ERROR_SHARING_VIOLATION);
#endif

    return fd;
}

PR_IMPLEMENT(NSFCAsyncStatus)
NSFC_TransmitAsync(NSFCNativeSocketDesc socket, NSFCEntry entry,
                   const void *headers, PRInt32 hdrlen,
                   PRInt64 *offset, NSFCCache cache)
{
    NSFCAsyncStatus rv;

#ifdef XP_WIN32
    /* XXX implement NSFC_MD_TransmitAsync on Windows? */
    rv = NSFC_ASYNCSTATUS_WOULDBLOCK;
#else
    rv = NSFC_MD_TransmitAsync(socket, entry, headers, hdrlen, offset, cache);
#endif

    if (rv == NSFC_ASYNCSTATUS_DONE)
        NSFC_RecordEntryHit(cache, entry);

    return rv;
}

/*
 * NSFC_TransmitEntryFile - transmit file for a entry
 *
 * caller need to have a valid entry handle
 *
 * if rv return -1 and statusInfo return NSFC_STATUSINFO_DELETED
 * or NSFC_STATUSINFO_STATFAIL, then no transmit has occured
 *
 */
PR_IMPLEMENT(PRInt64)
NSFC_TransmitEntryFile(PRFileDesc *socket,
                       NSFCEntry entry,
                       const void *headers, PRInt32 hdrlen,
                       const void *trailers, PRInt32 tlrlen,
                       PRIntervalTime timeout,
                       NSFCCache cache,
                       NSFCStatusInfo *statusInfo)
{
    NSFCEntryImpl *nep = entry;
    NSFCFileInfo finfo;
    PRInt64 rv;

    NSFCSTATUSINFO_INIT(statusInfo);

    NSFC_RecordEntryHit(cache, nep);

    rv = NSFC_MD_TransmitFile(socket, nep, headers, hdrlen,
                              trailers, tlrlen, timeout,
                              cache, statusInfo);

#ifdef XP_WIN32
    /* Work-around for NSPR error mapping deficiency */
    if ((rv < 0) && (PR_GetError() == PR_UNKNOWN_ERROR) &&
        (PR_GetOSError() == ERROR_NETNAME_DELETED)) {
        PR_SetError(PR_CONNECT_RESET_ERROR, ERROR_NETNAME_DELETED);
    }
#endif /* XP_WIN32 */

    return rv;
}

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
PR_IMPLEMENT(PRInt64)
NSFC_TransmitFile(PRFileDesc *socket, const char *filename,
                  const void *headers, PRInt32 hdrlen,
                  const void *trailers, PRInt32 tlrlen,
                  PRIntervalTime timeout, NSFCCache cache,
                  NSFCStatusInfo *statusInfo)
{
    NSFCEntry entry = NSFCENTRY_INIT;
    NSFCFileInfo finfo;
    PRInt64 rv = -1;
    NSFCStatus rfc;
    PRStatus statrv;
    int cache_transmit;

    NSFCSTATUSINFO_INIT(statusInfo);

    statrv = PR_FAILURE;
    cache_transmit = 0;
    rfc = NSFC_AccessFilename(filename, &entry, &finfo, cache, statusInfo);
    if (rfc == NSFC_OK) {
        rfc = NSFC_GetEntryFileInfo(entry, &finfo, cache);
        if (rfc == NSFC_OK) {
            cache_transmit = 1;
            statrv = PR_SUCCESS;
        }
        else {
            NSFC_ReleaseEntry(cache, &entry);
        }
    }
    if (statrv == PR_FAILURE && rfc != NSFC_STATFAIL)
        statrv = NSFC_GetNonCacheFileInfo(filename, &finfo);

    if (statrv == PR_FAILURE) {
        NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_STATFAIL);
        return -1;
    }

    if (cache_transmit) {
        PR_ASSERT(NSFCENTRY_ISVALID(&entry));
        PR_ASSERT((entry->flags & NSFCENTRY_HASINFO)
                  && !(entry->flags & NSFCENTRY_ERRORINFO));
        rv = NSFC_TransmitEntryFile(socket, entry,
                                    headers, hdrlen,
                                    trailers, tlrlen,
                                    timeout, cache,
                                    statusInfo);
        NSFC_ReleaseEntry(cache, &entry);
    }
    else {
        PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
        PR_ASSERT(statrv == PR_SUCCESS);
        rv = NSFC_TransmitFileNonCached(socket, filename, &finfo,
                                        headers, hdrlen, trailers, tlrlen,
                                        timeout, cache,
                                        statusInfo);
    }
    return rv;
}

PR_IMPLEMENT(PRInt64)
NSFC_TransmitFileNonCached(PRFileDesc *socket, 
                           const char *filename,
                           NSFCFileInfo *finfo,
                           const void *headers, PRInt32 hdrlen,
                           const void *trailers, PRInt32 tlrlen,
                           PRIntervalTime timeout,
                           NSFCCache cache, 
                           NSFCStatusInfo *statusInfo)
{
    PRInt64 rv;

    PR_ASSERT(finfo != NULL);

    PRFileDesc *fd = NSFC_PR_Open(filename, PR_RDONLY, 0);
    if (!fd) {
        NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_OPENFAIL);
        return -1;
    }

    // Determine the maximum content size for PR_SendFile
    PRInt32 sendfileSize;
    PRBool sizeOK = NSFC_isSizeOK(cache, finfo, hdrlen, tlrlen, sendfileSize);

    if (cache->cfg.useSendFile && !cache->cfg.copyFiles &&
        sizeOK) {
        /* We can use PR_SendFile() */
        rv = NSFC_PR_SendFile(socket, fd, finfo,
                              headers, hdrlen,
                              trailers, tlrlen,
                              timeout,
                              sendfileSize,
                              statusInfo);
    }
    else {
        /* We can't use PR_SendFile() */
        rv = NSFC_ReadWriteFile(socket, fd, finfo,
                                headers, hdrlen,
                                trailers, tlrlen,
                                timeout,
                                cache,
                                statusInfo);
    }

    PR_Close(fd);

    return rv;
}

/*
 * NSFC_ReadWriteFile - transmit file without a cache entry or PR_SendFile()
 *
 * Note that the passed fd must not be shared between threads (i.e. it must
 * not be cached).
 */
PR_IMPLEMENT(PRInt64)
NSFC_ReadWriteFile(PRFileDesc *socket,
                   PRFileDesc *fd,
                   NSFCFileInfo *finfo,
                   const void *headers, PRInt32 hdrlen,
                   const void *trailers, PRInt32 tlrlen,
                   PRIntervalTime timeout,
                   NSFCCache cache,
                   NSFCStatusInfo *statusInfo)
{
    PRInt32 buff_size;
    void *buff;
    PRBool use_local_memfns;
    PRInt64 rv;

    PR_ASSERT(finfo != NULL);

    NSFCSTATUSINFO_INIT(statusInfo);

    buff_size = cache->cfg.bufferSize;
    if (finfo && finfo->pr.size < buff_size) {
        buff_size = finfo->pr.size + 1;
    }
    PR_ASSERT(buff_size > 0);

    if (cache && cache->local_memfns && 
        cache->local_memfns->alloc && cache->local_memfns->free) {
        buff = cache->local_memfns->alloc((int)buff_size);
        use_local_memfns = PR_TRUE;
    }
    else {
        buff = malloc(buff_size);
        use_local_memfns = PR_FALSE;
    }

    if (!buff) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        rv = -1;
    }
    else {
        PRInt64 bytesWritten = 0;
        PRBool eof = PR_FALSE;

        rv = PR_Read(fd, buff, buff_size);
        if (rv >= 0) {
            if (rv < buff_size) {
                eof = PR_TRUE;
            }

            PRIOVec iov[3];
            PRInt32 iovcnt = 0;
            if (headers && (hdrlen > 0)) {
                iov[iovcnt].iov_base = (char *)headers;
                iov[iovcnt].iov_len = hdrlen;
                ++iovcnt;
            }
            if (rv > 0) {
                iov[iovcnt].iov_base = (char *)buff;
                iov[iovcnt].iov_len = rv;
                ++iovcnt;
            }
            if (eof && trailers && (tlrlen > 0)) {
                iov[iovcnt].iov_base = (char *)trailers;
                iov[iovcnt].iov_len = tlrlen;
                ++iovcnt;
            }

            while (iovcnt > 0) {
                rv = PR_Writev(socket, iov, iovcnt, timeout);
                if (rv < 0) {
                    _NSFC_PR_NT_CancelIo(socket);
                    break;
                }
                bytesWritten += rv;

                if (eof) {
                    break;
                }

                rv = PR_Read(fd, buff, buff_size);
                if (rv < 0) {
                    break;
                }
                if (rv < buff_size) {
                    eof = PR_TRUE;
                }

                iovcnt = 0;
                if (rv > 0) {
                    iov[iovcnt].iov_base = (char *)buff;
                    iov[iovcnt].iov_len = rv;
                    ++iovcnt;
                }
                if (eof && trailers && (tlrlen > 0)) {
                    iov[iovcnt].iov_base = (char *)trailers;
                    iov[iovcnt].iov_len = tlrlen;
                    ++iovcnt;
                }
            }
        }
        if (use_local_memfns == PR_TRUE) {
            cache->local_memfns->free(buff);
        }
        else {
            free(buff);
        }

        if (rv >= 0) { 
            rv = bytesWritten;
            if (finfo && rv != (hdrlen + finfo->pr.size + tlrlen))
                NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_FILESIZE);
        }
    }

#ifdef XP_WIN32
    /* Work-around for NSPR error mapping deficiency */
    if ((rv < 0) && (PR_GetError() == PR_UNKNOWN_ERROR) &&
        (PR_GetOSError() == ERROR_NETNAME_DELETED)) {
        PR_SetError(PR_CONNECT_RESET_ERROR, ERROR_NETNAME_DELETED);
    }
#endif /* XP_WIN32 */

    return rv;
}


/*
 * NSFC_OpenEntryFile() - nsfc private
 *
 * to be used only for NSFC_MD_TransmitFile
 * this function is called from MD TransmitFile functions
 * for Unaccelerated Response path - the response headers
 * are generated based on nep->finfo for nep->filename
 *
 */
PR_IMPLEMENT(PRFileDesc *)
NSFC_OpenEntryFile(NSFCEntryImpl *nep, NSFCCache cache, 
                   NSFCStatusInfo *statusInfo)
{
    PRFileDesc *fd = NULL;

    PR_ASSERT(nep != NULL);

    if (cache->cfg.copyFiles == PR_TRUE) {
        fd = NSFC_OpenTempCopy(cache, nep, statusInfo);
        if (fd != NULL)
            return fd;

        /*
         * Set NSFC_STATUSINFO_TMPUSEREALFILE to let our caller know that
         * PR_SendFile() should not be used because we couldn't open a
         * temporary copy
         */
        NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_TMPUSEREALFILE);
    }

    fd = NSFC_PR_Open(nep->filename, PR_RDONLY, 0);
    if (!fd)
        NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_OPENFAIL);

    return fd;
}

/*
 * NSFC_PR_SendFile - transmit file using PR_SendFile()
 */
PR_IMPLEMENT(PRInt32)
NSFC_PR_SendFile(PRFileDesc *socket, PRFileDesc *fd, NSFCFileInfo *finfo,
                 const void *headers, PRInt32 hdrlen,
                 const void *trailers, PRInt32 tlrlen,
                 PRIntervalTime timeout,
                 PRInt32 sendfileSize,
                 NSFCStatusInfo *statusInfo)
{
    PRInt64 rv;

    PR_ASSERT(finfo != NULL);
    PR_ASSERT((PRInt64)hdrlen + sendfileSize <= PR_INT32_MAX);
    PR_ASSERT((PRInt64)tlrlen + sendfileSize <= PR_INT32_MAX);

    if (finfo->pr.size == 0) {
        PRIOVec iov[2];
        PRInt32 iovcnt;

        iovcnt = 0;
        if (headers && (hdrlen > 0)) {
            iov[iovcnt].iov_base = (char *)headers;
            iov[iovcnt].iov_len = hdrlen;
            ++iovcnt;
        }
        if (trailers && (tlrlen > 0)) {
            iov[iovcnt].iov_base = (char *)trailers;
            iov[iovcnt].iov_len = tlrlen;
            ++iovcnt;
        }
        if (iovcnt > 0) {
            rv = PR_Writev(socket, iov, iovcnt, timeout);
        }
        else {
            rv = 0;
        }
    }
    else {
        PRSendFileData sfd;
        PROffset64 remain = finfo->pr.size - sendfileSize;

        sfd.fd = fd;
        sfd.file_offset = 0;
        sfd.file_nbytes = sendfileSize;
        sfd.header = headers;
        sfd.hlen = hdrlen;
        if (remain == 0) {
            PR_ASSERT((PRInt64)hdrlen + finfo->pr.size + tlrlen <= PR_INT32_MAX);
            sfd.trailer = trailers;
            sfd.tlen = tlrlen;
        }
        else {
            sfd.trailer = NULL;
            sfd.tlen = 0;
        }
        rv = PR_SendFile(socket, &sfd, PR_TRANSMITFILE_KEEP_OPEN, timeout);

        if (remain > 0 && rv >= 0) {
            sfd.header = NULL;
            sfd.hlen = 0; 
            sfd.file_offset += sendfileSize;

            while (remain > sendfileSize && rv >= 0) {
                rv = PR_SendFile(socket, &sfd, PR_TRANSMITFILE_KEEP_OPEN, 
                                 timeout);
                remain -= sendfileSize;
                sfd.file_offset += sendfileSize;
            }

            // Send the remaining bits
            if (rv >= 0) {
                sfd.file_nbytes = remain;
                sfd.trailer = trailers;
                sfd.tlen = tlrlen;
                rv = PR_SendFile(socket, &sfd, PR_TRANSMITFILE_KEEP_OPEN, 
                                 timeout);
            }

            // If there were no errors, set the rv value to the total size
            // written instead of the last chunk written. The caller may
            // do sanity check on number of bytes written based on returned
            // value
            if (rv >= 0) 
                rv = hdrlen + finfo->pr.size + tlrlen;

        } 
    }

    if (rv < 0) {
        _NSFC_PR_NT_CancelIo(socket);
#ifdef XP_WIN32
        PRErrorCode prerr = PR_GetError();
        PRInt32 oserr = PR_GetOSError();
        if (prerr == PR_UNKNOWN_ERROR && oserr == ERROR_FILE_INVALID) {
             NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_FILESIZE);
        }
#endif
    }
    return rv;
}

#ifdef XP_WIN32
static PRStatus
_NSFC_Dummy_NT_CancelIo(PRFileDesc *fd)
{
    return PR_SUCCESS;
}
#endif

static void
_NSFC_PR_NT_CancelIo(PRFileDesc *fd)
{
#ifdef XP_WIN32
    PRErrorCode prerr = PR_GetError();
    PRInt32 oserr = PR_GetOSError();

    /* Attempt to load PR_NT_CancelIo() from the DLL that contains PR_Send() */
    static PRStatus (*pfnPR_NT_CancelIo)(PRFileDesc *fd) = NULL;
    if (pfnPR_NT_CancelIo == NULL) {
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery(&PR_Send, &mbi, sizeof(mbi));
        pfnPR_NT_CancelIo = (PRStatus (*)(PRFileDesc *fd))GetProcAddress((HINSTANCE)mbi.AllocationBase, "PR_NT_CancelIo");
    }

    /* If we couldn't find PR_NT_CancelIo(), just use the dummy */
    if (pfnPR_NT_CancelIo == NULL)
        pfnPR_NT_CancelIo = &_NSFC_Dummy_NT_CancelIo;

    /* VB: _NSFC_PR_NT_CancelIo - calls PR_NT_CancelIo when an I/O timed out
     *     or was interrupted.
     */
    if (prerr == PR_IO_TIMEOUT_ERROR || prerr == PR_PENDING_INTERRUPT_ERROR) {
        // Need to cancel the i/o
        if (pfnPR_NT_CancelIo(fd) != PR_SUCCESS) {
            // VB: This should not happen. Assert when this happens
            // Get the error codes to make debugging a bit easier
            PRErrorCode cancelErr = PR_GetError();
            PRInt32 cancelOSErr = PR_GetOSError();
            PR_ASSERT(0);
        }
    }

    PR_SetError(prerr, oserr);
#endif
}

PR_IMPLEMENT(PRBool)
NSFC_isSizeOK(NSFCCache cache, NSFCFileInfo* finfo, PRInt32 hdrlen,
              PRInt32 tlrlen, PRInt32& sendfileSize) 
{
    PRBool sizeOK = PR_TRUE;
    sendfileSize = finfo->pr.size;
    if (cache->cfg.sendfileSize > 0 && 
        cache->cfg.sendfileSize < finfo->pr.size) {
        /* NSPR IO interfaces are 31-bit */
        // We will be breaking the PR_SendFile call
        if ((PRInt64)hdrlen + cache->cfg.sendfileSize > PR_INT32_MAX ||
            (PRInt64)tlrlen + cache->cfg.sendfileSize > PR_INT32_MAX)
            sizeOK = PR_FALSE;
        else
            sendfileSize = cache->cfg.sendfileSize;
            
    }
    else {
        if ((PRInt64)hdrlen + finfo->pr.size + tlrlen > PR_INT32_MAX)
            sizeOK = PR_FALSE;
    }

    return sizeOK;
}
