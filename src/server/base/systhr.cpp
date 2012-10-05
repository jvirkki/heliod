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
 * systhr.c: Abstracted threading mechanisms
 * 
 * Rob McCool
 */


#include "systhr.h"
#include "ereport.h"

#include "prinit.h"
#include "prthread.h"
#include "private/pprthred.h"

#include "systems.h"

#ifdef XP_UNIX
#include <poll.h>
#endif

#ifdef THREAD_WIN32
#include <process.h>

typedef struct {
    HANDLE hand;
    DWORD id;
} sys_thread_s;

#endif

#define DEFAULT_STACKSIZE (64*1024)

static unsigned long _systhr_stacksize = DEFAULT_STACKSIZE;

NSAPI_PUBLIC 
void systhread_set_default_stacksize(unsigned long size)
{
	_systhr_stacksize = size;
}

NSAPI_PUBLIC 
SYS_THREAD systhread_start(int prio, int stksz, thrstartfunc fn, void *arg)
{
    PRThread *ret = PR_CreateThread(PR_USER_THREAD, (void (*)(void *))fn,
				    (void *)arg, (PRThreadPriority)prio, 
				    PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
                                    stksz ? stksz : _systhr_stacksize);
    return (void *) ret;
}


NSAPI_PUBLIC SYS_THREAD systhread_current(void)
{
    return PR_GetCurrentThread();
}

NSAPI_PUBLIC void systhread_yield(void)
{
    PR_Sleep(PR_INTERVAL_NO_WAIT);
}


NSAPI_PUBLIC void systhread_timerset(int usec)
{
   /* This is an interesting problem.  If you ever do turn on interrupts
    * on the server, you're in for lots of fun with NSPR Threads
   PR_StartEvents(usec); */
}


NSAPI_PUBLIC 
SYS_THREAD systhread_attach(void)
{
    PRThread *ret;
    ret = PR_AttachThread(PR_USER_THREAD, PR_PRIORITY_NORMAL, NULL);

    return (void *) ret;
}

NSAPI_PUBLIC
void systhread_detach(SYS_THREAD thr)
{
    /* XXXMB - this is not correct! */
    PR_DetachThread();
}

NSAPI_PUBLIC void systhread_terminate(SYS_THREAD thr)
{
    PR_Interrupt((PRThread *)thr);
}

NSAPI_PUBLIC void systhread_sleep(int milliseconds)
{
#ifdef XP_WIN32
    PR_Sleep(PR_MillisecondsToInterval(milliseconds));
#else
    /* poll() is more efficient than PR_Sleep() */
    if (milliseconds > 0)
        poll(NULL, NULL, milliseconds);
#endif
}

NSAPI_PUBLIC void systhread_init(char *name)
{
    if (!PR_Initialized()) {
        PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 256);
    }
    // XXX: ruslan - this bug can potentially exist on all plafroms due to
    //	possible priority inversion on NSPR spin locks. This code will
    //	need to be remove as we get new NSPR drop
    // <WORKAROUND>
    /* VB: This is to fix bug# 364813 coupled with NSPR not wanting to patch
           their problems. The fix is to prevent NSPR itself from
           using atomic stacks.
    */
    // ruslan: this problem exists on DEC also. We will roll it back when
    //	we have the right fix from NSPR group. It has smth. to do with
    //	atomic operations on DEC, it's an assembly code which is different
    //	for every platform. NSPR_FD_CACHE_SIZE_HIGH env var will cause the
    //	same effect as this fix. Debug version of NSPR always works as it doesn't
    //  have FD stack.

    int maxPRFdCache = 8192;
    PR_SetFDCacheSize(0, maxPRFdCache);
    // </WORKAROUND>
}


NSAPI_PUBLIC int systhread_newkey()
{
    uintn newkey;

    PR_NewThreadPrivateIndex(&newkey, NULL);
    return (newkey);
}

NSAPI_PUBLIC void *systhread_getdata(int key)
{
    return PR_GetThreadPrivate(key);
}

NSAPI_PUBLIC void systhread_setdata(int key, void *data)
{
    PR_SetThreadPrivate(key, data);
}

NSAPI_PUBLIC void systhread_dummy(void)
{
}
