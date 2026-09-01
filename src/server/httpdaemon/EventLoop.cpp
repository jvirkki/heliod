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

#include "httpdaemon/EventLoop.h"

#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#if defined(__linux__)
#define USE_EPOLL
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
      defined(__OpenBSD__) || defined(__DragonFly__)
#define USE_KQUEUE
#include <sys/event.h>
#include <sys/time.h>
#elif defined(__sun)
#define USE_EVENT_PORTS
#include <port.h>
#else
#error "EventLoop: no supported event backend for this platform"
#endif

#include "private/pprio.h"  // PR_FileDesc2NativeHandle
#include "base/ereport.h"
#include "httpdaemon/connqueue.h"

// Idle deadlines are serviced with roughly one second of granularity: while
// anything is registered the loop wakes once a second and sweeps, rather than
// keeping the registry ordered by deadline.  Keep-alive timeouts are measured
// in tens of seconds, so a second of slack is invisible, and this keeps the
// per-request registration path down to two pointer writes.
static const int EVENT_LOOP_SWEEP_MS = 1000;

// Upper bound on the number of expiries one sweep pass hands off.  The registry
// lock is held while walking it, and acceptor and worker threads need that lock
// to register connections, so the walk must not run unbounded.  Anything still
// expired after a full batch is picked up by the next pass in the same sweep.
static const int EVENT_LOOP_MAX_EXPIRE_PER_PASS = 64;

// Fallback used when no idle timeout has been configured.  Matches the
// historical default keep-alive timeout.
static const PRUint32 EVENT_LOOP_DEFAULT_IDLE_SECONDS = 30;

EventLoop::EventLoop(void)
    : Thread("EventLoop"),
      backendFd_(-1),
      wakeupReadFd_(-1),
      wakeupWriteFd_(-1),
      terminating_(PR_FALSE),
      eventsProcessed_(0),
      numConnections_(0),
      idleTimeout_(PR_SecondsToInterval(EVENT_LOOP_DEFAULT_IDLE_SECONDS)),
      timeoutHead_(NULL),
      timeoutTail_(NULL),
      timeoutsReaped_(0)
{
    XP_InitLock(&timeoutLock_);
}

EventLoop::~EventLoop(void)
{
    shutdownLoop();
    XP_DestroyLock(&timeoutLock_);
}

void EventLoop::setIdleTimeout(PRUint32 seconds)
{
    idleTimeout_ = seconds ? PR_SecondsToInterval(seconds) : 0;
}

int EventLoop::nativeFd(PRFileDesc* fd)
{
    if (!fd)
        return -1;
    return PR_FileDesc2NativeHandle(fd);
}

PRStatus EventLoop::startLoop(void)
{
#if defined(USE_EPOLL)
    backendFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (backendFd_ < 0) {
        ereport(LOG_FAILURE, "EventLoop: epoll_create1 failed: %s", strerror(errno));
        return PR_FAILURE;
    }
#elif defined(USE_KQUEUE)
    backendFd_ = kqueue();
    if (backendFd_ < 0) {
        ereport(LOG_FAILURE, "EventLoop: kqueue failed: %s", strerror(errno));
        return PR_FAILURE;
    }
#elif defined(USE_EVENT_PORTS)
    backendFd_ = port_create();
    if (backendFd_ < 0) {
        ereport(LOG_FAILURE, "EventLoop: port_create failed: %s", strerror(errno));
        return PR_FAILURE;
    }
#endif

    // O_NONBLOCK matters on both ends.  The loop drains the pipe by reading
    // until read() fails, which on a blocking fd would park the loop thread
    // forever once the pipe ran dry; and wakeup() must never block a worker
    // thread just because a wakeup byte is already pending.
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) < 0) {
        ereport(LOG_FAILURE, "EventLoop: pipe2 failed: %s", strerror(errno));
        close(backendFd_);
        backendFd_ = -1;
        return PR_FAILURE;
    }
    wakeupReadFd_ = pipefd[0];
    wakeupWriteFd_ = pipefd[1];

