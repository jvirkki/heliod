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

#ifndef _NS_TAGHANDLERS_H_
#define _NS_TAGHANDLERS_H_

#include "shtml_public.h"

extern "C"
{
  TagUserData NSEchoCtor(const char* tag, pblock* pb, const char* buff, size_t len);
  TagUserData NSTextCtor(const char* tag, pblock* pb, const char* buff, size_t len);
  TagUserData NSIncludeCtor(const char* tag, 
                            pblock* pb, const char* buff, size_t len);
  TagUserData NSFileInfoCtor(const char* tag, 
                             pblock* pb, const char* buff, size_t len);
  TagUserData NSHeaderCtor(const char* tag,
                           pblock* pb, const char* buff, size_t len);
  TagUserData NSRedirectCtor(const char* tag,
                           pblock* pb, const char* buff, size_t len);
  TagUserData NSExecCtor(const char* tag,
                       pblock* pb, const char* buff, size_t len);
  TagUserData NSDefineCtor(const char* tag,
                       pblock* pb, const char* buff, size_t len);
  TagUserData NSConfigCtor(const char* tag,
                       pblock* pb, const char* buff, size_t len);
  TagUserData NSPrintenvCtor(const char* tag,
                       pblock* pb, const char* buff, size_t len);
  TagUserData NSServletCtor(const char* tag,
                       pblock* pb, const char* buff, size_t len);

  void NSDtor(void* data);
  int  NSHandler(pblock* pb, Session* sn, Request* rq, TagUserData p, TagUserData pageLoadData);

  TagUserData  NSPageLoadHandler(pblock* pb, Session* sn, Request* rq);
  void  NSPageUnLoadHandler(TagUserData pageLoadData);
};

#endif /* _NS_TAGHANDLERS_H_ */
