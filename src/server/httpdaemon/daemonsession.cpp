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

//
// DaemonSession.cpp
//
#if defined( XP_WIN32 ) || defined( AIX )
#include <time.h>
#endif

#include "definesEnterprise.h"
#include "prio.h"
#include "prerror.h"
#include "prlock.h"
#include "private/pprio.h"

#include <nss.h>
#include "frame/conf.h"
#include "frame/log.h"
#include "base/systhr.h"
#include "base/servnss.h"
#if defined(LINUX)
#include "base/pool.h"
#endif

#include "httpdaemon/daemonsession.h"
#include "httpdaemon/EventLoop.h"
#include "httpdaemon/H2Session.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/connqueue.h"
#include "httpdaemon/WebServer.h"                // WebServer::isTerminating()
#include "httpdaemon/ListenSockets.h"            // ListenSockets class
#include "httpdaemon/ListenSocket.h"
#include "httpdaemon/logmanager.h"
#include "base/sslconf.h"
#include "pollmanager.h"
#include "httpdaemon/vsconf.h"

#include "safs/nsfcsafs.h"
#include "safs/flexlog.h"
#include "safs/addlog.h"
#include "time/nstime.h"

#ifdef XP_UNIX
#include "base/unix_utils.h"                    // max_fdget
#endif

static const PRInt32 CLEANUP_BUFFER_SIZE = 65536;

/* Controlled by magnus.conf "KeepAliveTimeout" (in seconds) */
PRUint32 DaemonSession::keepAliveTimeout_;

PRUint32 DaemonSession::ioTimeoutSeconds_;
PRIntervalTime DaemonSession::ioTimeoutInterval_;
PRIntervalTime DaemonSession::rqHdrTimeoutInterval_;
PRUint64 DaemonSession::rqBodyTimeoutSeconds_;
PRIntervalTime DaemonSession::closeTimeoutInterval_;

// Keep-Alive poll handler
PollManager* DaemonSession::pollManager_ = NULL;

// Acceptor/KeepAlive <===> Worker connection queue
ConnectionQueue* DaemonSession::connQueue_ = NULL;

PRInt32 DaemonSession::bufSize_; // sizeof buffer for initial read

PRBool DaemonSession::fPollInDaemonSession_ = PR_FALSE;
PRBool DaemonSession::fTestedTCPNagle = PR_FALSE;
PRBool DaemonSession::fSetTCPNagleReq = PR_TRUE;

PRInt32 DaemonSession::nMaxSessions_ = 0;
PRInt32 DaemonSession::nMinSessions_ = 0;
PRInt32 DaemonSession::nMaxKeepAliveSessions_ = 0;
PRInt32 DaemonSession::nMaxKeepAliveConnections_ = 0;
PRInt32 DaemonSession::nKeepAliveThreads_ = 0;
PRIntervalTime DaemonSession::intervalKeepAlivePoll_ = 0;
PRInt32 DaemonSession::nMaxQueued_ = 0;
PRInt32 DaemonSession::nTotalSessions_ = 0;
PRInt32 DaemonSession::nActiveSessions_ = 0;
PRInt32 DaemonSession::nKeepAliveSessions_ = 0;
PRInt32 DaemonSession::nKeepAliveTimeouts_ = 0;

struct loadavg DaemonSession::avgActiveSessions_;
struct loadavg DaemonSession::avgIdleSessions_;
struct loadavg DaemonSession::avgKeepAliveSessions_;

CriticalSection* DaemonSession::sessionMgmtLock_ = NULL;

void
DaemonSession::ConfigureEarly(const ServerXMLSchema::Server& server)
{
    // DaemonSession thread pool bounds (formerly RqThrottle and RqThrottleMin)
    if (server.threadPool.enabled) {
        // DaemonSession thread pool (and connection queue) enabled
        if (server.threadPool.getMaxThreads()) {
            // Administrator explicitly specified an upper bound
            nMaxSessions_ = *server.threadPool.getMaxThreads();
            nMinSessions_ = WebServer::GetConcurrency(server.threadPool.getMinThreads());
            if (nMinSessions_ > nMaxSessions_)
                nMinSessions_ = nMaxSessions_;
        } else {
            // Administrator didn't explicitly specify an upper bound but may
            // have specified a lower bound
            nMaxSessions_ = WebServer::GetConcurrency(server.threadPool.getMaxThreads());
            if (nMaxSessions_ < 128)
                nMaxSessions_ = 128;
            nMinSessions_ = WebServer::GetConcurrency(server.threadPool.getMinThreads());
            if (nMaxSessions_ < nMinSessions_)
                nMaxSessions_ = nMinSessions_;
        }
    } else {
        // No separate DaemonSession thread pool.  The acceptor threads will be
        // the DaemonSessions.
        nMaxSessions_ = 0;
        nMinSessions_ = 0;
    }

    // Size of the read buffer (formerly HeaderBufferSize)
    bufSize_ = server.http.requestHeaderBufferSize;

    // Configure the NSPR and NSAPI read timeouts (formerly AcceptTimeout).
    // We need to use net_nspr_interval_to_nsapi_timeout() because NSAPI
    // timeouts are weird.
    ioTimeoutInterval_ = server.http.ioTimeout.getPRIntervalTimeValue();
    ioTimeoutSeconds_ = net_nspr_interval_to_nsapi_timeout(ioTimeoutInterval_);
    rqHdrTimeoutInterval_ = server.http.requestHeaderTimeout.getPRIntervalTimeValue();
    rqBodyTimeoutSeconds_ = server.http.requestBodyTimeout.getSecondsValue();
    
    // CloseTimeout
#if defined(HPUX) || defined(SOLARIS)
    int closeTimeoutSeconds = conf_getboundedinteger("CloseTimeout", 0, 60, 0);
#else
    int closeTimeoutSeconds = conf_getboundedinteger("CloseTimeout", 0, 60, 2);
#endif
    closeTimeoutInterval_ = PR_SecondsToInterval(closeTimeoutSeconds);

    // Tell NSAPI about the size of the default NSAPI thread pool
    if (nMaxSessions_ ) {
        // Default NSAPI thread pool is the DaemonSession thread pool
        pool_maxthreads = nMaxSessions_;
        pool_minthreads = nMinSessions_;
    } else {
        // Default NSAPI thread pool is the set of all listeners' acceptor
        // threads
        int numAcceptorThreads = 0;
        for (int i = 0; i < server.getHttpListenerCount(); i++) {
            if (server.getHttpListener(i)->enabled)
                numAcceptorThreads += WebServer::GetConcurrency(server.getHttpListener(i)->getAcceptorThreads());
        }
        pool_maxthreads = numAcceptorThreads;
        pool_minthreads = 1;
    }
}

