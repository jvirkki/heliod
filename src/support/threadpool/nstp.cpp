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
*           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
*              NETSCAPE COMMUNICATIONS CORPORATION
* Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
* Reserved.  Use of this Source Code is subject to the terms of the
* applicable license agreement from Netscape Communications Corporation.
* The copyright notice(s) in this Source Code does not indicate actual or
* intended publication of this Source Code.
*/

#include "nstp_pvt.h"

static PRCallOnceType once = { 0 };
static PRLock * NSTPLock = NULL;
static PRIntn   NSTPInstanceCount   = 0;
static NSTPPool NSTPInstanceList    = NULL;
static NSTPPool NSTPDefaultInstance = NULL;
static PRBool	hasTimeout = PR_FALSE;

static PRStatus InitializePoolLock(void)
{
    NSTPLock = PR_NewLock();
    return (NSTPLock) ? PR_SUCCESS : PR_FAILURE;
}

/*
* NSTP_CreatePool - create a new thread pool instance
*/

PR_IMPLEMENT(PRStatus)
NSTP_CreatePool (NSTPPoolConfig *pcfg, NSTPPool *pool)
{
    PRStatus rv = PR_SUCCESS;
    NSTPPool pip = NULL;
	
    /* Do default module initialization if it hasn't been done yet */
    if (!NSTPLock) {
        NSTP_Initialize(NULL, NULL, NULL);
    }
	
    /* Enter global lock */
    PR_Lock(NSTPLock);
	
    /* Pretend loop to avoid goto */
    while (1) {
		
        /* Allocate a new pool instance from the global pool */
        pip = PR_NEWZAP (struct NSTPPool_s);
        
		if (!pip) 
		{
            /* Failed to allocate pool instance structure */
            rv = PR_FAILURE;
            break;
        }
		
        if (pcfg->version != NSTP_API_VERSION) {
			
            /* Unsupported API version */
            PR_DELETE(pip);
            rv = PR_FAILURE;
            break;
        }
		
        /* Copy the configuration parameters into the instance */
        pip->config = *pcfg;
		
        /* Get a new lock for this instance */
        pip->lock = PR_NewLock();
        if (!pip->lock) {
            /* Failed to create lock for new pool instance */
            PR_DELETE(pip);
            rv = PR_FAILURE;
            break;
        }
		
        /* Create a condition variable for the lock */
        pip->cvar = PR_NewCondVar(pip->lock);
        if (!pip->cvar) {
            /* Failed to create condition variable for new pool instance */
            PR_DestroyLock(pip->lock);
            PR_DELETE(pip);
            rv = PR_FAILURE;
            break;
        }
		
        /* Add this instance to the global list of instances */
        pip->next = NSTPInstanceList;
        NSTPInstanceList = pip;
        ++NSTPInstanceCount;
		break;	// ruslan: need to get out of the loop, right Howard :-)?
    }
	
    PR_Unlock(NSTPLock);
	
    /* If that went well, continue initializing the new instance */
    if (rv == PR_SUCCESS) {
		
        /* Create initial threads */
        if (pip->config.initThread > 0) {
            PRThread *thread;
            int i;
			
            for (i = 0; i < pip->config.initThread; ++i) {
				
			/*
			* In solaris, all the threads  which are going to run
			* java needs to be bound thread so that we can reliably
			* get the register state to do GC.
				*/
                thread = PR_CreateThread(PR_USER_THREAD,
					NSTP_ThreadMain,
					(void *)pip,
					PR_PRIORITY_NORMAL,
					PR_GLOBAL_THREAD,
					PR_UNJOINABLE_THREAD,
					pip->config.stackSize);
				
                if (!thread) {
                    /* Failed, so shutdown threads already created */
                    PR_Lock(pip->lock);
                    if (pip->stats.threadCount > 0) {
                        pip->shutdown = PR_TRUE;
                        rv = PR_NotifyAllCondVar(pip->cvar);
                    }
                    PR_Unlock(pip->lock);
                    rv = PR_FAILURE;
                    break;
                }
				
                PR_Lock(pip->lock);
                ++pip->stats.threadCount;
                ++pip->stats.freeCount;
                PR_Unlock(pip->lock);
            }
        } /* initThread > 0 */
		*pool = pip;	// ruslan: need to assign it back
    }
	
    return rv;
}

