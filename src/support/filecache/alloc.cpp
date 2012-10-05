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

static const PRUint32 NSFCPageMask = ~(NSFC_MD_GetPageSize() - 1);

#define NSFC_ROUNDUPTOPAGE(size) (((size) + ~NSFCPageMask) & NSFCPageMask)

static inline PRBool
_NSFC_ReserveMemory(volatile XPUint64 *cur, XPUint64 max, PRInt32 size)
{
    PR_ASSERT(size >= 0);

    if (*cur + size > max)
        return PR_FALSE;

    XPUint64 rv = XP_AtomicAdd64(cur, size);
    if (rv > max) {
        XP_AtomicAdd64(cur, -size);
        return PR_FALSE;
    }

    return PR_TRUE;
}

static inline void
_NSFC_ReleaseMemory(volatile XPUint64 *cur, PRInt32 size)
{
    PR_ASSERT(size >= 0);

    XP_AtomicAdd64(cur, -size);
}

PR_IMPLEMENT(PRBool)
NSFC_ReserveMmap(NSFCCache cip, PRInt32 size)
{
    return _NSFC_ReserveMemory(&cip->curMmap,
                               cip->cfg.maxMmap,
                               NSFC_ROUNDUPTOPAGE(size));
}

PR_IMPLEMENT(void)
NSFC_ReleaseMmap(NSFCCache cip, PRInt32 size)
{
    _NSFC_ReleaseMemory(&cip->curMmap, NSFC_ROUNDUPTOPAGE(size));
}

PR_IMPLEMENT(void *)
NSFC_Calloc(PRUint32 nelem, PRUint32 elsize, NSFCCache cip)
{
    void *ptr;
    PRUint32 nbytes = nelem*elsize;

    if (cip) {

        /* Keep track of space used */
        if (!_NSFC_ReserveMemory(&cip->curHeap, cip->cfg.maxHeap, nbytes)) {
            return NULL;
        }

        if (cip->memfns) {
            ptr = cip->memfns->alloc(cip->memptr, nbytes);
            if (ptr) {
                memset(ptr, 0, nbytes);
            }
        }
        else {
            ptr = PR_CALLOC(nbytes);
        }

        if (!ptr) {
            _NSFC_ReleaseMemory(&cip->curHeap, nbytes);
        }
    }
    else {
        ptr = PR_CALLOC(nbytes);
    }
    return ptr;
}

PR_IMPLEMENT(void)
NSFC_Free(void *ptr, PRUint32 nbytes, NSFCCache cip)
{
    if (ptr) {
        if (cip) {

            /* Reduce amount of space in use */
            _NSFC_ReleaseMemory(&cip->curHeap, nbytes);

            if (cip->memfns) {
                cip->memfns->free(cip->memptr, ptr);
            }
            else {
                PR_DELETE(ptr);
            }
        }
        else {
            PR_DELETE(ptr);
        }
    }
}

PR_IMPLEMENT(void)
NSFC_FreeStr(char *str, NSFCCache cip)
{
    PRUint32 nbytes;

    if (str) {
        nbytes = strlen(str) + 1;
        NSFC_Free(str, nbytes, cip);
    }
}

PR_IMPLEMENT(void *)
NSFC_Malloc(PRUint32 nbytes, NSFCCache cip)
{
    void *ptr;

    if (cip) {

        /* Keep track of space used */
        if (!_NSFC_ReserveMemory(&cip->curHeap, cip->cfg.maxHeap, nbytes)) {
            return NULL;
        }

        ptr = (cip->memfns)? cip->memfns->alloc(cip->memptr, nbytes)
                           : PR_MALLOC(nbytes);

        if (!ptr) {
            _NSFC_ReleaseMemory(&cip->curHeap, nbytes);
        }
    }
    else {
        ptr = PR_MALLOC(nbytes);
    }
    return ptr;
}

PR_IMPLEMENT(char *)
NSFC_Strdup(const char *str, NSFCCache cip)
{
    PRUint32 nbytes;
    void *ptr = NULL;

    if (str) {

        nbytes = strlen(str) + 1;

        if (cip) {

            /* Keep track of space used */
            if (!_NSFC_ReserveMemory(&cip->curHeap, cip->cfg.maxHeap, nbytes)) {
                return NULL;
            }

            ptr = (cip->memfns)? cip->memfns->alloc(cip->memptr, nbytes)
                               : PR_MALLOC(nbytes);

            if (!ptr) {
                _NSFC_ReleaseMemory(&cip->curHeap, nbytes);
            }
        }
        else {
            ptr = PR_MALLOC(nbytes);
        }

        if (ptr) {
            strcpy((char *)ptr, str);
        }
    }

    return (char *)ptr;
}
