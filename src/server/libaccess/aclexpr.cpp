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
#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/aclerror.h>

/*
 * Allocate a new expression handle.
 *
 * Returns:
 *     NULL		If handle could not be allocated.
 *     pointer		New handle.
 */ 

NSAPI_PUBLIC ACLExprHandle_t *
ACL_ExprNew( const ACLExprType_t expr_type )
{
ACLExprHandle_t    *expr_handle;

    expr_handle = ( ACLExprHandle_t * ) PERM_CALLOC ( sizeof(ACLExprHandle_t) );
    if ( expr_handle ) {
        expr_handle->expr_arry = ( ACLExprEntry_t * ) 
            PERM_CALLOC( ACL_TERM_BSIZE * sizeof(ACLExprEntry_t) ) ;
        expr_handle->expr_arry_size = ACL_TERM_BSIZE;
	expr_handle->expr_type = expr_type;

        expr_handle->expr_raw = ( ACLExprRaw_t * ) 
            PERM_CALLOC( ACL_TERM_BSIZE * sizeof(ACLExprRaw_t) ) ;
        expr_handle->expr_raw_size = ACL_TERM_BSIZE;

    }
    return(expr_handle);
}

/*
 * LOCAL FUNCTION
 *
 * Free up memory associated with and ACLExprEntry.  Probably
 * only useful internally since we aren't exporting 
 * this structure.
 */

static void
ACL_ExprEntryDestroy( ACLExprEntry_t *entry )
{
    LASFlushFunc_t flushp;
    
    if ( entry == NULL )
        return;
    
    if ( entry->las_cookie )
/*	freeLAS(NULL, entry->attr_name, &entry->las_cookie);		*/
    {
	ACL_LasFindFlush( NULL, entry->attr_name, &flushp );
	if ( flushp )
	    ( *flushp )( &entry->las_cookie );
    }

    if ( entry->attr_name )
        PERM_FREE( entry->attr_name );

    if ( entry->attr_pattern )
        PERM_FREE( entry->attr_pattern );

    return;
}

/*
 * LOCAL FUNCTION
 *
 * This function is used to free all the pvalue memory
 * in a plist.
 */

static void 
acl_expr_auth_destroy(char *pname, const void *pvalue, void *user_data)
{
    PERM_FREE((char *) pvalue);
    return;
}

/*
 * Free up memory associated with and ACLExprHandle.  
 *
 * Input:
 *	expr	expression handle to free up
 */

NSAPI_PUBLIC void
ACL_ExprDestroy( ACLExprHandle_t *expr )
{
int    ii;

    if ( expr == NULL )
        return;

    if ( expr->expr_tag )
        PERM_FREE( expr->expr_tag );

    if ( expr->expr_argv ) {
        for ( ii = 0; ii < expr->expr_argc; ii++ )
            if ( expr->expr_argv[ii] )
                PERM_FREE( expr->expr_argv[ii] );
        PERM_FREE( expr->expr_argv );
    }

    for ( ii = 0; ii < expr->expr_term_index; ii++ )
        ACL_ExprEntryDestroy( &expr->expr_arry[ii] );

    if ( expr->expr_auth ) {
	PListEnumerate(expr->expr_auth, acl_expr_auth_destroy, NULL);
        PListDestroy(expr->expr_auth);
    }

    PERM_FREE( expr->expr_arry );
    PERM_FREE( expr->expr_raw );

    PERM_FREE( expr );

    return;
}

