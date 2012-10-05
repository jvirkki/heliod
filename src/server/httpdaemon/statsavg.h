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

#ifndef INSTANCESTATS_DEF_H_
#define INSTANCESTATS_DEF_H_

#include <nspr.h>
#include "public/iwsstats.h"

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry
//
// A class to store data to calculate request rate, error rate and response
// time (for request).
//-----------------------------------------------------------------------------

class StatsAvgQueueEntry
{
    private:
        PRUint64 countRequests_;
        PRUint64 countErrors_;
        PRUint64 countProfileRequests_;
        PRUint64 ticksResponse_;
    public:
        PRIntervalTime captureTime_;

        // Default constructor
        StatsAvgQueueEntry(void);

        // Copy constructor. (Doesn't do special). Compiler generated
        // constructor can do the same job.
        StatsAvgQueueEntry(const StatsAvgQueueEntry& rhs);

        // Utility constructor to initialize members from arguments.
        StatsAvgQueueEntry(PRUint64 countRequests,
                           PRUint64 countErrors,
                           PRUint64 countProfileRequests,
                           PRUint64 ticksResponse,
                           PRIntervalTime captureTime);

        // Destructor
        ~StatsAvgQueueEntry(void);

        // Operator =. Doesn't do much special. Compiler generated operator=
        // could do the job (I guess).
        StatsAvgQueueEntry& operator= (const StatsAvgQueueEntry& rhs);
        void setValues(PRUint64 countRequests,
                       PRUint64 countErrors_,
                       PRUint64 countProfileRequests_,
                       PRUint64 ticksResponse_,
                       PRIntervalTime captureTime);

        // Calculate the request average
        PRFloat64 getRequestAvg(void) const;

        // Calculate the error average.
        PRFloat64 getErrorAvg(void) const;

        // Calculate the response time in ticks.
        PRFloat64 getResponseTimeAvg(void) const;

        // Find the diff between entry1 and entry2 and saves in diff entry.
        static void getDiff(StatsAvgQueueEntry& diff,
                            const StatsAvgQueueEntry& entry1,
                            const StatsAvgQueueEntry& entry2);
};

//-----------------------------------------------------------------------------
// StatsAverageQueue
//
// StatsAverageQueue class maintains a queue of StatsAvgQueueEntry elements.
// Queue size is determined by nSizeQueue_. Averages can be calculated from
// diff of entries. This queue tries to keep stats entries which are
// avgInterval_ interval apart. There are possiblities that few entries at the
// end are beyond maxSecondsInterval_ time but they are not considered useful
// for calculating averages.
//-----------------------------------------------------------------------------

class StatsAverageQueue
{
    private:
        // Interval in seconds for which we are interested in finding the
        // average. Entries beyond maxSecondsInterval_ are not considered
        // valid to find the average unless queue size is 1.
        const int maxSecondsInterval_;

        // Average interval after which new entries are accepted. If new entry
        // is added before avgInterval_ time then entries are not added in
        // queue. This is to avoid only keeping very recent entries.
        const PRIntervalTime avgInterval_;

        // Maximum size of queue. Before adding any new entry after
        // nSizeQueue_, oldes entry is removed.
        const int nSizeQueue_;

        // Pointer to store the queue items.
        StatsAvgQueueEntry* queueItems_;

        // Number of current items in queue. After queue become full, it
        // always remain equal to nSizeQueue_
        int nItems_;

        // Head element of queue.
        int nHead_;

        // Tail element of queue.
        int nTail_;

        // Flag to save the condition if the new entry's time has crossed the
        // maxIntervalTime from the beginning of stats collection. Once this
        // is reached, it always remains PR_TRUE for the life of this object.
        PRBool fReachedMaxInterval_;

        // Variable to store the first node's (in the life time of queue)
        // capture time.
        PRIntervalTime statsBeginTime_;

        // Add entry to queue. Queue size should be less that nSizeQueue_
        // before calling this function.
        void add(const StatsAvgQueueEntry& entry);

        // remove the oldest entry from queue.
        void remove(void);

        // Retrieve the current items in queue.
        int length(void);

        // Retrieve nIndex (begin from 0) element from tail.
        const StatsAvgQueueEntry& getFromTail(int nIndex);

        // If fReachedMaxInterval_ is PR_FALSE then test captureTime has
        // crossed maxIntervalTime.
        void testAndSetMaxIntervalReached(PRIntervalTime entryCaptureTime);

        // Test to check if maxSecondsInterval_ is reached.
        PRBool isMaxIntervalReached(void);

    public:
        // Construct object.
        StatsAverageQueue(int maxSecondsInterval,
                          int queueEntries);

        // Destructor
        ~StatsAverageQueue(void);

        // Check few conditions and if valid then add the entry.
        void addEntryIf(const StatsAvgQueueEntry& entry);

        // Calculated request, error and response averages.
        void calcAverages(PRFloat64& requestAvg,
                          PRFloat64& errorAvg,
                          PRFloat64& responseTimeAvg);
};

//-----------------------------------------------------------------------------
// StatsMovingAverageHandler
//
// A class which maintains the average queue for 1, 5 and 15 minutes. Also
// provide method to calculate averages.
//-----------------------------------------------------------------------------

class StatsMovingAverageHandler
{
    private:
        // Average queue for 1 minute average.
        StatsAverageQueue* queueOneMinute_;

        // Average queue for 5 minute average.
        StatsAverageQueue* queueFiveMinute_;

        // Average queue for 15 minute average.
        StatsAverageQueue* queueFifteenMinute_;

        // Calculate the size of the queue required.
        static int calcQueueSize(int pollInterval, int queueMaxTime);
    public:
        // Constructor.
        StatsMovingAverageHandler(int secondsUpdateInterval);

        // Destructor
        ~StatsMovingAverageHandler();

        // Update the average queue.
        void
        updateAverageQueue(const StatsAccumulatedVSSlot* accumulatedVSStats,
                           PRIntervalTime captureTime);

        // Calculate averages from the average queue.
        void calcAverages(StatsAccumulatedVSSlot* accumulatedVSStats);
};


//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::getRequestAvg
//-----------------------------------------------------------------------------

inline
PRFloat64
StatsAvgQueueEntry::getRequestAvg(void) const
{
    float secondsInterval = PR_IntervalToSeconds(captureTime_);
    return (captureTime_ > 0)
           ? ((PRFloat64) (PRInt64) (countRequests_) /
                   PR_IntervalToSeconds(captureTime_))
           : 0;
}

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::getErrorAvg
//-----------------------------------------------------------------------------

PRFloat64
inline
StatsAvgQueueEntry::getErrorAvg(void) const
{
    float secondsIterval = PR_IntervalToSeconds(captureTime_);
    PRFloat64 countErrors = (PRFloat64) (PRInt64) countErrors_;
    return (captureTime_ > 0)
           ? ((countErrors) / PR_IntervalToSeconds(captureTime_))
           : 0;
}

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::getResponseTimeAvg
//-----------------------------------------------------------------------------

inline
PRFloat64
StatsAvgQueueEntry::getResponseTimeAvg(void) const
{
    return ((captureTime_ > 0) && (countProfileRequests_ > 0))
           ? ((PRFloat64) (PRInt64) (ticksResponse_)) /
             ((PRInt64) countProfileRequests_)
           : 0;
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::length
//-----------------------------------------------------------------------------

inline
int
StatsAverageQueue::length(void)
{
    return nItems_;
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::isMaxIntervalReached
//-----------------------------------------------------------------------------

inline
PRBool
StatsAverageQueue::isMaxIntervalReached(void)
{
    return fReachedMaxInterval_;
}

#endif

