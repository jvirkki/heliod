/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright (c) 2026, The Heliod Web Server Project. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "H2Stream.h"
#include "H2Session.h"
#include "H2StreamSocket.h"
#include "H2StreamPool.h"

#include "base/net.h"
#include "base/buffer.h"
#include "netsite.h"
#include "public/nsapi.h"
#include "frame/log.h"

#include <nghttp2/nghttp2.h>

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

PRBool find_crlf(const uint8_t* data, size_t len, size_t start, size_t& out)
{
    for (size_t i = start; i + 1 < len; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            out = i;
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

size_t find_colon(const uint8_t* data, size_t end, size_t start)
{
    for (size_t i = start; i < end; ++i) {
        if (data[i] == ':')
            return i;
    }
    return std::string::npos;
}

void to_lower(std::string& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<char>(tolower(static_cast<unsigned char>(s[i])));
}

PRBool is_hop_by_hop(const std::string& name)
{
    return (name == "connection" ||
            name == "transfer-encoding" ||
            name == "keep-alive" ||
            name == "upgrade" ||
            name == "proxy-connection" ||
            name == "te" ||
            name == "trailer");
}

// Headers buildHttp11Request() generates for itself, and must therefore not also
// pass through from the client.  Neither is hop-by-hop, so the hop-by-hop filter
// does not catch them, and the result is two of each in the synthesized request.
//
//  content-length  Advisory in HTTP/2 -- the body arrives in DATA frames and the
//                  header may be absent, wrong, or (for a request whose body was
//                  cut short) inconsistent with what actually arrived.  The count
//                  of buffered request-body bytes is the authoritative one, and it
//                  is exact: a stream is not dispatched until END_STREAM.
//  host            Synthesized from :authority, which RFC 9113 8.3.1 makes the
//                  authority of record when a request carries both.
PRBool is_synthesized_header(const std::string& name)
{
    return (name == "content-length" || name == "host");
}

// Fields that arrive split over HTTP/2 but have to reach the rest of the server as
// one header line, and the delimiter each is rejoined with.  NULL for everything
// else, meaning "leave it alone": repeats of those are folded by heliod's own
// HTTP/1.1 parser exactly as they would be on an HTTP/1.1 connection, which is the
// parity this function exists to provide.
//
// There is one entry, and that is not an oversight to be tidied up later.  RFC 9110
// 5.3 only lets a field be sent as several lines when its grammar is a
// comma-separated list, and the parser already folds those correctly with ", ".
// Cookie is the documented exception: RFC 6265 5.4 forbids repeating it at all over
// HTTP/1.1, and RFC 9113 8.2.3 re-legalizes the split purely so an unchanged cookie
// can be referenced by index in the HPACK dynamic table instead of resent -- on the
// condition that the receiver rejoin the crumbs with the two octets 0x3B 0x20 (the
// ASCII string "; ") before the request reaches a non-HTTP/2 context.  Go's HTTP/2
// transport does that splitting, so every request a Go-based reverse proxy forwards
// to an h2c upstream arrives crumbed.
//
// Before adding a row, check the field is genuinely legal to repeat *and* joins with
// something other than a comma.  A field that is illegal to repeat does not belong
// here: comma-folding a singleton is the corruption this table exists to prevent,
// and doing it from a table would only make it look deliberate.
const char* coalesce_delimiter(const std::string& lowerName)
{
    if (lowerName == "cookie")
        return "; ";
    return NULL;
}

// Did a field with this (already lower-cased) name appear before pair index i?  Used
// to emit a coalesced field once, at the position its first piece held.
PRBool header_seen_before(const std::vector<std::string>& headers,
                          size_t i, const std::string& lowerName)
{
    for (size_t j = 0; j + 1 < i; j += 2) {
        std::string earlier = headers[j];
        to_lower(earlier);
        if (earlier == lowerName)
            return PR_TRUE;
    }
    return PR_FALSE;
}

nghttp2_nv make_nv(const char* name, size_t nameLen,
                   const char* value, size_t valueLen)
{
    nghttp2_nv nv;
    nv.name = reinterpret_cast<uint8_t*>(const_cast<char*>(name));
    nv.value = reinterpret_cast<uint8_t*>(const_cast<char*>(value));
    nv.namelen = nameLen;
    nv.valuelen = valueLen;
    nv.flags = NGHTTP2_NV_FLAG_NO_COPY_NAME |
               NGHTTP2_NV_FLAG_NO_COPY_VALUE;
    return nv;
}

// Capacity of the per-stream response ring.  This is the whole point of the
// streaming response path: a response of any size travels through a window this
// big rather than being materialized in full.
//
// The size is a compromise.  It has to be large enough that the producer is not
// woken constantly (the SAF writes in units of a few KB, so this is several
// writes' worth) and small enough that the worst case is tolerable: a connection
// may have MAX_CONCURRENT_STREAMS responses open at once, so the bound is
// capacity * 100 per connection.  Rings are allocated on the first body byte, so
// a stream that produces only headers never pays for one.
const size_t RESPONSE_RING_CAPACITY = 32 * 1024;

// Cap on the captured status line and headers.  The SAF writes them into the
// same byte stream as the body, so they must be accumulated before the split
// point is known; a response that never produces a blank line must not be
// allowed to grow without bound.
const size_t MAX_RESPONSE_HEAD_BYTES = 64 * 1024;

// Longest chunk size line we will tolerate.  A legitimate one is a few hex
// digits plus optional extensions; anything beyond this is a malformed or
// hostile response, not something to keep buffering.
const size_t MAX_CHUNK_LINE_BYTES = 256;

} // anonymous namespace

namespace heliod {

H2Stream::H2Stream(uint32_t streamId, H2Session* session)
    : streamId_(streamId),
      session_(session),
      dispatchOk_(PR_FALSE),
      inFlight_(PR_FALSE),
      abandoned_(PR_FALSE),
      submitQueued_(PR_FALSE),
      submitted_(PR_FALSE),
      poolExhausted_(PR_FALSE),
      requestBodyPos_(0),
      requestComplete_(PR_FALSE),
      requestBodyRefused_(PR_FALSE),
      responseClosed_(PR_FALSE),
      headersReady_(PR_FALSE),
      statusCode_(200),
      chunkedResponse_(PR_FALSE),
      chunkState_(CHUNK_NONE),
      chunkRemaining_(0),
      ringHead_(0),
      ringCount_(0),
      bodyEof_(PR_FALSE),
      bodyAbort_(PR_FALSE),
      needResume_(PR_FALSE),
      responseLength_(-1),
      bodySent_(0),
      bodyDone_(PR_FALSE),
      connection_(NULL),
      streamSocket_(NULL)
{
    XP_InitLock(&bodyLock_);
    XP_InitCondition(&bodySpace_, &bodyLock_);
}

H2Stream::~H2Stream()
{
    if (streamSocket_) {
        PR_Close(streamSocket_);
        streamSocket_ = NULL;
    }
    if (connection_) {
        // connection_->fd was set to streamSocket_ during dispatchRequest().
        // streamSocket_ has already been closed above; make sure nothing tries
        // to close it a second time.
        connection_->fd = NULL;

        // Undo exactly what init() did, by hand, rather than calling
        // Connection::destroy():
        //
        //  - destroy() decrements Connection::countActive, but init() builds
        //    this Connection without going through Connection::create() and so
        //    never incremented it.  Calling destroy() here made the server's
        //    active-connection count (and the load average derived from it)
        //    drift downwards by one per HTTP/2 stream.
        //  - destroy() would also return the struct to a ConnectionQueue, but
        //    a per-stream Connection has no queue (connQueue is NULL).
        //
        // Releasing the ListenSocketConfig reference balances the ref() in
        // init(); returning the struct to the stream pool closes the per-stream
        // leak and lets the next stream reuse it.  connQueue stays NULL: this is
        // an H2 stream pool struct, not a ConnectionQueue one, so ~Connection()
        // (which only rejects *pooled* ConnectionQueue connections) never runs
        // for it -- the pool keeps the struct alive for reuse.
        if (connection_->lsConfig) {
            connection_->lsConfig->unref();
            connection_->lsConfig = NULL;
            connection_->sslconfig = NULL;
        }
        PR_ASSERT(connection_->connQueue == NULL);
        H2StreamConnectionPool::instance()->release(connection_);
        connection_ = NULL;
    }

    // Nobody can be waiting on these: a stream is only destroyed once it is no
    // longer in flight, and the producer is the pipeline running in flight.
    // Destroy the condition first, since it was initialized against the lock.
    XP_DestroyCondition(&bodySpace_);
    XP_DestroyLock(&bodyLock_);
}

PRStatus H2Stream::init(Connection* parentConn)
{
    if (!parentConn || !parentConn->lsConfig)
        return PR_FAILURE;

    // Draw the Connection from the process-wide HTTP/2 stream pool rather than
    // ConnectionQueue (see H2StreamPool.h for why) or a bare new (which was
    // unbounded).  A NULL return means the pool is at its cap: record it so the
    // caller can refuse the stream with REFUSED_STREAM instead of INTERNAL_ERROR.
    H2StreamConnectionPool* pool = H2StreamConnectionPool::instance();
    connection_ = pool ? pool->acquire() : NULL;
    if (!connection_) {
        poolExhausted_ = PR_TRUE;
        return PR_FAILURE;
    }

    // Initialize the per-stream Connection by hand.  We do not use
    // Connection::create() because we do not have a ListenSocket pointer and we
    // do not want to update the global connection statistics for a logical
    // HTTP/2 stream.
    connection_->fd = NULL;
    connection_->fNewlyAccepted = PR_FALSE;
    connection_->fSSLEnabled = parentConn->fSSLEnabled;
    connection_->fUncleanShutdown = PR_FALSE;
    connection_->fKeepAliveReservation = PR_FALSE;
    connection_->fEventDriven = PR_FALSE;
    connection_->fHttp2 = PR_TRUE;   // labels the request's protocol in perfdump
    connection_->eventLoop = NULL;
    connection_->eventHandler = NULL;
    connection_->remoteAddress = parentConn->remoteAddress;
    connection_->localAddress = parentConn->localAddress;
    connection_->remoteIP = parentConn->remoteIP;
    connection_->lsConfig = parentConn->lsConfig;
    if (connection_->lsConfig)
        connection_->lsConfig->ref();
    connection_->sslconfig = parentConn->sslconfig;
    connection_->connQueue = NULL;
    connection_->next = NULL;
    connection_->keepAliveTimeout = parentConn->keepAliveTimeout;

    streamSocket_ = H2StreamSocket::create(this);
    if (!streamSocket_)
        return PR_FAILURE;

    return PR_SUCCESS;
}

void H2Stream::setMethod(const uint8_t* value, size_t len)
{
    method_.assign(reinterpret_cast<const char*>(value), len);
}

void H2Stream::setAuthority(const uint8_t* value, size_t len)
{
    authority_.assign(reinterpret_cast<const char*>(value), len);
}

void H2Stream::setScheme(const uint8_t* value, size_t len)
{
    scheme_.assign(reinterpret_cast<const char*>(value), len);
}

void H2Stream::setPath(const uint8_t* value, size_t len)
{
    path_.assign(reinterpret_cast<const char*>(value), len);
}

void H2Stream::addHeader(const uint8_t* name, size_t nameLen,
                         const uint8_t* value, size_t valueLen)
{
    std::string n(reinterpret_cast<const char*>(name), nameLen);
    std::string v(reinterpret_cast<const char*>(value), valueLen);
    headers_.push_back(n);
    headers_.push_back(v);
}

void H2Stream::appendRequestBody(const uint8_t* data, size_t len)
{
    if (len > 0)
        requestBody_.insert(requestBody_.end(), data, data + len);
}

PRInt32 H2Stream::readRequestBody(void* buf, PRInt32 amount)
{
    if (amount <= 0)
        return 0;
    size_t available = requestBody_.size() - requestBodyPos_;
    if (available == 0) {
        if (requestComplete_)
            return 0; // EOF
        PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
        return -1;
    }
    size_t toCopy = (static_cast<size_t>(amount) < available) ? amount : available;
    memcpy(buf, &requestBody_[requestBodyPos_], toCopy);
    requestBodyPos_ += toCopy;
    return static_cast<PRInt32>(toCopy);
}

PRInt32 H2Stream::writeResponseBody(const void* data, PRInt32 amount)
{
    if (amount <= 0)
        return 0;

    // The SAF pipeline's write path.  This is where the response is split and
    // metered rather than accumulated: feedResponse() may block here until the
    // client has taken delivery of enough of the body to make room, which is
    // exactly the back-pressure an HTTP/1.1 SAF gets from a real socket.
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    if (feedResponse(bytes, static_cast<size_t>(amount)) != PR_SUCCESS) {
        // Report it the way a socket would, so the pipeline unwinds through its
        // existing error handling instead of needing a new path.
        PR_SetError(PR_CONNECT_ABORTED_ERROR, 0);
        return -1;
    }
    return amount;
}

PRInt32 H2Stream::requestBodyAvailable() const
{
    return static_cast<PRInt32>(requestBody_.size() - requestBodyPos_);
}

void H2Stream::markResponseClosed()
{
    responseClosed_ = PR_TRUE;
    finishBody();
}

//
// Response capture, producer side.  Everything from here to pushBody() runs on
// the stream's own worker thread, inside the SAF pipeline.
//

PRStatus H2Stream::feedResponse(const uint8_t* data, size_t len)
{
    if (!headersReady_) {
        // Look for the blank line that ends the head, and append only as far as
        // that.  A SAF may hand over the head and a multi-megabyte body in a
        // single write, so pushing the whole write through the header accumulator
        // would defeat the point of bounding it -- and would trip the bound.
        //
        // The terminator can straddle the boundary between what has already been
        // accumulated and what has just arrived (the SAF may split anywhere, even
        // between a CR and its LF), so the search window starts up to three bytes
        // back into responseHead_.
        size_t carry = (responseHead_.size() < 3) ? responseHead_.size() : 3;
        size_t scan = (len < MAX_RESPONSE_HEAD_BYTES) ? len
                                                      : MAX_RESPONSE_HEAD_BYTES;
        std::string window(responseHead_, responseHead_.size() - carry, carry);
        window.append(reinterpret_cast<const char*>(data), scan);

        size_t hit = window.find("\r\n\r\n");
        if (hit == std::string::npos) {
            // Check the bound before growing, so that a response which never
            // produces a blank line is rejected rather than buffered.
            if (responseHead_.size() + len > MAX_RESPONSE_HEAD_BYTES) {
                ereport(LOG_WARN,
                        "H2Stream[%u]: response headers exceed %u bytes",
                        streamId_, (unsigned)MAX_RESPONSE_HEAD_BYTES);
                return PR_FAILURE;
            }
            responseHead_.append(reinterpret_cast<const char*>(data), len);
            return PR_SUCCESS;              // not yet; wait for more
        }

        // Convert the window-relative hit into a count of bytes from this write
        // that belong to the head.  At least one of them does: a terminator
        // entirely inside the carry would have been found on the previous call.
        size_t consumed = hit + 4 - carry;
        PR_ASSERT(consumed >= 1 && consumed <= len);

        // The blank line stays in responseHead_, so the header parser has its
        // terminator.
        responseHead_.append(reinterpret_cast<const char*>(data), consumed);
        data += consumed;
        len -= consumed;

        if (parseResponseHead() != PR_SUCCESS) {
            ereport(LOG_WARN,
                    "H2Stream[%u]: failed to parse HTTP/1.1 response",
                    streamId_);
            return PR_FAILURE;
        }
        headersReady_ = PR_TRUE;
        if (chunkedResponse_)
            chunkState_ = CHUNK_SIZE;

        if (len == 0) {
            // The head arrived on its own, which means the producer flushed its
            // headers before it had anything else to say -- the shape of an SSE
            // or long-poll response.  Get them out now; otherwise the client sees
            // no response at all until the first body write, which may be minutes
            // away.  When body bytes follow in this same write, feedBody() does
            // the push and doing it here as well would only duplicate it.
            if (session_ && !session_->pushStreamBody(this))
                return PR_FAILURE;
            return PR_SUCCESS;
        }
    }

    return feedBody(data, len);
}

PRStatus H2Stream::feedBody(const uint8_t* data, size_t len)
{
    if (method_ == "HEAD") {
        // A HEAD response carries the headers of the equivalent GET but no body.
        // The SAF may well have produced one anyway; drop it here rather than
        // buffering bytes that will never be framed.
        return PR_SUCCESS;
    }

    PRStatus rv = chunkedResponse_ ? feedChunked(data, len)
                                   : pushBody(data, len);
    if (rv != PR_SUCCESS)
        return rv;

    // Every write that reaches this layer is one HTTP/1.1 would have put on the
    // wire.  The HTTP filter buffers small body writes in its own response buffer
    // and only hands them down on a flush, on a full buffer, or once a
    // long-running program has had its buffering switched off -- so the
    // coalescing has already happened above us.  Holding these bytes back until
    // the ring fills or the pipeline finishes would turn a streaming response
    // into a batched one.  A push takes one driver turn and never waits.
    if (session_ && !session_->pushStreamBody(this))
        return PR_FAILURE;

    return PR_SUCCESS;
}

PRStatus H2Stream::feedChunked(const uint8_t* data, size_t len)
{
    // Incremental equivalent of the old whole-buffer chunked decoder.  State
    // lives in chunkState_/chunkLine_/chunkRemaining_ across calls, because any
    // of these fields can be split between two SAF writes.
    while (len > 0) {
        switch (chunkState_) {
        case CHUNK_SIZE:
            while (len > 0 && chunkState_ == CHUNK_SIZE) {
                char c = static_cast<char>(*data++);
                --len;
                if (c != '\n') {
                    chunkLine_.push_back(c);
                    if (chunkLine_.size() > MAX_CHUNK_LINE_BYTES)
                        return PR_FAILURE;
                    continue;
                }

                std::string line(chunkLine_);
                chunkLine_.clear();
                if (!line.empty() && line[line.size() - 1] == '\r')
                    line.resize(line.size() - 1);
                size_t semi = line.find(';');
                if (semi != std::string::npos)
                    line.resize(semi);
                char* endPtr = NULL;
                unsigned long chunkSize = strtoul(line.c_str(), &endPtr, 16);
                if (endPtr == line.c_str())
                    return PR_FAILURE;
                chunkRemaining_ = chunkSize;
                chunkState_ = (chunkSize == 0) ? CHUNK_TRAILER : CHUNK_DATA;
            }
            break;

        case CHUNK_DATA: {
            size_t n = (len < chunkRemaining_) ? len : chunkRemaining_;
            if (pushBody(data, n) != PR_SUCCESS)
                return PR_FAILURE;
            data += n;
            len -= n;
            chunkRemaining_ -= n;
            if (chunkRemaining_ == 0)
                chunkState_ = CHUNK_CRLF;
            break;
        }

        case CHUNK_CRLF:
            // The CRLF that terminates the chunk payload; tolerate a split.
            while (len > 0 && chunkState_ == CHUNK_CRLF) {
                char c = static_cast<char>(*data++);
                --len;
                if (c == '\n')
                    chunkState_ = CHUNK_SIZE;
            }
            break;

        case CHUNK_TRAILER:
            // Trailers, up to the terminating empty line.  They are not
            // forwarded: HTTP/2 trailers would need a second HEADERS frame after
            // the DATA, and nothing in the SAF pipeline emits any.
            while (len > 0 && chunkState_ == CHUNK_TRAILER) {
                char c = static_cast<char>(*data++);
                --len;
                if (c != '\n') {
                    chunkLine_.push_back(c);
                    if (chunkLine_.size() > MAX_CHUNK_LINE_BYTES)
                        return PR_FAILURE;
                    continue;
                }
                PRBool blank = (chunkLine_.empty() || chunkLine_ == "\r");
                chunkLine_.clear();
                if (blank)
                    chunkState_ = CHUNK_DONE;
            }
            break;

        case CHUNK_DONE:
        default:
            // Anything after the last chunk is not part of the body.
            return PR_SUCCESS;
        }
    }
    return PR_SUCCESS;
}

size_t H2Stream::ringWriteLocked(const uint8_t* data, size_t len)
{
    if (ring_.empty())
        ring_.resize(RESPONSE_RING_CAPACITY);

    size_t capacity = ring_.size();
    size_t space = capacity - ringCount_;
    if (len > space)
        len = space;

    // Up to two copies: from the write cursor to the end of the buffer, then
    // wrapped around to the front.
    size_t tail = (ringHead_ + ringCount_) % capacity;
    size_t first = capacity - tail;
    if (first > len)
        first = len;
    memcpy(&ring_[tail], data, first);
    if (len > first)
        memcpy(&ring_[0], data + first, len - first);

    ringCount_ += len;
    return len;
}

PRStatus H2Stream::pushBody(const uint8_t* data, size_t len)
{
    for (;;) {
        PRBool flush;

        XP_Lock(&bodyLock_);
        if (bodyAbort_) {
            XP_Unlock(&bodyLock_);
            return PR_FAILURE;
        }
        if (bodyDone_) {
            // The body was closed out at its declared length, so this write is
            // past the end of the response and has nowhere to go.  Report success
            // rather than failure: the response the peer asked for was delivered in
            // full, and failing the write would unwind the pipeline as though the
            // client had vanished, which is the opposite of what happened.  An
            // HTTP/1.1 client would ignore these bytes for the same reason.
            XP_Unlock(&bodyLock_);
            ereport(LOG_VERBOSE,
                    "H2Stream[%u]: discarding %u byte(s) written past the "
                    "declared response length", streamId_, (unsigned)len);
            return PR_SUCCESS;
        }
        size_t wrote = ringWriteLocked(data, len);
        // Two reasons to get bytes onto the wire before returning to the SAF:
        // the ring could not take everything, so the consumer has to make room;
        // or the data provider ran dry and deferred, and nghttp2 will not ask
        // this stream for data again until it is explicitly resumed.
        flush = (wrote < len) || needResume_;
        XP_Unlock(&bodyLock_);

        data += wrote;
        len -= wrote;

        if (!flush)
            return PR_SUCCESS;
        if (!session_ || !session_->flushStreamBody(this))
            return PR_FAILURE;
        if (len == 0)
            return PR_SUCCESS;
    }
}

void H2Stream::finishBody(void)
{
    XP_Lock(&bodyLock_);
    bodyEof_ = PR_TRUE;
    XP_Broadcast(&bodySpace_);
    XP_Unlock(&bodyLock_);
}

void H2Stream::abortBody(void)
{
    XP_Lock(&bodyLock_);
    bodyAbort_ = PR_TRUE;
    // Whatever is buffered will never be sent, and a producer blocked waiting for
    // room has to be released so its pipeline can unwind.
    ringCount_ = 0;
    ringHead_ = 0;
    XP_Broadcast(&bodySpace_);
    XP_Unlock(&bodyLock_);
}

PRBool H2Stream::bodyHasSpace(void)
{
    XP_Lock(&bodyLock_);
    PRBool space = (ring_.empty() || ringCount_ < ring_.size());
    XP_Unlock(&bodyLock_);
    return space;
}

PRBool H2Stream::bodyAborted(void)
{
    XP_Lock(&bodyLock_);
    PRBool aborted = bodyAbort_;
    XP_Unlock(&bodyLock_);
    return aborted;
}

PRBool H2Stream::waitForBodySpace(XPInterval timeout)
{
    XP_Lock(&bodyLock_);
    if (!bodyAbort_ && !ring_.empty() && ringCount_ >= ring_.size())
        XP_Wait(&bodySpace_, timeout, NULL);
    PRBool ready = (bodyAbort_ || ring_.empty() || ringCount_ < ring_.size());
    XP_Unlock(&bodyLock_);
    return ready;
}

//
// Response body, consumer side.  Runs under the session's driver token, inside
// nghttp2's data-provider callback.
//

PRInt32 H2Stream::readBodyForProvider(uint8_t* out, size_t len,
                                      PRBool& eof, PRBool& deferred)
{
    eof = PR_FALSE;
    deferred = PR_FALSE;

    XP_Lock(&bodyLock_);

    if (ringCount_ == 0 || len == 0) {
        if (bodyEof_ || bodyAbort_ || bodyLengthReachedLocked()) {
            eof = PR_TRUE;
        } else {
            // Nothing buffered and the producer has not finished.  Deferring is
            // the only correct answer: returning 0 without EOF would end the
            // response body here, truncating it.  nghttp2 will not call back
            // again until the stream is resumed, so record the debt.
            needResume_ = PR_TRUE;
            deferred = PR_TRUE;
        }
        if (eof)
            bodyDone_ = PR_TRUE;
        XP_Unlock(&bodyLock_);
        return 0;
    }

    size_t n = (len < ringCount_) ? len : ringCount_;
    // Hand out one contiguous run; nghttp2 comes back for the wrapped remainder.
    size_t untilWrap = ring_.size() - ringHead_;
    if (n > untilWrap)
        n = untilWrap;
    memcpy(out, &ring_[ringHead_], n);
    ringHead_ = (ringHead_ + n) % ring_.size();
    ringCount_ -= n;
    bodySent_ += n;

    // Either the producer has said this is the last of it, or the response
    // declared a length and this is the byte that reaches it.  The second test is
    // what puts END_STREAM on the frame that carries the last byte instead of on
    // an empty frame some time later.
    if (ringCount_ == 0 && (bodyEof_ || bodyLengthReachedLocked()))
        eof = PR_TRUE;
    if (eof)
        bodyDone_ = PR_TRUE;

    // Room has appeared; wake a producer blocked at the high-water mark.
    XP_Broadcast(&bodySpace_);
    XP_Unlock(&bodyLock_);
    return static_cast<PRInt32>(n);
}

PRBool H2Stream::bodyLengthReachedLocked(void) const
{
    return (responseLength_ >= 0 &&
            bodySent_ >= static_cast<PRUint64>(responseLength_));
}

PRBool H2Stream::bodyDelivered(void)
{
    XP_Lock(&bodyLock_);
    PRBool done = bodyDone_;
    XP_Unlock(&bodyLock_);
    return done;
}

PRBool H2Stream::takeNeedResume(void)
{
    XP_Lock(&bodyLock_);
    PRBool owed = needResume_;
    needResume_ = PR_FALSE;
    XP_Unlock(&bodyLock_);
    return owed;
}

PRBool H2Stream::resumePending(void)
{
    XP_Lock(&bodyLock_);
    PRBool owed = needResume_;
    XP_Unlock(&bodyLock_);
    return owed;
}

PRStatus H2Stream::getRemoteAddress(PRNetAddr* addr) const
{
    if (!connection_ || !addr)
        return PR_FAILURE;
    *addr = connection_->remoteAddress;
    return PR_SUCCESS;
}

PRStatus H2Stream::getLocalAddress(PRNetAddr* addr) const
{
    if (!connection_ || !addr)
        return PR_FAILURE;
    *addr = connection_->localAddress;
    return PR_SUCCESS;
}

PRStatus H2Stream::buildHttp11Request(std::string& out)
{
    if (method_.empty() || path_.empty())
        return PR_FAILURE;

    out.reserve(512 + requestBody_.size());
    out.append(method_);
    out.append(" ");
    out.append(path_);
    out.append(" HTTP/1.1\r\n");

    if (!authority_.empty()) {
        out.append("Host: ");
        out.append(authority_);
        out.append("\r\n");
    }

    // Force a single request/response cycle for this logical stream; the
    // HTTP/2 framing layer handles multiplexing.
    out.append("Connection: close\r\n");

    // Provide an explicit Content-Length so the HTTP/1.1 parser does not
    // block waiting for a body that will never arrive on this GET path.
    if (requestBody_.empty()) {
        out.append("Content-Length: 0\r\n");
    }

    // Pass through regular headers, suppressing hop-by-hop fields and the ones
    // this function generates itself, and rejoining any field that HTTP/2 allows
    // to arrive split (see coalesce_delimiter) into a single line.
    //
    // The rejoin has to happen here.  Left as separate lines, the pieces reach
    // heliod's own HTTP/1.1 parser as duplicate headers and it folds them with
    // ", " -- right for a list-valued field, wrong for Cookie, whose RFC 6265
    // delimiter is ";".  The application would then parse a single cookie whose
    // value had swallowed all the others, and nothing downstream could undo it: a
    // cookie value may itself contain a comma, so the folded string is genuinely
    // ambiguous.  (This is what silently logged phpBB users out -- the session id
    // and the "keep me logged in" key were the cookies that disappeared.)
    for (size_t i = 0; i + 1 < headers_.size(); i += 2) {
        std::string lowerName = headers_[i];
        to_lower(lowerName);
        if (is_hop_by_hop(lowerName) || is_synthesized_header(lowerName))
            continue;

        // A coalesced field goes out once, at the position its first piece held, so
        // the synthesized request keeps the client's header order.  Its remaining
        // pieces are gathered onto that line here, then skipped when their own turn
        // comes round.
        const char* delimiter = coalesce_delimiter(lowerName);
        if (delimiter && header_seen_before(headers_, i, lowerName))
            continue;

        out.append(headers_[i]);
        out.append(": ");
        out.append(headers_[i + 1]);

        if (delimiter) {
            for (size_t j = i + 2; j + 1 < headers_.size(); j += 2) {
                std::string otherName = headers_[j];
                to_lower(otherName);
                if (otherName != lowerName)
                    continue;
                out.append(delimiter);
                out.append(headers_[j + 1]);
            }
        }

        out.append("\r\n");
    }

    // The one Content-Length for a request that has a body.  It has to go out
    // exactly once: heliod itself tolerates the duplicate (it folds the two
    // values to "N, N" and the body-length atoi() stops at the comma), but the
    // folded string is what reaches a CGI or FastCGI application as
    // CONTENT_LENGTH, and php-fpm is handed "27019, 27019" verbatim.
    if (!requestBody_.empty()) {
        char lenBuf[32];
        snprintf(lenBuf, sizeof(lenBuf), "%zu", requestBody_.size());
        out.append("Content-Length: ");
        out.append(lenBuf);
        out.append("\r\n");
    }

    out.append("\r\n");
    if (!requestBody_.empty()) {
        out.append(reinterpret_cast<const char*>(&requestBody_[0]),
                   requestBody_.size());
    }
    return PR_SUCCESS;
}

PRStatus H2Stream::dispatchRequest()
{
    if (!connection_ || !streamSocket_) {
        ereport(LOG_WARN, "H2Stream[%u]: missing connection or socket",
                streamId_);
        return PR_FAILURE;
    }

    std::string requestText;
    if (buildHttp11Request(requestText) != PR_SUCCESS) {
        ereport(LOG_WARN, "H2Stream[%u]: failed to build HTTP/1.1 request",
                streamId_);
        return PR_FAILURE;
    }

    // Use the persistent per-thread DaemonSession/HttpRequest.  The request
    // machinery caches thread-private state that must outlive this stream;
    // a transient DaemonSession here would leave dangling thread-privates
    // and crash the next request processed on this worker thread.
    DaemonSession* sessionPtr = DaemonSession::GetWorkerSession();
    if (!sessionPtr)
        return PR_FAILURE;
    DaemonSession& session = *sessionPtr;

    // Mark the thread pool so all per-stream allocations can be released.
    pool_handle_t* pool = session.pool_;
    void* poolMark = pool_mark(pool);

    netbuf* buf = netbuf_open(streamSocket_,
                              static_cast<int>(requestText.size()));
    if (!buf) {
        ereport(LOG_WARN, "H2Stream[%u]: netbuf_open failed", streamId_);
        pool_recycle(pool, poolMark);
        return PR_FAILURE;
    }

    if (!requestText.empty()) {
        size_t reqSize = requestText.size();
        buf->inbuf = (unsigned char*) MALLOC(reqSize + 1);
        memcpy(buf->inbuf, requestText.data(), reqSize);
        buf->inbuf[reqSize] = '\0';
    }
    buf->pos = 0;
    buf->cursize = static_cast<int>(requestText.size());
    buf->maxsize = static_cast<int>(requestText.size() + 1);

    Connection* oldConn = session.conn;
    session.conn = connection_;
    connection_->fd = streamSocket_;

    // This worker session never ran prepareConnection() for the synthetic
    // per-stream connection, so its serverIP/serverPort are unset.  Populate
    // them from the borrowed connection's addresses so SAFs that read the
    // server address (e.g. FastCGI's SERVER_ADDR, access logging) work and do
    // not dereference a NULL serverIP.
    session.SetServerAddrFromConn();

    // prepareConnection() is also where a connection's peer address reaches the
    // thread's realtime stats slot, so the same gap left perfdump's (and
    // stats-xml's) "Client" column showing whatever address this worker last
    // recorded: 0.0.0.0 on a worker that has never prepared a connection of its
    // own, or the peer of some unrelated earlier connection it did prepare --
    // while "Reported Client", which HandleRequest() maintains per request, was
    // correct.  Record the stream's peer (copied from the real connection in
    // init()) here.  This also stamps the request start time the "Age" column
    // measures; StartSession()'s beginRequest() would otherwise stamp it a
    // moment later, so no timing is lost.
    session.beginClient(&connection_->remoteAddress);

    PRBool ok = PR_FALSE;
    if (session.request_->StartSession(buf) == PR_TRUE) {
        PRIntervalTime timeout = DaemonSession::GetIOTimeout();
        session.request_->HandleRequest(buf, timeout);
        ok = PR_TRUE;
    } else {
        ereport(LOG_WARN, "H2Stream[%u]: StartSession failed", streamId_);
    }

    session.conn = oldConn;

    // The pipeline left this thread's stats slot in whatever mode the request
    // reached -- "processing", or "response" once httpact started sending.
    // endProcessing() clears the slot's VS/method/URI/age but deliberately not
    // its mode, because in the classic model run()'s next iteration resets that
    // to idle.  A worker-pool thread has no such loop, so without this the slot
    // keeps a stale busy mode for the life of the thread and perfdump lists the
    // thread as a live session with every other column blank.  One such row per
    // worker that has ever served an HTTP/2 stream, which is what a Sessions
    // list full of half-empty rows on a busy H2 instance actually is.
    session.setMode(STATS_THREAD_IDLE);

    // Free the netbuf and roll the thread pool back to the mark taken at
    // stream entry so per-stream allocations don't accumulate.
    netbuf_close(buf);
    pool_recycle(pool, poolMark);

    return ok ? PR_SUCCESS : PR_FAILURE;
}

void H2Stream::runJob(void)
{
    // The expensive part of serving a stream, run on a worker of its own.  No
    // session state is touched here: the response is captured into this stream's
    // own buffers, and framing it happens later on whichever thread holds the
    // session's driver token.
    dispatchOk_ = (dispatchRequest() == PR_SUCCESS);
    if (!dispatchOk_) {
        ereport(LOG_WARN, "H2Stream[%u]: dispatch failed", streamId_);
    }

    // Whatever the pipeline did or did not do, it is finished producing.  Say so
    // unconditionally: the data provider blocks the stream while it believes more
    // body is coming, so a pipeline that returned without closing its socket --
    // or that failed halfway through -- would otherwise leave the response hanging
    // one DATA frame short of complete.
    finishBody();

    // Reporting completion transfers this object back to the session, which may
    // free it immediately (if the peer reset the stream while we were running)
    // or frame its response on this very thread.  Either way it is not ours any
    // more, so read the session pointer out first and touch nothing else.
    H2Session* session = session_;
    if (session)
        session->streamFinished(this);
}

extern "C" ssize_t h2_stream_data_provider_read_cb(
    nghttp2_session* /*session*/,
    int32_t /*stream_id*/,
    uint8_t* outBuf,
    size_t length,
    uint32_t* data_flags,
    nghttp2_data_source* source,
    void* /*user_data*/)
{
    heliod::H2Stream* stream = static_cast<heliod::H2Stream*>(source->ptr);
    if (!stream)
        return NGHTTP2_ERR_CALLBACK_FAILURE;

    PRBool eof = PR_FALSE;
    PRBool deferred = PR_FALSE;
    PRInt32 n = stream->readBodyForProvider(outBuf, length, eof, deferred);

    if (deferred) {
        // The producer has not caught up.  Telling nghttp2 to hold this stream is
        // the only safe answer; returning 0 would mark the body complete.  The
        // stream is resumed once bytes arrive -- see H2Stream::pushBody() and
        // H2Session::resumeDeferredStreams().
        return NGHTTP2_ERR_DEFERRED;
    }
    if (eof)
        *data_flags = NGHTTP2_DATA_FLAG_EOF;
    return static_cast<ssize_t>(n);
}

PRStatus H2Stream::submitResponse(struct nghttp2_session* session)
{
    // The head has already been parsed, incrementally, as the SAF produced it --
    // which is what allows this to be called while the body is still arriving.
    if (!session || !headersReady_)
        return PR_FAILURE;

    int statusCode = statusCode_;

    responseNva_.clear();
    responseNva_.reserve(responseHeaders_.size() + 1);

    statusString_.clear();
    char statusBuf[4];
    snprintf(statusBuf, sizeof(statusBuf), "%d", statusCode);
    statusString_ = statusBuf;
    responseNva_.push_back(make_nv(":status", 7, statusString_.c_str(),
                                   statusString_.size()));

    for (size_t i = 0; i < responseHeaders_.size(); ++i) {
        const std::string& name = responseHeaders_[i].first;
        const std::string& value = responseHeaders_[i].second;
        responseNva_.push_back(make_nv(name.c_str(), name.size(),
                                       value.c_str(), value.size()));
    }

    dataProvider_.source.ptr = this;
    dataProvider_.read_callback = h2_stream_data_provider_read_cb;

    int rv = nghttp2_submit_response(session, streamId_,
                                     &responseNva_[0], responseNva_.size(),
                                     &dataProvider_);
    if (rv != 0) {
        ereport(LOG_WARN,
                "H2Stream[%u]: nghttp2_submit_response failed: %d",
                streamId_, rv);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PRStatus H2Stream::parseResponseHead(void)
{
    responseHeaders_.clear();
    statusCode_ = 200;
    chunkedResponse_ = PR_FALSE;
    responseLength_ = -1;

    if (responseHead_.empty())
        return PR_FAILURE;

    const uint8_t* data = reinterpret_cast<const uint8_t*>(responseHead_.data());
    size_t len = responseHead_.size();

    // Find the end of the status line.
    size_t lineEnd = 0;
    if (!find_crlf(data, len, 0, lineEnd))
        return PR_FAILURE;

    // Parse "HTTP/1.x NNN Reason"
    std::string statusLine(reinterpret_cast<const char*>(data), lineEnd);
    size_t sp1 = statusLine.find(' ');
    if (sp1 == std::string::npos)
        return PR_FAILURE;
    size_t sp2 = statusLine.find(' ', sp1 + 1);
    if (sp2 == std::string::npos)
        sp2 = statusLine.size();
    std::string codeStr = statusLine.substr(sp1 + 1, sp2 - sp1 - 1);
    statusCode_ = atoi(codeStr.c_str());
    if (statusCode_ < 100 || statusCode_ > 999)
        statusCode_ = 200;

    size_t pos = lineEnd + 2;

    while (pos + 1 < len) {
        size_t end = 0;
        if (!find_crlf(data, len, pos, end))
            return PR_FAILURE;
        if (end == pos) {
            pos += 2;
            break; // empty line
        }
        size_t colonPos = find_colon(data, end, pos);
        if (colonPos == std::string::npos || colonPos >= end) {
            pos = end + 2;
            continue;
        }
        std::string name(reinterpret_cast<const char*>(data + pos),
                         colonPos - pos);
        to_lower(name);
        size_t valueStart = colonPos + 1;
        while (valueStart < end && data[valueStart] == ' ')
            ++valueStart;
        std::string value(reinterpret_cast<const char*>(data + valueStart),
                          end - valueStart);
        if (name == "transfer-encoding" && value == "chunked")
            chunkedResponse_ = PR_TRUE;

        // Remember the declared body length: it is what lets the last DATA frame
        // be recognised as it goes out rather than after the pipeline has unwound.
        // Taken as a whole number of bytes only -- anything malformed leaves the
        // length unknown, which simply falls back to waiting for the pipeline.
        if (name == "content-length") {
            char* endp = NULL;
            long long declared = strtoll(value.c_str(), &endp, 10);
            if (endp && *endp == '\0' && declared >= 0)
                responseLength_ = declared;
        }

        // Hop-by-hop headers are dropped, which also disposes of
        // transfer-encoding: the body is de-chunked on the way through, so
        // advertising chunked framing to an HTTP/2 peer would be wrong (and
        // HTTP/2 forbids it).  A content-length that came with the response still
        // describes the bytes we forward, so it is passed through unchanged.
        if (!is_hop_by_hop(name)) {
            responseHeaders_.push_back(std::make_pair(name, value));
        }
        pos = end + 2;
    }

    // A chunked response describes its own end, and the body is de-chunked on the
    // way through, so any content-length that came with it does not describe the
    // bytes this stream hands over.
    if (chunkedResponse_)
        responseLength_ = -1;

    return PR_SUCCESS;
}

} // namespace heliod