void
DaemonSession::ConfigureLate(const ServerXMLSchema::KeepAlive& keepAlive, PRInt32 nMaxKeepAliveConnections, PRInt32 nMaxQueued)
{
    // Keep-alive
    if (keepAlive.enabled) {
        // Keep-alive connection inactivity timeout
        keepAliveTimeout_ = keepAlive.timeout.getSecondsValue();

        // Number of keep-alive connections we will support
        nMaxKeepAliveConnections_ = nMaxKeepAliveConnections;

        // Maximum number of DaemonSession threads we'll use for keep-alives
        // (note that we honour KeepAliveThrottle even when the DaemonSession
        // thread pool is disabled; in this case KeepAliveThrottle applies to
        // the acceptor threads)
        nMaxKeepAliveSessions_ = conf_getboundedinteger("KeepAliveThrottle",
                                                        0,
                                                        nMaxKeepAliveConnections_,
                                                        nMaxKeepAliveConnections_);

        // Number of dedicated keep-alive threads in the keep-alive subsystem
        nKeepAliveThreads_ = WebServer::GetConcurrency(keepAlive.getThreads());

        // If there isn't a separate DaemonSession thread pool (i.e. requests
        // are processed directly by the acceptor threads), we don't want any
        // dedicated keep-alive threads.
        PRInt32 nMaxKeepAliveThreads = nMaxKeepAliveConnections_;
        if (nMaxSessions_ == 0)
            nMaxKeepAliveThreads = 0;
        if (nKeepAliveThreads_ > nMaxKeepAliveThreads)
            nKeepAliveThreads_ = nMaxKeepAliveThreads;

        // Time between keep-alive subsystem poll() calls
        intervalKeepAlivePoll_ = keepAlive.pollInterval.getPRIntervalTimeValue();

        // Keep-alive is enabled
        log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonSessionKATimeOut2),
                    keepAliveTimeout_);
    } else {
        // Keep-alive is disabled
        log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonSessionKADisabled));
    }

    // Connection queue
    if (nMaxSessions_) {
        nMaxQueued_ = nMaxQueued;
    } else {
        nMaxQueued_ = 0;
    }
}

// Initialize global state for DaemonSession
PRStatus
DaemonSession::Initialize(void)
{
    sessionMgmtLock_ = new CriticalSection();
    if (!sessionMgmtLock_)
    {
        ereport(LOG_FAILURE, "Unable to create session management mutex lock");
        return PR_FAILURE;
    }

    // Create the queue that connects the DaemonSession thread pool to acceptor
    // threads and keep-alive threads
    PR_ASSERT(connQueue_ == NULL);
    if (nMaxSessions_) {
        PRUint32 nMaxConnections = nMaxQueued_ +
                                   nMaxSessions_ +
                                   nMaxKeepAliveConnections_;

        ereport(LOG_VERBOSE,
                "%d connection maximum (%d queued, %d active, %d keep-alive)",
                (int)nMaxConnections,
                (int)nMaxQueued_,
                (int)nMaxSessions_,
                (int)nMaxKeepAliveConnections_);

        connQueue_ = new ConnectionQueue(nMaxQueued_, nMaxConnections);
        if (!connQueue_) {
            ereport(LOG_FAILURE, "Unable to create connection queue");
            return PR_FAILURE;
        }
    } else {
        PR_ASSERT(nKeepAliveThreads_ == 0);
        ereport(LOG_VERBOSE, "connection queue disabled");
    }

    // Forcibly set the connection queue because ListenSockets::getInstance()
    // may have been invoked previously and at that time 
    // DaemonSession::GetConnQueue() would have returned NULL
    ListenSockets* ls = ListenSockets::getInstance();
    ls->setConnectionQueue(connQueue_);

    if (nKeepAliveThreads_ > 0) {
        // Initialize poll array and the keep-alive poll thread
        pollManager_ = new PollManager();
        if (!pollManager_) {
            ereport(LOG_FAILURE, "Unable to create KeepAlive poll handler");
            return PR_FAILURE;
        }

        pollManager_->Initialize(nMaxKeepAliveConnections_,
                                 nKeepAliveThreads_,
                                 intervalKeepAlivePoll_);
    }

    // Track average number of DaemonSessions
    loadavg_init(&avgActiveSessions_);
    loadavg_init(&avgIdleSessions_);
    loadavg_init(&avgKeepAliveSessions_);
    ft_register_cb(&DaemonSessionClock, NULL);

    return PR_SUCCESS;
}

PRStatus
DaemonSession::Terminate(void)
{
    ft_unregister_cb(&DaemonSessionClock, NULL);

    if (pollManager_)
    {
        // Stop all the keep-alive poll threads
        pollManager_->Terminate();
        log_ereport(LOG_VERBOSE,
                    XP_GetAdminStr(DBT_DaemonPxTerminatedKeepAlives));
    }

    if (connQueue_)
    {
        // Close connections and wake up DaemonSessions
        connQueue_->Terminate();
    }
    
    // be defensive and give all sessions a chance to exit even if some sessions
    // wait for the earlier of
    // - all sessions gone
    // - time up
    int waitTime = PR_IntervalToSeconds(WebServer::getTerminateTimeout());

    ereport(LOG_VERBOSE, "Waiting %d seconds for %d sessions to terminate",
            waitTime, nTotalSessions_);

    for (int nsecs = waitTime; nTotalSessions_ > 0 && nsecs > 0; nsecs--)
    {
        systhread_sleep(1000);
    }

    if (nTotalSessions_ == 0)
        log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxSessionCleanedUp));
    else
    {
        log_ereport(LOG_WARN, XP_GetAdminStr(DBT_DaemonPxSessionInProgress),
                    nTotalSessions_);
    }

    return PR_SUCCESS;
}

DaemonSession::DaemonSession(ListenSocket* listenSocket) : Thread("DaemonSession"),
    inbuf(NULL), serverIP(NULL), pool_(NULL)
{
    listenSocket_ = listenSocket;
    if (listenSocket_) {
        // This DaemonSession will accept Connections from the ListenSocket
        PR_ASSERT(connQueue_ == NULL);
        PR_ASSERT(pollManager_ == NULL);
        conn = new Connection;
    } else {
        // This DaemonSession will obtain Connections from the queue
        PR_ASSERT(connQueue_ != NULL);
        conn = NULL;
    }

    request_ = new HttpRequest(this);

    thread_data.slots = NULL;
    thread_data.count = 0;
}

