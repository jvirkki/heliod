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

#include "netsite.h"
#include "base/util.h"
#include "base/pool.h"
#include "frame/http.h"
#include "frame/httpfilter.h"
#include "safs/trace.h"


// Default maximum number of bytes allowed in TRACE method content body
#define TRACE_MAX_CONTENT_LEN 8192
#define TRACE_BUFSIZE 4096
#define TRACE_REQUEST_TIMEOUT 30


/* ---------------------------- service-trace ----------------------------- */

int service_trace(pblock *pb, Session *sn, Request *rq)
{
    PRIntervalTime epoch = PR_IntervalNow();
    PRIntervalTime timeout = PR_SecondsToInterval(TRACE_REQUEST_TIMEOUT);

    // Don't service requests with large request entity bodies
    const char *content_length = pblock_findkeyval(pb_key_content_length, rq->headers);
    if (content_length) {
        int cl = atoi(content_length);
        if (cl < 0 || cl > TRACE_MAX_CONTENT_LEN) {
            protocol_status(sn, rq, PROTOCOL_ENTITY_TOO_LARGE, NULL);
            return REQ_ABORTED;
        }
    }

    // Get the original request line
    char *clf_request = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
    if (!clf_request)
        return REQ_NOACTION;

    // Allocate a buffer for formatting the original request header and
    // buffering request body data
    char *buffer = (char *)pool_malloc(sn->pool, TRACE_BUFSIZE);
    if (!buffer)
        return REQ_ABORTED;

    // Format the original request header
    int pos = util_snprintf(buffer, TRACE_BUFSIZE, "%s\r\n", clf_request);
    if (pos >= (TRACE_BUFSIZE - 2)) {
        pool_free(sn->pool, buffer);
        protocol_status(sn, rq, PROTOCOL_BAD_REQUEST, NULL);
        return REQ_ABORTED;
    }
    buffer = http_dump822(rq->headers, buffer, &pos, TRACE_BUFSIZE);
    buffer[pos++] = '\r';
    buffer[pos++] = '\n';

    // Start the response
    httpfilter_buffer_output(sn, rq, PR_TRUE);
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "message/http", rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);

    // Send the original request header
    net_write(sn->csd, buffer, pos);

    // Send request body data from the input buffer
    int amount = sn->inbuf->cursize - sn->inbuf->pos;
    if (amount > 0) {
        net_write(sn->csd, sn->inbuf->inbuf + sn->inbuf->pos, amount);
        sn->inbuf->pos = sn->inbuf->cursize;
    }

    // Relay the remainder of the request body data
    do {
        int rv;

        rv = net_read(sn->csd, buffer, TRACE_BUFSIZE, TRACE_REQUEST_TIMEOUT);
        if (rv < 1)
            break;

        rv = net_write(sn->csd, buffer, rv);
        if (rv == IO_ERROR)
            break;
    } while ((PRIntervalTime)(PR_IntervalNow() - epoch) < timeout);

    pool_free(sn->pool, buffer);

    return REQ_PROCEED;
}
