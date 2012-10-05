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

#include "definesEnterprise.h"
#include "UnixSignals.h"
#include "base/systhr.h"
#include "frame/conf.h"
#include "frame/log.h"
#include "frame/func.h"
#include "httpdaemon/dbthttpdaemon.h"

#ifdef SOLARIS
#include <dlfcn.h>
#include <ucontext.h>
#include <demangle.h>
#endif

int              UnixSignals::_signal_catch_list[MAXSIGNALS];
int              UnixSignals::_signal_notify_list[MAXSIGNALS];
struct sigaction UnixSignals::_signal_sa;

static const char *_msgCrashSignal;
static const char *_msgCrashNsapiSaf;
static const char *_msgCrashFunctionAndModule;

/*--------------------------signal handling routines --------------------*/

void
UnixSignals::Init()
{
    static int _initialized = 0;
    int index;

    if (_initialized)
        return;

    for (index=0; index<MAXSIGNALS; index++)
        _signal_notify_list[index] = 0;
    for (index=0; index<MAXSIGNALS; index++)
        _signal_catch_list[index] = 0;

    /* Have these ready just in case we need them */
    _msgCrashSignal = XP_GetAdminStr(DBT_Crash_Signal);
    _msgCrashNsapiSaf = XP_GetAdminStr(DBT_Crash_NsapiSaf);
    _msgCrashFunctionAndModule = XP_GetAdminStr(DBT_Crash_FunctionAndModule);

    _initialized = 1;
}

void
UnixSignals::Ignore(int sig)
{
    PR_ASSERT(sig >= 0 && sig < MAXSIGNALS);

    // always ignore the signals; the thread in sigwait will catch them
    _signal_sa.sa_flags = SA_RESTART;
    if (sig == SIGCHLD)
        /* This prevents zombies without waits */
#if defined(LINUX)
        _signal_sa.sa_flags |= SA_NOCLDSTOP;
#else
        _signal_sa.sa_flags |= SA_NOCLDWAIT | SA_NOCLDSTOP;
#endif
    _signal_sa.sa_handler = SA_HANDLER_T(SIG_IGN);
    sigaction(sig, &_signal_sa, NULL);

    _signal_catch_list[sig] = 0;

}

void
UnixSignals::Default(int sig)
{
    PR_ASSERT(sig >= 0 && sig < MAXSIGNALS);

    // always ignore the signals; the thread in sigwait will catch them
    _signal_sa.sa_flags = SA_RESTART;
    _signal_sa.sa_handler = SA_HANDLER_T(SIG_DFL);
    sigaction(sig, &_signal_sa, NULL);

    _signal_catch_list[sig] = 0;
}

static void
_signal_handler(int signum)
{
    // Using locks or condition variables in signal handlers is unsafe.
    // So all we can do is set the flag. 
    // If you try to use PR_Lock() here, you will see assertion failures
    // from inside NSPR.
    if (signum >= 0 && signum < MAXSIGNALS)
        UnixSignals::_signal_notify_list[signum]++;
}

// Attempt to log some diagnostics in the event of a crash
#ifdef SOLARIS
static void _crash_handler(int signum, siginfo_t *sip, void *uap)
#else
static void _crash_handler(int signum)
#endif
{
    signal(signum, SIG_DFL);

    char signal[8];
    switch (signum) {
    case SIGSEGV: strcpy(signal, "SIGSEGV"); break;
    case SIGBUS:  strcpy(signal, "SIGBUS"); break;
    case SIGILL:  strcpy(signal, "SIGILL"); break;
    case SIGFPE:  strcpy(signal, "SIGFPE"); break;
    default:      sprintf(signal, "%d", signum); break;
    }

    ereport_disaster(LOG_CATASTROPHE, _msgCrashSignal, signal);

    const char *fn = func_current();
    if (fn) {
        ereport_disaster(LOG_INFORM, _msgCrashNsapiSaf, fn);
    }

#ifdef SOLARIS
#ifndef __i386
    void *addr = NULL;

    if (sip) {
        // Find address of faulting instruction
        switch (signum) {
        case SIGSEGV:
        case SIGBUS:
        case SIGILL:
        case SIGFPE:
            addr = sip->__data.__fault.__pc;
        }
    }

    if (!addr && uap) {
        ucontext_t *ucp = (ucontext_t*)uap;
        addr = (void*)ucp->uc_mcontext.gregs[REG_PC];
    }

    // Find symbol associated with faulting instruction
    if (addr) {
        Dl_info dli;
        if (dladdr(addr, &dli)) {
            char demangled[256];
            const char *symbol = dli.dli_sname;
            if (cplus_demangle(symbol, demangled, sizeof(demangled)) == 0)
                symbol = demangled;
            ereport_disaster(LOG_INFORM, _msgCrashFunctionAndModule, symbol,
                             dli.dli_fname);
        }
    }
#endif
#endif

    raise(signum);
}

void
UnixSignals::Catch(int sig)
{
    _signal_sa.sa_flags = SA_RESTART;
    _signal_sa.sa_handler = SA_HANDLER_T(_signal_handler);
    sigemptyset(&_signal_sa.sa_mask);
    sigaction(sig, &_signal_sa, NULL);

    _signal_catch_list[sig] = 1;
}

void
UnixSignals::CatchCrash(int sig)
{
    _signal_sa.sa_flags = SA_RESTART;
#ifdef SOLARIS
    _signal_sa.sa_flags |= SA_SIGINFO;
    _signal_sa.sa_sigaction = _crash_handler;
#else
    _signal_sa.sa_handler = SA_HANDLER_T(_crash_handler);
#endif
    sigemptyset(&_signal_sa.sa_mask);
    sigaction(sig, &_signal_sa, NULL);
}

void
UnixSignals::Fake(int sig)
{
    if (sig >= 0 && sig < MAXSIGNALS)
        _signal_notify_list[sig]++;
}

int
UnixSignals::Get()
{
    int index;

    for (index=0; index < MAXSIGNALS; index++) {
        if (_signal_notify_list[index] != 0) {
            _signal_notify_list[index]--;
            return index;
        }
    }

    return -1;
}
