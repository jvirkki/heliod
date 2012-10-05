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


/*
 * PROPRIETARY/CONFIDENTIAL.  Use of this product is subject to license terms.
 * Copyright  2000 Sun Microsystems, Inc. Some preexisting portions Copyright
 * 1999 Netscape Communications Corp. All rights reserved.
 *
 * kapollthr.cpp: Keep-Alive Poll thread logic
 *
 */

#include "xp/xpatomic.h"
#include "frame/conf.h"
#include "frame/log.h"
#include "base/systhr.h"
#include "prinrval.h"
#include "time/nstime.h"

#include "httpdaemon/connqueue.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/WebServer.h"
#include "httpdaemon/ListenSocket.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/statsmanager.h"
#include "pollmanager.h"
#include "kapollthr.h"

/*
 * Number of fixed descriptors in the pollarray. Currently 1, since flagNeedWakeup
 * is added to the pollarray
 */
#define NUM_FIXED_DESCRIPTORS 1

static const PRInt32 CLEANUP_BUFFER_SIZE = 65536;

PRIntervalTime KAPollThread::closeTimeoutInterval = 0;

/*
 * DESCRIPTOR_DATA_MASK defines the bits in the poll array's descriptor-
 * specific void *data in which we stash a DescriptorDataType value.
 */
static const size_t DESCRIPTOR_DATA_MASK = 0x3;

/*
 * DescriptorDataType is OR'd into the low order bits of the poll array's
 * descriptor-specific void *data to identify the type of data associated with
 * a poll descriptor.
 */
enum DescriptorDataType {
    DESCRIPTOR_DATA_CONNECTION = 0x0,   // data refers to a Connection *
    DESCRIPTOR_DATA_WAKEUP = 0x1        // descriptor is a "wakeup" fd *
};

/*
 * GetDescriptorDataType retrieves a DescriptorDataType from a poll array
 * descriptor-specific void *data value.
 */
static inline DescriptorDataType
GetDescriptorDataType(void *data)
{
    size_t type = (size_t) data & DESCRIPTOR_DATA_MASK;

    return (DescriptorDataType) type;
}

/*
 * GetDescriptorDataConnection extracts the Connection * from a poll array
 * descriptor-specific void *data value of type DESCRIPTOR_DATA_CONNECTION.
 */
static inline Connection *
GetDescriptorDataConnection(void *data)
{
    PR_ASSERT(GetDescriptorDataType(data) == DESCRIPTOR_DATA_CONNECTION);

    return (Connection *) ((size_t) data & ~DESCRIPTOR_DATA_MASK);
}

/*
 * FOREACH(i, n, d) { } iterates over i = 0 ... n - 1 or i = n - 1 ... 0,
 * depending on the value of the direction d.
 */
#define FOREACH(i, n, d) \
    PR_ASSERT((d) == FORWARD || (d) == BACKWARD); \
    for ((i) = (d) == FORWARD ? 0 : (n) - 1; \
         (i) != ((d) == FORWARD ? (n) : -1); \
         (i) += (d))

/*
 * PollArrayIndexAndConnection stores an active poll array index and the
 * Connection * associated with that index.  It is used during HandleEvents()
 * processing to construct arrays of connections that can be classified then
 * either enqueued for processing by the DaemonSession or serviced
 * asynchronously by the accelerator cache.
 */
struct PollArrayIndexAndConnection {
    int pollArrayIndex;
    Connection *connection;
};

PRBool XXXalwaysUseRequestClassifier = PR_FALSE;
PRBool XXXneverUseRequestClassifier = PR_FALSE;

const PRInt32 KAPollThread::minInputBufferSize = 64;

static inline VirtualServer *
GetVS(Connection *connection)
{
    VirtualServer *vs = NULL;

    const HHString *host = connection->httpHeader.GetHost();
    if (host) {
        char cHostSeparator = host->ptr[host->len];
        host->ptr[host->len] = '\0';

        char cPortSeparator;
        char *port = util_host_port_suffix(host->ptr);
        if (port) {
             cPortSeparator = *port;
             *port = '\0';
        }

        vs = connection->lsConfig->findVS(host->ptr);

        if (port)
            *port = cPortSeparator;

        host->ptr[host->len] = cHostSeparator;
    }

    if (!vs)
        vs = connection->lsConfig->getDefaultVS();

    return vs;
}

