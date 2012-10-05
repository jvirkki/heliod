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
 * This file defines types and macros related to the iPlanet Web Server stats
 * table.  The stats table tracks web server statistics and status in near 
 * real time.
 *
 * To ensure consistent structure packing across platforms, pad with chars to
 * align structure members on the appropriate boundaries.  For example, ensure
 * all 64 bit quantities lie on 64 bit boundaries and 32 bit quantities lie on
 * 32 bit boundaries.
 */

#ifndef PUBLIC_IWSSTATS_H
#define PUBLIC_IWSSTATS_H

#include <nspr.h>

PR_BEGIN_EXTERN_C

/* This offset value marks the end of a linked list */
#define STATS_OFFSET_INVALID        0

/* 
 * StatsHeader
 * 
 * This structure is located at the beginning of the stats table.  The presence
 * of STATS_MAGIC in the magic field indicates an initialized stats table.  Any
 * user of the stats table should confirm that its STATS_VERSION_MAJOR level
 * matches the table's versionStatsMajor and that its STATS_VERSION_MINOR level
 * is less than or equal to the table's versionStatsMinor.  The macro 
 * STATS_IS_COMPATIBLE is provided to simplify this determination.
 */

#define STATS_MAGIC                 "iWS\n"
#define STATS_VERSION_MAJOR         1
#define STATS_VERSION_MINOR         3

#define STATS_STATUS_NOT_ENABLED 0x00
#define STATS_STATUS_ENABLED     0x01

typedef struct _StatsHeader {
    char     magic[4];
    PRUint8  versionStatsMajor;
    PRUint8  versionStatsMinor;

    char     versionServer[129];
    char     reserved1[1];
    PRTime   timeStarted;
    PRUint32 ticksPerSecond;
    PRUint32 maxProcs;
    PRUint32 maxThreads;
    PRUint32 maxProfileBuckets;
    PRUint32 secondsRunning;
    PRUint32 countChildDied;
    PRUint32 countReconfig;
    PRInt32  jvmEnabled;
    PRInt32  webappsEnabled;
    PRInt32  secondsUpdateInterval; /* Poll time */
    PRPackedBool flagProfilingEnabled;
    char    reserved2[7];

    PRFloat64 load1MinuteAverage;
    PRFloat64 load5MinuteAverage;
    PRFloat64 load15MinuteAverage;
    PRUint64 rateBytesTransmitted;
    PRUint64 rateBytesReceived;

    /* Note: this structure may grow in future versions */
} StatsHeader;


/* 
 * StatsListenSlot
 * 
 * This structure contains listen socket-specific information and statistics.
 */

#define STATS_LISTEN_EMPTY          0x00
#define STATS_LISTEN_ACTIVE         0x01

typedef struct _StatsListenSlot {
    PRUint32 mode;
    char     id[129];
    char     reserved1[3];

    PRNetAddr address;

    char     reserved2[4];
    PRInt32  countAcceptors;
    PRPackedBool flagSecure;
    char     reserved3[3];
    char     defaultVsId[129];
    char     reserverd[7];

    /* Note: this structure may grow in future versions */
} StatsListenSlot;

/* 
 * StatsRequestBucket
 * 
 * This structure contains request/response statistics.
 */
typedef struct _StatsRequestBucket {
    PRUint64 countRequests;
    PRUint64 countBytesReceived;
    PRUint64 countBytesTransmitted;
    PRUint64 rateBytesTransmitted;
    PRUint32 countOpenConnections;
    PRInt32  reserved;

    PRUint64 count2xx;
    PRUint64 count3xx;
    PRUint64 count4xx;
    PRUint64 count5xx;
    PRUint64 countOther;

    PRUint64 count200;
    PRUint64 count302;
    PRUint64 count304;
    PRUint64 count400;
    PRUint64 count401;
    PRUint64 count403;
    PRUint64 count404;
    PRUint64 count503;

    char method[16];
    char uri[128];

    /* Note: this structure may grow in future versions */
} StatsRequestBucket;

/* 
 * StatsProfileBucket
 * 
 * This structure contains NSAPI profiling statistics.
 */
