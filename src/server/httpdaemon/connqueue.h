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

#ifndef _CONN_QUEUE_H_
#define _CONN_QUEUE_H_

#include "xp/xpsynch.h"
#include "httpdaemon/ListenSocketConfig.h"
#include "httpdaemon/httpheader.h"
#include "base/sslconf.h"
#include "base/loadavg.h"
#include "frame/accel.h"

class HttpHeader;
class ListenSocket;
class ConnectionQueue;

extern "C" void ConnectionQueueClock(void* context);

/**
 * This structure records information necessary for async operations on a
 * connection.
 */
struct ConnectionAsync
{
    /**
     * The OS-specific file descriptor for async operations, or -1 if async
     * operations are not supported.
     */
    int fd;

    /**
     * The input buffer used by the keep-alive threads for async operations.
     * Note that the buffer is only allocated when needed and may be NULL.
     */
    struct {
        unsigned char *buf;
        int cursize;
        int maxsize;
    } inbuf;

    /**
     * State for asynchronous accelerator cache operations.
     */
    AcceleratorConnectionAsyncState accel;
};

/**
 * This structure keeps information about every active socket connection.
 *
 * Instances of this structure are passed between the request handling
 * and the keep-alive subsystems.
 *
 * @since   iWS5.0
 */
class Connection
{
  public:
    /**
     * Construct an object that can be used for multiple connections
     */
    Connection();

    /**
     * Once allocated, a connection object is never destroyed
     */
    ~Connection();

    /**
     * Record connection creation
     */
    PRStatus create(PRFileDesc* fd, const PRNetAddr *addr, ListenSocket* ls);

    /**
     * Close the connection
     */
    void destroy();

    /**
     * Enable SSL/TLS
     */
    void enableSSL();

    /**
     * Indicate the connection is being aborted.  Called to ensure that NSS
     * doesn't block while trying to gracefully terminate an SSL connection.
     */
    void abort();

    /**
     * Indicate the connection timed out in the keep-alive subsystem.  Called
     * so we have a chance to update the NSS transmit timeout before it tries
     * to send the SSLv3 close notify alert.
     */
    void timeout();

    /**
     * Indicate there will be no more HTTP-level traffic on the socket.  Called
     * implicitly by destroy.
     */
    void done();

    /**
     * Return how much time has elapsed sinced the connection was added to or
     * removed from the connection queue.
     */
    PRIntervalTime elapsed() const;

    /**
     * The socket descriptor corresponding to the connection
     */
    PRFileDesc* fd;

    /**
     * The network address of the client (remote) socket
     *
     * This information is filled in by the <code>PR_Accept</code> call.
     */
    PRNetAddr remoteAddress;

    /**
     * The network address of the local socket
     *
     * This information is filled in by the <code>PR_GetSockName</code> call
     * (only if there are multiple virtual hosts).
     */
    PRNetAddr localAddress;

    /**
     * Nul-terminated string representation of the client (remote) IP address
     */
    struct {
        char buf[NET_ADDR_STRING_SIZE];
        int len;
    } remoteIP;

    /**
     * The properties of the "server" socket on which this connection
     * was <code>accept()</code>ed.
     */
    ListenSocketConfig* lsConfig;

    /**
     * Connection timestamp, set when the connection is added to or removed
     * from the queue
     */
    PRIntervalTime ticks;

    /**
     * Number of open connections
     */
    static PRInt32 countActive;

    /**
     * Highest number of simultaneous open connections
     */
    static PRInt32 peakActive;

    /**
     * The SSL configuration installed on the socket or NULL
     */
    const SSLSocketConfiguration *sslconfig;

    /**
     * The queue this connection should be returned to when it's destroyed.
     */
    ConnectionQueue *connQueue;

    /**
     * The next connection in a linked list of connections. This is currently
     * used by the keep-alive subsystem to maintain a list of connections
     * waiting to be added to the poll array and by the accelerator cache to
     * maintain a list of connections that need processing.
     */
    Connection *next;

    /**
     * The number of seconds the connection should be allowed to remain in
     * keep-alive.
     */
    PRUint32 keepAliveTimeout;

