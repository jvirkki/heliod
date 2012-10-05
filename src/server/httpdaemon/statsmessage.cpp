#include "httpdaemon/statsmessage.h"
#ifdef XP_UNIX
#include "httpdaemon/ParentAdmin.h"  // ParentAdmin
#else
#include "httpdaemon/NTStatsServer.h" // NTStatsServer
#endif
#include "httpdaemon/statsmanager.h"  // StatsManager
#include "httpdaemon/dbthttpdaemon.h" // DBT_xxx
#include "base/ereport.h"             // ereport
#include "private/pprio.h"            // PR_ImportFile


////////////////////////////////////////////////////////////////

// StatsServerMessage methods

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsServerMessage::StatsServerMessage
//-----------------------------------------------------------------------------

StatsServerMessage::StatsServerMessage(StatsFileHandle osFd ,
                                       PRBool fInvalidateAtEnd) :
                    StatsServerMessageBase(osFd),
                    fInvalidateAtEnd_(fInvalidateAtEnd)
{
    statsMsg_ = 0;
    nLastMsgSendLen_ = 0;
    memset(&msgHdr_, 0, sizeof(msgHdr_));
    //memset(statsBuffer_, 0, sizeof(statsBuffer_));
    char* buffer = getMessageBuffer();
    statsBuffer_.useStatic(buffer + STATS_MESSAGE_HEADER_SIZE,
                           STATS_MESSAGE_MAX_LEN, 0);
    fFDError_ = PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsServerMessage::~StatsServerMessage
//-----------------------------------------------------------------------------

StatsServerMessage::~StatsServerMessage(void)
{
    if (fInvalidateAtEnd_ == PR_TRUE)
        invalidate();
}

//-----------------------------------------------------------------------------
// private methods
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// StatsServerMessage::sendStatsReqMessage
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::sendStatsReqMessage(WDMessages msg,
                                               const void* reqBuffer,
                                               int reqLen)
{
    if (reqLen <= STATS_MESSAGE_MAX_LEN)
    {
        char* buffer = getMessageBuffer();
        msgHdr_.totalMsgLen = reqLen;
        msgHdr_.curPacketLen = reqLen;
        msgHdr_.msgFlags = statsmsgFlagComplete;
        return sendStatsMessage(buffer, msg, reqBuffer, reqLen);
    }
    PR_ASSERT(0);
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsServerMessage::sendStatsMessage
//
// This is the lowest level function which sends request/response/
// errors etc.
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::sendStatsMessage(char* buffer,
                                            WDMessages messageType,
                                            const void* msgdata,
                                            int datalength)
{
    fFDError_ = PR_FALSE;
    char nullBuffer[STATS_MESSAGE_HEADER_PAD];
    memset(nullBuffer, 0, sizeof(nullBuffer));
    int nPos = 0;
    // enum size must be an int and must be of similar size for 32 bit and 64
    // bit.
    memcpy(buffer, &messageType, sizeof(messageType));
    nPos += sizeof(messageType);
    // Append pad bytes
    memcpy(buffer + nPos, nullBuffer, sizeof(nullBuffer));
    nPos += sizeof(nullBuffer);
    // Append message header
    memcpy(buffer + nPos, &msgHdr_, sizeof(msgHdr_));
    nPos += sizeof(msgHdr_);
    if (msgdata)
    {
        // Now append message
        memcpy(buffer + nPos, msgdata, datalength);
    }
    nLastMsgSendLen_ = 0;
    if (writeMsg(buffer, datalength + nPos, &nLastMsgSendLen_) == 0)
    {
        fFDError_ = PR_TRUE;    // Error in sending message
        return PR_FALSE;
    }
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// StatsServerMessage::sendStatsRequest
//
// public members methods
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::sendStatsRequest(StatsMessages type,
                                            const void* reqBuffer,
                                            int reqLen)
{
    msgHdr_.msgType = type;
    msgHdr_.senderPid = -1;     // No need to send pid for normal requests
    return sendStatsReqMessage(wdmsgReqStatsData, reqBuffer, reqLen);
}

//-----------------------------------------------------------------------------
// StatsServerMessage::sendStatsNotification
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::sendStatsNotification(int type,
                                                 void* reqBuffer,
                                                 int reqLen)
{
    // On Linux, getpid for different threads are different so
    // we can't trust on getpid() call here. We need to get
    // the pid which is registered to primordial
    msgHdr_.senderPid = StatsManager::getPid();
    msgHdr_.msgType = type;
    return sendStatsReqMessage(wdmsgStatsNotification, reqBuffer, reqLen);
}

//-----------------------------------------------------------------------------
// StatsServerMessage::sendLongStatsResponse
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::sendLongStatsResponse(WDMessages messageType,
                                                 const char* msgdata,
                                                 int datalength)
{
    int nIndex = 0;
    int remainingLen = datalength;
    int sendLen = 0;
    for (nIndex = 0; (remainingLen > 0); ++nIndex)
    {
        msgHdr_.totalMsgLen = datalength;
        msgHdr_.senderPid = -1;

        // setup curPacketLen
        if (remainingLen > STATS_MESSAGE_MAX_LEN)
        {
            msgHdr_.curPacketLen = STATS_MESSAGE_MAX_LEN;
            remainingLen -= STATS_MESSAGE_MAX_LEN;
        }
        else
        {
            msgHdr_.curPacketLen = remainingLen;
            remainingLen = 0;
        }

        // setup the Message Flags
        if (nIndex == 0)
            msgHdr_.msgFlags = statsmsgFlagFirstMsg;
        else
        {
            if (remainingLen > 0)
                msgHdr_.msgFlags = statsmsgFlagNextMsg;
            else
                msgHdr_.msgFlags = statsmsgFlagLastMsg;
        }

        int nPacketLen = msgHdr_.curPacketLen;
        char* buffer = getMessageBuffer();
        if (sendStatsMessage(buffer, messageType,
                             msgdata + sendLen, nPacketLen) != PR_TRUE)
            return PR_FALSE;
        sendLen += msgHdr_.curPacketLen;
        if (remainingLen > 0)
        {
            if (RecvFromServer() == 0)
                return PR_FALSE;
            if (isStatsMessage() != PR_TRUE)
            {
                return PR_FALSE;
            }
            if (getLastMsgType() != wdmsgRespStatsDataAck)
            {
                return PR_FALSE;
            }
        }
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsServerMessage::recvLongStatsMessage
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::recvLongStatsMessage(StatsMessages statsMsgType)
{
    void* buffer = getMessageBuffer();
    int nTotalLen = msgHdr_.totalMsgLen;
    if (msgHdr_.totalMsgLen == -1 || msgHdr_.totalMsgLen == 0)
        return PR_FALSE;
    statsBuffer_.ensureCapacity(msgHdr_.totalMsgLen);
    while (!((msgHdr_.msgFlags == statsmsgFlagComplete) ||
             (msgHdr_.msgFlags == statsmsgFlagLastMsg)))
    {
        if (statsMsg_ == 0)
            break;
        statsBuffer_.append(statsMsg_, msgHdr_.curPacketLen);
        if (sendStatsResponse(wdmsgRespStatsDataAck) != PR_TRUE)
            break;
        char* msg = RecvFromServer();
        if (msg == NULL)
            break;
        if (isStatsMessage() != PR_TRUE)
            break;
        if (getStatsMsgType() != statsMsgType)
        {
            PR_ASSERT(0);
            break;
        }
    }

    if ((msgHdr_.msgFlags == statsmsgFlagComplete) ||
        (msgHdr_.msgFlags == statsmsgFlagLastMsg))
    {
        statsBuffer_.append(statsMsg_, msgHdr_.curPacketLen);
        statsMsg_ = const_cast <char*>(statsBuffer_.data());
        nStatsMsgSize_ = statsBuffer_.length();
        return PR_TRUE;
    }
    return PR_FALSE;
}


//-----------------------------------------------------------------------------
// StatsServerMessage::sendStatsResponse
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::sendStatsResponse(WDMessages messageType,
                                             const char* msgdata,
                                             int datalength)
{
    int totalHdrSize = STATS_MESSAGE_HEADER_SIZE;
    char* buffer = getMessageBuffer();
    if (datalength <= STATS_MESSAGE_MAX_LEN)
    {
        msgHdr_.totalMsgLen = datalength;
        msgHdr_.curPacketLen = datalength;
        msgHdr_.msgFlags = statsmsgFlagComplete;
        msgHdr_.senderPid = -1;
        return sendStatsMessage(buffer, messageType, msgdata, datalength);
    }
    return sendLongStatsResponse(messageType, msgdata, datalength);
}

//-----------------------------------------------------------------------------
// StatsServerMessage::sendStatsResponse
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::sendStatsResponse(WDMessages messageType,
                                             StatsMsgBuff& msgBuff)
{
    // Request type of header will contain the request type for
    // which the response is sent i.e. it remains unchanged.
    const char* msgdata = (const char*) msgBuff.data();
    int datalength = msgBuff.length();
    return sendStatsResponse(messageType, msgdata, datalength);
}

//-----------------------------------------------------------------------------
// StatsServerMessage::sendStatsError
//-----------------------------------------------------------------------------

int StatsServerMessage::sendStatsError(StatsErrorMessages errMsg)
{
    return sendStatsResponse(wdmsgRespError, (char*) &errMsg, sizeof(errMsg));
}

//-----------------------------------------------------------------------------
// StatsServerMessage::getStatsError
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::getStatsError(StatsErrorMessages& errMsg)
{
    if(getLastMsgType() == wdmsgRespError)
    {
        // Last message was an error.
        const void* msg = getStatsMessage();
        int recvLen = getStatsMessageLen();
        if (msg && (recvLen == sizeof(StatsErrorMessages)))
        {
            memcpy(&errMsg, msg, sizeof(StatsErrorMessages));
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// StatsServerMessage::setupStatsMessage
//-----------------------------------------------------------------------------

void StatsServerMessage::setupStatsMessage(char* msg)
{
    const int hdrSize = sizeof(msgHdr_);
    msg += STATS_MESSAGE_HEADER_PAD;
    int msgBytes = getRecvBytes();
    msgBytes -= (sizeof(WDMessages));
    msgBytes -= STATS_MESSAGE_HEADER_PAD;
    if (msgBytes >= hdrSize)
    {
        memcpy(&msgHdr_, msg, hdrSize);
        nStatsMsgSize_ = msgBytes - hdrSize;
        statsMsg_ = msg + hdrSize;
    }
    else
    {
        PR_ASSERT(0);
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_StatsServerMessage_InternalError));
    }
    return;
}

//-----------------------------------------------------------------------------
// StatsServerMessage::RecvFromServer
//-----------------------------------------------------------------------------

char* StatsServerMessage::RecvFromServer()
{
    resetMessage();
    fFDError_ = PR_FALSE;
    char* msg = StatsServerMessageBase::RecvFromServer();
    if (!msg)
    {
        fFDError_ = PR_TRUE;    // Error in receiving message
        return msg;
    }
    if (isStatsMessage() == PR_TRUE)
    {
        setupStatsMessage(msg);
    }
    return msg;
}

//-----------------------------------------------------------------------------
// StatsServerMessage::resetMessage
//-----------------------------------------------------------------------------

void StatsServerMessage::resetMessage(void)
{
    const int hdrSize = sizeof(msgHdr_);
    statsMsg_ = 0;
    nStatsMsgSize_ = 0;
    memset(&msgHdr_, 0, hdrSize);;
    nLastMsgSendLen_ = 0;
}


//-----------------------------------------------------------------------------
// StatsServerMessage::sendRecvStatsRequest
//
// This sends a wdmsgRespStatsData with stats request type statsMsgType.
// Then reads the response which must be wdmsgRespStatsData and
// Stats Request type must be mstType.
// Return PR_TRUE on success or else return PR_FALSE.
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::sendRecvStatsRequest(StatsMessages statsMsgType,
                                                const void* sendBuffer,
                                                int sendBufLen)
{
    PRBool fSuccess = PR_FALSE;
    if (sendStatsRequest(statsMsgType, sendBuffer, sendBufLen) != PR_TRUE)
        return PR_FALSE;        // Error sending message
    statsBuffer_.clear();
    char* msg = RecvFromServer();
    if (msg == NULL)
    {
        // Error in socket communication. Most probably peer got disconnected.
        // Caller should close the connection.
        return PR_FALSE;
    }
    if (isStatsMessage() == PR_TRUE)
    {
        if ((getLastMsgType() == wdmsgRespStatsData) &&
            (msgHdr_.msgType == statsMsgType))
        {
            if (msgHdr_.msgFlags != statsmsgFlagComplete)
            {
                fSuccess = recvLongStatsMessage(statsMsgType);
            }
            else
                fSuccess = PR_TRUE;
        }
        else if(getLastMsgType() == wdmsgRespError)
        {
            // Just pass the error to the client. Right now error messages
            // can't be long messages.
        }
        else
        {
            PR_ASSERT(0);
            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_StatsServerMessage_InternalError));
        }
    }
    else
    {
        PR_ASSERT(0);
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_StatsServerMessage_InternalError));
    }
    return fSuccess;

}

//-----------------------------------------------------------------------------
// StatsServerMessage::sendRecvStatsRequestString
//-----------------------------------------------------------------------------

PRBool
StatsServerMessage::sendRecvStatsRequestString(StatsMessages statsMsgType,
                                               const char* sendBuffer)
{
    StatsMsgBuff msgbuffReq;
    msgbuffReq.appendString(sendBuffer);
    return sendRecvStatsRequest(statsMsgType,
                                msgbuffReq.data(), msgbuffReq.length());
}

//-----------------------------------------------------------------------------
// StatsServerMessage::sendRecvNotification
//
// Sends the notification and receive the response
//-----------------------------------------------------------------------------

PRBool
StatsServerMessage::sendRecvNotification(StatsNoticeMessages statsNoticeType,
                                         void* sendBuffer,
                                         int sendBufLen)
{
    if (sendStatsNotification(statsNoticeType, sendBuffer, sendBufLen) !=
        PR_TRUE)
        return PR_FALSE;
    const char* dataRcvd = RecvFromServer();
    if (dataRcvd == NULL)
    {
        // Error in socket communication. Most probably peer got disconnected.
        // Caller should close the connection.
        return PR_FALSE;
    }
    WDMessages lastMsgType = getLastMsgType();
    if (lastMsgType == wdmsgRespError)
    {
        return PR_FALSE;
    }
    if (lastMsgType != wdmsgStatsNotificationAck)
    {
        PR_ASSERT(0);
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_StatsServerMessage_InternalError));
        return PR_FALSE;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsServerMessage::peekMessage
//
// This function returns PR_TRUE if there is a message to be read from file
// handle.  Also if connection is broken then too, it returns PR_TRUE so that
// caller will try to read the message and then it will find out whether
// connection is alive or not.
//-----------------------------------------------------------------------------

PRBool StatsServerMessage::peekMessage(void)
{
#ifdef XP_UNIX
    PRPollDesc pollArray[1];
    PRPollDesc& desc = pollArray[0];
    desc.fd = PR_ImportFile(getFD());
    desc.in_flags = (PR_POLL_READ|PR_POLL_EXCEPT);
    desc.out_flags = 0;
    PRIntervalTime timeout = 0;
    PRBool fSuccess = PR_FALSE;
    int nReady = PR_Poll(pollArray, 1, timeout);
    if (nReady <= 0)
    {
        return PR_FALSE;
    }
    else if (desc.out_flags & PR_POLL_READ)
    {
        return PR_TRUE;
    }
    return PR_FALSE;
#else
    return StatsServerMessageBase::peekMessage();
#endif
}

//-----------------------------------------------------------------------------
// StatsServerMessage::connectStatsChannelEx
//-----------------------------------------------------------------------------

StatsServerMessage*
StatsServerMessage::connectStatsChannelEx(const char* name,
                                          PRBool& fStatsEnabled)
{
    fStatsEnabled = PR_FALSE;
    StatsServerMessage* statsMsgChannel = NULL;
#ifdef XP_UNIX
    int retries = 1;
    statsMsgChannel = ParentAdmin::connectStatsChannel(name, retries);
    if (statsMsgChannel)
    {
        statsMsgChannel->SendToServer(wdmsgIdentifyStatsChannel, NULL);
        const char* msg = statsMsgChannel->RecvFromServer();
        if (msg == 0)
        {
            delete statsMsgChannel;
            statsMsgChannel = 0;
        }
        else if (strcmp(msg, "true") == 0)
            fStatsEnabled = PR_TRUE;
    }
#else
    char statsChannelName[1024];
    if (name == NULL)
    {
        memset(statsChannelName, 0, sizeof(statsChannelName));
        NTStatsServer::buildConnectionName(NULL, statsChannelName,
                                           (sizeof(statsChannelName) -1));
        name = statsChannelName;
    }
    statsMsgChannel = NTServerMessage::connectStatsChannel(name);
    if (statsMsgChannel != NULL)
        fStatsEnabled = PR_TRUE;
#endif
    return statsMsgChannel;
}

//-----------------------------------------------------------------------------
// StatsServerMessage::connectStatsChannel
//
// connect to stats channel. It returns NULL if connection is not successful.
// if stats are disabled then disconnect the connection and returns NULL.
// If name is NULL then it will connect to it's own instance (i.e primordial
// process).
//-----------------------------------------------------------------------------

StatsServerMessage* StatsServerMessage::connectStatsChannel(const char* name)
{
    PRBool fStatsEnabled = PR_FALSE;
    StatsServerMessage* statsMsgChannel = NULL;
    statsMsgChannel = StatsServerMessage::connectStatsChannelEx(name,
                                                                fStatsEnabled);
    if (fStatsEnabled != PR_TRUE)
    {
        delete statsMsgChannel;
        statsMsgChannel = NULL;
    }
    return statsMsgChannel;
}

////////////////////////////////////////////////////////////////

// StatsMessenger methods

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsMessenger::sendMsgAndRecvResp
//-----------------------------------------------------------------------------

PRBool StatsMessenger::sendMsgAndRecvResp(StatsMessages msgType,
                                          const char* msgStringArg)
{
    PRBool fSuccess = PR_FALSE;
    if (msgStringArg)
    {
        fSuccess = statsMsgChannel_->sendRecvStatsRequestString(msgType,
                                                                msgStringArg);
    }
    else
    {
        fSuccess = statsMsgChannel_->sendRecvStatsRequest(msgType);
    }

    if (fSuccess != PR_TRUE)
        return PR_FALSE;

    fSuccess = PR_TRUE;
    const char* recvBuffer = statsMsgChannel_->getStatsMessage();
    int recvLen = statsMsgChannel_->getStatsMessageLen();
    bufferReader_.initialize(recvBuffer, recvLen);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMessenger::sendMsgAndRecvResp
//-----------------------------------------------------------------------------

PRBool StatsMessenger::sendMsgAndRecvResp(StatsMessages msgType,
                                          const void* buffer, int bufferLen)
{
    PRBool fSuccess = PR_FALSE;
    fSuccess = statsMsgChannel_->sendRecvStatsRequest(msgType,
                                                      buffer, bufferLen);
    if (fSuccess != PR_TRUE)
        return PR_FALSE;

    fSuccess = PR_TRUE;
    const char* recvBuffer = statsMsgChannel_->getStatsMessage();
    int recvLen = statsMsgChannel_->getStatsMessageLen();
    bufferReader_.initialize(recvBuffer, recvLen);
    return fSuccess;
}

//-----------------------------------------------------------------------------
// StatsMessenger::sendMsgAndProcessResp
//-----------------------------------------------------------------------------

PRBool
StatsMessenger::sendMsgAndProcessResp(StatsMessages msgType,
                                      StatsMessages msgTypeAck,
                                      StatsRespReceiver& receiver,
                                      const char* msgStringArg)
{
    PRBool fSuccess = PR_FALSE;
    do
    {
        fSuccess = sendMsgAndRecvResp(msgType, msgStringArg);
        if (fSuccess != PR_TRUE)
            break;
        int recvLen = bufferReader_.getRemainingBytesCount();
        if (recvLen <= 0)
            break;
        const void* recvBuffer = bufferReader_.getBufferAddress();
        PR_ASSERT(recvBuffer);
        if (receiver.processRecvData(recvBuffer, recvLen) != PR_TRUE)
        {
            fSuccess = PR_FALSE;
            break;
        }

        // From now on send the acknowledgement message with NULL message
        // argument.
        msgType = msgTypeAck;
        msgStringArg = NULL;
    } while (PR_TRUE);
    return fSuccess;
}


////////////////////////////////////////////////////////////////

// StatsRespFdReceiver methods

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsRespFdReceiver::processRecvData
//-----------------------------------------------------------------------------

PRBool StatsRespFdReceiver::processRecvData(const void* recvData,
                                            int recvLen)
{
    net_write(fd_, recvData, recvLen);
    return PR_TRUE;
}


////////////////////////////////////////////////////////////////

// StatsRespStrReceiver methods

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsRespStrReceiver::processRecvData
//-----------------------------------------------------------------------------

PRBool StatsRespStrReceiver::processRecvData(const void* recvData,
                                             int recvLen)
{
    strResp_.append((const char*) recvData, recvLen);
    return PR_TRUE;
}

