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
 * ConnectionQueue implementation
 *
 * @author  $Author: us14435 $
 */

#include "httpdaemon/libdaemon.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/connqueue.h"
#include "httpdaemon/statsmanager.h"
#include "httpdaemon/ListenSocket.h"
#include "httpdaemon/httpheader.h"
#include "time/nstime.h"
#include "frame/conf_api.h"
#include "private/pprio.h"

PRInt32 Connection::countActive = 0;
PRInt32 Connection::peakActive = 0;

ConnectionQueue::ConnectionQueue(const PRUint32 maxQueueLength, const PRUint32 maxConnections)
  : _maxQueue(maxQueueLength), _maxConn(maxConnections),
    _head(-1), _tail(-1), _numItems(0), _peak(0), _totalDequeued(0),
    _totalNewConnections(0), _totalTicks(0), _numConnectionOverflows(0),
    _unused(NULL), _available(_lockQueue), _numWaiters(0),
    _terminating(PR_FALSE)
{
    PR_ASSERT(maxConnections >= maxQueueLength);

    _conns = new Connection[_maxConn];
    PR_ASSERT(_conns!=NULL);

    _queue = (Connection**) PERM_CALLOC(sizeof(Connection*) * _maxQueue);
    PR_ASSERT(_queue!=NULL);

    for (int i = 0; i < _maxConn; i++)
    {
        _conns[i].connQueue = this;
        _conns[i].next = _unused;
        _unused = &_conns[i];
    }

    loadavg_init(&_avgItems);
    loadavg_init(&_avgActive);

    // Make sure that the first overflow always gets logged
    _lastOverflowLogTime = ft_time() - 300;
    ft_register_cb(ConnectionQueueClock, this);
}

ConnectionQueue::~ConnectionQueue()
{
    Terminate();

    ft_unregister_cb(ConnectionQueueClock, this);
}

extern "C" void ConnectionQueueClock(void* context)
{
    ((ConnectionQueue*)context)->Clock();
}

void ConnectionQueue::Clock()
{
    loadavg_update(&_avgItems, _numItems);
    loadavg_update(&_avgActive, Connection::countActive);
}

void ConnectionQueue::Terminate()
{
    XP_Lock(&_lockQueue);
    _terminating = PR_TRUE;
    XP_Broadcast(&_available);
    XP_Unlock(&_lockQueue);
}

PRStatus
ConnectionQueue::AddReady(Connection* ready)
{
    PRStatus rv;

    PR_ASSERT(ready != NULL);

    if (StatsManager::isProfilingEnabled()) {
        ready->ticks = PR_IntervalNow();
    } else {
        ready->ticks = ft_timeIntervalNow();
    }

    XP_Lock(&_lockQueue);

    if (_numItems < _maxQueue) {
        // Add to head
        _head++;
        if (_head == _maxQueue) // reset circular queue index
            _head = 0;
        _queue[_head] = ready;   // Add the item to the queue
        _numItems++;
        if (_tail == -1)         // queue was empty
            _tail = 0;           // initialize tail

        if (_numItems > _peak)
            _peak = _numItems;

        if (ready->fNewlyAccepted)
            _totalNewConnections++;

        // we only notify if there are waiters
        if (_numWaiters)
            XP_Signal(&_available);

        rv = PR_SUCCESS;
    } else {
        _numConnectionOverflows++;
        rv = PR_FAILURE;
    }

    XP_Unlock(&_lockQueue);

    if (rv == PR_FAILURE) 
        logOverflow();

    return rv;
}

int
ConnectionQueue::AddReady(Connection** ready, int numReady)
{
    int i;

    PRIntervalTime now;
    if (StatsManager::isProfilingEnabled()) {
        now = PR_IntervalNow();
    } else {
        now = ft_timeIntervalNow();
    }

    int numNewConnections = 0;

    for (i = 0; i < numReady; i++) {
        if (ready[i]->fNewlyAccepted)
            numNewConnections++;
        ready[i]->ticks = now;
    }

    XP_Lock(&_lockQueue);

    // If there's not enough room for all the connections...
    int numAdded = numReady;
    if (numAdded > _maxQueue - _numItems) {
        // Figure out how many connections we can actually add
        numAdded = _maxQueue - _numItems;
        for (i = numAdded; i < numReady; i++) {
            if (ready[i]->fNewlyAccepted)
                numNewConnections--;
        }
        _numConnectionOverflows += (numReady - numAdded);
    }

    // Add to head
    for (i = 0; i < numAdded; i++)
    {
        _head++;
        if (_head == _maxQueue)     // reset circular queue index
            _head = 0;
        _queue[_head] = ready[i];    // Add the item to the queue
    }
    _numItems += numAdded;
    if (_tail == -1)         // queue was empty
        _tail = 0;           // initialize tail

    if (_numItems > _peak)
        _peak = _numItems;

    _totalNewConnections += numNewConnections;

    // we only notify if there are waiters
    if (_numWaiters)
    {
        if (numAdded > 1 && numAdded >= _numWaiters)
        {
            XP_Broadcast(&_available);
        }
        else
        {
            for (i = 0; i < numAdded; i++)
                XP_Signal(&_available);
        }
    }

    XP_Unlock(&_lockQueue);

    if (numAdded < numReady) 
        logOverflow();

    return numAdded;
}

