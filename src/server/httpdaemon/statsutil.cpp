#include "httpdaemon/statsutil.h"    // StatsManagerUtil

#define STATS_SET_MAX(dest, src) \
    if (dest < src) dest = src;

////////////////////////////////////////////////////////////////

// StatsMsgBuff Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsMsgBuff::StatsMsgBuff
//-----------------------------------------------------------------------------

StatsMsgBuff::StatsMsgBuff(int nCapacity)
{
    const int nStackBufSize = sizeof(stackBuffer_);
    useStatic(stackBuffer_, nStackBufSize, 0);
    setGrowthSize(4096);
    memset(stackBuffer_, 0, nStackBufSize);
    ensureCapacity(nCapacity);
}

//-----------------------------------------------------------------------------
// StatsMsgBuff::~StatsMsgBuff
//-----------------------------------------------------------------------------

StatsMsgBuff::~StatsMsgBuff(void)
{
}

//-----------------------------------------------------------------------------
// StatsMsgBuff::setData
//-----------------------------------------------------------------------------

int StatsMsgBuff::setData(const void* buffer,
                          int bufLen)
{
    ensureCapacity(bufLen);
    clear();
    append((const char*) buffer, bufLen);
    return length();
}

//-----------------------------------------------------------------------------
// StatsMsgBuff::appendString
//-----------------------------------------------------------------------------

int StatsMsgBuff::appendString(const char* string)
{
    char nullBuffer[8];
    memset(nullBuffer, 0, sizeof(nullBuffer));
    if (!string)
    {
        string = "";
        //return appendData(string, strlen(string) + 1);
    }
    // else
    int nStrLen = strlen(string);
    appendData(string, nStrLen + 1);    // Append null byte
    int nPadBytes = (STATS_ALIGN(nStrLen + 1)) - (nStrLen + 1);
    if (nPadBytes)
    {
        appendData(nullBuffer, nPadBytes);
    }
    return length();
}


////////////////////////////////////////////////////////////////

// StringArrayBuff Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// StringArrayBuff::StringArrayBuff
//-----------------------------------------------------------------------------

StringArrayBuff::StringArrayBuff(int nCapacity):
                                 StatsMsgBuff(nCapacity)
{
    nStringCount_ = 0;
    stringArray_ = 0;
}

//-----------------------------------------------------------------------------
// StringArrayBuff::~StringArrayBuff
//-----------------------------------------------------------------------------

StringArrayBuff::~StringArrayBuff(void)
{
    PR_FREEIF(stringArray_);
}

//-----------------------------------------------------------------------------
// StringArrayBuff::countStringsInBuffer
//-----------------------------------------------------------------------------

