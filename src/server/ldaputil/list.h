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

#ifndef _LDAPU_LIST_H
#define _LDAPU_LIST_H

#include <stdio.h>
#include <netsite.h>

typedef struct ldapu_list_node {
    void *info;				/* pointer to the corresponding info */
    struct ldapu_list_node *next;	/* pointer to the next node */
    struct ldapu_list_node *prev;	/* pointer to the prev node */
} LDAPUListNode_t;

typedef struct ldapu_list {
    LDAPUListNode_t *head;
    LDAPUListNode_t *tail;
} LDAPUList_t;

typedef struct {
    FILE *fp;
    void *arg;
} LDAPUPrintInfo_t;

typedef void * (*LDAPUListNodeFn_t)(void *info, void *arg);

#ifdef __cplusplus
extern "C" {
#endif

NSAPI_PUBLIC int ldapu_list_add_node (LDAPUList_t *list, LDAPUListNode_t *node);
NSAPI_PUBLIC int ldapu_list_remove_node (LDAPUList_t *list, LDAPUListNode_t *node);
NSAPI_PUBLIC int ldapu_list_alloc (LDAPUList_t **list);
NSAPI_PUBLIC void ldapu_list_free (LDAPUList_t *list, LDAPUListNodeFn_t free_fn);
NSAPI_PUBLIC int ldapu_list_add_info (LDAPUList_t *list, void *info);
NSAPI_PUBLIC void ldapu_list_move (LDAPUList_t* from, LDAPUList_t* into);
NSAPI_PUBLIC int ldapu_list_copy (const LDAPUList_t *from, LDAPUList_t **to, LDAPUListNodeFn_t copy_fn);
NSAPI_PUBLIC int ldapu_list_find_node (const LDAPUList_t *list,
				 LDAPUListNode_t **found,
				 LDAPUListNodeFn_t find_fn,
				 void *find_arg);
NSAPI_PUBLIC int ldapu_list_print (LDAPUList_t *list, LDAPUListNodeFn_t print_fn, LDAPUPrintInfo_t *pinfo);
NSAPI_PUBLIC void *ldapu_list_empty (LDAPUList_t *list, LDAPUListNodeFn_t free_fn, void *arg);

#ifdef __cplusplus
}
#endif

#endif
