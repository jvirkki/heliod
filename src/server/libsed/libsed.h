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
 *	Copyright (c) 1984 AT&T
 *	  All Rights Reserved  	
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

#ifndef LIBSED_H
#define LIBSED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#include "nspr.h"
#include "i18n.h"
#include "base/pool.h"
#include "base/util.h"
#include "dbtlibsed.h"

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#define SED_NLINES 256
#define SED_DEPTH 20
#define SED_LABSIZE 50
#define SED_ABUFSIZE 20

typedef struct sed_reptr_s sed_reptr_t;

struct sed_reptr_s {
    sed_reptr_t *next;
    char        *ad1;
    char        *ad2;
    char        *re1;
    sed_reptr_t *lb1;
    char        *rhs;
    int         findex;
    char        command;
    int         gfl;
    char        pfl;
    char        negfl;
    int         nrep;
};

typedef struct sed_label_s sed_label_t;

struct sed_label_s {
    char        asc[9];
    sed_reptr_t *chain;
    sed_reptr_t *address;
};

typedef void (sed_err_fn_t)(void *data, const char *fmt, va_list args);

typedef struct sed_commands_s sed_commands_t;
#define NWFILES 11 /* 10 plus one for standard output */

struct sed_commands_s {
    sed_err_fn_t *errfn;
    void         *data;

    unsigned     lsize;
    char         *linebuf;
    char         *lbend;
    const char   *saveq;

    char         *cp;
    char         *lastre;
    char         *respace;
    char         sseof;
    char         *reend;
    const char   *earg;
    int          eflag;
    int          gflag;
    int          nflag;
    PRInt64  tlno[SED_NLINES];
    int          nlno;
    int          depth;

    char         *fname[NWFILES];
    int          nfiles;

    sed_label_t  ltab[SED_LABSIZE];
    sed_label_t  *labtab;
    sed_label_t  *lab;
    sed_label_t  *labend;

    sed_reptr_t  **cmpend[SED_DEPTH];
    sed_reptr_t  *ptrspace;
    sed_reptr_t  *ptrend;
    sed_reptr_t  *rep;
    int          nrep;
    pool_handle_t   *pool;
};

typedef struct sed_eval_s sed_eval_t;

struct sed_eval_s {
    sed_err_fn_t   *errfn;
    void           *data;

    sed_commands_t *commands;

    PRInt64    lnum;
    void           *fout;

    unsigned       lsize;
    char           *linebuf;
    char           *lspend;

    unsigned       hsize;
    char           *holdbuf;
    char           *hspend;

    unsigned       gsize;
    char           *genbuf;
    char           *lcomend;

    PRFileDesc    *fcode[NWFILES];
    sed_reptr_t    *abuf[SED_ABUFSIZE];
    sed_reptr_t    **aptr;
    sed_reptr_t    *pending;
    unsigned char  *inar;
    int            nrep;

    int            dolflag;
    int            sflag;
    int            jflag;
    int            delflag;
    int            lreadyflag;
    int            quitflag;
    int            finalflag;
    int            numpass;
    int            nullmatch;
    int            col;
    pool_handle_t     *pool;
};

PRStatus sed_init_commands(sed_commands_t *commands, sed_err_fn_t *errfn, void *data,
                               pool_handle_t *p);
PRStatus sed_compile_string(sed_commands_t *commands, const char *s);
PRStatus sed_compile_file(sed_commands_t *commands, PRFileDesc *fin);
PRStatus sed_finalize_commands(sed_commands_t *commands);
void sed_destroy_commands(sed_commands_t *commands);

PRStatus sed_init_eval(sed_eval_t *eval, sed_commands_t *commands,
                           sed_err_fn_t *errfn, void *data, pool_handle_t *p);
PRStatus sed_reset_eval(sed_eval_t *eval, sed_commands_t *commands, sed_err_fn_t *errfn, void *data);
PRStatus sed_eval_buffer(sed_eval_t *eval, const char *buf, int bufsz, void *fout);
PRStatus sed_eval_file(sed_eval_t *eval, PRFileDesc *fin, void *fout);
PRStatus sed_finalize_eval(sed_eval_t *eval, void *f);
void sed_destroy_eval(sed_eval_t *eval);

#ifdef __cplusplus
}
#endif

#endif /* LIBSED_H */
