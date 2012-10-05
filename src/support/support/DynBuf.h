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

#ifndef __DYNBUF_H__
#define __DYNBUF_H__

#ifdef WIN32
#include <windows.h>
#else
#include <stdio.h>
#endif
#include "nspool.h"

#ifdef WIN32
#ifdef BUILD_SUPPORT_DLL
#define NSSTRING_EXPORT __declspec(dllexport)
#else
#define NSSTRING_EXPORT __declspec(dllimport)
#endif
#else /* Unix */
#define NSSTRING_EXPORT
#endif

class NSSTRING_EXPORT DynBuf {
public:
  DynBuf(nspool_handle_t *pool,
         unsigned long capacity, 
	 unsigned long growth_increment, 
	 const char *srcBuffer, 
	 unsigned long srcLen);
  DynBuf(const DynBuf &copy_me);

  DynBuf(nspool_handle_t *pool,
         const DynBuf &copy_me);
  ~DynBuf();

  DynBuf &operator=(const DynBuf &m);

  inline char& operator[] (unsigned long i) { return _buf[i]; };
  inline operator char *() { return _buf; };
  inline operator const char *() const { return _buf; };
  inline unsigned long capacity() const { return _capacity; };
  inline void ensureCapacity(unsigned long n) { if (_capacity >= n) return; resize(n); }
  unsigned long setGrowthSize(const unsigned long newGrowthSize);

private:
  void resize(unsigned long n);
  nspool_handle_t *_pool;
  char *_buf;
  unsigned long _capacity;
  unsigned long _incrementSize;
};

#endif // __DYNBUF_H__
