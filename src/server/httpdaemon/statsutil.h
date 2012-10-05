#ifndef _STATSUTIL_H_
#define _STATSUTIL_H_

#include "httpdaemon/libdaemon.h"  // HTTPDAEMON_DLL
#include "base/wdservermessage.h"  // WDMSGBUFFSIZE
#include "public/iwsstats.h"       // StatsxxxSlots
#include "support/NSString.h"      // NSString
#include "httpdaemon/statsnodes.h" // StatsxxxNodes

#define STATS_ALIGN(a) MAKE_ALIGNED((a), 8) // Proper alignment for <= 64 bit words
#define MAKE_ALIGNED(size, alignment) ((size) % (alignment) ? (size) + (alignment) - ((size) % (alignment)) : (size))

#define INVALID_PROCESS_ID ((PRInt32) -1)


//-----------------------------------------------------------------------------
// StatsMsgBuff
//
// Utitlity class to write data into buffer.
//-----------------------------------------------------------------------------

class StatsMsgBuff : public NSString
{
    private:
        char  stackBuffer_[WDMSGBUFFSIZE];

        // Disallow assignment operator and copy constructor
        StatsMsgBuff& operator=(const StatsMsgBuff& input);
        StatsMsgBuff(const StatsMsgBuff& input);
    public:
        StatsMsgBuff(int nCapacity = 0);
        ~StatsMsgBuff(void);

        // setData sets the data at the beginning.
        int setData(const void* buffer, int bufLen);

        // appendData appends the data at the end.
        int appendData(const void* buffer, int bufLen);

        // Write a 32 bit integer with 32 bit pad integer.
        int appendInteger(PRInt32 val, PRInt32 padValue = 0);

        // appendString appends the String. It takes care of the append length
        // for alignment bounaries.
        int appendString(const char* string);

        // Custom functions to write various structures. These helps in passing
        // the data with right length.

        // Ideally a template like
        //      template <class Slot>
        //      int appendSlot(const Slot& slot);
        // suites here best but Visual C++ 6.0 compiler has a bug with member
        // function templates hence defining various slots members manually.

        // Process Slot
        int appendSlot(const StatsProcessSlot& process);

        // Connection Queue
        int appendSlot(const StatsConnectionQueueSlot& connQueue);

        // ThreadPool Slot
        int appendSlot(const StatsThreadPoolBucket& threadPool);

        // Accumulated virtual server slot
        int appendSlot(const StatsAccumulatedVSSlot& accumulatedVSStats);

        // VirtualServer Slot
        int appendSlot(const StatsVirtualServerSlot& vss);

        // StatsHeader Slot
        int appendSlot(const StatsHeader& hdrSlot);

        // CpuInfo Slot
        int appendSlot(const StatsCpuInfoSlot& cps);

        // StatsProfileBucket
        int appendSlot(const StatsProfileBucket& profile);

        // StatsThreadSlot
        int appendSlot(const StatsThreadSlot& thread);

        // StatsListenSlot
        int appendSlot(const StatsListenSlot& ls);

        // StatsWebModuleSlot
        int appendSlot(const StatsWebModuleSlot& wms);

        // StatsWebModuleCacheSlot
        int appendSlot(const StatsWebModuleCacheSlot& wmCacheSlot);

        // StatsServletJSPSlot
        int appendSlot(const StatsServletJSPSlot& sjs);

        // StatsJdbcConnPoolSlot
        int appendSlot(const StatsJdbcConnPoolSlot& poolSlot);

        // StatsJvmManagementSlot
        int appendSlot(const StatsJvmManagementSlot& jvmMgmtSlot);

        // StatsSessionReplicationSlot
        int appendSlot(const StatsSessionReplicationSlot& sessReplSlot);
};



//-----------------------------------------------------------------------------
// StatsMsgBuff
//
// Utitlity class to read and write string arrays.
//-----------------------------------------------------------------------------