#if defined(USE_EPOLL)
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = NULL;  // NULL distinguishes the wakeup pipe
    if (epoll_ctl(backendFd_, EPOLL_CTL_ADD, wakeupReadFd_, &ev) < 0) {
        ereport(LOG_FAILURE, "EventLoop: epoll_ctl wakeup failed: %s", strerror(errno));
        close(backendFd_);
        backendFd_ = -1;
        close(wakeupReadFd_);
        close(wakeupWriteFd_);
        wakeupReadFd_ = wakeupWriteFd_ = -1;
        return PR_FAILURE;
    }
#elif defined(USE_KQUEUE)
    struct kevent kev;
    EV_SET(&kev, wakeupReadFd_, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(backendFd_, &kev, 1, NULL, 0, NULL) < 0) {
        ereport(LOG_FAILURE, "EventLoop: kqueue wakeup add failed: %s", strerror(errno));
        close(backendFd_);
        backendFd_ = -1;
        close(wakeupReadFd_);
        close(wakeupWriteFd_);
        wakeupReadFd_ = wakeupWriteFd_ = -1;
        return PR_FAILURE;
    }
#elif defined(USE_EVENT_PORTS)
    if (port_associate(backendFd_, PORT_SOURCE_FD, wakeupReadFd_, POLLIN, NULL) < 0) {
        ereport(LOG_FAILURE, "EventLoop: event ports wakeup associate failed: %s", strerror(errno));
        close(backendFd_);
        backendFd_ = -1;
        close(wakeupReadFd_);
        close(wakeupWriteFd_);
        wakeupReadFd_ = wakeupWriteFd_ = -1;
        return PR_FAILURE;
    }
#endif

    return this->start(PR_USER_THREAD, PR_UNJOINABLE_THREAD);
}

void EventLoop::shutdownLoop(void)
{
    terminating_ = PR_TRUE;
    wakeup();
    // The event-loop thread is unjoinable; shutdown relies on close() of
    // backendFd_ to force the wait call to return.
    if (backendFd_ >= 0) {
        close(backendFd_);
        backendFd_ = -1;
    }
    if (wakeupReadFd_ >= 0) {
        close(wakeupReadFd_);
        wakeupReadFd_ = -1;
    }
    if (wakeupWriteFd_ >= 0) {
        close(wakeupWriteFd_);
        wakeupWriteFd_ = -1;
    }
}

void EventLoop::wakeup(void)
{
    if (wakeupWriteFd_ >= 0) {
        char c = 1;
        ssize_t r = write(wakeupWriteFd_, &c, 1);
        (void)r;
    }
}

PRBool EventLoop::linkTimeoutLocked(Connection* conn)
{
    // Defensive: a connection that is somehow registered twice without an
    // intervening removal would otherwise splice the list into a cycle.
    unlinkTimeoutLocked(conn);

    PRBool wasEmpty = (timeoutHead_ == NULL);

    conn->evPrev = timeoutTail_;
    conn->evNext = NULL;
    if (timeoutTail_)
        timeoutTail_->evNext = conn;
    else
        timeoutHead_ = conn;
    timeoutTail_ = conn;

    // The loop blocks indefinitely while it owns nothing, and adding an fd to
    // the backend does not interrupt that wait when the fd is idle -- which is
    // exactly the case here.  The caller pokes it, once it has dropped the lock,
    // so the loop does not wake straight into contention for it.
    return wasEmpty;
}

void EventLoop::unlinkTimeoutLocked(Connection* conn)
{
    // Both links are NULL for the sole element of the list as well as for a
    // connection that was never linked, so the head pointer is what tells the
    // two apart.  Getting this wrong would corrupt the list on double removal,
    // which the teardown paths do perform.
    if (conn != timeoutHead_ && !conn->evPrev && !conn->evNext)
        return;

    if (conn->evPrev)
        conn->evPrev->evNext = conn->evNext;
    else
        timeoutHead_ = conn->evNext;

    if (conn->evNext)
        conn->evNext->evPrev = conn->evPrev;
    else
        timeoutTail_ = conn->evPrev;

    conn->evPrev = NULL;
    conn->evNext = NULL;
}

