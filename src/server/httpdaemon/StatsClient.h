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

#ifndef _STATS_CLIENT_H
#define _STATS_CLIENT_H

#include "httpdaemon/statsmessage.h"
#include "httpdaemon/statsnodes.h"
#include "support/SimpleHash.h"
#include "support/GenericVector.h"
#include "support/NSString.h"
#include "NsprWrap/Thread.h"

class StatsProcessNodesHandler;
class StatsVSNodesHandler;
class StringStore;

typedef PtrVector<NSString> StatsStringVector;

enum StatsClientErrorMessages
{
    sclerrmsgNoError,       // Place holder for no error.
    // Error messages related with stats communication with instance.
    sclerrmsgNoConnection,  // There is no connection with instance
    sclerrmsgServerBusy,    // Instance returned statsmsgErrorNotReady
    sclerrmsgPidNotExist,   // previously fetched pid no longer exist.
    sclerrmsgInternalError, // Any other types of error returned by server.
    sclerrmsgErrorUnknown,  // Unknown error (probable message corruption)

    // Internal error types
    sclerrmsgIllegalPid,      // StatsClient doesn't know about the Pid.
    sclerrmsgVSNotFound,      // Unable to find virtual server.
    sclerrmsgWebappNotFound,  // Unable to find webapp.
    sclerrmsgServletNotFound, // Unable to find servlet.
    sclerrmsgJvmNodeNotFound, // Unable to find jdbc node.
    sclerrmsgJdbcNodeNotFound, // Unable to find jdbc node.
    sclerrmsgSessReplNodeNotFound // Unable to find jdbc node.
};

//-----------------------------------------------------------------------------
// StatsClient
//-----------------------------------------------------------------------------

class HTTPDAEMON_DLL StatsClient
{
    private:
        // A lock to provide synchronized access to the methods of the object
        // of this class.
        PRLock*      clientLock_;

        // Stats message channel.
        StatsServerMessage* statsMsgChannel_;

        // Stats notice channel.
        StatsServerMessage* statsNoticeChannel_;

        // Last known reconfig count. If server does the reconfigure then the
        // reconfig count in header will increase and the client will know that
        // server has done the reconfigure. (This mechanism could optionally be
        // provided by stats notifications.
        PRUint32 prevReconfigCount_;

        // Previous count of number of child restart.
        PRUint32 prevChildDeathCount_;

        // Last error message.
        StatsClientErrorMessages lastErrorMsg_;

        // Last connection path used to create the connection.
        NSString connectionPath_;

        // StatsHeader node.
        StatsHeaderNode     statsHdr_;

        // process Nodes handler. Manages multiple child process of instance.
        StatsProcessNodesHandler* procNodesHandler_;

        // Array of connection Queue Ids.
        StatsStringVector connQueueIds_;

        // Array of threadpool  names.
        StatsStringVector threadPoolNames_;

        // Array of listen socket ids.
        StatsStringVector listenSocketIds_;

        // Array of profile names.
        StatsStringVector profileNames_;

        // Array of StatsVSNodesHandler for separate vss data for separate
        // childs.
        StatsVSNodesHandler* vssInstance_;

        // List of Virtual Server Ids
        StatsStringVector vsIds_;

        // List of web module ids.
        StatsStringVector webModulesList_;

        // Instance StringStore
        StringStore* strStoreInstance_;

        // A flag to store if the web modules have been initialized or not.
        PRBool fWebModulesInit_;

        // Get the string store of the instance
        PRBool initStringStore(void);

        // Initialize the listen slots.
        PRBool initListenSlots(int pid);

        // Initialize single process slot related objects e.g connection queue
        // ids, thread pool names etc.
        PRBool initProcessSlotMisc(StatsProcessNode* processNode);

        // intializes the process slots of instance
        PRBool initProcessSlots(void);

        // initializes the prifile names.
        PRBool initProfileNames(void);

        // initializes virtual server stats for the instance.
        PRBool initInstanceVSS(void);

        // Fetch the process id list from instance.
        PRBool fetchPidList(StatsBufferReader& strMsgBuff);

