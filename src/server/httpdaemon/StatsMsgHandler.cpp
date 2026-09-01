#include "httpdaemon/StatsMsgHandler.h"
#include "httpdaemon/statsmanager.h" // StatsManager
#include "httpdaemon/StatsMsgPreparer.h" // StatsMsgPreparer
#include "safs/perf.h"          // write_stats_xml
#include "safs/dump.h"          // write_stats_dump
#include "support/stringvalue.h" // StringValue

//-----------------------------------------------------------------------------
// isEnabled static (to file) function
//-----------------------------------------------------------------------------

static PRBool isEnabled(PList_t qlist,
                        const char* element)
{
    if (qlist)
    {
        ParamList* plist;
        plist = param_GetElem(qlist, element);
        if (plist && plist->value && StringValue::isBoolean(plist->value))
        {
            return StringValue::getBoolean(plist->value);
        }
    }

    // By default, everything is enabled
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsResponseChannel Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsResponseChannel::StatsResponseChannel
//-----------------------------------------------------------------------------

StatsResponseChannel::StatsResponseChannel(StatsServerMessage& msgChannel,
                                           int nCacheSize) :
                                           msgChannel_(msgChannel),
                                           nCacheSize_(nCacheSize)
{
    fError_ = PR_FALSE;
    cacheBuffer_.ensureCapacity(nCacheSize_);
}

//-----------------------------------------------------------------------------
// StatsResponseChannel::~StatsResponseChannel
//-----------------------------------------------------------------------------

StatsResponseChannel::~StatsResponseChannel(void)
{
}

//-----------------------------------------------------------------------------
// StatsResponseChannel::writeMessage
//-----------------------------------------------------------------------------

PRInt32
StatsResponseChannel::writeMessage(const char* buf, PRInt32 amount)
{
    PRBool fSuccess = PR_FALSE;
    fSuccess = msgChannel_.sendStatsResponse(wdmsgRespStatsData,
                                             (const char*) buf, amount);
    if (fSuccess == PR_TRUE)
    {
        if (msgChannel_.RecvFromServer() == 0)
            fError_ = PR_TRUE;
    }
    else
    {
        // Something wrong happened in sending message.
        fError_ = PR_TRUE;
        close();
        return 0;
    }
    return amount;
}

//-----------------------------------------------------------------------------
// StatsResponseChannel::flushCache
//-----------------------------------------------------------------------------

void StatsResponseChannel::flushCache(void)
{
    if (fError_ != PR_TRUE)
    {
        int length = cacheBuffer_.length();
        if (length > 0)
        {
            writeMessage(cacheBuffer_.data(), length);
            cacheBuffer_.clear();
        }
    }
}

//-----------------------------------------------------------------------------
// StatsResponseChannel::sendEndOfDataMarker
//-----------------------------------------------------------------------------

void StatsResponseChannel::sendEndOfDataMarker(void)
{
    if (fError_ != PR_TRUE)
    {
        // Send 0 bytes to inform that  no more data available
        msgChannel_.sendStatsResponse(wdmsgRespStatsData);
    }
}

//-----------------------------------------------------------------------------
// StatsResponseChannel::write
//-----------------------------------------------------------------------------

PRInt32 StatsResponseChannel::write(const void* buf, PRInt32 amount)
{
    if (amount > 0)
    {
        if (fError_ == PR_TRUE)
        {
            // close must have been already called.
            // so it should not arrive here.
            return amount;
        }
        if (nCacheSize_ == 0)
        {
            return writeMessage((const char*) buf, amount);
        }

        // Caching is enabled
        int length = cacheBuffer_.length();
        if (length + amount > nCacheSize_)
        {
            flushCache();
            if (amount > nCacheSize_)
            {
                // Flush the entire amount
                writeMessage((const char*) buf, amount);
            }
            else
            {
                // Save the data in cache
                cacheBuffer_.appendData(buf, amount);
            }
        }
        else
        {
            // Save the data in cache
            cacheBuffer_.appendData(buf, amount);
        }
    }
    return amount;
}



////////////////////////////////////////////////////////////////

// StatsNoticeSender Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsNoticeSender::StatsNoticeSender
//-----------------------------------------------------------------------------

StatsNoticeSender::StatsNoticeSender(StatsFileHandle osfd)
{
    osFd_ = osfd;
    next = 0;
}

//-----------------------------------------------------------------------------
// StatsNoticeSender::~StatsNoticeSender
//-----------------------------------------------------------------------------

StatsNoticeSender::~StatsNoticeSender(void)
{
    // Don't close the fd. It is closed by ParentAdmin on Unix platforms.
    // On Windows NTStatsServer closes the fd (pipe handle).
}

//-----------------------------------------------------------------------------
// StatsNoticeSender::sendNotification
//-----------------------------------------------------------------------------

PRBool
StatsNoticeSender::sendNotification(StatsNoticeMessages statsNoticeType)
{
    // Send the notification to the peer.
    StatsServerMessage msgChannel(osFd_,
                                  PR_TRUE       // Don't close the fd
                                 );
    msgChannel.sendStatsNotification(statsNoticeType, 0, 0);
    return PR_TRUE;
}



////////////////////////////////////////////////////////////////

// StatsMsgHandler Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsMsgHandler::StatsMsgHandler
//-----------------------------------------------------------------------------

StatsMsgHandler::StatsMsgHandler(StatsUpdateHandler& updateHandler) :
                                updateHandler_(updateHandler)
{
    noticeSender_ = NULL;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::~StatsMsgHandler
//-----------------------------------------------------------------------------

StatsMsgHandler::~StatsMsgHandler(void)
{
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::processStatsMessage
//
// Return PR_FALSE if there is error in msgChannel
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::processStatsMessage(StatsServerMessage& msgChannel)
{
    // Handle Stats Message here
    char* msg = msgChannel.RecvFromServer();
    WDMessages msgType = msgChannel.getLastMsgType();
    if (!msg)
    {
        return PR_FALSE;
    }
    PRBool fSuccess = PR_TRUE;
    if (msgType == wdmsgReqStatsData)
    {
        int statsMsgType = msgChannel.getStatsMsgType();
        PRBool fRequestNotHandled = PR_FALSE;
        switch (statsMsgType)
        {
            case statsmsgReqGetStatsXMLData:
            {
                handleStatXMLRequest(msgChannel);
                break;
            }
            case statsmsgReqGetServiceDump:
            {
                handleServiceDumpRequest(msgChannel);
                break;
            }
            case statsmsgReqGetStatsProcessInfo:
            {
                fSuccess = handleGetProcessInfo(msgChannel);
                break;
            }
            case statsmsgReqGetJdbcConnPools:
            {
                fSuccess = handleGetJdbcConnectionPool(msgChannel);
                break;
            }
            case statsmsgReqGetJvmMgmtData:
            {
                fSuccess = handleGetJvmMgmtStats(msgChannel);
                break;
            }
            case statsmsgReqGetListenSlotData:
            {
                fSuccess = handleGetListenSlots(msgChannel);
                break;
            }
            case statsmsgReqGetAccumulatedVSData:
            {
                updateHandler_.updateAccumulatedVSData();
                fRequestNotHandled = PR_TRUE;
                break;
            }
            case statsmsgReqGetStatsVirtualServerInfo:
            {
                fSuccess = updateVSCoreDataInfo(msgChannel);
                if (fSuccess == PR_TRUE)
                {
                    fRequestNotHandled = PR_TRUE;
                }
                break;
            }
            case statsmsgReqGetCpuInfo:
            {
                StatsManager::updateStats();
                fRequestNotHandled = PR_TRUE;
                break;
            }
            case statsmsgReqGetSessReplData:
            {
                updateHandler_.updateSessionReplicationData();
                fRequestNotHandled = PR_TRUE;
                break;
            }
            case statsmsgReqGetStatsHeader:
            case statsmsgReqGetPidList:
            case statsmsgReqGetStringStore:
            case statsmsgReqGetVSList:
            case statsmsgReqGetWebModuleList:
            {
                fRequestNotHandled = PR_TRUE;
                break;
            }
            case statsmsgReqGetWebModuleData:
            {
                fSuccess = updateWebModuleData(msgChannel);
                if (fSuccess == PR_TRUE)
                {
                    fRequestNotHandled = PR_TRUE;
                }
                break;
            }
            case statsmsgReqGetAllWebModuleData:
            {
                updateHandler_.updateWebModuleData();
                fRequestNotHandled = PR_TRUE;
                break;
            }
            case statsmsgReqGetServletData:
            {
                fSuccess = updateServletData(msgChannel);
                if (fSuccess == PR_TRUE)
                {
                    fRequestNotHandled = PR_TRUE;
                }
                break;
            }
            case statsmsgReqGetAllServletData:
            {
                updateHandler_.updateServletData();
                fRequestNotHandled = PR_TRUE;
                break;
            }
            case statsmsgIdentifyAsNoticeReceiver:
            {
                fSuccess = addNotificationReceiver(msgChannel);
                break;
            }
            default:
            {
                fSuccess = PR_FALSE;
                msgChannel.sendStatsError(statsmsgErrorInReq);
            }
        }
        if (fRequestNotHandled == PR_TRUE)
        {
            fSuccess = StatsManager::processStatsMessage(&msgChannel);
        }
    }
    else if (msgType == wdmsgStatsNotification)
    {
        int pid = msgChannel.getStatsSenderPid();
        StatsNoticeMessages statsNoticeType = (StatsNoticeMessages)
            msgChannel.getStatsMsgType();
        // Send the acknowledgement before handling notification
        // Because if handleNotification tried to do communication with
        // Child again it could deadlock.
        msgChannel.sendStatsResponse(wdmsgStatsNotificationAck, "", 0);
        handleNotifications(statsNoticeType, pid);
    }
    else
    {
        msgChannel.sendStatsError(statsmsgErrorInReq);
        fSuccess = PR_FALSE;
    }

    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::readProcessId
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::readProcessId(StatsServerMessage& msgChannel, PRInt32& pid)
{
    pid = INVALID_PROCESS_ID;
    int nLen = msgChannel.getStatsMessageLen();
    if (nLen == sizeof(PRInt32))
    {
        const void* buffer = msgChannel.getStatsMessage();
        memcpy(&pid, buffer, sizeof(PRInt32));
    }
    if (pid == INVALID_PROCESS_ID)
    {
        msgChannel.sendStatsError(statsmsgErrorInReq);
        return PR_FALSE;
    }
    if (StatsManager::isValidPid(pid) != PR_TRUE)
    {
        msgChannel.sendStatsError(statsmsgPidNotExist);
        return PR_FALSE;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::handleGetProcessInfo
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::handleGetProcessInfo(StatsServerMessage& msgChannel)
{
    PRInt32 pid = INVALID_PROCESS_ID;
    if (readProcessId(msgChannel, pid) != PR_TRUE)
    {
        return PR_FALSE;
    }

    PRBool fSuccess = PR_FALSE;
    fSuccess = updateHandler_.updateProcessSlotForPid(pid);
    if (fSuccess != PR_TRUE)
    {
        msgChannel.sendStatsError(statsmsgErrorProcess);
    }
    else
    {
        StatsMsgBuff msgbuff;
        StatsMsgPreparer msgPreparer(StatsManager::getHeader());
        fSuccess = msgPreparer.prepareProcessStats(msgbuff, NULL, pid);
        if (fSuccess == PR_TRUE)
        {
            msgChannel.sendStatsResponse(wdmsgRespStatsData, msgbuff);
        }
        else
        {
            msgChannel.sendStatsError(statsmsgErrorProcess);
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::handleGetJdbcConnectionPool
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::handleGetJdbcConnectionPool(StatsServerMessage& msgChannel)
{
    PRInt32 pid = INVALID_PROCESS_ID;
    if (readProcessId(msgChannel, pid) != PR_TRUE)
    {
        return PR_FALSE;
    }

    PRBool fSuccess = PR_FALSE;
    fSuccess = updateHandler_.updateJdbcConnPools(pid);
    if (fSuccess != PR_TRUE)
    {
        msgChannel.sendStatsError(statsmsgErrorProcess);
    }
    else
    {
        StatsMsgBuff msgbuff;
        StatsMsgPreparer msgPreparer(StatsManager::getHeader());
        fSuccess = msgPreparer.prepareJdbcConnPoolStats(msgbuff, pid);
        if (fSuccess == PR_TRUE)
        {
            msgChannel.sendStatsResponse(wdmsgRespStatsData, msgbuff);
        }
        else
        {
            msgChannel.sendStatsError(statsmsgErrorProcess);
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::handleGetJvmMgmtStats
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::handleGetJvmMgmtStats(StatsServerMessage& msgChannel)
{
    PRInt32 pid = INVALID_PROCESS_ID;
    if (readProcessId(msgChannel, pid) != PR_TRUE)
    {
        return PR_FALSE;
    }

    PRBool fSuccess = PR_FALSE;
    fSuccess = updateHandler_.updateJvmMgmtData(pid);
    if (fSuccess != PR_TRUE)
    {
        msgChannel.sendStatsError(statsmsgErrorProcess);
    }
    else
    {
        StatsMsgBuff msgbuff;
        StatsMsgPreparer msgPreparer(StatsManager::getHeader());
        fSuccess = msgPreparer.prepareJvmMgmtStats(msgbuff, pid);
        if (fSuccess == PR_TRUE)
        {
            msgChannel.sendStatsResponse(wdmsgRespStatsData, msgbuff);
        }
        else
        {
            msgChannel.sendStatsError(statsmsgErrorProcess);
        }
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::handleGetListenSlots
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::handleGetListenSlots(StatsServerMessage& msgChannel)
{
    PRBool fSuccess = PR_FALSE;
    PRInt32 pid = INVALID_PROCESS_ID;
    if (readProcessId(msgChannel, pid) != PR_TRUE)
    {
        return PR_FALSE;
    }
    StatsMsgBuff msgbuff;
    StatsMsgPreparer msgPreparer(StatsManager::getHeader());
    fSuccess = msgPreparer.prepareListenSlotsStats(msgbuff, NULL, pid);
    if (fSuccess == PR_TRUE)
    {
        msgChannel.sendStatsResponse(wdmsgRespStatsData, msgbuff);
    }
    else
    {
        msgChannel.sendStatsError(statsmsgErrorProcess);
    }
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::updateVSCoreDataInfo
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::updateVSCoreDataInfo(StatsServerMessage& msgChannel)
{
    StatsMsgBuff msgbuff;
    const char* vsId = msgChannel.getStatsMessage();
    int nLen = msgChannel.getStatsMessageLen();
    if (nLen == STATS_ALIGN(strlen(vsId) + 1))
    {
        updateHandler_.updateVSCoreData(vsId);
        return PR_TRUE;
    }
    else
    {
        msgChannel.sendStatsError(statsmsgErrorInReq);
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::updateWebModuleData
//-----------------------------------------------------------------------------

PRBool StatsMsgHandler::updateWebModuleData(StatsServerMessage& msgChannel)
{
    StatsMsgBuff msgbuff;
    const char* webModuleName = msgChannel.getStatsMessage();
    int nLen = msgChannel.getStatsMessageLen();
    if (nLen == STATS_ALIGN(strlen(webModuleName) + 1))
    {
        updateHandler_.updateWebModuleData(webModuleName);
        return PR_TRUE;
    }
    else
    {
        msgChannel.sendStatsError(statsmsgErrorInReq);
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::updateServletData
//-----------------------------------------------------------------------------

PRBool StatsMsgHandler::updateServletData(StatsServerMessage& msgChannel)
{
    StatsMsgBuff msgbuff;
    const char* webModuleName = msgChannel.getStatsMessage();
    int nLen = msgChannel.getStatsMessageLen();
    if (nLen == STATS_ALIGN(strlen(webModuleName) + 1))
    {
        updateHandler_.updateServletData(webModuleName);
        return PR_TRUE;
    }
    else
    {
        msgChannel.sendStatsError(statsmsgErrorInReq);
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::handleNotifications
//-----------------------------------------------------------------------------

void
StatsMsgHandler::handleNotifications(StatsNoticeMessages statsNoticeType,
                                     int senderPid)
{
    switch (statsNoticeType)
    {
        case statsNoticeWebModuleInit:
        {
            updateHandler_.initWebModuleList(senderPid);
            // Update the process slot also for that pid. After web modules
            // initialization few things e.g jvmManagentStats in
            // StatsProcessSlot may have been enabled.
            updateHandler_.updateProcessSlotForPid(senderPid);
            if (updateHandler_.isAllChildWebModInitDone() == PR_TRUE)
            {
                StatsManager::setWebappsEnabled();
                sendNotification(statsNoticeWebModuleInit);
            }
            break;
        }
        case statsNoticeReconfigureDone:
        {
            updateHandler_.updateOnReconfig(senderPid);
            break;
        }
        case statsNoticeJvmInitialized:
        {
            StatsManager::setJvmEnabled();
            break;
        }
        case statsNoticeJdbcNodesCountChanged:
        {
            updateHandler_.updateJdbcConnPools(senderPid);
            sendNotification(statsNoticeJdbcNodesCountChanged);
            break;
        }
        case statsNoticeSessReplNodeCountChanged:
        {
            updateHandler_.updateSessionReplicationData();
            sendNotification(statsNoticeSessReplNodeCountChanged);
            break;
        }
        default:
        {
            PR_ASSERT(0);
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::handleStatXMLRequest
//
// query string is the sent as part of the request.
// This method updates all the data and then generates the xml which is sent
// over the message channel to child.
//-----------------------------------------------------------------------------

void StatsMsgHandler::handleStatXMLRequest(StatsServerMessage& msgChannel)
{
    if (updateHandler_.isReadyForUpdate() != PR_TRUE)
    {
        msgChannel.sendStatsError(statsmsgErrorNotReady);
        return;
    }
    // Retrieve the Query and make qlist object
    PList_t qlist = NULL;
    const char* query = msgChannel.getStatsMessage();
    int recvLen = msgChannel.getStatsMessageLen();
    if (query && (recvLen > 0))
    {
        qlist = param_Parse(NULL, query, NULL);
    }
    updateHandler_.updateProcessSlots();
    updateHandler_.updateProcessJavaData();
    if (isEnabled(qlist, "thread") == PR_TRUE)
        updateHandler_.updateThreadSlots();
    if (isEnabled(qlist, "virtual-server") == PR_TRUE)
    {
        updateHandler_.updateVSCoreData();
        updateHandler_.updateVSJavaData();
    }
    if (isEnabled(qlist, "session-replication") == PR_TRUE)
    {
        updateHandler_.updateSessionReplicationData();
    }
    StatsResponseChannel response(msgChannel, STATS_MESSAGE_MAX_LEN);
    write_stats_xml(response, StatsManager::getHeader(), qlist);
    response.flushCache();
    response.sendEndOfDataMarker();
    PListDestroy(qlist);

    return;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::handleStatXMLRequest
//
// Updates stats data then calls the service_dumpstats function.
//-----------------------------------------------------------------------------

void StatsMsgHandler::handleServiceDumpRequest(StatsServerMessage& msgChannel)
{
    if (updateHandler_.isReadyForUpdate() != PR_TRUE)
    {
        msgChannel.sendStatsError(statsmsgErrorNotReady);
        return;
    }
    updateHandler_.updateProcessSlots();
    updateHandler_.updateThreadSlots();
    updateHandler_.updateVSCoreData();
    updateHandler_.updateVSJavaData();

    StatsResponseChannel response(msgChannel, STATS_MESSAGE_MAX_LEN);
    write_stats_dump(response, StatsManager::getHeader());
    response.flushCache();
    response.sendEndOfDataMarker();

    return;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::addNotificationReceiver
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::addNotificationReceiver(StatsServerMessage& msgChannel)
{
    StatsFileHandle osfd = msgChannel.getFD();
    if (!statsFindNodeFromList(noticeSender_, osfd))
    {
        StatsNoticeSender* noticeSender = new StatsNoticeSender(osfd);
        statsAppendLast(this, noticeSender);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::sendNotification
//-----------------------------------------------------------------------------

PRBool
StatsMsgHandler::sendNotification(StatsNoticeMessages statsNoticeType)
{
    StatsNoticeSender* noticeSender = noticeSender_;
    while (noticeSender)
    {
        noticeSender->sendNotification(statsNoticeType);
        noticeSender = noticeSender->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsMsgHandler::processCloseFd
//-----------------------------------------------------------------------------

void
StatsMsgHandler::processCloseFd(StatsFileHandle osfd)
{
    StatsNoticeSender* noticeSender = noticeSender_;
    statsFreeNode<StatsNoticeSender>(this, osfd);
}

