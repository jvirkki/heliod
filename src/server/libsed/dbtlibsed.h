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

#define LIBRARY_NAME "libsed"

static char dbtsedid[] = "$DBT: libsed referenced v1 $";

#include "i18n.h"

BEGIN_STR(libsed)
    ResDef(DBT_LibraryID_, -1, dbtsedid)
    ResDef(DBT_CGMES, 1, "command garbled: %s")
    ResDef(DBT_TMMES, 2, "too much text: %s")
    ResDef(DBT_LTLMES, 3, "label too long: %s")
    ResDef(DBT_ULMES, 4, "undefined label: %s")
    ResDef(DBT_DLMES, 5, "duplicate labels: %s")
    ResDef(DBT_TMLMES, 6, "too many labels: %s")
    ResDef(DBT_AD0MES, 7, "no addresses allowed: %s")
    ResDef(DBT_AD1MES, 8, "only one address allowed: %s")
    ResDef(DBT_TOOBIG, 9, "suffix too large: %s")
    ResDef(DBT_OOMMES, 10, "out of memory")
    ResDef(DBT_COPFMES, 11, "cannot open pattern file: %s")
    ResDef(DBT_COIFMES, 12, "cannot open input file: %s")
    ResDef(DBT_TMOMES, 13, "too many {'s")
    ResDef(DBT_TMCMES, 14, "too many }'s")
    ResDef(DBT_NRMES, 15, "first RE may not be null")
    ResDef(DBT_UCMES, 16, "unrecognized command: %s")
    ResDef(DBT_TMWFMES, 17, "too many files in w commands")
    ResDef(DBT_COMES, 18, "cannot open %s")
    ResDef(DBT_CCMES, 19, "cannot create %s")
    ResDef(DBT_TMLNMES, 20, "too many line numbers")
    ResDef(DBT_TMAMES, 21, "too many appends after line %lld")
    ResDef(DBT_TMRMES, 22, "too many reads after line %lld")
    ResDef(DBT_SMMES, 23, "Space missing before filename: %s")
    ResDef(DBT_DOORNG, 24, "``\\digit'' out of range: %s")
    ResDef(DBT_EDMOSUB, 25, "ending delimiter missing on substitution: %s")
    ResDef(DBT_EDMOSTR, 26, "ending delimiter missing on string: %s")
    ResDef(DBT_FNTL, 27, "file name too long: %s")
    ResDef(DBT_CLTL, 28 , "command line too long")
    ResDef(DBT_TSNTSS, 29, "transform strings not the same size: %s")
END_STR(libsed)
