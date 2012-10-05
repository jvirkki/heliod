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

#ifndef _XP_XPSYNCH_H
#define _XP_XPSYNCH_H

/*
 * xpsynch.h
 *
 * Declarations for synchronization primitives.
 */

#ifndef _XP_XPPLATFORM_H
#include "xpplatform.h"
#endif
#ifndef _XP_XPTYPES_H
#include "xptypes.h"
#endif
#ifndef _XP_XPTIME_H
#include "xptime.h"
#endif
#if defined(__sun)
#include "_xpsynch_sunos.h"
#else
#include "_xpsynch_pr.h"
#endif

/*
 * XPLock
 *
 * An XPLock is a mutual exclusion lock.  An XPLock must be initialized with
 * XP_InitLock before it can be used and can be destroyed with XP_DestroyLock
 * when it is no longer needed.
 */
typedef struct _XPLock XPLock;

/*
 * XPCondition
 *
 * An XPCondition is a condition variable.  An XPCondition must be initialized
 * with XP_InitCondition before it can be used and can be destroyed with
 * XP_DestroyCondition when it is no longer needed.
 */
typedef struct _XPCondition XPCondition;

/*
 * XP_ASSERT_LOCK_HELD
 *
 * Assert that a mutual exclusion lock is held.  Note that the assertion may
 * not be tested on all platforms.
 */
#if defined(_XP_LOCK_HELD)
#define XP_ASSERT_LOCK_HELD(lock) XP_ASSERT(_XP_LOCK_HELD(lock))
#else
#define XP_ASSERT_LOCK_HELD(lock)
#endif

/*
 * XP_InitLock
 *
 * Initialize a mutual exclusion lock.
 */
XP_INLINE void XP_InitLock(XPLock *lock);

/*
 * XP_DestroyLock
 *
 * Destroy a previously initialized mutual exclusion lock.
 */
XP_INLINE void XP_DestroyLock(XPLock *lock);

/*
 * XP_Lock
 *
 * Acquire a mutual exclusion lock.
 */
XP_INLINE void XP_Lock(XPLock *lock);

/*
 * XP_Unlock
 *
 * Release a mutual exclusion lock.
 */
XP_INLINE void XP_Unlock(XPLock *lock);

/*
 * XP_InitCondition
 *
 * Initialize a condition variable.
 */
XP_INLINE void XP_InitCondition(XPCondition *cv, XPLock *lock);

/*
 * XP_DestroyCondition
 *
 * Destroy a previously initialized condition variable.
 */
XP_INLINE void XP_InitCondition(XPCondition *cv);

/*
 * XP_Wait
 *
 * Wait for a condition variable to be signalled.  The mutual exclusion lock
 * associated with the condition variable must be held; the thread atomically
 * releases the lock and blocks on the condition variable.  The mutual
 * exclusion lock is reacquired before the function returns.  If non-NULL, the
 * remaining value is set to the timeout minus the time spent waiting.
 * Returns XP_SUCCESS if the condition variable was signalled and XP_FAILURE
 * if the timeout elapsed.
 */
XP_INLINE XPStatus XP_Wait(XPCondition *cv, XPInterval timeout, XPInterval *remaining);

/*
 * XP_Signal
 *
 * Signal one waiter on a condition variable.  The mutual exclusion lock
 * associated with the condition variable must be held.
 */
XP_INLINE void XP_Signal(XPCondition *cv);

/*
 * XP_Broadcast
 *
 * Signal all waiters on a condition variable.  The mutual exclusion lock
 * associated with the condition variable must be held.
 */
XP_INLINE void XP_Broadcast(XPCondition *cv);

/*
 * XPScopedLock
 *
 * An XPScopedLock is an XPLock mutual exclusion lock that is automatically
 * initialized when it comes into scope and automatically destroyed when it
 * goes out of scope.
 */
#if defined(__cplusplus)
class XPScopedLock : public XPLock {
public:
    XPScopedLock() { XP_InitLock(this); }
    ~XPScopedLock() { XP_DestroyLock(this); }
private:
    XPScopedLock(const XPScopedLock&);
    XPScopedLock& operator=(const XPScopedLock&);
};
#endif

/*
 * XPScopedCondition
 *
 * An XPScopedCondition is an XPCondition condition variable that is
 * automatically initialized when it comes into scope and automatically
 * destroyed when it goes out of scope.
 */
#if defined(__cplusplus)
class XPScopedCondition : public XPCondition {
public:
    XPScopedCondition(XPLock& l) { XP_InitCondition(this, &l); }
    ~XPScopedCondition() { XP_DestroyCondition(this); }
private:
    XPScopedCondition(const XPScopedCondition&);
    XPScopedCondition& operator=(const XPScopedCondition&);
};
#endif

#endif /* _XP_XPSYNCH_H */
