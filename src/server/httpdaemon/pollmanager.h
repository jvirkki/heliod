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

#ifndef _POLL_MANAGER_H_
#define _POLL_MANAGER_H_

#include "nspr.h"
#include "prthread.h"
#include "httpdaemon/internalstats.h"
#include "pollarray.h"
#include "kapollthr.h"


class PollManager
{
    public:
        /*
         * Void constructor
         */
        PollManager(void);

        /**
         * Destructor
         */
        ~PollManager();

        //
        // One-time routine for system startup - make the arrays.
        //
        PRStatus Initialize(PRUint32 maxKeepAlives, PRUint32 numThreads, PRIntervalTime pollTimeout);

        //
        // Add this connection to the poll set.  If this is a keep-alive
        // connection, the caller must have previously requested a keep-alive
        // reservation.
        //
        void AddConnection(Connection* conn, PRUint32 keepAliveTimeout);

        //
        // Reserve room in the keep-alive subsystem for a Connection.  The
        // Connection holds onto its reservation until it is removed from the
        // poll array.
        //
        PRBool RequestReservation(void);

        //
        // Release a previously granted reservation
        //
        void ReleaseReservation(void);

        //
        // Release multiple previously granted reservations
        //
        void ReleaseReservations(PRInt32 count);

        //
        // Get the number of keep-alive connections in the poll arrays
        //
        PRInt32 GetNumKeepAlives(void);

        //
        // Get the number of threads responsible for polling the poll arrays
        //
        PRInt32 GetConcurrency(void);

        // Gather statistics
        PRStatus GetKeepAliveInfo(keepAliveInfo *info);

        //
        // Stop all keep-alive poll threads
        //
        void Terminate(void);

        void Clock(void);

    private:

        /**
         * Maximum number of KeepAlive connections allowed under the
         * current configuration.
         */
        PRUint32 maxKeepAlives_;

        // number of keep-alive connections
        PRInt32 numKeepAlives_;

        // number of times we declined to grant a keep-alive reservation
        PRInt32 numKeepAliveRefusals_;

        // poll work array; is of size numThreads below.
        KAPollThread** threads_;

        /**
         * Number of KAPollThreads to handle the polling
         */
        PRUint32 numThreads_;

        /**
         * Round robin index (determines which poll array/thread to 
         * add a descriptor to)
         */
        PRInt32 paRoundRobin_;

        void Freeup();
        PRUint32 GetPollArrayIndex();
};

#endif /* _POLL_MANAGER_H_ */
