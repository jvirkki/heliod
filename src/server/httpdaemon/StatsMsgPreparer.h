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

#ifndef _STATS_MSG_PREPARER_H
#define _STATS_MSG_PREPARER_H

class StatsMsgPreparer
{
    private:
        StatsHeaderNode* hdr_;

    public:
        // constructor
        StatsMsgPreparer(StatsHeaderNode* hdr);

        // prepare process stats
        PRBool prepareProcessStats(StatsMsgBuff& msgbuff,
                                   StatsProcessNode* procCurrent,
                                   int pid = INVALID_PROCESS_ID);

        // prepare accumulated stats
        PRBool
        prepareAccumulatedVSStats(StatsMsgBuff& msgbuff,
                                  StatsAccumulatedVSSlot* accumulatedVSStats);

        // prepare virtual server stats (core vs stats only)
        PRBool prepareVirtualServerStats(StatsMsgBuff& msgbuff,
                                         const char* vsId);

        // prepare StatsHeader related stats.
        PRBool prepareHeaderStats(StatsMsgBuff & msgbuff);

        // prepare Cpu stats (whole list).
        PRBool prepareCpuStats(StatsMsgBuff& msgbuff, int countCpuStatsSlots);

        // prepare pid list.
        PRBool preparePidList(StatsMsgBuff& msgbuff);

        // prepare string store.
        PRBool prepareStringStore(StatsMsgBuff& msgbuff, StringStore& strings);

        // prepare virtual server id list.
        PRBool prepareVSIdList(StringArrayBuff& strmsgbuff);

        // prepare all listen slot stats
        PRBool prepareListenSlotsStats(StatsMsgBuff& strmsgbuff,
                                       StatsProcessNode* procCurrent,
                                       int pid = INVALID_PROCESS_ID);

        // prepare all servlet stats for all web modules.
        PRBool prepareAllServletStats(StatsMsgBuff& msgbuff);

        // prepare all servlet stats for specific web module webModuleName.
        PRBool prepareServletStatsForWebModule(StatsMsgBuff & msgbuff,
                                               const char* webModuleName);

        // prepate Thread stats for nIndex (thread index)
        PRBool prepareThreadSlotStats(StatsMsgBuff& msgbuff, int nIndex,
                                      StatsThreadNode* threadNode);

        // preparet thread indext list.
        PRBool prepareThreadIndexList(StatsMsgBuff& msgbuff,
                                      StatsThreadNode* threadNode);

        // prepare the connection pool data in message buffer.
        PRBool prepareJdbcConnPoolStats(StatsMsgBuff& msgbuff,
                                        int pid = INVALID_PROCESS_ID);

        // prepare jvm management stats.
        PRBool prepareJvmMgmtStats(StatsMsgBuff& msgbuff,
                                   int pid = INVALID_PROCESS_ID);

        // prepare session replication stats.
        PRBool prepareSessionReplicationStats(StatsMsgBuff& msgbuff);
        
        // prepare web module list.
        PRBool prepareWebModuleList(StringArrayBuff& strmsgbuff);

        // prepare all web module stats.
        PRBool prepareAllWebModuleStats(StatsMsgBuff& msgbuff);

        // prepare web module stats for a specific web module.
        PRBool prepareWebModuleStats(StatsMsgBuff& msgbuff,
                                     const char* webModuleName);
};

#endif
