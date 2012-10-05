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
 * custom stats collection routines for Linux added Sharath 10/10/2002
 * custom stats collection routines for HPUX added yingbin 06/21/2006
 */


#ifndef STATSMANAGER_H
#define STATSMANAGER_H

#include "httpdaemon/libdaemon.h"
#include "frame/func.h"
#include "public/iwsstats.h"
#include "support/LinkedList.hh"
#include "NsprWrap/Thread.h"
#include "httpdaemon/statsmessage.h"

class VirtualServer;
struct shmem_s;
struct StatsPrivate;
#ifdef SOLARIS
class SolarisKernelStats;
class SolarisProcessStats;
#endif

#ifdef Linux
class LinuxKernelStats;
class LinuxProcessStats;
#endif
#ifdef HPUX
class HpuxKernelStats;
class HpuxProcessStats;
#endif
class JavaStatsManagerItf;
class StatsMovingAverageHandler;
//-----------------------------------------------------------------------------
// StatsProfileIndex
//-----------------------------------------------------------------------------

enum StatsProfileIndex { STATS_PROFILE_ALL = 0, 
                         STATS_PROFILE_DEFAULT = 1, 
                         STATS_PROFILE_CACHE = 2 };

//-----------------------------------------------------------------------------
// StatsNotificationFlags
//
// Let RECONFIG be first notification as this is the most important one which
// parent want to listen.
//-----------------------------------------------------------------------------

enum StatsNotificationFlags { STATS_NOTICE_RECONFIG_DONE                  = 0,
                              STATS_NOTICE_JVM_INITIALIZED                = 1,
                              STATS_NOTICE_WEB_MODULE_INIT_DONE           = 2,
                              STATS_NOTICE_JDBC_NODES_COUNT_CHANGED       = 3,
                              STATS_NOTICE_SESSION_REPL_NODE_COUNT_CHANGE = 4,
                              STATS_NOTICE_FLAG_LAST                      = 5 };


//-----------------------------------------------------------------------------
// StringStore
//-----------------------------------------------------------------------------

class StringStore {
public:
    StringStore();
    StringStore(const void* inputBuffer, int sizeInput);
    ~StringStore();
    ptrdiff_t findString(const char* string) const;
    ptrdiff_t addString(const char* string);
    const char* getBuffer() const { return buffer; }
    int getSize() const { return size; }
    void freeze() { flagFrozen = PR_TRUE; }

private:
    void require(int length);

    char *buffer;
    int size;
    int capacity;
    PRBool flagFrozen;
};

//-----------------------------------------------------------------------------
// StatsManager
//-----------------------------------------------------------------------------

class HTTPDAEMON_DLL StatsManager {
public:
    static void enable();
    static void enableProfiling();
    static void setMaxListenSlots(int maxListen);
#ifdef PLATFORM_SPECIFIC_STATS_ON
    static void setMaxCpuInfoSlots(int maxCpuInfoSlots);
#endif
    static void setUpdateInterval(int secondsUpdateInterval);
    static int initEarly(int maxProcs, int maxThreads);
    static void initLate();
    static void startPollThread();
    static void terminateProcess();
    static void terminateServer();
    static ptrdiff_t getFunctionName(const char *name);
    static void addProfileBucket(const char* name, const char* description);
    static int getProfileBucketCount() { return countProfileBuckets; }
    static StatsHeaderNode* getHeader() { return hdr; }
    static StatsProcessNode* findProcessSlot();
    static void activateProcessSlot(PRInt32 pid);
    static void freeProcessSlot(PRInt32 pid);
    static StatsProcessNode* getProcessSlot();
    static StatsThreadNode* allocThreadSlot(const char* threadQName);
    static void recordKeepaliveHits(int countKeepaliveHits);
    static void lockStatsData();
    static void unlockStatsData();
    static StatsListenNode* allocListenSlot(const char* id);
    static void freeListenSlot(const char* id);
    static StatsVirtualServerNode* getVirtualServerSlot(const VirtualServer* vs);
    static PRBool isInitialized() { return flagInitialized; }
    static PRBool isProfilingEnabled() { return countProfileBuckets > 0; }
    static int getMethodSize() { return sizeMethod; }
    static PRIntervalTime getUpdateInterval() { return ticksUpdateInterval; }
    static int getUriSize() { return sizeUri; }
    static const char* getStringBase() { return stringbase; }
    static void runSelfDiagnostics();
    static void updateSecondsRunning();
    static const char* getMode(const StatsVirtualServerNode* vs);
    static const char* getMode(const StatsProcessNode* process);
    static const char* getMode(const StatsThreadSlot* thread);
    static const char* getMode(const StatsWebModuleSlot* wms);
    static StatsAccumulatedVSSlot* getAccumulatedVSSlot();
    static void updateStats();
#ifdef PLATFORM_SPECIFIC_STATS_ON
    static void initSystemStats();
    static void initPlatformSpecificStats();
    static void updateSystemStats();
    static void updatePlatformSpecificStats();
#endif
    static void updateJvmMgmtStats();
    static void initProfileBuckets(StatsProfileNode* profile);
    static PRBool isLocked() { return flagLocked; }

    static PRBool isEnabled();
    static int processStatsQuery(PRFileDesc * fd,
                                 const char* query,
                                 StatsMessages statsMsgType,
                                 StatsMessages statsMsgTypeAck);
    static int writeStatsXML(PRFileDesc* fd, const char* query);
    static int serviceDumpStats(PRFileDesc* fd, const char* query);
    static PRBool processStatsMessage(StatsServerMessage* msgChannel);
    static const char* getNameFromStringStore(PRInt32 offset);
    static int getPid();
    static PRBool isValidPid(int pid);

