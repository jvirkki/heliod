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

#define USE_NSPOOL
#include "NSString.h"
#include <ctype.h>
#include <nspr.h>

#ifdef TOLOWER
#undef TOLOWER
#endif
#define TOLOWER(x) ( (x >= 'A' && x <= 'Z') ? x | 0x20 : x)

#ifdef TOUPPER
#undef TOUPPER
#endif
#define TOUPPER(x) ( (x >= 'a' && x <= 'z') ? x & ~0x20 : x )

const unsigned long NSString::SMALL_STRING = 32;
const unsigned long NSString::MEDIUM_STRING = 80;
const unsigned long NSString::LARGE_STRING = 512;

const char* NSString::EmptyString = "";

NSString::NSString() : 
  _dBuf(NULL, 0, NSString::LARGE_STRING, NULL, 0), 
  _sBuf((char*)EmptyString),
  _sCapacity(0),
  _length(0), 
  _useStatic(1),
  _pool(NULL)
{
}

NSString::NSString(const char *copy_me, unsigned long len) :
  _dBuf(NULL, 0, NSString::LARGE_STRING, NULL, 0),
  _sBuf((char*)EmptyString),
  _sCapacity(0),
  _length(0), 
  _useStatic(1),
  _pool(NULL)
{
  if (copy_me) {
    if (!len)
      len = strlen(copy_me);
    setGrowthSize(len + 1);
    append(copy_me, len);
  }
}

NSString::NSString(nspool_handle_t* pool) : 
  _dBuf(pool, 0, NSString::LARGE_STRING, NULL, 0), 
  _sBuf((char*)EmptyString),
  _sCapacity(0),
  _length(0), 
  _useStatic(1),
  _pool(pool)
{
}


NSString::~NSString(void) {}

NSString::NSString(const NSString &src) :
  _dBuf(NULL, src._dBuf),
  _sBuf(src._sBuf),
  _length(src._length),
  _sCapacity(src._sCapacity),
  _useStatic(src._useStatic),
  _pool(NULL) 
{
  if (src._useStatic)
    forceDynamic();
}

NSString::NSString(nspool_handle_t *pool, const NSString &src) : 
  _dBuf(pool, src._dBuf),
  _sBuf(src._sBuf),
  _length(src._length), 
  _sCapacity(src._sCapacity),
  _useStatic(src._useStatic),
  _pool(pool)
{
  if (src._useStatic)
    forceDynamic();
}

NSString::NSString(nspool_handle_t *pool,
                   const char *copy_me, 
                   unsigned long len, 
		   unsigned long capacity, 
                   unsigned long growth_increment) : 
  _dBuf(pool, capacity, growth_increment, copy_me, len+1), 
  _sBuf((char *)EmptyString),
  _length(len), 
  _sCapacity(0), 
  _useStatic(0),
  _pool(pool)
{
}

NSString::NSString(nspool_handle_t *pool,
                   unsigned long capacity, 
                   unsigned long growth_size) :
  _dBuf(pool, capacity, growth_size, NULL, 0), 
  _sBuf((char *) EmptyString), 
  _length(0), 
  _sCapacity(0),
  _useStatic(0),
  _pool(pool)
{
  if (capacity == 0)
    _useStatic = 1; // set to the empty string.
}

void NSString::useStatic(char *buffer, unsigned long capacity, unsigned long len)
{
  _sCapacity = (capacity < len) ? len : capacity;
  _length = len;
  _sBuf = buffer;
  _useStatic = 1;
  if (_length == 0) {
    if (!_sCapacity) { 
      _sBuf = (char *) EmptyString;
    } else {
      _sBuf[0] = '\0';
    }
  }
}

NSString &NSString::operator=(const NSString &m) {
  if (this != &m) {
    _length = m._length;
    _sCapacity = m._sCapacity;
    _useStatic = m._useStatic;
    _sBuf = m._sBuf; // dangerous, but we'll do it...
    _dBuf = m._dBuf; // use assignment operator - causes a copy.
  }
  if (_useStatic)
    forceDynamic();
  return *this;
}

NSSTRING_EXPORT int operator==(const NSString &a, const NSString &b) {
  if (a._length != b._length) return 0;
  const char *A = (a._useStatic ? a._sBuf : (const char *) a._dBuf);
  const char *B = (b._useStatic ? b._sBuf : (const char *) b._dBuf);
  return (!memcmp(A, B, a._length));
}

