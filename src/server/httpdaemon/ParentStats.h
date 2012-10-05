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

#ifndef _PARENTSTATS_H_
#define _PARENTSTATS_H_
#ifdef XP_UNIX
#include "nspr.h"
#include "httpdaemon/ParentAdmin.h"      // ParentAdmin
#include "httpdaemon/StatsMsgHandler.h"  // StatsMsgHandler
#include "httpdaemon/statsbkupmgr.h"     // StatsBackupManager


//-----------------------------------------------------------------------------
// StatsChildConnInfo
//
// Class to store Child fd and connection information.
//-----------------------------------------------------------------------------

class StatsChildConnInfo
{
    private:
        PRFileDesc* childFD_;
        int childPid_;
        PRBool fChildInitialized_;
        PRBool fWebModuleInit_;
        PRBool fProcessAlive_;
        PRBool fGotReconfigResponse_;

    public:
        StatsChildConnInfo(void);
        ~StatsChildConnInfo(void);

        // Reset the members.
        void reset(void);

        // setters
        void setConnectionFD(PRFileDesc* fd);
        void setChildInitialized(void);
        void setWebModulesInitialized(void);
        void setChildPid(int pid);
        void setReconfigStatus(PRBool fStatus);

        // Getters
        PRFileDesc* getConnectionFD(void) const;
        PRBool isValidConnectionFD(void) const;
        PRBool isConnectionFDMatches(PRFileDesc* fd) const;
        PRBool isConnectionReady(void) const;
        PRBool isChildInitialized(void) const;
        PRBool isWebModuleInitialized(void) const;
        PRBool isPidInitialized(void) const;
        PRBool isReadyForUpdate(void) const;
        PRBool isReconfigureDone(void) const;
        int getChildPid(void) const;
};

//-----------------------------------------------------------------------------
// ParentStats
//
// Class to manage stats communication in primordial process.
//-----------------------------------------------------------------------------

class ParentStats  : public ParentAdmin , public StatsUpdateHandler
{

    //-------------------------------------------------------------------------
    // ParentStats::MsgErrorHandler
    //
    // Class to handle error messages with stats communication.
    //-------------------------------------------------------------------------
    class MsgErrorHandler
    {
        private:
            // Pointer to ParentStats class.
            ParentStats* parentStats_;
        public:

            // Constructor
            MsgErrorHandler(ParentStats* parentStats);

            // Handling message channel errors. It is called when function
            // call from StatsServerMessage returns failure.
            void handleMessageChannelError(PRFileDesc* fd,
                                           StatsServerMessage& msgChannel,
                                           const char* callerContext,
                                           int nChildNo);

            // log DBT_ParentStats_InternalError
            void logInternalStatsError(const char* callerContext,
                                       int nChildNo);
    };

    //-------------------------------------------------------------------------
    // ParentStats::Messenger
    //
    // A customized StatsMessenger class to communicate with a particular
    // child and process stats errors.
    //-------------------------------------------------------------------------
    class Messenger
    {
        private:
            // child number.
            int nChildNo_;

            // Child's stats socket file descriptor.
            PRFileDesc* fd_;

            // message channel
            StatsServerMessage statsMsgChannel_;

            // message sender.
            StatsMessenger msgSender_;

            // error handler.
            MsgErrorHandler& errHandler_;
        public:
            // Constructor
            Messenger(int nChildNo,
                      PRFileDesc* fd,
                      MsgErrorHandler& errHandler);

            // send msgType message and receive response. if msgStringArg is
            // non null then it is used as a message argument.
            PRBool sendMsgAndRecvResp(StatsMessages msgType,
                                      const char* callerContext,
                                      const char* msgStringArg = NULL);

            // send msgType message and receive response. buffer is sent along
            // with the message request.
            PRBool sendMsgAndRecvResp(StatsMessages msgType,
                                      const char* callerContext,
                                      const void* buffer,
                                      int bufferLen);

            // get the buffer reading containing the message response.
            StatsBufferReader& getBufferReader(void);

            // Create a copy of StatsMessenger's buffer reader into
            // bufferReader.
            void getBufferReaderCopy(StatsBufferReader& bufferReader) const;
    };

    // StatsBackupManager's member functions typedefs. These are used by
    // initOrUpdateStats methods.
    typedef PRBool (StatsBackupManager::* PFnInitOrUpdate)
                                    (StatsBufferReader& bufferReader);
    typedef PRBool (StatsBackupManager::* PFnProcInitOrUpdate)
                                    (int pid, StatsBufferReader& bufferReader);
    typedef PRBool (StatsBackupManager::* PFnStrProcInitOrUpdate)
                                    (int pid, const char* strArg,
                                     StatsBufferReader& bufferReader);

    public :
        ParentStats(const PRInt32 nChildren) ;
        ~ParentStats(void);

        // ------------- overloaded methods from ParentAdmin class ------

        // initialization when all childs are started.
        virtual void initLate(void);

        // process the death of child with process id pid.
        virtual void processChildDeath(int pid);

        // process the startup of child with process id pid.
        virtual void processChildStart(int pid);

