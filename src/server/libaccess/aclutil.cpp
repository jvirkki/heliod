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

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 1996 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

*/

#include <ldap.h>
#include <netsite.h>
#include <string.h>
#include <base/crit.h>
#include <base/pool.h>
#include <base/util.h>

#include "plhash.h"
#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include "aclpriv.h"
#include <libaccess/nserror.h>
#include <frame/http_ext.h>


// hashtable enumeration utility struct for acl_get_name() use
typedef struct acl_search_info_t {
    void * number;
    char * name;
} acl_search_info;


//
// ACLPermAllocOps - allocation ops passed to the PLHash routines for pool allocation
//

void *
ACL_PermAllocTable(void *pool, PRSize size)
{
    return pool_malloc((pool_handle_t *)pool, size);
}

void
ACL_PermFreeTable(void *pool, void *item)
{
    pool_free((pool_handle_t *)pool, item);
}

PRHashEntry *
ACL_PermAllocEntry(void *pool, const void *unused)
{
    return ((PRHashEntry *)pool_malloc((pool_handle_t *)pool, sizeof(PRHashEntry)));
}

void
ACL_PermFreeEntry(void *pool, PRHashEntry *he, uintn flag)
{
    if (flag == HT_FREE_ENTRY)
	pool_free((pool_handle_t *)pool, he);
}

PRHashAllocOps ACLPermAllocOps = {
    ACL_PermAllocTable,
    ACL_PermFreeTable,
    ACL_PermAllocEntry,
    ACL_PermFreeEntry
};

//
// other miscellaneous helper functions for PLHash etc.
//

