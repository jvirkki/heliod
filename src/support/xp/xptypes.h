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

#ifndef _XP_XPTYPES_H
#define _XP_XPTYPES_H

/*
 * xptypes.h
 *
 * Cross-platform type definitions.
 */

#ifndef _XP_XPPLATFORM_H
#include "xpplatform.h"
#endif

/*
 * XPInt32
 *
 * 32-bit signed integer with values between XP_INT32_MIN and XP_INT32_MAX.
 */
typedef int XPInt32;
XP_CONST XPInt32 XP_INT32_MIN = -2147483647 - 1;
XP_CONST XPInt32 XP_INT32_MAX = 2147483647;

/*
 * XPUint32
 *
 * 32-bit unsigned integer with values between 0 and XP_UINT32_MAX.
 */
typedef unsigned XPUint32;
XP_CONST XPUint32 XP_UINT32_MAX = 4294967295U;

/*
 * XPInt64
 *
 * 64-bit signed integer with values between XP_INT64_MIN and XP_INT64_MAX.
 */
#if defined(XP_WIN32)
typedef __int64 XPInt64;
#define XP_INT64(n) n##i64
#else
#if defined(XP_LONG64)
typedef long XPInt64;
#else
typedef long long XPInt64;
#endif
#define XP_INT64(n) n##LL
#endif
XP_CONST XPInt64 XP_INT64_MIN = XP_INT64(-9223372036854775807) - 1;
XP_CONST XPInt64 XP_INT64_MAX = XP_INT64(9223372036854775807);

/*
 * XPUint64
 *
 * 64-bit unsigned integer with values between 0 and XP_UINT64_MAX.
 */
#if defined(XP_WIN32)
typedef unsigned __int64 XPUint64;
#define XP_UINT64(n) n##Ui64
#else
#if defined(XP_LONG64)
typedef unsigned long XPUint64;
#else
typedef unsigned long long XPUint64;
#endif
#define XP_UINT64(n) n##ULL
#endif
XP_CONST XPUint64 XP_UINT64_MAX = XP_UINT64(18446744073709551615);

/*
 * XPStatus
 *
 * Status codes to indicate success and failure.
 */
typedef enum {
    XP_SUCCESS = 0,
    XP_FAILURE = -1
} XPStatus;

#endif /* _XP_XPTYPES_H */