    /**
     * HTTP request parser for this connection
     */
    HttpHeader httpHeader;

    /**
     * Information necessary for async operations
     */
    ConnectionAsync async;

    /**
     * Miscellaneous connection-related flags
     */
    unsigned fNewlyAccepted : 1;
    unsigned fSSLEnabled : 1;
    unsigned fUncleanShutdown : 1;
    unsigned fKeepAliveReservation : 1;
};

/**
 * This class implements a circular queue of <code>Connection</code>
 * objects.
 *
 * Connections are fed to this thru one of two channels
 * 1. New accepted connections
 * 2. Keep-Alive conections that have become ready for recv
 * If we want to do a native NCA prt etc, this can serve as a common
 * point for the requests being accepted from the NCA daemon.
 * <p>
 * The queue has a maximum size specified at creation time.
 * When this queue is destructed, any connections that are open 
 * are closed.
 * <p>
 * This is not completely appropriate. We may want to take a ptr
 * to a callback function, that can then send service unavailable 
 * status.
 *
 * @since   iWS5.0
 */
class ConnectionQueue
{
  public:

    /**
     * Default constructor that specifies the maximum number of connections
     * that the queue can hold and the maximum number of connections that
     * can exist simultaneously.
     */
    ConnectionQueue(const PRUint32 maxQueueLength, const PRUint32 maxConnections);

    /**
     * Destroy the connection queue.
     */
    ~ConnectionQueue();

    /**
     * Closes all connections.
     */
    void Terminate();

    /**
     * Adds a ready connection to the queue.
     *
     * @param ready The connection to be added to the queue
     * @param returns <code>PR_SUCCESS</code> if the connection was added to
     *                the queue or <code>PR_SUCCESS</code> if the queue was
     *                full
     */
    PRStatus AddReady(Connection* ready);

    /**
     * Adds a number of ready connections to the queue.
     *
     * @param ready    An array of connections to be added to the queue
     * @param numReady The number of connections in the ready array
     * @param returns  The number of connections added to the queue.  This may
     *                 be less than the number in the ready array, in which
     *                 case the connections at the end of the array were not
     *                 added to the queue.
     */
    int AddReady(Connection** ready, int numReady);

    /**
     * Returns an unused connection.
     *
     * @returns <code>NULL</code> if all connections are in use or a valid
     *          <code>Connection*</code> otherwise.
     */
    Connection* GetUnused();

    /**
     * Release access to an unused connection previously returned by
     * <code>GetUnused</code> or <code>AddReadyGetUnused</code>.  This
     * function is also called from <code>Connection::destroy</code> which
     * is the normal way to dispose of a connection.
     */
    void AddUnused(Connection* unused);

    /**
     * Returns a ready connection from the queue.  If none is available, it 
     * will block.
     *
     * @param to The amount of time to wait for a connection.
     * @returns  <code>NULL</code> if the server is being shutdown or the
     *           timeout expired or a valid <code>Connection*</code>
     *           otheriwse.
     */
    Connection* GetReady(PRIntervalTime to);

    /**
     * Returns the number of times an unused connection was requested when all
     * connections were in use.
     */
    PRUint64 GetNumConnectionOverflows(void) const;

    /**
     * Returns the number of connections that are currently in the queue
     */
    PRUint32 GetLength(void) const;

    /**
     * Returns the peak number of connections queued simultaneously
     */
    PRUint32 GetPeakLength(void) const;

    /**
     * Returns the total number of NEW connections queued
     */
    PRUint64 GetTotalNewConnections(void) const;

    /**
     * Obtains the total number of ready connections queued and the number of
     * ticks those connections spent in the queue
     */
    void GetQueueingDelay(PRUint64* totalQueued, PRUint64* totalTicks);

    /**
     * Returns the maximum number of connections that the queue
     * can hold
     */
    PRUint32 GetMaxLength(void) const;

    /**
     * Returns a pointer to time-averaged queue lengths
     */
    const struct loadavg* GetLengthAverages(void) const;   

    /**
     * Returns a pointer to the time-averaged number of open connections
     */
    const struct loadavg* GetActiveAverages(void) const;

