#ifndef STATSBKUPMGR_H_
#define STATSBKUPMGR_H_

#include "statsnodes.h"   // StatsxxxNode
#include "statsutil.h"    // StringArrayBuff


//-----------------------------------------------------------------------------
// StatsProcessNodesHandler
//
// Class which maintains the backup copy of process related stats for multiple
// child processes. 
//-----------------------------------------------------------------------------

class StatsProcessNodesHandler
{
    public:
        // do reconfigure related changes.
        PRBool doReconfigureChanges(void);

        // -------------------- Getters -------------------------

        // get Process id from buffer. buffer is a pointer to StatsProcessSlot.
        int getPidFromBuffer(const void* buffer);

        // get the index of process slot in processSlot_ list whose process id
        // matches with pid.
        int getProcessSlotIndex(int pid);

        // get pid from index
        int getPidFromIndex(int nIndex);

        // Return PR_TRUE if process node corresponding to pid have jvm
        // management data available.
        PRBool isJvmMgmtStatsEnabled(int pid) const;

        // get the pointer of process slot in processSlot_ list whose process
        // id matches with pid.
        StatsProcessNode* getProcessSlot(int pid) const;

        // get First process slot in processSlot_ list.
        StatsProcessNode* getFirstProcessSlot(void) const;

        // Return the accumulated process node.
        const StatsProcessNode* getAccumulatedProcessNode(void) const;

        // -------------------- Initialization methods ----------

        // Initializes the process slot from bufferReader data.
        PRBool initializeProcessSlot(StatsBufferReader& bufferReader);

        // Initialize Listen Slot for child process whose process id matches
        // pid.
        PRBool initListenSlots(int pid,
                               StatsBufferReader& bufferReader);

        // -------------------- Update methods ------------------

        // Update the process slot from bufferReader.
        PRBool updateProcessSlot(StatsBufferReader& bufferReader);

        // update the thread slot for nth thread for child process whose
        // process id matches with pid.
        PRBool updateThreadSlot(int pid,
                                int nThreadIndex,
                                StatsBufferReader& bufferReader);

        // Reset all thread slots to empty for child process whose process id
        // matches pid. If current thread slot count is less than nThreadCount
        // then creates the thread slots to match the count to nThreadCount.
        PRBool resetThreadSlots(int pid, int nThreadCount);

        // Update the jdbc connection pool data from buffer.
        PRBool updateJdbcConnectionPools(int pid,
                                         StatsBufferReader& bufferReader);

        // Update the jvm management data.
        PRBool updateJvmMgmtData(int pid, StatsBufferReader& bufferReader);

        // Update the session replication data.
        PRBool updateSessionReplicationData(StatsBufferReader& bufferReader);

        // Accumulate the process data from all child processe data.
        void accumulateProcessData(void);

        // Constructor
        StatsProcessNodesHandler(int nChildren,
                                 int nCountProfileBuckets);

        // Destructor
        ~StatsProcessNodesHandler(void);

    private:
        // Number of Child processes
        int nChildren_;

        // Number of profile Buckets
        int nCountProfileBuckets_;

        // List of process slots. This stores the pointer to the first element
        // of the process slot
        StatsProcessNode* processSlot_;

        // The process node which will store the accumulated process data. If
        // nChildren_ is 1 then this will simply point to first node or else it
        // will be a new node which stores the accumulated process data. If a
        // new node is created for nChildren_ > 1 then at this moment
        // underneath member e.g threadPool, listen, thread etc are unused.
        StatsProcessNode* accumulatedProcNode_;

        // Search for process slots in processSlot_ whose mode is set to Empty
        StatsProcessNode* getEmptyProcessSlot(void);

        // Initializes the process slots in processSlot_.
        void createProcessSlots(void);

        // Reads connection queues from bufferReader and copies into procSlot
        // connection queues.
        PRBool copyConnectionQueues(StatsProcessNode* procSlot,
                                    StatsBufferReader& bufferReader);

