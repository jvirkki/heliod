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
 * crit.c: Critical section abstraction. Used in threaded servers to protect
 *         areas where two threads can interfere with each other.
 *
 *         Condvars are condition variables that are used for thread-thread 
 *         synchronization.
 * 
 * Rob McCool
 */

#include "systems.h"

#include "netsite.h"
#include "crit.h"
#include "pool.h"
#include "ereport.h"

#include "base/dbtbase.h"

#include "prlock.h"
#include "prcvar.h"
#include "prthread.h"

/*
 * Defined to replace PR_Monitor() with PR_Lock().
 */
typedef struct critical {
	PRLock		*lock;
	PRUint32	count;
	PRThread	*owner;
} critical_t;

typedef struct condvar {
	critical_t	*lock;
	PRCondVar	*cvar;
} condvar_t;

/* -------------------------- critical sections --------------------------- */

/* Useful for ASSERTs only. Returns non-zero if the current thread is the owner.
 */
NSAPI_PUBLIC int crit_owner_is_me(CRITICAL id)
{
    critical_t *crit = (critical_t*)id;

    return (crit->owner == PR_GetCurrentThread());
}

NSAPI_PUBLIC CRITICAL crit_init(void)
{
    critical_t *crit = (critical_t*)PERM_MALLOC(sizeof(critical_t)) ;

    if (crit) {
        if (!(crit->lock = PR_NewLock())) {
            PERM_FREE(crit);
            return NULL;
        }
        crit->count = 0;
        crit->owner = 0;
    }
    return (void *) crit;
}

NSAPI_PUBLIC void crit_enter(CRITICAL id)
{
    critical_t *crit = (critical_t*)id;
    PRThread *me = PR_GetCurrentThread();

    if ( crit->owner == me) {
        NS_ASSERT(crit->count > 0);
        crit->count++;
    } 
    else {
        PR_Lock(crit->lock);
        NS_ASSERT(crit->count == 0);
        crit->count = 1;
        crit->owner = me;
    }
}

NSAPI_PUBLIC void crit_exit(CRITICAL id)
{
    critical_t	*crit = (critical_t*)id;

    if (crit->owner != PR_GetCurrentThread()) 
        return;

    if ( --crit->count == 0) {
        crit->owner = 0;
        PR_Unlock(crit->lock);
    }
    NS_ASSERT(crit->count >= 0);
}

NSAPI_PUBLIC void crit_terminate(CRITICAL id)
{
    critical_t	*crit = (critical_t*)id;

    PR_DestroyLock((PRLock*)crit->lock);
    PERM_FREE(crit);
}


/* ------------------------- condition variables -------------------------- */


NSAPI_PUBLIC CONDVAR condvar_init(CRITICAL id)
{
    critical_t	*crit = (critical_t*)id;

    condvar_t *cvar = (condvar_t*)PERM_MALLOC(sizeof(condvar_t)) ;

    if (crit) {
        cvar->lock = crit;
        if ((cvar->cvar = PR_NewCondVar((PRLock *)cvar->lock->lock)) == 0) {
            PERM_FREE(cvar);
            return NULL;
        }
    }
    return (void *) cvar;
}

NSAPI_PUBLIC void condvar_wait(CONDVAR _cv)
{
    condvar_t *cv = (condvar_t *)_cv;
    /* Save away recursion count so we can restore it after the wait */
    int saveCount = cv->lock->count;
    PRThread *saveOwner = cv->lock->owner;

    NS_ASSERT(cv->lock->owner == PR_GetCurrentThread());
    cv->lock->count = 0;
    cv->lock->owner = 0;

    PR_WaitCondVar(cv->cvar, PR_INTERVAL_NO_TIMEOUT);

    cv->lock->count = saveCount;
    cv->lock->owner = saveOwner;
}