class StringArrayBuff : public StatsMsgBuff
{
    private:
        int nStringCount_;
        char** stringArray_;
        int countStringsInBuffer(void);
        PRBool setupStringArray(void);

        // Disallow assignment operator and copy constructor
        StringArrayBuff& operator=(const StringArrayBuff& input);
        StringArrayBuff(const StringArrayBuff& input);
    public:
        StringArrayBuff(int nCapacity = 0);
        ~StringArrayBuff(void);
        void addString(const char* str);
        void freeze(void);
        int getStringCount(void) const ;
        char** getStringArray(void) const ;
        PRBool reinitialize(const void* buffer, int bufLen);
};


//-----------------------------------------------------------------------------
// StatsBufferReader
//
// Utitlity class to read data from message buffer.
//-----------------------------------------------------------------------------

class StatsBufferReader
{
    private:
        int nCurIndex_; // Index where current data is being Read
        void* buffer_;
        int nSize_;

        // Disallow assignment operator and copy constructor
        StatsBufferReader& operator=(const StatsBufferReader& input);
        StatsBufferReader(const StatsBufferReader& input);

    public:

        StatsBufferReader(const void* buffer,
                          int bufLen, PRBool fCopy = PR_FALSE);
        void initialize(const void* buffer,
                        int bufLen, PRBool fCopy = PR_FALSE);
        const char* readString(void);
        PRBool readStrings(int countStrings, const char** stringBuffer);
        const void* readBuffer(int nLen);
        PRBool isMoreDataAvailable(void);
        PRBool readInt32(PRInt32& val);
        int length(void);
        int getRemainingBytesCount(void);
        const void* getBufferAddress(void) const;
        int readStringAndSlot(int nSlotSize, const char*& strName,
                              const void*& buffer);
};


//-----------------------------------------------------------------------------
// StatsManagerUtil
//
// Utitlity class to reduce the size of StatsManager.
//-----------------------------------------------------------------------------

class HTTPDAEMON_DLL StatsManagerUtil
{
public:

    // ----------------------- copy methods. These copies data from src
    // structure to destination structes. These typically don't copy the
    // underneath pointers if any in those structures.

    static PRBool copyProfileBucket(StatsProfileNode* dest,
                                    const StatsProfileNode* src);
    static PRBool copyWebModuleSlot(StatsWebModuleNode* dest,
                                    const StatsWebModuleNode* src);
    static PRBool copyServletSlot(StatsServletJSPNode* dest,
                                  const StatsServletJSPNode* src);
    static PRBool copyProfileBucketChain(StatsProfileNode* dest,
                                         const StatsProfileNode* src);
    static PRBool copyVirtualServerSlot(StatsVirtualServerNode* dest,
                                        const StatsVirtualServerNode* src);

    // ----------------------- Aggregate methods. These does the aggregation
    // from src to dest. Like copy method they ignore the pointers if any in
    // those structures.
    static PRBool aggregateRequestBucket(StatsRequestBucket* dest,
                                         StatsRequestBucket* src);
    static PRBool aggregateVirtualServerSlot(StatsVirtualServerNode* dest,
                                             StatsVirtualServerNode* src);
    static PRBool aggregateProfileBucket(StatsProfileNode* dest,
                                         const StatsProfileNode* src);
    static PRBool aggregateProfileBucketChain(StatsProfileNode* dest,
                                              const StatsProfileNode* src);
    static PRBool aggregateWebModuleSlot(StatsWebModuleNode* dest,
                                         const StatsWebModuleNode* src);
    static PRBool aggregateServletSlot(StatsServletJSPNode* dest,
                                       const StatsServletJSPNode* src);


    // ----------------------- Accumulation methods -------------
    // Replace for older macros from iwsstats.h (STATS_ACCUMELATE_xxx)

