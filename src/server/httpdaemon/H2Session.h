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
 * @file H2Session.h
 * @author Joe McCabe
 * @brief HTTP/2 session wrapper around nghttp2
 */

#ifndef _H2Session_h_
#define _H2Session_h_

#include <nspr.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "xp/xpsynch.h"

#include "httpdaemon/H2Stream.h"

struct Connection;

namespace heliod {

/**
 * HTTP/2 per-connection session wrapper around nghttp2.
 *
 * A single H2Session owns one nghttp2_session and multiplexes
 * logical streams over one NSPR TLS/TCP file descriptor.
 * Each stream is mapped onto a transient NSAPISession/NSAPIRequest
 * and passed through the existing SAF pipeline.
 *
 * The session is owned by its Connection and outlives a single worker
 * dispatch.  A worker only runs the session while there is protocol work
 * to do: each dispatch drives nghttp2 as far as it can go without
 * blocking (pump()), and as soon as nghttp2 is waiting on the socket the
 * connection is handed back to the EventLoop and the worker is released.
 * An idle HTTP/2 connection therefore costs one fd and one Connection,
 * not a worker thread.
 *
 * Concurrency
 * -----------
 * Streams execute in parallel; the session does not.  Two rules cover it:
 *
 *  1. **The driver token.**  Exactly one thread at a time may touch the
 *     nghttp2 session, the real socket, or the streams_ map.  That thread holds
 *     the driver token (`driving_`).  nghttp2 is not thread-safe and the socket
 *     has one send buffer, so this is the write-serialization the multiplexing
 *     design needs.  A thread claims the token with claimDriver() and gives it
 *     up in drive(); a stream worker never holds it while running a pipeline.
 *
 *  2. **The ownership invariant.**  At every instant a live connection is in at
 *     least one of three states, and never in none of them:
 *       (a) registered with the EventLoop -- the loop will dispatch it,
 *       (b) held by a thread with the driver token -- that thread will finish
 *           it, or
 *       (c) `inFlight_ > 0` -- a stream is executing, and the worker that
 *           finishes last will claim the token and finish it.
 *     This is what guarantees every connection eventually gets closed or
 *     re-armed.  The states overlap: a busy connection is normally both (a) and
 *     (c) at once, and may briefly be all three.
 *
 *     Overlapping (a) with (c) is not an optimization, it is what makes
 *     multiplexing work.  A connection whose streams are executing still has to
 *     be *read*, because the peer is entitled to send its next request while the
 *     current one runs -- which is all any client behind an HTTP/2 reverse proxy
 *     ever does, since the proxy puts every request on one connection.  Leaving
 *     it unregistered meant those bytes sat unread in the socket buffer until the
 *     last in-flight stream finished, turning a multiplexed connection into a
 *     convoy: one request at a time, with everything that arrived meanwhile
 *     dispatched in a batch afterwards.  Under sustained load the backlog grew
 *     until the receive buffer filled and the peer could not write at all.
 *
 *     Three things had to be true before (a) and (c) could overlap, and all are:
 *
 *     A busy connection is parked with **no idle deadline**, so the sweep cannot
 *     reclaim a connection that streams are running on.  timeoutConnection()
 *     refuses to expire one anyway, because the cost of being wrong is a closed
 *     socket underneath running pipelines.
 *
 *     `parked_` and the registration itself move together, under `lock_` (see
 *     parkForRead()/unpark()).  Setting the flag and then registering looks
 *     equivalent and is not: the token holder is not the only thread that can
 *     clear the flag.  A dispatch does too, and a *requeued* dispatch arrives
 *     without the loop having deregistered anything, so it can land in that
 *     window and leave the fd registered with nothing recording it -- after which
 *     the next parking decision is made on a lie, the duplicate add fails, and
 *     the session concludes it cannot park a connection it should simply have
 *     kept.  For the same reason those dispatch paths call unpark() rather than
 *     assigning the flag: only unpark() knows whether an fd still has to come out.
 *
 *     EventLoop registration is idempotent in both directions, so no two paths
 *     have to agree on which of them registered or removed an fd first.  This is
 *     what lets a connection that was watched while busy arm its idle deadline by
 *     simply re-registering, and what keeps the connection count honest across the
 *     duplicate removals that ownership hand-offs produce.
 *
 *     The narrow window that remains -- registered, token not yet released, a
 *     dispatch arriving and finding the token taken -- is covered by `redrive_`,
 *     which the token holder honours before it lets go.
 *
 * A corollary worth stating because so much else depends on it: finish() is
 * only ever reached with `inFlight_ == 0` and nothing queued, so by then no
 * other thread can be inside this session at all.
 */
class H2Session {
public:
    H2Session(Connection* conn);
    ~H2Session();

