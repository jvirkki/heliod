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
#include <sys/types.h>          // pid_t, mode_t
#include <sys/stat.h>           // umask()
#include <signal.h>             // kill()
#include <unistd.h>             // getpid()
#include <sys/socket.h>         // socket()
#include <sys/un.h>             // sockaddr_un
#include "private/pprio.h"      // PR_ImportFile
#include "base/ereport.h"       // ereport()
#include "frame/conf.h"         // conf_get_true_globals()
#include "base/wdservermessage.h"       // wdservermessage
#include "httpdaemon/WebServer.h"       // WebServer class
#include "httpdaemon/ParentStats.h"     // ParentStats class
#include "httpdaemon/dbthttpdaemon.h"   // DBT_xxx
#include "httpdaemon/statsmessage.h"    // StatsServerMessage
#include "httpdaemon/statsmanager.h"    // StatsManager




////////////////////////////////////////////////////////////////

// StatsChildConnInfo Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsChildConnInfo::StatsChildConnInfo
//-----------------------------------------------------------------------------

StatsChildConnInfo::StatsChildConnInfo(void)
{
    reset();
}

//-----------------------------------------------------------------------------
// StatsChildConnInfo::~StatsChildConnInfo
//-----------------------------------------------------------------------------

StatsChildConnInfo::~StatsChildConnInfo(void)
{
    reset();
}

//-----------------------------------------------------------------------------
// StatsChildConnInfo::reset
//-----------------------------------------------------------------------------

void StatsChildConnInfo::reset(void)
{
    childFD_ = 0;
    childPid_ = -1;
    fChildInitialized_ = PR_FALSE;
    fProcessAlive_ = PR_FALSE;
    fGotReconfigResponse_ = PR_FALSE;
    fWebModuleInit_ = PR_FALSE;
}


////////////////////////////////////////////////////////////////

// ParentStats::MsgErrorHandler Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// ParentStats::MsgErrorHandler::handleMessageChannelError
//-----------------------------------------------------------------------------

void
ParentStats::MsgErrorHandler::handleMessageChannelError(
                                            PRFileDesc* fd,
                                            StatsServerMessage& msgChannel,
                                            const char* callerContext,
                                            int nChildNo)
{
    if (msgChannel.isFileDescriptorError() == PR_TRUE)
    {
        ereport(LOG_VERBOSE,
                XP_GetAdminStr(DBT_ParentStats_CloseConnection),
                nChildNo);
        if (parentStats_)
            parentStats_->processFileDescriptorError(fd);
    }
    else
    {
        logInternalStatsError(callerContext, nChildNo);
    }
}

void
ParentStats::MsgErrorHandler::logInternalStatsError(const char* callerContext,
                                                    int nChildNo)
{
    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentStats_InternalError),
            callerContext, nChildNo);
}

////////////////////////////////////////////////////////////////

// ParentStats::Messenger Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// ParentStats::Messenger::sendMsgAndRecvResp
//-----------------------------------------------------------------------------

