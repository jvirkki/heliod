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

#include "prio.h"
#include "prlog.h"
#include "prerror.h"

#include "httpdaemon/throttling.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/dbthttpdaemon.h"

#include "base/ereport.h"

PRIntervalTime TrafficStats::recomputeinterval = PR_MillisecondsToInterval(100);

// generic TrafficStats class
TrafficStats :: TrafficStats ()
{
    connections = bandwidth = 0;
};

PRInt32 TrafficStats :: getBandwidth() const   // returns previously computed bandwidth in bytes per second
{
    return bandwidth;
};

void TrafficStats :: setBandwidth(PRInt32 bw)
{
    PR_AtomicSet(&bandwidth, bw);
};

PRInt32 TrafficStats :: getConnections() const
{
    return connections;
};

void TrafficStats :: setConnections(PRInt32 conn)
{
    PR_AtomicSet(&connections, conn);
};

// TrafficSample

TrafficSample :: TrafficSample(PRInt32 b, PRIntervalTime t)
{
    bytes = b;
    timestamp = t;
};

// VSTrafficStats

VSTrafficStats :: VSTrafficStats(PRBool enable, PRInt32 metrics)
: temporary(0),
  samples(NULL),
  SampleNumber(0),
  totalbytes(0),
  held(0),
  oldest(0),
  newest(0),
  maxAge(PR_SecondsToInterval(metrics)),
  enabled(enable)
{
    if (enabled)
    {
        PRInt32 divisor = recomputeinterval;
        if (0 == divisor)
        {
            divisor++;
        };
        
        SampleNumber = PR_MillisecondsToInterval(1000*metrics)/divisor;
        if (0 == SampleNumber)
        {
            SampleNumber++;
        };
        samples = new TrafficSample[SampleNumber];
    };
};

VSTrafficStats :: ~VSTrafficStats()
{
    if (samples )
        delete[] samples;
};

// add a sample into the traffic statistics array
void VSTrafficStats :: record(TrafficSample& asample)
{
    PR_ASSERT(SampleNumber > 0 && samples != NULL);

    if (held == SampleNumber)
    {
    };

    if (0 == held)
    {
        newest = oldest = 0;
        samples[newest] = asample;
    }
    else
    if (newest == SampleNumber-1)
    {
        // we filled the array and need to wrap
        newest = 0;
        
        if (oldest == newest)
            totalbytes -= samples[oldest].bytes;

        samples[newest] = asample;

        // if the oldest sample was at the start of the array,
        // push it by one
        if (0 == oldest)
            oldest = 1;
    }
    else
    {
        // append to the end of the array
        newest++;
        if (oldest == newest)
            totalbytes -= samples[oldest].bytes;

        samples[newest] = asample;
        if (oldest == newest)
        {
            // we overwrote the tail of the array
            oldest++;
            // are we at the end ?
            if (oldest == SampleNumber)
            {
                oldest = 0;
            };
        };
    };
    if (held<SampleNumber)
        held++;
    totalbytes += asample.bytes;
};

void VSTrafficStats :: trashOldest()
{
    // discount bytes from total
    PRInt32 toremove = samples[oldest].bytes;

    totalbytes -= toremove;

    // increment index of oldest
    oldest ++ ;
    // are we at the end ?
    if (oldest == SampleNumber)
    {
        // yes, wrap around
        oldest = 0;
    };

    // decrement total number of samples
    held--;
};

PRInt32 VSTrafficStats :: computeBandwidth()
{
    setBandwidth(calculateBandwidth());
    return getBandwidth();
};

