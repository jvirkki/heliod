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
 * httpfilter.cpp: HTTP server (incoming requests/outgoing responses) filter
 * 
 * Chris Elving
 */

#include <limits.h>

#include "netsite.h"
#include "base/util.h"
#include "base/pool.h"
#include "frame/conf.h"
#include "frame/req.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/httpact.h"
#include "frame/dbtframe.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/throttling.h"
#include "httpdaemon/vsconf.h"
#include "httpparser/httpparser.h"
#include "NsprWrap/NsprError.h"
#include "time/nstime.h"
#include "outputvector.h"
#include "frame/httpfilter.h"
#include "filter_pvt.h"


#define HTTP11_100_CONTINUE            ("HTTP/1.1 100 Continue\r\n\r\n")
#define HTTP11_100_CONTINUE_SIZE       (sizeof(HTTP11_100_CONTINUE) - 1)
#define FINAL_CHUNK                    ("0\r\n\r\n")
#define FINAL_CHUNK_SIZE               (sizeof(FINAL_CHUNK) - 1)
#define TOKEN_CHUNKED                  ("chunked")
#define TOKEN_CHUNKED_SIZE             (sizeof(TOKEN_CHUNKED) - 1)
#define TOKEN_CLOSE                    ("close")
#define TOKEN_CLOSE_SIZE               (sizeof(TOKEN_CLOSE) - 1)
#define TOKEN_KEEP_ALIVE               ("keep-alive")
#define TOKEN_KEEP_ALIVE_SIZE          (sizeof(TOKEN_KEEP_ALIVE) - 1)
#define TOKEN_100_CONTINUE             ("100-continue")
#define TOKEN_100_CONTINUE_SIZE        (sizeof(TOKEN_100_CONTINUE) - 1)
#define CONTENT_LENGTH                 ("Content-length: ")
#define CONTENT_LENGTH_SIZE            (sizeof(CONTENT_LENGTH) - 1)
#define MAX_CONTENT_LENGTH_VALUE       ("18446744073709551615")
#define MAX_CONTENT_LENGTH_VALUE_SIZE  (sizeof(MAX_CONTENT_LENGTH_VALUE) - 1)
#define CONNECTION_KEEP_ALIVE          ("Connection: keep-alive\r\n")
#define CONNECTION_KEEP_ALIVE_SIZE     (sizeof(CONNECTION_KEEP_ALIVE) - 1)
#define CONNECTION_CLOSE               ("Connection: close\r\n")
#define CONNECTION_CLOSE_SIZE          (sizeof(CONNECTION_CLOSE) - 1)
#define TRANSFER_ENCODING_CHUNKED      ("Transfer-encoding: chunked\r\n")
#define TRANSFER_ENCODING_CHUNKED_SIZE (sizeof(TRANSFER_ENCODING_CHUNKED) - 1)

#define MAX_SECONDS_IN_PR_INTERVAL 21600

static const Filter *_httpfilter_filter;
static int _ticksPerSecond = PR_SecondsToInterval(1);
static char _garbage[65536];
static int _defaultOutputStreamSize = 0;
static PRInt64 _MaxRequestBodySize;
static PRIntervalTime _timeoutDrain;
static int _secondsFlush;
static PRIntervalTime _timeoutWrite;
static PRIntervalTime _timeoutSendfile;

static const HttpString HTTPSTRING_CLOSE(TOKEN_CLOSE);
static const HttpString HTTPSTRING_KEEP_ALIVE(TOKEN_KEEP_ALIVE);
static const HttpString HTTPSTRING_100_CONTINUE(TOKEN_100_CONTINUE);
static const PRInt64 MAX_INT64 = LL_INIT(2147483647 /* INT_MAX */, 4294967295U /* UINT_MAX */);

static FilterInsertFunc httpfilter_method_insert;
static FilterRemoveFunc httpfilter_method_remove;
static FilterFlushFunc httpfilter_method_flush;
static FilterReadFunc httpfilter_method_read;
static FilterWriteFunc httpfilter_method_write;
static FilterWritevFunc httpfilter_method_writev;
static FilterSendfileFunc httpfilter_method_sendfile;


/* -------------------------------- atoi64 -------------------------------- */

static inline PRInt64 atoi64(const char *a)
{
    PRInt64 i = 0;

    while (*a >= '0' && *a <= '9') {
        i *= 10;
        i += (*a - '0');
        a++;
    }

    return i;
}


/* ---------------------- get_content_length_digits ----------------------- */

static inline int get_content_length_digits(PRUint64 contentlength)
{
    int digits = MAX_CONTENT_LENGTH_VALUE_SIZE;

    // Find the number of digits required to represent the content length in
    // decimal.  Returns the number of digits required to represent the largest
    // possible 64 bit integer for large content lengths.
    if (contentlength < 10) {
        digits = 1; // 9
    } else if (contentlength < 100) {
        digits = 2; // 99
    } else if (contentlength < 1000) {
        digits = 3; // 999
    } else if (contentlength < 10000) {
        digits = 4; // 9999
    } else if (contentlength < 100000) {
        digits = 5; // 99999
    }

    return digits;
}


/* ----------------------- get_chunk_length_digits ------------------------ */

static inline int get_chunk_length_digits(unsigned chunklength)
{
    int digits = -1;

    // Find the number of digits required to represent the chunk length in hex.
    // Returns -1 for large chunk lengths.
    if (chunklength < 0x10) {
        digits = 1; // f
    } else if (chunklength < 0x100) {
        digits = 2; // ff
    } else if (chunklength < 0x1000) {
        digits = 3; // fff
    } else if (chunklength < 0x10000) {
        digits = 4; // ffff
    } else if (chunklength < 0x100000) {
        digits = 5; // fffff
    }

    return digits;
}


/* --------------------------- get_nspr_timeout --------------------------- */

static inline PRIntervalTime get_nspr_timeout(int net_timeout)
{
    PRIntervalTime timeout;

    if (net_timeout == NET_ZERO_TIMEOUT) {
        timeout = PR_INTERVAL_NO_WAIT;
    } else if (net_timeout == NET_INFINITE_TIMEOUT) {
        timeout = PR_INTERVAL_NO_TIMEOUT;
    } else {
        timeout = net_timeout * _ticksPerSecond;
    }

    return timeout;
}


/* ------------------------ HttpFilterRequestState ------------------------ */

enum HttpFilterRequestState {
    STATE_ENTITY,            // Waiting for request entity body
    STATE_UNCHUNK_LENGTH,    // Waiting for chunk length
    STATE_UNCHUNK_LF_CHUNK,  // Waiting for LF before chunk
    STATE_UNCHUNK_CHUNK,     // Waiting for chunk
    STATE_UNCHUNK_LF_LENGTH, // Waiting for LF before chunk length
    STATE_UNCHUNK_LF_DONE,   // Waiting for LF before EOR
    STATE_UNCHUNK_TRAILER,   // Waiting for a trailer header
    STATE_UNCHUNK_DONE,      // End of chunked request message body
    STATE_UNCHUNK_ERROR      // Error unchunking request message body
};


/* -------------------------- HttpFilterContext --------------------------- */

class HttpFilterContext {
public:
    /**
     * Construct an HttpFilterContext for use by the specified DaemonSession.
     * Caller must call insert() before using the HttpFilterContext.
     */
    HttpFilterContext(DaemonSession *dsn);

    /**
     * Prepare the HttpFilterContext for a new request.
     */
    inline void insert(Session *sn, Request *rq);

    /**
     * Mark the end of a response.
     */
    inline void remove(PRFileDesc *lower);

    /**
     * Read request entity body data into buf.  Unchunks data and enforces
     * content-length as appropriate.
     */
    inline int read(PRFileDesc *lower, void *buf, int amount, PRIntervalTime timeout);

    /**
     * Indicate that there is no request message body, regardless of what the
     * request headers indicate.
     */
    inline void ignoreRequestBody();

    /**
     * Return PR_FALSE if request doesn't have a body.
     */
    inline PRBool hasRequestBody();

    /**
     * Limit the number of bytes of request entity body the server will accept.
     * Returns PR_SUCCESS if the limit was set, or PR_FAILURE if the request's
     * Content-length is >size or >size bytes have already been received
     */
    inline PRStatus setRequestBodyLimit(int size);

    /**
     * Return PR_TRUE if there is request message body data sitting in our
     * buffer.  (Note that this does not necessarily mean that there is request
     * entity body data ready to be read; the buffered request message body
     * data might consist entirely of chunked encoding overhead.)
     */
    inline PRBool isRequestMessageBodyBuffered();

    /**
     * Indicate whether net_flush calls should result in data being sent to
     * lower layers.
     */
    inline void suppressFlush(PRBool suppressed);

    /**
     * Indicate that a 100 Continue response should not be automatically
     * generated.
     */
    inline void suppress100Continue();

    /**
     * Send/buffer response entity body data.
     */
    inline int writev(PRFileDesc *lower, const NSAPIIOVec *iov, int iov_size, int amount);

    /**
     * Send/buffer response entity body data.
     */
    inline int sendfile(PRFileDesc *lower, sendfiledata *sfd, PRInt64 flen, PRInt64 amount);

    /**
     * Flush response entity body data.
     */
    inline int flush(PRFileDesc *lower);

    /**
     * Suppress output.  Returns PR_SUCCESS if output (including headers) will
     * be suppressed, or PR_FAILURE if output has already been sent.
     */
    inline PRStatus suppressOutput(void);

    /**
     * Indicate all response entity body data has been buffered or sent.
     */
    inline void finalizeResponseBody();

    /**
     * Prepare the headers, initializing various flags as appropriate.  May
     * leave generation of a Content-length and Connection: keep-alive header
     * until finalizeResponseHeaders() is called.
     */
    void makeResponseHeaders();

    /**
     * Return PR_FALSE if response doesn't have a body.
     */
    inline PRBool hasResponseBody();

    /**
     * Set the output buffer size in bytes.
     */
    inline void setResponseBufferSize(int size);

    /**
     * Retrieve the size of the output buffer.
     */
    inline int getResponseBufferSize();

    /**
     * Set the timeout (in seconds) after which response body data will not be
     * buffered.
     */
    inline void setResponseBufferTimeout(int seconds);

    /**
     * Set whether the response body should be buffered.
     */
    inline void setBufferResponseBody(PRBool flag);

private:
    /**
     * Read request message (not entity) body data into buf.
     */
    inline int readRaw(PRFileDesc *lower, void *buf, int amount, PRIntervalTime timeout);

    /**
     * Read unbuffered request message (not entity) body data directly from the
     * lower layer.  Should only be called from readRaw.
     */
    inline int readRawLower(PRFileDesc *lower, void *buf, int amount, PRIntervalTime timeout);

    /**
     * Read a request message (not entity) body character into buf.
     */
    int getcRaw(PRFileDesc *lower, PRIntervalTime timeout);