/*
* NSTP_EnterGlobalLock - enter the thread pool module lock
*
* This function enters the lock associated with the thread pool
* module.
*/
PR_IMPLEMENT(void)
NSTP_EnterGlobalLock (void)
{
    PRStatus rv = PR_CallOnce(&once, InitializePoolLock);
    if (rv == PR_SUCCESS) {
        PR_Lock(NSTPLock);
    }
}

/*
* NSTP_ExitGlobalLock - exit the thread pool module lock
*
* This function exits the lock associated with the thread pool
* module, previously entered via NSTP_EnterGlobalLock().
*/
PR_IMPLEMENT(PRStatus)
NSTP_ExitGlobalLock (void)
{
    return PR_Unlock(NSTPLock);
}

PR_IMPLEMENT(PRStatus)
NSTP_Initialize (const NSTPGlobalConfig *config, PRIntn *vmin, PRIntn *vmax)
{
    PRStatus rv = PR_SUCCESS;
	
    /* Return the minimum and maximum version numbers if possible */
    if (vmin)
        *vmin = NSTP_API_VERSION_MIN;
    if (vmax)
        *vmax = NSTP_API_VERSION_MAX;
	
    /* If configuration is present, validate version number */
    if (config && (config -> version < NSTP_API_VERSION_MIN ||
		config -> version > NSTP_API_VERSION_MAX))
	{
        rv = PR_FAILURE;
    }
    else
	{
		hasTimeout = config -> tmoEnable;
        rv = PR_CallOnce (&once, InitializePoolLock);
    }
	
    return rv;
}

PR_IMPLEMENT(NSTPStatus)
NSTP_QueueWorkItem (NSTPPool tpool, NSTPWorkFN workfn, NSTPWorkArg workarg, PRIntervalTime timeout)
{
    PRStatus     rv;
    NSTPStatus rsts;
    NSTPWorkItem work;
    PRIntervalTime epoch;
	
    /* Pretend loop to avoid goto */
    while (1)
	{		
        /* Initialize work item on stack */
        work.next = NULL;
        work.workfn  =  workfn;
        work.workarg = workarg;
		
        work.work_status = NSTP_STATUS_WORK_QUEUED;
        work.work_complete = PR_FALSE;
		
        /* Acquire thread pool lock */
        PR_Lock (tpool -> lock);

        /* Determine whether work queue is full, or if shutdown in progress */
        if ((tpool -> config.maxQueue && (tpool -> stats.queueCount >= tpool -> config.maxQueue))
			|| tpool -> shutdown)
		{			
            /* Work queue is full, so reject request */
            PR_Unlock (tpool->lock);
            rsts = tpool->shutdown ? NSTP_STATUS_BUSY_REJECT : NSTP_STATUS_SHUTDOWN_REJECT;
            break;
        }

		// ruslan: add locks/cvars recycling

		if (tpool -> waiter_mon != NULL)
		{
			work.waiter_mon = tpool -> waiter_mon;
			tpool -> waiter_mon = tpool -> waiter_mon -> next;
		}
		else
		{
			CL_elem *lv = new CL_elem ();

			if (lv == NULL || lv -> lock == NULL || lv -> cvar == NULL)
			{
				PR_Unlock (tpool -> lock);

				if (lv != NULL)
					delete lv;

	            rsts = NSTP_STATUS_NSPR_FAILURE;
				break;
			}
			else
				work.waiter_mon = lv;
		}
		
        /* Queue work item */
        if (tpool -> tail)
            tpool -> tail -> next = &work;
        else
            tpool -> head = &work;

		tpool->tail = &work;
		
        /* Count number of work items queued */
        if (++tpool -> stats.queueCount > tpool -> stats.maxQueue) 
            tpool -> stats.maxQueue = tpool -> stats.queueCount;
		
        /* If all threads are busy, consider creating a new thread */
        if ((tpool->stats.freeCount <= 0) && tpool->config.maxThread &&
            (tpool->stats.threadCount < tpool->config.maxThread))
		{
            PRThread *thread;
			
            /*
			* Bump the total thread count before we release the pool
			* lock, so that we don't create more than the allowed
			* number of threads.
			*/
            ++tpool -> stats.threadCount;
            ++tpool -> stats.freeCount;
            PR_Unlock (tpool->lock);
			
            thread = PR_CreateThread(PR_USER_THREAD,
				NSTP_ThreadMain,
				(void *)tpool,
				PR_PRIORITY_NORMAL,
				PR_GLOBAL_THREAD,
				PR_UNJOINABLE_THREAD,
				tpool->config.stackSize);
			
            /* Reacquire the pool lock */
            PR_Lock(tpool->lock);
			
            if (!thread) {
                --tpool->stats.freeCount;
                --tpool->stats.threadCount;
                PR_Unlock(tpool->lock);
                rsts = PR_FAILURE;
                break;
            }
			
        }
		
        /* Wakeup a waiting thread */
        PR_NotifyCondVar (tpool -> cvar);
		
        /* Release thread pool lock */
        PR_Unlock (tpool -> lock);
		
		/* Record start time of wait */
		if (hasTimeout &&
			timeout != PR_INTERVAL_NO_TIMEOUT && timeout != PR_INTERVAL_NO_WAIT)
			epoch = PR_IntervalNow ();
		
        /* Acquire the work item lock */
        PR_Lock (work.waiter_mon -> lock);
		
        /* Now wait for work to be completed */
        while (work.work_complete == PR_FALSE) 
		{
			
            rv = PR_WaitCondVar (work.waiter_mon -> cvar, hasTimeout ? timeout : PR_INTERVAL_NO_TIMEOUT);
			if (rv == PR_FAILURE) 
			{
				/* Some kind of NSPR error */
				work.work_status = NSTP_STATUS_NSPR_FAILURE;
				work.work_complete = PR_TRUE;
			}
			else 
			if (hasTimeout &&
				!work.work_complete && timeout != PR_INTERVAL_NO_TIMEOUT &&
				timeout != PR_INTERVAL_NO_WAIT
				&&
				((PRIntervalTime)(PR_IntervalNow() - epoch) > timeout)) 
			{
					
				/* Wait timed out */
				/* XXX if (work not started yet) */
				work.work_status = NSTP_STATUS_WORK_TIMEOUT;
				work.work_complete = PR_TRUE;
				rv = PR_FAILURE;
				/* XXX else PR_Interrupt thread? */
			}

			if (rv == PR_FAILURE)
			{
				/* XXX remove work item from queue */
				break;
			}
		} /* while work_complete */
		
		rsts = work.work_status;
				
		/* Release the work item lock */
		PR_Unlock (work.waiter_mon -> lock);		
		
		PR_Lock   (tpool -> lock);		
			/* 
			Cleanup and continue 
			*/
			work.waiter_mon -> next = tpool -> waiter_mon;
			tpool -> waiter_mon = work.waiter_mon;

		PR_Unlock (tpool -> lock);

		break;
	} /* while */
		
	return rsts;
}