DaemonSession::~DaemonSession(void)
{
    // Ensure session_destroy_thread uses this DaemonSession's storage and
    // not a stale HttpRequest::CurrentRequest() that may be left behind by
    // an HTTP/2 stream processing path.
    HttpRequest *currentRequest = HttpRequest::CurrentRequest();
    HttpRequest::SetCurrentRequest(request_);
    session_destroy_thread(NULL);
    HttpRequest::SetCurrentRequest(currentRequest);

    delete request_;
    request_ = NULL;
}

/* GetConnection(PRIntervalTime timeout)
 *
 * Obtain a Connection from the ConnectionQueue or ListenSocket associated
 * with this DaemonSession thread.
 *
 * Returns PR_TRUE if conn has been set to a valid Connection*;
 * Returns PR_FALSE otherwise.
 */
PRBool
DaemonSession::GetConnection(PRIntervalTime timeout)
{
    if (connQueue_) {
        conn = connQueue_->GetReady(timeout);
        return (conn != NULL);
    }

    PR_ASSERT(conn != NULL);

    PRFileDesc *socketAccept;

    // XXX the following code is duplicated in Acceptor::run

    for (;;) {
        socketAccept = listenSocket_->accept(conn->remoteAddress, timeout);

        if (socketAccept != NULL)
            break;

        if (WebServer::isTerminating() || Thread::wasTerminated())
            return PR_FALSE;

        PRInt32 err = PR_GetError();

        if (err == PR_IO_TIMEOUT_ERROR)
            return PR_FALSE;

        // Load balancer "pings" will trigger this
        if (err == PR_CONNECT_ABORTED_ERROR)
            continue;

        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_DaemonSessionErrorAcceptConn),
                system_errmsg());

        //
        // If the process is out of file descriptors (ulimit reached or
        // denial-of-service attack, then sleep for a second and try to
        // recover
        //
        if (err == PR_PROC_DESC_TABLE_FULL_ERROR)
            systhread_sleep(1000);
    }

    if (conn->create(socketAccept, &conn->remoteAddress, listenSocket_) != PR_SUCCESS) {
        PR_Close(socketAccept);
        return PR_FALSE;
    }

    return PR_TRUE;
}

/* GetConnection(void)
 *
 * Will get the first "chunk" of data from the client.
 * If this is a keepalive connection, will just do a read from the socket.
 * If no connection is open, will do an accept()
 *
 * Returns TRUE if it got data and is ready to proceed; 
 * Returns FALSE otherwise.
 */
PRBool
DaemonSession::GetConnection(void)
{
    PRBool flagGotConnection = PR_FALSE;

    // If we have cached stats we'd like to flush...
    if (!StatsSession::isFlushed()) {
        // If a connection might be ready and StatsManager is busy...
        if ((!connQueue_ || connQueue_->GetLength() > 0) && StatsManager::isLocked()) {
            // Wait up to StatsManager::getUpdateInterval() for a connection
            flagGotConnection = GetConnection(StatsManager::getUpdateInterval());
        }

        // If we haven't got a connection yet...
        if (!flagGotConnection) {
            // There aren't any connections on the horizon.  Take this
            // opportunity to flush our cached stats before we block 
            // indefinitely below.
            StatsSession::flush();
        }
    }

    // If we don't have a connection yet, wait indefinitely for one to arrive
    if (!flagGotConnection) {
        flagGotConnection = GetConnection(PR_INTERVAL_NO_TIMEOUT);
        if (!flagGotConnection)
            return PR_FALSE;
    }

    return prepareConnection();
}

/* prepareConnection()
 *
 * Prepare the connection currently stored in this->conn for request
 * processing.  This is shared by the blocking DaemonSession::run() loop
 * and the event-driven processConnection() path.
 */
void
DaemonSession::SetServerAddrFromConn(void)
{
    // Derive the server IP/port the same way prepareConnection() does, but
    // without any of the connection setup (SSL, stats, initial read).  This is
    // safe to call for the synthetic per-stream HTTP/2 connections, whose
    // lsConfig and localAddress are copied from the real connection they
    // arrived on.
    ListenSocketConfig* lsc = conn->lsConfig;
    if (lsc && lsc->hasExplicitIP()) {
        serverIP = lsc->ip;
    } else {
        // Listening to INADDR_ANY: derive from the connection's local address.
        net_addr_to_string(&conn->localAddress, serverIP_, sizeof(serverIP_));
        serverIP = serverIP_;
    }
    serverPort = PR_ntohs(PR_NetAddrInetPort(&conn->localAddress));
}

