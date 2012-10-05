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

#include <netsite.h>
#include <libaccess/acl.h>              // generic ACL definitions
#include <libaccess/aclproto.h>         // internal prototypes
#include <libaccess/aclglobal.h>        // global data
#include <libaccess/aclerror.h>         // error codes
#include "aclpriv.h"                    // internal data structure definitions
#include <libaccess/dbtlibaccess.h>     // strings
#include "plhash.h"
#include "plstr.h"
#include <base/nsassert.h>

#include "symbols.h"

// forward declaration
static int acl_hash_entry_destroy(Symbol_t * sym, void * argp);

/*
 * Creates a handle for a new list of ACLs
 *
 * This function creates a new list of ACLs. The list is initially empty
 * and can be added to by ACL_ListAppend().  A resource manager would use
 * these functions to build up a list of all the ACLs applicable to a
 * particular resource access.
 *
 * Input:
 * Returns:
 *    NULL		failure, otherwise returns a new 
 *			ACLListHandle
 */

NSAPI_PUBLIC ACLListHandle_t *
ACL_ListNew(NSErr_t *errp)
{
ACLListHandle_t    *handle;

    handle = ( ACLListHandle_t * ) PERM_CALLOC ( sizeof(ACLListHandle_t) );
    handle->ref_count = 1;
    return(handle);
}

/*
 * Destroys an ACL List
 *
 * Input:
 *    acl_list		target list
 */
NSAPI_PUBLIC void
ACL_ListDestroy(NSErr_t *errp, ACLListHandle_t *acl_list )
{
    ACLWrapper_t    *wrapper;
    ACLWrapper_t    *tmp_wrapper;
    ACLHandle_t     *tmp_acl;

    // nothing to do?
    if ( acl_list == NULL )
        // nothing to do.
        return;

    if (acl_list->ref_count != 0) {
        NS_ASSERT(acl_list->ref_count == 0);
    }

    if ( acl_list->acl_sym_table ) {
        /* Destroy each entry in the symbol table */
        symTableEnumerate(acl_list->acl_sym_table, 0, acl_hash_entry_destroy);
        /* Destory the hash table itself */
        symTableDestroy(acl_list->acl_sym_table, 0);
    }

    // destroy evaluation cache (if there)
    ACL_EvalDestroyContext( (ACLListCache_t *)acl_list->cache );

    wrapper = acl_list->acl_list_head;
    
    while ( wrapper ) {
        tmp_acl = wrapper->acl;
        tmp_wrapper = wrapper;
        wrapper = wrapper->wrap_next;
        PERM_FREE( tmp_wrapper );
        ACL_AclDestroy(errp, tmp_acl );
    }

    PERM_FREE( acl_list );

    return;
}

/*
 * Allocates a handle for an ACL wrapper
 *
 * This wrapper is just used for ACL list creation.  It's a way of
 * linking ACLs into a list.  This is an internal function.
 */

static ACLWrapper_t *
acl_wrapper_new(void)
{
ACLWrapper_t    *handle;

    handle = ( ACLWrapper_t * ) PERM_CALLOC ( sizeof(ACLWrapper_t) );
    return(handle);
}

/*
 * Description 
 *
 *      This function destroys an entry a symbol table entry for an
 *      ACL.
 *
 * Arguments:
 *
 *      sym                     - pointer to Symbol_t for an ACL entry
 *      argp                    - unused (must be zero)
 *
 * Returns:
 *
 *      The return value is SYMENUMREMOVE.
 */

static
int acl_hash_entry_destroy(Symbol_t * sym, void * argp)
{
    if (sym != 0) {

        /* Free the acl name string if any */
        if (sym->sym_name != 0) {
            PERM_FREE(sym->sym_name);
        }

        /* Free the Symbol_t structure */
        PERM_FREE(sym);
    }

    /* Indicate that the symbol table entry should be removed */
    return SYMENUMREMOVE;
}


/*
 * LOCAL FUNCTION
 *
 * Create a new symbol with the sym_name equal to the
 * acl->tag value.  Attaches the acl to the sym_data
 * pointer.
 */

static Symbol_t *
acl_sym_new(ACLHandle_t *acl)
{
    Symbol_t *sym;
    /* It's not there, so add it */
    sym = (Symbol_t *) PERM_MALLOC(sizeof(Symbol_t));
    if ( sym == NULL ) 
        return(NULL);

    sym->sym_name = PERM_STRDUP(acl->tag);
    if ( sym->sym_name == NULL ) {
        PERM_FREE(sym);
        return(NULL);
    }

    sym->sym_type = ACLSYMACL;
    sym->sym_data = (void *) acl;
    return(sym);

}