        // Reads thread pools from bufferReader and copies into procSlot thread
        // pools.
        PRBool copyThreadPools(StatsProcessNode* procSlot,
                               StatsBufferReader& bufferReader);

        // Read process node members e.g connQueue and thread slots from buffer
        // Reader and copies into process Node.
        PRBool copyProcessNodeMembers(StatsProcessNode* procSlot,
                                      StatsBufferReader& bufferReader);

        // Delete process slots in processSlot_.
        void deleteProcessSlots(void);

};





//-----------------------------------------------------------------------------
// StatsVSNodesHandler
//
// Class which maintains the list of virtual server related stats for a single
// processes. 
//-----------------------------------------------------------------------------

class StatsVSNodesHandler
{
    public:
        // Constructor
        StatsVSNodesHandler(void);

        // Destructor
        ~StatsVSNodesHandler(void);

        // Return the first (tail) node of virtual server
        StatsVirtualServerNode* getFirstVSNode(void) const;

        // Allocates nCountWebModule slots. Webmodule names are provided by
        // char** webModuleNameArray. From webmodule name we can access the
        // virtual server it belongs to. This method also remove any empty
        // nodes which is not found in webModuleNameArray.
        PRBool initWebModules(int nCountWebMod,
                              char** webModuleNameArray);

        // Reads the data from bufferReader and updates the virtual server
        // stats.
        PRBool updateVSSlot(const char* vsId,
                            StatsBufferReader& bufferReader);

        // Intializes nCountVS vss nodes and set their ids.
        PRBool initVSS(int nCountVS, char** vsIds);

        // Free all the vss nodes.
        void freeVSS(void);

        // Reads the data from bufferReader and updates the accumulated
        // virtual server stats.
        PRBool updateAccumulatedVSData(StatsBufferReader& bufferReader);

        // Reads the data from bufferReader and updates the web module node.
        PRBool updateWebModuleData(StatsBufferReader& bufferReader);

        // Update the servlet stats from data provided by bufferReader.
        PRBool updateServletData(const char* webModuleName,
                                 StatsBufferReader& bufferReader);

        // return the VSNodes list and set's vssNodesList_ to NULL. It is the
        // caller responsibility now to delete the returned pointer which is a
        // list to nodes.
        StatsVirtualServerNode* detachVSNodesList(void);

        // Get accumulated virtual server slot.
        const StatsAccumulatedVSSlot* getAccumulatedVSSlot() const;

        // set the profile bucket count. Ideally this should be part of the
        // constructor but Solaris compiler has trouble in allocating class
        // array with non void constructors.
        void setProfileBucketCount(int nCountProfileBuckets);

        // do reconfigure related changes.
        PRBool doReconfigureChanges(void);

        // set the node list pointed by vss.
        void setNodeList(StatsVirtualServerNode* vss);

        // getFirst method for algorithms in statsutil.h
        void getFirst(StatsVirtualServerNode*& vss) const;

        // setFirst method for algorithms in statsutil.h
        void setFirst(StatsVirtualServerNode* vss);


    private:
        // Count of profile buckets per virtual server.
        int nCountProfileBuckets_;

        // Stats related with accumulated virtual severs.
        StatsAccumulatedVSSlot accumulatedVSStats_;

        // tail node of virtual servers.
        StatsVirtualServerNode* vssNodesList_;
};






//-----------------------------------------------------------------------------
// StatsBackupManager
//
// Class which maintains the backup copy of stats of all child processes. Right
// now this is used only in primordial process.
//-----------------------------------------------------------------------------

class StatsBackupManager
{
    public:
        // do reconfigure related changes.
        PRBool doReconfigureChanges(void);

        // -------------------- Getters -------------------------

        // get Process id from buffer. buffer is a pointer to StatsProcessSlot.
        int getPidFromBuffer(const void* buffer);

        // get the index of process slot in processSlot_ list whose process id
        // matches with pid.
        int getProcessSlotIndex(int pid);

        // get the pointer of process slot in processSlot_ list whose process
        // id matches with pid.
        StatsProcessNode* getProcessSlot(int pid) const;

