#ifndef _STATSMESSAGE_H_
#define _STATSMESSAGE_H_

#include <nspr.h>
#ifdef XP_WIN32
#include "base/ntservermessage.h"  // StatsServerMessageBase
#else
#include "base/wdservermessage.h"  // StatsServerMessageBase
#endif
#include "httpdaemon/statsutil.h"  // StatsManagerUtil
#include "support/NSString.h"      // NSString

struct StatsMsgHeader
{
    PRInt32 msgType; // This could be StatsMessages/StatsNoticeMessages
    PRInt32 senderPid; // used for identifying the sender of the message
    PRInt32 totalMsgLen; // length of the complete message
    PRInt32 curPacketLen; // length of the current packet
    PRInt32 msgFlags; // It is a StatsMessageFlags enumerator
    PRInt32 reserved;
};


// Structure of the Stats messages are :
// 1. WatchDog Message type
// 2. 4 bytes pad.
// 3. StatsMsgHeader
// 4. Stats Message

// Header pad is added to make 8 byte boundary alignment
#define STATS_MESSAGE_HEADER_PAD sizeof(PRInt32)

// Stats Message Header is = watchdog header + pad + stats header
#define STATS_MESSAGE_HEADER_SIZE (sizeof(WDMessages) + \
        STATS_MESSAGE_HEADER_PAD + sizeof(struct StatsMsgHeader))

// Maximum length of single package of stats message.
// As the receiver don't know how big is the message so it does
// receive the message with buffer whose size is WDMSGBUFFSIZE.
// As RecvFromServer sets up null byte at the end so substract 4 bytes form
// the size to avoid writing beyond allocated buffer.
#define STATS_MESSAGE_MAX_LEN \
    (WDMSGBUFFSIZE - STATS_MESSAGE_HEADER_SIZE - sizeof(PRInt32))

typedef enum
{
    statsmsgFlagBegin,
    statsmsgFlagComplete, // Complete message in one send call
    statsmsgFlagFirstMsg,
    statsmsgFlagNextMsg,
    statsmsgFlagLastMsg,
    statsmsgFlagEnd
} StatsMessageFlags;


// Stats Request message.
typedef enum
{
    statsmsgReqBegin,
    statsmsgReqGetStatsHeader,
    statsmsgReqGetCpuInfo,
    statsmsgReqGetStringStore,
    statsmsgReqGetStatsProcessInfo,
    statsmsgReqGetStatsXMLData,
    statsmsgReqGetStatsXMLDataAck,
    statsmsgReqGetServiceDump,
    statsmsgReqGetServiceDumpAck,
    statsmsgReqGetAccumulatedVSData,
    statsmsgReqGetStatsVirtualServerInfo,
    statsmsgReqGetVSList,
    statsmsgReqGetListenSlotData,
    statsmsgReqGetWebModuleList,
    statsmsgReqGetWebModuleData,
    statsmsgReqGetAllWebModuleData,
    statsmsgReqGetServletData,
    statsmsgReqGetAllServletData,
    statsmsgReqGetThreadIndexList,
    statsmsgReqGetThreadSlotData,
    statsmsgReqGetPidList,
    statsmsgReqGetJdbcConnPools,
    statsmsgReqGetJvmMgmtData,
    statsmsgReqGetSessReplData,
    statsmsgIdentifyAsNoticeReceiver,
    statsmsgReqTestGetBigFile,
    statsmsgReqEnd
} StatsMessages;

//-----------------------------------------------------------------------------
// StatsServerMessage
//
// Various types of error messages sent over the stats channel.
//-----------------------------------------------------------------------------

// Stats Error messages.
typedef enum
{
    statsmsgErrorBegin,
    statsmsgErrorNotReady, // Not Ready for serving request.
    statsmsgErrorInReq,    // Stats Error in Request
    statsmsgErrorVSData,   // Stats Error in VS data
    statsmsgErrorProcess,  // Stats Error in process data.
    statsmsgPidNotExist,   // Child pid is missing. Cause may be child restart.
    statsmsgErrorJvmStatsNoInit, // Jvm stats are not initialized.
    statsmsgErrorNoSessReplStats, // No session replication stats available.
    statsmsgErrorLast
} StatsErrorMessages ;

// Stats Notice Messages.
typedef enum
{
    statsNoticeBegin,
    statsNoticeVSCoreDataChange,
    statsNoticeJvmInitialized, // Webserver has jvm.
    statsNoticeWebModuleInit, // WebModule is initialized
    statsNoticeReconfigureDone, // Child is done with reconfigure.
    statsNoticeJdbcNodesCountChanged, // Jdbc nodes count has changed.
    statsNoticeSessReplNodeCountChanged, // Session Replication node counts changed.
    statsNoticeAllChildInitialized, // All child are initialized.
    statsNoticeLast
} StatsNoticeMessages ;



//-----------------------------------------------------------------------------
// StatsServerMessage
//
// Class to communicate stats messages
//-----------------------------------------------------------------------------