PRHashNumber
ACLPR_HashCaseString(const void *key)
{
    PRHashNumber h;
    const unsigned char *s;
 
    h = 0;
    for (s = (const unsigned char *)key; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ tolower(*s);
    return h;
}
 
int
ACLPR_CompareCaseStrings(const void *v1, const void *v2)
{
    const char *s1 = (const char *)v1;
    const char *s2 = (const char *)v2;

#ifdef XP_WIN32
    return (util_strcasecmp(s1, s2) == 0);
#else
    return (strcasecmp(s1, s2) == 0);
#endif
}
static CRITICAL acl_hash_crit = NULL;	/* Controls Global Hash */

/* 	Only used in ASSERT statements to verify that we have the lock 	 */
int
ACL_CritHeld(void)
{
    return (crit_owner_is_me(acl_hash_crit));
}

NSAPI_PUBLIC void
ACL_CritEnter(void)
{
    crit_enter(acl_hash_crit);
}

NSAPI_PUBLIC void 
ACL_CritExit(void)
{
    crit_exit(acl_hash_crit);
}

void
ACL_CritInit(void)
{
    acl_hash_crit = crit_init();
}

/*
 * The following routines are used to validate input parameters.  They always
 * return 1, or cause an NS_ASSERT failure.  The proper way to use them is 
 * with an NS_ASSERT in the calling function.  E.g.
 *	NS_ASSERT(ACL_AssertAcllist(acllist));
 */
int
ACL_AssertAcllist(ACLListHandle_t *acllist)
{
    ACLWrapper_t *wrap;

    if (acllist == ACL_LIST_NO_ACLS) return 1;
    NS_ASSERT(acllist);
    NS_ASSERT(acllist->acl_list_head);
    NS_ASSERT(acllist->acl_list_tail);
    NS_ASSERT(acllist->acl_count);
    NS_ASSERT(acllist->ref_count > 0);

    for (wrap=acllist->acl_list_head; wrap; wrap=wrap->wrap_next) {
	NS_ASSERT(ACL_AssertAcl(wrap->acl));
    }

    /* Artificially limit ACL lists to 10 ACLs for now */
    // NS_ASSERT(acllist->acl_count < 10);

    return 1;
}

int
ACL_AssertAcl(ACLHandle_t *acl)
{
    NS_ASSERT(acl);
    NS_ASSERT(acl->ref_count);
    NS_ASSERT(acl->expr_count);
    NS_ASSERT(acl->expr_list_head);
    NS_ASSERT(acl->expr_list_tail);

    return 1;
}

typedef struct HashEnumArg_s {
    char **names;
    int count;
} HashEnumArg_t;

typedef HashEnumArg_t *HashEnumArg_p;

static int acl_hash_enumerator (PRHashEntry *he, PRIntn i, void *arg)
{
    HashEnumArg_t *info = (HashEnumArg_t *)arg;
    char **names = info->names;

    names[info->count++] = STRDUP((const char *)he->key);

    return names[info->count-1] ? 0 : -1;
}

int acl_registered_names(PRHashTable *ht, int count, char ***names)
{
    HashEnumArg_t arg;
    int rv;

    if (count == 0 || names == 0) {
        if (names) *names = 0;
	return 0;
    }

    arg.names = (char **)MALLOC(count * sizeof(char *));
    arg.count = 0;

    if (!arg.names) return -1;

    rv = PR_HashTableEnumerateEntries(ht, acl_hash_enumerator, &arg);

    if (rv >= 0) {
	/* success */
	*names = arg.names;
    }
    else {
	*names = 0;
    }

    return rv;
}


/*      Generic evaluator of comparison operators in attribute evaluation
 *      statements.
 *      INPUT
 *              CmpOp_t ACL_TOKEN_EQ, ACL_TOKEN_NE etc.
 *              result          0 if equal, >0 if real > pattern, <0 if
 *                              real < pattern.
 *      RETURNS
 *              LAS_EVAL_TRUE or LAS_EVAL_FALSE	or LAS_EVAL_INVALID
 *	DEBUG
 *		Can add asserts that the strcmp failure cases are one of the
 *		remaining legal comparators.
 */
int
evalComparator(CmpOp_t ctok, int result)
{
    if (result == 0) {
        switch(ctok) {
        case CMP_OP_EQ:
        case CMP_OP_GE:
        case CMP_OP_LE:
            return LAS_EVAL_TRUE;
        case CMP_OP_NE:
        case CMP_OP_GT:
        case CMP_OP_LT:
            return LAS_EVAL_FALSE;
        default:
            return LAS_EVAL_INVALID;
        }
    } else if (result > 0) {
        switch(ctok) {
        case CMP_OP_GT:
        case CMP_OP_GE:
        case CMP_OP_NE:
            return LAS_EVAL_TRUE;
        case CMP_OP_LT:
        case CMP_OP_LE:
        case CMP_OP_EQ:
            return LAS_EVAL_FALSE;
        default:
            return LAS_EVAL_INVALID;
        }
    } else {				/* real < pattern */
        switch(ctok) {
        case CMP_OP_LT:
        case CMP_OP_LE:
        case CMP_OP_NE:
            return LAS_EVAL_TRUE;
        case CMP_OP_GT:
        case CMP_OP_GE:
        case CMP_OP_EQ:
            return LAS_EVAL_FALSE;
        default:
            return LAS_EVAL_INVALID;
        }
    }
}


/* 	Takes a string and returns the same string with all uppercase
*	letters converted to lowercase.
*/
void
makelower(char	*string)
{
     while (*string) {
          *string = tolower(*string);
          string++;
     }
}


/* 	Given an LAS_EVAL_* value, translates to ACL_RES_*  */
int
EvalToRes(int value)
{
	switch (value) {
	case LAS_EVAL_TRUE:
		return ACL_RES_ALLOW;
	case LAS_EVAL_FALSE:
		return ACL_RES_DENY;
	case LAS_EVAL_PWEXPIRED:
		return ACL_RES_DENY; /* for now */
	case LAS_EVAL_DECLINE:
		return ACL_RES_FAIL;
	case LAS_EVAL_FAIL:
		return ACL_RES_FAIL;
	case LAS_EVAL_INVALID:
		return ACL_RES_INVALID;
	case LAS_EVAL_NEED_MORE_INFO:
		return ACL_RES_DENY;
        default:
		NS_ASSERT(1);
		return ACL_RES_ERROR;
	}
}

NSAPI_PUBLIC const char *comparator_string (int comparator)
{
    static char invalid_cmp[32];

    switch(comparator) {
    case CMP_OP_EQ: return "CMP_OP_EQ";
    case CMP_OP_NE: return "CMP_OP_NE";
    case CMP_OP_GT: return "CMP_OP_GT";
    case CMP_OP_LT: return "CMP_OP_LT";
    case CMP_OP_GE: return "CMP_OP_GE";
    case CMP_OP_LE: return "CMP_OP_LE";
    default:
	sprintf(invalid_cmp, "unknown comparator %d", comparator);
	return invalid_cmp;
    }
}


const char *comparator_string_sym(int comparator)
{
    static char invalid_cmp[32];

    switch(comparator) {
    case CMP_OP_EQ: return "=";
    case CMP_OP_NE: return "!=";
    case CMP_OP_GT: return ">";
    case CMP_OP_LT: return "<";
    case CMP_OP_GE: return ">=";
    case CMP_OP_LE: return "<=";
    default:
	sprintf(invalid_cmp, "unknown comparator %d", comparator);
	return invalid_cmp;
    }
}


/* Return the pointer to the next token after replacing the following 'delim'
 * char with NULL.
 * WARNING - Modifies the first parameter */
char *acl_next_token (char **ptr, char delim)
{
    char *str = *ptr;
    char *token = str;
    char *comma;

    if (!token) { *ptr = 0; return 0; }

    /* ignore leading whitespace */
    while(*token && ldap_utf8isspace(token)) token++;

    if (!*token) { *ptr = 0; return 0; }

    if ((comma = strchr(token, delim)) != NULL) {
	*comma++ = 0;
    }

    {
	/* ignore trailing whitespace */
	int len = strlen(token);
	char *sptr = token+len-1;
	
	while(*sptr == ' ' || *sptr == '\t') *sptr-- = 0;
    }

    *ptr = comma;
    return token;
}


/* Returns a pointer to the next token and it's length */
/* tokens are separated by 'delim' characters */
/* ignores whitespace surrounding the tokens */
const char *acl_next_token_len (const char *ptr, char delim, int *len)
{
    const char *str = ptr;
    const char *token = str;
    const char *comma;

    *len = 0;

    if (!token) { return 0; }

    /* ignore leading whitespace */
    while(*token && ldap_utf8isspace((char *)token)) token++;

    if (!*token) { return 0; }
    if (*token == delim) { return token; } /* str starts with delim! */

    if ((comma = strchr(token, delim)) != NULL) {
	*len = comma - token;
    }
    else {
	*len = strlen(token);
    }

    {
	/* ignore trailing whitespace */
	const char *sptr = token + *len - 1;
	
	while(*sptr == ' ' || *sptr == '\t') {
	    sptr--;
	    (*len)--;
	}
    }

    return token;
}

/* acl_get_servername
 * If the REQ_SERVERNAME is available on the 'resource' plist, return it.
 * Otherwise, get the servername via the request.
 */
const char *
acl_get_servername(PList_t resource)
{
    const char *servername = 0;
    Request *rq;

    if (PListGetValue(resource, ACL_ATTR_SERVERNAME_INDEX, (void **)&servername, NULL) >= 0)
        return servername;

    // no found in resource, so let's go via request
    if (PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, (void **)&rq, NULL) < 0)
        return NULL;    // we're doomed.  

    if ((servername = GetServerHostname(rq)) == NULL)
        return NULL;    // we're doomed, too.

    // cache it.
    PListInitProp(resource, ACL_ATTR_SERVERNAME_INDEX, ACL_ATTR_SERVERNAME, (void *)servername, NULL);

    return servername;
}