/*
 * Add authentication information to an ACL
 *
 * This function adds authentication data to an expr, based on
 * the information provided by the parameters.
 *
 * Input:
 *	expr		an authenticate expression to add database
 *			and method information to. ie, auth_info
 *	auth_info	authentication information, eg database, 
 *			method, etc.
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int 
ACL_ExprAddAuthInfo( ACLExprHandle_t *expr, PList_t auth_info )
{
    if ( expr == NULL || auth_info == NULL )
        return(ACLERRUNDEF);

    expr->expr_auth = auth_info;

    return(0);
}

/*
 * Add rights information to an expression
 *
 * This function adds a right to an authorization, based on the information
 * provided by the parameters.
 *
 * Input:
 *    errp		The error stack
 *    access_right	strings which identify the access rights to be
 *			controlled by the generated expr.
 *    expr		handle for an attribute expression, which may be
 *			obtained by calling ACL_ExprNew()
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_ExprAddArg( NSErr_t *errp, 
    ACLExprHandle_t *expr, 
    char *arg )
{

    if ( expr == NULL ) 
        return(ACLERRUNDEF);

    if (expr->expr_argv == NULL)
        expr->expr_argv = (char **) PERM_MALLOC( 2 * sizeof(char *) );
    else
        expr->expr_argv = (char **) PERM_REALLOC( expr->expr_argv,
                                        (expr->expr_argc+2)
                                        * sizeof(char *) );
    
    if (expr->expr_argv == NULL) 
        return(ACLERRNOMEM);
    
    expr->expr_argv[expr->expr_argc] = PERM_STRDUP( arg );
    if (expr->expr_argv[expr->expr_argc] == NULL) 
        return(ACLERRNOMEM);
    expr->expr_argc++;
    expr->expr_argv[expr->expr_argc] = NULL;
    
    return(0);

}


NSAPI_PUBLIC int
ACL_ExprSetDenyWith( NSErr_t *errp, ACLExprHandle_t *expr, char *deny_type, char *deny_response)
{
int rv;

    if ( expr->expr_argc == 0 ) {
       if ( (rv = ACL_ExprAddArg(errp, expr, deny_type)) < 0 ) 
           return(rv);
       if ( (rv = ACL_ExprAddArg(errp, expr, deny_response)) < 0 ) 
           return(rv);
    } else if ( expr->expr_argc == 2 ) {
       if ( deny_type ) {
           if ( expr->expr_argv[0] ) 
               PERM_FREE(expr->expr_argv[0]);
           expr->expr_argv[0] = PERM_STRDUP(deny_type);
           if ( expr->expr_argv[0] == NULL )
               return(ACLERRNOMEM);
       }
       if ( deny_response ) {
           if ( expr->expr_argv[1] ) 
               PERM_FREE(expr->expr_argv[1]);
           expr->expr_argv[1] = PERM_STRDUP(deny_response);
           if ( expr->expr_argv[0] == NULL )
               return(ACLERRNOMEM);
       }
    } else {
        return(ACLERRINTERNAL);
    }
    return(0);
}

NSAPI_PUBLIC int
ACL_ExprGetDenyWith( NSErr_t *errp, ACLExprHandle_t *expr, char **deny_type,
char **deny_response)
{
    if ( expr->expr_argc == 2 ) {
        *deny_type = expr->expr_argv[0];
        *deny_response = expr->expr_argv[1];
        return(0);
    } else {
        return(ACLERRUNDEF);
    }
}

/*
 * Function to set the authorization statement processing flags.
 *
 * Input:
 *	errp	The error reporting stack
 *	expr	The authoization statement
 *	flags	The flags to set
 * Returns:
 *	0	success
 *	< 0	failure
 */

NSAPI_PUBLIC int
ACL_ExprSetPFlags( NSErr_t *errp,
    ACLExprHandle_t *expr,
    PFlags_t flags )
{
    if ( expr == NULL )
        return(ACLERRUNDEF);

    expr->expr_flags |= flags;
    return(0);
}
        
/*
 * Function to clear the authorization statement processing flags.
 *
 * Input:
 *	errp	The error reporting stack
 *	expr	The authoization statement
 * Returns:
 *	0	success
 *	< 0	failure
 */

NSAPI_PUBLIC int
ACL_ExprClearPFlags( NSErr_t *errp,
    ACLExprHandle_t *expr )
{
    if ( expr == NULL )
        return(ACLERRUNDEF);

    expr->expr_flags = 0;
    return(0);
}
        
/*
 * LOCAL FUNCTION
 *
 * displays the ASCII equivalent index value.
 */

static char *
acl_index_string ( int value, char *buffer )
{
	
    if ( value == ACL_TRUE_IDX ) {
        strcpy( buffer, "TRUE" );
        return( buffer );
    }	

    if ( value == ACL_FALSE_IDX ) {
        strcpy( buffer, "FALSE" );
        return( buffer );
    }	

    sprintf( buffer, "goto %d", value );
    return( buffer );
}
    
    
/*
 * LOCAL FUNCTION
 *
 * displays ASCII equivalent of CmpOp_t
 */

