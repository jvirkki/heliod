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

#ifndef __THREADSAFE_H__
#define __THREADSAFE_H__

// this class provides a central place for thread-safe wraps of thread-unsafe
// Operating System calls.  The ThreadSafe calls are all static.

#include <time.h>
#include <sys/types.h>

#ifndef BUILD_SUPPORT_DLL
#include "CriticalSection.h"
#else // BUILD_SUPPORT_DLL
#include "NsprWrap/CriticalSection.h"
#endif // BUILD_SUPPORT_DLL


#ifdef XP_PC
#ifdef BUILD_NSPRWRAP_DLL
#define THREADSAFE_API _declspec(dllexport)
#else // BUILD_NSPRWRAP_DLL
#define THREADSAFE_API _declspec(dllimport)
#endif // BUILD_NSPRWRAP_DLL
#else // XP_PC
#define THREADSAFE_API
#endif // XP_PC

class THREADSAFE_API ThreadSafe {
public:
  // Time Related functions:

  static struct tm *localtime(const time_t *time, struct tm *result);
  static struct tm *gmtime   (const time_t *time, struct tm *result);
  // Make result large enough to capture result. 
  // 12*strlen(format)+1 is safe. (assumes locale a.m./p.m. format < 15 chars)
  static int        strftime (char *result, const char *format,
			      const struct tm &t);

  // timeZone == 0 -> GMT.  timeZone == 1 -> Local time
  static int        strftimeNow(char *result, const char *format, 
				int useTimeZone);

  // current time in RFC 822/1123 format.  Fits in 32 byte buffer.
  static int        CurrentTimeFixedSize(char *result);

  // returns standard abbrev. for current timezone.  Not changeable at runtime.
  static const char *timeZone(void); 
   
  static const char *standardTimeFormat;
private:
  static void strftime_conv(char *, int, int, char);
  static int  strftime_add(char *dest, const char *src); 

  static int timeZoneInitialized;
  static CriticalSection _libc_timelock;
};

inline int ThreadSafe::strftime_add(char *dest, const char *src) {
  int count = 0;
  while (*dest++ = *src++)
    count++;
  return count;
}

#endif // __THREADSAFE_H__
