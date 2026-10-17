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

/**
 * @file H2Session.cpp
 * @author Joe McCabe
 * @brief HTTP/2 session wrapper around nghttp2
 */

#include "H2Session.h"

#include "connqueue.h"
#include "httpdaemon/H2StreamPool.h"    // initConfig() builds the pool eagerly
#include "httpdaemon/EventLoop.h"
#include "httpdaemon/EventDispatcher.h"
#include "frame/conf.h"                 // conf_getboundedinteger()
#include "base/pblock.h"
#include "libaccess/nsauth.h"
#include "base64.h"

#include <nghttp2/nghttp2.h>

#include <private/pprio.h>
#include <pratom.h>                     // PR_AtomicIncrement() for the counters

#include <algorithm>                    // std::remove() for forgetStreamLocked()
#include <climits>                      // INT_MAX for conf bounds
#include <cstring>
#include <cstdlib>

// Convenience to build an nghttp2_nv from string literals.
#define MAKE_STATIC_NV(N, V) \
    { \
        (uint8_t*)(N), (uint8_t*)(V), \
        sizeof(N) - 1, sizeof(V) - 1, \
        NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE \
    }

namespace {

// A peer that resets, aborts or shuts down the connection has simply gone
// away.  HTTP/2 clients routinely abandon a connection without sending GOAWAY,
// so these are ordinary ends of session and must not be reported as server
// faults; logging them at LOG_WARN fills the error log with client behaviour.
PRBool isPeerGone(PRErrorCode err)
{
    return (err == PR_CONNECT_RESET_ERROR ||
            err == PR_CONNECT_ABORTED_ERROR ||
            err == PR_SOCKET_SHUTDOWN_ERROR ||
            err == PR_NOT_CONNECTED_ERROR);
}

} // anonymous namespace

extern "C" {

static ssize_t h2_send_cb(nghttp2_session* /*session*/,
                          const uint8_t* data,
                          size_t length,
                          int /*flags*/,
                          void* user_data)
{
    heliod::H2Session* h2 = static_cast<heliod::H2Session*>(user_data);
    return h2->writeRaw(data, length);
}

static ssize_t h2_recv_cb(nghttp2_session* /*session*/,
                          uint8_t* buf,
                          size_t length,
                          int /*flags*/,
                          void* user_data)
{
    heliod::H2Session* h2 = static_cast<heliod::H2Session*>(user_data);
    return h2->readRaw(buf, length);
}

static int h2_begin_headers_cb(nghttp2_session* /*session*/,
                               const nghttp2_frame* frame,
                               void* user_data)
{
    heliod::H2Session* h2 = static_cast<heliod::H2Session*>(user_data);
    if (frame->hd.type == NGHTTP2_HEADERS &&
        frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
        h2->createStream(frame->hd.stream_id);
    }
    return 0;
}

static int h2_header_cb(nghttp2_session* /*session*/,
                        const nghttp2_frame* frame,
                        const uint8_t* raw_name,
                        size_t namelen,
                        const uint8_t* raw_value,
                        size_t valuelen,
                        uint8_t /*flags*/,
                        void* user_data)
{
    heliod::H2Session* h2 = static_cast<heliod::H2Session*>(user_data);
    if (frame->hd.type != NGHTTP2_HEADERS ||
        frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
        return 0;
    }

    uint32_t streamId = frame->hd.stream_id;
    if (namelen > 0 && raw_name[0] == ':') {
        if (namelen == 7 && memcmp(raw_name, ":method", 7) == 0) {
            h2->setStreamMethod(streamId, raw_value, valuelen);
        } else if (namelen == 10 && memcmp(raw_name, ":authority", 10) == 0) {
            h2->setStreamAuthority(streamId, raw_value, valuelen);
        } else if (namelen == 7 && memcmp(raw_name, ":scheme", 7) == 0) {
            h2->setStreamScheme(streamId, raw_value, valuelen);
        } else if (namelen == 5 && memcmp(raw_name, ":path", 5) == 0) {
            h2->setStreamPath(streamId, raw_value, valuelen);
        }
    } else {
        h2->addStreamHeader(streamId, raw_name, namelen, raw_value, valuelen);
    }
    return 0;
}

static int h2_frame_recv_cb(nghttp2_session* session,
                            const nghttp2_frame* frame,
                            void* user_data)
{
    heliod::H2Session* h2 = static_cast<heliod::H2Session*>(user_data);
    if (frame->hd.type == NGHTTP2_HEADERS &&
        (frame->headers.cat == NGHTTP2_HCAT_REQUEST ||
         frame->headers.cat == NGHTTP2_HCAT_HEADERS)) {
        // END_STREAM here means the peer has finished the request, so the stream
        // is ready to run.  It rides on the HEADERS frame for a request with no
        // body (HCAT_REQUEST), and on a trailing HEADERS frame -- HTTP/2 trailers
        // -- for a request whose body was followed by trailers (HCAT_HEADERS);
        // without the HCAT_HEADERS case a trailered request would never be
        // dispatched and the client would hang waiting for a response.
        // queueStreamForDispatch() is idempotent (it no-ops once the request is
        // already complete), so the two paths cannot double-queue a stream.
        //
        // Only queue the stream here.  Running the SAF pipeline inside this
        // callback -- which is what used to happen -- serializes every stream on
        // the connection behind the single thread driving nghttp2, which defeats
        // the point of multiplexing.
        if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
            h2->queueStreamForDispatch(frame->hd.stream_id);
        }
    } else if (frame->hd.type == NGHTTP2_DATA) {
        // The end of a request that had a body, and the only place it can be
        // detected.  It is tempting to notice END_STREAM in the data-chunk
        // callback instead -- that callback is handed the enclosing frame's flags
        // -- but nghttp2 only calls it when there are payload bytes to deliver, so
        // a DATA frame carrying END_STREAM and nothing else is invisible there and
        // the stream is never dispatched.  Its own documentation says as much:
        // "you should use nghttp2_on_frame_recv_callback to know all data frames
        // are received".
        //
        // An empty final DATA frame is not a curiosity.  Go's HTTP/2 client sends
        // one for every request whose body is non-nil but of unknown length, and
        // that is precisely what a reverse proxy produces when the request it is
        // forwarding arrived over HTTP/3: quic-go cannot know the length up front,
        // so ContentLength is -1, so net/http/httputil's "no body means nil body"
        // shortcut does not fire and the outbound request keeps an empty body of
        // unknown length.  Over an HTTP/1.1 upstream that becomes an empty chunked
        // body; over h2c it becomes HEADERS without END_STREAM followed by a
        // zero-length DATA frame with it.  Missing this case hung every HTTP/3
        // request through Caddy while leaving HTTP/1.1 and HTTP/2 clients -- whose
        // bodyless requests do get END_STREAM on HEADERS -- working normally.
        if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
            h2->queueStreamForDispatch(frame->hd.stream_id);
        }
    } else if (frame->hd.type == NGHTTP2_PING &&
               (frame->hd.flags & NGHTTP2_FLAG_ACK) == 0) {
        // Some SBC nghttp2 builds do not auto-ack PING frames.
        nghttp2_submit_ping(session, NGHTTP2_FLAG_ACK,
                            frame->ping.opaque_data);
    }
    return 0;
}

static int h2_data_chunk_recv_cb(nghttp2_session* session,
                                 uint8_t /*flags*/,
                                 int32_t stream_id,
                                 const uint8_t* data,
                                 size_t len,
                                 void* user_data)
{
    heliod::H2Session* h2 = static_cast<heliod::H2Session*>(user_data);

    // Nothing replenishes the flow-control windows here.  It used to: this callback
    // submitted a stream-level and a connection-level WINDOW_UPDATE for every DATA
    // payload it saw.  But the session is created with default options, which leave
    // nghttp2's own automatic window update enabled, and nghttp2 credits the same
    // bytes as soon as this callback returns success -- so every request-body byte
    // was credited twice.  A window that grows by two bytes for every byte spent
    // never closes, which is not flow control at all: the peer could send as fast as
    // it liked, and it also cost two WINDOW_UPDATE frames per DATA frame where
    // nghttp2 sends one per half-window.  Returning success is the whole of the
    // application's side of the contract.
    if (!h2->appendStreamRequestBody(stream_id, data, len)) {
        // Over the per-stream request-body cap.  Refuse the stream rather than keep
        // buffering: ENHANCE_YOUR_CALM rather than a 413, because there is no
        // response to put a status on -- the request has not been dispatched and
        // will not be.  Returning success still credits the connection window,
        // which is what the other streams on this connection need.
        nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id,
                                  NGHTTP2_ENHANCE_YOUR_CALM);
        return 0;
    }

    // Deliberately not looking at END_STREAM in `flags` here, even though it is
    // this frame's flags and doing so looks like the obvious way to spot the last
    // DATA frame.  It is not reliable: nghttp2 skips this callback entirely for a
    // DATA frame with no payload bytes, so the end of a request whose final frame
    // is empty would be missed.  h2_frame_recv_cb() sees every DATA frame and runs
    // after every chunk of it has been appended, so that is where the request is
    // declared complete.
    return 0;
}

static int h2_frame_send_cb(nghttp2_session* /*session*/,
                            const nghttp2_frame* frame,
                            void* /*user_data*/)
{
    // Once a response DATA/HEADERS frame has been fully serialized, we could
    // close the stream state here. For now the state is cleaned up when the
    // H2Session is destroyed.
    return 0;
}

static int h2_on_stream_close_cb(nghttp2_session* /*session*/,
                                 int32_t stream_id,
                                 uint32_t error_code,
                                 void* user_data)
{
    heliod::H2Session* h2 = static_cast<heliod::H2Session*>(user_data);
    h2->deleteStream(stream_id, error_code);
    return 0;
}

} // extern "C"

