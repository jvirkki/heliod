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

#include "httpdaemon/statsnodes.h"
#include "httpdaemon/statsutil.h"            // INVALID_PROCESS_ID
#include "httpdaemon/configurationmanager.h"    // ConfigurationManager
#include "httpdaemon/ListenSocketConfig.h"      // ListenSocketConfig
#include "base/util.h"          // util_snprintf
#include "httpdaemon/vsconf.h"  // VirtualServer

////////////////////////////////////////////////////////////////

// StatsProfileNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsProfileNode::StatsProfileNode
//-----------------------------------------------------------------------------

StatsProfileNode::StatsProfileNode()
{
    memset(&profileStats, 0, sizeof(profileStats));
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsProfileNode::~StatsProfileNode
//-----------------------------------------------------------------------------

StatsProfileNode::~StatsProfileNode()
{
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsProfileNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsProfileNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(profileStats);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsVirtualServerNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsVirtualServerNode::StatsVirtualServerNode
//-----------------------------------------------------------------------------

StatsVirtualServerNode::StatsVirtualServerNode(const VirtualServer* vs,
                                               int nProfileBucketsCount):
                                               wms(NULL),
                                               next(NULL),
                                               profile(NULL)
{
    nProfileBucketsCount_ = nProfileBucketsCount;
    if (nProfileBucketsCount > 0)
    {
        profile = new StatsProfileNode[nProfileBucketsCount];
        statsMakeListFromArray(profile, nProfileBucketsCount);
    }
    memset(&vssStats, 0, sizeof(vssStats));
    vssStats.mode = STATS_VIRTUALSERVER_ACTIVE;

    int nVsIdLen = sizeof(vssStats.id);
    if (vs)
    {
        strncpy(vssStats.id, vs->name, (nVsIdLen - 1));
        vssStats.id[nVsIdLen - 1] = 0;
    }
    else
    {
        memset(vssStats.id, 0, nVsIdLen);
    }
    if (vs)
    {
        setHostNames(vs);
        setInterfaces(vs);
    }
    return;
}


//-----------------------------------------------------------------------------
// StatsVirtualServerNode::~StatsVirtualServerNode
//-----------------------------------------------------------------------------

StatsVirtualServerNode::~StatsVirtualServerNode(void)
{
    delete[] profile;
    profile = NULL;
    statsFreeNodeList(wms);
}

//-----------------------------------------------------------------------------
// StatsVirtualServerNode::setHostNames
//-----------------------------------------------------------------------------

void
StatsVirtualServerNode::setHostNames(const VirtualServer* vs)
{
    hostnames.clear();
    hostnames.ensureCapacity(128);
    int nIndex = 0;
    int nHostCount = vs->getHostCount();
    for (nIndex = 0; nIndex < nHostCount; nIndex++)
    {
        hostnames.append((*vs->getHost(nIndex)));
        const char* spaceSeparater = " ";
        if (nIndex != nHostCount - 1)
        {
            hostnames.append(" ");
        }
    }
}

//-----------------------------------------------------------------------------
// StatsVirtualServerNode::setInterfaces
//-----------------------------------------------------------------------------

void
StatsVirtualServerNode::setInterfaces(const VirtualServer* vs)
{
    interfaces.clear();
    interfaces.ensureCapacity(128);
    int nIndex = 0;
    int nItfCount = vs->getHttpListenerNameCount();
    // Calculate the length requirement for assignment
    const Configuration* configuration =
        ConfigurationManager::getConfiguration();
    for (nIndex = 0; configuration && (nIndex < nItfCount); nIndex++)
    {
        const ListenSocketConfig* lsc;
        lsc = configuration->getLsc(*vs->getHttpListenerName(nIndex));
        if (lsc)
        {
            char iface[70] = "";
            util_snprintf(iface, sizeof(iface), "%s:%d",
                          lsc->ip.getStringValue(), lsc->port.getInt32Value());
            interfaces.append(iface);
            const char* spaceSeparater = " ";
            if (nIndex != nItfCount - 1)
            {
                interfaces.append(" ");
            }
        }
    }
    if (configuration) configuration->unref();
}

//-----------------------------------------------------------------------------
// StatsVirtualServerNode::isNullInterfaces
//-----------------------------------------------------------------------------

PRBool
StatsVirtualServerNode::isNullInterfaces(void)
{
    if (interfaces.length() == 0)
        return PR_TRUE;
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsVirtualServerNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsVirtualServerNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(vssStats);
    msgbuff.appendString(hostnames.data());
    msgbuff.appendString(interfaces.data());
    statsGetNodeListData(profile, msgbuff);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsVirtualServerNode::isEmptyNode
//-----------------------------------------------------------------------------

PRBool
StatsVirtualServerNode::isEmptyNode(void)
{
    if (vssStats.mode == STATS_VIRTUALSERVER_EMPTY)
        return PR_TRUE;
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsVirtualServerNode::resetStats
//-----------------------------------------------------------------------------

void
StatsVirtualServerNode::resetStats(void)
{
    memset(&vssStats.requestBucket, 0, sizeof(vssStats.requestBucket));
    StatsProfileNode* profileNode = profile;
    while (profileNode)
    {
        memset(&profileNode->profileStats, 0,
               sizeof(profileNode->profileStats));
        profileNode = profileNode->next;
    }
}


////////////////////////////////////////////////////////////////

// StatsThreadNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsThreadNode::StatsThreadNode
//-----------------------------------------------------------------------------

StatsThreadNode::StatsThreadNode(int nProfileBucketsCount):
                                 next(0),
                                 profile(0)
{
    // Reset the slot to null
    memset(&threadStats, 0, sizeof(threadStats));
    threadStats.timeStarted = PR_Now();
    threadStats.mode = STATS_THREAD_IDLE;

    // Allocate the profile
    nProfileBucketsCount_ = nProfileBucketsCount;
    if (nProfileBucketsCount)
    {
        profile = new StatsProfileNode[nProfileBucketsCount];
        statsMakeListFromArray(profile, nProfileBucketsCount);
    }

    // Create the lock that serializes access to realtime thread statistics
    lock_ = PR_NewLock();

}

//-----------------------------------------------------------------------------
// StatsThreadNode::~StatsThreadNode
//-----------------------------------------------------------------------------

StatsThreadNode::~StatsThreadNode(void)
{
    delete[] profile;
    profile = NULL;
}

//-----------------------------------------------------------------------------
// StatsThreadNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsThreadNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(threadStats);
    statsGetNodeListData(profile, msgbuff);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsListenNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsListenNode::StatsListenNode
//-----------------------------------------------------------------------------

StatsListenNode::StatsListenNode()
{
    memset(&lsStats, 0, sizeof(lsStats));
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsListenNode::~StatsListenNode
//-----------------------------------------------------------------------------

StatsListenNode::~StatsListenNode()
{
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsListenNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsListenNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(lsStats);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsThreadPoolNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsThreadPoolNode::StatsThreadPoolNode
//-----------------------------------------------------------------------------

StatsThreadPoolNode::StatsThreadPoolNode()
{
    memset(&threadPoolStats, 0, sizeof(threadPoolStats));
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsThreadPoolNode::~StatsThreadPoolNode
//-----------------------------------------------------------------------------

StatsThreadPoolNode::~StatsThreadPoolNode()
{
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsThreadPoolNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsThreadPoolNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(threadPoolStats);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsConnectionQueueNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsConnectionQueueNode::StatsConnectionQueueNode
//-----------------------------------------------------------------------------

StatsConnectionQueueNode::StatsConnectionQueueNode()
{
    memset(&connQueueStats, 0, sizeof(connQueueStats));
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsConnectionQueueNode::~StatsConnectionQueueNode
//-----------------------------------------------------------------------------

StatsConnectionQueueNode::~StatsConnectionQueueNode()
{
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsConnectionQueueNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsConnectionQueueNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(connQueueStats);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsJdbcConnPoolNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsJdbcConnPoolNode::StatsJdbcConnPoolNode
//-----------------------------------------------------------------------------

StatsJdbcConnPoolNode::StatsJdbcConnPoolNode(void)
{
    memset(&jdbcStats, 0, sizeof(jdbcStats));
    poolName.ensureCapacity(128); // default pool name size.
    next = 0;
}

//-----------------------------------------------------------------------------
// StatsJdbcConnPoolNode::~StatsJdbcConnPoolNode
//-----------------------------------------------------------------------------

StatsJdbcConnPoolNode::~StatsJdbcConnPoolNode(void)
{
}

//-----------------------------------------------------------------------------
// StatsJdbcConnPoolNode::getNodeData
//-----------------------------------------------------------------------------

PRBool StatsJdbcConnPoolNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    const char* strPoolName = poolName.data();
    if (!strPoolName)
        strPoolName = "";
    msgbuff.appendString(strPoolName);
    msgbuff.appendSlot(jdbcStats);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsJvmManagementNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsJvmManagementNode::StatsJvmManagementNode
//-----------------------------------------------------------------------------

StatsJvmManagementNode::StatsJvmManagementNode(void) :
                                               vmVersion(""),
                                               vmName(""),
                                               vmVendor("")
{
    memset(&jvmMgmtStats, 0, sizeof(jvmMgmtStats));
}

//-----------------------------------------------------------------------------
// StatsJvmManagementNode::~StatsJvmManagementNode
//-----------------------------------------------------------------------------

StatsJvmManagementNode::~StatsJvmManagementNode(void)
{
}

//-----------------------------------------------------------------------------
// StatsJvmManagementNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsJvmManagementNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(jvmMgmtStats);
    msgbuff.appendString(vmVersion);
    msgbuff.appendString(vmName);
    msgbuff.appendString(vmVendor);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsWebAppSessionStoreNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsWebAppSessionStoreNode::StatsWebAppSessionStoreNode
//-----------------------------------------------------------------------------

StatsWebAppSessionStoreNode::StatsWebAppSessionStoreNode(const char* id) :
                                                         storeId(id)
{
    mode = STATS_SR_WEBAPP_NODE_ACTIVE;
    countReplicatedSessions = 0;
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsWebAppSessionStoreNode::~StatsWebAppSessionStoreNode
//-----------------------------------------------------------------------------

StatsWebAppSessionStoreNode::~StatsWebAppSessionStoreNode(void)
{
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsWebAppSessionStoreNode::markNode
//-----------------------------------------------------------------------------

void
StatsWebAppSessionStoreNode::markNode(PRInt32 newModeVal)
{
    mode = newModeVal;
}

//-----------------------------------------------------------------------------
// StatsWebAppSessionStoreNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsWebAppSessionStoreNode::getNodeData(StatsMsgBuff& msgbuff,
                                         PRInt32 nWebappStoreIndex) const
{
    msgbuff.appendString(storeId);
    msgbuff.appendString(vsId);
    msgbuff.appendString(uri);
    // Write the store index as pad for debugging purposes.
    msgbuff.appendInteger(countReplicatedSessions,
                          nWebappStoreIndex++);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsWebAppSessionStoreNode::isEmptyNode
//-----------------------------------------------------------------------------

PRBool
StatsWebAppSessionStoreNode::isEmptyNode(void)
{
    if (mode == STATS_SR_WEBAPP_NODE_INVALID)
        return PR_TRUE;
    return PR_FALSE;
}


////////////////////////////////////////////////////////////////

// StatsSessReplInstanceNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode::StatsSessReplInstanceNode
//-----------------------------------------------------------------------------

StatsSessReplInstanceNode::StatsSessReplInstanceNode(const char* id) :
                                                     instanceId(id),
                                                     countWebappStores(0),
                                                     wassNode(NULL),
                                                     next(NULL)
{
    mode = STATS_SR_INST_NODE_ACTIVE;
}

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode::~StatsSessReplInstanceNode
//-----------------------------------------------------------------------------

StatsSessReplInstanceNode::~StatsSessReplInstanceNode(void)
{
    statsFreeNodeList(wassNode);
}

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode::markNode
//-----------------------------------------------------------------------------

void
StatsSessReplInstanceNode::markNode(PRInt32 newModeVal)
{
    mode = newModeVal;
}

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode::isEmptyNode
//-----------------------------------------------------------------------------

PRBool
StatsSessReplInstanceNode::isEmptyNode(void)
{
    if (mode == STATS_SR_INST_NODE_INVALID)
        return PR_TRUE;
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode::markWebappStoreNodes
//-----------------------------------------------------------------------------

void
StatsSessReplInstanceNode::markWebappStoreNodes(PRInt32 newModeVal)
{
    StatsWebAppSessionStoreNode* curWassNode = wassNode;
    while (curWassNode)
    {
        curWassNode->markNode(newModeVal);
        curWassNode = curWassNode->next;
    }
}

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode::deleteInactiveWebappStoreNodes
//-----------------------------------------------------------------------------

int
StatsSessReplInstanceNode::deleteInactiveWebappStoreNodes(void)
{
    int nCountDeletedNodes =
                statsFreeEmptyNodes(this,
                                    (StatsWebAppSessionStoreNode*) NULL);
    countWebappStores = statsCountNodes(wassNode);
    return nCountDeletedNodes;
}


////////////////////////////////////////////////////////////////

// StatsSessionReplicationNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsSessionReplicationNode::StatsSessionReplicationNode
//-----------------------------------------------------------------------------

StatsSessionReplicationNode::StatsSessionReplicationNode(void) :
                                                 instanceNode(NULL)
{
    memset(&sessReplStats, 0, sizeof(sessReplStats));
}

//-----------------------------------------------------------------------------
// StatsSessionReplicationNode::~StatsSessionReplicationNode
//-----------------------------------------------------------------------------

StatsSessionReplicationNode::~StatsSessionReplicationNode(void)
{
    statsFreeNodeList(instanceNode);
}

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode::markInstanceNodes
//-----------------------------------------------------------------------------

void
StatsSessionReplicationNode::markInstanceNodes(PRInt32 mode)
{
    StatsSessReplInstanceNode* curInstNode = instanceNode;
    while (curInstNode)
    {
        curInstNode->markNode(mode);
        curInstNode = curInstNode->next;
    }
}

//-----------------------------------------------------------------------------
// StatsSessionReplicationNode::deleteInactiveInstanceNodes
//-----------------------------------------------------------------------------

int
StatsSessionReplicationNode::deleteInactiveInstanceNodes(void)
{
    return statsFreeEmptyNodes(this,
                               (StatsSessReplInstanceNode*) NULL);
}

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsSessionReplicationNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(sessReplStats);
    msgbuff.appendString(listClusterMembers);
    msgbuff.appendString(currentBackupInstanceId);
    msgbuff.appendString(state);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsProcessNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsProcessNode::StatsProcessNode
//-----------------------------------------------------------------------------

StatsProcessNode::StatsProcessNode(void):
                                   connQueue(NULL),
                                   threadPool(NULL),
                                   ls(NULL),
                                   thread(NULL),
                                   jdbcPoolNode(NULL),
                                   jvmMgmtNode(NULL),
                                   sessReplNode(NULL),
                                   next(NULL)
{
    memset(&procStats, 0, sizeof(procStats));
    procStats.pid = INVALID_PROCESS_ID;
    procStats.mode = STATS_PROCESS_EMPTY;
}

//-----------------------------------------------------------------------------
// StatsProcessNode::~StatsProcessNode
//-----------------------------------------------------------------------------

StatsProcessNode::~StatsProcessNode(void)
{
    resetObject();
    // Setting next as 0 , if caller tries to access the next node after
    // deletion it will be caught immediately.
    next = 0;
}

//-----------------------------------------------------------------------------
// StatsProcessNode::accumulateThreadData
//-----------------------------------------------------------------------------

void
StatsProcessNode::accumulateThreadData(void)
{
    int countThread = 0;
    int countIdleThreads = 0;
    StatsRequestBucket& requestBucket = procStats.requestBucket;
    memset(&requestBucket, 0, sizeof(requestBucket));
    StatsThreadNode* threadNode = thread;
    while (threadNode)
    {
        if (threadNode->threadStats.mode != STATS_THREAD_EMPTY)
            countThread++;
        if (threadNode->threadStats.mode == STATS_THREAD_IDLE)
            countIdleThreads++;
        const StatsRequestBucket&
            threadRequest = threadNode->threadStats.requestBucket;
        StatsManagerUtil::accumulateRequest(&requestBucket,
                                            &threadRequest);
        threadNode = threadNode->next;
    }
    procStats.countIdleThreads = countIdleThreads;
    procStats.countThreads = countThread;
    return;
}

//-----------------------------------------------------------------------------
// StatsProcessNode::resetObject
//-----------------------------------------------------------------------------

void
StatsProcessNode::resetObject(void)
{
    statsFreeNodeList(connQueue);
    connQueue = 0;
    statsFreeNodeList(threadPool);
    threadPool = 0;
    statsFreeNodeList(ls);
    ls = 0;
    statsFreeNodeList(thread);
    thread = 0;
    statsFreeNodeList(jdbcPoolNode);
    jdbcPoolNode = 0;

    // delete takes care of the null pointers.
    delete jvmMgmtNode;
    jvmMgmtNode = 0;
    delete sessReplNode;
    sessReplNode = 0;

    // Right now, we don't want to reset the pid. caller may still using it.
    // Reset those variables which is used to access underlying nodes.
    procStats.countConnectionQueues = 0;
    procStats.countThreadPools = 0;
    procStats.jvmManagentStats = 0;
    procStats.countIdleThreads = 0;
    procStats.countThreads = 0;
}

//-----------------------------------------------------------------------------
// StatsProcessNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsProcessNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    // Set process stats, followed by process connection queue followed by
    // process thread pools.
    msgbuff.appendSlot(procStats);
    statsGetNodeListData(connQueue, msgbuff);
    statsGetNodeListData(threadPool, msgbuff);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsWebModuleNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsWebModuleNode::StatsWebModuleNode
//-----------------------------------------------------------------------------

StatsWebModuleNode::StatsWebModuleNode(const char* strName) : name(strName)
{
    memset(&wmsStats, 0, sizeof(wmsStats));
    memset(&wmCacheStats, 0, sizeof(wmCacheStats));
    wmCacheStats.fEnabled = PR_FALSE;
    sjs = 0;
    next = 0;
}

//-----------------------------------------------------------------------------
// StatsWebModuleNode::~StatsWebModuleNode
//-----------------------------------------------------------------------------

StatsWebModuleNode::~StatsWebModuleNode(void)
{
    statsFreeNodeList(sjs);
}

//-----------------------------------------------------------------------------
// StatsWebModuleNode::getCacheTypeName
//-----------------------------------------------------------------------------

const char*
StatsWebModuleNode::getCacheTypeName(int cacheType)
{
    switch (cacheType)
    {
        case WM_CACHE_TYPE_BASE_CACHE:
            return "base-cache";
        case WM_CACHE_TYPE_LRU_CACHE:
            return "lru-cache";
        case WM_CACHE_TYPE_MULTI_LRU_CACHE:
            return "multi-lru-cache";
        case WM_CACHE_TYPE_BOUNDED_MULTI_LRU_CACHE:
            return "bounded-multi-lru-cache";
        default:
            return "unknown-cache";
    }

}

//-----------------------------------------------------------------------------
// StatsWebModuleNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsWebModuleNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    const char* namePtr = name.data();
    if (namePtr && (wmsStats.mode != STATS_WEBMODULE_MODE_EMPTY))
    {
        msgbuff.appendString(namePtr);
        msgbuff.appendSlot(wmsStats);
        // Copy the webmodule cache data.
        msgbuff.appendSlot(wmCacheStats);
        return PR_TRUE;
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsWebModuleNode::isEmptyNode
//-----------------------------------------------------------------------------

PRBool
StatsWebModuleNode::isEmptyNode(void)
{
    if (wmsStats.mode == STATS_WEBMODULE_MODE_EMPTY)
        return PR_TRUE;
    return PR_FALSE;
}


////////////////////////////////////////////////////////////////

// StatsServletJSPNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsServletJSPNode::StatsServletJSPNode
//-----------------------------------------------------------------------------

StatsServletJSPNode::StatsServletJSPNode(const char* strName) : name(strName)
{
    memset(&sjsStats, 0, sizeof(sjsStats));
    next = 0;
}

//-----------------------------------------------------------------------------
// StatsServletJSPNode::~StatsServletJSPNode
//-----------------------------------------------------------------------------

StatsServletJSPNode::~StatsServletJSPNode(void)
{
}

//-----------------------------------------------------------------------------
// StatsServletJSPNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsServletJSPNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    const char* servletName = name.data();
    PR_ASSERT(servletName);
    if (!servletName) {
        return PR_FALSE;
    }
    // Append the servlet name and servlet data.
    msgbuff.appendString(servletName);
    msgbuff.appendSlot(sjsStats);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsCpuInfoNode Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsCpuInfoNode::StatsCpuInfoNode
//-----------------------------------------------------------------------------

StatsCpuInfoNode::StatsCpuInfoNode()
{
    memset(&cpuInfo, 0, sizeof(cpuInfo));
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsCpuInfoNode::~StatsCpuInfoNode
//-----------------------------------------------------------------------------

StatsCpuInfoNode::~StatsCpuInfoNode()
{
    next = NULL;
}

//-----------------------------------------------------------------------------
// StatsCpuInfoNode::getNodeData
//-----------------------------------------------------------------------------

PRBool
StatsCpuInfoNode::getNodeData(StatsMsgBuff& msgbuff) const
{
    msgbuff.appendSlot(cpuInfo);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsHeaderNode Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// StatsHeaderNode::StatsHeaderNode
//-----------------------------------------------------------------------------

StatsHeaderNode::StatsHeaderNode()
{
    memset(&hdrStats, 0, sizeof(hdrStats));
    process = NULL;
    vss = NULL;
    cps = NULL;
}

//-----------------------------------------------------------------------------
// StatsHeaderNode::~StatsHeaderNode
//-----------------------------------------------------------------------------

StatsHeaderNode::~StatsHeaderNode()
{
    // It doesn't delete the process node and vss node. In webserver process,
    // header is never deleted. In StatsClient, I think it is not used.
    statsFreeNodeList(cps);
    cps = NULL;
}

