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
 * _xpatomic_gcc.h
 *
 * Atomic operation inlines for gcc.
 */

#ifndef _XP_XPATOMIC_H
#error This file should only be included by xpatomic.h
#endif

#include "_xpatomic_pr.h"

XP_INLINE XPUint32 XP_AtomicCompareAndSwap32(volatile XPUint32 *target, XPUint32 cmpvalue, XPUint32 newvalue)
{
    XPUint32 rv;

    __asm__ __volatile__ ("lock\n"
                          "cmpxchgl %3, %1\n"
                          : "=a"(rv)
                          : "m"(*target), "a"(cmpvalue), "r"(newvalue)
                          : "memory");

    return rv;
}

static inline XPUint64 _XP_AtomicAdd3232(volatile XPUint64 *target, unsigned lodelta, int hidelta)
{
    XPUint64 rv;

    __asm__ __volatile__ ("        movl        (%1), %%eax\n"
                          "        movl        4(%1), %%edx\n"
                          "1:\n"
                          "        movl        %2, %%ebx\n"
                          "        movl        %3, %%ecx\n"
                          "        addl        %%eax, %%ebx\n"
                          "        adcl        %%edx, %%ecx\n"
                          "        lock\n"
                          "        cmpxchg8b   (%1)\n"
                          "        jne         1b\n"
                          "        movl        %%ebx, %%eax\n"
                          "        movl        %%ecx, %%edx\n"
                          : "=&A"(rv)
                          : "S"(target), "g"(lodelta), "g"(hidelta)
                          : "ebx", "ecx", "memory");

    return rv;
}

XP_INLINE XPUint64 XP_AtomicAdd64(volatile XPUint64 *target, XPInt64 delta)
{
#if defined(__LP64__)
    XPUint64 rv = delta;

    __asm__ __volatile__ ("lock\n" 
                          "xaddq  %0, %1;\n"
                          : "+r"(rv), "+m"(*target)
                          :
                          : "memory");
    return rv + delta;
#else
    return _XP_AtomicAdd3232(target, delta & 0xffffffff, delta >> 32);
#endif
}

XP_INLINE XPUint64 XP_AtomicDecrement64(volatile XPUint64 *target)
{
#if defined(__LP64__)
    return XP_AtomicAdd64(target, -1);
	
#else
    XPUint64 rv;

    __asm__ __volatile__ ("        movl        (%1), %%eax\n"
                          "        movl        4(%1), %%edx\n"
                          "1:\n"
                          "        movl        $-1, %%ebx\n"
                          "        movl        $-1, %%ecx\n"
                          "        addl        %%eax, %%ebx\n"
                          "        adcl        %%edx, %%ecx\n"
                          "        lock\n"
                          "        cmpxchg8b   (%1)\n"
                          "        jne         1b\n"
                          "        movl        %%ebx, %%eax\n"
                          "        movl        %%ecx, %%edx\n"
                          : "=&A"(rv)
                          : "S"(target)
                          : "ebx", "ecx", "memory");
    return rv;
#endif
}

XP_INLINE XPUint64 XP_AtomicIncrement64(volatile XPUint64 *target)
{
#if defined(__LP64__)
    return XP_AtomicAdd64(target, 1);

#else
    XPUint64 rv;

    __asm__ __volatile__ ("        movl        (%1), %%eax\n"
                          "        movl        4(%1), %%edx\n"
                          "1:\n"
                          "        movl        $1, %%ebx\n"
                          "        xorl        %%ecx, %%ecx\n"
                          "        addl        %%eax, %%ebx\n"
                          "        adcl        %%edx, %%ecx\n"
                          "        lock\n"
                          "        cmpxchg8b   (%1)\n"
                          "        jne         1b\n"
                          "        movl        %%ebx, %%eax\n"
                          "        movl        %%ecx, %%edx\n"
                          : "=&A"(rv)
                          : "S"(target)
                          : "ebx", "ecx", "memory");

    return rv;
#endif
}

static inline XPUint64 _XP_AtomicSwap3232(volatile PRUint64 *target, unsigned lonewvalue, unsigned hinewvalue)
{
    XPUint64 rv;

    __asm__ __volatile__ ("        movl        (%1), %%eax\n"
                          "        movl        4(%1), %%edx\n"
                          "1:\n"
                          "        lock\n"
                          "        cmpxchg8b   (%1)\n"
                          "        jne         1b\n"
                          : "=&A"(rv)
                          : "S"(target), "b"(lonewvalue), "c"(hinewvalue)
                          : "memory");

    return rv;
}

XP_INLINE XPUint64 XP_AtomicSwap64(volatile XPUint64 *target, XPUint64 newvalue)
{
#if defined(__LP64__)
    //Note: no "lock" prefix even on SMP: xchg always implies lock anyway
    __asm__ __volatile__ ("xchgq (%2),%0\n"
                          : "=r"(newvalue)
                          : "0"(newvalue), "r"(target)
                          : "memory");

    return newvalue;
#else
    return _XP_AtomicSwap3232(target, newvalue & 0xffffffff, newvalue >> 32);
#endif
}

static inline XPUint64 _XP_AtomicCompareAndSwap3232(volatile PRUint64 *target, unsigned locmpvalue, unsigned hicmpvalue, unsigned lonewvalue, unsigned hinewvalue)
{
    XPUint64 rv;

    __asm__ __volatile__ ("lock\n"
                          "cmpxchg8b (%1)\n"
                          : "=A"(rv)
                          : "S"(target), "a"(locmpvalue), "d"(hicmpvalue), "b"(lonewvalue), "c"(hinewvalue)
                          : "memory");

    return rv;
}

XP_INLINE XPUint64 XP_AtomicCompareAndSwap64(volatile PRUint64 *target, PRUint64 cmpvalue, PRUint64 newvalue)
{
#if defined(__LP64__)
    XPUint64 rv;

    __asm__ __volatile__ ("lock\n"
                          "cmpxchgq %3, %1\n"
                          : "=a"(rv)
                          : "m"(*target), "a"(cmpvalue), "r"(newvalue)
                          : "memory");
    return rv;
#else
    return _XP_AtomicCompareAndSwap3232(target, cmpvalue & 0xffffffff, cmpvalue >> 32, newvalue & 0xffffffff, newvalue >> 32);
#endif
}

XP_INLINE XPUint64 XP_AtomicLoad64(volatile XPUint64 *target)
{
#if defined(__LP64__)
    return *target;
#else
    XPUint64 rv;

    __asm__ ("movl        %%ebx, %%eax\n"
             "movl        %%ecx, %%edx\n"
             "lock\n"
             "cmpxchg8b   (%1)\n"
             : "=A"(rv)
             : "S"(target));

    return rv;
#endif
}

XP_INLINE void XP_ProducerMemoryBarrier(void)
{
#if defined(__LP64__)
    __asm__ __volatile__ ("sfence\n"
                          : // no output operands
                          : // no input operands
                          : "memory");
#else
    __asm__ __volatile__ ("lock\n"
                          "xorl $0, (%%esp)\n"
                          : // no output operands
                          : // no input operands
                          : "memory");
#endif
}

XP_INLINE void XP_ConsumerMemoryBarrier(void)
{
#if defined(__LP64__)
    __asm__ __volatile__ ("lfence\n"
                          : // no output operands
                          : // no input operands
                          : "memory");
#else
    __asm__ __volatile__ ("lock\n"
                          "xorl $0, (%%esp)\n"
                          : // no output operands
                          : // no input operands
                          : "memory");
#endif
}
