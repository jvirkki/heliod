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

#ifndef FRAME_ACCEL_H
#define FRAME_ACCEL_H

/*
 * accel.h: Accelerator cache
 * 
 * Chris Elving
 */

#define INTNSAPI
#include "netsite.h"
#include "public/iwsstats.h"

#ifndef XP_WIN32
#define HAS_ASYNC_ACCELERATOR
#endif

PR_BEGIN_EXTERN_C

/*
 * AcceleratorHandle is a thread-specific handle to the accelerator cache.
 */
typedef struct AcceleratorHandle AcceleratorHandle;

/*
 * AcceleratorAsync holds a reference to the current contents of the
 * accelerator cache and records caller-specific state for async accelerator
 * cache operations.
 */
typedef struct AcceleratorAsync AcceleratorAsync;

/*
 * AcceleratorAsyncStatus reports the status of an async operation involving
 * the accelerator cache.
 */
typedef enum AcceleratorAsyncStatus {
    ACCEL_ASYNC_AGAIN = -1, /* operation is in progress */
    ACCEL_ASYNC_FALSE = 0,  /* operation cannot be performed */
    ACCEL_ASYNC_DONE = 1    /* operation has completed */
} AcceleratorAsyncStatus;

/*
 * AcceleratorConnectionAsyncState records async operation state for a
 * connection.  Its contents should be manipulated only by the accelerator
 * cache.
 */
typedef struct AcceleratorConnectionAsyncState {
    struct AcceleratorGeneration *gen;
    const struct AcceleratorData *data;
    struct {
        char *p;
        int len;
    } headers;
    PRInt64 offset;
} AcceleratorConnectionAsyncState;

/*
 * accel_init_late initializes the accelerator cache subsystem.  This function
 * must be called after the fork() and prior to any accel_handle_create()
 * calls.
 */
void accel_init_late(void);

/*
 * accel_init_connection initializes the AcceleratorAsyncStatus member of a
 * connection object.  It should be called once for each connection object by
 * the connection constructor.
 */
void accel_init_connection(struct Connection *connection);

/*
 * accel_handle_create allocates a thread-specific handle to the accelerator
 * cache.
 */
NSAPI_PUBLIC AcceleratorHandle *accel_handle_create(void);

/*
 * accel_process_include attempts to process an include using the accelerator
 * cache.  Returns PR_TRUE if the request was processed or PR_FALSE if it was
 * not.
 *
 * If a response has not yet been cached, the response to an include request
 * can be cached using accel_enable() and accel_store().
 */
NSAPI_PUBLIC PRBool accel_process_include(AcceleratorHandle *handle, pool_handle_t *pool, PRFileDesc *csd, const VirtualServer *vs, const char *uri);

/*
 * accel_is_eligible indicates whether an HTTP request is potentially
 * acceleratable.  Returns PR_TRUE if the request is potentially acceleratable
 * or PR_FALSE if it is not.
 *
 * A request that is potentially acceleratable can be processed using a)
 * accel_process_http() or b) accel_async_lookup() and accel_async_service().
 * If a response has not yet been cached, the response to an eligible request
 * can be cached using accel_enable() and accel_store().
 */
PRBool accel_is_eligible(struct Connection *connection);

/*
 * accel_process_http attempts to process an HTTP request using the accelerator
 * cache.  Returns PR_TRUE if the request was processed or PR_FALSE if it was
 * not.
 *
 * If the request was processed, *status_num is set to the HTTP status code,
 * *transmitted reports the number of bytes transmitted, and an appropriate
 * entry is written to the access log.
 */
PRBool accel_process_http(AcceleratorHandle *handle, pool_handle_t *pool, struct Connection *connection, const VirtualServer *vs, int *status_num, PRInt64 *transmitted);

/*
 * accel_async_begin prepares the caller's accelerator cache handle for a
 * sequence of operation(s) by acquiring a reference to the current contents of
 * the accelerator cache and initializing caller-specific state.  Each
 * accel_async_begin() must be followed by a corresponding accel_async_end().
 */
AcceleratorAsync *accel_async_begin(AcceleratorHandle *handle, pool_handle_t *pool);

/*
 * accel_async_end finalizes the set of accelerator cache operations initiated
 * since accel_async_begin() and releases the caller's reference to the
 * contents of the accelerator cache.
 */
void accel_async_end(AcceleratorAsync *async);

/*
 * accel_async_lookup checks the accelerator cache to see if it contains a
 * cached HTTP response for the specified connection's current HTTP request.
 * Returns PR_TRUE if a cached HTTP response is found or PR_FALSE if it is not.
 *
 * If accel_async_lookup() indicates that a cached HTTP response was found, the
 * caller must send the response with accel_async_service() or abort the
 * response with accel_async_abort() before calling accel_async_end().
 */
PRBool accel_async_lookup(AcceleratorAsync *async, struct Connection *connection, const VirtualServer *vs);

/*
 * accel_async_service attempts to send a cached HTTP response that was
 * previously found by accel_async_lookup.  Returns AcceleratorAsyncStatus
 * indicating whether the response was sent.
 *
 * If ACCEL_ASYNC_AGAIN is returned, a subsequent accel_async_service() or
 * accel_async_abort() call must be made for the connection after it polls
 * ready for writing.
 *
 * If ACCEL_ASYNC_DONE is returned, *status_num is set to the HTTP status code,
 * *transmitted reports the number of bytes transmitted, and an appropriate
 * entry will be written to the access log by accel_async_end().
 */
AcceleratorAsyncStatus accel_async_service(AcceleratorAsync *async, struct Connection *connection, int *status_num, PRInt64 *transmitted);

/*
 * accel_async_abort aborts processing of a cached HTTP response that was
 * previously found by accel_async_lookup(), releasing the connection's
 * reference to the accelerator cache.  An appropriate entry will be written
 * to the access log by accel_async_end().
 */
void accel_async_abort(AcceleratorAsync *async, struct Connection *connection);

/*
 * accel_enable is called before request processing begins to indicate that the
 * response should be cached by a subsequent call to accel_store().
 *
 * Only responses to a) HTTP requests identified by accel_is_eligible() as
 * potentially acceleratable or b) internal requests should be cached.
 */
NSAPI_PUBLIC void accel_enable(Session *sn, Request *rq);

/*
 * accel_store is called after request processing has completed to add the
 * response to the accelerator cache.  Returns PR_TRUE if the response is
 * cacheable or PR_FALSE if it is not.
 *
 * A response can only be cached if accel_enable() was called before
 * processing of the request began.
 */
NSAPI_PUBLIC PRBool accel_store(Session *sn, Request *rq);

/*
 * accel_get_http_entries returns the number of cached HTTP responses
 * currently stored in the accelerator cache.
 */
int accel_get_http_entries(void);

/*
 * accel_get_stats retrieves accelerator cache statistics.
 */
void accel_get_stats(StatsCacheBucket *bucket);

/*
 * use_accel_async return TRUE if accel cache has not
 * been turned off
 */
PRBool use_accel_async();

PR_END_EXTERN_C

#endif /* !FRAME_ACCEL_H */
