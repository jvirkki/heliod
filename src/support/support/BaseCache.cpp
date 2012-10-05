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

#include "BaseCache.h"
#include "prlog.h"
#include "prmem.h"

void
BaseCacheEntryWrapper::DecrementUsageCount(BaseCache& cache)
{
  PR_AtomicDecrement(&_usageCount);
  if ((DeletePending() == PR_TRUE) && (_usageCount == 0)) {
    cache.EnterCacheMonitor(); 
      if (_usageCount == 0) {
        cache._deleteEntry(this);
      } 
    cache.ExitCacheMonitor();
  }
}

BaseCache::BaseCache(CacheConfig& cfg) 
: _cfg(cfg), _monitor(0), _table(0), _mruListHead(0), 
  _valid(PR_TRUE), _terminating(PR_FALSE)
{
  _monitor = PR_NewMonitor();
  if (_monitor == 0) {
    _valid = PR_FALSE;
  }
  else {
    _table = (BaseCacheEntryWrapper **) PR_Calloc(_cfg._hashSize,
                                    (sizeof(BaseCacheEntryWrapper *)));

    if (_table == NULL) {
      _valid = PR_FALSE;
      PR_DestroyMonitor(_monitor);
    }
  }
}

BaseCache::~BaseCache()
{
  if (_valid == PR_TRUE) {
    _terminating = PR_TRUE;
    EnterCacheMonitor();
    if (_table) {
      BaseCacheEntryWrapper* curr = _mruListHead; 
      while (curr) {
        _deleteEntry(curr);
        curr = _mruListHead; 
      }
    }
    _mruListHead = 0;
    ExitCacheMonitor();

    PR_DestroyMonitor(_monitor);
  }
}

PRBool
BaseCache::Insert(BaseCacheEntryWrapper* ptr)
{
  PRBool res = PR_TRUE;

  if (!ptr) {
    res = PR_FALSE;
  }
  else {
    EnterCacheMonitor();
    res = _insert(ptr);
    ExitCacheMonitor();
  }

  return res;
}


PRBool
BaseCache::_insert(BaseCacheEntryWrapper* newEntry)
{
  PR_ASSERT(newEntry);

  PRBool res = PR_TRUE;

  if (_cfg._currSize >= _cfg._maxSize) {
    BaseCacheEntryWrapper* delPtr = _mruListHead->_mruPrev;
    res = delPtr->DeletePending(); 
    if (res == PR_FALSE)
      res = _deleteEntry(delPtr);
    if (res == PR_FALSE) {
      // Somebody had a reference count and we could not delete
      _stats._insertFails++;
    }
  }
  if (res == PR_TRUE) {
    BaseCacheEntryWrapper* existingEntry = _lookup(newEntry->GetKey());
    if (existingEntry) {
      // There is already an existing entry; fail insertion
      res = PR_FALSE;
      _stats._insertFails++;
    }
    else { 
      // Finally succeeded; insert
      PRUint32 bucket = newEntry->GetKeyHash() % _cfg._hashSize;
      newEntry->_next = _table[bucket];
      _table[bucket] = newEntry;
      _cfg._currSize++;
      InsertIntoMRUList(newEntry);
    }
  }


  return res;
}


PRBool
BaseCache::_deleteEntry(BaseCacheEntryWrapper* entry)
{
  PR_ASSERT(entry);
  PRBool res = PR_TRUE;
  if ((entry->DeletePending() == PR_TRUE) &&
      (entry->GetUsageCount() > 0)) {
    res =  PR_FALSE;
  }
  else {
    entry->SetDeletePending();
    res = entry->CanBeDeleted();
    if (res == PR_TRUE) {
      /* Delete from the hash table */
      PRUint32 bucket = entry->GetKeyHash() % _cfg._hashSize;
      // We can actually delete
      BaseCacheEntryWrapper* ptr = _table[bucket];
      BaseCacheEntryWrapper* prev = 0;
      while (ptr) {
        if (ptr == entry)
          break;
        else {
          prev = ptr;
          ptr = ptr->_next;
        }
      }
      // We must find the entry here; assert for it
      PR_ASSERT(ptr);
    
      if (prev) {
        prev->_next = ptr->_next;
      }
      else {
        _table[bucket] = ptr->_next;
      }

      // Remove from MRU list
      RemoveFromMRUList(ptr);

      PR_ASSERT(_cfg._currSize);
      _cfg._currSize--;
      delete ptr;
    }
  }

  return res;
}