    /**
     * Send a 100 Continue response.  Should only be called if the client
     * requested a 100 Continue response and the caller is ready to read the
     * request body.  Returns PR_SUCCESS if the caller should read the request
     * body or PR_FAILURE if there was an error.
     */
    PRStatus send100Continue(PRFileDesc *lower);

    /**
     * Called by sendfile() for files >2GB.
     */
    int sendfile64(PRFileDesc *lower, sendfiledata *sfd, PRInt64 flen, PRInt64 amount);

    /**
     * Send/buffer a limited amount of respone entity body data.  This is
     * called by writev() to enforce Content-length restrictions.
     */
    int writevLimited(PRFileDesc *lower, const NSAPIIOVec *incoming_iov, int incoming_iov_size, PRInt64 limit);

    /**
     * Send/buffer a limited amount of respone entity body data.  This is
     * called by sendfile() to enforce Content-length restrictions.
     */
    int sendfileLimited(PRFileDesc *lower, sendfiledata *sfd, PRInt64 flen, PRInt64 limit);

    /**
     * Send response message (not response entity body) data to the lower
     * layer.
     */
    inline int writevRaw(PRFileDesc *lower, const PRIOVec *iov, int iov_size);

    /**
     * Send response message (not response entity body) data to the lower
     * layer.
     */
    inline int sendfileRaw(PRFileDesc *lower, sendfiledata *sfd);

    /**
     * Update QOS bytes received/transmitted stats.
     */
    inline void updateStats(PRInt32 count);

    /**
     * Finalize the headers for transmission.  May use the headers from
     * makeResponseHeaders() as is, or may insert a generated a Content-length
     * and Connection: keep-alive header.
     */
    inline void finalizeResponseHeaders();

    /**
     * Return the maximum amount of response entity body data that can be
     * transmitted/buffered at this point.
     */
    inline PRInt64 getResponseContentLimit();

    /**
     * Return PR_TRUE if response entity body data is being suppressed (e.g. if
     * this is a HEAD response) or PR_FALSE if it should be buffered/sent.
     */
    inline PRBool suppressResponseBody(PRInt64 amount);

    /**
     * Buffer response entity body data for later transmission.  Returns
     * PR_TRUE if the data was buffered or PR_FALSE if it was not.
     */
    inline PRBool bufferResponseBody(const PRIOVec *iov, int iov_size, int amount);

    /**
     * Attempt to append a sendfile header to the HTTP response headers.
     * Returns PR_TRUE if the passed sendfile header was buffered or PR_FALSE
     * if it was not.
     */
    inline PRBool bufferSendfileHeader(const void *header, int hlen);

    /**
     * Add the preamble which may include headers, a chunk header, and
     * previously buffered response message body data.  chunkLength specifies
     * the length of a response entity body fragment the caller will add to the
     * output vector before calling addResponseEpilogue().  The caller must
     * ensure there are at least 3 unused entries in the output vector's iov[]
     * array before calling addResponsePreamble().
     */
    inline void addResponsePreamble(OutputVector &output, int chunkLength = 0);

    /**
     * Add the epilogue which may include headers, a chunk trailer, and the
     * final (0-length) chunk.  chunkLength specifies the length of a response
     * entity body fragment the caller added to the output vector after calling
     * addResponsePreamble().  The caller must ensure there are at least 2
     * unused entries in the output vector's iov before calling
     * addResponseEpilogue().
     */
    inline void addResponseEpilogue(OutputVector &output, int chunkLength = 0);

    /**
     * Add a new chunk to _response.buffer.  Should only be called if
     * flagChunkBody is set.
     */
    inline PRBool addResponseChunk(const PRIOVec *iov, int iov_size, int amount);

    /**
     * Allocate _response.buffer.  Returns PR_FALSE if the buffer was not
     * allocated.
     */
    inline PRBool allocResponseBuffer();

    /**
     * Data relating to the HTTP request.
     */
    struct HttpFilterContextRequest {
        /**
         * Set if the client is expecting a 100 Continue response.
         */
        PRBool flagExpect100Continue;

        /**
         * Set if there was an error receiving the request message body.  If
         * this flag is set, errorReceiving contains the error state.
         */
        PRBool flagErrorReceiving;

        /**
         * Error state at time of request message body read failure.  Valid
         * only if flagErrorReceiving is set.
         */
        NsprError errorReceiving;

        /**
         * Value of the Content-length header, -1 if a transfer-encoding is
         * used, or 0 if there's no request message body.
         */
        PRInt64 contentLength;

        /**
         * Number of bytes of request entity body to receive.  -1 indicates any
         * amount of data may be received.
         */
        PRInt64 contentLimit;

        /**
         * Number of bytes of request entity body received.  Note that this is
         * after we remove transfer-encoding and should be <= contentLimit if
         * contentLimit != -1.
         */
        PRInt64 contentReceived;

        /**
         * The request message body state (e.g. unchunking status).
         */
        HttpFilterRequestState state;

        /**
         * Length of the current chunk.  Only set when unchunking.
         */
        PRInt32 chunkLength;

        /**
         * Timeout for individual PR_Recv() calls.
         */
        PRIntervalTime ioTimeout;

        /**
         * Timeout for the remainder of the request entity body.
         */
        PRUint64 bodyTimeout;

        /**
         * A copy of the original netbuf from the Session.
         */
        netbuf inbuf;
    } _request;

    /**
     * Data relating to the HTTP response.
     */
    struct HttpFilterContextResponse {
        /**
         * Set if we should buffer response body data.
         */
        PRPackedBool flagBufferBody;

        /**
         * Set if we should suppress output (e.g. if this is a HEAD response).
         */
        PRPackedBool flagSuppressBody;

        /**
         * Set if makeResponseHeaders() has been called.
         */
        PRPackedBool flagCalledMakeResponseHeaders;

        /**
         * Set if we should chunk the body.  Note that a higher level filter
         * may be doing the Transfer-encoding, in which case flagChunkBody
         * will be clear even though there is a Transfer-encoding header.
         */
        PRPackedBool flagChunkBody;

        /**
         * Set if we should try to generate a Content-length header after
         * buffering response entity body data.
         */
        PRPackedBool flagDeferContentLengthGeneration;

        /**
         * Set if we should try to generate a Connection header after buffering
         * response body data.
         */
        PRPackedBool flagDeferConnectionGeneration;

        /**
         * Set if some response data has been sent.
         */
        PRPackedBool flagCommitted;

        /**
         * Set if all response data has been buffered or sent.
         */
        PRPackedBool flagFinalizedBody;

        /**
         * Set if there was an error sending the response message body.  If
         * this flag is set, errorSending contains the error state.
         */
        PRBool flagErrorSending;

        /**
         * Error state at time of respoinse message body write failure.  Valid
         * only if flagErrorSending is set.
         */
        NsprError errorSending;

        /**
         * Number of bytes of response entity body to send, or -1 if a
         * transfer-encoding is used.
         */
        PRInt64 contentLimit;

        /**
         * Number of bytes of response entity body we saw (and buffered, sent,
         * or suppressed).  Note that this is before we apply transfer-encoding
         * and should be <= contentLimit if contentLimit != -1.
         */
        PRInt64 contentSeen;

        /**
         * Buffer that holds response message headers and response message body
         * data.
         */
        char *buffer;

        /**
         * Size of buffer.
         */
        int size;

        /**
         * Number of bytes of buffer that have been allocated.
         */
        int used;

        /**
         * Location of response message headers in buffer.
         */
        char *headers;

        /**
         * Length of the response message headers.
         */
        int lenHeaders;

        /**
         * Location of response message body data in buffer.
         */
        char *body;

        /**
         * Length of the response message body data in buffer.
         */
        int lenBody;

        /**
         * Timeout (in seconds) after which response body data will not be
         * buffered.
         */
        int secondsFlush;

        /**
         * Nonzero if we shouldn't send data on to lower layers when net_flush
         * is called.
         */
        int suppressFlushCount;

        /**
         * A buffer for temporarily storing a formatted chunk length.
         */
        char bufferChunkLength[sizeof("123456789abcdef0\r\n")];
    } _response;

    /**
     * The DaemonSession this HttpFilterContext is serviced on.
     */
    DaemonSession *_dsn;

    /**
     * NSAPI session.
     */
    Session *_sn;

    /**
     * Original NSAPI request.
     */
    Request *_rq;

    /**
     * QOS statistics or NULL if QOS is disabled.
     */
    VSTrafficStats *_stats;
};


/* ----------------- HttpFilterContext::HttpFilterContext ----------------- */

HttpFilterContext::HttpFilterContext(DaemonSession *dsn)
: _dsn(dsn)
{
    // N.B. must call insert() before using this HttpFilterContext
}


/* ---------------------- HttpFilterContext::insert ----------------------- */

inline void HttpFilterContext::insert(Session *sn, Request *rq)
{
    pb_param *pp;

    // NSAPI Session and Request
    _sn = sn;
    _rq = rq;

    // QOS statistics
    const VirtualServer *vs = request_get_vs(rq);
    if (vs->getTrafficStats().isEnabled()) {
        _stats = &vs->getTrafficStats();
        _stats->incrementConnections();
    } else {
        _stats = NULL;
    }

    /*
     * Initialize request-related members
     */

    // XXX I think HTTP request parsing should be moved into httpfilter.  For
    // now, at least, it will remain in httpdaemon/httprequest.cpp.

    // Check if client wants a 100 Continue response
    _request.flagExpect100Continue = PR_FALSE;
    if (pp = pblock_findkey(pb_key_expect, rq->headers)) {
        HttpTokenizer tokenizer(HttpString(pp->value));
        do {
            if (tokenizer.matches(HTTPSTRING_100_CONTINUE)) {
                // Expect: 100-continue
                _request.flagExpect100Continue = PR_TRUE;
            }
        } while (++tokenizer);
    }

    _request.flagErrorReceiving = PR_FALSE;

    _request.contentReceived = 0;

    // Check for presence of request message body
    if ((pp = pblock_findkey(pb_key_transfer_encoding, rq->headers))
        && (pp->value[0] != 'i' && pp->value[0] != 'I' || strcasecmp(pp->value, "identity")))
    {
        // We got a Transfer-encoding: chunked header
        _request.contentLength = -1;
        _request.contentLimit = _MaxRequestBodySize;
        _request.state = STATE_UNCHUNK_LENGTH;
        CHUNKED(rq) = PR_TRUE;

        // Remove "chunked" and replace with "identity" since we're unchunking.
        // XXX Since we blindly replace any and all transfer-coding tokens, we
        // end up masking future transfer-codings that httpfilter doesn't know
        // about but some other filter/SAF might.  That is sad =(
        pool_free(sn->pool, pp->value);
        pp->value = pool_strdup(sn->pool, "identity");

        // RFC 2616 4.4
        if ((pp = pblock_removekey(pb_key_content_length, rq->headers)) != NULL)
            param_free(pp);

    } else if (const char *cl = pblock_findkeyval(pb_key_content_length, rq->headers)) {
        // We got a Content-length header
        _request.contentLength = atoi64(cl);
        if (_request.contentLength < 0)
            _request.contentLength = 0;
        _request.contentLimit = _request.contentLength;
        if (_request.contentLimit > _MaxRequestBodySize && _MaxRequestBodySize != -1)
            _request.contentLimit = _MaxRequestBodySize;
        _request.state = STATE_ENTITY;

    } else {
        // No request message body
        _request.contentLength = 0;
        _request.contentLimit = 0;
        _request.state = STATE_ENTITY;
    }

    // Don't run Input stage if there's no request message body
    if (_request.contentLimit == 0) {
        ((NSAPISession *)_sn)->input_done = PR_TRUE;
        ((NSAPISession *)_sn)->input_rv = REQ_NOACTION;
    }

    // We'll manage the existing netbuf.  The http/netbuf code will allocate a
    // new buffer if one's needed.
    _request.inbuf = *sn->inbuf;
    sn->inbuf->inbuf = NULL;
    sn->inbuf->pos = sn->inbuf->maxsize;
    sn->inbuf->cursize = sn->inbuf->maxsize;

    _request.ioTimeout = get_nspr_timeout(sn->inbuf->rdtimeout);
    _request.bodyTimeout = _dsn->GetRqBodyTimeout();

    /*
     * Initialize response-related members
     */

    // Note that some values are not fully initialized until we generate the
    // headers

    _response.flagBufferBody = (_defaultOutputStreamSize > 0);
    _response.flagSuppressBody = ISMHEAD(rq);
    _response.flagCalledMakeResponseHeaders = PR_FALSE;
    _response.flagChunkBody = PR_FALSE;
    _response.flagDeferContentLengthGeneration = PR_FALSE;
    _response.flagDeferConnectionGeneration = PR_FALSE;
    _response.flagCommitted = PR_FALSE;
    _response.flagFinalizedBody = PR_FALSE;
    _response.flagErrorSending = PR_FALSE;

    _response.contentLimit = -1;
    _response.contentSeen = 0;

    _response.buffer = NULL;
    _response.size = PR_MAX(REQ_MAX_LINE, _defaultOutputStreamSize);
    _response.used = 0;

    _response.headers = NULL;
    _response.lenHeaders = 0;

    _response.body = NULL;
    _response.lenBody = 0;

    _response.secondsFlush = _secondsFlush;
    _response.suppressFlushCount = 0;
}


