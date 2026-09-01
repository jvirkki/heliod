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

#ifndef _H2_STREAM_H_
#define _H2_STREAM_H_

#include "nspr.h"
#include "base/pool.h"
#include "xp/xpsynch.h"
#include "httpdaemon/connqueue.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/WorkerPool.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include <nghttp2/nghttp2.h>

namespace heliod {

class H2Session;

/**
 * State for one HTTP/2 logical stream.
 *
 * Each stream is mapped to a transient HTTP/1.1 request/response cycle by
 * running the existing NSAPI/SAF pipeline against a synthetic HTTP/1.1 request
 * and capturing the raw response.  The HTTP/2 framing layer then rewrites the
 * captured response as HEADERS + DATA frames.
 *
 * A stream is also the unit of parallelism.  It is a WorkerJob, so once its
 * request is complete the owning H2Session hands it to the worker pool and the
 * SAF pipeline runs on a thread of its own; concurrent streams on one connection
 * therefore execute concurrently.  Everything the pipeline touches is
 * per-stream (its own synthetic Connection, its own H2StreamSocket) or
 * per-thread (the worker's DaemonSession), which is what makes that safe.  A
 * stream never touches the shared nghttp2 session or the real socket: framing
 * the response is done by whichever thread holds the session's driver token.
 * See the ownership rules on H2Session.
 *
 * The response is *not* accumulated whole.  The SAF pipeline writes the status
 * line, headers and body into writeResponseBody() as one byte stream; the stream
 * splits that stream as it arrives, parses the head as soon as the blank line
 * shows up, and passes body bytes through a bounded ring buffer that the nghttp2
 * data provider drains.  Two threads are therefore involved in the response:
 *
 *   producer  the stream's own worker, inside the SAF pipeline.  Appends to the
 *             ring and blocks at the high-water mark, which is where the flow
 *             control belongs: the pipeline is what has to be slowed down.
 *   consumer  whichever thread holds the session's driver token, from inside
 *             nghttp2's data-provider callback.  Drains the ring and signals.
 *
 * bodyLock_ guards everything they share.  Because a blocked producer cannot
 * rely on any other thread arriving to drain for it -- a connection with a
 * stream in flight is deliberately not parked in the event loop -- the producer
 * takes the driver token and pushes the bytes out itself.  See
 * H2Session::flushStreamBody().
 */
class H2Stream : public WorkerJob {
public:
    H2Stream(uint32_t streamId, H2Session* session);
    ~H2Stream();

    uint32_t streamId() const { return streamId_; }

    /**
     * WorkerJob: run this stream's SAF pipeline, then report back to the
     * session.  Runs on a worker thread.  The session may free this object as
     * soon as the report is made, so nothing is touched afterwards.
     */
    virtual void runJob(void);

    /**
     * Initialize the per-stream state.  Must be called before the first
     * pseudo-header is stored.  `parentConn` provides connection-level
     * addresses and listener configuration.
     */
    PRStatus init(Connection* parentConn);

    void setMethod(const uint8_t* value, size_t len);
    void setAuthority(const uint8_t* value, size_t len);
    void setScheme(const uint8_t* value, size_t len);
    void setPath(const uint8_t* value, size_t len);
    void addHeader(const uint8_t* name, size_t nameLen,
                   const uint8_t* value, size_t valueLen);

    void appendRequestBody(const uint8_t* data, size_t len);

    /** How much request body is buffered so far.  The session checks this against
     *  the per-stream cap before appending, because a stream's body is held whole
     *  until END_STREAM and so nothing else bounds it. */
    size_t requestBodyBuffered() const { return requestBody_.size(); }

    /** Whether this stream's request body has already been refused for exceeding
     *  the cap.  RST_STREAM does not stop DATA already in flight, so the frames
     *  after the refusal arrive and are dropped too -- but only the first of them
     *  is worth a log line.  A refused 1 GB upload would otherwise write one
     *  warning per 16 KB frame. */
    PRBool requestBodyRefused() const { return requestBodyRefused_; }
    void markRequestBodyRefused() { requestBodyRefused_ = PR_TRUE; }

    PRInt32 readRequestBody(void* buf, PRInt32 amount);
    PRInt32 writeResponseBody(const void* data, PRInt32 amount);
    PRInt32 requestBodyAvailable() const;

    void markResponseClosed();

    PRStatus getRemoteAddress(PRNetAddr* addr) const;
    PRStatus getLocalAddress(PRNetAddr* addr) const;

    /**
     * Build a synthetic HTTP/1.1 request and run the SAF pipeline against it.
     * The response is captured through writeResponseBody() as it is produced, so
     * this may block waiting for the client to keep up.  Returns PR_SUCCESS if
     * the pipeline completed.
     */
    PRStatus dispatchRequest();

    /**
     * Submit the response to nghttp2 as HTTP/2 HEADERS plus a DATA stream fed by
     * the ring buffer.  Called once, by the driver, as soon as headersReady() is
     * true -- which may be well before the body is finished.
     */
    PRStatus submitResponse(struct nghttp2_session* session);

