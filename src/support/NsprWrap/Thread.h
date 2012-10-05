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

#ifndef _Thread_h
#define _Thread_h

#include "nspr.h"              // NSPR threads declarations
#include "pratom.h"            // PR_AtomicIncrement/PR_AtomicDecrement

#ifdef XP_PC 
#ifdef BUILD_NSPRWRAP_DLL
#define THREAD_DLL_API _declspec(dllexport)
#else
#define THREAD_DLL_API _declspec(dllimport)
#endif
#else
#define THREAD_DLL_API 
#endif

/**
 * Models the behaviour of an execution path through a process. This class
 * provides a simple object oriented interface to
 * <a href="http://www.mozilla.org/docs/refList/refNSPR/contents.html">NSPR</a>
 * threads.
 *
 * A thread is an execution path through a process. The <code>Thread</code>
 * class models the behaviour of threads. This base class
 * allows users to create a new thread of execution by declaring a
 * subclass of <code>Thread</code>. The subclass then overrides and
 * implements the <code>run</code> method. When the <code>start</code> is 
 * invoked, a new thread of execution is created that in turn calls the 
 * <code>run</code> method of the newly created object.
 *
 * @author     $Author: elving $
 * @version    $Revision: 1.1.2.3.62.1 $
 */

class THREAD_DLL_API Thread
{
    public:

        /**
         * Standard constructor specifying the name of the thread being
         * created.
         *
         * @param threadName The name to use for identifying messages
         *                   printed by this thread
         */
        Thread(const char* threadName);

        /**
         * Virtual destructor
         */
        virtual ~Thread(void);

        /**
         * Creates a thread that will invoke the <code>run</code> 
         * method of this object
         *
         * See the <code>PR_CreateThread()</code> documentation for a list
         * of values for the parameters of this method.
         *
         * @param scope     Indicates whether this thread is to be created as
         *                  a local or a global thread.
         * @param state     Specifies whether the thread is joinable
         * @param stackSize The thread stack size in bytes. If 0, then a
         *                  default stack size is used.
         * @param priority  The initial priority of the newly created thread.
         * @param type      The type (user or system) of thread to create.
         * @returns         <tt>PR_SUCCESS</tt> if the thread was successfully
         *                  created and <tt>PR_FAILURE</tt> if there were 
         *                  errors.
         */
        PRStatus start(const PRThreadScope scope,
                       const PRThreadState state,
                       const PRUint32 stackSize = 0,
                       const PRThreadPriority priority = PR_PRIORITY_NORMAL,
                       const PRThreadType type = PR_USER_THREAD);

        /**
         * Creates a thread that will invoke the <code>run</code> 
         * method of this object
         *
         * See the <code>PR_CreateThread()</code> documentation for a list
         * of values for the parameters of this method.
         *
         * @param type      The type (user or system) of thread to create.
         * @param state     Specifies whether the thread is joinable
         * @param stackSize The thread stack size in bytes. If 0, then a
         *                  default stack size is used.
         * @param priority  The initial priority of the newly created thread.
         * @param scope     Indicates whether this thread is to be created as
         *                  a local or a global thread.
         * @returns         <tt>PR_SUCCESS</tt> if the thread was successfully
         *                  created and <tt>PR_FAILURE</tt> if there were 
         *                  errors.
         */
        PRStatus start(const PRThreadType type,
                       const PRThreadState state,
                       const PRUint32 stackSize = 0,
                       const PRThreadPriority priority = PR_PRIORITY_NORMAL,
                       const PRThreadScope scope = PR_LOCAL_THREAD);

        /**
         * Returns this thread's unique identifier that is assigned to
         * it by the operating system.
         */
        PRThread* self(void) const;

        /**
         * Returns this thread's name
         */
        const char* getName(void) const;

        /**
         * Waits for this thread to terminate.
         *
         * @returns <code>PR_SUCCESS</code> upon a successful termination
         *                of this thread. If the current thread is unjoinable,
         *                a status of <code>PR_FAILURE</code> is returned.
         */
        PRStatus join(void);

