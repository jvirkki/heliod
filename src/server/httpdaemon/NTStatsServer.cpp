#include <nspr.h>
#include "private/pprio.h"              // PR_ImportFile
#include "base/ereport.h"               // ereport()
#include "httpdaemon/NTStatsServer.h"
#include "frame/conf.h"                 // conf_get_true_globals()
#include "httpdaemon/WebServer.h"       // WebServer class
#include "httpdaemon/dbthttpdaemon.h"   // DBT_xxx
#include "httpdaemon/statsmessage.h"
#include "httpdaemon/statsmanager.h"
#include "base/util.h"                  // util_sprintf()
#include "frame/conf.h"                 // conf_get_true_globals

static const int NBUF_SIZE = 64 * 1024;

////////////////////////////////////////////////////////////////

// StatsFileDesc Class members

////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// StatsFileDesc::StatsFileDesc
//-----------------------------------------------------------------------------

StatsFileDesc::StatsFileDesc(void)
{
    fd_ = INVALID_HANDLE_VALUE;
    state_ = STAT_HANDLE_INVALID;
    lpoverlapped_ = 0;
    msgChannel_ = 0;
}

//-----------------------------------------------------------------------------
// StatsFileDesc::~StatsFileDesc
//-----------------------------------------------------------------------------

StatsFileDesc::~StatsFileDesc(void)
{
    fd_ = INVALID_HANDLE_VALUE;
    state_ = STAT_HANDLE_INVALID;
    if (lpoverlapped_)
        delete lpoverlapped_;
    if (msgChannel_)
    {
        msgChannel_->invalidate();
        delete msgChannel_;
    }
    lpoverlapped_ = 0;
    msgChannel_ = 0;
}





////////////////////////////////////////////////////////////////

// NTStatsServer Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// NTStatsServer::NTStatsServer
//-----------------------------------------------------------------------------

NTStatsServer::NTStatsServer(void) :
                Thread("NT Stats Thread"),
                statsMsgHandler_(*this) // Reference to StatsUpdateHandler
{
    maxPollItems_ = 10;         // Arbitrary number
    nPollItems_ = 0;
    pollEvents_ = (HANDLE*) PR_CALLOC(maxPollItems_* sizeof(HANDLE));
    if (pollEvents_ != NULL)
    {
        int nIndex = 0;
        for (nIndex = 0; nIndex < maxPollItems_; ++nIndex)
        {
            pollEvents_[nIndex] = INVALID_HANDLE_VALUE;
        }
    }
    fdArray_ = new StatsFileDesc[maxPollItems_];
}

//-----------------------------------------------------------------------------
// NTStatsServer::~NTStatsServer
//-----------------------------------------------------------------------------

NTStatsServer::~NTStatsServer(void)
{
    closeHandles();
}

//-----------------------------------------------------------------------------
// NTStatsServer::createPipe
//-----------------------------------------------------------------------------

HANDLE NTStatsServer::createPipe(void)
{
    const DWORD pipeMode = (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
                            PIPE_WAIT);
    char statsChannelName[1024];
    memset(statsChannelName, 0, sizeof(statsChannelName));
    buildConnectionName(conf_get_true_globals()->Vserver_id,
                        statsChannelName, (sizeof(statsChannelName) -1));
    HANDLE hPipe = CreateNamedPipe(statsChannelName,
                                   PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                   pipeMode,
                                   PIPE_UNLIMITED_INSTANCES,
                                   NBUF_SIZE, NBUF_SIZE,
                                   PIPE_WAIT, NULL);
    return hPipe;
}

//-----------------------------------------------------------------------------
// NTStatsServer::resetHandles
//-----------------------------------------------------------------------------