    /**
     * Initialize the nghttp2 session.  Returns PR_SUCCESS on success.
     */
    PRStatus init();

    /**
     * First dispatch of a connection that has just been identified as
     * HTTP/2.  Creates the persistent session, stores it on the Connection,
     * feeds any pre-read bytes, drives one non-blocking pump cycle, and then
     * either parks the connection in the event loop or tears it down.
     *
     * `initial` and `initialLen` contain bytes already read from the wire
     * (e.g. the TLS handshake may have buffered the HTTP/2 connection
     * preface).  They may be NULL/0 when no data has been pre-read.  The
     * buffer is fully consumed before this call returns and need not stay
     * valid afterwards.
     *
     * The Connection may be closed and recycled by the time this returns;
     * callers must not dereference it afterwards.
     */
    static void processConnection(Connection* conn,
                                  const uint8_t* initial = NULL,
                                  size_t initialLen = 0);

    /**
     * Every subsequent event-loop dispatch of a connection that already has
     * a live H2Session.  Same ownership contract as processConnection().
     */
    static void resumeConnection(Connection* conn);

    /**
     * The connection sat idle past its deadline.  Tells the peer the session
     * is going away (GOAWAY, NO_ERROR) on a best-effort basis and then closes
     * the connection and returns it to the pool.
     *
     * Runs on a worker thread, not the event-loop thread, because it drives
     * nghttp2 and writes to the socket.  Same ownership contract as
     * processConnection(): `conn` must not be dereferenced afterwards.
     */
    static void timeoutConnection(Connection* conn);

    /**
     * The event loop reported the socket dead (error, or a peer hangup with no
     * data left to read).  Tears the session and the connection down, or -- if
     * streams are still executing on it -- marks the session closed and leaves
     * the teardown to the worker that finishes last.
     *
     * That second case is why this exists rather than the dispatcher destroying
     * the connection itself.  A connection with streams in flight is registered
     * for readability (see the ownership invariant above), so the loop can now
     * report an error on one; destroying it inline would close the fd and recycle
     * the Connection underneath the workers still writing to it.
     *
     * Runs on the event-loop thread.  Same ownership contract as
     * processConnection(): `conn` must not be dereferenced afterwards.
     */
    static void errorConnection(Connection* conn);

    /**
     * Delete the session attached to `conn` (if any) and clear the field,
     * without touching the socket or the Connection itself.
     *
     * Called from Connection::destroyNow(), i.e. at the point the connection is
     * actually closed rather than the point the close is decided.  Those are not the
     * same moment: a dispatch may hold a reference that defers the close, and that
     * dispatch may be inside this session.  Nothing else may call this -- deciding to
     * close is closeConnection()'s job and performing it is the Connection's.
     */
    static void discard(Connection* conn);

    /**
     * HTTP/2 connection counters for this process, reported by /perfdump.
     * One session exists per HTTP/2 connection for that connection's lifetime,
     * so liveSessions() climbing without bound -- while the peer is not actually
     * holding that many connections open -- is the signature of a leaked session,
     * which nothing observable from outside the process would reveal.
     * peakSessions() is the high-water mark and totalSessions() a monotonic
     * total, so two samples give the connection rate.
     */
    static PRUint32 liveSessions(void);
    static PRUint32 peakSessions(void);
    static PRUint32 totalSessions(void);

    /**
     * Read every HTTP/2 magnus.conf directive, so that startup can account for
     * them.  Called once from WebServer's init, before conf_warnunaccessed().
     *
     * Each of these values is otherwise read lazily, on the first connection or
     * the first stream that needs it -- which is long after startup has decided
     * which directives went unread and warned "CORE2176: <directive> ignored"
     * about each one.  The directive was in fact honoured; the warning was purely
     * an artifact of when it was read, and told an operator who had correctly
     * tuned HTTP/2 the exact opposite of the truth.  Touching them here is what
     * makes the accounting match what the server actually does.
     *
     * Must run late enough that DaemonSession's thread limits are configured,
     * since the stream-pool default is derived from them.
     */
    static void initConfig(void);