/*
 * LOCAL FUNCTION
 *
 * Add a acl symbol to an acl_list's symbol table.
 *
 * Each acl list has a symbol table.  the symbol table
 * is a quick qay to reference named acl's
 */

static int
acl_sym_add(ACLListHandle_t *acl_list, ACLHandle_t *acl)
{
Symbol_t *sym;
int rv;

    if ( acl->tag == NULL )
        return(ACLERRUNDEF);

    rv = symTableFindSym(acl_list->acl_sym_table,
                         acl->tag,
                         ACLSYMACL,
                         (void **)&sym);
    if ( rv == SYMERRNOSYM ) {
         sym = acl_sym_new(acl);
         if ( sym )
              rv = symTableAddSym(acl_list->acl_sym_table, sym, (void *)sym);
    }

    if ( sym == NULL || rv < 0 )
        return(ACLERRUNDEF);

    return(0);
}

/*
 * LOCAL FUNCTION
 * 
 * Destroy an acl_list's symbol table and all memory referenced
 * by the symbol table.  This does not destroy an acl_list.
 */

static void
acl_symtab_destroy(ACLListHandle_t *acl_list)
{
    /* Destroy each entry in the symbol table */
    symTableEnumerate(acl_list->acl_sym_table, 0, acl_hash_entry_destroy);
    /* Destory the hash table itself */
    symTableDestroy(acl_list->acl_sym_table, 0);
    acl_list->acl_sym_table = NULL;
    return;
}


/*
 * Appends to a specified ACL
 *
 * This function appends a specified ACL to the end of a given ACL list.
 * 
 * Input:
 *    errp		The error stack
 *    flags		should always be zero now
 *    acl_list		target ACL list
 *    acl		new acl
 * Returns:
 *    > 0		The number of acl's in the current list
 *    < 0		failure
 */
NSAPI_PUBLIC int
ACL_ListAppend( NSErr_t *errp, ACLListHandle_t *acl_list, ACLHandle_t *acl,
                int flags )
{
    ACLWrapper_t	*wrapper;
    ACLHandle_t 	*tmp_acl;
    
    if ( acl_list == NULL || acl == NULL )
        return(ACLERRUNDEF);
    
    if ( acl_list->acl_sym_table == NULL && 
         acl_list->acl_count == ACL_TABLE_THRESHOLD ) {

        /*
         * The symbol table isn't really critical so we don't log
         * an error if its creation fails.
         */

        symTableNew(&acl_list->acl_sym_table);
        if ( acl_list->acl_sym_table ) {
            for (wrapper = acl_list->acl_list_head; wrapper; 
                 wrapper = wrapper->wrap_next ) {
                 tmp_acl = wrapper->acl;
                 if ( acl_sym_add(acl_list, tmp_acl) ) {
                    acl_symtab_destroy(acl_list);
                    break;
                 }
            } 
        }
    } 

    wrapper = acl_wrapper_new();
    if ( wrapper == NULL ) {
        return(ACLERRNOMEM);
    }
    
    wrapper->acl = acl;
    
    if ( acl_list->acl_list_head == NULL ) {
        acl_list->acl_list_head = wrapper;
        acl_list->acl_list_tail = wrapper;
    } else {
        acl_list->acl_list_tail->wrap_next = wrapper;
        acl_list->acl_list_tail = wrapper;
    }

    /* Bugfix: acl can be shared across multiple sprocs. */
    acl->ref_count++;

    acl_list->acl_count++;

    if ( acl_list->acl_sym_table ) {
        /*
         * If we fail to insert the ACL then we
         * might as well destroy this hash table since it is
         * useless.
         */
        if ( acl_sym_add(acl_list, acl) ) {
            acl_symtab_destroy(acl_list);
        }
    }
  
    return(acl_list->acl_count);
}

/*
 * Concatenates two ACL lists
 *
 * Attaches all ACLs in acl_list2 to the end of acl_list1.  acl_list2
 * is left unchanged.
 *
 * Input:
 *	errp		pointer to the error stack
 *	acl_list1	target ACL list
 *	acl_list2	source ACL list
 * Output:
 *	acl_list1	list contains the concatenation of acl_list1
 *			and acl_list2.  
 * Returns:
 *	> 0		Number of ACLs in acl_list1 after concat
 *	< 0		failure
 */

