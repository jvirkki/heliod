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

#include <nspr.h>
#include <private/pprio.h>

#include "netsite.h"
#include "base/ereport.h"
#include "base/util.h"
#include "base/shmem.h"
#include "base/systhr.h"
#include "base/loadavg.h"
#include "frame/conf.h"
#include "frame/func.h"
#include "frame/object.h"
#include "frame/accel.h"
#include "time/nstime.h"
#include "httpdaemon/vsmanager.h"
#include "httpdaemon/statsmanager.h"
#include "httpdaemon/libdaemon.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/internalstats.h"
#include "httpdaemon/WebServer.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/vsconf.h" // VirtualServer

#ifdef SOLARIS
#include "httpdaemon/solariskernelstats.h"
#include "httpdaemon/solarisprocessstats.h"
#endif


/* also change lib/safs/perf.cpp for process specific stats*/
#ifdef Linux
#include "httpdaemon/linuxkernelstats.h"
#include "httpdaemon/linuxprocessstats.h"
#endif

#ifdef HPUX
#include "httpdaemon/hpuxkernelstats.h"
#include "httpdaemon/hpuxprocessstats.h"
#endif

#ifdef XP_WIN32
#include "uniquename.h"
#include "httpdaemon/NTStatsServer.h"
#else
#include "base/unix_utils.h"
#include "httpdaemon/ParentStats.h"
#endif
#include "safs/nsfcsafs.h"
#include "ares/arapi.h"
#include "httpdaemon/statsavg.h" // StatsMovingAverageHandler
#include "httpdaemon/StatsMsgPreparer.h" // StatsMsgPreparer
#include "httpdaemon/javastatsmanageritf.h"


// Layout of stats file:
//
// Offset                  Description
// ----------------------- ----------------------------------------------------
// 0                       Header
// sizeof(StatsHeader)     Private
// offsetProcessSlot       [ Process [ MiscBuckets Thread Request [Profile] ] ]
// offsetVirtualServerSlot [ VirtualServer Request [Profile] ] 
// offsetCpuInfoSlot       [CpuInfo] 
//
// To add a new statistics bucket:
// 1) define the bucket's structure in iwsstats.h
// 2) declare an offset to that bucket from an existing bucket in iwsstats.h
// 3) define an accessor macro in iwsstats.h
// 4) increment STATS_VERSION_MINOR in iwsstats.h
// 5) adjust the "size" variables set during initEarly
// 6) initialize the offset(s) to your bucket during initEarly (make sure you
//    don't break any other offsets)
// 7) fill your bucket

//-----------------------------------------------------------------------------
// Imported variables not in a header file (sigh)
//-----------------------------------------------------------------------------

NSAPI_PUBLIC extern PoolElem poolTable[FUNC_MAXPOOLS + 1];

//-----------------------------------------------------------------------------
// StringStore::StringStore
//-----------------------------------------------------------------------------

StringStore::StringStore()
: buffer(0), size(0), capacity(0), flagFrozen(PR_FALSE)
{
}

StringStore::StringStore(const void* inputBuffer, int sizeInput)
: buffer(0), size(sizeInput), capacity(sizeInput), flagFrozen(PR_TRUE)
{
    if (size > 0) {
        buffer = (char*) malloc(size);
        // Use memcpy instead of string copy as the buffer is collection of strings.
        memcpy(buffer, inputBuffer, size);
    }
    // This string store object is now frozen.
}

//-----------------------------------------------------------------------------
// StringStore::~StringStore
//-----------------------------------------------------------------------------

StringStore::~StringStore()
{
    if (buffer)
        free(buffer);
    buffer = 0;
}

//-----------------------------------------------------------------------------
// StringStore::findString
//-----------------------------------------------------------------------------

ptrdiff_t StringStore::findString(const char* string) const
{
    int i;
    for (i = 1; i < size - 1; i++) {
        // Does this string match?
        if (!strcmp(&buffer[i], string)) {
            return i;
        }

        // Advance to the next string
        while (buffer[i]) {
            i++;
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
// StringStore::addString
//-----------------------------------------------------------------------------

ptrdiff_t StringStore::addString(const char* string)
{
    PR_ASSERT(!flagFrozen);

    ptrdiff_t offset;

    // Check for existing string
    offset = findString(string);
    if (offset) {
        return offset;
    }

    if (!flagFrozen) {
        // Start buffer with a character so that no string has an offset of 0
        if (!size) {
            require(1);
            buffer[size++] = 0;
        }

        // Append the new string
        int len = strlen(string) + 1;
        if (len > 1) {
            require(len);
            memcpy(&buffer[size], string, len);
            offset = size;
            size += len;
        }
    }

    return offset;
}

//-----------------------------------------------------------------------------
// StringStore::require
//-----------------------------------------------------------------------------

void StringStore::require(int length)
{
    if (capacity - size < length) {
        if (length < 1024) {
            length = 1024;
        }
        capacity += length;
        buffer = (char*)realloc(buffer, capacity);
    }
}

//-----------------------------------------------------------------------------
// ProfileBucketStrings
//-----------------------------------------------------------------------------

struct ProfileBucketStrings {
    ProfileBucketStrings(StringStore &strings, const char *nameNew, const char* descriptionNew)
    {
        name = strings.addString(nameNew);
        description = strings.addString(descriptionNew);
    }

    ptrdiff_t name;
    ptrdiff_t description;
};

//-----------------------------------------------------------------------------
// StatsPrivate
//
// Contents of this structure are version specific.  Only the server itself
// should see these declarations.
//-----------------------------------------------------------------------------

struct StatsPrivate {
    // StatsVirtualServerSlot information
    PRInt32 offsetEmptyVSChain;
    PRUint32 countEmptyVS;

    // Miscellaneous runtime-configured strings from the StringStore
    char strings[1];
    // Do not add members here, strings is variable length
};

//-----------------------------------------------------------------------------
// StatsManagerVSListener
//-----------------------------------------------------------------------------

class StatsManagerVSListener : public VSListener {
public:
    PRStatus initVS(VirtualServer* incoming, const VirtualServer* current)
    {
        StatsManager::initVS(incoming);
        return PR_SUCCESS;
    }
};

//-----------------------------------------------------------------------------
// StatsRunningThread
//-----------------------------------------------------------------------------

StatsRunningThread::StatsRunningThread()
: Thread("StatsRunningThread")
{
    if (start(PR_SYSTEM_THREAD, PR_UNJOINABLE_THREAD) == PR_FAILURE)
    {
        PRInt32 err = PR_GetError();
        PRInt32 oserr = PR_GetOSError();
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_StatsManager_FailedToCreateThread), system_errmsg());
    }
}

void StatsRunningThread::run()
{
    for (;;) {
        systhread_sleep(1000);
        StatsManager::updateSecondsRunning();
    }
}

//-----------------------------------------------------------------------------
// StatsPollThread
//-----------------------------------------------------------------------------

class StatsPollThread : public Thread {
public:
    StatsPollThread()
    : Thread("StatsPollThread")
    {
        StatsManager::poll();

        // This thread must be a global thread. In windows, NSPR creates a
        // fibre instead of native thread if we specify as local thread. As
        // this thread enters into java land which can make any native calls
        // which causes issues (observed on stress tests).
        if (start(PR_SYSTEM_THREAD, // thread type.
                  PR_UNJOINABLE_THREAD, // main does not join this thread.
                  0, // stack size (default)
                  PR_PRIORITY_NORMAL, // priority
                  PR_GLOBAL_THREAD // global thread.
                  ) == PR_FAILURE)
        {
            PRInt32 err = PR_GetError();
            PRInt32 oserr = PR_GetOSError();
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_StatsManager_FailedToCreateThread), system_errmsg());
        }
    }

private:
    void run(void)
    {
        int ms = PR_IntervalToMilliseconds(StatsManager::ticksUpdateInterval);
        for (;;) {
            systhread_sleep(ms);
            StatsManager::poll();
        }
    }
};

//-----------------------------------------------------------------------------
// StatsManager static members
//-----------------------------------------------------------------------------

