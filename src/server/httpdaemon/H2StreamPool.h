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

#ifndef _H2_STREAM_POOL_H_
#define _H2_STREAM_POOL_H_

#include "nspr.h"
#include "xp/xpsynch.h"
#include "httpdaemon/connqueue.h"       // Connection

namespace heliod {

/**
 * A process-global pool of Connection structs used to carry HTTP/2 streams
 * through the SAF pipeline.
 *
 * Each HTTP/2 stream is dispatched as a synthetic HTTP/1.1 request, and that
 * request needs a Connection to run against.  A stream's Connection is *not* a
 * TCP connection -- it has no socket of its own, it borrows its addresses and
 * listener configuration from the real connection the stream arrived on -- so it
 * deliberately does not come from the ConnectionQueue.  ConnectionQueue is sized
 * for concurrent *TCP connections* (_maxConn); routing streams through it would
 * let one client negotiating up to MAX_CONCURRENT_STREAMS streams consume that
 * many TCP-connection slots, so a handful of HTTP/2 clients could exhaust the
 * pool and starve HTTP/1.1 accepts -- a cheap denial-of-service vector.  This is
 * the sanctioned exception to the "Connections come from ConnectionQueue"
 * invariant recorded in AGENTS.md, and it is what keeps stream admission
 * independent of connection admission.
 *
 * What this class adds over the previous new/delete-per-stream code is the thing
 * that exception cost us: a bound.  A stream's ring buffer and its queued worker
 * job are both charged per live stream, so the number of live streams across the
 * whole process is the real resource-amplification factor, and it needs a
 * ceiling a client cannot route around by opening more connections.  This pool
 * *is* that ceiling.  acquire() hands back NULL once the cap is reached; the
 * caller then refuses the stream with RST_STREAM(REFUSED_STREAM) rather than
 * allocating without limit, and the client may retry elsewhere.  Reuse of freed
 * structs (rather than new/delete each time) is a secondary benefit.
 *
 * The cap self-scales from the request-processing capacity (RqThrottle, i.e.
 * DaemonSession::GetMaxSessions()), so a larger instance gets a larger stream
 * allowance, and it is overridable with the magnus.conf directive
 * Http2MaxStreams.  See the sizing note in H2StreamPool.cpp.
 *
 * Thread-safety: acquire() runs on the driver thread (from a stream-creating
 * nghttp2 callback) but release() can run on a worker thread (an abandoned
 * stream is freed by the worker that was executing it), so every operation takes
 * the internal lock.
 */
class H2StreamConnectionPool {
public:
    /** The process-wide pool, created and configured on first use. */
    static H2StreamConnectionPool* instance();

    /**
     * Hand back a Connection ready to be initialized by H2Stream::init(), or
     * NULL if the pool is at its cap (or out of memory).  A NULL return is a
     * back-pressure signal, not an error: refuse the stream and carry on.
     */
    Connection* acquire();

    /** Return a Connection previously handed out by acquire(). */
    void release(Connection* conn);

    PRUint32 maxStreams() const { return max_; }
    PRUint32 inUse() const;
    PRUint64 overflows() const;

    // High-water mark of inUse(), and how many structs exist at all (in use plus
    // free list).  Reported by /perfdump: inUse() alone is useless for diagnosing
    // a burst or a leak after the fact, because by the time anyone looks at it the
    // interesting moment has passed.  A peak that has reached maxStreams(), or a
    // non-zero overflows(), is what says "streams were refused".
    PRUint32 peakInUse() const;
    PRUint32 allocated() const;

private:
    explicit H2StreamConnectionPool(PRUint32 maxStreams);
    ~H2StreamConnectionPool();
    H2StreamConnectionPool(const H2StreamConnectionPool&);
    H2StreamConnectionPool& operator=(const H2StreamConnectionPool&);

    // PR_CallOnce target for instance().  A static member so it can reach the
    // private constructor; reads the configured cap and creates the singleton.
    static PRStatus PR_CALLBACK initOnce(void);

    XPLock lock_;

    // Free list of released structs, linked through Connection::next.  A live
    // stream never uses `next` (H2Stream::init() clears it and the synthetic
    // Connection enters neither the keep-alive nor the accelerator lists), so it
    // is free for the pool to reuse as the free-list link while a struct is
    // parked here.
    Connection* freeList_;

    PRUint32 max_;          // hard cap on simultaneously live stream Connections
    PRUint32 allocated_;    // structs that exist (in use + on the free list)
    PRUint32 inUse_;        // structs currently handed out
    PRUint32 peakInUse_;    // high-water mark of inUse_
    PRUint64 overflows_;    // acquire() calls refused because the cap was reached
};

} // namespace heliod

#endif // _H2_STREAM_POOL_H_