// Initialize 
KAPollThread::KAPollThread(PollManager *_pollManager,
                           PRInt32 _numDesc,
                           PRIntervalTime _pollInterval)
: Thread("KAPollThread"),
  StatsSession("none"),
  pollManager(_pollManager),
  pollArray(_numDesc),
  pollInterval(_pollInterval),
  connQ(DaemonSession::GetConnQueue()),
  pool(pool_create()),
  accel(accel_handle_create()),
  flagTerminate(PR_FALSE),
  flagNeedWakeup(PR_FALSE),
  flagUseRequestClassifier(PR_FALSE),
  direction(FORWARD),
  numKeepAliveTimeouts(0),
  avgReads(0),
  avgReadInputBufferOverflows(0),
  avgAcceleratorHits(0),
  avgAcceleratorMisses(0),
  desiredInputBufferSize(minInputBufferSize),
  addedConnectionList(NULL)
{
    maxEvents = _numDesc / 16;
    if (maxEvents < 16)
        maxEvents = 16;

    wakeup = PR_NewPollableEvent();

    readable = (PollArrayIndexAndConnection *)PERM_MALLOC(maxEvents * sizeof(PollArrayIndexAndConnection));
    writable = (PollArrayIndexAndConnection *)PERM_MALLOC(maxEvents * sizeof(PollArrayIndexAndConnection));
    acceptable = (ListenSocket **)PERM_MALLOC(maxEvents * sizeof(ListenSocket *));
    closed = (Connection **)PERM_MALLOC(maxEvents * sizeof(Connection *));
    enqueue = (Connection **)PERM_MALLOC(maxEvents * sizeof(Connection *));
    acceleratable = (PollArrayIndexAndConnection *)PERM_MALLOC(maxEvents * sizeof(PollArrayIndexAndConnection));

    XXXalwaysUseRequestClassifier = conf_getboolean("AlwaysUseRequestClassifier", XXXalwaysUseRequestClassifier);
    XXXneverUseRequestClassifier = conf_getboolean("NeverUseRequestClassifier", XXXneverUseRequestClassifier);

    // CloseTimeout
#if defined(HPUX) || defined(SOLARIS)
    int closeTimeoutSeconds = conf_getboundedinteger("CloseTimeout", 0, 60, 0);
#else
    int closeTimeoutSeconds = conf_getboundedinteger("CloseTimeout", 0, 60, 2);
#endif
    closeTimeoutInterval = PR_SecondsToInterval(closeTimeoutSeconds);

}

void
KAPollThread::RequestTermination()
{
    flagTerminate = PR_TRUE;

    if (wakeup)
        PR_SetPollableEvent(wakeup);
}

PRInt32
KAPollThread::HandleEvents(PRInt32 pollArrayIndex, PRInt32 numEvents)
{
    PR_ASSERT(numEvents <= maxEvents);

    PRInt32 numReadable = 0;
    PRInt32 numWritable = 0;
    PRInt32 numClosed = 0;
    PRInt32 numAcceleratable = 0;

    // Quickly scan the poll array, adding information about each ready
    // descriptor to readable[], writable[], acceptable[], or closed[] as
    // appropriate
    pollArrayIndex = ScanPollArray(pollArrayIndex, numEvents, numReadable, numWritable, numClosed);

    // Acquire a reference to the accelerator cache
    void *mark = pool_mark(pool);
    AcceleratorAsync *async = NULL;
#ifdef HAS_ASYNC_ACCELERATOR
    async = accel_async_begin(accel, pool);
#endif
    // Classify readable and writable descriptors, adding acceleratable
    // descriptors to acceleratable[] and enqueuing non-acceleratable
    // connections in the connection queue
    if (numReadable || numWritable) {
        PRInt32 numEnqueue = 0;
        ClassifyReadable(numReadable, async, numClosed, numAcceleratable, numEnqueue);
        ClassifyWritable(numWritable, numAcceleratable);
        if (numEnqueue)
            Enqueue(numEnqueue);
    }

#ifdef HAS_ASYNC_ACCELERATOR
    // Attempt to process connections using the accelerator cache, enqueuing
    // non-acceleratable connections in the connection queue
    if (numAcceleratable) {
        PRInt32 numEnqueue = 0;
        AcceleratedRespond(numAcceleratable, async, numClosed, numEnqueue);
        if (numEnqueue)
            Enqueue(numEnqueue);
    }

    // Release our reference to the accelerator cache
    accel_async_end(async);
#endif
    pool_recycle(pool, mark);

    // Close sockets for broken connections
    CloseConnections(closed, numClosed);

    return pollArrayIndex;
}