    // nghttp2 C callback targets.  Public because nghttp2 invokes them
    // via C function pointers, but they are not part of the public C++ API.
    PRInt32 readRaw(uint8_t* buf, size_t len);
    PRInt32 writeRaw(const uint8_t* data, size_t len);

    H2Stream* createStream(uint32_t streamId);
    H2Stream* getStream(uint32_t streamId);
    void deleteStream(uint32_t streamId, uint32_t errorCode);
    void setStreamMethod(uint32_t streamId, const uint8_t* value, size_t len);
    void setStreamAuthority(uint32_t streamId, const uint8_t* value, size_t len);
    void setStreamScheme(uint32_t streamId, const uint8_t* value, size_t len);
    void setStreamPath(uint32_t streamId, const uint8_t* value, size_t len);
    void addStreamHeader(uint32_t streamId,
                         const uint8_t* name, size_t nameLen,
                         const uint8_t* value, size_t valueLen);
    /**
     * Buffer request-body bytes for a stream.  Returns PR_FALSE if doing so would
     * take the stream past the per-stream request-body cap, in which case nothing
     * is buffered and the caller must refuse the stream: the body is held whole
     * until END_STREAM, so this cap is the only thing bounding it.
     */
    PRBool appendStreamRequestBody(uint32_t streamId,
                                   const uint8_t* data, size_t len);

    /**
     * The peer has finished sending the request on this stream, so the stream is
     * ready to run.  Called from an nghttp2 callback, i.e. by the driver, and it
     * only queues: the pipeline itself must not run here, because running it
     * inside the callback is precisely what serialized the streams.
     */
    void queueStreamForDispatch(uint32_t streamId);

    /**
     * A stream's SAF pipeline has finished, called by the worker that ran it.
     * Queues the stream's response for framing and then either takes the driver
     * token and completes the connection, or leaves it to the thread that
     * already holds it.
     *
     * Consumes `stream`: it may be freed before this returns.
     */
    void streamFinished(H2Stream* stream);

    /**
     * Called by a stream's worker when it has filled that stream's response ring
     * and cannot continue until some of it reaches the client, or when the data
     * provider deferred and nghttp2 has to be told there is data again.
     *
     * Returns when the ring has room, or PR_FALSE if the session is going away and
     * the pipeline should unwind.  May block for up to the configured I/O timeout
     * waiting for a slow client, which is the same bound the HTTP/1.1 path applies
     * to a SAF writing a large body.
     *
     * The calling thread does the pushing itself, taking the driver token if it is
     * free.  Handing the job to another thread is not an option: a connection with
     * a stream in flight is deliberately not parked in the event loop, so there is
     * no thread guaranteed to arrive and drain on the producer's behalf.  The
     * producer is the one thread that is definitely present.
     */
    PRBool flushStreamBody(H2Stream* stream);

    /**
     * Called by a stream's worker after every response write that reaches it, to
     * get whatever is buffered moving toward the client.  Unlike
     * flushStreamBody() this never waits: the producer is not blocked, it simply
     * must not have its bytes held back.
     *
     * This exists because the coalescing decision has already been made further
     * up.  The HTTP filter buffers small body writes in its own response buffer
     * and only hands them down on a flush, on a full buffer, or once a
     * long-running program has had its buffering switched off -- so a write that
     * arrives here is one HTTP/1.1 would have put on the wire.  Sending it only
     * when the ring fills or the pipeline finishes would turn a streaming
     * response into a batched one.
     *
     * Returns PR_FALSE if the session is going away and the pipeline should
     * unwind.
     */
    PRBool pushStreamBody(H2Stream* stream);

    /**
     * A stream's response headers have been parsed, so it can be framed even
     * though its body may still be arriving.  Queues it for the driver; the caller
     * is a stream worker, so it does not submit anything itself.  Idempotent.
     */
    void streamHeadersReady(H2Stream* stream);

private:
    /** Length of the mandatory HTTP/2 client connection preface. */
    static const size_t kPrefaceLen = 24;