int EventLoop::waitTimeoutMs(void)
{
    // With an empty registry there is no deadline to service, so waiting
    // indefinitely is both correct and cheaper than a periodic wakeup.
    // addConnection() breaks the wait when that stops being true.
    XP_Lock(&timeoutLock_);
    PRBool empty = (timeoutHead_ == NULL);
    XP_Unlock(&timeoutLock_);

    return empty ? -1 : EVENT_LOOP_SWEEP_MS;
}

void EventLoop::reapExpired(void)
{
    if (!idleTimeout_)
        return;

    PRIntervalTime now = PR_IntervalNow();
    int found;

    do {
        Connection* expired[EVENT_LOOP_MAX_EXPIRE_PER_PASS];
        found = 0;

        XP_Lock(&timeoutLock_);
        Connection* conn = timeoutHead_;
        while (conn && found < EVENT_LOOP_MAX_EXPIRE_PER_PASS) {
            Connection* next = conn->evNext;
            // Compare as a signed difference so the test stays correct across
            // the wraparound of NSPR's 32-bit interval clock.
            if (conn->eventDeadline != 0 &&
                (PRInt32)(now - conn->eventDeadline) >= 0) {
                unlinkTimeoutLocked(conn);
                expired[found++] = conn;
            }
            conn = next;
        }
        XP_Unlock(&timeoutLock_);

        for (int i = 0; i < found; i++) {
            Connection* c = expired[i];

            ereport(LOG_VERBOSE,
                    "EventLoop: idle timeout conn=%p fd=%p", c, c->fd);

            // Deregister before handing the connection on: the handler takes
            // ownership, so the loop must not be able to report a further
            // event for this fd.  The registry unlink above already happened and
            // unlinking is idempotent, so this only removes the fd from the
            // backend and adjusts the connection count.
            //
            // If it reports that the loop was not watching the fd after all, the
            // connection was taken from us between the sweep and here, and
            // expiring it would be expiring somebody else's connection.
            if (!takeConnection(c))
                continue;

            timeoutsReaped_++;

            // The loop's reference came back with the connection and is now
            // this sweep's to account for: onTimeout() takes it over, and with
            // no handler to hand it to there is nothing left holding the
            // connection, so it has to be dropped here or the struct would
            // never return to the pool.
            if (c->eventHandler)
                c->eventHandler->onTimeout(c);
            else
                c->releaseDispatch();
        }
    } while (found == EVENT_LOOP_MAX_EXPIRE_PER_PASS);
}