PRInt32
KAPollThread::ScanPollArray(PRInt32 pollArrayIndex, PRInt32 numEvents, PRInt32& numReadable, PRInt32& numWritable, PRInt32& numClosed)
{
    PR_ASSERT(numEvents <= maxEvents);

    while (numEvents--) {
        pollArrayIndex = pollArray.FindReadyDescriptor(pollArrayIndex);

        void *data = pollArray.GetDescriptorData(pollArrayIndex);

        switch (GetDescriptorDataType(data)) {
        case DESCRIPTOR_DATA_CONNECTION:
            if (pollArray.IsDescriptorReadable(pollArrayIndex)) {
                readable[numReadable].pollArrayIndex = pollArrayIndex;
                readable[numReadable].connection = GetDescriptorDataConnection(data);
                numReadable++;
            } else if (pollArray.IsDescriptorWritable(pollArrayIndex)) {
                writable[numWritable].pollArrayIndex = pollArrayIndex;
                writable[numWritable].connection = GetDescriptorDataConnection(data);
                numWritable++;
            } else {
                pollArray.RemoveDescriptor(pollArrayIndex);
                closed[numClosed++] = GetDescriptorDataConnection(data);
            }
            break;

        case DESCRIPTOR_DATA_WAKEUP:
            PR_ASSERT(pollArray.IsDescriptorReadable(pollArrayIndex));
            PR_WaitForPollableEvent(wakeup);
            break;

        default:
            PR_ASSERT(0);
            break;
        }

        pollArrayIndex++;
    }

    return pollArrayIndex;
}

void
KAPollThread::ClassifyReadable(PRInt32 numReadable, AcceleratorAsync *async, PRInt32& numClosed, PRInt32& numAcceleratable, PRInt32& numEnqueue)
{
    PRInt32 iReadable;

    PR_ASSERT(numReadable <= maxEvents);

    // Is the request classifier enabled?
    if (flagUseRequestClassifier) {
        // Classify requests based on whether they can be serviced by the
        // accelerator cache
        FOREACH(iReadable, numReadable, direction) {
            PRInt32 pollArrayIndex = readable[iReadable].pollArrayIndex;
            Connection *connection = readable[iReadable].connection;

            switch (ClassifyRequest(async, connection)) {
            case REQUEST_ASYNC:
                acceleratable[numAcceleratable].pollArrayIndex = pollArrayIndex;
                acceleratable[numAcceleratable].connection = connection;
                numAcceleratable++;
                break;

            case REQUEST_QUEUE:
                pollArray.RemoveDescriptor(pollArrayIndex);
                enqueue[numEnqueue++] = connection;
                break;

            case REQUEST_AGAIN:
                break;

            default:
                pollArray.RemoveDescriptor(pollArrayIndex);
                closed[numClosed++] = connection;
                break;
            }
        }
    } else {
        // Indicate that all connections should be enqueued for subsequent
        // processing by the DaemonSession threads
        FOREACH(iReadable, numReadable, direction) {
            PRInt32 pollArrayIndex = readable[iReadable].pollArrayIndex;
            Connection *connection = readable[iReadable].connection;

            pollArray.RemoveDescriptor(pollArrayIndex);
            enqueue[numEnqueue++] = connection;
        }
    }
}

