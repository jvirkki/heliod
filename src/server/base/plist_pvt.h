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

#ifndef _PLIST_PVT_H
#define _PLIST_PVT_H

/*
 * FILE:        plist_pvt.h
 *
 * DESCRIPTION:
 *
 *      This file contains private definitions for the property list
 *      utility implementation.
 */

#include "base/pool.h"

/* Forward declarations */
typedef struct PLValueStruct_s PLValueStruct_t;
typedef struct PLSymbol_s PLSymbol_t;
typedef struct PLSymbolTable_s PLSymbolTable_t;
typedef struct PListStruct_s PListStruct_t;

/*
 * TYPE:        PLValueStruct_t
 *
 * DESCRIPTION:
 *
 *      This type represents a property value. It is dynamically
 *      allocated when a new property is added to a property list.
 *      It contains a reference to a property list that contains
 *      information about the property value, and a reference to
 *      the property value data.
 */

#ifndef PBLOCK_H
#include "base/pblock.h"
#endif /* PBLOCK_H */

struct PLValueStruct_s {
    pb_entry pv_pbentry;	/* used for pblock compatibility */
    pb_param pv_pbparam;	/* property name and value pointers */
    const pb_key *pv_pbkey;     /* property pb_key pointer (optional) */
    PLValueStruct_t *pv_next;   /* property name hash collision link */
    PListStruct_t *pv_type;     /* property value type reference */
    int pv_pi;                  /* property index */
    pool_handle_t *pv_mempool;  /* pool we were allocated from */
};

#define pv_name pv_pbparam.name
#define pv_value pv_pbparam.value

/* Offset to pv_pbparam in PLValueStruct_t */
#define PVPBOFFSET ((char *)&((PLValueStruct_t *)0)->pv_pbparam)

/* Convert pb_param pointer to PLValueStruct_t pointer */
#define PATOPV(p) ((PLValueStruct_t *)((char *)(p) - PVPBOFFSET))

/*
 * TYPE:        PLSymbolTable_t
 *
 * DESCRIPTION:
 *
 *      This type represents a symbol table that maps property names
 *      to properties.  It is dynamically allocated the first time a
 *      property is named.
 */

#define PLSTSIZES       {7, 19, 31, 67, 123, 257, 513}
#define PLMAXSIZENDX    (sizeof(plistHashSizes)/sizeof(plistHashSizes[0]) - 1)

struct PLSymbolTable_s {
    int pt_sizendx;             /* pt_hash size, as an index in PLSTSIZES */
    int pt_nsyms;               /* number of symbols in table */
    PLValueStruct_t *pt_hash[1];/* variable-length array */
};

/*
 * TYPE:        PListStruct_t
 *
 * DESCRIPTION:
 *
 *      This type represents the top-level of a property list structure.
 *      It is dynamically allocated when a property list is created, and
 *      freed when the property list is destroyed.  It references a
 *      dynamically allocated array of pointers to property value
 *      structures (PLValueStruct_t).
 */

#define PLIST_DEFSIZE   8       /* default initial entries in pl_ppval */
#define PLIST_DEFGROW   16      /* default incremental entries for pl_ppval */

struct PListStruct_s {
    pblock pl_pb;		/* pblock subset of property list head */
    PLSymbolTable_t *pl_symtab; /* property name to index symbol table */
    pool_handle_t *pl_mempool;  /* associated memory pool handle */
    int pl_maxprop;             /* maximum number of properties */
    int pl_resvpi;              /* number of reserved property indices */
    int pl_lastpi;              /* last allocated property index */
    int pl_cursize;             /* current size of pl_ppval in entries */
};

#define pl_initpi pl_pb.hsize    /* number of pl_ppval entries initialized */
#define pl_ppval pl_pb.ht	/* pointer to array of value pointers */

/* Convert pblock pointer to PListStruct_t pointer */
#define PBTOPL(p) ((PListStruct_t *)(p))

#define PLSIZENDX(i) (plistHashSizes[i])
#define PLHASHSIZE(i) (sizeof(PLSymbolTable_t) + \
                       (PLSIZENDX(i) - 1)*sizeof(PLValueStruct_t *))

extern int plistHashSizes[7];

extern unsigned int PListHash(const char *string);

extern int PListHashName(PLSymbolTable_t *symtab, const char *pname);

extern int PListGetFreeIndex(PListStruct_t *pl);

extern PLSymbolTable_t *PListSymbolTable(PListStruct_t *pl);

#endif /* _PLIST_PVT_H */