PRStatus EventLoop::addConnection(Connection* conn, EventHandler* handler,
                                  PRBool writable, PRBool armDeadline)
{
    if (backendFd_ < 0 || !conn || !conn->fd)
        return PR_FAILURE;

    int nfd = nativeFd(conn->fd);
    if (nfd < 0)
        return PR_FAILURE;

    conn->eventHandler = handler;
    conn->eventLoop = this;
    conn->fEventDriven = PR_TRUE;

    PRIntervalTime deadline = 0;
    if (idleTimeout_ && armDeadline) {
        deadline = PR_IntervalNow() + idleTimeout_;
        // Zero is the "no deadline" sentinel, so nudge past it on the rare
        // tick where the interval clock wraps to exactly that value.
        if (!deadline)
            deadline = 1;
    }

    // One lock spans the deadline, the registry link, the backend call and the
    // registered flag, so that "the loop is watching this fd" is a single fact
    // rather than three that can be observed out of step.  Registering a
    // connection that is already registered is a legitimate call -- it is how an
    // HTTP/2 session that was watched while busy arms its idle deadline once its
    // streams finish -- and it must update the registration rather than fail.
    //
    // The deadline and the registry link go in before the fd can generate events.
    // In the other order the loop could report readability, hand the connection
    // to a worker, and only then have this thread link it -- leaving the registry
    // able to expire a connection a worker already owns.
    //
    // A caller that asked for no deadline still joins the registry; the sweep
    // skips a zero deadline, and staying linked means a later re-registration can
    // arm one without the connection having to leave and rejoin.
    XP_Lock(&timeoutLock_);

    PRBool wasRegistered = conn->fInEventLoop;
    conn->eventDeadline = deadline;
    PRBool wasEmpty = linkTimeoutLocked(conn);

#if defined(USE_EPOLL)
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLRDHUP;
    if (writable)
        ev.events |= EPOLLOUT;
    ev.data.ptr = conn;
    int op = wasRegistered ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    if (epoll_ctl(backendFd_, op, nfd, &ev) < 0) {
        // The flag said one thing and the kernel another.  That should not
        // happen, but an fd is cheap to re-derive and a connection is not: retry
        // with the opposite operation rather than fail a registration -- and so
        // lose a live connection -- over a bookkeeping disagreement.
        int retry = (op == EPOLL_CTL_ADD) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        if ((errno == EEXIST || errno == ENOENT) &&
            epoll_ctl(backendFd_, retry, nfd, &ev) == 0) {
            ereport(LOG_VERBOSE,
                    "EventLoop: conn=%p registration state was stale; recovered",
                    conn);
        } else {
            ereport(LOG_FAILURE, "EventLoop: epoll_ctl %s failed: %s",
                    op == EPOLL_CTL_ADD ? "ADD" : "MOD", strerror(errno));
            unlinkTimeoutLocked(conn);
            conn->eventDeadline = 0;
            XP_Unlock(&timeoutLock_);
            return PR_FAILURE;
        }
    }
#elif defined(USE_KQUEUE)
    // EV_ADD on an existing filter updates it, so this is already idempotent.
    struct kevent kev;
    EV_SET(&kev, nfd, EVFILT_READ, EV_ADD, 0, 0, conn);
    if (kevent(backendFd_, &kev, 1, NULL, 0, NULL) < 0) {
        ereport(LOG_FAILURE, "EventLoop: kqueue ADD read failed: %s", strerror(errno));
        unlinkTimeoutLocked(conn);
        conn->eventDeadline = 0;
        XP_Unlock(&timeoutLock_);
        return PR_FAILURE;
    }
    // Register the write filter enabled only when the caller has output
    // pending; otherwise add it disabled so modifyConnection() can enable it.
    EV_SET(&kev, nfd, EVFILT_WRITE,
           EV_ADD | (writable ? EV_ENABLE : EV_DISABLE), 0, 0, conn);
    kevent(backendFd_, &kev, 1, NULL, 0, NULL);  // ignore failure
#elif defined(USE_EVENT_PORTS)
    // port_associate() on an already-associated fd replaces the association, so
    // this is already idempotent.
    int events = POLLIN | POLLERR | POLLHUP;
    if (writable)
        events |= POLLOUT;
    if (port_associate(backendFd_, PORT_SOURCE_FD, nfd, events, conn) < 0) {
        ereport(LOG_FAILURE, "EventLoop: event ports associate failed: %s", strerror(errno));
        unlinkTimeoutLocked(conn);
        conn->eventDeadline = 0;
        XP_Unlock(&timeoutLock_);
        return PR_FAILURE;
    }
#endif

    if (!wasRegistered) {
        conn->fInEventLoop = PR_TRUE;
        PR_AtomicIncrement(&numConnections_);

        // The loop holds a dispatch reference for exactly as long as it is
        // watching the fd.  That is what makes a Connection* taken out of an
        // event batch trustworthy: the struct cannot be closed and handed to a
        // new client while the loop can still name it, because recycling it
        // requires the count to reach zero, and this reference can only be
        // dropped through takeConnection() -- under the same lock that clears
        // fInEventLoop, so exactly one thread can do it.  Without it there is a
        // window between takeConnection() saying "yes, it was mine" and the
        // handler retaining, in which the connection could be closed, recycled
        // and re-registered, leaving the handler working on a different client.
        //
        // Taken only on the transition, not on every call: re-registering an
        // already-watched connection is a legitimate way to arm a deadline, and
        // must not add a reference nobody will drop.
        conn->retainDispatch();
    }

    XP_Unlock(&timeoutLock_);

    if (wasEmpty)
        wakeup();

    return PR_SUCCESS;
}

