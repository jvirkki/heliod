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

#include "base/util.h"
#include "base/ereport.h"
#include "base/params.h"
#include "frame/conf.h"
#include "frame/protocol.h"
#include "frame/httpfilter.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/ListenSocketConfig.h"
#include "httpdaemon/statsmanager.h"
#include "httpdaemon/statssession.h"
#include "support/stringvalue.h"
#include "support/xmloutput.h"
#include "safs/perf.h"

#define TIME_TO_SECONDS(a) (((a) + PR_USEC_PER_SEC/2) / PR_USEC_PER_SEC) 

//-----------------------------------------------------------------------------
// sun-web-server-stats_7_0_1.dtd
//-----------------------------------------------------------------------------

#define IWSSTATS_DTD "sun-web-server-stats_7_0_1.dtd"
static char dtd[] = 
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"\n"
"<!ELEMENT stats (server*)>\n"
"<!ATTLIST stats\n"
"          enabled (0|1) #REQUIRED\n"
"          versionMajor CDATA #FIXED \"1\"\n"
"          versionMinor CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT server (connection-queue*,thread-pool*,profile*,process*,virtual-server*,session-replication?,cpu-info*)>\n"
"<!ATTLIST server\n"
"          id ID #REQUIRED\n"
"          versionServer CDATA #REQUIRED\n"
"          timeStarted CDATA #REQUIRED\n"
"          secondsRunning CDATA #REQUIRED\n"
"          ticksPerSecond CDATA #REQUIRED\n"
"          maxProcs CDATA #REQUIRED\n"
"          maxThreads CDATA #REQUIRED\n"
"          flagProfilingEnabled (0|1) #REQUIRED\n"
"          load1MinuteAverage CDATA #IMPLIED\n"
"          load5MinuteAverage CDATA #IMPLIED\n"
"          load15MinuteAverage CDATA #IMPLIED\n"
"          rateBytesTransmitted CDATA #IMPLIED\n"
"          rateBytesReceived CDATA #IMPLIED\n"
">\n"
"\n"
"<!ELEMENT connection-queue EMPTY>\n"
"<!ATTLIST connection-queue\n"
"          id ID #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT thread-pool EMPTY>\n"
"<!ATTLIST thread-pool\n"
"          id ID #REQUIRED\n"
"          name CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT profile EMPTY>\n"
"<!ATTLIST profile\n"
"          id ID #REQUIRED\n"
"          name CDATA #REQUIRED\n"
"          description CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT process (connection-queue-bucket*,thread-pool-bucket*,dns-bucket?,keepalive-bucket?,cache-bucket?,thread*,jdbc-resource-bucket*,jvm?)>\n"
"<!ATTLIST process\n"
"          pid CDATA #REQUIRED\n"
"          mode (unknown|active) #REQUIRED\n"
"          timeStarted CDATA #REQUIRED\n"
"          countConfigurations CDATA #REQUIRED\n"
"          sizeVirtual CDATA #IMPLIED\n"
"          sizeResident CDATA #IMPLIED\n"
"          fractionSystemMemoryUsage CDATA #IMPLIED\n"
">\n"
"\n"
"<!ELEMENT connection-queue-bucket EMPTY>\n"
"<!ATTLIST connection-queue-bucket\n"
"          connection-queue IDREF #REQUIRED\n"
"          countTotalConnections CDATA #REQUIRED\n"
"          countQueued CDATA #REQUIRED\n"
"          peakQueued CDATA #REQUIRED\n"
"          maxQueued CDATA #REQUIRED\n"
"          countOverflows CDATA #REQUIRED\n"
"          countTotalQueued CDATA #REQUIRED\n"
"          ticksTotalQueued CDATA #REQUIRED\n"
"          countQueued1MinuteAverage CDATA #IMPLIED\n"
"          countQueued5MinuteAverage CDATA #IMPLIED\n"
"          countQueued15MinuteAverage CDATA #IMPLIED\n"
">\n"
"\n"
"<!ELEMENT thread-pool-bucket EMPTY>\n"
"<!ATTLIST thread-pool-bucket\n"
"          thread-pool IDREF #REQUIRED\n"
"          countThreadsIdle CDATA #REQUIRED\n"
"          countThreads CDATA #REQUIRED\n"
"          maxThreads CDATA #REQUIRED\n"
"          countQueued CDATA #REQUIRED\n"
"          peakQueued CDATA #REQUIRED\n"
"          maxQueued CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT dns-bucket EMPTY>\n"
"<!ATTLIST dns-bucket\n"
"          flagCacheEnabled (0|1) #REQUIRED\n"
"          countCacheEntries CDATA #REQUIRED\n"
"          maxCacheEntries CDATA #REQUIRED\n"
"          countCacheHits CDATA #REQUIRED\n"
"          countCacheMisses CDATA #REQUIRED\n"
"          flagAsyncEnabled (0|1) #REQUIRED\n"
"          countAsyncNameLookups CDATA #REQUIRED\n"
"          countAsyncAddrLookups CDATA #REQUIRED\n"
"          countAsyncLookupsInProgress CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT keepalive-bucket EMPTY>\n"
"<!ATTLIST keepalive-bucket\n"
"          countConnections CDATA #REQUIRED\n"
"          maxConnections CDATA #REQUIRED\n"
"          countHits CDATA #REQUIRED\n"
"          countFlushes CDATA #REQUIRED\n"
"          countRefusals CDATA #REQUIRED\n"
"          countTimeouts CDATA #REQUIRED\n"
"          secondsTimeout CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT cache-bucket EMPTY>\n"
"<!ATTLIST cache-bucket\n"
"          flagEnabled (0|1) #REQUIRED\n"
"          secondsMaxAge CDATA #REQUIRED\n"
"          countEntries CDATA #REQUIRED\n"
"          maxEntries CDATA #REQUIRED\n"
"          countOpenEntries CDATA #REQUIRED\n"
"          maxOpenEntries CDATA #REQUIRED\n"
"          sizeHeapCache CDATA #REQUIRED\n"
"          maxHeapCacheSize CDATA #REQUIRED\n"
"          sizeMmapCache CDATA #REQUIRED\n"
"          maxMmapCacheSize CDATA #REQUIRED\n"
"          countHits CDATA #REQUIRED\n"
"          countMisses CDATA #REQUIRED\n"
"          countInfoHits CDATA #REQUIRED\n"
"          countInfoMisses CDATA #REQUIRED\n"
"          countContentHits CDATA #REQUIRED\n"
"          countContentMisses CDATA #REQUIRED\n"
"          countAcceleratorEntries CDATA #REQUIRED\n"
"          countAcceleratableRequests CDATA #REQUIRED\n"
"          countUnacceleratableRequests CDATA #REQUIRED\n"
"          countAcceleratableResponses CDATA #REQUIRED\n"
"          countUnacceleratableResponses CDATA #REQUIRED\n"
"          countAcceleratorHits CDATA #REQUIRED\n"
"          countAcceleratorMisses CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT thread (request-bucket?,profile-bucket*)>\n"
"<!ATTLIST thread\n"
"          mode (unknown|idle|DNS|request|processing|response|updating|keep-alive) #REQUIRED\n"
"          timeStarted CDATA #REQUIRED\n"
"          function CDATA #IMPLIED\n"
"          connection-queue IDREF #REQUIRED\n"
"          virtual-server IDREF #IMPLIED\n"
"          addressClient CDATA #IMPLIED\n"
"          timeRequestStarted CDATA #IMPLIED\n"
">\n"
"\n"
"<!ELEMENT jdbc-resource-bucket EMPTY>\n"
"<!ATTLIST jdbc-resource-bucket\n"
"          jndiName CDATA #REQUIRED\n"
"          maxConnections CDATA #REQUIRED\n"
"          countConnections CDATA #REQUIRED\n"
"          peakConnections CDATA #REQUIRED\n"
"          countTotalLeasedConnections CDATA #REQUIRED\n"
"          countFreeConnections CDATA #REQUIRED\n"
"          countLeasedConnections CDATA #REQUIRED\n"
"          countTotalFailedValidationConnections CDATA #REQUIRED\n"
"          countQueued CDATA #REQUIRED\n"
"          peakQueued CDATA #REQUIRED\n"
"          millisecondsPeakWait CDATA #REQUIRED\n"
"          millisecondsAverageQueued CDATA #REQUIRED\n"
"          countConnectionIdleTimeouts CDATA #REQUIRED\n"
"          countWaitQueueTimeouts CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT jvm EMPTY>\n"
"<!ATTLIST jvm\n"
"          countClassesLoaded CDATA #REQUIRED\n"
"          countTotalClassesLoaded CDATA #REQUIRED\n"
"          countTotalClassesUnloaded CDATA #REQUIRED\n"
"          sizeHeap CDATA #REQUIRED\n"
"          peakThreads CDATA #REQUIRED\n"
"          countTotalThreadsStarted CDATA #REQUIRED\n"
"          countThreads CDATA #REQUIRED\n"
"          version CDATA #REQUIRED\n"
"          name CDATA #REQUIRED\n"
"          vendor CDATA #REQUIRED\n"
"          countGarbageCollections CDATA #REQUIRED\n"
"          millisecondsGarbageCollection CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT virtual-server (request-bucket?,profile-bucket*,web-app-bucket*)>\n"
"<!ATTLIST virtual-server\n"
"          id ID #REQUIRED\n"
"          mode (unknown|active|disabled) #REQUIRED\n"
"          hosts CDATA #IMPLIED\n"
"          interfaces CDATA #IMPLIED\n"
">\n"
"\n"
"<!ELEMENT request-bucket EMPTY>\n"
"<!ATTLIST request-bucket\n"
"          method CDATA #IMPLIED\n"
"          uri CDATA #IMPLIED\n"
"          countRequests CDATA #REQUIRED\n"
"          countBytesReceived CDATA #REQUIRED\n"
"          countBytesTransmitted CDATA #REQUIRED\n"
"          rateBytesTransmitted CDATA #REQUIRED\n"
"          maxByteTransmissionRate CDATA #REQUIRED\n"
"          countOpenConnections CDATA #REQUIRED\n"
"          maxOpenConnections CDATA #REQUIRED\n"
"          count2xx CDATA #REQUIRED\n"
"          count3xx CDATA #REQUIRED\n"
"          count4xx CDATA #REQUIRED\n"
"          count5xx CDATA #REQUIRED\n"
"          countOther CDATA #REQUIRED\n"
"          count200 CDATA #REQUIRED\n"
"          count302 CDATA #REQUIRED\n"
"          count304 CDATA #REQUIRED\n"
"          count400 CDATA #REQUIRED\n"
"          count401 CDATA #REQUIRED\n"
"          count403 CDATA #REQUIRED\n"
"          count404 CDATA #REQUIRED\n"
"          count503 CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT profile-bucket EMPTY>\n"
"<!ATTLIST profile-bucket\n"
"          profile IDREF #REQUIRED\n"
"          countCalls CDATA #REQUIRED\n"
"          countRequests CDATA #REQUIRED\n"
"          ticksDispatch CDATA #REQUIRED\n"
"          ticksFunction CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT web-app-bucket (servlet-response-cache-bucket?,servlet-bucket*)>\n"
"<!ATTLIST web-app-bucket\n"
"          uri CDATA #REQUIRED\n"
"          mode (enabled|disabled|unknown) #REQUIRED\n"
"          countJsps CDATA #REQUIRED\n"
"          countReloadedJsps CDATA #REQUIRED\n"
"          countSessions CDATA #REQUIRED\n"
"          countActiveSessions CDATA #REQUIRED\n"
"          peakActiveSessions CDATA #REQUIRED\n"
"          countRejectedSessions CDATA #REQUIRED\n"
"          countExpiredSessions CDATA #REQUIRED\n"
"          secondsSessionAliveMax CDATA #REQUIRED\n"
"          secondsSessionAliveAverage CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT servlet-bucket EMPTY>\n"
"<!ATTLIST servlet-bucket\n"
"          name CDATA #REQUIRED\n"
"          countRequests CDATA #REQUIRED\n"
"          millisecondsProcessing CDATA #REQUIRED\n"
"          millisecondsPeakProcessing CDATA #REQUIRED\n"
"          countErrors CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT servlet-response-cache-bucket EMPTY>\n"
"<!ATTLIST servlet-response-cache-bucket\n"
"          maxEntries CDATA #REQUIRED\n"
"          threshold CDATA #REQUIRED\n"
"          tableSize CDATA #IMPLIED\n"
"          countEntries CDATA #REQUIRED\n"
"          countHits CDATA #REQUIRED\n"
"          countMisses CDATA #REQUIRED\n"
"          countEntriesRefreshed CDATA #REQUIRED\n"
"          countEntriesOverflowed CDATA #REQUIRED\n"
"          countEntriesAdded CDATA #REQUIRED\n"
"          countEntriesRemoved CDATA #IMPLIED\n"
"          sizeCurrent CDATA #IMPLIED\n"
"          sizeMax CDATA #IMPLIED\n"
">\n"
"\n"
"<!ELEMENT session-replication (instance-session-store*)>\n"
"<!ATTLIST session-replication\n"
"          countSelfRecoveryAttempts CDATA #REQUIRED\n"
"          countSelfRecoveryFailures CDATA #REQUIRED\n"
"          countFailoverAttempts CDATA #REQUIRED\n"
"          countFailoverFailures CDATA #REQUIRED\n"
"          listClusterMembers CDATA #REQUIRED\n"
"          currentBackupInstanceId CDATA #REQUIRED\n"
"          state (normal|nobackup|abnormal) #REQUIRED\n"
"          countBackupConnectionFailures CDATA #REQUIRED\n"
"          countBackupConnectionFailoverSuccesses CDATA #REQUIRED\n"
"          countSentPuts CDATA #REQUIRED\n"
"          countSentGets CDATA #REQUIRED\n"
"          countSentRemoves CDATA #REQUIRED\n"
"          countReceivedPuts CDATA #REQUIRED\n"
"          countReceivedGets CDATA #REQUIRED\n"
"          countReceivedRemoves CDATA #REQUIRED\n"
"          countAsyncQueueEntries CDATA #IMPLIED\n"
"          peakAsyncQueueEntries CDATA #IMPLIED\n"
"          countLockFailures CDATA #IMPLIED\n"
">\n"
"\n"
"<!ELEMENT instance-session-store (web-app-session-store*)>\n"
"<!ATTLIST instance-session-store\n"
"          instanceId CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT web-app-session-store EMPTY>\n"
"<!ATTLIST web-app-session-store\n"
"          vsId CDATA #REQUIRED\n"
"          uri CDATA #REQUIRED\n"
"          countSessions CDATA #REQUIRED\n"
">\n"
"\n"
"<!ELEMENT cpu-info EMPTY>\n"
"<!ATTLIST cpu-info\n"
"          cpu CDATA #REQUIRED\n"
"          percentIdle CDATA #REQUIRED\n"
"          percentUser CDATA #REQUIRED\n"
"          percentKernel CDATA #REQUIRED\n"
">\n";

