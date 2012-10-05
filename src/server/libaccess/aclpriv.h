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

/********************************************************************

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 1996 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

*********************************************************************/

/*
 * ACL private data structure definitions 
 */

#ifndef ACL_PARSER_HEADER
#define ACL_PARSER_HEADER

#include <netsite.h>
#include "plhash.h"
#include <base/pool.h>
#include <base/plist.h>

#define ACL_TERM_BSIZE		4
#define ACL_FALSE_IDX   	-2
#define ACL_TRUE_IDX    	-1
#define ACL_MIN_IDX		0
#define ACL_EXPR_STACK		1024
#define ACL_TABLE_THRESHOLD	10
#define ACL_MAX_RIGHT_LEN       64

typedef enum { ACL_EXPR_OP_AND, ACL_EXPR_OP_OR, ACL_EXPR_OP_NOT } ACLExprOp_t;

typedef struct ACLExprEntry {
	char			*attr_name;	/* LAS name input */
	CmpOp_t			comparator;	/* LAS comparator input */
	char			*attr_pattern;	/* LAS attribute input */
	int			false_idx; 	/* index, -1 true, -2 false */
	int			true_idx;	/* index, -1 true, -2 false */
	int			start_flag;	/* marks start of an expr */
	void			*las_cookie; 	/* private data store for LAS */
	LASEvalFunc_t		las_eval_func; 	/* LAS function */
} ACLExprEntry_t;

typedef struct ACLExprRaw {
	char			*attr_name;	/* expr lval */
	CmpOp_t			comparator;	/* comparator */
	char			*attr_pattern;	/* expr rval */
	ACLExprOp_t		logical;	/* logical operator */
} ACLExprRaw_t;

typedef struct ACLExprStack {
	char	 		*expr_text[ACL_EXPR_STACK];
	ACLExprRaw_t 		*expr[ACL_EXPR_STACK];
	int			stack_index;
	int			found_subexpression;
	int			last_subexpression;
} ACLExprStack_t;

struct ACLExprHandle {
	char			*expr_tag;
	char			*acl_tag;
	int			expr_number;
	ACLExprType_t		expr_type;
	int			expr_flags;
	int			expr_argc;
	char			**expr_argv; 
	PList_t			expr_auth;
	ACLExprEntry_t 		*expr_arry;
	int			expr_arry_size;
	int			expr_term_index;
	ACLExprRaw_t 		*expr_raw;
	int			expr_raw_index;
	int			expr_raw_size;
	struct ACLExprHandle 	*expr_next;	/* Null-terminated */
};

struct ACLHandle {
	int			ref_count;
	char			*tag;
	PFlags_t		flags;
	char			*las_name;
	pblock			*pb;
	char			**attr_name;
        int			expr_count;
	ACLExprHandle_t		*expr_list_head;	/* Null-terminated */
	ACLExprHandle_t		*expr_list_tail;
};

typedef struct ACLWrapper {
	ACLHandle_t		*acl;
	struct ACLWrapper 	*wrap_next;
} ACLWrapper_t;

#define ACL_LIST_STALE	0x1
#define	ACL_LIST_IS_STALE(x)	((x)->flags & ACL_LIST_STALE)

struct ACLListHandle {
	ACLWrapper_t		*acl_list_head;	/* Null-terminated */
	ACLWrapper_t		*acl_list_tail;	/* Null-terminated */
	int			acl_count;
        void			*acl_sym_table;
	void			*cache;	
	uint32			flags;
	int			ref_count;
};

typedef	struct	ACLAceNumEntry {
	int			acenum;
	struct ACLAceNumEntry	*next;
	struct ACLAceNumEntry	*chain;		/* only used for freeing memory */
} ACLAceNumEntry_t;

typedef struct ACLAceEntry {
	ACLExprHandle_t		*acep;
				/* Array of auth block ptrs for all the expr 
				   clauses in this ACE */
	PList_t			*autharray;	
				/* PList with auth blocks for ALL attributes */
	PList_t			global_auth;    
	struct ACLAceEntry	*next;		/* Null-terminated list	*/
} ACLAceEntry_t;

typedef struct PropList PropList_t;

struct ACLEvalHandle {
	pool_handle_t		*pool;
	ACLListHandle_t		*acllist;
	PList_t			subject;
	PList_t			resource;
	int                     default_result;
};

typedef	struct ACLListCache {
/* Hash table for all access rights used in all acls in this list.  Each
 * hash entry has a list of ACE numbers that relate to this referenced
 * access right.  
 */
	PRHashTable		*Table;			
	char			*deny_response;
	char			*deny_type;
	ACLAceEntry_t		*acelist;	/* Evaluation order 
                                                 * list of all ACEs 
						 */
	ACLAceNumEntry_t	*chain_head;	/* Chain of all Ace num 
                                                 * entries for this
                                                 * ACL list so we can free them 
                                                 */
	ACLAceNumEntry_t	*chain_tail;
} ACLListCache_t;

/* this is to speed up acl_to_str_append */
typedef struct acl_string_s {
	char * str;
	long str_size;
	long str_len;
} acl_string_t;


struct ACLAttrGetter {
	PRCList			list;	/* must be first */
	ACLMethod_t		method;
	ACLDbType_t		dbtype;
	ACLAttrGetterFn_t	fn;
	void			*arg;
};

NSPR_BEGIN_EXTERN_C
// local prototypes

extern int evalComparator(CmpOp_t ctok, int result);
extern void makelower(char *string);
extern int EvalToRes(int value);
extern NSAPI_PUBLIC const char *comparator_string (int comparator);
extern const char *comparator_string_sym(int comparator);
extern char *acl_next_token (char **ptr, char delim);
extern const char *acl_next_token_len (const char *ptr, char delim, int *len);
extern char * acl_attrlist_match(const char * target, const char * list);

extern int ACL_EvalDestroyContext ( ACLListCache_t *cache );
extern const char *acl_get_servername(PList_t resource);
extern char *acl_comp_string( CmpOp_t cmp );
extern int acl_to_str_append(acl_string_t * p_aclstr, const char *str2);
extern int acl_registered_names(PRHashTable *ht, int count, char ***names);
extern void ACL_Destroy(void);

extern void *ACL_PermAllocTable(void *pool, PRSize size);
extern void ACL_PermFreeTable(void *pool, void *item);
extern PRHashEntry *ACL_PermAllocEntry(void *pool, const void *unused);
extern void ACL_PermFreeEntry(void *pool, PRHashEntry *he, uintn flag);

extern PRHashAllocOps ACLPermAllocOps;
extern PRHashNumber ACLPR_HashCaseString(const void *key);
extern int ACLPR_CompareCaseStrings(const void *v1, const void *v2);
extern char * acl_get_name(PRHashTable *ht, void * num);

NSPR_END_EXTERN_C

#endif
