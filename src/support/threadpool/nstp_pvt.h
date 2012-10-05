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

#ifndef __nstp_pvt_h_
#define __nstp_pvt_h_

/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#include <string.h>              /* for memset() */
#include "nstp.h"

PR_BEGIN_EXTERN_C

/* TYPES */

typedef struct NSTPThread NSTPThread;
typedef struct NSTPWorkItem NSTPWorkItem;

class	CL_elem	{
public:
	CL_elem ()
	{
		lock = PR_NewLock ();

		if (lock != NULL)
			cvar = PR_NewCondVar (lock);
		else
			cvar = NULL;

		next = NULL;
	}

	~CL_elem()
	{
		if (cvar)
			PR_DestroyCondVar (cvar);

		if (lock)
			PR_DestroyLock    (lock);
	}

	PRLock		*lock;
	PRCondVar	*cvar;
	class CL_elem	*next;
};

/*
 * NSTPPool - thread pool instance structure
 *
 * This defines the structure of a thread pool instance.  Each instance
 * has its own configuration parameters, work queue, threads, and
 * statistics.
 */
struct NSTPPool_s {
    NSTPPool next;          /* next thread pool instance */
    PRLock * lock;          /* lock for access to pool instance */
    PRCondVar *cvar;        /* cvar notified when work queued	*/
    NSTPPoolConfig config;  /* configuration parameters */
    NSTPWorkItem *head;     /* first item on work queue */
    NSTPWorkItem *tail;     /* last item on work queue	*/
    NSTPThread *threads;    /* list of all threads		*/
    PRBool shutdown;        /* PR_TRUE: all threads exit*/
	NSTPPoolStats	stats;	/* pool statistics			*/
    CL_elem	  *waiter_mon;	/* list of available worker locks/cvars	*/
};

/*
 * NSTPThread - thread pool thread structure
 *
 * This defines a structure used for keeping track of thread pool threads.
 * It is normally allocated on the stack of the thread pool thread itself.
 */
struct NSTPThread {
    NSTPThread *next;    /* next thread on thread pool list */
    PRThread *prthread;  /* NSPR thread handle for this thread */
    NSTPWorkItem *work;  /* current work item, if any */
    PRBool shutdown;     /* shutdown flag for this thread */
};

/*
 * NSTPWorkItem - queued work item
 *
 * This defines the structure of a queued work item.  This is typically
 * allocated on the stack of a thread that calls NSTP_QueueWorkItem().
 */
struct NSTPWorkItem {
    NSTPWorkItem *next;     /* pointer to next item on work queue */
    NSTPWorkFN workfn;      /* pointer to work function */
    NSTPWorkArg workarg;    /* pointer to work function arguments */
    CL_elem	*waiter_mon;	/* cvar between calling and pool threads */
    NSTPStatus work_status; /* current/return status of work item */
    PRBool work_complete;   /* PR_TRUE when work is complete or rejected */
};

/* FUNCTIONS */
void NSTP_ThreadMain(void *arg);

PR_END_EXTERN_C

#endif /* __nstp_pvt_h_ */
