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
 * Copyright (c) 2005, 2008 Sun Microsystems, Inc. All Rights Reserved.
 * Use is subject to license terms.
 *
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	
 *	  All Rights Reserved  	
 *
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0. 
 * 
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express 
 * or implied. 
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */

#ifndef _REGEXP_H
#define _REGEXP_H

#include "libsed.h"

#ifdef __cplusplus
extern "C" {
#endif

#define    CBRA    2
#define    CCHR    4
#define    CDOT    8
#define    CCL    12
#define    CXCL    16
#define    CDOL    20
#define    CCEOF    22
#define    CKET    24
#define    CBACK    36
#define    NCCL    40

#define    STAR    01
#define    RNGE    03

#define    NBRA    9

#define    PLACE(c)    ep[c >> 3] |= bittab[c & 07]
#define    ISTHERE(c)    (ep[c >> 3] & bittab[c & 07])

typedef struct _step_vars_storage {
    char    *loc1, *loc2, *locs;
    char    *braslist[NBRA];
    char    *braelist[NBRA];
    int    low;
    int    size;
} step_vars_storage;

typedef struct _sed_comp_args {
    int circf; /* Regular expression starts with ^ */
    int nbra; /* braces count */
} sed_comp_args;

extern char *sed_compile(sed_commands_t *commands, sed_comp_args *compargs,
                         char *ep, char *endbuf, int seof);
extern void command_errf(sed_commands_t *commands, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _REGEXP_H */
