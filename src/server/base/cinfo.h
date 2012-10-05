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

#ifndef BASE_CINFO_H
#define BASE_CINFO_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * cinfo.h: Content Information for a file, i.e. its type, etc.
 * 
 * See cinfo.c for dependency information. 
 * 
 * Rob McCool
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

/* ------------------------------ Constants ------------------------------- */

/* The hash function for the database. Hashed on extension. */
#include <ctype.h>
#define CINFO_HASH(s) (isalpha(s[0]) ? tolower(s[0]) - 'a' : 26)

/* The hash table size for that function */
#define CINFO_HASHSIZE 27

/* --- Begin function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC void INTcinfo_init(void);
NSAPI_PUBLIC void INTcinfo_terminate(void);
NSAPI_PUBLIC char *INTcinfo_merge(char *fn);
NSAPI_PUBLIC cinfo *INTcinfo_find(char *uri);
NSAPI_PUBLIC cinfo *INTcinfo_lookup(char *type);
NSAPI_PUBLIC void INTcinfo_dump_database(FILE *dump);
NSAPI_PUBLIC const char* cinfo_find_ext_type(const char *ext);

NSPR_END_EXTERN_C

/* --- End function prototypes --- */

#define cinfo_init INTcinfo_init
#define cinfo_terminate INTcinfo_terminate
#define cinfo_merge INTcinfo_merge
#define cinfo_find INTcinfo_find
#define cinfo_lookup INTcinfo_lookup
#define cinfo_dump_database INTcinfo_dump_database

#endif /* INTNSAPI */

#endif /* !BASE_CINFO_H */

