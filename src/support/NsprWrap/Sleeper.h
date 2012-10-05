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

#ifndef __Sleeper_h__
#define __Sleeper_h__

#include "nspr.h"                             // NSPR declarations
#include "CriticalSection.h"                  // CriticalSection, SafeLock
#include "ConditionVar.h"                     // ConditionVar

#if !defined (_SLEEPER_EXPORT)
#ifdef XP_PC
#define _SLEEPER_EXPORT  _declspec(dllexport)
#define _SLEEPER_IMPORT  _declspec(dllimport)
#else
#define _SLEEPER_EXPORT
#define _SLEEPER_IMPORT
#endif
#endif


/**
 * A class that can be used as an alternative to PR_Sleep.
 *
 * Every invocation to <code>PR_Sleep</code> causes a condition variable
 * to be allocated and that condition variable is used to implement the
 * desired sleep timeout. For long running threads in an application that
 * frequently need to sleep, an object of this class can be allocated 
 * during thread initialization and used for the lifetime of the thread.
 * This class can also be used in the case when PR_Sleep is invoked in
 * a busy-wait loop.
 *
 */
class _SLEEPER_EXPORT Sleeper
{
    public:

        /**
         * Creates the condition variable (and its associated mutex) that
         * implements the sleep.
         */
        Sleeper(void);

        /**
         * Destroys its condition variable and mutex.
         */
        ~Sleeper(void);

        /**
         * Sleeps for the specified timeout.
         */
        void sleep(const PRIntervalTime timeOut);

    private:

        /**
         * A condition variable to implement sleep.
         */
        ConditionVar* sleeper_;

        /**
         * Mutex used by <code>sleeper_</code>.
         */
        CriticalSection* sleeperLock_;
};

#endif  // __Sleeper_h__