PRBool ParentStats::Messenger::sendMsgAndRecvResp(StatsMessages msgType,
                                                  const char* callerContext,
                                                  const char* msgStringArg)
{
    if (msgSender_.sendMsgAndRecvResp(msgType, msgStringArg) != PR_TRUE)
    {
        errHandler_.handleMessageChannelError(fd_,
                                              statsMsgChannel_,
                                              callerContext, nChildNo_);
        return PR_FALSE;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::Messenger::sendMsgAndRecvResp
//-----------------------------------------------------------------------------

PRBool ParentStats::Messenger::sendMsgAndRecvResp(StatsMessages msgType,
                                                  const char* callerContext,
                                                  const void* buffer,
                                                  int bufferLen)
{
    if (msgSender_.sendMsgAndRecvResp(msgType, buffer, bufferLen) != PR_TRUE)
    {
        errHandler_.handleMessageChannelError(fd_,
                                              statsMsgChannel_,
                                              callerContext, nChildNo_);
        return PR_FALSE;
    }
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// ParentStats Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// ParentStats::ParentStats
//-----------------------------------------------------------------------------

ParentStats::ParentStats(const PRInt32 nChildren) :
    ParentAdmin(nChildren),
    statsMsgHandler_(*this), // Pass StatsUpdateHandler reference
    msgErrorHandler_(this)
{
    childConnInfo_ = new StatsChildConnInfo[getChildrenCount()];
    nConnectedChild_ = 0;
    statsBackupManager_ = 0;
    fLateInitDone_ = PR_FALSE;
    fReconfigureInProgress_ = PR_FALSE;
}

//-----------------------------------------------------------------------------
// ParentStats::~ParentStats
//-----------------------------------------------------------------------------

ParentStats::~ParentStats(void)
{
    if (childConnInfo_)
        delete[] childConnInfo_;
    childConnInfo_ = 0;
}

//-----------------------------------------------------------------------------
// ParentStats::processCloseFd
//-----------------------------------------------------------------------------

void ParentStats::processCloseFd(PRFileDesc* fd)
{
    int nChildNo = 0;
    if (fd == NULL)
        return;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isConnectionFDMatches(fd) == PR_TRUE)
        {
            childConnInfo_[nChildNo].reset();
            --nConnectedChild_;
            break;
        }
    }
    statsMsgHandler_.processCloseFd(PR_FileDesc2NativeHandle(fd));
    return;
}


//-----------------------------------------------------------------------------
// ParentStats::initLate
//-----------------------------------------------------------------------------

void ParentStats::initLate(void)
{
    if (fLateInitDone_ == PR_TRUE)
    {
        return;
    }
    fLateInitDone_ = PR_TRUE;
    statsBackupManager_ =
        StatsBackupManager::createBackupManager(getChildrenCount());
    statsBackupManager_->setupNewProcessNode();
}

//-----------------------------------------------------------------------------
// ParentStats::processChildDeath
//-----------------------------------------------------------------------------

void ParentStats::processChildDeath(int pid)
{
    if (statsBackupManager_)
        statsBackupManager_->markProcessSlotEmpty(pid);
    int nChildNo = getProcessSlotIndex(pid);
    if (nChildNo != -1)
    {
        PRFileDesc* fd = childConnInfo_[nChildNo].getConnectionFD();
        processFileDescriptorError(fd);
    }
}

//-----------------------------------------------------------------------------
// ParentStats::processReconfigure
//-----------------------------------------------------------------------------

void ParentStats::processReconfigure(void)
{
    // overloaded method from ParentAdmin base class
    if (isReadyForUpdate() == PR_TRUE)
    {
        // So now reconfigure is started, so let's save this state.
        fReconfigureInProgress_ = PR_TRUE;
    }
    else
    {
        // we got reconfig request when childs were not even connected. Ignore
        // this event. With the design of primordial this looks difficult if
        // not impossible.
        ereport(LOG_INFORM, XP_GetAdminStr(DBT_ParentStats_IgnoredReconfigure));
    }
}

//-----------------------------------------------------------------------------
// ParentStats::processChildStart
//-----------------------------------------------------------------------------

void ParentStats::processChildStart(int pid)
{
    // Now one of the child is being created so
    // unmark the first entry which child will pickup for it's inherited data
    // so that next died child will pick up the next marked entry.
    if (statsBackupManager_)
        statsBackupManager_->unmarkLastProcessSlot(pid);
}

//-----------------------------------------------------------------------------
// ParentStats::handleChildInit
//-----------------------------------------------------------------------------

void ParentStats::handleChildInit(PRFileDesc* fd)
{
    // overloaded method from ParentAdmin base class
    int nChildNo = 0;
    //sleep(30);
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isValidConnectionFD() == PR_FALSE)
        {
            childConnInfo_[nChildNo].setConnectionFD(fd);
            ++nConnectedChild_;
            childConnInfo_[nChildNo].setChildInitialized();
            initChildData(nChildNo);
            if (fReconfigureInProgress_ == PR_TRUE)
            {
                // So child died while reconfiguring was taking place.
                // Primordial will be waiting for notice to complete
                // reconfigure from child. So to avoid failure of future
                // reconfiguration let's set the reconfigure state to TRUE so
                // that when other childs are done, parent will do the
                // reconfigure properly.
                childConnInfo_[nChildNo].setReconfigStatus(PR_TRUE);
            }
            break;
        }
    }
    if (nChildNo >= getChildrenCount())
    {
        // This should not happen
        PR_ASSERT(0);
    }
    if (isReadyForUpdate() == PR_TRUE)
    {
        statsMsgHandler_.sendNotification(statsNoticeAllChildInitialized);
    }
}


