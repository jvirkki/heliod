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
    throttling.cpp
    author : jpierre

    implement bandwidth throttling

*/

#ifndef TRAFFIC

#define TRAFFIC

#include <prcvar.h>
#include "NsprWrap/Thread.h"        // Thread class
#include "generated/ServerXMLSchema/QosLimits.h"

// configuration for QOS

class TrafficSample
{
    public:
        TrafficSample(PRInt32 b = 0, PRIntervalTime t = 0);
        PRInt32 bytes;
        PRIntervalTime timestamp;
};

// generic class for traffic stats
class TrafficStats
{
    friend class BandwidthManager;
    public:
        TrafficStats();

        PRInt32 getBandwidth() const;       // returns previously computed bandwidth in bytes per second
        void setBandwidth(PRInt32 bw);

        PRInt32 getConnections() const;     // returns the current number of connections
        void setConnections(PRInt32 conn);  // atomically sets the value of the current connections total
        inline void incrementConnections(); // atomically add one connection to the current total
        inline void decrementConnections(); // atomically remove one connection from the current total

    protected:
        // internal functions & data
        PRInt32 connections;    // current connections
        PRInt32 bandwidth;      // current bandwidth
        static PRIntervalTime recomputeinterval;
};

// VSTrafficStats keeps track of statistics for an individual virtual server

class VSTrafficStats : public TrafficStats
{
    friend class BandwidthManager;
    public:
        VSTrafficStats(PRBool enabled,          // whether stats are tracked
                       PRInt32 metrics);        // metrics interval in seconds
        ~VSTrafficStats();

        void record(TrafficSample& asample);    // to record traffic

        PRInt32 computeBandwidth();             // calculates and returns bandwidth in bytes per second
        inline PRInt32& getTemporary();         // returns a reference to bytes transferred
        inline void recordBytes(PRInt32 bytes); // record the transmission of some bytes
        inline PRBool isEnabled();              // indicates whether QOS is enabled
    private:
        // internal functions & data
        PRInt32  calculateBandwidth();
        void trashOldest();

        PRInt32 temporary;      // holds bytes transferred
                                // since the last snapshot

        TrafficSample* samples;    // circular array of traffic samples
        PRInt32 SampleNumber;       // number of samples in the array
        PRInt64 totalbytes; // sum of all the bytes from all traffic samples

        PRInt32 held, oldest, newest;
        PRIntervalTime maxAge; // maximum age of a traffic sample
        PRBool enabled;
};

class BandwidthManager : public Thread
{
    public:
        BandwidthManager();
        void run(void);
        static void init();
};

inline void TrafficStats :: incrementConnections()
{
    PR_AtomicIncrement(&connections);
};

inline void TrafficStats :: decrementConnections()
{
    PR_AtomicDecrement(&connections);
};

inline PRInt32& VSTrafficStats :: getTemporary()
{
    return temporary;
};

inline void VSTrafficStats :: recordBytes(PRInt32 bytes)
{
    PR_AtomicAdd(&temporary, bytes);
};

inline PRBool VSTrafficStats :: isEnabled()
{
    return enabled;
};

#endif

