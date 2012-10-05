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
 * dump.cpp: service-dump Service SAF (originally from service.c)
 *
 * Mike Belshe et al.
 */

#ifdef XP_WIN32
#include <process.h>
#endif

#include "netsite.h"
#include "support/NSString.h"
#include "frame/http.h"
#include "frame/httpfilter.h"
#include "httpdaemon/statsmanager.h"
#include "safs/dump.h"

#define SAFESTRING(a) ((a) ? (a) : "")

/*
 * SessionColumnEnum is a 0-based index of a column in the service-dump
 * Sessions list.
 */
enum SessionColumnEnum {
    SESSION_COLUMN_PROCESS = 0,
    SESSION_COLUMN_STATUS,
    SESSION_COLUMN_CLIENT,
    SESSION_COLUMN_AGE,
    SESSION_COLUMN_VS,
    SESSION_COLUMN_METHOD,
    SESSION_COLUMN_URI,
    SESSION_COLUMN_FUNCTION,
    SESSION_COLUMN_COUNT
};

/*
 * session_column_labels contains the labels of the columns in the service-dump
 * Sessions list, arranged in SessionColumnEnum order.
 */
static const NSString session_column_labels[SESSION_COLUMN_COUNT] = {
    "Process",
    "Status",
    "Client",
    "Age",
    "VS",
    "Method",
    "URI",
    "Function"
};

/*
 * SessionRow contains data from a row of the service-dump Sessions list.
 */
struct SessionRow {
    PRTime timeRequestStarted;
    NSString columns[SESSION_COLUMN_COUNT];
};


/* ------------------------------- percent -------------------------------- */

static PRFloat64 percent(PRInt64 hits, PRInt64 lookups)
{
    return (lookups > 0) ? (hits * 100.0 / lookups) : 0.0;
}


/* ---------------------------- format_session ---------------------------- */

static int format_session(char *line, int *offsets, const NSString *columns)
{
    int pos = 0;

    for (int x = 0; x < SESSION_COLUMN_COUNT; x++) {
        // Pad prior column with whitespace as necessary
        while (pos < offsets[x])
            line[pos++] = ' ';

        // Copy this column's value
        const char *p = columns[x];
        while (*p)
            line[pos++] = *p++;
    }

    line[pos++] = '\n';

    return pos;
}


/* ----------------------------- session_cmp ------------------------------ */

static int session_cmp(const void *p1, const void *p2)
{
    const SessionRow *row1 = *(const SessionRow **)p1;
    const SessionRow *row2 = *(const SessionRow **)p2;

    PRTime time1 = row1->timeRequestStarted;
    PRTime time2 = row2->timeRequestStarted;

    // keep-alive threads come last
    if (time1 == 0)
        time1 = LL_MAXINT;
    if (time2 == 0)
        time2 = LL_MAXINT;

    // Older requests come before newer requests
    if (time1 < time2)
        return -1;
    if (time1 > time2)
        return 1;

    // For requests that arrived at the same time, lower SessionRow addresses
    // (that is, lower-numbered threads) come first
    if (row1 < row2)
        return -1;
    if (row1 > row2)
        return 1;

    return 0;
}


/* -------------------------- service_dumpstats --------------------------- */

int service_dumpstats(pblock *pb, Session *sn, Request *rq)
{
    char *refresh, *refresh_val = NULL;

    /* See if client asked for automatic refresh in query string */
    if ((refresh = pblock_findval("query", rq->reqpb)) != NULL ) {
        if (!strncmp("refresh", refresh, 7)) {
             refresh_val = strchr(refresh, '=');
             if (refresh_val)
                 refresh_val++;
        }
    }

    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/plain", rq->srvhdrs);
    if (refresh_val)
        pblock_nvinsert("refresh", refresh_val, rq->srvhdrs);
    httpfilter_buffer_output(sn, rq, PR_TRUE);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    protocol_start_response(sn,rq);

    PR_fprintf(sn->csd, PRODUCT_DAEMON_BIN" pid: %d\n", getpid());

    StatsHeaderNode *hdr = StatsManager::getHeader();

    if (!hdr) {
        PR_fprintf(sn->csd, "\nStatistics disabled\n");
        return REQ_PROCEED;
    }
    int rv = REQ_PROCEED;
#ifdef XP_WIN32
    rv = write_stats_dump(sn->csd, hdr);
#else
    if (hdr && (hdr->hdrStats.maxProcs == 1)) {
        rv = write_stats_dump(sn->csd, hdr);
    } else {
        StatsManager::serviceDumpStats(sn->csd, NULL);
    }
#endif
    return rv;
}


