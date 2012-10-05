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

#ifndef __LockManager_h__
#define __LockManager_h__

#include "nspr.h"                             // NSPR declarations
#include "NsprWrap/Sleeper.h"                 // Sleeper class
#include "NsprWrap/ProcessLock.h"             // ProcessLock class

#if !defined (_NS_SERVLET_EXPORT)
#ifdef XP_PC
#define _NS_SERVLET_EXPORT  _declspec(dllexport)
#define _NS_SERVLET_IMPORT  _declspec(dllimport)
#else
#define _NS_SERVLET_EXPORT
#define _NS_SERVLET_IMPORT
#endif
#endif


/**
 * A class that manages a set of cross-process locks for use in Java HTTP
 * session management when MaxProcs > 1.
 *
 * The set of locks managed by this class is used to ensure session
 * data integrity across all the sessions in a specific web-application.
 * It is the responsibility of the caller to maintain the mapping between
 * the session ID and a lock ID. A session ID must ALWAYS map to the same
 * lock ID as this calculation is performed in every web server process
 * and not necessarily the one that created the session.
 *
 */
class _NS_SERVLET_EXPORT LockManager
{
    public:

        /**
         * Creates <code>maxLocks</code> cross-process locks using
         * <code>appName</code> as a prefix for the lock names.
         */
        LockManager(const char* appName, const PRUint32 maxLocks);

        /**
         * Closes the cross-process locks.
         */
        ~LockManager(void);

        /**
         * Initializes the process ID of the parent process. This number
         * is a component of the lock file name so as to distinguish between
         * lock files of different iWS instances on the same machine.
         *
         * This must be invoked from the primordial process (i.e EarlyInit stage).
         */
        static PRStatus Initialize(void);

        /**
         * Acquires the specified cross process lock. 
         *
         * @param lockID Must be less than <code>maxLocks</code>.
         * @returns      <code>PR_SUCCESS</code> if successful and 
         *               <code>PR_FAILURE</code> if an error/timeout occurred 
         *               while acquiring the lock.
         */
        PRStatus lock(const PRUint32 lockID);

        /**
         * Releases the specified cross process lock. 
         *
         * @param lockID Must be less than <code>maxLocks</code>.
         * @returns      Returns <code>PR_SUCCESS</code> if successful and 
         *               <code>PR_FAILURE</code> if an error occurred while
         *               releasing the lock.
         */
        PRStatus unlock(const PRUint32 lockID);

    private:

        /**
         * The array of cross-process locks created and managed by this
         * object.
         */
        ProcessLock** locks_;

        /**
         * The number of entries in the <code>locks_</code> array.
         */
        PRUint32 nLocks_;

        /**
         * Used to implement the busy-wait loop when a deadlock is detected
         * on the process lock.
         */
        Sleeper sleeper_;
         
        /**
         * Specifies the process ID of the parent (primordial) process.
         *
         * This value is set by the Initialize method in order to 
         * circumvent the quirky behaviour of getppid() on a multithreaded
         * application on Linux.
         */
        static PRInt32 ppid_;
};

#endif  // __LockManager_h__