PRBool
DaemonSession::prepareConnection(void)
{
    // Set flagNew if this is a newly accepted connection
    PRBool flagNew = (conn->fNewlyAccepted);
    conn->fNewlyAccepted = PR_FALSE;

    // Get the ListenSocketConfig associated with this connection
    ListenSocketConfig* lsc = conn->lsConfig;
    PR_ASSERT(lsc);

    // Get the server address/port
    SetServerAddrFromConn();

    // Track session statistics
    beginClient(&conn->remoteAddress);

    // If this is a newly accepted SSL connection...
    if (flagNew && conn->sslconfig) {
        // Enable input buffering because 1. we want to peek at the SSL client
        // hello before passing it to NSS and 2. NSS does a lot of small reads
        net_buffer_input(conn->fd, 1024);

        // Check if the client mistakenly sent HTTP to an SSL listener
        PRBool flagEnableSSL = PR_TRUE;
        char hello[3];
        if (net_peek(conn->fd, hello, sizeof(hello), inbuf->rdtimeout) == 3 &&
            hello[0] >= 'A' && hello[0] <= 'Z' &&
            hello[1] >= 'A' && hello[1] <= 'Z' &&
            hello[2] >= 'A' && hello[2] <= 'Z')
        {
            log_ereport(LOG_VERBOSE,
                        "Received malformed SSL client hello from %s (Possible non-SSL request on SSL listener)",
                        conn->remoteIP.buf);

            if (conn->lsConfig->handleProtocolMismatch)
                flagEnableSSL = PR_FALSE;
        }

        // Enable SSL (note that the handshake doesn't happen yet)
        if (flagEnableSSL)
            conn->enableSSL();
    }

    PRIntervalTime timeout = rqHdrTimeoutInterval_;
    if (timeout > ioTimeoutInterval_)
        timeout = ioTimeoutInterval_;

    // Check to see if there's anything on the socket
    // for new SSL connections, the handshake will happen right here
    // we leave room for a trailing null for HttpHeader::ParseRequest()
    // XXX elving clean up handling of async inbuf, reuse HttpHeader
    if (conn->async.inbuf.cursize > 0) {
        // XXX elving don't copy
        // XXX elving maybe there should be one buffer per DaemonSession and ConnectionQueue entry, not one per Connection?
        PR_ASSERT(inbuf->maxsize >= conn->async.inbuf.cursize);
        memcpy(inbuf->inbuf, conn->async.inbuf.buf, conn->async.inbuf.cursize);
        inbuf->cursize = conn->async.inbuf.cursize;
        conn->async.inbuf.cursize = 0;
    } else if (conn->fEventDriven && !conn->sslconfig) {
        // Event-driven path: epoll already confirmed data is available, so
        // try a non-blocking recv first.  This avoids blocking the worker
        // thread on NSPR's internal poll() which can introduce delays under
        // concurrent load.
        //
        // This fast path is restricted to plaintext connections. On an SSL
        // socket, the first PR_Recv() also drives the TLS handshake, which
        // can require additional network round trips the peer hasn't sent
        // yet. NSS surfaces that condition on a PR_INTERVAL_NO_WAIT recv as
        // PR_IO_TIMEOUT_ERROR (-5990), not PR_WOULD_BLOCK_ERROR, so the
        // WOULD_BLOCK-triggered fallback below never ran and the connection
        // was silently torn down mid-handshake (IO_TIMEOUT is a "quiet"
        // close error below). Blocking recv with the full timeout drives
        // the handshake correctly, as it always has for the blocking path.
        inbuf->cursize = PR_Recv(conn->fd, (void *)(inbuf->inbuf),
                                 inbuf->maxsize - 1, 0, PR_INTERVAL_NO_WAIT);
        if (inbuf->cursize < 0 && PR_GetError() == PR_WOULD_BLOCK_ERROR) {
            // Rare: epoll said data ready but NSPR disagrees.  Fall back to
            // a blocking recv with the full timeout.
            inbuf->cursize = PR_Recv(conn->fd, (void *)(inbuf->inbuf),
                                     inbuf->maxsize - 1, 0, timeout);
        }
    } else {
        inbuf->cursize = PR_Recv(conn->fd, (void *)(inbuf->inbuf),
                                 inbuf->maxsize - 1, 0, timeout);
    }
    if (inbuf->cursize <= 0)
    {
        if (inbuf->cursize < 0)
        {
            int err = PR_GetError();
            INTnet_cancelIO(conn->fd);
            if ((err != PR_IO_TIMEOUT_ERROR) &&
                (err != PR_CONNECT_RESET_ERROR) &&
                (err != PR_END_OF_FILE_ERROR))
            {
                log_ereport(LOG_FAILURE,
                            XP_GetAdminStr(DBT_DaemonSessionErrorRecvReq),
                            conn->remoteIP.buf, system_errmsg());
            }
        }
        else
        {
            // inbuf->cursize == 0
            // Normal EOF
        }
        CloseConnection();
        return PR_FALSE;
    }

    // If this is a newly accepted non-SSL connection...
    if (flagNew && !conn->sslconfig) {
        // Check whether the client mistakenly sent SSL to a non-SSL listener
        if (servssl_maybe_client_hello(inbuf->inbuf, inbuf->cursize)) {
            log_ereport(LOG_VERBOSE,
                        "Received unexpected SSL client hello from %s (Possible SSL request on non-SSL listener)",
                        conn->remoteIP.buf);

            if (conn->lsConfig->handleProtocolMismatch) {
                // Send an SSL protocol_version alert
                unsigned char alert[] = { 21, 3, 1, 0, 2, 2, 70 };
                PR_Send(conn->fd, alert, sizeof(alert), 0, PR_INTERVAL_NO_WAIT);

                CloseConnection();
                return PR_FALSE;
            }
        }
    }

    if (!flagNew) recordKeepaliveHit();

    inbuf->sd = conn->fd;
    inbuf->pos = 0;
    return PR_TRUE;
}

void
DaemonSession::run(void)
{
    pool_ = pool_create();
    PR_SetThreadPrivate(getThreadMallocKey(), pool_);

    while (WebServer::isTerminating() == PR_FALSE && Thread::wasTerminated() == PR_FALSE)
    {
        // Track session statistics
        setMode(STATS_THREAD_IDLE);

        /*
         * We might still have an input buffer if the last iteration
         * didn't actually result in a session.  Normally there will
         * have been a session, and the input buffer will be gone with
         * all the other memory that the session acquired through MALLOC.
         */
        if (inbuf == NULL) {
            // allocate associated i/o buffer for request
            if ((inbuf = CreateInputBuffer()) == NULL) {
                PR_ASSERT(0);   // this really should not happen...
                break;
            }
        }

        // grab a connection
        // this can either be a brand new one right from the acceptor thread's factory
        // or a keepalive connection that happens to have new data.
        if (GetConnection() == PR_FALSE) {
            // start over...
            continue;
        }

        PR_ASSERT(pool_ == PR_GetThreadPrivate(getThreadMallocKey()));

        processOneConnection();

        PostThreads();
    }

    //
    // this DaemonSession is just about to end.
    // right now, the only way this can happen is when the server shuts down.
    //

    PR_SetThreadPrivate(getThreadMallocKey(), NULL);
    pool_destroy(pool_);
    pool_ = NULL;

    StatsSession::flush();

    SafeLock guard(*sessionMgmtLock_);
    PR_AtomicDecrement(&nTotalSessions_);
}

/* processOneConnection()
 *
 * Process a single connection after it has been obtained. Used by both
 * the blocking DaemonSession::run() loop and the event-driven
 * processConnection() path.
 */
