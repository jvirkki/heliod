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

#ifndef _BASE_CACHE_H_
#define _BASE_CACHE_H_

#include "support_common.h"
#include "CacheConfig.h"
#include "CacheStats.h"
#include "prmon.h"
#include "prthread.h"
#include "pratom.h"


class CacheData;
class BaseCache;

class SUPPORT_EXPORT CacheKey {
public:

    virtual PRBool      IsEqual(const CacheKey &key) const = 0;
    virtual PRUint32    Hash()                       const = 0;

protected:
    CacheKey() {}
    ~CacheKey() {}
};

class SUPPORT_EXPORT BaseCacheEntryWrapper {
  public :
    BaseCacheEntryWrapper()
    : _deletePending(PR_FALSE), _accessCount(0), 
      _next(0), _prev(0), _mruNext(0), _mruPrev(0), _usageCount(0)
    {}
    virtual ~BaseCacheEntryWrapper() 
    {
       _next = 0;
       _prev = 0;
       _mruNext = 0;
       _mruPrev = 0;
    };

    virtual CacheKey     &GetKey()       const = 0;
    virtual PRUint32      GetKeyHash()   const = 0;

    void IncrementUsageCount() {PR_AtomicIncrement(&_usageCount);}
    void DecrementUsageCount(BaseCache& cache);
    PRInt32 GetUsageCount() { return _usageCount;}
    PRBool DeletePending() { return _deletePending;}
   
    friend class BaseCache; 

  private: 
    void SetDeletePending() { _deletePending = PR_TRUE;}
    void IncrementAccessCount() { _accessCount++;}

    PRBool CanBeDeleted() 
    { 
         return ((DeletePending() == PR_TRUE) && 
                 (GetUsageCount() == 0));
    }

    PRBool             _deletePending;
    PRInt32            _usageCount;
    PRInt32            _accessCount;

    BaseCacheEntryWrapper* _next;
    BaseCacheEntryWrapper* _prev;
    BaseCacheEntryWrapper* _mruNext;
    BaseCacheEntryWrapper* _mruPrev;

};

class SUPPORT_EXPORT BaseCache {
  public :
    BaseCache(CacheConfig& cfg);
    virtual ~BaseCache();

    PRBool                   Insert(BaseCacheEntryWrapper* ptr);
    PRBool                   DeleteEntry(BaseCacheEntryWrapper* entry);
    BaseCacheEntryWrapper   *Lookup(CacheKey &key);
    PRBool                   IsTerminating() { return _terminating; }
    PRMonitor               *GetMonitor() { return _monitor; }

    friend class BaseCacheEntryWrapper;

  private :

    PRBool _insert(BaseCacheEntryWrapper* newEntry);
    PRBool _deleteEntry(BaseCacheEntryWrapper* entry);
    BaseCacheEntryWrapper* _lookup(CacheKey &key);
    void   MoveToHeadOfMRU(BaseCacheEntryWrapper* entry);
    void   RemoveFromMRUList(BaseCacheEntryWrapper* entry);
    void   InsertIntoMRUList(BaseCacheEntryWrapper* entry);
    void EnterCacheMonitor();
    void ExitCacheMonitor();



    CacheConfig _cfg;
    CacheStatistics  _stats;

    BaseCacheEntryWrapper** _table;
    BaseCacheEntryWrapper* _mruListHead;

    PRBool             _valid;
    PRBool             _terminating;

    PRMonitor*         _monitor;
};

#endif /*_BASE_CACHE_H_ */