PRBool StatsManager::flagEnabled = PR_FALSE;
PRBool StatsManager::flagInitialized = PR_FALSE;
PRBool StatsManager::flagLocked = PR_FALSE;
PRLock* StatsManager::lockPerProcess = 0;
int StatsManager::sizePerProcess = 0;
int StatsManager::sizePerVirtualServer = 0;
int StatsManager::countListenSlots = 200;
int StatsManager::countProfileBuckets = 0;
#ifdef PLATFORM_SPECIFIC_STATS_ON
int StatsManager::sizePerCpuInfo = 0;
int StatsManager::countCpuInfoSlots = 1;
#endif
shmem_s* StatsManager::shmem = 0;
StatsHeaderNode* StatsManager::hdr = 0;
StatsPrivate* StatsManager::prv = 0;
const char* StatsManager::stringbase = 0;
int StatsManager::slotVSPrivateData = -1;
StatsProcessNode* StatsManager::procCurrent = 0;
int StatsManager::sizeMethod = 16;
int StatsManager::sizeUri = 128;
PRBool StatsManager::flagChildInitialized_ = PR_FALSE;
PRBool StatsManager::flagServerReconfigured = PR_FALSE;
PRBool StatsManager::flagNotification[STATS_NOTICE_FLAG_LAST];
PRIntervalTime StatsManager::ticksUpdateInterval = PR_SecondsToInterval(5);
int StatsManager::countKeepaliveHits = 0;
#ifdef SOLARIS
SolarisKernelStats* StatsManager::kernelStats=0;
SolarisProcessStats* StatsManager::processStats=0;
#endif
#ifdef Linux
LinuxKernelStats* StatsManager::kernelStats=0;
LinuxProcessStats* StatsManager::processStats=0;
#endif
#ifdef HPUX
HpuxKernelStats* StatsManager::kernelStats=0;
HpuxProcessStats* StatsManager::processStats=0;
#endif
#ifdef XP_WIN32
NTStatsServer* ntStatsServer = 0;
#endif
StatsMovingAverageHandler* StatsManager::statsAvgHandler = 0;
StatsAccumulatedVSSlot* StatsManager::accumulatedVSStats = 0;
StatsServerMessage* StatsManager::statsMsgChannel = 0;
PRLock* StatsManager::lockStatsChannel = 0;
JavaStatsManagerItf* StatsManager::javaStatsManager = 0;

static StringStore strings;
static GenericVector profiles;

//-----------------------------------------------------------------------------
// StatsManager::enable
//-----------------------------------------------------------------------------
void StatsManager::enable()
{
    if (!flagEnabled) {
        flagEnabled = PR_TRUE;
    }
}

//-----------------------------------------------------------------------------
// StatsManager::enableProfiling
//-----------------------------------------------------------------------------

void StatsManager::enableProfiling()
{
    // This should be called from an EarlyInit Init SAF

    enable();

    if (!profiles.length()) {
        // STATS_PROFILE_ALL, STATS_PROFILE_DEFAULT, and STATS_PROFILE_CACHE
        profiles.append(new ProfileBucketStrings(strings, "all-requests", "All requests"));
        profiles.append(new ProfileBucketStrings(strings, "default-bucket", "Default bucket"));
        profiles.append(new ProfileBucketStrings(strings, "cache-bucket", "Cached responses"));
    }
}

//-----------------------------------------------------------------------------
// StatsManager::setMaxListenSlots
//-----------------------------------------------------------------------------

void StatsManager::setMaxListenSlots(int maxListenSlots)
{
    PR_ASSERT(!flagInitialized);
    if (!flagInitialized) {
        countListenSlots = maxListenSlots;
    }
}

#ifdef PLATFORM_SPECIFIC_STATS_ON
//-----------------------------------------------------------------------------
// StatsManager::setMaxCpuInfoSlots
//-----------------------------------------------------------------------------

void StatsManager::setMaxCpuInfoSlots(int maxCpuInfo)
{
    PR_ASSERT(!flagInitialized);
    if (!flagInitialized) {
        countCpuInfoSlots = maxCpuInfo;
    }
}
#endif

//-----------------------------------------------------------------------------
// StatsManager::setUpdateInterval
//-----------------------------------------------------------------------------

void StatsManager::setUpdateInterval(int secondsUpdateInterval)
{
    PR_ASSERT(!flagInitialized);
    if (!flagInitialized) {
        ticksUpdateInterval = PR_SecondsToInterval(secondsUpdateInterval);
    }
}

//-----------------------------------------------------------------------------
// StatsManager::initEarly
//-----------------------------------------------------------------------------

int StatsManager::initEarly(int maxProcs, int maxThreads)
{
    // This should be called from the primordial process prior to the first fork

    if (!flagEnabled) return 0; // Success
    PR_ASSERT(!flagInitialized);

    // Per-process lock to serialize allocation of thread slots, etc.
    lockPerProcess = PR_NewLock();
    lockStatsChannel = 0;
    hdr = new StatsHeaderNode;

#ifdef PLATFORM_SPECIFIC_STATS_ON

    // Initialize the system statistics
    initSystemStats();

    // get the no of CPUs and set the Max CPU Slots
#ifdef SOLARIS
    // for Solaris only
    StatsManager::setMaxCpuInfoSlots(sysconf(_SC_NPROCESSORS_CONF));
#endif
#ifdef Linux
     // we use the linux specific code to get the number of processors
     // the init function would have already been called
    StatsManager::setMaxCpuInfoSlots( kernelStats->getNumberOfCpus() );
#endif
#ifdef HPUX
     // we use the HPUX specific code to get the number of processors
     // the init function would have already been called
    StatsManager::setMaxCpuInfoSlots( kernelStats->getNumberOfCpus() );
#endif
#endif

    // Store thread pool names in the StringStore
    int i;
    for (i = 0; i < FUNC_MAXPOOLS; i++) {
        if (poolTable[i].name) {
            strings.addString(poolTable[i].name);
        }
    }

    // Useful strings
    strings.addString("unknown");

    // No more profile bucket entries can be added after this point
    if (maxThreads)
        countProfileBuckets = profiles.length();

    // Bytes in the private area (including strings from the StringStore).  We
    // can't add anything to the StringStore after this point.
    strings.freeze();
    stringbase = strings.getBuffer();

    // Bytes reserved per request bucket for the most recent method and URI
    PR_ASSERT(sizeMethod == STATS_ALIGN(sizeMethod));
    PR_ASSERT(sizeUri == STATS_ALIGN(sizeUri));

    // Setup the header
    util_snprintf(hdr->hdrStats.versionServer, 
                  sizeof(hdr->hdrStats.versionServer), "%s", 
                  PRODUCT_ID" "PRODUCT_FULL_VERSION_ID" B"BUILD_NUM
                  " ("BUILD_PLATFORM" "BUILD_SECURITY")");
    hdr->hdrStats.timeStarted = PR_Now();
    hdr->hdrStats.ticksPerSecond = PR_TicksPerSecond();
    hdr->hdrStats.maxProcs = maxProcs;
    hdr->hdrStats.maxThreads = maxThreads;
    hdr->hdrStats.maxProfileBuckets = countProfileBuckets;
    hdr->hdrStats.versionStatsMajor = STATS_VERSION_MAJOR;
    hdr->hdrStats.versionStatsMinor = STATS_VERSION_MINOR;
    hdr->hdrStats.flagProfilingEnabled = (countProfileBuckets > 0);
    hdr->hdrStats.secondsUpdateInterval =
                                     PR_IntervalToSeconds(ticksUpdateInterval);
    hdr->hdrStats.jvmEnabled = STATS_STATUS_NOT_ENABLED;
    hdr->process = 0;
    hdr->vss = 0;

    accumulatedVSStats = new StatsAccumulatedVSSlot;
    memset(accumulatedVSStats, 0, sizeof(StatsAccumulatedVSSlot));

    // Kick off a thread to poll per-process statistics
    procCurrent = new StatsProcessNode;
    hdr->process = procCurrent;


    int countProcs = maxProcs;
    StatsProcessNode* proc = hdr->process;
    if (proc) {
        // Setup StatsConnectionQueueSlot
        // XXX right now there's only one connection queue per process.  Given
        // that having multiple connection queues can reduce mutex contention,
        // this may change.
        proc->connQueue = new StatsConnectionQueueNode;
        strcpy(proc->connQueue->connQueueStats.id, "cq1");
        proc->procStats.countConnectionQueues = 1;

        // For every StatsThreadPoolNode for this process...
        for (i = 0; poolTable[i].name; i++) {
            StatsThreadPoolNode* thrPoolNode = new StatsThreadPoolNode;
            thrPoolNode->threadPoolStats.offsetName =
                        strings.findString(poolTable[i].name);
            statsAppendLast(proc, thrPoolNode);
        }
        proc->procStats.countThreadPools = i;
    }

#ifdef PLATFORM_SPECIFIC_STATS_ON
    // Allocate all StatsCpuInfoNode
    for (i = 0; i < countCpuInfoSlots; i++) {
        StatsCpuInfoNode* cps = new StatsCpuInfoNode;
        // Set the default CPU id, an index starting with 1.
        util_snprintf(cps->cpuInfo.id, (sizeof(cps->cpuInfo.id) -1), "%d", i+1);
        statsAppendLast(hdr, cps);
    }
#endif


    // Setting the magic is the last thing we do as the presence of a correct
    // magic is used to indicate an initialized stats table
    memcpy(hdr->hdrStats.magic, STATS_MAGIC, sizeof(hdr->hdrStats.magic));

    flagInitialized = PR_TRUE;

    ereport(LOG_VERBOSE, "Collecting statistics for up to %d processes with %d threads, %d listen sockets", 
            hdr->hdrStats.maxProcs, 
            hdr->hdrStats.maxThreads, 
            countListenSlots);

    return 0; // Success
}