/* ---------------------- HttpFilterContext::remove ----------------------- */

inline void HttpFilterContext::remove(PRFileDesc *lower)
{
    // QOS statistics
    if (_stats)
        _stats->decrementConnections();

    /*
     * Cleanup response-related members
     */

    // The response is complete
    finalizeResponseBody();

    // Send pending data
    OutputVector output;
    addResponsePreamble(output);
    addResponseEpilogue(output);
    writevRaw(lower, output.iov, output.iov_size);

    // Free our response buffer
    pool_free(_sn->pool, _response.buffer);

    /*
     * Cleanup request-related members
     */

    // Check whether we need to drain request message body data
    PRBool flagDrainRequired = PR_FALSE;
    if (_sn->csd_open == 1 && KEEP_ALIVE(_rq)) {
        if (_request.state == STATE_ENTITY) {
            // Do we need to drain a body with a predefined Content-length?
            if (_request.contentReceived < _request.contentLength)
                flagDrainRequired = PR_TRUE;

        } else if (_request.state != STATE_UNCHUNK_DONE && _request.state != STATE_UNCHUNK_ERROR) {
            // We need to drain a Transfer-encoding: chunked body
            flagDrainRequired = PR_TRUE;
        }
    }

    // If we need to drain request message body data...
    if (flagDrainRequired) {
        PRIntervalTime epoch = PR_IntervalNow();

        // Drain request message body data
        do {
            int rv = read(lower, _garbage, sizeof(_garbage), _timeoutDrain);
            if (rv < 1)
                break;
        } while ((PRIntervalTime)(PR_IntervalNow() - epoch) < _timeoutDrain);

        // If we failed to drain the body, make sure the connection goes away
        if (_request.contentReceived != _request.contentLength && _request.state != STATE_UNCHUNK_DONE)
            KEEP_ALIVE(_rq) = PR_FALSE;
    }

    // Free any newly allocated netbuf
    if (_sn->inbuf->inbuf && _sn->inbuf->inbuf != _request.inbuf.inbuf)
        pool_free(_sn->pool, _sn->inbuf->inbuf);

    // Give back the original netbuf
    *_sn->inbuf = _request.inbuf;
    _request.inbuf.inbuf = NULL;

    // insert() must be called before reusing this HttpFilterContext
}


/* ----------------------- HttpFilterContext::read ------------------------ */

