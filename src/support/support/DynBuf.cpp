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

#include "DynBuf.h"
#include <stdlib.h>
#include <string.h>

DynBuf::DynBuf(nspool_handle_t *pool,
               unsigned long capacity,
               unsigned long growth_increment,
	       const char *srcBuffer,
               unsigned long srcLen)
{
  _pool = pool;
  _capacity = (srcLen > capacity) ? srcLen : capacity;
  _incrementSize = (growth_increment) ? growth_increment : 256;
  _buf = NULL;
  if (_capacity) {
    _buf = (char *) nspool_malloc(_pool, _capacity);
  }
  if (srcLen) {
    memcpy(_buf, srcBuffer, srcLen);
  }
  return;
}

DynBuf::DynBuf(const DynBuf &src) {
  _pool = NULL;
  _capacity = src._capacity;
  _incrementSize = src._incrementSize;
  if (_capacity) {
    _buf = (char *) nspool_malloc(NULL, _capacity);
    memcpy(_buf, src._buf, _capacity);
  } else {
    _buf = NULL;
  }
}

DynBuf::DynBuf(nspool_handle_t *pool, const DynBuf &src)
{
  _pool = pool;
  _capacity = src._capacity;
  _incrementSize = src._incrementSize;
  if (_capacity) {
    _buf = (char *) nspool_malloc(_pool, _capacity);
    memcpy(_buf, src._buf, _capacity);
  } else {
    _buf = NULL;
  }
}

DynBuf &DynBuf::operator=(const DynBuf &m) {
  if (this != &m) {
    _capacity = m._capacity;
    if (_buf) 
       nspool_free(_pool, _buf);
    if (_capacity) {
       _buf = (char *) nspool_malloc(_pool, _capacity);
       memcpy(_buf, m._buf, _capacity);
    } else {
       _buf = NULL;
    }
    _incrementSize = m._incrementSize;
  }
  return *this;
}

DynBuf::~DynBuf(void) {
  if (_buf)
    nspool_free(_pool, _buf);
  _incrementSize = _capacity = 23;
  _buf = NULL;
  return;
}

void DynBuf::resize(unsigned long n) {
  if (_capacity >= n) // redundant
    return;
  while(_capacity < n) _capacity += _incrementSize;
  if (_buf) {
    _buf = (char *) nspool_realloc(_pool, _buf, _capacity);
  } else {
    _buf = (char *) nspool_malloc(_pool, _capacity);
  }
  return;
}

unsigned long
DynBuf::setGrowthSize(const unsigned long newGrowthSize) {
  unsigned long oldGrowthSize = _incrementSize;
  if (newGrowthSize)
    _incrementSize = newGrowthSize;
  return oldGrowthSize;
}