        // send stats message and receive the response.
        PRBool sendRecvStatsRequest(StatsMessenger& messenger,
                                    StatsMessages msgType,
                                    const void* sendBuffer = 0,
                                    int sendBufLen = 0);

        // Simpied version for sendRecvStatsRequest for passing strings.
        PRBool sendRecvStatsRequestString(StatsMessenger& messenger,
                                          StatsMessages msgType,
                                          const char* sendBuffer);

        // send message and receive stats. Response is sent in a way where
        // length of the output is not known to sender before so sender keep
        // sending response and at the end, it sends zero length response to
        // show the end of response. This method copies the response to input
        // argument "response"
        PRBool sendRecvStatsRequest(StatsMessages msgType,
                                    StatsMessages msgTypeAck,
                                    const char* sendBuffer,
                                    NSString& response);

        // Do reconfigure changes for elements which are per process based.
        PRBool doProcessNodesReconfigure(void);

        // get the StatsProcessNode from procNodesHandler_
        const StatsProcessNode* getProcessNodeInternal(int pid);

        // set last error
        void setLastError(StatsErrorMessages errMsg);

        // Pop all the elements in strings and free the memory
        static void freeStringsInVector(StatsStringVector& vectorObj);

        // fill strings in vector.
        static void fillStringsInVector(StatsStringVector& vectorObj,
                                        int nCount,
                                        char** stringArray);

    public:
        // Constructor
        StatsClient(void);

        // Desctructor
        ~StatsClient(void);

        // Lock the channel. Channel must be locked/unlocked if used by
        // multiple threads at the same time.
        PRBool lock(void);

        // Unlock the channel. Channel must be locked/unlocked if used by
        // multiple threads at the same time.
        PRBool unlock(void);

        // intitialization of connection
        PRBool init(const char* connName, StatsServerMessage* statsMsgChannel);

        // late update of stats to recover any missed notifications.
        PRBool lateUpdate();

        // Reset the connection and internally allocated objects for stats.
        void resetConnection(void);

        // Check to see if the connection with the instance is alive.
        PRBool isConnectionAlive(void);

        // Check to see if Jvm is enabled in instance.
        PRBool isJvmEnabled(void);

        // Check to see if webapps are enabled in instance.
        PRBool isWebappsEnabled(void);

        // get the instance connection path.
        const NSString& getConnectionPath();

        // Return the string for offset.
        const char* getStringFromOffset(ptrdiff_t offset);

        // get the StatsHeader
        const StatsHeaderNode*
              getStatsHeader(PRBool fUpdateFromServer = PR_TRUE);

        // get the last error message.
        StatsClientErrorMessages getLastError(void);

        // reset the last error message.
        void resetLastError();

        // get Process id for nIndex ( beginning with 0 to
        // (headerStats.MaxProcs - 1) ). It returns -1 if nIndex is invalid or
        // connection is broken.
        int getPid(int nIndex);

        // Return the process Node for process id pid.
        const StatsProcessNode*
              getProcessNode(int pid, PRBool fUpdateFromServer = PR_TRUE);

        // Return the accumulated process node.
        const StatsProcessNode* getAccumulatedProcessNode(void);

        // Return the Jdbc connection pool corresponding to poolName with
        // process id pid.
        const StatsJdbcConnPoolNode*
              getJdbcPoolNode(int pid,
                              const char* poolName,
                              PRBool fUpdateFromServer = PR_TRUE);

        // Return the jvm management node for process pid.
        const StatsJvmManagementNode*
              getJvmMgmtNode(int pid,
                             PRBool fUpdateFromServer = PR_TRUE);

        // Return the session replication stats node.
        const StatsSessionReplicationNode*
              getSessionReplicationNode(PRBool fUpdateFromServer = PR_TRUE);

        // Return the Virtual server id list (Vector of NSString pointers).
        const StatsStringVector& getVsIdList(void);

        // Return the list of connection queue names.
        const StatsStringVector& getConnectionQueueIds(void) const;

        // Return the list of thread pool names.
        const StatsStringVector& getThreadPoolNames(void) const;

        // Return the list of listen socket ids.
        const StatsStringVector& getListenSocketIds(void) const;

        // Return the list of profile names.
        const StatsStringVector& getProfileNames(void);

