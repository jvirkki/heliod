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


#include "ReadWriteLock.h"


ReadWriteLock::ReadWriteLock(void) 
{
    this->lock_ = PR_NewRWLock(0, "ReadWriteLock");
    PR_ASSERT(this->lock_ != NULL);
}

// Terminate the associated condvars and critical sections
ReadWriteLock::~ReadWriteLock(void)
{
    PR_DestroyRWLock(this->lock_);
}

// Obtain a shared lock. The caller would
// block if there are writers or writeWaiters.
void ReadWriteLock::acquireRead(void)
{
    PR_RWLock_Rlock(this->lock_);
}

// Obtain a shared lock. The caller would
// block if there are other readers or a writer.
void ReadWriteLock::acquireWrite(void)
{
    PR_RWLock_Wlock(this->lock_);
}

// Releases the lock. 
void ReadWriteLock::release(void)
{
    PR_RWLock_Unlock(this->lock_);
}
                                                         
//------------------------------------------------------------------------------
// SafeReadLock
//------------------------------------------------------------------------------

SafeReadLock::SafeReadLock(ReadWriteLock& lock) : lock_(lock)
{
    this->lock_.acquireRead();
}

SafeReadLock::~SafeReadLock(void)
{
    this->lock_.release();
}

//------------------------------------------------------------------------------
// SafeWriteLock
//------------------------------------------------------------------------------

SafeWriteLock::SafeWriteLock(ReadWriteLock& lock) : lock_(lock)
{
    this->lock_.acquireWrite();
}

SafeWriteLock::~SafeWriteLock(void)
{
    this->lock_.release();
}