class HTTPDAEMON_DLL StatsServerMessage : public StatsServerMessageBase
{
    public:
        // constructor; requires a valid fd as returned from a listener if
        // fInvalidateAtEnd is TRUE then it calles invalidate of base class at
        // the end.
        StatsServerMessage(StatsFileHandle osFd,
                           PRBool fInvalidateAtEnd = PR_FALSE);
       
        // Desctructor
        ~StatsServerMessage(void);

        // ------------ Get Methods --------------------

        // get Stats Message (After the stats header)
        const char* getStatsMessage(void) const;

        // Get the length of the stats message (after substracting the stats
        // message header.
        int getStatsMessageLen(void) const;

        // Get the sender process id.
        int getStatsSenderPid(void) const;

        // Returns the msgType of the stats header.
        StatsMessages getStatsMsgType(void) const;

        // If there is an error in sending/receiving message then it
        // returns PR_TRUE
        PRBool isFileDescriptorError(void) const;

        // Reset the message header from previous message, various pointers and
        // lenght etc.
        void resetMessage(void);

        // If the message received is a stats message, this methods sets up
        // message header, sets the message pointer, calculate length etc.
        void setupStatsMessage(char* msg);

        // Function to simplify sending error message.
        int sendStatsError(StatsErrorMessages errMsg);

        // Retrieve the StatsError in previous call.
        PRBool getStatsError(StatsErrorMessages& errMsg);

        // Sends the stats request. It sends the request using
        // wdmsgReqStatsData. reqBuffer and reqLen are the corresponding
        // message request buffer and length.
        PRBool sendStatsRequest(StatsMessages type,
                                 const void* reqBuffer,
                                 int reqLen);

        // sends the stats notification.
        PRBool sendStatsNotification(int type, void* reqBuffer, int reqLen);

        // send the stats Response
        PRBool sendStatsResponse(WDMessages messageType,
                                  const char* msgdata = 0,
                                  int datalength = 0);
        // send the stats response.
        PRBool sendStatsResponse(WDMessages messageType,
                                 StatsMsgBuff& msgBuff);

        // Send the notification and receive the response.
        PRBool sendRecvNotification(StatsNoticeMessages msgType,
                                    void* sendBuffer,
                                    int sendBufLen);

        // Send the stats request with msgType request and receive the
        // response.
        PRBool sendRecvStatsRequest(StatsMessages msgType,
                                    const void* sendBuffer = 0,
                                    int sendBufLen = 0);

        // Simplified version of sendRecvStatsRequest where the sending buffer
        // is a string.
        PRBool sendRecvStatsRequestString(StatsMessages msgType,
                                          const char* sendBuffer);

        // ------------ Overridden methods from base class ----------

        // It calls base class RecvFromServer and if the message is a stats
        // message, it sets up the request header etc.
        virtual char* RecvFromServer() ;

        // Return PR_TRUE if there is message available to be read.
        PRBool peekMessage(void);


        // ----------- static methods -------------------------------

        // Return the StatsServerMessage pointer. Also return the status of
        // stats (enabled/disabled) in fStatsEnabled
        static StatsServerMessage* connectStatsChannelEx(const char* name,
                                                         PRBool& fStatsEnabled);

        // Methods for clients to connect to stats channel.
        static StatsServerMessage* connectStatsChannel(const char* name);

    private:

        // Pointer to beginning of the recently received stats Message. This is
        // not allocated and hence don't try to delete it.
        char* statsMsg_;

        // Stats Message size after deducting the headers.
        int nStatsMsgSize_;

        // Single Structure for sending and receiving messages header.
        StatsMsgHeader msgHdr_;

        // Last Received message length (last packet size inclusive of all
        // headers, pad etc.).
        int nLastMsgSendLen_;

        // A buffer holder. If the messages are small, it uses the base class
        // buffer. For long messages, it switch to dynamic message.
        NSString statsBuffer_;

        // Flag to store if we need to call invalidate at destructor.
        PRBool fInvalidateAtEnd_;

        // Flag to store if there is any file descriptor error.
        PRBool fFDError_;

        // Sending a long Response message.
        PRBool sendLongStatsResponse(WDMessages messageType,
                                     const char* msgdata,
                                     int datalength);
        // Receiving a long message.
        PRBool recvLongStatsMessage(StatsMessages statsMsgType);

        // Lowest level function to send request/response or notice messages.
        PRBool sendStatsMessage(char* buffer, WDMessages messageType,
                                const void* msgdata,
                                int datalength);

        // Sending stats request.
        PRBool sendStatsReqMessage(WDMessages msg,
                                   const void* reqBuffer, int reqLen);
};


//-----------------------------------------------------------------------------
// StatsRespReceiver interface
//-----------------------------------------------------------------------------

class StatsRespReceiver
{
    public:
        virtual PRBool processRecvData(const void* recvData, int recvLen) = 0;
};