KAPollThread::RequestClassification
KAPollThread::ClassifyRequest(AcceleratorAsync *async, Connection *connection)
{
    PR_ASSERT(connection->async.inbuf.cursize == 0);
    PR_ASSERT(connection->fKeepAliveReservation);

#ifdef HAS_ASYNC_ACCELERATOR
    if (connection->async.fd == -1)
        return REQUEST_QUEUE;

    PR_ASSERT(desiredInputBufferSize > sizeof("GET "));

    // Resize the input buffer if it looks like it might be too small to hold
    // a request
    if (connection->async.inbuf.maxsize < desiredInputBufferSize) {
        unsigned char *buf = (unsigned char *) PERM_MALLOC(desiredInputBufferSize);
        if (!buf)
            return REQUEST_QUEUE;

        PERM_FREE(connection->async.inbuf.buf);

        connection->async.inbuf.buf = buf;
        connection->async.inbuf.maxsize = desiredInputBufferSize;
    }

    // Attempt to read a request into the input buffer
    int rv = read(connection->async.fd, connection->async.inbuf.buf, connection->async.inbuf.maxsize - 1);
    if (rv == -1) {
        int e = errno;
        if (e == EAGAIN || e == EWOULDBLOCK)
            return REQUEST_AGAIN;
        return REQUEST_CLOSE;
    }
    if (rv == 0)
        return REQUEST_CLOSE;

    // Track how often we read data into an input buffer
    avgReads++;

    // Record how much data was read in case we need to pass the connection to
    // a DaemonSession thread
    connection->async.inbuf.cursize = rv;

    // Attempt to parse a request from the input buffer
    netbuf inbuf;
    inbuf.sd = connection->fd;
    inbuf.pos = 0;
    inbuf.cursize = rv;
    inbuf.maxsize = rv + 1;
    inbuf.rdtimeout = 0;
    inbuf.inbuf = connection->async.inbuf.buf;
    HHStatus status = connection->httpHeader.ParseRequest(&inbuf, PR_INTERVAL_NO_WAIT);
    if (status != HHSTATUS_SUCCESS) {
        // Track how often an input buffer was too small to accomodate a GET
        // request
        if (status == HHSTATUS_TOO_LARGE && connection->httpHeader.GetMethodNumber() == METHOD_GET)
            avgReadInputBufferOverflows++;

        return REQUEST_QUEUE;
    }

    // XXX elving handle pipelined requests
    if (inbuf.pos != inbuf.cursize)
        return REQUEST_QUEUE;

    // Check whether this request could potentially be serviced by the
    // accelerator cache
    if (!accel_is_eligible(connection))
        return REQUEST_QUEUE;

    // Check whether a response to this request has already been cached
    if (!accel_async_lookup(async, connection, GetVS(connection))) {
        // XXX elving don't parse the HTTP request again in the DaemonSession

        // Track how often we couldn't use the accelerator cache
        avgAcceleratorMisses++;

        return REQUEST_QUEUE;
    }

    // Track how often we could use the accelerator cache
    avgAcceleratorHits++;

    return REQUEST_ASYNC;
#else
    return REQUEST_QUEUE;
#endif
}

void
KAPollThread::ClassifyWritable(PRInt32 numWritable, PRInt32& numAcceleratable)
{
    PRInt32 iWritable;

    PR_ASSERT(numWritable <= maxEvents);

    // Currently, the only writable connections are those with pending
    // asynchronous accelerator cache operations
    FOREACH(iWritable, numWritable, direction) {
        acceleratable[numAcceleratable].pollArrayIndex = writable[iWritable].pollArrayIndex;
        acceleratable[numAcceleratable].connection = writable[iWritable].connection;
        numAcceleratable++;
    }
}

