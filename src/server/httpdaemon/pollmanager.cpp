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
 * Copyright  1999 Sun Microsystems, Inc. Some preexisting portions Copyright
 * 1999 Netscape Communications Corp. All rights reserved.
 *
 * pollmanager.h
 * Keepalive poll manager class: manage a number of poll threads and arrays.
 */

#ifdef Linux 
#define _XOPEN_SOURCE 1
#define _BSD_SOURCE 1
#endif
#ifdef SOLARIS_TNF
#include <tnf/probe.h>
#endif
#include <limits.h>
#include "base/systhr.h"
#include "frame/conf.h"                    // conf_getglobals()
#include "httpdaemon/dbthttpdaemon.h"      // DBT_*
#include "httpdaemon/daemonsession.h"      // DaemonSession class
#include "httpdaemon/ListenSockets.h"
#include "httpdaemon/ListenSocket.h"

#include "pollmanager.h"


PollManager::PollManager(void)
: maxKeepAlives_(0),
  numKeepAlives_(0),
  numKeepAliveRefusals_(0),
  threads_(NULL),
  numThreads_(0),
  paRoundRobin_(0)
{ }

//
// One-time routine for poll manager initialization.
//
PRStatus 
PollManager::Initialize(PRUint32 maxKeepAlives,
                        PRUint32 numThreads,
                        PRIntervalTime pollInterval)
{
    // Number of keepalive connections we will support
    this->maxKeepAlives_ = maxKeepAlives;

    // Number of poll threads
    this->numThreads_ = numThreads;

    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_PollManagerMaxKAConn),
                this->maxKeepAlives_);

    threads_ = (KAPollThread **)PERM_MALLOC(numThreads_ * sizeof(KAPollThread *));

    // Figure out the number of keep-alive connections each keep-alive poll
    // thread should accomodate then add some margin for acceptors and newly
    // accepted connections
    PRInt32 numDescPerThread = (maxKeepAlives_ + numThreads_) / numThreads_ + 1;

    PRUint32 i;

    PRStatus status = PR_SUCCESS;
    if (threads_)
    {
        // Initialize the poll arrays and start those poll threads.
        for (i = 0; i < this->numThreads_; i++)
        {
            KAPollThread *thr = new KAPollThread(this, numDescPerThread, pollInterval);
            if (thr)
                status = thr->start(PR_GLOBAL_BOUND_THREAD, PR_UNJOINABLE_THREAD);
            if (!thr || (status == PR_FAILURE))
            {
                if (thr) 
                    delete thr;
                break;
            }
            threads_[i] = thr;
        }
    }

    if (i != this->numThreads_)
    {
        Freeup();
        return PR_FAILURE;
    }
    else
        return PR_SUCCESS;
}


PollManager::~PollManager()
{
    Freeup();
}

// free up poll manager resources
void 
PollManager::Freeup()
{
    if (this->threads_)
    {
        for (PRUint32 i = 0; i < this->numThreads_; i++)
            delete threads_[i];
        PERM_FREE(this->threads_);
        this->threads_ = NULL;
        this->numThreads_ = 0;
    }
}

//
// Request a reservation in the keep-alive system
//
PRBool
PollManager::RequestReservation(void)
{
    PR_ASSERT(numKeepAlives_ >= 0);

    PRInt32 numKeepAlives = PR_AtomicIncrement(&numKeepAlives_);
    if (numKeepAlives > maxKeepAlives_) {
        PR_AtomicDecrement(&numKeepAlives_);

        if (PR_AtomicIncrement(&numKeepAliveRefusals_) == 1)
            ereport(LOG_VERBOSE, "PollManager::RequestReservation() keep-alive subsystem full");

        return PR_FALSE;
    }

    return PR_TRUE;
}

//
// Cancel a previously granted reservation
//
void
PollManager::ReleaseReservation(void)
{
    PR_ASSERT(numKeepAlives_ > 0);
    PR_AtomicDecrement(&numKeepAlives_);
}

//
// Release multiple previously granted reservations
//
void
PollManager::ReleaseReservations(PRInt32 count)
{
    PR_AtomicAdd(&numKeepAlives_, -count);
    PR_ASSERT(numKeepAlives_ >= 0);
}