        // Return the virtual server node whose id matches vsId.
        const StatsVirtualServerNode*
              getVirtualServerNode(const char* vsId,
                                   PRBool fUpdateFromServer = PR_TRUE);

        // Returns the virtual server head node.
        const StatsVirtualServerNode* getVirtualServerTailNode(void);

        // Return the web module list (Vector of NSString pointers) for all
        // virtual servers.
        const StatsStringVector& getWebModuleList(void);

        // Return the webmodule node for webModuleName
        const StatsWebModuleNode*
              getWebModuleNode(const char* webModuleName,
                               PRBool fUpdateFromServer = PR_TRUE);

        // Return the webmodule node for webModuleName
        const StatsServletJSPNode*
              getServletNode(const char* webModuleName,
                             const char* servletName);

        // If PLATFORM_SPECIFIC_STATS_ON macro is not defined then this
        // function will return NULL. Right now it is only defined for Solaris
        // and Linux.
        const StatsCpuInfoNode*
              getCpuInfoNode(PRBool fUpdateFromServer = PR_TRUE);

        // Get accumulated virtual server slot.
        const StatsAccumulatedVSSlot*
              getAccumulatedVSSlot(PRBool fUpdateFromServer = PR_TRUE);

        // get StatsXml in strStatsXml. It is always updated from server.
        PRBool getStatsXml(NSString& strStatsXml, const char* queryString);

        // get service dump output in strPerfDump. It is always updated from
        // server.
        PRBool getPerfDump(NSString& strPerfDump, const char* queryString);

        // Update the stats header.
        PRBool updateStatsHeader(void);

        // Update the jdbc connection pool for process whose process id matches
        // pid.
        PRBool updateJdbcConnectionPools(int pid);

        // Update the jvm management data for process whose process id matches
        // pid.
        PRBool updateJvmMgmtData(int pid);

        // Update session replication data.
        PRBool updateSessionReplicationData();

        // Update the virtual server list for the instance.
        PRBool updateVSList(void);

        // Update the virtual server stats for vsId.
        PRBool updateVSNode(const char* vsId);

        // Update all virtual server stats (not the underneath webapps/servlet
        // stats)
        PRBool updateAllVSNodes(void);

        // Update webmodule list
        PRBool updateWebModuleList(void);

        // Update the webmodule stats for webModuleName.
        PRBool updateWebModuleData(const char* webModuleName);

        // If we want to fetch all webmodules data then this function retrieves
        // all webmodules data in single transaction. One can access individual
        // webmodule by calling getWebModuleNode with fUpdateFromServer as
        // PR_FALSE.
        PRBool updateAllWebModules(void);

        // Update all servlet for webmoduleName
        PRBool updateAllServlets(const char* webModuleName);

        // Update all servlets for all webmodules
        PRBool updateAllServletsForAllWebModules(void);

        // Update accumulated virtual server stats.
        PRBool updateAccumulatedVSData(void);

        // Return the flag to tell whether the server has reconfigured.
        PRBool checkServerReconfigured(void);

        // Return TRUE if one or more child died and restarted.
        PRBool checkChildRestart(void);

        // After making this call any of the internal data structures could
        // change. So caller should not use any of the previously returned
        // pointer. Caller must make a fresh call to get the new pointers.
        PRBool doReconfigureChanges(void);

        // If one of the child webserver process dies, primordial restarts the
        // process. StatsClient need to do clean up for the died process. This
        // function clean up the old process node and replace it with a new
        // child node. This function is not useful for windows because on
        // windows if webserver dies, we will loose the connection.
        PRBool doChildRestartChanges(void);

        // Add a new connection for receiving notices from instance.
        PRBool addNotificationConnection(void);

        // Check to see if there is some notification from instance.
        PRBool recvNotification(StatsNoticeMessages& noticeMsg);

        // process the error. If error happened with file descriptor i.e
        // sending/receiving data then it closes the connection.
        void processError(void);
};


//-----------------------------------------------------------------------------
// StatsClientManager
//-----------------------------------------------------------------------------

class HTTPDAEMON_DLL StatsClientManager
{
    private:
        struct StatsClientHashEntry
        {
            StatsClient* client;
            StatsServerMessage* statsMsgChannel;
        };

