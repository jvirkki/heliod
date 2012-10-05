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

#include "httpdaemon/statsavg.h"

#define SECONDS_IN_ONE_MINUTE (60)
#define SECONDS_IN_FIVE_MINUTES (SECONDS_IN_ONE_MINUTE * 5)
#define SECONDS_IN_FIFTEEN_MINUTES (SECONDS_IN_ONE_MINUTE * 15)

#define MIN_QUEUE_SIZE 2
#define MAX_QUEUE_SIZE 60


////////////////////////////////////////////////////////////////

// StatsAvgQueueEntry Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::StatsAvgQueueEntry
//-----------------------------------------------------------------------------

StatsAvgQueueEntry::StatsAvgQueueEntry(void)
{
    countRequests_ = 0;
    countErrors_ = 0;
    countProfileRequests_ = 0;
    ticksResponse_ = 0;
    captureTime_ = 0;
}

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::StatsAvgQueueEntry
//-----------------------------------------------------------------------------

StatsAvgQueueEntry::StatsAvgQueueEntry(const StatsAvgQueueEntry& rhs)
{
    setValues(rhs.countRequests_, rhs.countErrors_,
              rhs.countProfileRequests_, rhs.ticksResponse_, rhs.captureTime_);
}

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::StatsAvgQueueEntry
//-----------------------------------------------------------------------------

StatsAvgQueueEntry::StatsAvgQueueEntry(PRUint64 countRequests,
                                       PRUint64 countErrors,
                                       PRUint64 countProfileRequests,
                                       PRUint64 ticksResponse,
                                       PRIntervalTime captureTime)
{
    setValues(countRequests, countErrors, countProfileRequests,
              ticksResponse, captureTime);
}

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::~StatsAvgQueueEntry
//-----------------------------------------------------------------------------

StatsAvgQueueEntry::~StatsAvgQueueEntry(void)
{
}

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::operator=
//-----------------------------------------------------------------------------

StatsAvgQueueEntry&
StatsAvgQueueEntry::operator=(const StatsAvgQueueEntry& rhs)
{
    if (this != &rhs)
    {
        setValues(rhs.countRequests_, rhs.countErrors_,
                  rhs.countProfileRequests_,
                  rhs.ticksResponse_, rhs.captureTime_);
    }
    return *this;
}

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::getDiff
//-----------------------------------------------------------------------------

void StatsAvgQueueEntry::getDiff(StatsAvgQueueEntry& diff,
                                 const StatsAvgQueueEntry& entry1,
                                 const StatsAvgQueueEntry& entry2)
{
    diff.setValues(entry1.countRequests_ - entry2.countRequests_,
                   entry1.countErrors_ - entry2.countErrors_,
                   entry1.countProfileRequests_ - entry2.countProfileRequests_,
                   entry1.ticksResponse_ - entry2.ticksResponse_,
                   entry1.captureTime_ - entry2.captureTime_);
}

//-----------------------------------------------------------------------------
// StatsAvgQueueEntry::setValues
//-----------------------------------------------------------------------------

void StatsAvgQueueEntry::setValues(PRUint64 countRequests,
                                   PRUint64 countErrors,
                                   PRUint64 countProfileRequests,
                                   PRUint64 ticksResponse,
                                   PRIntervalTime captureTime)
{
    countRequests_        = countRequests;
    countErrors_          = countErrors;
    countProfileRequests_ = countProfileRequests;
    ticksResponse_        = ticksResponse;
    captureTime_          = captureTime;
}




////////////////////////////////////////////////////////////////

// StatsAverageQueue Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// StatsAverageQueue::StatsAverageQueue
//-----------------------------------------------------------------------------

