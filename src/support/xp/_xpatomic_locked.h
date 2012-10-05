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
 * _xpatomic_locked.h
 *
 * Atomic operation inlines for platforms that lack suitable atomic primitives.
 */

#ifndef _XP_XPATOMIC_H
#error This file should only be included by xpatomic.h
#endif

#define _XP_XPATOMIC_LOCKED

XP_IMPORT XPUint32 _XP_LockedAdd32(volatile XPUint32 *target, XPInt32 delta);
XP_IMPORT XPUint32 _XP_LockedDecrement32(volatile XPUint32 *target);
XP_IMPORT XPUint32 _XP_LockedIncrement32(volatile XPUint32 *target);
XP_IMPORT XPUint32 _XP_LockedSwap32(volatile XPUint32 *target, XPUint32 newvalue);
XP_IMPORT XPUint32 _XP_LockedLoad32(volatile XPUint32 *target);
XP_IMPORT XPUint32 _XP_LockedCompareAndSwap32(volatile XPUint32 *target, XPUint32 cmpvalue, XPUint32 newvalue);
XP_IMPORT XPUint64 _XP_LockedAdd64(volatile XPUint64 *target, XPInt64 delta);
XP_IMPORT XPUint64 _XP_LockedDecrement64(volatile XPUint64 *target);
XP_IMPORT XPUint64 _XP_LockedIncrement64(volatile XPUint64 *target);
XP_IMPORT XPUint64 _XP_LockedSwap64(volatile XPUint64 *target, XPUint64 newvalue);
XP_IMPORT XPUint64 _XP_LockedCompareAndSwap64(volatile XPUint64 *target, XPUint64 cmpvalue, XPUint64 newvalue);
XP_IMPORT XPUint64 _XP_LockedLoad64(volatile XPUint64 *target);

XP_INLINE XPUint32 XP_AtomicAdd32(volatile XPUint32 *target, XPInt32 delta)
{
    return _XP_LockedAdd32(target, delta);
}

XP_INLINE XPUint32 XP_AtomicDecrement32(volatile XPUint32 *target)
{
    return _XP_LockedDecrement32(target);
}

XP_INLINE XPUint32 XP_AtomicIncrement32(volatile XPUint32 *target)
{
    return _XP_LockedIncrement32(target);
}

XP_INLINE XPUint32 XP_AtomicSwap32(volatile XPUint32 *target, XPUint32 newvalue)
{
    return _XP_LockedSwap32(target, newvalue);
}

XP_INLINE XPUint32 XP_AtomicCompareAndSwap32(volatile XPUint32 *target, XPUint32 cmpvalue, XPUint32 newvalue)
{
    return _XP_LockedCompareAndSwap32(target, cmpvalue, newvalue);
}

XP_INLINE XPUint32 XP_AtomicLoad32(volatile XPUint32 *target)
{
    return _XP_LockedLoad32(target);
}

XP_INLINE XPUint64 XP_AtomicAdd64(volatile XPUint64 *target, XPInt64 delta)
{
    return _XP_LockedAdd64(target, delta);
}

XP_INLINE XPUint64 XP_AtomicDecrement64(volatile XPUint64 *target)
{
    return _XP_LockedDecrement64(target);
}

XP_INLINE XPUint64 XP_AtomicIncrement64(volatile XPUint64 *target)
{
    return _XP_LockedIncrement64(target);
}

XP_INLINE XPUint64 XP_AtomicSwap64(volatile XPUint64 *target, XPUint64 newvalue)
{
    return _XP_LockedSwap64(target, newvalue);
}

XP_INLINE XPUint64 XP_AtomicCompareAndSwap64(volatile XPUint64 *target, XPUint64 cmpvalue, XPUint64 newvalue)
{
    return _XP_LockedCompareAndSwap64(target, cmpvalue, newvalue);
}

XP_INLINE XPUint64 XP_AtomicLoad64(volatile XPUint64 *target)
{
    return _XP_LockedLoad64(target);
}
