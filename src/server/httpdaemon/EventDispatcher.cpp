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

#include "httpdaemon/EventDispatcher.h"

#include <stdlib.h>

#include "base/ereport.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/connqueue.h"
#include "httpdaemon/H2Session.h"

EventDispatcher* globalEventDispatcher = NULL;

PRBool httpdaemon_use_event_model(void)
{
    static PRBool checked = PR_FALSE;
    static PRBool enabled = PR_FALSE;
    if (!checked) {
        enabled = (getenv("HELIOD_EVENT_MODEL") != NULL);
        checked = PR_TRUE;
    }
    return enabled;
}

EventDispatcher::EventDispatcher(PRUint32 minWorkers, PRUint32 maxWorkers)
    : eventLoop_(),
      workerPool_(minWorkers, maxWorkers, this),
      started_(PR_FALSE)
{
}

EventDispatcher::~EventDispatcher(void)
{
    shutdown();
}

PRStatus EventDispatcher::start(void)
{
    if (started_)
        return PR_SUCCESS;

    // An idle event-driven connection is a keep-alive connection whatever
    // protocol it speaks, so reclaim it on the configured keep-alive timeout.
    // Without this nothing ever closes a parked connection: the PollManager
    // that enforces the timeout on the blocking path is not in this picture.
    // HELIOD_EVENT_IDLE_TIMEOUT is authoritative when set, including a 0 that
    // disables expiry, and is how the smoke tests get a deadline they can wait
    // for.  Otherwise the configured keep-alive timeout wins; if keep-alive is
    // switched off that value is 0, which must not be read as "never expire" --
    // an HTTP/2 connection stays parked between requests regardless of the
    // HTTP/1.1 keep-alive setting, so the loop keeps its own default.
    const char* idleOverride = getenv("HELIOD_EVENT_IDLE_TIMEOUT");
    if (idleOverride) {
        eventLoop_.setIdleTimeout((PRUint32)atoi(idleOverride));
    } else if (DaemonSession::GetKeepAliveTimeout() > 0) {
        eventLoop_.setIdleTimeout(DaemonSession::GetKeepAliveTimeout());
    }

    PRStatus rv = eventLoop_.startLoop();
    if (rv != PR_SUCCESS) {
        ereport(LOG_FAILURE, "EventDispatcher: failed to start event loop");
        return rv;
    }

    rv = workerPool_.start();
    if (rv != PR_SUCCESS) {
        ereport(LOG_FAILURE, "EventDispatcher: failed to start worker pool");
        eventLoop_.shutdownLoop();
        return rv;
    }

    started_ = PR_TRUE;
    return PR_SUCCESS;
}

void EventDispatcher::shutdown(void)
{
    if (!started_)
        return;

    eventLoop_.shutdownLoop();
    workerPool_.shutdown();
    started_ = PR_FALSE;
}

PRStatus EventDispatcher::addConnection(Connection* conn)
{
    if (!started_ || !conn)
        return PR_FAILURE;

    conn->fEventDriven = PR_TRUE;
    conn->eventLoop = &eventLoop_;
    conn->eventHandler = this;

    ereport(LOG_VERBOSE,
            "EventDispatcher::addConnection conn=%p fd=%p",
            conn, conn ? conn->fd : NULL);

    return eventLoop_.addConnection(conn, this);
}

PRBool EventDispatcher::enqueueJob(WorkerJob* job)
{
    if (!started_ || !job)
        return PR_FALSE;

    workerPool_.enqueue(job);
    return PR_TRUE;
}

PRBool EventDispatcher::requeueConnection(Connection* conn)
{
    if (!started_ || !conn)
        return PR_FALSE;

    // No removeConnection() here: the caller already owns this connection, so it
    // is not registered with the event loop and there is nothing to deregister.
    // The reference is still needed: the requeue exists so that a session can be
    // driven by a worker rather than by the caller, and the caller's own reason
    // for holding the connection can end before that worker runs.
    conn->retainDispatch();
    workerPool_.enqueue(conn);
    return PR_TRUE;
}

void EventDispatcher::onReadable(Connection* conn)
{
    ereport(LOG_VERBOSE,
            "EventDispatcher::onReadable conn=%p fd=%p",
            conn, conn ? conn->fd : NULL);

    // Remove the fd from epoll while the worker processes it. This prevents
    // duplicate dispatch and lets the worker decide whether to re-register
    // the connection for keep-alive.
    //
    // The return value is the ownership test: if the loop was no longer watching
    // this connection then the event describes an ownership that has already
    // ended, and dispatching on it would run a worker against a socket somebody
    // else has closed -- or against a new client that inherited the struct.
    //
    // A true return also hands over the dispatch reference the loop was holding.
    // That reference has been held since the fd was registered, so it is proof
    // that this is still the same connection -- not a struct that has been closed
    // and reissued to a new client since the event was reported.  Retaining here
    // instead would leave exactly that gap, narrow but real.
    if (!eventLoop_.takeConnection(conn))
        return;

    // Event loop detected data. Hand off to a worker thread so the loop stays
    // responsive. The worker will re-register the connection if keep-alive.
    //
    // The inherited reference travels with the connection into the queue and is
    // released by processConnection(), so the connection cannot be closed and
    // recycled out from under the dispatch while it waits.
    workerPool_.enqueue(conn);
}

