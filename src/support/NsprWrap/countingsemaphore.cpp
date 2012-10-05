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
 * Counting semaphore implementation using NSPR
 * Original implementation from Rob McCool
 * Converted to a C++ class by Kayshav Dattatri
 */
#include "countingsemaphore.h"


#include "nspr.h"

typedef void* COUNTING_SEMAPHORE;


/* ----------------------- Counting semaphores ---------------------------- */

#if defined(SOLARIS) && defined(HW_THREADS)
#include <synch.h>
typedef sema_t counting_sem_t;
#elif defined(IRIX) && defined(HW_THREADS)
#include <ulocks.h>
typedef usema_t *counting_sem_t;
#else
typedef struct counting_sem_t {
#ifdef USE_MONITOR
	PRMonitor *monitor;
#else
	PRLock* lock;
	PRLock* cv_lock;
	PRCondVar* cv;
#endif /* USE_MONITOR */
	int count;
	int max;
} counting_sem_t;
#endif

COUNTING_SEMAPHORE
cs_init(int initial_count)
{
	counting_sem_t *cs = (counting_sem_t *)malloc(sizeof(counting_sem_t));
#if defined(SOLARIS) && defined(HW_THREADS)
    if ( sema_init(cs, initial_count, USYNC_THREAD, NULL) < 0) {
		free(cs);
		return NULL;
	}

	return (COUNTING_SEMAPHORE)cs;
#elif defined(IRIX) && defined(HW_THREADS)
	usptr_t *arena;

	usconfig(CONF_INITSIZE, 64*1024);
	if ( (arena = usinit("/tmp/cs.locks")) == NULL) 
		return NULL;

	if ( (cs = (counting_sem_t *)usnewsema(arena, 0)) == NULL)
		return NULL;

	return cs;
	
#else

	cs->count = initial_count;
#ifdef USE_MONITOR
	cs->monitor = PR_NewMonitor();
#else
	cs->lock = PR_NewLock();
	cs->cv_lock = PR_NewLock();
	cs->cv = PR_NewCondVar ( cs->cv_lock);
#endif /* USE_MONITOR */

	return (COUNTING_SEMAPHORE)cs;
#endif
}

void
cs_terminate(COUNTING_SEMAPHORE csp)
{
	counting_sem_t *cs = (counting_sem_t *)csp;

#if defined(SOLARIS) && defined(HW_THREADS)
    if ( sema_destroy(cs) < 0 ) {
	}
    free(cs);
	return;
#elif defined(IRIX) && defined(HW_THREADS)
	/* usfreesema() */
	return;
#else
#ifdef USE_MONITOR
	PR_DestroyMonitor(cs->monitor);
#else
	PR_DestroyCondVar( (PRCondVar *)cs->cv);
	PR_DestroyLock( cs->cv_lock);
	PR_DestroyLock( cs->lock);
#endif /* USE_MONITOR */
	free(cs);

	return;
#endif
}

int
cs_wait(COUNTING_SEMAPHORE csp)
{
	counting_sem_t *cs = (counting_sem_t *)csp;
	int ret;

#if defined(SOLARIS) && defined(HW_THREADS)
    if ( (ret = sema_wait(cs)) < 0 ) {
		return -1;
	}
	return ret;
#elif defined(IRIX) && defined(HW_THREADS)
	uspsema(cs);
	return 0;
#else
#ifdef USE_MONITOR
	PR_EnterMonitor(cs->monitor);
	while (cs->count == 0) {
		PR_Wait(cs->monitor, PR_INTERVAL_NO_TIMEOUT);
	}
	--cs->count;
	PR_ExitMonitor(cs->monitor);
#else
	PR_Lock(cs->lock);
	while ( cs->count == 0 ) {
		PR_Lock(cs->cv_lock);
		PR_Unlock(cs->lock);
		PR_WaitCondVar((PRCondVar*)cs->cv, PR_INTERVAL_NO_TIMEOUT);
		PR_Unlock(cs->cv_lock);
		PR_Lock(cs->lock);
	}
	ret = --(cs->count);
	PR_Unlock(cs->lock);
#endif /* USE_MONITOR */

	return 0;
#endif
}

int
cs_trywait(COUNTING_SEMAPHORE csp)
{
	counting_sem_t *cs = (counting_sem_t *)csp;
	int ret;

#if defined(SOLARIS) && defined(HW_THREADS)
	ret = sema_trywait(cs)?-1:0;
	return ret;
#elif defined(IRIX) && defined(HW_THREADS)
	ret = uscpsema(cs);
	return (ret == 1)?0:-1;
#else
#ifdef USE_MONITOR
	ret = -1;
	PR_EnterMonitor(cs->monitor);
	if (cs->count > 0) {
		--cs->count;
		ret = 0;
	}
	PR_ExitMonitor(cs->monitor);
	return ret;
#else
	PR_Lock(cs->lock);
	if (cs->count > 0) {
		ret = --(cs->count);
		PR_Unlock(cs->lock);
		return 0;
	}
	PR_Unlock(cs->lock);
	return -1;
#endif /* USE_MONITOR */
#endif
}

int 
cs_release(COUNTING_SEMAPHORE csp)
{
	counting_sem_t *cs = (counting_sem_t *)csp;
	int ret;

#if defined(SOLARIS) && defined(HW_THREADS)
    if ( (ret = sema_post(cs)) < 0 ) {
		return -1;
	}
	return ret;
#elif defined(IRIX) && defined(HW_THREADS)
	usvsema(cs);
	return 0;
#else
#ifdef USE_MONITOR
	PR_EnterMonitor(cs->monitor);
	++cs->count;
	PR_Notify(cs->monitor);
	PR_ExitMonitor(cs->monitor);
	return 0;
#else
	PR_Lock(cs->lock);
	ret = ++(cs->count);
	if (cs->count == 1) {
		PR_Lock(cs->cv_lock);
		PR_NotifyCondVar((PRCondVar*)cs->cv);
		PR_Unlock(cs->cv_lock);
	}
	PR_Unlock(cs->lock);

	return 0;
#endif /* USE_MONITOR */
#endif
}


CountingSemaphore::CountingSemaphore(int initialCount /* = 1 */)
{
	_sem = cs_init(initialCount);

}

CountingSemaphore::~CountingSemaphore()
{
	cs_terminate(_sem);
}

void CountingSemaphore::acquire()
{
	cs_wait(_sem);
}

 // If the acquire was successful, return 1. Otherwise return 0
int CountingSemaphore::try_acquire()
{
	return (cs_trywait(_sem)==0) ? 1 : 0;
}

void CountingSemaphore::release()
{
	cs_release(_sem);
}