inline int HttpFilterContext::read(PRFileDesc *lower, void *buf, int amount, PRIntervalTime timeout)
{
    // If the client wanted a 100 Continue response...
    if (_request.flagExpect100Continue) {
        if (send100Continue(lower) != PR_SUCCESS)
            return -1;
    }

    // Handle Transfer-encoding: identity
    if (_request.state == STATE_ENTITY) {
        // Enforce content-length
        if (_request.contentLimit != -1 && _request.contentReceived + amount > _request.contentLimit) {
            amount = _request.contentLimit - _request.contentReceived;
            if (amount < 1)
                return 0;
        }

        // Read raw data
        int rv = readRaw(lower, buf, amount, timeout);
        if (rv > 0)
            _request.contentReceived += rv;

        return rv;
    }

    // Handle Transfer-encoding: chunked
    for (;;) {
        int rv;
        int c;

        switch (_request.state) {
        case STATE_UNCHUNK_LENGTH:
            // Consume chunk length
            _request.chunkLength = 0;
            for (;;) {
                c = getcRaw(lower, timeout);
                if (c == -1) {
                    return -1;
                } else if (c >= '0' && c <= '9') {
                    _request.chunkLength = (_request.chunkLength << 4) + (c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    _request.chunkLength = (_request.chunkLength << 4) + (c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    _request.chunkLength = (_request.chunkLength << 4) + (c - 'A' + 10);
                } else {
                    break;
                }
            }
            if (_request.contentLimit != -1 && _request.contentReceived + _request.chunkLength > _request.contentLimit) {
                _request.state = STATE_UNCHUNK_ERROR;
                break;
            }
            switch (c) {
            case '\n':
                _request.state = STATE_UNCHUNK_CHUNK;
                break;
            case '\r':
            case ' ':
            case '\t':
            case ';':
                _request.state = STATE_UNCHUNK_LF_CHUNK;
                break;
            default:
                _request.state = STATE_UNCHUNK_ERROR;
                break;
            }
            break;

        case STATE_UNCHUNK_LF_CHUNK:
            // Consume any chunk extensions and the LF before the chunk data
            while (_request.state == STATE_UNCHUNK_LF_CHUNK) {
                c = getcRaw(lower, timeout);
                if (c == -1) {
                    return -1;
                } else if (c == '\n') {
                    _request.state = STATE_UNCHUNK_CHUNK;
                }
            }
            break;

        case STATE_UNCHUNK_CHUNK:
            // Consume the chunk data
            if (_request.chunkLength > 0) {
                if (amount > _request.chunkLength)
                    amount = _request.chunkLength;
                rv = readRaw(lower, buf, amount, timeout);
                if (rv > 0) {
                    _request.contentReceived += rv;
                    _request.chunkLength -= rv;
                    if (_request.chunkLength == 0)
                        _request.state = STATE_UNCHUNK_LF_LENGTH;
                }
                return rv;
            } else if (_request.chunkLength == 0) {
                _request.state = STATE_UNCHUNK_LF_DONE;
            } else {
                _request.state = STATE_UNCHUNK_ERROR;
            }
            break;

        case STATE_UNCHUNK_LF_LENGTH:
            // Consume the LF before the chunk length
            while (_request.state == STATE_UNCHUNK_LF_LENGTH) {
                c = getcRaw(lower, timeout);
                if (c == -1) {
                    return -1;
                } else if (c == '\n') {
                    _request.state = STATE_UNCHUNK_LENGTH;
                } else if (c != '\r') {
                    _request.state = STATE_UNCHUNK_ERROR;
                }
            }
            break;

        case STATE_UNCHUNK_LF_DONE:
            // Consume any trailer headers and the final LF
            while (_request.state == STATE_UNCHUNK_LF_DONE) {
                c = getcRaw(lower, timeout);
                if (c == -1) {
                    return -1;
                } else if (c == '\n') {
                    _request.state = STATE_UNCHUNK_DONE;
                } else if (c != '\r') {
                    _request.state = STATE_UNCHUNK_TRAILER;
                }
            }
            break;

        case STATE_UNCHUNK_TRAILER:
            // Consume a trailer header
            while (_request.state == STATE_UNCHUNK_TRAILER) {
                c = getcRaw(lower, timeout);
                if (c == -1) {
                    return -1;
                } else if (c == '\n') {
                    _request.state = STATE_UNCHUNK_LF_DONE;
                }
            }
            break;

        case STATE_UNCHUNK_DONE:
            // End of chunked request message body
            return 0;

        case STATE_UNCHUNK_ERROR:
            // Invalid chunked data
            KEEP_ALIVE(_rq) = PR_FALSE;
            NsprError::setError(PR_END_OF_FILE_ERROR, XP_GetAdminStr(DBT_ErrorReadingChunkedRequestBody));
            return -1;
        }
    }
}


/* ---------------------- HttpFilterContext::readRaw ---------------------- */

inline int HttpFilterContext::readRaw(PRFileDesc *lower, void *buf, int amount, PRIntervalTime timeout)
{
    // If our buffer is empty and caller only requested a little data...
    if (_request.inbuf.pos == _request.inbuf.cursize && amount < _request.inbuf.maxsize / 2) {
        _request.inbuf.pos = 0;
        _request.inbuf.cursize = 0;

        // Read raw data into our buffer
        int rv = readRawLower(lower, _request.inbuf.inbuf, _request.inbuf.maxsize, timeout);
        if (rv < 1)
            return rv;

        _request.inbuf.cursize = rv;
    }

    // Copy from our buffer into caller's
    int available = _request.inbuf.cursize - _request.inbuf.pos;
    if (available > 0) {
        if (amount > available)
            amount = available;

        memcpy(buf, _request.inbuf.inbuf + _request.inbuf.pos, amount);
        _request.inbuf.pos += amount;

        // Track bytes received
        ((NSAPISession *)_sn)->received += amount;
        updateStats(amount);

        return amount;
    }

    // Read raw data directly into caller's buffer
    int rv = readRawLower(lower, buf, amount, timeout);
    if (rv < 1)
        return rv;

    // Track bytes received
    ((NSAPISession *)_sn)->received += rv;
    updateStats(rv);

    return rv;
}


/* ------------------- HttpFilterContext::readRawLower -------------------- */

inline int HttpFilterContext::readRawLower(PRFileDesc *lower, void *buf, int amount, PRIntervalTime timeout)
{
    // If we previously timed out, etc., we'll fail this read
    if (_request.flagErrorReceiving) {
        _request.errorReceiving.restore();
        return -1;
    }

    // If we're enforcing a request body timeout...
    PRIntervalTime epoch;

    if (_request.bodyTimeout != (PRUint64)-1)
        epoch = PR_IntervalNow();

    // No matter what the caller says, we won't block longer than the IO
    // timeout on a single recv() or more than the request body timeout in
    // total
    if (timeout > _request.ioTimeout)
        timeout = _request.ioTimeout;

    PRIntervalTime rqBodyTimeoutInterval = PR_INTERVAL_NO_TIMEOUT;
    // PRIntervalTime overflows in 6 hours
    if ((_request.bodyTimeout != (PRUint64)-1) &&
        (_request.bodyTimeout < MAX_SECONDS_IN_PR_INTERVAL)) {
        rqBodyTimeoutInterval = PR_SecondsToInterval(_request.bodyTimeout);
        if (timeout > rqBodyTimeoutInterval)
            timeout = rqBodyTimeoutInterval;
    }

    // Read raw data directly from the lower layer
    int rv = lower->methods->recv(lower, buf, amount, 0, timeout);

    // Remember failures
    if (rv == -1) {
        // Timeout errors only "stick" if we exceeded the request's IO timeout
        // or the request body timeout (vs. an individual net_read()'s timeout)
        if (PR_GetError() != PR_IO_TIMEOUT_ERROR || timeout >= _request.ioTimeout || timeout >= rqBodyTimeoutInterval) {
            _request.errorReceiving.save();
            _request.flagErrorReceiving = PR_TRUE;
        }
    }

    // If we're enforcing a request body timeout...
    if (_request.bodyTimeout != (PRUint64)-1) {
        PRIntervalTime elapsed = PR_IntervalNow() - epoch;
        PRUint64 secondsElapsed = PR_IntervalToSeconds(elapsed);
        if (_request.bodyTimeout > secondsElapsed) {
            _request.bodyTimeout -= secondsElapsed;
        } else {
            _request.bodyTimeout = 0;
        }
    }

    return rv;
}


/* ---------------------- HttpFilterContext::getcRaw ---------------------- */

int HttpFilterContext::getcRaw(PRFileDesc *lower, PRIntervalTime timeout)
{
    char c;
    if (readRaw(lower, &c, 1, timeout) == 1)
        return c;
    return -1;
}


/* ------------------ HttpFilterContext::send100Continue ------------------ */

PRStatus HttpFilterContext::send100Continue(PRFileDesc *lower)
{
    PR_ASSERT(_request.flagExpect100Continue);

    // If we previously timed out, etc., we'll fail this send
    if (_response.flagErrorSending) {
        _response.errorSending.restore();
        return PR_FAILURE;
    }

    // If we haven't started the response headers...
    if (!_rq->senthdrs) {
        // Send a 100 Continue response
        int rv = lower->methods->send(lower, HTTP11_100_CONTINUE, sizeof(HTTP11_100_CONTINUE) - 1, 0, _timeoutWrite);

        // Remember failures
        if (rv == -1) {
            _response.errorSending.save();
            _response.flagErrorSending = PR_TRUE;
            return PR_FAILURE;
        }

        // Track bytes transmitted
        if (rv > 0) {
            ((NSAPISession *)_sn)->transmitted += rv;
            updateStats(rv);
        }
    }

    _request.flagExpect100Continue = PR_FALSE;

    return PR_SUCCESS;
}


/* ------------------ HttpFilterContext::ignoreRequestBody ---------------- */

inline void HttpFilterContext::ignoreRequestBody()
{
    _request.contentLength = 0;
    _request.contentLimit  = 0;
    _request.state = STATE_ENTITY;
}


/* ------------------ HttpFilterContext::hasRequestBody ------------------- */

inline PRBool HttpFilterContext::hasRequestBody()
{
    return (_request.contentLimit != 0);
}


/* ---------------- HttpFilterContext::setRequestBodyLimit ---------------- */

inline PRStatus HttpFilterContext::setRequestBodyLimit(int size)
{
    if (_request.contentReceived > size)
        return PR_FAILURE;

    if (_request.contentLimit == -1 || _request.contentLimit > size)
        _request.contentLimit = size;

    return PR_SUCCESS;
}


/* ----------- HttpFilterContext::isRequestMessageBodyBuffered ------------ */

inline PRBool HttpFilterContext::isRequestMessageBodyBuffered()
{
    return (_request.contentLength != 0 && _request.inbuf.cursize > _request.inbuf.pos);
}



/* ------------------- HttpFilterContext::suppressFlush ------------------- */

inline void HttpFilterContext::suppressFlush(PRBool suppressed)
{
    if (suppressed) {
        _response.suppressFlushCount++;
    } else {
        _response.suppressFlushCount--;
    }

    PR_ASSERT(_response.suppressFlushCount >= 0 && _response.suppressFlushCount <= 2);
}


/* ---------------- HttpFilterContext::suppress100Continue ---------------- */

inline void HttpFilterContext::suppress100Continue()
{
    _request.flagExpect100Continue = PR_FALSE;
}


/* ---------------------- HttpFilterContext::writev ----------------------- */

inline int HttpFilterContext::writev(PRFileDesc *lower, const NSAPIIOVec *iov, int iov_size, int amount)
{
    int rv;

    // Suppress content on HEAD responses
    if (suppressResponseBody(amount))
        return amount;

    // Make sure caller isn't trying to send more response entity body data
    // than was previously advertised
    PRInt64 limit = getResponseContentLimit();
    if (amount > limit) {
        // This is an atypical error case
        NsprError::setError(PR_NO_DEVICE_SPACE_ERROR, XP_GetAdminStr(DBT_ResponseContentLengthExceeded));
        if (limit < 1)
            return -1;
        return writevLimited(lower, iov, iov_size, limit);
    }

    // Buffer response entity body data as appropriate
    if (_response.flagBufferBody) {
        if (bufferResponseBody((const PRIOVec *)iov, iov_size, amount))
            return amount;
    }

    // Build up an IO vector so we can send headers and body with one writev()
    OutputVector output;
    int sent = 0;

    // Prepend HTTP headers, etc.
    addResponsePreamble(output, amount);

    // Add the new response entity body data
    int iov_consumed = output.addIov((const struct PRIOVec *)iov, iov_size, 2 /* addEpilogue() slack */);
    while (iov_size > iov_consumed) {
        // iov[] is full; send some data
        rv = writevRaw(lower, output.iov, output.iov_size);
        if (rv == -1)
            return -1;
        sent += rv;
        output.reset();

        // Add more of the new response entity body data
        iov_consumed += output.addIov((const struct PRIOVec *)&iov[iov_consumed], iov_size - iov_consumed, 2 /* addEpilogue() slack */);
    }

    // Append chunk trailer
    addResponseEpilogue(output, amount);

    // Send (the rest of?) the data
    rv = writevRaw(lower, output.iov, output.iov_size);
    if (rv == -1)
        return -1;
    sent += rv;

    return sent >= amount ? amount : sent;
}


/* ------------------- HttpFilterContext::writevLimited ------------------- */

int HttpFilterContext::writevLimited(PRFileDesc *lower, const NSAPIIOVec *incoming_iov, int incoming_iov_size, PRInt64 limit)
{
    NSAPIIOVec iov[PR_MAX_IOVECTOR_SIZE];
    int iov_size;
    PRInt64 amount = 0;

    // Create a new iov that contains no more than limit bytes
    for (iov_size = 0; iov_size < incoming_iov_size && amount < limit; iov_size++) {
        iov[iov_size] = incoming_iov[iov_size];
        if (amount + iov[iov_size].iov_len > limit)
            iov[iov_size].iov_len = limit - amount;
        amount += iov[iov_size].iov_len;
    }

    // Recurse back into writev
    return writev(lower, iov, iov_size, amount);
}


/* --------------------- HttpFilterContext::writevRaw --------------------- */

inline int HttpFilterContext::writevRaw(PRFileDesc *lower, const PRIOVec *iov, int iov_size)
{
    int rv;

    // If we previously timed out, etc., we'll fail this writev
    if (_response.flagErrorSending) {
        _response.errorSending.restore();
        return -1;
    }

    // Send data to the next layer
    if (iov_size > 1) {
        rv = lower->methods->writev(lower, iov, iov_size, _timeoutWrite);
    } else if (iov_size > 0) {
        rv = lower->methods->send(lower, iov->iov_base, iov->iov_len, 0, _timeoutWrite);
    } else {
        rv = 0;
    }

    // Remember failures
    if (rv == -1) {
        _response.errorSending.save();
        _response.flagErrorSending = PR_TRUE;
    }

    // Track bytes transmitted
    if (rv > 0) {
        ((NSAPISession *)_sn)->transmitted += rv;
        updateStats(rv);
    }

    return rv;
}


/* --------------------- HttpFilterContext::sendfile ---------------------- */

inline int HttpFilterContext::sendfile(PRFileDesc *lower, sendfiledata *sfd, PRInt64 flen, PRInt64 amount)
{
    int rv;

    // Suppress content on HEAD responses
    if (suppressResponseBody(amount))
        return amount;

    // We need special handling for >2GB files
    if (amount > INT_MAX)
        return sendfile64(lower, sfd, flen, amount);

    // Make sure caller isn't trying to send more response entity body data
    // than was previously advertised
    PRInt64 limit = getResponseContentLimit();
    if (amount > limit) {
        // This is an atypical error case
        NsprError::setError(PR_NO_DEVICE_SPACE_ERROR, XP_GetAdminStr(DBT_ResponseContentLengthExceeded));
        if (limit < 1)
            return -1;
        return sendfileLimited(lower, sfd, flen, limit);
    }

    // Concatenate sendfile headers and HTTP headers if possible
    int lenBufferedSendfileHeader = 0;
    if (sfd->hlen) {
        if (bufferSendfileHeader(sfd->header, sfd->hlen))
            lenBufferedSendfileHeader = sfd->hlen;
    }

    // Build up an IO vector so we can send headers and body with one writev()
    OutputVector output;
    struct sendfiledata output_sfd;
    int sent = 0;

    // Prepend HTTP headers, etc.
    addResponsePreamble(output, amount - lenBufferedSendfileHeader);

    // Prepend sendfile headers
    if (lenBufferedSendfileHeader == 0)
        output.addBuffer(sfd->header, sfd->hlen);

    // Attempt to add headers to the outgoing sendfiledata
    if (output.iov_size == 1) {
        // We can send all the headers on the sendfile() call
        output_sfd.header = output.iov[0].iov_base;
        output_sfd.hlen = output.iov[0].iov_len;
        output.reset();
    } else {
        // If we have headers to send, we'll need to send them separately
        output_sfd.header = NULL;
        output_sfd.hlen = 0;
    }

    // If we can't send all the headers on the sendfile() call...
    if (output.iov_size > 0) {
        // Send the headers
        rv = writevRaw(lower, output.iov, output.iov_size);
        if (rv == -1)
            return -1;
        sent += rv;
        output.reset();

        // We sent all the headers
        output_sfd.header = NULL;
        output_sfd.hlen = 0;        
    }

    /*
     * At this point, headers have been sent or added to the output_sfd
     */

    // Append sendfile trailers
    output.addBuffer(sfd->trailer, sfd->tlen);

    // Append chunk trailer
    addResponseEpilogue(output, amount - lenBufferedSendfileHeader);

    // Attempt to add trailers to the outgoing sendfiledata
    if (output.iov_size == 1) {
        // We can send all the trailers on the sendfile() call
        output_sfd.trailer = output.iov[0].iov_base;
        output_sfd.tlen = output.iov[0].iov_len;
        output.reset();
    } else {
        // If we have tailers to send, we'll need to send them separately
        output_sfd.trailer = NULL;
        output_sfd.tlen = 0;
    }

    // Do the actual sendfile
    output_sfd.fd = sfd->fd;
    output_sfd.offset = sfd->offset;
    output_sfd.len = flen;
    rv = sendfileRaw(lower, &output_sfd);
    if (rv == -1)
        return -1;
    sent += rv;

    /*
     * At this point, headers and the file itself have been sent.  We may or
     * may not have sent the trailers.
     */

    // If we didn't send all the trailers on the sendfile() call...
    if (output.iov_size > 0) {
        // Send the trailers
        rv = writevRaw(lower, output.iov, output.iov_size);
        if (rv == -1)
            return -1;
        sent += rv;
        output.reset();
    }

    return sent >= amount ? amount : sent;
}


/* -------------------- HttpFilterContext::sendfile64 --------------------- */

int HttpFilterContext::sendfile64(PRFileDesc *lower, sendfiledata *sfd, PRInt64 flen, PRInt64 amount)
{
    // XXX Should we implement this?  NSPR's implementation won't deal with
    // >2GB files, but that doesn't mean net_sendfile() on an HTTP connection
    // shouldn't.
    PR_SetError(PR_FILE_TOO_BIG_ERROR, 0);
    return -1;
}


/* ------------------ HttpFilterContext::sendfileLimited ------------------ */

int HttpFilterContext::sendfileLimited(PRFileDesc *lower, sendfiledata *sfd, PRInt64 flen, PRInt64 limit)
{
    sendfiledata output_sfd;

    output_sfd.fd = sfd->fd;
    output_sfd.offset = sfd->offset;
    output_sfd.len = flen;
    output_sfd.header = sfd->header;
    output_sfd.hlen = sfd->hlen;
    output_sfd.trailer = sfd->trailer;
    output_sfd.tlen = sfd->tlen;

    // Constrain headers to the specified limit
    if (output_sfd.hlen > limit)
        output_sfd.hlen = limit;
    limit -= output_sfd.hlen;

    // Constrain file to the specified limit
    if (output_sfd.len > limit)
        output_sfd.len = limit;
    limit -= output_sfd.len;

    // Constrain trailers to the specified limit
    if (output_sfd.tlen > limit)
        output_sfd.tlen = limit;
    limit -= output_sfd.tlen;

    // Recurse back into sendfile
    return sendfile(lower, &output_sfd, output_sfd.len, output_sfd.hlen + output_sfd.len + output_sfd.tlen);
}


/* -------------------- HttpFilterContext::sendfileRaw -------------------- */

inline int HttpFilterContext::sendfileRaw(PRFileDesc *lower, sendfiledata *sfd)
{
    int rv;

    // If we previously timed out, etc., we'll fail this writev
    if (_response.flagErrorSending) {
        _response.errorSending.restore();
        return -1;
    }

    // Send data to the next layer
    rv = lower->methods->sendfile(lower, (PRSendFileData *)sfd, PR_TRANSMITFILE_KEEP_OPEN, _timeoutSendfile);

    // Remember failures
    if (rv == -1) {
        _response.errorSending.save();
        _response.flagErrorSending = PR_TRUE;
    }

    // Track bytes transmitted
    if (rv > 0) {
        ((NSAPISession *)_sn)->transmitted += rv;
        updateStats(rv);
    }

    return rv;
}


/* ----------------------- HttpFilterContext::flush ----------------------- */

inline int HttpFilterContext::flush(PRFileDesc *lower)
{
    int rv;

    if (_response.suppressFlushCount > 0)
        return 0;

    // Send pending data
    OutputVector output;
    addResponsePreamble(output);
    addResponseEpilogue(output);
    rv = writevRaw(lower, output.iov, output.iov_size);
    if (rv != output.amount)
        return -1;

    // Flush lower layers
    /*
    PRDescIdentity id = PR_GetLayersIdentity(lower); //hack for NSS, bug4874422
    const char* name = PR_GetNameForIdentity(id);
    if (name && !strcmp(name,"SSL"))
        return 0;
    return lower->methods->fsync(lower);
    */
    //NSS throws an assertion and return PR_FAILURE on fsync,
    //bug4874422,4875799 and mozilla bug208685
    return 0;
}


/* -------------------- HttpFilterContext::updateStats -------------------- */

inline void HttpFilterContext::updateStats(PRInt32 count)
{
    if (_stats)
        _stats->recordBytes(count);
}


/* ------------------ HttpFilterContext::suppressOutput ------------------- */

inline PRStatus HttpFilterContext::suppressOutput()
{
    if (_response.flagCommitted) {
        // We've already sent some of the response, and we won't be sending
        // any more
        finalizeResponseBody();
        return PR_FAILURE;
    }

    // Generate the headers so we won't try to generate them again later, and
    // indicate that no headers or body should be sent
    _response.flagSuppressBody = PR_TRUE;
    finalizeResponseHeaders();
    _rq->senthdrs = PR_TRUE;

    // Any transfer-encoding/content-length/connection decisions we made are
    // invalid
    if (_response.flagChunkBody)
        param_free(pblock_removekey(pb_key_transfer_encoding, _rq->srvhdrs));
    param_free(pblock_removekey(pb_key_content_length, _rq->srvhdrs));
    param_free(pblock_removekey(pb_key_connection, _rq->srvhdrs));
    _response.flagChunkBody = PR_FALSE;
    _response.flagDeferContentLengthGeneration = PR_FALSE;
    _response.flagDeferConnectionGeneration = PR_FALSE;

    return PR_SUCCESS;
}


/* ---------------- HttpFilterContext::makeResponseHeaders ---------------- */

void HttpFilterContext::makeResponseHeaders()
{
    // Make headers at most once per response
    if (_response.flagCalledMakeResponseHeaders)
        return;

    _response.flagCalledMakeResponseHeaders = PR_TRUE;

    // If we're doing HTTP/0.9...
    if (_rq->protv_num < PROTOCOL_VERSION_HTTP10) {
        // We have to close the connection and we don't send headers
        KEEP_ALIVE(_rq) = PR_FALSE;
        return;
    }

    // Check for an existing Transfer-encoding header
    PRBool flagHasTransferEncoding = (pblock_findkey(pb_key_transfer_encoding, _rq->srvhdrs) != NULL);

    // Check for an existing Connection header
    PRBool flagHasConnection = PR_FALSE;
    PRBool flagHasConnectionClose = PR_FALSE;
    PRBool flagHasConnectionKeepAlive = PR_FALSE;
    if (pb_param *pp = pblock_findkey(pb_key_connection, _rq->srvhdrs)) {
        // Someone above us in the stack added a Connection header
        flagHasConnection = PR_TRUE;
        HttpTokenizer tokenizer(HttpString(pp->value));
        do {
            if (tokenizer.matches(HTTPSTRING_CLOSE)) {
                // Connection: close
                KEEP_ALIVE(_rq) = PR_FALSE;
                flagHasConnectionClose = PR_TRUE;
            } else if (tokenizer.matches(HTTPSTRING_KEEP_ALIVE)) {
                // Connection: keep-alive
                flagHasConnectionKeepAlive = PR_TRUE;
            }
        } while (++tokenizer);
    }
        
    // Don't generate headers if someone else already sent them
    if (_rq->senthdrs) {
        if (!flagHasTransferEncoding && !flagHasConnection) {
            // Someone above us generated headers, but they didn't specify any
            // transfer-encoding or connection semantics; we'll have to assume
            // they don't grok HTTP/1.1
            KEEP_ALIVE(_rq) = PR_FALSE;
        }
        return;
    }

    // Make our own Transfer-encoding and Connection decisions
    if (!flagHasTransferEncoding) {
        /*
         * At this point, we know nobody above us in the filter stack is doing
         * the transfer-encoding.  We need to make our own transfer-encoding
         * and connection decisions.
         */

        if (pb_param *pp = pblock_findkey(pb_key_content_length, _rq->srvhdrs)) {
            // Check for a user-supplied Content-length header.  We will not
            // send more than this amount of response entity body data.
            _response.contentLimit = atoi64(pp->value);

        } else if (ISMHEAD(_rq)) {
            // This is a HEAD response.  We won't send a response entity body,
            // but we want to know how much data would be sent on the
            // corresponding GET response.
            _response.flagDeferContentLengthGeneration = PR_TRUE;

        } else if (!hasResponseBody()) {
            // It is an error to send a response entity body
            _response.contentLimit = 0;

        } else if (KEEP_ALIVE(_rq)) {
            // We want to keep the connection open
            if (_rq->protv_num >= PROTOCOL_VERSION_HTTP11) {
                // We'll chunk the response body ourselves
                _response.flagChunkBody = PR_TRUE;
                pblock_kvinsert(pb_key_transfer_encoding, TOKEN_CHUNKED, TOKEN_CHUNKED_SIZE, _rq->srvhdrs);

            } else if (_response.flagBufferBody) {
                // We may be able to compute a Content-length value later on...
                _response.flagDeferContentLengthGeneration = PR_TRUE;
                _response.flagDeferConnectionGeneration = PR_TRUE;

            } else {
                // We have to close the connection
                KEEP_ALIVE(_rq) = PR_FALSE;
            }
        }

        // If we're not deferring Connection header generation...
        if (!_response.flagDeferConnectionGeneration) {
            // Tell the DaemonSession whether it should do keep-alive for this
            // connection
            KEEP_ALIVE(_rq) = _dsn->RequestKeepAlive(KEEP_ALIVE(_rq));

            // Insert a Connection header
            if (KEEP_ALIVE(_rq)) {
                if (_rq->protv_num < PROTOCOL_VERSION_HTTP11) {
                    if (!flagHasConnectionKeepAlive)
                        pblock_kvinsert(pb_key_connection, TOKEN_KEEP_ALIVE, TOKEN_KEEP_ALIVE_SIZE, _rq->srvhdrs);
                }
            } else {
                if (!flagHasConnectionClose)
                    pblock_kvinsert(pb_key_connection, TOKEN_CLOSE, TOKEN_CLOSE_SIZE, _rq->srvhdrs);
            }
        }
    }

    // Find how much space we need to reserve after the headers
    // and Connection header
    int slack = 0;
    if (_response.flagDeferContentLengthGeneration) {
        // Save room for "Content-length: x\r\n"
        slack += CONTENT_LENGTH_SIZE; // "Content-length: "
        slack += MAX_CONTENT_LENGTH_VALUE_SIZE; // 'x'
        slack += 2; // "\r\n"
    }
    if (_response.flagDeferConnectionGeneration) {
        // Save room for "Connection: keep-alive\r\n"
        slack += CONNECTION_KEEP_ALIVE_SIZE;
    }

    // Format the headers
    PR_ASSERT(_response.size - _response.used >= REQ_MAX_LINE);
    if (!allocResponseBuffer())
        return;
    int usedBeforeHeaders = _response.used;
    PR_ASSERT(usedBeforeHeaders == 0);
    _response.buffer = http_make_headers(_sn, _rq, _response.buffer, &_response.used, _response.size, slack + 2);
    _response.headers = _response.buffer + usedBeforeHeaders;
    _response.buffer[_response.used++] = '\r';
    _response.buffer[_response.used++] = '\n';
    _response.lenHeaders = _response.buffer + _response.used - _response.headers;

    // Reserve space for a generated Content-length or Connection header
    _response.used += slack;

    // Body follows headers
    _response.body = _response.buffer + _response.used;
}


/* -------------- HttpFilterContext::finalizeResponseHeaders -------------- */

inline void HttpFilterContext::finalizeResponseHeaders()
{
    if (!_response.flagCalledMakeResponseHeaders)
        makeResponseHeaders();

    // If the headers haven't been sent yet...
    if (!_rq->senthdrs) {
        /*
         * We need to finalize the response headers so we can start sending the
         * response.  If we've already seen the entire response body, we can
         * set Content-length and Connection to keep the connection open.  We
         * also want to set Content-length (even for non-keep-alive requests)
         * for HEAD responses.
         */

        if (_response.flagFinalizedBody) {
            // Append a Content-length header if appropriate
            if (_response.flagDeferContentLengthGeneration) {
                if (_response.contentSeen || !ISMHEAD(_rq)) {
                    // We're going to add something to the headers
                    PR_ASSERT(_response.lenHeaders >= 2);
                    _response.lenHeaders -= 2; // "\r\n"

                    // Append a Content-length header
                    memcpy(_response.headers + _response.lenHeaders, CONTENT_LENGTH, CONTENT_LENGTH_SIZE);
                    _response.lenHeaders += CONTENT_LENGTH_SIZE;
                    if (_response.contentSeen < INT_MAX) {
                        _response.lenHeaders += util_itoa(_response.contentSeen, &_response.headers[_response.lenHeaders]);
                    } else {
                        _response.lenHeaders += PR_snprintf(&_response.headers[_response.lenHeaders], MAX_CONTENT_LENGTH_VALUE_SIZE, "%lld", _response.contentSeen);
                    }
                    _response.headers[_response.lenHeaders++] = '\r';
                    _response.headers[_response.lenHeaders++] = '\n';

                    // Terminate headers
                    _response.headers[_response.lenHeaders++] = '\r';
                    _response.headers[_response.lenHeaders++] = '\n';
                }

                _response.flagDeferContentLengthGeneration = PR_FALSE;
            }

            // Append a Connection header if appropriate
            if (_response.flagDeferConnectionGeneration) {
                // We're going to add something to the headers
                PR_ASSERT(_response.lenHeaders >= 2);
                _response.lenHeaders -= 2; // "\r\n"

                // Tell the DaemonSession whether it should do keep-alive for
                // this connection
                KEEP_ALIVE(_rq) = _dsn->RequestKeepAlive(KEEP_ALIVE(_rq));

                // Append a Connection: keep-alive or Connection: close header
                if (KEEP_ALIVE(_rq)) {
                    if (_rq->protv_num < PROTOCOL_VERSION_HTTP11) {
                        memcpy(_response.headers + _response.lenHeaders, CONNECTION_KEEP_ALIVE, CONNECTION_KEEP_ALIVE_SIZE);
                        _response.lenHeaders += CONNECTION_KEEP_ALIVE_SIZE;
                        pblock_kvinsert(pb_key_connection, TOKEN_KEEP_ALIVE, TOKEN_KEEP_ALIVE_SIZE, _rq->srvhdrs);
                    }
                } else {
                    memcpy(_response.headers + _response.lenHeaders, CONNECTION_CLOSE, CONNECTION_CLOSE_SIZE);
                    _response.lenHeaders += CONNECTION_CLOSE_SIZE;
                    pblock_kvinsert(pb_key_connection, TOKEN_CLOSE, TOKEN_CLOSE_SIZE, _rq->srvhdrs);
                }

                // Terminate headers
                _response.headers[_response.lenHeaders++] = '\r';
                _response.headers[_response.lenHeaders++] = '\n';

                _response.flagDeferConnectionGeneration = PR_FALSE;
            }

            // We should have reserved enough space back in makeResponseHeaders
            PR_ASSERT(_response.body ? _response.headers + _response.lenHeaders <= _response.body : _response.lenHeaders < _response.size);

        } else {
            // Append a Connection header if appropriate
            if (_response.flagDeferConnectionGeneration) {
                // Couldn't generate a Content-length header, so we'll need to
                // close the connection
                KEEP_ALIVE(_rq) = PR_FALSE;

                // Append a Connection: close header if appropriate
                if (_rq->protv_num >= PROTOCOL_VERSION_HTTP11) {
                    memcpy(_response.headers + _response.lenHeaders, CONNECTION_CLOSE, CONNECTION_CLOSE_SIZE);
                    _response.lenHeaders += CONNECTION_CLOSE_SIZE;
                    pblock_kvinsert(pb_key_connection, TOKEN_CLOSE, TOKEN_CLOSE_SIZE, _rq->srvhdrs);
                }

                _response.flagDeferConnectionGeneration = PR_FALSE;
            }
        }
    }
}


/* --------------- HttpFilterContext::finalizeResponseBody ---------------- */

inline void HttpFilterContext::finalizeResponseBody()
{
    if (_response.flagFinalizedBody)
        return;

    // We set a content-length that makes sense for AddLog.  Because of HEAD,
    // it might not make sense in the HTTP response, so make sure we've
    // formatted the headers before we muck with things.
    _response.flagFinalizedBody = PR_TRUE;
    finalizeResponseHeaders();

    // Setup content-length for AddLog
    if (!hasResponseBody()) {
        // This was a HEAD, etc. response.  For AddLog, content-length is 0.
        if (pblock_findkey(pb_key_content_length, _rq->srvhdrs)) {
            pb_param *pp = pblock_removekey(pb_key_content_length, _rq->srvhdrs);
            if (pp)
                param_free(pp);
        }
        pblock_kvinsert(pb_key_content_length, "0", 1, _rq->srvhdrs);

    } else if (_response.contentSeen != _response.contentLimit) {
        // If the Service SAF failed to send all the response entity body...
        if (_response.contentLimit != -1) {
            // Log a warning and make sure the connection goes away
            if (!_response.flagErrorSending) {
                log_error(LOG_WARN, "finish-response", _sn, _rq, XP_GetAdminStr(DBT_ResponseContentLengthMismatchXofY), _response.contentSeen, _response.contentLimit);
            }
            KEEP_ALIVE(_rq) = PR_FALSE;

            // Remove the invalid Content-length header
            pb_param *pp = pblock_removekey(pb_key_content_length, _rq->srvhdrs);
            if (pp)
                param_free(pp);
        }

        // Format a new Content-length value
        char content_length[21];
        int content_length_len;
        if (_response.contentSeen < INT_MAX) {
            content_length_len = util_itoa(_response.contentSeen, content_length);
        } else {
            content_length_len = PR_snprintf(content_length, sizeof(content_length), "%lld", _response.contentSeen);
        }

        // Add a new Content-length header for AddLog
        pblock_kvinsert(pb_key_content_length, content_length, content_length_len, _rq->srvhdrs);
    }

    // We're all done, no more data can be sent
    _response.contentLimit = _response.contentSeen;
}


/* ----------------- HttpFilterContext::addResponseChunk ------------------ */

inline PRBool HttpFilterContext::addResponseChunk(const PRIOVec *iov, int iov_size, int chunkLength)
{
    // Find the number of digits required to represent the chunk length
    int digits = get_chunk_length_digits(chunkLength);
    if (digits < 1)
        return PR_FALSE;

    // Check that there's enough room for the chunk.  Note that we always save
    // space for the final chunk.
    int extra = digits + 2 + 2; // "<chunk length>\r\n<response data>\r\n"
    if (_response.used + extra + chunkLength + FINAL_CHUNK_SIZE > _response.size)
        return PR_FALSE;

    // Buffer chunk length
    _response.lenBody += digits;
    char *p = _response.body + _response.lenBody;
    int remaining = chunkLength;
    while (digits--) {
        int nibble = (remaining & 0xf);
        *--p = (nibble >= 10) ? 'a' + nibble - 10 : '0' + nibble;
        remaining >>= 4;
    }
    _response.body[_response.lenBody++] = '\r';
    _response.body[_response.lenBody++] = '\n';

    // Buffer new response entity body data
    for (int i = 0; i < iov_size; i++) {
        memcpy(_response.body + _response.lenBody, iov[i].iov_base, iov[i].iov_len);
        _response.lenBody += iov[i].iov_len;
    }

    // Buffer chunk trailer
    _response.body[_response.lenBody++] = '\r';
    _response.body[_response.lenBody++] = '\n';

    // We used some of buffer for body
    _response.used = _response.body + _response.lenBody - _response.buffer;

    return PR_TRUE;
}


/* ------------------ HttpFilterContext::hasResponseBody ------------------ */

inline PRBool HttpFilterContext::hasResponseBody()
{
    /*
     * RFC 2616 4.3
     */

    // HEAD responses don't have bodies
    if (ISMHEAD(_rq))
        return PR_FALSE;

    // 1xx, 204, and 304 responses don't have bodies
    if (const char *status = pblock_findkeyval(pb_key_status, _rq->srvhdrs)) {
        if (status[0] == '1')
            return PR_FALSE;
        if (status[0] == '2' && status[1] == '0' && status[2] == '4')
            return PR_FALSE;
        if (status[0] == '3' && status[1] == '0' && status[2] == '4')
            return PR_FALSE;
    }

    // All other responses have bodies by default
    return !_response.flagSuppressBody;
}


/* -------------- HttpFilterContext::getResponseContentLimit -------------- */

inline PRInt64 HttpFilterContext::getResponseContentLimit()
{
    // If there's no limit, return a big number
    if (_response.contentLimit == -1)
        return MAX_INT64;

    return _response.contentLimit - _response.contentSeen;
}


/* --------------- HttpFilterContext::suppressResponseBody ---------------- */

inline PRBool HttpFilterContext::suppressResponseBody(PRInt64 amount)
{
    if (!_response.flagSuppressBody)
        return PR_FALSE;

    _response.contentSeen += amount;

    return PR_TRUE;
}


/* ---------------- HttpFilterContext::bufferResponseBody ----------------- */

inline PRBool HttpFilterContext::bufferResponseBody(const PRIOVec *iov, int iov_size, int amount)
{
    PR_ASSERT(_response.flagBufferBody || _response.used);

    // Generate the response headers before buffering.  We need to know whether
    // we're doing chunked encoding.
    if (!_response.flagCalledMakeResponseHeaders)
        makeResponseHeaders();

    PR_ASSERT(_response.buffer + _response.used == _response.body + _response.lenBody);
    PR_ASSERT(!_response.flagSuppressBody);

    if (amount < 1)
        return PR_TRUE;

    if (!allocResponseBuffer())
        return PR_FALSE;

    // Disable buffering for long-running programs
    if (_response.lenBody && ft_time() > _rq->req_start + _response.secondsFlush) {
        _response.flagBufferBody = PR_FALSE;
        return PR_FALSE;
    }

    // If we're supposed to chunk the response body...
    if (_response.flagChunkBody) {
        // Create a new chunk if appropriate
        if (addResponseChunk(iov, iov_size, amount)) {
            _response.contentSeen += amount;
            return PR_TRUE;
        }

        return PR_FALSE;
    }

    // Check that there's enough room for the data
    if (_response.used + amount > _response.size)
        return PR_FALSE;

    // Buffer new response entity body data
    for (int i = 0; i < iov_size; i++) {
        memcpy(_response.body + _response.lenBody, iov[i].iov_base, iov[i].iov_len);
        _response.lenBody += iov[i].iov_len;
    }
    _response.contentSeen += amount;

    // We used some of buffer for body
    _response.used = _response.body + _response.lenBody - _response.buffer;

    return PR_TRUE;
}


/* --------------- HttpFilterContext::bufferSendfileHeader ---------------- */

inline PRBool HttpFilterContext::bufferSendfileHeader(const void *header, int hlen)
{
    PRBool flagBuffered = PR_FALSE;

    // If we haven't sent the HTTP headers yet, we'd like to concatenate them
    // with the sendfile headers if possible.  Note that we do this regardless
    // of the value of _response.flagBufferBody.
    if (_response.used) {
        PRIOVec header_iov[1];
        header_iov[0].iov_base = (char *)header;
        header_iov[0].iov_len = hlen;
        flagBuffered = bufferResponseBody(header_iov, 1, hlen);
    }

    return flagBuffered;
}


/* ---------------- HttpFilterContext::addResponsePreamble ---------------- */

inline void HttpFilterContext::addResponsePreamble(OutputVector &output, int chunkLength)
{
    /*
     * N.B. we require 3 unused output vector iov[] entries
     */

    if (!_response.flagCalledMakeResponseHeaders)
        makeResponseHeaders();

    _response.contentSeen += chunkLength;

    // Prepend HTTP headers
    if (_response.lenHeaders && !_rq->senthdrs) {
        finalizeResponseHeaders();
        output.addBuffer(_response.headers, _response.lenHeaders);
        _rq->senthdrs = PR_TRUE;
        // lenHeaders is reset in addEpilogue()
    }

    // Prepend previously buffered response entity body data
    if (_response.lenBody && !_response.flagSuppressBody) {
        output.addBuffer(_response.body, _response.lenBody);
        // lenBody is reset in addEpilogue()
    }

    // Prepend chunk length if caller has response entity body data to send
    if (_response.flagChunkBody && chunkLength) {
        PR_ASSERT(!_response.flagSuppressBody);

        char *p = _response.bufferChunkLength + sizeof(_response.bufferChunkLength);
        *--p = '\n';
        *--p = '\r';
        int remaining = chunkLength;
        do {
            int nibble = (remaining & 0xf);
            *--p = (nibble >= 10) ? 'a' + nibble - 10 : '0' + nibble;
            remaining >>= 4;
        } while (remaining > 0);

        output.addBuffer(p, _response.bufferChunkLength + sizeof(_response.bufferChunkLength) - p);
    }

    // Set flagCommitted if any data will be sent
    if (chunkLength || output.iov_size)
        _response.flagCommitted = PR_TRUE;
}


/* ---------------- HttpFilterContext::addResponseEpilogue ---------------- */

inline void HttpFilterContext::addResponseEpilogue(OutputVector &output, int chunkLength)
{
    /*
     * N.B. we require 2 unused output vector iov[] entries
     */

    // Append chunk trailer if caller has response entity body data to send
    if (_response.flagChunkBody && chunkLength > 0) {
        PR_ASSERT(!_response.flagSuppressBody);
        output.addBuffer("\r\n", 2);
    }

    // If all the response entity body data has been buffered...
    if (_response.flagFinalizedBody) {
        // Append "0\r\n\r\n" to the end of chunked output
        if (_response.flagChunkBody) {
            PR_ASSERT(_response.used + FINAL_CHUNK_SIZE <= _response.size);
            if (_response.buffer) {
                output.addBuffer(_response.buffer + _response.used, FINAL_CHUNK_SIZE);
                for (int i = 0; i < FINAL_CHUNK_SIZE; i++)
                    _response.buffer[_response.used++] = FINAL_CHUNK[i];
            } else {
                output.addBuffer(FINAL_CHUNK, FINAL_CHUNK_SIZE);
            }
        }
    }

    // buffer will be empty after the upcoming send()
    _response.headers = NULL;
    _response.lenHeaders = 0;
    _response.lenBody = 0;
    _response.used = _response.body - _response.buffer;
}


/* ---------------- HttpFilterContext::allocResponseBuffer ---------------- */

inline PRBool HttpFilterContext::allocResponseBuffer()
{
    PR_ASSERT(_response.size >= REQ_MAX_LINE);

    if (!_response.buffer) {
        _response.buffer = (char *)pool_malloc(_sn->pool, _response.size);
        _response.body = _response.buffer;
        PR_ASSERT(_response.used == 0);
    }

    return (_response.buffer != NULL);
}


/* --------------- HttpFilterContext::setResponseBufferSize --------------- */

inline void HttpFilterContext::setResponseBufferSize(int size)
{
    if (size > _response.size) {
        // Grow buffer
        _response.size = size;
        if (_response.buffer) {
            int offsetHeaders = _response.headers - _response.buffer;
            int offsetBody = _response.body - _response.buffer;
            _response.buffer = (char *)pool_realloc(_sn->pool, _response.buffer, _response.size);
            if (_response.headers)
                _response.headers = _response.buffer + offsetHeaders;
            if (_response.body)
                _response.body = _response.buffer + offsetBody;
        }
        _response.flagBufferBody = PR_TRUE;

    } else if (size > _response.used && size > REQ_MAX_LINE) {
        // Shrink buffer to requested size
        _response.size = size;
        _response.flagBufferBody = PR_TRUE;

    } else {
        // No more buffering
        _response.flagBufferBody = PR_FALSE;
    }
}


/* --------------- HttpFilterContext::getResponseBufferSize --------------- */

inline int HttpFilterContext::getResponseBufferSize()
{
    return _response.size;
}


/* ------------- HttpFilterContext::setResponseBufferTimeout -------------- */

inline void HttpFilterContext::setResponseBufferTimeout(int seconds)
{
    _response.secondsFlush = seconds;
}


/* --------------- HttpFilterContext::setBufferResponseBody --------------- */

inline void HttpFilterContext::setBufferResponseBody(PRBool flag)
{
    _response.flagBufferBody = flag;
}


/* ------------------------ httpfilter_method_read ------------------------ */

static int httpfilter_method_read(FilterLayer *layer, void *buf, int amount, int timeout)
{
    HttpFilterContext *httpfilter = (HttpFilterContext *)layer->context->data;

    filter_read_callback(layer->context->sn);

    return httpfilter->read(layer->lower, buf, amount, get_nspr_timeout(timeout));
}


/* ----------------------- httpfilter_method_write ------------------------ */

static int httpfilter_method_write(FilterLayer *layer, const void *buf, int amount)
{
    Session *sn = layer->context->sn;

    HttpFilterContext *httpfilter = (HttpFilterContext *)layer->context->data;

    int rv = filter_output_callback(sn);
    if (rv == REQ_PROCEED) {
        // Added new filters, so commit the response headers and start again
        // from the top of the stack
        httpfilter->makeResponseHeaders();
        return net_write(sn->csd, buf, amount);
    }
    if (rv != REQ_NOACTION)
        return -1;

    NSAPIIOVec iov;
    iov.iov_base = (char *)buf;
    iov.iov_len = amount;
    return httpfilter->writev(layer->lower, &iov, 1, amount);
}


/* ----------------------- httpfilter_method_writev ----------------------- */

static int httpfilter_method_writev(FilterLayer *layer, const NSAPIIOVec *iov, int iov_size)
{
    Session *sn = layer->context->sn;

    HttpFilterContext *httpfilter = (HttpFilterContext *)layer->context->data;

    int rv = filter_output_callback(sn);
    if (rv == REQ_PROCEED) {
        // Added new filters, so commit the response headers and start again
        // from the top of the stack
        httpfilter->makeResponseHeaders();
        return net_writev(sn->csd, iov, iov_size);
    }
    if (rv != REQ_NOACTION)
        return -1;

    // Calculate length of new response entity body data
    int amount = 0;
    for (int i = 0; i < iov_size; i++)
        amount += iov[i].iov_len;

    return httpfilter->writev(layer->lower, iov, iov_size, amount);
}


/* ---------------------- httpfilter_method_sendfile ---------------------- */

static int httpfilter_method_sendfile(FilterLayer *layer, sendfiledata *sfd)
{
    Session *sn = layer->context->sn;

    HttpFilterContext *httpfilter = (HttpFilterContext *)layer->context->data;

    int rv = filter_output_callback(sn);
    if (rv == REQ_PROCEED) {
        // Added new filters, so commit the response headers and start again
        // from the top of the stack
        httpfilter->makeResponseHeaders();
        return net_sendfile(sn->csd, sfd);
    }
    if (rv != REQ_NOACTION)
        return -1;

    // If caller didn't say how much to send from the file...
    PRInt64 flen = sfd->len;
    if (flen <= 0) {
        // Get the file length
        PRFileInfo64 finfo;
        if (PR_GetOpenFileInfo64(sfd->fd, &finfo) != PR_SUCCESS)
            return -1;

        // We'll send from the caller's offset to EOF; how many bytes is that?
        flen = finfo.size - sfd->offset;
        if (flen < 0) {
            PR_SetError(PR_FILE_SEEK_ERROR, 0);
            return -1;
        }
    }

    // Response entity body data includes the headers, file, and trailers
    PRInt64 amount = sfd->hlen + flen + sfd->tlen;

    return httpfilter->sendfile(layer->lower, sfd, flen, amount);
}


/* ----------------------- httpfilter_method_flush ------------------------ */

static int httpfilter_method_flush(FilterLayer *layer)
{
    HttpFilterContext *httpfilter = (HttpFilterContext *)layer->context->data;

    filter_generic_callback(layer->context->sn);

    return httpfilter->flush(layer->lower);
}


/* ----------------------- httpfilter_method_remove ----------------------- */

static void httpfilter_method_remove(FilterLayer *layer)
{
    HttpFilterContext *httpfilter = (HttpFilterContext *)layer->context->data;

    httpfilter->remove(layer->lower);

    if (layer->context->sn)
        session_set_httpfilter_context(layer->context->sn, NULL);
}


/* ----------------------- httpfilter_method_insert ----------------------- */

static int httpfilter_method_insert(FilterLayer *layer, pblock *pb)
{
    HttpFilterContext *httpfilter = (HttpFilterContext *)layer->context->data;
    if (!httpfilter)
        return REQ_NOACTION;

    if (layer->context->sn)
        session_set_httpfilter_context(layer->context->sn, httpfilter);

    httpfilter->insert(layer->context->sn, layer->context->rq);

    return REQ_PROCEED;
}


/* ------------------------ httpfilter_get_filter ------------------------- */

const Filter *httpfilter_get_filter(void)
{
    return _httpfilter_filter;
}


/* ---------------------- httpfilter_create_context ----------------------- */

HttpFilterContext *httpfilter_create_context(DaemonSession *dsn)
{
    HttpFilterContext *httpfilter = new HttpFilterContext(dsn);
    return httpfilter;
}


/* ------------------ httpfilter_set_output_buffer_size ------------------- */

void httpfilter_set_output_buffer_size(Session *sn, Request *rq, int size)
{
    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    if (httpfilter)
        httpfilter->setResponseBufferSize(size);
}


/* ------------------ httpfilter_get_output_buffer_size ------------------- */

int httpfilter_get_output_buffer_size(Session *sn, Request *rq)
{
    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    if (!httpfilter)
        return 0;

    return httpfilter->getResponseBufferSize();
}


/* ----------------- httpfilter_set_output_buffer_timeout ----------------- */

void httpfilter_set_output_buffer_timeout(Session *sn, Request *rq, int ms)
{
    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    if (httpfilter)
        httpfilter->setResponseBufferTimeout(ms / 1000);
}


/* ----------------------- httpfilter_buffer_output ----------------------- */

void httpfilter_buffer_output(Session *sn, Request *rq, PRBool enabled)
{
    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    if (httpfilter)
        httpfilter->setBufferResponseBody(enabled);
}


/* ------------------ httpfilter_set_request_body_limit ------------------- */

int httpfilter_set_request_body_limit(Session *sn, Request *rq, int size)
{
    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    if (!httpfilter)
        return REQ_NOACTION;

    if (!httpfilter->hasRequestBody())
        return REQ_NOACTION;

    if (httpfilter->setRequestBodyLimit(size) != PR_SUCCESS)
        return REQ_ABORTED;

    return REQ_PROCEED;
}


/* ---------------------- httpfilter_suppress_flush ----------------------- */

void httpfilter_suppress_flush(Session *sn, Request *rq, PRBool suppressed)
{
    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    if (httpfilter)
        httpfilter->suppressFlush(suppressed);
}


/* ------------------- httpfilter_suppress_100_continue ------------------- */

void httpfilter_suppress_100_continue(Session *sn, Request *rq)
{
    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    if (httpfilter)
        httpfilter->suppress100Continue();
}


/* ---------------------- httpfilter_start_response ----------------------- */

int httpfilter_start_response(Session *sn, Request *rq)
{
    if (sn->csd_open == 0)
        return REQ_EXIT;

    int rv = filter_output_callback(sn);
    if (rv != REQ_NOACTION && rv != REQ_PROCEED)
        return REQ_ABORTED;

    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);
    if (!httpfilter)
        return REQ_NOACTION;

    httpfilter->makeResponseHeaders();

    if (!httpfilter->hasResponseBody())
        return REQ_NOACTION;

    return REQ_PROCEED;
}


/* ---------------------- httpfilter_finish_request ----------------------- */

int httpfilter_finish_request(Session *sn, Request *rq)
{
    PR_ASSERT(!INTERNAL_REQUEST(rq));

    if (sn->csd_open == 0)
        return REQ_EXIT;

    int rv = filter_output_callback(sn);
    if (rv != REQ_NOACTION && rv != REQ_PROCEED)
        return REQ_ABORTED;

    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);
    if (httpfilter) {
        // Return an error if there was an Input stage failure
        if (session_input_done(sn)) {
            rv = session_input_rv(sn);
            if (rv != REQ_NOACTION && rv != REQ_PROCEED)
                return REQ_ABORTED;
        }

        // Remove Request-based filters above the httpfilter
        filter_finish_request(sn, rq, _httpfilter_filter);

        // Indicate all data has been buffered/sent.  We can't do this until
        // we remove the layers above the httpfilter as they may have buffered
        // data the httpfilter hasn't seen yet.
        httpfilter->finalizeResponseBody();
    }

    return REQ_PROCEED;
}


