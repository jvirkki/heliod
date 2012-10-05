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

#ifndef _DAEMONSESSION_H_ 
#define _DAEMONSESSION_H_ 

#include "nspr.h"
#include "NsprWrap/Thread.h"                 // Thread class
#include "NsprWrap/CriticalSection.h"        // CriticalSection class

#include "httpdaemon/libdaemon.h"            // HTTPDAEMON_DLL
#include "private/pprio.h"
#include "frame/domain.h"
#include "httpdaemon/internalstats.h"
#include "httpdaemon/statssession.h"
#include "httpdaemon/connqueue.h"             // ConnectionQueue class
#include "definesEnterprise.h"

#define SESSION_DEAD_KEY        0x87654321

class DaemonSession;
class PollManager;
class HttpRequest;
class ListenSocket;

extern "C" void DaemonSessionClock(void* context);

/**
 * The DaemonSession class represents the "worker thread" of the new
 * Acceptor-Worker thread model that was implemented for iWS5.0.
 *
 * <code>Acceptor</code> threads <code>accept()</code> connections and
 * enqueue these connections onto a threadsafe queue 
 * (<code>ConnectionQueue)</code> from which <code>DaemonSession</code> threads
 * dequeue the connections. The DaemonSession threads then read the request
 * from the connection and process the request. If necessary, the DaemonSession
 * threads may hand over the connection to the keep-alive handling subsystem.
 * When the keep-alive subsystem detects that there is pending data on a
 * connection, it enqueues the connection onto the threadsafe queue from which
 * the next available DaemonSession thread will dequeue it and process the
 * pending request.
 *
 * There will typically be a small number of <code>Acceptor</code> threads
 * and a larger pool of <code>DaemonSession</code> threads.
 *
 * Some of the static methods of this class implement Session management
 * functionality.
 *
 * @author  $Author: us14435 $
 * @version $Revision: 1.1.2.27.4.1.2.3.14.10.8.5 $
 */

class HTTPDAEMON_DLL DaemonSession : public Thread, public StatsSession
{
    public:

        // Initialize thread- and timeout-related constants
        static void ConfigureEarly(const ServerXMLSchema::Server& server);

        // Initialize connection-related constants
        static void ConfigureLate(const ServerXMLSchema::KeepAlive& keepAlive, PRInt32 nMaxKeepAliveConnections, PRInt32 nMaxQueued);

        // Module initialization
        static PRStatus Initialize(void);

        // Module termination/cleanup
        static PRStatus Terminate(void);

        static DaemonSession* StartNewSession(void);

        DaemonSession(ListenSocket* listenSocket);

        ~DaemonSession (void);

        void run(void);

        /**
         * Starts additonal DaemonSession threads if necessary.
         *
         * No additional threads will be created if the number of 
         * <code>DaemonSession</code> threads has already reached the
         * limit or if the number of "idle" <code>DaemonSession</code> threads
         * are more than the number of pending connections in the
         * <code>ConnectionQueue</code>.
         *
         * @returns The number of new threads started.
         */
        static PRUint32 PostThreads(void);

        // Indicate whether the current connection should be kept alive
        PRBool RequestKeepAlive(PRBool fKeepAlive);

        // Misc accessors
        const char* GetServerIP(void) const;
        int GetServerPort(void) const;
        PRNetAddr * GetLocalAddress(void);
        PRNetAddr * GetRemoteAddress(void);
        pool_handle_t* GetThreadPool(void);
        const char* GetClientIP(void) const;
        PRBool DisableTCPDelay(void);

        /**
         * Information about the connection associated with the request
         * being serviced by this thread.
         */
        Connection* conn;

        /**
         * Per-DaemonSession storage, exposed through NSAPISession.
         */
        SessionThreadData thread_data;

        /* Access to timeout intervals */
        static PRIntervalTime GetIOTimeout(void);
        static PRUint64 GetRqBodyTimeout(void);

        static PRUint32 GetKeepAliveTimeout(void);
        static void SetKeepAliveTimeout(const PRUint32 seconds);

        /**
         * Returns a pointer to the thread-safe queue onto which Acceptor/
         * KeepAlive threads enqueue incoming/returning connection and from 
         * which DaemonSession threads dequeue Connections for request
         * processing.
         *
         * Currently, there is just a single such queue.
         */
        static ConnectionQueue* GetConnQueue(void);


        PRBool isSecure(void) const;
        PRBool isValid(void) const ;

        /**
         * Returns the maximum number of <code>DaemonSession</code> threads
         * that are allowed to run in this configuration.
         */
        static PRInt32 GetMaxSessions(void);
        
        /**
         * Returns the total number of <code>DaemonSession</code> threads
         * that are currently running.
         */
        static PRInt32 GetTotalSessions(void);

        /**
         * Returns the total number of connections that are being serviced by 
         * <code>DaemonSession</code> threads
         */
        static PRInt32 GetActiveSessions(void);

        /**
         * Get keepalive stats
         */
        static PRStatus GetKeepAliveInfo(keepAliveInfo *info);

        /**
         * Returns the input buffer size, i.e. the size of the largest request
         */
        static PRInt32 GetBufSize();

private:
        pool_handle_t* pool_;
        netbuf             *inbuf;      // information about input

        /**
         * Information about the current request i.e. the request
         * being processed by this thread.
         */
        HttpRequest* request_;

        /**
         * ListenSocket this DaemonSession accepts Connections from.  If NULL,
         * the DaemonSession obtains Connections from connQueue_.
         */
        ListenSocket* listenSocket_;

        /**
         * sizeof buffer for initial read
         */
        static PRInt32 bufSize_;

