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

#include "time/nstime.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/statssession.h"
#include "httpdaemon/vsconf.h"
#include "support/prime.h"

//-----------------------------------------------------------------------------
// StatsSession static variable definitions
//-----------------------------------------------------------------------------

int StatsSession::countMaxRequestsCached = 2000;
int StatsSession::countMaxCacheEntries = 40;

//-----------------------------------------------------------------------------
// StatsCacheEntry
//-----------------------------------------------------------------------------

struct StatsCacheEntry {
    StatsRequestBucket request;
    StatsProfileBucket profiles[1];
};

//-----------------------------------------------------------------------------
// minimum
//-----------------------------------------------------------------------------

static inline int minimum(int a, int b)
{
    return (a < b) ? a : b;        
}

//-----------------------------------------------------------------------------
// StatsSession::StatsSession
//-----------------------------------------------------------------------------

StatsSession::StatsSession(const char* threadQName)
: ticksMaxCached(StatsManager::getUpdateInterval()), 
  countProfileBuckets(StatsManager::getProfileBucketCount()),
  countCacheEntries(countMaxCacheEntries),
  sizeCacheEntry(sizeof(*entryThread) - sizeof(entryThread->profiles) + countProfileBuckets * sizeof(entryThread->profiles)),
  countDirtyCacheEntries(0),
  countRequestsCached(0),
  thread(0),
  profiles(0),
  entriesVs(0),
  entryThread(0),
  entryCurrentVs(0),
  countKeepaliveHits(0),
  ticksFlushed(PR_IntervalNow()),
  countFunctionNesting(0),
  vsEntryHash(findPrime(countCacheEntries))
{
    if (!StatsManager::isInitialized()) return;

    thread = StatsManager::allocThreadSlot(threadQName);

    if (countProfileBuckets) {
        profiles = (StatsProfileBucket*)malloc(countProfileBuckets * sizeof(profiles[0]));
        memset(profiles, 0, countProfileBuckets * sizeof(profiles[0]));
    }

    entriesVs = (StatsCacheEntry*)malloc(countCacheEntries * sizeCacheEntry);
    entryThread = (StatsCacheEntry*)malloc(sizeCacheEntry);
    memset(entryThread, 0, sizeCacheEntry);
}

//-----------------------------------------------------------------------------
// StatsSession::~StatsSession
//-----------------------------------------------------------------------------

StatsSession::~StatsSession()
{
    if (!StatsManager::isInitialized()) return;

    setMode(STATS_THREAD_EMPTY);
    flush();
    free(profiles);
    free(entriesVs);
    free(entryThread);
}

//-----------------------------------------------------------------------------
// StatsSession::beginClient
//-----------------------------------------------------------------------------

void StatsSession::beginClient(PRNetAddr* remoteAddress)
{
    if (!StatsManager::isInitialized()) return;

    PRTime now = ft_timeNow();

    // Update realtime thread stats
    thread->lock();
    thread->threadStats.mode = STATS_THREAD_REQUEST;
    net_addr_copy(&thread->threadStats.addressClient, remoteAddress);
    thread->threadStats.timeRequestStarted = now;
    thread->unlock();
}

//-----------------------------------------------------------------------------
// StatsSession::beginRequest
//-----------------------------------------------------------------------------

void StatsSession::beginRequest()
{
    // We always do this PR_IntervalNow() (even if profiling is disabled) as
    // it's used for profiling AND to determine when we need to flush
    ticksBeginRequest = PR_IntervalNow();
    ticksEndFunction = ticksBeginRequest;

    if (!StatsManager::isInitialized()) return;

    if (countProfileBuckets) {
        memset(profiles, 0, countProfileBuckets * sizeof(profiles[0]));
    }

    if (thread->threadStats.mode != STATS_THREAD_REQUEST) {
        PRTime now = ft_timeNow();

        // Update realtime thread stats
        thread->lock();
        thread->threadStats.mode = STATS_THREAD_REQUEST;
        thread->threadStats.timeRequestStarted = now;
        thread->unlock();
    }
}

//-----------------------------------------------------------------------------
// StatsSession::beginProcessing
//-----------------------------------------------------------------------------