typedef struct _StatsProfileBucket {
    PRInt32  offsetName;
    PRInt32  offsetDescription;
    PRUint64 countCalls;
    PRUint64 countRequests;
    PRUint64 ticksDispatch;
    PRUint64 ticksFunction;

    /* Note: this structure may grow in future versions */
} StatsProfileBucket;

/* 
 * StatsThreadPoolBucket
 * 
 * This structure contains NSAPI thread pool statistics.
 */
typedef struct _StatsThreadPoolBucket {
    PRInt32  offsetName;
    PRUint32 countThreadsIdle;
    PRUint32 countThreads;
    PRUint32 maxThreads;
    PRUint32 countQueued;
    PRUint32 peakQueued;
    PRUint32 maxQueued;
    PRUint32 reserved;

    /* Note: this structure may grow in future versions */
} StatsThreadPoolBucket;

/* 
 * StatsDnsBucket
 * 
 * This structure contains DNS statistics.
 */
typedef struct _StatsDnsBucket {
    PRPackedBool flagCacheEnabled;
    PRPackedBool flagAsyncEnabled;
    char     reserved[2];

    PRUint32 countCacheEntries;
    PRUint32 maxCacheEntries;
    PRUint32 countCacheHits;
    PRUint32 countCacheMisses;

    PRUint32 countAsyncNameLookups;
    PRUint32 countAsyncAddrLookups;
    PRUint32 countAsyncLookupsInProgress;

    /* Note: this structure may grow in future versions */
} StatsDnsBucket;

/* 
 * StatsKeepaliveBucket
 * 
 * This structure contains keepalive statistics.
 */
typedef struct _StatsKeepaliveBucket {
    PRUint32 countConnections;
    PRUint32 maxConnections;
    PRUint32 numThreads;
    PRUint32 secondsTimeout;
    PRUint64 countHits;
    PRUint64 countFlushes;
    PRUint64 countTimeouts;
    PRUint64 countRefusals;
} StatsKeepaliveBucket;

/* 
 * StatsCacheBucket
 * 
 * This structure contains file cache statistics.
 */
typedef struct _StatsCacheBucket {
    PRPackedBool flagEnabled;
    char     reserved[3];

    PRInt32  secondsMaxAge;
    PRUint32 countEntries;
    PRUint32 maxEntries;
    PRUint32 countOpenEntries;
    PRUint32 maxOpenEntries;
    PRUint64 sizeHeapCache;
    PRUint64 maxHeapCacheSize;
    PRUint64 sizeMmapCache;
    PRUint64 maxMmapCacheSize;
    PRUint64 countHits;
    PRUint64 countMisses;
    PRUint64 countInfoHits;
    PRUint64 countInfoMisses;
    PRUint64 countContentHits;
    PRUint64 countContentMisses;
    PRUint32 countAcceleratorEntries;
    PRInt32  reserved2;
    PRUint64 countAcceleratableRequests;
    PRUint64 countUnacceleratableRequests;
    PRUint64 countAcceleratableResponses;
    PRUint64 countUnacceleratableResponses;
    PRUint64 countAcceleratorHits;
    PRUint64 countAcceleratorMisses;
} StatsCacheBucket;

/* 
 * StatsConnectionQueueSlot
 * 
 * This structure contains connection queue-specific information and
 * statistics.
 */
typedef struct _StatsConnectionQueueSlot {
    char     id[129];
    char     reserved[7];
    PRUint32 countQueued;
    PRUint32 peakQueued;
    PRUint32 maxQueued;
    PRUint32 countOverflows;
    PRUint64 countTotalQueued;
    PRUint64 ticksTotalQueued;
    PRUint64 countTotalConnections;
    PRFloat64 countQueued1MinuteAverage;
    PRFloat64 countQueued5MinuteAverage;
    PRFloat64 countQueued15MinuteAverage;

    /* Note: this structure may grow in future versions */
} StatsConnectionQueueSlot;

/* 
 * StatsCpuInfoSlot
 * 
 * This structure contains processor-specific information and statistics.
 */

#define STATS_CPU_INFO_EMPTY   0x00
#define STATS_CPU_INFO_ACTIVE  0x01

