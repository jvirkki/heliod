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

#ifndef _STATSNODES_H
#define _STATSNODES_H

#include "public/iwsstats.h"         // StatsxxxSlot
#include "httpdaemon/libdaemon.h"    // HTTPDAEMON_DLL
#include "support/NSString.h"        // NSString

class VirtualServer;
class StatsMsgBuff;

//----------------------------------------------------------
//
// Stats nodes structures wraps the Stats Slot structures.
// getFirst, setFirst and compareId members of these nodes
// are used by algorithms defined in statsutil.h. These
// methods must conform to the declaration expected by those
// algorithms.
// getNodeData is used by prepareXXXStats methods in
// StatsMsgPreparer.
//
//----------------------------------------------------------

//-----------------------------------------------------------------------------
// StatsProfileNode
//
// Profile bucket Node
//-----------------------------------------------------------------------------

struct StatsProfileNode
{
    StatsProfileBucket profileStats;
    StatsProfileNode* next;

    StatsProfileNode();
    ~StatsProfileNode();
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
};

//-----------------------------------------------------------------------------
// StatsThreadNode
//
// N.B. Unlike other statistics, some thread-specific statistics are updated in
// realtime without acquiring calling StatsManager::lockStatsData().  These
// statistics are protected by StatsThreadNode::lock().
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsThreadNode
{
private:
    PRLock* lock_;
    int nProfileBucketsCount_;

public:
    StatsThreadSlot threadStats;
    StatsProfileNode* profile;
    StatsThreadNode* next;

    StatsThreadNode(int nProfileBucketsCount);
    ~StatsThreadNode(void);
    void lock() { PR_Lock(lock_); }
    void unlock() { PR_Unlock(lock_); }
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
    PRBool compareId(int mode) const;
};

//-----------------------------------------------------------------------------
// StatsListenNode
//
// Stats node for StatsListenSlot
//-----------------------------------------------------------------------------

struct StatsListenNode
{
    StatsListenSlot lsStats;
    StatsListenNode* next;

    StatsListenNode();
    ~StatsListenNode();
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
    PRBool compareId(const char* id) const;
};

//-----------------------------------------------------------------------------
// StatsThreadPoolNode
//
// Stats node for StatsThreadPoolBucket
//-----------------------------------------------------------------------------

struct StatsThreadPoolNode
{
    StatsThreadPoolBucket threadPoolStats;
    StatsThreadPoolNode* next;

    StatsThreadPoolNode();
    ~StatsThreadPoolNode();
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
};

//-----------------------------------------------------------------------------
// StatsConnectionQueueNode
//
// Stats node fo StatsConnectionQueueSlot
//-----------------------------------------------------------------------------

struct StatsConnectionQueueNode
{
    StatsConnectionQueueSlot connQueueStats;
    StatsConnectionQueueNode* next;

    StatsConnectionQueueNode();
    ~StatsConnectionQueueNode();
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
};

//-----------------------------------------------------------------------------
// StatsJdbcConnPoolNode
//
// StatsJdbcConnPoolNode contains the Jdbc connection pool name and jdbc
// connection pool stats.
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsJdbcConnPoolNode
{
    StatsJdbcConnPoolSlot jdbcStats;
    NSString poolName;
    StatsJdbcConnPoolNode* next;

    StatsJdbcConnPoolNode(void);
    ~StatsJdbcConnPoolNode(void);
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
    PRBool compareId(const char* poolNameStr) const;
};

//-----------------------------------------------------------------------------
// StatsJvmManagementNode
//
// Stats node for StatsJvmManagementSlot
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsJvmManagementNode
{
    StatsJvmManagementSlot jvmMgmtStats;
    NSString vmVersion;
    NSString vmName;
    NSString vmVendor;

    StatsJvmManagementNode(void);
    ~StatsJvmManagementNode(void);
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
};