PRBool StringArrayBuff::countStringsInBuffer(void)
{
    nStringCount_ = 0;
    char* buffer = (char*) data();
    int nCurSize = length();
    const char* bufferEnd = buffer + nCurSize;
    // Parse the buffer to
    char* prevBuffer = buffer;
    while (1)
    {
        // Safely use memchr instead of strlen as the search for null is bounded
        char* curBuffer =
            (char*) memchr(prevBuffer, 0, bufferEnd - prevBuffer);
        if (!curBuffer)
        {
            // There is no null character at end
            return PR_FALSE;
        }
        if ((curBuffer - prevBuffer) > 0)
            ++nStringCount_;
        else
        {
            if ((bufferEnd - curBuffer) <= 2)
                break;
            // zero length strings are not allowed
            return PR_FALSE;
        }
        if ((bufferEnd - curBuffer) <= 2)
            break;
        prevBuffer = curBuffer + 1;
    }
    if (nStringCount_ > 0)
        return PR_TRUE;
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StringArrayBuff::setupStringArray
//-----------------------------------------------------------------------------

PRBool StringArrayBuff::setupStringArray(void)
{
    int nStringIndex = 0;
    char* buffer = (char*) data();
    int nCurSize = length();
    const char* bufferEnd = buffer + nCurSize;
    // Parse the buffer to
    char* prevBuffer = buffer;
    while (1)
    {
        // Safely use memchr instead of strlen as the search for null is bounded
        char* curBuffer =
            (char*) memchr(prevBuffer, 0, bufferEnd - prevBuffer);
        if (!curBuffer)
        {
            // There is no null character at end
            return PR_FALSE;
        }
        if ((curBuffer - prevBuffer) > 0)
        {
            stringArray_[nStringIndex++] = prevBuffer;
        }
        else
        {
            if ((bufferEnd - curBuffer) <= 2)
                break;
            // zero length strings are not allowed
            return PR_FALSE;
        }
        if ((bufferEnd - curBuffer) <= 2)
            break;
        prevBuffer = curBuffer + 1;
    }
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// StringArrayBuff::reinitialize
//-----------------------------------------------------------------------------

PRBool StringArrayBuff::reinitialize(const void* buffer,
                                     int bufLen)
{
    nStringCount_ = 0;
    if (bufLen <= 1)
    {
        // There is no strings.  One character comes as a result of appending
        // null string at the end during freeze.
        return PR_TRUE;
    }
    if (bufLen == 2)
    {
        return PR_FALSE;
    }
    // Reset the data in base class buffer
    setData(buffer, bufLen);
    if (countStringsInBuffer() != PR_TRUE)
        return PR_FALSE;
    stringArray_ = (char**) PR_REALLOC(stringArray_,
                                        nStringCount_ * sizeof(char*));
    return setupStringArray();
}

//-----------------------------------------------------------------------------
// StringArrayBuff::addString
//-----------------------------------------------------------------------------

void StringArrayBuff::addString(const char* str)
{
    ++nStringCount_;
    // Append string with null
    appendData(str, strlen(str) + 1);
}

//-----------------------------------------------------------------------------
// StringArrayBuff::freeze
//-----------------------------------------------------------------------------

void StringArrayBuff::freeze(void)
{
    // Add Null string at the end
    addString("");
}




////////////////////////////////////////////////////////////////

// StatsBufferReader Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsBufferReader::StatsBufferReader
//-----------------------------------------------------------------------------

StatsBufferReader::StatsBufferReader(const void* buffer,
                                      int bufLen,
                                      PRBool fCopy)
{
    initialize(buffer, bufLen, fCopy);
}

//-----------------------------------------------------------------------------
// StatsBufferReader::initialize
//-----------------------------------------------------------------------------

void StatsBufferReader::initialize(const void* buffer,
                                   int bufLen,
                                   PRBool fCopy)
{
    nCurIndex_ = 0;
    if (fCopy == PR_TRUE)
    {
        if (bufLen > 0)
        {
            buffer_ = PR_CALLOC(bufLen);
            memcpy(buffer_, buffer, bufLen);
        }
        else
        {
            buffer_ = 0;
        }
    }
    else
    {
        buffer_ = const_cast <void*>(buffer);
    }
    nSize_ = bufLen;
}

//-----------------------------------------------------------------------------
// StatsBufferReader::readString
//-----------------------------------------------------------------------------

const char* StatsBufferReader::readString(void)
{
    int nRemnBytes = nSize_ - nCurIndex_;
    if (nRemnBytes <= 0)
        return 0;
    const char* strBegin = (const char*) buffer_ + nCurIndex_;
    // Search for null character
    const char* strEnd = (const char*) memchr(strBegin, 0, nRemnBytes);
    if (strEnd == 0)
    {
        return 0;
    }
    // Found null character
    int nStrLen = strEnd - strBegin;
    // Update the current index
    nCurIndex_ += STATS_ALIGN(nStrLen + 1);
    return strBegin;
}

//-----------------------------------------------------------------------------
// StatsBufferReader::readStrings
//
// Read countStirngs number of strings from the buffer. If fails return NULL
// or else set the string pointers in stringBuffer.
//-----------------------------------------------------------------------------

PRBool StatsBufferReader::readStrings(int countStrings,
                                      const char** stringBuffer)
{
    int nIndex = 0;
    for (nIndex = 0; nIndex < countStrings; ++nIndex)
    {
        const char* str = readString();
        if (!str)
            return PR_FALSE;
        stringBuffer[nIndex] = str;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsBufferReader::readInt32
//-----------------------------------------------------------------------------

PRBool StatsBufferReader::readInt32(PRInt32& val)
{
    int nRemnBytes = nSize_ - nCurIndex_;
    const int sizeInt = sizeof(PRInt32);
    if (nRemnBytes < sizeInt)
        return PR_FALSE;
    const void* bufCurrent = (const char*) buffer_ + nCurIndex_;
    memcpy(&val, bufCurrent, sizeInt);
    nCurIndex_ += sizeInt;
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// StatsBufferReader::readBuffer
//-----------------------------------------------------------------------------

const void* StatsBufferReader::readBuffer(int nLen)
{
    int nRemnBytes = nSize_ - nCurIndex_;
    if (nRemnBytes < nLen)
        return 0;
    const void* bufBegin = (const char*) buffer_ + nCurIndex_;
    // Update the current index
    nCurIndex_ += nLen;
    return bufBegin;
}

//-----------------------------------------------------------------------------
// StatsBufferReader::isMoreDataAvailable
//-----------------------------------------------------------------------------

PRBool StatsBufferReader::isMoreDataAvailable(void)
{
    return (((nSize_ - nCurIndex_) > 0) ? PR_TRUE : PR_FALSE);
}

//-----------------------------------------------------------------------------
// StatsBufferReader::readStringAndSlot
//-----------------------------------------------------------------------------

PRBool StatsBufferReader::readStringAndSlot(int nSlotSize, const char*& strName,
                                            const void*& slotBuffer)
{
    strName = readString();
    if (strName == NULL)
        return PR_FALSE;
    slotBuffer = readBuffer(nSlotSize);
    if (slotBuffer == NULL)
        return PR_FALSE;
    return PR_TRUE;
}



////////////////////////////////////////////////////////////////

// StatsManagerUtil

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsManagerUtil::copyProfileBucket
//
// Copy the profile slot data from src to dest but ignore the pointers data
//-----------------------------------------------------------------------------

PRBool StatsManagerUtil::copyProfileBucket(StatsProfileNode* dest,
                                           const StatsProfileNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    memcpy(&dest->profileStats,
           &src->profileStats,
           sizeof(dest->profileStats));
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::copyProfileBucketChain
//-----------------------------------------------------------------------------

PRBool StatsManagerUtil::copyProfileBucketChain(StatsProfileNode* dest,
                                                const StatsProfileNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    int nIndex = 0;
    while (dest && src)
    {
        copyProfileBucket(dest, src);
        dest = dest->next;
        src = src->next;
    }
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// StatsManagerUtil::copyVirtualServerSlot
//-----------------------------------------------------------------------------

PRBool
StatsManagerUtil::copyVirtualServerSlot(StatsVirtualServerNode* dest,
                                        const StatsVirtualServerNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    memcpy(&dest->vssStats, &src->vssStats, sizeof(dest->vssStats));
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::copyWebModuleSlot
//
// Copy the webmodule slot data from src to dest but ignore the pointers data
//-----------------------------------------------------------------------------

PRBool StatsManagerUtil::copyWebModuleSlot(StatsWebModuleNode* dest,
                                           const StatsWebModuleNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    memcpy(&dest->wmsStats, &src->wmsStats, sizeof(dest->wmsStats));
    memcpy(&dest->wmCacheStats, &src->wmCacheStats, sizeof(dest->wmCacheStats));
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::copyServletSlot
//
// Copy the servlet slot data from src to dest but ignore the pointers data
//-----------------------------------------------------------------------------

PRBool StatsManagerUtil::copyServletSlot(StatsServletJSPNode* dest,
                                         const StatsServletJSPNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    memcpy(&dest->sjsStats, &src->sjsStats, sizeof(dest->sjsStats));
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::aggregateRequestBucket
//-----------------------------------------------------------------------------

PRBool StatsManagerUtil::aggregateRequestBucket(StatsRequestBucket* dest,
                                                StatsRequestBucket* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    dest->countRequests += src->countRequests;
    dest->countBytesReceived += src->countBytesReceived;
    dest->countBytesTransmitted += src->countBytesTransmitted;

    dest->count2xx += src->count2xx;
    dest->count3xx += src->count3xx;
    dest->count4xx += src->count4xx;
    dest->count5xx += src->count5xx;
    dest->countOther += src->countOther;

    dest->count200 += src->count200;
    dest->count302 += src->count302;
    dest->count304 += src->count304;
    dest->count400 += src->count400;
    dest->count401 += src->count401;
    dest->count403 += src->count403;
    dest->count404 += src->count404;
    dest->count503 += src->count503;

    // Just assign the rateBytesTransmitted and countOpenConnections
    dest->rateBytesTransmitted = src->rateBytesTransmitted;
    dest->countOpenConnections = src->countOpenConnections;
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// StatsManagerUtil::aggregateVirtualServerSlot
//-----------------------------------------------------------------------------

PRBool
StatsManagerUtil::aggregateVirtualServerSlot(StatsVirtualServerNode* dest,
                                             StatsVirtualServerNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    aggregateRequestBucket(&dest->vssStats.requestBucket,
                           &src->vssStats.requestBucket);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::aggregateProfileBucket
//-----------------------------------------------------------------------------

PRBool StatsManagerUtil::aggregateProfileBucket(StatsProfileNode* dest,
                                                const StatsProfileNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    dest->profileStats.countCalls += src->profileStats.countCalls;
    dest->profileStats.countRequests += src->profileStats.countRequests;
    dest->profileStats.ticksDispatch += src->profileStats.ticksDispatch;
    dest->profileStats.ticksFunction += src->profileStats.ticksFunction;
    return PR_TRUE;
}



//-----------------------------------------------------------------------------
// StatsManagerUtil::aggregateProfileBucketChain
//-----------------------------------------------------------------------------

PRBool
StatsManagerUtil::aggregateProfileBucketChain(StatsProfileNode* dest,
                                              const StatsProfileNode* src)
{
    int nIndex = 0;
    while (dest && src)
    {
        aggregateProfileBucket(dest, src);
        dest = dest->next;
        src = src->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::aggregateWebModuleSlot
//-----------------------------------------------------------------------------

PRBool StatsManagerUtil::aggregateWebModuleSlot(StatsWebModuleNode* dest,
                                                const StatsWebModuleNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    StatsWebModuleSlot* destSlot = &dest->wmsStats;
    const StatsWebModuleSlot* srcSlot = &src->wmsStats;
    STATS_SET_MAX(destSlot->countJSP, srcSlot->countJSP);
    STATS_SET_MAX(destSlot->countJSPReload, srcSlot->countJSPReload);

    // As sessions are shared among child processes their count should be
    // the same
    destSlot->countSessions = srcSlot->countSessions;
    destSlot->countActiveSessions = srcSlot->countActiveSessions;
    destSlot->peakActiveSessions = srcSlot->peakActiveSessions;
    destSlot->countRejectedSessions = srcSlot->countRejectedSessions;
    destSlot->countExpiredSessions = srcSlot->countExpiredSessions;
    destSlot->sessionMaxAliveTime = srcSlot->sessionMaxAliveTime;
    destSlot->sessionAvgAliveTime = srcSlot->sessionAvgAliveTime;

    // Aggregation of cache stats for web apps.
    StatsWebModuleCacheSlot* destCacheSlot = &dest->wmCacheStats;
    const StatsWebModuleCacheSlot* srcCacheSlot = &src->wmCacheStats;
    destCacheSlot->maxEntries    = srcCacheSlot->maxEntries;
    destCacheSlot->threshold     = srcCacheSlot->threshold;
    destCacheSlot->tableSize     = srcCacheSlot->tableSize;
    destCacheSlot->entryCount    += srcCacheSlot->entryCount;
    destCacheSlot->hitCount      += srcCacheSlot->hitCount;
    destCacheSlot->missCount     += srcCacheSlot->missCount;
    destCacheSlot->removalCount  += srcCacheSlot->removalCount;
    destCacheSlot->refreshCount  += srcCacheSlot->refreshCount;
    destCacheSlot->overflowCount += srcCacheSlot->overflowCount;
    destCacheSlot->addCount      += srcCacheSlot->addCount;
    destCacheSlot->lruListLength = srcCacheSlot->lruListLength;
    destCacheSlot->trimCount     += srcCacheSlot->trimCount;
    destCacheSlot->segmentSize   = srcCacheSlot->segmentSize;
    destCacheSlot->currentSize   += srcCacheSlot->currentSize;
    destCacheSlot->maxSize       = srcCacheSlot->maxSize;
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::aggregateServletSlot
//-----------------------------------------------------------------------------

PRBool StatsManagerUtil::aggregateServletSlot(StatsServletJSPNode* dest,
                                              const StatsServletJSPNode* src)
{
    if ((!dest) || (!src))
        return PR_FALSE;
    StatsServletJSPSlot* destSlot = &dest->sjsStats;
    const StatsServletJSPSlot* srcSlot = &src->sjsStats;
    destSlot->countRequest += srcSlot->countRequest;
    destSlot->millisecProcessing += srcSlot->millisecProcessing;
    STATS_SET_MAX(destSlot->millisecPeakProcessing,
                  srcSlot->millisecPeakProcessing);
    destSlot->countError += srcSlot->countError;
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// StatsManagerUtil::accumulateKeepAlive
//
// Replacement function for macro STATS_ACCUMULATE_KEEPALIVE
//-----------------------------------------------------------------------------

void StatsManagerUtil::accumulateKeepAlive(StatsKeepaliveBucket* sum,
                                           const StatsKeepaliveBucket* delta)
{
    if (!(delta && sum))
        return;
    sum->countConnections += delta->countConnections;
    sum->maxConnections   += delta->maxConnections;
    sum->countHits        += delta->countHits;
    sum->countFlushes     += delta->countFlushes;
    sum->countTimeouts    += delta->countTimeouts;
    sum->secondsTimeout    = delta->secondsTimeout;
    sum->countRefusals    += delta->countRefusals;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::accumulateCache
//
// Replacement function for macro STATS_ACCUMULATE_CACHE
//-----------------------------------------------------------------------------

void StatsManagerUtil::accumulateCache(StatsCacheBucket* sum,
                                       const StatsCacheBucket* delta)
{
    if (!(delta && sum))
        return;
    sum->flagEnabled                   |= delta->flagEnabled;
    sum->secondsMaxAge                  = delta->secondsMaxAge;
    sum->countEntries                  += delta->countEntries;
    sum->maxEntries                    += delta->maxEntries;
    sum->countOpenEntries              += delta->countOpenEntries;
    sum->maxOpenEntries                += delta->maxOpenEntries;
    sum->sizeHeapCache                 += delta->sizeHeapCache;
    sum->maxHeapCacheSize              += delta->maxHeapCacheSize;
    sum->sizeMmapCache                 += delta->sizeMmapCache;
    sum->maxMmapCacheSize              += delta->maxMmapCacheSize;
    sum->countHits                     += delta->countHits;
    sum->countMisses                   += delta->countMisses;
    sum->countInfoHits                 += delta->countInfoHits;
    sum->countInfoMisses               += delta->countInfoMisses;
    sum->countContentHits              += delta->countContentHits;
    sum->countContentMisses            += delta->countContentMisses;
    sum->countAcceleratorEntries       += delta->countAcceleratorEntries;
    sum->countAcceleratableRequests    += delta->countAcceleratableRequests;
    sum->countUnacceleratableRequests  += delta->countUnacceleratableRequests;
    sum->countAcceleratableResponses   += delta->countAcceleratableResponses;
    sum->countUnacceleratableResponses += delta->countUnacceleratableResponses;
    sum->countAcceleratorHits          += delta->countAcceleratorHits;
    sum->countAcceleratorMisses        += delta->countAcceleratorMisses;
}


//-----------------------------------------------------------------------------
// StatsManagerUtil::accumulateDNS
//
// Replacement function for macro STATS_ACCUMULATE_DNS
//-----------------------------------------------------------------------------

void StatsManagerUtil::accumulateDNS(StatsDnsBucket* sum,
                                     const StatsDnsBucket* delta)
{
    if (!(delta && sum))
        return;
    sum->flagCacheEnabled            |= delta->flagCacheEnabled;
    sum->flagAsyncEnabled            |= delta->flagAsyncEnabled;
    sum->countCacheEntries           += delta->countCacheEntries;
    sum->maxCacheEntries             += delta->maxCacheEntries;
    sum->countCacheHits              += delta->countCacheHits;
    sum->countCacheMisses            += delta->countCacheMisses;
    sum->countAsyncNameLookups       += delta->countAsyncNameLookups;
    sum->countAsyncAddrLookups       += delta->countAsyncAddrLookups;
    sum->countAsyncLookupsInProgress += delta->countAsyncLookupsInProgress;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::accumulateRequest
//
// Replacement function for macro STATS_ACCUMULATE_REQUEST
//-----------------------------------------------------------------------------

void StatsManagerUtil::accumulateRequest(StatsRequestBucket* sum,
                                         const StatsRequestBucket* delta)
{
    if (!(delta && sum))
        return;
    sum->countRequests         += delta->countRequests;;
    sum->countBytesReceived    += delta->countBytesReceived;;
    sum->countBytesTransmitted += delta->countBytesTransmitted;;
    sum->rateBytesTransmitted  += delta->rateBytesTransmitted;;
    sum->countOpenConnections  += delta->countOpenConnections;;
    sum->countOther            += delta->countOther;;
    sum->count2xx              += delta->count2xx;;
    sum->count3xx              += delta->count3xx;;
    sum->count4xx              += delta->count4xx;;
    sum->count5xx              += delta->count5xx;;
    sum->count200              += delta->count200;;
    sum->count302              += delta->count302;;
    sum->count304              += delta->count304;;
    sum->count400              += delta->count400;;
    sum->count401              += delta->count401;;
    sum->count403              += delta->count403;;
    sum->count404              += delta->count404;;
    sum->count503              += delta->count503;;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::accumulateProfile
//
// Replacement function for macro STATS_ACCUMULATE_PROFILE
//-----------------------------------------------------------------------------

void StatsManagerUtil::accumulateProfile(StatsProfileBucket* sum,
                                         const StatsProfileBucket* delta)
{
    if (!(delta && sum))
        return;
    sum->offsetName        = delta->offsetName;
    sum->offsetDescription = delta->offsetDescription;
    sum->countCalls       += delta->countCalls;;
    sum->countRequests    += delta->countRequests;;
    sum->ticksDispatch    += delta->ticksDispatch;;
    sum->ticksFunction    += delta->ticksFunction;;

}

//-----------------------------------------------------------------------------
// StatsManagerUtil::accumulateThreadPool
//
// Replacement function for macro STATS_ACCUMULATE_THREADPOOL
//-----------------------------------------------------------------------------

void StatsManagerUtil::accumulateThreadPool(StatsThreadPoolBucket* sum,
                                            const StatsThreadPoolBucket* delta)
{
    if (!(delta && sum))
        return;
    sum->offsetName        = delta->offsetName;
    sum->countThreadsIdle += delta->countThreadsIdle;
    sum->countThreads     += delta->countThreads;
    sum->maxThreads       += delta->maxThreads;
    sum->countQueued      += delta->countQueued;
    sum->peakQueued       += delta->peakQueued;
    sum->maxQueued        += delta->maxQueued;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::accumulateStatsAvgBucket
//
// Average can't be calculated without knowing the count of items. This method
// just sum them. It is on caller's responsibility to divide the sum by item
// counts.
//-----------------------------------------------------------------------------

void StatsManagerUtil::accumulateStatsAvgBucket(StatsAvgBucket* sum,
                                                const StatsAvgBucket* delta)
{
    sum->oneMinuteAverage += delta->oneMinuteAverage;
    sum->fiveMinuteAverage += delta->fiveMinuteAverage;
    sum->fifteenMinuteAverage += delta->fifteenMinuteAverage;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::accumulateAccumulatedVSSlot
//-----------------------------------------------------------------------------

void StatsManagerUtil::accumulateAccumulatedVSSlot(
                                        StatsAccumulatedVSSlot* sum,
                                        const StatsAccumulatedVSSlot* delta)
{
    if (!(delta && sum))
        return;
    accumulateRequest(&sum->requestBucket, &delta->requestBucket);
    accumulateProfile(&sum->allReqsProfileBucket, &delta->allReqsProfileBucket);
    accumulateStatsAvgBucket(&sum->requestAvgBucket, &delta->requestAvgBucket);
    accumulateStatsAvgBucket(&sum->errorAvgBucket, &delta->errorAvgBucket);
    accumulateStatsAvgBucket(&sum->responseTimeAvgBucket,
                             &delta->responseTimeAvgBucket);
    STATS_SET_MAX(sum->maxResponseTime, delta->maxResponseTime);
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getSeconds
//-----------------------------------------------------------------------------

PRInt64 StatsManagerUtil::getSeconds(PRInt64 time)
{
    PRInt64 temp = 0;
    PRInt64 dividend = 0;
    PRInt64 divisor = 0;

    LL_UI2L(temp, PR_USEC_PER_SEC/2);
    LL_ADD(dividend, time, temp);
    LL_UI2L(divisor, PR_USEC_PER_SEC);
    LL_DIV(temp, dividend, divisor);

    return temp;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getResponseTime
//-----------------------------------------------------------------------------

PRFloat64 StatsManagerUtil::getResponseTime(StatsProfileBucket& profile)
{
    return ((PRFloat64) (PRInt64) (profile.ticksDispatch +
                                   profile.ticksFunction)) /
                        ((PRInt64) profile.countRequests * PR_TicksPerSecond());
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::allocListenSlot
//-----------------------------------------------------------------------------

StatsListenNode*
StatsManagerUtil::allocListenSlot(const char* id,
                                  StatsProcessNode* proc)
{
    //Search the list if not found then create a listenSlot
    StatsListenNode* ls = 0;
    ls = statsFindNode<StatsListenNode, StatsProcessNode>(proc, id);
    if (!ls)
    {
        ls = new StatsListenNode;
        strcpy(ls->lsStats.id, id);
        statsAppendLast(proc, ls);
    }
    return ls;
}


//-----------------------------------------------------------------------------
// StatsManagerUtil::findWebModuleSlotInVSList
//
// Find the webModule slot in virtual server list pointed by vss
// Return null if not found.
//-----------------------------------------------------------------------------

StatsWebModuleNode*
StatsManagerUtil::findWebModuleSlotInVSList(StatsVirtualServerNode* vss,
                                            const char* webModuleName)
{
    while (vss)
    {
        if (isWebModuleBelongs(vss, webModuleName) == PR_TRUE)
        {
            return statsFindNode<StatsWebModuleNode> (vss, webModuleName);
        }
        vss = vss->next;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getWebModuleSlot
//
// It search for the webModule with in vss if found then returns or else
// It creates a new webmodule slot and appends in vss at the end and return
// the pointer to the newly allocated slot.
//-----------------------------------------------------------------------------

StatsWebModuleNode*
StatsManagerUtil::getWebModuleSlot(StatsVirtualServerNode* vss,
                                   const char* webModuleName)
{
    StatsWebModuleNode* wms = 0;
    wms = statsFindNode<StatsWebModuleNode> (vss, webModuleName);
    if (wms)
        return wms;
    // Allocate a new webmodule slot
    StatsWebModuleNode* webmodNew = new StatsWebModuleNode(webModuleName);
    webmodNew->wmsStats.mode = STATS_WEBMODULE_MODE_ENABLED;

    statsAppendLast(vss, webmodNew);
    return webmodNew;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getWebModuleSlot
//-----------------------------------------------------------------------------

StatsWebModuleNode*
StatsManagerUtil::getWebModuleSlot(const StatsHeaderNode* hdrInput,
                                   const char* vsId,
                                   const char* webModuleName)
{
    StatsVirtualServerNode* vss = 0;
    vss = statsFindNode<StatsVirtualServerNode>(hdrInput, vsId);
    if (!vss)
        return 0;
    return getWebModuleSlot(vss, webModuleName);
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::isWebModuleBelongs
//
// Return PR_TRUE if webModuleName is deployed on virtual server vss
//-----------------------------------------------------------------------------

PRBool
StatsManagerUtil::isWebModuleBelongs(StatsVirtualServerNode* vss,
                                     const char* webModuleName)
{
    // webmodule name is "//<vsname>/<true_name>" e.g. if /simple webmodule
    // is deployed on vs named test then webapp name
    // will be "//test/simple"
    // See the class
    // pwc-commons/src/java/com/sun/enterprise/web/monitor/
    // impl/PwcWebModuleStatsImpl.java
    if (webModuleName && vss && vss->vssStats.id[0])
    {
        const char chSlash = '/';
        int vsIdLen = strlen(vss->vssStats.id);
        int webModuleNameLen = strlen(webModuleName);
        if (webModuleNameLen < (vsIdLen + 3))
        {
            return PR_FALSE;
        }
        if ((webModuleName[0] != chSlash) ||
            (webModuleName[1] != chSlash) ||
            (webModuleName[vsIdLen + 2] != chSlash))
        {
            return PR_FALSE;
        }
        if (strncmp(vss->vssStats.id, (webModuleName + 2), vsIdLen) == 0)
        {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getWebModuleUriFromName
//-----------------------------------------------------------------------------

const char* StatsManagerUtil::getWebModuleUriFromName(const char* webModuleName)
{
    if (!webModuleName)
        return NULL;
    // webmodule name is "//<vsname>/<true_name>" e.g. if /simple webmodule
    // is deployed on vs named test then webapp name
    // will be "//test/simple"
    int webModuleNameLen = strlen(webModuleName);
    if (webModuleNameLen < 3)
    {
        return NULL;
    }
    const char* vsNameBegin = webModuleName + 2;
    const char chSlash = '/';
    return strchr(vsNameBegin, chSlash);
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getVSSFromWebModuleName
//
// Search for all vss for which the webModuleName belongs.
// Note that it only searches the name. It does not expect to have
// webModule slot exists with webModuleName
//-----------------------------------------------------------------------------

StatsVirtualServerNode*
StatsManagerUtil::getVSSFromWebModuleName(StatsVirtualServerNode* vss,
                                          const char* webModuleName)
{
    while (vss)
    {
        if (isWebModuleBelongs(vss, webModuleName) == PR_TRUE)
        {
            return vss;
        }
        vss = vss->next;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getServletSlot
//
// This funciton searches for the servlet with in wms. If found then returns
// the pointer or else it creates a new servlet/JSP node and appends in wms at
// the end and return the pointer to the newly allocated node.
//-----------------------------------------------------------------------------

StatsServletJSPNode*
StatsManagerUtil::getServletSlot(StatsWebModuleNode* wms,
                                 const char* servletName)
{
    StatsServletJSPNode* sjs = 0;
    sjs = statsFindNode<StatsServletJSPNode> (wms, servletName);
    if (sjs)
        return sjs;
    // Allocate a new servlet slot
    StatsServletJSPNode* sjsNew = new StatsServletJSPNode(servletName);

    // If there is no servlet/JSP yet in webapp then set as
    // first servlet/JSP
    statsAppendLast(wms, sjsNew);
    return sjsNew;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getJdbcConnPoolNode
//-----------------------------------------------------------------------------

StatsJdbcConnPoolNode*
StatsManagerUtil::getJdbcConnPoolNode(StatsProcessNode* proc,
                                      const char* poolName,
                                      PRBool& fNewNode)
{
    StatsJdbcConnPoolNode* poolNode = 0;
    fNewNode = PR_FALSE;
    poolNode = statsFindNode<StatsJdbcConnPoolNode, StatsProcessNode>(proc,
                                                                      poolName);
    if (poolNode)
        return poolNode;

    // Append the connection pool at the end. If it is the first node then set
    // as the first node in process node.
    StatsJdbcConnPoolNode* poolNodeNew = new StatsJdbcConnPoolNode();
    poolNodeNew->poolName = poolName;
    statsAppendLast(proc, poolNodeNew);
    fNewNode = PR_TRUE;
    return poolNodeNew;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getSessReplInstanceNode
//
// This function searches for the instance node in sessReplNode with id
// instanceId, if it founds, it returned the found pointer. If it can not find
// it then it creates a new node and append at the last node in sessReplNode's
// instanceNode list.
//-----------------------------------------------------------------------------

StatsSessReplInstanceNode*
StatsManagerUtil::getSessReplInstanceNode(
                                    StatsSessionReplicationNode* sessReplNode,
                                    const char* instanceId,
                                    PRBool& fNewNode)
{
    StatsSessReplInstanceNode* instanceNode = NULL;
    fNewNode = PR_FALSE;
    instanceNode = statsFindNode<StatsSessReplInstanceNode>(sessReplNode,
                                                            instanceId);
    if (instanceNode)
        return instanceNode;
    // Allocate a new instance Node
    instanceNode = new StatsSessReplInstanceNode(instanceId);
    statsAppendLast(sessReplNode, instanceNode);
    sessReplNode->sessReplStats.countInstances =
                        statsCountNodes(sessReplNode->instanceNode);
    fNewNode = PR_TRUE;
    return instanceNode;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::getWebAppSessStoreNode
//
// This function searches for webapp store whose id matches storeId if it is
// not found then a new node is created and it is appended at the last of web
// app session store node list of instanceNode.
//-----------------------------------------------------------------------------

StatsWebAppSessionStoreNode*
StatsManagerUtil::getWebAppSessStoreNode(
                                    StatsSessReplInstanceNode* instanceNode,
                                    const char* storeId,
                                    PRBool& fNewNode)
{
    StatsWebAppSessionStoreNode* wassNode = NULL;
    fNewNode = PR_FALSE;
    wassNode = statsFindNode<StatsWebAppSessionStoreNode>(instanceNode,
                                                          storeId);
    if (wassNode)
        return wassNode;
    wassNode = new StatsWebAppSessionStoreNode(storeId);
    statsAppendLast(instanceNode, wassNode);
    instanceNode->countWebappStores = statsCountNodes(instanceNode->wassNode);
    fNewNode = PR_TRUE;
    return wassNode;
}

//-----------------------------------------------------------------------------
// StatsManagerUtil::resetVSSForEmptyNodes
//-----------------------------------------------------------------------------

void
StatsManagerUtil::resetVSSForEmptyNodes(StatsVirtualServerNode* vss)
{
    while (vss)
    {
        if (vss->vssStats.mode == STATS_VIRTUALSERVER_EMPTY)
        {
            vss->resetStats();
        }
        vss = vss->next;
    }
}

