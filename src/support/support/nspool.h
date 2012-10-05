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
 * pool.h
 *
 * Module for handling memory allocations.
 *
 * Notes:
 * This module is used instead of the NSPR prarena module because the prarenas
 * did not fit as cleanly into the existing server.
 *
 * Mike Belshe
 * 10-02-95
 *
 * Did some cleanup to avoid symbol name clashes with NSAPI
 * Julien Pierre
 * 08-01-2000 
 *
 */

#ifndef NSPOOL_H
#define NSPOOL_H

#ifdef WIN32
#ifdef BUILD_SUPPORT_DLL
#define POOL_EXPORT __declspec(dllexport)
#else
#define POOL_EXPORT __declspec(dllimport)
#endif
#else
#define POOL_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *nspool_handle_t;

//int pool_internal_init(void);
POOL_EXPORT int nspool_init(int, int);

POOL_EXPORT nspool_handle_t *nspool_create(void);
POOL_EXPORT void nspool_destroy(nspool_handle_t *nspool_handle);
POOL_EXPORT int nspool_enabled(void);
POOL_EXPORT void *nspool_malloc(nspool_handle_t *nspool_handle, size_t size );
POOL_EXPORT void nspool_free(nspool_handle_t *nspool_handle, void *ptr );
POOL_EXPORT void *nspool_calloc(nspool_handle_t *nspool_handle, size_t nelem, size_t elsize);
POOL_EXPORT void *nspool_realloc(nspool_handle_t *nspool_handle, void *ptr, size_t size );
POOL_EXPORT char *nspool_strdup(nspool_handle_t *nspool_handle, const char *orig_str );

#ifdef __cplusplus
}
#endif

#endif /* !NSPOOL_H_ */