    static void accumulateKeepAlive(StatsKeepaliveBucket* sum,
                                    const StatsKeepaliveBucket* delta);
    static void accumulateCache(StatsCacheBucket* sum,
                                const StatsCacheBucket* delta);
    static void accumulateDNS(StatsDnsBucket* sum,
                              const StatsDnsBucket* delta);
    static void accumulateRequest(StatsRequestBucket* sum,
                                  const StatsRequestBucket* delta);
    static void accumulateProfile(StatsProfileBucket* sum,
                                  const StatsProfileBucket* delta);
    static void accumulateThreadPool(StatsThreadPoolBucket* sum,
                                     const StatsThreadPoolBucket* delta);
    static void accumulateStatsAvgBucket(StatsAvgBucket* sum,
                                         const StatsAvgBucket* delta);
    static
    void accumulateAccumulatedVSSlot(StatsAccumulatedVSSlot* sum,
                                     const StatsAccumulatedVSSlot* delta);
    static PRInt64 getSeconds(PRInt64 time);

    static PRFloat64 getResponseTime(StatsProfileBucket& profile);

    static StatsListenNode* allocListenSlot(const char* id,
                                            StatsProcessNode* proc);
    static
    StatsWebModuleNode* findWebModuleSlotInVSList(StatsVirtualServerNode* vss,
                                                  const char* webModuleName);
    static StatsWebModuleNode* getWebModuleSlot(StatsVirtualServerNode* vss,
                                                const char* webModuleName);
    static StatsWebModuleNode* getWebModuleSlot(const StatsHeaderNode* hdrInput,
                                                const char* vsId,
                                                const char* webModuleName);
    static PRBool isWebModuleBelongs(StatsVirtualServerNode* vss,
                                     const char* webModuleName);
    static const char* getWebModuleUriFromName(const char* webModuleName);
    static
    StatsVirtualServerNode* getVSSFromWebModuleName(StatsVirtualServerNode* vss,
                                                    const char* webModuleName);
    static StatsServletJSPNode* getServletSlot(StatsWebModuleNode* wms,
                                               const char* servletName);
    static
    StatsJdbcConnPoolNode* getJdbcConnPoolNode(StatsProcessNode* proc,
                                               const char* poolName,
                                               PRBool& fNewNode);
    static
    StatsSessReplInstanceNode*
    getSessReplInstanceNode(StatsSessionReplicationNode* sessReplNode,
                            const char* instanceId,
                            PRBool& fNewNode);
    static
    StatsWebAppSessionStoreNode*
    getWebAppSessStoreNode(StatsSessReplInstanceNode* instanceNode,
                           const char* storeId,
                           PRBool& fNewNode);
    static void resetVSSForEmptyNodes(StatsVirtualServerNode* vss);
};


//-----------------------------------------------------------------------------
//------------------------ Template functions ---------------------------------
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// statsCountNodes
//
// Count the number of nodes in nodePtr
//-----------------------------------------------------------------------------

template <class Node>
int statsCountNodes(const Node* nodePtr)
{
    int nCount = 0;
    while (nodePtr)
    {
        ++nCount;
        nodePtr = nodePtr->next;
    }
    return nCount;
}

//-----------------------------------------------------------------------------
// statsElementAt
//
// Return the Node at nPos th Element
//-----------------------------------------------------------------------------

template <class Node>
Node* statsElementAt(const Node* nodePtr, int nPos)
{
    int nCount = 0;
    while (nodePtr)
    {
        if (nCount == nPos)
            break;
        ++nCount;
        nodePtr = nodePtr->next;
    }
    return const_cast<Node*> (nodePtr);
}

//-----------------------------------------------------------------------------
// statsMakeListFromArray
//
// This template connects the array of Node (nCount size) by their next member.
// As a result nodePtr becomes the tail of the list.
//-----------------------------------------------------------------------------

template <class Node>
void statsMakeListFromArray(Node* nodePtr, int nCount)
{
    if ((nCount <=0) || (nodePtr == NULL))
        return;
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCount -1; ++nIndex)
    {
        nodePtr->next = (nodePtr + 1);
        nodePtr = nodePtr->next;
    }
    nodePtr->next = 0;
}