        // get First process slot in processSlot_ list.
        StatsProcessNode* getFirstProcessSlot(void) const;

        // Return PR_TRUE if process node corresponding to pid have jvm
        // management data available.
        PRBool isJvmMgmtStatsEnabled(int pid) const;

        // get the virtual server id array.
        const StringArrayBuff& getVsIdArray(void) const;

        // Check to see if the webmodule list has been initialized.
        PRBool isWebModuleInitialized(void) const;

        // Return the number of web modules for all virtual servers.
        int getWebModuleCount(void) const;

        // Return the web module names array
        char** getWebModuleNames(void) const;



        // -------------------- Initialization methods ----------

        // Initializes the process slot from bufferReader data.
        PRBool initializeProcessSlot(StatsBufferReader& bufferReader);

        // Initializes the virtual server id list from the data in bufferReader.
        PRBool initVSIdList(StatsBufferReader& bufferReader);

        // Initializes the web module list for child process whose process id
        // matches pid.
        PRBool initWebModuleList(int pid,
                                 StatsBufferReader& bufferReader);

        // Initialize Listen Slot for child process whose process id matches
        // pid.
        PRBool initListenSlots(int pid,
                               StatsBufferReader& bufferReader);


        // -------------------- Update methods ------------------

        // Update the process slot from bufferReader.
        PRBool updateProcessSlot(StatsBufferReader& bufferReader);

        // Update the jdbc connection pool data from buffer.
        PRBool updateJdbcConnectionPools(int pid,
                                         StatsBufferReader& bufferReader);

        // Update the jvm management data.
        PRBool updateJvmMgmtData(int pid, StatsBufferReader& bufferReader);

        // update the Virtual server slot for child process whose process id
        // matches with pid and virutal server id matches vsId.
        PRBool updateVSSlot(int pid, const char* vsId,
                            StatsBufferReader& bufferReader);

        // update the accumulated vs stats for child process whose process id
        // matches with pid.
        PRBool updateAccumulatedVSData(int pid,
                                       StatsBufferReader& bufferReader);

        // update the web module slot for child process whose process id
        // matches with pid.
        PRBool updateWebModuleData(int pid,
                                   StatsBufferReader& bufferReader);

        // update the servlet slot for webmodule belonging to webModuleName for
        // child process whose process id matches with pid.
        PRBool updateServletData(int pid,
                                 const char* webModuleName,
                                 StatsBufferReader& bufferReader);

        // update the thread slot for nth thread for child process whose
        // process id matches with pid.
        PRBool updateThreadSlot(int pid,
                                int nThreadIndex,
                                StatsBufferReader& bufferReader);

        // Reset all thread slots to empty for child process whose process id
        // matches pid. If current thread slot count is less than nThreadCount
        // then creates the thread slots to match the count to nThreadCount.
        PRBool resetThreadSlots(int pid, int nThreadCount);

        // Update the session replication data.
        PRBool updateSessionReplicationData(StatsBufferReader& bufferReader);


        // -------------------- Aggregate methods ---------------

        // Aggregate Virtual server data for virtual server whose id matches
        // vsId. If vsId is null then aggregate for all virtual servers.
        PRBool aggregateVSData(const char* vsId = 0);

        // aggregate web module stats for web module whose name matches
        // webModuleName.
        PRBool aggregateWebModuleData(const char* webModuleName);

        // aggregate all web modules ( for all virtual server)
        PRBool aggregateAllWebModules();

        // aggregate servlets fo web module whose name matches webModuleName.
        PRBool aggregateServletsForWebModule(const char* webModuleName);

        // aggregate all servlets for all web modules.
        PRBool aggregateAllServlets();

        // Aggregate accumulated virtual servers stats.
        PRBool aggregateAccumulatedVSData(void);

        // ------ Methods related with child process death ------

        // When child processes dies, it marks the process slot for child whose
        // pid matches pid in processSlot_ as empty
        StatsProcessNode* markProcessSlotEmpty(int pid);

