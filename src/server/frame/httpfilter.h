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

#ifndef FRAME_HTTPFILTER_H
#define FRAME_HTTPFILTER_H

/*
 * httpfilter.h: HTTP server (incoming requests/outgoing responses) filter
 * 
 * Chris Elving
 */

#define INTNSAPI
#include "netsite.h"
#include "frame/filter.h"

NSPR_BEGIN_EXTERN_C

#ifdef __cplusplus
class HttpFilterContext;
class DaemonSession;
#else
typedef struct HttpFilterContext HttpFilterContext;
typedef struct DaemonSession DaemonSession;
#endif

/*
 * httpfilter_init initializes the HTTP filter subsystem
 */
PRStatus httpfilter_init(void);

/*
 * httpfilter_get_filter returns the HTTP filter
 */
const Filter *httpfilter_get_filter(void);

/*
 * httpfilter_create_context creates a new HTTP filter context structure.  Each
 * concurrent instance of an HTTP filter requires its own context structure.
 * The HTTP filter context should be passed to filter_insert as the data
 * parameter.
 */
HttpFilterContext *httpfilter_create_context(DaemonSession *dsn);

/*
 * httpfilter_set_default_output_buffer_size sets the size of the output buffer
 * used when one isn't explicitly configured at request time.
 */
void httpfilter_set_default_output_buffer_size(int size);

/*
 * httpfilter_get_default_output_buffer_size returns the size of the output
 * buffer used when one isn't explicitly configured at request time.
 */
NSAPI_PUBLIC int httpfilter_get_default_output_buffer_size(void);

/*
 * httpfilter_set_output_buffer_size configures output buffering.  Specify a
 * size > 0 to enable buffering with a specific buffer size or size = 0 to
 * disable buffering.
 */
NSAPI_PUBLIC void httpfilter_set_output_buffer_size(Session *sn, Request *rq, int size);

/*
 * httpfilter_get_output_buffer_size sets the size of the output buffer.
 */
NSAPI_PUBLIC int httpfilter_get_output_buffer_size(Session *sn, Request *rq);

/*
 * httpfilter_set_output_buffer_timeout sets the timeout in milliseconds (the
 * "flushTimer") after which response body data will no longer be buffered.
 */
NSAPI_PUBLIC void httpfilter_set_output_buffer_timeout(Session *sn, Request *rq, int ms);

/*
 * httpfilter_buffer_output enables/disables output buffering.  If buffering
 * is enabled, the default buffer size is used.
 */
NSAPI_PUBLIC void httpfilter_buffer_output(Session *sn, Request *rq, PRBool enabled);

/*
 * httpfilter_set_request_body_limit sets the maximum number of bytes the
 * server will accept in a request entity body.  Returns REQ_PROCEED if the
 * caller should proceed to read the request entity body, REQ_NOACTION if there
 * is no request entity body data associated with the request, or REQ_ABORTED
 * if the entity body's Content-lenfth is larger than the specified limit.
 */
NSAPI_PUBLIC int httpfilter_set_request_body_limit(Session *sn, Request *rq, int size);

/*
 * httpfilter_suppress_flush advises the HTTP filter whether it should send
 * response data on to the lower layers of the filter stack (i.e. out on the
 * wire) when net_flush is called.
 */
NSAPI_PUBLIC void httpfilter_suppress_flush(Session *sn, Request *rq, PRBool suppressed);

/*
 * httpfilter_suppress_100_continue instructs the HTTP filter not to
 * automatically generate a 100 Continue response.
 */
void httpfilter_suppress_100_continue(Session *sn, Request *rq);

/*
 * httpfilter_start_response begins the HTTP response.  Returns REQ_PROCEED if
 * the caller should send a response entity body.
 */
int httpfilter_start_response(Session *sn, Request *rq);

/*
 * httpfilter_finish_request marks the end of the HTTP response.  Note that the
 * response may not be completely flushed until filter_remove is called.
 * Returns REQ_PROCEED if the response has been finished, or the error returned
 * by an Output directive that failed to initialize.
 */
int httpfilter_finish_request(Session *sn, Request *rq);

/*
 * httpfilter_reset_response clears any error that occurred while calling
 * Output directives (i.e. constructing the filter stack), allowing an error
 * response to be sent.  Returns REQ_NOACTION if the response was already
 * committed (i.e. the caller should not send an error response), REQ_PROCEED
 * if there was no Output error, or the error returned by an Output directive.
 */
int httpfilter_reset_response(Session *sn, Request *rq);

/*
 * httpfilter_request_body_buffered returns PR_TRUE if the HTTP filter has
 * request message body data in its buffer.
 *
 * XXX This is a temporary kludge that lets Proxy know whether the client
 * has started sending the request body.  If the client has started sending
 * the request body, Proxy can safely block waiting for the request body to
 * arrive in its entirety.  If the client hasn't started sending the
 * request body, Proxy will need to PR_Poll() both the client and origin so
 * it can wait for the earlier of a) request body from the client or b) an
 * HTTP/1.1 100 Continue response from the origin.
 *
 * XXX Ultimately this should be replaced by a more general NSAPI filter
 * method (e.g. available, dataready, or poll) that's hooked into the NSPR
 * poll IO method.
 */
PRBool httpfilter_request_body_buffered(Session *sn, Request *rq);

NSPR_END_EXTERN_C

#endif /* !FRAME_HTTPFILTER_H */