//-----------------------------------------------------------------------------
// statsFindNodeFromList
//
// Find a node beginning from nodePtr whose id matches with input id.
//-----------------------------------------------------------------------------

template <class Node, class Id>
Node* statsFindNodeFromList(const Node* nodePtr, Id id)
{
    while (nodePtr)
    {
        if (nodePtr->compareId(id) == PR_TRUE)
            return const_cast<Node*> (nodePtr);
        nodePtr = nodePtr->next;
    }
    return 0;
}


//-----------------------------------------------------------------------------
// statsFindNode
//
// Find a node from the first node of cont whose id matches with input id.
//-----------------------------------------------------------------------------

template <class Node, class Container, class Id>
Node* statsFindNode(const Container* cont, Id id)
{
    Node* nodePtr = 0;
    cont->getFirst(nodePtr);
    return statsFindNodeFromList<Node>(nodePtr, id);
}

//-----------------------------------------------------------------------------
// statsFindLastNode
//
// Find the pointer tho the last node of nodePtr.
//-----------------------------------------------------------------------------

template <class Node>
Node* statsFindLastNode(const Node* nodePtr)
{
    const Node* oldNodePtr = nodePtr;
    while (nodePtr)
    {
        oldNodePtr = nodePtr;
        nodePtr = nodePtr->next;
    }
    return const_cast<Node*> (oldNodePtr);
}


//-----------------------------------------------------------------------------
// statsAppendLast
//
// Append a node at the end to container cont. If there is no node then it sets
// the First node.
//-----------------------------------------------------------------------------

template <class Node, class Container>
void statsAppendLast(Container* cont, Node* newNode)
{
    Node* nodePtr = 0;
    cont->getFirst(nodePtr);
    if (nodePtr)
    {
        Node* nodePtrLast = statsFindLastNode(nodePtr);
        nodePtrLast->next = newNode;
    }
    else
    {
        cont->setFirst(newNode);
    }
    return;
}

//-----------------------------------------------------------------------------
// statsFreeNodeFromList
//-----------------------------------------------------------------------------

template <class Node, class Container>
void statsFreeNodeFromList(const Container* cont, Node* nodePtr,
                           Node* prevNode)
{
    if (nodePtr)
    {
        Node* nextNode = nodePtr->next;
        if (prevNode)
        {
            prevNode->next = nextNode;
        }
        else
        {
            (const_cast<Container*>(cont))->setFirst(nextNode);
        }
        delete nodePtr;
    }
}

//-----------------------------------------------------------------------------
// statsFreeNode
//
// statsFreeNode frees the node of container cont whose id matches with id.
//-----------------------------------------------------------------------------

template <class Node, class Container, class Id>
Node* statsFreeNode(const Container* cont, Id id)
{
    Node* prevNode = 0;
    Node* nodePtr = 0;
    cont->getFirst(nodePtr);
    while (nodePtr)
    {
        if (nodePtr->compareId(id) == PR_TRUE)
            break;
        prevNode = nodePtr;
        nodePtr = nodePtr->next;
    }
    statsFreeNodeFromList(cont, nodePtr, prevNode);
    return 0;
}

//-----------------------------------------------------------------------------
// statsFreeNodeList
//
// Free the list of nodes beginning with node.
//-----------------------------------------------------------------------------

template <class Node>
void statsFreeNodeList(Node* node)
{
    while (node)
    {
        Node* nextNodePtr = node->next;
        delete node;
        node = nextNodePtr;
    }
}

//-----------------------------------------------------------------------------
// statsTrimList
//
// Delete the node list begin with beginNode. Also adjust the next pointer
// accordingly if whole list is deleted then set the first node points to NULL.
//-----------------------------------------------------------------------------