void StatsSession::beginProcessing(const VirtualServer* vs, const HHString& method, const HHString& uri)
{
    if (!StatsManager::isInitialized()) return;

    PR_ASSERT(countFunctionNesting == 0);
    countFunctionNesting = 0;

    PR_ASSERT(thread->threadStats.vsId[0] == '\0');
    PR_ASSERT(thread->threadStats.requestBucket.method[0] == '\0');
    PR_ASSERT(thread->threadStats.requestBucket.uri[0] == '\0');

    int vsidlen = minimum(strlen(vs->name), sizeof(thread->threadStats.vsId) - 1);
    int methodlen = minimum(method.len, sizeof(thread->threadStats.requestBucket.method) - 1);
    int urilen = minimum(uri.len, sizeof(thread->threadStats.requestBucket.uri) - 1);

    // Update realtime thread stats
    thread->lock();
    thread->threadStats.mode = STATS_THREAD_PROCESSING;
    memcpy(thread->threadStats.vsId, vs->name, vsidlen);
    thread->threadStats.vsId[vsidlen] = '\0';
    memcpy(thread->threadStats.requestBucket.method, method.ptr, methodlen);
    thread->threadStats.requestBucket.method[methodlen] = '\0';
    memcpy(thread->threadStats.requestBucket.uri, uri.ptr, urilen);
    thread->threadStats.requestBucket.uri[urilen] = '\0';
    thread->unlock();

    // Get a StatsCacheEntry for this VS.  We will buffer VS-specific stats
    // updates in this entry until StatsSession::flush() is called.
    entryCurrentVs = getVsEntry(vs);

    // Cache request URI for this VS
    memcpy(entryCurrentVs->request.method, method.ptr, methodlen);
    entryCurrentVs->request.method[methodlen] = '\0';
    memcpy(entryCurrentVs->request.uri, uri.ptr, urilen);
    entryCurrentVs->request.uri[urilen] = '\0';
}

//-----------------------------------------------------------------------------
// StatsSession::endProcessing
//-----------------------------------------------------------------------------

void StatsSession::endProcessing(const VirtualServer* vs, int statusHttp, PRInt64 countBytesReceived, PRInt64 countBytesTransmitted)
{
    if (!StatsManager::isInitialized()) return;

    PR_ASSERT(countFunctionNesting == 0);

    // If there wasn't already a VS associated with this request... (N.B.
    // malformed requests may not yet have any associated VS)    
    if (!entryCurrentVs) {
        entryCurrentVs = getVsEntry(vs);
    }

    // Cache profile and request counter deltas for this VS
    cacheProfiles(entryCurrentVs);
    cacheRequest(entryCurrentVs, statusHttp, countBytesReceived, countBytesTransmitted);

    // End of the request for this VS
    entryCurrentVs = 0;

    // Cache profile and request counter deltas for this thread
    cacheProfiles(entryThread);
    cacheRequest(entryThread, statusHttp, countBytesReceived, countBytesTransmitted);

    // We've updated StatsCacheEntrys
    countRequestsCached++;

    // The session is idle
    inFunction(0);
    thread->lock();
    thread->threadStats.vsId[0] = '\0';
    thread->threadStats.requestBucket.method[0] = '\0';
    thread->threadStats.requestBucket.uri[0] = '\0';
    thread->threadStats.timeRequestStarted = 0;
    thread->unlock();

    // Implicit flush if it's time
    if (isTimeForFlush()) flush();
}

//-----------------------------------------------------------------------------
// StatsSession::abortProcessing
//-----------------------------------------------------------------------------

void StatsSession::abortProcessing(const VirtualServer* vs)
{
    if (!StatsManager::isInitialized()) return;

    PR_ASSERT(countFunctionNesting == 0);

    // End of the request for this VS
    entryCurrentVs = 0;

    // The session is idle
    inFunction(0);
    thread->lock();
    thread->threadStats.vsId[0] = '\0';
    thread->threadStats.requestBucket.method[0] = '\0';
    thread->threadStats.requestBucket.uri[0] = '\0';
    thread->threadStats.timeRequestStarted = 0;
    thread->unlock();
}

