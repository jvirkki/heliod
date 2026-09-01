#ifndef _NTSERVERMESSAGE_H_
#define _NTSERVERMESSAGE_H_

#include "base/wdservermessage.h" // WDMessages
#include "httpdaemon/libdaemon.h" // HTTPDAEMON_DLL

class StatsServerMessage;

//-----------------------------------------------------------------------------
// NTServerMessage
//
// This is a windows specific class whose functionality is similar to
// wdServerMessage.
//-----------------------------------------------------------------------------

class HTTPDAEMON_DLL NTServerMessage {

    public:
        //  constructor; requires a valid fd as returned from a listener
        NTServerMessage(HANDLE osFd);

        // Destructor
        virtual ~NTServerMessage(void);

        // get Last received message type.
        WDMessages    getLastMsgType(void)    { return msgType;    }

        // get number of bytes receive in last message
        int getRecvBytes(void) { return msgbytesRead; }

        // Test to see if the message is a stats message. It is a Windows
        // implementation of wdServerMessage and used by derive classes. On
        // Windows we must never receive other message.
        int isStatsMessage(void);

        virtual char* RecvFromServer(void) ;

        // Returns the underlying handle to the socket
        HANDLE getFD(void) const { return msgFd_; }

        // Sets msgFd_ to an invalid value so that the descriptor will NOT
        // be closed in the destructor
        void invalidate(void);

        // set the overlapped io structure pointer
        void setOverlappedIO(LPOVERLAPPED lpOverlapped);

        // Receive data asynchronously
        PRBool asyncRecvFromServer(void);

        // Check to see if the Receive data is pending.
        PRBool isPendingRead(void);

        // Check to see if message is ready to be read.
        PRBool peekMessage(void);

    private:
        //  os fd
        HANDLE       msgFd_;

        //  number of bytes read
        DWORD        msgbytesRead;

        //  recvmsg buffer
        char         msgbuff[WDMSGBUFFSIZE];

        //  Type from last message received
        WDMessages   msgType;

        // Overlapped structure pointer used for overlapped io.
        LPOVERLAPPED lpOverlapped_;

        // Flag to store whether data has been already read.
        BOOL         fCacheData; 

        // Flag to store if io is pending. If so then GetOverlappedResult need
        // to be called during next call to RecvFromServer
        BOOL         fPendingRead_;

        // Message contains the 32 bit header and then data. It returns the
        // message pointer from the buffer.
        char* parseMessage(char* buffer);

        static StatsServerMessage* connectStatsChannel(const char* name);

    protected:

        // write message pointed by buf
        int writeMsg(const void* buf, int writeLen, int* sentLen);

        // Get Received message buffer.
        char* getMessageBuffer(void) { return msgbuff; }
};

// On Unix NTServerMessage functionality is implemented by wdServerMessage
// This typedef is to use to make both classes similar to be used by derive
// classes.
typedef NTServerMessage StatsServerMessageBase;

// Platform indenpendent name for file handle.
// Is there already exist in webserver code ??
typedef HANDLE StatsFileHandle;

inline
PRBool
NTServerMessage::isPendingRead(void)
{
    return fPendingRead_;
}

#endif