        /**
         * Waits for the specified thread to terminate
         *
         * @param target  The identifer of the thread to join.
         * @returns       <code>PR_SUCCESS</code> if the target thread 
         *                terminated. If the target thread is unjoinable,
         *                then a status of <code>PR_FAILURE</code> is returned.
         */
        PRStatus join(PRThread* target);

        /**
         * Sets a pending interrupt request for this thread.
         */
        PRStatus interrupt(void);

        /**
         * Clears the interrupt request for this thread.
         */
        void clearInterrupt(void);

        /**
         * Returns <code>PR_TRUE</code> if this thread is joinable, 
         * <code>PR_FALSE</code> if is not joinable.
         *
         * If this thread was created as either a detached thread or
         * as a daemon thread then it cannot be joined.
         */
        PRBool isJoinable(void);

        /**
         * Sets the terminate flag to <code>PR_TRUE</code> indicating that
         * the thread should exit.
         *
         * The logic in the <code>run</code> method should check this flag to 
         * determine whether the thread processing is to be terminated.
         */
        void setTerminatingFlag(void);

        /**
         * Indicates whether the terminate flag has been set on this 
         * thread.
         *
         * @see setTerminatingFlag
         */
        PRBool wasTerminated(void) const;

        /**
         * Indicates whether the thread is currently running (i.e.
         * executing code within the while loop of its <code>run</code>
         * method).
         *
         * This method can be used to check whether the thread has
         * exited from the <code>run</code> method.
         *
         * <pre>
         * <code>
         *  t.setTerminatingFlag();
         *  .
         *  .
         *  .
         *  while (t.isRunning())
         *      PR_Sleep(ONE_SECOND);
         * </code>
         * </pre>
         *
         * @see setTerminatingFlag
         */
        PRBool isRunning(void) const;

        /**
         * Yields the execution of this thread and releases the processor
         * for use by other threads.
         *
         * @param ticks The number of ticks that the thread is to sleep for.
         * @returns     <code>PR_FAILURE</code> if an error occurs.
         */
        PRStatus yield(PRIntervalTime ticks);

        /**
         * Returns the current priority of this thread
         */
        PRThreadPriority getPriority(void) const;

        /**
         * Adjusts the current priority of this thread to the specified
         * value
         *
         * @param priority The new priority setting for this thread
         */
        void setPriority(const PRThreadPriority priority);

        /**
         * Returns the type of this thread
         */
        PRThreadType getType(void) const;

        /**
         * Returns the scope of this thread
         */
        PRThreadScope getScope(void) const;

        /**
         * Returns the state of this thread
         */
        PRThreadState getState(void) const;

        /**
         * Returns a count of the total number of Threads that are running
         */
        static PRInt32 getTotalThreads(void);

        /**
         * Controls whether threads other than global ones 
         * (<code>PR_GLOBAL_BOUND_THREAD</code>) can be created.
         *
         * By default, users can create any kind of thread. This
         * property is controlled by the KernelThread tag in magnus.conf.
         *
         * @param bForce Set to PR_FALSE to allow users to specify
         *               the type of thread to create via the 
         *               <code>scope</code> parameter to the 
         *               <code>start</code> method.
         */
        static void forceGlobalThreadCreation(const PRBool bForce);


        /**
         * Indicates whether this class overrides the <code>scope</code>
         * parameter to the <code>start</code> method.
         *
         * @returns The default value for this property is 
         *          <code>PR_FALSE</code>
         * @see     #forceGlobalThreadCreation
         */
        static PRBool forcesGlobalThreadCreation(void);

        /**
         * Sets the default stack size. The default stack size is used when
         * the <code>start</code> method is called with <code>stackSize</code>
         * = 0.
         */
        static void setDefaultStackSize(const PRUint32 stackSize);

        /**
         * This virtual method should be implemented in the user-defined 
         * subclass of <code>Thread</code>. The code in this method will 
         * be executed in a separate thread of execution.
         *
         * This method should not be invoked directly by users.
         */
        virtual void run(void) = 0;

