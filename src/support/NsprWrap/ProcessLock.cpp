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
 * ProcessLock implementation
 *
 * @author  Arvind Srinivasan
 * @version $Revision: 1.1.2.2 $
 */

#ifdef XP_UNIX
#include <fcntl.h>                                     // fcntl, struct flock, open
#include <limits.h>                                    // PATH_MAX
#include <unistd.h>                                    // close()
#include <errno.h>                                     // errno
#endif
#include <string.h>                                    // strlen, strcpy

#include "ProcessLock.h"                               // ProcessLock class

const PRInt32 ProcessLock::DEFAULT_SLEEP_TIMEOUT = 10; // 10ms sleep
const PRInt32 ProcessLock::DEFAULT_RETRY_COUNT = 6000; // 6000 * 10 = 1 minute

ProcessLock::ProcessLock(const char* name, const PRInt32 timeout,
                         const PRInt32 retries) : lockName_(NULL), errorCode_(0)
{
#ifdef XP_UNIX
    PR_ASSERT(name != NULL);
    PR_ASSERT(timeout > 0);
    PR_ASSERT(retries > 0);

    int length = strlen(name);
    PR_ASSERT(length > 0);             // Empty names are invalid


    lockName_ = new char[length + 1];
    PR_ASSERT(lockName_ != NULL);

    strcpy(lockName_, name);

    timeOut_ = PR_MillisecondsToInterval(timeout);
    maxRetries_ = retries;

    //
    // Associate a mutex with the cross process lock to ensure that only 1 thread 
    // in a process tries to acquire the cross-process lock
    //
    lock_ = new CriticalSection;
    PR_ASSERT(lock_ != NULL);

    // Create or open the lock file
    lockFD_ = ::open(lockName_, (O_CREAT|O_RDWR), 0600);
    if (lockFD_ == -1)
        errorCode_ = errno;                        // save the error code

#endif
}

ProcessLock::~ProcessLock()
{
#ifdef XP_UNIX

    if (lock_ != NULL && lockFD_ != -1)
    {
        lock_->acquire();
        ::close(lockFD_);
        lock_->release();
        delete lock_;
        lock_ = NULL;
    }

    if (lockName_ != NULL)
    {
        // Delete the lock file
        PR_Delete(lockName_);
        delete [] lockName_;
        lockName_ = NULL;
    }

#endif
}



/*
 * Upon successful return from this method, both the cross process lock and
 * its intra-process lock are acquired.
 */
PRStatus
ProcessLock::acquire(Sleeper& sleeper)
{
#ifdef XP_UNIX

    PRStatus status = PR_FAILURE;

    if (lock_ != NULL && lockFD_ != -1)
    {
        struct flock lockArgs;

        for (int i = 0; i < maxRetries_; i++)
        {
            // Allow only 1 thread in this process to try and acquire
            // the cross-process file lock. fcntl is guaranteed to
            // work correctly ACROSS processes and not between threads in
            // a process.
            lock_->acquire();

            // Attempt to acquire the cross-process lock
            memset(&lockArgs, 0, sizeof(lockArgs));
            lockArgs.l_type = F_WRLCK;

            PRInt32 rv = ::fcntl(lockFD_, F_SETLKW, &lockArgs);

            if (rv != -1)
                status = PR_SUCCESS;
            else
                errorCode_ = errno;                           // save the errno

            if (status != PR_SUCCESS)
            {
                // EDEADLK is treated as a recoverable error because of the 
                // limitations of fcntl's deadlock detection when used in 
                // multithread processes
                if (errorCode_ == EDEADLK)
                {
                    lock_->release();         // Allow other threads to try and 
                    sleeper.sleep(timeOut_);  // acquire the cross process lock 
                                              // while this thread goes to sleep
                }
                else
                    break;                    // other errors are considered fatal
            }
            else
            {
                break;                        // Successfully acquired the lock
            }
        }
    }

    // If successful, then both locks - the mutex and the file lock are acquired
    // by the thread and will only be released when release() is invoked
    return status;
#else
    return PR_FAILURE;
#endif
}


/*
 * Unlock the cross-process lock and also release the protecting intra-process 
 * lock that was also acquired during lock()
 */
PRStatus
ProcessLock::release(void)
{
#ifdef XP_UNIX
    PRStatus status = PR_FAILURE;

    if (lock_ != NULL && lockFD_ != -1)
    {
        struct flock lockArgs;
        memset(&lockArgs, 0, sizeof(lockArgs));
        lockArgs.l_type = F_UNLCK;

        PRInt32 rv = ::fcntl(lockFD_, F_SETLK, &lockArgs);
        if (rv != -1)
            status = PR_SUCCESS;
        else
            errorCode_ = errno;
    }

    if (status == PR_SUCCESS)
    {
        lock_->release();     // Let other threads contend for access to the lock
    }
    return status;
#else
    return PR_FAILURE;
#endif
}

PRBool
ProcessLock::isValid(void) const
{
    if (lock_ != NULL && lockFD_ != -1)
        return PR_TRUE;
    else
        return PR_FALSE;
}

const char* const
ProcessLock::getName(void) const
{
    return lockName_;
}

PRInt32
ProcessLock::getErrorCode(void) const
{
    return errorCode_;
}

PRInt32
ProcessLock::getMaxRetries(void) const
{
    return maxRetries_;
}

PRInt32
ProcessLock::getSleepTimeout(void) const
{
    return PR_IntervalToMilliseconds(timeOut_);
}
