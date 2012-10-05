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

// Copyright © 1997 Netscape Communications Corp.  All rights reserved.
// Name:      CriticalSection.h
//
// Summary:
//  Some C++ classes that use NSPR locks to provide non-recursive critical
//  section locking.

#ifndef _NSPR_WRAP_CRITICAL_SECTION_H
#define _NSPR_WRAP_CRITICAL_SECTION_H

#include "prlock.h"
#include <assert.h>

//------------------------------------------------------------------------------
// CriticalSection: A C++ wrapper for an NSPR lock;
// This class creates a new lock on construction and destroys it on
// destruction. It provides methods to acquire and release the lock.
//
// This does not support recursion - i.e. the same thread cannot recursively
// re-enter the same critcal section.
//------------------------------------------------------------------------------

#ifdef XP_PC 
#ifdef BUILD_NSPRWRAP_DLL
#define CRITICALSECTION_DLL_API _declspec(dllexport)
#else
#define CRITICALSECTION_DLL_API _declspec(dllimport)
#endif
#else
#define CRITICALSECTION_DLL_API 
#endif

class CRITICALSECTION_DLL_API CriticalSection
{
public:
    CriticalSection();
    ~CriticalSection();
    void acquire();
    void release();

private:
    PRLock *_crtsec;

friend class ConditionVar; // allows access to the PRLock*
};

//------------------------------------------------------------------------------
// SafeLock: A class that acquires a lock on construction and releases it on
// destruction.
//
// Example usage:
//
//  class Test {
//  public:
//      void f();
//  private:
//      CriticalSection _crit;
//  };
//
//  Test::f() {
//      // some code
//      {
//          SafeLock lock(_crit);
//          // critical code
//      }
//      // some more code
//  }
//
//------------------------------------------------------------------------------

class CRITICALSECTION_DLL_API SafeLock
{
public:
    SafeLock (CriticalSection& crit);   // acquire lock
    ~SafeLock ();                       // release lock
private:
    CriticalSection& _lock; 
};

//------------------------------------------------------------------------------
// Inlines for CriticalSection
//------------------------------------------------------------------------------

inline CriticalSection::CriticalSection():_crtsec(0)
{
    _crtsec = PR_NewLock();
    assert((_crtsec!=0));
}

inline CriticalSection::~CriticalSection()
{
    if (_crtsec) PR_DestroyLock(_crtsec);
}

inline void CriticalSection::acquire()
{
    PR_Lock(_crtsec);
}

inline void CriticalSection::release()
{
    PR_Unlock(_crtsec);
}

//------------------------------------------------------------------------------
// Inlines for SafeLock
//------------------------------------------------------------------------------

inline SafeLock::SafeLock (CriticalSection& lock)
: _lock(lock)
{
    _lock.acquire();
}

inline SafeLock::~SafeLock ()
{
    _lock.release();
}

#endif /*_NSPR_WRAP_CRITICAL_SECTION_H */

