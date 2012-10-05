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

/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#ifndef BASE_KEYWORD_H
#define BASE_KEYWORD_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * keyword.h: support for string-valued keyword to integer mappings
 * 
 */

#include "nspr.h"

#ifdef INTNSAPI

/*
 * NSKW_HASHNEXT implements a case insensitive hash algorithm optimized for
 * known HTTP methods and header field names.
 */
#define NSKW_HASHNEXT(hash, ch) { hash = ((hash) << 1) ^ ((ch) & 0x5f); }

PR_BEGIN_EXTERN_C

typedef struct NSKWSpace NSKWSpace;

PR_EXTERN(NSKWSpace *) NSKW_CreateNamespace(int resvcnt);
PR_EXTERN(int) NSKW_DefineKeyword(const char * const keyword,
                                  int index, NSKWSpace *ns);
PR_EXTERN(unsigned long) NSKW_HashKeyword(const char * const keyword);
PR_EXTERN(int) NSKW_LookupKeyword(const char * const keyword, int len,
                                  PRBool caseMatters,
                                  unsigned long hash, NSKWSpace *ns);
PR_EXTERN(int) NSKW_Optimize(NSKWSpace *ns);
PR_EXTERN(void) NSKW_DestroyNamespace(NSKWSpace *ns);

PR_END_EXTERN_C

#endif /* INTNSAPI */

#endif /* !BASE_KEYWORD_H */
