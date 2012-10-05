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
#include "NSTokenizer.h"
#include <stdlib.h>

NSTokenizer::NSTokenizer(const NSString &copy_me, const char *_tokenCharacters)
  : contents(NULL, copy_me) 
{
  tokenCharacters = NULL;
  offset = 0;
  length = contents.length();
  if (_tokenCharacters) {
    tokenCharacters = strdup(_tokenCharacters);
  }
}

NSTokenizer::NSTokenizer(const char *copy_me, const char *_tokenCharacters) 
  : contents(NULL, copy_me, strlen(copy_me), 0, 0) 
{
  tokenCharacters = NULL;
  offset = 0;
  length = contents.length();
  if (_tokenCharacters) {
    tokenCharacters = strdup(_tokenCharacters);
  }
    
}
NSTokenizer::~NSTokenizer(void) {
  if (tokenCharacters) {
    free(tokenCharacters);
  }
  tokenCharacters = NULL;
}

const char *NSTokenizer::next(const char *ReqTokenCharacters) {
  if (offset >= length)
    return NULL;
  // use member token characters if none are specified
  const char *tokenChars 
    = (ReqTokenCharacters ? ReqTokenCharacters : tokenCharacters);
  const char *source = &(contents.operator[](offset));
  // if no token chars, return current segment, mark tokenizer as done.
  if (!tokenChars) {
    offset = contents.length();
  } else {
    size_t delta;
    // if we are currently on a token character, skip it.
    while ((offset <= length && (delta=strcspn(source, tokenChars))==0)) {
      source++;
      offset++;
    }
    offset += delta;
    if (offset > length) {
      return NULL;
    }
    if (offset < length) {
      contents.operator[](offset) = '\0';
	  offset++;
    }
  }
  return source;
}
