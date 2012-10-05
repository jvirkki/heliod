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
 * internal statistics.
 *
 * A common header file for various pieces of internal statistics.
 *
 *
 */

#ifndef _INTERNAL_STATS_
#define _INTERNAL_STATS_

#include <prtypes.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct listenSocketInfo {
    PRUint32      port;
    char          *ipAddr;
    PRBool        securityEnabled;

    PRUint32      numActiveThreads;
    PRUint32      numWaitingThreads;
    PRUint32      numBusyThreads;
    PRUint32      numIdleThreads;

    PRUint32      numMinThreads;
    PRUint32      numMaxThreads;
} listenSocketInfo;

PRBool GetListenSocketInfo(PRUint32 *numSockets, listenSocketInfo **infoArray);


typedef struct keepAliveInfo {
    PRUint32      numKeepAlives;
    PRUint32      numKeepAliveThreads;
    PRUint32      maxKeepAlives;

    PRUint32      numKeepAliveHits;
    PRUint32      numKeepAliveFlushes;
    PRUint32      numKeepAliveRefusals;
    PRUint32      numKeepAliveTimeouts;
    PRUint32      tmoKeepAlive;
} keepAliveInfo;

PRBool GetKeepAliveInfo(keepAliveInfo *info);


typedef struct sessionCreationInfo {
    PRUint32      totalThreadsLimit;
    PRUint32      numSessionsCreated;
    PRUint32      numSessionsInFreeList;
    PRUint32      numSessionsInBusyList;
} sessionCreationInfo;

PRBool GetSessionCreationInfo(sessionCreationInfo *info);


typedef struct cacheInfo {
    PRBool        enabled;
    PRUint32      numCacheEntries;
    PRUint32      maxCacheEntries;
    PRUint32      numOpenCacheEntries;
    PRUint32      maxOpenCacheEntries;

    PRUint32      numCacheHits;
    PRUint32      numCacheMisses;

    PRUint32      numCacheInsertsOk;
    PRUint32      numCacheInsertsFail;
    PRUint32      numCacheDeletes;

    PRUint32      pollInterval;
    PRUint32      maxTotalCacheSize;
    PRUint32      maxCurrentTotalCacheSize;
    PRUint32      maxFileSize;
} cacheInfo;

PRBool GetCacheInfo(cacheInfo *info);


typedef struct DNSCacheInfo {
    PRBool        enabled;
    PRUint32      numCacheEntries;
    PRUint32      maxCacheEntries;

    PRUint32      numCacheHits;
    PRUint32      numCacheMisses;

    PRUint32      numCacheInsertsOk;
    PRUint32      numCacheInsertsFail;
    PRUint32      numCacheDeletes;
} DNSCacheInfo;

PRBool GetDNSCacheInfo(DNSCacheInfo *info);

typedef struct NativeThreadPoolInfo {
    PRInt32 pool_thread_count;
    PRInt32 pool_thread_max;
    PRInt32 pool_thread_available;
    PRInt32 pool_work_pending;
    PRInt32 pool_work_queue_max;
    PRInt32 pool_reject_count;
    PRInt32 pool_work_queue_peak;
}NativeThreadPoolInfo;

PRBool GetNativeThreadPoolInfo(NativeThreadPoolInfo *info);

typedef struct MemUsageInfo {
    PRUint64 process_size_in_Kbytes;
    PRUint64 process_resident_size_in_Kbytes;
    PRUint16 fraction_system_memory_usage;
}MemUsageInfo;

typedef struct LoadAveragesInfo {
    PRFloat64 load_avg_for_1min;
    PRFloat64 load_avg_for_5min;
    PRFloat64 load_avg_for_15min;
}LoadAveragesInfo;

typedef struct NetworkStatsInfo {
    PRUint64 in_bytes_per_sec;
    PRUint64 out_bytes_per_sec;
}NetworkStatsInfo;

typedef struct ProcessorInfo {
    PRFloat64 percent_idle_time;
    PRFloat64 percent_user_time;
    PRFloat64 percent_kernel_time;
}ProcessorInfo;

#ifdef __cplusplus
}
#endif

#endif