    PRBool responseClosed() const { return responseClosed_; }
    PRBool requestComplete() const { return requestComplete_; }

    /**
     * True once the response status line and headers have been parsed out of the
     * SAF's output, so the response can be submitted.  Body bytes may still be
     * arriving.
     */
    PRBool headersReady() const { return headersReady_; }

    // Response body flow control.  All of these lock internally.

    /** Consumer side: copy up to `len` body bytes out for a DATA frame.
     *  Sets `eof` when this is the last of the body, or `deferred` when there is
     *  nothing buffered yet and the producer has not finished. */
    PRInt32 readBodyForProvider(uint8_t* out, size_t len,
                                PRBool& eof, PRBool& deferred);

    /** Test-and-clear: did the data provider defer since the last check?  If so
     *  the driver owes the stream an nghttp2_session_resume_data(). */
    PRBool takeNeedResume(void);

    /** Peek at the same flag without clearing it.  The producer uses this to tell
     *  "my bytes are buffered and nghttp2 knows about them" from "my bytes are
     *  buffered and nghttp2 has stopped asking", which needs a resume to unstick
     *  even though the producer is not itself blocked. */
    PRBool resumePending(void);

    /** Producer side: is there room in the ring right now? */
    PRBool bodyHasSpace(void);

    /** Producer side: has the session given up on this stream?  A blocked
     *  producer must unwind rather than wait for a drain that will never come. */
    PRBool bodyAborted(void);

    /** Whether the whole response body has been handed to nghttp2 with
     *  END_STREAM.  A stream that closes in this state has been answered in full,
     *  however the close came about, so nothing about it is an error -- which is
     *  what distinguishes a peer tidying up after a completed response from a peer
     *  cancelling one that was still being written. */
    PRBool bodyDelivered(void);

    /** Wait up to `timeout` nanoseconds for the consumer to make room.  Returns
     *  whether there is room (or the stream was aborted) on return. */
    PRBool waitForBodySpace(XPInterval timeout);

    /** Tear down the response body path: unblock the producer and make every
     *  subsequent write fail.  Called by the session when the stream or the
     *  whole connection is going away. */
    void abortBody(void);

    /** The SAF pipeline has finished producing; whatever is in the ring is the
     *  last of the body. */
    void finishBody(void);

    /**
     * The peer has finished sending this request (END_STREAM seen).  Must be set
     * before the pipeline runs, so that a SAF reading the request body sees a
     * clean EOF at the end of it rather than a would-block.
     */
    void markRequestComplete() { requestComplete_ = PR_TRUE; }

    /** Whether runJob()'s call to dispatchRequest() succeeded. */
    PRBool dispatchSucceeded() const { return dispatchOk_; }

    /**
     * Whether init() failed specifically because the process-wide stream pool
     * was at its cap, as opposed to any other init failure.  Valid to read after
     * init() returns PR_FAILURE; lets the session refuse the stream with
     * RST_STREAM(REFUSED_STREAM) -- "try elsewhere" -- rather than INTERNAL_ERROR.
     */
    PRBool poolExhausted() const { return poolExhausted_; }

    // Scheduling state.  Written by the owning H2Session under its lock, never
    // by the stream itself, so these are plain flags rather than atomics.
    //
    //  - inFlight: a worker is executing this stream right now, so it must not
    //    be freed.
    //  - abandoned: the stream was closed (peer RST_STREAM, or session
    //    teardown) while in flight, so the worker that is executing it owns the
    //    free and there is no response to frame.
    //  - submitQueued: the stream is sitting in readyToSubmit_ right now.
    //  - submitted: nghttp2 has been given the response head.
    //
    //    Both are needed because a response is framed as soon as its headers have
    //    been parsed, which is generally before its body is finished, and the
    //    stream can be queued from more than one place (a full ring, or the
    //    pipeline finishing).  submitQueued keeps queueing idempotent; submitted
    //    distinguishes "still needs framing" from "already framing, but the
    //    driver may still have something to say about it".
    void markInFlight() { inFlight_ = PR_TRUE; }
    void clearInFlight() { inFlight_ = PR_FALSE; }
    PRBool inFlight() const { return inFlight_; }
    void markAbandoned() { abandoned_ = PR_TRUE; }
    PRBool abandoned() const { return abandoned_; }
    void markSubmitQueued() { submitQueued_ = PR_TRUE; }
    void clearSubmitQueued() { submitQueued_ = PR_FALSE; }
    PRBool submitQueued() const { return submitQueued_; }
    void markSubmitted() { submitted_ = PR_TRUE; }
    PRBool submitted() const { return submitted_; }

private:
    H2Stream(const H2Stream&);
    H2Stream& operator=(const H2Stream&);

    void appendBuffer(std::vector<uint8_t>& dest,
                      const uint8_t* data, size_t len);

    PRStatus buildHttp11Request(std::string& out);