namespace heliod {

/*
 * Live-session accounting, for /perfdump.
 *
 * One H2Session exists per HTTP/2 connection and is destroyed with it, so a live
 * count that keeps climbing while the peer's connection count does not is a
 * leaked session -- the one failure mode that cannot be seen from outside the
 * process, because the socket looks perfectly healthy from the peer's end.  The
 * total is a monotonic counter so the rate can be read from two samples, and the
 * peak survives the burst that caused it.
 *
 * Plain PRInt32 with atomics rather than the shared stats table: these belong to
 * the process serving the request, exactly like the Event/Async pool numbers
 * /perfdump already reports, and they must stay cheap enough to touch on every
 * connection.
 */
static PRInt32 s_liveSessions = 0;
static PRInt32 s_peakSessions = 0;
static PRInt32 s_totalSessions = 0;

PRUint32 H2Session::liveSessions(void)  { return (PRUint32)s_liveSessions; }
PRUint32 H2Session::peakSessions(void)  { return (PRUint32)s_peakSessions; }
PRUint32 H2Session::totalSessions(void) { return (PRUint32)s_totalSessions; }

// Inbound flow-control windows advertised to the peer, per stream and for the
// connection as a whole.  See the note where they are applied in init().
static const uint32_t H2_STREAM_WINDOW     = 1024 * 1024;
static const int32_t  H2_CONNECTION_WINDOW = 4 * 1024 * 1024;

/*
 * Cap on the request body a single HTTP/2 stream will buffer.
 *
 * A stream is not dispatched until END_STREAM, so its entire request body is held
 * in memory before the SAF pipeline ever sees it.  Inbound flow control cannot be
 * what bounds that buffer: crediting the window only as the pipeline consumed the
 * body would deadlock, because the pipeline does not start until the body is
 * complete.  So the bytes are credited as they arrive, and something else has to
 * put a ceiling on them -- otherwise one client on one stream can push unbounded
 * DATA into the heap, which is a plain memory-exhaustion DoS.
 *
 * HTTP/1.1 needs no equivalent, which is why MaxRequestBodySize can default to "no
 * limit" and this cannot: httpfilter streams a request body through a fixed buffer
 * as the SAF reads it rather than accumulating the whole thing.
 *
 * MaxRequestBodySize still wins where an operator has set it.  It is the existing
 * answer to "how large a request body will this server accept", and on this path it
 * would otherwise bind only once the body had been buffered in full -- far too late
 * to be the bound that matters.
 */
static const PRInt32 H2_DEFAULT_MAX_REQUEST_BODY = 16 * 1024 * 1024;

static PRInt64 s_maxRequestBody = H2_DEFAULT_MAX_REQUEST_BODY;
static PRCallOnceType s_maxRequestBodyOnce;

static PRStatus PR_CALLBACK initMaxRequestBody(void)
{
    // The floor is 1 KB rather than 0: a cap that rejects every request body would
    // be a foot-gun with no legitimate use, and "reject all uploads" is properly
    // expressed in obj.conf, not here.
    PRInt32 limit = conf_getboundedinteger("Http2MaxRequestBodySize",
                                           1024, INT_MAX,
                                           H2_DEFAULT_MAX_REQUEST_BODY);

    PRInt32 general = conf_getboundedinteger("MaxRequestBodySize",
                                             -1, INT_MAX, -1);
    if (general >= 0 && general < limit)
        limit = general;

    s_maxRequestBody = limit;
    ereport(LOG_VERBOSE,
            "HTTP/2: buffering at most %d bytes of request body per stream",
            (int)limit);
    return PR_SUCCESS;
}

static PRInt64 h2MaxRequestBody(void)
{
    if (PR_CallOnce(&s_maxRequestBodyOnce, initMaxRequestBody) != PR_SUCCESS)
        return H2_DEFAULT_MAX_REQUEST_BODY;
    return s_maxRequestBody;
}

/*
 * MAX_CONCURRENT_STREAMS, the per-connection cap advertised to the peer.
 *
 * The default of 100 is the value RFC 9113 6.5.2 recommends as a floor so as not to
 * needlessly limit multiplexing, and matches common servers.  It does not need to be
 * lowered to protect memory: the *aggregate* bound across all connections is the
 * stream pool (Http2MaxStreams), which a client cannot route around by opening more
 * connections.  Lower this only for workloads where each stream is individually
 * expensive.
 *
 * Read once rather than per session: it cannot change without a restart, and init()
 * runs on every accepted HTTP/2 connection.
 */
static const PRInt32 H2_DEFAULT_MAX_CONCURRENT_STREAMS = 100;

static PRInt32 s_maxConcurrentStreams = H2_DEFAULT_MAX_CONCURRENT_STREAMS;
static PRCallOnceType s_maxConcurrentStreamsOnce;

static PRStatus PR_CALLBACK initMaxConcurrentStreams(void)
{
    s_maxConcurrentStreams =
        conf_getboundedinteger("Http2MaxConcurrentStreams", 1, 10000,
                               H2_DEFAULT_MAX_CONCURRENT_STREAMS);
    ereport(LOG_VERBOSE,
            "HTTP/2: %d concurrent streams per connection "
            "(Http2MaxConcurrentStreams)",
            (int)s_maxConcurrentStreams);
    return PR_SUCCESS;
}

static PRInt32 h2MaxConcurrentStreams(void)
{
    if (PR_CallOnce(&s_maxConcurrentStreamsOnce,
                    initMaxConcurrentStreams) != PR_SUCCESS)
        return H2_DEFAULT_MAX_CONCURRENT_STREAMS;
    return s_maxConcurrentStreams;
}

void H2Session::initConfig(void)
{
    h2MaxConcurrentStreams();
    h2MaxRequestBody();

    // Creates the stream pool as a side effect, which is the point: its size is
    // derived from the thread limits, so building it now -- while startup is still
    // single-threaded and can report a failure -- is better than on the first
    // stream of the first HTTP/2 connection.
    H2StreamConnectionPool::instance();
}

H2Session::H2Session(Connection* conn)
    : connection_(conn), session_(NULL), callbacks_(NULL), initialized_(PR_FALSE),
      prefaceHave_(0), prefaceDone_(PR_FALSE),
      bytesRead_(0), readWouldBlock_(PR_FALSE), writeWouldBlock_(PR_FALSE),
      peerClosed_(PR_FALSE),
      streamsOpened_(0), maxStreamsReached_(PR_FALSE),
      inFlight_(0), driving_(PR_FALSE), wantClose_(PR_FALSE),
      parked_(PR_FALSE), redrive_(PR_FALSE), driver_(NULL),
      tlsSecOn_(PR_FALSE), tlsKeySize_(0), tlsSecretKeySize_(0),
      tlsPeerCert_(NULL)
{
    memset(preface_, 0, sizeof(preface_));
    XP_InitLock(&lock_);

    PR_AtomicIncrement(&s_totalSessions);
    PRInt32 live = PR_AtomicIncrement(&s_liveSessions);
    // Racing writers can each lose the peak to the other; a stat does not need
    // the compare-and-swap loop that would fix it.
    if (live > s_peakSessions)
        s_peakSessions = live;
}

H2Session::~H2Session()
{
    // Deleting the session while a worker is still executing one of its streams
    // would pull that stream out from under the worker.  The ownership invariant
    // rules this out: every teardown path runs either with the driver token and
    // inFlight_ == 0, or from the event loop while the connection is parked,
    // which only happens when nothing is in flight.
    PR_ASSERT(inFlight_ == 0);

    for (std::map<uint32_t, H2Stream*>::iterator it = streams_.begin();
         it != streams_.end(); ++it) {
        delete it->second;
    }
    streams_.clear();

    // Borrowed pointers into streams_, now dangling; drop them without freeing.
    readyToRun_.clear();
    readyToSubmit_.clear();

    if (session_) {
        nghttp2_session_del(static_cast<nghttp2_session*>(session_));
        session_ = NULL;
    }
    if (callbacks_) {
        nghttp2_session_callbacks_del(
            static_cast<nghttp2_session_callbacks*>(callbacks_));
        callbacks_ = NULL;
    }

    if (tlsPeerCert_) {
        CERT_DestroyCertificate(tlsPeerCert_);
        tlsPeerCert_ = NULL;
    }

    // Last, because nghttp2_session_del() above fires the stream-close callback
    // for every stream still open, and that path reaches deleteStream().
    XP_DestroyLock(&lock_);

    PR_AtomicDecrement(&s_liveSessions);
}

PRStatus H2Session::init()
{
    if (initialized_) {
        return PR_SUCCESS;
    }

    nghttp2_session_callbacks* cb = NULL;
    if (nghttp2_session_callbacks_new(&cb) != 0) {
        return PR_FAILURE;
    }
    callbacks_ = cb;

    nghttp2_session_callbacks_set_send_callback(cb, h2_send_cb);
    nghttp2_session_callbacks_set_recv_callback(cb, h2_recv_cb);
    nghttp2_session_callbacks_set_on_begin_headers_callback(cb, h2_begin_headers_cb);
    nghttp2_session_callbacks_set_on_header_callback(cb, h2_header_cb);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(cb, h2_data_chunk_recv_cb);
    nghttp2_session_callbacks_set_on_frame_recv_callback(cb, h2_frame_recv_cb);
    nghttp2_session_callbacks_set_on_frame_send_callback(cb, h2_frame_send_cb);
    nghttp2_session_callbacks_set_on_stream_close_callback(cb, h2_on_stream_close_cb);

    nghttp2_session* s = NULL;
    if (nghttp2_session_server_new2(&s, cb, this, NULL) != 0) {
        return PR_FAILURE;
    }

    // Raise both inbound flow-control windows off the RFC 9113 default of 64 KiB.
    //
    // This matters now that the windows are credited once rather than twice (see
    // h2_data_chunk_recv_cb).  While every byte was credited twice the window only
    // ever grew, so a peer uploading a large body was never throttled at all; with
    // the accounting fixed, a 64 KiB window bounds an upload to 64 KiB per round
    // trip, which on a high-latency link is a real downgrade rather than a fix.
    //
    // It costs nothing to grant.  Worst-case memory on this path is set by the
    // per-stream request-body cap, not by the window: whatever arrives is buffered
    // whole until END_STREAM either way, so a larger window changes when the bytes
    // arrive and not how many are held.
    nghttp2_session_set_local_window_size(s, NGHTTP2_FLAG_NONE, 0,
                                          H2_CONNECTION_WINDOW);

    // Explicitly queue the server's initial SETTINGS frame.  The nghttp2
    // server session normally does this, but doing it explicitly makes the
    // smoke test more deterministic with the SBC build of the library.
    //
    PRInt32 maxConcurrent = h2MaxConcurrentStreams();
    nghttp2_settings_entry iv[] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, (uint32_t)maxConcurrent},
        {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, H2_STREAM_WINDOW}
    };
    nghttp2_submit_settings(s, 0, iv, sizeof(iv) / sizeof(iv[0]));

    session_ = s;
    initialized_ = PR_TRUE;

    if (connection_ && connection_->fSSLEnabled && connection_->fd) {
        PRInt32 secon = -1;
        PRInt32 keySize = 0;
        PRInt32 secretKeySize = 0;
        char* cipher = NULL;
        char* issuerDn = NULL;
        char* userDn = NULL;
        if (SSL_SecurityStatus(connection_->fd, &secon, &cipher, &keySize,
                               &secretKeySize, &issuerDn, &userDn) == SECSuccess &&
            secon > 0)
        {
            tlsSecOn_ = PR_TRUE;
            tlsKeySize_ = keySize;
            tlsSecretKeySize_ = secretKeySize;
            if (cipher)
                tlsCipher_.assign(cipher);
            if (issuerDn)
                tlsIssuerDn_.assign(issuerDn);
            if (userDn)
                tlsUserDn_.assign(userDn);

            SECItem* iditem = SSL_GetSessionID(connection_->fd);
            if (iditem) {
                if (iditem->data && iditem->len > 0) {
                    char* idstr = BTOA_DataToAscii(iditem->data, iditem->len);
                    if (idstr) {
                        tlsSessionId_.assign(idstr);
                        PORT_Free(idstr);
                    }
                }
                SECITEM_FreeItem(iditem, PR_TRUE);
            }
        }
        if (cipher)
            PORT_Free(cipher);
        if (issuerDn)
            PORT_Free(issuerDn);
        if (userDn)
            PORT_Free(userDn);

        tlsPeerCert_ = SSL_PeerCertificate(connection_->fd);
    }

    return PR_SUCCESS;
}

