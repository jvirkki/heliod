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

#ifndef _ReadWriteLock_h
#define _ReadWriteLock_h

#include "nspr.h"              // NSPR threads declarations
#include "CriticalSection.h"
#include "ConditionVar.h"

//------------------------------------------------------------------------------
// ReadWriteLock:
//    Implements a shared/exclusive lock package atop the
// critical section/condition variables. It allows multiple
// shared lock holders and only one exclusive lock holder
// on a lock variable.
//
// NOTE : It currently favors writers over readers and writers
//  may starve readers. It is usually preferable to allow updates
//  if they are not as frequent. We may have to change this if
//  the common usage pattern differs from this.
//------------------------------------------------------------------------------

#ifdef XP_PC 
#ifdef BUILD_NSPRWRAP_DLL
#define RWLOCK_DLL_API _declspec(dllexport)
#else
#define RWLOCK_DLL_API _declspec(dllimport)
#endif
#else
#define RWLOCK_DLL_API 
#endif

/**
 * This class provides a simple object oriented interface to
 * <a href="http://www.mozilla.org/docs/refList/refNSPR/contents.html">NSPR</a>
 * reader-writer locks.
 *
 * @author  $Author: arvinds $
 * @version $Revision: 1.7.2.1 $
 */
class RWLOCK_DLL_API ReadWriteLock
{
    public:
        ReadWriteLock(void);
        ~ReadWriteLock(void);

        void acquireWrite(void);
        void acquireRead(void);
        void release(void);

    private:
        PRRWLock* lock_;
};


/**
 * SafeReadLock: A class that acquires a read lock on construction and
 * releases it on destruction.
 *
 * @author  $Author: arvinds $
 * @version $Revision: 1.7.2.1 $
 */
class RWLOCK_DLL_API SafeReadLock
{
    public:
        SafeReadLock(ReadWriteLock& lock);   // acquire lock
        ~SafeReadLock(void);                 // release lock

    private:
        ReadWriteLock& lock_; 
};

/**
 * SafeWriteLock: A class that acquires a write lock on construction and
 * releases it on destruction.
 *
 * @author  $Author: arvinds $
 * @version $Revision: 1.7.2.1 $
 */

class RWLOCK_DLL_API SafeWriteLock
{
    public:
        SafeWriteLock(ReadWriteLock& lock);   // acquire lock
        ~SafeWriteLock(void);                 // release lock

    private:
        ReadWriteLock& lock_; 
};

#endif // _ReadWriteLock_h
