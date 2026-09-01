#ifndef _NTSTATHANDLER_H
#define _NTSTATHANDLER_H

#ifdef XP_WIN32

#include "nspr.h"                        // NSPR API's
#include "NsprWrap/Thread.h"             // Thread
#include "httpdaemon/StatsMsgHandler.h"  // StatsMsgHandler

typedef enum
{
    STAT_HANDLE_INVALID,
    STAT_HANDLE_WAIT_CONNECTION,
    STAT_HANDLE_CONNECTED,
    STAT_HANDLE_WAIT_READING
}StatsHandleState;

class StatsFileDesc
{
    public:
        StatsFileDesc(void);
        ~StatsFileDesc(void);

        HANDLE fd_;
        LPOVERLAPPED lpoverlapped_;
        StatsHandleState state_;
        StatsServerMessage* msgChannel_;
};

//-----------------------------------------------------------------------------
// NTStatsServer
//
// Server Class to serve stats requests on windows. It runs in it's own thread.
//-----------------------------------------------------------------------------

class NTStatsServer  : public Thread, public StatsUpdateHandler
{
    public :
        NTStatsServer(void) ;
        ~NTStatsServer(void);

        // Initialization. Creates the stats channel, creates the thread.
        PRBool init(void);

        // Terminate the thread. Unused so far.
        PRBool terminate(void);

        // Overloaded method from base Class Thread.
        void run(void);

        // Similar functionality as ParentAdmin::buildConnectionPath.  It sets
        // the connectionName in outputPath from instanceName. It returns the
        // number of bytes written.
        static int buildConnectionName(const char* instanceName,
                                       char* outputPath,
                                       int sizeOutputPath);

    private :
        // Stats message handler
        StatsMsgHandler statsMsgHandler_;

        // Array of poll events MSGWaitForMultipleObjects.
        HANDLE* pollEvents_;

        // Array of StatsFileDescriptor.
        StatsFileDesc* fdArray_;

        // Size (maximum number of items) of pollArray_.
        PRInt32 maxPollItems_;

        // Number of Current Poll Items.
        PRInt32 nPollItems_;


        // Creates a stats channel.
        PRBool createStatsChannel(void);

        // Accepts the incoming connection request.
        void acceptStatsConnection(int nIndex);

        // process the stats message request.
        void processStatsMessage(int nIndex);

        // process incoming request.
        void processIncomingMessage(int nIndex);

        // Invalidate poll Item.
        PRBool invalidatePollItem(int nIndex);

        // Add poll Item.
        PRBool addPollItem(HANDLE hFileDesc, HANDLE hEvent);

        // Process error for file descriptore for nIndex
        void processError(int nIndex);

        // Remove the invalid items.
        PRBool removeInvalidItems(void);

        // Close handles in pollEvents_ array.
        void closeHandles(void);

        // Free up the handles in pollEvents_ and fdArray_
        PRBool resetHandles(int nIndex);

        // Create named pipe and returns it's handle
        HANDLE createPipe(void);

        // Do a overlapped connectPipe
        PRBool connectPipe(int nIndex);

        // On Poll timeout, this function handles any timeout maintainence
        // events. Unused right now.
        void updateTimeoutStats(void);

        // Start Asynchronous read operation nth File descriptor
        void startAsyncReadOperation(int nIndex);

        // ------------- StatsUpdateHandler methods implementation --------

        virtual PRBool isReadyForUpdate(void);
        virtual PRBool updateProcessSlotForPid(int pid);
        virtual PRBool updateVSCoreData(const char* vsId = 0);
        virtual PRBool updateAccumulatedVSData(void);
        virtual PRBool updateVSJavaData(void);
        virtual PRBool initWebModuleList(int pid);
        virtual PRBool isAllChildWebModInitDone(void);
        virtual PRBool updateOnReconfig(int pid);
        virtual PRBool updateWebModuleData(const char* webModuleName = NULL);
        virtual PRBool updateServletData(const char* webModuleName = NULL);
        virtual PRBool updateThreadSlots(void);
        virtual PRBool updateProcessSlots(void);
        virtual PRBool updateJdbcConnPools(int pid);
        virtual PRBool updateJvmMgmtData(int pid);
        virtual PRBool updateSessionReplicationData(void);
        virtual PRBool updateProcessJavaData(void);
};

#endif

#endif

