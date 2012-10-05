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

#include "httpdaemon/StatsClient.h"
#include "httpdaemon/statsbkupmgr.h"
#include "httpdaemon/statsmanager.h" // StringStore
#ifdef XP_WIN32
#include "httpdaemon/NTStatsServer.h" // NTStatsServer class
#else
#include "httpdaemon/ParentAdmin.h" // ParentAdmin class
#endif
#include "libserverxml/ServerXML.h" // ServerXML
#include "support/EreportableException.h" // for EreportableException

// Class static members
StatsClientManager* StatsClientManager::defaultStatsClientMgr_ = NULL;

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

////////////////////////////////////////////////////////////////

// StatsClient Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsClient::StatsClient
//-----------------------------------------------------------------------------

StatsClient::StatsClient(void)
{
    statsMsgChannel_  = NULL;
    statsNoticeChannel_ = NULL;
    clientLock_       = NULL;
    procNodesHandler_ = NULL;
    vssInstance_      = NULL;
    strStoreInstance_ = NULL;
    fWebModulesInit_  = PR_FALSE;
    memset(&statsHdr_, 0, sizeof(statsHdr_));
    prevReconfigCount_ = 0;
    prevChildDeathCount_= 0;
    lastErrorMsg_ = sclerrmsgNoConnection; // No connection
}

//-----------------------------------------------------------------------------
// StatsClient::~StatsClient
//-----------------------------------------------------------------------------

StatsClient::~StatsClient(void)
{
    resetConnection();
    if (clientLock_)
    {
        PR_DestroyLock(clientLock_);
    }
}

//-----------------------------------------------------------------------------
// StatsClient::lock
//
// Lock the channel. Channel must be locked/unlocked if used by
// multiple threads at the same time.
//-----------------------------------------------------------------------------