NSAPI_PUBLIC int
ACL_ListConcat( NSErr_t *errp, ACLListHandle_t *acl_list1,
                ACLListHandle_t *acl_list2, int flags ) 
{
    ACLWrapper_t *wrapper;
    int rv;

    if ( acl_list1 == NULL || acl_list2 == NULL )
        return(ACLERRUNDEF);

    for ( wrapper = acl_list2->acl_list_head; 
		wrapper != NULL; wrapper = wrapper->wrap_next ) 
        if ( (rv = ACL_ListAppend ( errp, acl_list1, wrapper->acl, 0 )) < 0 )
            return(rv);

    return (acl_list1->acl_count);
}

/*
 * Delete a named ACL from an ACL list
 *
 * Input:
 *	acl_list	Target ACL list handle
 *	acl_name	Name of the target ACL
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_ListAclDelete(NSErr_t *errp, ACLListHandle_t *acl_list, char *acl_name, int flags ) 
{
ACLHandle_t *acl = NULL;
ACLWrapper_t *wrapper;
ACLWrapper_t *wrapper_prev = NULL;
Symbol_t *sym;

    if ( acl_list == NULL || acl_name == NULL )
        return(ACLERRUNDEF);

    if ( flags & ACL_CASE_INSENSITIVE ) {
        for ( wrapper = acl_list->acl_list_head; wrapper != NULL; 
              wrapper = wrapper->wrap_next ) {
            if ( wrapper->acl->tag &&
                 PL_strcasecmp( wrapper->acl->tag, acl_name ) == 0 ) {
                acl = wrapper->acl;
                break;
            }
            wrapper_prev = wrapper;
        }
    } else {
        for ( wrapper = acl_list->acl_list_head; wrapper != NULL; 
              wrapper = wrapper->wrap_next ) {
            if ( wrapper->acl->tag &&
                 strcmp( wrapper->acl->tag, acl_name ) == 0 ) {
                acl = wrapper->acl;
                break;
            }
            wrapper_prev = wrapper;
        }
    }

    if ( acl ) {

        if ( wrapper_prev ) {
	    wrapper_prev->wrap_next = wrapper->wrap_next;
        } else {
	    acl_list->acl_list_head = wrapper->wrap_next;
        }

        if ( acl_list->acl_list_tail == wrapper ) {
            acl_list->acl_list_tail = wrapper_prev;
        }

        acl = wrapper->acl;
        acl_list->acl_count--;
        PERM_FREE(wrapper);

        if ( acl_list->acl_sym_table ) {
            if ( symTableFindSym(acl_list->acl_sym_table, 
                              acl->tag, ACLSYMACL, (void **) &sym) < 0 ) {

            /* not found, this is an error of some sort */

            } else {
                symTableRemoveSym(acl_list->acl_sym_table, sym);
                acl_hash_entry_destroy(sym, 0);
            }
        }

        ACL_AclDestroy(errp, acl);
        return(0);
    }

    return(ACLERRUNDEF);
}

/*
 * FUNCTION:    ACL_ListGetFirst
 *
 * DESCRIPTION:
 *
 *      This function is used to start an enumeration of an
 *      ACLListHandle_t.  It returns an ACLHandle_t* for the first
 *      ACL on the list, and initializes a handle supplied by the
 *      caller, which is used to track the current position in the
 *      enumeration.  This function is normally used in a loop
 *      such as:
 *
 *          ACLListHandle_t *acl_list = <some ACL list>;
 *          ACLHandle_t *cur_acl;
 *          ACLListEnum_t acl_enum;
 *
 *          for (cur_acl = ACL_ListGetFirst(acl_list, &acl_enum);
 *               cur_acl != 0;
 *               cur_acl = ACL_ListGetNext(acl_list, &acl_enum)) {
 *              ...
 *          }
 *
 *      The caller should guarantee that no ACLs are added or removed
 *      from the ACL list during the enumeration.
 *
 * ARGUMENTS:
 *
 *      acl_list                - handle for the ACL list
 *      acl_enum                - pointer to uninitialized enumeration handle
 *
 * RETURNS:
 *
 *      As described above.  If the acl_list argument is null, or the
 *      referenced ACL list is empty, the return value is null.
 */