//-----------------------------------------------------------------------------
// StatsRespFdReceiver
//
// This class implements StatsRespReceiver interface. The received data is
// written to a file descriptor.
//-----------------------------------------------------------------------------

class StatsRespFdReceiver : public StatsRespReceiver
{
    private:
        PRFileDesc* fd_;
    public:
        StatsRespFdReceiver(PRFileDesc* fd);
        virtual PRBool processRecvData(const void* recvData, int recvLen);
};


//-----------------------------------------------------------------------------
// StatsRespStrReceiver
//
// This class implements StatsRespReceiver interface. The received data is
// appeneded to a string.
//-----------------------------------------------------------------------------

class StatsRespStrReceiver : public StatsRespReceiver
{
    private:
        NSString& strResp_;
    public:
        StatsRespStrReceiver(NSString& strResp);
        virtual PRBool processRecvData(const void* recvData, int recvLen);
};


//-----------------------------------------------------------------------------
// StatsMessenger
//
// A wrapper class for StatsServerMessage. Send message and collects response.
// It also prepares bufferReader which could be used by caller directly.
//-----------------------------------------------------------------------------

class StatsMessenger
{
    private:
        // Pointer to message channel
        StatsServerMessage* statsMsgChannel_;

        // Buffer reader.
        StatsBufferReader bufferReader_;
    public:
        // constructor
        StatsMessenger(StatsServerMessage* statsMsgChannel);

        // send message and receive response. If msgStringArg is non NULL then
        // it is used as a message request argument.
        PRBool sendMsgAndRecvResp(StatsMessages msgType,
                                  const char* msgStringArg = NULL);

        // send message and receive response. buffer of size bufferLen is used
        // as an argument to message request.
        PRBool sendMsgAndRecvResp(StatsMessages msgType,
                                  const void* buffer, int bufferLen);

        // Return the buffer reader.
        StatsBufferReader& getBufferReader(void);

        // copy the message channel received buffer in bufferReader
        void getBufferReaderCopy(StatsBufferReader& bufferReader) const;

        // create a new StringArrayBuff from message channel's buffer reader.
        void getStringArrayBuffer(StringArrayBuff& strMsgBuff) const;

        // send message and process response. The response is sent in multiple
        // messages. Caller keeps asking for stats response until it receive a
        // zero byte response from peer.
        PRBool sendMsgAndProcessResp(StatsMessages msgType,
                                     StatsMessages msgTypeAck,
                                     StatsRespReceiver& receiver,
                                     const char* msgStringArg);
};


//-----------------------------------------------------------------------------
// StatsServerMessage inline methods
//-----------------------------------------------------------------------------


inline
const char*
StatsServerMessage::getStatsMessage(void) const
{
    return statsMsg_;
}

inline
int
StatsServerMessage::getStatsMessageLen(void) const
{
    return nStatsMsgSize_;
}

inline
StatsMessages
StatsServerMessage::getStatsMsgType(void) const
{
    return (StatsMessages) msgHdr_.msgType;
}

inline
int
StatsServerMessage::getStatsSenderPid(void) const
{
    return msgHdr_.senderPid;
}

inline
PRBool
StatsServerMessage::isFileDescriptorError(void) const
{
    return fFDError_;
}


//-----------------------------------------------------------------------------
// StatsRespFdReceiver inline methods
//-----------------------------------------------------------------------------

inline
StatsRespFdReceiver::StatsRespFdReceiver(PRFileDesc* fd) :
                                         fd_(fd)
{
}


//-----------------------------------------------------------------------------
// StatsRespStrReceiver inline methods
//-----------------------------------------------------------------------------

inline
StatsRespStrReceiver::StatsRespStrReceiver(NSString& strResp) :
                                           strResp_(strResp)
{
}


//-----------------------------------------------------------------------------
// StatsMessenger inline methods
//-----------------------------------------------------------------------------

inline
StatsMessenger::StatsMessenger(StatsServerMessage* statsMsgChannel) :
                           statsMsgChannel_(statsMsgChannel),
                           bufferReader_(NULL, 0)
{
}

inline
StatsBufferReader&
StatsMessenger::getBufferReader(void)
{
    return bufferReader_;
}

inline
void
StatsMessenger::getStringArrayBuffer(StringArrayBuff& strMsgBuff) const
{
    const char* recvBuffer = statsMsgChannel_->getStatsMessage();
    int recvLen = statsMsgChannel_->getStatsMessageLen();
    strMsgBuff.reinitialize(recvBuffer, recvLen);
}

inline
void
StatsMessenger::getBufferReaderCopy(StatsBufferReader& bufferReader) const
{
    const char* recvBuffer = statsMsgChannel_->getStatsMessage();
    int recvLen = statsMsgChannel_->getStatsMessageLen();
    bufferReader.initialize(recvBuffer, recvLen, PR_TRUE);   // Make a copy
}

#endif