PRStatus EventLoop::modifyConnection(Connection* conn, PRBool readable, PRBool writable)
{
    if (backendFd_ < 0 || !conn || !conn->fd)
        return PR_FAILURE;

    int nfd = nativeFd(conn->fd);
    if (nfd < 0)
        return PR_FAILURE;

#if defined(USE_EPOLL)
    struct epoll_event ev;
    ev.events = EPOLLRDHUP;
    if (readable)
        ev.events |= EPOLLIN;
    if (writable)
        ev.events |= EPOLLOUT;
    ev.data.ptr = conn;
    if (epoll_ctl(backendFd_, EPOLL_CTL_MOD, nfd, &ev) < 0) {
        if (errno == ENOENT)
            return PR_FAILURE;
        ereport(LOG_FAILURE, "EventLoop: epoll_ctl MOD failed: %s", strerror(errno));
        return PR_FAILURE;
    }
#elif defined(USE_KQUEUE)
    struct kevent kev[2];
    int n = 0;
    EV_SET(&kev[n++], nfd, EVFILT_READ, readable ? EV_ENABLE : EV_DISABLE, 0, 0, conn);
    EV_SET(&kev[n++], nfd, EVFILT_WRITE, writable ? EV_ENABLE : EV_DISABLE, 0, 0, conn);
    if (kevent(backendFd_, kev, n, NULL, 0, NULL) < 0) {
        ereport(LOG_FAILURE, "EventLoop: kqueue MOD failed: %s", strerror(errno));
        return PR_FAILURE;
    }
#elif defined(USE_EVENT_PORTS)
    int events = POLLERR | POLLHUP;
    if (readable)
        events |= POLLIN;
    if (writable)
        events |= POLLOUT;
    // Event ports are one-shot; re-associate with new events.
    if (port_dissociate(backendFd_, PORT_SOURCE_FD, nfd) < 0 && errno != ENOENT) {
        ereport(LOG_FAILURE, "EventLoop: event ports dissociate failed: %s", strerror(errno));
    }
    if (port_associate(backendFd_, PORT_SOURCE_FD, nfd, events, conn) < 0) {
        ereport(LOG_FAILURE, "EventLoop: event ports re-associate failed: %s", strerror(errno));
        return PR_FAILURE;
    }
#endif
    return PR_SUCCESS;
}

PRStatus EventLoop::removeConnection(Connection* conn)
{
    if (!conn)
        return PR_FAILURE;

    // For callers that do not want the reference takeConnection() hands back.
    // Dropping it can perform a deferred close, so a caller that goes on to
    // touch the connection must hold a reference of its own -- or use
    // takeConnection() directly and release it when it has finished.
    if (takeConnection(conn))
        conn->releaseDispatch();

    return PR_SUCCESS;
}

