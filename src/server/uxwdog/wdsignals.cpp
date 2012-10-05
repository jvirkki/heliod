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
 * wdsignals.c - Watchdog signal handling
 *
 *
 */

#include <unistd.h>
#include <signal.h>
#include "wdsignals.h"

/* Defined in watchdog.c */
extern int _watchdog_death;
extern int _watchdog_server_init_done;
extern int _watchdog_server_death;
extern int _watchdog_server_rotate;
extern int _watchdog_server_restart;
extern int _watchdog_server_start_error;

static int watchdog_pending_signal = 0;

static void
sig_term(int sig)
{
    _watchdog_death = 1;
    watchdog_pending_signal = 1;
}

static void
sig_int(int sig)
{
    _watchdog_death = 1;
    watchdog_pending_signal = 1;
}

static void
sig_usr1(int sig)
{
    _watchdog_server_rotate = 1;
    watchdog_pending_signal = 1;
}

static void
sig_usr2(int sig)
{
    watchdog_pending_signal = 1;
}

static void
sig_hup(int sig)
{
    _watchdog_server_restart = 1;
    watchdog_pending_signal = 1;
}

static void
sig_chld(int sig)
{
    _watchdog_server_death = 1;
    watchdog_pending_signal = 1;
}

static void
parent_sig_chld(int sig)
{
    watchdog_pending_signal = 1;
}

static void
parent_sig_usr1(int sig)
{
    _watchdog_server_start_error = 0;
    watchdog_pending_signal = 1;
}

static void
parent_sig_usr2(int sig)
{
    watchdog_pending_signal = 1;
}

void
parent_watchdog_create_signal_handlers(void)
{
    struct sigaction sa;

    sa.sa_handler = parent_sig_usr1;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = parent_sig_usr2;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask,SIGUSR2);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

    sa.sa_handler = parent_sig_chld;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD);
#ifdef SA_NOCLDSTOP
    sa.sa_flags = SA_NOCLDSTOP;
#else
    sa.sa_flags = 0;
#endif /* SA_NOCLDSTOP */
    sigaction(SIGCHLD, &sa, NULL);

    sigset_t unblockset;
    sigemptyset(&unblockset);
    sigaddset(&unblockset, SIGUSR1);
    sigaddset(&unblockset, SIGUSR2);
    sigaddset(&unblockset, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &unblockset, NULL);
}

void
watchdog_create_signal_handlers(void)
{
    struct sigaction sa;

    sa.sa_handler = sig_term;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sig_int;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGPIPE);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    sa.sa_handler = sig_usr1;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = sig_usr2;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR2);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

    sa.sa_handler = sig_hup;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGHUP);
    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = sig_chld;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD);
#ifdef SA_NOCLDSTOP
    sa.sa_flags = SA_NOCLDSTOP;
#else
    sa.sa_flags = 0;
#endif /* SA_NOCLDSTOP */
    sigaction(SIGCHLD, &sa, NULL);

    sigset_t unblockset;
    sigemptyset(&unblockset);
    sigaddset(&unblockset, SIGTERM);
    sigaddset(&unblockset, SIGINT);
    sigaddset(&unblockset, SIGUSR1);
    sigaddset(&unblockset, SIGUSR2);
    sigaddset(&unblockset, SIGHUP);
    sigaddset(&unblockset, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &unblockset, NULL);
}

void
watchdog_delete_signal_handlers(void)
{
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR2);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGHUP);
    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);
}

void
watchdog_wait_signal()
{
    sigset_t entryset;
    sigset_t holdset;

    sigfillset(&holdset);
    sigdelset(&holdset, SIGTERM);
    sigdelset(&holdset, SIGCHLD);
    sigdelset(&holdset, SIGHUP);
    sigdelset(&holdset, SIGUSR1);
    sigdelset(&holdset, SIGUSR2);
    sigprocmask(SIG_SETMASK, &holdset, &entryset);

    for (;;) {
        if (watchdog_pending_signal) {
            watchdog_pending_signal = 0;
            sigprocmask(SIG_SETMASK, &entryset, NULL);
            break;
        }
        sigsuspend(&holdset);
    }
}