#ifdef HAS_ASYNC_ACCELERATOR
void
KAPollThread::AcceleratedRespond(PRInt32 numAcceleratable, AcceleratorAsync *async, PRInt32& numClosed, PRInt32& numEnqueue)
{
    PRInt32 numKeepAliveHits = 0;
    time_t now = ft_time();

    for (PRInt32 i = 0; i < numAcceleratable; i++) {
        PRInt32 pollArrayIndex = acceleratable[i].pollArrayIndex;
        Connection *connection = acceleratable[i].connection;

        // Attempt to service the request using the accelerator cache
        int status_num;
        PRInt64 transmitted;
        time_t expiration = now + connection->keepAliveTimeout;
        // Track session statistics
        beginRequest();
        VirtualServer* vs = GetVS(connection);
        beginProcessing(vs, connection->httpHeader.GetMethod(), 
                        connection->httpHeader.GetRequestAbsPath());
        // Track perf bucket statistics
        beginFunction();
        AcceleratorAsyncStatus status = accel_async_service(async, connection, &status_num, &transmitted);
        switch (status) {
        case ACCEL_ASYNC_DONE:
            // We processed the request, so indicate the input buffer is empty
            connection->async.inbuf.cursize = 0;

            // Track how often we process a request on a connection that had
            // been kept alive
            if (connection->fNewlyAccepted) {
                connection->fNewlyAccepted = PR_FALSE;
            } else {
                numKeepAliveHits++;
            }

            // Should the connection be kept alive?
            if (connection->httpHeader.IsKeepAliveRequested()) {
                pollArray.SetDescriptorEvents(pollArrayIndex, PollAdapter::READABLE);
                pollArray.SetDescriptorExpirationTime(pollArrayIndex, expiration);
            } else {
                pollArray.RemoveDescriptor(pollArrayIndex);
                closed[numClosed++] = connection;
            }
            endFunction(STATS_PROFILE_CACHE);
            endProcessing(vs, status_num, connection->async.inbuf.cursize,
                          transmitted);
            break;

        case ACCEL_ASYNC_AGAIN:
            // We started sending the response, but we couldn't send all of
            // it.  We'll wait for the socket to be writable again.
            pollArray.SetDescriptorEvents(pollArrayIndex, PollAdapter::WRITABLE);
            // XXX elving need to set an IO timeout, perhaps value based on NetwriteTimeout directive?
            pollArray.SetDescriptorExpirationTime(pollArrayIndex, expiration);
            break;

        case ACCEL_ASYNC_FALSE:
            // We couldn't service the request asynchronously, so send it to a
            // DaemonSession thread
            pollArray.RemoveDescriptor(pollArrayIndex);
            enqueue[numEnqueue++] = connection;
            abortFunction();
            abortProcessing(vs);
            break;
        }

        // Track session statistics
        setMode(STATS_THREAD_IDLE);
    }

    // record keep-alive hits
    StatsManager::recordKeepaliveHits(numKeepAliveHits);

}
#endif

void
KAPollThread::Enqueue(PRInt32 numEnqueue)
{
    PRInt32 numKeepAliveReservationsToRelease = 0;
    PRInt32 i;

    // Check for keep-alive connections amongst those being enqueued
    for (i = 0; i < numEnqueue; i++) {
        PR_ASSERT(!IsConnectionPolled(enqueue[i]));
        if (enqueue[i]->fKeepAliveReservation) {
            enqueue[i]->fKeepAliveReservation = PR_FALSE;
            numKeepAliveReservationsToRelease++;
        }
    }

    // Enqueue the connections
    PRInt32 numActuallyEnqueued = connQ->AddReady(enqueue, numEnqueue);

    // Surrender the connections' keep-alive reservations
    pollManager->ReleaseReservations(numKeepAliveReservationsToRelease);

    // Handle connection queue overflows
    for (i = numActuallyEnqueued; i < numEnqueue; i++) {
        enqueue[i]->abort();
        enqueue[i]->destroy();
    }
}

#ifdef DEBUG
PRBool
KAPollThread::IsConnectionPolled(Connection *connection)
{
    void *data = (void *) ((size_t) connection | DESCRIPTOR_DATA_CONNECTION);

    return pollArray.IsDataPresent(data);
}
#endif

void
KAPollThread::AddConnection(Connection *connection, 
                            PRUint32 keepAliveTimeout)
{
    const void *head;

    PR_ASSERT(connection->fKeepAliveReservation);

    connection->keepAliveTimeout = keepAliveTimeout;
    XP_Lock(&lockAddedConnectionList);
    head = addedConnectionList;
    connection->next = addedConnectionList;
    addedConnectionList = connection;
    XP_Unlock(&lockAddedConnectionList);

    // XXX elving what if we're adding a connection we just accepted? we don't want to wake up ourselves

    // If we're the first thread to ask for a connection to be 
    // added to the poll array on this poll pass and the poll thread 
    // is blocked, wake the poll thread so that it adds this 
    // connection promptly
    if (!head && flagNeedWakeup)
        PR_SetPollableEvent(wakeup);
}

