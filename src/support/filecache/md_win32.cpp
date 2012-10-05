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
#include "private/pprio.h"

PR_IMPLEMENT(PRIntn)
NSFC_CmpFilename(const char *fn1, const char *fn2)
{
    return (PRIntn)stricmp(fn1, fn2);
}

PR_IMPLEMENT(void)
NSFC_MD_FreeContent(NSFCCache cip, NSFCEntryImpl *nep)
{
    if ((nep->flags & NSFCENTRY_OPENFD) && nep->md.fd) {
        PR_Close(nep->md.fd);
        nep->md.fd = NULL;
        nep->flags &= ~(NSFCENTRY_OPENFD|NSFCENTRY_HASCONTENT);
        PR_AtomicDecrement((PRInt32 *)&cip->curOpen);
    }
    if (nep->flags & NSFCENTRY_TMPFILE) {
        nep->flags &= ~NSFCENTRY_TMPFILE;
    }
}

PR_IMPLEMENT(PRInt64)
NSFC_MD_TransmitFile(PRFileDesc *socket, NSFCEntryImpl *nep,
                     const void *headers, PRInt32 hdrlen,
                     const void *trailers, PRInt32 tlrlen,
                     PRIntervalTime timeout, NSFCCache cache,
                     NSFCStatusInfo *statusInfo)
{
    NSFCFileInfo *finfo = &nep->finfo;
    PRFileDesc *fd = NULL;
    PRInt64 rv = -1;

    PR_ASSERT(nep != NULL && (nep->flags & NSFCENTRY_HASINFO) &&
              !(nep->flags & NSFCENTRY_ERRORINFO));

    NSFCSTATUSINFO_INIT(statusInfo);

    // Determine the maximum content size for PR_SendFile
    PRInt32 sendfileSize;
    PRBool sizeOK = NSFC_isSizeOK(cache, finfo, hdrlen, tlrlen,
                                  sendfileSize);

    /* Note that we can't use a cached fd for >2GB files */

    if (!(nep->flags & NSFCENTRY_OPENFD) || !sizeOK) {

        (void)PR_AtomicIncrement((PRInt32 *)&cache->ctntMiss);

        PRBool needtocache = PR_TRUE;

        if (finfo->pr.size > PR_INT32_MAX)
            needtocache = PR_FALSE;

        /* Open the file */

        NSFCStatusInfo mystatusInfo;
        NSFCSTATUSINFO_INIT(&mystatusInfo);
        fd = NSFC_OpenEntryFile(nep, cache, &mystatusInfo);
        if (!fd) {
            NSFCSTATUSINFO_SET(statusInfo, mystatusInfo);
            return -1;
        }
       
        if (cache->cfg.copyFiles == PR_TRUE &&
            (mystatusInfo == NSFC_STATUSINFO_TMPUSEREALFILE)) {
             rv = NSFC_ReadWriteFile(socket, fd, finfo,
                                     headers, hdrlen,
                                     trailers, tlrlen,
                                     timeout, cache,
                                     statusInfo);
             PR_Close(fd);
             goto done;
        }

        if (needtocache == PR_TRUE) {
            /* Ensure that the limit on open fds is not exceeded */
            if (PR_AtomicIncrement((PRInt32 *)&cache->curOpen) > cache->cfg.maxOpen) {

                /* Restore cache->curOpen */
                PR_AtomicDecrement((PRInt32 *)&cache->curOpen);
                needtocache = PR_FALSE;
            }
        }

        if (needtocache == PR_TRUE) {
                   
            needtocache = PR_FALSE;

            /* Get write access to the cache entry */
            if (NSFC_AcquireEntryWriteLock(cache, nep) == NSFC_OK) {

                /* See if open fd got cached while we acquired the lock */
                if (!(nep->flags & NSFCENTRY_OPENFD)) {
                    needtocache = PR_TRUE;
                    nep->md.fd = fd;
                    nep->flags |= (NSFCENTRY_HASCONTENT|NSFCENTRY_OPENFD);
                }
                NSFC_ReleaseEntryWriteLock(cache, nep);
            }
        }

        rv = NSFC_PR_SendFile(socket, fd, finfo, headers, hdrlen,
                              trailers, tlrlen, timeout, sendfileSize,
                              statusInfo);
  
        if (needtocache != PR_TRUE) {
            PR_Close(fd);
        }

    }
    else {
        PR_AtomicIncrement((PRInt32 *)&cache->ctntHits);

        fd = nep->md.fd;
        if (fd) {
            rv = NSFC_PR_SendFile(socket, fd, finfo, headers, hdrlen,
                                  trailers, tlrlen, timeout, 
                                  sendfileSize, statusInfo);
        }
    }
   
done:
    /* the transmit file size has been changed, caller can check this
     * status
     */
    if (rv >= 0 && rv != (hdrlen + finfo->pr.size + tlrlen)) {
        NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_FILESIZE);
        NSFC_DeleteEntry(cache, nep, PR_FALSE);
    }

    return rv;
}

PR_IMPLEMENT(PRInt32)
NSFC_MD_GetPageSize()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    PR_ASSERT(si.dwPageSize > 0);
    return si.dwPageSize;
}