void EventDispatcher::onWritable(Connection* conn)
{
    if (!conn)
        return;

    // Remove the fd while a worker owns it, exactly as onReadable does, and for
    // the same reason drop the event if the loop had already let go.
    // The inherited reference is held before anything is read out of the
    // connection, not merely before the handoff: h2Session decides whether this
    // connection gets closed below, and a worker finishing the last stream of a
    // parked session can be clearing it -- and closing the connection -- at this
    // very moment.  The reference is what makes the answer keep meaning something.
    // When the connection is handed to a worker the reference goes with it, and
    // processConnection() drops it.
    if (!eventLoop_.takeConnection(conn))
        return;

    // An HTTP/2 connection registers write interest when its output is
    // blocked on the socket send buffer.  Hand it back to a worker so the
    // session can drain the rest of its pending frames.
    if (conn->h2Session) {
        workerPool_.enqueue(conn);
        return;
    }

    // HTTP/1.1 request processing never registers write interest, so this is
    // spurious; close rather than spin.
    conn->fKeepAliveReservation = PR_FALSE;
    conn->destroy();
    conn->releaseDispatch();
}

void EventDispatcher::onError(Connection* conn)
{
    ereport(LOG_VERBOSE,
            "EventDispatcher::onError conn=%p fd=%p",
            conn, conn ? conn->fd : NULL);

    if (!conn)
        return;

    // Remove from the backend before closing to avoid spurious events, and drop
    // the event outright if the loop had already handed this connection on: the
    // error belongs to a socket that is no longer ours to tear down.
    //
    // The reference that comes back with it is held across everything below, for
    // the reason onWritable() gives: the session pointer is read here and a worker
    // may be tearing the connection down as it is read.  errorConnection() then
    // works on the session itself, which the reference is what keeps alive.
    if (!eventLoop_.takeConnection(conn))
        return;

    if (conn->h2Session) {
        // An HTTP/2 connection can be registered with the loop while workers are
        // still running its streams, so an error event here does not mean the
        // connection is unused -- and tearing it down inline would close the fd
        // and recycle the Connection underneath those workers.  H2Session decides
        // whether it can be destroyed now or has to wait for them.
        heliod::H2Session::errorConnection(conn);
        conn->releaseDispatch();
        return;
    }

    // Release any keep-alive reservation before destroy() asserts.
    conn->fKeepAliveReservation = PR_FALSE;

    // destroy() closes the fd, unrefs lsConfig, and returns the
    // Connection to the unused pool so it can be reused by the acceptor.
    conn->destroy();
    conn->releaseDispatch();
}

void EventDispatcher::onTimeout(Connection* conn)
{
    if (!conn)
        return;

    ereport(LOG_VERBOSE,
            "EventDispatcher::onTimeout conn=%p fd=%p",
            conn, conn->fd);

    // The expiry sweep has already deregistered the fd and passed on the reference
    // the loop was holding, so this connection is ours in the only sense that
    // matters: it cannot be recycled while we look at it.  It can still be *closed*
    // by a worker -- h2Session may be cleared as it is read -- which is why the
    // reference has to be in hand before the read, not merely before the handoff.
    if (conn->h2Session) {
        // HTTP/2 owes the peer a GOAWAY before the socket disappears, and
        // producing one means driving nghttp2 and writing through TLS --
        // neither belongs on the event-loop thread, where it would stall every
        // other connection.  Mark the dispatch as an expiry and let a worker do
        // it, exactly as a readable connection would be handled.
        conn->fEventTimedOut = PR_TRUE;
        workerPool_.enqueue(conn);
        return;
    }

    // An idle HTTP/1.1 keep-alive connection has no protocol obligation on
    // expiry, so close it here rather than paying for a worker dispatch.
    // timeout() is what lets NSS shorten its transmit timeout before any
    // close-notify alert, matching what the PollManager keep-alive path does.
    conn->fKeepAliveReservation = PR_FALSE;
    conn->timeout();
    conn->destroy();
    conn->releaseDispatch();
}

void EventDispatcher::processConnection(Connection* conn)
{
    ereport(LOG_VERBOSE,
            "EventDispatcher::processConnection enter conn=%p fd=%p",
            conn, conn ? conn->fd : NULL);

    if (!conn)
        return;

    // Every path that enqueues a connection retains it first, so the struct and
    // its socket are still the ones the dispatch was made for, whatever else has
    // happened in the meantime.  Releasing it is this function's job and has to
    // happen on every exit -- including the ones where the connection has been
    // closed underneath us.
    if (conn->closePending()) {
        // Closed while this dispatch was queued.  Nothing here is servable: the
        // session is gone and the socket is only still open because the close is
        // waiting on this reference.  Dropping it performs the close.
        ereport(LOG_VERBOSE,
                "EventDispatcher::processConnection conn=%p closing, dropping dispatch",
                conn);
        conn->releaseDispatch();
        return;
    }

    if (conn->fEventTimedOut) {
        // Dispatched because the idle deadline expired, not because a request
        // arrived.  Shut the session down cleanly instead of waiting for bytes
        // that are never coming.  Handled before the conn->fd check below
        // because the connection has to be released either way.
        conn->fEventTimedOut = PR_FALSE;
        heliod::H2Session::timeoutConnection(conn);
        conn->releaseDispatch();
        return;
    }

    // Process the connection using the existing request/response machinery.
    // The connection is removed from the event loop while the worker runs; if
    // it survives and is keep-alive, the worker must re-register it.
    if (conn->fd) {
        if (conn->h2Session) {
            // An established HTTP/2 connection: resume its persistent session
            // rather than re-running HTTP/1.1 setup and protocol detection.
            heliod::H2Session::resumeConnection(conn);
        } else {
            DaemonSession::processConnection(conn);
        }
    }

    // The dispatch reference is what makes it safe to still be here: without it
    // the connection could have been closed and handed to a new client by now,
    // and touching it would be touching somebody else's session.  Releasing it is
    // the last thing this dispatch does, so nothing below may dereference conn.
    ereport(LOG_VERBOSE,
            "EventDispatcher::processConnection exit conn=%p", conn);

    conn->releaseDispatch();
}
