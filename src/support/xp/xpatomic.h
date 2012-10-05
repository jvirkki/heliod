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

#ifndef _XP_XPATOMIC_H
#define _XP_XPATOMIC_H

/*
 * xpatomic.h
 *
 * Declarations for atomic operations.
 */

#ifndef _XP_XPPLATFORM_H
#include "xpplatform.h"
#endif
#ifndef _XP_XPTYPES_H
#include "xptypes.h"
#endif
#if defined(_MSC_VER)
#include "_xpatomic_msvc.h"
#elif defined(__GNUC__)
#include "_xpatomic_gcc.h"
#elif defined(__SUNPRO_CC)
#include "_xpatomic_sun.h"
#elif defined(__hppa)
#include "_xpatomic_hppa.h"
#elif defined(_OPENMP)
#include "_xpatomic_omp.h"
#elif defined(AIX)
#include "_xpatomic_hppa.h"
#else
#error Missing atomic operation implementations for this compiler
#endif

/*
 * XP_AtomicLoad32
 *
 * Atomically load an ussigned 32-bit integer.
 */
XP_INLINE XPUint32 XP_AtomicLoad32(volatile XPUint32 *target);

/*
 * XP_AtomicAdd32
 *
 * Atomically add a signed 32-bit delta to the unsigned 32-bit integer at the
 * target address and return the result.
 */
XP_INLINE XPUint32 XP_AtomicAdd32(volatile XPUint32 *target, XPInt32 delta);

/*
 * XP_AtomicDecrement32
 *
 * Atomically decrement the unsigned 32-bit integer at the target address and
 * return the result.
 */
XP_INLINE XPUint32 XP_AtomicDecrement32(volatile XPUint32 *target);

/*
 * XP_AtomicIncrement32
 *
 * Atomically increment the unsigned 32-bit integer at the target address and
 * return the result.
 */
XP_INLINE XPUint32 XP_AtomicIncrement32(volatile XPUint32 *target);

/*
 * XP_AtomicSwap32
 *
 * Atomically set the unsigned 32-bit integer at the target address to a new
 * value and return its old value.
 */
XP_INLINE XPUint32 XP_AtomicSwap32(volatile XPUint32 *target, XPUint32 newvalue);

/*
 * XP_AtomicCompareAndSwap32
 *
 * Atomically set the unsigned 32-bit integer at the target address to a new
 * value if its old value matches the specified value.  Always returns the old
 * value, regardless of whether the new value was set.
 */
XP_INLINE XPUint32 XP_AtomicCompareAndSwap32(volatile XPUint32 *target, XPUint32 cmpvalue, XPUint32 newvalue);

/*
 * XP_AtomicLoad64
 *
 * Atomically load an unsigned 64-bit integer.
 */
XP_INLINE XPUint64 XP_AtomicLoad64(volatile XPUint64 *target);

/*
 * XP_AtomicAdd64
 *
 * Atomically add a signed 64-bit delta to the unsigned 64-bit integer at the
 * target address and return the result.
 */
XP_INLINE XPUint64 XP_AtomicAdd64(volatile XPUint64 *target, XPInt64 delta);

/*
 * XP_AtomicDecrement64
 *
 * Atomically decrement the unsigned 64-bit integer at the target address and
 * return the result.
 */
XP_INLINE XPUint64 XP_AtomicDecrement64(volatile XPUint64 *target);

/*
 * XP_AtomicIncrement64
 *
 * Atomically increment the unsigned 64-bit integer at the target address and
 * return the result.
 */
XP_INLINE XPUint64 XP_AtomicIncrement64(volatile XPUint64 *target);

/*
 * XP_AtomicSwap64
 *
 * Atomically set the unsigned 64-bit integer at the target address to a new
 * value and return its old value.
 */
XP_INLINE XPUint64 XP_AtomicSwap64(volatile XPUint64 *target, XPUint64 newvalue);

/*
 * XP_AtomicCompareAndSwap64
 *
 * Atomically set the unsigned 64-bit integer at the target address to a new
 * value if its old value matches the specified value.  Always returns the old
 * value, regardless of whether the new value was set.
 */
XP_INLINE XPUint64 XP_AtomicCompareAndSwap64(volatile XPUint64 *target, XPUint64 cmpvalue, XPUint64 newvalue);

/*
 * XP_AtomicSwapPtr
 *
 * Atomically set the pointer at the target address to a new value and return
 * its old value.
 */
XP_INLINE void * XP_AtomicSwapPtr(volatile void *target, void *newvalue)
{
#if defined(XP_PTR32)
    return (void *) XP_AtomicSwap32((volatile XPUint32 *) target, (XPUint32) newvalue);
#endif
#if defined(XP_PTR64)
    return (void *) XP_AtomicSwap64((volatile XPUint64 *) target, (XPUint64) newvalue);
#endif
}

/*
 * XP_AtomicCompareAndSwapPtr
 *
 * Atomically set the pointer at the target address to a new value if its old
 * value matches the specified value.  Always returns the old value,
 * regardless of whether the new value was set.
 */
XP_INLINE void * XP_AtomicCompareAndSwapPtr(volatile void *target, void *cmpvalue, void *newvalue)
{
#if defined(XP_PTR32)
    return (void *) XP_AtomicCompareAndSwap32((volatile XPUint32 *) target, (XPUint32) cmpvalue, (XPUint32) newvalue);
#endif
#if defined(XP_PTR64)
    return (void *) XP_AtomicCompareAndSwap64((volatile XPUint64 *) target, (XPUint64) cmpvalue, (XPUint64) newvalue);
#endif
}

/*
 * XP_ProducerMemoryBarrier
 *
 * Ensure that all previously issued stores complete before any stores issued
 * after the call returns.
 */
XP_INLINE void XP_ProducerMemoryBarrier(void);

/*
 * XP_ConsumerMemoryBarrier
 *
 * Ensure that all previously issued loads complete before any loads issued
 * after the call returns.
 */
XP_INLINE void XP_ConsumerMemoryBarrier(void);

#endif /* _XP_XPATOMIC_H */
