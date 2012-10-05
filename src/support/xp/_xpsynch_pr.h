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
 * _xpsynch_pr.h
 *
 * Synchronization primitive inlines for platforms that use the NSPR
 * implementations.
 */

#ifndef _XP_XPSYNCH_H
#error This file should only be included by xpsynch.h
#endif

#include "prlock.h"
#include "prcvar.h"

struct _XPLock {
    PRLock *lock;
};

struct _XPCondition {
    PRCondVar *cv;
    struct _XPLock *lock;
};

XP_INLINE void XP_InitLock(struct _XPLock *lock)
{
    lock->lock = PR_NewLock();
}

XP_INLINE void XP_DestroyLock(struct _XPLock *lock)
{
    PR_DestroyLock(lock->lock);
}

XP_INLINE void XP_Lock(struct _XPLock *lock)
{
    PR_Lock(lock->lock);
}

XP_INLINE void XP_Unlock(struct _XPLock *lock)
{
    PR_Unlock(lock->lock);
}

XP_INLINE void XP_InitCondition(struct _XPCondition *cv, struct _XPLock *lock)
{
    cv->cv = PR_NewCondVar(lock->lock);
    cv->lock = lock;
}

XP_INLINE void XP_DestroyCondition(struct _XPCondition *cv)
{
    PR_DestroyCondVar(cv->cv);
}

XP_INLINE XPStatus XP_Wait(struct _XPCondition *cv, XPInterval timeout, XPInterval *remaining)
{
    PRStatus rv;

    PRIntervalTime prtimeout = XP_IntervalToPRIntervalTime(timeout);

    if (timeout == XP_INTERVAL_INFINITE) {
        rv = PR_WaitCondVar(cv->cv, prtimeout);

        if (remaining)
            *remaining = timeout;
    } else if (remaining) {
        PRIntervalTime prepoch = PR_IntervalNow();

        rv = PR_WaitCondVar(cv->cv, prtimeout);

        PRIntervalTime prelapsed = PR_IntervalNow() - prepoch;
        if (prtimeout > prelapsed) {
            *remaining = prtimeout - prelapsed;
        } else {
            *remaining = 0;
            rv = PR_FAILURE;
        }
    } else {
        rv = PR_WaitCondVar(cv->cv, prtimeout);
    }

    return (XPStatus) rv;
}

XP_INLINE void XP_Signal(struct _XPCondition *cv)
{
    PR_NotifyCondVar(cv->cv);
}

XP_INLINE void XP_Broadcast(struct _XPCondition *cv)
{
    PR_NotifyAllCondVar(cv->cv);
}
