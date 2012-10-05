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

#include "ThreadSafe.h"
#include <string.h>
#include <stdio.h>

CriticalSection ThreadSafe::_libc_timelock;
const char *ThreadSafe::standardTimeFormat = "%a, %d %b %Y %H:%M:%S GMT";

struct tm *ThreadSafe::localtime(const time_t *clock, struct tm *res) {
#ifdef HAVE_TIME_R
  return localtime_r(clock, res);
#else
  SafeLock lock(_libc_timelock);
  struct tm *temp = ::localtime(clock);
  *res = *temp;
  return res;
#endif
}

struct tm *ThreadSafe::gmtime(const time_t *clock, struct tm *res) {
#ifdef HAVE_TIME_R
  return gmtime_r(clock, res);
#else
  SafeLock lock(_libc_timelock);
  struct tm *temp = ::gmtime(clock);
  *res = *temp;
  return res;
#endif  
}

/* ------------------------- ThreadSafe::strftime --------------------------- */
/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// someone should verify that these statements appear in our licenses. I don't
// think they do right now.  They certainly don't appear in our advertisements.

#if defined(LIBC_SCCS) && !defined(lint)
//static char sccsid[] = "@(#)strftime.c	5.11 (Berkeley) 2/24/91";
#endif /* LIBC_SCCS and not lint */

#define TM_YEAR_BASE 1900


/* util_strftime()
 * This is an optimized version of strftime for speed.  Avoids the thread
 * unsafeness of BSD strftime calls. 
 */
int ThreadSafe::strftime(char *pt, const char *format, const struct tm &t)
{
  static const char *afmt[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
  };
  static const char *Afmt[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
    "Saturday",
  };

  static const char *bfmt[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
    "Oct", "Nov", "Dec",
  };
  static const char *Bfmt[] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December",
  };

  char *start = pt;
  const char *scrap;
  
  for (; *format; ++format) {
    if (*format == '%')
      switch(*++format) {
      case 'a': /* abbreviated weekday name */
	*pt++ = afmt[t.tm_wday][0];
	*pt++ = afmt[t.tm_wday][1];
	*pt++ = afmt[t.tm_wday][2];
	continue;
      case 'd': /* day of month */
	strftime_conv(pt, t.tm_mday, 2, '0');
	pt += 2;
	continue;
      case 'S':
	strftime_conv(pt, t.tm_sec, 2, '0');
	pt += 2;
	continue;
      case 'M':
	strftime_conv(pt, t.tm_min, 2, '0');
	pt += 2;
	continue;
      case 'H':
	strftime_conv(pt, t.tm_hour, 2, '0');
	pt += 2;
	continue;
      case 'Y':
	if (t.tm_year < 100) {
	  *pt++ = '1';
	  *pt++ = '9';
	  strftime_conv(pt, t.tm_year, 2, '0');
	} else {
	  /* will fail after 2100; but who cares? */
	  *pt++ = '2';
	  *pt++ = '0';
	  strftime_conv(pt, t.tm_year-100, 2, '0');
	}
	pt += 2;
	continue;
      case 'b': /* abbreviated month name */
      case 'h':
	*pt++ = bfmt[t.tm_mon][0];
	*pt++ = bfmt[t.tm_mon][1];
	*pt++ = bfmt[t.tm_mon][2];
	continue;
      case 'T':
      case 'X':
	pt += ThreadSafe::strftime(pt, "%H:%M:%S", t);
	continue;
      case '\0':
	--format;
	break;
      case 'A':
	if (t.tm_wday < 0 || t.tm_wday > 6)
	  return(0);
	scrap = Afmt[t.tm_wday];
	pt += strftime_add(pt, scrap);
	continue;
      case 'B':
	if (t.tm_mon < 0 || t.tm_mon > 11)
	  return(0);
	scrap = Bfmt[t.tm_mon];
	pt += strftime_add(pt, scrap);
	continue;
      case 'C':
	// use Solaris semantics for %C
	pt += ThreadSafe::strftime(pt, "%a %b %e %H:%M:%S %Y", t);
	continue;
      case 'c':
	pt += ThreadSafe::strftime(pt, "%m/%d/%y %H:%M:%S", t);
	continue;
      case 'D':
	pt += ThreadSafe::strftime(pt, "%m/%d/%y", t);
	continue;
      case 'e':
	strftime_conv(pt, t.tm_mday, 2, ' ');
	pt += 2;
	continue;
      case 'I':
	strftime_conv(pt, t.tm_hour % 12 ?
		      t.tm_hour % 12 : 12, 2, '0');
	pt += 2;
	continue;
      case 'j':
	strftime_conv(pt, t.tm_yday + 1, 3, '0');
	pt += 3;
	continue;
      case 'k':
	strftime_conv(pt, t.tm_hour, 2, ' ');
	pt += 2;
	continue;
      case 'l':
	strftime_conv(pt, t.tm_hour % 12 ?
		      t.tm_hour % 12 : 12, 2, ' ');
	pt += 2;
	continue;
      case 'm':
	strftime_conv(pt, t.tm_mon + 1, 2, '0');
	pt += 2;
	continue;
      case 'n':
	*pt = '\n';
	pt++;
	continue;
      case 'p':
	if (t.tm_hour >= 12) {
	  *pt = 'P';
	  pt++;
	} else {
	  *pt = 'A';
	  pt++;
	}
	*pt = 'M';
	pt++;
	continue;
      case 'R':
	pt += ThreadSafe::strftime(pt, "%H:%M", t);
	continue;
      case 'r':
	pt += ThreadSafe::strftime(pt, "%I:%M:%S %p", t);
	continue;
      case 't':
	*pt = '\t';
	pt++;
	continue;
      case 'U':
	strftime_conv(pt, (t.tm_yday + 7 - t.tm_wday) / 7, 2, '0');
	pt += 2;
	continue;
      case 'W':
	strftime_conv
	  (pt, (t.tm_yday + 7 - (t.tm_wday ? (t.tm_wday - 1) : 6)) / 7, 2, '0');
	pt += 2;
	continue;
      case 'w':
	strftime_conv(pt, t.tm_wday, 1, '0');
	pt += 1;
	continue;
      case 'x':
	pt += ThreadSafe::strftime(pt, "%m/%d/%y", t);
	continue;
      case 'y':
	strftime_conv(pt, (t.tm_year + TM_YEAR_BASE)
			    % 100, 2, '0');
	pt += 2;
	continue;
      case 'Z':
	pt += strftime_add(pt, timeZone());
	continue;
      case '%':
	/*
	 * X311J/88-090 (4.12.3.5): if conversion char is
	 * undefined, behavior is undefined.  Print out the
	 * character itself as printf(3) does.
	 */
      default:
	break;
      }
    *pt = *format;
    pt++;
  }
  
  start[pt-start] = '\0';
  
  return pt - start;
}

