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
 * xpatomic.cpp
 *
 * Atomic operation implementations.
 */

#include <stdio.h>
#include "prthread.h"
#include "xpatomic.h"
#include "xpsynch.h"
#include "xpunit.h"

#if defined(_XP_XPATOMIC_LOCKED)

/*
 * _XP_XPATOMIC_LOCKED is defined on platforms that lack suitable atomic
 * primitives (i.e. HP-UX on PA-RISC).  On these platforms, we use a hash
 * table of locks to implement atomic operations.
 */

static const int LOCK_HT_MASK = 0x1f;

static XPScopedLock *lock_ht = new XPScopedLock[LOCK_HT_MASK + 1];

static inline size_t HashAddress(const volatile void *target)
{
    return (size_t) target / 17;
}

static inline int LockAddress(volatile void *target)
{
    int i = HashAddress(target) & LOCK_HT_MASK;
    XP_Lock(&lock_ht[i]);
    return i;
}

static inline void UnlockAddressIndex(int i)
{
    XP_Unlock(&lock_ht[i]);
}

XP_EXPORT XPUint32 _XP_LockedAdd32(volatile XPUint32 *target, XPInt32 delta)
{
    XPUint32 rv;
    int i = LockAddress(target);
    *target += delta;
    rv = *target;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint32 _XP_LockedDecrement32(volatile XPUint32 *target)
{
    XPUint32 rv;
    int i = LockAddress(target);
    (*target)--;
    rv = *target;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint32 _XP_LockedIncrement32(volatile XPUint32 *target)
{
    XPUint32 rv;
    int i = LockAddress(target);
    (*target)++;
    rv = *target;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint32 _XP_LockedSwap32(volatile XPUint32 *target, XPUint32 newvalue)
{
    XPUint32 rv;
    int i = LockAddress(target);
    rv = *target;
    *target = newvalue;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint32 _XP_LockedCompareAndSwap32(volatile XPUint32 *target, XPUint32 cmpvalue, XPUint32 newvalue)
{
    XPUint32 rv;
    int i = LockAddress(target);
    rv = *target;
    if (rv == cmpvalue)
        *target = newvalue;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint32 _XP_LockedLoad32(volatile XPUint32 *target)
{
    XPUint32 rv;
    int i = LockAddress(target);
    rv = *target;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint64 _XP_LockedAdd64(volatile XPUint64 *target, XPInt64 delta)
{
    XPUint64 rv;
    int i = LockAddress(target);
    *target += delta;
    rv = *target;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint64 _XP_LockedDecrement64(volatile XPUint64 *target)
{
    XPUint64 rv;
    int i = LockAddress(target);
    (*target)--;
    rv = *target;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint64 _XP_LockedIncrement64(volatile XPUint64 *target)
{
    XPUint64 rv;
    int i = LockAddress(target);
    (*target)++;
    rv = *target;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint64 _XP_LockedSwap64(volatile XPUint64 *target, XPUint64 newvalue)
{
    XPUint64 rv;
    int i = LockAddress(target);
    rv = *target;
    *target = newvalue;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint64 _XP_LockedCompareAndSwap64(volatile XPUint64 *target, XPUint64 cmpvalue, XPUint64 newvalue)
{
    XPUint64 rv;
    int i = LockAddress(target);
    rv = *target;
    if (rv == cmpvalue)
        *target = newvalue;
    UnlockAddressIndex(i);
    return rv;
}

XP_EXPORT XPUint64 _XP_LockedLoad64(volatile XPUint64 *target)
{
    XPUint64 rv;
    int i = LockAddress(target);
    rv = *target;
    UnlockAddressIndex(i);
    return rv;
}

#endif /* _XP_XPATOMIC_LOCKED */

#if defined(_XP_XPATOMIC_MEMORY_BARRIER_IS_FUNCTION_CALL)

XP_EXPORT void _XP_FunctionCall(void)
{
    /*
     * On platforms that guarantee sequential ordering at the hardware level
     * (e.g. HP-UX PA-RISC 2.0), we still need to keep the compiler from
     * reordering memory accesses.  The presence of any function call is
     * enough for that.
     */
}

#endif /* _XP_XPATOMIC_MEMORY_BARRIER_IS_FUNCTION_CALL */

#if defined(DEBUG)

#define NUM_UNIT_TEST_THREADS 4
#define NUM_UNIT_TEST_ITERATIONS 16384
#if defined(LINUX) || defined(AIX)
#define CONST_VALUE(x)  x##LL 
#else
#define CONST_VALUE(x)  x
#endif


static XPScopedLock unit_test_lock;
static int unit_test_start;
static XPScopedCondition unit_test_cv(unit_test_lock);
static volatile XPUint32 unit_test_add_32 = 0xfedcba98;
static volatile XPUint64 unit_test_add_64 = CONST_VALUE(0xfedcba9876543210);
static volatile XPUint32 unit_test_swap_32 = 0xdeadc0de;
static volatile XPUint64 unit_test_swap_64 = CONST_VALUE(0xdeadc0dedeadc0de);
static volatile XPUint32 unit_test_cas_32 = 0xdeadc0de;
static volatile XPUint64 unit_test_cas_64 = CONST_VALUE(0xdeadc0dedeadc0de);

XP_EXTERN_C void AtomicUnitTestThread(void *arg)
{
    XPUint64 value_64;
    XPUint32 value_32;
    int i = 0;

    XP_Lock(&unit_test_lock);
    while (!unit_test_start)
        XP_Wait(&unit_test_cv, XP_INTERVAL_INFINITE, NULL);
    XP_Unlock(&unit_test_lock);

    for (i = 0; i < NUM_UNIT_TEST_ITERATIONS; i++) {
        value_64 = XP_AtomicSwap64(&unit_test_swap_64, CONST_VALUE(0xdeadc0dedeadc0de));
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        value_64 = XP_AtomicLoad64(&unit_test_swap_64);
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));       

        value_64 = XP_AtomicSwap64(&unit_test_swap_64, CONST_VALUE(0xa5a5a5a5a5a5a5a5));
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        value_64 = XP_AtomicLoad64(&unit_test_swap_64);
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));       

        value_64 = XP_AtomicSwap64(&unit_test_swap_64, CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        value_64 = XP_AtomicLoad64(&unit_test_swap_64);
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));
    }

    for (i = 0; i < NUM_UNIT_TEST_ITERATIONS; i++) {
        value_64 = XP_AtomicCompareAndSwap64(&unit_test_cas_64, CONST_VALUE(0xdeadc0dedeadc0de), CONST_VALUE(0xa5a5a5a5a5a5a5a5));
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        value_64 = XP_AtomicLoad64(&unit_test_cas_64);
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));

        value_64 = XP_AtomicCompareAndSwap64(&unit_test_cas_64, CONST_VALUE(0xa5a5a5a5a5a5a5a5), CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        value_64 = XP_AtomicLoad64(&unit_test_cas_64);
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));

        value_64 = XP_AtomicCompareAndSwap64(&unit_test_cas_64, CONST_VALUE(0xa5a5a5a5a5a5a5a5), CONST_VALUE(0xdeadc0dedeadc0de));
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        value_64 = XP_AtomicLoad64(&unit_test_cas_64);
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));

        value_64 = XP_AtomicCompareAndSwap64(&unit_test_cas_64, CONST_VALUE(0x5a5a5a5a5a5a5a5a), CONST_VALUE(0xdeadc0dedeadc0de));
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a));
        value_64 = XP_AtomicLoad64(&unit_test_cas_64);
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a)) ;

        value_64 = XP_AtomicCompareAndSwap64(&unit_test_cas_64, CONST_VALUE(0x1111111111111111), CONST_VALUE(0x2222222222222222));
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a)) ;
        value_64 = XP_AtomicLoad64(&unit_test_cas_64);
        XP_ASSERT(value_64 == CONST_VALUE(0xdeadc0dedeadc0de) || value_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || value_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a)) ;
    }

    for (i = 0; i < NUM_UNIT_TEST_ITERATIONS; i++) {
        value_64 = XP_AtomicDecrement64(&unit_test_add_64);
        value_64 = XP_AtomicAdd64(&unit_test_add_64, CONST_VALUE(0x4321000000000001));
        value_64 = XP_AtomicAdd64(&unit_test_add_64, 6);
        value_64 = XP_AtomicIncrement64(&unit_test_add_64);
        value_64 = XP_AtomicAdd64(&unit_test_add_64, -2);
        value_64 = XP_AtomicAdd64(&unit_test_add_64, CONST_VALUE(-0x4321000000000002));
        value_64 = XP_AtomicAdd64(&unit_test_add_64, -3);
    }

    for (i = 0; i < NUM_UNIT_TEST_ITERATIONS; i++) {
        value_32 = XP_AtomicSwap32(&unit_test_swap_32, 0xdeadc0de);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
        value_32 = XP_AtomicLoad32(&unit_test_swap_32);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);

        value_32 = XP_AtomicSwap32(&unit_test_swap_32, 0xa5a5a5a5);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
        value_32 = XP_AtomicLoad32(&unit_test_swap_32);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);

        value_32 = XP_AtomicSwap32(&unit_test_swap_32, 0x5a5a5a5a);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
        value_32 = XP_AtomicLoad32(&unit_test_swap_32);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
    }

    for (i = 0; i < NUM_UNIT_TEST_ITERATIONS; i++) {
        value_32 = XP_AtomicCompareAndSwap32(&unit_test_cas_32, 0xdeadc0de, 0xa5a5a5a5);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
        value_32 = XP_AtomicLoad32(&unit_test_cas_32);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);

        value_32 = XP_AtomicCompareAndSwap32(&unit_test_cas_32, 0xa5a5a5a5, 0x5a5a5a5a);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
        value_32 = XP_AtomicLoad32(&unit_test_cas_32);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);

        value_32 = XP_AtomicCompareAndSwap32(&unit_test_cas_32, 0xa5a5a5a5, 0xdeadc0de);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
        value_32 = XP_AtomicLoad32(&unit_test_cas_32);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);

        value_32 = XP_AtomicCompareAndSwap32(&unit_test_cas_32, 0x5a5a5a5a, 0xdeadc0de);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
        value_32 = XP_AtomicLoad32(&unit_test_cas_32);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);

        value_32 = XP_AtomicCompareAndSwap32(&unit_test_cas_32, 0x11111111, 0x22222222);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
        value_32 = XP_AtomicLoad32(&unit_test_cas_32);
        XP_ASSERT(value_32 == 0xdeadc0de || value_32 == 0xa5a5a5a5 || value_32 == 0x5a5a5a5a);
    }

    for (i = 0; i < NUM_UNIT_TEST_ITERATIONS; i++) {
        XP_AtomicDecrement32(&unit_test_add_32);
        XP_AtomicAdd32(&unit_test_add_32, 6);
        XP_AtomicAdd32(&unit_test_add_32, 0x43210001);
        XP_AtomicIncrement32(&unit_test_add_32);
        XP_AtomicAdd32(&unit_test_add_32, -2);
        XP_AtomicAdd32(&unit_test_add_32, -0x43210002);
        XP_AtomicAdd32(&unit_test_add_32, -3);
    }
}