PRBool
StatsClient::lock(void)
{
    if (clientLock_)
    {
        PR_Lock(clientLock_);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::Unlock
//
// Unlock the channel.
//-----------------------------------------------------------------------------

PRBool
StatsClient::unlock(void)
{
    if (clientLock_)
    {
        PR_Unlock(clientLock_);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::init
//-----------------------------------------------------------------------------

PRBool
StatsClient::init(const char* connName, StatsServerMessage* statsMsgChannel)
{
    if (clientLock_ == NULL)
    {
        clientLock_ = PR_NewLock();
    }
    PR_ASSERT(connName);
    // save the connection Path.
    connectionPath_ = connName;

    resetConnection();
    if (statsMsgChannel)
        statsMsgChannel_ = statsMsgChannel;
    else
    {
        statsMsgChannel_ = StatsServerMessage::connectStatsChannel(connName);
    }

    if (!statsMsgChannel_)
        return PR_FALSE;
    // else
    if (!updateStatsHeader())
    {
        resetConnection();
        return PR_FALSE;
    }
    // Store the reconfig count and number of child died.
    prevReconfigCount_ = statsHdr_.hdrStats.countReconfig;
    prevChildDeathCount_= statsHdr_.hdrStats.countChildDied;
    if (!initStringStore())
    {
        resetConnection();
        return PR_FALSE;
    }
    if (initProcessSlots() != PR_TRUE)
    {
        resetConnection();
        return PR_FALSE;
    }
    if (updateSessionReplicationData() != PR_TRUE)
    {
        resetConnection();
        return PR_FALSE;
    }
    lastErrorMsg_ = sclerrmsgNoError; // No error
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::lateUpdate
// This method does update those stats which are notified by instance. This
// method is typically called to avoid any missed notification during the delay
// between "init" and addNotificationConnection call. updateStatsHeader will
// set the enable flags for jvm/webapp while updateSessionReplicationStats will
// enable the session replication in instance if it is available.
//-----------------------------------------------------------------------------

PRBool
StatsClient::lateUpdate()
{
    if (updateStatsHeader() != PR_TRUE)
    {
        resetConnection();
        return PR_FALSE;
    }
    if (updateSessionReplicationData() != PR_TRUE)
    {
        resetConnection();
        return PR_FALSE;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::getStringFromOffset
//-----------------------------------------------------------------------------

const char*
StatsClient::getStringFromOffset(ptrdiff_t offset)
{
    if (strStoreInstance_ == NULL)
        return NULL;
    if (offset == 0)
        return 0;
    if (offset >= strStoreInstance_->getSize())
    {
        return 0;
    }
    return (strStoreInstance_->getBuffer() + offset);
}

//-----------------------------------------------------------------------------
// StatsClient::initStringStore
//-----------------------------------------------------------------------------

PRBool
StatsClient::initStringStore(void)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger,
                                           statsmsgReqGetStringStore);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    PR_ASSERT(strStoreInstance_ == NULL);
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    const void* recvBuffer = bufferReader.getBufferAddress();
    int recvLen = bufferReader.getRemainingBytesCount();
    strStoreInstance_ = new StringStore(recvBuffer, recvLen);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::initListenSlots
//-----------------------------------------------------------------------------

PRBool
StatsClient::initListenSlots(int pid)
{
    PRBool fSuccess = PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    fSuccess = sendRecvStatsRequest(messenger,
                                    statsmsgReqGetListenSlotData, &pid,
                                    sizeof(pid));
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }

    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess = procNodesHandler_->initListenSlots(pid, bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::initProcessSlotMisc
//-----------------------------------------------------------------------------

PRBool
StatsClient::initProcessSlotMisc(StatsProcessNode* processNode)
{
    // Initialize connection queue ids
    StatsConnectionQueueNode* connQueue = processNode->connQueue;
    while (connQueue)
    {
        // Push the new vs index at the end.
        connQueueIds_.push(new NSString(connQueue->connQueueStats.id));
        connQueue = connQueue->next;
    }

    // Initialize thread pool names
    StatsThreadPoolNode* threadPool = processNode->threadPool;
    while (threadPool)
    {
        // Push the new vs index at the end.
        int offsetThreadPoolName = threadPool->threadPoolStats.offsetName;
        const char* poolName = getStringFromOffset(offsetThreadPoolName);
        if (!poolName)
        {
            // It should not come here.
            PR_ASSERT(0);
            poolName = "";
        }
        threadPoolNames_.push(new NSString(poolName));
        threadPool = threadPool->next;
    }

    // Initialize listen slot ids;
    StatsListenNode* ls = processNode->ls;
    while (ls)
    {
        listenSocketIds_.push(new NSString(ls->lsStats.id));
        ls = ls->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::initProcessSlots
//-----------------------------------------------------------------------------

PRBool
StatsClient::initProcessSlots(void)
{
    if (!statsMsgChannel_)
        return PR_FALSE;

    int nChildren = statsHdr_.hdrStats.maxProcs;
    if (nChildren <= 0)
        return PR_FALSE;

    if (!procNodesHandler_)
    {
        int nCountProfileBuckets = statsHdr_.hdrStats.maxProfileBuckets;
        procNodesHandler_ = new StatsProcessNodesHandler(nChildren,
                                                         nCountProfileBuckets);
    }
    StatsBufferReader recvMsgBuff(0, 0);
    PRBool fSuccess = fetchPidList(recvMsgBuff);
    if (fSuccess != PR_TRUE)
        return PR_FALSE;

    PRInt32 pid = INVALID_PROCESS_ID;
    int nIndex = 0;
    while (recvMsgBuff.readInt32(pid) == PR_TRUE)
    {
        if (procNodesHandler_->getProcessSlot(pid) != NULL)
        {
            // process node already existed so we don't need to initialize this
            // pid.
            continue;
        }
        StatsMessenger messenger(statsMsgChannel_);
        fSuccess = sendRecvStatsRequest(messenger,
                                        statsmsgReqGetStatsProcessInfo,
                                        &pid,
                                        sizeof(PRInt32));
        if (fSuccess != PR_TRUE)
        {
            return fSuccess;
        }
        StatsBufferReader& bufferReader = messenger.getBufferReader();
        if (procNodesHandler_->initializeProcessSlot(bufferReader) != PR_TRUE) {
            return PR_FALSE;
        }
        initListenSlots(pid);
        if (nIndex == 0)
        {
            // Need only one time initialization
            StatsProcessNode* processNode = 0;
            processNode = procNodesHandler_->getFirstProcessSlot();
            // initialize the single process specific data.
            initProcessSlotMisc(processNode);
        }
        ++nIndex;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::initProfileNames
//-----------------------------------------------------------------------------

PRBool
StatsClient::initProfileNames(void)
{
    if (!vssInstance_)
        return PR_FALSE;

    StatsVirtualServerNode* vss = vssInstance_->getFirstVSNode();
    if (!vss)
        return PR_FALSE;

    StatsProfileNode* profile = vss->profile;
    while (profile)
    {
        int offsetProfileName = profile->profileStats.offsetName;
        const char* profileName = getStringFromOffset(offsetProfileName);
        if (!profileName)
        {
            // It should not come here.
            profileName = "";
        }
        profileNames_.push(new NSString(profileName));
        profile = profile->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::initInstanceVSS
//-----------------------------------------------------------------------------

PRBool
StatsClient::initInstanceVSS(void)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    if (vssInstance_)
        return PR_TRUE;

    vssInstance_ = new StatsVSNodesHandler();
    int nCountProfileBuckets = statsHdr_.hdrStats.maxProfileBuckets;
    vssInstance_->setProfileBucketCount(nCountProfileBuckets);
    PRBool fSuccess = updateVSList();
    if (fSuccess == PR_TRUE)
    {
        if (vsIds_.length() != 0)
        {
            // update VS node so that it pulls one Virtual server data.
            updateVSNode(*vsIds_[0]);
            fSuccess = initProfileNames();
        }
    }

    if (fSuccess == PR_FALSE)
    {
        delete vssInstance_;
        vssInstance_ = 0;
        // free the vsIds_ list.
        freeStringsInVector(vsIds_);

        // free profile names.
        freeStringsInVector(profileNames_);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::getStatsHeader
//
// get the StatsHeader
//-----------------------------------------------------------------------------

const StatsHeaderNode*
StatsClient::getStatsHeader(PRBool fUpdateFromServer)
{
    if (!statsMsgChannel_)
        return 0;
    PRBool fSuccess = PR_TRUE;
    if (fUpdateFromServer == PR_TRUE)
        fSuccess = updateStatsHeader();
    if (fSuccess == PR_TRUE)
    {
        return &statsHdr_;
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// StatsClient::getPid
//-----------------------------------------------------------------------------

int StatsClient::getPid(int nIndex)
{
    if (!statsMsgChannel_)
        return INVALID_PROCESS_ID;
    if (!procNodesHandler_)
        return INVALID_PROCESS_ID;
    return procNodesHandler_->getPidFromIndex(nIndex);
}

//-----------------------------------------------------------------------------
// StatsClient::fetchPidList
//-----------------------------------------------------------------------------

PRBool
StatsClient::fetchPidList(StatsBufferReader& recvMsgBuff)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger, statsmsgReqGetPidList);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    messenger.getBufferReaderCopy(recvMsgBuff);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::getProcessNodeInternal
//
// Try to access the process node from procNodesHandler_. If it doesn't find
// the node, sets the last error to sclerrmsgIllegalPid.
//-----------------------------------------------------------------------------

const StatsProcessNode*
StatsClient::getProcessNodeInternal(int pid)
{
    StatsProcessNode* procNode = procNodesHandler_->getProcessSlot(pid);
    if (!procNode)
        lastErrorMsg_ = sclerrmsgIllegalPid;
    return procNode;
}

//-----------------------------------------------------------------------------
// StatsClient::getProcessNode
//
// Return the process Node for process id pid.
//-----------------------------------------------------------------------------

const StatsProcessNode*
StatsClient::getProcessNode(int pid,
                            PRBool fUpdateFromServer)
{
    PRBool fSuccess = PR_FALSE;
    if (!procNodesHandler_)
        return 0;
    if (fUpdateFromServer == PR_FALSE)
    {
        return getProcessNodeInternal(pid);
    }
    StatsMessenger messenger(statsMsgChannel_);
    // Update data from server.
    fSuccess = sendRecvStatsRequest(messenger,
                                    statsmsgReqGetStatsProcessInfo,
                                    &pid,
                                    sizeof(PRInt32));
    if (fSuccess != PR_TRUE)
    {
        return 0;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    procNodesHandler_->updateProcessSlot(bufferReader);
    return getProcessNodeInternal(pid);
}

//-----------------------------------------------------------------------------
// StatsClient::getAccumulatedProcessNode
//-----------------------------------------------------------------------------

const StatsProcessNode*
StatsClient::getAccumulatedProcessNode(void)
{
    if (!procNodesHandler_)
        return NULL;
    procNodesHandler_->accumulateProcessData();
    return procNodesHandler_->getAccumulatedProcessNode();
}

//-----------------------------------------------------------------------------
// StatsClient::getJdbcPoolNode
//-----------------------------------------------------------------------------

const StatsJdbcConnPoolNode*
StatsClient::getJdbcPoolNode(int pid,
                             const char* poolName,
                             PRBool fUpdateFromServer)
{
    if (!procNodesHandler_)
        return NULL;
    const StatsProcessNode* procNode = getProcessNodeInternal(pid);
    if (!procNode)
        return NULL; // invalid pid
    if (fUpdateFromServer)
    {
        updateJdbcConnectionPools(pid);
    }
    const StatsJdbcConnPoolNode* jdbcPoolNode =
        statsFindNodeFromList<StatsJdbcConnPoolNode>(procNode->jdbcPoolNode,
                                                     poolName);
    if (!jdbcPoolNode)
        lastErrorMsg_ = sclerrmsgJdbcNodeNotFound;
    return jdbcPoolNode;
}

//-----------------------------------------------------------------------------
// StatsClient::getJvmMgmtNode
//-----------------------------------------------------------------------------

const StatsJvmManagementNode*
StatsClient::getJvmMgmtNode(int pid,
                            PRBool fUpdateFromServer)
{
    if (!procNodesHandler_)
        return NULL;
    const StatsProcessNode* procNode = getProcessNodeInternal(pid);
    if (!procNode)
        return NULL; // invalid pid
    if (fUpdateFromServer == PR_TRUE)
    {
        updateJvmMgmtData(pid);
    }
    if (!procNode->jvmMgmtNode)
    {
        if (lastErrorMsg_ == sclerrmsgNoError)
            lastErrorMsg_ = sclerrmsgJvmNodeNotFound;
    }
    return procNode->jvmMgmtNode;
}

//-----------------------------------------------------------------------------
// StatsClient::getSessionReplicationNode
//-----------------------------------------------------------------------------

const StatsSessionReplicationNode*
StatsClient::getSessionReplicationNode(PRBool fUpdateFromServer)
{
    if (!procNodesHandler_)
        return NULL;
    StatsProcessNode* processNode = 0;
    processNode = procNodesHandler_->getFirstProcessSlot();
    if (!processNode)
        return NULL;
    if (fUpdateFromServer == PR_TRUE)
    {
        if (updateSessionReplicationData() != PR_TRUE)
        {
            return NULL;
        }
    }
    if (!processNode->sessReplNode)
    {
        if (lastErrorMsg_ == sclerrmsgNoError)
            lastErrorMsg_ = sclerrmsgSessReplNodeNotFound;
    }
    return processNode->sessReplNode;
}

//-----------------------------------------------------------------------------
// StatsClient::getVsIdList
//-----------------------------------------------------------------------------

const StatsStringVector&
StatsClient::getVsIdList(void)
{
    if (!vssInstance_)
    {
        initInstanceVSS();
    }
    return vsIds_;
}

//-----------------------------------------------------------------------------
// StatsClient::getVirtualServerNode
//-----------------------------------------------------------------------------

const StatsVirtualServerNode*
StatsClient::getVirtualServerNode(const char* vsId,
                                  PRBool fUpdateFromServer)
{
    if (!vsId)
        return 0;
    if (initInstanceVSS() != PR_TRUE)
        return 0;

    if (fUpdateFromServer == PR_TRUE)
    {
        if (updateVSNode(vsId) != PR_TRUE)
            return 0;
    }
    StatsVirtualServerNode* vss = vssInstance_->getFirstVSNode();
    const StatsVirtualServerNode* vssNode = statsFindNodeFromList(vss, vsId);
    if (!vssNode)
        lastErrorMsg_ = sclerrmsgVSNotFound;
    return vssNode;
}

//-----------------------------------------------------------------------------
// StatsClient::getVirtualServerTailNode
//-----------------------------------------------------------------------------

const StatsVirtualServerNode*
StatsClient::getVirtualServerTailNode()
{
    if (initInstanceVSS() != PR_TRUE)
        return 0;
    StatsVirtualServerNode* vss = vssInstance_->getFirstVSNode();
    return vss;
}

//-----------------------------------------------------------------------------
// StatsClient::getWebModuleList
//-----------------------------------------------------------------------------

const StatsStringVector&
StatsClient::getWebModuleList(void)
{
    updateWebModuleList();
    return webModulesList_;
}

//-----------------------------------------------------------------------------
// StatsClient::getWebModuleNode
//-----------------------------------------------------------------------------

const StatsWebModuleNode*
StatsClient::getWebModuleNode(const char* webModuleName,
                              PRBool fUpdateFromServer)
{
    if (!webModuleName)
        return 0;
    if (updateWebModuleList() != PR_TRUE)
        return 0;

    if (fUpdateFromServer == PR_TRUE)
    {
        if (updateWebModuleData(webModuleName) != PR_TRUE)
        {
            return 0;
        }
    }
    StatsVirtualServerNode* vss = vssInstance_->getFirstVSNode();
    StatsWebModuleNode* wms = 0;
    wms = StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
    if (!wms)
        lastErrorMsg_ = sclerrmsgWebappNotFound;
    return wms;
}

//-----------------------------------------------------------------------------
// StatsClient::getServletNode
//-----------------------------------------------------------------------------

const StatsServletJSPNode*
StatsClient::getServletNode(const char* webModuleName,
                            const char* servletName)
{
    if ((!webModuleName) || (!servletName))
        return 0;

    updateWebModuleList();

    StatsVirtualServerNode* vss = vssInstance_->getFirstVSNode();
    StatsWebModuleNode* wms = 0;
    wms = StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
    if (!wms)
    {
        lastErrorMsg_ = sclerrmsgWebappNotFound;
        return 0;
    }
    StatsServletJSPNode* sjs = 0;
    sjs = StatsManagerUtil::getServletSlot(wms, servletName);
    if (!sjs)
        lastErrorMsg_ = sclerrmsgServletNotFound;
    return sjs;
}


//-----------------------------------------------------------------------------
// StatsClient::getCpuInfoNode
//-----------------------------------------------------------------------------

const StatsCpuInfoNode*
StatsClient::getCpuInfoNode(PRBool fUpdateFromServer)
{
    StatsCpuInfoNode* cps = 0;
#ifdef PLATFORM_SPECIFIC_STATS_ON
    if ((statsHdr_.cps) && (fUpdateFromServer == PR_FALSE))
    {
        return statsHdr_.cps;
    }
    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger, statsmsgReqGetCpuInfo);
    if (fSuccess != PR_TRUE)
    {
        return 0;
    }

    const int sizeCpuSlot = sizeof(StatsCpuInfoSlot);
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    int recvLen = bufferReader.getRemainingBytesCount();

    // Receive bytes must be multiples of cpu slots size
    if (recvLen % sizeCpuSlot != 0)
        return 0;
    int nCpuCount = recvLen / sizeCpuSlot;
    statsAssureNodes(statsHdr_.cps, &statsHdr_, nCpuCount);
    int nIndex = 0;
    cps = statsHdr_.cps;
    for (nIndex = 0; nIndex < nCpuCount; ++nIndex)
    {
        StatsCpuInfoSlot* cpuInfo = 0;
        cpuInfo = (StatsCpuInfoSlot*) bufferReader.readBuffer(sizeCpuSlot);
        PR_ASSERT(cpuInfo);
        if (!cpuInfo)
            return 0;
        PR_ASSERT(cps);
        memcpy(&cps->cpuInfo, cpuInfo, sizeCpuSlot);
        cps = cps->next;
    }
    cps = statsHdr_.cps;
#endif
    return cps;
}

//-----------------------------------------------------------------------------
// StatsClient::getAccumulatedVSSlot
//-----------------------------------------------------------------------------

const StatsAccumulatedVSSlot*
StatsClient::getAccumulatedVSSlot(PRBool fUpdateFromServer)
{
    if (initInstanceVSS() != PR_TRUE)
        return NULL;
    if (fUpdateFromServer == PR_TRUE)
    {
        if (updateAccumulatedVSData() != PR_TRUE)
            return NULL;
    }
    return vssInstance_->getAccumulatedVSSlot();
}

//-----------------------------------------------------------------------------
// StatsClient::getStatsXml
//-----------------------------------------------------------------------------

PRBool
StatsClient::getStatsXml(NSString& strStatsXml, const char* queryString)
{
    return sendRecvStatsRequest(statsmsgReqGetStatsXMLData,
                                statsmsgReqGetStatsXMLDataAck,
                                queryString,
                                strStatsXml);
}

//-----------------------------------------------------------------------------
// StatsClient::getPerfDump
//-----------------------------------------------------------------------------

PRBool
StatsClient::getPerfDump(NSString& strPerfDump, const char* queryString)
{
    return sendRecvStatsRequest(statsmsgReqGetServiceDump,
                                statsmsgReqGetServiceDumpAck,
                                queryString,
                                strPerfDump);
}

//-----------------------------------------------------------------------------
// StatsClient::updateStatsHeader
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateStatsHeader(void)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger,
                                           statsmsgReqGetStatsHeader);
    if (fSuccess != PR_TRUE)
    {
        return fSuccess;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    const int slotSize = sizeof(StatsHeader);
    const void* recvBuffer = bufferReader.readBuffer(slotSize);

    if (!recvBuffer) // Insufficent data
        return PR_FALSE;
    memcpy(&statsHdr_.hdrStats, recvBuffer, slotSize);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateJdbcConnectionPools
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateJdbcConnectionPools(int pid)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    if (!procNodesHandler_)
        return PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger,
                                           statsmsgReqGetJdbcConnPools,
                                           &pid, sizeof(pid));
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess =  procNodesHandler_->updateJdbcConnectionPools(pid,
                                                             bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateJvmMgmtData
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateJvmMgmtData(int pid)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    if (!procNodesHandler_)
        return PR_FALSE;
    if (procNodesHandler_->isJvmMgmtStatsEnabled(pid) <= 0)
    {
        // No jvm stats enabled so do nothing.
        return PR_TRUE;
    }
    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger,
                                           statsmsgReqGetJvmMgmtData,
                                           &pid, sizeof(pid));
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess =  procNodesHandler_->updateJvmMgmtData(pid, bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateSessionReplicationData
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateSessionReplicationData()
{
    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger,
                                           statsmsgReqGetSessReplData);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess =  procNodesHandler_->updateSessionReplicationData(bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateVSList
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateVSList(void)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger, statsmsgReqGetVSList);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }

    StringArrayBuff strMsgBuff;
    messenger.getStringArrayBuffer(strMsgBuff);
    int nCountVS = strMsgBuff.getStringCount();
    char** vsIdsArray = strMsgBuff.getStringArray();
    fSuccess = vssInstance_->initVSS(nCountVS, vsIdsArray);

    if (fSuccess == PR_TRUE)
    {
        fillStringsInVector(vsIds_, nCountVS, vsIdsArray);
    }

    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateVSNode
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateVSNode(const char* vsId)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    PRBool fSuccess = PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    fSuccess = sendRecvStatsRequestString(messenger,
                                          statsmsgReqGetStatsVirtualServerInfo,
                                          vsId);
    if (fSuccess != PR_TRUE)
    {
        return fSuccess;
    }
    // Save VirtualServer Info
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess = vssInstance_->updateVSSlot(vsId, bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateAllVSNodes
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateAllVSNodes(void)
{
    int nIndex = 0;
    PRBool fSuccess = PR_TRUE;
    int nCountVSIds = vsIds_.length();
    for (nIndex = 0; nIndex < nCountVSIds; ++nIndex)
    {
        if (updateVSNode(*vsIds_[nIndex]) != PR_TRUE)
        {
            fSuccess = PR_FALSE;
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateWebModuleList
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateWebModuleList(void)
{
    if (initInstanceVSS() != PR_TRUE)
        return PR_FALSE;

    if (fWebModulesInit_ == PR_TRUE)
        return PR_TRUE;

    if (isWebappsEnabled() != PR_TRUE)
    {
        return PR_TRUE;
    }

    StatsMessenger messenger(statsMsgChannel_);
    PRBool fSuccess = sendRecvStatsRequest(messenger,
                                           statsmsgReqGetWebModuleList);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }

    StringArrayBuff strMsgBuff;
    messenger.getStringArrayBuffer(strMsgBuff);

    int nCountWebMod = strMsgBuff.getStringCount();
    char** webModuleNameArray = strMsgBuff.getStringArray();
    fSuccess = vssInstance_->initWebModules(nCountWebMod,
                                            webModuleNameArray);
    if (fSuccess == PR_TRUE)
    {
        fillStringsInVector(webModulesList_, nCountWebMod, webModuleNameArray);
    }
    if (webModulesList_.length() > 0)
    {
        fWebModulesInit_ = PR_TRUE;
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateWebModuleData
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateWebModuleData(const char* webModuleName)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    PRBool fSuccess = PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    fSuccess = sendRecvStatsRequestString(messenger,
                                          statsmsgReqGetWebModuleData,
                                          webModuleName);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess = vssInstance_->updateWebModuleData(bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateAllWebModules
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateAllWebModules(void)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    PRBool fSuccess = PR_FALSE;
    int nCountWebMod = webModulesList_.length();
    if (nCountWebMod == 0)
        return PR_TRUE; // Webmodule list is not yet initialized so do nothing.
    StatsMessenger messenger(statsMsgChannel_);
    fSuccess = sendRecvStatsRequest(messenger, statsmsgReqGetAllWebModuleData);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess = vssInstance_->updateWebModuleData(bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateAllServlets
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateAllServlets(const char* webModuleName)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    PRBool fSuccess = PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    fSuccess = sendRecvStatsRequestString(messenger,
                                          statsmsgReqGetServletData,
                                          webModuleName);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess = vssInstance_->updateServletData(webModuleName, bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateAllServletsForAllWebModules
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateAllServletsForAllWebModules(void)
{
    int nIndex = 0;
    PRBool fSuccess = PR_TRUE;
    int nCountWebMod = webModulesList_.length();
    for (nIndex = 0; nIndex < nCountWebMod; ++nIndex)
    {
        if (updateAllServlets(*webModulesList_[nIndex]) != PR_TRUE)
        {
            fSuccess = PR_FALSE;
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::updateAccumulatedVSData
//-----------------------------------------------------------------------------

PRBool
StatsClient::updateAccumulatedVSData(void)
{
    if (!statsMsgChannel_)
        return PR_FALSE;
    PRBool fSuccess = PR_FALSE;
    StatsMessenger messenger(statsMsgChannel_);
    fSuccess = sendRecvStatsRequest(messenger, statsmsgReqGetAccumulatedVSData);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    fSuccess = vssInstance_->updateAccumulatedVSData(bufferReader);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::checkServerReconfigured
//-----------------------------------------------------------------------------

PRBool
StatsClient::checkServerReconfigured(void)
{
    if (prevReconfigCount_ < statsHdr_.hdrStats.countReconfig)
        return PR_TRUE;
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsClient::doProcessNodesReconfigure
//-----------------------------------------------------------------------------

PRBool
StatsClient::doProcessNodesReconfigure(void)
{
    PRBool fSuccess = PR_TRUE;
    StatsProcessNode* processNode = 0;
    processNode = procNodesHandler_->getFirstProcessSlot();
    // Listen slots update
    // We don't need to do for all processes. We just need to do it for first
    // process.
    fSuccess = initListenSlots(processNode->procStats.pid);
    if (fSuccess == PR_TRUE)
    {
        freeStringsInVector(listenSocketIds_);
        StatsListenNode* ls = processNode->ls;
        while (ls)
        {
            listenSocketIds_.push(new NSString(ls->lsStats.id));
            ls = ls->next;
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::checkChildRestart
//-----------------------------------------------------------------------------

PRBool
StatsClient::checkChildRestart(void)
{
    if (prevChildDeathCount_ < statsHdr_.hdrStats.countChildDied)
        return PR_TRUE;
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsClient::doChildRestartChanges
//-----------------------------------------------------------------------------

PRBool
StatsClient::doChildRestartChanges(void)
{
    StatsProcessNode* processNode = 0;
    processNode = procNodesHandler_->getFirstProcessSlot();
    // First mark all the process slots as empty.
    while (processNode)
    {
        processNode->procStats.mode = STATS_PROCESS_EMPTY;
        processNode = processNode->next;
    }

    // now mark those processes which are yet active.
    StatsBufferReader recvMsgBuff(0, 0);
    PRBool fSuccess = fetchPidList(recvMsgBuff);
    if (fSuccess != PR_TRUE)
        return PR_FALSE;

    PRInt32 pid = INVALID_PROCESS_ID;
    int nIndex = 0;
    while (recvMsgBuff.readInt32(pid) == PR_TRUE)
    {
        processNode = procNodesHandler_->getProcessSlot(pid);
        if (processNode)
        {
            // So we found a process node so this process still active
            processNode->procStats.mode = STATS_PROCESS_ACTIVE;
        }
    }

    // Now cleanup the empty process slots.
    processNode = procNodesHandler_->getFirstProcessSlot();
    while (processNode)
    {
        if (processNode->procStats.mode == STATS_PROCESS_EMPTY)
        {
            // This is a died process. Cleanup the died node
            processNode->resetObject();
            // Set the pid to invalid value. StatsProcessNodesHandler search
            // for empty process slot using invalid process entries.
            processNode->procStats.pid = INVALID_PROCESS_ID;
        }
        processNode = processNode->next;
    }

    prevChildDeathCount_= statsHdr_.hdrStats.countChildDied;
    // If child again died between the two calls then behaviour is not yet
    // clear.
    return initProcessSlots();
}

//-----------------------------------------------------------------------------
// StatsClient::isJvmEnabled
//-----------------------------------------------------------------------------

PRBool
StatsClient::isJvmEnabled(void)
{
    return (statsHdr_.hdrStats.jvmEnabled == STATS_STATUS_ENABLED) ?
           PR_TRUE : PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsClient::isWebappsEnabled
//-----------------------------------------------------------------------------

PRBool
StatsClient::isWebappsEnabled(void)
{
    return (statsHdr_.hdrStats.webappsEnabled == STATS_STATUS_ENABLED) ?
           PR_TRUE : PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsClient::doReconfigureChanges
//-----------------------------------------------------------------------------

PRBool
StatsClient::doReconfigureChanges(void)
{
    if (prevReconfigCount_ == statsHdr_.hdrStats.countReconfig)
        return PR_TRUE;
    prevReconfigCount_ = statsHdr_.hdrStats.countReconfig;
    vssInstance_->doReconfigureChanges();
    if (updateVSList() != PR_TRUE)
        return PR_FALSE;
    if (updateAllVSNodes() != PR_TRUE)
        return PR_FALSE;
    if (doProcessNodesReconfigure() != PR_TRUE)
        return PR_FALSE;
    fWebModulesInit_ = PR_FALSE;
    if (updateWebModuleList() != PR_TRUE)
        return PR_FALSE;
    if (updateAllWebModules() != PR_TRUE)
        return PR_FALSE;
    if (updateAllServletsForAllWebModules() != PR_TRUE)
        return PR_FALSE;
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::recvNotification
//
// This methods receives notification in non blocking manner. If there is no
// notification available it returns PR_FALSE. If notification is available
// then it read single notification and returns PR_TRUE. It must be called
// in a loop to read all available notification.
//-----------------------------------------------------------------------------

PRBool
StatsClient::recvNotification(StatsNoticeMessages& noticeMsg)
{
    if (statsNoticeChannel_ == NULL)
    {
        return PR_FALSE;
    }
    if (statsNoticeChannel_->peekMessage() == PR_FALSE)
        return PR_FALSE;
    PRBool fSuccess = PR_FALSE;
    if (statsNoticeChannel_->RecvFromServer() != 0)
    {
        WDMessages msgType = statsNoticeChannel_->getLastMsgType();
        if (msgType == wdmsgStatsNotification)
        {
            noticeMsg = (StatsNoticeMessages)
                        statsNoticeChannel_->getStatsMsgType();
            fSuccess = PR_TRUE;
        }
        else
        {
            // Error
            PR_ASSERT(0);
        }
    }
    if (fSuccess == PR_FALSE)
    {
        resetConnection();
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::addNotificationConnection
//-----------------------------------------------------------------------------

PRBool
StatsClient::addNotificationConnection(void)
{
    if (statsNoticeChannel_ != NULL)
    {
        PR_ASSERT(0);
        return PR_TRUE;
    }

    statsNoticeChannel_ =
                StatsServerMessage::connectStatsChannel(connectionPath_);
    if (!statsNoticeChannel_)
        return PR_FALSE;
    if (statsNoticeChannel_->sendStatsRequest(statsmsgIdentifyAsNoticeReceiver,
                                              NULL, 0) != PR_TRUE)
    {
        delete statsNoticeChannel_;
        statsNoticeChannel_ = NULL;
        return PR_FALSE;
    }
    if (lateUpdate() != PR_TRUE) {
        delete statsNoticeChannel_;
        statsNoticeChannel_ = NULL;
        return PR_FALSE;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClient::resetConnection
//-----------------------------------------------------------------------------

void StatsClient::resetConnection(void)
{
    lastErrorMsg_ = sclerrmsgNoConnection; // No connection
    if (statsMsgChannel_)
    {
        delete statsMsgChannel_;
        statsMsgChannel_ = 0;
    }
    if (statsNoticeChannel_)
    {
        delete statsNoticeChannel_;
        statsNoticeChannel_ = 0;
    }
    prevReconfigCount_ = 0;
    fWebModulesInit_  = PR_FALSE;
    delete vssInstance_;
    delete strStoreInstance_;
    delete procNodesHandler_;
    vssInstance_ = 0;
    strStoreInstance_ = 0;
    procNodesHandler_ = 0;
    freeStringsInVector(connQueueIds_);
    freeStringsInVector(threadPoolNames_);
    freeStringsInVector(listenSocketIds_);
    freeStringsInVector(profileNames_);
    freeStringsInVector(vsIds_);
    freeStringsInVector(webModulesList_);
    memset(&statsHdr_.hdrStats, 0, sizeof(statsHdr_.hdrStats));
}

//-----------------------------------------------------------------------------
// StatsClient::sendRecvStatsRequest
//-----------------------------------------------------------------------------

PRBool
StatsClient::sendRecvStatsRequest(StatsMessenger& messenger,
                                  StatsMessages msgType,
                                  const void* sendBuffer,
                                  int sendBufLen)
{
    PRBool fSuccess = PR_FALSE;
    if (!statsMsgChannel_)
    {
        lastErrorMsg_ = sclerrmsgNoConnection; // No connection
        return fSuccess;
    }
    lastErrorMsg_ = sclerrmsgNoError; // Begin with no error.
    if (sendBuffer)
        fSuccess = messenger.sendMsgAndRecvResp(msgType,
                                                sendBuffer, sendBufLen);
    else
        fSuccess = messenger.sendMsgAndRecvResp(msgType);
    if (fSuccess != PR_TRUE)
        processError();
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::sendRecvStatsRequestString
//-----------------------------------------------------------------------------

PRBool
StatsClient::sendRecvStatsRequestString(StatsMessenger& messenger,
                                        StatsMessages msgType,
                                        const char* sendBuffer)
{
    if (!statsMsgChannel_)
    {
        lastErrorMsg_ = sclerrmsgNoConnection; // No connection
        return PR_FALSE;
    }
    lastErrorMsg_ = sclerrmsgNoError; // Begin with no error.
    PRBool fSuccess = messenger.sendMsgAndRecvResp(msgType, sendBuffer);
    if (fSuccess != PR_TRUE)
        processError();
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::sendRecvStatsRequest
//-----------------------------------------------------------------------------

PRBool
StatsClient::sendRecvStatsRequest(StatsMessages msgType,
                                  StatsMessages msgTypeAck,
                                  const char* sendBuffer,
                                  NSString& response)
{
    PRBool fSuccess = PR_FALSE;
    if (!statsMsgChannel_)
    {
        lastErrorMsg_ = sclerrmsgNoConnection; // No connection
        return fSuccess;
    }
    lastErrorMsg_ = sclerrmsgNoError; // Begin with no error.
    StatsMessenger messenger(statsMsgChannel_);
    StatsRespStrReceiver receiver(response);
    fSuccess = messenger.sendMsgAndProcessResp(msgType,
                                               msgTypeAck,
                                               receiver,
                                               sendBuffer);
    if (fSuccess != PR_TRUE)
        processError();
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsClient::setLastError
//-----------------------------------------------------------------------------

void 
StatsClient::setLastError(StatsErrorMessages errMsg)
{
    switch (errMsg)
    {
        case statsmsgErrorNotReady:
        {
            lastErrorMsg_ = sclerrmsgServerBusy;
            break;
        }
        case statsmsgPidNotExist:
        {
            lastErrorMsg_ = sclerrmsgPidNotExist;
            break;
        }
        case statsmsgErrorInReq:
        case statsmsgErrorVSData:
        case statsmsgErrorProcess:
        case statsmsgErrorJvmStatsNoInit:
        case statsmsgErrorNoSessReplStats:
        {
            lastErrorMsg_ = sclerrmsgInternalError;
            break;
        }
        default:
        {
            lastErrorMsg_ = sclerrmsgErrorUnknown;
            break;
        }
   }
}

//-----------------------------------------------------------------------------
// StatsClient::processError
//-----------------------------------------------------------------------------

void
StatsClient::processError(void)
{
    StatsErrorMessages errMsg = statsmsgErrorBegin;
    if (statsMsgChannel_)
    {
        if (statsMsgChannel_->isFileDescriptorError() == PR_TRUE)
        {
            resetConnection();
        }
        else if (statsMsgChannel_->getStatsError(errMsg) == PR_TRUE)
        {
            // We got the error code in lastErrorMsg_
            setLastError(errMsg);
        }
        else
        {
            lastErrorMsg_ = sclerrmsgErrorUnknown;
            PR_ASSERT(0); // It should not reach here.
        }
    }
}


//-----------------------------------------------------------------------------
// StatsClient static methods
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// StatsClient::freeStringsInVector
//-----------------------------------------------------------------------------

void
StatsClient::freeStringsInVector(StatsStringVector& vectorObj)
{
    NSString* vsCurId = 0;
    while ((vsCurId = vectorObj.pop()) != NULL)
    {
        delete vsCurId;
    }
}

//-----------------------------------------------------------------------------
// StatsClient::fillStringsInVector
//-----------------------------------------------------------------------------

void
StatsClient::fillStringsInVector(StatsStringVector& vectorObj,
                                 int nCount,
                                 char** stringArray)
{
    // Fill the stringArray in vectorObj vector.
    int nCurArrayLength = vectorObj.length();
    int nIndex = 0;
    for (nIndex=0; nIndex < nCount; ++nIndex)
    {
        char* vsIdIndex = stringArray[nIndex];
        if (nIndex < nCurArrayLength)
        {
            NSString* vsCurId = vectorObj[nIndex];
            // Set the new virtual server id. Create the new string if it
            // is null.
            if (vsCurId)
                *vectorObj[nIndex] = vsIdIndex;
            else
                vectorObj[nIndex] = new NSString(vsIdIndex);
        }
        else
        {
            // Push the new vs index at the end.
            vectorObj.push(new NSString(vsIdIndex));
        }
    }

    // pop the rest of the elements and delete if any.
    nCurArrayLength = vectorObj.length();
    for (nIndex=nCount; nIndex < nCurArrayLength; ++nIndex)
    {
        NSString* str = vectorObj.pop();
        if (str)
        {
            delete str;
        }
    }
    PR_ASSERT(vectorObj.length() == nCount);
}





////////////////////////////////////////////////////////////////

// StatsClientManager Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// StatsClientManager::StatsClientManager
//-----------------------------------------------------------------------------

StatsClientManager::StatsClientManager() :
                    statsClientHash_(32) // initial size
{
    clientMgrLock_ = PR_NewLock();
}

//-----------------------------------------------------------------------------
// StatsClientManager::~StatsClientManager
//-----------------------------------------------------------------------------

StatsClientManager::~StatsClientManager()
{
    // TODO :
    // delete the stats client in statsClientHash_
    if (clientMgrLock_)
    {
        PR_DestroyLock(clientMgrLock_);
    }
}

//-----------------------------------------------------------------------------
// StatsClientManager::getHashEntry
//-----------------------------------------------------------------------------

StatsClientManager::StatsClientHashEntry*
StatsClientManager::getHashEntry(const char* serverId)
{
    StatsClientHashEntry* clientHashEntry = (StatsClientHashEntry*)
                                statsClientHash_.lookup((void*) serverId);
    if (clientHashEntry == NULL)
    {
        clientHashEntry = new StatsClientHashEntry;
        clientHashEntry->client = 0;
        clientHashEntry->statsMsgChannel = 0;
        statsClientHash_.insert((void*) serverId, clientHashEntry);
    }
    return clientHashEntry;
}

//-----------------------------------------------------------------------------
// StatsClientManager::tryNewStatsConnection
//
// Try to connect to instance. If successful then returns the connected
// StatsClient pointer. On unix platforms since the stats channel is shared
// between watchdog messages and stats messages so connection to the socket
// will be successful. If so then this function will also check if the
// connection with the instance is alive or not (for stats disabled case).
//-----------------------------------------------------------------------------

StatsClient* StatsClientManager::tryNewStatsConnection(
                                   StatsClientHashEntry* clientHashEntry,
                                   const char* serverId,
                                   const char* instanceSockPath) const
{
    StatsServerMessage* statsMsgChannel = clientHashEntry->statsMsgChannel;
    if (statsMsgChannel)
    {
        // This happends only on unix platform as stats channel is shared
        // between watchdog and stats messages. The code reach here only if
        // stats are disabled for instance.
        if (statsMsgChannel->peekMessage() == PR_TRUE)
        {
            // It should only happen if connection to instance is closed. No
            // message is expected from instance.
            PR_ASSERT(statsMsgChannel->RecvFromServer() == 0);
            delete statsMsgChannel;
            statsMsgChannel = NULL;
            clientHashEntry->statsMsgChannel = NULL;
        }
        return 0;
    }
    // else fall through

    // Prepare the channel name.
    char channelName[MAX_PATH];
    memset(channelName, 0, sizeof(channelName));
    // On unix platform we need to use the instanceSockPath and then get the
    // socket path name. On windows we need to connect to pipe created by
    // NTStatsServer.
#ifdef XP_UNIX
    if ((instanceSockPath == NULL) || (*instanceSockPath == 0))
        return 0;
    ParentAdmin::buildConnectionPath(instanceSockPath, channelName,
                                     sizeof(channelName));
#else
    NTStatsServer::buildConnectionName(serverId, channelName,
                                       sizeof(channelName));
#endif

    // Try connecting to stats channel.
    PRBool fStatsEnabled = PR_FALSE;
    statsMsgChannel = StatsServerMessage::connectStatsChannelEx(
                                                channelName,
                                                fStatsEnabled);
    StatsClient* client = 0;
    if ((statsMsgChannel != NULL) && (fStatsEnabled == PR_TRUE))
    {
        // Stats connection to instance is made successfully.
        client = new StatsClient();
        if (client->init(channelName, statsMsgChannel) == PR_TRUE)
        {
            clientHashEntry->client = client;
        }
        else
        {
            // This could possibly only happen if some error happens in
            // StatsClient or instance connection is lost. Another time this
            // could happen is if instance is just started and we connected to
            // primordial process and child processes have not yet connected to
            // primordial. In such cases instance returns statsmsgErrorNotReady
            // error message (during initialization inside init). After the
            // instance is up and running properly, it should succeed in next
            // poll iteration.
            delete client;
            client = 0;
        }
        // statsMsgChannel will now be owned by StatsClient object and the
        // destructor of StatsClient will delete it.
        statsMsgChannel = NULL;
    }
    clientHashEntry->statsMsgChannel = statsMsgChannel;
    return client;
}

//-----------------------------------------------------------------------------
// StatsClientManager::getStatsClient
//
// Caller should call unlock for the returned stats client object after it has
// finished using it.
//-----------------------------------------------------------------------------

StatsClient*
StatsClientManager::getStatsClient(const char* serverId)
{
    if (!serverId)
        return 0;
    lock();
    StatsClientHashEntry* clientHashEntry = getHashEntry(serverId);
    if (!clientHashEntry)
    {
        // Error. It should not happen.
        PR_ASSERT(0);
        unlock();
        return 0;
    }
    StatsClient* client = clientHashEntry->client;
    if (client)
    {
        client->lock();
    }
    unlock();
    return client;
}

//-----------------------------------------------------------------------------
// StatsClientManager::connectInstance
//
// connect to the instance whose server.xml path is given by serverXmlPath.
// Caller should call unlock for the returned stats client object after it has
// finished using it.
//-----------------------------------------------------------------------------

StatsClient*
StatsClientManager::connectInstance(const char* serverId,
                                    const char* instanceSockPath)
{
    if (!serverId)
        return 0;
    lock();
    StatsClientHashEntry* clientHashEntry = getHashEntry(serverId);
    if (!clientHashEntry)
    {
        // Error. It should not happen.
        PR_ASSERT(0);
        unlock();
        return 0;
    }
    StatsClient* client = clientHashEntry->client;
    if (client == NULL)
    {
        client = tryNewStatsConnection(clientHashEntry,
                                       serverId, instanceSockPath);
    }
    if (client)
    {
        client->lock();
    }
    unlock();
    return client;
}

//-----------------------------------------------------------------------------
// StatsClientManager::freeConnection
//-----------------------------------------------------------------------------

void
StatsClientManager::freeConnection(const char* serverId)
{
    lock();
    StatsClientHashEntry* clientHashEntry = (StatsClientHashEntry*)
                                statsClientHash_.lookup((void*) serverId);
    if (clientHashEntry)
    {
        statsClientHash_.remove((void*) serverId);
        StatsClient* client = clientHashEntry->client;
        if (client)
        {
            // We need to make sure that no other threads are using this stats
            // client. As getStatsClient locks the client before returning the
            // StatsClient pointer, if any other thread is using the
            // StatsClient pointer then acquiring the lock will make sure that
            // other threads have released the lock and hence stopped using
            // this client.
            client->lock();
            client->unlock();
            // Deletion seems to be safe here as all other threads have
            // released the lock.
            delete client;
        }
        delete clientHashEntry;
        clientHashEntry = 0;
    }
    unlock();
    return;
}

//-----------------------------------------------------------------------------
//
//               StatsClientManager static members
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// StatsClientManager::getDefaultManager
//-----------------------------------------------------------------------------

StatsClientManager*
StatsClientManager::getDefaultManager(void)
{
    if (defaultStatsClientMgr_ == NULL)
        defaultStatsClientMgr_ = new StatsClientManager();
    return defaultStatsClientMgr_;
}

