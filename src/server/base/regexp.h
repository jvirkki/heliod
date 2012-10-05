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

#ifndef BASE_REGEXP_H
#define BASE_REGEXP_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * regexp.h: Wrapper for system's regular expression handlers
 *
 *
 *
 *
 *
 *
 * Ari Luotonen
 * Copyright (c) 1995 Netscape Communcations Corporation
 *
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

/* See public/base/regexp.h or public/base/shexp.h concerning USE_REGEX */

/*
 * This little bit of nonsense is because USE_REGEX is currently
 * supposed to be recognized only by the proxy.  If that's the
 * case, only the proxy should define USE_REGEX, but I'm playing
 * it safe.  XXXHEP 12/96
 */
#ifndef MCC_PROXY
#ifdef USE_REGEX
#define SAVED_USE_REGEX USE_REGEX
#undef USE_REGEX
#endif /* USE_REGEX */
#endif /* !MCC_PROXY */

/* --- Begin function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC int INTregexp_valid(const char *exp);

NSAPI_PUBLIC int INTregexp_match(const char *str, const char *exp);

NSAPI_PUBLIC int INTregexp_cmp(const char *str, const char *exp);

NSAPI_PUBLIC int INTregexp_casecmp(const char *str, const char *exp);

NSPR_END_EXTERN_C

#define regexp_valid INTregexp_valid
#define regexp_match INTregexp_match
#define regexp_cmp INTregexp_cmp
#define regexp_casecmp INTregexp_casecmp

#endif /* INTNSAPI */

/* Restore USE_REGEX definition for non-proxy.  See above. */
#ifdef SAVED_USE_REGEX
#define USE_REGEX SAVED_USE_REGEX
#undef SAVED_USE_REGEX
#endif /* SAVED_USE_REGEX */

#endif /* !BASE_REGEXP_H */
