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

#ifndef __nserror_h
#define __nserror_h

#ifndef NOINTNSACL
#define INTNSACL
#endif /* !NOINTNSACL */

/*
 * Description (nserror.h)
 *
 *	This file describes the interface to an error handling mechanism
 *	that is intended for general use.  This mechanism uses a data
 *	structure known as an "error frame" to capture information about
 *	an error.  Multiple error frames are used in nested function calls
 *	to capture the interpretation of an error at the different levels
 *	of a nested call.
 */

#include <stdarg.h>
#include <prtypes.h>
#include "public/nsacl/nserrdef.h"

#ifdef INTNSACL

NSPR_BEGIN_EXTERN_C

/* Functions in nseframe.c */
extern void nserrDispose(NSErr_t * errp);
extern NSEFrame_t * nserrFAlloc(NSErr_t * errp);
extern void nserrFFree(NSErr_t * errp, NSEFrame_t * efp);
NSAPI_PUBLIC NSEFrame_t * nserrGenerate(NSErr_t * errp, long retcode, long errorid,
				  char * program, int errc, ...);

/* Functions in nserrmsg.c */
extern char * nserrMessage(NSEFrame_t * efp, int flags);
extern char * nserrRetrieve(NSEFrame_t * efp, int flags);

NSPR_END_EXTERN_C

#endif /* INTNSACL */

#endif /* __nserror_h */