/*-----------------------------------------------------------------------------
 * If any of the elements in 'target' appear in 'list', return the name of
 * the first matching element (see 6226967).
 *
 * target: One or more (comma-separated) target strings.
 * list: One or more (comma-separated) list of elements.
 *
 * RETURN:
 *  char * to matching element or NULL. Caller must free if non-NULL.
 *
 */
char * acl_attrlist_match(const char * target, const char * list)
{
    char * loc_target;
    char * loc_list;
    char * one_target;
    char * one_list;
    char * tmp_target;
    char * tmp_list;
    char * ret = NULL;
    char ** list_entry;
    int list_count;
    int done = 0;
    int i;
    
    if (!target || !*target || !list || !*list) { return NULL; }

    // count entries in 'list'

    i = 0;
    list_count = 1;
    while (list[i]) {
        if (list[i++] == ',') { list_count++; }
    }

    // working in a new local copy of list (loc_list), get start of each
    // list entry and keep these in list_entry
    
    if ((loc_list = STRDUP(list)) == NULL) { return NULL; }
    if ((list_entry = (char **)CALLOC(list_count * sizeof(char *))) == NULL) {
        FREE(loc_list);
        return NULL;
    }

    tmp_list = loc_list;
    i = 0;
    while (tmp_list && *tmp_list) {
        one_list = acl_next_token(&tmp_list, ',');
        list_entry[i++] = one_list;
    }

    // loop thru entries in target looking for matches to list_entry'ies
    
    if ((loc_target = STRDUP(target)) == NULL) {
        FREE(loc_list);
        FREE(list_entry);
        return NULL;
    }
    
    tmp_target = loc_target;
    while (!done && tmp_target && *tmp_target) {
        one_target = acl_next_token(&tmp_target, ',');
        for (i=0; i < list_count; i++) {
            if (!strcmp(list_entry[i], one_target)) {
                done = 1;
                break;
            }
        }
    }

    // if found match, return the name of the [first] group which matched
    
    if (done) {
        ret = STRDUP(one_target);
    }

    FREE(loc_target);
    FREE(loc_list);
    FREE(list_entry);
    
    return(ret);
}


/*-----------------------------------------------------------------------------
 * Hashtable enumerator, for use only by acl_get_name. If search_info->number
 * matches the current entry's number, populates search_info->name with the
 * name and stops enumeration, otherwise continues enumeration.
 *
 */
static int acl_name_enumerator(PRHashEntry *he, PRIntn i, void *arg)
{
    acl_search_info * info = (acl_search_info *)arg;
    void * target = info->number;
    void * thisone = (void *)(he->value);

    if (target == thisone) {
        info->name = (char *)he->key;
        return HT_ENUMERATE_STOP;
    }

    return HT_ENUMERATE_NEXT;
}


/*-----------------------------------------------------------------------------
 * Find the string name of an ACL method (from ACLMethodHash) or 
 * type (from ACLDbTypeHash) which corresponds to the numeric value.
 * Useful for creating readable debug messages.
 *
 * ht: The appropriate hashtable (PRHashTable) shown above.
 * num: The target identifier.
 *
 * RETURN:
 *  char * to matching name (or 'UNKNOWN' on failure). Caller must FREE()
 *
 */
char * acl_get_name(PRHashTable *ht, void * num)
{
    char * ret;
    acl_search_info info;

    info.number = num;
    info.name = (char *)"UNKNOWN";

    ACL_CritEnter();
    int rv = PR_HashTableEnumerateEntries(ht, 
                                          acl_name_enumerator, (void *)&info);
    ret = STRDUP(info.name);
    ACL_CritExit();

    return ret;
}