typedef struct _StatsCpuInfoSlot {
    char     id[129];
    char     reserved[7];

    PRFloat64  percentIdle;
    PRFloat64  percentUser;
    PRFloat64  percentKernel;

    /* Note: this structure may grow in future versions */
} StatsCpuInfoSlot;


/* 
 * StatsServletJSPSlot
 * 
 * This structure contains Servlet or JSP statistics
 * This structure size and alignment should be similar for 32/64 bit
 * platforms.
 */

typedef struct _StatsServletJSPSlot {
    PRInt32  countRequest;
    PRInt32  countError;
    PRInt64  millisecProcessing;
    PRInt64  millisecPeakProcessing;
} StatsServletJSPSlot;

/* 
 * StatsWebModuleSlot
 * 
 * This structure contains WebModule statistics
 * This structure size and alignment should be similar for 32/64 bit
 * platforms.
 *
 * STATS_WEBMODULE_MODE_EMPTY mode is for deleted web module nodes.
 * STATS_WEBMODULE_MODE_UNKNOWN mode represents error in web module slot stats.
 * mode member could have following values :
 */

#define STATS_WEBMODULE_MODE_EMPTY    0x00
#define STATS_WEBMODULE_MODE_ENABLED  0x01
#define STATS_WEBMODULE_MODE_DISABLED 0x02
#define STATS_WEBMODULE_MODE_UNKNOWN  0x03

typedef struct _StatsWebModuleSlot {
    PRInt32  mode;
    PRInt32  countJSP;
    PRInt32  countJSPReload;
    PRInt32  countSessions;
    PRInt32  countActiveSessions;
    PRInt32  peakActiveSessions;
    PRInt32  countRejectedSessions;
    PRInt32  countExpiredSessions;
    PRInt32  sessionMaxAliveTime;
    PRInt32  sessionAvgAliveTime;
} StatsWebModuleSlot;



/*
 * StatsWebModuleCacheSlot
 *
 * This structure contains WebModule cache statistics. The entries in the
 * structure is valid only if the cache is enabled for the webapps. Cache type
 * is determined by the following macros.
 */

#define WM_CACHE_TYPE_UNKNOWN                 0x00
#define WM_CACHE_TYPE_BASE_CACHE              0x01
#define WM_CACHE_TYPE_LRU_CACHE               0x02
#define WM_CACHE_TYPE_MULTI_LRU_CACHE         0x03
#define WM_CACHE_TYPE_BOUNDED_MULTI_LRU_CACHE 0x04

typedef struct _StatsWebModuleCacheSlot {
    PRInt32  fEnabled; /* Store PR_TRUE/PR_FALSE */
    PRInt32  cacheType; /* type of cache as defined by WM_CACHE_xxx macros */
    PRInt32  maxEntries;
    PRInt32  threshold;
    PRInt32  tableSize;
    PRInt32  entryCount;
    PRInt32  hitCount;
    PRInt32  missCount;
    PRInt32  removalCount;
    PRInt32  refreshCount;
    PRInt32  overflowCount;
    PRInt32  addCount;
    PRInt32  lruListLength;
    PRInt32  trimCount;
    PRInt32  segmentSize;
    PRInt32  reserved;

    PRInt64  currentSize;
    PRInt64  maxSize;
} StatsWebModuleCacheSlot;

/*
 * StatsJvmManagementSlot
 *
 * This structure contains Jvm management statistics.
 */

typedef struct _StatsJvmManagementSlot {
    PRInt32  reserved;
    PRInt32  loadedClassCount;
    PRInt64  totalLoadedClassCount;
    PRInt64  unloadedClassCount;
    PRInt64  sizeHeapUsed;
    PRInt32  threadCount;
    PRInt32  peakThreadCount;
    PRInt64  totalStartedThreadCount;
    PRInt64  garbageCollectionCount;
    PRInt64  garbageCollectionTime;
} StatsJvmManagementSlot;

/*
 * StatsSessionReplicationSlot
 *
 * This structure contains session replication statistics.
 */