        // It resets the process slot in processSlot_ list whose process id
        // matches with pid.
        void unmarkLastProcessSlot(int pid);


        // -------------------- Static methods ------------------

        // Copy the profile bucket chain from bufferReader to dest.
        static PRBool copyProfileBucketChain(StatsProfileNode* dest,
                                             int nCountProfileBuckets,
                                             StatsBufferReader& bufferReader);

        // Static method to crate the backup manager. Right now it is a
        // singlton class.
        static StatsBackupManager* createBackupManager(int nChildren);

        // Child process calls this method during late initializaiton. It
        // recovers the virtual server data from an died process slot and
        // deletes the rest of the data to free up some memory.
        static void processChildLateInit(void);

        // setup new process Node
        void setupNewProcessNode();

        // Reset the original process Node attached in StatsHeader node
        void resetDefaultStatsProcessNode();

    private:

        // pointer to backup Manager.
        static StatsBackupManager* backupManager_;

        // Number of Child processes
        int nChildren_;

        // StatsProcessNodes Handler. This object handle all process related
        // stats.
        StatsProcessNodesHandler procNodesHandler_;

        // This is the process Slot which is first created and this is the
        // backup or original process node created in StatsManager::initEarly
        StatsProcessNode* defaultProcessNode_;

        // Pointer to the consolidated Virtual server stats. If number of
        // children is 1 then it is not allocated, it merely points to first
        // element of vssPerProcNodesArray_
        StatsVSNodesHandler* vssConsolidated_;

        // Array of StatsVSNodesHandler for separate vss data for separate childs.
        StatsVSNodesHandler* vssPerProcNodesArray_;

        // List of Virtual Server Ids
        StringArrayBuff vsIdArray_;

        // List of web module ids.
        StringArrayBuff webModuleArray_;

        // Constructor
        StatsBackupManager(int nChildren);

        // Destructor
        ~StatsBackupManager(void);

        // Search for process slots in processSlot_ whose mode is set to Empty
        // but process id is not INVALID_PROCESS_ID.
        int getLastDiedProcessIndex(void);

        // setupDied process retrieve the nth slot in vssChildProc_ and resets
        // the nth virutal server slot in vssChildProc_  to null.
        void setupDiedChildVSS(int nChildNo);

        // Initialize internal data structures.
        void init(void);

        // free the vssPerProcNodesArray_ and vssConsolidated_ data.
        void freeVSSForAllChilds(void);

        // Aggregate virtual server data from vss to vssAggregate. nVSIndex
        // represents the index number for which vss and vssAggregate belongs.
        PRBool aggregateVSData(StatsVirtualServerNode* vss,
                               StatsVirtualServerNode* vssAggregate,
                               int nVSIndex);

        // aggregate servlet stats for servletName which belongs to
        // webModuleName web module.
        PRBool aggregateServletData(StatsServletJSPNode* sjsAggregate,
                                    const char* webModuleName,
                                    const char* servletName);

        // aggregate web module stats for wms node.
        PRBool aggregateServletsForWebModule(StatsWebModuleNode* wms);

};




//-----------------------------------------------------------------------------
// StatsProcessNodesHandler inline methods
//-----------------------------------------------------------------------------

inline
StatsProcessNode*
StatsProcessNodesHandler::getFirstProcessSlot(void) const
{
    return processSlot_;
}

inline
StatsProcessNode* 
StatsProcessNodesHandler::getEmptyProcessSlot(void)
{
    return getProcessSlot(INVALID_PROCESS_ID);
}

inline
const StatsProcessNode*
StatsProcessNodesHandler::getAccumulatedProcessNode(void) const
{
    return accumulatedProcNode_;
}


//-----------------------------------------------------------------------------
// StatsVirtualServerNode inline methods
//-----------------------------------------------------------------------------


inline
StatsVirtualServerNode* 
StatsVSNodesHandler::getFirstVSNode(void) const
{
    return vssNodesList_;
}

inline
const StatsAccumulatedVSSlot*
StatsVSNodesHandler::getAccumulatedVSSlot() const
{
    return &accumulatedVSStats_;
}