char *
acl_comp_string( CmpOp_t cmp )
{
    switch (cmp) {
    case CMP_OP_EQ:
        return("=");
    case CMP_OP_NE:
        return("!=");
    case CMP_OP_GT:
        return(">");
    case CMP_OP_LT:
        return("<");
    case CMP_OP_GE:
        return(">=");
    case CMP_OP_LE:
        return("<=");
    default:
        return("unknown op");
    }
}

/*
 * Add a term to the specified attribute expression.  
 *
 * Input:
 *    errp		Error stack
 *    acl_expr		Target expression handle
 *    attr_name		Term Attribute name 
 *    cmp		Comparison operator
 *    attr_pattern	Pattern for comparison
 * Ouput:
 *    acl_expr		New term added
 * Returns:
 *    0			Success
 *    < 0		Error
 */

NSAPI_PUBLIC int 
ACL_ExprTerm( NSErr_t *errp, ACLExprHandle_t *acl_expr,
        char *attr_name, 
        CmpOp_t cmp, 
        char *attr_pattern )
{
ACLExprEntry_t	*expr;
ACLExprRaw_t	*raw_expr;

    if ( acl_expr == NULL || acl_expr->expr_arry == NULL )
        return(ACLERRUNDEF);

    if ( acl_expr->expr_term_index >= acl_expr->expr_arry_size  ) {
        acl_expr->expr_arry = ( ACLExprEntry_t *) 
            PERM_REALLOC ( acl_expr->expr_arry, 
			   (acl_expr->expr_arry_size + ACL_TERM_BSIZE)
			   * sizeof(ACLExprEntry_t));
        if ( acl_expr->expr_arry == NULL )
            return(ACLERRNOMEM); 
        acl_expr->expr_arry_size += ACL_TERM_BSIZE;
    }

    expr = &acl_expr->expr_arry[acl_expr->expr_term_index];
    acl_expr->expr_term_index++;
    
    expr->attr_name = PERM_STRDUP(attr_name);
    if ( expr->attr_name == NULL )
        return(ACLERRNOMEM);
    expr->comparator = cmp; 
    expr->attr_pattern = PERM_STRDUP(attr_pattern);
    if ( expr->attr_pattern == NULL )
        return(ACLERRNOMEM);
    expr->true_idx = ACL_TRUE_IDX;
    expr->false_idx = ACL_FALSE_IDX;
    expr->start_flag = 1;
    expr->las_cookie = 0;
    expr->las_eval_func = 0;

    if ( acl_expr->expr_raw_index >= acl_expr->expr_raw_size  ) {
        acl_expr->expr_raw = ( ACLExprRaw_t *) 
            PERM_REALLOC ( acl_expr->expr_raw, 
			   (acl_expr->expr_raw_size + ACL_TERM_BSIZE)
			   * sizeof(ACLExprRaw_t));
        if ( acl_expr->expr_raw == NULL )
            return(ACLERRNOMEM); 
        acl_expr->expr_raw_size += ACL_TERM_BSIZE;
    }
    
    raw_expr = &acl_expr->expr_raw[acl_expr->expr_raw_index];
    acl_expr->expr_raw_index++;

    raw_expr->attr_name = expr->attr_name;
    raw_expr->comparator = cmp;
    raw_expr->attr_pattern = expr->attr_pattern;
    raw_expr->logical = (ACLExprOp_t)0;

#ifdef DEBUG_LEVEL_2
    printf ( "%d: %s %s %s, t=%d, f=%d\n",
            acl_expr->expr_term_index - 1,
            expr->attr_name,
            acl_comp_string( expr->comparator ),
            expr->attr_pattern,
            expr->true_idx,
            expr->false_idx );
#endif

    return(0);
}

/*
 * Negate the previous term or subexpression.
 *
 * Input:
 *    errp		The error stack
 *    acl_expr		The expression to negate
 * Ouput
 *    acl_expr		The negated expression
 * Returns:
 *    0			Success
 *    < 0		Failure
 */