PRBool NTStatsServer::resetHandles(int nIndex)
{
    if (nIndex >= maxPollItems_)
        return PR_FALSE;
    pollEvents_[nIndex] = INVALID_HANDLE_VALUE;
    StatsFileDesc* statFileDesc = &fdArray_[nIndex];
    statFileDesc->fd_ = INVALID_HANDLE_VALUE;
    statFileDesc->state_ = STAT_HANDLE_INVALID;
    if (statFileDesc->lpoverlapped_)
    {
        delete statFileDesc->lpoverlapped_;
        statFileDesc->lpoverlapped_ = 0;
    }
    if (statFileDesc->msgChannel_)
    {
        statFileDesc->msgChannel_->invalidate();
        delete statFileDesc->msgChannel_;
        statFileDesc->msgChannel_ = 0;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::connectPipe
//-----------------------------------------------------------------------------

PRBool NTStatsServer::connectPipe(int nIndex)
{
    if (nIndex >= nPollItems_)
        return PR_FALSE;
    HANDLE hPipe = fdArray_[nIndex].fd_;
    LPOVERLAPPED lpOverlapped = fdArray_[nIndex].lpoverlapped_;
    HANDLE hEvent = pollEvents_[nIndex];
    if (ConnectNamedPipe(hPipe, lpOverlapped))
    {
        return PR_FALSE;
    }
    DWORD dwError = GetLastError();
    if (dwError == ERROR_IO_PENDING)
    {
        fdArray_[nIndex].state_ = STAT_HANDLE_WAIT_CONNECTION;
        return PR_TRUE;
    }
    else if (dwError == ERROR_PIPE_CONNECTED)
    {
        fdArray_[nIndex].state_ = STAT_HANDLE_CONNECTED;
        SetEvent(hEvent);       // Signal the event manually so that
        // connection is accepted in next call
        return PR_TRUE;
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::createStatsChannel
//-----------------------------------------------------------------------------

PRBool NTStatsServer::createStatsChannel(void)
{
    HANDLE hPipe = createPipe();
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        return PR_FALSE;
    }
    HANDLE hEvent = CreateEvent(NULL,   // Security descriptor
                                TRUE,   // Manual Rest
                                TRUE,   // Initially Signaled state
                                NULL);  // Unnamed
    if (hEvent == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hPipe);
        return PR_FALSE;
    }
    if (addPollItem(hPipe, hEvent) != PR_TRUE)
    {
        CloseHandle(hPipe);
        CloseHandle(hEvent);
        return PR_FALSE;
    }
    int nIndex = nPollItems_ - 1;
    if (connectPipe(nIndex) == PR_FALSE)
    {
        return PR_FALSE;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::init
//-----------------------------------------------------------------------------

PRBool NTStatsServer::init(void)
{
    PRBool retval = PR_FALSE;
    //Sleep(15000);
    if ((retval = createStatsChannel()) != PR_TRUE)
    {
        PR_ASSERT(0);
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_NTStatsServer_ErrorChannelCreation));
        return retval;
    }
    // We need to create a global bound thread as this thread
    // doesn't wait on PR APIs. It waits on MsgWaitForMultipleObjects
    // By default, it seems PR_CreateThread creates a fibre which
    // hangs the StatsPollThread.
    if (start(PR_SYSTEM_THREAD, PR_UNJOINABLE_THREAD,
              0, PR_PRIORITY_NORMAL, PR_GLOBAL_BOUND_THREAD) == PR_FAILURE)
    {
        PRInt32 err = PR_GetError();
        PRInt32 oserr = PR_GetOSError();
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_StatsManager_FailedToCreateThread),
                system_errmsg());
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::addPollItem
//-----------------------------------------------------------------------------

PRBool NTStatsServer::addPollItem(HANDLE hFileDesc,
                                   HANDLE hEvent)
{
    PRBool status = PR_FALSE;
    if (pollEvents_ != NULL)
    {
        if (nPollItems_ < maxPollItems_)
        {
            // Add the item at the end of the poll array
            status = PR_TRUE;
            pollEvents_[nPollItems_] = hEvent;
            fdArray_[nPollItems_].fd_ = hFileDesc;
            if (fdArray_[nPollItems_].lpoverlapped_ == NULL)
            {
                LPOVERLAPPED lpOverlapped = new OVERLAPPED;
                memset(lpOverlapped, 0, sizeof(OVERLAPPED));
                lpOverlapped->hEvent = INVALID_HANDLE_VALUE;
                fdArray_[nPollItems_].lpoverlapped_ = lpOverlapped;
            }
            if (fdArray_[nPollItems_].msgChannel_ == NULL)
            {
                StatsServerMessage* msgChannel =
                    new StatsServerMessage(hFileDesc);
                fdArray_[nPollItems_].msgChannel_ = msgChannel;
                msgChannel->setOverlappedIO(fdArray_[nPollItems_].
                                            lpoverlapped_);
            }
            fdArray_[nPollItems_].lpoverlapped_->hEvent = hEvent;
            fdArray_[nPollItems_].state_ = STAT_HANDLE_WAIT_CONNECTION;
            nPollItems_++;
        }
    }
    return status;
}

//-----------------------------------------------------------------------------
// NTStatsServer::closeHandles
//-----------------------------------------------------------------------------

void NTStatsServer::closeHandles(void)
{
    if (pollEvents_ != NULL)
    {
        for (PRInt32 i = 0; i < nPollItems_; i++)
        {
            if (pollEvents_ && (pollEvents_[i] != INVALID_HANDLE_VALUE))
            {
                CloseHandle(pollEvents_[i]);
                pollEvents_[i] = INVALID_HANDLE_VALUE;
            }
        }
    }
    return;
}

//-----------------------------------------------------------------------------
// NTStatsServer::invalidatePollItem
//-----------------------------------------------------------------------------

PRBool NTStatsServer::invalidatePollItem(int nIndex)
{
    PRBool status = PR_FALSE;
    if (pollEvents_ != NULL)
    {
        CloseHandle(pollEvents_[nIndex]);
        DisconnectNamedPipe(fdArray_[nIndex].fd_);
        CloseHandle(fdArray_[nIndex].fd_);
        resetHandles(nIndex);
        status = PR_TRUE;
    }
    return status;
}

//-----------------------------------------------------------------------------
// NTStatsServer::run
//-----------------------------------------------------------------------------

void NTStatsServer::run(void)
{
    int timeout = 1000;          // milliseconds
    while (this->wasTerminated() == PR_FALSE)
    {
        BOOL fWaitAny = FALSE; // Wait any of the poll items.
        DWORD dwWaitReturnVal = MsgWaitForMultipleObjects(nPollItems_,
                                                          pollEvents_,
                                                          fWaitAny,
                                                          timeout,
                                                          QS_ALLEVENTS);
        if (dwWaitReturnVal == WAIT_TIMEOUT)
        {
            // Timeout.
            updateTimeoutStats();
            continue;
        }
        int index = dwWaitReturnVal - WAIT_OBJECT_0;
        if (index == nPollItems_)
        {
            // Special case of return value
            // do nothing and just continue
            MSG msg;
            PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
            continue;
        }
        if ((index < 0) || (index > nPollItems_))
        {
            PR_ASSERT(0);
            return;
        }

        processIncomingMessage(index);
        removeInvalidItems();
        PR_ASSERT(nPollItems_ >= 0);
        if (nPollItems_ == 0)
        {
            // Create a new stats channel.
            if (createStatsChannel() != PR_TRUE)
            {
                ereport(LOG_FAILURE,
                        XP_GetAdminStr(DBT_NTStatsServer_ErrorChannelCreation));
                return;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// NTStatsServer::acceptStatsConnection
//-----------------------------------------------------------------------------

void NTStatsServer::acceptStatsConnection(int nIndex)
{
    DWORD dwRet = 0;
    BOOL fSuccess = GetOverlappedResult(fdArray_[nIndex].fd_,
                                        fdArray_[nIndex].lpoverlapped_,
                                        &dwRet,
                                        FALSE);   // blocking
    if (!fSuccess)
    {
        // Problem with the connection
        processError(nIndex);   // Close the handles
        return;
    }
    else
    {
        fdArray_[nIndex].state_ = STAT_HANDLE_WAIT_READING;
    }
    // Create a new channel. Current channel will now in reading/writing state
    PRBool fRetVal = createStatsChannel();
    if (fRetVal != PR_TRUE)
    {
        PR_ASSERT(0);
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_NTStatsServer_ErrorChannelCreation));
    }
    return;
}

//-----------------------------------------------------------------------------
// NTStatsServer::startAsyncReadOperation
//-----------------------------------------------------------------------------

void NTStatsServer::startAsyncReadOperation(int nIndex)
{
    StatsFileDesc* statFileDesc = &fdArray_[nIndex];
    if (statFileDesc->state_ != STAT_HANDLE_WAIT_READING)
        return;
    StatsServerMessage* msgChannel = statFileDesc->msgChannel_;
    PRBool fSuccess = msgChannel->asyncRecvFromServer();
    if (fSuccess == PR_TRUE)
    {
        processStatsMessage(nIndex);
        return;
    }
    if (msgChannel->isPendingRead() == PR_FALSE)
    {
        // Error Happened in IO
        processError(nIndex);
    }
    return;
}

//-----------------------------------------------------------------------------
// NTStatsServer::processIncomingMessage
//-----------------------------------------------------------------------------

void NTStatsServer::processIncomingMessage(int nIndex)
{
    StatsHandleState& state = fdArray_[nIndex].state_;
    if (state == STAT_HANDLE_WAIT_CONNECTION)
    {
        // Connection request Arrived
        acceptStatsConnection(nIndex);
        startAsyncReadOperation(nIndex);
    }
    else if (state == STAT_HANDLE_CONNECTED)
    {
        PRBool fRetVal = createStatsChannel();
        if (fRetVal != PR_TRUE)
        {
            PR_ASSERT(0);
            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_NTStatsServer_ErrorChannelCreation));
            return;
        }
        state = STAT_HANDLE_WAIT_READING;
        startAsyncReadOperation(nIndex);
    }
    else if (state == STAT_HANDLE_WAIT_READING)
    {
        processStatsMessage(nIndex);
    }
    else
    {
        // Unexpected state :
        PR_ASSERT(0);
    }
}

//-----------------------------------------------------------------------------
// NTStatsServer::removeInvalidItems
//-----------------------------------------------------------------------------

PRBool NTStatsServer::removeInvalidItems(void)
{
    if (pollEvents_ != NULL)
    {
        PRInt32 nRemoved = 0;
        for (PRInt32 i = nPollItems_ - 1; i >= 0; i--)
        {
            if (pollEvents_[i] == INVALID_HANDLE_VALUE)
            {
                // Move everything else to fill the space created by the
                // one entry that was removed
                for (PRInt32 j = i + 1; j < nPollItems_; j++)
                {
                    pollEvents_[j - 1] = pollEvents_[j];
                    // swap the lpoverlapped from j-1 to j.
                    // It is important. We don't want to copy
                    // the overlapped pointer.
                    LPOVERLAPPED lpoverlapped = fdArray_[j - 1].lpoverlapped_;
                    // Also swap the msgChannel_
                    StatsServerMessage* msgChannel =
                        fdArray_[j - 1].msgChannel_;
                    memcpy(&fdArray_[j - 1], &fdArray_[j],
                           sizeof(StatsFileDesc));
                    fdArray_[j].lpoverlapped_ = lpoverlapped;
                    fdArray_[j].msgChannel_ = msgChannel;
                    resetHandles(j);
                }
                nRemoved++;
            }
        }
        nPollItems_ -= nRemoved;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateTimeoutStats
//-----------------------------------------------------------------------------

void NTStatsServer::updateTimeoutStats(void)
{
    // Right now do nothing
}

//-----------------------------------------------------------------------------
// NTStatsServer::processStatsMessage
//-----------------------------------------------------------------------------

void NTStatsServer::processStatsMessage(int nIndex)
{
    // Handle Stats Message here
    StatsServerMessage* msgChannel = fdArray_[nIndex].msgChannel_;
    if (!msgChannel)
        return;
    if (statsMsgHandler_.processStatsMessage(*msgChannel) != PR_TRUE)
    {
        processError(nIndex);
    }
    else
    {
        // Start a new Async Operation now
        startAsyncReadOperation(nIndex);
    }
    return;
}

//-----------------------------------------------------------------------------
// NTStatsServer::processError
//-----------------------------------------------------------------------------

void NTStatsServer::processError(int nIndex)
{
    statsMsgHandler_.processCloseFd(fdArray_[nIndex].fd_);
    invalidatePollItem(nIndex);
}

//-----------------------------------------------------------------------------
// NTStatsServer::terminate
//-----------------------------------------------------------------------------

PRBool NTStatsServer::terminate(void)
{
    // Is terminate really needed ??
    setTerminatingFlag();
    while (isRunning())
    {
        PR_Sleep(PR_MillisecondsToInterval(1000));
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatHandler::isReadyForUpdate
//-----------------------------------------------------------------------------

PRBool NTStatsServer::isReadyForUpdate(void)
{
    // Windows always returns true.
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateProcessSlotForPid
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateProcessSlotForPid(int pid)
{
    // In unix the accumulation is done by child process during call of
    // getProcessInfo. Windows don't get any chance to do accumulation. Though
    // this logic could be moved to StatsManager::poll but it is unnecessary.
    // This accumulated data is used only outside process which don't want to
    // pull all thread data and do accumulation.
    StatsProcessNode* procNode = StatsManager::findProcessSlot();
    if (procNode)
        procNode->accumulateThreadData();
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateVSCoreData
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateVSCoreData(const char* vsId)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateAccumulatedVSData
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateAccumulatedVSData(void)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateVSJavaData
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateVSJavaData(void)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::initWebModuleList
//-----------------------------------------------------------------------------

PRBool NTStatsServer::initWebModuleList(int pid)
{
    // Nothing to be done for windows
    return PR_TRUE;
}


//-----------------------------------------------------------------------------
// NTStatsServer::isAllChildWebModInitDone
//-----------------------------------------------------------------------------

PRBool NTStatsServer::isAllChildWebModInitDone(void)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateOnReconfig
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateOnReconfig(int pid)
{
    statsMsgHandler_.sendNotification(statsNoticeReconfigureDone);
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateWebModuleData
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateWebModuleData(const char* webModuleName)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateServletData
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateServletData(const char* webModuleName)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateProcessSlots
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateProcessSlots(void)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateThreadSlots
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateThreadSlots(void)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateJdbcConnPools
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateJdbcConnPools(int pid)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateProcessJavaData
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateProcessJavaData(void)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateJvmMgmtData
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateJvmMgmtData(int pid)
{
    // ignore pid as there is single child process in windows.
    StatsManager::updateJvmMgmtStats();
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer::updateSessionReplicationData
//-----------------------------------------------------------------------------

PRBool NTStatsServer::updateSessionReplicationData(void)
{
    // Nothing to be done for windows
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// NTStatsServer static methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// NTStatsServer::buildConnectionName
//-----------------------------------------------------------------------------

int NTStatsServer::buildConnectionName(const char* instanceName,
                                       char* outputPath,
                                       int sizeOutputPath)
{
    if (instanceName == NULL)
    {
        instanceName = conf_get_true_globals()->Vserver_id;
    }
    return util_snprintf(outputPath, sizeOutputPath,
                         "\\\\.\\pipe\\iWSStats-%s",
                         instanceName);
}
