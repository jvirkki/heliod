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
 *
 * nstime.c
 *
 * A library for time manipulation and performance routines.
 *
 * Mike Belshe
 * parsing code stolen from zawinski's xp_time
 * 11-6-97
 * 
 */

#ifndef _NS_TIME_H_
#define _NS_TIME_H_

#ifdef WIN32
#ifdef BUILD_TIME_DLL
#define TIME_DLL_EXPORT __declspec(dllexport)
#else
#define TIME_DLL_EXPORT __declspec(dllexport)
#endif
#else
#define TIME_DLL_EXPORT
#endif

#ifdef WIN32
#include <windows.h>
#include <time.h>
#else
#ifdef Linux
#include <time.h>
#include <sys/poll.h>
#else
#include <sys/types.h>
#include <sys/time.h>
#endif
#endif
#if defined(AIX)
#include <time.h>
#endif

#include "support/NSString.h"
#include "prtime.h"
#include "prinrval.h"

/************************************************
 * General functions
 ************************************************/

void TIME_DLL_EXPORT nstime_init();


/************************************************
 * Performance time routines
 ************************************************/

extern "C" {
    typedef void (*ft_callback_fn)(void *context);
}

/*
 * Get the time in seconds since epoch
 *
 * This routine is just like unix's time() call, but it doesn't take a 
 * system call.
 */
TIME_DLL_EXPORT time_t ft_time(void);

/*
 * Get PR_Now in a cached manner avoiding calls to gettimeofday which happen
 * happen when PR_Now is directly used
 */
TIME_DLL_EXPORT PRTime ft_timeNow();

/*
 * Get PR_IntervalNow in a cached manner avoiding calls to gettimeofday
 * which happen when PR_IntervalNow is directly used
 */
TIME_DLL_EXPORT PRIntervalTime ft_timeIntervalNow();

/*
 * Register a function to be called once every second
 */
TIME_DLL_EXPORT int ft_register_cb(ft_callback_fn fn, void *context);

/*
 * Unregister a previously registered callback function
 */
TIME_DLL_EXPORT int ft_unregister_cb(ft_callback_fn fn, void *context);


/************************************************
 * Utility functions
 ************************************************/

/* This parses a time/date string into a time_t
 * (seconds after "1-Jan-1970 00:00:00 GMT")
 * If it can't be parsed, 0 is returned.
 *
 *  Many formats are handled, including:
 *
 *    14 Apr 89 03:20:12
 *    14 Apr 89 03:20 GMT
 *    Fri, 17 Mar 89 4:01:33
 *   Fri, 17 Mar 89 4:01 GMT
 *   Mon Jan 16 16:12 PDT 1989
 *   Mon Jan 16 16:12 +0130 1989
 *   6 May 1992 16:41-JST (Wednesday)
 *   22-AUG-1993 10:59:12.82
 *   22-AUG-1993 10:59pm
 *   22-AUG-1993 12:59am
 *   22-AUG-1993 12:59 PM
 *   Friday, August 04, 1995 3:54 PM
 *   06/21/95 04:24:34 PM
 *   20/06/95 21:07
 *   95-06-08 19:32:48 EDT
 *
 * If the input string doesn't contain a description of the timezone,
 * we consult the `default_to_gmt' to decide whether the string should
 * be interpreted relative to the local time zone (FALSE) or GMT (TRUE).
 * The correct value for this argument depends on what standard specified
 * the time string which you are parsing.
 */
TIME_DLL_EXPORT time_t time_parse_string(const char *fmt, int default_to_gmt);

/************************************************
 * Time comparison functions
 ************************************************/

/* nstime_str_time_equal()
 *
 * Function to compare if two time strings are equal
 *
 * Acceptible date formats:
 *      Saturday, 17-Feb-96 19:41:34 GMT        <RFC850>
 *      Sat, 17 Mar 1996 19:41:34 GMT           <RFC1123>
 *
 * Argument t1 MUST be RFC1123 format.
 *
 * Note- it is not the intention of this routine to *always* match
 *       There are cases where we would return != when the strings might
 *       be equal (especially with case).  The converse should not be true.
 *
 * Return 0 if equal, -1 if not equal.
 */
TIME_DLL_EXPORT int nstime_str_time_equal(char *t1, char *t2);

/* nstime_time_equal
 * Determine if two times are equal.
 *
 * Return 0 if equal, 1 otherwise.
 * - XXXMB - UNTESTED IN 4.0
 */
TIME_DLL_EXPORT int nstime_time_equal(struct tm *lms, char *ims);

/* nstime_later_than
 *
 * Compare two times
 *
 * Returns 0 if lms later than ims
 * Returns 1 if equal
 * Returns -1 if ims later than lms
 * - XXXMB - UNTESTED IN 4.0
 */
TIME_DLL_EXPORT int nstime_later_than(struct tm *lms, char *ims);

#endif
