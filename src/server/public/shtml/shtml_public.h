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

#ifndef _SHTML_PUBLIC_H_
#define _SHTML_PUBLIC_H_

#include "netsite.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TagUserData void*
typedef TagUserData (*ShtmlTagInstanceLoad)(const char* tag, 
                                            pblock*, const char*, size_t); 
typedef void (*ShtmlTagInstanceUnload)(TagUserData); 
typedef int (*ShtmlTagExecuteFunc)(pblock*, Session*, Request*, 
                                   TagUserData, TagUserData); 
typedef TagUserData (*ShtmlTagPageLoadFunc)(pblock* pb, Session*, Request*); 
typedef void (*ShtmlTagPageUnLoadFunc)(TagUserData);


NSAPI_PUBLIC int shtml_add_tag(const char* tag,
                               ShtmlTagInstanceLoad ctor,
                               ShtmlTagInstanceUnload dtor,
                               ShtmlTagExecuteFunc execFn, 
                               ShtmlTagPageLoadFunc pageLoadFn,
                               ShtmlTagPageUnLoadFunc pageUnLoadFn);

NSAPI_PUBLIC int shtml_add_html_tag(const char* tag,
                               ShtmlTagInstanceLoad ctor,
                               ShtmlTagInstanceUnload dtor,
                               ShtmlTagExecuteFunc execFn, 
                               ShtmlTagPageLoadFunc pageLoadFn,
                               ShtmlTagPageUnLoadFunc pageUnLoadFn);

#ifdef __cplusplus
}
#endif

#endif /* _SHTML_PUBLIC_H_ */
