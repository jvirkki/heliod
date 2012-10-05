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

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#if defined(_WINDOWS) || defined(SOLARIS) || defined(AIXV3) || defined(OSF1)
#include "cdefs.h"
#elif !defined(HPUX) && !defined(UNIXWARE)
#include <sys/cdefs.h>
#endif

#include <ncompat.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

int
#ifdef __STDC__
#ifdef IRIX6_5
snprintf(char *str, ssize_t n, const char *fmt, ...)
#else
snprintf(char *str, size_t n, const char *fmt, ...)
#endif
#else
snprintf(str, n, fmt, va_alist)
	char *str;
	size_t n;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
#ifdef VSPRINTF_CHARSTAR
	char *rp;
#endif
	int rval;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
#ifdef VSPRINTF_CHARSTAR
	rp = vsprintf(str, fmt, ap);
	va_end(ap);
	return (strlen(rp));
#else
	rval = vsprintf(str, fmt, ap);
	va_end(ap);
	return (rval);
#endif
}

int
#ifdef __STDC__
#ifdef IRIX6_5
vsnprintf(char *str, ssize_t n, const char *fmt, va_list ap)
#else
vsnprintf(char *str, size_t n, const char *fmt, va_list ap)
#endif
#else
vsnprintf(str, n, fmt, ap)
	char *str;
	size_t n;
	const char *fmt;
	va_list ap;
#endif
{
#ifdef VSPRINTF_CHARSTAR
	return (strlen(vsprintf(str, fmt, ap)));
#else
	return (vsprintf(str, fmt, ap));
#endif
}