void ThreadSafe::strftime_conv(char *pt, int n, int digits, char pad)
{
  char buf[10];
  register char *p;
  
  if (n >= 100) {
    p = buf + sizeof(buf)-2;
    for (; n > 0 && p > buf; n /= 10, --digits)
      *p-- = n % 10 + '0';
    while (p > buf && digits-- > 0)
      *p-- = pad;
    p++;
    pt += strftime_add(pt, p);
  } else {
    int tens;
    
    tens = 0;
    if ( n >= 10 ) {
      while ( n >= 10 ) {
	tens++;
	n-=10;
      }
      *pt++ = '0'+tens;
      digits--;
    }
    else 
      *pt++ = '0';
    *pt++ = '0'+n;
    digits--;
    while(digits--)
      *pt++ = pad;
  }
  return;
}

int ThreadSafe::timeZoneInitialized = 0;

const char *ThreadSafe::timeZone(void) {
  // this may require expansion for other platforms.
  if (!timeZoneInitialized) {
    SafeLock lock(_libc_timelock);
    if (!timeZoneInitialized) {
      tzset();
      timeZoneInitialized = 1;
    }
  }
  return tzname[1];
}

int ThreadSafe::CurrentTimeFixedSize(char *result) {
  return strftimeNow(result, standardTimeFormat, 0);
}

int ThreadSafe::strftimeNow(char *result, const char *format, int tz) {
  time_t now = time(NULL);
  struct tm now_tm;
  if (!tz) {
    ThreadSafe::gmtime(&now, &now_tm);
  } else {
    ThreadSafe::localtime(&now, &now_tm);
  }
  return strftime(result, format, now_tm);
}