/* ---------------------- httpfilter_reset_response ----------------------- */

int httpfilter_reset_response(Session *sn, Request *rq)
{
    if (sn->csd_open == 0)
        return REQ_EXIT;

    if (!request_output_done(rq))
        return REQ_PROCEED; // No output sent yet, nothing to reset

    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    FilterLayer *layer = filter_layer(sn->csd, _httpfilter_filter);

    if (!httpfilter && !layer)
        return REQ_PROCEED; // No httpfilter present, nothing to reset

    PR_ASSERT(httpfilter && layer && layer->context->data == (void *)httpfilter);
    if (!httpfilter || !layer || layer->context->data != (void *)httpfilter)
        return REQ_NOACTION; // Hmm, filter stack inconsistent with Session?

    if (layer->context->sn != sn || layer->context->rq != rq)
        return REQ_NOACTION; // We're in a child request; don't reset

    // Suppress output at the httpfilter level
    if (httpfilter->suppressOutput() != PR_SUCCESS)
        return REQ_NOACTION; // Response already committed

    // Remember any Output stage error
    int rv = request_output_rv(rq);
    if (rv == REQ_NOACTION)
        rv = REQ_PROCEED;

    // Remove the Request-based filters (including httpfilter)
    filter_finish_request(sn, rq, NULL);

    PR_ASSERT(!filter_layer(sn->csd, _httpfilter_filter));

    // Add back the httpfilter (it was removed above)
    filter_insert(NULL, NULL, sn, rq, httpfilter, _httpfilter_filter);

    // By this point, any entity boday has either been consumed by a SAF or was
    // drained when httpfilter was removed
    httpfilter->ignoreRequestBody();

    // Make the Request look like it's new
    rq->senthdrs = PR_FALSE;
    if (rv == REQ_PROCEED) {
        // We'll build a new filter stack when the error response is sent
        ((NSAPIRequest *)rq)->output_done = PR_FALSE;
    } else {
        // There was an error constructing the original filter stack, so don't
        // even try to build one for the error response (as we'd probably hit
        // the same filter configuration error again then)
        ((NSAPIRequest *)rq)->output_rv = REQ_NOACTION;
    }

    return rv;
}


