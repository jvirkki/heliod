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
 * LockManager implementation
 *
 * @author  Arvind Srinivasan
 * @version $Revision: 1.1.2.3 $
 */


#ifdef XP_UNIX
#include <fcntl.h>                                     // fcntl, struct flock
#include <limits.h>                                    // PATH_MAX

#include "base/SemPool.h"                              // SemPool class
#include "base/util.h"                                 // util_snprintf
#include "prlog.h"
#endif
#include "NSJavaUtil.h"                                // NSJavaUtil::log

#include "com_iplanet_server_http_util_LockManager.h"  // JNI prototype decls
#include "LockManager.h"                               // LockManager class

// PID of the primordial process.
PRInt32 LockManager::ppid_ = 0;

#ifdef DEBUG
static int IWS_LOCK_DEBUG = 0;    // set to 1 or 2 from dbx to enable dbg msgs
#endif

LockManager::LockManager(const char* appName, const PRUint32 nLocks)
{
#ifdef XP_UNIX
    NS_JAVA_ASSERT(nLocks > 0);
    NS_JAVA_ASSERT(appName != NULL);

    int length = strlen(appName);
    NS_JAVA_ASSERT(length > 0);             // Empty names are invalid

    nLocks_ = nLocks;

    PRStatus status = PR_FAILURE;

    // Ensure that name only contains valid filename characters
    char name[PATH_MAX];
    strcpy(name, appName);
    int i;
    for (i = 0; i < length; i++)
    {
        if (!isalnum(name[i]))
            name[i] = '_';              // Convert / etc to _
    }

    char prefix[PATH_MAX];
    util_snprintf(prefix, sizeof(prefix), "%s/%d_%s",
                  system_get_temp_dir(), LockManager::ppid_, name);

    //
    // Create the process locks
    //
    locks_ = new ProcessLock*[nLocks_];
    NS_JAVA_ASSERT(locks_ != NULL);

    for (i = 0; i < nLocks_; i++)
        locks_[i] = NULL;

    status = PR_SUCCESS;

    SEMAPHORE sem = SemPool::get("SessionSem");

    // Allow only 1 process to CREATE the lock files. The others will 
    // simply OPEN them.
    int rv = Sem_grab(sem);

    NS_JAVA_ASSERT(rv != SEM_ERROR);

    // Create/open the lock files (retry timeout = 10ms, max timeout = 1 minute)
    char fileName[PATH_MAX];
    for (i = 0; i < nLocks_; i++)
    {
        util_snprintf(fileName, sizeof(fileName), "%s.%d.lock", prefix, i);
        ProcessLock* lock = new ProcessLock(fileName);
        NS_JAVA_ASSERT(lock != NULL);
        if (lock->isValid())
        {
            locks_[i] = lock;
#ifdef DEBUG
            if (IWS_LOCK_DEBUG)
            {
                NSJavaUtil::log(LOG_INFORM, "Created/opened lock %d - %s",
                                i, lock->getName());
            }
#endif
        }
        else
        {
            status = PR_FAILURE;
            NSJavaUtil::log(LOG_CATASTROPHE,
                            "Error %d creating/opening lockfile %s",
                            lock->getErrorCode(), lock->getName());
        }
    }

    rv = Sem_release(sem); 

    NS_JAVA_ASSERT(status != PR_FAILURE);
#endif
}

PRStatus
LockManager::Initialize(void)
{
#ifdef XP_UNIX
    LockManager::ppid_ = getpid();
#endif
    return PR_SUCCESS;
}

LockManager::~LockManager()
{
#ifdef XP_UNIX

    if (locks_ != NULL)
    {
        for (int i = 0; i < nLocks_; i++)
        {
            ProcessLock* lock = locks_[i];
            if (lock != NULL)
            {
#ifdef DEBUG
                if (IWS_LOCK_DEBUG)
                {
                    NSJavaUtil::log(LOG_INFORM, "Destroyed lock %d - %s",
                                    i, lock->getName());
                }
#endif
                delete lock;
            }
        }
        delete [] locks_;
        locks_ = NULL;
    }
#endif
}


/*
 * Upon successful return from this method, both the cross process lock and
 * its corresponding intra-process lock are acquired.
 */