    static void setupVirtualServerSlot(StatsVirtualServerNode* vss);
    static void setProcessNode(StatsProcessNode* procNode);
    // setJavaStatsManager : Initializing JavaStatsMangager interface
    static void setJavaStatsManager(JavaStatsManagerItf* javaStatsMgr);

    // Search for all vss for which the webModuleName belongs.
    static StatsVirtualServerNode* 
           getVSSFromWebModuleName(const char* webModuleName);

    // Search for connection pool whose name matches with poolName. Create a
    // new one if it doesn't exist.
    static StatsJdbcConnPoolNode* getJdbcConnPoolNode(const char* poolName,
                                                      PRBool& fNewNode);

    static PRBool getWebModuleList(StringArrayBuff& strmsgbuff);
    static PRBool getAllWebModuleData(StatsMsgBuff& msgbuff);
    static PRBool getWebModuleData(StatsMsgBuff& msgbuff, 
                                   const char* webModuleName);
    static PRBool sendNotification(StatsNoticeMessages msgType,
                                   void* buffer,
                                   int bufLen);
    static void markNotificationFlag(StatsNotificationFlags notificationFlag);
    static void sendPendingNotifications(void);
    static void setChildInitialized();
    static void incrementReconfigCount(void);
    static void processReconfigure(void);
    static void setJvmEnabled(void);
    static void setWebappsEnabled(void);
    static PRBool isJvmEnabled(void);
    static PRBool isWebappsEnabled(void);

    static StatsVirtualServerNode* findVirtualServerSlot(const char* vsId);

    static StatsServletJSPNode* findServletSlot(const StatsWebModuleNode* wms,
                                                const char* servletName);
    static StatsWebModuleNode* findWebModuleSlot(const StatsVirtualServerNode* vss,
                                                 const char* webModuleName);
    static StatsThreadNode* findThreadSlot(const StatsProcessNode* proc,
                                           int mode);


private:
    static void poll();
    static void pollStats(void);
    static void initVS(VirtualServer* vs);
    static int findChainLength(StatsVirtualServerNode* vss);
    static void doReconfigureChanges(void);
    static void setWebModuleStatusFromConfig(const StatsVirtualServerNode* vss,
                                             const char* uri,
                                             StatsWebModuleNode* wms);
    static int getSearchAppStatus(const StatsVirtualServerNode* vss,
                                  NSString& searchAppUri);

    static PRBool flagEnabled;
    static PRBool flagInitialized;
    static PRBool flagLocked;
    static PRLock* lockPerProcess;
    static int sizeMethod;
    static int sizeUri;
    static int sizePerProcess;
    static int sizePerVirtualServer;
    static int countListenSlots;
    static int countProfileBuckets;
#ifdef PLATFORM_SPECIFIC_STATS_ON
    static int sizePerCpuInfo;
    static int countCpuInfoSlots;
#endif
    static PRIntervalTime ticksUpdateInterval;
    static shmem_s* shmem;
    static StatsHeaderNode* hdr;
    static StatsPrivate* prv;
    static const char* stringbase;
    static int slotVSPrivateData;
    static StatsProcessNode* procCurrent;
    static int countKeepaliveHits;

#ifdef SOLARIS
    static SolarisKernelStats* kernelStats;
    static SolarisProcessStats* processStats;
#endif

#ifdef Linux
    static LinuxKernelStats* kernelStats;
    static LinuxProcessStats* processStats;
#endif
#ifdef HPUX
    static HpuxKernelStats* kernelStats;
    static HpuxProcessStats* processStats;
#endif
    static PRBool flagChildInitialized_;
    static PRBool flagServerReconfigured;
    static PRBool flagNotification[STATS_NOTICE_FLAG_LAST];
    static StatsMovingAverageHandler* statsAvgHandler;
    static StatsAccumulatedVSSlot* accumulatedVSStats;
    static StatsServerMessage* statsMsgChannel;
    static PRLock* lockStatsChannel;
    static JavaStatsManagerItf* javaStatsManager;


friend class StatsManagerVSListener;
friend class StatsPollThread;
};

//-----------------------------------------------------------------------------
// StatsRunningThread
//-----------------------------------------------------------------------------

class StatsRunningThread : public Thread {
public:
    StatsRunningThread();

private:
    void run();
};

inline
PRBool 
StatsManager::isEnabled()
{
    return flagEnabled;
}

inline
StatsProcessNode*
StatsManager::getProcessSlot()
{
    return (hdr ? hdr->process : NULL);
}

inline
void
StatsManager::setJvmEnabled(void)
{
    StatsHeaderNode* headerNode = getHeader();
    if (headerNode)
        headerNode->hdrStats.jvmEnabled = STATS_STATUS_ENABLED;
}

inline
void
StatsManager::setWebappsEnabled(void)
{
    StatsHeaderNode* headerNode = getHeader();
    if (headerNode)
        headerNode->hdrStats.webappsEnabled = STATS_STATUS_ENABLED;
}

inline
PRBool
StatsManager::isJvmEnabled(void)
{
    StatsHeaderNode* headerNode = getHeader();
    if (! headerNode)
        return PR_FALSE;
    return (headerNode->hdrStats.jvmEnabled == STATS_STATUS_ENABLED)
           ? PR_TRUE : PR_FALSE;
}

inline
PRBool
StatsManager::isWebappsEnabled(void)
{
    StatsHeaderNode* headerNode = getHeader();
    if (! headerNode)
        return PR_FALSE;
    return (headerNode->hdrStats.webappsEnabled == STATS_STATUS_ENABLED)
           ? PR_TRUE : PR_FALSE;
}

inline
StatsAccumulatedVSSlot*
StatsManager::getAccumulatedVSSlot()
{
    return accumulatedVSStats;
}

#endif // STATSMANAGER_H