NSAPI_PUBLIC ACLHandle_t *
ACL_ListGetFirst(ACLListHandle_t *acl_list, ACLListEnum_t *acl_enum)
{
    ACLWrapper_t *wrapper;
    ACLHandle_t *acl = 0;

    *acl_enum = 0;

    if (acl_list) {

        wrapper = acl_list->acl_list_head;
        *acl_enum = (ACLListEnum_t)wrapper;

        if (wrapper) {
            acl = wrapper->acl;
        }
    }

    return acl;
}

NSAPI_PUBLIC ACLHandle_t *
ACL_ListGetNext(ACLListHandle_t *acl_list, ACLListEnum_t *acl_enum)
{
    ACLWrapper_t *wrapper = (ACLWrapper_t *)(*acl_enum);
    ACLHandle_t *acl = 0;

    if (wrapper) {

        wrapper = wrapper->wrap_next;
        *acl_enum = (ACLListEnum_t)wrapper;

        if (wrapper) acl = wrapper->acl;
    }

    return acl;
}

/*
 * Finds a named ACL in an input list.
 *
 * Input:
 *	acl_list	a list of ACLs to search
 *	acl_name	the name of the ACL to find
 * 	flags		e.g. ACL_CASE_INSENSITIVE
 * Returns:
 *	NULL		No ACL found
 *	acl		A pointer to an ACL with named acl_name
 */
NSAPI_PUBLIC ACLHandle_t *
ACL_ListFind (NSErr_t *errp, ACLListHandle_t *acl_list, char *acl_name, int flags )
{
    ACLHandle_t *result = NULL;
    ACLWrapper_t *wrapper;
    Symbol_t *sym;

    if ( acl_list == NULL || acl_name == NULL )
        return NULL;

    /*
     * right now the symbol table exists if there hasn't been
     * any collisions based on using case insensitive names.
     * if there are any collisions then the table will be
     * deleted and we will look up using list search.
     *
     * we should probably create two hash tables, one for case
     * sensitive lookups and the other for insensitive.
     */ 
    if ( acl_list->acl_sym_table ) {
        if ( symTableFindSym(acl_list->acl_sym_table, acl_name, ACLSYMACL, (void **) &sym) >= 0 ) {
             result = (ACLHandle_t *) sym->sym_data;
             if ( result && (flags & ACL_CASE_SENSITIVE) &&
                  strcmp(result->tag, acl_name) ) {
                 result = NULL; /* case doesn't match */
             }
        }
        return( result );
    }

    //
    // if there's no symbol table, we search in a linear fashion.
    //
    if ( flags & ACL_CASE_INSENSITIVE ) {
        for ( wrapper = acl_list->acl_list_head; wrapper != NULL; wrapper = wrapper->wrap_next ) {
            if ( wrapper->acl->tag && PL_strcasecmp( wrapper->acl->tag, acl_name ) == 0 ) {
                result = wrapper->acl;
                break;
            }
        }
    } else {
        for ( wrapper = acl_list->acl_list_head; wrapper != NULL; wrapper = wrapper->wrap_next ) {
            if ( wrapper->acl->tag && strcmp( wrapper->acl->tag, acl_name ) == 0 ) {
                result = wrapper->acl;
                break;
            }
        }
    }

    return( result );
}

/*
 * Gets a name list of consisting of all ACL names for input list. 
 *
 * Input:
 *	acl_list	an ACL List handle	
 *	name_list	pointer to a list of string pointers	
 * Returns:
 *    0			success
 *    < 0		failure
 */
NSAPI_PUBLIC int
ACL_ListGetNameList(NSErr_t *errp, ACLListHandle_t *acl_list, char ***name_list)
{
    const int block_size = 50;
    ACLWrapper_t 	*wrapper;
    int			list_index;
    int			list_size;
    char		**tmp_list;
    char		**local_list;
    char		*name;
    

    if ( acl_list == NULL )
        return(ACLERRUNDEF);

    list_size = block_size;
    local_list = (char **) PERM_MALLOC(sizeof(char *) * list_size);
    if ( local_list == NULL ) 
        return(ACLERRNOMEM); 
    list_index = 0;
    local_list[list_index] = NULL; 

    for ( wrapper = acl_list->acl_list_head; wrapper != NULL; 
                        wrapper = wrapper->wrap_next ) {
        if ( wrapper->acl->tag ) 
            name = wrapper->acl->tag;
        else 
            name = "noname";
        if ( list_index + 2 > list_size ) {
            list_size += block_size;
            tmp_list = (char **) PERM_REALLOC(local_list, 
                                              sizeof(char *) * list_size);
            if ( tmp_list == NULL ) {
                ACL_NameListDestroy(errp, local_list);
                return(ACLERRNOMEM); 
            }
            local_list = tmp_list;
        } 
        local_list[list_index] = PERM_STRDUP(name);
        if ( local_list[list_index] == NULL ) {
            ACL_NameListDestroy(errp, local_list);
            return(ACLERRNOMEM); 
        }
        list_index++;
        local_list[list_index] = NULL; 
    }    
    *name_list = local_list;
    return(0);
}

