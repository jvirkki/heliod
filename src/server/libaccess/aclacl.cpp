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

#include <stdio.h>
#include <string.h>
#include <netsite.h>
#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include <libaccess/aclerror.h>
#include "aclpriv.h"

/*
 * Allocate a new ACL handle
 *
 * This function creates a new ACL structure that will be used for
 * access control information.
 *
 * Input:
 *    tag		Specifies an identifier name for the new ACL, or
 *			it may be NULL when no name is required.
 * Returns:
 *			A new ACL structure.
 */

NSAPI_PUBLIC ACLHandle_t *
ACL_AclNew(NSErr_t *errp, char *tag )
{
ACLHandle_t *handle;

    handle = ( ACLHandle_t * ) PERM_CALLOC ( 1 * sizeof (ACLHandle_t) );
    if ( handle && tag ) {
        handle->tag = PERM_STRDUP( tag );    
        if ( handle->tag == NULL ) {
            PERM_FREE(handle);
            return(NULL);
        }
    }
    return(handle);
}

/*
 * Free up memory associated with and ACLHandle.  
 *
 * Input:
 *	acl	target acl
 */

NSAPI_PUBLIC void
ACL_AclDestroy(NSErr_t *errp, ACLHandle_t *acl )
{
ACLExprHandle_t    *handle;
ACLExprHandle_t    *tmp;

    if ( acl == NULL )
        return;

#ifdef OLDIRIX
    /* Bugfix: acl can be shared across multiple sprocs. */
    _MD_ATOMIC_DECREMENT(&acl->ref_count);
#else
    acl->ref_count--;
#endif

    if ( acl->ref_count )
        return;

    if ( acl->tag )
        PERM_FREE( acl->tag );

    if ( acl->las_name )
        PERM_FREE( acl->las_name );

    if ( acl->attr_name )
        PERM_FREE( acl->attr_name );

    handle = acl->expr_list_head;
    while ( handle ) {
        tmp = handle;
        handle = handle->expr_next;
        ACL_ExprDestroy( tmp );
    }

    PERM_FREE(acl);

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
 *    < 0		failure
 *    > 0		The number of acl's in the current list
 */

NSAPI_PUBLIC int
ACL_ExprAppend( NSErr_t *errp, ACLHandle_t *acl, 
        ACLExprHandle_t *expr )
{
    
    if ( acl == NULL || expr == NULL )
        return(ACLERRUNDEF);

    expr->acl_tag = acl->tag;

    if ( expr->expr_type == ACL_EXPR_TYPE_AUTH || 
         expr->expr_type == ACL_EXPR_TYPE_RESPONSE ) {
        expr->expr_number = -1;  // expr number isn't valid 
    } else {
        acl->expr_count++;
        expr->expr_number = acl->expr_count;
    }

    if ( acl->expr_list_head == NULL ) {
        acl->expr_list_head = expr;
        acl->expr_list_tail = expr;
    } else {
        acl->expr_list_tail->expr_next = expr;
        acl->expr_list_tail = expr;
    }

    return(acl->expr_count);
}

/*
 * Add authorization information to an ACL
 *
 * This function adds an authorization to a given ACL, based on the information
 * provided by the parameters.
 *
 * Input:
 *    errp		The error stack
 *    access_rights	strings which identify the access rights to be
 *			controlled by the generated expr.
 *    flags		processing flags
 *    allow		non-zero to allow the indicated rights, or zero to
 *			deny them.
 *    attr_expr		handle for an attribute expression, which may be
 *			obtained by calling ACL_ExprNew()
 * Returns:
 *    0			success
 *    < 0 		failure
 */

NSAPI_PUBLIC int 
ACL_AddPermInfo( NSErr_t *errp, ACLHandle_t *acl,
        char **access_rights,
        PFlags_t flags,
        int allow,
        ACLExprHandle_t *expr,
        char *tag )
{
    if ( acl == NULL || expr == NULL ) 
        return(ACLERRUNDEF);
    
    expr->expr_flags = flags;
    expr->expr_argv = (char **) access_rights;
    expr->expr_tag = PERM_STRDUP( tag );
    if ( expr->expr_tag == NULL )
        return(ACLERRNOMEM);
    return(ACL_ExprAppend( errp, acl, expr ));
}

/*
 * FUNCTION:    ACL_AclGetTag
 *
 * DESCRIPTION:
 *
 *      Returns the tag string associated with an ACL.
 *
 * ARGUMENTS:
 *
 *      acl                     - handle for an ACL
 *
 * RETURNS:
 *
 *      The return value is a pointer to the ACL tag string.
 */

NSAPI_PUBLIC const char *
ACL_AclGetTag(ACLHandle_t *acl)
{
    return (acl) ? (const char *)(acl->tag) : 0;
}

