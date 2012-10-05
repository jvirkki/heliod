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

#ifndef _PLIST_H
#define _PLIST_H

#ifndef NOINTNSACL
#define INTNSACL
#endif /* !NOINTNSACL */

/*
 * TYPE:        PList_t
 *
 * DESCRIPTION:
 *
 *      This type defines a handle for a property list.
 */

#include "base/pool.h"

#ifndef PUBLIC_NSACL_PLISTDEF_H
#include "../public/nsacl/plistdef.h"
#endif /* !PUBLIC_NSACL_PLISTDEF_H */

#ifdef INTNSACL

/* Functions in plist.c */
NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC extern int PListAssignValue(PList_t plist, const char *pname,
                            const void *pvalue, PList_t ptype);
NSAPI_PUBLIC extern PList_t PListCreate(pool_handle_t *mempool,
                           int resvprop, int maxprop, int flags);
NSAPI_PUBLIC extern int PListDefProp(PList_t plist, int pindex, 
                        const char *pname, const int flags);
NSAPI_PUBLIC extern const void * PListDeleteProp(PList_t plist, int pindex, const char *pname);
NSAPI_PUBLIC extern int PListFindValue(PList_t plist,
                          const char *pname, void **pvalue, PList_t *type);
NSAPI_PUBLIC extern int PListInitProp(PList_t plist, int pindex, const char *pname,
                         const void *pvalue, PList_t ptype);
NSAPI_PUBLIC extern PList_t PListNew(pool_handle_t *mempool);
NSAPI_PUBLIC extern void PListDestroy(PList_t plist);
NSAPI_PUBLIC extern int PListGetValue(PList_t plist,
                         int pindex, void **pvalue, PList_t *type);
NSAPI_PUBLIC extern int PListNameProp(PList_t plist, int pindex, const char *pname);
NSAPI_PUBLIC extern int PListSetType(PList_t plist, int pindex, PList_t type);
NSAPI_PUBLIC extern int PListSetValue(PList_t plist,
                         int pindex, const void *pvalue, PList_t type);
NSAPI_PUBLIC extern void PListEnumerate(PList_t plist, PListFunc_t *user_func, 
                           void *user_data);
NSAPI_PUBLIC extern PList_t
PListDuplicate(PList_t plist, pool_handle_t *new_mempool, int flags);
NSAPI_PUBLIC extern pool_handle_t *PListGetPool(PList_t plist);
NSAPI_PUBLIC extern int PListDefKey(PList_t plist, pb_key *key, const char *pname, const int flags);
NSAPI_PUBLIC extern int PListInitKey(PList_t plist, pb_key *key, const void *pvalue, PList_t ptype);

NSPR_END_EXTERN_C

#endif /* INTNSACL */

#endif /* _PLIST_H */