void
DaemonSession::processOneConnection(void)
{
    //
    // initialize the DeamonSession for request processing
    //
    if (!request_->StartSession(inbuf))
        return;

    PR_AtomicIncrement(&nActiveSessions_);

    //
    // continue processing requests as long as KeepAlive is requested
    // and data is available immediately
    //
    // if the client requests keepalive, but does not come up with data immediately,
    // we give the connection to the keepalive subsystem and reuse the DaemonSession.
    // when some data arrives later on, we'll get it back via GetConnection.
    //
    PRBool fReturnToEventLoop = PR_FALSE;

    do {
        PR_ASSERT(conn->fd != NULL);

        if (inbuf->cursize <= 0)
            break;

        // Figure out how much longer we can wait for the request header
        PRIntervalTime timeout = rqHdrTimeoutInterval_;
        if (timeout != PR_INTERVAL_NO_TIMEOUT) {
            PRIntervalTime elapsed = conn->elapsed();
            if (timeout > elapsed) {
                timeout -= elapsed;
            } else {
                timeout = 0;
            }
        }

        // HttpRequest::HandleRequest will call RequestKeepAlive() to set
        // conn->fKeepAlive as appropriate
        PRBool fSocketOpen = request_->HandleRequest(inbuf, timeout);
        if (fSocketOpen == PR_FALSE) {
            ereport(LOG_VERBOSE,
                    "processOneConnection: HandleRequest returned socket closed, conn=%p",
                    conn);
            conn->fd = NULL;
            break;
        }

        ereport(LOG_VERBOSE,
                "processOneConnection: after HandleRequest conn=%p fKeepAliveReservation=%d fEventDriven=%d",
                conn, conn->fKeepAliveReservation, conn->fEventDriven);

        if (!conn->fKeepAliveReservation)
            break;

        // Event-driven connections are returned to the event loop after a
        // single request instead of being handed to the blocking keep-alive
        // subsystem. The fd must stay open and must not be recycled here;
        // the caller (processConnection) re-registers it with the event
        // loop once this function returns.
        if (conn->fEventDriven) {
            fReturnToEventLoop = PR_TRUE;
            break;
        }

        // HandleKeepAlive will set conn = NULL if it passes the
        // Connection * to the keep-alive subsystem
        if (!HandleKeepAlive())
            break;

    } while (WebServer::isTerminating() == PR_FALSE && Thread::wasTerminated() == PR_FALSE);

    ereport(LOG_VERBOSE,
            "processOneConnection: exit loop conn=%p fReturnToEventLoop=%d fd=%p",
            conn, fReturnToEventLoop, conn ? conn->fd : NULL);

    if (conn && !fReturnToEventLoop)
        CloseConnection();

    request_->EndSession();

    /*
     * After a session ends, everything acquired through MALLOC is
     * released, including the input buffer.
     */
    inbuf = NULL;
    pool_recycle(pool_, NULL);

    PR_AtomicDecrement(&nActiveSessions_);
}

/* Per-worker-thread persistent DaemonSession for the event-driven path.
 *
 * HttpRequest::StartSession() performs one-time thread-specific
 * initialization (guarded by its myTpdIndex thread-private) that caches
 * the HttpRequest pointer and its memory pool for the LIFETIME OF THE
 * THREAD.  Creating a transient DaemonSession/HttpRequest per connection
 * left that thread-private pointing at a deleted HttpRequest; the next
 * HttpRequest on the same worker thread then skipped its initialization,
 * ran HandleRequest() with an uninitialized pool, and crashed in
 * pool_mark().  Each worker thread therefore reuses a single persistent
 * DaemonSession, exactly like the blocking model's one-session-per-thread
 * design.
 */
static PRUintn workerSessionIndex_;
static PRCallOnceType workerSessionOnce_;

static PRStatus initWorkerSessionIndex(void)
{
    return PR_NewThreadPrivateIndex(&workerSessionIndex_, NULL);
}

DaemonSession*
DaemonSession::GetWorkerSession(void)
{
    if (PR_CallOnce(&workerSessionOnce_, initWorkerSessionIndex) != PR_SUCCESS)
        return NULL;

    DaemonSession* session =
        (DaemonSession*)PR_GetThreadPrivate(workerSessionIndex_);
    if (!session) {
        session = new DaemonSession(NULL);
        session->pool_ = pool_create();
        PR_SetThreadPrivate(getThreadMallocKey(), session->pool_);
        PR_SetThreadPrivate(workerSessionIndex_, session);
    }
    return session;
}

/* processConnection(Connection*)
 *
 * Static entry point for the event-driven I/O path. Processes a single
 * connection on the calling thread without consuming a long-lived
 * DaemonSession thread.
 */
