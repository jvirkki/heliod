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

#ifndef PUBLIC_NSACL_PLISTDEF_H
#define PUBLIC_NSACL_PLISTDEF_H

/*
 * File:        plistdef.h
 *
 * Description:
 *
 *      This file defines the interface to property lists.  Property
 *      lists are a generalization of parameter blocks (pblocks).
 */

#ifndef PUBLIC_NSAPI_H
#include "nsapi.h"
#endif /* !PUBLIC_NSAPI_H */

typedef struct PListStruct_s *PList_t;

/* Define error codes returned from property list routines */

#define ERRPLINVPI      -1      /* invalid property index */
#define ERRPLEXIST      -2      /* property already exists */
#define ERRPLFULL       -3      /* property list is full */
#define ERRPLNOMEM      -4      /* insufficient dynamic memory */
#define ERRPLUNDEF      -5      /* undefined property name */

#define PLFLG_OLD_MPOOL	0	/* use the plist memory pool */
#define PLFLG_NEW_MPOOL	1	/* use the input memory pool */
#define PLFLG_IGN_RES	2	/* ignore the reserved properties */
#define PLFLG_USE_RES	3	/* use the reserved properties */

#ifdef __cplusplus
typedef void (PListFunc_t)(char*, const void*, void*);
#else
typedef void (PListFunc_t)();
#endif

#ifndef INTNSACL
#define PListAssignValue (*__nsacl_table->f_PListAssignValue)
#define PListCreate (*__nsacl_table->f_PListCreate)
#define PListDefProp (*__nsacl_table->f_PListDefProp)
#define PListDeleteProp (*__nsacl_table->f_PListDeleteProp)
#define PListFindValue (*__nsacl_table->f_PListFindValue)
#define PListInitProp (*__nsacl_table->f_PListInitProp)
#define PListNew (*__nsacl_table->f_PListNew)
#define PListDestroy (*__nsacl_table->f_PListDestroy)
#define PListGetValue (*__nsacl_table->f_PListGetValue)
#define PListNameProp (*__nsacl_table->f_PListNameProp)
#define PListSetType (*__nsacl_table->f_PListSetType)
#define PListSetValue (*__nsacl_table->f_PListSetValue)
#define PListEnumerate (*__nsacl_table->f_PListEnumerate)
#define PListDuplicate (*__nsacl_table->f_PListDuplicate)
#define PListGetPool (*__nsacl_table->f_PListGetPool)

#endif /* !INTNSACL */

#endif /* !PUBLIC_NSACL_PLISTDEF_H */