Connection*
ConnectionQueue::GetUnused()
{
    Connection* unused;

    XP_Lock(&_lockUnused);

    unused = _unused;
    if (unused) {
        _unused = unused->next;
    } else {
        // Max open connections exceeded
        _numConnectionOverflows++;
    }

    XP_Unlock(&_lockUnused);

    if (!unused) 
        logOverflow();

    return unused;
}

void
ConnectionQueue::AddUnused(Connection* unused)
{
    PR_ASSERT(!unused->fKeepAliveReservation);

    XP_Lock(&_lockUnused);

    unused->next = _unused;
    _unused = unused;

    XP_Unlock(&_lockUnused);
}

Connection*
ConnectionQueue::GetReady(PRIntervalTime to)
{
    Connection* conn = NULL;

    if (_terminating == PR_FALSE)
    {
        XPInterval remaining = XP_PRIntervalTimeToInterval(to);

        PRBool profiling = StatsManager::isProfilingEnabled();

        PRIntervalTime now;
        if (profiling) {
            now = PR_IntervalNow();
        } else {
            now = ft_timeIntervalNow();
        }

        XP_Lock(&_lockQueue);

        _numWaiters++;    // update # of pending GetConnection()s

        /* Loop while queue is empty and not terminated */
        while ((_head == -1) && (_terminating == PR_FALSE))
        {
            XPStatus rv = XP_Wait(&_available, remaining, &remaining);
            if (profiling) {
                now = PR_IntervalNow();
            } else {
                now = ft_timeIntervalNow();
            }
            if (rv == XP_FAILURE)
                break;
        }

        _numWaiters--;    // update # of pending GetConnection()s

        if (_terminating == PR_FALSE && _head != -1)
        {
            PR_ASSERT(_tail != -1);
            PR_ASSERT(_head != -1);

            conn = _queue[_tail];
            _numItems--;
            
            if (_head == _tail)
            {
                // queue empty (i.e. removed last item)
                _head = -1;
                _tail = -1;
            }
            else
            {
                _tail++;
                if (_tail == _maxQueue)
                    _tail = 0;
            }

            _totalDequeued++;

            // N.B. conn->ticks can be > now
            PRInt32 elapsed = (PRInt32)(now - conn->ticks);
            if (elapsed > 0)
                _totalTicks += elapsed;

            conn->ticks = now;
        }

        XP_Unlock(&_lockQueue);
    }

    return conn;
}

void
ConnectionQueue::GetQueueingDelay(PRUint64* totalQueued, PRUint64* totalTicks)
{
    PRIntervalTime now;
    if (StatsManager::isProfilingEnabled()) {
        now = PR_IntervalNow();
    } else {
        now = ft_timeIntervalNow();
    }

    XP_Lock(&_lockQueue);

    PRUint64 totalDequeued = _totalDequeued;
    PRUint32 numItems = _numItems;

    PRInt64 ticks = _totalTicks;
    if (_head != -1) {
        int i = _tail;
        int count = numItems;
        while (count--) {
            // Do the "- _queue[i]->ticks" part of "(now - _queue[i]->ticks)"
            ticks -= _queue[i]->ticks;
            i++;
            if (i == _maxQueue)
                i = 0;
        }
    }

    XP_Unlock(&_lockQueue);

    *totalQueued = totalDequeued + numItems;

    // Do the "now" part of "(now - _queue[i]->ticks)" for i = 0 ... numItems
    ticks += (PRInt64)numItems * now;
    if (ticks > 0) {
        *totalTicks = ticks;
    } else {
        *totalTicks = 0;
    }
}