void
DaemonSession::processConnection(Connection* connection)
{
    if (!connection || !connection->fd)
        return;

    DaemonSession* sessionPtr = GetWorkerSession();
    if (!sessionPtr)
        return;
    DaemonSession& session = *sessionPtr;
    session.conn = connection;

    // Reuse the input buffer left over from the previous connection, if
    // any (mirrors DaemonSession::run()).
    if (session.inbuf == NULL) {
        session.inbuf = session.CreateInputBuffer();
        if (session.inbuf == NULL) {
            session.conn = NULL;
            return;
        }
    }

    // Reset the connection timestamp so that elapsed() measures from the
    // moment data arrived (event dispatch) rather than from the original
    // accept time.  Without this, connections that idle in epoll would have
    // their request-header timeout budget consumed by the idle period.
    if (StatsManager::isProfilingEnabled()) {
        connection->ticks = PR_IntervalNow();
    } else {
        connection->ticks = ft_timeIntervalNow();
    }

    // Note: the event-dispatch path leaves the fd in whatever blocking mode
    // the OS accepted it with (typically blocking, since PR_Accept inherits
    // from the listen socket).  prepareConnection() uses PR_Recv with a
    // timeout, which works correctly in both blocking and non-blocking modes
    // under NSPR.

    if (session.prepareConnection() == PR_FALSE) {
        // prepareConnection() already closed the connection; keep the
        // session, pool, and input buffer for the next connection on this
        // thread (mirrors the failed-GetConnection path in run()).
        //
        // prepareConnection() has already called beginClient(), which puts this
        // thread's stats slot in "request" mode.  The blocking model leaves a
        // mode behind too, but its run() loop resets it to idle at the top of
        // the next iteration; a worker-pool thread has no such loop, so every
        // exit from here has to reset it by hand.  Otherwise the slot keeps the
        // mode of the last request the thread served for as long as the thread
        // lives, and perfdump reports the thread as a live session -- with a
        // blank URI/VS/method/age, because endProcessing() clears those fields
        // but not the mode.  Under the event model that shows up as a Sessions
        // list full of half-empty rows and an inflated active-session count.
        session.setMode(STATS_THREAD_IDLE);
        session.conn = NULL;
        return;
    }

    // Detect HTTP/2 by TLS ALPN or by the HTTP/2 connection preface in the
    // already-read initial bytes.  The preface fallback is useful for testing
    // before ALPN h2 selection is fully working.
    static const char h2Preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    const size_t h2PrefaceLen = sizeof(h2Preface) - 1;
    PRBool isH2 = PR_FALSE;
    if (connection->sslconfig != NULL) {
        SSLNextProtoState npState;
        unsigned char proto[16];
        unsigned int protoLen = 0;
        if (SSL_GetNextProto(connection->fd, &npState, proto, &protoLen,
                             sizeof(proto)) == SECSuccess) {
            if ((npState == SSL_NEXT_PROTO_NEGOTIATED ||
                 npState == SSL_NEXT_PROTO_SELECTED) &&
                protoLen == 2 && memcmp(proto, "h2", 2) == 0) {
                isH2 = PR_TRUE;
            }
        }
    }
    if (!isH2 && session.inbuf != NULL &&
        (unsigned int)session.inbuf->cursize >= 3 &&
        memcmp(session.inbuf->inbuf, h2Preface, 3) == 0) {
        isH2 = PR_TRUE;
    }

    ereport(LOG_VERBOSE,
            "processConnection: inbuf cursize=%d isH2=%d",
            session.inbuf ? session.inbuf->cursize : -1,
            isH2);

    if (isH2) {
        // Hand the connection to a persistent H2Session owned by the
        // Connection.  processConnection() consumes the pre-read bytes
        // synchronously and then either parks the connection back in the
        // event loop (releasing this worker while the client is idle) or
        // tears it down.  Either way `connection` must not be dereferenced
        // after this call: it may already have been closed and recycled.
        heliod::H2Session::processConnection(
            connection,
            reinterpret_cast<const uint8_t*>(session.inbuf->inbuf),
            session.inbuf->cursize);
        // Release everything allocated for this connection (including the
        // input buffer) but keep the persistent per-thread pool.
        session.inbuf = NULL;
        pool_recycle(session.pool_, NULL);
        session.conn = NULL;
        session.setMode(STATS_THREAD_IDLE);
        session.flush();
        return;
    }

    session.processOneConnection();

    // After processOneConnection(), session.conn is NULL if the connection
    // was closed (CloseConnection → destroy → AddUnused).  In that case the
    // Connection object may already be recycled by the acceptor on another
    // thread, so we must NOT dereference the original `connection` pointer.
    // processOneConnection() already recycled the pool and cleared inbuf.
    Connection* conn = session.conn;
    session.conn = NULL;

    // If the connection is still open and event-driven, return it to the
    // event loop so the next request triggers another dispatch.
    if (conn && conn->fd && conn->fEventDriven && conn->eventLoop) {
        // Release the keep-alive reservation flag before re-registering;
        // the event loop manages idle-timeout independently and destroy()
        // asserts !fKeepAliveReservation.
        conn->fKeepAliveReservation = PR_FALSE;
        conn->fNewlyAccepted = PR_FALSE;

        conn->eventLoop->addConnection(conn, conn->eventHandler);
    }

    // This worker is about to go back to waiting for the next item on the queue;
    // its stats slot must say so.  See the comment at the prepareConnection()
    // failure above for why the event path has to do this at every exit.
    session.setMode(STATS_THREAD_IDLE);
    session.flush();
}

DaemonSession*
DaemonSession::StartNewSession(void)
{
    PR_ASSERT(connQueue_ != NULL && nMaxSessions_ != 0);

    PRStatus status = PR_SUCCESS;
    DaemonSession* session = NULL;
    PR_ASSERT(nTotalSessions_ <= nMaxSessions_);
    if (nTotalSessions_ <= nMaxSessions_)
    {
        session = new DaemonSession(NULL);
        if (session)
        {
            if (session->isValid())
            {
                status = session->start(PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD);
                if (status == PR_FAILURE)
                {
                    delete session;
                    session = NULL;
                }
            }
        }
    }

    return session;
}

// New KeepAlive handling
//    move this connection to the KeepAlive PollArray.
//    the main thread loop may go to accept new connection or check 
//    to see if there are keep-alive connections.
PRBool
DaemonSession::HandleKeepAlive(void)
{
    PR_ASSERT(!conn->fNewlyAccepted);
    PR_ASSERT(conn->fKeepAliveReservation);

    PRUint32 keepAliveTimeoutRemaining = keepAliveTimeout_;

    // Poll here in the DaemonSession if we're under light load
    if (fPollInDaemonSession_ && nKeepAliveSessions_ < nMaxKeepAliveSessions_) {
        PRInt32 nKeepAliveSessions = PR_AtomicIncrement(&nKeepAliveSessions_);
        PRBool flagConnectionPolledReady = PR_FALSE;

        if (nKeepAliveSessions <= nMaxKeepAliveSessions_) {
            // Release the Connection's Configuration reference
            ListenSocketConfig* lsConfig = conn->lsConfig;
            PRInt32 idConfig = lsConfig->getConfiguration()->getID();
            conn->lsConfig->unref();
            conn->lsConfig= NULL;

            // Track session statistics
            setMode(STATS_THREAD_KEEPALIVE);

            for (;;) {
                // Don't sleep for too long in case load spikes
                PRUint32 timeout = keepAliveTimeoutRemaining;
                if (timeout > 5)
                    timeout = 5;

                // Wait for some data
                int rv;
#ifdef XP_WIN32
                if (!net_is_timeout_safe()) {
                    PRPollDesc pd[1];
                    pd[0].fd = conn->fd;
                    pd[0].in_flags = PR_POLL_READ;
                    rv = PR_Poll(pd, 1, PR_SecondsToInterval(timeout));
                    if (rv == 1) {
                        rv = net_read(conn->fd, inbuf->inbuf, inbuf->maxsize - 1, NET_INFINITE_TIMEOUT);
                    } else {
                        PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                        rv = -1;
                    }
                } else
#endif
                {
                    rv = net_read(conn->fd, inbuf->inbuf, inbuf->maxsize - 1, timeout);
                }
                if (rv > 0) {
                    recordKeepaliveHit();
                    inbuf->cursize = rv;
                    flagConnectionPolledReady = PR_TRUE;
                    break;
                }
                if (rv == 0) {
                    flagConnectionPolledReady = PR_TRUE;
                    break;
                }
                if (PR_GetError() != PR_IO_TIMEOUT_ERROR) {
                    flagConnectionPolledReady = PR_TRUE;
                    break;
                }

                keepAliveTimeoutRemaining -= timeout;

                // Track keep-alive timeouts
                if (keepAliveTimeoutRemaining == 0) {
                    PR_AtomicIncrement(&nKeepAliveTimeouts_);
                    break;
                }

                // Bail if the server is shutting down, load increases, or
                // the connection queue backs up
                if (WebServer::isTerminating() || Thread::wasTerminated())
                    break;
                if (!fPollInDaemonSession_)
                    break;
                PRInt32 nIdleSessions = nTotalSessions_ - nActiveSessions_;
                if (connQueue_ && connQueue_->GetLength() > nIdleSessions)
                    break;
            }

            // Track session statistics
            setMode(STATS_THREAD_IDLE);

            // Restore the Connection's Configuration reference
            Configuration *configuration = ConfigurationManager::getConfiguration();
            if (lsConfig && configuration && 
                idConfig == configuration->getID()) {
                // Restore our reference to the lsConfig
                conn->lsConfig = lsConfig;
                conn->lsConfig->ref();
            }
            else {
                configuration->unref();
                PR_AtomicDecrement(&nKeepAliveSessions_);
                RequestKeepAlive(PR_FALSE);
                return PR_FALSE;
            }
            configuration->unref();
        }

        PR_AtomicDecrement(&nKeepAliveSessions_);

        if (flagConnectionPolledReady) {
            RequestKeepAlive(PR_FALSE);
            return PR_TRUE;
        }

        if (keepAliveTimeoutRemaining == 0)
            return PR_FALSE;
    }

    // Put this connection into the KeepAlive poll array 
    if (pollManager_) {
        pollManager_->AddConnection(conn, keepAliveTimeoutRemaining);
        conn = NULL;
    }

    return PR_FALSE;
}