//
// Choose a poll array index based on load.  The algorithm was designed to
// scale (it's O(1) with respect to the number of poll arrays) and reduce lock
// contention (it is unlikely that two separate threads will attempt to access
// a given poll array at the same) while effectively balancing load without
// ringing or primary clustering.
//
// UMA: This should be GetKAPollThreadIndex --> determines which KaPollThread
// to use.
PRUint32
PollManager::GetPollArrayIndex()
{
    PRUint32 paIndex;

    if (numThreads_ == 1) {
        paIndex = 0;
    } else {
        unsigned paRoundRobin = (PRUint32) PR_AtomicDecrement(&paRoundRobin_);
        unsigned halfNumThreads = (numThreads_ + 1) / 2;
        unsigned quotient = paRoundRobin / halfNumThreads;
        unsigned remainder = paRoundRobin - quotient * halfNumThreads;
        unsigned x = remainder * 2 + quotient;

        paIndex = x % numThreads_;

        PRUint32 paAlternateIndex = paIndex + 1;
        if (paAlternateIndex == numThreads_)
            paAlternateIndex = 0;

        if (threads_[paAlternateIndex]->GetLoad() < threads_[paIndex]->GetLoad())
            paIndex = paAlternateIndex;
    }

    return paIndex;
}

//
// Put this connection into the keepalive poll set.
// Any thread can call this. Make an attempt to add this to one of the
// poll arrays using a roundrobin scheme; 
// Caller should have requested a reservation before calling us.
//
void
PollManager::AddConnection(Connection* conn, PRUint32 keepAliveTimeout)
{
    threads_[GetPollArrayIndex()]->AddConnection(conn, keepAliveTimeout);
}

//
// Get the number of keep-alive connections in the poll arrays
//
PRInt32 
PollManager::GetNumKeepAlives() 
{
    return numKeepAlives_;
}

//
// Get the number of threads responsible for polling the poll arrays
//
PRInt32
PollManager::GetConcurrency()
{
    return numThreads_;
}

// Get KeepAlive stats
PRStatus
PollManager::GetKeepAliveInfo(keepAliveInfo *info)
{
    PRUint32      totalNumKeepAliveFlushes = 0;
    PRUint32      totalNumKeepAliveTimeouts = 0;

    if (!info)
        return PR_FAILURE;

    info->maxKeepAlives    = this->maxKeepAlives_;
    info->numKeepAliveThreads = this->numThreads_;
    info->numKeepAliveHits = 0; // ??? XXX - Arvind
    info->tmoKeepAlive     = DaemonSession::GetKeepAliveTimeout();

    for (PRUint32 i = 0; i < this->numThreads_; i++)
    {
        totalNumKeepAliveTimeouts += threads_[i]->GetNumKeepAliveTimeouts();
    }
    info->numKeepAliveFlushes = totalNumKeepAliveFlushes;
    info->numKeepAliveTimeouts = totalNumKeepAliveTimeouts;
    info->numKeepAliveRefusals = numKeepAliveRefusals_;
    info->numKeepAlives = numKeepAlives_;

    return PR_SUCCESS;
}


//
// Send a to the poll workers (to make sure it is not blocked)
// and clear the connections.
//
void 
PollManager::Terminate()
{
    PRUint32 i;

    for (i = 0; i < numThreads_; i++)
        threads_[i]->RequestTermination();

    for (i = 0; i < numThreads_; i++) {
        while (threads_[i]->isRunning())
            systhread_sleep(10);
    }
}


void 
PollManager::Clock()
{
    static int invocations;
    static int min = INT_MAX;
    static int max = 0;
    static int total = 0;

    for (int i = 0; i < numThreads_; i++) {
        int n = threads_[i]->GetLoad();
        if (n < min)
            min = n;
        if (n > max)
            max = n;
        total += n;
    }

    invocations++;
    if ((invocations % 5) == 0) {
        double average = (double) total / 5 / numThreads_;

/*
        ereport(LOG_INFORM,
                "PollManager::Clock(): %d minimum, %f average, %d maximum",
                min,
                average,
                max);

        for (int i = 0; i < numThreads_; i++)
            ereport(LOG_INFORM,
                    "PollManager::Clock(): %d",
                    threads_[i]->GetLoad());
                    */

        min = INT_MAX;
        max = 0;
        total = 0;
    }

}