void H2Session::fillSSLSession(Session* sn) const
{
    if (!sn)
        return;

    if (tlsSecOn_ && sn->client) {
        sn->ssl = 1;
        pblock_kvinsert(pb_key_cipher, tlsCipher_.c_str(),
                        static_cast<int>(tlsCipher_.size()), sn->client);
        pblock_kninsert(pb_key_keysize, tlsKeySize_, sn->client);
        pblock_kninsert(pb_key_secret_keysize, tlsSecretKeySize_, sn->client);
        pblock_kvinsert(pb_key_issuer_dn, tlsIssuerDn_.c_str(),
                        static_cast<int>(tlsIssuerDn_.size()), sn->client);
        pblock_kvinsert(pb_key_user_dn, tlsUserDn_.c_str(),
                        static_cast<int>(tlsUserDn_.size()), sn->client);
        if (!tlsSessionId_.empty()) {
            pblock_kvinsert(pb_key_ssl_id, tlsSessionId_.c_str(),
                            static_cast<int>(tlsSessionId_.size()), sn->client);
        }
    }

    if (tlsPeerCert_ && sn->clauth) {
        ClAuth_t* cla = static_cast<ClAuth_t*>(sn->clauth);
        if (cla->cla_cert)
            CERT_DestroyCertificate(cla->cla_cert);
        cla->cla_cert = CERT_DupCertificate(tlsPeerCert_);
    }
}

void H2Session::discard(Connection* conn)
{
    if (!conn)
        return;

    H2Session* h2 = conn->h2Session;
    conn->h2Session = NULL;
    delete h2;      // deletes any streams still open on the session
}

void H2Session::timeoutConnection(Connection* conn)
{
    if (!conn)
        return;

    H2Session* h2 = conn->h2Session;
    nghttp2_session* s = h2 ? (nghttp2_session*)h2->session_ : NULL;

    // The sweep pulled the connection out of the loop before dispatching here, so
    // this only has to bring the session's view into line -- but go through
    // unpark() rather than the flag, for the same reason resumeConnection() does.
    if (h2)
        h2->unpark();

    // The connection was parked in the event loop until the sweep pulled it out,
    // so nobody is driving it and nothing is in flight.  Take the token anyway:
    // pump() below is driver-only work, and claiming it keeps that rule
    // unconditional rather than something each entry point argues about.
    if (h2 && !h2->claimDriver()) {
        ereport(LOG_VERBOSE,
                "H2Session: conn=%p being driven; skipping idle teardown", conn);
        return;
    }

    // Already closing: the decision was made by a thread that held the token, so
    // now that we hold it the answer is stable.  There is nothing left to expire --
    // and nothing to send a GOAWAY on -- so leave the close to the reference that is
    // still holding it off.  See resumeConnection() for the full argument.
    if (conn->closePending()) {
        if (h2)
            h2->releaseDriver();
        ereport(LOG_VERBOSE,
                "H2Session: conn=%p already closing; skipping idle teardown", conn);
        return;
    }

    // Belt and braces against expiring a connection that is still working.  A
    // busy connection is parked with no idle deadline precisely so the sweep
    // cannot select it, so reaching here with streams in flight should not be
    // possible -- but the cost of being wrong is a GOAWAY and a closed socket
    // underneath running pipelines, which is far too expensive to leave to an
    // argument about reachability.  Put the connection back and let the streams
    // finish; the last one out will park it as idle and it can expire then.
    if (h2) {
        XP_Lock(&h2->lock_);
        PRBool busy = (h2->inFlight_ > 0);
        XP_Unlock(&h2->lock_);

        if (busy) {
            ereport(LOG_VERBOSE,
                    "H2Session: conn=%p expired with streams in flight; "
                    "re-arming instead of closing", conn);
            h2->parkForRead();

            // Decide and release together, for the reason errorConnection()
            // explains: a worker that finished while we held the token did not take
            // it, so handing the connection over on the strength of an earlier
            // reading could leave it owned by nobody.
            XP_Lock(&h2->lock_);
            PRBool deferred = (h2->inFlight_ > 0);
            if (deferred) {
                h2->driving_ = PR_FALSE;
                h2->driver_ = NULL;
            }
            XP_Unlock(&h2->lock_);

            if (deferred)
                return;

            // The last of them finished while we were re-arming, so this thread is
            // still the driver.  Expire the connection after all rather than leave
            // it parked with a response queued and nobody to frame it.
            h2->unpark();
        }
    }

    if (s && conn->fd) {
        ereport(LOG_VERBOSE,
                "H2Session: idle timeout, sending GOAWAY on conn=%p", conn);

        // Queue GOAWAY with NO_ERROR: the peer did nothing wrong, the server is
        // simply reclaiming an idle connection.  A client that is still willing
        // to talk can reconnect, which it cannot infer from a bare TCP close.
        nghttp2_session_terminate_session(s, NGHTTP2_NO_ERROR);

        // Best effort only.  The socket is non-blocking, so a full send buffer
        // means the GOAWAY is dropped -- which is acceptable for a connection
        // that has been silent for the whole idle timeout, and far better than
        // parking it again or blocking this worker to force the write out.
        h2->pump();
    }

    closeConnection(conn);
}

void H2Session::errorConnection(Connection* conn)
{
    if (!conn)
        return;

    H2Session* h2 = conn->h2Session;
    if (!h2) {
        conn->fKeepAliveReservation = PR_FALSE;
        conn->destroy();
        return;
    }

    // The loop deregistered the fd before dispatching here, so this only brings
    // the session's view into line -- but go through unpark() rather than the
    // flag, for the same reason resumeConnection() does.
    h2->unpark();

    // Everything below is teardown, and teardown is driver-only work: it walks
    // the stream map and it ends in destroy().  A thread that is driving checks
    // wantClose_ on its way through and owns the teardown itself, so leaving it a
    // note is both necessary and sufficient.
    if (!h2->claimDriver()) {
        h2->requestClose();
        return;
    }

    // Already closing, decided under the token we now hold: the socket error changes
    // nothing about a connection that is only waiting for a reference to be dropped.
    // See resumeConnection() for the full argument.
    if (conn->closePending()) {
        h2->releaseDriver();
        ereport(LOG_VERBOSE,
                "H2Session: conn=%p already closing; error needs no teardown", conn);
        return;
    }

    XP_Lock(&h2->lock_);
    PRBool busy = (h2->inFlight_ > 0);
    XP_Unlock(&h2->lock_);

    if (busy) {
        // The socket is gone but the workers on it are not, and destroy() would
        // close the fd and recycle the Connection underneath them -- including
        // underneath a producer sitting in PR_Poll on that very fd.  Mark the
        // session closed instead: beginClose() releases any producer parked at its
        // ring's high-water mark, so those workers unwind promptly rather than
        // waiting out an I/O timeout on a socket that will never drain.
        ereport(LOG_VERBOSE,
                "H2Session: conn=%p failed with streams in flight; "
                "closing once they finish", conn);
        h2->beginClose();

        // Hand the teardown to the last worker out -- but the hand-off and the
        // token release have to be one step.  A worker that finishes while we hold
        // the token does not take it (streamFinished() only drives when nobody is
        // driving), so releasing it after a separate check would leave a window in
        // which the last worker has already gone and we have decided it is coming:
        // nothing would ever close the connection.  Deciding under the same lock
        // hold that releases the token closes that window in both directions.
        XP_Lock(&h2->lock_);
        PRBool deferred = (h2->inFlight_ > 0);
        if (deferred) {
            h2->driving_ = PR_FALSE;
            h2->driver_ = NULL;
        }
        XP_Unlock(&h2->lock_);

        if (deferred)
            return;

        // They all finished during the above, so this thread is still the driver
        // and the teardown is ours after all.
    }

    closeConnection(conn);
}

void H2Session::closeConnection(Connection* conn)
{
    if (!conn)
        return;

    // Tear the session down before the socket goes away, then hand the
    // Connection back to its pool.
    //
    // The previous one-shot implementation closed conn->fd directly and never
    // called destroy(), so every HTTP/2 connection permanently consumed a
    // ConnectionQueue slot and a ListenSocketConfig reference for the life of
    // the process.  destroy() closes the fd, drops the lsConfig reference,
    // decrements the active-connection count, and returns the Connection to
    // the unused list.
    // Make sure the loop has let go before destroy() closes the fd and returns
    // the Connection to its pool.  Normally it has -- a dispatch deregisters, and
    // the parking paths unpark before they get here -- but a session that was
    // watched while its streams ran has broken the old assumption that a
    // worker-owned connection is never registered, so the guarantee has to come
    // from asking rather than from reasoning.  Removal is idempotent and does not
    // miscount the usual case where there is nothing to remove.
    //
    // takeConnection() rather than removeConnection() because the loop's dispatch
    // reference has to be kept, not dropped: dropping it here could perform the
    // close before destroy() has even been called, and everything below would then
    // be operating on a struct the acceptor has reissued.  Held until the end of
    // this function, so the close happens at the one point where nothing is left
    // that could still be looking at the connection.
    PRBool heldByLoop = PR_FALSE;
    if (conn->eventLoop)
        heldByLoop = conn->eventLoop->takeConnection(conn);

    // The session is *not* deleted here.  destroy() only closes the connection once
    // the last dispatch reference has gone, and a dispatch that is holding one may
    // be inside this very session -- it read conn->h2Session, and is on its way to
    // unpark() or claimDriver() on it.  Deleting the session at the moment the close
    // is decided would pull it out from under that dispatch, which is precisely what
    // the reference exists to prevent.  So the session is connection state like the
    // socket is, and Connection::destroyNow() frees both together.
    conn->fKeepAliveReservation = PR_FALSE;
    conn->destroy();

    // Last, and nothing may touch conn after it: if this was the final reference
    // the close happens inside here.
    if (heldByLoop)
        conn->releaseDispatch();
}