PRBool
DaemonSession::RequestKeepAlive(PRBool fKeepAlive)
{
    if (conn->fEventDriven) {
        // Event-driven connections do not use the blocking keep-alive poll
        // subsystem; their idle-wait is handled by the event loop. Grant or
        // revoke the reservation directly without consuming a slot from the
        // shared PollManager reservation pool.
        conn->fKeepAliveReservation = fKeepAlive;
    } else if (pollManager_) {
        if (fKeepAlive) {
            if (!conn->fKeepAliveReservation) {
                // Ask keep-alive subsystem for a keep-alive "reservation"
                conn->fKeepAliveReservation = pollManager_->RequestReservation();
            }
        } else {
            if (conn->fKeepAliveReservation) {
                // Release our existing keep-alive "reservation"
                pollManager_->ReleaseReservation();
                conn->fKeepAliveReservation = PR_FALSE;
            }
        }
    }
    return conn->fKeepAliveReservation;
}

PRStatus
DaemonSession::GetKeepAliveInfo(keepAliveInfo *info)
{
    PRStatus rv = PR_SUCCESS;

    memset(info, 0, sizeof(*info));

    info->tmoKeepAlive = keepAliveTimeout_;
    info->maxKeepAlives = nMaxKeepAliveConnections_;

    if (pollManager_)
        rv = pollManager_->GetKeepAliveInfo(info);

    info->numKeepAliveTimeouts += nKeepAliveTimeouts_;

    return rv;
}

netbuf *
DaemonSession::CreateInputBuffer(void)
{
    netbuf * nbuf = (netbuf *)pool_malloc(pool_, sizeof(netbuf));
    if (nbuf)
    {
        PRInt32 nBytes = bufSize_;

        nbuf->inbuf = (unsigned char *)pool_malloc(pool_, nBytes);
        nbuf->rdtimeout = ioTimeoutSeconds_;
        nbuf->pos = nBytes;
        nbuf->cursize = nBytes;
        nbuf->maxsize = nBytes;
        nbuf->errmsg = NULL;
    }
    return nbuf;
}

/*
 * disable the nagle handling (TCP_NODELAY) on a socket to improve
 * the speed of short writes
 */
PRBool
DaemonSession::DisableTCPDelay(void)
{
    PRSocketOptionData opt;
    PRStatus rv;

    /*
     * If this socket family doesn't support TCP_NODELAY, we don't need to do
     * anything
     */
    if (!conn->lsConfig->isNoDelaySupported())
    {
        return PR_TRUE;
    }

    /* TCP_NODELAY should have been set on the listen socket; on Solaris 
     * releases prior to 2.6 (or 2.5 with ISS patches) nagle isn't 
     * inherited by the accepted socket.  Test to see if the current 
     * machine passes this on
     */
    if (fTestedTCPNagle == PR_FALSE)
    {

        opt.option = PR_SockOpt_NoDelay;
        opt.value.no_delay = PR_FALSE;

        rv = PR_GetSocketOption(conn->fd, &opt);
        if (rv == PR_FAILURE)
        {
            ereport(LOG_WARN, "getsocktopt (TCP_NODELAY) failure (%s)",
                    system_errmsg());
        }
        else
        {
            if (opt.value.no_delay == PR_TRUE)
            {
                /* TCP_NODELAY is inherited */
                fSetTCPNagleReq = PR_FALSE;
            }
        }
        fTestedTCPNagle = PR_TRUE;
    }

    /*
     * If TCP_NODELAY is inherited, we don't need to do anything
     */
    if(fSetTCPNagleReq == PR_FALSE)
    {
        return PR_TRUE;
    }


    /* 
     * we have to set it each time
     */
    opt.option = PR_SockOpt_NoDelay;
    opt.value.no_delay = PR_TRUE;
    rv = PR_SetSocketOption(conn->fd, &opt);
    if (rv == PR_FAILURE)
    {
        static int  reportedError = 0;
        /* don't whine more than once otherwise we'll fill the log*/
        if (!reportedError)
        {
            ereport(LOG_WARN, 
                    "Unable to set TCP_NODELAY on connected socket (%s)",
                    system_errmsg());
            reportedError = 1;
        }
    }

    return PR_FALSE;
}


