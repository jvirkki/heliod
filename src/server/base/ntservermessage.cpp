#include <windows.h>
#include "base/ntservermessage.h"
#include "httpdaemon/statsmessage.h"  // StatsServerMessage

//-----------------------------------------------------------------------------
// NTServerMessage::NTServerMessage
//-----------------------------------------------------------------------------

NTServerMessage::NTServerMessage(HANDLE osFd):
                                  msgFd_(osFd)
{
    lpOverlapped_ = NULL;
    fCacheData = PR_FALSE;
    fPendingRead_ = PR_FALSE;
}

//-----------------------------------------------------------------------------
// NTServerMessage::~NTServerMessage
//-----------------------------------------------------------------------------

NTServerMessage::~NTServerMessage(void)
{
    if (msgFd_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(msgFd_);
    }
}

//-----------------------------------------------------------------------------
// NTServerMessage::invalidate
//-----------------------------------------------------------------------------

void NTServerMessage::invalidate(void)
{
    msgFd_ = INVALID_HANDLE_VALUE;
}

//-----------------------------------------------------------------------------
// NTServerMessage::setOverlappedIO
//-----------------------------------------------------------------------------

void NTServerMessage::setOverlappedIO(LPOVERLAPPED lpOverlapped)
{
    lpOverlapped_ = lpOverlapped;
}

//-----------------------------------------------------------------------------
// NTServerMessage::writeMsg
//-----------------------------------------------------------------------------

int NTServerMessage::writeMsg(const void* buf,
                              int writeLen,
                              int* sentLen)
{
    DWORD dwBytesWritten;
    if (WriteFile(msgFd_, buf, writeLen, &dwBytesWritten, NULL) == TRUE)
    {
        *sentLen = (int) dwBytesWritten;
        return 1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// NTServerMessage::parseMessage
//-----------------------------------------------------------------------------

char* NTServerMessage::parseMessage(char* buffer)
{
    if (msgbytesRead < sizeof(WDMessages))
        return 0;
    msgType = *(WDMessages*) buffer;
    return (buffer + sizeof(WDMessages));
}

//-----------------------------------------------------------------------------
// NTServerMessage::RecvFromServer
//-----------------------------------------------------------------------------

char* NTServerMessage::RecvFromServer()
{
    if (fCacheData == PR_TRUE)
    {
        fCacheData = PR_FALSE;
        return parseMessage(msgbuff);
    }
    if (fPendingRead_ == PR_TRUE)
    {
        fPendingRead_ = PR_FALSE;
        if (lpOverlapped_ == NULL)
            return 0;           // Error
        DWORD dwBytesRead = 0;
        BOOL fRetVal = GetOverlappedResult(msgFd_,
                                           lpOverlapped_,
                                           &dwBytesRead,
                                           TRUE); // blocking
        if (!fRetVal)
            return 0;           // Error
        if (msgbytesRead == 0)  // ReadFile might have returned 0
            msgbytesRead = dwBytesRead;
        return parseMessage(msgbuff);
    }
    char* buffer = msgbuff;
    int bufferLen = sizeof(msgbuff);
    DWORD dwBytesRead = 0;

    // Blocking Call
    if (ReadFile(msgFd_, buffer, bufferLen, &msgbytesRead, NULL) == TRUE)
    {
        return parseMessage(buffer);
    }
    return 0;
}

//-----------------------------------------------------------------------------
// NTServerMessage::asyncRecvFromServer
//
// Return PR_TRUE if data was read successfully
//-----------------------------------------------------------------------------

PRBool NTServerMessage::asyncRecvFromServer(void)
{
    char* buffer = msgbuff;
    int bufferLen = sizeof(msgbuff);
    fCacheData = PR_FALSE;
    fPendingRead_ = PR_FALSE;
    BOOL fSuccess = ReadFile(msgFd_, buffer, bufferLen,
                             &msgbytesRead, lpOverlapped_);
    if ((fSuccess == TRUE) && (msgbytesRead != 0))
    {
        fCacheData = PR_TRUE;
        return PR_TRUE;
    }
    else
    {
        if (lpOverlapped_ && (GetLastError() == ERROR_IO_PENDING))
        {
            fPendingRead_ = PR_TRUE;
            return PR_FALSE;
        }
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// NTServerMessage::isStatsMessage
//-----------------------------------------------------------------------------

int NTServerMessage::isStatsMessage(void)
{
    if ((getLastMsgType() >= wdmsgStatsMessageBegin) &&
        (getLastMsgType() <= wdmsgStatsMessageLast))
    {
        return 1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// NTServerMessage::peekMessage
//
// This function returns PR_TRUE if there is a message to be read from pipe.
// Also if pipe connection is broken then too, it returns PR_TRUE so that
// caller will try to read the message and find out if connection is broken.
//-----------------------------------------------------------------------------

PRBool NTServerMessage::peekMessage(void)
{
    DWORD dwBytesAvail = 0;
    if (PeekNamedPipe(msgFd_,
                      NULL, // lpBuffer
                      0,    // nBufferSize
                      NULL, // lpBytesRead
                      &dwBytesAvail, // lpTotalBytesAvail
                      NULL  // lpBytesLeftThisMessage
                      ) == TRUE)
    {
        if (dwBytesAvail > 0)
            return PR_TRUE;
    }
    else
    {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_BROKEN_PIPE)
        {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}


//-----------------------------------------------------------------------------
// NTServerMessage::connectStatsChannel
//-----------------------------------------------------------------------------

StatsServerMessage* NTServerMessage::connectStatsChannel(const char* name)
{
    HANDLE hPipe;
    int nIndex = 0;
    int nTrialCount = 5;
    for (nIndex = 0; nIndex < nTrialCount; ++nIndex)
    {
        DWORD dwAccess = GENERIC_READ | GENERIC_WRITE;
        DWORD dwShared = FILE_SHARE_READ | FILE_SHARE_WRITE;
        DWORD dwFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;
        hPipe = CreateFile(name,
                           dwAccess,
                           dwShared,
                           NULL,        // Security Attribute
                           OPEN_EXISTING,
                           dwFlags, // File Attribute
                           NULL);
        if (hPipe != INVALID_HANDLE_VALUE)
            break;              // Got Connected successfully
        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            // Unable to connect.
            return 0;
        }
        int waitTime = 2000;    // 2 seconds
        if (!WaitNamedPipe(name, waitTime))
        {
            // Instance of pipe is not available
            return 0;
        }
        // continue
    }
    if (hPipe == INVALID_HANDLE_VALUE)
        return 0;
    DWORD dwPipeMode = PIPE_READMODE_MESSAGE;
    BOOL fSuccess = SetNamedPipeHandleState(hPipe,
                                            &dwPipeMode,
                                            NULL, // lpMaxCollectionCount
                                            NULL);// lpCollectDataTimeout
    if (!fSuccess)
    {
        // Unable to setup pipe message option
        CloseHandle(hPipe);
        return 0;
    }
    StatsServerMessage* statsMsgChannel = 0;
    statsMsgChannel = new StatsServerMessage(hPipe);
    return statsMsgChannel;
}