//-----------------------------------------------------------------------------
// StatsManager::terminateProcess
//-----------------------------------------------------------------------------

void StatsManager::terminateProcess()
{
    // Keep out StatsRunningThread, etc. during shutdown
    if (statsMsgChannel) {
        PR_Lock(lockStatsChannel);
        delete statsMsgChannel ;
        statsMsgChannel = 0;
        PR_Unlock(lockStatsChannel);
    }
    if (lockPerProcess)
        lockStatsData();
}

//-----------------------------------------------------------------------------
// StatsManager::terminateServer
//-----------------------------------------------------------------------------

void StatsManager::terminateServer()
{
    if (hdr) memset(hdr->hdrStats.magic, 0, sizeof(hdr->hdrStats.magic));
    if (shmem) shmem_free(shmem);
#ifdef XP_WIN32
    if (ntStatsServer) {
        ntStatsServer->terminate();
        delete ntStatsServer;
        ntStatsServer = 0;
    }
#endif
}

//-----------------------------------------------------------------------------
// StatsManager::initLate
//-----------------------------------------------------------------------------

void StatsManager::initLate()
{
    // This should be called by a child process near LateInit time

    if (!flagInitialized) return;

    // We don't need per process lock as there is no memory which is shared between
    // different child or primordial process. But we need the lock to protect StatsManager
    // data member from the multiple threads simulataneously.
    // So free the old lock and create new one.
    // It should be already locked
    PR_ASSERT(flagLocked == PR_FALSE);
    PR_DestroyLock(lockPerProcess);
    lockPerProcess = PR_NewLock();

    lockStatsChannel = PR_NewLock();

#ifdef PLATFORM_SPECIFIC_STATS_ON

    // Initialize platform specific statistics
    initPlatformSpecificStats();

#endif

    // Allocate some VirtualServer*-specific data.  We'll use this slot to 
    // cache StatsVirtualServerNode*s on a per-VS basis.
    slotVSPrivateData = VSManager::allocSlot();
    statsAvgHandler = new StatsMovingAverageHandler(
                                hdr->hdrStats.secondsUpdateInterval);


    // Let us know when a VS is initialized so we can diddle its objset
    // XXX StatsManagerVSListener() is never delete'd
    VSManager::addListener(new StatsManagerVSListener());
    PR_ASSERT(procCurrent);

    setProcessNode(procCurrent);
    // System depeendent code
#ifdef XP_UNIX
    // Here the child will recover the previously died Virtual Server
    // data. If it is created first time then there is no effect.
    StatsBackupManager::processChildLateInit();
    activateProcessSlot(getpid());
#endif

    //runSelfDiagnostics();

}

//-----------------------------------------------------------------------------
// StatsManager::startPollThread
//-----------------------------------------------------------------------------

void StatsManager::startPollThread()
{
    if (flagInitialized)
        new StatsPollThread();
}

//-----------------------------------------------------------------------------
// StatsManager::initProfileBuckets
//-----------------------------------------------------------------------------

void StatsManager::initProfileBuckets(StatsProfileNode* profile)
{
    // For every StatsProfileNode...
    int i;
    for (i = 0; profile && (i < countProfileBuckets); i++) {
        memset(profile, 0, sizeof(StatsProfileNode));
        // Setup StatsProfileNode offsets
        profile->next = (i < (countProfileBuckets -1)) ? 
                                        (profile + 1) 
                                        : NULL;
        profile->profileStats.offsetName = ((ProfileBucketStrings*)
                                           profiles[i])->name;
        profile->profileStats.offsetDescription = ((ProfileBucketStrings*)
                                                  profiles[i])->description;
        profile = profile->next;
    }
}

#ifdef PLATFORM_SPECIFIC_STATS_ON
//-----------------------------------------------------------------------------
// StatsManager::initSystemStats
//-----------------------------------------------------------------------------

void StatsManager::initSystemStats()
{
#ifdef SOLARIS
    kernelStats = new SolarisKernelStats();
#endif
#ifdef Linux
    kernelStats = new LinuxKernelStats();
#endif
#ifdef HPUX
    kernelStats = new HpuxKernelStats();
#endif
    if (kernelStats->init() != PR_SUCCESS) {
        delete kernelStats;
        kernelStats = 0;
    }

}

//-----------------------------------------------------------------------------
// StatsManager::initPlatformSpecificStats
//-----------------------------------------------------------------------------

void StatsManager::initPlatformSpecificStats()
{
#ifdef SOLARIS
    processStats = new SolarisProcessStats();
#endif
#ifdef Linux
    processStats = new LinuxProcessStats();
#endif
#ifdef HPUX
    processStats = new HpuxProcessStats();
#endif
    if (processStats->init() != PR_SUCCESS) {
        delete processStats;
        processStats = 0;
    }
}
#endif

//-----------------------------------------------------------------------------
// StatsManager::getFunctionName
//-----------------------------------------------------------------------------

ptrdiff_t StatsManager::getFunctionName(const char *name)
{
    if (flagInitialized) {
        // Too late, we've already frozen the StringStore
        return strings.findString("unknown");
    }

    return strings.addString(name);
}

//-----------------------------------------------------------------------------
// StatsManager::addProfileBucket
//-----------------------------------------------------------------------------

void StatsManager::addProfileBucket(const char* name, const char* description)
{
    // This should be called from EarlyInit Init SAFs

    if (flagInitialized) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_StatsManager_TooLateToAdd), name);
        return;
    }

    // This vector is processed during StatsManager::initEarly processing
    profiles.append(new ProfileBucketStrings(strings, name, description));
}

//-----------------------------------------------------------------------------
// StatsManager::findProcessSlot
//-----------------------------------------------------------------------------

StatsProcessNode* StatsManager::findProcessSlot()
{
    if (!flagInitialized) return 0;

    PR_ASSERT(procCurrent != NULL);

    // At this point, the caller (the primordial process) will fork off a 
    // child.  If the fork is successful, the primordial process will call
    // activateProcessSlot().

    return procCurrent;
}

//-----------------------------------------------------------------------------
// StatsManager::activateProcessSlot
//-----------------------------------------------------------------------------

void StatsManager::activateProcessSlot(PRInt32 pid)
{
    if (!flagInitialized) return;

    procCurrent->procStats.pid = pid;
    procCurrent->procStats.timeStarted = PR_Now();
    procCurrent->procStats.mode = STATS_PROCESS_ACTIVE;
}

//-----------------------------------------------------------------------------
// StatsManager::freeProcessSlot
//-----------------------------------------------------------------------------

void StatsManager::freeProcessSlot(PRInt32 pid)
{
    if (!flagInitialized) return;

    // This should only be called from the primordial process following child
    // death
    hdr->hdrStats.countChildDied++;
}

//-----------------------------------------------------------------------------
// StatsManager::allocThreadSlot
//-----------------------------------------------------------------------------

StatsThreadNode* StatsManager::allocThreadSlot(const char* threadQName)
{
    if (!flagInitialized) return 0;

    // This should only be called from a child process
    const char* connQueueName = threadQName;
    if (connQueueName == NULL) {
        connQueueName = "cq1";
    }

    lockStatsData();

    // Find an empty thread slot
    StatsThreadNode* threadNode = findThreadSlot(procCurrent, STATS_THREAD_EMPTY);
    if (threadNode) {
        StatsThreadSlot* thread = &threadNode->threadStats;
        thread->timeStarted = PR_Now();
        thread->mode = STATS_THREAD_IDLE;
        strcpy(thread->vsId, "");
    }
    else {
        threadNode = new StatsThreadNode(countProfileBuckets);
        initProfileBuckets(threadNode->profile);
        // Add the new thread slot to the list
        statsAppendLast(procCurrent, threadNode);
    }
    const int sizeConnQueue = sizeof(threadNode->threadStats.connQueueId);
    // Make sure connQueueId is null terminated.
    char* connQueueId = threadNode->threadStats.connQueueId;
    connQueueId[sizeConnQueue - 1] = 0;
    strncpy(connQueueId, connQueueName, sizeConnQueue - 1);

    unlockStatsData();

    return threadNode;
}