XP_UNIT_TEST(xpatomic)
{
    PRThread *threads[NUM_UNIT_TEST_THREADS];
    int i;

    for (i = 0; i < NUM_UNIT_TEST_THREADS; i++) {
        threads[i] = PR_CreateThread(PR_USER_THREAD,
                                     AtomicUnitTestThread,
                                     NULL,
                                     PR_PRIORITY_NORMAL,
                                     PR_GLOBAL_BOUND_THREAD,
                                     PR_JOINABLE_THREAD,
                                     0);
    }

    XP_Lock(&unit_test_lock);
    unit_test_start = 1;
    XP_Broadcast(&unit_test_cv);
    XP_Unlock(&unit_test_lock);

    for (i = 0; i < NUM_UNIT_TEST_THREADS; i++)
        PR_JoinThread(threads[i]);

    XP_ASSERT(unit_test_swap_64 == CONST_VALUE(0xdeadc0dedeadc0de) || unit_test_swap_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || unit_test_swap_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a)) ;
    XP_ASSERT(unit_test_cas_64 == CONST_VALUE(0xdeadc0dedeadc0de) || unit_test_cas_64 == CONST_VALUE(0xa5a5a5a5a5a5a5a5) || unit_test_cas_64 == CONST_VALUE(0x5a5a5a5a5a5a5a5a)) ;
    XP_ASSERT(unit_test_add_64 == CONST_VALUE(0xfedcba9876543210));
    XP_ASSERT(unit_test_swap_32 == 0xdeadc0de || unit_test_swap_32 == 0xa5a5a5a5 || unit_test_swap_32 == 0x5a5a5a5a);
    XP_ASSERT(unit_test_cas_32 == 0xdeadc0de || unit_test_cas_32 == 0xa5a5a5a5 || unit_test_cas_32 == 0x5a5a5a5a);
    XP_ASSERT(unit_test_add_32 == 0xfedcba98);

    return XP_SUCCESS;
}

#endif /* DEBUG */