/* ------------------- httpfilter_request_body_buffered ------------------- */

PRBool httpfilter_request_body_buffered(Session *sn, Request *rq)
{
    // XXX This is a temporary kludge that lets Proxy know whether the client
    // has started sending the request body.  If the client has started sending
    // the request body, Proxy can safely block waiting for the request body to
    // arrive in its entirety.  If the client hasn't started sending the
    // request body, Proxy will need to PR_Poll() both the client and origin so
    // it can wait for the earlier of a) request body from the client or b) an
    // HTTP/1.1 100 Continue response from the origin.
    //
    // XXX Ultimately this should be replaced by a more general NSAPI filter
    // method (e.g. available, dataready, or poll) that's hooked into the NSPR
    // poll IO method.

    HttpFilterContext *httpfilter = session_get_httpfilter_context(sn);

    if (httpfilter)
        return httpfilter->isRequestMessageBodyBuffered();

    return PR_FALSE;   
}


/* -------------- httpfilter_set_default_output_buffer_size --------------- */

void httpfilter_set_default_output_buffer_size(int size)
{
    _defaultOutputStreamSize = size;
}


/* -------------- httpfilter_get_default_output_buffer_size --------------- */

int httpfilter_get_default_output_buffer_size(void)
{
    return _defaultOutputStreamSize;
}