PR_IMPLEMENT(void)
NSTP_Terminate(PRBool doitnow)
{
    NSTPPool pip;
	
    PR_Lock(NSTPLock);
    while ((pip = NSTPInstanceList) != NULL) {
		NSTPInstanceList = pip->next;
		NSTP_DestroyPool(pip, doitnow);
    }
    PR_Unlock(NSTPLock);
}

void
NSTP_ThreadMain (void *arg)
{
    NSTPPool pip = (NSTPPool)arg;
    NSTPWorkItem *work = NULL;
    NSTPThread self;
	
    /* Initialize structure describing this thread */
    self.prthread = PR_GetCurrentThread ();
    self.work = NULL;
    self.shutdown = PR_FALSE;
	
    PR_Lock(pip->lock);
	
    /* Add this thread to the list of threads in the thread pool instance */
    self.next = pip->threads;
    pip->threads = &self;
	
    /*
	* The free thread count was incremented when this thread was created.
	* The thread is free, but we're going to increment the count at the
	* beginning of the loop below, so decrement it here.
	*/
    --pip -> stats.freeCount;
	
    /*
	* Begin main service loop.  The pool lock is held at the beginning
	* of the loop, either because it was acquired above, or because it
	* was acquired at the end of the previous iteration.
	*/
    while (!self.shutdown) {
		
        /* Count this thread as free */
        ++pip -> stats.freeCount;
		
        /* Wait for something on the work queue, or for shutdown */
        while (!pip->head && !self.shutdown) {
            PR_WaitCondVar (pip->cvar, PR_INTERVAL_NO_TIMEOUT);
        }
		
        /* Dequeue the head work item */
        work = pip->head;
        pip -> head = work -> next;
		pip -> stats.queueCount--;	// decrement the queue count

        if (!pip->head) {
            pip->tail = NULL;
			
			/*
			* If the pool shutdown flag is set, wake all threads waiting
			* on the pool cvar, so that the one that is waiting for the
			* work queue to be empty will wake up and see that.  The
			* other (worker) threads will immediately go back to waiting
			* on the pool cvar when they see that the work queue is
			* empty.
			*/
			if (pip->shutdown) {
				PR_NotifyAllCondVar(pip->cvar);
			}
        }
        self.work = work;
		
        /* This thread is no longer free */
        --pip -> stats.freeCount;
		
        /* Release the pool instance lock */
        PR_Unlock(pip->lock);
		
        /* Call the work function */
        work->workfn(work->workarg);
		
        /* Acquire the lock used by the calling, waiting thread */
        PR_Lock(work -> waiter_mon -> lock);
		
        /* Set work completion status */
        work->work_status = NSTP_STATUS_WORK_DONE;
        work->work_complete = PR_TRUE;
		
        /* Wake up the calling, waiting thread */
        PR_NotifyCondVar (work -> waiter_mon -> cvar);
		
        /* Release the lock */
        PR_Unlock(work -> waiter_mon -> lock);
		
        /* Acquire the pool instance lock for the next iteration */
        PR_Lock(pip->lock);
    }
	
    /* Decrement the thread count before this thread terminates */
    if (--pip -> stats.threadCount <= 0)
	{
		
		/* Notify shutdown thread when this is the last thread */
		PR_NotifyCondVar(pip->cvar);
    }
	
    PR_Unlock(pip->lock);
}

