#ifndef _STATMSGHANDLER_H
#define _STATMSGHANDLER_H

#include "nspr.h"                      // NSPR API's
#include "httpdaemon/statsmessage.h"   // StatsServerMessage
#include "NsprWrap/NsprDescriptor.h"   // NsprDescriptor

//-----------------------------------------------------------------------------
// StatsResponseChannel
//
// This is a wrapper class which wraps the data send on PRFileDesc using stats
// channel.
//-----------------------------------------------------------------------------

class StatsResponseChannel : public NsprDescriptor
{
    private:
        // Stats message channel
        StatsServerMessage& msgChannel_;

        // Store if error happened while communicating with message channel.
        PRBool fError_;


        // Number of bytes to cache
        int nCacheSize_;

        // Cache buffer
        StatsMsgBuff cacheBuffer_;

        // write message
        PRInt32 writeMessage(const char* buf, PRInt32 amount);

    public:
        StatsResponseChannel(StatsServerMessage& msgChannel,
                             int nCacheSize = 0);
        ~StatsResponseChannel(void);

        // Flush the cache data stored in cache buffer.
        void flushCache(void);

        // send a empty message to tell that we are done with sending data.
        void sendEndOfDataMarker(void);

        // Overloaded methods from NsprDescriptor
        PRInt32 write(const void* buf, PRInt32 amount);
};


//-----------------------------------------------------------------------------
// StatsUpdateHandler
//
// This interface updates the stats data.
//-----------------------------------------------------------------------------

class StatsUpdateHandler
{
    public:
        // List of pure virtual function.
        virtual PRBool isReadyForUpdate(void) = 0;

        virtual PRBool initWebModuleList(int pid) = 0;
        virtual PRBool isAllChildWebModInitDone(void) = 0;
        virtual PRBool updateOnReconfig(int pid) = 0;
        virtual PRBool updateProcessSlotForPid(int pid) = 0;
        virtual PRBool updateVSCoreData(const char* vsId = 0) = 0;
        virtual PRBool updateAccumulatedVSData(void) = 0;
        virtual PRBool updateVSJavaData(void) = 0;
        virtual PRBool updateWebModuleData(const char* webModuleName = NULL) = 0;
        virtual PRBool updateServletData(const char* webModuleName = NULL) = 0;
        virtual PRBool updateThreadSlots(void) = 0;
        virtual PRBool updateProcessSlots(void) = 0;
        virtual PRBool updateJdbcConnPools(int pid) = 0;
        virtual PRBool updateJvmMgmtData(int pid) = 0;
        virtual PRBool updateProcessJavaData(void) = 0;
        virtual PRBool updateSessionReplicationData(void) = 0;
};

//-----------------------------------------------------------------------------
// StatsNoticeSender
//-----------------------------------------------------------------------------

class StatsNoticeSender
{
    private:
        StatsFileHandle osFd_;

    public:
        // Pointer to next connection in list.
        StatsNoticeSender* next;

        StatsNoticeSender(StatsFileHandle fd);
        ~StatsNoticeSender(void);
        PRBool sendNotification(StatsNoticeMessages statsNoticeType);
        StatsFileHandle getFd(void) const;
        // compareId is used by algorithms in statsutil.h
        PRBool compareId(StatsFileHandle osFd) const;
};


//-----------------------------------------------------------------------------
// StatsMsgHandler
//
// Class to handle stats request in primordial process on unix.
// On Windows this is used in ChildProcess.
//-----------------------------------------------------------------------------

class StatsMsgHandler
{
    private:
        StatsUpdateHandler& updateHandler_;

        // List of notice listeners.
        StatsNoticeSender* noticeSender_;

        // Read the process id from message channel buffer and copy it in pid.
        // Return error if pid is invalid. If message doesn't contain enough
        // buffer then also return the error to the peer.
        PRBool readProcessId(StatsServerMessage& msgChannel, PRInt32& pid);

        // handle getProcessInfo request.
        PRBool handleGetProcessInfo(StatsServerMessage& msgChannel);

        // handle listen slots info.
        PRBool handleGetListenSlots(StatsServerMessage& msgChannel);

        // handle Jdbc connection pool data request.
        PRBool handleGetJdbcConnectionPool(StatsServerMessage& msgChannel);

        // handle Jvm Management stats.
        PRBool handleGetJvmMgmtStats(StatsServerMessage& msgChannel);

        // handle Virtual server core data request
        PRBool updateVSCoreDataInfo(StatsServerMessage& msgChannel);

        // Handle webmodule data request for a specific web module.
        PRBool updateWebModuleData(StatsServerMessage& msgChannel);

        // Handle all servlet data request for a specific web module.
        PRBool updateServletData(StatsServerMessage& msgChannel);

        // Handle notifications.
        void handleNotifications(StatsNoticeMessages statsNoticeType,
                                 int senderPid);

        // handle statsmsgReqGetPidList request
        void handleGetPidList(StatsServerMessage& msgChannel);

    public:
        // Constructor
        StatsMsgHandler(StatsUpdateHandler& updateHandler);

        // Destructor
        virtual ~StatsMsgHandler(void);

        // process Stats Message typically send by admin or child process.
        PRBool processStatsMessage(StatsServerMessage& msgChannel);

        // handle the Stats xml request.
        void handleStatXMLRequest(StatsServerMessage& msgChannel);

        // handle the service dump request.
        void handleServiceDumpRequest(StatsServerMessage& msgChannel);

        // getFirst and setFirst methods used by algorithms in statsutil.h
        // return the first noticeSender pointer in passed reference to
        // pointer.
        void getFirst(StatsNoticeSender*& noticeSender) const;

        // set first Notice sender pointer.
        void setFirst(StatsNoticeSender* noticeSender);

        // add a new notification receiver
        PRBool addNotificationReceiver(StatsServerMessage& msgChannel);

        // send Notification messages.
        PRBool sendNotification(StatsNoticeMessages statsNoticeType);

        // process the closing file descriptor request.
        void processCloseFd(StatsFileHandle osfd);
};

inline
StatsFileHandle
StatsNoticeSender::getFd(void) const
{
    return osFd_;
}

inline
PRBool
StatsNoticeSender::compareId(StatsFileHandle osFd) const
{
    return (getFd() == osFd) ? PR_TRUE : PR_FALSE;
}

inline
void
StatsMsgHandler::getFirst(StatsNoticeSender*& noticeSender) const
{
    noticeSender = noticeSender_;
}

inline
void
StatsMsgHandler::setFirst(StatsNoticeSender* noticeSender)
{
    noticeSender_ = noticeSender;
}


#endif