/* --------------------------- httpfilter_init ---------------------------- */

PRStatus httpfilter_init(void)
{
    if (_httpfilter_filter)
        return PR_SUCCESS;

    int maxSeconds = ((PRIntervalTime)0x7fffffff / _ticksPerSecond / 3600) * 3600;

    _MaxRequestBodySize = conf_getboundedinteger("MaxRequestBodySize",
                                                 -1,
                                                 INT_MAX,
                                                 -1); // default is no limit

    int RqTransactionTimeout = conf_getboundedinteger("RqTransactionTimeout",
                                                   0,    // minimum 0
                                                   3600, // maximum 1 hour
                                                   30);  // default 30 seconds
    _timeoutDrain = RqTransactionTimeout * _ticksPerSecond;

    int flushTimer = conf_getboundedinteger("flushTimer",
                                            0,       // minimum 0
                                            3600000, // maximum 1 hour
                                            3000);   // default 3 seconds
    _secondsFlush = flushTimer / 1000;

    int NetWriteTimeout = conf_getboundedinteger("NetWriteTimeout",
                                                 0,          // minimum 0 (no timeout)
                                                 maxSeconds, // maximum is platform dependent
                                                 1800);      // default 30 minutes
    if (NetWriteTimeout) {
        _timeoutWrite = NetWriteTimeout * _ticksPerSecond;
    } else {
        _timeoutWrite = PR_INTERVAL_NO_TIMEOUT;
    }

    int NetSendfileTimeout = conf_getboundedinteger("NetSendfileTimeout",
                                                    0,          // minimum 0 (no timeout)
                                                    maxSeconds, // maximum is platform dependent
                                                    1800);      // default 0 (no timeout)
    if (NetSendfileTimeout) {
        _timeoutSendfile = NetSendfileTimeout * _ticksPerSecond;
    } else {
        _timeoutSendfile = PR_INTERVAL_NO_TIMEOUT;
    }

    FilterMethods httpfilter_methods = FILTER_METHODS_INITIALIZER;

    httpfilter_methods.insert = &httpfilter_method_insert;
    httpfilter_methods.remove = &httpfilter_method_remove;
    httpfilter_methods.flush = &httpfilter_method_flush;
    httpfilter_methods.read = &httpfilter_method_read;
    httpfilter_methods.write = &httpfilter_method_write;
    httpfilter_methods.writev = &httpfilter_method_writev;
    httpfilter_methods.sendfile = &httpfilter_method_sendfile;

    _httpfilter_filter = filter_create_internal("magnus-internal/http-server",
                                                FILTER_MESSAGE_CODING,
                                                &httpfilter_methods,
                                                FILTER_CALLS_CALLBACKS);
    if (!_httpfilter_filter)
        return PR_FAILURE;

    return PR_SUCCESS;
}

