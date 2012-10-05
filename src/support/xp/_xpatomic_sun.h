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
 * _xpatomic_sun.h
 *
 * Atomic operation inlines for the Sun C and Sun C++ compilers.
 */

#ifndef _XP_XPATOMIC_H
#error This file should only be included by xpatomic.h
#endif

#include "_xpatomic_pr.h"

/*
 * As of Solaris 10, the following functions are built into libc and described
 * in atomic_ops(3C).  However, we use our own private definitions in order to
 * support earlier Solaris releases.  On Solaris 10, our symbols will take
 * precedence over the weak libc symbols with the same names.
 */
XP_EXTERN_C XPUint32 atomic_cas_32(volatile XPUint32 *, XPUint32, XPUint32);
XP_EXTERN_C XPUint64 atomic_add_64_nv(volatile XPUint64 *, XPInt64);
XP_EXTERN_C XPUint64 atomic_dec_64_nv(volatile XPUint64 *);
XP_EXTERN_C XPUint64 atomic_inc_64_nv(volatile XPUint64 *);
XP_EXTERN_C XPUint64 atomic_swap_64(volatile XPUint64 *, XPUint64);
XP_EXTERN_C XPUint64 atomic_cas_64(volatile XPUint64 *, XPUint64, XPUint64);
XP_EXTERN_C XPUint64 atomic_load_64(volatile XPUint64 *);
XP_EXTERN_C void membar_producer(void);
XP_EXTERN_C void membar_consumer(void);

XP_INLINE XPUint32 XP_AtomicCompareAndSwap32(volatile XPUint32 *target, XPUint32 cmpvalue, XPUint32 newvalue)
{
    return atomic_cas_32(target, cmpvalue, newvalue);
}

XP_INLINE XPUint64 XP_AtomicAdd64(volatile XPUint64 *target, XPInt64 delta)
{
    return atomic_add_64_nv(target, delta);
}

XP_INLINE XPUint64 XP_AtomicDecrement64(volatile XPUint64 *target)
{
    return atomic_dec_64_nv(target);
}

XP_INLINE XPUint64 XP_AtomicIncrement64(volatile XPUint64 *target)
{
    return atomic_inc_64_nv(target);
}

XP_INLINE XPUint64 XP_AtomicSwap64(volatile XPUint64 *target, XPUint64 newvalue)
{
    return atomic_swap_64(target, newvalue);
}

XP_INLINE XPUint64 XP_AtomicCompareAndSwap64(volatile XPUint64 *target, XPUint64 cmpvalue, XPUint64 newvalue)
{
    return atomic_cas_64(target, cmpvalue, newvalue);
}

XP_INLINE XPUint64 XP_AtomicLoad64(volatile XPUint64 *target)
{
    /*
     * Since we can only guarantee atomicity if *target is contained entirely
     * in a single cache line, we require that all 64-bit values are 64-bit
     * aligned.
     */
    XP_ASSERT(((size_t) target & (sizeof(XPUint64) - 1)) == 0);

#if defined(_LP64)
    return *target;
#else
    return atomic_load_64(target);
#endif
}

XP_INLINE void XP_ProducerMemoryBarrier(void)
{
    membar_producer();
}

XP_INLINE void XP_ConsumerMemoryBarrier(void)
{
    membar_consumer();
}
