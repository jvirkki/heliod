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

#ifndef _SHTML_PAGEFNLIST_H_
#define _SHTML_PAGEFNLIST_H_

#include "shtml_public.h"
#include "base/pool.h"

struct ShtmlPageFnData {
  char*                tag;
  ShtmlTagPageLoadFunc loadFn;
  ShtmlTagPageUnLoadFunc unLoadFn;
  int                    pageLoadDataIndex;
};

#define DEFAULT_SIZE 5
#define DEFAULT_INCREMENT 5
class ShtmlPageFnList {
  public :
    ShtmlPageFnList(int size = DEFAULT_SIZE, 
                    int increment = DEFAULT_INCREMENT);
    ~ShtmlPageFnList();
    int NumPageDataSlotsRequired();
    int Find(const char* tag);
    int Find(ShtmlTagPageLoadFunc pageLoadFn);
    int Insert(const char* tag, 
               ShtmlTagPageLoadFunc loadFn, 
               ShtmlTagPageUnLoadFunc unLoadFn);
    const ShtmlPageFnData& GetElement(int i);
    int Length() { return _currSize;}


  private :
    ShtmlPageFnData* _array;
    int              _maxSize;
    int              _increment;
    int              _currSize;
    int              _currPageDataSlotIndex;
    int              _numPageDataSlots;
};



inline ShtmlPageFnList::ShtmlPageFnList(int size, int increment)
: _maxSize(size <= 0 ? DEFAULT_SIZE : size), _currSize(0),
  _numPageDataSlots(0), _array(0), 
  _increment(increment <= 0 ? DEFAULT_INCREMENT : increment),
  _currPageDataSlotIndex(0)
{
  _array = (ShtmlPageFnData*) PERM_CALLOC(sizeof(ShtmlPageFnData)*size);
}

inline ShtmlPageFnList::~ShtmlPageFnList()
{
  for (int i=0; i < _currSize; i++) {
    PERM_FREE(_array[i].tag);  
  }
  if (_array) {
    PERM_FREE(_array); 
  }
}

inline int 
ShtmlPageFnList::NumPageDataSlotsRequired() 
{
  return _numPageDataSlots;
}

inline int 
ShtmlPageFnList::Find(const char* tag)
{
  int res = -1;
  if (_array) {
    for (int i = 0; i < _currSize; i++) {
      if (strcmp(_array[i].tag, tag) == 0) {
        res = i;
        break;
      }
    }
  }

  return res;
}

inline int 
ShtmlPageFnList::Find(ShtmlTagPageLoadFunc fn)
{
  int res = -1;
  if (_array) {
    for (int i = 0; i < _currSize; i++) {
      if (_array[i].loadFn == fn) {
        res = i;
        break;
      }
    }
  }

  return res;
}



inline int 
ShtmlPageFnList::Insert(const char* tag, 
                        ShtmlTagPageLoadFunc loadFn, 
                        ShtmlTagPageUnLoadFunc unLoadFn)
{
  int res = _currSize;
  if (_currSize == _maxSize) {
    _maxSize += _increment;
    _array = (ShtmlPageFnData*) PERM_REALLOC(_array, _maxSize);
  }
  _array[_currSize].tag = PERM_STRDUP(tag);
  _array[_currSize].loadFn = loadFn;
  _array[_currSize].unLoadFn = unLoadFn;
  if (loadFn) {
    _numPageDataSlots++;
    _array[_currSize].pageLoadDataIndex = _currPageDataSlotIndex;
    _currPageDataSlotIndex++;
  }
  _currSize++;

  return res;
}

inline const ShtmlPageFnData& 
ShtmlPageFnList::GetElement(int i)
{
  PR_ASSERT(i>=0);
  PR_ASSERT(i < _currSize);
  PR_ASSERT(_array);
  return _array[i];
}

#endif /* _SHTML_PAGEFNLIST_H_ */
