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
 *  This grammar is intended to parse the version 3.0 
 *  and version 2.0 ACL text files and output an ACLListHandle_t 
 *  structure.
 */

%{
#include <string.h>
#include <netsite.h>
#include <base/util.h>
#include <base/plist.h>
#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/nserror.h>
#include "parse.h"
#include "aclscan.h"

#define MAX_LIST_SIZE 255
static ACLListHandle_t *curr_acl_list;	/* current acl list */
static ACLHandle_t *curr_acl;		/* current acl */
static ACLExprHandle_t *curr_expr;	/* current expression */
static PFlags_t	pflags;			/* current authorization flags */
static PList_t curr_auth_info;		/* current authorization method */

int
acl_PushListHandle(ACLListHandle_t *handle)
{
	curr_acl_list = handle;
	return(0);
}

static void
acl_string_lower(char *s)
{
    int     ii;
    int     len;

    len = strlen(s);
    for (ii = 0; ii < len; ii++)
        s[ii] = tolower(s[ii]);

    return;
}

%}

%union {
	char	*string;
	int	ival;
}

%token ACL_ABSOLUTE_TOK
%token ACL_ACL_TOK
%token ACL_ALLOW_TOK
%token ACL_ALWAYS_TOK
%token ACL_AND_TOK
%token ACL_AT_TOK
%token ACL_AUTHENTICATE_TOK
%token ACL_CONTENT_TOK
%token ACL_DEFAULT_TOK
%token ACL_DENY_TOK
%token ACL_GROUP_TOK
%token ACL_IN_TOK
%token ACL_INHERIT_TOK
%token ACL_NOT_TOK 
%token ACL_NULL_TOK
%token ACL_OR_TOK
%token <string> ACL_QSTRING_TOK
%token ACL_READ_TOK
%token ACL_TERMINAL_TOK
%token <string> ACL_VARIABLE_TOK
%token ACL_VERSION_TOK
%token ACL_WRITE_TOK
%token ACL_WITH_TOK

%token <ival> ACL_EQ_TOK
%token <ival> ACL_GE_TOK
%token <ival> ACL_GT_TOK
%token <ival> ACL_LE_TOK
%token <ival> ACL_LT_TOK
%token <ival> ACL_NE_TOK

%%

/*
 * There are no more 2.0 ACLs anymore...
 */
start:	| ACL_VERSION_TOK ACL_VARIABLE_TOK 
	{
		free($<string>2);
	}
	';' start_acl_v3
	;

/*
 ************************************************************ 
 * Parse version 3.0 ACL
 ************************************************************ 
 */

start_acl_v3: acl_list
	;

acl_list: acl
	| acl_list acl
	;

acl:	named_acl ';' body_list
	| named_acl ';'  
	;

named_acl: ACL_ACL_TOK ACL_VARIABLE_TOK 
	{
		curr_acl = ACL_AclNew(NULL, $<string>2);
		free($<string>2);
		if ( ACL_ListAppend(NULL, curr_acl_list, curr_acl, 0) < 0 ) {
			yyerror("Couldn't add ACL to list.");
			return(-1);
		}
	}
	| ACL_ACL_TOK ACL_QSTRING_TOK 
	{
		curr_acl = ACL_AclNew(NULL, $<string>2);
		free($<string>2);
		if ( ACL_ListAppend(NULL, curr_acl_list, curr_acl, 0) < 0 ) {
			yyerror("Couldn't add ACL to list.");
			return(-1);
		}
	}
	;

body_list: body
	| body body_list
	;

body: authenticate_statement ';'
	| authorization_statement ';'
	| deny_statement ';'
	;

deny_statement: 	
	ACL_ABSOLUTE_TOK ACL_DENY_TOK ACL_WITH_TOK 
	{
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_RESPONSE) ;
		if ( curr_expr == NULL ) {
			yyerror("ACL_ExprNew(deny) failed");
			return(-1);
		}
		if ( ACL_ExprAppend(NULL, curr_acl, curr_expr) < 0 ) {
			yyerror("Could not add authorization");
			return(-1);
		}
		if ( ACL_ExprSetPFlags(NULL, curr_expr,
                                        ACL_PFLAG_ABSOLUTE) < 0 ) {
                        yyerror("Could not set deny processing flags");
                        return(-1);
                }
	}
        deny_common
	| ACL_DENY_TOK ACL_WITH_TOK 
	{
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_RESPONSE) ;
		if ( curr_expr == NULL ) {
			yyerror("ACL_ExprNew(deny) failed");
			return(-1);
		}
		if ( ACL_ExprAppend(NULL, curr_acl, curr_expr) < 0 ) {
			yyerror("Could not add authorization");
			return(-1);
		}
	}
	deny_common
	;