        // process the start of reconfigure event.
        virtual void processReconfigure(void);

        // process the stats messages.
        virtual void processStatsMessage(int fdIndex);

        // Handle error with stats file descriptor.
        void processFileDescriptorError(PRFileDesc* fd);

    private :
        // Array to store child connection info.
        StatsChildConnInfo* childConnInfo_;

        // Count of connected childs.
        PRInt32 nConnectedChild_;

        // statsBackupManager pointer. StatsBackupManager stores all the
        // statistics data managed by primordial process
        StatsBackupManager* statsBackupManager_;

        // A flag to save whether late Init has already been done or not.
        PRBool fLateInitDone_;

        // A flag to save the state when child is doing the reconfigure.
        PRBool fReconfigureInProgress_;

        // Stats message handler
        StatsMsgHandler statsMsgHandler_;

        // Message error handler object
        MsgErrorHandler msgErrorHandler_;

        // Initialize Child Data. After a child is intializes properly, it
        // informs the primordial process. initChildData does the
        // initialization of child. It is the point when primordial starts
        // accessing the stats data from child process.
        PRBool initChildData(int nChildNo);

        // Intialize the Virtual Server Id list.
        PRBool initVSIdList(int nChildNo);

        // Initializes listen slots.
        PRBool initListenSlots(int nChildNo);

        // Reconfigure can change lots of things so refresh stats data
        // structure and update stats.
        PRBool doStatsReconfigure(void);

        // ------------ Update methods -------------------

        // Update Virtual server core data for nth Child
        PRBool updateVSCoreData(int nChildNo);

        // Update Virtual Server core data for nth Child whose id matches with
        // vsId
        PRBool updateVSCoreData(int nChildNo, const char* vsId);

        // update accumulated virtual servers data for nth child.
        PRBool updateAccumulatedVSData(int nChildNo);

        // Update Process slot for nth Child
        PRBool updateProcessSlot(int nChildNo);

        // Update webModule data for nth Child for webmodule whose name
        // matches webModuleName. If webModuleName is NULL then it update all
        // web modules.
        PRBool updateWebModuleData(int nChildNo, const char* webModuleName);

        // Update servlet data for nth Child for webmodule whose name
        // matches webModuleName.
        PRBool updateServletData(int nChildNo, const char* webModuleName);

        // Update servlet data for nth Child
        PRBool updateServletData(int nChildNo);

        // Update thread slot data for nth Child
        PRBool updateThreadSlot(int nChildNo);

        // Utiltity function to get the process index from process id.
        int getProcessSlotIndex(int pid) const;

        // Update those java data which is per process based e.g jdbc
        // connection pool, jvm management etc.
        PRBool updatePerProcessJavaData(int nChildNo);

        // Update per process java data for all child processes.
        PRBool updateProcessJavaData(void);

        // Send a msgType message to nChildNo child and get it's response and
        // then call StatsBackupManager's bkupMgrFunc method. If msgArgString
        // is non null, it is sent as an argument to message.
        PRBool initOrUpdateStats(int nChildNo,
                                 StatsMessages msgType,
                                 const char* callerContext,
                                 PFnInitOrUpdate bkupMgrFunc,
                                 const char* msgArgString = NULL);

        // Send a msgType message to nChildNo child and get it's response and
        // then call StatsBackupManager's bkupMgrFunc method. If msgArgString
        // is non null, it is sent as an argument to message. pid is also
        // passed to bkupMgrFunc method.
        PRBool initOrUpdateStats(int nChildNo, int pid,
                                 StatsMessages msgType,
                                 const char* callerContext,
                                 PFnProcInitOrUpdate bkupMgrFunc,
                                 const char* msgArgString = NULL);

        // Send a msgType message to nChildNo child and get it's response and
        // then call StatsBackupManager's bkupMgrFunc method. If msgArgString
        // is non null, it is sent as an argument to message. pid and
        // msgArgString is passed to bkupMgrFunc method.
        PRBool initOrUpdateStats(int nChildNo, int pid,
                                 StatsMessages msgType,
                                 const char* callerContext,
                                 PFnStrProcInitOrUpdate bkupMgrFunc,
                                 const char* msgArgString = NULL);


        // ------------- overloaded methods from ParentAdmin class ------

        // When child does it's initialization. It sends a message to
        // primordial. This overloaded function is called to handle this event.
        // This is the time to do the initializing stats data for the child
        virtual void handleChildInit(PRFileDesc* fd);

        // This function is called if primordial is closing the file
        // descriptor. In this function right now we check if fd matches with
        // any of the child fd, if so then we uninitialize the connection with
        // that child.
        virtual void processCloseFd(PRFileDesc* fd);

        // ------------- StatsUpdateHandler methods implementation --------

        // If all childs are connected it returns PR_TRUE
        virtual PRBool isReadyForUpdate(void);

        // Update the jdbc connection pool data.
        virtual PRBool updateJdbcConnPools(int pid);

        // Update the jvm management data.
        virtual PRBool updateJvmMgmtData(int pid);