NSAPI_PUBLIC void condvar_timed_wait(CONDVAR _cv, long secs)
{
    condvar_t *cv = (condvar_t *)_cv;
    /* Save away recursion count so we can restore it after the wait */
    int saveCount = cv->lock->count;
    PRThread *saveOwner = cv->lock->owner;
 
    NS_ASSERT(cv->lock->owner == PR_GetCurrentThread());
    cv->lock->count = 0;
    cv->lock->owner = 0;
    
    PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;
    if (secs > 0) 
	timeout = PR_SecondsToInterval(secs); 
    PR_WaitCondVar(cv->cvar, timeout);
 
    cv->lock->count = saveCount;
    cv->lock->owner = saveOwner;
}

NSAPI_PUBLIC void condvar_notify(CONDVAR _cv)
{
    condvar_t *cv = (condvar_t *)_cv;
    NS_ASSERT(cv->lock->owner == PR_GetCurrentThread());
    PR_NotifyCondVar(cv->cvar);
}

NSAPI_PUBLIC void condvar_notifyAll(CONDVAR _cv)
{
    condvar_t *cv = (condvar_t *)_cv;
    NS_ASSERT(cv->lock->owner == PR_GetCurrentThread());
    PR_NotifyAllCondVar(cv->cvar);
}

NSAPI_PUBLIC void condvar_terminate(CONDVAR _cv)
{
    condvar_t *cv = (condvar_t *)_cv;
    PR_DestroyCondVar(cv->cvar);
    PERM_FREE(cv);
}

/* ----------------------- Counting semaphores ---------------------------- */
/* These are currently not listed in crit.h because they aren't yet used; 
 * although they do work.
 * XXXMB - these should be in NSPR.  
 */

typedef struct counting_sem_t {
	CRITICAL lock;
	CRITICAL cv_lock;
	CONDVAR cv;
	int count;
	int max;
} counting_sem_t;

NSAPI_PUBLIC COUNTING_SEMAPHORE
cs_init(int initial_count)
{
	counting_sem_t *cs = (counting_sem_t *)PERM_MALLOC(sizeof(counting_sem_t));

	cs->count = initial_count;
	cs->lock = crit_init();
	cs->cv_lock = crit_init();
	cs->cv = condvar_init(cs->cv_lock);

	return (COUNTING_SEMAPHORE)cs;
}

NSAPI_PUBLIC void
cs_terminate(COUNTING_SEMAPHORE csp)
{
	counting_sem_t *cs = (counting_sem_t *)csp;

	condvar_terminate(cs->cv);
	crit_terminate(cs->cv_lock);
	crit_terminate(cs->lock);
	PERM_FREE(cs);
}

NSAPI_PUBLIC int
cs_wait(COUNTING_SEMAPHORE csp)
{
	counting_sem_t *cs = (counting_sem_t *)csp;
	int ret;

	crit_enter(cs->lock);
	while ( cs->count == 0 ) {
		crit_enter(cs->cv_lock);
		crit_exit(cs->lock);
		condvar_wait(cs->cv);
		crit_exit(cs->cv_lock);
		crit_enter(cs->lock);
	}
	ret = --(cs->count);
	crit_exit(cs->lock);

	return 0;
}

NSAPI_PUBLIC int
cs_trywait(COUNTING_SEMAPHORE csp)
{
        counting_sem_t *cs = (counting_sem_t *)csp;
        int ret;

        crit_enter(cs->lock);
        if (cs->count > 0) {
                ret = --(cs->count);
                crit_exit(cs->lock);
                return 0;
        }
        crit_exit(cs->lock);
        return -1;
}

NSAPI_PUBLIC int 
cs_release(COUNTING_SEMAPHORE csp)
{
	counting_sem_t *cs = (counting_sem_t *)csp;
	int ret;

	crit_enter(cs->lock);
	ret = ++(cs->count);
	if (cs->count == 1) {
		crit_enter(cs->cv_lock);
		condvar_notify(cs->cv);
		crit_exit(cs->cv_lock);
	}
	crit_exit(cs->lock);

	return 0;
}
