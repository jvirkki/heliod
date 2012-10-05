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
 * _xpatomic_msvc.h
 *
 * Atomic operation inlines for Microsoft Visual C++.
 */

#ifndef _XP_XPATOMIC_H
#error This file should only be included by xpatomic.h
#endif

#include "_xpatomic_pr.h"

/*
 * Suppress warnings about missing return values.
 */
#pragma warning(disable:4035)

XP_INLINE XPUint32 XP_AtomicCompareAndSwap32(volatile XPUint32 *target, XPUint32 cmpvalue, XPUint32 newvalue)
{
    __asm {
        mov     esi, [target]
        mov     eax, dword ptr [cmpvalue]
        mov     ebx, dword ptr [newvalue]
        lock cmpxchg [esi], ebx
        // return old value in eax
    }
}

XP_INLINE XPUint64 XP_AtomicAdd64(volatile XPUint64 *target, XPInt64 delta)
{
    __asm {
        mov     esi, [target]
        mov     eax, dword ptr [esi]
        mov     edx, dword ptr [esi+4]
l1:
        mov     ebx, dword ptr [delta]
        mov     ecx, dword ptr [delta+4]
        add     ebx, eax
        adc     ecx, edx
        lock cmpxchg8b [esi]
        jne     l1
        mov     eax, ebx
        mov     edx, ecx
        // return new value in edx:eax
    }
}

XP_INLINE XPUint64 XP_AtomicDecrement64(volatile XPUint64 *target)
{
    __asm {
        mov     esi, [target]
        mov     eax, dword ptr [esi]
        mov     edx, dword ptr [esi+4]
l1:
        mov     ebx, -1
        mov     ecx, -1
        add     ebx, eax
        adc     ecx, edx
        lock cmpxchg8b [esi]
        jne     l1
        mov     eax, ebx
        mov     edx, ecx
        // return new value in edx:eax
    }
}

XP_INLINE XPUint64 XP_AtomicIncrement64(volatile XPUint64 *target)
{
    __asm {
        mov     esi, [target]
        mov     eax, dword ptr [esi]
        mov     edx, dword ptr [esi+4]
l1:
        mov     ebx, 1
        xor     ecx, ecx
        add     ebx, eax
        adc     ecx, edx
        lock cmpxchg8b [esi]
        jne     l1
        mov     eax, ebx
        mov     edx, ecx
        // return new value in edx:eax
    }
}

XP_INLINE XPUint64 XP_AtomicSwap64(volatile XPUint64 *target, XPUint64 newvalue)
{
    __asm {
        mov     esi, [target]
        mov     ebx, dword ptr [newvalue]
        mov     ecx, dword ptr [newvalue+4]
        mov     eax, dword ptr [esi]
        mov     edx, dword ptr [esi+4]
l1:
        lock cmpxchg8b [esi]
        jne     l1
        // return old value in edx:eax
    }
}

XP_INLINE XPUint64 XP_AtomicCompareAndSwap64(volatile PRUint64 *target, PRUint64 cmpvalue, PRUint64 newvalue)
{
    __asm {
        mov     esi, [target]
        mov     eax, dword ptr [cmpvalue]
        mov     edx, dword ptr [cmpvalue+4]
        mov     ebx, dword ptr [newvalue]
        mov     ecx, dword ptr [newvalue+4]
        lock cmpxchg8b [esi]
        // return old value in edx:eax
    }
}

XP_INLINE XPUint64 XP_AtomicLoad64(volatile XPUint64 *target)
{
    __asm {
        mov     esi, [target]
        mov     eax, ebx
        mov     edx, ecx
        lock cmpxchg8b [esi]
        // return old value in edx:eax
    }
}

/*
 * Re-enable warnings about missing return values.
 */
#pragma warning(default:4035)

XP_INLINE void XP_ProducerMemoryBarrier(void)
{
    __asm {
        lock xor [esp], 0
    }
}

XP_INLINE void XP_ConsumerMemoryBarrier(void)
{
    __asm {
        lock xor [esp], 0
    }
}