    /** Parse the accumulated status line and headers out of responseHead_. */
    PRStatus parseResponseHead(void);

    // The response-capture pipeline, producer side.  feedResponse() is the entry
    // point from writeResponseBody(); the rest are the stages it drives.
    PRStatus feedResponse(const uint8_t* data, size_t len);
    PRStatus feedBody(const uint8_t* data, size_t len);
    PRStatus feedChunked(const uint8_t* data, size_t len);
    PRStatus pushBody(const uint8_t* data, size_t len);
    size_t ringWriteLocked(const uint8_t* data, size_t len);

    uint32_t streamId_;

    // The session that owns this stream.  Used only to report completion back
    // from the worker thread; the stream never drives the session itself.
    H2Session* session_;

    PRBool dispatchOk_;
    PRBool inFlight_;
    PRBool abandoned_;
    PRBool submitQueued_;
    PRBool submitted_;
    PRBool poolExhausted_;      // init() failed because the stream pool was full

    // Pseudo-headers
    std::string method_;
    std::string authority_;
    std::string scheme_;
    std::string path_;

    // Regular headers.  Stored as name\0value pairs to simplify nghttp2_nv
    // construction later.
    std::vector<std::string> headers_;

    std::vector<uint8_t> requestBody_;
    size_t requestBodyPos_;
    PRBool requestComplete_;
    PRBool requestBodyRefused_;     // over the cap; body dropped, stream reset

    PRBool responseClosed_;

    // --- Response head capture (producer thread only) ---------------------
    //
    // The SAF writes the status line and headers into the same byte stream as
    // the body, so they are accumulated here until the terminating blank line
    // appears and can then be parsed.  Bounded, because a response that never
    // produces that blank line must not be allowed to grow without limit.
    std::string responseHead_;
    PRBool headersReady_;
    int statusCode_;
    PRBool chunkedResponse_;

    // Incremental chunked-transfer decoder.  Needed because the body is consumed
    // as it arrives, so a chunk size line can be split across two SAF writes.
    enum ChunkState {
        CHUNK_NONE,     // response is not chunked
        CHUNK_SIZE,     // reading the hex size line
        CHUNK_DATA,     // copying chunkRemaining_ bytes of payload
        CHUNK_CRLF,     // consuming the CRLF that follows a chunk
        CHUNK_TRAILER,  // reading trailers up to the final blank line
        CHUNK_DONE
    };
    ChunkState chunkState_;
    std::string chunkLine_;
    size_t chunkRemaining_;

    // --- Response body ring (shared; guarded by bodyLock_) ----------------
    XPLock bodyLock_;
    XPCondition bodySpace_;         // producer waits here for room
    std::vector<uint8_t> ring_;     // allocated on first body byte; size() is
                                    // the capacity, ringCount_ the fill
    size_t ringHead_;               // index of the next byte to hand out
    size_t ringCount_;
    PRBool bodyEof_;                // producer has finished
    PRBool bodyAbort_;              // session is discarding this stream
    PRBool needResume_;             // provider deferred; owes a resume_data()

    // How the end of the body is recognised when the response declared its
    // length, which is the case for every static file and most SAF responses.
    //
    // Waiting for bodyEof_ is not good enough on its own.  It is set when the
    // pipeline finishes, and the last body bytes usually reach the peer before
    // that: the producer's final write fills the ring, flushes it, and only then
    // unwinds.  END_STREAM would follow in a later, empty DATA frame -- and in
    // between, a peer that counts Content-Length has a complete response on a
    // stream that is still open.  An HTTP/2 reverse proxy in that position hands
    // the body to its own client, that client is satisfied and disconnects, and
    // the proxy cancels a stream we were about to close cleanly.  So the last DATA
    // frame has to carry END_STREAM, which means recognising the last byte by
    // count at the moment it is handed over.
    PRInt64 responseLength_;        // Content-Length, or -1 if not declared
    PRUint64 bodySent_;             // body bytes handed to nghttp2 so far
    PRBool bodyDone_;               // END_STREAM has been reported to nghttp2

    /** Whether every declared body byte has been handed over.  bodyLock_ held. */
    PRBool bodyLengthReachedLocked(void) const;

    // Storage for the response header name/value pairs; kept alive while
    // nghttp2_submit_response reads the nghttp2_nv array.
    std::vector<std::pair<std::string, std::string> > responseHeaders_;

    // Holds the :status value across nghttp2_submit_response(); must outlive
    // the call because nghttp2 stores the pointer when using NO_COPY flags.
    std::string statusString_;

    // Response nghttp2_nv array and data provider are stored as members so
    // the pointers passed to nghttp2_submit_response remain valid if nghttp2
    // stores references to them rather than copies (observed as release-only
    // SIGBUS with the SBC nghttp2 build).
    std::vector<nghttp2_nv> responseNva_;
    nghttp2_data_provider dataProvider_;

    Connection* connection_;
    PRFileDesc* streamSocket_;
};

} // namespace heliod

#endif // _H2_STREAM_H_
