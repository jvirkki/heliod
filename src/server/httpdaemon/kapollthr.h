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
 * kapollthr.h
 *
 */
#ifndef _KAPOLLTHR_H_
#define _KAPOLLTHR_H_

#include "xp/xpsynch.h"
#include "nspr.h"                       // NSPR declarations
#include "NsprWrap/Thread.h"            // Thread class declarations
#include "frame/accel.h"
#include "httpdaemon/statssession.h"
#include "base/loadavg.h"
#include "pollarray.h"                  // PollArray class declarations
#include "polladapter.h"                // PollAdapter class declarations
#include "support/GenericList.h"

class PollManager;
class ConnectionQueue;
struct PollArrayIndexAndConnection;

class KAPollThread : public Thread, public StatsSession {
public:
    KAPollThread(PollManager *pollManager, PRInt32 numDesc, PRIntervalTime pollInterval);
    void run();
    void AddConnection(Connection *connection, PRUint32 keepAliveTimeout);
    PRInt32 GetLoad() { return pollArray.GetNumDescriptors(); }
    PRUint64 GetNumKeepAliveTimeouts() { return numKeepAliveTimeouts; }
    void RequestTermination();

private:
    enum Direction {
        FORWARD = 1,
        BACKWARD = 1
    };

    enum RequestClassification {
        REQUEST_CLOSE = -1,
        REQUEST_AGAIN = 0,
        REQUEST_QUEUE = 1,
        REQUEST_ASYNC = 2
    };

    PRInt32 HandleEvents(PRInt32 pollArrayIndex, PRInt32 numEvents);
    PRInt32 ScanPollArray(PRInt32 pollArrayIndex, PRInt32 numEvents, PRInt32& numReadable, PRInt32& numWritable, PRInt32& numClosed);
    void ClassifyReadable(PRInt32 numReadable, AcceleratorAsync *async, PRInt32& numClosed, PRInt32& numAcceleratable, PRInt32& numEnqueue);
    RequestClassification ClassifyRequest(AcceleratorAsync *async, Connection *connection);
    void ClassifyWritable(PRInt32 numWritable, PRInt32& numAcceleratable);
    void AcceleratedRespond(PRInt32 numAcceleratable, AcceleratorAsync *async, PRInt32& numClosed, PRInt32& numEnqueue);
    void Enqueue(PRInt32 numEnqueue);
    PRBool IsConnectionPolled(Connection *connection);
    void HandleAddedConnections();
    void CloseConnections(Connection **array, PRInt32 numClosed);
    void ReapConnections(time_t now);

    PollManager *pollManager;
    PollArray pollArray;
    PRIntervalTime pollInterval;
    ConnectionQueue *connQ;
    pool_handle_t *pool;
    AcceleratorHandle *accel;
    PRFileDesc *wakeup;

    PRBool flagTerminate;
    PRBool flagNeedWakeup;
    PRBool flagUseRequestClassifier;
    Direction direction;
    PRUint64 numKeepAliveTimeouts;
    PRInt32 avgAcceleratorHits;
    PRInt32 avgAcceleratorMisses;
    PRInt32 avgReads;
    PRInt32 avgReadInputBufferOverflows;
    PRInt32 desiredInputBufferSize;

    XPScopedLock lockAddedConnectionList;
    Connection *addedConnectionList;

    XPScopedLock lockKAPollThreadRunning;

    PRInt32 maxEvents;
    PollArrayIndexAndConnection *readable;
    PollArrayIndexAndConnection *writable;
    ListenSocket **acceptable;
    Connection **closed;
    Connection **enqueue;
    PollArrayIndexAndConnection *acceleratable;

    static const PRInt32 minInputBufferSize;
    static PRIntervalTime closeTimeoutInterval;
};

#endif /* _KAPOLLTHR_H_ */