PRStatus H2Session::parkForRead(void)
{
    Connection* conn = connection_;
    if (!conn || !conn->fd)
        return PR_FAILURE;

    // Same requirement finish() has: without a loop and a handler there is
    // nothing to suspend into, and the caller has to fall back to keeping the
    // connection owned by its in-flight streams.
    if (!conn->fEventDriven || !conn->eventLoop || !conn->eventHandler)
        return PR_FAILURE;

    // The registration happens under lock_, not merely the flag.  Doing the flag
    // first and the syscall after leaves a window in which another thread sees a
    // parked session whose fd is not yet in the loop -- and a requeued dispatch,
    // which arrives without the loop having deregistered anything, can clear the
    // flag inside it.  The connection then stays registered with nothing
    // recording that, so the next parking decision is made on a lie.
    //
    // Holding lock_ across the call orders it against every other park decision.
    // The lock order is lock_ then the loop's registry lock, which is the only
    // direction either is ever taken in: the expiry sweep drops the registry lock
    // before it dispatches, so no path arrives here holding it.
    XP_Lock(&lock_);

    if (parked_) {
        // Already watched.  This happens on a second turn through drive() after
        // an earlier turn parked.  Re-registering would be harmless now that the
        // loop is idempotent, but there is nothing to change.
        XP_Unlock(&lock_);
        return PR_SUCCESS;
    }

    PRStatus rv = conn->eventLoop->addConnection(conn, conn->eventHandler,
                                                 PR_FALSE /* writable */,
                                                 PR_FALSE /* armDeadline */);
    if (rv == PR_SUCCESS)
        parked_ = PR_TRUE;

    XP_Unlock(&lock_);
    return rv;
}

void H2Session::unpark(void)
{
    Connection* conn = connection_;

    // Symmetric with parkForRead(): the flag and the deregistration move together
    // so that no other thread can observe one without the other.
    PRBool heldByLoop = PR_FALSE;

    XP_Lock(&lock_);
    if (parked_) {
        parked_ = PR_FALSE;
        if (conn && conn->eventLoop)
            heldByLoop = conn->eventLoop->takeConnection(conn);
    }
    XP_Unlock(&lock_);

    // Outside lock_, without exception.  Releasing the loop's reference can perform
    // a deferred close, and that runs discard() -- which deletes this session and
    // with it the lock being held.  Callers that go on to use the connection after
    // unparking it (finish(), resumeConnection()) hold references of their own, so
    // this release cannot be the one that closes it while they are still working.
    if (heldByLoop)
        conn->releaseDispatch();
}

void H2Session::finish(Connection* conn, PumpResult result)
{
    if (!conn)
        return;

    // Give up the driver token before the connection leaves this thread's
    // control.  drive() only gets here with nothing in flight and nothing
    // queued, so no other thread is waiting to take it and the release needs no
    // further ordering.  It has to happen before the connection is parked,
    // though: once it is back in the event loop the next dispatch has to be able
    // to claim the token.
    H2Session* h2 = conn->h2Session;
    if (h2) {
        // A busy connection may already be registered for readability by
        // parkForRead().  Everything below assumes this thread is the only owner
        // -- it either closes the connection or registers it afresh with an idle
        // deadline -- so take it back out of the loop first.  Done while the token
        // is still held, so no dispatch can be racing us for it.
        h2->unpark();

        XP_Lock(&h2->lock_);
        PR_ASSERT(h2->inFlight_ == 0);
        h2->driving_ = PR_FALSE;
        h2->driver_ = NULL;
        XP_Unlock(&h2->lock_);
    }

    if (result == H2_CLOSE || !conn->fd) {
        closeConnection(conn);
        return;
    }

    // Without an event loop to park on there is nothing to suspend into, and
    // spinning on a non-blocking socket would burn the worker.  Close instead.
    // A missing handler would be worse than closing: the fd would sit in the
    // loop with nothing able to dispatch it again.
    if (!conn->fEventDriven || !conn->eventLoop || !conn->eventHandler) {
        ereport(LOG_VERBOSE,
                "H2Session: no event loop for conn=%p; closing", conn);
        closeConnection(conn);
        return;
    }

    PRBool writable = (result == H2_WANT_WRITE || result == H2_WANT_RDWR);

    // The event loop owns idle-connection bookkeeping from here on; the
    // keep-alive reservation belongs to the HTTP/1.1 PollManager path and
    // destroy() asserts that it has been released.
    conn->fKeepAliveReservation = PR_FALSE;
    conn->fNewlyAccepted = PR_FALSE;

    ereport(LOG_VERBOSE,
            "H2Session: parking conn=%p in event loop (writable=%d)",
            conn, (int)writable);

    // Read interest is always requested: even when output is blocked we still
    // want to notice the peer closing or sending WINDOW_UPDATE/RST_STREAM.
    if (conn->eventLoop->addConnection(conn, conn->eventHandler, writable)
            != PR_SUCCESS) {
        ereport(LOG_WARN,
                "H2Session: failed to re-arm conn=%p; closing", conn);
        closeConnection(conn);
    }
}

void H2Session::processConnection(Connection* conn,
                                  const uint8_t* initial,
                                  size_t initialLen)
{
    if (!conn || !conn->fd)
        return;

    // HTTP/2 processing uses non-blocking socket I/O because nghttp2's
    // send/recv callbacks expect a primitive they can call repeatedly until no
    // more progress can be made.  It also lets pump() return to the event loop
    // instead of blocking a worker thread on an idle connection.
    PRSocketOptionData opt;
    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_TRUE;
    PR_SetSocketOption(conn->fd, &opt);

    H2Session* h2 = new H2Session(conn);
    if (!h2 || h2->init() != PR_SUCCESS) {
        ereport(LOG_WARN, "H2Session: init failed for conn=%p", conn);
        delete h2;
        conn->fKeepAliveReservation = PR_FALSE;
        conn->destroy();
        return;
    }
    conn->h2Session = h2;

    // Nothing else can have reached this session yet, so the claim is a
    // formality -- but taking it here is what lets feedPreface() and drive()
    // assume the token is held, as every other entry point does.
    h2->claimDriver();

    PRBool prefaceComplete = PR_FALSE;
    if (h2->feedPreface(initial, initialLen, prefaceComplete) != PR_SUCCESS) {
        finish(conn, H2_CLOSE);
        return;
    }
    if (!prefaceComplete) {
        // Partial preface: wait for the rest rather than blocking here.
        finish(conn, H2_WANT_READ);
        return;
    }

    h2->drive();
}

void H2Session::resumeConnection(Connection* conn)
{
    if (!conn)
        return;

    H2Session* h2 = conn->h2Session;
    if (!h2 || !conn->fd) {
        // The dispatcher only routes here when a session exists, but bail out
        // by closing rather than returning: the caller has already removed the
        // fd from the event loop, so simply returning would strand the
        // connection with nothing left to dispatch or reclaim it.
        finish(conn, H2_CLOSE);
        return;
    }

    // This dispatch takes the connection off the loop, and that has to be true of
    // the session's own view of it before going for the token: if another thread
    // holds the token, that thread makes the next parking decision and has to see
    // that the connection is no longer being watched.
    //
    // unpark() rather than clearing the flag, because the two ways of getting
    // here differ in exactly that respect.  A dispatch from the loop arrives with
    // the fd already deregistered, and unpark() then has nothing to do beyond the
    // flag.  A requeue does not deregister anything -- the requeueing thread
    // already owned the connection -- so the fd is still in the loop and has to
    // come out.  Clearing the flag alone was right for the first case and left the
    // second registered behind the session's back.
    h2->unpark();

    // An idle parked connection is not being driven by anyone, so this normally
    // succeeds.  A connection parked while its streams run is a different matter:
    // a worker can finish a stream and take the token at any moment, so losing the
    // race here is expected rather than exceptional.
    //
    // Either way this thread must not proceed -- two threads in one nghttp2
    // session is exactly what the token exists to prevent -- and ownership is not
    // in question, because the holder re-parks or tears down the connection before
    // it lets go.  What is in question is the *wakeup*: the holder may already
    // have pumped, in which case the bytes that caused this dispatch would sit
    // unread until something unrelated happened to drive the session again.  So
    // leave a note rather than drop it.
    if (!h2->claimDriver()) {
        XP_Lock(&h2->lock_);
        h2->redrive_ = PR_TRUE;
        XP_Unlock(&h2->lock_);
        ereport(LOG_VERBOSE,
                "H2Session: conn=%p already being driven; deferring dispatch",
                conn);
        return;
    }

    // Holding the token is what makes this question answerable.  Every path that
    // decides to close a connection -- finish(), errorConnection(),
    // timeoutConnection() -- holds the token while it decides, so once we have it the
    // answer cannot change under us.  If the close is already decided then it is
    // waiting only for this dispatch's reference to be dropped, and the session is
    // still here only because of that reference.  Driving it now would start fresh
    // streams on a connection whose teardown is committed, and the teardown would
    // then run underneath them.
    if (conn->closePending()) {
        h2->releaseDriver();
        ereport(LOG_VERBOSE,
                "H2Session: conn=%p is closing; dropping dispatch", conn);
        return;
    }

    if (!h2->prefaceDone_) {
        PRBool prefaceComplete = PR_FALSE;
        if (h2->feedPreface(NULL, 0, prefaceComplete) != PR_SUCCESS) {
            finish(conn, H2_CLOSE);
            return;
        }
        if (!prefaceComplete) {
            finish(conn, H2_WANT_READ);
            return;
        }
    }

    h2->drive();
}