    /** What the session needs before it can make further progress. */
    enum PumpResult {
        H2_WANT_READ,   // waiting for the peer to send more
        H2_WANT_WRITE,  // output is blocked on the socket send buffer
        H2_WANT_RDWR,   // both of the above
        H2_CLOSE        // session finished, peer gone, or fatal error
    };

    /**
     * Drive nghttp2 as far as possible without ever blocking.  Returns what
     * the session is waiting on.  Never sleeps and never calls PR_Poll.
     */
    PumpResult pump();

    /**
     * Accumulate and validate the 24-byte client connection preface,
     * tolerating a peer that dribbles it across several reads.  On success
     * `complete` reports whether the full preface has now been consumed; if
     * it is PR_FALSE the caller should wait for more data and call again.
     */
    PRStatus feedPreface(const uint8_t* initial, size_t initialLen,
                         PRBool& complete);

    /** Park `conn` in the event loop, or tear it down, per `result`. */
    static void finish(Connection* conn, PumpResult result);

    /**
     * Register the connection for readability while its streams keep running, so
     * that a request arriving mid-flight is noticed instead of waiting for them.
     *
     * Read interest only, and no idle deadline: writes while streams are in
     * flight are the producer's job (it is blocked on them and polls the socket
     * itself), and a busy connection must not be expirable.  Idempotent -- a
     * session that is already parked stays as it is.
     *
     * Driver only, and the token must be held across the call: that is what makes
     * "register at most once" true without a second lock.
     */
    PRStatus parkForRead(void);

    /**
     * Take the connection back out of the event loop if parkForRead() put it
     * there, so that a caller which is about to close, requeue, or re-park it as
     * idle is the only owner again.  Idempotent.  Driver only.
     */
    void unpark(void);

    /**
     * Decide that this connection is finished: take it out of the event loop and
     * destroy it.  The session and the socket go away with it, but not necessarily
     * before this returns -- an outstanding dispatch reference defers both.  Either
     * way `conn` must not be dereferenced afterwards.
     */
    static void closeConnection(Connection* conn);

    /**
     * Take the driver token.  PR_FALSE means another thread already holds it and
     * will see this thread's work before it lets the connection go, so the
     * caller should simply return.
     */
    PRBool claimDriver(void);

    /**
     * Give the driver token back without having driven anything.  For the entry
     * points that claim the token and then discover there is nothing to do -- the
     * connection is already closing -- so that the token does not stay held by a
     * thread that has walked away from the session.
     */
    void releaseDriver(void);

    /**
     * Drive the connection to a resting state: frame any finished responses,
     * start any newly runnable streams, pump nghttp2, and then park the
     * connection, tear it down, or hand it over to an in-flight stream.
     *
     * The caller must hold the driver token.  The token is released before this
     * returns, and the session may have been deleted by then, so no member may
     * be touched afterwards.
     */
    void drive(void);

    /** Hand every stream in readyToRun_ to the worker pool.  Driver only. */
    void dispatchReadyStreams(void);

    /** Frame the response of every stream in readyToSubmit_.  Driver only. */
    void submitReadyStreams(void);

    /**
     * Resume every stream whose data provider deferred for want of body bytes.
     * nghttp2 will not ask a deferred stream for data again until it is told to,
     * so this has to run before each pump.  Driver only.
     */
    void resumeDeferredStreams(void);

    /**
     * Decide to close the session, and release every stream worker that is
     * blocked writing a response body: nothing more will be sent, so a producer
     * waiting for ring space would otherwise wait out its whole I/O timeout.
     * Driver only.
     */
    void beginClose(void);

    /**
     * Ask for the session to be closed without doing any of the work.  For a
     * caller that has concluded the connection is finished but cannot take the
     * driver token -- and so cannot touch the stream map that beginClose() walks.
     * The driver observes this on its next turn and calls beginClose() itself.
     */
    void requestClose(void);

    /**
     * Wait up to `timeout` for the real socket to be ready for whatever `want`
     * says nghttp2 is waiting on.  Called by a producer that has already pushed as
     * much as nghttp2 would take.  Returns PR_FALSE only if the wait cannot be
     * made at all; an expired timeout is a normal return, because the caller owns
     * the overall deadline.
     */
    PRBool waitIo(PumpResult want, PRIntervalTime timeout);

