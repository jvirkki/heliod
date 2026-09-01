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

#ifndef _EVENT_LOOP_H_
#define _EVENT_LOOP_H_

#include "nspr.h"
#include "NsprWrap/Thread.h"
#include "httpdaemon/libdaemon.h"
#include "xp/xpsynch.h"

class Connection;

/**
 * EventHandler is the interface invoked by EventLoop when a registered
 * file descriptor becomes readable or writable.
 */
class HTTPDAEMON_DLL EventHandler
{
public:
    virtual ~EventHandler() {}

    /**
     * Called when the associated fd has data available to read.
     * The handler should consume what it can and return. If the handler
     * cannot complete processing synchronously, it may dispatch to a worker.
     */
    virtual void onReadable(Connection* conn) = 0;

    /**
     * Called when the associated fd is ready to accept more write data.
     */
    virtual void onWritable(Connection* conn) = 0;

    /**
     * Called when the fd should be removed from the event loop (error/close).
     */
    virtual void onError(Connection* conn) = 0;

    /**
     * Called when the connection sat idle past its deadline.  The loop has
     * already deregistered the fd, so the handler owns the connection and must
     * either close it or hand it to a worker that will.
     *
     * Ownership here is literal: the dispatch reference the loop held while it was
     * watching the fd has been transferred to this call, and the handler must
     * release it on every path -- directly, or by enqueueing the connection so
     * that the worker releases it.  Unlike the readability and error callbacks,
     * this one must not call takeConnection(): the sweep has already done so.
     *
     * This is separate from onError() because expiry is not a failure: a
     * protocol that owes the peer a shutdown frame (HTTP/2 GOAWAY) needs to
     * send one, whereas onError() means the socket is already unusable.
     */
    virtual void onTimeout(Connection* conn) = 0;
};

/**
 * EventLoop is a readiness notification loop that manages idle connections
 * without dedicating an OS thread to each one.
 *
 * The implementation is backend-specific: epoll on Linux, kqueue on BSD/
 * macOS, and Solaris event ports.  It runs in its own thread and dispatches
 * readable/writable events to registered EventHandler instances. A worker
 * thread pool executes request-processing logic asynchronously.
 *
 * The loop also owns idle-connection reclaim.  None of the three backends
 * offers a per-fd deadline that is cheap enough to arm on every request, so
 * the loop keeps its own registry of the connections it owns, bounds its wait,
 * and sweeps that registry for expired deadlines.  Without this a client could
 * open connections and never speak again, and nothing would ever close them.
 */
class HTTPDAEMON_DLL EventLoop : public Thread
{
public:
    EventLoop(void);
    ~EventLoop(void);

    /**
     * Initialize the backend state and start the event-loop thread.
     */
    PRStatus startLoop(void);

    /**
     * Signal the event loop to stop and wait for it to exit.
     */
    void shutdownLoop(void);

    /**
     * Set how long a registered connection may sit with no event before the
     * loop reclaims it.  Zero disables expiry entirely.
     *
     * Must be called before startLoop(); it is read without synchronization by
     * the loop thread on the assumption that it does not change afterwards.
     */
    void setIdleTimeout(PRUint32 seconds);

    /**
     * Register a connection's underlying OS fd for readability events.
     * Uses level-triggered semantics; the fd is removed from the loop while a
     * worker owns it and must be re-added when the worker is done.
     *
     * When `writable` is PR_TRUE the fd is also registered for write
     * readiness in the same operation.  Registering both interests in one
     * call (rather than addConnection() followed by modifyConnection()) is
     * required for correctness: between the two calls the loop could already
     * have reported readability, removed the fd, and handed the connection to
     * a worker, in which case the follow-up modify would either fail or
     * mutate a connection this thread no longer owns.
     *
     * Registering also arms the connection's idle deadline, so re-registering
     * after each dispatch is what keeps an active connection alive.
     *
     * `armDeadline` is for the one case where that is wrong: a connection that
     * is registered while it is still *busy*, so that the loop keeps watching
     * for input, must not also be expirable -- it is not idle, and reclaiming it
     * would pull the socket out from under whatever is running on it.  Clearing
     * it registers the fd with no deadline, which the expiry sweep skips.  The
     * caller then owns liveness for that connection until it parks it as idle.
     *
     * Idempotent: registering an already-registered connection updates its
     * interest and deadline in place and succeeds.  That is not a defensive
     * nicety but the busy case's exit path -- the connection above arms its idle
     * deadline by re-registering once its work finishes -- and it means callers
     * do not have to agree on who registered the fd first.
     *
     * A successful registration takes a dispatch reference on the connection --
     * one per registration, not per call -- which the loop holds until something
     * deregisters it.  A caller that registers a connection is therefore free to
     * drop its own reference immediately: the loop's keeps the struct from being
     * recycled, and takeConnection() hands that reference on to whichever thread
     * takes the connection back out.
     */
    PRStatus addConnection(Connection* conn, EventHandler* handler,
                           PRBool writable = PR_FALSE,
                           PRBool armDeadline = PR_TRUE);