//-----------------------------------------------------------------------------
// StatsSession::cacheRequest
//-----------------------------------------------------------------------------

void StatsSession::cacheRequest(StatsCacheEntry* entry, int statusHttp, PRInt64 countBytesReceived, PRInt64 countBytesTransmitted)
{
    StatsRequestBucket& request = entry->request;

    request.countRequests++;
    request.countBytesReceived += countBytesReceived;
    request.countBytesTransmitted += countBytesTransmitted;

    switch (statusHttp / 100) {
    case 2:  request.count2xx++; break; 
    case 3:  request.count3xx++; break; 
    case 4:  request.count4xx++; break; 
    case 5:  request.count5xx++; break; 
    default: request.countOther++; break;
    }

    switch (statusHttp) {
    case 200: request.count200++; break;
    case 302: request.count302++; break;
    case 304: request.count304++; break;
    case 400: request.count400++; break;
    case 401: request.count401++; break;
    case 403: request.count403++; break;
    case 404: request.count404++; break;
    case 503: request.count503++; break;
    }
}

//-----------------------------------------------------------------------------
// StatsSession::cacheProfiles
//-----------------------------------------------------------------------------

void StatsSession::cacheProfiles(StatsCacheEntry* entry)
{
    int i;
    for (i = 0; i < countProfileBuckets; i++) {
        if (profiles[i].countCalls) {
            entry->profiles[i].countRequests++;
            sumProfile(&entry->profiles[i], &profiles[i]);
        }
    }
}

//-----------------------------------------------------------------------------
// StatsSession::flush
//-----------------------------------------------------------------------------

void StatsSession::flush()
{
    if (!countRequestsCached) return;

    SimpleHashUnlockedIterator iterator(&vsEntryHash);

    // Set our mode to STATS_THREAD_UPDATING, remembering the previous mode
    PRUint32 mode;
    if (thread) {
        mode = thread->threadStats.mode;
        thread->threadStats.mode = STATS_THREAD_UPDATING;
    }

    StatsManager::lockStatsData();

    StatsAccumulatedVSSlot* accumulatedVSStats =
                StatsManager::getAccumulatedVSSlot();
    StatsRequestBucket* sumVSReqBucket = (accumulatedVSStats == NULL)
                                         ? NULL
                                         : &accumulatedVSStats->requestBucket;
    StatsProfileBucket* sumAllReqsProfileBucket = (accumulatedVSStats == NULL)
                                  ? NULL
                                  : &accumulatedVSStats->allReqsProfileBucket;
    // For every entry in the hash table...
    StatsCacheEntry* entryCachedVs;
    while (entryCachedVs = (StatsCacheEntry*)iterator.next()) {
        StatsVirtualServerNode* vss = (StatsVirtualServerNode*)iterator.getKey();

        // Accumulate stats counter deltas for this VS
        flushProfiles(vss->profile, entryCachedVs);
        sumRequest(&vss->vssStats.requestBucket, &entryCachedVs->request);
        // Record the VS's most recently accessed URI
        strcpy(vss->vssStats.requestBucket.method, entryCachedVs->request.method);
        strcpy(vss->vssStats.requestBucket.uri, entryCachedVs->request.uri);

        countDirtyCacheEntries--;

        // Accumulate stats in accumulated vs slot buckets too.
        if (sumVSReqBucket)
            sumRequest(sumVSReqBucket, &entryCachedVs->request);

        if (sumAllReqsProfileBucket && (countProfileBuckets > 0)) {
            // profiling is enabled
            StatsProfileBucket& profileAll =
                                entryCachedVs->profiles[STATS_PROFILE_ALL];
            if (profileAll.countRequests)
                sumProfile(sumAllReqsProfileBucket, &profileAll);
        }
    }

    if (thread) {
        // Accumulate stats counter deltas for this thread
        flushProfiles(thread->profile, entryThread);
        sumRequest(&thread->threadStats.requestBucket, &entryThread->request);
    }

    // Track keepalives
    StatsManager::recordKeepaliveHits(countKeepaliveHits);

    // Restore our previous mode
    if (thread) thread->threadStats.mode = mode;

    // Nothing's cached
    vsEntryHash.removeAll();

    StatsManager::unlockStatsData();

    countRequestsCached = 0;
    countKeepaliveHits = 0;
    memset(entryThread, 0, sizeCacheEntry);
    ticksFlushed = ticksBeginRequest;
    PR_ASSERT(!countDirtyCacheEntries);
}

