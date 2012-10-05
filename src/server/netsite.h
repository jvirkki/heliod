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

#ifndef NETSITE_H
#define NETSITE_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * Standard defs for NetSite servers.
 */

#include "nspr.h"

#ifndef BASE_SYSTEMS_H
#include "base/systems.h"
#endif /* !BASE_SYSTEMS_H */

#include "definesEnterprise.h"

#define MAGNUS_VERSION_STRING INTsystem_version()
#define MAGNUS_VERSION PRODUCT_VERSION_ID

/* Include the public nsapi.h definitions */
#ifndef PUBLIC_NETSITE_H
#include "public/nsapi.h"
#endif /* PUBLIC_NETSITE_H */

NSPR_BEGIN_EXTERN_C

/*
 * Only the mainline needs to set the malloc key.
 */

NSAPI_PUBLIC void InitThreadMallocKey(void);

NSAPI_PUBLIC void system_set_temp_dir(const char *dir);

NSAPI_PUBLIC const char *system_get_temp_dir(void);

/* This probably belongs somewhere else, perhaps with a different name */
NSAPI_PUBLIC char *INTdns_guess_domain(char * hname);

/* --- Begin public functions --- */

#ifdef INTNSAPI

NSAPI_PUBLIC char *INTsystem_version(void);

/*
   Depending on the system, memory allocated via these macros may come from 
   an arena. If these functions are called from within an Init function, they 
   will be allocated from permanent storage. Otherwise, they will be freed 
   when the current request is finished.
 */

#define MALLOC(size) INTsystem_malloc(size)
NSAPI_PUBLIC void *INTsystem_malloc(int size);

#define CALLOC(size) INTsystem_calloc(size)
NSAPI_PUBLIC void *INTsystem_calloc(int size);

#define REALLOC(ptr, size) INTsystem_realloc(ptr, size)
NSAPI_PUBLIC void *INTsystem_realloc(void *ptr, int size);

#define FREE(ptr) INTsystem_free(ptr)
NSAPI_PUBLIC void INTsystem_free(void *ptr);

#define STRDUP(ptr) INTsystem_strdup(ptr)
NSAPI_PUBLIC char *INTsystem_strdup(const char *ptr);

/*
   These macros always provide permanent storage, for use in global variables
   and such. They are checked at runtime to prevent them from returning NULL.
 */

#define PERM_MALLOC(size) INTsystem_malloc_perm(size)
NSAPI_PUBLIC void *INTsystem_malloc_perm(int size);

#define PERM_CALLOC(size) INTsystem_calloc_perm(size)
NSAPI_PUBLIC void *INTsystem_calloc_perm(int size);

#define PERM_REALLOC(ptr, size) INTsystem_realloc_perm(ptr, size)
NSAPI_PUBLIC void *INTsystem_realloc_perm(void *ptr, int size);

#define PERM_FREE(ptr) INTsystem_free_perm(ptr)
NSAPI_PUBLIC void INTsystem_free_perm(void *ptr);

#define PERM_STRDUP(ptr) INTsystem_strdup_perm(ptr)
NSAPI_PUBLIC char *INTsystem_strdup_perm(const char *ptr);

/* Thread-Private data key index for accessing the thread-private memory pool.
 * Each thread creates its own pool for allocating data.  The MALLOC/FREE/etc
 * macros have been defined to check the thread private data area with the
 * thread_malloc_key index to find the address for the pool currently in use.
 *
 * If a thread wants to use a different pool, it must change the thread-local-
 * storage[thread_malloc_key].
 */

NSAPI_PUBLIC int INTgetThreadMallocKey(void);

NSAPI_PUBLIC pool_handle_t *INTsystem_pool(void);

/* Not sure where to put this. */
NSAPI_PUBLIC void INTmagnus_atrestart(void (*fn)(void *), void *data);

NSAPI_PUBLIC void INTsystem_setnewhandler(void);

#endif /* INTNSAPI */

/* --- End public functions --- */

NSPR_END_EXTERN_C

#ifdef INTNSAPI

#define dns_guess_domain INTdns_guess_domain
#define system_version INTsystem_version
#define system_malloc INTsystem_malloc
#define system_calloc INTsystem_calloc
#define system_realloc INTsystem_realloc
#define system_free INTsystem_free
#define system_strdup INTsystem_strdup
#define system_malloc_perm INTsystem_malloc_perm
#define system_calloc_perm INTsystem_calloc_perm
#define system_realloc_perm INTsystem_realloc_perm
#define system_free_perm INTsystem_free_perm
#define system_strdup_perm INTsystem_strdup_perm
#define getThreadMallocKey INTgetThreadMallocKey
#define system_pool INTsystem_pool
#define magnus_atrestart INTmagnus_atrestart
#define system_setnewhandler INTsystem_setnewhandler

#endif /* INTNSAPI */

#ifndef HTTPS_ADMSERV 
#define HTTPS_ADMSERV "https-admserv"
#endif

#endif /* NETSITE_H */
