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
 * Description (symbols.c)
 *
 *	This module implements a symbol table for ACL-related structures.
 *	The symbol table associates string names and types with pointers
 *	to various kinds of structures.
 */

#include <base/systems.h>
#include <base/util.h>
#include <netsite.h>
#define __PRIVATE_SYMBOLS
#include "symbols.h"
#include <ctype.h>

/* Table of pointers to functions associated with the hash table */
static PRHashAllocOps SymAllocOps = {
    symAllocTable,			/* allocate the hash table */
    symFreeTable,			/* free the hash table */
    symAllocEntry,			/* allocate a table entry */
    symFreeEntry,			/* free a table entry */
};

static PRHashEntry * symAllocEntry(void * pool, const void *ignored)
{
    PRHashEntry * he;

    he =  (PRHashEntry *) PERM_MALLOC(sizeof(PRHashEntry));

    return he;
}

static void * symAllocTable(void * pool, PRSize size)
{
    return (void *)PERM_MALLOC(size);
}

static int symCmpName(const void * name1, const void * name2)
{
    Symbol_t * sym1 = (Symbol_t *)name1;
    Symbol_t * sym2 = (Symbol_t *)name2;

    return ((sym1->sym_type == sym2->sym_type) &&
	    !strcasecmp(sym1->sym_name, sym2->sym_name));
}

static int symCmpValue(const void * value1, const void * value2)
{
    return (value1 == value2);
}

static void symFreeEntry(void * pool, PRHashEntry * he, uintn flag)
{
    if (flag == HT_FREE_ENTRY) {
	/* Just free the hash entry, not anything it references */
	PERM_FREE(he);
    }
}

static void symFreeTable(void * pool, void * item)
{
    PERM_FREE(item);
}

static PRHashNumber symHash(const void * symkey)
{
    Symbol_t * sym = (Symbol_t *)symkey;
    char * cp;
    PRHashNumber h;

    h = sym->sym_type;
    cp = sym->sym_name;
    if (cp) {
	while (*cp) {
	    h ^= (h << 3) ^ tolower(*cp);
	    ++cp;
	}
    }

    return h;
}

/*
 * Description (symTableAddSym)
 *
 *	This function adds a symbol definition to the symbol table.
 *	The symbol definition includes a name string, a type, and a
 *	reference to a structure.
 *
 * Arguments:
 *
 *	table			- handle for symbol table
 *	newsym			- pointer to new symbol name and type
 *	symref			- pointer to structure named by symbol
 *
 * Returns:
 *
 *	If successful, the return code is zero.  An error is indicated
 *	by a negative return code (SYMERRxxxx - see symbols.h).
 */

int symTableAddSym(void * table, Symbol_t * newsym, void * symref)
{
    SymTable_t * st = (SymTable_t *)table;
    PRHashEntry * he;
    PRHashEntry **hep;
    PRHashNumber keyhash;
    int rv = 0;

    /* Compute the hash value for this symbol */
    keyhash = symHash((const void *)newsym);

    crit_enter(st->stb_crit);

    /* See if another symbol already has the same name and type */
    hep = PR_HashTableRawLookup(st->stb_ht, keyhash, (void *)newsym);
    if (*hep == 0) {

	/* Expand the hash table if necessary and allocate an entry */
	he = PR_HashTableRawAdd(st->stb_ht,
				hep, keyhash, (void *)newsym, symref);
    }
    else {
	/* The symbol is already there.  It's an error */
	rv = SYMERRDUPSYM;
    }

    crit_exit(st->stb_crit);
    return rv;
}

/*
 * Description (symTableRemoveSym)
 *
 *	This function removes an entry from a symbol table.  It does
 *	not free the entry itself, just the hash entry that references
 *	it.
 *
 * Arguments:
 *
 *	table			- symbol table handle
 *	sym			- pointer to symbol structure
 */

void symTableRemoveSym(void * table, Symbol_t * sym)
{
    SymTable_t * st = (SymTable_t *)table;

    if (sym->sym_name != 0) {
	crit_enter(st->stb_crit);
	PR_HashTableRemove(st->stb_ht, (void *)sym);
	crit_exit(st->stb_crit);
    }
}

/* Helper function for symTableEnumerate() */
typedef struct {
    enumSTfunc func;
    void * argp;
} SymTableEnum_t;

