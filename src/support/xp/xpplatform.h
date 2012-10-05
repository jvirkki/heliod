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

#ifndef _XP_XPPLATFORM_H
#define _XP_XPPLATFORM_H

/*
 * xpplatform.h
 *
 * Platform detection and macro definition for the XP cross-platform library.
 */

#if !defined(XP_WIN32) && !defined(XP_UNIX)
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define XP_WIN32
#else
#define XP_UNIX
#endif
#endif

#if defined(__cplusplus)
#define XP_EXTERN_C extern "C"
#define XP_BEGIN_EXTERN_C extern "C" {
#define XP_END_EXTERN_C }
#define XP_INLINE inline
#define XP_CONST const
#else
#define XP_EXTERN_C
#define XP_BEGIN_EXTERN_C
#define XP_END_EXTERN_C
#define XP_INLINE static inline
#define XP_CONST static const
#endif

#if defined(XP_WIN32)
#define XP_IMPORT XP_EXTERN_C __declspec(dllimport)
#define XP_EXPORT XP_EXTERN_C __declspec(dllexport)
#else
#define XP_IMPORT XP_EXTERN_C
#define XP_EXPORT XP_EXTERN_C
#endif

#if defined(__sparcv9) || defined(__amd64)
#define XP_INT32
#define XP_LONG64
#define XP_PTR64
#else
#define XP_INT32
#define XP_LONG32
#define XP_PTR32
#endif

#if defined(DEBUG)
#include <assert.h>
#define XP_ASSERT(expr) assert(expr)
#else
#define XP_ASSERT(expr)
#endif

#endif /* _XP_XPPLATFORM_H */