        /**
         * Sets the running state of the thread to the appropriate value
         * and invokes the user-implemented <code>run</code> method.
         *
         * The <code>start</code> method of the Thread class calls
         * <code>PR_CreateThread()</code> passing it a function pointer
         * to the <code>ThreadMain</code> function method. The <code>arg</code> 
         * parameter passed to <code>PR_CreateThread()</code> is simply the 
         * <code>this</code> pointer cast as a void pointer. 
         * When the ThreadMain function is invoked (by the OS thread 
         * mechanism), it casts the <code>thisObject</code> to a 
         * baseclass pointer - <code>Thread*</code> and invokes the 
         * <code>run_</code> which in turn invokes the (virtual)
         * <code>run</code> method of the object.
         *
         * This method should not be invoked directly by users.
         */
        void run_(void);

        /**
         * Returns the name of the class as a character string.
         *
         * @returns "Thread"
         */        
        inline const char* getClassName(void) const;

    private:

        /**
         * The name of the thread
         *
         * This is used for identification/debug purposes
         */
        char* name_;

        /**
         * The unique thread identifier of this thread.
         *
         * This is assigned (by the OS) when the thread is created via the
         * <code>thr_create()</code> call
         */
        PRThread* self_;

        /**
         * Contains the value of the <code>type</code> parameter that was 
         * passed to <code>PR_CreateThread()</code>.
         */
        PRThreadType type_;

        /**
         * Contains the value of the <code>scope</code> parameter that was 
         * passed to <code>PR_CreateThread()</code>.
         */
        PRThreadScope scope_;

        /**
         * Contains the value of the <code>state</code> parameter that was 
         * passed to <code>PR_CreateThread()</code>.
         *
         * This information is used to determine whether this thread 
         * can be joined or not.
         */
        PRThreadState state_;

        /**
         * Indicates whether the thread was marked for termination (by
         * invoking the <code>setTerminatingFlag</code> method)
         */
        PRBool bTerminated_;

        /**
         * Indicates whether the thread is currently running (i.e.
         * executing code within the while loop of its <code>run</code>
         * method).
         *
         * This flag is set to <code>PR_FALSE</code>, when the 
         * <code>run</code> method finishes executing.
         */
        PRBool bRunning_;

        /**
         * A count of ALL instances of subclasses of this class that are 
         * <i>running</i> i.e. executing code in their <code>run</code> method.
         */
        static PRInt32 nThreads_;

        /**
         * If set to <code>PR_TRUE</code> then the <code>start</code> 
         * method will always set the <code>scope</code>
         * parameter passed to <code>PR_CreateThread</code> to 
         * <code>PR_GLOBAL_BOUND_THREAD</code>
         *
         * The default value of this flag is <code>PR_FALSE</code>.
         */
        static PRBool bAlwaysCreateGlobalThreads_;

        /**
         * Default stack size is used when the <code>start</code> method is
         * called with <code>stackSize</code> = 0.
         */
        static PRUint32 defaultStackSize_;

        /**
         * Copy constructor
         *
         * Don't allow users to copy-construct Thread objects
         */
        Thread(const Thread& m);

        /**
         * operator=
         *
         * Don't allow users to clone Thread objects
         */
        Thread& operator=(const Thread& m);
};


inline
PRThread*
Thread::self(void) const
{
    return this->self_;
}

inline
void
Thread::setTerminatingFlag(void)
{
    this->bTerminated_ = PR_TRUE;
}

inline
PRBool
Thread::wasTerminated(void) const
{
    return this->bTerminated_;
}

inline
PRBool
Thread::isRunning(void) const
{
    return this->bRunning_;
}

inline
PRInt32
Thread::getTotalThreads(void)
{
    return Thread::nThreads_;
}

inline
const char*
Thread::getClassName(void) const
{
    return "Thread";
}

#endif /* _Thread_h */