PRStatus
LockManager::lock(const PRUint32 lockID)
{
    PRStatus status = PR_FAILURE;
#ifdef XP_UNIX
    if (lockID >= nLocks_)
    {
        NSJavaUtil::log(LOG_WARN,
                        "Invalid lock index %d (Max. = %d)", lockID, nLocks_);
        return PR_FAILURE;
    }

    PRInt32 errorCode = 0;

#ifdef DEBUG
    if (IWS_LOCK_DEBUG > 1)
        NSJavaUtil::log(LOG_INFORM, "Acquiring lock %d", lockID);
#endif
    ProcessLock* lock = locks_[lockID];
    if (lock != NULL)
    {
        status = lock->acquire(sleeper_);
    }

    if (status == PR_SUCCESS)
    {
#ifdef DEBUG
        if (IWS_LOCK_DEBUG)
            NSJavaUtil::log(LOG_INFORM, "Acquired lock %d", lockID);
#endif
    }
    else
    {
        NSJavaUtil::log(LOG_CATASTROPHE, "Error %d acquiring lock %d",
                        lock->getErrorCode(), lockID);
    }
#endif
    return status;
}


/*
 * Unlock the cross-process lock 
 */
PRStatus
LockManager::unlock(const PRUint32 lockID)
{
    PRStatus status = PR_FAILURE;
#ifdef XP_UNIX
    if (lockID >= nLocks_)
    {
        NSJavaUtil::log(LOG_WARN,
                        "Invalid lock index %d (Max. = %d)", lockID, nLocks_);
        return PR_FAILURE;
    }

    ProcessLock* lock = locks_[lockID];
    if (lock != NULL)
    {
#ifdef DEBUG
        if (IWS_LOCK_DEBUG > 1)
            NSJavaUtil::log(LOG_INFORM, "Releasing lock %d", lockID);
#endif
        status = lock->release();
    }

    if (status == PR_SUCCESS)
    {
#ifdef DEBUG
        if (IWS_LOCK_DEBUG)
            NSJavaUtil::log(LOG_INFORM, "Released lock %d", lockID);
#endif
    }
    else
    {
        NSJavaUtil::log(LOG_CATASTROPHE, "Error %d releasing lock %d",
                        lock->getErrorCode(), lockID);
    }
#endif
    return status;
}


JNIEXPORT jlong JNICALL
Java_com_iplanet_server_http_util_LockManager_createNativeObject(JNIEnv *env,
    jobject obj, jstring name, jint nLocks)
{
//    PRBool attach = NSJavaUtil::cb_enter();

    const char* appName = NULL;
    if (name != NULL)
        appName = env->GetStringUTFChars(name, NULL);

    LockManager* locker = new LockManager(appName, (int) nLocks);
    NS_JAVA_ASSERT(locker != NULL);

    if (name != NULL)
        env->ReleaseStringUTFChars(name, appName);

//    NSJavaUtil::cb_leave(attach);

    return (jlong)locker;
}

JNIEXPORT void JNICALL
Java_com_iplanet_server_http_util_LockManager_destroyNativeObject(JNIEnv *env,
    jobject obj, jlong nativePtr)
{
//    PRBool attach = NSJavaUtil::cb_enter();

    LockManager* locker = (LockManager*) nativePtr;
    if (locker != NULL)
        delete locker;

//    NSJavaUtil::cb_leave(attach);
}

JNIEXPORT jboolean JNICALL
Java_com_iplanet_server_http_util_LockManager_nativeLock(JNIEnv *env,
    jobject obj, jlong nativePtr, jint lockID)
{
    PRStatus status = PR_FAILURE;
//    PRBool attach = NSJavaUtil::cb_enter();

    LockManager* locker = (LockManager*) nativePtr;
    if (locker != NULL)
        status = locker->lock((int)lockID);
    
//    NSJavaUtil::cb_leave(attach);

    if (status == PR_SUCCESS)
        return JNI_TRUE;
    else
        return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_iplanet_server_http_util_LockManager_nativeUnlock(JNIEnv *env,
    jobject obj, jlong nativePtr, jint lockID)
{
    PRStatus status = PR_FAILURE;
//    PRBool attach = NSJavaUtil::cb_enter();

    LockManager* locker = (LockManager*) nativePtr;
    if (locker != NULL)
        status = locker->unlock((int)lockID);
    
//    NSJavaUtil::cb_leave(attach);

    if (status == PR_SUCCESS)
        return JNI_TRUE;
    else
        return JNI_FALSE;
}
