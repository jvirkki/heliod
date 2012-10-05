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

#ifndef _KEYWORD_PVT_H
#define _KEYWORD_PVT_H

#include "base/systems.h"
#include "base/pool.h"
#include "base/plist.h"
#include "base/keyword.h"

PR_BEGIN_EXTERN_C

#define NSKW_DEFAULT_ITSIZE  16    /* default size of a namespace */
#define NSKW_ITABLE_INC      8     /* increment for namespace size */
#define NSKW_OPTIMIZE_PASSES 100   /* how hard we try to optimize */

typedef struct NSKWEntry NSKWEntry;
struct NSKWEntry {
    NSKWEntry *next;
    const char *keyword;
    unsigned long hash;
    int index;
};

struct NSKWSpace {
    int nextkwi;               /* next keyword index to allocate */
    int itsize;                /* number of slots in itable */
    int htsize;                /* number of buckets in htable */
    NSKWEntry **itable;        /* index-to-keyword lookup array */
    NSKWEntry **htable;        /* keyword to index hash table */
};

PR_END_EXTERN_C

#endif /* _KEYWORD_PVT_H */