//-----------------------------------------------------------------------------
// perf_init (perf-init SAF)
//-----------------------------------------------------------------------------

int perf_init(pblock *pb, Session *sn, Request *rq)
{
    if (conf_is_late_init(pb)) {
        pblock_nvinsert("error", "Must not be LateInit", pb);
        return REQ_ABORTED;
    }

    char *disable = pblock_findval("disable", pb);
    if (!disable || !StringValue::getBoolean(disable)) {
        StatsManager::enableProfiling();
    }

    return REQ_NOACTION;
}

//-----------------------------------------------------------------------------
// perf_define_bucket (perf-define-bucket SAF)
//-----------------------------------------------------------------------------

int perf_define_bucket(pblock *pb, Session *sn, Request *rq)
{
    char *name = pblock_findval("name", pb);
    char *description = pblock_findval("description", pb);
    if (!description) description = name;
    if (!name) {
        pblock_nvinsert("error", "name is missing", pb);
    	return REQ_ABORTED;
    }

    StatsManager::addProfileBucket(name, description);

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// isEnabled
//-----------------------------------------------------------------------------

static PRBool isEnabled(PList_t qlist, const char *element)
{
    if (qlist) {
        ParamList *plist;
        plist = param_GetElem(qlist, element);
        if (plist && plist->value && StringValue::isBoolean(plist->value)) {
            return StringValue::getBoolean(plist->value);
        }
    }

    // By default, everything is enabled
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// outputRequestBucket
//-----------------------------------------------------------------------------

static void outputRequestBucket(XMLOutput &xml, PList_t qlist, const StatsRequestBucket *request, const ServerXMLSchema::QosLimits *qos)
{
    if (!isEnabled(qlist, "request-bucket"))
        return;

    PRInt32 maxByteTransmissionRate = 0;
    PRInt32 maxOpenConnections = 0;
    if (qos) {
        if (qos->getMaxBps())
            maxByteTransmissionRate = *qos->getMaxBps();
        if (qos->getMaxConnections())
            maxOpenConnections = *qos->getMaxConnections();
    }

    xml.beginElement("request-bucket");

    // No need to atomic access to strings as caller has already acquired the
    // stats lock.
    xml.flush();
    xml.attribute("method", STATS_GET_METHOD(request));
    xml.attribute("uri", STATS_GET_URI(request));

    xml.attribute("countRequests", request->countRequests);
    xml.attribute("countBytesReceived", request->countBytesReceived);
    xml.attribute("countBytesTransmitted", request->countBytesTransmitted);
    xml.attribute("rateBytesTransmitted", request->rateBytesTransmitted);
    xml.attribute("maxByteTransmissionRate", maxByteTransmissionRate);
    xml.attribute("countOpenConnections", request->countOpenConnections);
    xml.attribute("maxOpenConnections", maxOpenConnections);
    xml.attribute("count2xx", request->count2xx);
    xml.attribute("count3xx", request->count3xx);
    xml.attribute("count4xx", request->count4xx);
    xml.attribute("count5xx", request->count5xx);
    xml.attribute("countOther", request->countOther);
    xml.attribute("count200", request->count200);
    xml.attribute("count302", request->count302);
    xml.attribute("count304", request->count304);
    xml.attribute("count400", request->count400);
    xml.attribute("count401", request->count401);
    xml.attribute("count403", request->count403);
    xml.attribute("count404", request->count404);
    xml.attribute("count503", request->count503);
    xml.endElement("request-bucket");
}

//-----------------------------------------------------------------------------
// outputProfileBuckets
//-----------------------------------------------------------------------------

static void outputProfileBuckets(XMLOutput &xml, PList_t qlist, StatsProfileNode *profileNode)
{
    if (!isEnabled(qlist, "profile-bucket"))
        return;

    int id = 0;
    while (profileNode) {
        StatsProfileBucket* profile = &profileNode->profileStats;
        xml.beginElement("profile-bucket");
        xml.attributef("profile", "profile-%d", id);
        xml.attribute("countCalls", profile->countCalls);
        xml.attribute("countRequests", profile->countRequests);

        // No need to atomic access to 64 bit counters as caller has already
        // acquired the stats lock.
        xml.flush();
        xml.attribute("ticksDispatch", profile->ticksDispatch);
        xml.attribute("ticksFunction", profile->ticksFunction);

        xml.endElement("profile-bucket");
        id++;
        profileNode = profileNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputWebModuleCacheBucket
//-----------------------------------------------------------------------------

static void outputWebModuleCacheBucket(XMLOutput &xml,
                                       PList_t qlist,
                                       StatsWebModuleNode* wmsNode)
{
    if (!wmsNode)
        return;
    StatsWebModuleCacheSlot* wmCacheStats = &wmsNode->wmCacheStats;
    if (wmCacheStats->fEnabled == PR_FALSE) {
        // Cache is not enabled for this webmodule so do nothing.
        return;
    }

    PRInt32 cacheType = wmCacheStats->cacheType;
    if ((cacheType != WM_CACHE_TYPE_BASE_CACHE)
        && (cacheType != WM_CACHE_TYPE_LRU_CACHE)
        && (cacheType != WM_CACHE_TYPE_MULTI_LRU_CACHE)
        && (cacheType != WM_CACHE_TYPE_BOUNDED_MULTI_LRU_CACHE)) {
        // Illegal Cache type.
        // Error condition
        PR_ASSERT(0);
        return;
    }

    xml.beginElement("servlet-response-cache-bucket");
    const char* cacheTypeName = NULL;
    cacheTypeName = StatsWebModuleNode::getCacheTypeName(cacheType);
    xml.attribute("maxEntries", wmCacheStats->maxEntries);
    xml.attribute("threshold", wmCacheStats->threshold);

    xml.attribute("countEntries", wmCacheStats->entryCount);
    xml.attribute("countHits", wmCacheStats->hitCount);
    xml.attribute("countMisses", wmCacheStats->missCount);
    xml.attribute("countEntriesRefreshed", wmCacheStats->refreshCount);
    xml.attribute("countEntriesOverflowed", wmCacheStats->overflowCount);
    xml.attribute("countEntriesAdded", wmCacheStats->addCount);

    // Print specific cache attributes.
    if ((cacheType == WM_CACHE_TYPE_LRU_CACHE)
        || (cacheType == WM_CACHE_TYPE_MULTI_LRU_CACHE)
        || (cacheType == WM_CACHE_TYPE_BOUNDED_MULTI_LRU_CACHE)) {
        xml.attribute("tableSize", wmCacheStats->tableSize);
        xml.attribute("countEntriesRemoved", wmCacheStats->trimCount);
    }

    // 64 bit entries
    if ((cacheType == WM_CACHE_TYPE_BOUNDED_MULTI_LRU_CACHE)) {
        xml.attribute("sizeCurrent", wmCacheStats->currentSize);
        xml.attribute("sizeMax", wmCacheStats->maxSize);
    }

    xml.endElement("servlet-response-cache-bucket");
}

//-----------------------------------------------------------------------------
// outputServletBuckets
//-----------------------------------------------------------------------------

static void outputServletBuckets(XMLOutput &xml, PList_t qlist, 
                                 StatsWebModuleNode* wms)
{
    if (!isEnabled(qlist, "servlet-bucket"))
        return;

    StatsServletJSPNode* sjsNode = wms->sjs;
    while (sjsNode) {
        const char* servletName = sjsNode->name.data();
        if (servletName) {
            StatsServletJSPSlot* sjs = &sjsNode->sjsStats;
            xml.beginElement("servlet-bucket");
            xml.attribute("name", servletName );
            xml.attribute("countRequests", sjs->countRequest);
            xml.attribute("countErrors", sjs->countError);
            // No need to atomic access to 64 bit counters as caller has
            // already acquired the stats lock.
            xml.attribute("millisecondsProcessing", sjs->millisecProcessing);
            xml.attribute("millisecondsPeakProcessing",
                          sjs->millisecPeakProcessing);

            xml.endElement("servlet-bucket");
        }
        sjsNode = sjsNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputWebModuleBuckets
//-----------------------------------------------------------------------------

static void outputWebModuleBuckets(XMLOutput &xml, PList_t qlist, StatsWebModuleNode* wmsNode)
{
    if (!isEnabled(qlist, "web-app-bucket"))
        return;

    while (wmsNode) {
        const char* webModuleName = wmsNode->name.data();
        const char* uri = webModuleName
                    ? StatsManagerUtil::getWebModuleUriFromName(webModuleName)
                    : NULL;
        if (webModuleName &&  uri &&
                (wmsNode->wmsStats.mode != STATS_WEBMODULE_MODE_EMPTY)) {
            const StatsWebModuleSlot* wms = &wmsNode->wmsStats;
            xml.beginElement("web-app-bucket");
            xml.attribute("uri", uri);
            xml.attribute("mode", StatsManager::getMode(wms));
            xml.attribute("countJsps", wms->countJSP);
            xml.attribute("countReloadedJsps", wms->countJSPReload);
            xml.attribute("countSessions", wms->countSessions);
            xml.attribute("countActiveSessions", wms->countActiveSessions);
            xml.attribute("peakActiveSessions", wms->peakActiveSessions);
            xml.attribute("countRejectedSessions", wms->countRejectedSessions);
            xml.attribute("countExpiredSessions", wms->countExpiredSessions);
            xml.attribute("secondsSessionAliveMax", wms->sessionMaxAliveTime);
            xml.attribute("secondsSessionAliveAverage",
                          wms->sessionAvgAliveTime);
            outputWebModuleCacheBucket(xml, qlist, wmsNode);
            outputServletBuckets( xml, qlist,wmsNode);

            xml.endElement("web-app-bucket");
        }
        wmsNode = wmsNode->next;
    }
}

//-----------------------------------------------------------------------------
// getSeconds
//-----------------------------------------------------------------------------

static PRInt64 getSeconds(PRInt64 time)
{
    // return (time + 1000000/2) / 1000000;

    PRInt64 temp;
    PRInt64 dividend;
    PRInt64 divisor;

    LL_UI2L(temp, PR_USEC_PER_SEC/2);
    LL_ADD(dividend, time, temp);
    LL_UI2L(divisor, PR_USEC_PER_SEC);
    LL_DIV(temp, dividend, divisor);

    return temp;
}

//-----------------------------------------------------------------------------
// outputCpuInfos
//-----------------------------------------------------------------------------

static void outputCpuInfos(XMLOutput &xml, PList_t qlist, StatsHeaderNode *header)
{
    if (!isEnabled(qlist, "cpu-info"))
        return;

#ifdef PLATFORM_SPECIFIC_STATS_ON
    StatsCpuInfoNode *cpsNode;
    cpsNode = header->cps;
    int cpuid = 1;
    while (cpsNode) {
        StatsCpuInfoSlot* cps = &cpsNode->cpuInfo;
        xml.beginElement("cpu-info");
        xml.attribute("cpu", cpuid);
        xml.attribute("percentIdle", cps->percentIdle);
        xml.attribute("percentUser", cps->percentUser);
        xml.attribute("percentKernel", cps->percentKernel);
        xml.endElement("cpu-info");
        cpuid++;
        cpsNode = cpsNode->next;
    }
#endif
}

//-----------------------------------------------------------------------------
// outputVirtualServers
//-----------------------------------------------------------------------------

static void outputVirtualServers(XMLOutput &xml, PList_t qlist, const Configuration *configuration, StatsHeaderNode *header)
{
    if (!isEnabled(qlist, "virtual-server"))
        return;

    StatsVirtualServerNode* vssNode = header->vss;
    while (vssNode) {
        StatsVirtualServerSlot* vss = &vssNode->vssStats;
        if (vss->mode != STATS_VIRTUALSERVER_EMPTY) {
            xml.beginElement("virtual-server");
            xml.attribute("id", vss->id);
            xml.attribute("mode", StatsManager::getMode(vssNode));
            const char* hostnames = vssNode->hostnames.data();
            if ( hostnames && hostnames[0] )
                xml.attribute("hosts", hostnames);
            const char* interfaces = vssNode->interfaces.data();
            if ( interfaces && interfaces[0] )
                xml.attribute("interfaces", interfaces );

            const ServerXMLSchema::QosLimits *qos = 0;

            if (configuration) {
                const VirtualServer *vs;
                vs = configuration->getVS(vss->id);
    
                // Find the most specific <qos-limits> for this VS
                if (vs)
                    qos = &vs->qosLimits;
                if (!qos) {
                    if (configuration)
                        qos = &configuration->qosLimits;
                }
            }

            outputRequestBucket(xml, qlist, &vss->requestBucket, qos);
            outputProfileBuckets(xml, qlist, vssNode->profile);
            outputWebModuleBuckets(xml, qlist, vssNode->wms);

            xml.endElement("virtual-server");
        }

        vssNode = vssNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputSessionReplicationStats
//-----------------------------------------------------------------------------

static void outputSessionReplicationStats(XMLOutput &xml, PList_t qlist,
                                          const Configuration *configuration,
                                          const StatsHeaderNode *header)
{
    if (!isEnabled(qlist, "session-replication"))
        return;

    const StatsProcessNode *processNode = header->process;
    if (!processNode)
        return; // No process node.

    const StatsSessionReplicationNode* sessReplNode = processNode->sessReplNode;
    if (!sessReplNode)
        return; // There is no session replication stats

    // no session replication stats if MaxProcs > 1 enabled.
    PR_ASSERT(header->hdrStats.maxProcs == 1);

    xml.beginElement("session-replication");
    const StatsSessionReplicationSlot* sessReplStats = NULL;
    sessReplStats = &sessReplNode->sessReplStats;
    xml.attribute("countSelfRecoveryAttempts",
                  sessReplStats->countSelfRecoveryAttempts);
    xml.attribute("countSelfRecoveryFailures",
                  sessReplStats->countSelfRecoveryFailures);
    xml.attribute("countFailoverAttempts",
                  sessReplStats->countFailoverAttempts);
    xml.attribute("countFailoverFailures",
                  sessReplStats->countFailoverFailures);
    xml.attribute("listClusterMembers", sessReplNode->listClusterMembers);
    xml.attribute("currentBackupInstanceId",
                  sessReplNode->currentBackupInstanceId);
    xml.attribute("state", sessReplNode->state);
    xml.attribute("countBackupConnectionFailures",
                  sessReplStats->countBackupConnFailures);
    xml.attribute("countBackupConnectionFailoverSuccesses",
                  sessReplStats->countBackupConnFailoverSucc);
    xml.attribute("countSentPuts", sessReplStats->countSentPuts);
    xml.attribute("countSentGets", sessReplStats->countSentGets);
    xml.attribute("countSentRemoves", sessReplStats->countSentRemoves);
    xml.attribute("countReceivedPuts", sessReplStats->countReceivedPuts);
    xml.attribute("countReceivedGets", sessReplStats->countReceivedGets);
    xml.attribute("countReceivedRemoves", sessReplStats->countReceivedRemoves);
    if (sessReplStats->flagAsyncQueueEnabled) {
        xml.attribute("countAsyncQueueEntries",
                      sessReplStats->countAsyncQueueEntries);
        xml.attribute("peakAsyncQueueEntries",
                      sessReplStats->peakAsyncQueueEntries);
        xml.attribute("countLockFailures", sessReplStats->countLockFailures);
    }

    StatsSessReplInstanceNode *instanceNode = sessReplNode->instanceNode;
    while (instanceNode) {
        if (instanceNode->mode == STATS_SR_INST_NODE_ACTIVE) {
            xml.beginElement("instance-session-store");
            xml.attribute("instanceId", instanceNode->instanceId);
            StatsWebAppSessionStoreNode *wassNode = instanceNode->wassNode;
            while (wassNode) {
                if (wassNode->mode == STATS_SR_WEBAPP_NODE_ACTIVE) {
                    xml.beginElement("web-app-session-store");
                    xml.attribute("vsId", wassNode->vsId);
                    xml.attribute("uri", wassNode->uri);
                    xml.attribute("countSessions",
                                  wassNode->countReplicatedSessions);
                    xml.endElement("web-app-session-store");
                }
                wassNode = wassNode->next;
            }
            xml.endElement("instance-session-store");
        }
        instanceNode = instanceNode->next;
    }

    xml.endElement("session-replication");
}

//-----------------------------------------------------------------------------
// outputThreads
//-----------------------------------------------------------------------------

static void outputThreads(XMLOutput &xml, PList_t qlist, const Configuration *configuration, StatsProcessNode *process)
{
    if (!isEnabled(qlist, "thread"))
        return;

    StatsThreadNode *threadNode;
    threadNode = process->thread;
    while (threadNode) {
        threadNode->lock();

        StatsThreadSlot* thread = &threadNode->threadStats;
        if (thread->mode != STATS_THREAD_EMPTY) {
            xml.beginElement("thread");
            xml.attribute("mode", StatsManager::getMode(thread));
            xml.attribute("timeStarted", getSeconds(thread->timeStarted));
            const char *function = STATS_GET_FUNCTION_NAME(thread);
            if (function)
                xml.attribute("function", function);
            if (thread->connQueueId[0])
                xml.attribute("connection-queue", thread->connQueueId);
            if (thread->vsId[0])
                xml.attribute("virtual-server", thread->vsId);
            if (thread->addressClient.raw.family != 0)
                xml.attribute("addressClient", thread->addressClient);
            if (thread->timeRequestStarted != 0)
                xml.attribute("timeRequestStarted", thread->timeRequestStarted);

            outputRequestBucket(xml, qlist, &thread->requestBucket,
                                configuration ? &configuration->qosLimits : 0);
            outputProfileBuckets(xml, qlist, threadNode->profile);

            xml.endElement("thread");
        }

        threadNode->unlock();

        threadNode = threadNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputCacheBucket
//-----------------------------------------------------------------------------

static void outputCacheBucket(XMLOutput &xml, PList_t qlist, StatsProcessNode *process)
{
    if (!isEnabled(qlist, "cache-bucket"))
        return;

    StatsCacheBucket *cache = &process->procStats.cacheBucket;
    xml.beginElement("cache-bucket");
    xml.attribute("flagEnabled", cache->flagEnabled);
    xml.attribute("secondsMaxAge", cache->secondsMaxAge);
    xml.attribute("countEntries", cache->countEntries);
    xml.attribute("maxEntries", cache->maxEntries);
    xml.attribute("countOpenEntries", cache->countOpenEntries);
    xml.attribute("maxOpenEntries", cache->maxOpenEntries);
    xml.attribute("sizeHeapCache", cache->sizeHeapCache);
    xml.attribute("maxHeapCacheSize", cache->maxHeapCacheSize);
    xml.attribute("sizeMmapCache", cache->sizeMmapCache);
    xml.attribute("maxMmapCacheSize", cache->maxMmapCacheSize);
    xml.attribute("countHits", cache->countHits);
    xml.attribute("countMisses", cache->countMisses);
    xml.attribute("countInfoHits", cache->countInfoHits);
    xml.attribute("countInfoMisses", cache->countInfoMisses);
    xml.attribute("countContentHits", cache->countContentHits);
    xml.attribute("countContentMisses", cache->countContentMisses);
    xml.attribute("countAcceleratorEntries", cache->countAcceleratorEntries);
    xml.attribute("countAcceleratableRequests", cache->countAcceleratableRequests);
    xml.attribute("countUnacceleratableRequests", cache->countUnacceleratableRequests);
    xml.attribute("countAcceleratableResponses", cache->countAcceleratableResponses);
    xml.attribute("countUnacceleratableResponses", cache->countUnacceleratableResponses);
    xml.attribute("countAcceleratorHits", cache->countAcceleratorHits);
    xml.attribute("countAcceleratorMisses", cache->countAcceleratorMisses);
    xml.endElement("cache-bucket");
}

//-----------------------------------------------------------------------------
// outputKeepAliveBucket
//-----------------------------------------------------------------------------

static void outputKeepAliveBucket(XMLOutput &xml, PList_t qlist, StatsProcessNode *process)
{
    if (!isEnabled(qlist, "keepalive-bucket"))
        return;

    StatsKeepaliveBucket *keepalive = &process->procStats.keepAliveBucket;
    xml.beginElement("keepalive-bucket");
    xml.attribute("countConnections", keepalive->countConnections);
    xml.attribute("maxConnections", keepalive->maxConnections);
    xml.attribute("countHits", keepalive->countHits);
    xml.attribute("countFlushes", keepalive->countFlushes);
    xml.attribute("countRefusals", keepalive->countRefusals);
    xml.attribute("countTimeouts", keepalive->countTimeouts);
    xml.attribute("secondsTimeout", keepalive->secondsTimeout);
    xml.endElement("keepalive-bucket");
}

//-----------------------------------------------------------------------------
// outputDnsBucket
//-----------------------------------------------------------------------------

static void outputDnsBucket(XMLOutput &xml, PList_t qlist, StatsProcessNode *process)
{
    if (!isEnabled(qlist, "dns-bucket"))
        return;

    StatsDnsBucket *dns = &process->procStats.dnsBucket;
    xml.beginElement("dns-bucket");
    xml.attribute("flagCacheEnabled", dns->flagCacheEnabled);
    xml.attribute("countCacheEntries", dns->countCacheEntries);
    xml.attribute("maxCacheEntries", dns->maxCacheEntries);
    xml.attribute("countCacheHits", dns->countCacheHits);
    xml.attribute("countCacheMisses", dns->countCacheMisses);
    xml.attribute("flagAsyncEnabled", dns->flagAsyncEnabled);
    xml.attribute("countAsyncNameLookups", dns->countAsyncNameLookups);
    xml.attribute("countAsyncAddrLookups", dns->countAsyncAddrLookups);
    xml.attribute("countAsyncLookupsInProgress", dns->countAsyncLookupsInProgress);
    xml.endElement("dns-bucket");
}

//-----------------------------------------------------------------------------
// outputThreadPoolBuckets
//-----------------------------------------------------------------------------

static void outputThreadPoolBuckets(XMLOutput &xml, PList_t qlist, StatsProcessNode *process)
{
    if (!isEnabled(qlist, "thread-pool-bucket"))
        return;

    StatsThreadPoolNode *poolNode = process->threadPool;
    int id = 0;
    while (poolNode) {
        StatsThreadPoolBucket* pool = &poolNode->threadPoolStats;
        if (STATS_GET_NAME(pool)) {
            xml.beginElement("thread-pool-bucket");
            xml.attributef("thread-pool", "thread-pool-%d", id);
            xml.attribute("countThreadsIdle", pool->countThreadsIdle);
            xml.attribute("countThreads", pool->countThreads);
            xml.attribute("maxThreads", pool->maxThreads);
            xml.attribute("countQueued", pool->countQueued);
            xml.attribute("peakQueued", pool->peakQueued);
            xml.attribute("maxQueued", pool->maxQueued);
            xml.endElement("thread-pool-bucket");
        }
        id++;
        poolNode = poolNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputConnectionQueueBuckets
//-----------------------------------------------------------------------------

static void outputConnectionQueueBuckets(XMLOutput &xml, PList_t qlist, StatsProcessNode *process)
{
    if (!isEnabled(qlist, "connection-queue-bucket"))
        return;

    StatsConnectionQueueNode *queueNode = process->connQueue;
    while (queueNode) {
        StatsConnectionQueueSlot* queue = &queueNode->connQueueStats;
        xml.beginElement("connection-queue-bucket");
        xml.attribute("connection-queue", queue->id);
        xml.attribute("countTotalConnections", queue->countTotalConnections);
        xml.attribute("countQueued", queue->countQueued);
        xml.attribute("peakQueued", queue->peakQueued);
        xml.attribute("maxQueued", queue->maxQueued);
        xml.attribute("countOverflows", queue->countOverflows);
        xml.attribute("countTotalQueued", queue->countTotalQueued);

        // No need to atomic access to 64 bit counters as caller has already
        // acquired the stats lock.
        xml.flush();
        xml.attribute("ticksTotalQueued", queue->ticksTotalQueued);

        xml.attribute("countQueued1MinuteAverage", queue->countQueued1MinuteAverage);
        xml.attribute("countQueued5MinuteAverage", queue->countQueued5MinuteAverage);
        xml.attribute("countQueued15MinuteAverage", queue->countQueued15MinuteAverage);

        xml.endElement("connection-queue-bucket");
        queueNode = queueNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputJdbcResourceBuckets
//-----------------------------------------------------------------------------

static void outputJdbcResourceBuckets(XMLOutput &xml, PList_t qlist, StatsProcessNode *process)
{
    if (!isEnabled(qlist, "jdbc-resource-bucket"))
        return;
    const StatsJdbcConnPoolNode* poolNode = process->jdbcPoolNode;
    while (poolNode) {
        xml.beginElement("jdbc-resource-bucket");
        const char* poolName = poolNode->poolName.data();
        const StatsJdbcConnPoolSlot* poolSlot = &poolNode->jdbcStats;
        if (!poolName)
            poolName = "";
        xml.attribute("jndiName", poolName);
        xml.attribute("maxConnections", poolSlot->maxConnections);
        xml.attribute("countConnections", poolSlot->currentConnections);
        xml.attribute("peakConnections", poolSlot->peakConnections);
        xml.attribute("countTotalLeasedConnections",
                      poolSlot->totalLeasedConnections);
        xml.attribute("countFreeConnections", poolSlot->freeConnections);
        xml.attribute("countLeasedConnections", poolSlot->leasedConnections);
        xml.attribute("countTotalFailedValidationConnections", poolSlot->totalFailedValidation);
        xml.attribute("countQueued", poolSlot->queueSize);
        xml.attribute("peakQueued", poolSlot->peakQueueSize);
        xml.attribute("millisecondsPeakWait", poolSlot->peakWaitTime);
        xml.attribute("millisecondsAverageQueued", poolSlot->averageQueueTime);
        xml.attribute("countConnectionIdleTimeouts", poolSlot->totalResized);
        xml.attribute("countWaitQueueTimeouts", poolSlot->totalTimedout);

        xml.endElement("jdbc-resource-bucket");
        poolNode = poolNode->next;
    }
}

static void outputJvmManagementStats(XMLOutput &xml, PList_t qlist, const StatsProcessNode *process)
{
    const StatsJvmManagementNode* jvmMgmtNode = process->jvmMgmtNode;
    if (jvmMgmtNode) {
        const StatsJvmManagementSlot& jvmMgmtStats = jvmMgmtNode->jvmMgmtStats;
        xml.beginElement("jvm");
        xml.attribute("countClassesLoaded", jvmMgmtStats.loadedClassCount);
        xml.attribute("countTotalClassesLoaded",
                      jvmMgmtStats.totalLoadedClassCount);
        xml.attribute("countTotalClassesUnloaded", jvmMgmtStats.unloadedClassCount);
        xml.attribute("sizeHeap", jvmMgmtStats.sizeHeapUsed);
        xml.attribute("peakThreads", jvmMgmtStats.peakThreadCount);
        xml.attribute("countTotalThreadsStarted",
                      jvmMgmtStats.totalStartedThreadCount);
        xml.attribute("countThreads", jvmMgmtStats.threadCount);
        xml.attribute("version", jvmMgmtNode->vmVersion);
        xml.attribute("name", jvmMgmtNode->vmName);
        xml.attribute("vendor", jvmMgmtNode->vmVendor);
        xml.attribute("countGarbageCollections",
                      jvmMgmtStats.garbageCollectionCount);
        xml.attribute("millisecondsGarbageCollection",
                      jvmMgmtStats.garbageCollectionTime);
        xml.endElement("jvm");
    }
}

//-----------------------------------------------------------------------------
// outputProcesses
//-----------------------------------------------------------------------------

static void outputProcesses(XMLOutput &xml, PList_t qlist, const Configuration *configuration, StatsHeaderNode *header)
{
    if (!isEnabled(qlist, "process"))
        return;

    StatsProcessNode *processNode = header->process;
    while (processNode) {
        StatsProcessSlot* process = &processNode->procStats;
        if (process->mode != STATS_PROCESS_EMPTY) {
            xml.beginElement("process");
            xml.attribute("pid", process->pid);
            xml.attribute("mode", StatsManager::getMode(processNode));
            xml.attribute("timeStarted", getSeconds(process->timeStarted));
            xml.attribute("countConfigurations", process->countConfigurations);
#ifdef PLATFORM_SPECIFIC_STATS_ON
#ifndef Linux
            //see comments in lib/httpdaemon/linuxprocessstats.cpp

            // process memory usage statistics
            xml.attribute("sizeVirtual", process->sizeVirtual);
            xml.attribute("sizeResident", process->sizeResident);
            // fractionSystemMemoryUsage is a 16 bit percent number
            if ( 0x8000 & process->fractionSystemMemoryUsage )
                xml.attribute("fractionSystemMemoryUsage", "1.0");
            else
                xml.attributef("fractionSystemMemoryUsage", "%.4f", (float)(process->fractionSystemMemoryUsage)/0x8000);
#endif
#endif

            outputConnectionQueueBuckets(xml, qlist, processNode);

            outputThreadPoolBuckets(xml, qlist, processNode);

            outputDnsBucket(xml, qlist, processNode);

            outputKeepAliveBucket(xml, qlist, processNode);

            outputCacheBucket(xml, qlist, processNode);
                
            outputThreads(xml, qlist, configuration, processNode);

            outputJdbcResourceBuckets(xml, qlist, processNode);

            outputJvmManagementStats(xml, qlist, processNode);

            xml.endElement("process");
        }

        processNode = processNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputProfiles
//-----------------------------------------------------------------------------

static void outputProfiles(XMLOutput &xml, PList_t qlist, StatsHeaderNode *header)
{
    if (!isEnabled(qlist, "profile"))
        return;

    int id = 0;
    if ( ! ( header && header->process && 
                   header->process->thread && 
                   header->process->thread->profile ) )
        return;
    StatsProfileNode *profileNode = header->process->thread->profile;
    while (profileNode) {
        StatsProfileBucket* profile = &profileNode->profileStats;
        xml.beginElement("profile");
        xml.attributef("id", "profile-%d", id);
        xml.attribute("name", STATS_GET_NAME(profile));
        xml.attribute("description", STATS_GET_DESCRIPTION(profile));
        xml.endElement("profile");
        id++;
        profileNode = profileNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputThreadPools
//-----------------------------------------------------------------------------

static void outputThreadPools(XMLOutput &xml, PList_t qlist, StatsHeaderNode *header)
{
    if (!isEnabled(qlist, "thread-pool"))
        return;

    if ( ! ( header && header->process && 
                       header->process->threadPool))
        return;

    StatsThreadPoolNode *poolNode;
    poolNode = header->process->threadPool;
    int id = 0;
    while (poolNode) {
        StatsThreadPoolBucket* pool = &poolNode->threadPoolStats;
        if (STATS_GET_NAME(pool)) {
            xml.beginElement("thread-pool");
            xml.attributef("id", "thread-pool-%d", id);
            xml.attribute("name", STATS_GET_NAME(pool));
            xml.endElement("thread-pool");
        }
        id++;
        poolNode = poolNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputConnectionQueues
//-----------------------------------------------------------------------------

static void outputConnectionQueues(XMLOutput &xml, PList_t qlist, StatsHeaderNode *header)
{
    if (!isEnabled(qlist, "connection-queue"))
        return;

    if ( ! ( header && header->process && 
                       header->process->connQueue))
        return;

    StatsConnectionQueueNode *queueNode;
    queueNode = header->process->connQueue;
    while (queueNode) {
        StatsConnectionQueueSlot* queue = &queueNode->connQueueStats;
        xml.beginElement("connection-queue");
        xml.attribute("id", STATS_GET_ID(queue));
        xml.endElement("connection-queue");
        queueNode = queueNode->next;
    }
}

//-----------------------------------------------------------------------------
// outputServer
//-----------------------------------------------------------------------------

static void outputServer(XMLOutput &xml, PList_t qlist, const Configuration *configuration, StatsHeaderNode *headerNode)
{
    if (!isEnabled(qlist, "server"))
        return;

    StatsHeader* header = &headerNode->hdrStats;
    xml.beginElement("server");
    xml.attribute("id", conf_get_true_globals()->Vserver_id);
    xml.attribute("versionServer", header->versionServer);
    xml.attribute("timeStarted", getSeconds(header->timeStarted));
    xml.attribute("secondsRunning", header->secondsRunning);
    xml.attribute("ticksPerSecond", header->ticksPerSecond);
    xml.attribute("maxProcs", header->maxProcs);
    xml.attribute("maxThreads", header->maxThreads);
    xml.attribute("flagProfilingEnabled", header->flagProfilingEnabled);
#ifdef PLATFORM_SPECIFIC_STATS_ON
    // load averages
    xml.attribute("load1MinuteAverage", header->load1MinuteAverage);
    xml.attribute("load5MinuteAverage", header->load5MinuteAverage);
    xml.attribute("load15MinuteAverage", header->load15MinuteAverage);

    // Network throughput
    xml.attribute("rateBytesTransmitted", header->rateBytesTransmitted);
    xml.attribute("rateBytesReceived", header->rateBytesReceived);
#endif

    outputConnectionQueues(xml, qlist, headerNode);

    outputThreadPools(xml, qlist, headerNode);

    outputProfiles(xml, qlist, headerNode);

    outputProcesses(xml, qlist, configuration, headerNode);

    outputVirtualServers(xml, qlist, configuration, headerNode);

    outputSessionReplicationStats(xml, qlist, configuration, headerNode);

    outputCpuInfos(xml, qlist, headerNode);

    xml.endElement("server");
}

//-----------------------------------------------------------------------------
// getDtdUri
//-----------------------------------------------------------------------------

static const char* getDtdUri(PList_t qlist)
{
    ParamList *plist;
    plist = param_GetElem(qlist, "dtd");
    if (plist && plist->value)
        return plist->value;
    return IWSSTATS_DTD;
}

//-----------------------------------------------------------------------------
// stats_xml (stats-xml Service SAF)
//-----------------------------------------------------------------------------

int stats_xml(pblock *pb, Session *sn, Request *rq)
{
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/xml; charset=utf-8", rq->srvhdrs);
    httpfilter_buffer_output(sn, rq, PR_TRUE);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    protocol_start_response(sn,rq);

    // If the last portion of the URI is the DTD filename...
    const char *uri = pblock_findval("uri", rq->reqpb);
    if (uri && (strstr(uri, ".dtd") == (uri + strlen(uri) - 4))) {
        // Send the DTD
        net_write(sn->csd, dtd, sizeof(dtd) - 1);
        return REQ_PROCEED;
    }

    // Get parameters from the query string
    PList_t qlist = NULL;
    char *query = pblock_findval("query", rq->reqpb);
    if (query) {
        qlist = param_Parse(sn->pool, query, NULL);
    }

    StatsHeaderNode* header = StatsManager::getHeader();

    int rv = -1;

#ifdef XP_WIN32
    StatsManager::updateJvmMgmtStats();
    rv = write_stats_xml(sn->csd, header, qlist);
#else
    if ( header && (header->hdrStats.maxProcs == 1) )
    {
        StatsManager::updateStats();
        StatsManager::updateJvmMgmtStats();
        rv = write_stats_xml(sn->csd, header, qlist);
    }
    else
        rv = StatsManager::writeStatsXML(sn->csd, query);
#endif

    PListDestroy(qlist);

    if ( rv < 0 )
        return REQ_ABORTED;

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// write_stats_dtd
//-----------------------------------------------------------------------------

int write_stats_dtd(PRFileDesc *fd)
{
    int size = sizeof(dtd) - 1;

    int rv = PR_Write(fd, dtd, size);
    if (rv != size)
        return -1;

    return 0;
}

//-----------------------------------------------------------------------------
// write_stats_xml
//-----------------------------------------------------------------------------

int write_stats_xml(PRFileDesc *fd, void *hdr, PList_t qlist)
{
    // Note that we're called by stats_xml() when the stats-xml Service SAF is
    // invoked during request processing and by the stats-xml admin server CGI
    StatsHeaderNode* header = (StatsHeaderNode*)hdr;
    int rv = 0;

    XMLOutput xml(fd);
    StatsProcessNode* process = header->process;

    xml.output("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n\n");

    xml.output("<!DOCTYPE stats SYSTEM \"");
    xml.characters(getDtdUri(qlist));
    xml.output("\">\n\n");

    xml.beginElement("stats");
    xml.attribute("versionMajor", STATS_VERSION_MAJOR);
    xml.attribute("versionMinor", STATS_VERSION_MINOR);

    if (!header) {
        xml.attribute("enabled", 0);
        rv = -1;

    } else if (!STATS_IS_COMPATIBLE(&header->hdrStats) || !process || 
                    process->procStats.mode == STATS_PROCESS_EMPTY) {
        xml.attribute("enabled", 0);
        ereport(LOG_FAILURE, "Statistics subsystem error");
        rv = -1;

    } else {
        xml.attribute("enabled", 1);

        const Configuration* configuration = ConfigurationManager::getConfiguration();

        // Acquire the lock for generating the stats-xml
        StatsManager::lockStatsData();
        outputServer(xml, qlist, configuration, header);
        StatsManager::unlockStatsData();

        if (configuration) configuration->unref();
    }

    xml.endElement("stats");

    xml.flush();

    return rv;
}