/* --------------------------- write_stats_dump --------------------------- */

/*
 * This function may be called by either the child or primordial process
 * depending on MaxProcs.
 */

int write_stats_dump(PRFileDesc *fd, StatsHeaderNode *hdr)
{
    int i = 0;
    StatsManager::lockStatsData();
    const StatsHeader *hdrStats = &hdr->hdrStats;

    // Version
    PR_fprintf(fd, "\n%s\n", hdrStats->versionServer);

    // Uptime
    char buffer[25];
    PRExplodedTime tm;
    PR_ExplodeTime(hdrStats->timeStarted, PR_LocalTimeParameters, &tm);
    PR_FormatTimeUSEnglish(buffer, sizeof(buffer), "%c", &tm);
    PR_fprintf(fd, "\nServer started %s\n", buffer);
    {
        StatsProcessNode *process = hdr->process;
        while (process) {
            const StatsProcessSlot *procStats = &process->procStats;
            PR_ExplodeTime(procStats->timeStarted, PR_LocalTimeParameters, &tm);
            PR_FormatTimeUSEnglish(buffer, sizeof(buffer), "%c", &tm);
            PR_fprintf(fd, "Process %d started %s\n", procStats->pid, buffer);
            process = process->next;
        }
    }

    // ConnectionQueue
    {
        StatsConnectionQueueSlot connqueues;
        memset(&connqueues, 0, sizeof(connqueues));

        // Accumulate connection queue buckets for every process
        StatsProcessNode *process = hdr->process;
        while (process) {
            StatsConnectionQueueNode *connQueueNode = process->connQueue;
            while (connQueueNode) {
                const StatsConnectionQueueSlot *connqueue = &connQueueNode->connQueueStats;
                connqueues.countQueued += connqueue->countQueued;
                connqueues.peakQueued += connqueue->peakQueued;
                connqueues.maxQueued += connqueue->maxQueued;
                connqueues.countOverflows += connqueue->countOverflows;
                connqueues.countTotalQueued += connqueue->countTotalQueued;
                connqueues.ticksTotalQueued += connqueue->ticksTotalQueued;
                connqueues.countTotalConnections += connqueue->countTotalConnections;
                connqueues.countQueued1MinuteAverage += connqueue->countQueued1MinuteAverage;
                connqueues.countQueued5MinuteAverage += connqueue->countQueued5MinuteAverage;
                connqueues.countQueued15MinuteAverage += connqueue->countQueued15MinuteAverage;
                connQueueNode = connQueueNode->next;
            }
            process = process->next;
        }

        PRFloat64 count = (PRInt64)connqueues.countTotalQueued;
        PRFloat64 ticks = (PRInt64)connqueues.ticksTotalQueued;
        if (count < 1.0)
            count = 1.0;

        PR_fprintf(fd,
                   "\nConnectionQueue:\n"
                   "-----------------------------------------\n"
                   "Current/Peak/Limit Queue Length            %d/%d/%d\n"
                   "Total Connections Queued                   %llu\n"
                   "Average Queue Length (1, 5, 15 minutes)    %4.2f, %4.2f, %4.2f\n"
                   "Average Queueing Delay                     %.2f milliseconds\n",
                   connqueues.countQueued, connqueues.peakQueued, connqueues.maxQueued,
                   connqueues.countTotalQueued,
                   connqueues.countQueued1MinuteAverage,
                   connqueues.countQueued5MinuteAverage,
                   connqueues.countQueued15MinuteAverage,
                   ticks / count / hdrStats->ticksPerSecond * 1000.0);
    }

    // Listen sockets
    {
        StatsProcessNode *process = hdr->process;
        StatsListenNode *lsNode = (process) ? process->ls : 0;
        while (lsNode) {
            StatsListenSlot *ls = &lsNode->lsStats;
            PRNetAddr addr;
            memcpy(&addr, &ls->address, sizeof(ls->address));
            char szAddress[512];
            memset(szAddress, 0, sizeof(szAddress));
            net_addr_to_string(&addr, szAddress, sizeof(szAddress)-1);
            PR_fprintf(fd, 
                       "\nListenSocket %s:\n"
                       "------------------------\n"
                       "Address                   %s://%s:%hu\n"
                       "Acceptor Threads          %d\n"
                       "Default Virtual Server    %s\n",
                       ls->id,
                       ls->flagSecure ? "https" : "http",
                       szAddress,
                       PR_ntohs(ls->address.inet.port),
                       ls->countAcceptors,
                       ls->defaultVsId
                    );
            lsNode = lsNode->next;
        }
    }

    // Keepalive Info
    {
        StatsKeepaliveBucket keepalives;
        memset(&keepalives, 0, sizeof(keepalives));

        // Accumulate keepalive buckets for every process
        StatsProcessNode *process = hdr->process;
        while (process) {
            StatsKeepaliveBucket *procKeepAlive = NULL;
            procKeepAlive = &process->procStats.keepAliveBucket;
            StatsManagerUtil::accumulateKeepAlive(&keepalives, procKeepAlive);
            process = process->next;
        }

        PR_fprintf(fd,
                   "\nKeepAliveInfo:\n"
                   "--------------------\n"
                   "KeepAliveCount        %lu/%lu\n"
                   "KeepAliveHits         %llu\n"
                   "KeepAliveFlushes      %llu\n"
                   "KeepAliveRefusals     %llu\n"
                   "KeepAliveTimeouts     %llu\n"
                   "KeepAliveTimeout      %lu seconds\n",
                   keepalives.countConnections,
                   keepalives.maxConnections,
                   keepalives.countHits,
                   keepalives.countFlushes,
                   keepalives.countRefusals,
                   keepalives.countTimeouts,
                   keepalives.secondsTimeout);
    }

    // Session Creation Info
    {
        PRUint32 countThreads = 0;
        PRUint32 countBusy = 0;
        PRUint32 countKeepAlive = 0;
        PRUint32 countKeepAliveThreads = 0;

        // Count the number of active threads
        StatsProcessNode *process = hdr->process;
        while (process) {
            StatsProcessSlot *procStats = &process->procStats;
            if (procStats->mode != STATS_PROCESS_EMPTY) {
                StatsThreadNode *thread = process->thread;
                while (thread) {
                    const StatsThreadSlot *threadStats = &thread->threadStats;
                    if (threadStats->mode != STATS_THREAD_EMPTY) {
                        countThreads++;
                        if (threadStats->mode == STATS_THREAD_KEEPALIVE) {
                            countKeepAlive++;
                        } else if (threadStats->mode != STATS_THREAD_IDLE) {
                            countBusy++;
                        }
                    }
                    thread = thread->next;
                }
            }
            // Get number of keepalive threads
            StatsKeepaliveBucket *procKeepAlive = NULL;
            procKeepAlive = &process->procStats.keepAliveBucket;
            countKeepAliveThreads += procKeepAlive->numThreads;

            process = process->next;
        }

        PR_fprintf(fd, 
                   "\nSessionCreationInfo:\n"
                   "------------------------\n"
                   "Active Sessions           %lu\n"
                   "Keep-Alive Sessions       %lu\n"
                   "Total Sessions Created    %lu/%lu\n",
                   countBusy,
                   countKeepAlive,
                   countThreads, (hdrStats->maxThreads * hdrStats->maxProcs + countKeepAliveThreads));
    }

    // Cache Info
    {
        StatsCacheBucket caches;
        memset(&caches, 0, sizeof(caches));

        // Accumulate cache buckets for every process
        StatsProcessNode *process = hdr->process;
        while (process) {
            StatsCacheBucket *procCache = NULL;
            procCache = &process->procStats.cacheBucket;
            StatsManagerUtil::accumulateCache(&caches, procCache);
            process = process->next;
        }

        if (caches.flagEnabled) {
            PR_fprintf(fd,
                       "\nCacheInfo:\n"
                       "-----------------------\n");

            PRInt64 fileLookups = caches.countHits + caches.countMisses;

            PR_fprintf(fd,
                       "File Cache Enabled       yes\n"
                       "File Cache Entries       %lu/%lu\n"
                       "File Cache Hit Ratio     %llu/%lld (%6.2f%%)\n"
                       "Maximum Age              %d\n",
                       caches.countEntries,
                       caches.maxEntries,
                       caches.countHits,
                       fileLookups,
                       percent(caches.countHits, fileLookups),
                       caches.secondsMaxAge);

            PRInt64 accelRequests = caches.countAcceleratableRequests + caches.countUnacceleratableRequests;
            PRInt64 accelResponses = caches.countAcceleratableResponses + caches.countUnacceleratableResponses;
            PRInt64 accelLookups = caches.countAcceleratorHits + caches.countAcceleratorMisses;

            PR_fprintf(fd,
                       "Accelerator Entries      %lu/%lu\n"
                       "Acceleratable Requests   %llu/%lld (%6.2f%%)\n"
                       "Acceleratable Responses  %llu/%lld (%6.2f%%)\n"
                       "Accelerator Hit Ratio    %llu/%lld (%6.2f%%)\n",
                       caches.countAcceleratorEntries,
                       caches.maxEntries,
                       caches.countAcceleratableRequests,
                       accelRequests,
                       percent(caches.countAcceleratableRequests, accelRequests),
                       caches.countAcceleratableResponses,
                       accelResponses,
                       percent(caches.countAcceleratableResponses, accelResponses),
                       caches.countAcceleratorHits,
                       accelLookups,
                       percent(caches.countAcceleratorHits, accelLookups));
        } else {
            PR_fprintf(fd, "\nServer cache disabled\n");
        }
    }

    // Thread pool info
    {
        StatsThreadPoolBucket pools[FUNC_MAXPOOLS];
        const char *names[FUNC_MAXPOOLS];
        memset(pools, 0, sizeof(pools));
        memset(names, 0, sizeof(names));

        // Accumulate thread pool buckets for every process
        StatsProcessNode *process = hdr->process;
        while (process) {
            StatsThreadPoolNode *pool = process->threadPool;
            for (i = 0; pool && (i < FUNC_MAXPOOLS); i++) {
                StatsManagerUtil::accumulateThreadPool(&pools[i],
                                                       &pool->threadPoolStats);
                names[i] = STATS_GET_NAME(&pool->threadPoolStats);
                pool = pool->next;
            }
            process = process->next;
        }

        int poolFlag = 0;

        for (i = 0; i < FUNC_MAXPOOLS; i++)
        {
            if (names[i]) {
                if (!poolFlag)
                {
                    poolFlag = 1;
                    PR_fprintf(fd,
                               "\nNative pools:\n"
                               "----------------------------\n");
                }

                PR_fprintf(fd,
                           "%s:\n"
                           "Idle/Peak/Limit               %lu/%lu/%lu\n"
                           "Work Queue Length/Peak/Limit  %lu/%lu/%lu\n",
                           names[i],
                           pools[i].countThreadsIdle, 
                           pools[i].countThreads, 
                           pools[i].maxThreads,
                           pools[i].countQueued, 
                           pools[i].peakQueued, 
                           pools[i].maxQueued);
            }
        }
    }

    // DNS info
    {
        StatsDnsBucket dnss;
        memset(&dnss, 0, sizeof(dnss));

        // Accumulate DNS buckets for every process
        StatsProcessNode *process = hdr->process;
        while (process) {
            StatsManagerUtil::accumulateDNS(&dnss,
                                            &process->procStats.dnsBucket);
            process = process->next;
        }

        if (dnss.flagCacheEnabled) {
            PRUint32 totalhits = dnss.countCacheMisses + dnss.countCacheHits;
            float hitratio;

            if (totalhits > 0.0)
                hitratio = (float)dnss.countCacheHits / (float)totalhits;
            else
                hitratio = 0.0;
            
            PR_fprintf(fd, 
                       "\nDNSCacheInfo:\n"
                       "------------------\n"
                       "enabled             yes\n"
                       "CacheEntries        %lu/%lu\n"
                       "HitRatio            %lu/%lu (%6.2f%%)\n",
                       dnss.countCacheEntries,
                       dnss.maxCacheEntries,
                       dnss.countCacheHits,
                       totalhits,
                       hitratio * 100.0);
        } else {
            PR_fprintf(fd, "\nServer DNS cache disabled\n");
        }

        if (dnss.flagAsyncEnabled) {
            PR_fprintf(fd, 
                       "\nAsyncDNS Data:\n"
                       "------------------\n"
                       "enabled             yes\n"
                       "NameLookups         %lu\n"
                       "AddrLookups         %lu\n"
                       "LookupsInProgress   %lu\n",
                       dnss.countAsyncNameLookups,
                       dnss.countAsyncAddrLookups,
                       dnss.countAsyncLookupsInProgress);
        } else {
            PR_fprintf(fd, "\nAsync DNS disabled\n");
        }
    }

    // NSAPI Profile Info
    if (hdrStats->maxProfileBuckets) {
        int i;

        StatsProfileBucket *profiles = new StatsProfileBucket[hdrStats->maxProfileBuckets];
        const char **names = new const char*[hdrStats->maxProfileBuckets];
        const char **descs = new const char*[hdrStats->maxProfileBuckets];
        memset(profiles, 0, sizeof(profiles[0]) * hdrStats->maxProfileBuckets);
        memset(names, 0, sizeof(names[0]) * hdrStats->maxProfileBuckets);
        memset(descs, 0, sizeof(descs[0]) * hdrStats->maxProfileBuckets);

        // Accumulate profiles for every thread
        StatsProcessNode *process = hdr->process;
        while (process) {
            StatsThreadNode *thread = process->thread;
            while (thread) {
                StatsProfileNode *profile = thread->profile;
                for (i = 0; i < hdrStats->maxProfileBuckets; i++) {
                    StatsManagerUtil::accumulateProfile(&profiles[i],
                                                        &profile->profileStats);
                    names[i] = STATS_GET_NAME(&profile->profileStats);
                    descs[i] = STATS_GET_DESCRIPTION(&profile->profileStats);
                    profile = profile->next;
                }
                thread = thread->next;
            }
            process = process->next;
        }

        PRInt64 tr = profiles[STATS_PROFILE_ALL].countRequests;
        PRInt64 tc = profiles[STATS_PROFILE_ALL].countCalls;
        float td = (float)(PRInt64)profiles[STATS_PROFILE_ALL].ticksDispatch;
        float tf = (float)(PRInt64)profiles[STATS_PROFILE_ALL].ticksFunction;
        td /= hdrStats->ticksPerSecond;
        tf /= hdrStats->ticksPerSecond;
        float tt = td + tf;

        PR_fprintf(fd, 
                   "\nPerformance Counters:\n"
                   "------------------------------------------------\n"
                   "                           Average         Total      Percent\n\n"
                   "Total number of requests:             %10llu\n"
                   "Request processing time:   %7.4f  %12.4f\n",
                   tr,
                   (tr > 0) ? (tt / (float)tr) : 0, tt);

        for (i = hdrStats->maxProfileBuckets-1; i >= STATS_PROFILE_DEFAULT; i--) {
            PRInt64 r = profiles[i].countRequests;
            PRInt64 c = profiles[i].countCalls;
            const char *name = names[i];

            // Don't display cache-bucket if it's empty.  This is a predefined
            // bucket that stored NSAPI accelerator cache statistics in
            // previous versions of the server.
            if (!c && i == STATS_PROFILE_CACHE && !strcmp(name, "cache-bucket")) {
                continue;
            }

            float d = (float)(PRInt64)profiles[i].ticksDispatch;
            float f = (float)(PRInt64)profiles[i].ticksFunction;
            d /= hdrStats->ticksPerSecond;
            f /= hdrStats->ticksPerSecond;
            float t = d + f;

            PR_fprintf(fd, 
                       "\n%s (%s)\n"
                       "Number of Requests:                 %12llu    (%6.2f%%)\n"
                       "Number of Invocations:              %12llu    (%6.2f%%)\n"
                       "Latency:                   %7.4f  %12.4f    (%6.2f%%)\n"
                       "Function Processing Time:  %7.4f  %12.4f    (%6.2f%%)\n"
                       "Total Response Time:       %7.4f  %12.4f    (%6.2f%%)\n",
                       name, SAFESTRING(descs[i]),
                       r, (tr > 0) ? (100.0 * r / (PRFloat64) tr) : 0,
                       c, (tc > 0) ? (100.0 * c / (PRFloat64) tc) : 0,
                       (r > 0)? (d / r): 0, d, (tt > 0)? (100 * d / tt): 0,
                       (r > 0)? (f / r): 0, f, (tt > 0)? (100 * f / tt): 0,
                       (r > 0)? (t / r): 0, t, (tt > 0)? (100 * t / tt): 0);
        }

        delete[] profiles;
        delete[] names;
        delete[] descs;

    } else {
        PR_fprintf(fd, "\nPerformance Statistics disabled\n");
    }

    // Session info
    {
        StatsProcessNode *process;
        int x;
        int y;

        // Count the number of rows in our session table
        int nr = 0;
        for (process = hdr->process; process; process = process->next) {
            StatsThreadNode *thread;
            for (thread = process->thread; thread; thread = thread->next)
                nr++;
        }

        SessionRow *rows = new SessionRow[nr];

        PRTime now = PR_Now();

        // Collect stats from each session
        y = 0;
        for (process = hdr->process; process; process = process->next) {
            StatsThreadNode *thread;
            for (thread = process->thread; thread; thread = thread->next) {
                thread->lock();

                PRUint32 mode = thread->threadStats.mode;
                if (mode != STATS_THREAD_EMPTY && mode != STATS_THREAD_IDLE &&
                    mode != STATS_THREAD_KEEPALIVE) {
                    rows[y].columns[SESSION_COLUMN_PROCESS].printf("%u", process->procStats.pid);
                    rows[y].columns[SESSION_COLUMN_STATUS] = StatsManager::getMode(&thread->threadStats);

                    char addr[NET_ADDR_STRING_SIZE];
                    net_addr_to_string(&thread->threadStats.addressClient, addr, sizeof(addr));
                    rows[y].columns[SESSION_COLUMN_CLIENT] = addr;

                    rows[y].timeRequestStarted = thread->threadStats.timeRequestStarted;
                    if (rows[y].timeRequestStarted != 0) {
                        PRTime microseconds = now - rows[y].timeRequestStarted;
                        int seconds = (microseconds + PR_USEC_PER_SEC/2) / PR_USEC_PER_SEC;
                        rows[y].columns[SESSION_COLUMN_AGE].printf("%d", seconds);
                    }

                    rows[y].columns[SESSION_COLUMN_VS] = thread->threadStats.vsId;
                    rows[y].columns[SESSION_COLUMN_METHOD] = thread->threadStats.requestBucket.method;
                    rows[y].columns[SESSION_COLUMN_URI] = thread->threadStats.requestBucket.uri;
                    rows[y].columns[SESSION_COLUMN_FUNCTION] = STATS_GET_FUNCTION_NAME(&thread->threadStats);

                    y++;
                }

                thread->unlock();
            }
        }

        // We'll only display stats for active sessions
        PR_ASSERT(y <= nr);
        nr = y;

        // Each column is at least as wide as its label
        int widths[SESSION_COLUMN_COUNT];
        for (x = 0; x < SESSION_COLUMN_COUNT; x++)
            widths[x] = session_column_labels[x].length();

        // Ensure each column is wide enough to fit its largest value
        for (y = 0; y < nr; y++) {
            for (x = 0; x < SESSION_COLUMN_COUNT; x++) {
                if (widths[x] < rows[y].columns[x].length())
                    widths[x] = rows[y].columns[x].length();
            }
        }

        // Leave at least 2 spaces between columns
        for (x = 0; x < SESSION_COLUMN_COUNT - 1; x++)
            widths[x] += 2;

        // Calculate total line width and individual column offsets
        int width = 0;
        int offsets[SESSION_COLUMN_COUNT];
        for (x = 0 ; x < SESSION_COLUMN_COUNT; x++) {
            offsets[x] = width;
            width += widths[x];
        }

        char *line = (char *) malloc(width + sizeof('\n'));

        // Output table heading and column labels
        PR_fprintf(fd, "\nSessions:\n");
        memset(line, '-', width);
        line[width] = '\n';
        PR_Write(fd, line, width + sizeof('\n'));
        PR_Write(fd, line, format_session(line, offsets, session_column_labels));
        PR_Write(fd, "\n", 1);

        // Sort rows by age
        const SessionRow **sorted = new const SessionRow *[nr];
        for (y = 0; y < nr; y++)
            sorted[y] = &rows[y];
        qsort(sorted, nr, sizeof(sorted[0]), &session_cmp);

        // Output individual rows
        for (y = 0; y < nr; y++)
            PR_Write(fd, line, format_session(line, offsets, sorted[y]->columns));

        delete [] sorted;

        free(line);

        delete [] rows;
    }

    StatsManager::unlockStatsData();
    return REQ_PROCEED;
}