static int symTableEnumHelp(PRHashEntry * he, int n, void * step)
{
    SymTableEnum_t * ste = (SymTableEnum_t *)step;
    int ret = 0;
    int rv;

    rv = (*ste->func)((Symbol_t *)(he->key), ste->argp);
    if (rv != 0) {
	if (rv & SYMENUMREMOVE) ret = HT_ENUMERATE_REMOVE;
	if (rv & SYMENUMSTOP) ret |= HT_ENUMERATE_STOP;
    }

    return ret;
}

/*
 * Description (symTableEnumerate)
 *
 *	This function enumerates all of the entries in a symbol table,
 *	calling a specified function for each entry.  The function
 *	specified by the caller may return flags indicating actions
 *	to be taken for each entry or whether to terminate the
 *	enumeration.  These flags are defined in symbols.h as
 *	SYMENUMxxxx.
 *
 * Arguments:
 *
 *	table				- symbol table handle
 *	argp				- argument for caller function
 *	func				- function to be called for each entry
 */

void symTableEnumerate(void * table, void * argp, enumSTfunc func)
{
    SymTable_t * st = (SymTable_t *)table;
    SymTableEnum_t ste;		/* enumeration arguments */

    ste.func = func;
    ste.argp = argp;

    crit_enter(st->stb_crit);
    (void)PR_HashTableEnumerateEntries(st->stb_ht,
				       symTableEnumHelp, (void *)&ste);
    crit_exit(st->stb_crit);
}

/*
 * Description (symTableFindSym)
 *
 *	This function locates a symbol with a specified name and type
 *	in a given symbol table.  It returns a pointer to the structure
 *	named by the symbol.
 *
 * Arguments:
 *
 *	table				- symbol table handle
 *	symname				- symbol name string pointer
 *	symtype				- symbol type code
 *	psymref				- pointer to returned structure pointer
 *
 * Returns:
 *
 *	If successful, the return code is zero and the structure pointer
 *	associated with the symbol name and type is returned in the
 *	location specified by 'psymref'.  An error is indicated by a
 *	negative return code (SYMERRxxxx - see symbols.h).
 */

int symTableFindSym(void * table, char * symname,
		      int symtype, void **psymref)
{
    SymTable_t * st = (SymTable_t *)table;
    Symbol_t sym;
    void * symref;

    /* Create temporary entry with fields needed by symHash() */
    sym.sym_name = symname;
    sym.sym_type = symtype;
    
    crit_enter(st->stb_crit);

    symref = PR_HashTableLookup(st->stb_ht, (void *)&sym);

    crit_exit(st->stb_crit);

    *psymref = symref;

    return (symref) ? 0 : SYMERRNOSYM;
}

/*
 * Description (symTableDestroy)
 *
 *	This function destroys a symbol table created by symTableNew().
 *
 * Arguments:
 *
 *	table			- symbol table handle from symTableNew()
 *	flags			- bit flags (unused - must be zero)
 */

void symTableDestroy(void * table, int flags)
{
    SymTable_t * st = (SymTable_t *)table;

    if (st) {
	if (st->stb_crit) {
	    crit_terminate(st->stb_crit);
	}

	if (st->stb_ht) {
	    PR_HashTableDestroy(st->stb_ht);
	}

	PERM_FREE(st);
    }
}

/*
 * Description (symTableNew)
 *
 *	This function creates a new symbol table, and returns a handle
 *	for it.
 *
 * Arguments:
 *
 *	ptable			- pointer to returned symbol table handle
 *
 * Returns:
 *
 *	If successful, the return code is zero and a handle for the new
 *	symbol table is returned in the location specified by 'ptable'.
 *	An error is indicated by a negative return code (SYMERRxxxx
 *	- see symbols.h).
 */

int symTableNew(void **ptable)
{
    SymTable_t * st;

    /* Allocate the symbol table object */
    st = (SymTable_t *)PERM_MALLOC(sizeof(SymTable_t));
    if (st == 0) goto err_nomem;

    /* Get a monitor for it */
    st->stb_crit = crit_init();

    st->stb_ht = PR_NewHashTable(0, symHash, symCmpName, symCmpValue,
				 &SymAllocOps, 0);
    if (st->stb_ht == 0) goto err_nomem;

    *ptable = st;
    return 0;

  err_nomem:
    if (st) {
	symTableDestroy(st, 0);
    }
    return SYMERRNOMEM;
}
