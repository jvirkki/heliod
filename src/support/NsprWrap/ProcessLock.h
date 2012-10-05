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

#ifndef __ProcessLock_h__
#define __ProcessLock_h__

#include "nspr.h"                          // NSPR declarations
#include "CriticalSection.h"               // CriticalSection, SafeLock
#include "Sleeper.h"                       // Sleeper class

#ifdef XP_PC 
#ifdef BUILD_NSPRWRAP_DLL
#define PROCESSLOCK_DLL_API _declspec(dllexport)
#else
#define PROCESSLOCK_DLL_API _declspec(dllimport)
#endif
#else
#define PROCESSLOCK_DLL_API 
#endif

/**
 * A class that represents a cross-process lock that can be used in a 
 * multi-threaded application on <b>Unix</b>.
 *
 * This class ensures that only one thread in a process tries to acquire
 * the cross process lock. This implementation uses file locks as the
 * synchronization primitive for the cross-process lock. <code>fcntl</code>
 * is *not* thread aware and can fail returning an <code>errno</code> of 
 * <code>EDEADLK</code> when processes use more than one file lock. In
 * a multi-threaded application this may not be a true deadlock and so
 * the <code>lock</code> semantics of this class implements a sleep and retry
 * loop when an <code>EDEADLK</code> occurs. The sleep timeout and the 
 * number of retries is configured during construction.
 *
 * This implementation does NOT use PR_LockFile. See Bug# 539968.
 */
class PROCESSLOCK_DLL_API ProcessLock
{
    public:

        /**
         * Default sleep timeout (in ms) between deadlock retries.
         */
        static const PRInt32 DEFAULT_SLEEP_TIMEOUT;

        /**
         * Default number of times that the <code>lock</code> method will
         * sleep for <code>DEFAULT_SLEEP_TIMEOUT</code> ms and try to acquire the
         * cross-process lock before returning <code>PR_FAILURE</code>.
         *
         * Default total retry time = (6000 * 10) ms = 60 seconds.
         */
        static const PRInt32 DEFAULT_RETRY_COUNT;

        /**
         * Creates the cross-process lock and the mutex that serializes
         * access to the cross-process lock.
         *
         * @param name    A unique name for the cross-process lock
         * @param timeout The amount of time (in ms) to sleep between deadlock 
         *                retries
         * @param retries The number of times to try and acquire the lock when
         *                a deadlock error is reported.
         */
        ProcessLock(const char* name,
                    const PRInt32 timeout = ProcessLock::DEFAULT_SLEEP_TIMEOUT,
                    const PRInt32 retries = ProcessLock::DEFAULT_RETRY_COUNT);

        /**
         * Closes and destroys the cross process lock.
         */
        ~ProcessLock(void);

        /**
         * Acquires the cross-process lock.
         *
         * @param sleeper The object to use for sleeping between deadlock retries.
         * @returns       <code>PR_SUCCESS</code> if successful and 
         *                <code>PR_FAILURE</code> if an error (other than EDEADLK)
         *                occurred while acquiring the lock or if the deadlock
         *                retry loop could not acquire the lock.
         */
        PRStatus acquire(Sleeper& sleeper);

        /**
         * Releases the cross-process lock and its associated mutex thus allowing
         * other threads in this process to contend for the lock.
         *
         * Returns <code>PR_SUCCESS</code> if successful and 
         * <code>PR_FAILURE</code> if an error occurred while releasing the 
         * cross process lock.
         */
        PRStatus release(void);

        /**
         * Indicates whether or not this object is properly initialized.
         *
         * If this method returns <code>PR_FALSE</code>, then use the
         * <code>getErrorCode</code> method to determine the cause of failure.
         * This method MUST be used immediately after construction of this
         * object to determine if the process lock was properly initialized.
         * Failure to create/open the lock file will result in an initialization
         * error.
         */
        PRBool isValid(void) const;
        
        /**
         * Returns the error code (<code>errno</code>) for the last/previous
         * operation on the lock that encountered an error.
         *
         * @returns A valid <code>errno</code> value if an error occurred on
         *          the lock.
         */
        PRInt32 getErrorCode(void) const;

        /**
         * Returns the name given to this cross process lock.
         */
        const char* const getName(void) const;

        /**
         * Returns the maximum number of attempts that will be made to try
         * and acquire the cross process lock.
         *
         * Error recovery is attempted only on EDEADLK errors.
         */
        PRInt32 getMaxRetries(void) const;

        /**
         * Returns the timeout (in ms) that this thread will sleep (when a
         * deadlock error is encountered) before trying to re-acquire the cross 
         * process lock.
         */
        PRInt32 getSleepTimeout(void) const;


    private:

        /**
         * The file descriptor of the cross-process lock file.
         */
        PRInt32 lockFD_;

        /**
         * Ensures that only 1 thread in a process attempts to acquire the
         * cross process lock.
         *
         * Cross-process file locks are not guaranteed to work correctly
         * when used simultaneously by multiple threads within a process.
         */
        CriticalSection* lock_;

        /**
         * Specifies the name of the lock.
         *
         * File locks are typically created/located in TempDir.
         */
        char* lockName_;

        /**
         * The sleep interval [converted from ms] (after a deadlock error is
         * detected while acquiring the cross-process lock) before retrying.
         */
        PRIntervalTime timeOut_;

        /**
         * The maximum number of times to try and acquire the cross process
         * lock (after an <code>EDEADLK</code> error is encountered).
         */
        PRInt32 maxRetries_;

        /**
         * Upon failure, the errno value is saved here and can be retrieved
         * via the <code>getErrorCode</code> method.
         */
        PRInt32 errorCode_;
};

#endif  // __ProcessLock_h__