    /**
     * Log overflow message is appropriate
     */
    void logOverflow(void);
    
  private:

    /**
     * Once-per-second callback used to update moving averages.
     */
    void Clock();

    /**
     * The array of all <code>Connection</code> objects.
     */
    Connection* _conns;

    /**
     * The array of ready <code>Connection</code> pointers that, together with
     * _head and _tail, forms a queue using a circular array.
     */
    Connection** _queue;

    /**
     * The linked list of unused <code>Connection</code> pointers that.
     */
    Connection* _unused;

    /** 
     * Mutex that synchronizes access to <code>_queue</code>.
     */
    XPScopedLock _lockQueue;

    /**
     * The condition variable that is used to notify threads that
     * are waiting to remove ready connections from the list.
     *
     * If a thread tries to get a <code>Connection</code> from the
     * queue and the queue is currently empty, then the thread waits
     * on this variable.
     */
    XPScopedCondition _available; 

    /** 
     * Mutex that synchronizes access to <code>_unused</code>.
     */
    XPScopedLock _lockUnused;

    /**
     * The index of the first ready connection in _queue.
     */
    PRInt32 _head;

    /**
     * The index of the last ready connection in _queue.
     */
    PRInt32 _tail;

    /**
     * The number of ready connections currently in the queue.
     */
    PRUint32 _numItems;

    /**
     * The peak number of ready connections that have been stored in the queue.
     */
    PRUint32 _peak;

    /**
     * The total number of NEW connections that have been stored in the queue.
     */
    PRUint64 _totalNewConnections;

    /**
     * The total number of ready connections that have been removed from the
     * queue.
     */
    PRUint64 _totalDequeued;

    /**
     * The total number of ticks that now-dequeued ready connections spent in
     * the queue.
     */
    PRUint64 _totalTicks;

    /**
     * The maximum number of ready connections that can be stored in the
     * queue (i.e. the size of the <code>_queue</code> array).
     */
    PRUint32 _maxQueue;

    /**
     * The maximum number of connections that can be exist at one time (i.e.
     * the the size of the <code>_conn</code> array, the maximum number of
     * nodes in the <code>_unused</code> linked list, and the sum of the
     * maximum number of connections being serviced by request-processing
     * threads, in the queue, and in the keep-alive subsystem.)
     */
    PRUint32 _maxConn;

    /**
     * The number of times <code>GetUnused</code>, etc. failed because all
     * <code>Connection</code> objects were in use.
     */
    PRInt64 _numConnectionOverflows;

    /**
     * Indicates whether the queue is being terminated
     */
    PRBool  _terminating;

    /**
     * Indicates the number of <code>GetReady</code> calls that are waiting
     * for _available to be notified.
     */
    PRInt32 _numWaiters;

    /**
     * Average number of ready connections in the queue.
     */
    struct loadavg _avgItems;

    /**
     * Average number of open connections.
     */
    struct loadavg _avgActive;

   /**
    * The last time when the server logged connection overflow error.
    * This field is used to avoid logging overflow error repeatedly when 
    * system is overloaded. After the first error  message, the next 
    * overflow message is only logged after  300 seconds
    */
    time_t _lastOverflowLogTime;

friend void ConnectionQueueClock(void* context);
};

inline
PRUint32
ConnectionQueue::GetLength(void) const
{
    return _numItems;
}

inline
PRUint32
ConnectionQueue::GetPeakLength(void) const
{
    return _peak;
}

inline
PRUint32
ConnectionQueue::GetMaxLength(void) const
{
    return _maxQueue;
}

inline
PRUint64
ConnectionQueue::GetTotalNewConnections(void) const
{
    return _totalNewConnections;
}

inline
const struct loadavg*
ConnectionQueue::GetLengthAverages(void) const
{
    return &_avgItems;
}

inline
const struct loadavg*
ConnectionQueue::GetActiveAverages(void) const
{
    return &_avgActive;
}

inline
PRUint64
ConnectionQueue::GetNumConnectionOverflows(void) const
{
    return _numConnectionOverflows;
}

#endif /* _CONN_QUEUE_H_ */