NSAPI_PUBLIC int 
ACL_ExprNot( NSErr_t *errp, ACLExprHandle_t *acl_expr )
{
int		idx;
int		ii;
int		expr_one = 0;
ACLExprRaw_t	*raw_expr;

    if ( acl_expr == NULL )
        return(ACLERRUNDEF);


    if ( acl_expr->expr_raw_index >= acl_expr->expr_raw_size  ) {
        acl_expr->expr_raw = ( ACLExprRaw_t *) 
            PERM_REALLOC ( acl_expr->expr_raw, 
			   (acl_expr->expr_raw_size + ACL_TERM_BSIZE)
			   * sizeof(ACLExprRaw_t));
        if ( acl_expr->expr_raw == NULL )
            return(ACLERRNOMEM); 
        acl_expr->expr_raw_size += ACL_TERM_BSIZE;
    }
    
    raw_expr = &acl_expr->expr_raw[acl_expr->expr_raw_index];
    acl_expr->expr_raw_index++;

    raw_expr->logical = ACL_EXPR_OP_NOT;
    raw_expr->attr_name = NULL;

    /* Find the last expression */
    idx = acl_expr->expr_term_index - 1;
    for ( ii = idx; ii >= 0; ii-- ) {
        if ( acl_expr->expr_arry[ii].start_flag ) {
	    expr_one = ii;
	    break;
	}
    } 

#ifdef DEBUG_LEVEL_2
    printf("not, start index=%d\n", expr_one);
#endif


    /*
     * The intent here is negate the last expression by
     * modifying the true and false links.
     */

    for ( ii = expr_one; ii < acl_expr->expr_term_index; ii++ ) {
        if ( acl_expr->expr_arry[ii].true_idx == ACL_TRUE_IDX ) 
            acl_expr->expr_arry[ii].true_idx = ACL_FALSE_IDX; 
        else if ( acl_expr->expr_arry[ii].true_idx == ACL_FALSE_IDX ) 
            acl_expr->expr_arry[ii].true_idx = ACL_TRUE_IDX; 

        if ( acl_expr->expr_arry[ii].false_idx == ACL_TRUE_IDX ) 
            acl_expr->expr_arry[ii].false_idx = ACL_FALSE_IDX; 
        else if ( acl_expr->expr_arry[ii].false_idx == ACL_FALSE_IDX ) 
            acl_expr->expr_arry[ii].false_idx = ACL_TRUE_IDX; 

    }

    return(0) ;
}

/*
 * Logical 'and' the previous two terms or subexpressions.
 *
 * Input:
 *    errp		The error stack
 *    acl_expr		The terms or subexpressions
 * Output:
 *    acl_expr		The expression after logical 'and'
 */

NSAPI_PUBLIC int 
ACL_ExprAnd( NSErr_t *errp, ACLExprHandle_t *acl_expr )
{
int    		idx;
int		ii;
int		expr_one = ACL_FALSE_IDX;
int		expr_two = ACL_FALSE_IDX;
ACLExprRaw_t 	*raw_expr;

    if ( acl_expr == NULL )
        return(ACLERRUNDEF);

    if ( acl_expr->expr_raw_index >= acl_expr->expr_raw_size  ) {
        acl_expr->expr_raw = ( ACLExprRaw_t *) 
            PERM_REALLOC ( acl_expr->expr_raw, 
			   (acl_expr->expr_raw_size + ACL_TERM_BSIZE)
			   * sizeof(ACLExprRaw_t) );
        if ( acl_expr->expr_raw == NULL )
            return(ACLERRNOMEM); 
        acl_expr->expr_raw_size += ACL_TERM_BSIZE;
    }
    
    raw_expr = &acl_expr->expr_raw[acl_expr->expr_raw_index];
    acl_expr->expr_raw_index++;

    raw_expr->logical = ACL_EXPR_OP_AND;
    raw_expr->attr_name = NULL;

    /* Find the last two expressions */
    idx = acl_expr->expr_term_index - 1;
    for ( ii = idx; ii >= 0; ii-- ) {
        if ( acl_expr->expr_arry[ii].start_flag ) {
	    if ( expr_two == ACL_FALSE_IDX )
                expr_two = ii;
            else if ( expr_one == ACL_FALSE_IDX ) {
                expr_one = ii;
                break;
            }
	}
    } 

#ifdef DEBUG_LEVEL_2
    printf("and, index=%d, first expr=%d, second expr=%d\n", idx, expr_one, expr_two);
#endif

    for ( ii = expr_one; ii < expr_two; ii++) {
        if ( acl_expr->expr_arry[ii].true_idx == ACL_TRUE_IDX )
            acl_expr->expr_arry[ii].true_idx = expr_two;
        if ( acl_expr->expr_arry[ii].false_idx == ACL_TRUE_IDX )
            acl_expr->expr_arry[ii].false_idx = expr_two;
    }

    acl_expr->expr_arry[expr_two].start_flag = 0; 
    return(0);
}