//-----------------------------------------------------------------------------
// StatsWebAppSessionStoreNode
//
// Stats node for web app session store (session replication).  The store is
// uniquely identified by it's storeId. (Unique within one instance)
//
// "mode" value could either be STATS_WEBAPP_STORE_NODE_INVALID or
// STATS_WEBAPP_STORE_NODE_ACTIVE
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsWebAppSessionStoreNode
{
    NSString storeId;
    NSString vsId;
    NSString uri;
    PRInt32 countReplicatedSessions;
    PRInt32 mode;
    StatsWebAppSessionStoreNode* next;

    StatsWebAppSessionStoreNode(const char* id);
    ~StatsWebAppSessionStoreNode(void);
    PRBool isEmptyNode(void);
    PRBool getNodeData(StatsMsgBuff& msgbuff,
                       PRInt32 nWebappStoreIndex) const;
    PRBool compareId(const char* id) const;
    void markNode(PRInt32 newModeVal);
};

//-----------------------------------------------------------------------------
// StatsSessReplInstanceNode
//
// Stats node for session replication instance.
// countWebappStores is the number of webapp store buckets for instance node.
// wassNode points to the list of those web app stores.
//
// "mode" value could either be STATS_INSTANCE_NODE_INVALID or
// STATS_INSTANCE_NODE_ACTIVE
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsSessReplInstanceNode
{
    NSString instanceId;
    PRInt32 mode;
    PRInt32 countWebappStores;
    StatsWebAppSessionStoreNode* wassNode;
    StatsSessReplInstanceNode* next;

    StatsSessReplInstanceNode(const char* id);
    ~StatsSessReplInstanceNode(void);
    void markNode(PRInt32 newModeVal);
    PRBool isEmptyNode(void);
    void markWebappStoreNodes(PRInt32 mode);
    int deleteInactiveWebappStoreNodes(void);

    PRBool compareId(const char* id) const;
    void getFirst(StatsWebAppSessionStoreNode*& wass) const;
    void setFirst(StatsWebAppSessionStoreNode* wass);
};

//-----------------------------------------------------------------------------
// StatsSessionReplicationNode
//
// Stats node for StatsSessionReplicationSlot
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsSessionReplicationNode
{
    StatsSessionReplicationSlot sessReplStats;
    NSString listClusterMembers;
    NSString currentBackupInstanceId;
    NSString state;
    StatsSessReplInstanceNode* instanceNode;

    StatsSessionReplicationNode(void);
    ~StatsSessionReplicationNode(void);
    void markInstanceNodes(PRInt32 mode);
    int deleteInactiveInstanceNodes(void);

    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
    void getFirst(StatsSessReplInstanceNode*& instNodeInp) const;
    void setFirst(StatsSessReplInstanceNode* instNodeInp);
};


//-----------------------------------------------------------------------------
// StatsProcessNode
//
// StatsProcess node contains StatsProcessSlot and other process related nodes.
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsProcessNode
{
    StatsProcessSlot procStats;

    StatsConnectionQueueNode* connQueue;
    StatsThreadPoolNode* threadPool;
    StatsListenNode* ls;
    StatsThreadNode* thread;
    StatsJdbcConnPoolNode* jdbcPoolNode;
    StatsJvmManagementNode* jvmMgmtNode;
    StatsSessionReplicationNode* sessReplNode;
    StatsProcessNode* next;

    StatsProcessNode(void);
    ~StatsProcessNode(void);
    void accumulateThreadData(void);
    void resetObject(void);
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
    PRBool compareId(int pid) const;

    void getFirst(StatsConnectionQueueNode*& connQueueNode) const;
    void setFirst(StatsConnectionQueueNode* connQueueNode);

    void getFirst(StatsThreadPoolNode*& threadPoolNode) const;
    void setFirst(StatsThreadPoolNode* threadPoolNode);

    void getFirst(StatsThreadNode*& threadNode) const;
    void setFirst(StatsThreadNode* threadNode);

    void getFirst(StatsListenNode*& lsNode) const;
    void setFirst(StatsListenNode* lsNode);

    void getFirst(StatsJdbcConnPoolNode*& poolNode) const;
    void setFirst(StatsJdbcConnPoolNode* poolNode);
};

//-----------------------------------------------------------------------------
// StatsServletJSPNode
//
// Stats node for StatsServletJSPSlot
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsServletJSPNode
{
    StatsServletJSPSlot sjsStats;
    NSString name;
    StatsServletJSPNode* next;

    StatsServletJSPNode(const char* strName);
    ~StatsServletJSPNode(void);
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
    PRBool compareId(const char* servletName) const;
};

