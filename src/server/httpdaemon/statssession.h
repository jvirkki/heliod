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

#ifndef STATSSESSION_H
#define STATSSESSION_H

#include <stddef.h>
#include <nspr.h>

#include "httpdaemon/statsmanager.h"
#include "support/SimpleHash.h"

typedef struct HHString HHString;
struct StatsCacheEntry;

//-----------------------------------------------------------------------------
// StatsSession
//-----------------------------------------------------------------------------

class StatsSession {
public:
    StatsSession(const char* threadQName = NULL);
    ~StatsSession();

    void setMode(PRUint32 mode);
    void recordKeepaliveHit();
    void beginClient(PRNetAddr* remoteAddress);
    void beginRequest();
    inline ptrdiff_t inFunction(ptrdiff_t name);
    inline void beginFunction();
    inline void endFunction(int indexProfileBucket);
    inline void abortFunction();
    void beginProcessing(const VirtualServer* vs, const HHString& method, const HHString& uri);
    void endProcessing(const VirtualServer* vs, int statusHttp, PRInt64 countBytesReceived, PRInt64 countBytesTransmitted);
    void abortProcessing(const VirtualServer* vs);
    PRIntervalTime getRequestStartTime() { return ticksBeginRequest; }
    PRBool isFlushed();
    void flush();

    static int countMaxRequestsCached;
    static int countMaxCacheEntries;

private:
    void cacheRequest(StatsCacheEntry* entry, int statusHttp, PRInt64 countBytesReceived, PRInt64 countBytesTransmitted);
    void cacheProfiles(StatsCacheEntry* entry);
    void flushProfiles(StatsProfileNode* profile, const StatsCacheEntry* entry);
    void sumRequest(StatsRequestBucket* sum, const StatsRequestBucket* delta);
    void sumProfile(StatsProfileBucket* sum, const StatsProfileBucket* delta);
    StatsCacheEntry* allocEntry();
    StatsCacheEntry* getVsEntry(const VirtualServer* vs);
    PRBool isTimeForFlush();

    const PRIntervalTime ticksMaxCached;
    const int countProfileBuckets;
    const int countCacheEntries;
    const int sizeCacheEntry;
    int countDirtyCacheEntries;
    int countRequestsCached;
    StatsThreadNode* thread;
    StatsProfileBucket* profiles;
    StatsCacheEntry* entriesVs;
    StatsCacheEntry* entryThread;
    StatsCacheEntry* entryCurrentVs;
    int countKeepaliveHits;
    int countFunctionNesting;
    PRIntervalTime ticksBeginRequest;
    PRIntervalTime ticksBeginFunction;
    PRIntervalTime ticksEndFunction;
    PRIntervalTime ticksFlushed;
    SimpleIntHash vsEntryHash;
};

//-----------------------------------------------------------------------------
// StatsSession::setMode
//-----------------------------------------------------------------------------

inline void StatsSession::setMode(PRUint32 mode)
{
    if (thread) thread->threadStats.mode = mode;
}

//-----------------------------------------------------------------------------
// StatsSession::recordKeepaliveHit
//-----------------------------------------------------------------------------

inline void StatsSession::recordKeepaliveHit()
{
    countKeepaliveHits++;
}

//-----------------------------------------------------------------------------
// StatsSession::inFunction
//-----------------------------------------------------------------------------

inline ptrdiff_t StatsSession::inFunction(ptrdiff_t name)
{
    ptrdiff_t old = 0;

    const char* stringbase = StatsManager::getStringBase();
    if (thread && stringbase) {
        if (thread->threadStats.offsetFunctionName) {
            old = thread->threadStats.offsetFunctionName;
        }
        if (name) {
            thread->threadStats.offsetFunctionName = name;
        } else {
            thread->threadStats.offsetFunctionName = 0;
        }
    }

    return old;
}

//-----------------------------------------------------------------------------
// StatsSession::beginFunction
//-----------------------------------------------------------------------------

inline void StatsSession::beginFunction()
{
    if (!StatsManager::isInitialized()) return;

    if (countProfileBuckets) {
        if (!countFunctionNesting) {
            ticksBeginFunction = PR_IntervalNow();
        }
    }

    countFunctionNesting++;
}

//-----------------------------------------------------------------------------
// StatsSession::endFunction
//-----------------------------------------------------------------------------

inline void StatsSession::endFunction(int indexProfileBucket)
{
    if (!StatsManager::isInitialized()) return;

    countFunctionNesting--;

    if (!countFunctionNesting) {
        if (indexProfileBucket < countProfileBuckets) {
            PRIntervalTime ticksNow = PR_IntervalNow();
            PRIntervalTime ticksDispatch = ticksBeginFunction - ticksEndFunction;
            PRIntervalTime ticksFunction = ticksNow - ticksBeginFunction;

            profiles[STATS_PROFILE_ALL].countCalls++;
            profiles[STATS_PROFILE_ALL].ticksDispatch += ticksDispatch;
            profiles[STATS_PROFILE_ALL].ticksFunction += ticksFunction;

            profiles[indexProfileBucket].countCalls++;
            profiles[indexProfileBucket].ticksDispatch += ticksDispatch;
            profiles[indexProfileBucket].ticksFunction += ticksFunction;

            ticksEndFunction = ticksNow;
        }
    }
}

//-----------------------------------------------------------------------------
// StatsSession::abortFunction
//-----------------------------------------------------------------------------

inline void StatsSession::abortFunction()
{
    if (!StatsManager::isInitialized()) return;

    countFunctionNesting--;
}

//-----------------------------------------------------------------------------
// StatsSession::isFlushed
//-----------------------------------------------------------------------------

inline PRBool StatsSession::isFlushed()
{
    return (countRequestsCached == 0);
}

#endif // STATSSESSION_H