        // A Hash table to store StatsClient object pointer with
        // serverId.
        SimpleStringHash statsClientHash_;

        // default StatsClientManager pointer.
        static StatsClientManager* defaultStatsClientMgr_;

        // A lock to prevent simultaneous access from multiple threads.
        PRLock* clientMgrLock_;

        // Try to connect to instance. Return the newly created StatsClient if
        // connection is successful.
        StatsClient*
        tryNewStatsConnection(StatsClientHashEntry* clientHashEntry,
                              const char* serverId,
                              const char* instanceSockPath) const;

        // Look into the hash table for entry for serverId. If found returns
        // it or else create a new hash entry, insert into hash table and
        // returns the newly created entry.
        StatsClientManager::StatsClientHashEntry* getHashEntry(
                                                        const char* serverId);

    public:
        // Constructor
        StatsClientManager(void);

        // Destructor
        ~StatsClientManager(void);

        // Return the StatsClient pointer for the serverId.
        // serverId is the name of the instance e.g https-test.
        StatsClient* getStatsClient(const char* serverId);

        // connect to instance whose socket path is instanceSockPath and whose
        // id is serverId.
        StatsClient* connectInstance(const char* serverId,
                                     const char* instanceSockPath);

        // Lock the channel.
        PRBool lock(void);

        // Unlock the channel.
        PRBool unlock(void);

        // Disconnect and free the memory associated for serverId.
        void freeConnection(const char* serverId);

        // Return the default created StatsClientManager
        static StatsClientManager* getDefaultManager(void);
};



//-----------------------------------------------------------------------------
//
// StatsClient inline methods
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// StatsClient::isConnectionAlive
//-----------------------------------------------------------------------------

inline
PRBool
StatsClient::isConnectionAlive(void)
{
    return (statsMsgChannel_ ? PR_TRUE : PR_FALSE);
}

//-----------------------------------------------------------------------------
// StatsClient::getConnectionPath
//-----------------------------------------------------------------------------

inline
const NSString&
StatsClient::getConnectionPath()
{
    return connectionPath_;
}

//-----------------------------------------------------------------------------
// StatsClient::getLastError
//-----------------------------------------------------------------------------

inline
StatsClientErrorMessages
StatsClient::getLastError(void)
{
    return lastErrorMsg_;
}

//-----------------------------------------------------------------------------
// StatsClient::resetLastError
//-----------------------------------------------------------------------------

inline
void
StatsClient::resetLastError(void)
{
    lastErrorMsg_ = sclerrmsgNoError;
}

//-----------------------------------------------------------------------------
// StatsClient::getConnectionQueueIds
//-----------------------------------------------------------------------------

inline
const StatsStringVector&
StatsClient::getConnectionQueueIds(void) const
{
    return connQueueIds_;
}

//-----------------------------------------------------------------------------
// StatsClient::getThreadPoolNames
//-----------------------------------------------------------------------------

inline
const StatsStringVector&
StatsClient::getThreadPoolNames(void) const
{
    return threadPoolNames_;
}

//-----------------------------------------------------------------------------
// StatsClient::getListenSocketIds
//-----------------------------------------------------------------------------

inline
const StatsStringVector&
StatsClient::getListenSocketIds(void) const
{
    return listenSocketIds_;
}

//-----------------------------------------------------------------------------
// StatsClient::getProfileNames
//-----------------------------------------------------------------------------

inline
const StatsStringVector&
StatsClient::getProfileNames(void)
{
    if (!vssInstance_)
        initInstanceVSS();
    return profileNames_;
}



//-----------------------------------------------------------------------------
//
// StatsClientManager inline methods
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// StatsClientManager::lock
//-----------------------------------------------------------------------------

inline
PRBool
StatsClientManager::lock(void)
{
    if (clientMgrLock_)
    {
        PR_Lock(clientMgrLock_);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// StatsClientManager::unlock
//-----------------------------------------------------------------------------

inline
PRBool
StatsClientManager::unlock(void)
{
    if (clientMgrLock_)
    {
        PR_Unlock(clientMgrLock_);
    }
    return PR_TRUE;
}

#endif