//-----------------------------------------------------------------------------
// StatsWebModuleNode
//
// Stats node for StatsWebModuleSlot
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsWebModuleNode
{
    StatsWebModuleSlot wmsStats;
    StatsWebModuleCacheSlot wmCacheStats;
    NSString name;
    StatsServletJSPNode* sjs;
    StatsWebModuleNode* next;

    StatsWebModuleNode(const char* strName);
    ~StatsWebModuleNode(void);
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
    PRBool isEmptyNode(void);
    static const char* getCacheTypeName(int cacheType);

    PRBool compareId(const char* webModuleName) const;
    void getFirst(StatsServletJSPNode*& sjsNode) const;
    void setFirst(StatsServletJSPNode* sjsNode);
};

//-----------------------------------------------------------------------------
// StatsVirtualServerNode
//
// Stats node for StatsVirtualServerSlot
//-----------------------------------------------------------------------------

struct HTTPDAEMON_DLL StatsVirtualServerNode
{
private:
    int nProfileBucketsCount_;
public:
    StatsVirtualServerSlot vssStats;
    NSString hostnames;
    NSString interfaces;
    StatsProfileNode* profile;
    StatsWebModuleNode* wms;
    StatsVirtualServerNode* next;


    StatsVirtualServerNode(const VirtualServer* vs, int nProfileBucketsCount);
    ~StatsVirtualServerNode(void);
    void setHostNames(const VirtualServer* vs);
    void setInterfaces(const VirtualServer* vs);
    PRBool isNullInterfaces(void);
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
    PRBool isEmptyNode(void);
    void resetStats(void);

    PRBool compareId(const char* id) const;
    void getFirst(StatsWebModuleNode*& wmsNode) const;
    void setFirst(StatsWebModuleNode* wmsNode);
};

//-----------------------------------------------------------------------------
// StatsCpuInfoNode
//
// Stats node for StatsCpuInfoSlot
//-----------------------------------------------------------------------------

struct StatsCpuInfoNode
{
    StatsCpuInfoSlot cpuInfo;
    StatsCpuInfoNode* next;
    StatsCpuInfoNode();
    ~StatsCpuInfoNode();
    PRBool getNodeData(StatsMsgBuff& msgbuff) const;
};

//-----------------------------------------------------------------------------
// StatsHeaderNode
//
// Stats node for StatsHeader and it's child nodes.
//-----------------------------------------------------------------------------

struct StatsHeaderNode
{
    StatsHeader hdrStats;
    StatsProcessNode* process;
    StatsVirtualServerNode* vss;
    StatsCpuInfoNode* cps;

    StatsHeaderNode();
    ~StatsHeaderNode();

    void getFirst(StatsCpuInfoNode*& cpsNode) const;
    void setFirst(StatsCpuInfoNode* cpsNode);

    void getFirst(StatsVirtualServerNode*& vssNode) const;
    void setFirst(StatsVirtualServerNode* vssNode);

    void getFirst(StatsProcessNode*& processNode) const;
    void setFirst(StatsProcessNode* processNode);
};


//-----------------------------------------------------------------------------
// Various node's compareId, getFirst and setFirst inline methods.
//
// getFirst methods sets the first node to the input reference passed.
//
// setFirst methods sets the first node to the valute passed as input.
//
// compareId method compare the id of the node to the input argument, if
// matches, it returns PR_TRUE else returns PR_FALSE.
//-----------------------------------------------------------------------------


inline
PRBool
StatsThreadNode::compareId(int mode) const
{
    if (threadStats.mode == (PRUint32) mode)
        return PR_TRUE;
    return PR_FALSE;
}

inline
PRBool
StatsListenNode::compareId(const char* id) const
{
    if (strcmp(lsStats.id, id) == 0)
        return PR_TRUE;
    return PR_FALSE;
}

inline
PRBool
StatsJdbcConnPoolNode::compareId(const char* poolNameStr) const
{
    const char* name = poolName.data();
    if (!name)
        return PR_FALSE;
    if (strcmp(name, poolNameStr) == 0)
        return PR_TRUE;
    return PR_FALSE;
}

inline
PRBool
StatsVirtualServerNode::compareId(const char* id) const
{
    if (strcmp(vssStats.id, id) == 0)
        return PR_TRUE;
    return PR_FALSE;
}

inline
void
StatsVirtualServerNode::getFirst(StatsWebModuleNode*& wmsNode) const
{
    wmsNode = wms;
}