//-----------------------------------------------------------------------------
// StatsManager::recordKeepaliveHits
//-----------------------------------------------------------------------------

void StatsManager::recordKeepaliveHits(int countKeepaliveHits_)
{
    countKeepaliveHits += countKeepaliveHits_;
}

//-----------------------------------------------------------------------------
// StatsManager::lockStatsData
//-----------------------------------------------------------------------------

void StatsManager::lockStatsData()
{
    if (!flagInitialized) return;

    // Set flagLocked to warn other threads not to try to lock
    flagLocked = PR_TRUE;
    PR_Lock(lockPerProcess);
    flagLocked = PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManager::unlockStatsData
//-----------------------------------------------------------------------------

void StatsManager::unlockStatsData()
{
    if (!flagInitialized) return;
    PR_ASSERT(flagLocked);
    flagLocked = PR_FALSE;

    PR_Unlock(lockPerProcess);
}

//-----------------------------------------------------------------------------
// StatsManager::allocListenSlot
//-----------------------------------------------------------------------------

StatsListenNode* StatsManager::allocListenSlot(const char* id)
{
    if (!flagInitialized) return 0;

    if (!id) return 0;
    StatsManager::lockStatsData();
    StatsListenNode* ls = StatsManagerUtil::allocListenSlot(id, procCurrent);
    StatsManager::unlockStatsData();
    return ls;
}

//-----------------------------------------------------------------------------
// StatsManager::freeListenSlot
//-----------------------------------------------------------------------------

void StatsManager::freeListenSlot(const char* id)
{
    if (!flagInitialized) return;

    lockStatsData();
    statsFreeNode<StatsListenNode>(procCurrent, id);

    unlockStatsData();
}

//-----------------------------------------------------------------------------
// computeStringHash
//-----------------------------------------------------------------------------

static PRUint32 computeStringHash(const char* string)
{
    PRUint32 hash = 0;

    while (*string) {
        hash ^= ((hash << 5) ^ (*string) ^ (hash >> 27));
        string++;
    }

    return hash;
}

//-----------------------------------------------------------------------------
// StatsManager::getVirtualServerSlot
//-----------------------------------------------------------------------------

StatsVirtualServerNode* StatsManager::getVirtualServerSlot(const VirtualServer* vs)
{
    if (!flagInitialized) return 0;

    if (!vs) return 0;
    StatsVirtualServerNode* vss = 0;

    // Fast path if we've already assigned a slot to this VS
    vss = (StatsVirtualServerNode*) vs->getUserData(slotVSPrivateData);
    if (vss) {
        return vss;
    }

    // vss was not found in private cached slot so search in list.
    lockStatsData();

    vss = findVirtualServerSlot(vs->name);
    if (! vss) {
        // Allocate a new Virtual server slot
        vss = new StatsVirtualServerNode(vs, countProfileBuckets);
        initProfileBuckets(vss->profile);
        statsAppendLast(hdr, vss);
    }

    // Store a reference to vss in vs so we can find it quickly next time
    vs->setUserData(slotVSPrivateData, vss);

    unlockStatsData();

    return vss;
}

//-----------------------------------------------------------------------------
// StatsManager::initVS
//-----------------------------------------------------------------------------

void StatsManager::initVS(VirtualServer* vs)
{
    httpd_objset* os = vs->getObjset();

    // Profile bucket name to index fixups
    int obj_x;
    for (obj_x = os->pos-1; obj_x != -1; obj_x--) {
        int dt_x;
        for (dt_x = 0; dt_x < os->obj[obj_x]->nd; dt_x++) {
            int dir_x;
            for (dir_x = 0; dir_x < os->obj[obj_x]->dt[dt_x].ni; dir_x++) {
	        char *bucket = pblock_findval("bucket", os->obj[obj_x]->dt[dt_x].inst[dir_x].param.pb);
                if (bucket) {
                    int x;
		    for (x = STATS_PROFILE_DEFAULT; x < countProfileBuckets; x++) {
                        const char* name = getNameFromStringStore(((ProfileBucketStrings*)profiles[x])->name);
                        if (!strcmp(name, bucket)) {
                            os->obj[obj_x]->dt[dt_x].inst[dir_x].bucket = x;
                        }
		    }
                }
            }
        }
    }

    // (Try to) allocate a statistics slot for this VS
    getVirtualServerSlot(vs);
}

void StatsManager::setupVirtualServerSlot(StatsVirtualServerNode* vss)
{
    // This must be only called by Primordial process
    hdr->vss = vss;
}

//-----------------------------------------------------------------------------
// StatsManager::setProcessNode
//-----------------------------------------------------------------------------

void StatsManager::setProcessNode(StatsProcessNode* procNode)
{
    hdr->process = procNode;
}

//-----------------------------------------------------------------------------
// StatsManager::updateSecondsRunning
//-----------------------------------------------------------------------------

void StatsManager::updateSecondsRunning()
{
    if (!flagInitialized) return;

    static PRUint32 secondsMax;
    static PRIntervalTime ticksEpoch;
    static PRUint32 secondsOverflow;

    // Initialization
    if (!secondsMax) {
        // secondsMax is the number of seconds needed to overflow a 
        // PRIntervalTime (42950 on Solaris)
        secondsMax = 2 * ((0x80000000UL + PR_SecondsToInterval(1)) / PR_SecondsToInterval(1));
        ticksEpoch = PR_IntervalNow();
    }

    lockStatsData();

    PRIntervalTime ticksNow = PR_IntervalNow();

    // Accumulate secondsRunning.  The complexity is to remove the 
    // systematic drift found in a loaded server while allowing 
    // secondsRunning to grow beyond what would fit in PRIntervalTime.
    PRUint32 secondsRunning = PR_IntervalToSeconds(ticksNow - ticksEpoch);
    if ((secondsRunning + secondsOverflow) < hdr->hdrStats.secondsRunning) {
        secondsOverflow += secondsMax;
    }
    secondsRunning += secondsOverflow;

    hdr->hdrStats.secondsRunning = secondsRunning;

    unlockStatsData();
}

//-----------------------------------------------------------------------------
// StatsManager::getMode
//-----------------------------------------------------------------------------

const char* StatsManager::getMode(const StatsVirtualServerNode* vss)
{
    switch (vss->vssStats.mode) {
    case STATS_VIRTUALSERVER_ACTIVE: return "active";
    case STATS_VIRTUALSERVER_DISABLED: return "disabled";
    default:                         return "unknown";
    }
}

const char* StatsManager::getMode(const StatsProcessNode* process)
{
    switch (process->procStats.mode) {
    case STATS_PROCESS_ACTIVE: return "active";
    default:                   return "unknown";
    }
}

const char* StatsManager::getMode(const StatsThreadSlot* thread)
{
    switch (thread->mode) {
    case STATS_THREAD_IDLE:       return "idle";
    case STATS_THREAD_DNS:        return "DNS";
    case STATS_THREAD_REQUEST:    return "request";
    case STATS_THREAD_PROCESSING: return "processing";
    case STATS_THREAD_RESPONSE:   return "response";
    case STATS_THREAD_UPDATING:   return "updating";
    case STATS_THREAD_KEEPALIVE:  return "keep-alive";
    default:                      return "unknown";
    }
}

const char* StatsManager::getMode(const StatsWebModuleSlot* wms)
{
    switch (wms->mode) {
    case STATS_WEBMODULE_MODE_ENABLED:  return "enabled";
    case STATS_WEBMODULE_MODE_DISABLED: return "disabled";
    default:                            return "unknown";
    }
}

//-----------------------------------------------------------------------------
// StatsManager::pollStats
//-----------------------------------------------------------------------------

void StatsManager::pollStats(void)
{
    if (WebServer::isTerminating() == PR_TRUE)
    {
        // webserver is terminating so don't poll.
        return;
    }
    lockStatsData();

    PRIntervalTime captureTime = PR_IntervalNow();
    Configuration* configuration = ConfigurationManager::getConfiguration();

    // Update StatsProcessNode
    procCurrent->procStats.countConfigurations = 
        ConfigurationManager::getConfigurationCount();
    procCurrent->procStats.countConnections = Connection::countActive;
    procCurrent->procStats.peakConnections = Connection::peakActive;

#ifdef PLATFORM_SPECIFIC_STATS_ON
    // Update process memory usage stats
    if (processStats) {
        MemUsageInfo infoMemUsageInfo;
        if (processStats->getMemUsage(&infoMemUsageInfo) == PR_SUCCESS) {
            procCurrent->procStats.sizeVirtual = 
                infoMemUsageInfo.process_size_in_Kbytes;
            procCurrent->procStats.sizeResident = 
                infoMemUsageInfo.process_resident_size_in_Kbytes;
            procCurrent->procStats.fractionSystemMemoryUsage = 
                infoMemUsageInfo.fraction_system_memory_usage;
        } else {
            delete processStats;
            processStats = 0;
        }
    }
#endif

    // Update StatsThreadPoolBuckets
    StatsThreadPoolNode* poolNode = procCurrent->threadPool;
    int i;
    for (i = 0; poolNode; i++) {
        PoolElem *p = &poolTable[i];
        if (p->name == 0)
            break;

        StatsThreadPoolBucket* pool = &poolNode->threadPoolStats;
        pool->maxThreads = p->config.maxThread;
        pool->maxQueued = p->config.maxQueue;

        if (p->pool) {
            NSTPPoolStats *stats = NSTP_GetPoolStats(p->pool);
            pool->countThreads = stats->threadCount;
            pool->countThreadsIdle = stats->freeCount;
            pool->countQueued = stats->queueCount;
            pool->peakQueued = stats->maxQueue;
        }

        poolNode = poolNode->next;
    }

    // Update StatsCacheBucket
    nsfc_get_stats(&procCurrent->procStats.cacheBucket);
    accel_get_stats(&procCurrent->procStats.cacheBucket);

    // Update StatsDnsBucket
    StatsDnsBucket* dns = &procCurrent->procStats.dnsBucket;
    DNSCacheInfo infoDnsCache;
    if (GetDNSCacheInfo(&infoDnsCache)) {
        dns->flagCacheEnabled = infoDnsCache.enabled;
        dns->countCacheEntries = infoDnsCache.numCacheEntries;
        dns->maxCacheEntries = infoDnsCache.maxCacheEntries;
        dns->countCacheHits = infoDnsCache.numCacheHits;
        dns->countCacheMisses = infoDnsCache.numCacheMisses;
    }
#if !defined(XP_WIN32)
    asyncDNSInfo infoAsyncDns;
    if (GetAsyncDNSInfo(&infoAsyncDns)) {
        dns->flagAsyncEnabled = infoAsyncDns.enabled;
        dns->countAsyncNameLookups = infoAsyncDns.numNameLookups;
        dns->countAsyncAddrLookups = infoAsyncDns.numAddrLookups;
        dns->countAsyncLookupsInProgress = infoAsyncDns.numLookupsInProgress;
    }
#endif

    // Update StatsKeepaliveBucket
    StatsKeepaliveBucket* keepalive = &procCurrent->procStats.keepAliveBucket;
    keepAliveInfo infoKeepalive;
    if (DaemonSession::GetKeepAliveInfo(&infoKeepalive) == PR_SUCCESS) {
        keepalive->countConnections = infoKeepalive.numKeepAlives;
        keepalive->maxConnections = infoKeepalive.maxKeepAlives;
        keepalive->numThreads = infoKeepalive.numKeepAliveThreads;
        keepalive->countHits = countKeepaliveHits;
        keepalive->countFlushes = infoKeepalive.numKeepAliveFlushes;
        keepalive->countTimeouts = infoKeepalive.numKeepAliveTimeouts;
        keepalive->secondsTimeout = infoKeepalive.tmoKeepAlive;
        keepalive->countRefusals = infoKeepalive.numKeepAliveRefusals;
    }

    // Update StatsVirtualServerSlots
    StatsVirtualServerNode* vss = hdr->vss;
    while (vss) {
        StatsRequestBucket* request = &vss->vssStats.requestBucket;
        VirtualServer* vs = configuration ? configuration->getVS(vss->vssStats.id) : 0;
        PRUint64 rateBytesTransmitted = request->rateBytesTransmitted;
        PRUint32 countOpenConnections = request->countOpenConnections;
        if (vs) {
            if (vs->enabled) {
                vss->vssStats.mode = STATS_VIRTUALSERVER_ACTIVE;
                request->countOpenConnections = vs->getTrafficStats().getConnections();
                request->rateBytesTransmitted = vs->getTrafficStats().getBandwidth();
            } else {
                vss->vssStats.mode = STATS_VIRTUALSERVER_DISABLED;
            }
            if (vss->isNullInterfaces()) {
                vss->setInterfaces(vs);
            }
        } else {
            // There is no vs for this id so let us disable it.
            vss->vssStats.mode = STATS_VIRTUALSERVER_EMPTY;
            request->countOpenConnections = 0;
            request->rateBytesTransmitted = 0;
        }
        vss = vss->next;
    }

    PR_ASSERT(accumulatedVSStats != NULL);
    // Update accumulated VS stats.
    if (countProfileBuckets > 0) {
        StatsProfileBucket& allReqsProfile =
                    accumulatedVSStats->allReqsProfileBucket;
        PRFloat64 responseTime = 0;
        if (allReqsProfile.countRequests > 0) {
            responseTime = StatsManagerUtil::getResponseTime(allReqsProfile);
        }
        if (accumulatedVSStats->maxResponseTime < responseTime)
            accumulatedVSStats->maxResponseTime = responseTime;
    }
    statsAvgHandler->updateAverageQueue(accumulatedVSStats, captureTime);

    // Update StatsConnectionQueueNode
    StatsConnectionQueueSlot* queue = &procCurrent->connQueue->connQueueStats;
    ConnectionQueue* cq = DaemonSession::GetConnQueue();
    if (cq) {
        PRFloat64 avg[3];

        queue->countQueued = cq->GetLength();
        queue->peakQueued = cq->GetPeakLength();
        queue->maxQueued = cq->GetMaxLength();
        queue->countOverflows = cq->GetNumConnectionOverflows();
        cq->GetQueueingDelay(&queue->countTotalQueued, &queue->ticksTotalQueued);
        queue->countTotalConnections = cq->GetTotalNewConnections();

        loadavg_get(cq->GetLengthAverages(), avg, 3);
        queue->countQueued1MinuteAverage = avg[0];
        queue->countQueued5MinuteAverage = avg[1];
        queue->countQueued15MinuteAverage = avg[2];
    }

    if (configuration) configuration->unref();
    // Release  the lock before java side poll as it may want to aquire other
    // locks for notification.
    unlockStatsData();

    if (javaStatsManager && WebServer::isReady()) {
        javaStatsManager->poll();
    }
}

//-----------------------------------------------------------------------------
// StatsManager::poll
//-----------------------------------------------------------------------------

void StatsManager::poll()
{
    pollStats();
    if ((flagChildInitialized_ == PR_TRUE) && (statsMsgChannel == NULL)) {
        PR_Lock(lockStatsChannel);
        statsMsgChannel = StatsServerMessage::connectStatsChannel(NULL);
        PR_Unlock(lockStatsChannel);
    }
    if (flagServerReconfigured == PR_TRUE) {
        flagServerReconfigured = PR_FALSE;
        if (javaStatsManager)
        {
            javaStatsManager->processReconfigure();
        }
        // First refresh the stats so that all reconfig changes are captured.
        pollStats();
        lockStatsData();
        doReconfigureChanges();
        unlockStatsData();
    }
    sendPendingNotifications();
    return;
}

//-----------------------------------------------------------------------------
// StatsManager::getSearchAppStatus
//
// Return the search-app mode (enabled/disabled/empty). Also returned the
// serach app uri in searchAppUri.
//-----------------------------------------------------------------------------

int
StatsManager::getSearchAppStatus(const StatsVirtualServerNode* vss,
                                 NSString& searchAppUri)
{
    Configuration* configuration = ConfigurationManager::getConfiguration();
    VirtualServer* vs = configuration ? configuration->getVS(vss->vssStats.id) : 0;
    PRBool fFound = PR_FALSE;
    PRInt32 mode = STATS_WEBMODULE_MODE_EMPTY;
    if (vs) {
        const ServerXMLSchema::SearchApp& searchApp = vs->searchApp;
        const char* uri = (const char*) searchApp.uri;
        searchAppUri = uri;
        if (searchApp.enabled) {
            mode = STATS_WEBMODULE_MODE_ENABLED;
        }
        else {
            mode = STATS_WEBMODULE_MODE_DISABLED;
        }
    }
    if (configuration) configuration->unref();
    return mode;
}

//-----------------------------------------------------------------------------
// StatsManager::setWebModuleStatusFromConfig
//-----------------------------------------------------------------------------

void
StatsManager::setWebModuleStatusFromConfig(const StatsVirtualServerNode* vss,
                                           const char* uri,
                                           StatsWebModuleNode* wms)
{
    Configuration* configuration = ConfigurationManager::getConfiguration();
    VirtualServer* vs = configuration ? configuration->getVS(vss->vssStats.id) : 0;
    PRBool fFound = PR_FALSE;
    if (vs) {
        int nWebModuleCount = vs->getWebAppCount();
        int nIndex = 0;
        for (nIndex = 0; nIndex < nWebModuleCount; ++nIndex) {
            const ServerXMLSchema::WebApp* webApp = vs->getWebApp(nIndex);
            const char* curUri = webApp ? (const char*) webApp->uri : NULL;
            if (curUri && (strcmp(curUri, uri) == 0)) {
                // we found the node. Now set the mode.
                fFound = PR_TRUE;
                if (webApp->enabled) {
                    wms->wmsStats.mode = STATS_WEBMODULE_MODE_ENABLED;
                }
                else {
                    wms->wmsStats.mode = STATS_WEBMODULE_MODE_DISABLED;
                }
                break; // for loop
            }
            // else continue the for loop
        }
    }
    // Default web-app / is enabled if it is missing.
    if (fFound != PR_TRUE) {
        if ((strcmp(uri, "/") != 0)) {
            wms->wmsStats.mode = STATS_WEBMODULE_MODE_EMPTY;
        }
        else {
            // Default web-app "/" is enabled by default.
            wms->wmsStats.mode = STATS_WEBMODULE_MODE_ENABLED;
        }
    }

    if (configuration) configuration->unref();
}

//-----------------------------------------------------------------------------
// StatsManager::doReconfigureChanges
//-----------------------------------------------------------------------------

void StatsManager::doReconfigureChanges(void)
{
    StatsManagerUtil::resetVSSForEmptyNodes(hdr->vss);

    // Mark the flag for reconfig notification.
    markNotificationFlag(STATS_NOTICE_RECONFIG_DONE);

    // set the web module mode if changed during reconfiguration.
    StatsVirtualServerNode* vss = hdr->vss;
    while (vss) {
        NSString searchAppUri = "";
        int searchAppMode = getSearchAppStatus(vss, searchAppUri);
        StatsWebModuleNode* wms = vss->wms;
        while (wms) {
            if (vss->vssStats.mode == STATS_VIRTUALSERVER_EMPTY) {
                wms->wmsStats.mode = STATS_WEBMODULE_MODE_EMPTY;
            }
            else if (vss->vssStats.mode == STATS_VIRTUALSERVER_DISABLED) {
                wms->wmsStats.mode = STATS_WEBMODULE_MODE_DISABLED;
            }
            else {
                const char* webModuleName = wms->name.data();
                const char* uri =
                    StatsManagerUtil::getWebModuleUriFromName(webModuleName);
                if (strcmp(uri, (const char*) searchAppUri) != 0) {
                    setWebModuleStatusFromConfig(vss, uri, wms);
                } else {
                    // Search app.
                    wms->wmsStats.mode = searchAppMode;
                }
            }
            wms = wms->next;
        }
        vss = vss->next;
    }
}

//-----------------------------------------------------------------------------
// StatsManager::runSelfDiagnostics
//-----------------------------------------------------------------------------

void StatsManager::runSelfDiagnostics()
{
    lockStatsData();

    ereport(LOG_INFORM, "Structure Name                  Size");
    int nSize = sizeof(struct _StatsListenSlot);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsListenSlot                %d", nSize);
    nSize = sizeof(struct _StatsRequestBucket);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsRequestBucket             %d", nSize);
    nSize = sizeof(struct _StatsProfileBucket);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsProfileBucket             %d", nSize);
    nSize = sizeof(struct _StatsThreadPoolBucket);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsThreadPoolBucket          %d", nSize);
    nSize = sizeof(struct _StatsDnsBucket);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsDnsBucket                 %d", nSize);
    nSize = sizeof(struct _StatsKeepaliveBucket);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsKeepaliveBucket           %d", nSize);
    nSize = sizeof(struct _StatsCacheBucket);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsCacheBucket               %d", nSize);
    nSize = sizeof(struct _StatsConnectionQueueSlot);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsConnectionQueueSlot       %d", nSize);
    nSize = sizeof(struct _StatsCpuInfoSlot);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsCpuInfoSlot               %d", nSize);
    nSize = sizeof(struct _StatsServletJSPSlot);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsServletJSPSlot            %d", nSize);
    nSize = sizeof(struct _StatsWebModuleSlot);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsWebModuleSlot             %d", nSize);
    nSize = sizeof(struct _StatsVirtualServerSlot);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsVirtualServerSlot         %d", nSize);
    nSize = sizeof(struct _StatsThreadSlot);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsThreadSlot                %d", nSize);
    nSize = sizeof(struct _StatsProcessSlot);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsProcessSlot               %d", nSize);
    nSize = sizeof(struct _StatsHeader);
    PR_ASSERT(nSize % 8 == 0);
    ereport(LOG_INFORM, "_StatsHeader                    %d", nSize);
    unlockStatsData();

}


//-----------------------------------------------------------------------------
// StatsManager::findChainLength
//-----------------------------------------------------------------------------

int StatsManager::findChainLength(StatsVirtualServerNode* vss)
{
    int lengthChain = 0;
    while (vss) {
        lengthChain++;
        vss = vss->next;
    }

    return lengthChain;
}

#ifdef PLATFORM_SPECIFIC_STATS_ON
//-----------------------------------------------------------------------------
// StatsManager::updateSystemStats
//-----------------------------------------------------------------------------

void StatsManager::updateSystemStats()
{
    // Update the kernel statistics to get it in sync with the OS
    if (kernelStats)
        kernelStats->update();
}
#endif

//-----------------------------------------------------------------------------
// StatsManager::updateStats
//-----------------------------------------------------------------------------

void StatsManager::updateStats()
{
    updateSecondsRunning();

#ifdef PLATFORM_SPECIFIC_STATS_ON
    static PRIntervalTime ticksEpoch = 0;

    if (!ticksEpoch || (ft_timeIntervalNow() - ticksEpoch) > ticksUpdateInterval) {
        updatePlatformSpecificStats();
        ticksEpoch = ft_timeIntervalNow();
    }
#endif
}

#ifdef PLATFORM_SPECIFIC_STATS_ON
//-----------------------------------------------------------------------------
// StatsManager::updatePlatformSpecificStats
//-----------------------------------------------------------------------------

void StatsManager::updatePlatformSpecificStats()
{
    if (!flagInitialized) return;
    lockStatsData();
    
    // Update the statistics structures to get it in sync with the kernel's
    StatsManager::updateSystemStats();

    // Update System Load Averages
    if (kernelStats) {
        LoadAveragesInfo infoLoadAveragesInfo;
        if (kernelStats->getLoadAverages(&infoLoadAveragesInfo) == PR_SUCCESS) {
            hdr->hdrStats.load1MinuteAverage = infoLoadAveragesInfo.load_avg_for_1min;
            hdr->hdrStats.load5MinuteAverage = infoLoadAveragesInfo.load_avg_for_5min;
            hdr->hdrStats.load15MinuteAverage = infoLoadAveragesInfo.load_avg_for_15min;
        } else {
            delete kernelStats;
            kernelStats = 0;
        }
    }

    // Update the NetworkThroughPut
    if (kernelStats) {
        NetworkStatsInfo infoNetworkStatsInfo;
        if (kernelStats->getNetworkThroughPut(&infoNetworkStatsInfo) == PR_SUCCESS) {
            hdr->hdrStats.rateBytesTransmitted = infoNetworkStatsInfo.in_bytes_per_sec;
            hdr->hdrStats.rateBytesReceived = infoNetworkStatsInfo.out_bytes_per_sec;
        } else {
            delete kernelStats;
            kernelStats = 0;
        }
    }

    // Update CpuInfoSlot
    if (kernelStats) {
        ProcessorInfo *infoProcessorInfo = (ProcessorInfo *) MALLOC(countCpuInfoSlots*sizeof(ProcessorInfo));
        if (kernelStats->getProcessorInfo(infoProcessorInfo) == PR_SUCCESS) {
            StatsCpuInfoNode* cps = hdr->cps;
            int index = 0;
            while (cps) {
                cps->cpuInfo.percentIdle = infoProcessorInfo[index].percent_idle_time;
                cps->cpuInfo.percentUser = infoProcessorInfo[index].percent_user_time;
                cps->cpuInfo.percentKernel = infoProcessorInfo[index].percent_kernel_time;
                index++;
                cps = cps->next;
            }
        } else {
            delete kernelStats;
            kernelStats = 0;
        }
        FREE(infoProcessorInfo);
    }

    unlockStatsData();
}
#endif

//-----------------------------------------------------------------------------
// StatsManager::processStatsQuery
//
// Generic method to send a statsMsgType stats request and keep receiving
// response and forward the response to fd. On every receive send a
// acknowlegment message (statsMsgTypeAck)
//-----------------------------------------------------------------------------

int StatsManager::processStatsQuery(PRFileDesc * fd,
                                    const char* query,
                                    StatsMessages statsMsgType,
                                    StatsMessages statsMsgTypeAck)
{
    int retval = 0;
    if (!statsMsgChannel)
        return retval;
    PR_Lock(lockStatsChannel);

    StatsMessenger messenger(statsMsgChannel);
    StatsRespFdReceiver receiver(fd);
    PRBool fSuccess = messenger.sendMsgAndProcessResp(statsMsgType,
                                                      statsMsgTypeAck,
                                                      receiver,
                                                      query);
    statsMsgChannel->resetMessage();
    PR_Unlock(lockStatsChannel);
    return retval;
}

//-----------------------------------------------------------------------------
// StatsManager::writeStatsXML
//-----------------------------------------------------------------------------

int StatsManager::writeStatsXML(PRFileDesc * fd, const char* query)
{
    return processStatsQuery(fd, query, statsmsgReqGetStatsXMLData,
                             statsmsgReqGetStatsXMLDataAck);
}

//-----------------------------------------------------------------------------
// StatsManager::serviceDumpStats
//
// service dump request
//-----------------------------------------------------------------------------

int StatsManager::serviceDumpStats(PRFileDesc * fd, const char* query)
{
    return processStatsQuery(fd, query, statsmsgReqGetServiceDump,
                             statsmsgReqGetServiceDumpAck);
}

//-----------------------------------------------------------------------------
// StatsManager::getNameFromStringStore
//-----------------------------------------------------------------------------

const char* StatsManager::getNameFromStringStore(PRInt32 offset)
{
    PR_ASSERT(flagInitialized);
    if (offset >= strings.getSize()) {
        return 0;
    }
    if (offset == 0)
        return 0;
    return (strings.getBuffer() + offset);
}

//-----------------------------------------------------------------------------
// StatsManager::getPid
//
// This must be called by child process only as the primordial
// process may never set it's on pid here.
//-----------------------------------------------------------------------------

int StatsManager::getPid()
{
    StatsProcessNode* proc = hdr->process;
    if (proc) {
        return (int) proc->procStats.pid;
    }
    return -1;
}

//-----------------------------------------------------------------------------
// StatsManager::isValidPid
//
// Check to see if the there is a child process whose id matches pid.
//-----------------------------------------------------------------------------

PRBool StatsManager::isValidPid(int pid)
{
    StatsHeaderNode* header = getHeader();
    StatsProcessNode* process = 0;
    process = statsFindNode<StatsProcessNode, StatsHeaderNode>(header, pid);
    if (process) {
        return PR_TRUE;
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsManager::sendNotification
//
// Caller of this function must release the lockPerProcess lock or else
// there might be deadlock.
// If the function returns FALSE then caller should try again after some time,
// as the receiver may not be ready to receive this notificaiton.
//-----------------------------------------------------------------------------

PRBool
StatsManager::sendNotification(StatsNoticeMessages msgType,
                               void* buffer, int bufLen)
{
    if (!statsMsgChannel)
        return PR_FALSE;
    PR_Lock(lockStatsChannel);
    PRBool fSuccess = statsMsgChannel->sendRecvNotification(msgType,
                                                            buffer, bufLen);
    if (fSuccess != PR_TRUE) {
        if (statsMsgChannel->isFileDescriptorError()) {
            delete statsMsgChannel;
            // Try creating a new channel.
            statsMsgChannel = StatsServerMessage::connectStatsChannel(NULL);
        }
        else {
            WDMessages lastWDMessage = statsMsgChannel->getLastMsgType();
            // Typically it should not happen. But potentially it could happen.
            // Since primordial process receives messages in any order so what
            // can happen is that the notification message arrives before
            // childs are initialized. Typically primoridal process sends the
            // error (statsmsgErrorNotReady)
            StatsErrorMessages errMsg = statsmsgErrorBegin;
            if (lastWDMessage == wdmsgRespError) {
                statsMsgChannel->getStatsError(errMsg);
            }
            PR_ASSERT(lastWDMessage == wdmsgRespError);
        }
    }
    if (statsMsgChannel) {
        statsMsgChannel->resetMessage();
    }
    PR_Unlock(lockStatsChannel);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsManager::markNotificationFlag
//-----------------------------------------------------------------------------

void StatsManager::markNotificationFlag(StatsNotificationFlags notificationFlag)
{
    if (notificationFlag < STATS_NOTICE_FLAG_LAST) {
        flagNotification[notificationFlag] = PR_TRUE;
    }
    else {
        PR_ASSERT(0); // Improper use of enum by caller.
    }
}

//-----------------------------------------------------------------------------
// StatsManager::sendPendingNotifications
//-----------------------------------------------------------------------------

void StatsManager::sendPendingNotifications(void)
{
    int nIndex = 0;
    for (nIndex = 0; nIndex < STATS_NOTICE_FLAG_LAST; ++nIndex) {
        if (flagNotification[nIndex] != PR_TRUE) {
            continue; // next Notification.
        }
        StatsNoticeMessages noticeMsg = statsNoticeBegin;
        switch (nIndex)
        {
            case STATS_NOTICE_RECONFIG_DONE:
                noticeMsg = statsNoticeReconfigureDone;
                break;
            case STATS_NOTICE_JVM_INITIALIZED:
                noticeMsg = statsNoticeJvmInitialized;
                break;
            case STATS_NOTICE_WEB_MODULE_INIT_DONE:
                noticeMsg = statsNoticeWebModuleInit;
                break;
            case STATS_NOTICE_JDBC_NODES_COUNT_CHANGED:
                noticeMsg = statsNoticeJdbcNodesCountChanged;
                break;
            case STATS_NOTICE_SESSION_REPL_NODE_COUNT_CHANGE:
                noticeMsg = statsNoticeSessReplNodeCountChanged;
                break;
            default:
                PR_ASSERT(0);
                break;
        }
        if (noticeMsg != statsNoticeBegin) {
            if (sendNotification(noticeMsg, 0, 0) == PR_TRUE) {
                flagNotification[nIndex] = PR_FALSE;
            }
            else {
                break; // Try any other notifications later.
            }
        }
    }
}

//-----------------------------------------------------------------------------
// StatsManager::setJavaStatsManager
//-----------------------------------------------------------------------------

void StatsManager::setJavaStatsManager(JavaStatsManagerItf* javaStatsMgr)
{
    // This will be called from j2eeplugin. This must be called once only
    javaStatsManager = javaStatsMgr;
    setJvmEnabled();
    markNotificationFlag(STATS_NOTICE_JVM_INITIALIZED);
}

//-----------------------------------------------------------------------------
// StatsManager::getVSSFromWebModuleName
//
// Search for all vss for which the webModuleName belongs.
// Note that it only searches the name. It does not expect to have
// webModule slot exists with webModuleName
//-----------------------------------------------------------------------------

StatsVirtualServerNode* 
StatsManager::getVSSFromWebModuleName(const char* webModuleName)
{
    StatsVirtualServerNode* vss = hdr->vss;
    return StatsManagerUtil::getVSSFromWebModuleName(vss, webModuleName);
}

//-----------------------------------------------------------------------------
// StatsManager::getJdbcConnPoolNode
//-----------------------------------------------------------------------------

StatsJdbcConnPoolNode*
StatsManager::getJdbcConnPoolNode(const char* poolName, PRBool& fNewNode)
{
    return StatsManagerUtil::getJdbcConnPoolNode(procCurrent, poolName,
                                                 fNewNode);
}

//-----------------------------------------------------------------------------
// StatsManager::findVirtualServerSlot
//-----------------------------------------------------------------------------

StatsVirtualServerNode* StatsManager::findVirtualServerSlot(const char* vsId)
{
    return statsFindNode<StatsVirtualServerNode>(hdr, vsId);
}


//-----------------------------------------------------------------------------
// StatsManager::findWebModuleSlot
//
// Search the webmodule in virtualserver slot vss which has name equal to
// webModuleName
// Retruns null if not found.
//-----------------------------------------------------------------------------

StatsWebModuleNode* StatsManager::
findWebModuleSlot(const StatsVirtualServerNode* vss, const char* webModuleName)
{
    return statsFindNode<StatsWebModuleNode> (vss, webModuleName);
}


//-----------------------------------------------------------------------------
// StatsManager::findServletSlot
//-----------------------------------------------------------------------------

StatsServletJSPNode* StatsManager::
findServletSlot(const StatsWebModuleNode* wms, const char* servletName)
{
    return statsFindNode<StatsServletJSPNode> (wms, servletName);
}

//-----------------------------------------------------------------------------
// StatsManager::findThreadSlot
//
// find first thread slot whose mode matches the input argument mode.
//-----------------------------------------------------------------------------

StatsThreadNode* StatsManager::findThreadSlot(const StatsProcessNode* proc,
                                              int mode)
{
    return statsFindNode<StatsThreadNode> (proc, mode);
}


//-----------------------------------------------------------------------------
// StatsManager::setChildInitialized
//-----------------------------------------------------------------------------

void StatsManager::setChildInitialized()
{
    if (!flagInitialized)
        return;
#ifdef XP_WIN32
    ntStatsServer = new NTStatsServer();
    ntStatsServer->init();
#endif
    PR_Lock(lockStatsChannel);
    flagChildInitialized_ = PR_TRUE;
    PR_Unlock(lockStatsChannel);
}

//-----------------------------------------------------------------------------
// StatsManager::incrementReconfigCount
//-----------------------------------------------------------------------------

void StatsManager::incrementReconfigCount(void)
{
    if (!flagInitialized)
        return;
    StatsHeaderNode* header = getHeader();
    // increment the reconfig count.
    ++header->hdrStats.countReconfig;
}

//-----------------------------------------------------------------------------
// StatsManager::processReconfigure
// Right now this function is called by ChildAdminThread (on unix platforms)
// which should not send any notification as it could dead lock. So it marks a
// flag for Reconfigure. StatsManager thread later sends the notification
// to Parent process during poll.
//-----------------------------------------------------------------------------

void StatsManager::processReconfigure(void)
{
    if (!flagInitialized)
        return;
    incrementReconfigCount();
    flagServerReconfigured = PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsManager::updateJvmMgmtStats
//-----------------------------------------------------------------------------

void StatsManager::updateJvmMgmtStats(void)
{
    if (!flagInitialized)
        return;
    if (javaStatsManager)
    {
        javaStatsManager->updateJvmMgmtStats();
    }
    return;
}

//-----------------------------------------------------------------------------
// StatsManager::processStatsMessage
//
// This is called by ChildAdminThread (from the child process). This is also
// called by primordial process to serve the stats data to outside process.
// Message formats for most of the messages are same for child/Parent and
// Parent/StatsClient.
//-----------------------------------------------------------------------------

PRBool StatsManager::processStatsMessage(StatsServerMessage *msgChannel)
{
    if (!flagInitialized)
        return PR_FALSE;
    if (msgChannel->getLastMsgType() != wdmsgReqStatsData) {
        PR_ASSERT(0);
        msgChannel->sendStatsError(statsmsgErrorInReq);
        msgChannel->resetMessage();
        return PR_FALSE;
    }
    const char *statsMsg = msgChannel->getStatsMessage();
    if (statsMsg == 0) {
        PR_ASSERT(0);
        msgChannel->sendStatsError(statsmsgErrorInReq);
        msgChannel->resetMessage();
        return PR_FALSE;
    }
    PRBool fSuccess = PR_TRUE;
    StatsMessages statsMsgType = (StatsMessages) msgChannel->getStatsMsgType();
    StatsMsgBuff msgbuff;
    StringArrayBuff strmsgbuff;
    lockStatsData();
    StatsMsgPreparer msgPreparer(hdr);
    switch (statsMsgType) {
        case statsmsgReqGetStatsHeader:
        {
            msgPreparer.prepareHeaderStats(msgbuff);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetCpuInfo:
        {
#ifdef PLATFORM_SPECIFIC_STATS_ON
            msgPreparer.prepareCpuStats(msgbuff, countCpuInfoSlots);
#endif
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetStatsProcessInfo:
        {
            msgPreparer.prepareProcessStats(msgbuff, procCurrent);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetPidList:
        {
            msgPreparer.preparePidList(msgbuff);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetStringStore:
        {
            msgPreparer.prepareStringStore(msgbuff, strings);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetAccumulatedVSData:
        {
            if (statsAvgHandler) {
                // It should be child process here.
                statsAvgHandler->calcAverages(accumulatedVSStats);
            }
            // else : Primordial process need not to calculate average. It is
            // done in StatsBackupManager class.
            msgPreparer.prepareAccumulatedVSStats(msgbuff, accumulatedVSStats);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetStatsVirtualServerInfo:
        {
            const char *vsId = msgChannel->getStatsMessage();
            int nLen = msgChannel->getStatsMessageLen();
            if (nLen == STATS_ALIGN(strlen(vsId) + 1)) {
                if (msgPreparer.prepareVirtualServerStats(msgbuff,
                                                          vsId) == PR_TRUE) {
                    msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
                } else {
                    msgChannel->sendStatsError(statsmsgErrorVSData);
                }
            } else {
                msgChannel->sendStatsError(statsmsgErrorInReq);
            }
            break;
        }
        case statsmsgReqGetListenSlotData:
        {
            msgPreparer.prepareListenSlotsStats(msgbuff, procCurrent);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetVSList:
        {
            msgPreparer.prepareVSIdList(strmsgbuff);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, strmsgbuff);
            break;
        }
        case statsmsgReqGetWebModuleList:
        {
            msgPreparer.prepareWebModuleList(strmsgbuff);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, strmsgbuff);
            break;
        }
        case statsmsgReqGetAllWebModuleData:
        {
            msgPreparer.prepareAllWebModuleStats(msgbuff);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetWebModuleData:
        {
            const char *webModuleName = msgChannel->getStatsMessage();
            int nLen = msgChannel->getStatsMessageLen();
            if (nLen == STATS_ALIGN(strlen(webModuleName) + 1)) {
                msgPreparer.prepareWebModuleStats(msgbuff, webModuleName);
                msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            } else {
                msgChannel->sendStatsError(statsmsgErrorInReq);
            }
            break;
        }
        case statsmsgReqGetAllServletData:
        {
            msgPreparer.prepareAllServletStats(msgbuff);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetServletData:
        {
            const char *webModuleName = msgChannel->getStatsMessage();
            int nLen = msgChannel->getStatsMessageLen();
            if (nLen == STATS_ALIGN(strlen(webModuleName) + 1)) {
                msgPreparer.prepareServletStatsForWebModule(msgbuff,
                                                            webModuleName);
                msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            } else {
                msgChannel->sendStatsError(statsmsgErrorInReq);
            }
            break;
        }
        case statsmsgReqGetThreadIndexList:
        {
            msgPreparer.prepareThreadIndexList(msgbuff, procCurrent->thread);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetThreadSlotData:
        {
            const void *statsMsg = msgChannel->getStatsMessage();
            int nLen = msgChannel->getStatsMessageLen();
            if (nLen == sizeof(PRInt32)) {
                PRInt32 nThreadIndex = 0;
                // Unaligned access so better copy
                memcpy(&nThreadIndex, statsMsg, sizeof(PRInt32));
                msgPreparer.prepareThreadSlotStats(msgbuff,
                                                   nThreadIndex,
                                                   procCurrent->thread);
                msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            } else {
                msgChannel->sendStatsError(statsmsgErrorInReq);
            }
            break;
        }
        case statsmsgReqGetJdbcConnPools:
        {
            msgPreparer.prepareJdbcConnPoolStats(msgbuff);
            msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            break;
        }
        case statsmsgReqGetJvmMgmtData:
        {
            if (javaStatsManager) {
                // updateJvmMgmtStats internally acquire locks so release it
                // first, call the method and then acquire the lock again.
                unlockStatsData();
                updateJvmMgmtStats();
                lockStatsData();
                msgPreparer.prepareJvmMgmtStats(msgbuff);
                msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            }
            else {
                msgChannel->sendStatsError(statsmsgErrorJvmStatsNoInit);
            }
            break;
        }
        case statsmsgReqGetSessReplData:
        {
            StatsProcessNode* procNode = getProcessSlot();
            if (procNode) {
                msgPreparer.prepareSessionReplicationStats(msgbuff);
                msgChannel->sendStatsResponse(wdmsgRespStatsData, msgbuff);
            }
            else {
                msgChannel->sendStatsError(statsmsgErrorNoSessReplStats);
            }
            break;
        }
        default:
        {
            fSuccess = PR_FALSE;
            PR_ASSERT(0);
            msgChannel->sendStatsError(statsmsgErrorInReq);
            break;
        }
    }
    msgChannel->resetMessage();
    unlockStatsData();
    return fSuccess;
}