StatsAverageQueue::StatsAverageQueue(int maxSecondsInterval,
                                     int queueEntries):
                        maxSecondsInterval_(maxSecondsInterval),
                        avgInterval_(PR_SecondsToInterval(maxSecondsInterval_) /
                                     queueEntries),
                        nSizeQueue_(queueEntries)
{
    PR_ASSERT(nSizeQueue_ >= 2);
    queueItems_      = new StatsAvgQueueEntry[queueEntries];
    nHead_           = 0;
    nTail_           = 0;
    nItems_          = 0;
    statsBeginTime_  = 0;
    fReachedMaxInterval_ = PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::~StatsAverageQueue
//-----------------------------------------------------------------------------

StatsAverageQueue::~StatsAverageQueue(void)
{
    delete[] queueItems_;
    queueItems_ = 0;
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::add
//-----------------------------------------------------------------------------

void StatsAverageQueue::add(const StatsAvgQueueEntry& entry)
{
    if (nItems_ >= nSizeQueue_)
    {
        PR_ASSERT(0);
        return;
    }
    queueItems_[nTail_] = entry;
    ++nTail_;
    if (nTail_ == nSizeQueue_)
        nTail_ = 0;
    ++nItems_;
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::remove
//-----------------------------------------------------------------------------

void StatsAverageQueue::remove(void)
{
    PR_ASSERT(nItems_ > 0);
    ++nHead_;
    --nItems_;
    if (nHead_ == nSizeQueue_)
        nHead_ = 0;
    return;
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::getFromTail
//-----------------------------------------------------------------------------

const StatsAvgQueueEntry& StatsAverageQueue::getFromTail(int nIndex)
{
    PR_ASSERT(nIndex < nItems_);
    PR_ASSERT(nItems_ > 0);
    int nItemIndex = nHead_ + (nItems_ - 1) - nIndex;
    if (nItemIndex >= nSizeQueue_)
        nItemIndex -= nSizeQueue_;
    PR_ASSERT(nItemIndex < nSizeQueue_);
    PR_ASSERT(nItemIndex >= 0);

    return queueItems_[nItemIndex];
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::testAndSetMaxIntervalReached
//-----------------------------------------------------------------------------

void
StatsAverageQueue::testAndSetMaxIntervalReached(PRIntervalTime entryCaptureTime)
{
    if (fReachedMaxInterval_ == PR_FALSE)
    {
        PRIntervalTime maxInterval = PR_SecondsToInterval(maxSecondsInterval_);
        if (entryCaptureTime - statsBeginTime_ > maxInterval)
        {
            fReachedMaxInterval_ = PR_TRUE;
        }
    }
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::addEntryIf
//
// After the queue is full, if we try to add the entry, oldest entry is
// removed (head entry). If entry arrived too early (time interval between
// this new entry and tail entry is less than avgInterval_) then this entry is
// not added. The reason is to keep the queue entries avgInterval_ apart as
// best as possible.
//-----------------------------------------------------------------------------

void StatsAverageQueue::addEntryIf(const StatsAvgQueueEntry& entry)
{
    if (nItems_ < 2)
    {
        if (nItems_ == 0)
            statsBeginTime_  = entry.captureTime_;
        else
            testAndSetMaxIntervalReached(entry.captureTime_);
        add(entry);
        return;
    }
    testAndSetMaxIntervalReached(entry.captureTime_);
    const StatsAvgQueueEntry& tailEntry = getFromTail(0);
    if (entry.captureTime_ - tailEntry.captureTime_ > avgInterval_)
    {
        // If queue size is reached full then first remove the entry.
        if (length() >= nSizeQueue_)
        {
            remove();
        }
        add(entry);
    }
    // else don't add the entry.
}

//-----------------------------------------------------------------------------
// StatsAverageQueue::calcAverages
//-----------------------------------------------------------------------------

void StatsAverageQueue::calcAverages(PRFloat64& requestAvg,
                                     PRFloat64& errorAvg,
                                     PRFloat64& responseTimeAvg)
{
    requestAvg = 0;
    errorAvg = 0;
    responseTimeAvg = 0;

    // If the recent stats entry has not reached maxSecondsInterval_ then we
    // can't calcuate the average.
    if (isMaxIntervalReached() != PR_TRUE)
        return;

    int lengthQueue = length();
    if (lengthQueue < 2)
    {
        return;
    }

    const StatsAvgQueueEntry& tailEntry = getFromTail(0);
    PRIntervalTime maxInterval = PR_SecondsToInterval(maxSecondsInterval_);
    PRUint32 prevDiffFromMax = 0;
    // find the last entry whose capture time is within maxSecondsInterval.
    int nIndex = 0;
    for (nIndex = lengthQueue; nIndex >= 2; --nIndex)
    {
        const StatsAvgQueueEntry& entry = getFromTail(nIndex -1);
        PRIntervalTime curEntryDiff =
                            (tailEntry.captureTime_ - entry.captureTime_);
        if (curEntryDiff <= maxInterval)
        {
            // This is the last entry which is under maxInterval time. We are
            // not interested in entries which is beyond maxInterval time.
            PRUint32 curDiffFromMax = maxInterval - curEntryDiff;
            if (nIndex < lengthQueue)
            {
                if (curDiffFromMax > prevDiffFromMax)
                {
                    // Previous node was closer to maxInterval time so choose
                    // the previous node.
                    ++nIndex;
                }
            }
            break;
        }
        else
        {
            prevDiffFromMax = curEntryDiff - maxInterval;
        }
    }
    if (nIndex == 1)
    {
        // Not found any entry.
        ++nIndex;
    }
    int nAvgEntriesCount = nIndex;
    const StatsAvgQueueEntry& lastEntry = getFromTail(nAvgEntriesCount - 1);
    StatsAvgQueueEntry diffEntry;
    StatsAvgQueueEntry::getDiff(diffEntry, tailEntry, lastEntry);

    requestAvg = diffEntry.getRequestAvg();
    errorAvg = diffEntry.getErrorAvg();
    responseTimeAvg = diffEntry.getResponseTimeAvg();
    return;
}



////////////////////////////////////////////////////////////////

// StatsMovingAverageHandler Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsMovingAverageHandler::StatsMovingAverageHandler
//-----------------------------------------------------------------------------

StatsMovingAverageHandler::StatsMovingAverageHandler(int secondsUpdateInterval)
{
    const int oneMinuteQueueSize = calcQueueSize(secondsUpdateInterval,
                                                 SECONDS_IN_ONE_MINUTE);
    queueOneMinute_ = new StatsAverageQueue(SECONDS_IN_ONE_MINUTE,
                                            oneMinuteQueueSize);

    const int fiveMinuteQueueSize = calcQueueSize(secondsUpdateInterval,
                                                  SECONDS_IN_FIVE_MINUTES);
    queueFiveMinute_ = new StatsAverageQueue(SECONDS_IN_FIVE_MINUTES,
                                             fiveMinuteQueueSize);
    const int fifteenMinuteQueueSize = calcQueueSize(
                                            secondsUpdateInterval,
                                            SECONDS_IN_FIFTEEN_MINUTES);

    queueFifteenMinute_ = new StatsAverageQueue(SECONDS_IN_FIFTEEN_MINUTES,
                                                fifteenMinuteQueueSize);
}

//-----------------------------------------------------------------------------
// StatsMovingAverageHandler::~StatsMovingAverageHandler
//-----------------------------------------------------------------------------

StatsMovingAverageHandler::~StatsMovingAverageHandler()
{
    delete queueOneMinute_;
    delete queueFiveMinute_;
    delete queueFifteenMinute_;
}

//-----------------------------------------------------------------------------
// StatsMovingAverageHandler::updateAverageQueue
//-----------------------------------------------------------------------------

void
StatsMovingAverageHandler::updateAverageQueue(
                            const StatsAccumulatedVSSlot* accumulatedVSStats,
                            PRIntervalTime captureTime)
{
    PRUint64 countErrors = accumulatedVSStats->requestBucket.count4xx
                           + accumulatedVSStats->requestBucket.count4xx;
    const StatsProfileBucket& allReqsProfileBucket =
                    accumulatedVSStats->allReqsProfileBucket;
    PRUint64 ticksResponseTime = allReqsProfileBucket.ticksDispatch +
                                 allReqsProfileBucket.ticksFunction;
    StatsAvgQueueEntry entry(accumulatedVSStats->requestBucket.countRequests,
                             countErrors,
                             allReqsProfileBucket.countRequests,
                             ticksResponseTime,
                             captureTime);
    queueOneMinute_->addEntryIf(entry);
    queueFiveMinute_->addEntryIf(entry);
    queueFifteenMinute_->addEntryIf(entry);
}

//-----------------------------------------------------------------------------
// StatsMovingAverageHandler::calcAverages
//
// If web server start time is less than 1,5 or 15 minute then the averages
// will be set to 0.
//-----------------------------------------------------------------------------

void StatsMovingAverageHandler::calcAverages(
                        StatsAccumulatedVSSlot* accumulatedVSStats)
{
    StatsAvgBucket& requestAvgBucket = accumulatedVSStats->requestAvgBucket;
    StatsAvgBucket& errorAvgBucket = accumulatedVSStats->errorAvgBucket;
    StatsAvgBucket& responseTimeAvgBucket =
                        accumulatedVSStats->responseTimeAvgBucket;
    queueOneMinute_->calcAverages(requestAvgBucket.oneMinuteAverage,
                                  errorAvgBucket.oneMinuteAverage,
                                  responseTimeAvgBucket.oneMinuteAverage);
    queueFiveMinute_->calcAverages(requestAvgBucket.fiveMinuteAverage,
                                   errorAvgBucket.fiveMinuteAverage,
                                   responseTimeAvgBucket.fiveMinuteAverage);
    queueFifteenMinute_->calcAverages(
                                requestAvgBucket.fifteenMinuteAverage,
                                errorAvgBucket.fifteenMinuteAverage,
                                responseTimeAvgBucket.fifteenMinuteAverage);
}

//-----------------------------------------------------------------------------
// StatsMovingAverageHandler::calcQueueSize
//
// Calculate the elements required in queue based upon the time stats are
// updated. It is bounded by MIN_QUEUE_SIZE and MAX_QUEUE_SIZE.
//-----------------------------------------------------------------------------

int StatsMovingAverageHandler::calcQueueSize(int secondsUpdateInterval,
                                             int queueMaxTime)
{
    // 2 is added in queueSize to take care of boundary elements. For example
    // if queueMaxTime is 60 seconds and secondsUpdateInterval is 7 seconds
    // then queueSize is 60/7 + 2 ==> 10. This will allow queue to save stats
    // data from 0 to 63 seconds stats.
    int queueSize = (queueMaxTime / secondsUpdateInterval) + 2;
    if (queueSize < MIN_QUEUE_SIZE)
        queueSize = MIN_QUEUE_SIZE;
    else if (queueSize > MAX_QUEUE_SIZE)
        queueSize = MAX_QUEUE_SIZE;
    return queueSize;
}