deny_common: ACL_VARIABLE_TOK ACL_EQ_TOK ACL_QSTRING_TOK
	{
		acl_string_lower($<string>1);
                if ( ACL_ExprSetDenyWith(NULL, curr_expr, 
                                         $<string>1, $<string>3) < 0 ) {
                        yyerror("ACL_ExprSetDenyWith() failed");
                        return(-1);
                }
                free($<string>1);
                free($<string>3);
	}
	;

authenticate_statement: ACL_AUTHENTICATE_TOK 
	{
		pflags = 0;
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_AUTH) ;
		if ( curr_expr == NULL ) {
			yyerror("ACL_ExprNew(allow) failed");
			return(-1);
		}
                curr_auth_info = PListCreate(NULL, ACL_ATTR_INDEX_MAX, NULL, NULL);
		if ( ACL_ExprAddAuthInfo(curr_expr, curr_auth_info) < 0 ) {
			yyerror("Could not set authorization info");
			return(-1);
		}
	}
	'(' attribute_list ')' '{' parameter_list '}'
	{
		if ( ACL_ExprAppend(NULL, curr_acl, curr_expr) < 0 ) {
			yyerror("Could not add authorization");
			return(-1);
		}
	}
	;

attribute_list: attribute
	| attribute_list ',' attribute

attribute: ACL_VARIABLE_TOK
	{
		acl_string_lower($<string>1);
		if ( ACL_ExprAddArg(NULL, curr_expr, $<string>1) < 0 ) {
			yyerror("ACL_ExprAddArg() failed");
			return(-1);
		}
		free($<string>1);
	}
	;

parameter_list: parameter ';'
	| parameter ';' parameter_list
	;

parameter: ACL_VARIABLE_TOK ACL_EQ_TOK ACL_QSTRING_TOK
	{
		acl_string_lower($<string>1);
		if ( PListInitProp(curr_auth_info, 
                                   ACL_Attr2Index($<string>1), $<string>1, $<string>3, NULL) < 0 ) {
		}
		free($<string>1);
	}
	| ACL_VARIABLE_TOK ACL_EQ_TOK ACL_VARIABLE_TOK
	{
		acl_string_lower($<string>1);
		if ( PListInitProp(curr_auth_info, 
                                   ACL_Attr2Index($<string>1), $<string>1, $<string>3, NULL) < 0 ) {
		}
		free($<string>1);
	}
	;

authorization_statement: ACL_ALLOW_TOK 
	{
		pflags = 0;
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_ALLOW) ;
		if ( curr_expr == NULL ) {
			yyerror("ACL_ExprNew(allow) failed");
			return(-1);
		}
	}
	auth_common_action
	| ACL_DENY_TOK 
	{
		pflags = 0;
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_DENY) ;
		if ( curr_expr == NULL ) {
			yyerror("ACL_ExprNew(deny) failed");
			return(-1);
		}
	}
	auth_common_action
	;

auth_common_action: 
	{
		if ( ACL_ExprAppend(NULL, curr_acl, curr_expr) < 0 ) {
			yyerror("Could not add authorization");
			return(-1);
		}
	}
	auth_common
	{
		if ( ACL_ExprSetPFlags (NULL, curr_expr, pflags) < 0 ) {
			yyerror("Could not set authorization processing flags");
			return(-1);
		}
#if ACLDEBUG
		if ( ACL_ExprDisplay(curr_expr) < 0 ) {
			yyerror("ACL_ExprDisplay() failed");
			return(-1);
		}
		printf("Parsed authorization.\n");
#endif
	}
	;

auth_common: flag_list '(' args_list ')' expression 
	;

flag_list: 
	| ACL_ABSOLUTE_TOK
	{
		pflags = ACL_PFLAG_ABSOLUTE;
	}
	| ACL_ABSOLUTE_TOK content_static
	{
		pflags = ACL_PFLAG_ABSOLUTE;
	}
	| ACL_CONTENT_TOK 
	{
		pflags = ACL_PFLAG_CONTENT;
	}
	| ACL_CONTENT_TOK absolute_static
	{
		pflags = ACL_PFLAG_CONTENT;
	}
	| ACL_TERMINAL_TOK
	{
		pflags = ACL_PFLAG_TERMINAL;
	}
	| ACL_TERMINAL_TOK content_absolute
	{
		pflags = ACL_PFLAG_TERMINAL;
	}
	;