template <class Node, class Container>
PRBool statsTrimList(Container* cont, Node* beginNode)
{
    Node* nodePtr = 0;
    cont->getFirst(nodePtr);
    Node* firstNode = nodePtr;
    Node* prevNode = 0;
    while (nodePtr)
    {
        if (nodePtr == beginNode)
            break;
        prevNode = nodePtr;
        nodePtr = nodePtr->next;
    }
    if (!nodePtr)
    {
        // no node found beginning with beginNode.
        return PR_FALSE;
    }
    statsFreeNodeList(nodePtr);
    if (prevNode)
    {
        // List after prev is deleted so set the termintion point here.
        prevNode->next = 0;
    }
    else
    {
        PR_ASSERT(firstNode == beginNode);
        // List is deleted from the first node itself so set the first node
        // itself to NULL.
        cont->setFirst((Node*) 0);
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// statsFreeEmptyNodes
//
// Delete the nodes from the list which are empty. Nodes must implement a
// method named isEmptyNode which must return PR_TRUE/PR_FALSE.
//
// The function returns the number of nodes it deletes.
//-----------------------------------------------------------------------------

template <class Node, class Container>
int statsFreeEmptyNodes(Container* cont,
                        Node* unusedNode = 0)
{
    Node* prevNode = NULL;
    Node* nodePtr = 0;
    cont->getFirst(nodePtr);
    int nCountDeletedNodes = 0;
    while (nodePtr)
    {
        if (nodePtr->isEmptyNode() == PR_TRUE)
        {
            statsFreeNodeFromList(cont, nodePtr, prevNode);
            if (prevNode == NULL)
            {
                // Now start from the first node as the deleted node was first
                // node.
                cont->getFirst(nodePtr);
                prevNode = NULL;
                ++nCountDeletedNodes;
            }
            else
            {
                nodePtr = prevNode->next;
                // no change in prevNode.
            }
        }
        else
        {
            prevNode = nodePtr;
            nodePtr = nodePtr->next;
        }
    }
    return nCountDeletedNodes;
}

//-----------------------------------------------------------------------------
// statsAssureNodes
//
// Assure the Exact nCount Nodes in Container cont. It allocates if the current
// numbers are less. It frees if the number of elements are more than nCount.
//-----------------------------------------------------------------------------

template <class Node, class Container>
void statsAssureNodes(const Node* unusedNode, const Container* cont, int nCount)
{
    Node* prevNode = 0;
    Node* nodePtr = 0;
    cont->getFirst(nodePtr);
    int nCurCount = statsCountNodes(nodePtr);
    int nDiffCount = nCount - nCurCount;

    if (nDiffCount == 0)
        return;
    if (nDiffCount > 0)
    {
        Node* nodePtrLast = statsFindLastNode(nodePtr);
        int nIndex = 0;
        for (nIndex = 0; nIndex < nDiffCount; ++nIndex)
        {
            Node* nodePtrNew = new Node;
            nodePtrNew->next = 0;
            if (! nodePtrLast)
            {
                (const_cast<Container*>(cont))->setFirst(nodePtrNew);
            }
            else
            {
                nodePtrLast->next = nodePtrNew;
            }
            nodePtrLast = nodePtrNew;
        }
        return;
    }
    else
    {
        // Free the extra nodes
        // Get the element at nth position. Retrieve the next member in list
        // and set the next to 0.
        Node* curNodePtr = statsElementAt(nodePtr, nCount);
        Node* nextNodePtr = curNodePtr->next;
        curNodePtr->next = 0;

        // Now Delete the list beginning with nextNodePtr.
        statsFreeNodeList(curNodePtr);
    }
    return;
}

//-----------------------------------------------------------------------------
// statsGetNodeListData
//
// Fills the buffer with node data for all the elements of the list beginning
// with node.
//-----------------------------------------------------------------------------

template <class Node>
PRBool statsGetNodeListData(const Node* node, StatsMsgBuff& msgbuff)
{
    while (node)
    {
        if (node->getNodeData(msgbuff) != PR_TRUE)
            return PR_FALSE;
        node = node->next;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
//-----------  StatsMsgBuff inline methods ------------------------------------
//-----------------------------------------------------------------------------

inline
int
StatsMsgBuff::appendData(const void* buffer, int bufLen)
{
    append((const char*) buffer, bufLen);
    return length();
}

inline
int
StatsMsgBuff::appendInteger(PRInt32 val, PRInt32 padValue)
{
    appendData(&val, sizeof(PRInt32));
    return appendData(&padValue, sizeof(PRInt32));
}

inline
int
StatsMsgBuff::appendSlot(const StatsProcessSlot& process)
{
    return appendData(&process, sizeof(process));
}

inline
int
StatsMsgBuff::appendSlot(const StatsConnectionQueueSlot& connQueue)
{
    return appendData(&connQueue, sizeof(connQueue));
}

inline
int
StatsMsgBuff::appendSlot(const StatsThreadPoolBucket& threadPool)
{
    return appendData(&threadPool, sizeof(threadPool));
}

inline
int
StatsMsgBuff::appendSlot(const StatsAccumulatedVSSlot& accumulatedVSStats)
{
    return appendData(&accumulatedVSStats, sizeof(accumulatedVSStats));
}

inline
int
StatsMsgBuff::appendSlot(const StatsVirtualServerSlot& vss)
{
    return appendData(&vss, sizeof(vss));
}

inline
int
StatsMsgBuff::appendSlot(const StatsHeader& hdrSlot)
{
    return appendData(&hdrSlot, sizeof(hdrSlot));
}

inline
int
StatsMsgBuff::appendSlot(const StatsCpuInfoSlot& cps)
{
    return appendData(&cps, sizeof(cps));
}

inline
int
StatsMsgBuff::appendSlot(const StatsProfileBucket& profile)
{
    return appendData(&profile, sizeof(profile));
}

inline
int
StatsMsgBuff::appendSlot(const StatsThreadSlot& thread)
{
    return appendData(&thread, sizeof(thread));
}

inline
int
StatsMsgBuff::appendSlot(const StatsListenSlot& ls)
{
    return appendData(&ls, sizeof(ls));
}

inline
int
StatsMsgBuff::appendSlot(const StatsWebModuleSlot& wms)
{
    return appendData(&wms, sizeof(wms));
}

inline
int
StatsMsgBuff::appendSlot(const StatsWebModuleCacheSlot& wmCacheSlot)
{
    return appendData(&wmCacheSlot, sizeof(wmCacheSlot));
}

inline
int
StatsMsgBuff::appendSlot(const StatsServletJSPSlot& sjs)
{
    return appendData(&sjs, sizeof(sjs));
}

inline
int
StatsMsgBuff::appendSlot(const StatsJdbcConnPoolSlot& poolSlot)
{
    return appendData(&poolSlot, sizeof(poolSlot));
}

inline
int
StatsMsgBuff::appendSlot(const StatsJvmManagementSlot& jvmMgmtSlot)
{
    return appendData(&jvmMgmtSlot, sizeof(jvmMgmtSlot));
}

inline
int
StatsMsgBuff::appendSlot(const StatsSessionReplicationSlot& sessReplSlot)
{
    return appendData(&sessReplSlot, sizeof(sessReplSlot));
}

//-----------------------------------------------------------------------------
//-----------  StringArrayBuff inline methods ---------------------------------
//-----------------------------------------------------------------------------


inline
int StringArrayBuff::getStringCount(void) const
{
    return nStringCount_;
}

inline
char** StringArrayBuff::getStringArray(void) const
{
    return stringArray_;
}

inline
int
StatsBufferReader::length(void)
{
    return nSize_;
}

inline
int
StatsBufferReader::getRemainingBytesCount(void)
{
    return (nSize_ - nCurIndex_);
}

inline
const void*
StatsBufferReader::getBufferAddress(void) const
{
    return buffer_;
}

#endif