typedef struct _StatsSessionReplicationSlot {
    PRInt32 countInstances;
    PRInt32 countSelfRecoveryAttempts;
    PRInt32 countSelfRecoveryFailures;
    PRInt32 countFailoverAttempts;
    PRInt32 countFailoverFailures;
    PRInt32 countBackupConnFailures;
    PRInt32 countBackupConnFailoverSucc;
    PRInt32 countSentPuts;
    PRInt32 countSentGets;
    PRInt32 countSentRemoves;
    PRInt32 countReceivedPuts;
    PRInt32 countReceivedGets;
    PRInt32 countReceivedRemoves;
    PRInt32 flagAsyncQueueEnabled; /* 1 : enabled , 0 : disabled. */
    PRInt32 countAsyncQueueEntries;
    PRInt32 peakAsyncQueueEntries;
    PRInt32 countLockFailures;
    PRInt32 reserved;

} StatsSessionReplicationSlot;

/*
 * Session replication instance node and webapp session store node status.
 */
#define STATS_SR_INST_NODE_INVALID   0x00
#define STATS_SR_INST_NODE_ACTIVE    0x01

#define STATS_SR_WEBAPP_NODE_INVALID   0x00
#define STATS_SR_WEBAPP_NODE_ACTIVE    0x01


/* 
 * StatsVirtualServerSlot
 * 
 * This structure contains virtual server-specific information and statistics.
 *
 * Virtual Server mode could have four possible value at this moment.
 * STATS_VIRTUALSERVER_EMPTY mode is to mark that previously existing virtual
 *                           server which is deleted.
 * STATS_VIRTUALSERVER_ACTIVE : virtual server is enabled and active.
 * STATS_VIRTUALSERVER_DISABLED : virtual server was enabled at the start time
 *                                but disabled at this moment. If virtual
 *                                server is disabled in the beginning, node is
 *                                never created.
 * STATS_VIRTUALSERVER_UNKNOWN : Showing error in virtual server stats.
 */

#define STATS_VIRTUALSERVER_EMPTY    0x00
#define STATS_VIRTUALSERVER_ACTIVE   0x01
#define STATS_VIRTUALSERVER_DISABLED 0x02
#define STATS_VIRTUALSERVER_UNKNOWN  0x03

typedef struct _StatsVirtualServerSlot {
    PRUint32 mode;
    PRInt32  reserved;
    char     id[129];
    char     reserved2[7];

    StatsRequestBucket requestBucket;

    /* Note: this structure may grow in future versions */
} StatsVirtualServerSlot;

/* 
 * StatsThreadSlot
 * 
 * This structure contains thread-specific information and statistics.
 */

#define STATS_THREAD_EMPTY          0x00
#define STATS_THREAD_IDLE           0x01
#define STATS_THREAD_DNS            0x02
#define STATS_THREAD_REQUEST        0x03
#define STATS_THREAD_PROCESSING     0x04
#define STATS_THREAD_RESPONSE       0x05
#define STATS_THREAD_UPDATING       0x06
#define STATS_THREAD_KEEPALIVE      0x07

typedef struct _StatsThreadSlot {
    PRTime   timeStarted;
    PRUint32 mode;
    PRInt32  offsetFunctionName;
    char     connQueueId[129];
    char     reserved1[7];
    char     vsId[129];
    char     reserved2[7];

    StatsRequestBucket requestBucket;

    PRNetAddr addressClient;
    PRTime    timeRequestStarted;

    /* Note: this structure may grow in future versions */
} StatsThreadSlot;

/* 
 * StatsProcessSlot
 * 
 * This structure contains process-specific information and statistics.
 * requestBucket member stores the request count of sum of request of all
 * threads.
 */

#define STATS_PROCESS_EMPTY         0x00
#define STATS_PROCESS_ACTIVE        0x01

typedef struct _StatsProcessSlot {
    PRTime   timeStarted;
    PRUint32 mode;
    PRInt32  pid;
    PRUint32 countConnections;
    PRUint32 peakConnections;
    PRUint32 countConfigurations;
    PRUint16 fractionSystemMemoryUsage;
    char     reserved[2];

    PRInt32  countConnectionQueues;
    PRInt32  countThreadPools;
    PRInt32  jvmManagentStats; /* 0 or 1 (Disable/Enabled) */
    PRInt32  countIdleThreads;
    PRInt32  countThreads;
    PRInt32  reserved2;

    StatsCacheBucket         cacheBucket;
    StatsDnsBucket           dnsBucket;
    StatsKeepaliveBucket     keepAliveBucket;
    StatsRequestBucket       requestBucket;

    PRUint64 sizeVirtual;
    PRUint64 sizeResident;

    /* Note: this structure may grow in future versions */
} StatsProcessSlot;