        static PRIntervalTime ioTimeoutInterval_;
        static PRUint32 ioTimeoutSeconds_;
        static PRIntervalTime rqHdrTimeoutInterval_;
        static PRUint64 rqBodyTimeoutSeconds_;
        static PRIntervalTime closeTimeoutInterval_;

        static PRUint32 keepAliveTimeout_;
        static PRBool fPollInDaemonSession_;

        static PRBool fTestedTCPNagle;
        static PRBool fSetTCPNagleReq;


        /**
         * The maximumum number of DaemonSession instances (i.e threads) that
         * will be created by this server.
         */
        static PRInt32 nMaxSessions_;

        /**
         * The number of DaemonSession instances (i.e threads) that
         * will be created by this server upon startup.
         */
        static PRInt32 nMinSessions_;

        /**
         * The maximum number of concurrent DaemonSession threads that will be
         * will be used to poll keep-alive connections.
         *
         * The "KeepAliveThrottle" field of magnus.conf can be used to specify
         * this value.
         */
        static PRInt32 nMaxKeepAliveSessions_;

        /**
         * The maximum number of concurrent keep-alive connections the server
         * will handle.
         */
        static PRInt32 nMaxKeepAliveConnections_ ;

        /**
         * The number of dedicated keep-alive threads the keep-alive poll
         * handler will use.
         */
        static PRInt32 nKeepAliveThreads_;

        /**
         * The amount of time the keep-alive poll handler will wait between
         * polls.
         */
        static PRIntervalTime intervalKeepAlivePoll_;

        /**
         * The maximum number of connections that the connection queue can
         * accomodate.
         */
        static PRInt32 nMaxQueued_;

        /**
         * A count of the total number of DaemonSession instances that
         * have been created.
         *
         * Must be changed only when <code>sessionMgmtLock_</code> is acquired.
         */
        static PRInt32 nTotalSessions_;

        /**
         * A count of the DaemonSession instances that are currently
         * processing requests.
         *
         * Idle session count = <code>nTotalSessions_ - nActiveSessions_</code>
         */
        static PRInt32 nActiveSessions_;

        /**
         * A count of the DaemonSession instances that are currently
         * polling keep-alive connections.
         */
        static PRInt32 nKeepAliveSessions_;

        /**
         * Number of times a keep-alive connection timed out while being polled
         * by a DaemonSession thread
         */
        static PRInt32 nKeepAliveTimeouts_;

        /**
         * Time-averaged values for nActiveSessions_
         */
        static struct loadavg avgActiveSessions_;

        /**
         * Time-averaged values for nTotalSessions_ - nActiveSessions_
         */
        static struct loadavg avgIdleSessions_;

        /**
         * Time-averaged values for nKeepAliveSessions__
         */
        static struct loadavg avgKeepAliveSessions_;

        /**
         * Thread-safe queue onto which Acceptor/KeepAlive threads enqueue 
         * incoming/returning connection and from which DaemonSession threads
         * dequeue Connections for request processing
         */
        static ConnectionQueue* connQueue_;

        /**
         * Keep-Alive poll handler
         */
        static PollManager* pollManager_;

        const char *serverIP;
        char serverIP_[256];
        int serverPort;

        /**
         * Protects against multiple threads executing in PostThreads() and
         * also synchronizes changes to <code>nTotalSessions_</code>
         */
        static CriticalSection* sessionMgmtLock_;

        // Function to get Connection from connQueue_ or listenSocket_
        PRBool GetConnection(PRIntervalTime timeout);

        // Function to get initial request data from client
        PRBool GetConnection(void);

        /**
         * Returns PR_TRUE if the socket is still open and has received a 
         * keepalive request.  Returns PR_FALSE if no keepalive request was
         * received.
         *
         * Side effects:
         *    If returning PR_FALSE, closes the socket before returning.
         */
        PRBool HandleKeepAlive(void);

        /**
         * Close the socket associated with the connection and release
         * the reference to its Configuration
         */
        void CloseConnection(void);

        netbuf* CreateInputBuffer(void);

        /**
         * Once-per-second callback used to update averages.
         */
        static void Clock();

friend void DaemonSessionClock(void* context);
};

inline
pool_handle_t*
DaemonSession::GetThreadPool(void)
{
    return pool_;
}

inline
const char*
DaemonSession::GetClientIP(void) const
{
    return conn->remoteIP.buf;
}

inline
const char*
DaemonSession::GetServerIP(void) const
{
    return serverIP;
}

inline
int
DaemonSession::GetServerPort(void) const
{
    return serverPort;
}
 
inline
PRNetAddr *
DaemonSession::GetLocalAddress(void)
{
    return &conn->localAddress;
}

inline
PRNetAddr *
DaemonSession::GetRemoteAddress(void)
{
    return &conn->remoteAddress;
}

inline
PRUint64
DaemonSession::GetRqBodyTimeout(void)
{
    return rqBodyTimeoutSeconds_;
}

inline
PRIntervalTime
DaemonSession::GetIOTimeout(void)
{
    return ioTimeoutInterval_;
}

inline
PRUint32
DaemonSession::GetKeepAliveTimeout(void)
{
    return keepAliveTimeout_;
}


inline
PRBool
DaemonSession::isValid(void) const
{
    PRBool res = PR_TRUE;
    if (!request_)
        res = PR_FALSE;
    return res;
}

inline
ConnectionQueue*
DaemonSession::GetConnQueue(void)
{
    return connQueue_;
}

inline
PRInt32
DaemonSession::GetBufSize()
{
    return bufSize_;
}

#endif // _DaemonSession_h_