PRStatus H2Session::feedPreface(const uint8_t* initial, size_t initialLen,
                                PRBool& complete)
{
    complete = PR_FALSE;

    nghttp2_session* s = static_cast<nghttp2_session*>(session_);
    static const char prefaceExpectChars[kPrefaceLen + 1] =
        "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

    // Take what we can from the caller's pre-read buffer first.
    size_t consumed = 0;
    if (initial && initialLen > 0 && prefaceHave_ < kPrefaceLen) {
        size_t want = kPrefaceLen - prefaceHave_;
        size_t n = (initialLen < want) ? initialLen : want;
        memcpy(preface_ + prefaceHave_, initial, n);
        prefaceHave_ += n;
        consumed = n;
    }

    // Top up from the socket if that was not enough.  The socket is
    // non-blocking, so PR_Recv returns immediately with PR_WOULD_BLOCK_ERROR
    // when the rest of the preface has not arrived yet.
    while (prefaceHave_ < kPrefaceLen) {
        PRInt32 n = PR_Recv(connection_->fd, preface_ + prefaceHave_,
                            kPrefaceLen - prefaceHave_, 0,
                            PR_INTERVAL_NO_TIMEOUT);
        if (n > 0) {
            prefaceHave_ += n;
            continue;
        }
        if (n == 0) {
            ereport(LOG_VERBOSE,
                    "H2Session: peer closed during preface (have %d of %d)",
                    (int)prefaceHave_, (int)kPrefaceLen);
            return PR_FAILURE;
        }
        if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
            return PR_SUCCESS;      // complete stays PR_FALSE; come back later
        ereport(LOG_WARN, "H2Session: preface read failed err=%s",
                system_errmsg());
        return PR_FAILURE;
    }

    if (memcmp(preface_, prefaceExpectChars, kPrefaceLen) != 0) {
        ereport(LOG_WARN, "H2Session: malformed HTTP/2 connection preface");
        return PR_FAILURE;
    }

    ereport(LOG_VERBOSE,
            "H2Session: preface received, sending server SETTINGS");

    ssize_t rv = nghttp2_session_mem_recv(s, preface_, kPrefaceLen);
    if (rv != (ssize_t)kPrefaceLen)
        return PR_FAILURE;

    // Feed anything the caller pre-read beyond the preface.
    if (initial && initialLen > consumed) {
        rv = nghttp2_session_mem_recv(s, initial + consumed,
                                      initialLen - consumed);
        if (rv < 0)
            return PR_FAILURE;
    }

    prefaceDone_ = PR_TRUE;
    complete = PR_TRUE;
    return PR_SUCCESS;
}

H2Session::PumpResult H2Session::pump()
{
    nghttp2_session* s = static_cast<nghttp2_session*>(session_);

    // Bound the work done in one dispatch so a busy or hostile peer cannot
    // hold a worker indefinitely; whatever is left is picked up on the next
    // event-loop dispatch (the fd stays level-triggered readable).
    static const int maxIterations = 64;

    for (int i = 0; i < maxIterations; i++) {
        if (nghttp2_session_want_write(s)) {
            writeWouldBlock_ = PR_FALSE;
            peerClosed_ = PR_FALSE;
            int rv = nghttp2_session_send(s);
            if (peerClosed_) {
                ereport(LOG_VERBOSE, "H2Session: peer closed connection");
                return H2_CLOSE;
            }
            if (rv != 0 && rv != NGHTTP2_ERR_WOULDBLOCK) {
                ereport(LOG_WARN, "H2Session: nghttp2 send error %d", rv);
                return H2_CLOSE;
            }
        }

        PRBool readProgress = PR_FALSE;
        if (nghttp2_session_want_read(s)) {
            bytesRead_ = 0;
            readWouldBlock_ = PR_FALSE;
            peerClosed_ = PR_FALSE;

            int rv = nghttp2_session_recv(s);
            // A peer that simply goes away is an ordinary end of connection,
            // not an error worth logging: readRaw() turns a zero-length read
            // into NGHTTP2_ERR_EOF, which nghttp2 propagates back to us here.
            if (peerClosed_ || rv == NGHTTP2_ERR_EOF) {
                ereport(LOG_VERBOSE, "H2Session: peer closed connection");
                return H2_CLOSE;
            }
            if (rv != 0 && rv != NGHTTP2_ERR_WOULDBLOCK) {
                ereport(LOG_WARN, "H2Session: nghttp2 recv error %d", rv);
                return H2_CLOSE;
            }
            readProgress = (bytesRead_ > 0);
        }

        int wantRead = nghttp2_session_want_read(s);
        int wantWrite = nghttp2_session_want_write(s);

        // nghttp2 is done with this connection: no more frames to send and it
        // no longer expects any.  This is the normal GOAWAY/end-of-session
        // exit.
        if (!wantRead && !wantWrite)
            return H2_CLOSE;

        // Keep going while we are still making forward progress.
        if (wantWrite && !writeWouldBlock_)
            continue;
        if (readProgress)
            continue;

        if (wantRead && wantWrite)
            return H2_WANT_RDWR;
        if (wantWrite)
            return H2_WANT_WRITE;
        return H2_WANT_READ;
    }

    // Iteration budget exhausted: re-arm and finish the work next time.
    int wantRead = nghttp2_session_want_read(s);
    int wantWrite = nghttp2_session_want_write(s);
    if (!wantRead && !wantWrite)
        return H2_CLOSE;
    if (wantRead && wantWrite)
        return H2_WANT_RDWR;
    return wantWrite ? H2_WANT_WRITE : H2_WANT_READ;
}

