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
 * _xpsynch_sunos.h
 *
 * Synchronization primitive inlines for Solaris.
 */

#ifndef _XP_XPSYNCH_H
#error This file should only be included by xpsynch.h
#endif

#ifndef DEFAULTMUTEX
#include <synch.h>
#endif

struct _XPLock {
    mutex_t mutex;
};

struct _XPCondition {
    cond_t cv;
    struct _XPLock *lock;
};

#define _XP_LOCK_HELD(lock) _XP_LockHeld(lock)

XP_INLINE int _XP_LockHeld(struct _XPLock *lock)
{
    if (mutex_trylock(&lock->mutex))
        return 1;

    mutex_unlock(&lock->mutex);

    return 0;
}

XP_INLINE void XP_InitLock(struct _XPLock *lock)
{
    const mutex_t default_mutex = DEFAULTMUTEX;
 
    lock->mutex = default_mutex;

    XP_ASSERT(!_XP_LockHeld(lock));
}

XP_INLINE void XP_DestroyLock(struct _XPLock *lock)
{
    XP_ASSERT(!_XP_LockHeld(lock));

    mutex_destroy(&lock->mutex);
}

XP_INLINE void XP_Lock(struct _XPLock *lock)
{
    mutex_lock(&lock->mutex);

    XP_ASSERT(_XP_LockHeld(lock));
}

XP_INLINE void XP_Unlock(struct _XPLock *lock)
{
    XP_ASSERT(_XP_LockHeld(lock));

    mutex_unlock(&lock->mutex);
}

XP_INLINE void XP_InitCondition(struct _XPCondition *cv, struct _XPLock *lock)
{
    const cond_t default_cv = DEFAULTCV;
 
    cv->cv = default_cv;
    cv->lock = lock;
}

XP_INLINE void XP_DestroyCondition(struct _XPCondition *cv)
{
    cond_destroy(&cv->cv);
}

XP_INLINE XPStatus XP_Wait(struct _XPCondition *cv, XPInterval timeout, XPInterval *remaining)
{
    int rv;

    XP_ASSERT(_XP_LockHeld(cv->lock));

    if (timeout == XP_INTERVAL_INFINITE) {
        rv = cond_wait(&cv->cv, &cv->lock->mutex);

        if (remaining)
            *remaining = timeout;
    } else if (remaining) {
        XPInterval epoch = gethrtime();

        timestruc_t tv;
        tv.tv_sec = timeout / XP_NANOSECONDS_PER_SECOND;
        tv.tv_nsec = timeout % XP_NANOSECONDS_PER_SECOND;
        rv = cond_reltimedwait(&cv->cv, &cv->lock->mutex, &tv);

        XPInterval elapsed = gethrtime() - epoch;
        if (timeout > elapsed) {
            *remaining = timeout - elapsed;
        } else {
            *remaining = 0;
            rv = -1;
        }
    } else {
        timestruc_t tv;
        tv.tv_sec = timeout / XP_NANOSECONDS_PER_SECOND;
        tv.tv_nsec = timeout % XP_NANOSECONDS_PER_SECOND;
        rv = cond_reltimedwait(&cv->cv, &cv->lock->mutex, &tv);
    }

    XP_ASSERT(_XP_LockHeld(cv->lock));

    return rv ? XP_FAILURE : XP_SUCCESS;
}

XP_INLINE void XP_Signal(struct _XPCondition *cv)
{
    XP_ASSERT(_XP_LockHeld(cv->lock));

    cond_signal(&cv->cv);
}

XP_INLINE void XP_Broadcast(struct _XPCondition *cv)
{
    XP_ASSERT(_XP_LockHeld(cv->lock));

    cond_broadcast(&cv->cv);
}
