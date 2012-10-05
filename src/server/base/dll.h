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
 * dll.h: Handle dynamically linked libraries
 * 
 * Rob McCool
 */

#ifndef _DLL_H
#define _DLL_H

#include "systems.h"

#if defined(DLL_CAPABLE)

#if defined (DLL_DLOPEN)
#include <dlfcn.h>
#endif

/* --------------------------- Data structures ---------------------------- */
#if !defined (DLL_DLOPEN_FLAGS)
#define DLL_DLOPEN_FLAGS 0
#endif

#if !defined (RTLD_LAZY)
#define RTLD_LAZY	0
#endif

#if !defined (RTLD_NOW)
#define RTLD_NOW	0
#endif

#if !defined (RTLD_GLOBAL)
#define RTLD_GLOBAL 0
#endif

#if !defined (RTLD_LOCAL)
#define RTLD_LOCAL	0
#endif


typedef void *DLHANDLE;
#define DLHANDLE_ERROR NULL

/* ------------------------------ Prototypes ------------------------------ */


/*
 * dll_open loads the library at the given path into memory, and returns
 * a handle to be used in later calls to dll_findsym and dll_close.
 */
NSAPI_PUBLIC DLHANDLE dll_open(const char *fn);

NSAPI_PUBLIC DLHANDLE dll_open2(const char *fn, int mode);

/*
 * dll_findsym looks for a symbol with the given name in the library 
 * pointed to by the given handle. Returns a pointer to the named function.
 */
NSAPI_PUBLIC void *dll_findsym(DLHANDLE dlp, const char *name);


/*
 * dll_error returns a string describing the last error on the given handle
 */
NSAPI_PUBLIC char *dll_error(DLHANDLE dlp);


/*
 * dll_close closes the previously opened library given by handle
 */
NSAPI_PUBLIC void dll_close(DLHANDLE dlp);

/*
 * dll_library returns the fully qualified path to the server library
 */
NSAPI_PUBLIC const char *dll_library(void);

#endif /* DLL_CAPABLE */
#endif