inline
StatsVirtualServerNode* 
StatsVSNodesHandler::detachVSNodesList(void)
{
    StatsVirtualServerNode* vss = vssNodesList_;
    vssNodesList_ = 0;
    return vss;
}

inline
void 
StatsVSNodesHandler::setProfileBucketCount(int nCountProfileBuckets)
{
    nCountProfileBuckets_ = nCountProfileBuckets;
}

inline
void
StatsVSNodesHandler::setNodeList(StatsVirtualServerNode* vss)
{
    vssNodesList_ = vss;
}

inline
void
StatsVSNodesHandler::getFirst(StatsVirtualServerNode*& vss) const
{
    vss = vssNodesList_;
}

inline
void
StatsVSNodesHandler::setFirst(StatsVirtualServerNode* vss)
{
    vssNodesList_ = vss;
}


//-----------------------------------------------------------------------------
// StatsBackupManager inline methods
//-----------------------------------------------------------------------------

inline
StatsProcessNode*
StatsBackupManager::getFirstProcessSlot(void) const
{
    return procNodesHandler_.getFirstProcessSlot();
}

inline
PRBool
StatsBackupManager::isJvmMgmtStatsEnabled(int pid) const
{
    if (getWebModuleCount() > 0)
    {
        // jvm is enabled so process should have jvm management data.
        return PR_TRUE;
    }
    return procNodesHandler_.isJvmMgmtStatsEnabled(pid);
}

inline
const StringArrayBuff&
StatsBackupManager::getVsIdArray(void) const
{
    return vsIdArray_;
}

inline
int
StatsBackupManager::getWebModuleCount(void) const
{
    return webModuleArray_.getStringCount();
}

inline
char**
StatsBackupManager::getWebModuleNames(void) const
{
    return webModuleArray_.getStringArray();
}

inline
int 
StatsBackupManager::getPidFromBuffer(const void* buffer)
{
    return procNodesHandler_.getPidFromBuffer(buffer);
}

inline
PRBool
StatsBackupManager::initializeProcessSlot(StatsBufferReader& bufferReader)
{
    return procNodesHandler_.initializeProcessSlot(bufferReader);
}

inline
PRBool StatsBackupManager::updateProcessSlot(StatsBufferReader& bufferReader)
{
    return procNodesHandler_.updateProcessSlot(bufferReader);
}

inline
PRBool 
StatsBackupManager::updateJdbcConnectionPools(int pid,
                                              StatsBufferReader& bufferReader)
{
    return procNodesHandler_.updateJdbcConnectionPools(pid, bufferReader);
}

inline
PRBool
StatsBackupManager::updateJvmMgmtData(int pid, StatsBufferReader& bufferReader)
{
    return procNodesHandler_.updateJvmMgmtData(pid, bufferReader);
}

inline
PRBool
StatsBackupManager::updateSessionReplicationData(
                                            StatsBufferReader& bufferReader)
{
    return procNodesHandler_.updateSessionReplicationData(bufferReader);
}

inline
PRBool
StatsBackupManager::resetThreadSlots(int pid,
                                     int nThreadCount)
{
    return procNodesHandler_.resetThreadSlots(pid, nThreadCount);
}

inline
PRBool StatsBackupManager::updateThreadSlot(int pid,
                                            int nThreadIndex,
                                            StatsBufferReader& bufferReader)
{
    return procNodesHandler_.updateThreadSlot(pid, nThreadIndex, bufferReader);
}

inline
PRBool 
StatsBackupManager::initListenSlots(int pid,
                                    StatsBufferReader& bufferReader)
{
    return procNodesHandler_.initListenSlots(pid, bufferReader);
}

inline
StatsProcessNode* 
StatsBackupManager::getProcessSlot(int pid) const
{
    return procNodesHandler_.getProcessSlot(pid);
}

inline
int 
StatsBackupManager::getProcessSlotIndex(int pid)
{
    return procNodesHandler_.getProcessSlotIndex(pid);
}

#endif