void NSString::switchToDynamic(unsigned long ensureCap) {
  if (_useStatic) {
    if (ensureCap < _length +1 ) ensureCap = _length+1;
    _dBuf.ensureCapacity(ensureCap);
    if (_length) {
      memcpy((char *)_dBuf, _sBuf, _length+1);
    } else if (_dBuf.capacity()) {
      _dBuf[0UL] = '\0';
    }
    _useStatic = 0;
  }
}

void NSString::prepend(const char *s, unsigned long len) {
  ensureCapacity(_length+len+1);
  _length+=len;
  char *base = (char *) data();
  for (unsigned long i = _length; i >= len; i--) {
    base[i] = base[i - len];
  }
  memcpy(base, s, len);
  if (len == _length) {
	// this was the original fill.  Must null terminate.
	base[_length] = '\0';
  }
}

void NSString::toUpper(void) {
  if (!_length) return;
  char *start = (_useStatic ? _sBuf : (char *) _dBuf);
  for (unsigned int i = 0; i < _length; i++) 
    start[i] = TOUPPER(start[i]);
  return;
}

void NSString::toLower(void) {
  if (!_length)
    return;
  char *start = (_useStatic ? _sBuf : (char *) _dBuf);
  for (unsigned int i = 0; i < _length; i++)
    start[i] = TOLOWER(start[i]);
  return;
}

void NSString::mixCase(void) {
  if (!_length)
    return;
  char *start = (_useStatic ? _sBuf : (char *) _dBuf);
  start[0] = TOUPPER(start[0]);
  for (unsigned int i = 1; i < _length; i++)
    start[i] = TOLOWER(start[i]);
  return;
}

int NSString::last(char c) const {
  if (!_length)
    return -1;
  const char *beginString = (_useStatic ? _sBuf : (const char *) _dBuf);
  const char *endString = beginString + _length - 1; // do not look at the null
  for (int index = _length - 1; index >= 0; index--) {
    if (beginString[index] == c) return index;
  }
  return -1;
}

unsigned int NSString::hashMe(void) const {
  return hash(*this);
}

unsigned int NSString::replace(char original, char replacement) {
  unsigned int replacementCount = 0;
  char *buffer = (_useStatic ? _sBuf : (char *) _dBuf);
  for (unsigned long i = 0; i < _length; i++) {
    if (buffer[i] == original) {
      buffer[i] = replacement;
      replacementCount++;
    }
  }
  return replacementCount;
}

unsigned int NSString::hash(const NSString &in) {
  // provide a suitable hash key for rogue wave storage classes.
  unsigned hv = in._length;
  unsigned i  = in._length * (sizeof(char)/sizeof(unsigned));
  const unsigned *p = (const unsigned *) 
    (in._useStatic ? in._sBuf : (const char *) in._dBuf);
  {
    while (i--)
      hv = (*p++ ^ ((hv << 5) | (hv >> (8*sizeof(unsigned) -5))));
  }
  if (( i = in._length * sizeof(char) % sizeof(unsigned))) {
    unsigned h = 0;
    const char *c = (const char *) p;
    while (i--)
      h = ((h << 8*sizeof(char)) | *c++);
    hv = (h ^ ((hv << 5) | hv >> (8*sizeof(unsigned) -5)));
  }
  return hv;
}

void NSString::printv(const char *fmt, va_list args) {
  // this could be done much more efficiently
  char *formatted = PR_vsmprintf(fmt, args);
  if (formatted) {
    append(formatted);
    PR_smprintf_free(formatted);
  }
}

void NSString::printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printv(fmt, args);
  va_end(args);
}

unsigned long
NSString::setGrowthSize(const unsigned long newGrowthSize) {
  return _dBuf.setGrowthSize(newGrowthSize);
}

void 
NSString::strip(StripEnum where, char charToSkip)
{
    int stripBegin = ((where == LEADING)|| (where == BOTH)) ? 1 : 0;
    int stripTrailing = ((where == TRAILING)|| (where == BOTH)) ? 1 : 0;

    if (stripBegin) {
	const char* begin = data();
        const char* curr = data();
	int i = 0;
	while ((i < length()) && (*curr == charToSkip)) {
	    i++;
	    curr++;
        }
	if (curr != begin) {
	    int numBytes = curr - begin;
	    memmove((void*)begin, (void*)curr, length()-numBytes+1);
	    _length = _length - numBytes;
        }
    }

    if (stripTrailing && (length() > 0)) {
	const char* begin = data();
	const char* curr = data() + length() - 1;
	int i = 0;
	while ((curr != begin) && (*curr == charToSkip)) {
	    curr--;
	    i++;
        }
        if ((curr == begin) && (*curr == charToSkip)) {
	    _length = 0;
        }
	else {
	    _length = _length - i;
	    curr++;
        }
	*((char*)curr) = '\0';
    }
}