        // initWebModuleList initializes the webModulelist for child process
        // whose process id matches pid.
        virtual PRBool initWebModuleList(int pid);

        // Return PR_TRUE if all child has finished the web module
        // initialization.
        virtual PRBool isAllChildWebModInitDone(void);

        // Child with process id = pid has completed the reconfig.
        virtual PRBool updateOnReconfig(int pid);

        // updates Virtual server core side data. If vsId is null then it
        // updates data to all virtual servers.
        virtual PRBool updateVSCoreData(const char* vsId = 0);

        // update accumulated virtual servers data
        virtual PRBool updateAccumulatedVSData(void);

        // update Virtual server Java side data.
        virtual PRBool updateVSJavaData(void);

        // Update all thread slots for all childs.
        virtual PRBool updateThreadSlots(void);

        // Update all process slots for all childs.
        virtual PRBool updateProcessSlots(void);

        // update the process slot whose process id matches pid.
        virtual PRBool updateProcessSlotForPid(int pid);

        // updata webModule stats for all childs for web module whose name
        // matches webModuleName. If webModuleName is NULL then it updates all
        // web modules for all childs.
        virtual PRBool updateWebModuleData(const char* webModuleName = NULL);

        // update all servlets for all childs
        virtual PRBool updateServletData(const char* webModuleName = NULL);

        // update session replication data.
        virtual PRBool updateSessionReplicationData(void);
};

//-----------------------------------------------------------------------------
//----------- StasChildConnInfo inline methods --------------------------------
//-----------------------------------------------------------------------------

inline
PRFileDesc*
StatsChildConnInfo::getConnectionFD(void) const
{
    return childFD_;
}

inline
void
StatsChildConnInfo::setConnectionFD(PRFileDesc* fd)
{
    childFD_ = fd;
    return;
}


inline
PRBool
StatsChildConnInfo::isValidConnectionFD(void) const
{
    return ((childFD_ == 0) ? PR_FALSE : PR_TRUE);
}

inline
PRBool
StatsChildConnInfo::isConnectionFDMatches(PRFileDesc* fd) const
{
    return ((childFD_ == fd) ? PR_TRUE : PR_FALSE);
}

inline
void
StatsChildConnInfo::setChildInitialized(void)
{
    fChildInitialized_ = PR_TRUE;
}

inline
void
StatsChildConnInfo::setWebModulesInitialized(void)
{
    fWebModuleInit_ = PR_TRUE;
}

inline
PRBool
StatsChildConnInfo::isChildInitialized(void) const
{
    return fChildInitialized_;
}

inline
PRBool
StatsChildConnInfo::isWebModuleInitialized(void) const
{
    return fWebModuleInit_;
}

inline
PRBool
StatsChildConnInfo::isPidInitialized(void) const
{
    if (childPid_ != -1)
    {
        return PR_TRUE;
    }
    return PR_FALSE;
}

inline
PRBool
StatsChildConnInfo::isConnectionReady(void) const
{
    PRBool retVal = ((isValidConnectionFD() == PR_TRUE) &&
                    (isChildInitialized() == PR_TRUE)
                    ) ? PR_TRUE : PR_FALSE;
    return retVal;
}

inline
void
StatsChildConnInfo::setChildPid(int pid)
{
    childPid_ = pid;
}

inline
void
StatsChildConnInfo::setReconfigStatus(PRBool fStatus)
{
    fGotReconfigResponse_ = fStatus;
}

inline
int
StatsChildConnInfo::getChildPid(void) const
{
    return childPid_;
}

inline
int
StatsChildConnInfo::isReadyForUpdate(void) const
{
    return (((isPidInitialized() == PR_TRUE) &&
              (isConnectionReady() == PR_TRUE)) ? PR_TRUE : PR_FALSE);
}

inline
PRBool
StatsChildConnInfo::isReconfigureDone(void) const
{
    return fGotReconfigResponse_;
}

//-----------------------------------------------------------------------------
//----------- ParentStats::MsgErrorHandler inline methods ---------------------
//-----------------------------------------------------------------------------

inline
ParentStats::MsgErrorHandler::MsgErrorHandler(ParentStats* parentStats)
{
    parentStats_ = parentStats;
}

//-----------------------------------------------------------------------------
//----------- ParentStats::Messenger inline methods ---------------------------
//-----------------------------------------------------------------------------

inline
ParentStats::Messenger::Messenger(int nChildNo,
                                  PRFileDesc* fd,
                                  MsgErrorHandler& errHandler):
                        nChildNo_(nChildNo),
                        fd_(fd),
                        statsMsgChannel_(PR_FileDesc2NativeHandle(fd),
                                         PR_TRUE), // Don't close the fd
                        msgSender_(&statsMsgChannel_),
                        errHandler_(errHandler)
{
}

inline
StatsBufferReader&
ParentStats::Messenger::getBufferReader(void)
{
    return msgSender_.getBufferReader();
}

inline
void
ParentStats::Messenger::getBufferReaderCopy(
                                StatsBufferReader& bufferReader) const
{
    return msgSender_.getBufferReaderCopy(bufferReader);
}

#endif // XP_UNIX
#endif