inline
void
StatsVirtualServerNode::setFirst(StatsWebModuleNode* wmsNode)
{
    wms = wmsNode;
}

inline
PRBool
StatsWebModuleNode::compareId(const char* webModuleName) const
{
    if (strcmp(name, webModuleName) == 0)
        return PR_TRUE;
    return PR_FALSE;
}

inline
void
StatsWebModuleNode::getFirst(StatsServletJSPNode*& sjsNode) const
{
    sjsNode = sjs;
}

inline
void
StatsWebModuleNode::setFirst(StatsServletJSPNode* sjsNode)
{
    sjs = sjsNode;
}

inline
PRBool
StatsServletJSPNode::compareId(const char* servletName) const
{
    if (strcmp(name, servletName) == 0)
        return PR_TRUE;
    return PR_FALSE;
}

inline
PRBool
StatsWebAppSessionStoreNode::compareId(const char* id) const
{
    if (strcmp(storeId, id) == 0)
        return PR_TRUE;
    return PR_FALSE;
}

inline
PRBool
StatsSessReplInstanceNode::compareId(const char* id) const
{
    if (strcmp(instanceId, id) == 0)
        return PR_TRUE;
    return PR_FALSE;
}

inline
void
StatsSessReplInstanceNode::getFirst(StatsWebAppSessionStoreNode*& wass) const
{
    wass = wassNode;
}

inline
void
StatsSessReplInstanceNode::setFirst(StatsWebAppSessionStoreNode* wass)
{
    wassNode = wass;
}

inline
void
StatsSessionReplicationNode::getFirst(
                             StatsSessReplInstanceNode*& instNodeInp) const
{
    instNodeInp = instanceNode;
}

inline
void
StatsSessionReplicationNode::setFirst(StatsSessReplInstanceNode* instNodeInp)
{
    instanceNode = instNodeInp;
}

inline
PRBool
StatsProcessNode::compareId(int pid) const
{
    if (procStats.pid == pid)
        return PR_TRUE;
    return PR_FALSE;
}

inline
void
StatsProcessNode::getFirst(StatsConnectionQueueNode*& connQueueNode) const
{
    connQueueNode = connQueue;
}

inline
void
StatsProcessNode::setFirst(StatsConnectionQueueNode* connQueueNode)
{
    connQueue = connQueueNode;
}

inline
void
StatsProcessNode::getFirst(StatsJdbcConnPoolNode*& poolNode) const
{
    poolNode = jdbcPoolNode;
}

inline
void
StatsProcessNode::setFirst(StatsJdbcConnPoolNode* poolNode)
{
    jdbcPoolNode = poolNode;
}

inline
void
StatsProcessNode::getFirst(StatsThreadPoolNode*& threadPoolNode) const
{
    threadPoolNode = threadPool;
}

inline
void
StatsProcessNode::setFirst(StatsThreadPoolNode* threadPoolNode)
{
    threadPool = threadPoolNode;
}

inline
void
StatsProcessNode::getFirst(StatsThreadNode*& threadNode) const
{
    threadNode = thread;
}

inline
void
StatsProcessNode::setFirst(StatsThreadNode* threadNode)
{
    thread = threadNode;
}

inline
void
StatsProcessNode::getFirst(StatsListenNode*& lsNode) const
{
    lsNode = ls;
}

inline
void
StatsProcessNode::setFirst(StatsListenNode* lsNode)
{
    ls = lsNode;
}

inline
void
StatsHeaderNode::getFirst(StatsCpuInfoNode*& cpsNode) const
{
    cpsNode = cps;
}

inline
void
StatsHeaderNode::setFirst(StatsCpuInfoNode* cpsNode)
{
    cps = cpsNode;
}

inline
void
StatsHeaderNode::getFirst(StatsVirtualServerNode*& vssNode) const
{
    vssNode = vss;
}

inline
void
StatsHeaderNode::setFirst(StatsVirtualServerNode* vssNode)
{
    vss = vssNode;
}

inline
void
StatsHeaderNode::getFirst(StatsProcessNode*& processNode) const
{
    processNode = process;
}

inline
void
StatsHeaderNode::setFirst(StatsProcessNode* processNode)
{
    process = processNode;
}

#endif // _STATSNODES_H
