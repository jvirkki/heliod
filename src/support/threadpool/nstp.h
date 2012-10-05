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

#ifndef __nstp_h_
#define __nstp_h_

/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#include "nspr.h"

PR_BEGIN_EXTERN_C

/* CONSTANTS */

/* NSTPStatus values */
#define NSTP_STATUS_NSPR_FAILURE    -4
#define NSTP_STATUS_SHUTDOWN_REJECT -3
#define NSTP_STATUS_WORK_TIMEOUT    -2
#define NSTP_STATUS_BUSY_REJECT     -1
#define NSTP_STATUS_WORK_DONE        0
#define NSTP_STATUS_WORK_QUEUED      1
#define NSTP_STATUS_WORK_ACTIVE      2

/* Thread pool API version numbers */
#define NSTP_API_VERSION        1  /* Current API version number */
#define NSTP_API_VERSION_MIN    1  /* Minimum version number supported */
#define NSTP_API_VERSION_MAX    1  /* Maximum version number supported */

/* TYPES */

/*
 * NSTPGlobalConfig - thread pool module configuration parameters
 *
 * This type defines a structure containing configuration parameters for
 * the thread pool module.  Note that there is also a structure containing
 * configuration parameters for a thread pool instance.
 */
typedef struct NSTPGlobalConfig NSTPGlobalConfig;
struct NSTPGlobalConfig {
    PRIntn version;       /* NSTP API version number */
    PRBool tmoEnable;     /* PR_TRUE: enable timeout on queued work */
                          /* PR_FALSE: disable timeouts on queued work */
};

/*
 * NSTPPool - handle for thread pool instance
 *
 * This defines a handle for a thread pool instance.  The structure it
 * references is private to the thread pool module.
 */
typedef struct NSTPPool_s *NSTPPool;

/*
 * NSTPPoolConfig - thread pool instance configuration parameters
 *
 * This defines a structure containing configuration parameters for a
 * thread pool instance, including the initial and maximum number of
 * threads, maximum work queue length, and default timeout.
 */
typedef struct NSTPPoolConfig NSTPPoolConfig;
struct NSTPPoolConfig	{
    PRIntn version;             /* NSTP API version number */
    PRIntn initThread;          /* initial number of threads to create */
    PRIntn maxThread;           /* maximum number of threads to create */
    PRIntn stackSize;           /* stack size of pool threads */
    PRIntn maxQueue;            /* maximum work queue length */
    PRIntervalTime defTimeout;  /* default timeout on thread wait */
};

typedef struct NSTPPoolStats_s NSTPPoolStats;
struct NSTPPoolStats_s	{
    PRInt32 queueCount;     /* current count of queued work items */
    PRInt32 maxQueue;       /* maximum number of items ever queued */
    PRInt32 freeCount;      /* number of free threads */
    PRInt32 threadCount;    /* total number of threads in pool */
};


/* NSTPStatus - thread pool status/result code */
typedef PRIntn NSTPStatus;

/*
 * NSTPWorkArg - pointer to work function arguments and return values
 *
 * This defines a pointer to a structure containing arguments and return
 * values for a work function.  The definition of "struct NSTPWorkArg_s" is
 * typically local to the code invoking the work function, and to the
 * work function itself.
 */
typedef struct NSTPWorkArg_s *NSTPWorkArg;

/*
 * NSTPWorkFN - pointer to a work function
 *
 * This defines a pointer to a work function.  A work function gets its
 * arguments and returns values through the NSTPWorkArg structure for
 * which it receives a pointer.
 */
typedef void (PR_CALLBACK *NSTPWorkFN)(NSTPWorkArg arg);

/* FUNCTION DECLARATIONS */

/*
 * NSTP_CreatePool - create a thread pool instance
 *
 * This function creates a new thread pool instance, and returns a handle
 * for it.  Each thread pool instance has its own pool of dedicated threads
 * and associated work queue.
 *
 *      pcfg - pointer to configuration parameters for this pool instance
 *      tpool - returned handle for new thread pool instance
 */
PR_EXTERN(PRStatus) NSTP_CreatePool(NSTPPoolConfig *pcfg, NSTPPool *tpool);

/*
 * NSTP_DestroyPool - destroy a thread pool instance
 *
 * This function destroys a specified thread pool instance.
 *
 *      doitnow - see NSTP_Terminate
 */
PR_EXTERN(void) NSTP_DestroyPool(NSTPPool pool, PRBool doitnow);

/*
 * NSTP_Initialize - initialize thread pool module
 *
 * This function should be called once to initialize the thread pool module,
 * prior to creating any thread pool instances.
 *
 *      config - pointer to module configuration parameters
 *      vmin - returned minimum API version number supported
 *      vmax - returned maximum API version number supported
 */
PR_EXTERN(PRStatus) NSTP_Initialize(const NSTPGlobalConfig *config,
                                    PRIntn *vmin, PRIntn *vmax);

/*
 * NSTP_QueueWorkItem - queue a work item on a thread pool instance
 *
 * This function queues a work item for execution using a specified
 * thread pool instance.  The calling thread is blocked until the
 * work item is either completed or rejected.
 *
 *      tpool - thread pool instance handle
 *      workfn - pointer to work function to be called
 *      workarg - pointer to work function argument/return value structure
 *      timeout - timeout on waiting for a thread
 */
PR_EXTERN(NSTPStatus) NSTP_QueueWorkItem(NSTPPool tpool,
                                         NSTPWorkFN workfn,
                                         NSTPWorkArg workarg,
                                         PRIntervalTime timeout);

PR_EXTERN(NSTPPoolStats *)	NSTP_GetPoolStats (NSTPPool tpool);

/*
 * NSTP_Terminate - shutdown thread pool module
 *
 * This function is called to shutdown the thread pool module.  It shuts
 * down existing thread pool instances, which terminates their associated
 * threads.
 *
 *      doitnow - PR_TRUE:  return error for queued work items, but finish
 *                          work in progress
 *                PR_FALSE: complete all queued work and work in progress,
 *                          but reject new work items
 */

PR_EXTERN(void) NSTP_Terminate(PRBool doitnow);

PR_END_EXTERN_C

#endif /* __nstp_h_ */