//-----------------------------------------------------------------------------
// StatsSession::flushProfiles
//-----------------------------------------------------------------------------

void StatsSession::flushProfiles(StatsProfileNode* profile, const StatsCacheEntry* entry)
{
    // Accumulate from the StatsCacheEntry into the profile chain

    int i;
    for (i = 0; i < countProfileBuckets; i++) {
        if (profile && entry->profiles[i].countRequests) {
            sumProfile(&profile->profileStats, &entry->profiles[i]);
        }
        profile = profile->next;
    }
}

//-----------------------------------------------------------------------------
// StatsSession::sumRequest
//-----------------------------------------------------------------------------

void StatsSession::sumRequest(StatsRequestBucket* sum, const StatsRequestBucket* delta)
{
    sum->countRequests += delta->countRequests;
    sum->countBytesReceived += delta->countBytesReceived;
    sum->countBytesTransmitted += delta->countBytesTransmitted;

    sum->count2xx += delta->count2xx;
    sum->count3xx += delta->count3xx;
    sum->count4xx += delta->count4xx;
    sum->count5xx += delta->count5xx;
    sum->countOther += delta->countOther;

    sum->count200 += delta->count200;
    sum->count302 += delta->count302;
    sum->count304 += delta->count304;
    sum->count400 += delta->count400;
    sum->count401 += delta->count401;
    sum->count403 += delta->count403;
    sum->count404 += delta->count404;
    sum->count503 += delta->count503;
}

//-----------------------------------------------------------------------------
// StatsSession::sumProfile
//-----------------------------------------------------------------------------

void StatsSession::sumProfile(StatsProfileBucket* sum, const StatsProfileBucket* delta)
{
    sum->countCalls += delta->countCalls;
    sum->countRequests += delta->countRequests;
    sum->ticksDispatch += delta->ticksDispatch;
    sum->ticksFunction += delta->ticksFunction;
}

//-----------------------------------------------------------------------------
// StatsSession::allocEntry
//-----------------------------------------------------------------------------

StatsCacheEntry* StatsSession::allocEntry()
{
    // If we've run out of StatsCacheEntrys...
    if (countDirtyCacheEntries == countCacheEntries) {
        // Flush all pending updates, freeing all the StatsCacheEntrys
        flush();
        PR_ASSERT(!countDirtyCacheEntries);
    }

    StatsCacheEntry* entry;
    entry = (StatsCacheEntry*)((char*)entriesVs + countDirtyCacheEntries * sizeCacheEntry);
    countDirtyCacheEntries++;

    memset(entry, 0, sizeCacheEntry);

    return entry;
}

//-----------------------------------------------------------------------------
// StatsSession::getVsEntry
//-----------------------------------------------------------------------------

StatsCacheEntry* StatsSession::getVsEntry(const VirtualServer* vs)
{
    PR_ASSERT(vs != 0);
    PR_ASSERT(entryCurrentVs == 0);

    StatsVirtualServerNode* vss = StatsManager::getVirtualServerSlot(vs);
    StatsCacheEntry* entry = (StatsCacheEntry*)vsEntryHash.lookup((void*)vss);
    if (!entry) {
        // New entry
        entry = allocEntry();
        vsEntryHash.insert((void*)vss, (void*)entry);
    }

    return entry;
}

//-----------------------------------------------------------------------------
// StatsSession::isTimeForFlush
//-----------------------------------------------------------------------------

PRBool StatsSession::isTimeForFlush()
{
    if (!countRequestsCached) return PR_FALSE;
    if (countRequestsCached >= countMaxRequestsCached) return PR_TRUE;
    if ((PRIntervalTime)(ticksBeginRequest - ticksFlushed) >= ticksMaxCached) return PR_TRUE;
    return PR_FALSE;
}