void
KAPollThread::HandleAddedConnections()
{
    Connection *connection;

    XP_Lock(&lockAddedConnectionList);
    connection = addedConnectionList;
    addedConnectionList = NULL;
    XP_Unlock(&lockAddedConnectionList);

    while (connection) {
        Connection *next = connection->next;

        PR_ASSERT(!IsConnectionPolled(connection));
        PR_ASSERT(connection->fKeepAliveReservation);

        void *data = (void *) ((size_t) connection | DESCRIPTOR_DATA_CONNECTION);

        time_t expiration = ft_time() + connection->keepAliveTimeout;

        if (pollArray.AddDescriptor(connection->fd,
                                    PollAdapter::READABLE,
                                    connection->lsConfig->getConfiguration(),
                                    expiration,
                                    data) != PR_SUCCESS)
        {
            ereport(LOG_VERBOSE, "unable to add connection to poll array");
            CloseConnections(&connection, 1);
        } else {
            PR_ASSERT(IsConnectionPolled(connection));
        }

        connection = next;
    }
}

void KAPollThread::CloseConnections(Connection **array, PRInt32 numClosed)
{
    PRInt32 numKeepAliveReservationsToRelease = 0;

    if (numClosed)
        ereport(LOG_FINEST, "keep-alive subsystem closing %d connections", numClosed);

    for (PRInt32 i = 0; i < numClosed; i++) {
        PR_ASSERT(!IsConnectionPolled(array[i]));
        PR_ASSERT(array[i]->fKeepAliveReservation);

        if (array[i]->fKeepAliveReservation) {
            array[i]->fKeepAliveReservation = PR_FALSE;
            numKeepAliveReservationsToRelease++;
        }

        if (array[i]->fd) {
            
            // No more HTTP-level traffic on the socket
            array[i]->done();

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

            if (closeTimeoutInterval > 0) {
                // Shut socket down so client gets EOF on recv
                PR_Shutdown(array[i]->fd, PR_SHUTDOWN_SEND);

                // Read off residual data until we get EOF from client or
                // closeTimeoutInterval elapses
                PRIntervalTime epoch = ft_timeIntervalNow();
                PRIntervalTime elapsed = 0;
                while (elapsed < closeTimeoutInterval) {
                    PRIntervalTime timeout = closeTimeoutInterval - elapsed;

                    static char buf[CLEANUP_BUFFER_SIZE];
                    int rv = PR_Recv(array[i]->fd, buf, sizeof(buf), 0, timeout);
                    if (rv < 1)
                        break;
                    
                    elapsed = ft_timeIntervalNow() - epoch;
                }
            }
        }

        array[i]->timeout();
        array[i]->destroy();
    }

    pollManager->ReleaseReservations(numKeepAliveReservationsToRelease);
}

void
KAPollThread::ReapConnections(time_t now)
{
    PRInt32 pollArrayIndex = 0;
    PRInt32 numClosed = 0;

    Configuration *configuration = ConfigurationManager::getConfiguration();

    for (;;) {
        pollArrayIndex = pollArray.FindExpiredDescriptor(configuration, now, pollArrayIndex);
        if (pollArrayIndex == -1)
            break;

// XXX elving if we reap writable connections that are currently being serviced asynchronously, we need to cancel that pending IO operation (and release the connection's accelerator cache reference) using accel_async_abort()

        numKeepAliveTimeouts++;

        void *data = pollArray.GetDescriptorData(pollArrayIndex);
        pollArray.RemoveDescriptor(pollArrayIndex);
        closed[numClosed++] = GetDescriptorDataConnection(data);

        if (numClosed == maxEvents) {
            ereport(LOG_VERBOSE, "reaping %d keep-alive connections", numClosed);
            CloseConnections(closed, numClosed);
            numClosed = 0;
        }

        pollArrayIndex++;
    }

    if (configuration)
        configuration->unref();

    if (numClosed) {
        ereport(LOG_VERBOSE, "reaping %d keep-alive connections", numClosed);
        CloseConnections(closed, numClosed);
    }
}