    /**
     * Update interest for an already-registered connection.
     */
    PRStatus modifyConnection(Connection* conn, PRBool readable, PRBool writable);

    /**
     * Remove a connection from the event loop.
     *
     * Idempotent in the same way addConnection() is: removing a connection the
     * loop is not watching succeeds and changes nothing, including the
     * connection count.  Several paths can independently conclude that the loop
     * should let go of an fd -- a dispatch, a session parking decision, a
     * teardown -- and none of them has to know whether another got there first.
     *
     * Drops the dispatch reference the loop was holding, which can perform a
     * deferred close.  A caller that means to keep using the connection
     * afterwards must therefore hold a reference of its own, or call
     * takeConnection() and keep the one it returns.
     */
    PRStatus removeConnection(Connection* conn);

    /**
     * Remove a connection from the event loop and report whether the loop was in
     * fact still watching it.
     *
     * This is removeConnection() for the one caller that has to know the answer:
     * an event handler deciding whether the event it is servicing is real.  An
     * epoll batch is a snapshot of pointers, and nothing can recall an entry once
     * the batch has been returned, so between the wakeup and the handler running
     * some other thread may have taken the connection away -- and if it went as
     * far as closing it, the acceptor may already have handed the struct to a new
     * client.  A false return means the event belongs to an ownership that has
     * since ended and must be dropped rather than dispatched.
     *
     * On a true return the loop's dispatch reference is **transferred to the
     * caller**, who must release it (or pass it on to a worker by enqueueing the
     * connection, which is the same thing).  That transfer is what closes the gap
     * a plain "ask, then retain" would leave: the caller does not have to retain
     * anything to be sure of what it is holding, because the reference it now owns
     * has been held continuously since the fd was registered.  Nothing may
     * dereference the connection after releasing it.
     */
    PRBool takeConnection(Connection* conn);

    /**
     * Number of events processed since startup.
     */
    PRUint64 getEventsProcessed(void) const;

    /**
     * Number of connections currently registered.
     */
    PRUint32 getNumConnections(void) const;

    /**
     * Number of connections closed for exceeding their idle deadline.
     */
    PRUint64 getTimeoutsReaped(void) const;

private:
    virtual void run(void);

    // native OS fd from the NSPR PRFileDesc
    static int nativeFd(PRFileDesc* fd);

    // internal wake-up fd; write a byte to break out of the wait
    void wakeup(void);

    // Timeout registry: the set of connections the loop currently owns, kept
    // as an intrusive list so that arming a deadline costs two pointer writes
    // on the per-request registration path.  timeoutLock_ also guards each
    // connection's registered flag, so both must be held by the caller; link
    // returns whether the registry had been empty, which the caller acts on
    // after dropping the lock.
    PRBool linkTimeoutLocked(Connection* conn);
    void unlinkTimeoutLocked(Connection* conn);

    // Close out every registered connection whose deadline has passed.
    // Runs on the loop thread only.
    void reapExpired(void);

    // How long the next backend wait may block, in milliseconds.
    int waitTimeoutMs(void);

    int backendFd_;       // epoll/kqueue/port fd
    int wakeupReadFd_;    // self-pipe read end
    int wakeupWriteFd_;   // self-pipe write end
    PRBool terminating_;
    PRUint64 eventsProcessed_;
    PRInt32 numConnections_;

    PRIntervalTime idleTimeout_;   // 0 disables expiry
    XPLock timeoutLock_;           // guards the registry links below, each
                                   // connection's eventDeadline/fInEventLoop,
                                   // and the backend add/remove that goes with
                                   // them
    Connection* timeoutHead_;
    Connection* timeoutTail_;
    PRUint64 timeoutsReaped_;
};

#endif /* _EVENT_LOOP_H_ */