PRInt32 H2Session::readRaw(uint8_t* buf, size_t len)
{
    if (!connection_ || !connection_->fd) {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    PRInt32 n = PR_Recv(connection_->fd, buf, len, 0, PR_INTERVAL_NO_TIMEOUT);
    if (n < 0) {
        PRErrorCode err = PR_GetError();
        if (err == PR_WOULD_BLOCK_ERROR) {
            readWouldBlock_ = PR_TRUE;
            return NGHTTP2_ERR_WOULDBLOCK;
        }
        if (isPeerGone(err)) {
            peerClosed_ = PR_TRUE;
            ereport(LOG_VERBOSE, "H2Session: peer disconnected (%d)", err);
            return NGHTTP2_ERR_EOF;
        }
        ereport(LOG_WARN, "H2Session: readRaw error %d", err);
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    if (n == 0) {
        peerClosed_ = PR_TRUE;
        return NGHTTP2_ERR_EOF;
    }
    bytesRead_ += (size_t)n;
    return n;
}

PRInt32 H2Session::writeRaw(const uint8_t* data, size_t len)
{
    if (!connection_ || !connection_->fd) {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    PRInt32 n = PR_Send(connection_->fd, data, len, 0, PR_INTERVAL_NO_TIMEOUT);
    if (n < 0) {
        PRErrorCode err = PR_GetError();
        if (err == PR_WOULD_BLOCK_ERROR) {
            writeWouldBlock_ = PR_TRUE;
            return NGHTTP2_ERR_WOULDBLOCK;
        }
        if (isPeerGone(err)) {
            // nghttp2's send_callback contract only allows WOULDBLOCK or
            // CALLBACK_FAILURE here, so signal the clean close out of band via
            // peerClosed_ and let pump() distinguish it from a real failure.
            peerClosed_ = PR_TRUE;
            ereport(LOG_VERBOSE, "H2Session: peer disconnected (%d)", err);
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
        ereport(LOG_WARN, "H2Session: writeRaw error %d", err);
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    if (n == 0) {
        writeWouldBlock_ = PR_TRUE;
        return NGHTTP2_ERR_WOULDBLOCK;
    }
    return n;
}

H2Stream* H2Session::createStream(uint32_t streamId)
{
    std::map<uint32_t, H2Stream*>::iterator it = streams_.find(streamId);
    if (it != streams_.end())
        return it->second;

    // Already refused (RST_STREAM submitted).  Do not try again or resubmit:
    // createStream() runs once per pseudo-header, so the following headers of a
    // refused request would otherwise each re-refuse it.
    if (refusedStreams_.find(streamId) != refusedStreams_.end())
        return NULL;

    // Past the per-connection stream cap (GOAWAY already submitted with the
    // Nth stream's id).  Refuse any further stream opened before the peer saw
    // our GOAWAY.
    if (maxStreamsReached_) {
        nghttp2_session* s = (nghttp2_session*)session_;
        if (s) {
            nghttp2_submit_rst_stream(s, NGHTTP2_FLAG_NONE, streamId,
                                      NGHTTP2_REFUSED_STREAM);
        }
        refusedStreams_.insert(streamId);
        return NULL;
    }

    H2Stream* stream = new H2Stream(streamId, this);
    if (stream->init(connection_) != PR_SUCCESS) {
        // Distinguish "the server is globally full" from a genuine internal
        // failure.  A full stream pool is not this stream's fault and not
        // permanent, so REFUSED_STREAM tells the client it may retry (on this or
        // another connection); anything else is INTERNAL_ERROR.  nghttp2 already
        // enforces the *per-connection* concurrency limit itself, so reaching
        // here on exhaustion means the *process-wide* pool cap was hit.
        PRBool exhausted = stream->poolExhausted();
        delete stream;

        nghttp2_session* s = (nghttp2_session*)session_;
        if (s) {
            nghttp2_submit_rst_stream(s, NGHTTP2_FLAG_NONE, streamId,
                                      exhausted ? NGHTTP2_REFUSED_STREAM
                                                : NGHTTP2_INTERNAL_ERROR);
        }
        if (exhausted) {
            ereport(LOG_VERBOSE,
                    "H2Session: stream pool full; refusing stream %u", streamId);
        }
        refusedStreams_.insert(streamId);
        return NULL;
    }
    streams_[streamId] = stream;
    streamsOpened_++;
    if (connection_)
        connection_->numRequests = streamsOpened_;

    PRUint32 maxReq = connection_ ? connection_->maxRequests() : 0;
    if (maxReq > 0 && streamsOpened_ >= maxReq) {
        maxStreamsReached_ = PR_TRUE;
        nghttp2_session* s = (nghttp2_session*)session_;
        if (s) {
            ereport(LOG_VERBOSE,
                    "H2Session: max-requests-per-connection (%u) reached at "
                    "stream %u on conn=%p; sending GOAWAY",
                    maxReq, streamId, connection_);
            nghttp2_submit_goaway(s, NGHTTP2_FLAG_NONE, streamId,
                                  NGHTTP2_NO_ERROR, NULL, 0);
        }
    }
    return stream;
}

H2Stream* H2Session::getStream(uint32_t streamId)
{
    std::map<uint32_t, H2Stream*>::iterator it = streams_.find(streamId);
    if (it != streams_.end())
        return it->second;
    return NULL;
}

void H2Session::deleteStream(uint32_t streamId, uint32_t errorCode)
{
    // A refused stream never entered streams_, but nghttp2 still opened it and
    // fires this close callback once it has processed the RST_STREAM we sent.
    // Prune the id here -- before the streams_ lookup below returns early on the
    // miss -- so refusedStreams_ tracks only ids nghttp2 still considers open.
    refusedStreams_.erase(streamId);

    std::map<uint32_t, H2Stream*>::iterator it = streams_.find(streamId);
    if (it == streams_.end())
        return;

    H2Stream* stream = it->second;
    streams_.erase(it);

    XP_Lock(&lock_);
    if (stream->inFlight()) {
        // nghttp2 closes a stream when it has finished the response, but also
        // when the peer sends RST_STREAM -- and that can land while the stream's
        // SAF pipeline is still running on a worker.  Freeing it here would pull
        // the object out from under that worker, so mark it and let the worker's
        // completion path do the free.
        stream->markAbandoned();

        // Take it out of the ready lists as well.  Its worker frees it as soon as
        // it notices it has been abandoned, and a producer thread can have queued
        // it for framing at any moment -- including the moment before this callback
        // ran, since queueing happens off the driver thread -- so leaving the
        // pointer in a list would leave the driver to frame a freed stream.
        forgetStreamLocked(stream);

        // Everything that touches the stream has to happen while lock_ is still
        // held: the worker's streamFinished() frees an abandoned stream as soon
        // as it acquires lock_, so this pointer is only valid until we unlock.
        // Taking bodyLock_ under lock_ is safe -- H2Stream never calls into the
        // session while holding bodyLock_, so the order is one-directional.
        PRBool delivered = stream->bodyDelivered();
        if (!delivered) {
            // Nothing it writes from here on will ever be sent, so release it if
            // it is blocked at its ring's high-water mark; otherwise a stream
            // reset mid-response would leave its worker waiting out the I/O
            // timeout for a drain that will never come.  Its next write fails
            // and the pipeline unwinds.
            stream->abortBody();
        }
        XP_Unlock(&lock_);

        if (delivered) {
            // The response went out in full and the stream closed on the strength
            // of its own END_STREAM; the pipeline is merely still tidying up behind
            // it.  Nothing to report and nothing to abort -- writes past the end of
            // a delivered body are discarded rather than failed, so the pipeline
            // finishes normally and its worker frees the stream.
            ereport(LOG_VERBOSE,
                    "H2Session: stream %u closed after its response was "
                    "delivered; pipeline still finishing", streamId);
            return;
        }

        // Worth a warning rather than a verbose note: the request is going to be
        // answered with a truncated response, and the error code says why -- CANCEL
        // or NO_ERROR is the peer giving up on the stream, anything else is the
        // session having taken exception to it.
        ereport(LOG_WARN,
                "H2Session: stream %u closed (error %u) with its response "
                "incomplete; it will be truncated", streamId, errorCode);
        return;
    }

    forgetStreamLocked(stream);
    XP_Unlock(&lock_);

    delete stream;
}

void H2Session::forgetStreamLocked(H2Stream* stream)
{
    // Every occurrence, not just the first: readyToSubmit_ can hold the same
    // stream twice.  submitReadyStreams() clears submitQueued as it pops an entry
    // but cannot mark the stream submitted until nghttp2 has taken its head, and a
    // producer that fills its ring inside that window queues a second entry.  The
    // duplicate is harmless while the stream lives -- the second visit finds it
    // submitted and moves on -- but this function is what makes the pointer safe
    // to free, so leaving one behind is a use-after-free with a nasty signature:
    // the stale entry is popped after the H2Stream has been recycled for a later
    // stream, read as a submitted stream whose pipeline failed, and answered with
    // RST_STREAM(INTERNAL_ERROR) -- aimed at whichever live stream now owns that
    // memory.
    readyToRun_.erase(std::remove(readyToRun_.begin(), readyToRun_.end(), stream),
                      readyToRun_.end());
    readyToSubmit_.erase(std::remove(readyToSubmit_.begin(),
                                     readyToSubmit_.end(), stream),
                         readyToSubmit_.end());
}

void H2Session::queueStreamForDispatch(uint32_t streamId)
{
    H2Stream* stream = getStream(streamId);
    if (!stream) {
        if (refusedStreams_.find(streamId) == refusedStreams_.end() &&
            !maxStreamsReached_) {
            ereport(LOG_WARN,
                    "H2Session: END_STREAM for unknown stream %u", streamId);
        }
        return;
    }

    // A stream's request completes exactly once, and it is queued for dispatch
    // exactly once.  A second END_STREAM must not queue it again: a conformance
    // test or a hostile peer can send DATA or a HEADERS (trailers) frame with
    // END_STREAM on a stream that has already been dispatched -- nghttp2 still
    // delivers those to this callback -- and by then the first dispatch may be
    // running on a worker, or the stream may already have been framed and freed
    // by deleteStream().  Queueing it a second time enqueues the same H2Stream
    // to the worker pool twice; the second runJob() then runs on a freed object.
    // (Observed as a SIGSEGV in WorkerPool::runWorker() under h2spec's
    // closed-stream cases.)  requestComplete_ is set only here, so it doubles as
    // the "already queued" guard.
    if (stream->requestComplete())
        return;

    // A stream whose body was refused for exceeding Http2MaxRequestBodySize must
    // not run.  It has been RST_STREAMed and its buffered body is short by however
    // much was dropped, so dispatching it would hand a SAF a truncated request and
    // answer a stream the peer has been told is dead.  The check belongs here
    // rather than at the two call sites because both of them can reach a refused
    // stream: the peer's DATA keeps arriving after a RST_STREAM, and the last of it
    // carries END_STREAM, as can a trailing HEADERS frame.
    if (stream->requestBodyRefused())
        return;

    // The peer will send no more of this request, so a SAF that reads the
    // request body must now see EOF rather than a would-block.
    stream->markRequestComplete();

    XP_Lock(&lock_);
    readyToRun_.push_back(stream);
    XP_Unlock(&lock_);
}

PRBool H2Session::claimDriver(void)
{
    XP_Lock(&lock_);
    PRBool mine = PR_FALSE;
    if (!driving_) {
        driving_ = PR_TRUE;
        driver_ = PR_GetCurrentThread();
        mine = PR_TRUE;
    }
    XP_Unlock(&lock_);
    return mine;
}

void H2Session::releaseDriver(void)
{
    XP_Lock(&lock_);
    driving_ = PR_FALSE;
    driver_ = NULL;
    XP_Unlock(&lock_);
}

PRBool H2Session::driverIsMe(void) const
{
    // Racy in general, but not for the only question asked of it: "am I the
    // driver?".  No other thread can make this thread the driver, and this thread
    // cannot be in claimDriver() while it is here, so a PR_TRUE answer is stable
    // and a PR_FALSE one means the token was not ours to begin with.
    return (driving_ && driver_ == PR_GetCurrentThread());
}

void H2Session::beginClose(void)
{
    XP_Lock(&lock_);
    PRBool first = !wantClose_;
    wantClose_ = PR_TRUE;
    XP_Unlock(&lock_);

    if (!first)
        return;

    // Release any stream worker parked at its ring's high-water mark.  Nothing
    // more will be written to this connection, so such a worker would otherwise
    // sit there until its I/O timeout expired waiting for a drain that is never
    // going to happen.  Safe to walk streams_ because only the driver calls this.
    for (std::map<uint32_t, H2Stream*>::iterator it = streams_.begin();
         it != streams_.end(); ++it) {
        it->second->abortBody();
    }
}

void H2Session::requestClose(void)
{
    PRBool wake = PR_FALSE;

    XP_Lock(&lock_);
    if (!wantClose_) {
        wantClose_ = PR_TRUE;

        // Make sure somebody looks.  A driver that is on its way out re-checks
        // redrive_ before releasing the token and takes another turn; one that is
        // blocked in waitIo() gets there when its slice expires.  Either way the
        // close is honoured by a thread that is allowed to walk the stream map,
        // which is what this function exists to avoid doing itself.
        redrive_ = PR_TRUE;
        wake = PR_TRUE;
    }
    XP_Unlock(&lock_);

    if (!wake)
        return;

    ereport(LOG_VERBOSE, "H2Session: conn=%p close requested while another thread "
            "was driving", connection_);
}

void H2Session::resumeDeferredStreams(void)
{
    nghttp2_session* s = static_cast<nghttp2_session*>(session_);
    if (!s)
        return;

    // A data provider that ran out of body bytes returned NGHTTP2_ERR_DEFERRED,
    // and nghttp2 will not ask that stream for data again until it is told to.
    // Every stream that owes a resume gets one here, before the pump that would
    // otherwise have nothing to send.
    for (std::map<uint32_t, H2Stream*>::iterator it = streams_.begin();
         it != streams_.end(); ++it) {
        H2Stream* stream = it->second;
        if (stream->submitted() && stream->takeNeedResume())
            nghttp2_session_resume_data(s, stream->streamId());
    }
}

void H2Session::streamHeadersReady(H2Stream* stream)
{
    if (!stream)
        return;

    XP_Lock(&lock_);
    // Not for an abandoned stream: it is out of streams_ and its worker frees it
    // on the way out, so queueing it here -- this runs on the producer's thread,
    // not the driver's -- would hand the driver a pointer that is about to dangle.
    // There is nothing to frame in any case; the stream is gone.
    if (!stream->submitQueued() && !stream->submitted() &&
        !stream->abandoned() && !wantClose_) {
        readyToSubmit_.push_back(stream);
        stream->markSubmitQueued();
    }
    XP_Unlock(&lock_);
}

PRBool H2Session::waitIo(PumpResult want, PRIntervalTime timeout)
{
    // Reading connection_->fd without the driver token is safe here because the
    // caller is a stream worker that is in flight, and a connection with a stream
    // in flight is not eligible for teardown -- arm (c) of the ownership invariant.
    // Nothing else can close this fd.
    //
    // Note that such a connection *is* parked in the event loop, which is the
    // point of the convoy fix, so an event on it can reach the loop while this
    // wait is in progress.  That is why errorConnection() defers teardown instead
    // of doing it: an fd this poll is sitting on must not be closed underneath it.
    Connection* conn = connection_;
    if (!conn || !conn->fd)
        return PR_FALSE;

    PRPollDesc pd;
    pd.fd = conn->fd;
    pd.out_flags = 0;

    // Wait for what nghttp2 said it was waiting on, which is not always the
    // socket.  A producer blocked at its ring's high-water mark is usually
    // blocked on the peer's flow-control window rather than on the send buffer,
    // and a closed window leaves the socket writable the whole time -- so
    // polling for writability alone would spin at full CPU until a WINDOW_UPDATE
    // happened to arrive.  Only a *read* can reopen that window.
    switch (want) {
    case H2_WANT_WRITE:
        pd.in_flags = PR_POLL_WRITE;
        break;
    case H2_WANT_RDWR:
        pd.in_flags = PR_POLL_READ | PR_POLL_WRITE;
        break;
    case H2_WANT_READ:
    default:
        pd.in_flags = PR_POLL_READ;
        break;
    }

    // A timed-out slice is not a failure: the caller owns the overall deadline
    // and wants the loop re-run either way.  Only a broken poll ends the wait.
    PRInt32 rv = PR_Poll(&pd, 1, timeout);
    return (rv >= 0) ? PR_TRUE : PR_FALSE;
}

PRBool H2Session::flushStreamBody(H2Stream* stream)
{
    return driveForProducer(stream, PR_TRUE);
}

PRBool H2Session::pushStreamBody(H2Stream* stream)
{
    return driveForProducer(stream, PR_FALSE);
}

PRBool H2Session::driveForProducer(H2Stream* stream, PRBool waitForSpace)
{
    // How long a producer waits for the thread that currently holds the token to
    // drain its ring before giving up and taking the token itself.  That thread
    // may well stop driving without having emptied this ring -- it has its own
    // turn budget, and the socket may be full -- so the wait has to be bounded,
    // but it should be long enough that the common case is one handoff rather
    // than a fight over the token.
    static const XPInterval driverHandoffWait = 50 * 1000 * 1000;    // 50ms

    if (!stream)
        return PR_FALSE;

    // The response cannot go out until nghttp2 has been given it, and the first
    // opportunity to do that is now: the headers were parsed as they arrived, so
    // there is no need to wait for the body to finish.  Idempotent, and cheap
    // enough to call on every turn.
    if (stream->headersReady())
        streamHeadersReady(stream);

    // The overall bound on a waiting call: the same one the HTTP/1.1 path applies
    // to a SAF writing a large body to a slow client.  Waits are taken in slices
    // of it rather than in one go, so that a sibling stream which finishes while
    // this one is blocked gets framed promptly instead of waiting out the whole
    // timeout -- we are holding the driver token, so nobody else can frame it.
    const PRIntervalTime start = PR_IntervalNow();
    const PRIntervalTime budget = DaemonSession::GetIOTimeout();
    const PRIntervalTime maxSlice = PR_MillisecondsToInterval(250);

    // Whether this call took the driver token and therefore owes its release.  It
    // is never taken when the pipeline is already running on the driver thread.
    PRBool tookToken = PR_FALSE;
    PRBool ok = PR_FALSE;

    // Why the response body could not be written, for the one caller that cannot
    // ask: the SAF above us turns a failed write into PR_CONNECT_ABORTED_ERROR and
    // reports it as a client that went away, which is misleading when the cause was
    // on this side.  NULL when the failing path has already said so itself.
    const char* why = NULL;

    for (;;) {
        if (stream->bodyAborted()) {
            why = "the stream was closed before its body was written";
            break;
        }

        // Two things bring a *waiting* producer here, and only one of them is a
        // full ring.  The other is a data provider that ran out of body bytes and
        // deferred: nghttp2 will not ask that stream for data again until it is
        // resumed, so the bytes just written need a trip through the driver even
        // though the producer itself is not blocked.  Without this a trickling
        // response would sit in the ring until the pipeline finished.
        //
        // A pushing producer has no such condition to test -- it is asking for a
        // turn, not for room -- so it always takes one.
        if (waitForSpace) {
            if (stream->bodyHasSpace() && !stream->resumePending()) {
                ok = PR_TRUE;
                break;
            }

            if ((PRIntervalTime)(PR_IntervalNow() - start) >= budget) {
                ereport(LOG_VERBOSE, "H2Session: stream %u timed out writing its "
                        "response body", stream->streamId());
                break;
            }
        } else {
            // Nothing below this point can fail a push except the session going
            // away, which is checked explicitly.
            ok = PR_TRUE;
        }

        if (!driverIsMe() && !tookToken) {
            if (!claimDriver()) {
                if (!waitForSpace) {
                    // Another thread is driving, and it pumps before it lets the
                    // connection go, so these bytes leave with it.  A push has
                    // nothing to wait for and must not contend for the token.
                    break;
                }
                // Another thread is driving and will drain the ring on its way
                // through.  Wait to be signalled rather than contend for the
                // token; if that thread stops driving without emptying the ring,
                // the wait expires and we take the token over.
                stream->waitForBodySpace(driverHandoffWait);
                continue;
            }
            tookToken = PR_TRUE;
        }

        // Being the driver, push out as much as nghttp2 will take.  This is
        // drive()'s turn body minus its parking decision, which is not ours to
        // make: the connection has a stream in flight, so it stays owned by that
        // stream either way.
        submitReadyStreams();
        if (tookToken) {
            // Only when this call is the one driving.  Skipped when the pipeline
            // is running inline on a thread that was already the driver, because
            // dispatchReadyStreams() is what runs a stream inline when there is no
            // worker pool -- calling it from here would recurse through the very
            // frame we are in.  The driver frame above us dispatches instead.
            dispatchReadyStreams();
        }
        resumeDeferredStreams();
        PumpResult result = pump();

        if (result == H2_CLOSE) {
            beginClose();
            if (stream->bodyDelivered()) {
                ok = PR_TRUE;
                break;
            }
            why = "the session is closing";
            ok = PR_FALSE;
            break;
        }
        if (!waitForSpace)
            break;                          // one turn was the whole job
        if (stream->bodyAborted()) {
            why = "the stream was closed while its body was being written";
            break;
        }
        if (stream->bodyHasSpace()) {
            ok = PR_TRUE;
            break;
        }

        PRIntervalTime elapsed = PR_IntervalNow() - start;
        if (elapsed >= budget)
            continue;                   // report the timeout at the top
        PRIntervalTime slice = budget - elapsed;
        if (slice > maxSlice)
            slice = maxSlice;
        if (!waitIo(result, slice)) {
            why = "waiting on the socket failed";
            break;
        }
    }

    if (!ok && why) {
        ereport(LOG_WARN, "H2Session: stream %u could not write its response "
                "body: %s", stream->streamId(), why);
    }

    if (tookToken) {
        // Release the token exactly as drive() does when streams are in flight,
        // which is the state this session is guaranteed to be in: this producer's
        // own stream is one of them.  So the connection must not be torn down here;
        // the worker that finishes last claims the token and does that.  Arm (c) of
        // the ownership invariant.
        //
        // Nor is it parked here.  A producer can hold the token for as long as its
        // I/O timeout, and a connection that is already registered stays registered
        // across that -- but one that is not is left to whoever comes next, because
        // parking is drive()'s decision and this frame has a pipeline to return to.
        PRBool owed = PR_FALSE;
        XP_Lock(&lock_);
        driving_ = PR_FALSE;
        driver_ = NULL;
        owed = redrive_;
        redrive_ = PR_FALSE;
        XP_Unlock(&lock_);

        // A dispatch arrived while this producer held the token, and could not
        // drive.  For the same reason this frame does not park, it cannot take the
        // extra turn itself either -- so hand the connection to a worker.  Waiting
        // for the last stream to finish would put the wakeup behind exactly the
        // delay this fix exists to remove, and a slow producer is when that delay
        // is longest.
        //
        // Being on the worker queue while also in flight is harmless: it is parking
        // that has to have a single owner, and parked_ is what guarantees that.
        if (owed && globalEventDispatcher)
            globalEventDispatcher->requeueConnection(connection_);
    }

    return ok;
}

void H2Session::dispatchReadyStreams(void)
{
    for (;;) {
        H2Stream* stream = NULL;

        XP_Lock(&lock_);
        if (wantClose_) {
            // The session is going away, so starting more pipelines would only
            // produce responses nobody can send -- and would keep the connection
            // alive waiting for them.  The streams stay in streams_ and are
            // freed with the session.
            readyToRun_.clear();
            XP_Unlock(&lock_);
            return;
        }
        if (!readyToRun_.empty()) {
            stream = readyToRun_.front();
            readyToRun_.erase(readyToRun_.begin());
            // Counted as outstanding before the handoff, so that a completion on
            // another thread can never observe inFlight_ == 0 while this stream
            // is still going.
            stream->markInFlight();
            inFlight_++;
        }
        XP_Unlock(&lock_);

        if (!stream)
            return;

        if (globalEventDispatcher &&
            globalEventDispatcher->enqueueJob(stream)) {
            continue;
        }

        // No worker pool willing to take the stream: the classic I/O model, or
        // shutdown in progress.  Run it here instead.  Serial, but correct, and
        // it keeps the invariant intact -- an outstanding count that never came
        // back down would leave the connection with no owner at all.  runJob()
        // reports completion, which lands on readyToSubmit_ because this thread
        // already holds the driver token.
        stream->runJob();
    }
}

void H2Session::submitReadyStreams(void)
{
    nghttp2_session* s = static_cast<nghttp2_session*>(session_);

    for (;;) {
        H2Stream* stream = NULL;
        PRBool closing = PR_FALSE;
        PRBool running = PR_FALSE;

        XP_Lock(&lock_);
        if (!readyToSubmit_.empty()) {
            stream = readyToSubmit_.front();
            readyToSubmit_.erase(readyToSubmit_.begin());
            stream->clearSubmitQueued();
            // Read with the pop: whether the pipeline has finished is what makes
            // its result meaningful below, and it can finish at any moment.
            running = stream->inFlight();
        }
        closing = wantClose_;
        XP_Unlock(&lock_);

        if (!stream)
            return;

        // Nothing to frame a response onto once the session is going away.  The
        // stream stays in streams_ and is freed along with the session.
        if (closing || !s)
            continue;

        // dispatchSucceeded() only means anything once the pipeline has stopped
        // running: it is set at the very end of runJob(), so a stream that is
        // still in flight reads as failed no matter how well it is doing.  That
        // is not a hypothetical -- a stream can be queued here twice.  The
        // producer queues on a full ring whenever the stream is neither queued
        // nor submitted, and this loop clears submitQueued as it pops but cannot
        // set submitted until nghttp2 has taken the head; a producer that fills
        // the ring inside that window queues a second entry, which is popped
        // moments later with submitted true and the pipeline still going.  So
        // wait for the completion that always re-queues (see streamFinished())
        // rather than judging the stream now.
        if (stream->submitted()) {
            if (running)
                continue;

            // Already framed, and streaming its body through the ring.  The one
            // thing still left to decide is whether that body is going to be
            // complete: a pipeline that failed after its headers had gone out has
            // produced a truncated response, and the peer has to be told, because
            // an ordinary end of stream would look like success.
            if (!stream->dispatchSucceeded()) {
                ereport(LOG_WARN,
                        "H2Session: stream %u failed after its response headers "
                        "were sent; resetting", stream->streamId());
                nghttp2_submit_rst_stream(s, NGHTTP2_FLAG_NONE,
                                          stream->streamId(),
                                          NGHTTP2_INTERNAL_ERROR);
            }
            continue;
        }

        // A stream reaches this list as soon as its response *headers* have been
        // parsed, which is generally before its body is finished; the body then
        // follows through the ring.  So the test is whether there is a head to
        // submit, not whether the pipeline has finished -- it usually has not.
        if (!stream->headersReady()) {
            if (running)
                continue;

            // No parseable response, and the pipeline has finished, so none is
            // coming.  Reset the stream rather than leaving it open: a client
            // waiting on a stream that will never be answered burns its own
            // timeout instead of failing fast.
            nghttp2_submit_rst_stream(s, NGHTTP2_FLAG_NONE, stream->streamId(),
                                      NGHTTP2_INTERNAL_ERROR);
            continue;
        }

        if (stream->submitResponse(s) != PR_SUCCESS) {
            ereport(LOG_WARN,
                    "H2Session: submit response failed for stream %u",
                    stream->streamId());
            nghttp2_submit_rst_stream(s, NGHTTP2_FLAG_NONE, stream->streamId(),
                                      NGHTTP2_INTERNAL_ERROR);
            continue;
        }

        XP_Lock(&lock_);
        stream->markSubmitted();
        XP_Unlock(&lock_);
    }
}

void H2Session::streamFinished(H2Stream* stream)
{
    if (!stream)
        return;

    H2Stream* toFree = NULL;
    PRBool mine = PR_FALSE;

    XP_Lock(&lock_);
    inFlight_--;
    stream->clearInFlight();
    if (stream->abandoned()) {
        // Closed while we were running it, so it is already out of streams_ and
        // the free is ours.  There is no response worth framing.
        toFree = stream;
    } else if (!stream->submitQueued()) {
        // Queued even if the response has already been framed -- which it will
        // have been for any body big enough to have filled the ring once.  The
        // driver still has work to do on it: pump the tail of the body out of the
        // ring, and reset the stream if the pipeline failed partway through.
        readyToSubmit_.push_back(stream);
        stream->markSubmitQueued();
    }
    if (!driving_) {
        driving_ = PR_TRUE;
        driver_ = PR_GetCurrentThread();
        mine = PR_TRUE;
    }
    XP_Unlock(&lock_);

    // Safe outside the lock: an abandoned stream is reachable from neither
    // streams_ nor the ready lists, so no other thread can see it.
    delete toFree;

    // One of the two threads always ends up responsible for the connection: if
    // another holds the token it re-checks readyToSubmit_ before it lets go, and
    // if nobody does we take it ourselves.
    if (mine)
        drive();
}

void H2Session::drive(void)
{
    Connection* conn = connection_;
    PumpResult result = H2_CLOSE;

    // Each turn of this loop is one pump plus the framing of everything that
    // finished during it.  A peer that keeps opening streams could otherwise hold
    // this worker on one connection indefinitely, so bound the turns for the same
    // reason pump() bounds its own iterations.
    static const int maxDriveTurns = 16;
    int turns = 0;

    for (;;) {
        // One turn: frame whatever finished while we were away, start whatever
        // became runnable, then let nghttp2 read and write.
        //
        // The order matters and the pump has to come last.  submitReadyStreams()
        // only hands a response to nghttp2; the bytes do not reach the socket
        // until something pumps.  Framing a response after the final pump of the
        // turn would leave it sitting in nghttp2's output queue while the
        // connection was parked waiting for a peer that is itself waiting for
        // that response -- a stall that only the idle sweep would break.
        submitReadyStreams();
        dispatchReadyStreams();
        resumeDeferredStreams();
        result = pump();

        // Record a decision to close before looking at the ready lists again, so
        // that both helpers see it on the next turn: neither should start new
        // work or frame new responses onto a session that is finished.
        if (result == H2_CLOSE)
            beginClose();

        XP_Lock(&lock_);
        PRBool closing = wantClose_;
        XP_Unlock(&lock_);

        // wantClose_ can also have been set by a thread that wanted this session
        // shut down but could not drive: errorConnection() leaves the request
        // behind rather than tear down a session whose streams are still running.
        // Finish the job it started, on the turn we notice it.  beginClose() is
        // idempotent, so this costs nothing when we set the flag ourselves above --
        // but until it runs, the producers it releases are still waiting for room
        // on a socket that is never going to drain again.
        if (closing) {
            beginClose();
            result = H2_CLOSE;
        }

        XP_Lock(&lock_);

        // Two ways work can have appeared during the turn: a worker finished and
        // left a response to be framed, or pump() parsed a HEADERS frame and
        // queued a new stream to run.  Either needs another turn -- and in the
        // framing case that turn is what actually writes the response.  We hold
        // the token, so this is ours to deal with rather than the job of a thread
        // that has already gone.
        PRBool moreWork = !readyToSubmit_.empty() || !readyToRun_.empty();

        // A dispatch reached this session while we held the token, so the loop has
        // already taken the fd back and the dispatching worker could not drive.
        // The peer sent something and nothing else is going to read it, so honour
        // that wakeup here.  The turn budget is reset rather than spent: this is
        // fresh peer input, not more of our own backlog, and it is the arrival of
        // new requests that the budget must not be allowed to starve.
        if (redrive_) {
            redrive_ = PR_FALSE;
            XP_Unlock(&lock_);
            turns = 0;
            continue;
        }

        if (moreWork && ++turns < maxDriveTurns) {
            XP_Unlock(&lock_);
            continue;
        }

        if (inFlight_ > 0) {
            // Streams are still executing, which used to end the matter: the
            // connection was left unregistered and the worker that finished last
            // came back through here.  That is what made a multiplexed connection
            // behave like a convoy -- while a stream ran, nothing was watching the
            // socket, so a peer pipelining its next request had those bytes sit
            // unread until the current one finished.  Behind an HTTP/2 reverse
            // proxy, where every request in the site shares one connection, the
            // backlog compounded until the receive buffer filled and the peer could
            // not write at all.
            //
            // So park for readability *as well*: states (a) and (c) of the
            // ownership invariant overlap deliberately here.  No idle deadline is
            // armed, so the sweep cannot reclaim a connection its streams are still
            // using, and the registration happens while the token is held, so the
            // fd cannot be added twice.
            XP_Unlock(&lock_);

            // Nothing to park on, or a session on its way out: fall back to arm (c)
            // alone.  Correct either way -- the last worker out still finishes the
            // connection -- just without the early wakeup.
            if (!closing && parkForRead() != PR_SUCCESS) {
                ereport(LOG_VERBOSE,
                        "H2Session: conn=%p could not be parked while busy; "
                        "leaving it to its in-flight streams", conn);
            }

            // Release the token only now, after the registration.  That ordering is
            // what makes "at most one registration" hold without a second lock: no
            // other thread can reach a parking decision while we hold the token.
            // It leaves a window in which a dispatch arrives and finds the token
            // taken, which is what redrive_ records -- so re-check it, along with
            // the ready lists, rather than let that wakeup wait out the streams
            // that are still running.
            XP_Lock(&lock_);
            if (redrive_ || !readyToSubmit_.empty() || !readyToRun_.empty()) {
                redrive_ = PR_FALSE;
                XP_Unlock(&lock_);
                turns = 0;
                continue;
            }

            driving_ = PR_FALSE;
            driver_ = NULL;
            XP_Unlock(&lock_);
            return;
        }

        // Past this point inFlight_ is zero, so there is no worker in this
        // session and this thread is the only one that can touch the connection.
        // That is what makes both remaining paths safe.

        if (moreWork) {
            // Out of turns with work still queued.  Parking is not an option:
            // nothing the peer does would trigger this work, so waiting for
            // readability could strand it.  Requeue the connection instead --
            // that releases this worker and gives other connections a turn,
            // without ever leaving the connection unowned.
            //
            // The token is released before the requeue, not after: the requeued
            // dispatch has to be able to claim it.  Nothing can free `conn` in
            // between, because nothing else holds a reference to it yet.
            XP_Unlock(&lock_);

            // An earlier turn may have parked this connection for readability while
            // it had streams in flight.  Those streams have since finished, and the
            // requeue below is about to give the connection a second owner, so give
            // up the registration first.
            unpark();

            XP_Lock(&lock_);
            driving_ = PR_FALSE;
            driver_ = NULL;
            XP_Unlock(&lock_);

            if (globalEventDispatcher &&
                globalEventDispatcher->requeueConnection(conn))
                return;

            // Nothing to requeue onto (classic I/O model, or shutting down), so
            // there would be no second turn.  Take the token back and keep going
            // rather than dropping the responses.
            if (!claimDriver())
                return;
            turns = 0;
            continue;
        }

        XP_Unlock(&lock_);
        if (maxStreamsReached_ && streams_.empty() &&
            result == H2_WANT_READ) {
            result = H2_CLOSE;
        }
        break;
    }

    // Nothing in flight and nothing queued: park the connection or tear it down.
    finish(conn, result);
}

void H2Session::setStreamMethod(uint32_t streamId, const uint8_t* value, size_t len)
{
    H2Stream* stream = createStream(streamId);
    if (stream)
        stream->setMethod(value, len);
}

void H2Session::setStreamAuthority(uint32_t streamId, const uint8_t* value, size_t len)
{
    H2Stream* stream = createStream(streamId);
    if (stream)
        stream->setAuthority(value, len);
}

void H2Session::setStreamScheme(uint32_t streamId, const uint8_t* value, size_t len)
{
    H2Stream* stream = createStream(streamId);
    if (stream)
        stream->setScheme(value, len);
}

void H2Session::setStreamPath(uint32_t streamId, const uint8_t* value, size_t len)
{
    H2Stream* stream = createStream(streamId);
    if (stream)
        stream->setPath(value, len);
}

void H2Session::addStreamHeader(uint32_t streamId,
                                const uint8_t* name, size_t nameLen,
                                const uint8_t* value, size_t valueLen)
{
    H2Stream* stream = createStream(streamId);
    if (stream)
        stream->addHeader(name, nameLen, value, valueLen);
}

PRBool H2Session::appendStreamRequestBody(uint32_t streamId,
                                          const uint8_t* data, size_t len)
{
    H2Stream* stream = getStream(streamId);
    if (!stream)
        return PR_TRUE;     // nothing buffered, so nothing to refuse

    // Refusal is sticky, and has to be.  RST_STREAM does not stop DATA the peer has
    // already put on the wire, so more frames for this stream keep arriving, and
    // without stickiness each one would be measured against the limit on its own
    // and resume buffering a body that was deliberately abandoned.  Once refused,
    // refused; queueStreamForDispatch() declines the stream on the same flag.
    if (stream->requestBodyRefused())
        return PR_FALSE;

    // Checked before the append, not after: the point is not to notice that the
    // heap has grown too far but to decline to grow it.
    PRInt64 limit = h2MaxRequestBody();
    if ((PRInt64)stream->requestBodyBuffered() + (PRInt64)len > limit) {
        stream->markRequestBodyRefused();
        ereport(LOG_WARN,
                "H2Session: stream %u request body exceeds the %d byte limit "
                "(Http2MaxRequestBodySize); refusing the stream",
                streamId, (int)limit);
        return PR_FALSE;
    }

    stream->appendRequestBody(data, len);
    return PR_TRUE;
}

} // namespace heliod