/*
 * ACL_ListPostParseForAuth - post-process an ACL list
 *
 * go through the auth_info's for every expression and
 * - replace method names by method types
 * XXX This used to change method to method plus DBTYPE, and registers
 * databases.
 *
 * Input:
 *	errp		error stack	
 *	acl_list	Target ACL list handle
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_ListPostParseForAuth(NSErr_t *errp, ACLListHandle_t *acl_list ) 
{
    ACLHandle_t *acl;
    ACLWrapper_t *wrap;
    ACLExprHandle_t *expr;
    char *method;
    char *database;
    int rv;
    ACLDbType_t *dbtype;
    ACLMethod_t *methodtype;

    if ( acl_list == NULL )
        return(0);

    // for all ACLs
    for ( wrap = acl_list->acl_list_head; wrap; wrap = wrap->wrap_next ) {

        acl = wrap->acl;
        if ( acl == NULL )
            continue;

        // for all expressions with the ACL
        for ( expr = acl->expr_list_head; expr; expr = expr->expr_next ) {

            if ( expr->expr_type != ACL_EXPR_TYPE_AUTH || expr->expr_auth == NULL) 
                continue;

            // get method attribute - this is a name now
            rv = PListGetValue(expr->expr_auth, ACL_ATTR_METHOD_INDEX, (void **) &method, NULL);
            if ( rv >= 0 ) {
		methodtype = (ACLMethod_t *)PERM_MALLOC(sizeof(ACLMethod_t));
		rv = ACL_MethodFind(errp, method, methodtype);
		if (rv < 0) {
		    nserrGenerate(errp, ACLERRUNDEF, ACLERR3800, ACL_Program,
				  3, acl->tag, "method", method);
		    PERM_FREE(methodtype);
		    return(ACLERRUNDEF);
		}

                // replace it with a method type
	        rv = PListSetValue(expr->expr_auth, ACL_ATTR_METHOD_INDEX, methodtype, NULL);
		if ( rv < 0 ) {
		    nserrGenerate(errp, ACLERRNOMEM, ACLERR3810, ACL_Program, 0);
		    return(ACLERRNOMEM);
		}
		PERM_FREE(method);
	    }
        }
    }
    return(0);
}

//
// ACL_ListDecrement - decrement the ACLList's refcount safely
//
NSAPI_PUBLIC int
ACL_ListDecrement(NSErr_t *errp, ACLListHandle_t *acllist)
{
    if (!acllist  ||  acllist == ACL_LIST_NO_ACLS)
	return 0;

    NS_ASSERT(ACL_AssertAcllist(acllist));

    ACL_CritEnter();
    NS_ASSERT(ACL_CritHeld());
    if (--acllist->ref_count == 0)
        ACL_ListDestroy(errp, acllist);
    ACL_CritExit();

    return 0;
}

//
// ACL_ListIncrement - increment the ACLList's refcount safely
//
NSAPI_PUBLIC void
ACL_ListIncrement(NSErr_t *errp, ACLListHandle_t *acllist)
{
    if (!acllist  ||  acllist == ACL_LIST_NO_ACLS)
	return;

    NS_ASSERT(ACL_AssertAcllist(acllist));

    ACL_CritEnter();
    NS_ASSERT(ACL_CritHeld());
    acllist->ref_count++;
    ACL_CritExit();
    return;
}

#ifdef MCC_DEBUG
NSAPI_PUBLIC int
ACL_ListPrint (ACLListHandle_t *acl_list ) 
{
    ACLHandle_t *acl;
    ACLWrapper_t *wrap;
    int rv;

    if ( acl_list == NULL || acl_list == ACL_LIST_NO_ACLS)
        fprintf(stderr, "\tThe ACL list is empty.\n");

    for ( wrap = acl_list->acl_list_head; wrap; wrap = wrap->wrap_next ) {
        acl = wrap->acl;
	fprintf(stderr, "\t%s\n", acl->tag);
    }

    return 0;
}
#endif /* MCC_DEBUG */