PRBool EventLoop::takeConnection(Connection* conn)
{
    if (!conn)
        return PR_FALSE;

    // Same single lock as addConnection(), and for the same reason.  Removing a
    // connection that is not registered is a legitimate call -- more than one
    // path can conclude the loop should stop watching an fd, and they need not
    // agree on which of them got there first -- so it succeeds and changes
    // nothing.  Making the count follow the recorded flag rather than the call is
    // what keeps it honest under those duplicate removals.
    XP_Lock(&timeoutLock_);

    // Leave the registry whatever else is true of this connection: the registry
    // must never outlive the loop's ownership, or a sweep could expire a
    // connection that has already been recycled.
    unlinkTimeoutLocked(conn);
    conn->eventDeadline = 0;

    PRBool wasRegistered = conn->fInEventLoop;
    conn->fInEventLoop = PR_FALSE;

    if (!wasRegistered) {
        XP_Unlock(&timeoutLock_);
        return PR_FALSE;
    }

    // A closed fd leaves the backend by itself, so a connection whose socket has
    // already gone is still correctly accounted for as removed here.
    int nfd = (backendFd_ >= 0 && conn->fd) ? nativeFd(conn->fd) : -1;
    if (nfd >= 0) {
#if defined(USE_EPOLL)
        epoll_ctl(backendFd_, EPOLL_CTL_DEL, nfd, NULL);
#elif defined(USE_KQUEUE)
        struct kevent kev[2];
        EV_SET(&kev[0], nfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        EV_SET(&kev[1], nfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        kevent(backendFd_, kev, 2, NULL, 0, NULL);
#elif defined(USE_EVENT_PORTS)
        port_dissociate(backendFd_, PORT_SOURCE_FD, nfd);
#endif
    }

    PR_AtomicDecrement(&numConnections_);
    XP_Unlock(&timeoutLock_);
    return PR_TRUE;
}

PRUint64 EventLoop::getEventsProcessed(void) const
{
    return eventsProcessed_;
}

PRUint32 EventLoop::getNumConnections(void) const
{
    return (PRUint32)numConnections_;
}

PRUint64 EventLoop::getTimeoutsReaped(void) const
{
    return timeoutsReaped_;
}

void EventLoop::run(void)
{
    if (backendFd_ < 0)
        return;

    while (!terminating_ && backendFd_ >= 0) {
        // Bound the wait whenever there is a deadline to service.  Expired
        // connections are reaped after the event batch below, never before it:
        // a connection reported in this batch is about to be dispatched, and
        // closing it here would leave the batch holding a dangling pointer.
        int waitMs = waitTimeoutMs();

#if defined(USE_EPOLL)
        static const int MAX_EVENTS = 64;
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(backendFd_, events, MAX_EVENTS, waitMs);
        if (nfds < 0) {
            if (errno == EINTR)
                continue;
            if (!terminating_)
                ereport(LOG_FAILURE, "EventLoop: epoll_wait failed: %s", strerror(errno));
            break;
        }

        // Only when something actually happened: with a bounded wait this is
        // otherwise one log line per second for as long as the server is idle.
        if (nfds > 0)
            ereport(LOG_VERBOSE,
                    "EventLoop::run epoll_wait returned nfds=%d", nfds);

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.ptr == NULL) {
                // Wakeup pipe: drain it
                char buf[16];
                while (read(wakeupReadFd_, buf, sizeof(buf)) > 0) {}
                continue;
            }

            Connection* conn = (Connection*)events[i].data.ptr;
            if (!conn)
                continue;

            eventsProcessed_++;

            ereport(LOG_VERBOSE,
                    "EventLoop::run event conn=%p events=0x%x",
                    conn, events[i].events);

            // Hard errors (EPOLLERR, EPOLLHUP without data) are fatal.
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                if (!(events[i].events & EPOLLIN)) {
                    if (conn->eventHandler)
                        conn->eventHandler->onError(conn);
                    continue;
                }
            }

            // Dispatch at most one callback per event.  Every handler takes
            // ownership of the connection (it removes the fd from the loop and
            // hands the connection to a worker, or tears it down), so a second
            // callback for the same event would dispatch a connection this
            // loop no longer owns -- with EPOLLIN|EPOLLOUT both set that means
            // two workers processing one connection concurrently.
            //
            // Prefer reading available data even if EPOLLRDHUP is set.
            // A client that sends a request and immediately half-closes
            // (Connection: close) will set both EPOLLIN and EPOLLRDHUP.
            if (events[i].events & EPOLLIN) {
                if (conn->eventHandler)
                    conn->eventHandler->onReadable(conn);
            } else if (events[i].events & EPOLLOUT) {
                if (conn->eventHandler)
                    conn->eventHandler->onWritable(conn);
            } else if (events[i].events & EPOLLRDHUP) {
                // Remote hung up with no pending data to read.
                if (conn->eventHandler)
                    conn->eventHandler->onError(conn);
            }
        }
#elif defined(USE_KQUEUE)
        static const int MAX_EVENTS = 64;
        struct kevent events[MAX_EVENTS];
        struct timespec kqTimeout;
        struct timespec* kqTimeoutPtr = NULL;
        if (waitMs >= 0) {
            kqTimeout.tv_sec = waitMs / 1000;
            kqTimeout.tv_nsec = (long)(waitMs % 1000) * 1000000L;
            kqTimeoutPtr = &kqTimeout;
        }
        int nfds = kevent(backendFd_, NULL, 0, events, MAX_EVENTS, kqTimeoutPtr);
        if (nfds < 0) {
            if (errno == EINTR)
                continue;
            if (!terminating_)
                ereport(LOG_FAILURE, "EventLoop: kevent failed: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].udata == NULL) {
                // Wakeup pipe: drain it
                char buf[16];
                while (read(wakeupReadFd_, buf, sizeof(buf)) > 0) {}
                continue;
            }

            Connection* conn = (Connection*)events[i].udata;
            if (!conn)
                continue;

            eventsProcessed_++;

            if (events[i].flags & (EV_ERROR | EV_EOF)) {
                if (conn->eventHandler)
                    conn->eventHandler->onError(conn);
                continue;
            }

            if (events[i].filter == EVFILT_READ) {
                if (conn->eventHandler)
                    conn->eventHandler->onReadable(conn);
            } else if (events[i].filter == EVFILT_WRITE) {
                if (conn->eventHandler)
                    conn->eventHandler->onWritable(conn);
            }
        }
#elif defined(USE_EVENT_PORTS)
        static const int MAX_EVENTS = 64;
        port_event_t events[MAX_EVENTS];
        unsigned int nget = 1;
        timespec_t portTimeout;
        timespec_t* portTimeoutPtr = NULL;
        if (waitMs >= 0) {
            portTimeout.tv_sec = waitMs / 1000;
            portTimeout.tv_nsec = (long)(waitMs % 1000) * 1000000L;
            portTimeoutPtr = &portTimeout;
        }
        if (port_get_n(backendFd_, events, MAX_EVENTS, &nget,
                       portTimeoutPtr) < 0) {
            if (errno == EINTR)
                continue;
            // ETIME just means the bounded wait expired before the requested
            // minimum number of events arrived.  nget still reports how many
            // did arrive, so fall through and process them, then sweep.
            if (errno != ETIME) {
                if (!terminating_)
                    ereport(LOG_FAILURE, "EventLoop: port_get_n failed: %s", strerror(errno));
                break;
            }
        }

        for (unsigned int i = 0; i < nget; i++) {
            if (events[i].portev_user == NULL) {
                // Wakeup pipe: drain it
                char buf[16];
                while (read(wakeupReadFd_, buf, sizeof(buf)) > 0) {}
                // Re-associate wakeup pipe for next time
                port_associate(backendFd_, PORT_SOURCE_FD, wakeupReadFd_, POLLIN, NULL);
                continue;
            }

            Connection* conn = (Connection*)events[i].portev_user;
            if (!conn)
                continue;

            eventsProcessed_++;

            if (events[i].portev_events & (POLLERR | POLLHUP | POLLNVAL)) {
                if (conn->eventHandler)
                    conn->eventHandler->onError(conn);
                continue;
            }

            // Dispatch at most one callback per event; see the epoll backend
            // for why handling both POLLIN and POLLOUT would hand the same
            // connection to two workers.
            if (events[i].portev_events & POLLIN) {
                if (conn->eventHandler)
                    conn->eventHandler->onReadable(conn);
            } else if (events[i].portev_events & POLLOUT) {
                if (conn->eventHandler)
                    conn->eventHandler->onWritable(conn);
            }
        }
#endif

        // The event batch is fully drained, so every connection still in the
        // registry is one this loop owns and nobody else is touching.
        reapExpired();
    }
}
