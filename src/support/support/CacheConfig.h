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

#ifndef _CACHE_CONFIG_H_
#define _CACHE_CONFIG_H_

#include <prtypes.h>
#include <prinrval.h>

#include "support_common.h"

#define DEFAULT_CACHE_MIN_SIZE 1024
#define DEFAULT_CACHE_MAX_SIZE 4096

class SUPPORT_EXPORT CacheConfig {
  public :
    CacheConfig(PRInt32 hashSize = DEFAULT_CACHE_MIN_SIZE,
                PRInt32 maxSize = DEFAULT_CACHE_MAX_SIZE)
    : _hashSize(hashSize), _maxSize(maxSize), _currSize(0) 
    {};
    ~CacheConfig() {};
    PRInt32 GetMaxSize() { return _maxSize; }
    PRInt32 GetCurrSize() { return _currSize; }

    friend class BaseCache;

  private :
    PRInt32 _hashSize;
    PRInt32 _maxSize;
    PRInt32 _currSize;
    PRIntervalTime _reapInterval;
};

#endif /* _CACHE_CONFIG_H_ */
