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
 * Implementation of the Thread class. 
 *
 * @author     $Author: elving $
 * @version    $Revision: 1.1.2.5.62.2 $
 */

#include <string.h>
#include <signal.h>
//#include "base/ereport.h"                  // ereport()
#include "Thread.h"   


extern "C" void ThreadMain(void* thisObject);

PRInt32 Thread::nThreads_ = 0;
PRBool Thread::bAlwaysCreateGlobalThreads_ = PR_FALSE;
PRUint32 Thread::defaultStackSize_ = 0;

/*
 * Default constructor
 * Initializes the name of the thread
 */
Thread::Thread(const char* threadName) : bTerminated_(PR_FALSE),
                                         bRunning_(PR_FALSE)
{
    if (threadName != NULL)
        this->name_ = strdup(threadName);
    else
        this->name_ = strdup("Anonymous");
    if (this->name_ == NULL)
    {
        // TB_THROW(OutOfMemoryException("Thread name allocation error"));
        PR_ASSERT(1);
    }
}




#ifdef SOLARIS
#pragma disable_warn
#endif

/*
 * Copy constructor
 * This method is private so that users cannot copy-construct Thread
 * objects
 */
Thread::Thread(const Thread& t)
{
    PR_ASSERT(1);
}


/*
 * operator=
 * This method is private so that users cannot clone Thread objects
 */
Thread&
Thread::operator=(const Thread& t)
{
    PR_ASSERT(1);
    return *this;
}

#ifdef SOLARIS
#pragma enable_warn
#endif

/*
 * Destructor
 */
Thread::~Thread(void)
{
    if (this->name_ != NULL)
    {
        free(this->name_);
        this->name_ = NULL;
    }
}



/*
 * Creates a thread that will invoke the <code>run()</code> 
 * method of the class. The parameters are passed directly
 * to the <code>PR_CreateThread()</code> function
 */
PRStatus
Thread::start(const PRThreadScope scope,  const PRThreadState state,
              const PRUint32 stackSize, const PRThreadPriority priority,
              const PRThreadType type)
{
    this->type_ = type;
    if (Thread::forcesGlobalThreadCreation() == PR_TRUE)
        this->scope_ = PR_GLOBAL_BOUND_THREAD;
    else
        this->scope_ = scope;
    PRUint32 threadStackSize = stackSize;
    if (threadStackSize == 0)
        threadStackSize = Thread::defaultStackSize_;
    this->bRunning_ = PR_TRUE;
    this->state_ = state;
    this->self_= PR_CreateThread(type,
                                 (&ThreadMain), (void*)this,
                                 priority, this->scope_, state,
                                 threadStackSize);
    PRStatus status = PR_SUCCESS;
    if (this->self_ == NULL)     /* error creating thread */
    {
//        ereport(LOG_FAILURE, "Error creating thread (%d/%d) - %s",
//                PR_GetError(), PR_GetOSError(), system_errmsg());
        this->bRunning_ = PR_FALSE;
        status = PR_FAILURE;
    }
    return status;
}


PRStatus
Thread::start(const PRThreadType type, const PRThreadState state,
              const PRUint32 stackSize, const PRThreadPriority priority,
              const PRThreadScope scope)
{
    return this->start(scope, state, stackSize, priority, type);
}

/*
 * Returns this thread's name
 */
const char*
Thread::getName(void) const
{
    return this->name_;
}


/*
 * Wait for this thread to terminate.
 */
PRStatus
Thread::join(void)
{
    return PR_JoinThread(this->self_);
}


/*
 * Wait for the thread specified by the <code>target</code>
 * parameter to terminate.
 */
PRStatus
Thread::join(PRThread* target)
{
    return PR_JoinThread(target);
}


/*
 * Indicates whether the thread is joinable or not
 */
PRBool
Thread::isJoinable(void)
{
    return ((this->state_ == PR_JOINABLE_THREAD) ? PR_TRUE : PR_FALSE);
}


/*
 * Yields the execution of this thread and releases the processor
 * for use by other threads (for a specified amount of time).
 */
PRStatus
Thread::yield(PRIntervalTime ticks)
{
    return PR_Sleep(ticks);
}


/*
 * Returns the current priority of this thread
 */
PRThreadPriority
Thread::getPriority(void) const
{
    return PR_GetThreadPriority(this->self_);
}


/*
 * Adjusts the thread priority to the specified value
 */
void
Thread::setPriority(const PRThreadPriority priority)
{
    PR_SetThreadPriority(this->self_, priority);
}

 
void
Thread::forceGlobalThreadCreation(const PRBool bForce)
{
    Thread::bAlwaysCreateGlobalThreads_ = bForce;
}

PRBool
Thread::forcesGlobalThreadCreation(void)
{
    return Thread::bAlwaysCreateGlobalThreads_;
}

void
Thread::setDefaultStackSize(const PRUint32 stackSize)
{
    Thread::defaultStackSize_ = stackSize;
}


/*
 * Invokes the <code>run()</code> method of the object that is
 * specified by the <code>thisObject</code> parameter.
 * When the <code>start()</code> method is invoked, it calls
 * <code>PR_CreateThread()</code> passing it a function pointer
 * to this static method and the argument that is passed in
 * as a parameter to this method. The <code>arg</code> parameter
 * passed to <code>PR_CreateThread()</code> is simply the 
 * <code>this</code> pointer cast as a void pointer. When this
 * static method is invoked it casts the <code>thisObject</code>
 * to a baseclass pointer -  <code>Thread*</code> and invokes 
 * the <code>run()</code> of that object. Users should not invoke
 * this method directly.
 */
extern "C"
void
ThreadMain(void* thisObject)
{
    Thread* activeObject = (Thread*)thisObject;
    activeObject->run_();
}

void
Thread::run_(void)
{
    PR_AtomicIncrement(&(Thread::nThreads_));
    this->run();                 // invoke the user-defined method
    this->bRunning_ = PR_FALSE;
    PR_AtomicDecrement(&(Thread::nThreads_));
}

PRStatus
Thread::interrupt(void)
{
    return PR_Interrupt(this->self_);
}

void
Thread::clearInterrupt(void)
{
    PR_ClearInterrupt();
}