//-----------------------------------------------------------------------------
// ParentStats::processStatsMessage
//-----------------------------------------------------------------------------

void ParentStats::processStatsMessage(int fdIndex)
{
    // Handle Stats Message here
    PRFileDesc* fd = getPollFileDesc(fdIndex);
    StatsServerMessage msgChannel(PR_FileDesc2NativeHandle(fd),
                                  PR_TRUE       // Don't close the fd
                                 );
    if (nConnectedChild_ != getChildrenCount())
    {
        char* msg = msgChannel.RecvFromServer();
        if (msg == NULL)
        {
            processFileDescriptorError(fd);
        }
        else
        {
            msgChannel.sendStatsError(statsmsgErrorNotReady);
        }
        return;
    }
    if (statsMsgHandler_.processStatsMessage(msgChannel) != PR_TRUE)
    {
        if (msgChannel.isFileDescriptorError() == PR_TRUE)
        {
            processFileDescriptorError(fd);
        }
        // else statsMsgHandler_ is expected to send the proper error to the
        // peer. If it won't then connection will hang so do the primordial and
        // StatsClient
    }
    return;
}

//-----------------------------------------------------------------------------
// ParentStats::processFileDescriptorError
//-----------------------------------------------------------------------------

void ParentStats::processFileDescriptorError(PRFileDesc* fd)
{
    // processError will close the Fd. It will call processCloseFd which will
    // reset the childConnInfo[nChildNo]
    PRInt32 nIndex = findFileDescIndex(fd);
    if (nIndex != -1)
        processError(nIndex);
    else
    {
        PR_ASSERT(0);
    }
}

//-----------------------------------------------------------------------------
// ParentStats::getProcessSlotIndex
//
// Return the position of processSlot which contains the process id = pid.
// Return -1 if not found.
//-----------------------------------------------------------------------------

int ParentStats::getProcessSlotIndex(int pid) const
{
    int nChildNo = 0;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isReadyForUpdate() == PR_TRUE)
        {
            if (pid == childConnInfo_[nChildNo].getChildPid())
            {
                return nChildNo;
            }
        }
    }
    return -1;
}

//-----------------------------------------------------------------------------
// ParentStats::initChildData
//-----------------------------------------------------------------------------

