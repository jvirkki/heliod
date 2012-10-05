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
#include "httpdaemon/statsmanager.h"
#include "httpdaemon/StatsMsgPreparer.h"

//-----------------------------------------------------------------------------
// StatsMsgPreparer::StatsMsgPreparer
//-----------------------------------------------------------------------------

StatsMsgPreparer::StatsMsgPreparer(StatsHeaderNode* hdr)
{
    hdr_ = hdr;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareProcessStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareProcessStats(StatsMsgBuff& msgbuff,
                                             StatsProcessNode* procCurrent,
                                             int pid)
{
    PRBool fSuccess = PR_FALSE;
    StatsProcessNode* process = 0;
    if (pid == INVALID_PROCESS_ID) {
        process = procCurrent;
        // Accumulate the thread data at this point.
        if (process)
            process->accumulateThreadData();
        fSuccess = PR_TRUE;
    } else {
        process = statsFindNode<StatsProcessNode, StatsHeaderNode> (hdr_, pid);
        if (process) {
            fSuccess = PR_TRUE;
        }
    }
    if (fSuccess && process) {
        fSuccess = process->getNodeData(msgbuff);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareHeaderStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareHeaderStats(StatsMsgBuff& msgbuff)
{
    msgbuff.appendSlot(hdr_->hdrStats);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareCpuStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareCpuStats(StatsMsgBuff& msgbuff,
                                         int countCpuInfoSlots)
{
#ifdef PLATFORM_SPECIFIC_STATS_ON
    statsGetNodeListData(hdr_->cps, msgbuff);
#endif
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::preparePidList
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::preparePidList(StatsMsgBuff& msgbuff)
{
    StatsProcessNode* process = hdr_->process;
    while (process) {
        PRInt32 pid = process->procStats.pid;
        msgbuff.appendData(&pid, sizeof(pid));
        process = process->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareStringStore
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareStringStore(StatsMsgBuff& msgbuff,
                                            StringStore& strings)
{
    const char* data = strings.getBuffer();
    int nSize = strings.getSize();
    if (nSize > 0) {
        msgbuff.appendData(data, nSize);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareAccumulatedVSStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareAccumulatedVSStats(
                                StatsMsgBuff& msgbuff,
                                StatsAccumulatedVSSlot* accumulatedVSStats)
{
    if (!accumulatedVSStats) {
        return PR_FALSE;
    }
    msgbuff.appendSlot(*accumulatedVSStats);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareVirtualServerStats
//
// Output buffer will contain the virtual server slot followed by
// hostnames followed by interface names followed by
// virtual server profiles.
//-----------------------------------------------------------------------------

PRBool
StatsMsgPreparer::prepareVirtualServerStats(StatsMsgBuff& msgbuff,
                                            const char* vsId)
{
    PRBool fSuccess = PR_FALSE;
    StatsVirtualServerNode* vss =
                    statsFindNode<StatsVirtualServerNode>(hdr_, vsId);
    if (vss) {
        fSuccess = vss->getNodeData(msgbuff);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareThreadSlotStats
//
// As thread is not identified by any id. prepareThreadSlotStats returns
// the nIndex'th slot data if the slot is not empty.
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareThreadSlotStats(StatsMsgBuff& msgbuff,
                                                int nIndex,
                                                StatsThreadNode* threadNode)
{
    PRBool fSuccess = PR_TRUE;  // Return true by default.
    int nCurIndex = 0;
    while (threadNode) {
        if (nCurIndex > nIndex)
            break;
        if (threadNode->threadStats.mode == STATS_THREAD_EMPTY) {
            ++nCurIndex;
            continue;
        }
        if (nCurIndex == nIndex) {
            // N.B. we must serialize access to the StatsThreadNode as the
            // DaemonSession can update the requestBucket in realtime
            threadNode->lock();
            fSuccess = threadNode->getNodeData(msgbuff);
            threadNode->unlock();
            break;
        }
        threadNode = threadNode->next;
        ++nCurIndex;
    }
    return fSuccess;
}


//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareVSIdList
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareVSIdList(StringArrayBuff& strmsgbuff)
{
    StatsVirtualServerNode* vss = hdr_->vss;
    while (vss) {
        if (vss->vssStats.mode != STATS_VIRTUALSERVER_EMPTY) {
            strmsgbuff.addString(vss->vssStats.id);
        }
        vss = vss->next;
    }
    strmsgbuff.freeze();
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareThreadIndexList
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareThreadIndexList(StatsMsgBuff& msgbuff,
                                                StatsThreadNode* threadNode)
{
    PRBool fSuccess = PR_TRUE;  // Return true by default.
    PRInt32 nCurIndex = 0;
    while (threadNode) {
        StatsThreadSlot& thread = threadNode->threadStats;
        if (thread.mode == STATS_THREAD_EMPTY) {
            ++nCurIndex;
            continue;
        }
        msgbuff.appendData(&nCurIndex, sizeof(nCurIndex));
        threadNode = threadNode->next;
        ++nCurIndex;
    }
    return fSuccess;
}


//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareListenSlotsStats
//-----------------------------------------------------------------------------

PRBool
StatsMsgPreparer::prepareListenSlotsStats(StatsMsgBuff& msgbuff,
                                          StatsProcessNode* procCurrent,
                                          int pid)
{

    StatsProcessNode* process = 0;
    if (pid == INVALID_PROCESS_ID) {
        process = procCurrent;
    } else {
        process = statsFindNode<StatsProcessNode, StatsHeaderNode> (hdr_, pid);
    }
    if (!process) {
        return PR_FALSE;
    }
    PRBool fSuccess = statsGetNodeListData(process->ls, msgbuff);
    return fSuccess;
}


//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareWebModuleList
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareWebModuleList(StringArrayBuff& strmsgbuff)
{
    StatsVirtualServerNode* vss = hdr_->vss;
    while (vss) {
        if (vss->vssStats.mode != STATS_VIRTUALSERVER_EMPTY) {
            StatsWebModuleNode* wms = vss->wms;
            while (wms) {
                const char* name = wms->name.data();
                if (name &&
                        (wms->wmsStats.mode != STATS_WEBMODULE_MODE_EMPTY)) {
                    strmsgbuff.addString(name);
                }
                wms = wms->next;
            }
        }
        vss = vss->next;
    }
    strmsgbuff.freeze();
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareAllWebModuleStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareAllWebModuleStats(StatsMsgBuff& msgbuff)
{
    StatsVirtualServerNode* vss = hdr_->vss;
    // msgbuff will return the name of the webmodule followed by
    // StatsWebModuleSlot structure for all web modules
    while (vss) {
        if (vss->vssStats.mode != STATS_VIRTUALSERVER_EMPTY) {
            StatsWebModuleNode* wms = vss->wms;
            statsGetNodeListData(wms, msgbuff);
        }
        vss = vss->next;
    }
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareWebModuleStats
//-----------------------------------------------------------------------------

PRBool
StatsMsgPreparer::prepareWebModuleStats(StatsMsgBuff& msgbuff,
                                        const char* webModuleName)
{
    StatsVirtualServerNode* vss = hdr_->vss;
    // msgbuff will return the StatsWebModuleSlot structure corresponding
    // to webmodule name matching webModuleName
    // Receiver must ignore the pointers in the struct.
    PRBool fSuccess = PR_FALSE;
    StatsWebModuleNode* wms = 0;
    wms = StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
    if (wms) {
        wms->getNodeData(msgbuff);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareAllServletStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareAllServletStats(StatsMsgBuff& msgbuff)
{
    StatsVirtualServerNode* vss = hdr_->vss;
    while (vss) {
        if (vss->vssStats.mode != STATS_VIRTUALSERVER_EMPTY) {
            StatsWebModuleNode* wms = vss->wms;
            while (wms) {
                const char* name = wms->name.data();
                StatsServletJSPNode* sjs = wms->sjs;
                PR_ASSERT(name);
                if (name && wms->wmsStats.mode != STATS_WEBMODULE_MODE_EMPTY) {
                    while (sjs) {
                        const char* servletName = sjs->name.data();
                        PR_ASSERT(servletName);
                        if (!servletName) {
                            // Error : servlet without name
                            break;
                        }
                        // Append the triplet, webmodule name, servlet name and
                        // servlet data.
                        msgbuff.appendString(name);
                        sjs->getNodeData(msgbuff);
                        sjs = sjs->next;
                    }
                }
                wms = wms->next;
            }
        }
        vss = vss->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareServletStatsForWebModule
//-----------------------------------------------------------------------------

PRBool
StatsMsgPreparer::prepareServletStatsForWebModule(StatsMsgBuff& msgbuff,
                                                  const char* webModuleName)
{
    StatsVirtualServerNode* vss = hdr_->vss;
    StatsWebModuleNode* wms = 0;
    wms = StatsManagerUtil::findWebModuleSlotInVSList(vss, webModuleName);
    PRBool fSuccess = PR_TRUE;
    if (wms && (wms->wmsStats.mode != STATS_WEBMODULE_MODE_EMPTY)) {
        StatsServletJSPNode* sjs = wms->sjs;
        fSuccess = statsGetNodeListData(sjs, msgbuff);
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareJdbcConnPoolStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareJdbcConnPoolStats(StatsMsgBuff& msgbuff,
                                                  int pid)
{
    StatsProcessNode* process = 0;
    PRBool fSuccess = PR_FALSE;
    if (pid == INVALID_PROCESS_ID) {
        process = hdr_->process;
        fSuccess = PR_TRUE;
    } else {
        process = statsFindNode<StatsProcessNode, StatsHeaderNode>(hdr_, pid);
        if (process) {
            fSuccess = PR_TRUE;
        }
    }
    if (fSuccess && process) {
        StatsJdbcConnPoolNode* poolNode = process->jdbcPoolNode;
        if (!poolNode)
            return PR_TRUE;
        statsGetNodeListData(poolNode, msgbuff);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareJvmMgmtStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareJvmMgmtStats(StatsMsgBuff& msgbuff, int pid)
{
    StatsProcessNode* process = 0;
    PRBool fSuccess = PR_FALSE;
    if (pid == INVALID_PROCESS_ID) {
        process = hdr_->process;
        fSuccess = PR_TRUE;
    } else {
        process = statsFindNode<StatsProcessNode, StatsHeaderNode>(hdr_, pid);
        if (process) {
            fSuccess = PR_TRUE;
        }
    }
    if (fSuccess && process) {
        StatsJvmManagementNode* jvmMgmtNode = process->jvmMgmtNode;
        if (jvmMgmtNode) {
            fSuccess = jvmMgmtNode->getNodeData(msgbuff);
        }
        // else jvm management data is not available. Return no data in
        // msgbuff.
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgPreparer::prepareSessionReplicationStats
//-----------------------------------------------------------------------------

PRBool StatsMsgPreparer::prepareSessionReplicationStats(StatsMsgBuff& msgbuff)
{
    StatsProcessNode* procNode = hdr_->process;
    if (!(procNode && procNode->sessReplNode)) {
        return PR_TRUE; // no session replication stats
    }
    StatsSessionReplicationNode* sessReplNode = procNode->sessReplNode;
    sessReplNode->getNodeData(msgbuff);
    StatsSessReplInstanceNode* instanceNode = NULL;
    instanceNode = sessReplNode->instanceNode;
    PRInt32 nInstanceIndex = 0;
    while (instanceNode) {
        if (instanceNode->mode == STATS_SR_INST_NODE_ACTIVE) {
            msgbuff.appendString(instanceNode->instanceId);
            // Write the count of webapp store so reader knows how many webapp
            // stores are there to read.
            // Write a instanceIndex for debugging purposes as pad value.
            msgbuff.appendInteger(instanceNode->countWebappStores,
                                  nInstanceIndex++);
            // Now write the web app stores for this instance.
            StatsWebAppSessionStoreNode* wassNode = instanceNode->wassNode;
            PRInt32 nWebappStoreIndex = 0;
            while (wassNode) {
                if (wassNode->mode == STATS_SR_WEBAPP_NODE_ACTIVE) {
                    // Write the store index as pad for debugging purposes.
                    wassNode->getNodeData(msgbuff, nWebappStoreIndex++);
                }
                wassNode = wassNode->next;
            }
        }
        instanceNode = instanceNode->next;
    }
}

