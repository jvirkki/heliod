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

#ifndef _XP_XPTIME_H
#define _XP_XPTIME_H

/*
 * xptime.h
 *
 * Declarations related to time.
 */

#include "prinrval.h"
#ifndef _XP_XPTYPES_H
#include "xptypes.h"
#endif

/*
 * XPInterval
 *
 * A time interval expressed as a number of nanoseconds or the special value
 * XP_INTERVAL_INFINITE.
 */
typedef PRUint64 XPInterval;

/*
 * XP_INTERVAL_INFINITE
 *
 * An XPInterval value corresponding to an infinitely long interval, typically
 * used to indicate that an operation should never time out.
 */
static const XPInterval XP_INTERVAL_INFINITE = (XPInterval) -1;

/*
 * XP_NANOSECONDS_PER_SECOND
 *
 * 1000000000, the number of nanoseconds per second.
 */
static const PRUint64 XP_NANOSECONDS_PER_SECOND = 1000000000;

/*
 * XP_IntervalToPRIntervalTime
 *
 * Convert an XPInterval nanosecond value to a PRIntervalTime tick value,
 * rounding up fractional ticks and avoiding overflow.
 */
XP_INLINE PRIntervalTime XP_IntervalToPRIntervalTime(XPInterval interval)
{
    if (interval == XP_INTERVAL_INFINITE)
        return PR_INTERVAL_NO_TIMEOUT;

    static const PRUint64 nanoseconds_per_tick = XP_NANOSECONDS_PER_SECOND / PR_TicksPerSecond();

    PRUint64 rv = (interval + nanoseconds_per_tick - 1) / nanoseconds_per_tick;
    if (rv > PR_UINT32_MAX - 1 || rv == 0 && interval != 0)
        rv = PR_UINT32_MAX - 1;

    return (PRIntervalTime) rv;
}

/*
 * XP_PRIntervalTimeToInterval
 *
 * Convert a PRIntervalTime tick value to an XPInterval nanosecond value.
 */
XP_INLINE XPInterval XP_PRIntervalTimeToInterval(PRIntervalTime interval)
{
    if (interval == PR_INTERVAL_NO_TIMEOUT)
        return XP_INTERVAL_INFINITE;

    static const PRUint64 nanoseconds_per_tick = XP_NANOSECONDS_PER_SECOND / PR_TicksPerSecond();

    return interval * nanoseconds_per_tick;
}

#endif /* _XP_XPTIME_H */