void 
ConnectionQueue::logOverflow() {
    time_t now = ft_time();
    // Log overflow only if 5 mins have passed since the last log
    if ((now - _lastOverflowLogTime) > 300) {
        _lastOverflowLogTime = now;
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ConnectionOverflow), 
                GetMaxLength());
    }
}

Connection::Connection()
{
    connQueue = NULL;
    async.inbuf.buf = NULL;
    async.inbuf.cursize = 0;
    async.inbuf.maxsize = 0;

    accel_init_connection(this);
}

Connection::~Connection()
{
    // Once allocated, a connection object is never destroyed
    PR_ASSERT(0);
}

PRStatus Connection::create(PRFileDesc* fd_, const PRNetAddr *addr, ListenSocket* ls_)
{
    fd = fd_;
    fNewlyAccepted = PR_TRUE;
    fSSLEnabled = PR_FALSE;
    fUncleanShutdown = PR_FALSE;
    fKeepAliveReservation = PR_FALSE;

    // get a ListenSocketConfig reference
    lsConfig = ls_->getConfig();

    // get the remote IP
    net_addr_copy(&remoteAddress, addr);
    net_addr_to_string(&remoteAddress, remoteIP.buf, sizeof(remoteIP.buf));
    remoteIP.len = strlen(remoteIP.buf);

    // get the local IP
    if (ls_->hasExplicitIP()) {
        localAddress = ls_->getAddress();
    } else {
        if (PR_GetSockName(fd_, &localAddress) != PR_SUCCESS)
            return PR_FAILURE;
        lsConfig = lsConfig->getIPSpecificConfig(localAddress);
    }

    // get the SSL configuration (should be NULL when SSL is disabled)
    sslconfig = lsConfig->getSSLParams();
    PR_ASSERT(lsConfig->ssl.enabled == (sslconfig != NULL));

#ifdef HAS_ASYNC_ACCELERATOR
    // we can use the async accelerator cache unless there's an NSS SSL IO
    // layer between us and the socket or if it has not been explicitly
    // turned off in magnus.conf
    if (sslconfig || !use_accel_async()) {
        async.fd = -1;
    } else {
        async.fd = PR_FileDesc2NativeHandle(fd);
    }
    async.inbuf.cursize = 0;
#endif

    PRInt32 peakNew = PR_AtomicIncrement(&countActive);
    while (peakNew > peakActive) peakNew = PR_AtomicSet(&peakActive, peakNew);

    return PR_SUCCESS;
}

void Connection::destroy()
{
    // No more HTTP-level traffic on the socket
    done();

    // Keep-alive reservations and references to the contents of the
    // accelerator cache must be released before closing the socket
    PR_ASSERT(!fKeepAliveReservation);
#ifdef HAS_ASYNC_ACCELERATOR
    PR_ASSERT(!async.accel.gen);
    PR_ASSERT(!async.accel.headers.p);
#endif

    if (fd) {
        PR_Close(fd);
        fd = NULL;
    }

    if (lsConfig) {
        lsConfig->unref();
        lsConfig = NULL;
        sslconfig = NULL;
        PR_AtomicDecrement(&countActive);
    }

    if (connQueue)
        connQueue->AddUnused(this);
}

void Connection::enableSSL()
{
    if (sslconfig) {
        sslconfig->enable(fd);
        fSSLEnabled = PR_TRUE;
    }
}

void Connection::abort()
{
    if (fd && fSSLEnabled) {
        // Unconditionally suppress the SSLv3 close notify
        SSL_OptionSet(fd, SSL_SECURITY, PR_FALSE);
    }
}

void Connection::timeout()
{
    if (fd && fSSLEnabled && !fUncleanShutdown) {
        // Tell NSS not to block trying to send the SSLv3 close notify alert
        PR_Send(fd, "", 0, 0, PR_INTERVAL_NO_WAIT);
    }
}

void Connection::done()
{
    if (fd && fSSLEnabled && fUncleanShutdown) {
        // Suppress the SSLv3 close notify alert to keep MSIE happy
        SSL_OptionSet(fd, SSL_SECURITY, PR_FALSE);
    }
}

PRIntervalTime Connection::elapsed() const
{
    PRIntervalTime now;
    if (StatsManager::isProfilingEnabled()) {
        now = PR_IntervalNow();
    } else {
        now = ft_timeIntervalNow();
    }

    return now - ticks;
}