void
DaemonSession::CloseConnection(void)
{
    // Release any keep-alive reservation
    RequestKeepAlive(PR_FALSE);

    if (conn->fd) {
        // No more HTTP-level traffic on the socket
        conn->done();

        // HP seems to have a problem where under stress the server even after
        // clients have gone away seems to be in a 100% CPU utilization state.
        // We are sure that clients have gone away since netstat does not show
        // any connections from the client machine to the server.
        // However, glanceplus (gpm) tells us that these sockets are in the 
        // server's process fd table.
        // The server is in the PR_Recv below alternating between recv 
        // returning EAGAIN and poll returning that 1 descriptor is available 
        // for reading.

		// SOLARIS does not seem to have a problem with draining data off the
		// socket. Setting the CloseTimeout to 0 to avoid the extra system
		// calls on the drain.

        if (closeTimeoutInterval_ > 0) {
            // Shut socket down so client gets EOF on recv
            PR_Shutdown(conn->fd, PR_SHUTDOWN_SEND);

            // Read off residual data until we get EOF from client or
            // closeTimeoutInterval elapses
            PRIntervalTime epoch = ft_timeIntervalNow();
            PRIntervalTime elapsed = 0;
            while (elapsed < closeTimeoutInterval_) {
                PRIntervalTime timeout = closeTimeoutInterval_ - elapsed;

                static char buf[CLEANUP_BUFFER_SIZE];
                int rv = PR_Recv(conn->fd, buf, sizeof(buf), 0, timeout);
                if (rv < 1)
                    break;

                elapsed = ft_timeIntervalNow() - epoch;
            }
        }
    }

    conn->destroy();

    if (connQueue_)
        conn = NULL;
}

void
DaemonSession::SetKeepAliveTimeout(const PRUint32 seconds)
{
    /* Setting the timeout to zero disables keepalive */
    keepAliveTimeout_ = seconds;
}

PRBool
DaemonSession::isSecure(void) const
{
    return PR_FALSE;
}

PRUint32
DaemonSession::PostThreads(void)
{
    // If we've reached the limit for the number of sessions, then
    // there is nothing more to do
    if (nTotalSessions_ >= nMaxSessions_)
        return 0;

    PRInt32 nSessionsToAdd = Connection::countActive + 1 - nTotalSessions_;

    // If the number of unused DaemonSession threads is sufficient then
    // we don't create more threads
    if (nSessionsToAdd < 1)
        return 0;

    SafeLock guard(*sessionMgmtLock_);

    if (WebServer::isTerminating())
        return 0;

    nSessionsToAdd = Connection::countActive + 1 - nTotalSessions_;
    if (nSessionsToAdd < nMinSessions_ - nTotalSessions_)
        nSessionsToAdd = nMinSessions_ - nTotalSessions_;
    if (nSessionsToAdd > nMaxSessions_ - nTotalSessions_)
        nSessionsToAdd = nMaxSessions_ - nTotalSessions_;

    PRInt32 nThreads;
    for (nThreads = 0; nThreads < nSessionsToAdd; nThreads++)
    {
        PR_SetError(0, 0);
        if (StartNewSession() == NULL)
        {
            static PRBool logged = PR_FALSE;
            if (!logged)
            {
                ereport(LOG_FAILURE,
                        XP_GetAdminStr(DBT_DaemonSession_ThreadXOfYError),
                        nTotalSessions_ + 1,
                        nMaxSessions_,
                        system_errmsg());
                logged = PR_TRUE;
            }
            break;
        }
        nTotalSessions_++;
    }

    return nThreads;
}

PRInt32
DaemonSession::GetMaxSessions(void)
{
    return nMaxSessions_;
}

PRInt32
DaemonSession::GetMinSessions(void)
{
    return nMinSessions_;
}

PRInt32
DaemonSession::GetTotalSessions(void)
{
    return nTotalSessions_;
}

extern "C" void DaemonSessionClock(void* context)
{
    ((DaemonSession*)context)->Clock();
}

void
DaemonSession::Clock(void)
{
    PRInt32 nTotalSessions = nTotalSessions_;
    PRInt32 nActiveSessions = nActiveSessions_;
    PRInt32 nIdleSessions = nTotalSessions - nActiveSessions;
    if (nIdleSessions < 0)
        nIdleSessions = 0;

    loadavg_update(&avgActiveSessions_, nActiveSessions);
    loadavg_update(&avgIdleSessions_, nIdleSessions);
    loadavg_update(&avgKeepAliveSessions_, nKeepAliveSessions_);

    // Wait for the server to go online before starting new DaemonSessions
    static PRBool fFirstTick = PR_TRUE;
    if (fFirstTick == PR_TRUE) {
        if (request_is_server_active())
            fFirstTick = PR_FALSE;
        return;
    }

        PRInt32 nAvgActiveSessions = avgActiveSessions_.avgn[0] / LOADAVG_FSCALE;
        PRInt32 nAvgIdleSessions = avgIdleSessions_.avgn[0] / LOADAVG_FSCALE;
        PRInt32 nAvgActiveConnections = 0;
        PRInt32 nAvgQueueLength = 0;
        PRInt32 nQueueLength = 0;
        if (connQueue_) {
            nAvgActiveConnections = connQueue_->GetActiveAverages()->avgn[0] / LOADAVG_FSCALE;
            nAvgQueueLength = connQueue_->GetLengthAverages()->avgn[0] / LOADAVG_FSCALE;
            nQueueLength = connQueue_->GetLength();
        }
        PRInt32 nActiveConnections = Connection::countActive;

        static int invocations;
        invocations++;
#if 0
        if ((invocations % 30) == 0) {
            ereport(LOG_INFORM, "DaemonSession nAvgActiveSessions = %d, nAvgQueueLength = %d", nAvgActiveSessions, nAvgQueueLength);
        }

#endif
    // If we're configured to poll from DaemonSessions and the poll manager...
    if (nMaxKeepAliveSessions_ && pollManager_) {
        if (nTotalSessions > nAvgActiveConnections &&
            nTotalSessions > nActiveConnections &&
            nAvgIdleSessions > nAvgQueueLength &&
            nIdleSessions > nQueueLength)
        {
            if (fPollInDaemonSession_ == PR_FALSE)
                ereport(LOG_VERBOSE, "entering low latency mode");
            fPollInDaemonSession_ = PR_TRUE;
        } else {
            if (fPollInDaemonSession_ == PR_TRUE)
                ereport(LOG_VERBOSE, "entering high concurrency mode");
            fPollInDaemonSession_ = PR_FALSE;
        }
    } else if (nMaxKeepAliveSessions_) {
        fPollInDaemonSession_ = PR_TRUE;
    } else if (pollManager_) {
        fPollInDaemonSession_ = PR_FALSE;
    }

    PostThreads();

    if (pollManager_)
        pollManager_->Clock();
}
