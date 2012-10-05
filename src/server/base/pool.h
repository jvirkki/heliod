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

#ifndef BASE_POOL_H
#define BASE_POOL_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

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
 */

#ifndef NETSITE_H
#include "netsite.h"
#endif /* !NETSITE_H */

#ifndef BASE_PBLOCK_H
#include "pblock.h"
#endif /* !BASE_PBLOCK_H */

#ifndef BASE_SESSION_H
#include "session.h"
#endif /* !BASE_SESSION_H */

#ifndef FRAME_REQ_H
#include "frame/req.h"
#endif /* !FRAME_REQ_H */

/* --- Begin function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC int INTpool_init(pblock *pb, Session *sn, Request *rq);

#ifdef DEBUG_CACHES
NSAPI_PUBLIC int INTpool_service_debug(pblock *pb, Session *sn, Request *rq);
#endif

NSAPI_PUBLIC pool_handle_t *INTpool_create(void);

NSAPI_PUBLIC void *INTpool_mark(pool_handle_t *pool_handle);

NSAPI_PUBLIC void INTpool_recycle(pool_handle_t *pool_handle, void *mark);

NSAPI_PUBLIC void INTpool_destroy(pool_handle_t *pool_handle);

NSAPI_PUBLIC int INTpool_enabled(void);

NSAPI_PUBLIC void *INTpool_malloc(pool_handle_t *pool_handle, size_t size );

NSAPI_PUBLIC void INTpool_free(pool_handle_t *pool_handle, void *ptr );

NSAPI_PUBLIC 
void *INTpool_calloc(pool_handle_t *pool_handle, size_t nelem, size_t elsize);

NSAPI_PUBLIC 
void *INTpool_realloc(pool_handle_t *pool_handle, void *ptr, size_t size );

NSAPI_PUBLIC
char *INTpool_strdup(pool_handle_t *pool_handle, const char *orig_str );

#ifdef DEBUG
NSAPI_PUBLIC void INTpool_assert(pool_handle_t *pool_handle, const void *ptr);
#endif

NSPR_END_EXTERN_C

#define pool_init INTpool_init

#ifdef DEBUG_CACHES
#define pool_service_debug INTpool_service_debug
#endif /* DEBUG_CACHES */

#ifdef DEBUG
#define POOL_ASSERT(pool, ptr) INTpool_assert(pool, ptr);
#else
#define POOL_ASSERT(pool, ptr)
#endif

#define pool_create INTpool_create
#define pool_mark INTpool_mark
#define pool_recycle INTpool_recycle
#define pool_destroy INTpool_destroy
#define pool_enabled INTpool_enabled
#define pool_malloc INTpool_malloc
#define pool_free INTpool_free
#define pool_calloc INTpool_calloc
#define pool_realloc INTpool_realloc
#define pool_strdup INTpool_strdup

#endif /* INTNSAPI */

#endif /* !BASE_POOL_H_ */
