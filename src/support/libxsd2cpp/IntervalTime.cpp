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

#include <stdlib.h>
#include <limits.h>
#include "xercesc/util/XMLString.hpp"
#include "libxsd2cpp/IntervalTime.h"

using namespace XERCES_CPP_NAMESPACE;
using namespace LIBXSD2CPP_NAMESPACE;

/**
 * MAX_SECONDS_IN_MILLISECONDS is the largest interval in milliseconds we will
 * squeeze into an int after converting to seconds.
 */
#if (defined(__GNUC__) && (__GNUC__ > 2))
static const PRInt64 MAX_SECONDS_IN_MILLISECONDS = 2073600000000LL;
#else
static const PRInt64 MAX_SECONDS_IN_MILLISECONDS = 2073600000000;
#endif

/**
 * MAX_SECONDS is the largest interval in seconds we will store in an int.
 */
static const int MAX_SECONDS = (MAX_SECONDS_IN_MILLISECONDS / 1000);

/**
 * MAX_PRINTERVALTIME_IN_MILLISECONDS is the largest interval in milliseconds
 * we will squeeze into a PRIntervalTime.  (The ratio between PRIntervalTime
 * ticks and milliseconds varies by platform, but there are never more than
 * 100 ticks per millisecond.)
 */
static const PRInt64 MAX_PRINTERVALTIME_IN_MILLISECONDS = 21600000;

/**
 * MAX_PRINTERVALTIME is the largest interval in PRIntervalTime ticks we will
 * store in a PRIntervalTime.
 */
static const int MAX_PRINTERVALTIME = PR_MillisecondsToInterval(MAX_PRINTERVALTIME_IN_MILLISECONDS);

//-----------------------------------------------------------------------------
// IntervalTime::IntervalTime
//-----------------------------------------------------------------------------

IntervalTime::IntervalTime(const DOMElement *elementArg)
: Simple(elementArg)
{
    char *transcoded = XMLString::transcode(elementArg->getTextContent());

    double d = strtod(transcoded, NULL) * 1000.0;
    if (d >= LL_MAXINT) {
        millisecondsValue = LL_MAXINT;
    } else if (d > 0.0) {
        millisecondsValue = (PRInt64)d;
        if (millisecondsValue < 1)
            millisecondsValue = 1;
    } else if (d == 0.0) {
        millisecondsValue = 0;
    } else {
        millisecondsValue = -1;
    }

    XMLString::release(&transcoded);
}

//-----------------------------------------------------------------------------
// IntervalTime::getSecondsValue
//-----------------------------------------------------------------------------

int IntervalTime::getSecondsValue() const
{
    int seconds;

    if (millisecondsValue >= MAX_SECONDS_IN_MILLISECONDS) {
        seconds = MAX_SECONDS;
    } else if (millisecondsValue >= 0) {
        seconds = (int)((millisecondsValue + 999) / 1000);
    } else {
        seconds = -1;
    }

    return seconds;
}

//-----------------------------------------------------------------------------
// IntervalTime::getMillisecondsValue
//-----------------------------------------------------------------------------

int IntervalTime::getMillisecondsValue() const
{
    int milliseconds;

    if (millisecondsValue > INT_MAX) {
        milliseconds = INT_MAX;
    } else {
        milliseconds = (int)millisecondsValue;
    }

    return milliseconds;
}

//-----------------------------------------------------------------------------
// IntervalTime::getPRIntervalTimeValue
//-----------------------------------------------------------------------------

PRIntervalTime IntervalTime::getPRIntervalTimeValue() const
{
    PRIntervalTime interval;

    PR_ASSERT(MAX_PRINTERVALTIME != PR_INTERVAL_NO_TIMEOUT);

    if (millisecondsValue >= MAX_PRINTERVALTIME_IN_MILLISECONDS) {
        interval = MAX_PRINTERVALTIME;
    } else if (millisecondsValue >= 0) {
        interval = PR_MillisecondsToInterval((int)millisecondsValue);
        if (interval < 1)
            interval = 1;
    } else if (millisecondsValue == 0) {
        interval = PR_INTERVAL_NO_WAIT;
    } else {
        interval = PR_INTERVAL_NO_TIMEOUT;
    }

    return interval;
}
