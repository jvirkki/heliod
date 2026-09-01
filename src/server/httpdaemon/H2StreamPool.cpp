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

#include "httpdaemon/H2StreamPool.h"
#include "httpdaemon/daemonsession.h"   // DaemonSession::GetMaxSessions()
#include "frame/conf.h"                 // conf_getboundedinteger()
#include "base/ereport.h"

namespace heliod {

/*
 * Default sizing.
 *
 * The cap defaults to the request-processing capacity times a headroom factor.
 * "Request-processing capacity" is DaemonSession::GetMaxSessions() -- the value
 * historically configured as RqThrottle and now as <thread-pool><max-threads> --
 * because that is what the instance is sized for.  The factor exists because an
 * HTTP/2 stream can be *live* without executing: a stream blocked on its
 * flow-control window holds a pool slot (and a ring) while it waits, so the
 * number of live streams runs ahead of the number actively on a worker thread.
 *
 * Worst-case memory follows directly: each live stream's ring is bounded by
 * RESPONSE_RING_CAPACITY (32 KB), so the aggregate ceiling is
 * maxStreams * 32 KB.  At the 128-thread floor below that is 2048 streams, or
 * ~64 MB of ring in the worst case.  An operator on a tighter memory budget
 * lowers Http2MaxStreams; one serving many heavily-multiplexed clients raises
 * it.
 */
static const PRUint32 H2_STREAMS_PER_THREAD = 16;

// Matches ConfigureEarly()'s own floor for an instance that enables the thread
// pool without naming an upper bound, and covers the acceptor-thread model
// (GetMaxSessions() == 0), so HTTP/2 is never left with a zero stream budget.
static const PRInt32 H2_MIN_RQTHROTTLE = 128;

static PRCallOnceType s_poolOnce;
static H2StreamConnectionPool* s_pool = NULL;

PRStatus PR_CALLBACK H2StreamConnectionPool::initOnce(void)
{
    PRInt32 rqThrottle = DaemonSession::GetMaxSessions();
    if (rqThrottle < H2_MIN_RQTHROTTLE)
        rqThrottle = H2_MIN_RQTHROTTLE;

    // The configurable range is deliberately wide: an operator on a tiny box may
    // want a small cap (each slot can cost up to a 32 KB ring), so the lower
    // bound is 1, not the default's scaling floor.  The upper bound caps the
    // worst-case ring memory at a bit over 32 GB, a guard against a fat-fingered
    // value rather than a recommendation.  The 128 floor above governs only how
    // the *default* is derived, not what may be configured.
    const PRInt32 lo = 1;
    const PRInt32 hi = 1048576;

    // Clamp the derived default into range before handing it to
    // conf_getboundedinteger(), which asserts its default is within [min,max].
    PRInt32 def = rqThrottle * (PRInt32)H2_STREAMS_PER_THREAD;
    if (def < lo) def = lo;
    if (def > hi) def = hi;

    PRInt32 maxStreams = conf_getboundedinteger("Http2MaxStreams", lo, hi, def);

    s_pool = new H2StreamConnectionPool((PRUint32)maxStreams);
    if (!s_pool)
        return PR_FAILURE;

    ereport(LOG_VERBOSE,
            "HTTP/2 stream pool: %d stream maximum (Http2MaxStreams)",
            (int)maxStreams);
    return PR_SUCCESS;
}

H2StreamConnectionPool* H2StreamConnectionPool::instance()
{
    if (PR_CallOnce(&s_poolOnce, initOnce) != PR_SUCCESS)
        return NULL;
    return s_pool;
}

H2StreamConnectionPool::H2StreamConnectionPool(PRUint32 maxStreams)
    : freeList_(NULL),
      max_(maxStreams),
      allocated_(0),
      inUse_(0),
      peakInUse_(0),
      overflows_(0)
{
    XP_InitLock(&lock_);
}

H2StreamConnectionPool::~H2StreamConnectionPool()
{
    // The pool is a process-lifetime singleton, so this normally never runs.
    while (freeList_) {
        Connection* c = freeList_;
        freeList_ = c->next;
        delete c;
    }
}

Connection* H2StreamConnectionPool::acquire()
{
    XP_Lock(&lock_);

    if (inUse_ >= max_) {
        overflows_++;
        XP_Unlock(&lock_);
        return NULL;
    }

    Connection* conn = freeList_;
    if (conn) {
        freeList_ = conn->next;

        // A recycled struct must arrive inert.  H2Stream::init() overwrites the
        // fields it uses, but it does not touch these, so reuse is only correct
        // while the synthetic-connection dispatch path leaves them alone (it
        // parses from its own netbuf via the worker's request object, and the
        // stream connection never enters keep-alive or the accelerator cache).
        // These asserts fail loudly if that ever stops being true.
        PR_ASSERT(conn->connQueue == NULL);
        PR_ASSERT(conn->h2Session == NULL);
        PR_ASSERT(conn->async.inbuf.buf == NULL);
        PR_ASSERT(conn->evNext == NULL && conn->evPrev == NULL);
    } else {
        // Allocate a fresh struct.  allocated_ never exceeds max_ because inUse_
        // gates entry above and a struct is either in use or on the free list.
        conn = new Connection();
        if (!conn) {
            overflows_++;
            XP_Unlock(&lock_);
            return NULL;
        }
        allocated_++;
    }
    inUse_++;
    if (inUse_ > peakInUse_)
        peakInUse_ = inUse_;

    XP_Unlock(&lock_);

    // Left for H2Stream::init() to populate; cleared here so a reused struct
    // never carries a stale free-list link into use.
    conn->next = NULL;
    return conn;
}

void H2StreamConnectionPool::release(Connection* conn)
{
    if (!conn)
        return;

    XP_Lock(&lock_);
    conn->next = freeList_;
    freeList_ = conn;
    if (inUse_ > 0)
        inUse_--;
    XP_Unlock(&lock_);
}

PRUint32 H2StreamConnectionPool::inUse() const
{
    // A racy read is fine for a stat; do not bother locking.
    return inUse_;
}

PRUint64 H2StreamConnectionPool::overflows() const
{
    return overflows_;
}

PRUint32 H2StreamConnectionPool::peakInUse() const
{
    // Racy reads are fine for stats; see inUse().
    return peakInUse_;
}

PRUint32 H2StreamConnectionPool::allocated() const
{
    return allocated_;
}

} // namespace heliod
