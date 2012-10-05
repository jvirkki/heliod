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
// Name:      ConditionVar.h
//
// Summary:
//  A C++ wrapper for NSPR CondVars (Condition variables).

#ifndef _NSPR_WRAP_CONDITION_VAR_H
#define _NSPR_WRAP_CONDITION_VAR_H

#include "prcvar.h"

//------------------------------------------------------------------------------
// ConditionVar: A C++ wrapper for an NSPR condition variable;
//------------------------------------------------------------------------------

#ifdef XP_PC 
#ifdef BUILD_NSPRWRAP_DLL
#define CONDITIONVAR_DLL_API _declspec(dllexport)
#else
#define CONDITIONVAR_DLL_API _declspec(dllimport)
#endif
#else
#define CONDITIONVAR_DLL_API 
#endif

class CONDITIONVAR_DLL_API ConditionVar
{
public:
    ConditionVar(CriticalSection& crit);
    ~ConditionVar();
    void wait();
    void notify();
    void notifyAll();
    void timedWait (PRIntervalTime timeOut);

private:
    PRCondVar*       _cvar;
};

//------------------------------------------------------------------------------
// Inlines for ConditionVar
//------------------------------------------------------------------------------

inline ConditionVar::ConditionVar(CriticalSection& crit)
{
    _cvar = PR_NewCondVar(crit._crtsec);
}

inline ConditionVar::~ConditionVar()
{
    if (_cvar) PR_DestroyCondVar(_cvar);
}

inline void ConditionVar::wait()
{
    PR_WaitCondVar(_cvar, PR_INTERVAL_NO_TIMEOUT);
}

inline void ConditionVar::timedWait(PRIntervalTime timeOut)
{
    PR_WaitCondVar(_cvar, timeOut);
}

inline void ConditionVar::notify()
{
    PR_NotifyCondVar(_cvar);
}

inline void ConditionVar::notifyAll()
{
    PR_NotifyAllCondVar(_cvar);
}


#endif /* _NSPR_WRAP_CONDITION_VAR_H */