    /**
     * The shared body of flushStreamBody() and pushStreamBody(): take a turn as
     * the driver on behalf of a stream worker that has just produced body bytes.
     *
     * `waitForSpace` is what separates the two callers.  Set, this keeps taking
     * turns until the stream's ring has room or the I/O timeout expires, because
     * the caller cannot continue without room.  Clear, it takes exactly one turn
     * and returns, because the caller only wants its bytes moving.
     */
    PRBool driveForProducer(H2Stream* stream, PRBool waitForSpace);

    /** Does this thread already hold the driver token? */
    PRBool driverIsMe(void) const;

    /**
     * Drop `stream` from the ready lists.  Called with lock_ held, before the
     * stream is freed: the lists hold borrowed pointers, so a stream that is
     * closed between finishing its pipeline and being framed has to come out of
     * them first.
     */
    void forgetStreamLocked(H2Stream* stream);

    Connection* connection_;
    void* session_;     // nghttp2_session*
    void* callbacks_;   // nghttp2_session_callbacks*
    PRBool initialized_;

    // Connection-preface state, carried across dispatches because the peer
    // may not deliver all 24 bytes in one segment.
    uint8_t preface_[kPrefaceLen];
    size_t prefaceHave_;
    PRBool prefaceDone_;

    // Set by readRaw()/writeRaw() so that pump() can distinguish "made
    // progress" from "socket would block".  nghttp2_session_recv() returns 0
    // in both cases, so the I/O callbacks have to report this out of band.
    size_t bytesRead_;
    PRBool readWouldBlock_;
    PRBool writeWouldBlock_;
    PRBool peerClosed_;

    // Owns every live stream on this connection.  Driver-only, like the nghttp2
    // session itself.
    std::map<uint32_t, H2Stream*> streams_;

    // Count of client-initiated streams opened on this session, and whether
    // the connection's max-requests-per-connection cap has been reached
    // (GOAWAY submitted with the Nth stream's id).  Driver-only.
    PRUint32 streamsOpened_;
    PRBool maxStreamsReached_;

    // Stream ids we refused (RST_STREAM already submitted) because a stream
    // could not be created -- almost always the process-wide stream pool being
    // at its cap.  Driver-only, like streams_.  createStream() is called more
    // than once per HEADERS frame (once per pseudo-header), so this keeps the
    // refusal idempotent: without it a refused id would resubmit an RST_STREAM
    // on every following pseudo-header.  Pruned in deleteStream() when nghttp2
    // reports the id closed, so it cannot grow without bound over a long
    // connection.
    std::set<uint32_t> refusedStreams_;

    // Scheduling state, all of it guarded by lock_.  The two ready lists hold
    // borrowed pointers into streams_ (an abandoned stream is the one exception,
    // and it is owned by the worker running it).
    XPLock lock_;
    std::vector<H2Stream*> readyToRun_;      // request complete, awaiting a worker
    std::vector<H2Stream*> readyToSubmit_;   // pipeline done, awaiting framing
    PRInt32 inFlight_;                       // streams executing on workers
    PRBool driving_;                         // the driver token
    PRBool wantClose_;                       // teardown deferred for inFlight_

    // Whether the connection's fd is currently registered with the event loop.
    // Only the driver may change it, which is what keeps the fd from being added
    // twice -- a duplicate EPOLL_CTL_ADD fails, and the failure path closes the
    // connection.  Every dispatch entry point clears it, because the loop takes
    // the fd back before handing a connection to a worker.
    PRBool parked_;

    // A dispatch reached this session while another thread held the token, so the
    // loop has already taken the fd back and the dispatch could not drive.  The
    // peer sent something and nothing else will read it, so the token holder owes
    // a further turn before it releases.  Without this the wakeup would wait out
    // whatever streams happen to be running, which is the convoy again.
    PRBool redrive_;

    // Who holds the token.  Only meaningful while driving_ is set, and only ever
    // compared against the current thread.  This exists because a stream's
    // pipeline can run *on* the driver thread -- dispatchReadyStreams() falls back
    // to running a stream inline when there is no worker pool -- and a producer
    // that blocks there must push its own bytes out directly rather than try to
    // claim a token it is already holding.
    PRThread* driver_;
};

} // namespace heliod

#endif // _H2Session_h_