/*
 * Logical 'or' the previous two terms or subexpressions.
 *
 * Input:
 *    errp		The error stack
 *    acl_expr		The terms or subexpressions
 * Output:
 *    acl_expr		The expression after logical 'or'
 */

NSAPI_PUBLIC int 
ACL_ExprOr( NSErr_t *errp, ACLExprHandle_t *acl_expr )
{
int    		idx;
int		ii;
int		expr_one = ACL_FALSE_IDX;
int		expr_two = ACL_FALSE_IDX;
ACLExprRaw_t 	*raw_expr;

    if ( acl_expr == NULL )
        return(ACLERRUNDEF);

    if ( acl_expr->expr_raw_index >= acl_expr->expr_raw_size  ) {
        acl_expr->expr_raw = ( ACLExprRaw_t *) 
            PERM_REALLOC ( acl_expr->expr_raw, 
			   (acl_expr->expr_raw_size + ACL_TERM_BSIZE)
			   * sizeof(ACLExprRaw_t) );
        if ( acl_expr->expr_raw == NULL )
            return(ACLERRNOMEM); 
        acl_expr->expr_raw_size += ACL_TERM_BSIZE;
    }
    
    raw_expr = &acl_expr->expr_raw[acl_expr->expr_raw_index];
    acl_expr->expr_raw_index++;

    raw_expr->logical = ACL_EXPR_OP_OR;
    raw_expr->attr_name = NULL;

    /* Find the last two expressions */
    idx = acl_expr->expr_term_index - 1;
    for ( ii = idx; ii >= 0; ii-- ) {
        if ( acl_expr->expr_arry[ii].start_flag ) {
	    if ( expr_two == ACL_FALSE_IDX )
                expr_two = ii;
            else if ( expr_one == ACL_FALSE_IDX ) {
                expr_one = ii;
                break;
            }
	}
    } 

#ifdef DEBUG_LEVEL_2
    printf("or, index=%d, first expr=%d, second expr=%d\n", idx, expr_one, expr_two);
#endif

    for ( ii = expr_one; ii < expr_two; ii++) {
        if ( acl_expr->expr_arry[ii].true_idx == ACL_FALSE_IDX )
		acl_expr->expr_arry[ii].true_idx = expr_two;
        if ( acl_expr->expr_arry[ii].false_idx == ACL_FALSE_IDX )
		acl_expr->expr_arry[ii].false_idx = expr_two;
    } 
    acl_expr->expr_arry[expr_two].start_flag = 0; 

    return(0);
}

/*
 * INTERNAL FUNCTION (GLOBAL)
 *
 * Write an expression array to standard output.  This
 * is only useful debugging.
 */

int 
ACL_ExprDisplay( ACLExprHandle_t *acl_expr )
{
int    ii;
char   buffer[256];

    if ( acl_expr == NULL )
        return(0);

    for ( ii = 0; ii < acl_expr->expr_term_index; ii++ ) {
        printf ("%d: if ( %s %s %s ) ",
            ii,
            acl_expr->expr_arry[ii].attr_name,
            acl_comp_string( acl_expr->expr_arry[ii].comparator ),
            acl_expr->expr_arry[ii].attr_pattern );

        printf("%s ", acl_index_string(acl_expr->expr_arry[ii].true_idx, buffer));
        printf("else %s\n", 
		acl_index_string(acl_expr->expr_arry[ii].false_idx, buffer) );
    }

    return(0);
}

