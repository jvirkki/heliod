#include "httpdaemon/statsbkupmgr.h"    // StatsBackupManager
#include "httpdaemon/statsmanager.h"    // StatsManager

StatsBackupManager* StatsBackupManager::backupManager_ = 0;

////////////////////////////////////////////////////////////////

// StatsProcessNodesHandler Class members

////////////////////////////////////////////////////////////////

StatsProcessNodesHandler::StatsProcessNodesHandler(int nChildren,
                                                   int nCountProfileBuckets) :
                               nChildren_(nChildren),
                               nCountProfileBuckets_(nCountProfileBuckets)
{
    PR_ASSERT(nChildren_ >= 1);
    processSlot_ = 0;
    accumulatedProcNode_ = 0;
    createProcessSlots();
}

StatsProcessNodesHandler::~StatsProcessNodesHandler(void)
{
    deleteProcessSlots();
}


//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::createProcessSlots
//-----------------------------------------------------------------------------

void StatsProcessNodesHandler::createProcessSlots(void)
{
    if (nChildren_ <= 0)
        return;
    int nIndex = 0;
    processSlot_ = new StatsProcessNode[nChildren_];
    StatsProcessNode* procSlot = processSlot_;
    for (nIndex = 0; nIndex < nChildren_; ++nIndex)
    {
        if (nIndex != nChildren_ - 1)
        {
            procSlot->next = (procSlot + 1);
        }
        else
        {
            procSlot->next = 0;
        }
        ++procSlot;
    }
    // If there is only one child we don't need to do any accumulation so it is
    // just a pointer. Be careful during deletion.
    if (nChildren_ == 1)
    {
        accumulatedProcNode_ = processSlot_;
    }
    else
    {
        accumulatedProcNode_ = new StatsProcessNode();
    }
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::doReconfigureChanges
//-----------------------------------------------------------------------------

PRBool StatsProcessNodesHandler::doReconfigureChanges(void)
{
    // Right now, no code is needed here. Listen slots are taken care by
    // initListenSlots. Jdbc pools, we don't need to destroy. If during
    // reconfig, some of the jdbc pools are deleted, older stats will still be
    // there.  Threads, Thread pool and connection queue can't change during
    // reconfig so we don't need to handle here.
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::copyConnectionQueues
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::copyConnectionQueues(StatsProcessNode* procSlot,
                                               StatsBufferReader& bufferReader)
{
    StatsConnectionQueueNode* connQueue = procSlot->connQueue;
    const int nSizeConnQueueSlot = sizeof(StatsConnectionQueueSlot);
    while (connQueue)
    {
        const void* buffer = bufferReader.readBuffer(nSizeConnQueueSlot);
        if (! buffer)
            return PR_FALSE;
        memcpy(&connQueue->connQueueStats, buffer, nSizeConnQueueSlot);
        connQueue = connQueue->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::copyThreadPools
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::copyThreadPools(StatsProcessNode* procSlot,
                                          StatsBufferReader& bufferReader)
{
    const int nSizeThreadPoolBucket = sizeof(StatsThreadPoolBucket);
    StatsThreadPoolNode* threadPool = procSlot->threadPool;
    while (threadPool)
    {
        const void* buffer = bufferReader.readBuffer(nSizeThreadPoolBucket);
        if (! buffer)
            return PR_FALSE;
        memcpy(&threadPool->threadPoolStats, buffer, nSizeThreadPoolBucket);
        threadPool = threadPool->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::copyProcessNodeMembers
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::copyProcessNodeMembers(StatsProcessNode* procSlot,
                                                 StatsBufferReader& bufferReader)
{
    const int slotSize = sizeof(StatsProcessSlot);
    const int nSizeConnQueueSlot = sizeof(StatsConnectionQueueSlot);
    const int nSizeThreadPoolBucket = sizeof(StatsThreadPoolBucket);

    StatsProcessSlot* procStats = &procSlot->procStats;
    int nCountConnQueue = procStats->countConnectionQueues;
    int nCountThreadPools = procStats->countThreadPools;
    int nConnQueueBytes = nCountConnQueue * nSizeConnQueueSlot;
    int nThreadPoolBytes = nCountThreadPools * nSizeThreadPoolBucket;

    int nRemnBytes = bufferReader.getRemainingBytesCount();

    if (nRemnBytes != (nConnQueueBytes + nThreadPoolBytes))
    {
        return PR_FALSE;
    }

    int nIndex = 0;
    statsAssureNodes(procSlot->connQueue, procSlot, nCountConnQueue);
    statsAssureNodes(procSlot->threadPool, procSlot, nCountThreadPools);
    if (copyConnectionQueues(procSlot, bufferReader) != PR_TRUE)
        return PR_FALSE;
    return copyThreadPools(procSlot, bufferReader);
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::deleteProcessSlots
//-----------------------------------------------------------------------------

void StatsProcessNodesHandler::deleteProcessSlots(void)
{
    delete[] processSlot_;
    processSlot_ = 0;
    if (nChildren_ > 1)
    {
        // Delete only if there is more than one child.
        delete accumulatedProcNode_;
    }
    accumulatedProcNode_ = 0;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::getPidFromBuffer
//
// Caller must ensure the buffer length of the buffer to be
// sizeof(StatsProcessSlot)
//-----------------------------------------------------------------------------

int StatsProcessNodesHandler::getPidFromBuffer(const void* buffer)
{
    StatsProcessSlot* procSlotSrc = 0;
    STATS_BUFFER_ACCESS(StatsProcessSlot,
                        procSlotSrc,
                        buffer);
    return procSlotSrc->pid;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::getProcessSlotIndex
//
// Return the position of processSlot which contains the process id = pid.
// Return -1 if not found.
//-----------------------------------------------------------------------------

int StatsProcessNodesHandler::getProcessSlotIndex(int pid)
{
    StatsProcessNode* procSlot = getProcessSlot(pid);
    return (procSlot ? (procSlot - processSlot_) : -1);
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::getPidFromIndex
//-----------------------------------------------------------------------------

int StatsProcessNodesHandler::getPidFromIndex(int nIndex)
{
    StatsProcessNode* procSlot = statsElementAt(processSlot_, nIndex);
    if (procSlot)
    {
        return procSlot->procStats.pid;
    }
    return INVALID_PROCESS_ID;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::getProcessSlot
//-----------------------------------------------------------------------------

StatsProcessNode* StatsProcessNodesHandler::getProcessSlot(int pid) const
{
    return statsFindNodeFromList(processSlot_, pid);
}


//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::initializeProcessSlot
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::initializeProcessSlot(StatsBufferReader& bufferReader)
{
    int slotSize = sizeof(StatsProcessSlot);
    if (bufferReader.getRemainingBytesCount() < slotSize)
    {
        return PR_FALSE;
    }
    const void* buffer = bufferReader.readBuffer(slotSize);
    int pid = getPidFromBuffer(buffer);
    StatsProcessNode* procSlot = getEmptyProcessSlot();
    if (!procSlot)
        return PR_FALSE;
    StatsProcessSlot* procStats = &procSlot->procStats;
    // This will also set the mode to STATS_PROCESS_ACTIVE
    memcpy(procStats, buffer, slotSize);
    // Delete the older jdbc pool nodes.
    if (procSlot->jdbcPoolNode)
    {
        statsFreeNodeList(procSlot->jdbcPoolNode);
        procSlot->jdbcPoolNode = 0;
    }
    return copyProcessNodeMembers(procSlot, bufferReader);
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::updateProcessSlot
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::updateProcessSlot(StatsBufferReader& bufferReader)
{
    int slotSize = sizeof(StatsProcessSlot);
    if (bufferReader.getRemainingBytesCount() < slotSize)
    {
        return PR_FALSE;
    }
    const void* buffer = bufferReader.readBuffer(slotSize);
    int pid = getPidFromBuffer(buffer);
    StatsProcessNode* procSlot = getProcessSlot(pid);
    if (!procSlot)
        return PR_FALSE;
    StatsProcessSlot* procStats = &procSlot->procStats;
    memcpy(procStats, buffer, slotSize);
    return copyProcessNodeMembers(procSlot, bufferReader);
}


//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::resetThreadSlots
//-----------------------------------------------------------------------------

PRBool StatsProcessNodesHandler::resetThreadSlots(int pid,
                                                  int nThreadCount)
{
    StatsProcessNode* procSlot = getProcessSlot(pid);
    StatsThreadNode* thread = procSlot->thread;
    int nCurThreadCount = statsCountNodes(thread);
    int nDiffCount = nThreadCount - nCurThreadCount;
    if (nDiffCount > 0)
    {
        // We need to create thread slots
        int nIndex = 0;
        for (nIndex = 0; nIndex < nDiffCount; ++nIndex)
        {
            StatsThreadNode* newThread = NULL;
            newThread = new StatsThreadNode(nCountProfileBuckets_);
            // Add the new thread slot to the list
            statsAppendLast(procSlot, newThread);
        }
    }

    // Reset all thread mode to empty.
    thread = procSlot->thread;
    while (thread)
    {
        thread->threadStats.mode = STATS_THREAD_EMPTY;
        thread = thread->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::updateThreadSlot
//-----------------------------------------------------------------------------

PRBool StatsProcessNodesHandler::updateThreadSlot(int pid,
                                                  int nThreadIndex,
                                                  StatsBufferReader& bufferReader)
{
    // ProfileBuckets Size
    int nProfileMemSize = nCountProfileBuckets_ * sizeof(StatsProfileBucket);
    int nExpectedMsgSize = (sizeof(StatsThreadSlot) + nProfileMemSize);
    int nRemnBytes = bufferReader.getRemainingBytesCount();
    if (nRemnBytes != nExpectedMsgSize)
        return PR_FALSE;

    StatsProcessNode* procSlot = getProcessSlot(pid);
    StatsThreadNode* thread = procSlot->thread;
    int nIndex = 0;
    while (thread)
    {
        if (nIndex == nThreadIndex)
            break;
        ++nIndex;
        thread = thread->next;
    }
    if (!thread)
        return PR_FALSE;
    const void* threadBuffer = bufferReader.readBuffer(sizeof(StatsThreadSlot));
    if (!threadBuffer)
        return PR_FALSE;
    memcpy(&thread->threadStats, threadBuffer, sizeof(StatsThreadSlot));

    return StatsBackupManager::copyProfileBucketChain(thread->profile,
                                                      nCountProfileBuckets_,
                                                      bufferReader);
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::initListenSlots
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::initListenSlots(int pid,
                                          StatsBufferReader& bufferReader)
{
    StatsProcessNode* procSlot = getProcessSlot(pid);
    if (procSlot == 0)
        return PR_FALSE;
    StatsListenNode* ls = procSlot->ls;
    // Reset the previous data in ls nodes.
    while(ls)
    {
        memset(&ls->lsStats, 0, sizeof(ls->lsStats));
        ls = ls->next;
    }

    ls = procSlot->ls;

    PRBool fSuccess = PR_TRUE;
    while (1)
    {
        if (bufferReader.isMoreDataAvailable() != PR_TRUE)
            break;
        const int sizeListenSlot = sizeof(StatsListenSlot);
        const void* lsBuffer = bufferReader.readBuffer(sizeListenSlot);
        if (lsBuffer == NULL)
        {
            if (bufferReader.getRemainingBytesCount() != 0)
            {
                // Error
                fSuccess = PR_FALSE;
            }
            break;
        }
        StatsListenSlot* srcLsPtr = 0;
        STATS_BUFFER_ACCESS(StatsListenSlot,
                            srcLsPtr,
                            lsBuffer);
        if (!ls)
        {
            // There is no element in list left so we need to create new
            // listener node.
            StatsListenNode* lsNew = 0;
            lsNew = StatsManagerUtil::allocListenSlot(srcLsPtr->id, procSlot);
            if (lsNew == NULL)
            {
                // Error
                fSuccess = PR_FALSE;
                break;
            }
            memcpy(&lsNew->lsStats, srcLsPtr, sizeof(lsNew->lsStats));
        }
        else
        {
            // Copy into previous existed listener node.
            memcpy(&ls->lsStats, srcLsPtr, sizeof(ls->lsStats));
            ls = ls->next;
        }
    }
    if (ls)
    {
        statsTrimList(procSlot, ls);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::updateJdbcConnectionPools
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::updateJdbcConnectionPools(
                                        int pid,
                                        StatsBufferReader& bufferReader)
{
    StatsProcessNode* procSlot = getProcessSlot(pid);
    if (!procSlot)
        return PR_FALSE;
    PRBool fSuccess = PR_TRUE;
    while (1)
    {
        if (bufferReader.isMoreDataAvailable() != PR_TRUE)
            break;
        const char* poolName = NULL;
        const void* jdbcPoolBuffer = NULL;
        const int sizeJdbcSlot = sizeof(StatsJdbcConnPoolSlot);
        fSuccess = bufferReader.readStringAndSlot(sizeJdbcSlot,
                                                  poolName, jdbcPoolBuffer);
        if (fSuccess != PR_TRUE)
            break;
        // Find the Jdbc Connetion pool node. Create if not found.
        StatsJdbcConnPoolNode* poolNode = 0;
        PRBool fNewNode = PR_FALSE;
        poolNode = StatsManagerUtil::getJdbcConnPoolNode(procSlot, poolName,
                                                         fNewNode);
        memcpy(&poolNode->jdbcStats, jdbcPoolBuffer, sizeJdbcSlot);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::isJvmMgmtStatsEnabled
//-----------------------------------------------------------------------------

PRBool StatsProcessNodesHandler::isJvmMgmtStatsEnabled(int pid) const
{
    StatsProcessNode* procSlot = getProcessSlot(pid);
    if (!procSlot)
        return PR_FALSE;
    return procSlot->procStats.jvmManagentStats ? PR_TRUE : PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::updateJvmMgmtData
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::updateJvmMgmtData(int pid,
                                            StatsBufferReader& bufferReader)
{
    StatsProcessNode* procSlot = getProcessSlot(pid);
    if (!procSlot)
        return PR_FALSE;
    PRBool fSuccess = PR_TRUE;
    if (bufferReader.isMoreDataAvailable() != PR_TRUE)
        return PR_TRUE; // No stats available.
    const void* jvmSlotBuffer = 0;
    jvmSlotBuffer = bufferReader.readBuffer(sizeof(StatsJvmManagementSlot));
    if (!jvmSlotBuffer)
        return PR_FALSE;
    const int vmVersionIndex = 0;
    const int vmNameIndex = 1;
    const int vmVendorIndex = 2;
    const int nStringCount = 3;
    const char* strBuffer[nStringCount];

    // Read the string data.
    if (bufferReader.readStrings(nStringCount, strBuffer) != PR_TRUE)
        return PR_FALSE;

    StatsJvmManagementNode* jvmMgmtNode = procSlot->jvmMgmtNode;
    if (!jvmMgmtNode)
    {
        procSlot->jvmMgmtNode = jvmMgmtNode = new StatsJvmManagementNode;
        // Assign the strings only once as they won't change
        jvmMgmtNode->vmVersion = strBuffer[vmVersionIndex];
        jvmMgmtNode->vmName = strBuffer[vmNameIndex];
        jvmMgmtNode->vmVendor = strBuffer[vmVendorIndex];
    }
    memcpy(&jvmMgmtNode->jvmMgmtStats, jvmSlotBuffer,
           sizeof(StatsJvmManagementSlot));
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::updateSessionReplicationData
//-----------------------------------------------------------------------------

PRBool
StatsProcessNodesHandler::updateSessionReplicationData(
                                            StatsBufferReader& bufferReader)
{
    StatsProcessNode* procNode = getFirstProcessSlot();
    if (!procNode)
        return PR_FALSE;
    if (bufferReader.isMoreDataAvailable() != PR_TRUE)
        return PR_TRUE; // No stats available.
    const int nSlotSize = sizeof(StatsSessionReplicationSlot);
    const void* sessReplBuffer = bufferReader.readBuffer(nSlotSize);
    if (sessReplBuffer == NULL)
        return PR_FALSE; // not enough data.

    // Specify the position of strings.
    const int clusterMembersIndex = 0;
    const int currBackupInstIdIndex = 1;
    const int stateIndex = 2;
    const int nStringCount = 3;
    const char* strBuffer[nStringCount];

    // Read the string data.
    if (bufferReader.readStrings(nStringCount, strBuffer) != PR_TRUE)
        return PR_FALSE;

    if (procNode->sessReplNode == NULL)
    {
        procNode->sessReplNode = new StatsSessionReplicationNode;
    }
    StatsSessionReplicationNode* sessReplNode = procNode->sessReplNode;
    memcpy(&sessReplNode->sessReplStats, sessReplBuffer, nSlotSize);
    sessReplNode->listClusterMembers = strBuffer[clusterMembersIndex];
    sessReplNode->currentBackupInstanceId = strBuffer[currBackupInstIdIndex];
    sessReplNode->state = strBuffer[stateIndex];

    // Mark all the underneath instance nodes as invalid.
    sessReplNode->markInstanceNodes(STATS_SR_INST_NODE_INVALID);

    int nInstanceIndex = 0;
    while(bufferReader.isMoreDataAvailable() == PR_TRUE)
    {
        const char* instanceId = bufferReader.readString();
        if (!instanceId)
            return PR_FALSE;
        PRInt32 countWebappStores = 0;
        if (bufferReader.readInt32(countWebappStores) != PR_TRUE)
            return PR_FALSE;
        PRInt32 nCurInstanceIndex = 0;
        if (bufferReader.readInt32(nCurInstanceIndex) != PR_TRUE)
            return PR_FALSE;
        if (nInstanceIndex != nCurInstanceIndex) {
            // data corrupted
            return PR_FALSE;
        }
        ++nInstanceIndex;
        PRBool fNewNode = PR_FALSE;
        StatsSessReplInstanceNode* instanceNode =
                     StatsManagerUtil::getSessReplInstanceNode(sessReplNode,
                                                               instanceId,
                                                               fNewNode);
        PR_ASSERT(instanceNode != NULL);
        if (!instanceNode)
            return PR_FALSE;
        // Now mark this instance node as active as stats are getting updated
        // for this instance node.
        instanceNode->markNode(STATS_SR_INST_NODE_ACTIVE);

        // Mark all the webapp store nodes underneath this instance node as
        // invalid. During updating the web app store node these will be
        // marked as active.
        instanceNode->markWebappStoreNodes(STATS_SR_WEBAPP_NODE_INVALID);

        PRInt32 nIndex = 0;
        for (nIndex = 0; nIndex < countWebappStores; ++nIndex)
        {
            // Read 3 strings (storeId, vsId and uri) followed by 2 32 bit
            // integers.
            const int storeIdIndex = 0;
            const int vsIdIndex = 1;
            const int uriIndex = 2;
            const int nStoreStringCount = 3;
            const char* storeStringBuffer[nStoreStringCount];
            if (bufferReader.readStrings(nStoreStringCount,
                                         storeStringBuffer) != PR_TRUE)
                return PR_FALSE;
            PRInt32 countReplicatedSessions = 0;
            if (bufferReader.readInt32(countReplicatedSessions) != PR_TRUE)
                return PR_FALSE;
            PRInt32 nWebappStoreIndex = 0;
            if (bufferReader.readInt32(nWebappStoreIndex) != PR_TRUE)
                return PR_FALSE;
            if (nWebappStoreIndex != nIndex)
            {
                // data corrupted
                return PR_FALSE;
            }
            const char* storeId = storeStringBuffer[storeIdIndex];
            StatsWebAppSessionStoreNode* wassNode = NULL;
            wassNode = StatsManagerUtil::getWebAppSessStoreNode(instanceNode,
                                                                storeId,
                                                                fNewNode);
            wassNode->vsId = storeStringBuffer[vsIdIndex];
            wassNode->uri = storeStringBuffer[uriIndex];
            wassNode->countReplicatedSessions = countReplicatedSessions;

            // As stats are available for this node so mark it as active node.
            wassNode->markNode(STATS_SR_WEBAPP_NODE_ACTIVE);
        }

        // Delete the inactive webapp store nodes.
        instanceNode->deleteInactiveWebappStoreNodes();
    }
    sessReplNode->deleteInactiveInstanceNodes();
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsProcessNodesHandler::accumulateProcessData
//
// Right now this function only accumulates the request bucket and thread
// counts. Other accumulation may be needed later.
//-----------------------------------------------------------------------------

void StatsProcessNodesHandler::accumulateProcessData(void)
{
    if (nChildren_ == 1)
        return; // There is no accumulation needed.
    if (!accumulatedProcNode_)
    {
        PR_ASSERT(0);
        return;
    }
    StatsProcessSlot* accumulatedSlot = &accumulatedProcNode_->procStats;
    memset(accumulatedSlot, 0, sizeof(*accumulatedSlot));
    int nIndex = 0;
    StatsProcessNode* procSlot = processSlot_;
    StatsRequestBucket* requestBucket = &accumulatedSlot->requestBucket;
    while (procSlot)
    {
        const StatsProcessSlot& procStats = procSlot->procStats;
        StatsManagerUtil::accumulateRequest(requestBucket,
                                            &procStats.requestBucket);
        accumulatedSlot->countThreads += procStats.countThreads;
        accumulatedSlot->countIdleThreads += procStats.countIdleThreads;
        procSlot = procSlot->next;
    }
}


////////////////////////////////////////////////////////////////

// StatsVSNodesHandler Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// StatsVSNodesHandler::StatsVSNodesHandler
//-----------------------------------------------------------------------------

StatsVSNodesHandler::StatsVSNodesHandler(void)
{
    vssNodesList_ = 0;
    nCountProfileBuckets_ = 0;
    memset(&accumulatedVSStats_, 0, sizeof(accumulatedVSStats_));
}

//-----------------------------------------------------------------------------
// StatsVSNodesHandler::~StatsVSNodesHandler
//-----------------------------------------------------------------------------

StatsVSNodesHandler::~StatsVSNodesHandler(void)
{
    freeVSS();
}

//-----------------------------------------------------------------------------
// StatsVSNodesHandler::freeVSS
//-----------------------------------------------------------------------------

void StatsVSNodesHandler::freeVSS(void)
{
    statsFreeNodeList(vssNodesList_);
    vssNodesList_ = 0;
    return;
}

//-----------------------------------------------------------------------------
// StatsVSNodesHandler::initVSS
//
// This function make sure that vs node exists for all ids pointed by array
// vsIds. If nodes doesn't exist it create the new nodes. Any other nodes which
// is not there in vsIds array (and which are marked empty before during
// reconfigure) are removed.
//-----------------------------------------------------------------------------

PRBool StatsVSNodesHandler::initVSS(int nCountVS, char** vsIds)
{
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCountVS; ++nIndex)
    {
        StatsVirtualServerNode* vss = statsFindNodeFromList(vssNodesList_,
                                                            vsIds[nIndex]);
        if (vss)
        {
            // Mark them as active.  During update mode will be set correctly.
            vss->vssStats.mode = STATS_VIRTUALSERVER_ACTIVE;
            // Node already exists so continue.
            continue;
        }
        // Create a new node and append at the end.
        vss = new StatsVirtualServerNode(NULL, nCountProfileBuckets_);
        if (!vssNodesList_)
        {
            vssNodesList_ = vss;
        }
        else
        {
            StatsVirtualServerNode* vssLastNode = 0;
            vssLastNode = statsFindLastNode(vssNodesList_);
            vssLastNode->next = vss;
        }
        vss->vssStats.mode = STATS_VIRTUALSERVER_ACTIVE;
        const int sizeId = sizeof(vss->vssStats.id);
        PR_ASSERT(strlen(vsIds[nIndex]) < sizeId);
        memset(vss->vssStats.id, 0, sizeId);
        strncpy(vss->vssStats.id, vsIds[nIndex], sizeId - 1);
    }
    statsFreeEmptyNodes(this, (StatsVirtualServerNode*) NULL);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsVSNodesHandler::doReconfigureChanges
//-----------------------------------------------------------------------------

PRBool StatsVSNodesHandler::doReconfigureChanges(void)
{
    StatsVirtualServerNode* vss = vssNodesList_;
    while (vss)
    {
        // Mark all VS node's to empty so that if some of the virtual servers
        // are deleted during reconfiguration, their node will be removed by
        // initVSS.
        vss->vssStats.mode = STATS_VIRTUALSERVER_EMPTY;

        // Set all web modules mode to empty.
        StatsWebModuleNode* wms = vss->wms;
        while (wms)
        {
            wms->wmsStats.mode = STATS_WEBMODULE_MODE_EMPTY;
            wms = wms->next;
        }
        vss = vss->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsVSNodesHandler::initWebModules
//-----------------------------------------------------------------------------

PRBool StatsVSNodesHandler::initWebModules(int nCountWebMod,
                                           char** webModuleNameArray)
{
    PRBool fSuccess = PR_TRUE;
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCountWebMod; ++nIndex)
    {
        char* webModuleName = webModuleNameArray[nIndex];
        StatsVirtualServerNode* vss = 0;
        vss = StatsManagerUtil::getVSSFromWebModuleName(vssNodesList_,
                                                        webModuleName);
        if (!vss)
        {
            // There is no vss for exist for this webmodule. This is error.
            fSuccess = PR_FALSE;
            continue; // Still continue to create other web module list.
        }
        StatsWebModuleNode* wms =
            StatsManagerUtil::getWebModuleSlot(vss, webModuleName);
        PR_ASSERT((wms != NULL));
        if (wms)
        {
            // Set the mode to enabled first. It will be set appripriately
            // during update. This is to avoid gettting deleted.
            wms->wmsStats.mode = STATS_WEBMODULE_MODE_ENABLED;
        }
    }

    // Now delete any nodes which are marked empty (they will not be in
    // webModuleNameArray)
    StatsVirtualServerNode* vss = getFirstVSNode();
    while (vss)
    {
        statsFreeEmptyNodes(vss, (StatsWebModuleNode*) NULL);
        vss = vss->next;
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsVSNodesHandler::updateVSSlot
//-----------------------------------------------------------------------------

PRBool StatsVSNodesHandler::updateVSSlot(const char* vsId,
                                         StatsBufferReader& bufferReader)
{
    StatsVirtualServerNode* vss = statsFindNodeFromList(vssNodesList_,
                                                        vsId);
    const int vsSlotSize = sizeof(StatsVirtualServerSlot);
    const void* vssBuffer = bufferReader.readBuffer(vsSlotSize);
    if (!vssBuffer)
        return PR_FALSE;        // Insufficient buffer
    memcpy(&vss->vssStats, vssBuffer, sizeof(vss->vssStats));

    const char* hostnames = bufferReader.readString();
    if (!hostnames)
        return PR_FALSE;        // Insufficient buffer
    vss->hostnames = hostnames;
    const char* interfaces = bufferReader.readString();
    if (!interfaces)
        return PR_FALSE;        // Insufficient buffer
    vss->interfaces = interfaces;
    return StatsBackupManager::copyProfileBucketChain(vss->profile,
                                                      nCountProfileBuckets_,
                                                      bufferReader);
}

//-----------------------------------------------------------------------------
// StatsVSNodesHandler::updateAccumulatedVSData
//-----------------------------------------------------------------------------

PRBool
StatsVSNodesHandler::updateAccumulatedVSData(StatsBufferReader& bufferReader)
{
    const int slotSize = sizeof(StatsAccumulatedVSSlot);
    const void* buffer = bufferReader.readBuffer(slotSize);
    if (!buffer)
        return PR_FALSE;        // Insufficient buffer
    memcpy(&accumulatedVSStats_, buffer, slotSize);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsVSNodesHandler::updateWebModuleData
//-----------------------------------------------------------------------------

PRBool StatsVSNodesHandler::updateWebModuleData(StatsBufferReader& bufferReader)
{
    StatsVirtualServerNode* vss = vssNodesList_;
    while (1)
    {
        if (bufferReader.isMoreDataAvailable() != PR_TRUE)
            break;
        const char* webModuleName = 0;
        const int sizeWMSlot = sizeof(StatsWebModuleSlot);
        const void* wmsBuffer = 0;
        PRBool fSuccess = bufferReader.readStringAndSlot(sizeWMSlot,
                                                         webModuleName,
                                                         wmsBuffer);
        if (fSuccess != PR_TRUE)
        {
            // Error
            return PR_FALSE;
        }
        const int sizeWMCacheSlot = sizeof(StatsWebModuleCacheSlot);
        // Read web module cache slot.
        const void* wmCacheBuffer = bufferReader.readBuffer(sizeWMCacheSlot);
        if (!wmCacheBuffer)
            return PR_FALSE; // Error
        StatsWebModuleNode* wms = 0;
        wms = StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
        if (wms == NULL)
        {
            // Error
            return PR_FALSE;
        }
        memcpy(&wms->wmsStats, wmsBuffer, sizeWMSlot);
        memcpy(&wms->wmCacheStats, wmCacheBuffer, sizeWMCacheSlot);
    }
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// StatsVSNodesHandler::updateServletData
//-----------------------------------------------------------------------------

PRBool StatsVSNodesHandler::updateServletData(const char* webModuleName,
                                              StatsBufferReader& bufferReader)
{
    StatsVirtualServerNode* vss = vssNodesList_;
    PRBool fSuccess = PR_TRUE;

    StatsWebModuleNode* wms = 0;
    wms = StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
    if (wms == NULL)
    {
        // Error
        fSuccess = PR_FALSE;
        return fSuccess;
    }
    while (1)
    {
        if (bufferReader.isMoreDataAvailable() != PR_TRUE)
            break;
        const char* servletName = 0;
        const int sizeSJSlot = sizeof(StatsServletJSPSlot);
        const void* sjsBuffer = 0;
        PRBool fSuccess = bufferReader.readStringAndSlot(sizeSJSlot,
                                                         servletName,
                                                         sjsBuffer);
        if (fSuccess != PR_TRUE)
        {
            // Error
            return PR_FALSE;
        }
        // Find the servlet slot. Create if not found.
        StatsServletJSPNode* sjs = 0;
        sjs = StatsManagerUtil::getServletSlot(wms, servletName);

        memcpy(&sjs->sjsStats, sjsBuffer, sizeSJSlot);
    }
    return fSuccess;
}





////////////////////////////////////////////////////////////////

// StatsBackupManager Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// StatsBackupManager::StatsBackupManager
//-----------------------------------------------------------------------------

StatsBackupManager::StatsBackupManager(int nChildren) :
                    nChildren_(nChildren),
                    procNodesHandler_(nChildren,
                                      StatsManager::getProfileBucketCount())
{
    defaultProcessNode_ = 0;
    init();
}

//-----------------------------------------------------------------------------
// StatsBackupManager::~StatsBackupManager
//-----------------------------------------------------------------------------

StatsBackupManager::~StatsBackupManager(void)
{
    // This descructor is called by replacement child i.e the childs
    // which are created if old child process dies.
    freeVSSForAllChilds();
}

//-----------------------------------------------------------------------------
// StatsBackupManager::markProcessSlotEmpty
//
// markProcessSlotEmpty don't reset the pid. It just marks it to
// be empty so that child could pick up the last dead process
// index.
//-----------------------------------------------------------------------------

StatsProcessNode* StatsBackupManager::markProcessSlotEmpty(int pid)
{
    StatsProcessNode* procSlot = getProcessSlot(pid);
    if (procSlot)
    {
        ereport(LOG_VERBOSE, "Marked slot for pid = %d as empty",
                procSlot->procStats.pid);
        procSlot->procStats.mode = STATS_PROCESS_EMPTY;
        return procSlot;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::unmarkLastProcessSlot
//-----------------------------------------------------------------------------

void StatsBackupManager::unmarkLastProcessSlot(int pid)
{
    // pid not used
    int nChildNo = getLastDiedProcessIndex();
    if (nChildNo == -1)
        return;
    PR_ASSERT(nChildNo <= nChildren_);
    StatsProcessNode* procSlot = getFirstProcessSlot() + nChildNo;
    ereport(LOG_VERBOSE, "Unmarked slot for pid = %d",
            procSlot->procStats.pid);
    procSlot->procStats.pid = INVALID_PROCESS_ID;
    return;
}


//-----------------------------------------------------------------------------
// StatsBackupManager::getLastDiedProcessIndex
//-----------------------------------------------------------------------------

int StatsBackupManager::getLastDiedProcessIndex(void)
{
    StatsProcessNode* procSlot = getFirstProcessSlot();
    int nIndex = -1;
    while (procSlot)
    {
        ++nIndex;
        if ((procSlot->procStats.pid != INVALID_PROCESS_ID) &&
            (procSlot->procStats.mode == STATS_PROCESS_EMPTY))
        {
            return nIndex;
        }
        procSlot = procSlot->next;
    }
    return nIndex;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::init
//-----------------------------------------------------------------------------

void StatsBackupManager::init(void)
{
    vssConsolidated_ = 0;
    int nCountProfileBuckets = StatsManager::getProfileBucketCount();

    // Allocate array for virtual servers for various processes.
    vssPerProcNodesArray_ = new StatsVSNodesHandler[nChildren_];
    int nIndex = 0;
    for (nIndex = 0; nIndex < nChildren_; ++nIndex)
    {
        StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[nIndex];
        vssNodesHandler.setProfileBucketCount(nCountProfileBuckets);
    }

    // Allocate vssConsolidated_ if there is more than one child
    if (nChildren_ > 1)
    {
        vssConsolidated_ = new StatsVSNodesHandler();
        vssConsolidated_->setProfileBucketCount(nCountProfileBuckets);
    }
    else
    {
        vssConsolidated_ = vssPerProcNodesArray_;
    }
}

//-----------------------------------------------------------------------------
// StatsBackupManager::freeVSSForAllChilds
//-----------------------------------------------------------------------------

void StatsBackupManager::freeVSSForAllChilds(void)
{
    // Free VS Slots
    int nIndex = 0;
    delete[] vssPerProcNodesArray_;
    vssPerProcNodesArray_ = 0;
    if (nChildren_ > 1)
    {
        delete vssConsolidated_;
    }
}

//-----------------------------------------------------------------------------
// StatsBackupManager::doReconfigureChanges
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::doReconfigureChanges(void)
{
    procNodesHandler_.doReconfigureChanges();
    int nIndex = 0;
    for (nIndex = 0; nIndex < nChildren_; ++nIndex)
    {
        vssPerProcNodesArray_[nIndex].doReconfigureChanges();
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateVSData
//
// nVSIndex represents the index number for which vss and vssAggregate
// belongs
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::aggregateVSData(StatsVirtualServerNode* vss,
                                           StatsVirtualServerNode* vssAggregate,
                                           int nVSIndex)
{
    PRBool fSuccess = PR_TRUE;
    int nChildIndex = 0;
    for (nChildIndex = 0; nChildIndex < nChildren_; ++nChildIndex)
    {
        if (nChildIndex == 0)
        {
            // Copy the data
            StatsManagerUtil::copyVirtualServerSlot(vssAggregate,
                                                    vss);
            StatsManagerUtil::
                copyProfileBucketChain(vssAggregate->profile,
                                       vss->profile);
        }
        else
        {
            // Now do the aggregation
            // First get the vss for nth Child
            StatsVSNodesHandler&
            vssNodesHandler = vssPerProcNodesArray_[nChildIndex];
            StatsVirtualServerNode* vssCurChild = vssNodesHandler.getFirstVSNode();
            vssCurChild = statsElementAt(vssCurChild, nVSIndex);

            if (!vssCurChild)
            {
                fSuccess = PR_FALSE;
                break;
            }
            StatsManagerUtil::aggregateVirtualServerSlot(vssAggregate,
                                                         vssCurChild);
            StatsManagerUtil::
                aggregateProfileBucketChain(vssAggregate->profile,
                                            vssCurChild->profile);
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateVSData
//
// Aggregate VS data for virtual server vsId. If vsId is zero then do
// aggregate for all virtual servers.
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::aggregateVSData(const char* vsId)
{
    // We don't need to do any consolidation if there is only 1 child
    if (nChildren_ <= 1)
        return PR_TRUE;
    StatsVirtualServerNode* vssAggregate = vssConsolidated_->getFirstVSNode();
    int nCountVS = vsIdArray_.getStringCount();
    char** vsIds = vsIdArray_.getStringArray();
    StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[0];
    StatsVirtualServerNode* vss = vssNodesHandler.getFirstVSNode();
    PRBool fSuccess = PR_TRUE;
    int nCount = 0;
    while (vss)
    {
        if (nCount > nCountVS)
        {
            // Error Condition. It should not come here
            fSuccess = PR_FALSE;
            break;
        }
        if ((!vsId) || (strcmp(vss->vssStats.id, vsId) == 0))
        {
            fSuccess = aggregateVSData(vss, vssAggregate, nCount);
            if (vsId)           // Did the aggregation of vsId so break
                break;
        }
        if (fSuccess == PR_FALSE)
            break;
        vss = vss->next;
        vssAggregate = vssAggregate->next;
        ++nCount;
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateWebModuleData
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::aggregateWebModuleData(const char* webModuleName)
{
    // We don't need to do any consolidation if there is only 1 child
    if (nChildren_ <= 1)
        return PR_TRUE;

    PRBool fSuccess = PR_TRUE;
    int nChildIndex = 0;
    StatsVirtualServerNode* vssAggregate = vssConsolidated_->getFirstVSNode();
    StatsWebModuleNode* wmsAggregate = 0;
    wmsAggregate = StatsManagerUtil::findWebModuleSlotInVSList(vssAggregate,
                                                               webModuleName);
    if (!wmsAggregate)
    {
        return PR_FALSE;
    }
    for (nChildIndex = 0; nChildIndex < nChildren_; ++nChildIndex)
    {
        StatsVSNodesHandler& vssNodesHandler =
                                        vssPerProcNodesArray_[nChildIndex];
        StatsVirtualServerNode* vss = vssNodesHandler.getFirstVSNode();
        StatsWebModuleNode* wms = 0;
        wms = StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
        if (!wms)
        {
            PR_ASSERT(0);
            return PR_FALSE;
        }
        if (nChildIndex == 0)
        {
            if (StatsManagerUtil::copyWebModuleSlot(wmsAggregate,
                                                    wms) != PR_TRUE)
            {
                fSuccess = PR_FALSE;
            }
        }
        else
        {
            if (StatsManagerUtil::aggregateWebModuleSlot(wmsAggregate,
                                                         wms) != PR_TRUE)
            {
                fSuccess = PR_FALSE;
            }
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateAllWebModules
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::aggregateAllWebModules()
{
    // We don't need to do any consolidation if there is only 1 child
    if (nChildren_ <= 1)
        return PR_TRUE;

    PRBool fSuccess = PR_TRUE;
    StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[0];
    StatsVirtualServerNode* vss = vssNodesHandler.getFirstVSNode();
    while (vss)
    {
        StatsWebModuleNode* wms = vss->wms;
        while (wms)
        {
            if (aggregateWebModuleData(wms->name) != PR_TRUE)
            {
                fSuccess = PR_FALSE;
            }
            wms = wms->next;
        }
        vss = vss->next;
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateServletData
//-----------------------------------------------------------------------------

PRBool
StatsBackupManager::aggregateServletData(StatsServletJSPNode* sjsAggregate,
                                         const char* webModuleName,
                                         const char* servletName)
{
    PRBool fSuccess = PR_TRUE;
    for (int nChildIndex = 0; nChildIndex < nChildren_; ++nChildIndex)
    {
        StatsVSNodesHandler& vssNodesHandler =
                                        vssPerProcNodesArray_[nChildIndex];
        StatsVirtualServerNode* vss = vssNodesHandler.getFirstVSNode();
        StatsWebModuleNode* wms = 0;
        wms = StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
        if (!wms)
        {
            PR_ASSERT(0);
            return PR_FALSE;
        }
        // Find the servlet node for this child, if it doesn't exist, create a
        // empty node.
        StatsServletJSPNode* sjs = StatsManagerUtil::getServletSlot(wms,
                                                                    servletName);

        if (nChildIndex == 0)
        {
            if (StatsManagerUtil::copyServletSlot(sjsAggregate, sjs) != PR_TRUE)
            {
                fSuccess = PR_FALSE;
            }
        }
        else
        {
            if (StatsManagerUtil::aggregateServletSlot(sjsAggregate,
                                                       sjs) != PR_TRUE)
            {
                fSuccess = PR_FALSE;
            }
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateServletsForWebModule
//-----------------------------------------------------------------------------

PRBool
StatsBackupManager::aggregateServletsForWebModule(StatsWebModuleNode* wms)
{
    StatsVirtualServerNode* vssAggregate = vssConsolidated_->getFirstVSNode();
    StatsWebModuleNode* wmsAggregate = 0;
    wmsAggregate = StatsManagerUtil::findWebModuleSlotInVSList(vssAggregate,
                                                               wms->name);
    if (!wmsAggregate)
    {
        return PR_FALSE;
    }

    StatsServletJSPNode* sjs = wms->sjs;
    PRBool fSuccess = PR_TRUE;
    while (sjs)
    {
        // Find the aggregate node. If it doesn't exist then create one.
        StatsServletJSPNode* sjsAggregate =
                    StatsManagerUtil::getServletSlot(wmsAggregate, sjs->name);
        if (aggregateServletData(sjsAggregate, wms->name, sjs->name) != PR_TRUE)
        {
            fSuccess = PR_FALSE;
        }
        sjs = sjs->next;
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateServletsForWebModule
//-----------------------------------------------------------------------------

PRBool
StatsBackupManager::aggregateServletsForWebModule(const char* webModuleName)
{
    if (nChildren_ <= 1)
        return PR_TRUE;
    int nChildNo = 0; // First child
    StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[nChildNo];
    StatsVirtualServerNode* vss = vssNodesHandler.getFirstVSNode();
    StatsWebModuleNode* wms =
            StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
    if (!wms)
    {
        return PR_FALSE;
    }
    return aggregateServletsForWebModule(wms);
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateAllServlets
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::aggregateAllServlets()
{
    if (nChildren_ <= 1)
        return PR_TRUE;

    int nChildNo = 0; // First child
    StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[nChildNo];
    StatsVirtualServerNode* vss = vssNodesHandler.getFirstVSNode();
    PRBool fSuccess = PR_TRUE;
    while (vss)
    {
        StatsWebModuleNode* wms = vss->wms;
        while (wms)
        {
            if (aggregateServletsForWebModule(wms) != PR_TRUE)
            {
                fSuccess = PR_FALSE;
            }
            wms = wms->next;
        }
        vss = vss->next;
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::aggregateAccumulatedVSData
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::aggregateAccumulatedVSData(void)
{
    // Even if there is a single process, still we need to copy the stats.
    StatsAccumulatedVSSlot* accumVSStats = NULL;
    accumVSStats = StatsManager::getAccumulatedVSSlot();
    PR_ASSERT(accumVSStats != NULL);

    PRBool fSuccess = PR_TRUE;
    int nChildIndex = 0;
    for (nChildIndex = 0; nChildIndex < nChildren_; ++nChildIndex)
    {
        StatsVSNodesHandler& vssNodesHandler =
                                vssPerProcNodesArray_[nChildIndex];
        const StatsAccumulatedVSSlot* accumVSStatsProc =
                                vssNodesHandler.getAccumulatedVSSlot();
        const int slotSize = sizeof(StatsAccumulatedVSSlot);
        if (nChildIndex == 0)
        {
            // Copy the data
            memcpy(accumVSStats, accumVSStatsProc, slotSize);
        }
        else
        {
            StatsManagerUtil::accumulateAccumulatedVSSlot(accumVSStats,
                                                          accumVSStatsProc);
        }
    }
    accumVSStats->responseTimeAvgBucket.oneMinuteAverage /= nChildren_;
    accumVSStats->responseTimeAvgBucket.fiveMinuteAverage /= nChildren_;
    accumVSStats->responseTimeAvgBucket.fifteenMinuteAverage /= nChildren_;
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::initVSIdList
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::initVSIdList(StatsBufferReader& bufferReader)
{
    int bufferLen = bufferReader.getRemainingBytesCount();
    const void* buffer = bufferReader.readBuffer(bufferLen);
    if (vsIdArray_.reinitialize(buffer, bufferLen) != PR_TRUE)
        return PR_FALSE;
    int nCountVS = vsIdArray_.getStringCount();
    char** vsIds = vsIdArray_.getStringArray();

    int nChildNo = 0;
    for (nChildNo = 0; nChildNo < nChildren_; ++nChildNo)
    {
        vssPerProcNodesArray_[nChildNo].initVSS(nCountVS, vsIds);
    }
    if (nChildren_ > 1)
    {
        vssConsolidated_->initVSS(nCountVS, vsIds);
    }

    // Setup the VirtualServerSlot Ptr
    StatsManager::setupVirtualServerSlot(vssConsolidated_->getFirstVSNode());
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::copyProfileBucketChain
//-----------------------------------------------------------------------------

PRBool
StatsBackupManager::copyProfileBucketChain(StatsProfileNode* dest,
                                           int nCountProfileBuckets,
                                           StatsBufferReader& bufferReader)
{
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCountProfileBuckets; ++nIndex)
    {
        if (!dest)
            return PR_FALSE;
        const void* buffer =
            bufferReader.readBuffer(sizeof(StatsProfileBucket));
        if (!buffer)
            return PR_FALSE;
        memcpy(&dest->profileStats, buffer, sizeof(StatsProfileBucket));
        dest = dest->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::updateVSSlot
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::updateVSSlot(int pid,
                                        const char* vsId,
                                        StatsBufferReader& bufferReader)
{
    int nCountProfileBuckets = StatsManager::getProfileBucketCount();
    // ProfileBuckets Size
    int nProfileMemSize = nCountProfileBuckets * sizeof(StatsProfileBucket);
    const int vsSlotSize = sizeof(StatsVirtualServerSlot);
    if (bufferReader.getRemainingBytesCount() < vsSlotSize + nProfileMemSize)
        return PR_FALSE;

    int nChildNo = getProcessSlotIndex(pid);
    if (nChildNo == -1)
        return PR_FALSE;
    if (!vssPerProcNodesArray_)
        return PR_FALSE;
    StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[nChildNo];
    return vssNodesHandler.updateVSSlot(vsId, bufferReader);
}

//-----------------------------------------------------------------------------
// StatsBackupManager::updateAccumulatedVSData
//-----------------------------------------------------------------------------

PRBool
StatsBackupManager::updateAccumulatedVSData(int pid,
                                            StatsBufferReader& bufferReader)
{
    int nChildNo = getProcessSlotIndex(pid);
    if (nChildNo == -1)
        return PR_FALSE;
    if (!vssPerProcNodesArray_)
        return PR_FALSE;
    StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[nChildNo];
    return vssNodesHandler.updateAccumulatedVSData(bufferReader);
}

//-----------------------------------------------------------------------------
// StatsBackupManager::initWebModuleList
//
// Update webmodule list
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::initWebModuleList(int pid,
                                             StatsBufferReader& bufferReader)
{
    // This must be called after vs slot has been initialized i.e. after
    // initVSS
    // For multiple child this list will be reinitialized again but it must be
    // the same for all child
    int bufferLen = bufferReader.getRemainingBytesCount();
    const void* buffer = bufferReader.readBuffer(bufferLen);
    if (webModuleArray_.reinitialize(buffer, bufferLen) != PR_TRUE)
        return PR_FALSE;
    int nChildNo = getProcessSlotIndex(pid);
    if (nChildNo == -1)
        return PR_FALSE;
    int nCountWebMod = webModuleArray_.getStringCount();
    char** webModuleNameArray = webModuleArray_.getStringArray();
    PRBool fSuccess = PR_FALSE;
    StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[nChildNo];
    fSuccess = vssNodesHandler.initWebModules(nCountWebMod,
                                              webModuleNameArray);
    if (fSuccess != PR_TRUE)
    {
        return PR_FALSE;
    }
    if (nChildren_ > 1)
    {
        // For Children > 1 it may be called several times but it won't do
        // any harm
        vssConsolidated_->initWebModules(nCountWebMod,
                                         webModuleNameArray);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::updateWebModuleData
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::updateWebModuleData(int pid,
                                               StatsBufferReader& bufferReader)
{
    int nChildNo = getProcessSlotIndex(pid);
    if (nChildNo == -1)
        return PR_FALSE;
    if (!vssPerProcNodesArray_)
        return PR_FALSE;
    return vssPerProcNodesArray_[nChildNo].updateWebModuleData(bufferReader);
}

//-----------------------------------------------------------------------------
// StatsBackupManager::updateServletData
//
// Update the servlet slots. Create the slot if doesn't exist.
//-----------------------------------------------------------------------------

PRBool StatsBackupManager::updateServletData(int pid,
                                             const char* webModuleName,
                                             StatsBufferReader& bufferReader)
{
    int nChildNo = getProcessSlotIndex(pid);
    if (nChildNo == -1)
        return PR_FALSE;
    if (!vssPerProcNodesArray_)
        return PR_FALSE;
    return vssPerProcNodesArray_[nChildNo].updateServletData(webModuleName,
                                                             bufferReader);
}

//-----------------------------------------------------------------------------
// StatsBackupManager::setupDiedChildVSS
//-----------------------------------------------------------------------------

void StatsBackupManager::setupDiedChildVSS(int nChildNo)
{
    StatsVSNodesHandler& vssNodesHandler = vssPerProcNodesArray_[nChildNo];
    StatsVirtualServerNode* vss = vssNodesHandler.detachVSNodesList();
    StatsManager::setupVirtualServerSlot(vss);
}

//-----------------------------------------------------------------------------
// StatsBackupManager::setupNewProcessNode
//
// This function manipulates the StatsHeaderNode's process node. Inside
// primordial process StatsHeaderNode's process node should be a list of child
// process node. This function replace the StatsHeader's process node with it's
// own allocated process Node. This saves the original process Node. If child
// dies and restarts it calls resetDefaultStatsProcessNode to recover the
// original process node. Child deletes the old primordial list of process
// nodes.
//-----------------------------------------------------------------------------

void StatsBackupManager::setupNewProcessNode()
{
    StatsHeaderNode* hdr = StatsManager::getHeader();
    StatsProcessNode* procSlot = getFirstProcessSlot();
    StatsProcessNode* hdrProcSlot = hdr->process;
    if (hdrProcSlot != procSlot)
    {
        // Save the processSlot allocated by StatsManager::initEarly so that if
        // the process is restarted it receives the default process Slot.
        defaultProcessNode_ = hdrProcSlot;
        StatsManager::setProcessNode(procSlot);
    }
}

//-----------------------------------------------------------------------------
// StatsBackupManager::resetDefaultStatsProcessNode
//
// Recovers the store process node inside StatsHeaderNode.
//-----------------------------------------------------------------------------

void StatsBackupManager::resetDefaultStatsProcessNode()
{
    if ( defaultProcessNode_ )
        StatsManager::setProcessNode(defaultProcessNode_);
}

//-----------------------------------------------------------------------------
// StatsBackupManager::createBackupManager
//-----------------------------------------------------------------------------

StatsBackupManager* StatsBackupManager::createBackupManager(int nChildren)
{
    if (!backupManager_)
    {
        backupManager_ = new StatsBackupManager(nChildren);
    }
    return backupManager_;
}

//-----------------------------------------------------------------------------
// StatsBackupManager::processChildLateInit
//-----------------------------------------------------------------------------

void StatsBackupManager::processChildLateInit(void)
{
    if (!backupManager_)
    {
        // This will happen if child is created first time not
        // as the result of other child death
        return;
    }
    // It will come here when a child is created as a replacement of
    // old child.
    int nChildNo = backupManager_->getLastDiedProcessIndex();
    backupManager_->resetDefaultStatsProcessNode();
    if (nChildNo != -1)
    {
        backupManager_->setupDiedChildVSS(nChildNo);
    }
    delete backupManager_;
    backupManager_ = 0;
    return;

}