PRBool ParentStats::initChildData(int nChildNo)
{
    PR_ASSERT(nChildNo < getChildrenCount());
    // Initialized PID info
    updateProcessSlot(nChildNo);
    PRBool fPidInitialized = childConnInfo_[nChildNo].isPidInitialized();
    if (fPidInitialized == PR_TRUE)
    {
        initListenSlots(nChildNo);
        initVSIdList(nChildNo);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::initVSIdList
//-----------------------------------------------------------------------------

PRBool ParentStats::initVSIdList(int nChildNo)
{
    const char* callerContext = "initVSIdList";
    return initOrUpdateStats(nChildNo, statsmsgReqGetVSList, callerContext,
                             &StatsBackupManager::initVSIdList);
}

//-----------------------------------------------------------------------------
// ParentStats::initListenSlots
//-----------------------------------------------------------------------------

PRBool ParentStats::initListenSlots(int nChildNo)
{
    const char* callerContext = "initListenSlots";
    int pid = childConnInfo_[nChildNo].getChildPid();
    return initOrUpdateStats(nChildNo,
                             pid, statsmsgReqGetListenSlotData, callerContext,
                             &StatsBackupManager::initListenSlots);
}

//-----------------------------------------------------------------------------
// ParentStats::initWebModuleList
//-----------------------------------------------------------------------------

PRBool ParentStats::initWebModuleList(int pid)
{
    ereport(LOG_VERBOSE, "WebModule list initialization request came for %d\n",
            pid);
    //sleep(30);
    int nChildNo = getProcessSlotIndex(pid);
    if (nChildNo == -1)
    {
        ereport(LOG_VERBOSE, "initWebModuleList : Child not yet initialized\n");
        // If the function is called as a result of notice then
        // we should not worry as the initWebModuleList will be called
        // later after the child is initialized
        return PR_FALSE;
    }
    const char* callerContext = "initWebModuleList";
    PRBool fSuccess = initOrUpdateStats(nChildNo,
                                        pid,
                                        statsmsgReqGetWebModuleList,
                                        callerContext,
                                        &StatsBackupManager::initWebModuleList);
    if (fSuccess == PR_TRUE)
        childConnInfo_[nChildNo].setWebModulesInitialized();
    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::isAllChildWebModInitDone
//-----------------------------------------------------------------------------

PRBool ParentStats::isAllChildWebModInitDone(void)
{
    PRBool fInitialized = PR_TRUE;
    int nChildNo = 0;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isWebModuleInitialized() != PR_TRUE)
        {
            fInitialized = PR_FALSE;
            break;
        }
    }
    return fInitialized;
}

//-----------------------------------------------------------------------------
// ParentStats::updateOnReconfig
//-----------------------------------------------------------------------------

PRBool ParentStats::updateOnReconfig(int pid)
{
    if (fReconfigureInProgress_ != PR_TRUE)
    {
        // Child sent late reconfigure response
        return PR_TRUE; // yes return true.
    }
    int nChildNo = getProcessSlotIndex(pid);
    if (nChildNo == -1)
    {
        PR_ASSERT(0);
        return PR_FALSE;
    }
    childConnInfo_[nChildNo].setReconfigStatus(PR_TRUE);
    // Now check to see if all childs are done with reconfigure.
    int nCountReconfigDone = 0;
    int nUninitializeCount = 0;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isPidInitialized() != PR_TRUE)
        {
            // So child has not even initialized so it won't send the reconfig.
            // This seems to happen only when child dies while parent was
            // waiting for reconfigure.
            ++nCountReconfigDone;
            ++nUninitializeCount;
            continue;
        }
        if (childConnInfo_[nChildNo].isReconfigureDone() == PR_TRUE)
        {
            ++nCountReconfigDone;
        }
    }
    if (nCountReconfigDone != getChildrenCount())
        return PR_TRUE; // Not all childs are done with reconfig.

    // All childs are done with reconfigure. This is the time to do the
    // reconfig updates.

    // first reset the flags for reconfiguring.
    fReconfigureInProgress_ = PR_FALSE;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        childConnInfo_[nChildNo].setReconfigStatus(PR_FALSE);
    }

    if (nUninitializeCount == 0)
    {
        doStatsReconfigure();
        StatsManager::incrementReconfigCount();
        statsMsgHandler_.sendNotification(statsNoticeReconfigureDone);
    }
    else
    {
        ereport(LOG_INFORM, XP_GetAdminStr(DBT_ParentStats_IgnoredReconfigure));
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::updateThreadSlot
//-----------------------------------------------------------------------------

PRBool ParentStats::updateThreadSlot(int nChildNo)
{
    if (childConnInfo_[nChildNo].isReadyForUpdate() != PR_TRUE)
        return PR_FALSE;
    PRFileDesc* fd = childConnInfo_[nChildNo].getConnectionFD();
    const char* callerContext = "updateThreadSlot";
    PRBool fSuccess = PR_FALSE;
    Messenger messenger(nChildNo, fd, msgErrorHandler_);
    if (messenger.sendMsgAndRecvResp(statsmsgReqGetThreadIndexList,
                                     callerContext) != PR_TRUE)
        return fSuccess;
    StatsBufferReader& thrIndexBufferReader = messenger.getBufferReader();
    // Received bytes must be sizeof(PRInt32) multiples
    int nMsgLen = thrIndexBufferReader.getRemainingBytesCount();
    if ((nMsgLen % sizeof(PRInt32)) != 0)
    {
        return PR_FALSE;
    }
    int pid = childConnInfo_[nChildNo].getChildPid();
    fSuccess = PR_FALSE;
    int nCountThreads = nMsgLen / sizeof(PRInt32);
    if (statsBackupManager_->resetThreadSlots(pid, nCountThreads) == PR_TRUE)
    {
        // Now we have received a list of non empty thread index
        // Make a copy of the received buffer as we are going to use the
        // messenger's message channel's internal buffer.
        StatsBufferReader statsRecvBuffer(NULL, 0);
        messenger.getBufferReaderCopy(statsRecvBuffer);
        PRInt32 nThrIndex = 0;
        while (statsRecvBuffer.readInt32(nThrIndex) == PR_TRUE)
        {
            if (messenger.sendMsgAndRecvResp(statsmsgReqGetThreadSlotData,
                                             callerContext,
                                             &nThrIndex,
                                             sizeof(nThrIndex)) != PR_TRUE)
            {
                return PR_FALSE;
            }
            StatsBufferReader& bufferReader = messenger.getBufferReader();
            fSuccess = statsBackupManager_->updateThreadSlot(pid, nThrIndex,
                                                             bufferReader);
            if (fSuccess != PR_TRUE)
            {
                msgErrorHandler_.logInternalStatsError(callerContext, nChildNo);
                break;
            }
        }
    }
    return fSuccess;
}


//-----------------------------------------------------------------------------
// ParentStats::updateProcessSlot
//-----------------------------------------------------------------------------

PRBool ParentStats::updateProcessSlot(int nChildNo)
{
    if (childConnInfo_[nChildNo].isConnectionReady() != PR_TRUE)
        return PR_FALSE;
    PRFileDesc* fd = childConnInfo_[nChildNo].getConnectionFD();
    const char* callerContext = "updateProcessSlot";
    PRBool fSuccess = PR_FALSE;
    Messenger messenger(nChildNo, fd, msgErrorHandler_);
    if (messenger.sendMsgAndRecvResp(statsmsgReqGetStatsProcessInfo,
                                     callerContext) != PR_TRUE)
        return fSuccess;
    StatsBufferReader& bufferReader = messenger.getBufferReader();
    if (childConnInfo_[nChildNo].isPidInitialized() == PR_FALSE)
    {
        fSuccess = statsBackupManager_->initializeProcessSlot(bufferReader);
        if (fSuccess == PR_TRUE)
        {
            const void* recvBuffer = bufferReader.getBufferAddress();
            int pid = statsBackupManager_->getPidFromBuffer(recvBuffer);
            childConnInfo_[nChildNo].setChildPid(pid);
        }
    }
    else
    {
        fSuccess = statsBackupManager_->updateProcessSlot(bufferReader);
    }
    if (fSuccess != PR_TRUE)
    {
        msgErrorHandler_.logInternalStatsError(callerContext, nChildNo);
    }

    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::updateProcessSlots
//-----------------------------------------------------------------------------

PRBool ParentStats::updateProcessSlots(void)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    int nChildNo = 0;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        updateProcessSlot(nChildNo);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::updateProcessSlotForPid
//-----------------------------------------------------------------------------

PRBool ParentStats::updateProcessSlotForPid(int pid)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    int nChildNo = getProcessSlotIndex(pid);
    int fSuccess = PR_FALSE;
    if (nChildNo != -1)
    {
        return updateProcessSlot(nChildNo);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::updateThreadSlots
//-----------------------------------------------------------------------------

PRBool ParentStats::updateThreadSlots(void)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    int nChildNo = 0;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        updateThreadSlot(nChildNo);
    }
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// ParentStats::updateVSCoreData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateVSCoreData(int nChildNo,
                                     const char* vsId)
{
    PR_ASSERT(nChildNo < getChildrenCount());
    const char* callerContext = "updateVSCoreData";
    int pid = childConnInfo_[nChildNo].getChildPid();
    PFnStrProcInitOrUpdate fnPtr = &StatsBackupManager::updateVSSlot;
    return initOrUpdateStats(nChildNo, pid,
                             statsmsgReqGetStatsVirtualServerInfo,
                             callerContext, fnPtr, vsId);
}

//-----------------------------------------------------------------------------
// ParentStats::updateVSCoreData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateVSCoreData(int nChildNo)
{
    const StringArrayBuff& vsIdArray = statsBackupManager_->getVsIdArray();
    int nCountVS = vsIdArray.getStringCount();
    char** vsIds = vsIdArray.getStringArray();
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCountVS; ++nIndex)
    {
        updateVSCoreData(nChildNo, vsIds[nIndex]);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::updateVSCoreData
//
// Update the VS Core Data for vsId. If vsId is 0 then updates for all
// virtual servers
//-----------------------------------------------------------------------------

PRBool ParentStats::updateVSCoreData(const char* vsId)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    int nChildNo = 0;
    PRBool updatedVS = PR_FALSE;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isReadyForUpdate() == PR_TRUE)
        {
            updatedVS = PR_TRUE;
            if (vsId == 0)
            {
                updateVSCoreData(nChildNo);
            }
            else
            {
                updateVSCoreData(nChildNo, vsId);
            }
        }
    }
    if (updatedVS == PR_TRUE)
        statsBackupManager_->aggregateVSData(vsId);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::updateAccumulatedVSData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateAccumulatedVSData(int nChildNo)
{
    PR_ASSERT(nChildNo < getChildrenCount());
    const char* callerContext = "updateAccumulatedVSData";
    int pid = childConnInfo_[nChildNo].getChildPid();
    return initOrUpdateStats(nChildNo,
                             pid,
                             statsmsgReqGetAccumulatedVSData, callerContext,
                             &StatsBackupManager::updateAccumulatedVSData);
}

