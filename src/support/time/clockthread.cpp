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
 * clockthread.c - Platform independent clockthread
 *
 */

#include "nstime.h"
#include "nspr.h"
#include <time.h>
#ifdef XP_UNIX
#include <poll.h>
#endif

#define SECONDS_UNTIL_SYNC 30 // Resync every 30 seconds

struct ft_callback {
    ft_callback *next;
    ft_callback_fn fn;
    void *context;
};

static PRBool _ft_initialized = PR_FALSE;
static time_t _ft_myclock = 0;
static PRTime _ft_mymicroseconds = 0;
static PRIntervalTime _ft_myticks = 0;
static PRThread *_ft_clockthread;
static ft_callback *_ft_callbacks;
static PRLock *_ft_lock;

static time_t ft_md_time(void)
{
#ifdef SOLARIS
    // On Solaris, gettimeofday() is NOT a system call
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != -1) {
        return tv.tv_sec;
    }
#endif

    return time(NULL);
}

static void ft_md_sleep(int ms)
{
#ifdef XP_WIN32
    PR_Sleep(PR_MillisecondsToInterval(ms));
#else
    // poll() is more efficient than PR_Sleep()
    poll(NULL, NULL, ms);
#endif
}

static void clockthread_main(void *unused)
{
    _ft_myclock = ft_md_time();
    _ft_mymicroseconds = PR_Now();
    _ft_myticks = PR_IntervalNow();

    _ft_initialized = PR_TRUE;

    int seconds_until_sync = 0;
    PRIntervalTime sync_epoch = _ft_myticks;

    for (;;) {
        // Wait until the next second.  We'd rather sleep a little too long so
        // we don't see a given time value twice and potentially miss the next
        // time value altogether.
        int ms_since_sync = PR_IntervalToMilliseconds(PR_IntervalNow() - sync_epoch);
        int ms_until_next_second = 1000 - (ms_since_sync % 1000) + 9;
        if (seconds_until_sync < 1) {
            // Time to resync, wake up on a second boundary
            ft_md_sleep(ms_until_next_second * 3 / 4);
            for (;;) {
                time_t myclock = ft_md_time();
                if (myclock != _ft_myclock) {
                    _ft_myclock = myclock;
                    break;
                }
                ft_md_sleep(10);
            }
            _ft_mymicroseconds = PR_Now();
            _ft_myticks = PR_IntervalNow();
            sync_epoch = _ft_myticks;
            seconds_until_sync = SECONDS_UNTIL_SYNC;            
        } else {
            // Sleep for about a second
            ft_md_sleep(ms_until_next_second);
            _ft_myclock = ft_md_time();
            _ft_mymicroseconds = PR_Now();
            _ft_myticks = PR_IntervalNow();
            seconds_until_sync--;
        }

        // Make callbacks
        PR_Lock(_ft_lock);
        ft_callback *callback = _ft_callbacks;
        while (callback) {
            callback->fn(callback->context);
            callback = callback->next;
        }
        PR_Unlock(_ft_lock);
    }
}

PRBool ft_init()
{
    PRBool rv = PR_TRUE;

    _ft_lock = PR_NewLock();

    _ft_clockthread = PR_CreateThread(PR_SYSTEM_THREAD,
                                      clockthread_main,
                                      NULL,
                                      PR_PRIORITY_NORMAL,
#ifdef SOLARIS
/* Unbound threads aren't guaranteed to wakeup every second... */
                                      PR_GLOBAL_BOUND_THREAD,
#else
                                      PR_GLOBAL_THREAD,
#endif
                                      PR_UNJOINABLE_THREAD,
                                      0);
    if (!_ft_clockthread)
        rv = PR_FALSE;

    return rv;
}

TIME_DLL_EXPORT time_t ft_time()
{
    if (_ft_initialized == PR_FALSE)
        return ft_md_time();
    return _ft_myclock;
}

TIME_DLL_EXPORT PRTime ft_timeNow()
{
    if (_ft_initialized == PR_FALSE)
        return PR_Now();
    return _ft_mymicroseconds;
}

TIME_DLL_EXPORT PRIntervalTime ft_timeIntervalNow()
{
    if (_ft_initialized == PR_FALSE)
        return PR_IntervalNow();
    return _ft_myticks;
}

TIME_DLL_EXPORT int ft_register_cb(ft_callback_fn fn, void *context)
{
    ft_callback *callback = (ft_callback *)malloc(sizeof(*callback));
    if (!callback)
        return -1;

    if (_ft_lock)
        PR_Lock(_ft_lock);

    callback->next = NULL;
    callback->fn = fn;
    callback->context = context;

    ft_callback **pnext = &_ft_callbacks;
    while (*pnext)
        pnext = &(*pnext)->next;
    *pnext = callback;

    if (_ft_lock)
        PR_Unlock(_ft_lock);

    return 0;
}

TIME_DLL_EXPORT int ft_unregister_cb(ft_callback_fn fn, void *context)
{
    int rv = -1;

    if (_ft_lock)
        PR_Lock(_ft_lock);

    ft_callback *callback = _ft_callbacks;
    ft_callback **ptr = &_ft_callbacks;
    while (callback) {
        ft_callback *next = callback->next;

        if (callback->fn == fn && callback->context == context) {
            // Remove this callback entry
            *ptr = next;
            free(callback);
            rv = 0;
        } else {
            ptr = &callback->next;
        }

        callback = next;
    }

    if (_ft_lock)
        PR_Unlock(_ft_lock);

    return rv;
}