PRBool
BaseCache::DeleteEntry(BaseCacheEntryWrapper* entry)
{
  PR_ASSERT(entry);

  EnterCacheMonitor();
  PRBool res = _deleteEntry(entry);
  ExitCacheMonitor();

  return res;
}


BaseCacheEntryWrapper*
BaseCache::Lookup(CacheKey &key)
{
  EnterCacheMonitor();
  BaseCacheEntryWrapper* res = _lookup(key);
  ExitCacheMonitor();

  return res;
}


BaseCacheEntryWrapper*
BaseCache::_lookup(CacheKey &key)
{
    PRUint32 bucket = key.Hash() % _cfg._hashSize;
    BaseCacheEntryWrapper* ptr;

    _stats._lookups++;

    /* Seach hash collision list */
    for (ptr = _table[bucket]; ptr; ptr = ptr->_next) {

        if ((ptr->DeletePending() == PR_FALSE)  &&
            (key.IsEqual(ptr->GetKey()) == PR_TRUE)) {

            /* Found matching entry */
            ptr->IncrementUsageCount();
            ptr->IncrementAccessCount();
            MoveToHeadOfMRU(ptr); 
            _stats._hits++;
            break;
        }
    }

    return ptr;
} 


void
BaseCache::InsertIntoMRUList(BaseCacheEntryWrapper* entry)
{
  if (!_mruListHead) {
    entry->_mruNext = entry;
    entry->_mruPrev = entry;
  }
  else {
    entry->_mruNext = _mruListHead;
    entry->_mruPrev = _mruListHead->_mruPrev;
    _mruListHead->_mruPrev->_mruNext = entry;
    _mruListHead->_mruPrev = entry;
  }
  
  _mruListHead = entry;
}

void
BaseCache::MoveToHeadOfMRU(BaseCacheEntryWrapper* entry)
{
  PR_ASSERT(entry);

  PR_ASSERT(_mruListHead);
  {
    // atleast one element in the list
    BaseCacheEntryWrapper* prevNode = entry->_mruPrev;
    BaseCacheEntryWrapper* nextNode = entry->_mruNext;

    prevNode->_mruNext = nextNode;
    nextNode->_mruPrev = prevNode;

    entry->_mruNext = _mruListHead;
    entry->_mruPrev = _mruListHead->_mruPrev;
    _mruListHead->_mruPrev->_mruNext = entry;
    _mruListHead->_mruPrev = entry;
  }

  _mruListHead = entry;

}


void
BaseCache::RemoveFromMRUList(BaseCacheEntryWrapper* entry)
{
  PR_ASSERT(entry);
  PR_ASSERT(_mruListHead);

  if ((entry->_mruPrev == entry) &&
      (entry->_mruNext == entry)) {
    // only element on list
    _mruListHead = 0;
  }
  else {
    BaseCacheEntryWrapper* prevNode = entry->_mruPrev;
    BaseCacheEntryWrapper* nextNode = entry->_mruNext;
  
    prevNode->_mruNext = nextNode;
    nextNode->_mruPrev = prevNode;

    if (entry == _mruListHead) {
      _mruListHead = nextNode;
    }
  }

  entry->_mruNext = 0;
  entry->_mruPrev = 0;
}


void
BaseCache::EnterCacheMonitor()
{
  PR_EnterMonitor(_monitor);
}

void
BaseCache::ExitCacheMonitor()
{
  PR_ExitMonitor(_monitor);
}