//-----------------------------------------------------------------------------
// ParentStats::updateAccumulatedVSData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateAccumulatedVSData(void)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    int nChildNo = 0;
    PRBool updatedStats = PR_FALSE;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isReadyForUpdate() == PR_TRUE)
        {
            updatedStats = PR_TRUE;
            updateAccumulatedVSData(nChildNo);
        }
    }
    if (updatedStats == PR_TRUE)
        statsBackupManager_->aggregateAccumulatedVSData();
    return updatedStats;
}

//-----------------------------------------------------------------------------
// ParentStats::updateWebModuleData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateWebModuleData(int nChildNo, const char* webModuleName)
{
    PR_ASSERT(nChildNo < getChildrenCount());
    const char* callerContext = "updateWebModuleData";
    int pid = childConnInfo_[nChildNo].getChildPid();
    PFnProcInitOrUpdate fnPtr = &StatsBackupManager::updateWebModuleData;
    return initOrUpdateStats(nChildNo, pid, statsmsgReqGetAllWebModuleData,
                             callerContext, fnPtr, webModuleName);
}

//-----------------------------------------------------------------------------
// ParentStats::updateWebModuleData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateWebModuleData(const char* webModuleName)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    int nChildNo = 0;
    int nCountWebModInitChild = 0;
    PRBool fSuccess = PR_TRUE;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isReadyForUpdate() == PR_TRUE)
        {
            if (childConnInfo_[nChildNo].isWebModuleInitialized() == PR_TRUE)
            {
                if (updateWebModuleData(nChildNo, webModuleName) != PR_TRUE)
                    fSuccess = PR_FALSE;
                ++nCountWebModInitChild;
            }
        }
    }

    if ((fSuccess == PR_TRUE) && (nCountWebModInitChild == getChildrenCount()))
    {
        if (webModuleName == NULL)
        {
            fSuccess = statsBackupManager_->aggregateAllWebModules();
        }
        else
        {
            fSuccess =
                statsBackupManager_->aggregateWebModuleData(webModuleName);
        }
        if (fSuccess != PR_TRUE)
        {
            const char* callerContext = "aggregateWebModuleData";
            msgErrorHandler_.logInternalStatsError(callerContext, -1);
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::updateServletData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateServletData(int nChildNo, const char* webModuleName)
{
    const char* callerContext = "updateServletData";
    int pid = childConnInfo_[nChildNo].getChildPid();
    PFnStrProcInitOrUpdate fnPtr = &StatsBackupManager::updateServletData;
    return initOrUpdateStats(nChildNo, pid,
                             statsmsgReqGetServletData,
                             callerContext, fnPtr, webModuleName);
}

//-----------------------------------------------------------------------------
// ParentStats::updateServletData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateServletData(int nChildNo)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    PR_ASSERT(nChildNo < getChildrenCount());
    int nWebModuleCount = statsBackupManager_->getWebModuleCount();
    char** webModuleNameArray = statsBackupManager_->getWebModuleNames();
    int nIndex = 0;
    PRBool fSuccess = PR_TRUE;
    for (nIndex = 0; nIndex < nWebModuleCount; ++nIndex)
    {
        char* webModuleName = webModuleNameArray[nIndex];
        updateServletData(nChildNo, webModuleName);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::updateServletData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateServletData(const char* webModuleName)
{
    int nChildNo = 0;
    PRBool fSuccess = PR_TRUE;
    int nCountWebModInitChild = 0;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isReadyForUpdate() == PR_TRUE)
        {
            if (childConnInfo_[nChildNo].isWebModuleInitialized() == PR_TRUE)
            {
                if (webModuleName == NULL)
                {
                    if (updateServletData(nChildNo) != PR_TRUE)
                        fSuccess = PR_FALSE;
                }
                else
                {
                    if (updateServletData(nChildNo, webModuleName) != PR_TRUE)
                        fSuccess = PR_FALSE;
                }
                ++nCountWebModInitChild;
            }
        }
    }
    if ((fSuccess == PR_TRUE) && (nCountWebModInitChild == getChildrenCount()))
    {
        if (webModuleName == NULL)
            fSuccess = statsBackupManager_->aggregateAllServlets();
        else
            fSuccess = statsBackupManager_->aggregateServletsForWebModule(
                                                            webModuleName);
        if (fSuccess != PR_TRUE)
        {
            msgErrorHandler_.logInternalStatsError("aggregateServletData",
                                                   -1);
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::updateJdbcConnPools
//-----------------------------------------------------------------------------

PRBool ParentStats::updateJdbcConnPools(int pid)
{
    int nChildNo = getProcessSlotIndex(pid);
    const char* callerContext = "updateJdbcConnPools";
    return initOrUpdateStats(nChildNo,
                             pid,
                             statsmsgReqGetJdbcConnPools, callerContext,
                             &StatsBackupManager::updateJdbcConnectionPools);
}

//-----------------------------------------------------------------------------
// ParentStats::updateJvmMgmtData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateJvmMgmtData(int pid)
{
    if (statsBackupManager_->isJvmMgmtStatsEnabled(pid) != PR_TRUE)
    {
        // No jvm management data. Jvm may be disabled.
        return PR_TRUE;
    }
    int nChildNo = getProcessSlotIndex(pid);
    const char* callerContext = "updateJvmMgmtData";
    return initOrUpdateStats(nChildNo, pid,
                             statsmsgReqGetJvmMgmtData, callerContext,
                             &StatsBackupManager::updateJvmMgmtData);
}

//-----------------------------------------------------------------------------
// ParentStats::updatePerProcessJavaData
//-----------------------------------------------------------------------------

PRBool ParentStats::updatePerProcessJavaData(int nChildNo)
{
    if (childConnInfo_[nChildNo].isConnectionReady() != PR_TRUE)
        return PR_FALSE;

    if (childConnInfo_[nChildNo].isPidInitialized() == PR_FALSE)
        return PR_TRUE; // process must be initialized first.

    int pid = childConnInfo_[nChildNo].getChildPid();
    PRBool fSuccess = PR_TRUE;
    if (updateJdbcConnPools(pid) != PR_TRUE)
        fSuccess = PR_FALSE;

    if (updateJvmMgmtData(pid) != PR_TRUE)
        fSuccess = PR_FALSE;

    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::updateProcessJavaData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateProcessJavaData(void)
{
    int nChildNo = 0;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        updatePerProcessJavaData(nChildNo);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::updateVSJavaData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateVSJavaData(void)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    updateWebModuleData();
    updateServletData();
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::updateSessionReplicationData
//-----------------------------------------------------------------------------

PRBool ParentStats::updateSessionReplicationData(void)
{
    if (fReconfigureInProgress_ == PR_TRUE)
    {
        // No update as reconfigure is in process.
        // Let's don't bug the childs while they are doing reconfigure.
        return PR_TRUE;
    }
    if (getChildrenCount() > 1)
    {
        // Session replication is not enabled for maxProcs > 1.
        return PR_TRUE;
    }
    int nChildNo = 0; // Get stats from first child.
    const char* callerContext = "updateSessionReplicationData";
    return initOrUpdateStats(nChildNo,
                             statsmsgReqGetSessReplData, callerContext,
                             &StatsBackupManager::updateSessionReplicationData);
}

//-----------------------------------------------------------------------------
// ParentStats::isReadyForUpdate
//-----------------------------------------------------------------------------

PRBool ParentStats::isReadyForUpdate(void)
{
    if (nConnectedChild_ != getChildrenCount())
        return PR_FALSE;
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::doStatsReconfigure
//-----------------------------------------------------------------------------

PRBool ParentStats::doStatsReconfigure(void)
{
    if (!statsBackupManager_->doReconfigureChanges())
    {
        PR_ASSERT(0);
        return PR_FALSE;
    }
    int nChildNo = 0;
    for (nChildNo = 0; nChildNo < getChildrenCount(); ++nChildNo)
    {
        if (childConnInfo_[nChildNo].isValidConnectionFD() == PR_TRUE)
        {
            initChildData(nChildNo);
            int pid = childConnInfo_[nChildNo].getChildPid();
            initWebModuleList(pid);
        }
    }
    updateProcessSlots();
    updateThreadSlots();
    updateVSCoreData();
    updateProcessJavaData();
    // VSJava data don't need to initialize here as child will inform when web
    // module list is reinitialized.
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// ParentStats::initOrUpdateStats
//-----------------------------------------------------------------------------

PRBool ParentStats::initOrUpdateStats(int nChildNo,
                                      StatsMessages msgType,
                                      const char* callerContext,
                                      PFnInitOrUpdate bkupMgrFunc,
                                      const char* msgStringArg)
{
    PRFileDesc* fd = childConnInfo_[nChildNo].getConnectionFD();
    Messenger messenger(nChildNo, fd, msgErrorHandler_);
    PRBool fSuccess = PR_FALSE;
    if (messenger.sendMsgAndRecvResp(msgType,
                                     callerContext, msgStringArg) != PR_TRUE)
        return fSuccess;
    fSuccess = (statsBackupManager_->*bkupMgrFunc)(messenger.getBufferReader());
    if (fSuccess != PR_TRUE)
    {
        msgErrorHandler_.logInternalStatsError(callerContext, nChildNo);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::initOrUpdateStats
//-----------------------------------------------------------------------------

PRBool ParentStats::initOrUpdateStats(int nChildNo, int pid,
                                      StatsMessages msgType,
                                      const char* callerContext,
                                      PFnProcInitOrUpdate bkupMgrFunc,
                                      const char* msgStringArg)
{
    PRFileDesc* fd = childConnInfo_[nChildNo].getConnectionFD();
    Messenger messenger(nChildNo, fd, msgErrorHandler_);
    PRBool fSuccess = PR_FALSE;
    if (messenger.sendMsgAndRecvResp(msgType,
                                     callerContext, msgStringArg) != PR_TRUE)
        return fSuccess;
    fSuccess = (statsBackupManager_->*bkupMgrFunc)(pid,
                                                   messenger.getBufferReader());
    if (fSuccess != PR_TRUE)
    {
        msgErrorHandler_.logInternalStatsError(callerContext, nChildNo);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// ParentStats::initOrUpdateStats
//-----------------------------------------------------------------------------

PRBool ParentStats::initOrUpdateStats(int nChildNo, int pid,
                                      StatsMessages msgType,
                                      const char* callerContext,
                                      PFnStrProcInitOrUpdate bkupMgrFunc,
                                      const char* msgStringArg)
{
    PRFileDesc* fd = childConnInfo_[nChildNo].getConnectionFD();
    Messenger messenger(nChildNo, fd, msgErrorHandler_);
    PRBool fSuccess = PR_FALSE;
    if (messenger.sendMsgAndRecvResp(msgType,
                                     callerContext, msgStringArg) != PR_TRUE)
        return fSuccess;
    fSuccess = (statsBackupManager_->*bkupMgrFunc)(pid, msgStringArg,
                                                   messenger.getBufferReader());
    if (fSuccess != PR_TRUE)
    {
        msgErrorHandler_.logInternalStatsError(callerContext, nChildNo);
    }
    return fSuccess;
}