content_absolute: ACL_CONTENT_TOK
	{
		pflags |= ACL_PFLAG_CONTENT;
	}
	| ACL_ABSOLUTE_TOK
	{
		pflags |= ACL_PFLAG_ABSOLUTE;
	}
	| ACL_CONTENT_TOK ACL_ABSOLUTE_TOK
	{
		pflags |= ACL_PFLAG_ABSOLUTE | ACL_PFLAG_CONTENT;
	}
	| ACL_ABSOLUTE_TOK ACL_CONTENT_TOK
	{
		pflags |= ACL_PFLAG_ABSOLUTE | ACL_PFLAG_CONTENT;
	}
	;

content_static: ACL_CONTENT_TOK
	{
		pflags |= ACL_PFLAG_CONTENT;
	}
	| ACL_TERMINAL_TOK
	{
		pflags |= ACL_PFLAG_TERMINAL;
	}
	| ACL_CONTENT_TOK ACL_TERMINAL_TOK
	{
		pflags |= ACL_PFLAG_TERMINAL | ACL_PFLAG_CONTENT;
	}
	| ACL_TERMINAL_TOK ACL_CONTENT_TOK
	{
		pflags |= ACL_PFLAG_TERMINAL | ACL_PFLAG_CONTENT;
	}
	;

absolute_static: ACL_ABSOLUTE_TOK
	{
		pflags |= ACL_PFLAG_ABSOLUTE;
	}
	| ACL_TERMINAL_TOK
	{
		pflags |= ACL_PFLAG_TERMINAL;
	}
	| ACL_ABSOLUTE_TOK ACL_TERMINAL_TOK
	{
		pflags |= ACL_PFLAG_TERMINAL | ACL_PFLAG_ABSOLUTE;
	}
	| ACL_TERMINAL_TOK ACL_ABSOLUTE_TOK
	{
		pflags |= ACL_PFLAG_TERMINAL | ACL_PFLAG_ABSOLUTE;
	}
	;

args_list: arg
	| args_list ',' arg
	;

arg: ACL_VARIABLE_TOK 
	{
		acl_string_lower($<string>1);
		if ( ACL_ExprAddArg(NULL, curr_expr, $<string>1) < 0 ) {
			yyerror("ACL_ExprAddArg() failed");
			return(-1);
		}
		free( $<string>1 );
	}
	;

expression: factor
	| factor ACL_AND_TOK expression
        {
                if ( ACL_ExprAnd(NULL, curr_expr) < 0 ) {
                        yyerror("ACL_ExprAnd() failed");
                        return(-1);
                }
        }
	| factor ACL_OR_TOK expression
        {
                if ( ACL_ExprOr(NULL, curr_expr) < 0 ) {
                        yyerror("ACL_ExprOr() failed");
                        return(-1);
                }
        }
	;

factor: base_expr
	| '(' expression ')'
	| ACL_NOT_TOK factor
	{
                if ( ACL_ExprNot(NULL, curr_expr) < 0 ) {
                        yyerror("ACL_ExprNot() failed");
                        return(-1);
                }
        }
	;

base_expr: ACL_VARIABLE_TOK relop ACL_QSTRING_TOK 
	{
		acl_string_lower($<string>1);
		if ( ACL_ExprTerm(NULL, curr_expr,
				$<string>1, (CmpOp_t) $<ival>2, $<string>3) < 0 ) {
			yyerror("ACL_ExprTerm() failed");
			free($<string>1);
			free($<string>3);	
			return(-1);
		}
		free($<string>1);
		free($<string>3);	
	}
	| ACL_VARIABLE_TOK relop ACL_VARIABLE_TOK 
	{
		acl_string_lower($<string>1);
		if ( ACL_ExprTerm(NULL, curr_expr,
				$<string>1, (CmpOp_t) $<ival>2, $<string>3) < 0 ) {
			yyerror("ACL_ExprTerm() failed");
			free($<string>1);
			free($<string>3);	
			return(-1);
		}
		free($<string>1);
		free($<string>3);	
	}
	;

relop: ACL_EQ_TOK
	| ACL_GE_TOK
	| ACL_GT_TOK
	| ACL_LT_TOK
	| ACL_LE_TOK
	| ACL_NE_TOK
	;
%%
