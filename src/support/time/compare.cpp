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
 * compare.c: Time comparison routines
 * 
 * Based on util.cpp - Rob McCool
 */

#include "nstime.h"
#include <stdio.h>
#if defined(AIX)
#include <strings.h>
#endif

/* --------------------------- util_later_than ---------------------------- */

#ifdef WIN32
#ifndef strcasecmp
#define strcasecmp(x,y) stricmp(x,y)
#endif
#else
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#endif


int _mstr2num(char *str) {
    if(!strcasecmp(str, "Jan")) return 0;
    if(!strcasecmp(str, "Feb")) return 1;
    if(!strcasecmp(str, "Mar")) return 2;
    if(!strcasecmp(str, "Apr")) return 3;
    if(!strcasecmp(str, "May")) return 4;
    if(!strcasecmp(str, "Jun")) return 5;
    if(!strcasecmp(str, "Jul")) return 6;
    if(!strcasecmp(str, "Aug")) return 7;
    if(!strcasecmp(str, "Sep")) return 8;
    if(!strcasecmp(str, "Oct")) return 9;
    if(!strcasecmp(str, "Nov")) return 10;
    if(!strcasecmp(str, "Dec")) return 11;
    return -1;
}

int _time_compare(struct tm *lms, char *ims, int later_than_op)
{
    int y = 0, mnum = 0, d = 0, h = 0, m = 0, s = 0, x;
    char t[128];

    /* Supported formats start with weekday (which we don't care about) */
    /* The sizeof(t) is to avoid buffer overflow with t */
    if((!(ims = strchr(ims,' '))) || (strlen(ims) > (sizeof(t) - 2)))
        return 0;

    while(*ims && isspace(*ims)) ++ims;
    if((!(*ims)) || (strlen(ims) < 2))
        return 0;

    /* Standard HTTP (RFC 850) starts with dd-mon-yy */
    if(ims[2] == '-') {
        sscanf(ims, "%s %d:%d:%d", t, &h, &m, &s);
        if(strlen(t) < 6)
            return 0;
        t[2] = '\0';
        t[6] = '\0';
        d = atoi(t);
        mnum = _mstr2num(&t[3]);
        x = atoi(&t[7]);
        /* Postpone wraparound until 2070 */
        y = x + (x < 70 ? 2000 : 1900);
    }
    /* The ctime format starts with a month name */
    else if(isalpha(*ims)) {
        sscanf(ims,"%s %d %d:%d:%d %*s %d", t, &d, &h, &m, &s, &y);
        mnum = _mstr2num(t);
    }
    /* RFC 822 */
    else {
        sscanf(ims, "%d %s %d %d:%d:%d", &d, t, &y, &h, &m, &s);
        mnum = _mstr2num(t);
    }

    if (later_than_op) {
	if( (x = (1900 + lms->tm_year) - y) )
	    return x < 0;

	if(mnum == -1)
	    return 0;

	/* XXXMB - this will fail if you check if december 31 1996 is later
	 * than january 1 1997
	 */
	if((x = lms->tm_mon - mnum) || (x = lms->tm_mday - d) || 
	   (x = lms->tm_hour - h) || (x = lms->tm_min - m) || 
	   (x = lms->tm_sec - s))
	  return x < 0;

	return 1;
    }
    else {
	return (mnum != -1 &&
		1900 + lms->tm_year == y    &&
		lms->tm_mon         == mnum &&
		lms->tm_mday        == d    &&
		lms->tm_hour        == h    &&
		lms->tm_min         == m    &&
		lms->tm_sec         == s);
    }
}


/* Returns 0 if lms later than ims
 * Returns 1 if equal
 * Returns 1 if ims later than lms
 */
TIME_DLL_EXPORT int nstime_later_than(struct tm *lms, char *ims)
{
    return _time_compare(lms, ims, 1);
}


TIME_DLL_EXPORT int nstime_time_equal(struct tm *lms, char *ims)
{
    return _time_compare(lms, ims, 0);
}

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
#define MINIMUM_LENGTH  18
#define RFC1123_DAY      5
#define RFC1123_MONTH    8
#define RFC1123_YEAR     12
#define RFC1123_HOUR     17
#define RFC1123_MINUTE   20
#define RFC1123_SECOND   23
TIME_DLL_EXPORT int nstime_str_time_equal(char *t1, char *t2)
{
    int index;

    /* skip over leading whitespace... */
    while(*t1 && isspace(*t1)) ++t1;
    while(*t2 && isspace(*t2)) ++t2;

    /* Check weekday */
    if ( (t1[0] != t2[0]) || (t1[1] != t2[1]) )
        return -1;

    /* Skip to date */
    while(*t2 && !isspace(*t2)) ++t2;
    t2++;

    /* skip if not strings not long enough */
    if ( (strlen(t1) < MINIMUM_LENGTH) || (strlen(t2) < MINIMUM_LENGTH) )
        return -1;

    if ( (t1[RFC1123_DAY] != t2[0]) || (t1[RFC1123_DAY+1] != t2[1]) )
        return -1;

    /* Skip to the month */
    t2 += 3;

    if ( (t1[RFC1123_MONTH] != t2[0]) || (t1[RFC1123_MONTH+1] != t2[1]) ||
        (t1[RFC1123_MONTH+2] != t2[2]) )
        return -1;

    /* Skip to year */
    t2 += 4;

    if ( (t1[RFC1123_YEAR] != t2[0]) ) {
        /* Assume t2 is RFC 850 format */
        if ( (t1[RFC1123_YEAR+2] != t2[0]) || (t1[RFC1123_YEAR+3] != t2[1]) )
            return -1;

        /* skip to hour */
        t2 += 3;
    } else {
        /* Assume t2 is RFC 1123 format */
        if ( (t1[RFC1123_YEAR+1] != t2[1]) || (t1[RFC1123_YEAR+2] != t2[2]) ||
            (t1[RFC1123_YEAR+3] != t2[3]) )
            return -1;

        /* skip to hour */
        t2 += 5;
    }

    /* check date */
    for (index=0; index<8; index++) {
        if ( t1[RFC1123_HOUR+index] != t2[index] )
            return -1;
    }

    /* Ignore timezone */

    return 0;
}