PRInt32 VSTrafficStats :: calculateBandwidth()
{
    // do we have any traffic recorded ?
    if (0 == totalbytes)
    {
        // no, therefore no need to calculate anything
        return 0;
    };

    PR_ASSERT(held); // if we have traffic, we must have samples

    // first, we need to select the samples we actually want to use, as
    // some might be too old

    PRIntervalTime current = samples[newest].timestamp;
    PRIntervalTime oldstamp = current;
    while (held)
    {
        oldstamp = samples[oldest].timestamp;
        if (current - oldstamp > maxAge)
        {
            // the sample is too old, trash it
            trashOldest();
        }
        else
            break;
    };

    // did we get rid of all the samples ?
    if ( (0 == held) || (0 == totalbytes) )
    {
        PR_ASSERT( 0 == totalbytes ); // then we should have no traffic
        return 0; // and therefore no bandwidth
    };

    PR_ASSERT(totalbytes); // we got traffic
    oldstamp = samples[oldest].timestamp;
    PRIntervalTime elapsed = current - oldstamp;
    if (0 == elapsed)
    {
                  // zero interval. we probably only have one sample here,
        return 0; // and this is not enough to calculate the bandwidth
    };

    PRInt32 microseconds = PR_IntervalToMicroseconds(elapsed);
    if (0 == microseconds)
    {
        // we need at least a one microsecond interval to calculate the bandwidth
        return 0;
    };

    return (PRInt32) ( (double) ((double) ((double)1000000.0*(double)totalbytes) / (double) microseconds)) ;
};

BandwidthManager :: BandwidthManager() : Thread("Bandwidth thread")
{
};

void BandwidthManager :: run(void)
{
    PRLock* lock = PR_NewLock();
    PRCondVar* condvar = PR_NewCondVar(lock);
    PRIntervalTime reconfiginterval = PR_SecondsToInterval(5);
    PRBool wasQOSon = PR_FALSE;

    while (PR_TRUE)
    {
        PRIntervalTime current = PR_IntervalNow();

        // acquire the configuration object
        Configuration* config = ConfigurationManager :: getConfiguration();
        PR_ASSERT(config);

        // we only calculate stats if QOS is enabled
        PRBool QOSon = config->qos.enabled;

        if (QOSon != wasQOSon)
        {
            if (QOSon)
            {
                ereport(LOG_VERBOSE, "QOS statistics tracking enabled");
            }
            else
            {
                ereport(LOG_VERBOSE, "QOS statistics tracking disabled");
            }
            wasQOSon = QOSon;
        }

        if (QOSon)
        {
            // global server counters
            PRInt64 serverconn = 0, serverbw = 0;
    
            // walk the list of all virtual servers
            for (int i = 0; i < config->getVSCount(); i++)
            {
                VSTrafficStats& stats = config->getVS(i)->getTrafficStats();
                PRInt32 transferred = PR_AtomicSet(&stats.getTemporary(), 0); // reset traffic to 0
        
                // make a traffic sample
                TrafficSample sample(transferred, current);
                // and record it
                stats.record(sample);
        
                // then calculate the bandwidth currently used
                PRInt32 bw = stats.computeBandwidth();
    
                // add stats from this VS to the global stats
                serverconn += stats.getConnections();
                serverbw += bw;
            };

            // set the global stats
            TrafficStats& stats = config->getTrafficStats();

            stats.setConnections(serverconn);
            stats.setBandwidth(serverbw);
        };

        config->unref();

        PR_Lock(lock);
        if (PR_TRUE == QOSon)
        {
            // sleep for 100ms between bandwidth recalculations
            PRStatus status = PR_WaitCondVar(condvar, TrafficStats::recomputeinterval);
        }
        else
        {
            // sleep for 5s if QOS is disabled
            // this will cause less context switches and still
            // allow QOS to be dynamically configured
            PRStatus status = PR_WaitCondVar(condvar, reconfiginterval);
        };
        PR_Unlock(lock);
    };
};

void BandwidthManager :: init()
{
    BandwidthManager* bwThread = new BandwidthManager();
    if (bwThread->start(PR_GLOBAL_BOUND_THREAD, PR_UNJOINABLE_THREAD) != PR_SUCCESS)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_Configuration_CannotStartBMThread));
    };
};