/*
 * StatsJdbcConnPoolSlot
 *
 * This structure contains Jdbc connection pool statistics (single connection
 * pool).
 */

typedef struct _StatsJdbcConnPoolSlot {
    PRInt32 maxConnections;
    PRInt32 currentConnections;
    PRInt32 peakConnections;
    PRInt32 freeConnections;
    PRInt32 leasedConnections;
    PRInt32 totalFailedValidation;
    PRInt32 totalRecreatedConnections;
    PRInt32 queueSize;
    PRInt32 peakQueueSize;
    PRInt32 totalResized;
    PRInt32 totalTimedout;
    PRInt32 reserved;

    PRUint64 totalLeasedConnections;
    PRUint64 peakWaitTime;
    PRFloat64 averageQueueTime;
} StatsJdbcConnPoolSlot;


/*
 * Stats Bucket for storing averages for different time periods.
 */

typedef struct _StatsAvgBucket {
    PRFloat64 oneMinuteAverage;
    PRFloat64 fiveMinuteAverage;
    PRFloat64 fifteenMinuteAverage;
} StatsAvgBucket;


/*
 * Stats slot for sum of all virtual server (and it's all profile bucket)
 * stats per process. This slot also contains various averages.
 */

typedef struct _StatsAccumulatedVSSlot {
    StatsRequestBucket requestBucket;
    StatsProfileBucket allReqsProfileBucket;
    StatsAvgBucket requestAvgBucket;
    StatsAvgBucket errorAvgBucket;
    StatsAvgBucket responseTimeAvgBucket;
    PRFloat64 maxResponseTime;
} StatsAccumulatedVSSlot;

/*
 * The following macros aid in navigation of the stats table.
 */

#define STATS_IS_STATS_TABLE(hdr) \
        ((hdr)->magic[0] == STATS_MAGIC[0] && \
        (hdr)->magic[1] == STATS_MAGIC[1] && \
        (hdr)->magic[2] == STATS_MAGIC[2] && \
        (hdr)->magic[3] == STATS_MAGIC[3])

#define STATS_IS_COMPATIBLE(hdr) \
        (STATS_IS_STATS_TABLE(hdr) && \
        (hdr)->versionStatsMajor == STATS_VERSION_MAJOR && \
        (hdr)->versionStatsMinor >= STATS_VERSION_MINOR)

#define STATS_GET_ID(slot) \
        ((const char*)((slot) ? (slot)->id : NULL))

#define STATS_GET_NAME(slot) \
        ((const char*)((slot) ? \
        StatsManager::getNameFromStringStore((slot)->offsetName) : \
        NULL))

#define STATS_GET_DESCRIPTION(slot) \
        ((const char*)((slot) ? \
        StatsManager::getNameFromStringStore((slot)->offsetDescription) \
        : NULL))

#define STATS_GET_METHOD(slot) \
        ((const char*)(((slot) && (slot)->method[0]) ? \
        ((slot)->method ) : NULL))

#define STATS_GET_URI(slot) \
        ((const char*)(((slot) && (slot)->uri[0]) ? \
        ((slot)->uri ) : NULL))

#define STATS_GET_FUNCTION_NAME(slot) \
        ((const char*)((slot) ? \
        StatsManager::getNameFromStringStore((slot)->offsetFunctionName) : \
        NULL))

#define STATS_BUFFER_ALIGNMENT_OK 1

#ifdef STATS_BUFFER_ALIGNMENT_OK
#define STATS_BUFFER_ACCESS(type,ptrVar,buffer) \
        ptrVar = (type*) (buffer);
#else 
#define STATS_BUFFER_ACCESS(type,ptrVar,buffer) \
        type src##ptrVar; \
        memcpy( &src##ptrVar, (buffer), sizeof(type)); \
        (ptrVar) = &src##ptrVar;

#endif 


PR_END_EXTERN_C

#endif /* PUBLIC_IWSSTATS_H */