void 
KAPollThread::run(void)
{
    int usPollIntervalConfigured = PR_IntervalToMicroseconds(pollInterval);
    int usPollIntervalTarget = usPollIntervalConfigured;
    PRInt32 numThreads = pollManager->GetConcurrency();
    time_t lastReapTime = ft_time();
    PRUint32 numSuccessfulPolls = 0;
    PRUint32 numSkippedRequestClassifier = 0;
    PRInt32 numEvents = 0;

    if (wakeup) {
        pollArray.AddDescriptor(wakeup,
                                PollAdapter::READABLE,
                                NULL,
                                -1,
                                (void *) DESCRIPTOR_DATA_WAKEUP);
    }

    XP_Lock(&lockKAPollThreadRunning);

    while (!flagTerminate) {
        // Track session statistics
        setMode(STATS_THREAD_IDLE);

        // When we're under light load, we use a "wakeup" fd to reduce latency
        // and keep the server from burning CPU when it's completely idle.
        // (Under heavier load, we switch to using a poll interval.)  Set
        // flagNeedWakeup before the HandleAddedConnections() call to ensure
        // that other threads know whether they need to notify the wakeup fd
        // after adding connections.
        flagNeedWakeup = (wakeup && numEvents < 1);

        // Update the poll array with recently added connections
        HandleAddedConnections();

        // Reap idle connections and compact the poll array once a second
        time_t now = ft_time();
        if (now != lastReapTime) {
            ReapConnections(now);
            pollArray.Compact();
            lastReapTime = ft_time();
        }

        // Wait only if flagNeedWakeup and there are no connections in 
        // the pollarray. If there are connections in the pollarray, then they
        // may have to be reaped before any new connections becomes active.
        // Therefore wait for pollinterval and wakeup and reapconnections.
        int numActiveDesc = pollArray.GetNumDescriptors();
        PRIntervalTime timeout;
        if (flagNeedWakeup) {
            if (numActiveDesc == NUM_FIXED_DESCRIPTORS)
                timeout = PR_INTERVAL_NO_TIMEOUT;
            else
                timeout = pollInterval;
        }

        // Poll the descriptors in the poll array
        XP_Unlock(&lockKAPollThreadRunning);
        // Old way was to poll every 10ms.NO_TIMEOUT: wait_for_ever, relying on the wakeup
        // desc or an existing poll ready connection to wake us up. If flagneedwakeup false,
        // return from poll rightaway.
        numEvents = pollArray.Poll(flagNeedWakeup ? 
                                   timeout: PR_INTERVAL_NO_WAIT);
        XP_Lock(&lockKAPollThreadRunning);

        // Keep trying until at least one descriptor polls ready
         if (numEvents < 1) {
             // If we have cached stats we'd like to flush...
             if (!StatsSession::isFlushed()) {
                 // There aren't any connections on the horizon.  Take this
                 // opportunity to flush our cached stats before we poll
                 // again
                 StatsSession::flush();
             }
             continue;
         }

        numSuccessfulPolls++;

        // Make a note of how backlogged the server is right now
        PRInt32 queueLength = connQ->GetLength();

        // Should we use the request classifier?
        if (accel_get_http_entries() < 1) {
            // There's nothing in the accelerator cache.  Don't waste CPU
            // classifying requests because everything will need to be
            // enqueued anyway.
            flagUseRequestClassifier = PR_FALSE;
        } else if (avgAcceleratorMisses >= avgAcceleratorHits) {
            // We're not getting many hits in the accelerator cache.  We don't
            // want to waste time classifying requests if the majority will
            // need to be enqueued anyway, but we also don't want to miss out
            // on the opportunity to service requests asynchronously once the
            // accelerator cache is fully populated.
            if (numSkippedRequestClassifier < 16) {
                // Skip classification 15/16 times to avoid wasting CPU
                numSkippedRequestClassifier++;
                flagUseRequestClassifier = PR_FALSE;
            } else {
                // Try classifying 1/16 times in case things have changed
                numSkippedRequestClassifier = 0;
                flagUseRequestClassifier = PR_TRUE;
            }
        } else {
            // Classify requests in hopes that some can be serviced
            // asynchronously
            flagUseRequestClassifier = PR_TRUE;
        }
        if (flagUseRequestClassifier) {
            // Decay averages slowly when the request classifier is active
            while ((avgAcceleratorMisses + avgAcceleratorHits) > 1024) {
                avgAcceleratorHits /= 2;
                avgAcceleratorMisses /= 2;
            }
        } else {
            // Decay averages quickly when the request classifier is inactive
            while ((avgAcceleratorMisses + avgAcceleratorHits) > 16) {
                avgAcceleratorHits /= 2;
                avgAcceleratorMisses /= 2;
            }
        }

        // flagUseRequestClassifier = PR_TRUE; // XXX elving
        if (XXXalwaysUseRequestClassifier)
            flagUseRequestClassifier = PR_TRUE;
        if (XXXneverUseRequestClassifier)
            flagUseRequestClassifier = PR_FALSE;
        // flagUseRequestClassifier = PR_TRUE; // XXX elving

        // Alternate the direction in which we process ready descriptors to
        // avoid prejudicing those at the end of the poll array
        direction = (numSuccessfulPolls & 1) ? FORWARD : BACKWARD;

        // Process the descriptors that polled ready
        PRInt32 pollArrayIndex = 0;
        PRInt32 numEventsRemaining = numEvents;
        while (numEventsRemaining > 0) {
            PRInt32 numEventsToHandle = numEventsRemaining;
            if (numEventsToHandle > maxEvents)
                numEventsToHandle = maxEvents;

            pollArrayIndex = HandleEvents(pollArrayIndex, numEventsToHandle);

            numEventsRemaining -= numEventsToHandle;
        }

        // If more than 1/64 of reads overflow the input buffer, it's too
        // small
        if (avgReadInputBufferOverflows * 64 > avgReads) {
            desiredInputBufferSize += (desiredInputBufferSize / 16);
            if (desiredInputBufferSize > DaemonSession::GetBufSize())
                desiredInputBufferSize = DaemonSession::GetBufSize();
            avgReads -= avgReadInputBufferOverflows;
            avgReadInputBufferOverflows -= avgReadInputBufferOverflows;
        }
        while (avgReads > 1024) {
            avgReads /= 2;
            avgReadInputBufferOverflows /= 2;
        }

        // XXX elving need to record which requests each keep-alive thread is currently processing for /.perf and stats-xml

        // Sleep a bit between polls
        int usToSleep;
        int numQueuedPerThread = (queueLength + numThreads / 2) / numThreads;
        if (numEvents > 2 * numQueuedPerThread) {
            // The connection queue is draining faster than we can fill it.
            // Either we're doing all the useful work here in the poll thread
            // or there's CPU sitting idle.  In either case, there's no need
            // to sleep between polls.
            usPollIntervalTarget = usPollIntervalTarget / 2;
            usToSleep = 0;
        } else if (numEvents > numQueuedPerThread) {
            // The connection queue is draining at about the same rate we fill
            // it.  Because waking idle DaemonSession threads is inefficient,
            // decrease the amount we sleep between polls in an attempt to
            // keep the connection queue from emptying.
            usPollIntervalTarget = usPollIntervalTarget * 4 / 5;
            usToSleep = usPollIntervalTarget;
        } else {
            // The connection queue is backing up, so increase the amount we
            // sleep between polls to reduce poll overhead and give the
            // DaemonSession threads a chance to catch up
            usPollIntervalTarget = (usPollIntervalConfigured + usPollIntervalTarget + 1) / 2;
            if (usPollIntervalTarget < 1)
                usPollIntervalTarget = 1;
            usToSleep = usPollIntervalTarget;
        }
        if (usToSleep >= 500) {
            XP_Unlock(&lockKAPollThreadRunning);
            systhread_sleep((usToSleep + 500) / 1000);
            XP_Lock(&lockKAPollThreadRunning);
        }
    }

    StatsSession::flush();
    XP_Unlock(&lockKAPollThreadRunning);
}