PR_IMPLEMENT(NSTPPoolStats *)
NSTP_GetPoolStats (NSTPPool pool)
{
	return &pool -> stats;
}

/*
* NSTP_DestroyPool - destroy a thread pool instance
*/
PR_IMPLEMENT(void)
NSTP_DestroyPool(NSTPPool pool, PRBool doitnow)
{
    NSTPWorkItem *work;
	
    PR_Lock(pool->lock);
	
    /*
	* Indicate pool is being shut down, so no more requests
	* will be accepted.
	*/
    pool->shutdown = PR_TRUE;
	
    if (doitnow) {
		
		/* Complete all queued work items with NSTP_STATUS_SHUTDOWN_REJECT */
		while ((work = pool->head) != NULL) {
			
			/* Dequeue work item */
			pool->head = work->next;
			if (pool->head == NULL) {
				pool->tail = NULL;
			}
			
			PR_Unlock(pool->lock);
			
			/* Acquire the lock used by the calling, waiting thread */
			PR_Lock ( work -> waiter_mon -> lock);
			
			/* Set work completion status */
			work->work_status = NSTP_STATUS_SHUTDOWN_REJECT;
			work->work_complete = PR_TRUE;
			
			/* Wake up the calling, waiting thread */
			PR_NotifyCondVar (work -> waiter_mon -> cvar);
			
			/* Release the lock */
			PR_Unlock (work -> waiter_mon -> lock);
			
			PR_Lock (pool -> lock);
		}
    }
    else {
		/* doitnow is PR_FALSE */
		
		/* Wait for work queue to be empty */
		while (pool->head != NULL) {
			PR_WaitCondVar(pool->cvar, PR_INTERVAL_NO_TIMEOUT);
		}
    }
	
    if (pool -> stats.threadCount > 0)
	{
		NSTPThread *thread;
		NSTPThread *nxt_thread;
		
		for (thread = pool->threads; thread; thread = nxt_thread) {
			nxt_thread = thread->next;
			thread->shutdown = PR_TRUE;
		}
		
		/* Wakeup all threads to look at their shutdown flags */
		PR_NotifyAllCondVar(pool->cvar);
		
		/* Wait for threadCount to go to zero */
		while (pool -> stats.threadCount > 0) 
		{
			PR_WaitCondVar(pool->cvar, PR_INTERVAL_NO_TIMEOUT);
		}
    }
	
    PR_Unlock(pool->lock);
	
    PR_DestroyCondVar(pool->cvar);
    PR_DestroyLock(pool->lock);
    PR_DELETE(pool);
}

