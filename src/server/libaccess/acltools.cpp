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

/***************************************************************************

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 1996 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

****************************************************************************/

/*
 * Tools to build and maintain access control lists.
 */

#include <stdio.h>
#include <string.h>
#include <netsite.h>
#include <base/crit.h>

#define ALLOCATE_ATTR_TABLE     1       /* Include the table of PList names */

#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/aclerror.h>
#include <libaccess/dbtlibaccess.h>

#include "aclscan.h"
#include "parse.h"

static CRITICAL 	acl_parse_crit = NULL;

//
// ACL_ModuleRegister - register and initialize an access control module
//
// in reality, all this does is call the init function - no registration going on here
//
NSAPI_PUBLIC int
ACL_ModuleRegister (NSErr_t *errp, const char *module_name, AclModuleInitFunc func)
{
    int rv;

    if (!module_name || !*module_name) {
	nserrGenerate(errp, ACLERRFAIL, ACLERR4200, ACL_Program, 1,
	              XP_GetAdminStr(DBT_ModuleRegisterModuleNameMissing));
	return -1;
    }

    rv = (*func)(errp);

    if (rv < 0) {
	nserrGenerate(errp, ACLERRFAIL, ACLERR4210, ACL_Program, 2,
	              XP_GetAdminStr(DBT_ModuleRegisterFailed), module_name);
	return rv;
    }

    return 0;
}

/*
 * LOCAL FUNCTION
 *
 * Convert sub-expression to string. 
 */

static int
acl_expr_string( ACLExprOp_t logical, ACLExprStack_t *expr_stack )
{
char 		**expr_text;
char 		**prev_expr_text;
char 		*tmp;

    switch (logical) {
    case ACL_EXPR_OP_NOT:
        if ( expr_stack->stack_index < 1 ) {
            printf("expression stack underflow.\n");
            return(ACLERRINTERNAL);
        }

        expr_text = &expr_stack->expr_text[expr_stack->stack_index - 1];
        tmp = (char *) PERM_MALLOC(strlen(*expr_text) + 7);
        if ( tmp == NULL )
            return(ACLERRNOMEM);

        if ( expr_stack->found_subexpression ) {
	    sprintf(tmp, "not (%s)", *expr_text);
            expr_stack->found_subexpression = 0; 
            expr_stack->last_subexpression = expr_stack->stack_index - 1; 
	} else {
	    sprintf(tmp, "not %s", *expr_text);
	}

        PERM_FREE(*expr_text);
        *expr_text = tmp;
        return(0);

    case ACL_EXPR_OP_AND:
    case ACL_EXPR_OP_OR:
        if ( expr_stack->stack_index < 2 ) {
            printf("expression stack underflow.\n");
            return(ACLERRINTERNAL);
        }
    
        expr_stack->stack_index--;
        prev_expr_text = &expr_stack->expr_text[expr_stack->stack_index];
        expr_stack->stack_index--;
        expr_text = &expr_stack->expr_text[expr_stack->stack_index];
    
        tmp = (char *) PERM_MALLOC (strlen(*expr_text) 
                                + strlen(*prev_expr_text) + 15);
        if ( tmp == NULL ) 
            return(ACLERRNOMEM);
    
        if ( expr_stack->found_subexpression &&
             expr_stack->stack_index == expr_stack->last_subexpression &&
             logical == ACL_EXPR_OP_AND ) {
            sprintf(tmp, "%s and\n    (%s)", *expr_text, *prev_expr_text);
        } else if ( expr_stack->found_subexpression &&
             expr_stack->stack_index == expr_stack->last_subexpression ) { 
            sprintf(tmp, "%s or\n    (%s)", *expr_text, *prev_expr_text);
        } else if ( logical == ACL_EXPR_OP_AND ) {
            sprintf(tmp, "%s and\n    %s", *expr_text, *prev_expr_text);
        } else {
            sprintf(tmp, "%s or\n    %s", *expr_text, *prev_expr_text);
        }
    
	expr_stack->found_subexpression++;
        expr_stack->stack_index++;
        PERM_FREE(*expr_text);
        PERM_FREE(*prev_expr_text);
        *expr_text = tmp;
        *prev_expr_text = NULL;
        return(0);

    default:
        printf("Bad boolean logic value.\n");
        return(ACLERRINTERNAL);
    }

}

/*
 * LOCAL FUNCTION
 *
 * Reduce all sub-expressions to a single string. 
 */

static int
acl_reduce_expr_logic( ACLExprStack_t *expr_stack, ACLExprRaw_t *expr_raw ) 
{
char 		**expr_text;
char 		**prev_expr_text;
char		*tmp;

    if (expr_raw->attr_name) {
        if (expr_stack->stack_index >= ACL_EXPR_STACK ) {
            printf("expression stack overflow.");
            return(ACLERRINTERNAL);
        }

        if ( expr_stack->found_subexpression && expr_stack->stack_index > 0 ) {
            prev_expr_text = &expr_stack->expr_text[expr_stack->stack_index-1];
            tmp = (char *) PERM_MALLOC(strlen(*prev_expr_text) + 3);
            sprintf(tmp, "(%s)", *prev_expr_text);
            PERM_FREE(*prev_expr_text);
            *prev_expr_text = tmp;
            expr_stack->found_subexpression = 0;
            expr_stack->last_subexpression = expr_stack->stack_index - 1; 
        }

        expr_stack->expr[expr_stack->stack_index] = expr_raw;
        expr_text = &expr_stack->expr_text[expr_stack->stack_index];
        *expr_text = (char *) PERM_MALLOC(strlen(expr_raw->attr_name)
			       + strlen(expr_raw->attr_pattern)
			       + 7);
        if ( *expr_text == NULL )
            return(ACLERRNOMEM);

        sprintf(*expr_text, "%s %s \"%s\"", expr_raw->attr_name,
	        acl_comp_string(expr_raw->comparator),
	        expr_raw->attr_pattern); 

        expr_stack->stack_index++;
        expr_stack->expr_text[expr_stack->stack_index] = NULL;
    } else {
        return(acl_expr_string(expr_raw->logical, expr_stack));
    }
    return(0);
}

/*
 * LOCAL FUNCTION
 *
 * Appends str2 to str1.
 *
 * Input:
 *	str1	an existing dynamically allocated string
 * 	str2	a text string
 * Returns:
 *	0	success
 *	< 0	failure
 */

int
acl_to_str_append(acl_string_t * p_aclstr, const char *str2)
{
    int str2len, newlen;

    if (p_aclstr == NULL || str2 == NULL) 
        return (ACLERRINTERNAL);
    if (p_aclstr->str == NULL) {
        p_aclstr->str = (char *) PERM_MALLOC(4096);
        if (p_aclstr->str == NULL)
            return (ACLERRNOMEM);
        p_aclstr->str_size = 4096;
        p_aclstr->str_len  = 0;
    }
    
    str2len = strlen(str2);
    newlen = p_aclstr->str_len + str2len;
    if (newlen >= p_aclstr->str_size) {
        p_aclstr->str_size = str2len > 4095 ? str2len+p_aclstr->str_size+1 : 4096+p_aclstr->str_size ; 
        p_aclstr->str = (char *) PERM_REALLOC(p_aclstr->str, p_aclstr->str_size);
        if (p_aclstr->str == NULL)
            return (ACLERRNOMEM);
    }
    memcpy((void *)&(p_aclstr->str[p_aclstr->str_len]), (void *) str2, str2len+1);
    p_aclstr->str_len += str2len;
    return 0;
}

/*
 * LOCAL FUNCTION
 *
 * Output Authorization Expression type either "Allow" or "Deny"
 */

static int
acl_to_str_expr_type( acl_string_t *str_t, ACLExprHandle_t *expr )
{
    switch (expr->expr_type) {
    case ACL_EXPR_TYPE_ALLOW:
        acl_to_str_append(str_t, "allow ");
        if ( IS_ABSOLUTE(expr->expr_flags) )
            acl_to_str_append(str_t, "absolute ");
        return(0);
    case ACL_EXPR_TYPE_DENY:
        acl_to_str_append(str_t, "deny ");
        if ( IS_ABSOLUTE(expr->expr_flags) )
            acl_to_str_append(str_t, "absolute ");
        return(0);
    case ACL_EXPR_TYPE_AUTH:
        acl_to_str_append(str_t, "authenticate ");
        if ( IS_ABSOLUTE(expr->expr_flags) )
            acl_to_str_append(str_t, "absolute ");
        return(0);
    case ACL_EXPR_TYPE_RESPONSE:
        acl_to_str_append(str_t, "deny with ");
        return(0);
    default:
        return(ACLERRINTERNAL);
    }
}

/*
 * LOCAL FUNCTION
 *
 * Output Authorization Expression Rights "(right, right)" 
 */

static int
acl_to_str_expr_arg( acl_string_t *str_t, ACLExprHandle_t *expr )
{
int 		ii;

    if ( expr->expr_argc <= 0 ) {
        return(ACLERRINTERNAL);
    }

    if ( expr->expr_type == ACL_EXPR_TYPE_RESPONSE ) {
	acl_to_str_append(str_t, expr->expr_argv[0]);
	acl_to_str_append(str_t, "=\"");
	acl_to_str_append(str_t, expr->expr_argv[1]);
        acl_to_str_append(str_t, "\";\n");
        return(0);
    }
   
    acl_to_str_append(str_t, "(");
    for (ii = 0; ii < expr->expr_argc; ii++) {
        acl_to_str_append(str_t, expr->expr_argv[ii]);
        if ( ii < expr->expr_argc - 1 ) {
            acl_to_str_append(str_t, ",");
        }
    }
    acl_to_str_append(str_t, ") ");

    return(0);
}

/*
 * LOCAL FUNCTION
 *
 * Walks through the authentication statement PList_t and
 * prints the structure to a string.
 */

static void 
acl_to_str_auth_expr(char *lval, const void *rval, void *user_data)
{
    acl_string_t * p_aclstr = (acl_string_t *) user_data;

    acl_to_str_append(p_aclstr, "\t");
    acl_to_str_append(p_aclstr, lval);
    acl_to_str_append(p_aclstr, " = \"");
    acl_to_str_append(p_aclstr, (char *) rval);
    acl_to_str_append(p_aclstr, "\";\n");

    return;
}

/*
 * LOCAL FUNCTION
 *
 * Output the logic part of the authencation statement to a string. 
 */

static int
acl_to_str_auth_logic( acl_string_t *str_t, ACLExprHandle_t *expr)
{

    if ( expr->expr_auth == NULL ) {
        acl_to_str_append(str_t, "{\n");
        acl_to_str_append(str_t, "# Authenticate statement with no body?\n");
        acl_to_str_append(str_t, "\tnull=null;\n");
        acl_to_str_append(str_t, "};\n");
        return(0);
    }

    acl_to_str_append(str_t, "{\n");
    PListEnumerate(expr->expr_auth, acl_to_str_auth_expr, (void *) str_t);
    acl_to_str_append(str_t, "};\n");

    return(0);
}


/*
 * LOCAL FUNCTION
 *
 * Output the logic part of the authorization statement to a string. 
 */

static int
acl_to_str_expr_logic( acl_string_t *str_t, ACLExprHandle_t *expr, ACLExprStack_t *expr_stack)
{
int 		rv = 0;
int 		ii;

    expr_stack->stack_index = 0;
    expr_stack->found_subexpression = 0;
    expr_stack->last_subexpression = -1;

    for (ii = 0; ii < expr->expr_raw_index; ii++) {
        rv = acl_reduce_expr_logic(expr_stack, &expr->expr_raw[ii]);
        if (rv) break;
    }

    if (!rv && expr_stack->expr_text[0]) {
        acl_to_str_append(str_t, "\n    ");
        acl_to_str_append(str_t, expr_stack->expr_text[0]);
        acl_to_str_append(str_t, ";\n");
        PERM_FREE(expr_stack->expr_text[0]);
    }

    return(rv);
}

/*
 * LOCAL FUNCTION
 *
 * Output an ACL list to a string. 
 */

static int
acl_to_str_create( acl_string_t *str_t, ACLListHandle_t *acl_list )
{
ACLWrapper_t 	*wrap;
ACLHandle_t 	*acl;
ACLExprHandle_t *expr;
int 		rv = 0;
ACLExprStack_t 	*expr_stack;

    expr_stack = (ACLExprStack_t *) PERM_MALLOC(sizeof(ACLExprStack_t));
    if ( expr_stack == NULL )
        return(ACLERRNOMEM);

    acl_to_str_append(str_t, "# File automatically written\n");
    acl_to_str_append(str_t, "#\n");
    acl_to_str_append(str_t, "# You may edit this file by hand\n");
    acl_to_str_append(str_t, "#\n\n");
    if ( acl_list->acl_list_head == NULL ) {
        PERM_FREE(expr_stack);
        return(0);
    }

    acl_to_str_append(str_t, "version 3.0;\n");
    for (wrap = acl_list->acl_list_head; wrap && !rv; 
         wrap = wrap->wrap_next ) {
        acl = wrap->acl;
        if ( acl->tag ) {
            acl_to_str_append(str_t,  "\nacl \"");
            acl_to_str_append(str_t, acl->tag);
            acl_to_str_append(str_t, "\";\n");
        } else {
            acl_to_str_append(str_t, "\nacl;\n");
        }
            
        for (expr = acl->expr_list_head; expr && rv == 0; 
             expr = expr->expr_next ) {

            if ( (rv = acl_to_str_expr_type(str_t, expr)) < 0 ) 
                break;

            if ( (rv = acl_to_str_expr_arg(str_t, expr)) < 0) 
                break;

            switch (expr->expr_type) {
            case ACL_EXPR_TYPE_DENY:
            case ACL_EXPR_TYPE_ALLOW:
                rv = acl_to_str_expr_logic(str_t, expr, expr_stack);
                break;
            case ACL_EXPR_TYPE_AUTH:
                rv = acl_to_str_auth_logic(str_t, expr);
                break;
            case ACL_EXPR_TYPE_RESPONSE:
                break;
	    }

        }
    }

    PERM_FREE(expr_stack);
    return(rv);
}


/*
 * Creates an ACL text string from an ACL handle
 *
 * Input:
 *	errp		error stack	
 *	acl		target text string pointer 
 *	acl_list	Source ACL list handle
 * Ouput:
 *	acl		a chunk of dynamic memory pointing to ACL text
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_WriteString(NSErr_t *errp, char **acl, ACLListHandle_t *acl_list)
{
    int rv;
    acl_string_t str_t = {NULL,0,0};

    if ( acl_list == NULL || acl == NULL )
        return(ACLERRUNDEF);

    rv = acl_to_str_create(&str_t, acl_list);
    *acl = str_t.str;

    return ( rv );
}

/*
 * Write an ACL text file from an input ACL list structure.
 *
 * Input:
 *	filename 	name for the output text file
 *	acl_list 	a list of ACLs to convert to text
 * Output:
 *	errp 		an error stack, set if there are errors 
 *			to report
 * Returns:
 * 	0		success
 *	ACLERROPEN, 
 *	ACLERRNOMEM 	on failure
 */

NSAPI_PUBLIC int
ACL_WriteFile( NSErr_t *errp, char *filename, ACLListHandle_t *acl_list )
{
int		rv;
int		eid;
char		*errmsg;
#ifdef UTEST
FILE		*ofp;
#else
SYS_FILE	ofp;
#endif
acl_string_t    aclstr = {NULL,0,0};
char		*acl_text = NULL;

    if ( filename == NULL || acl_list == NULL ) {
        rv = ACLERROPEN;
	eid = ACLERR1900;
        errmsg = system_errmsg(); // Will this be set? XXX RSS
        nserrGenerate(errp, rv, eid, ACL_Program, 2, filename, errmsg);
	return(ACLERROPEN);
    }

#ifdef UTEST
    ofp = fopen(filename, "w");
    if ( ofp == NULL ) {
#else
    ofp = system_fopenWT(filename);
    if ( ofp == SYS_ERROR_FD ) {
#endif
        rv = ACLERROPEN;
	eid = ACLERR1900;
        errmsg = system_errmsg();
        nserrGenerate(errp, rv, eid, ACL_Program, 2, filename, errmsg);
	return(ACLERROPEN);
    }

    rv = acl_to_str_create(&aclstr, acl_list); 
    acl_text = aclstr.str;

    if ( rv ) {
        eid = ACLERR3000;
        rv = ACLERRNOMEM;
        nserrGenerate(errp, rv, eid, ACL_Program, 0);
    } else {
#ifdef UTEST
	if (fputs(acl_text, ofp) == 0) {
#else
        if (system_fwrite_atomic(ofp, acl_text, strlen(acl_text))==IO_ERROR) {
#endif
             eid = ACLERR3200;
             rv = ACLERRIO;
             errmsg = system_errmsg();
             nserrGenerate(errp, rv, eid, ACL_Program, 2, filename, errmsg);
        }
    }

    if ( acl_text ) 
        PERM_FREE(acl_text);

#ifdef UTEST
    fclose(ofp);
#else
    system_fclose(ofp);
#endif

    return(rv);
}

/*
 * local function: translate string to lower case
 * return <0: fail
 *        0: succeed
 */
int 
open_file_buf(FILE ** file, char * filename, char *mode, char ** buf, long * size)
{
	int rv = 0;
	long cur = 0;
	long in = 0;
	struct stat fi;

	if (filename==NULL || mode==NULL) {
		rv = ACLERROPEN;
		goto open_cleanup;
	}

	if ((*file=fopen(filename,mode))==NULL) {
		rv = ACLERROPEN;
		goto open_cleanup;
	}

	if (system_stat(filename, &fi)==-1) {
		rv = ACLERROPEN;
		goto open_cleanup;
	}

	*size = fi.st_size;
	
	if ((*buf=(char *)PERM_MALLOC(*size+1))==NULL) {
		rv = ACLERRNOMEM;
		goto open_cleanup;
	}

	
	rv = 0;
	while (cur<*size) {
		in=fread(&(*buf)[cur], 1, *size, *file);
		cur = cur+in;
		if (feof(*file)) {
			break;
		}
		if (ferror(*file)) {
			rv = ACLERRIO;
			break;
		}
	}
	if (rv==0)
		(*buf)[cur] = 0;

open_cleanup:
	if (rv<0) {
		if (*file)
			fclose(*file);
		if (*buf)
			PERM_FREE(*buf);
	}
	return rv;
}


/*
 * local function: writes buf to disk and close the file
 */
void 
close_file_buf(FILE * file, char * filename, char * mode, char * buf) 
{
	if (file==NULL)
		return;
	fclose(file);
	if (strchr(mode, 'w')!=NULL || strchr(mode, 'a')!=NULL) {
		file = fopen(filename, "wb");
		fwrite(buf,1,strlen(buf),file);  
		fclose(file);
	}
	PERM_FREE(buf);
}


/*
 * local function: translate string to lower case
 */
char *
str_tolower(char * string)
{
	register char * p = string;
	for (; *p; p++)
		*p = tolower(*p);
	return string;
}

/*
 * local function: get the first name appear in block
 * return: 0 : not found, 
 *         1 : found
 */
int 
acl_get_first_name(char * block, char ** name, char ** next) 
{
	char bounds[] = "\t \"\';";
	char boundchar;
	char *p=NULL, *q=NULL, *start=NULL, *end=NULL;

	if (block==NULL)
		return 0;
try_next:
	if ((p=strstr(block, "acl"))!=NULL) {

		// check if this "acl" is the first occurance in this line.
		for (q=p-1; ((q>=block) && *q!='\n'); q--) {
			if (strchr(" \t",*q)==NULL) {
				// if not, try next;
				block = p+3;
				goto try_next;
			}
		}
		
		p+=3;
		while (strchr(bounds,*p)&&(*p!=0))
			p++;
		if (*p==0)
			return 0;
		boundchar = *(p-1);
		start = p;
		while ((boundchar!=*p)&&(*p!=0)&&(*p!=';'))
			p++;
		if (*p==0)
			return 0;
		end = p;
		*name = (char *)PERM_MALLOC(end-start+1);
		strncpy(*name, start, (end-start));
		(*name)[end-start]=0;
		*next = end;
		return 1;
	}
	return 0;
}

/*
 * local function: find the pointer to acl string from the given block
 */
char *
acl_strstr(char * block, char * aclname) 
{
	const char set[] = "\t \"\';";
	char * name, * rstr = NULL; 
	char *namestart;
	char * lowerb = block;
	int found = 0;

	if (block==NULL||aclname==NULL)
		return NULL;

	while ((name = strstr(block, aclname))!=NULL && !found) {
		if (name>lowerb) {	// This should be true, just in case
			namestart = name;
			int acllen = strlen(aclname);
			if ((strchr(set,name[-1])!=0) && (strchr(set,name[acllen])!=0)) {
			  // Require that quotes match
			  if ((name[-1] == '\"' && name[acllen] != '\"') ||
			      (name[-1] != '\"' && name[acllen] == '\"')) 
				/* LINTED */
				;
			  else {
				// 2 sides of boundary set, this is an exact match.
				while (&name[-1]>=lowerb) {
					name --;
					if (strchr(set, *name)==0)
						break; // should point to 'l'
				}

				if (name==lowerb)
					return NULL;

				if ((name-2)>=lowerb) 
					if ((name[-2]=='a') && (name[-1]=='c') && (*name=='l')) {
						name -= 2;	// name point to 'a'
						rstr = name;
						while (TRUE) {
							if (name==lowerb) {
								found = 1;
								break;
							}
							else if (name[-1]==' '||name[-1]=='\t') 
								name --;
							else if (name[-1]=='\n') {
								found = 1;
								break;
							}
							else 
								break; // acl is not at the head, there are other chars.
						}
					}
				}
			} // endif strchr
			block = namestart + strlen(aclname);
		} // endif (name > lowerb)
	}
	return rstr;
}



/*
 * local function: find the acl string from mapfile and return its acl structure
 */
int
get_acl_from_file(char * filename, char * aclname, ACLListHandle_t ** acllist_pp) 
{
	int  rv = 0;
	char * pattern=NULL;
	char header[] = "version 3.0;\n";
	int  headerlen = strlen(header);
	long  filesize;
	FILE * file;
	char * mirror=NULL, * text=NULL, *nextname=NULL;
	char * block=NULL, * aclhead=NULL, * aclend=NULL;

	*acllist_pp = NULL;

	// build the acl name pattern, which should be acl "..."
	// the ".." is built by acl_to_str_create

	if (aclname==NULL) {
		rv = ACLERRUNDEF;
		goto get_cleanup;
	}

	if ((pattern=(char *)PERM_MALLOC(strlen(aclname) + 1))==NULL) {
		rv = ACLERRNOMEM;
		goto get_cleanup;
	}
	else {
		sprintf(pattern,"%s", aclname);
		str_tolower(pattern);
	}

	/* get the acl text from the mapfile */
	if ((rv=open_file_buf(&file, filename, "rb", &block, &filesize))<0)
		goto get_cleanup;

	if ((mirror = (char *) PERM_MALLOC(filesize+1))==NULL) {
		rv = ACLERRNOMEM;
		goto get_cleanup;
	}

	memcpy(mirror, block, filesize);
	mirror[filesize]=0;
	str_tolower(mirror);

	if ((aclhead = acl_strstr(mirror, pattern))!=NULL) {
		// use mirror to search, then transfer to work on block;
		aclhead = block + (aclhead - mirror); 
		acl_get_first_name(aclhead+3, &nextname, &aclend);
		aclend = acl_strstr(aclhead+3, nextname);
		if (aclend == NULL) {
			// this is the last acl in the file
			aclend = &aclhead[strlen(aclhead)];
		}
	
		int len = aclend - aclhead; 
		text = (char *) PERM_MALLOC(len + headerlen + 1);
		sprintf(text, "%s", header);
		memcpy(&text[headerlen], aclhead, len);
		text[headerlen + len] = 0;

		if ((*acllist_pp=ACL_ParseString(NULL, text))==NULL) {
			rv = ACLERRPARSE;
		}
	}

get_cleanup:
	if (pattern)
		PERM_FREE(pattern);
	if (file)
		close_file_buf(file, filename, "rb", block);
	if (mirror)
		PERM_FREE(mirror);
	if (text)
		PERM_FREE(text);
	if (nextname)
		PERM_FREE(nextname);
	return rv;
}


/*
 * local function: delete the acl string from mapfile
 */
int 
delete_acl_from_file(char * filename, char * aclname) 
{
	char * pattern=NULL;
	char header[] = "version 3.0;\n";
	int  headerlen = strlen(header);
	int  rv = ACLERRUNDEF;
	long filesize;
	FILE * file;
	char * mirror=NULL, * text=NULL, * nextname=NULL;
	char * block=NULL, * aclhead=NULL, * aclend=NULL;
	int  remain;

	// build the acl name pattern, which should be acl "..."
	// the ".." is built by acl_to_str_create

	if (aclname==NULL) {
		rv = ACLERRUNDEF;
		goto delete_cleanup;
	}

	if ((pattern=(char *)PERM_MALLOC(strlen(aclname) + 10))==NULL) {
		rv = ACLERRNOMEM;
		goto delete_cleanup;
	}
	else {
		sprintf(pattern,"%s", aclname);
		str_tolower(pattern);
	}

	/* file the acl text from the mapfile */
	if ((rv=open_file_buf(&file, filename, "rb", &block, &filesize))<0)
		goto delete_cleanup;

	if ((mirror = (char *) PERM_MALLOC(filesize+1))==NULL) {
		rv = ACLERRNOMEM;
		goto delete_cleanup;
	}

	memcpy(mirror, block, filesize);
	mirror[filesize]=0;
	str_tolower(mirror);

	if ((aclhead = acl_strstr(mirror, pattern))!=NULL) {
		// use mirror to search, then transfer to work on block;
		aclhead = block + (aclhead - mirror); 
		acl_get_first_name(aclhead+3, &nextname, &aclend);
		aclend = acl_strstr(aclhead+3, nextname);
		if (aclend == NULL) {
			// this is the last acl in the file
			aclend = &aclhead[strlen(aclhead)];
		}
	
		int len = aclend - aclhead; 
		text = (char *) PERM_MALLOC(len + headerlen + 1);
		sprintf(text, "%s", header);
		memcpy(&text[headerlen], aclhead, len);
		text[headerlen + len] = 0;

		if (ACL_ParseString(NULL, text)==NULL) {
			rv = ACLERRPARSE;
			goto delete_cleanup;
		}
	}

	if (aclhead!=NULL) { // found the acl in the map file

		// int filesize = mpfile->Size();

		remain = strlen(aclend);
		if (memcpy(aclhead, aclend, remain)!=NULL) 
			rv = 0;
		else
			rv = ACLERRIO;

		aclhead[remain]=0;

		block = (char *) PERM_REALLOC(block, strlen(block)+1);	
	}
	else
		rv = ACLERRUNDEF;

delete_cleanup:
	if (pattern)
		PERM_FREE(pattern);
	if (text)
		PERM_FREE(text);
	if (mirror)
		PERM_FREE(mirror);
	if (nextname)
		PERM_FREE(nextname);
	if (file)
		close_file_buf(file, filename, "wb", block);
	return rv;
}

/*
 * local function: append the acl string to file
 */
int 
append_acl_to_file(char * filename, char * aclname, char * acltext) 
{
	int rv;
	/* acltext has been parsed to verify syntax up to this point */
	char * pattern=NULL;
	char * start=NULL;
	char * block;
	long filesize;
	FILE * file;
	long len;

	if ((pattern=(char *)PERM_MALLOC(strlen(aclname) + 10))==NULL) {
		rv = ACLERRNOMEM;
		goto append_cleanup;
	}
	else {
		sprintf(pattern,"%s", aclname);
	}
	
	if ((rv=open_file_buf(&file, filename, "rb", &block, &filesize))<0)
		goto append_cleanup;

	// find the begining of acl, skip the version part

	len = strlen(block);
	start = acl_strstr(acltext, pattern);
	if ((block=(char *)PERM_REALLOC(block, len+strlen(start)+1))==NULL) {
		rv = ACLERRNOMEM;
		goto append_cleanup;
	}
	strcat(block, start);

append_cleanup:
	if (pattern)
		PERM_FREE(pattern);
	if (file)
		close_file_buf(file, filename, "wb", block);
	
	return rv;
}



/*
 * local function: rename the acl name in the file
 */
int 
rename_acl_in_file(char * filename, char * aclname, char * newname) 
{
	ACLListHandle_t * racllist=NULL;
	char * pattern=NULL;
	char header[] = "version 3.0;\n";
	int  headerlen = strlen(header);
	int  rv = 0;
	long filesize;
	FILE * file;
	int  remain;
	long len;
	char * text=NULL, * mirror=NULL, * nextname=NULL;
	char * block=NULL, * aclhead=NULL, * aclend=NULL;
	char * cut=NULL;
	acl_string_t str_t = {NULL,0,0};

	// build the acl name pattern, which should be acl "..."
	// the ".." is built by acl_to_str_create

	if (aclname==NULL || newname==NULL) {
		rv = ACLERRUNDEF;
		goto rename_cleanup;
	}

	if ((pattern=(char *)PERM_MALLOC(strlen(aclname) + 10))==NULL) {
		rv = ACLERRNOMEM;
		goto rename_cleanup;
	}
	else {
		sprintf(pattern,"%s", aclname);
		str_tolower(pattern);
	}

	// file the acl text from the mapfile
	if ((rv=open_file_buf(&file, filename, "rb", &block, &filesize))<0)
		goto rename_cleanup;

	if ((mirror = (char *) PERM_MALLOC(filesize+1))==NULL) {
		rv = ACLERRNOMEM;
		goto rename_cleanup;
	}

	memcpy(mirror, block, filesize);
	mirror[filesize]=0;
	str_tolower(mirror);

	if ((aclhead = acl_strstr(mirror, pattern))!=NULL) {
		// use mirror to search, then transfer to work on block;
		aclhead = block + (aclhead - mirror); 
		acl_get_first_name(aclhead+3, &nextname, &aclend);
		aclend = acl_strstr(aclhead+3, nextname);
		if (aclend == NULL) {
			// this is the last acl in the file
			aclend = &aclhead[strlen(aclhead)];
		}
	
		len = aclend - aclhead; 
		text = (char *) PERM_MALLOC(len + headerlen + 1);
		sprintf(text, "%s", header);
		memcpy(&text[headerlen], aclhead, len);
		text[headerlen + len] = 0;

		if ((racllist=ACL_ParseString(NULL, text))==NULL) {
			rv = ACLERRPARSE;
			goto rename_cleanup;
		}
	}

	if (aclhead!=NULL) { // found the acl in the map file

		remain = strlen(aclend);
		// delete the acltext from where it is
		if (memcpy(aclhead, aclend, remain)!=NULL) 
			rv = 0;
		else
			rv = ACLERRUNDEF;

		aclhead[remain] = 0;
		len = strlen(block);

		/* establish the renamed the acl */
		acl_to_str_append(&str_t, "acl \"");
		acl_to_str_append(&str_t, newname);
		acl_to_str_append(&str_t, "\";");
		/* skip acl "..."; the semicollon in the last counts for the +1
		   add the rest acl text to str_t */
		cut = strchr(text, ';'); // skip version ...;
		cut = strchr(cut+1, ';') + 1; // skip acl ...;
		if (cut==NULL) {
			rv = ACLERRUNDEF;
			goto rename_cleanup;
		}
		acl_to_str_append(&str_t, cut);
		// acl_to_str_append(&str_t, "\n");

		if ((block=(char *) PERM_REALLOC(block, len + strlen(str_t.str) + 1))==NULL) {
			rv = ACLERRNOMEM;
			goto rename_cleanup;
		}
		// strcat(block, "\n");
		strcat(block, str_t.str);
	}
	else 
		rv = ACLERRUNDEF;

rename_cleanup:
	if (pattern)
		PERM_FREE(pattern);
	if (text)
		PERM_FREE(text);
	if (mirror)
		PERM_FREE(mirror);
	if (nextname)
		PERM_FREE(nextname);
	if (str_t.str)
		PERM_FREE(str_t.str);
	if (file)
		close_file_buf(file, filename, "wb", block);
	return rv;
}


/*
 * Retrieves the definition of a named ACL 
 *
 * Input:
 *	errp		a error stack	
 *	filename	Target ACL file 
 *	acl_name	Name of the target ACL
 *	acl_text 	a dynmaically allocated text (result)
 * Output:
 *	errp		error stack is set on error
 * Returns:
 *    0			success
 *    <0		failure
 */
NSAPI_PUBLIC int
ACL_FileGetAcl(NSErr_t *errp,
               char *filename,
               char *acl_name,
               // ACLListHandle_t **acllist_p,
			   char ** acltext,
               int flags) 
{
	int rv;
	ACLListHandle_t * acllist_p;

    if (acl_parse_crit == NULL)
		acl_parse_crit = crit_init();

    crit_enter( acl_parse_crit );

	rv = get_acl_from_file(filename, acl_name, &acllist_p);

	if (acllist_p == NULL) {
		*acltext = NULL;
		goto get_cleanup;
	}

	/*
	if ((rv=ACL_Decompose(errp, acltext, acllist_p))<0) {
		*acltext = NULL;
		goto get_cleanup;
	}
	*/
	if ((rv=ACL_WriteString(errp, acltext, acllist_p))<0) {
		*acltext = NULL;
		goto get_cleanup;
	}
	

get_cleanup:

	ACL_ListDestroy(errp, acllist_p);
    crit_exit( acl_parse_crit );

	return rv;
}
	


/*
 * Delete a named ACL from an ACL file
 *
 * Input:
 *	errp		a error stack	
 *	filename	Target ACL file 
 *	acl_name	Name of the target ACL
 * Output:
 *	errp		error stack is set on error
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_FileDeleteAcl(NSErr_t *errp, 
                  char *filename, 
                  char *acl_name,
                  int flags)
{
    int	rv = 0;

    if ( acl_parse_crit == NULL )
        acl_parse_crit = crit_init();

    crit_enter( acl_parse_crit );

	rv = delete_acl_from_file(filename, acl_name);

    crit_exit( acl_parse_crit );
    return(rv);
}


/*
 * Sets the definition of an ACL in an ACL file
 *
 * Input:
 *	errp		a error stack	
 *	filename	Target ACL file 
 *	acl_name	Name of the target ACL
 *	acl_text	a string that defines the new ACL	
 * Output:
 *	errp		error stack is set on error
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_FileSetAcl(NSErr_t *errp, 
               char *filename, 
               char *acl_text,
               int flags)
{
    int	rv = 0;
    ACLListHandle_t *new_acl_list = NULL;
    char **acl_name_list = NULL;

    if ( acl_parse_crit == NULL )
        acl_parse_crit = crit_init();

    crit_enter( acl_parse_crit );

    // get the acl name.
    new_acl_list = ACL_ParseString(errp, acl_text);
    if ( new_acl_list == NULL ) {
            rv = ACLERRPARSE;
            goto set_cleanup;
    }

    if ( ACL_ListGetNameList(errp, new_acl_list, &acl_name_list) < 0 ) {
            rv = ACLERRNOMEM;
            goto set_cleanup;
    }

    delete_acl_from_file(filename, acl_name_list[0]);
    rv = append_acl_to_file(filename, acl_name_list[0], acl_text);

set_cleanup:

    crit_exit( acl_parse_crit );
    if (new_acl_list)
        ACL_ListDestroy(errp, new_acl_list);
    if (acl_name_list)
        ACL_NameListDestroy(errp, acl_name_list);
    return(rv);
}


/*
 * Rename a named ACL in ACL text file
 *
 * Input:
 *	errp		a error stack	
 *	filename	Target ACL file 
 *	acl_name	Name of the target ACL
 *	new_acl_name	New ACL name 
 * Output:
 *	errp		error stack is set on error
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_FileRenameAcl(NSErr_t *errp, 
                  char *filename, 
                  char *aclname, 
                  char *newname,
                  int  flags)
{	
    int	rv = 0;

    if ( acl_parse_crit == NULL )
        acl_parse_crit = crit_init();

    crit_enter( acl_parse_crit );

    rv = rename_acl_in_file(filename, aclname, newname);

    crit_exit( acl_parse_crit );
    return(rv);

}


//
// Merge a list of ACLs into one ACL
//
// Input:
//	filename	the target acl file
//	acl_list	ACLs to merge
//	new_acl_name	resultant ACL
//	flags		currently ignored
// Returns:
//	0		success
//	< 0		failure
//

NSAPI_PUBLIC int
ACL_FileMergeAcl(NSErr_t *errp, 
                  char *filename, 
                  char **acl_name_list, 
                  char *new_acl_name,
                  int  flags)
{
    ACLListHandle_t	*new_acl_list = NULL;
    ACLListHandle_t	*tmp_acl_list = NULL;
    int			ii;
    int			rv;
    ACLHandle_t		*tmp_acl;
    ACLHandle_t		*new_acl;
    ACLExprHandle_t	*expr;


    tmp_acl_list = ACL_ParseFile(errp, filename);
    if ( tmp_acl_list == NULL ) {
        rv = ACLERRPARSE;
        goto cleanup;
    }

    new_acl_list = ACL_ParseFile(errp, filename);
    if ( new_acl_list == NULL ) {
        rv = ACLERRPARSE;
        goto cleanup;
    }

    // first get rid of all the ACLs that will be merged

    for (ii = 0; acl_name_list[ii]; ii++) {
        rv = ACL_ListAclDelete(errp, new_acl_list, acl_name_list[ii], flags);
        if ( rv  < 0 ) 
            goto cleanup;
    }

    // now create ACL to house the merged result
    new_acl = ACL_AclNew(errp, new_acl_name);
    if ( new_acl == NULL ) {
        rv = ACLERRNOMEM;
        goto cleanup;
    }

    rv = ACL_ListAppend(errp, new_acl_list, new_acl, flags);
    if ( rv < 0 ) 
        goto cleanup;
 
    for (ii = 0; acl_name_list[ii]; ii++) {
        tmp_acl = ACL_ListFind(errp, tmp_acl_list, acl_name_list[ii], flags);
        if ( tmp_acl == NULL ) {
            rv = ACLERRUNDEF;
            goto cleanup;
        } 
        for (expr = tmp_acl->expr_list_head; expr; expr = expr->expr_next) {
            // This call can't really fail unless we pass it a NULL
            // or some memory is corrupt.
            rv = ACL_ExprAppend(errp, new_acl, expr);
            if ( rv < 0 ) 
                goto cleanup;
            tmp_acl->expr_list_head = expr->expr_next;
            tmp_acl->expr_count--;
        }

        // Last bit of clean up so the destroy routine isn't confused. 

        tmp_acl->expr_list_tail = NULL;
        tmp_acl->expr_count = 0;
    }

    rv = ACL_WriteFile(errp, filename, new_acl_list);

cleanup:
    if ( new_acl_list )
        ACL_ListDestroy(errp, new_acl_list);
    if ( tmp_acl_list )
        ACL_ListDestroy(errp, tmp_acl_list);
    return(rv);
}

//
// Merge a list of ACL files into one ACL file
//
// Input:
//	filename	the target acl file
//	file_list	ACL files to merge
//	flags		currently ignored
// Returns:
//	0		success
//	< 0		failure
//

NSAPI_PUBLIC int
ACL_FileMergeFile(NSErr_t *errp, 
                  char *filename, 
                  char **file_list, 
                  int  flags)
{
    ACLListHandle_t	*new_acl_list = NULL;
    ACLListHandle_t	*tmp_acl_list = NULL;
    int			ii;
    int			rv;

    // we don't care if they have nothing to do

    if ( filename == NULL || file_list == NULL )
        return(0);

    new_acl_list = ACL_ListNew(errp);
    if (new_acl_list == NULL)
        return(ACLERRNOMEM);

    for (ii = 0; file_list[ii]; ii++) {
        tmp_acl_list = ACL_ParseFile(errp, file_list[ii]);
        if (tmp_acl_list == NULL) {
            rv = ACLERRPARSE;
            goto cleanup;
        }
        rv = ACL_ListConcat(errp, new_acl_list, tmp_acl_list, flags);
        if ( rv < 0 ) 
            goto cleanup;
        ACL_ListDestroy(errp, tmp_acl_list);
        tmp_acl_list = NULL;
    }

    rv = ACL_WriteFile(errp, filename, new_acl_list);

cleanup:
    if ( new_acl_list ) 
        ACL_ListDestroy(errp, new_acl_list);
    if ( tmp_acl_list ) 
        ACL_ListDestroy(errp, tmp_acl_list);
    return(rv);
}

/*
 * Gets a name list of consisting of all ACL names from the input aclfile 
 *
 * Input:
 *	filename	acl file
 *	name_list	pointer to a list of string pointers	
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_FileGetNameList(NSErr_t *errp, char * filename, char ***name_list)
{

    const int block_size = 50;
    int  rv, list_size, list_index;
    char ** local_list = NULL;
    char * block ;
    char * name;
    char * next;
    long filesize;
    FILE * file;
    char * head;
    
    if ((rv=open_file_buf(&file, filename, "rb", &block, &filesize))<0)
            goto list_cleanup;

    list_size = block_size;
    local_list = (char **) PERM_MALLOC(sizeof(char *) * list_size);
    if ( local_list == NULL ) {
		rv = ACLERRNOMEM;
		goto list_cleanup;
    }
    list_index = 0;
    local_list[list_index] = NULL; 

	head = block;
    while ((acl_get_first_name(head, &name, &next))) {

        if (list_index+2 > list_size) {
			list_size += block_size;
            char ** tmp_list = (char **) PERM_REALLOC(local_list, sizeof(char *) * list_size);
            if ( tmp_list == NULL ) {
				rv = ACLERRNOMEM;
				goto list_cleanup;
            }
            local_list = tmp_list;
        } 
        // local_list[list_index] = PERM_STRDUP(name);
		local_list[list_index] = name;
        if ( local_list[list_index] == NULL ) {            
            rv = ACLERRNOMEM;
			goto list_cleanup;
        }
        list_index++;
        local_list[list_index] = NULL; 
		head = next;
    }    

	rv = 0;
    *name_list = local_list;

list_cleanup:
	if (local_list && rv<0)
		ACL_NameListDestroy(errp, local_list);
	if (file)
		close_file_buf(file, filename, "rb", block);

    return rv;
}	

/*  
 * Function parses an input ACL file and resturns an
 * ACLListHandle_t pointer that represents the entire
 * file without the comments.
 * 
 * Input:
 *	filename	the name of the target ACL text file
 *	errp		a pointer to an error stack
 *
 * Returns:
 * 	NULL 		parse failed
 *  
 */

NSAPI_PUBLIC ACLListHandle_t *
ACL_ParseFile( NSErr_t *errp, char *filename )
{
ACLListHandle_t 	*handle = NULL;
int			eid = 0;
int			rv = 0;
char			*errmsg;

    ACL_InitAttr2Index();

    if ( acl_parse_crit == NULL )
        acl_parse_crit = crit_init();

    crit_enter( acl_parse_crit );

    if ( acl_InitScanner( errp, filename, NULL ) < 0 ) {
        rv = ACLERROPEN;
	eid = ACLERR1900;
        errmsg = system_errmsg();
        nserrGenerate(errp, rv, eid, ACL_Program, 2, filename, errmsg);
    } else {
    
        handle = ACL_ListNew(errp);
        if ( handle == NULL ) {
            rv = ACLERRNOMEM;
            eid = ACLERR1920;
            nserrGenerate(errp, rv, eid, ACL_Program, 0);
        } else if ( acl_PushListHandle( handle ) < 0 ) {
            rv = ACLERRNOMEM;
            eid = ACLERR1920;
            nserrGenerate(errp, rv, eid, ACL_Program, 0);
        } else if ( acl_Parse() ) {
            rv = ACLERRPARSE;
            eid = ACLERR1780;
        }
    
        if ( acl_EndScanner() < 0 ) {
            rv = ACLERROPEN;
            eid = ACLERR1500;
            errmsg = system_errmsg();
            nserrGenerate(errp, rv, eid, ACL_Program, 2, filename, errmsg);
        }

    }

    if ( rv || eid ) {
        ACL_ListDestroy(errp, handle);
        handle = NULL;
    }

    crit_exit( acl_parse_crit );
    return(handle);

}

/*  
 * Function parses an input ACL string and returns an
 * ACLListHandle_t pointer that represents the entire
 * file without the comments.
 * 
 * Input:
 *	buffer		the target ACL buffer 
 *	errp		a pointer to an error stack
 *
 * Returns:
 * 	NULL 		parse failed
 *  
 */

NSAPI_PUBLIC ACLListHandle_t *
ACL_ParseString( NSErr_t *errp, char *buffer )
{
ACLListHandle_t 	*handle = NULL;
int			eid = 0;
int			rv = 0;
char			*errmsg;

    ACL_InitAttr2Index();

    if ( acl_parse_crit == NULL )
        acl_parse_crit = crit_init();

    crit_enter( acl_parse_crit );

    if ( acl_InitScanner( errp, NULL, buffer ) < 0 ) {
        rv = ACLERRNOMEM;
        eid = ACLERR1920;
        nserrGenerate(errp, rv, eid, ACL_Program, 0);
    } else {
    
        handle = ACL_ListNew(errp);
        if ( handle == NULL ) {
            rv = ACLERRNOMEM;
            eid = ACLERR1920;
            nserrGenerate(errp, rv, eid, ACL_Program, 0);
        } else if ( acl_PushListHandle( handle ) < 0 ) {
            rv = ACLERRNOMEM;
            eid = ACLERR1920;
            nserrGenerate(errp, rv, eid, ACL_Program, 0);
        } else if ( acl_Parse() ) {
            rv = ACLERRPARSE;
            eid = ACLERR1780;
        }
    
        if ( acl_EndScanner() < 0 ) {
            rv = ACLERROPEN;
            eid = ACLERR1500;
            errmsg = system_errmsg();
            nserrGenerate(errp, rv, eid, ACL_Program, 2, "buffer", errmsg);
        }

    }

    if ( rv || eid ) {
        ACL_ListDestroy(errp, handle);
        handle = NULL;
    }

    crit_exit( acl_parse_crit );
    return(handle);

}

/*
 * LOCAL FUNCTION
 *
 * Output Authorization Expression Rights "right, right" 
 */

static int
acl_decompose_expr_arg( acl_string_t *str_t, ACLExprHandle_t *expr )
{
int 		ii;

    if ( expr->expr_argc <= 0 ) {
        return(ACLERRINTERNAL);
    }

    if ( expr->expr_type == ACL_EXPR_TYPE_RESPONSE ) {
        acl_to_str_append(str_t, expr->expr_argv[0]);
        acl_to_str_append(str_t, " \"");
        acl_to_str_append(str_t, expr->expr_argv[1]);
        acl_to_str_append(str_t, "\";\n");
        return(0);
    }
   
    for (ii = 0; ii < expr->expr_argc; ii++) {
        acl_to_str_append(str_t, expr->expr_argv[ii]);
        if ( ii < expr->expr_argc - 1 ) {
            acl_to_str_append(str_t, ",");
        }
    }
    acl_to_str_append(str_t, ";\n");

    return(0);
}

/*
 * LOCAL FUNCTION
 *
 * Walks through the authentication statement PList_t and
 * prints the structure to a string.
 */

static void 
acl_decompose_auth_expr(char *lval, const void *rval, void *user_data)
{
    acl_string_t * p_aclstr = (acl_string_t *) user_data;

    // ####

    acl_to_str_append(p_aclstr, " ");
    acl_to_str_append(p_aclstr, lval);
    acl_to_str_append(p_aclstr, "=\"");
    acl_to_str_append(p_aclstr, (char *) rval);
    acl_to_str_append(p_aclstr, "\"");

    return;
}

/*
 * LOCAL FUNCTION
 *
 * Output the logic part of the authencation statement to a string. 
 */

static int
acl_decompose_auth_logic( acl_string_t * str_t, ACLExprHandle_t *expr)
{

    if ( expr->expr_auth == NULL ) 
        return(0);

    acl_to_str_append(str_t, "exprs");
    PListEnumerate(expr->expr_auth, acl_decompose_auth_expr, (void *) str_t);
    acl_to_str_append(str_t, ";\n");

    return(0);
}

/*
 * LOCAL FUNCTION
 *
 * Output the logic part of the authorization statement to a string. 
 */

static int
acl_decompose_expr_logic( acl_string_t *str_t, ACLExprHandle_t *expr, ACLExprStack_t *expr_stack)
{
int 		rv = 0;
int 		ii;

    expr_stack->stack_index = 0;
    expr_stack->found_subexpression = 0;
    expr_stack->last_subexpression = -1;

    for (ii = 0; ii < expr->expr_raw_index; ii++) {
        rv = acl_reduce_expr_logic(expr_stack, &expr->expr_raw[ii]);
        if (rv) break;
    }

    if (!rv && expr_stack->expr_text[0]) {
        acl_to_str_append(str_t, "exprs ");
        acl_to_str_append(str_t, expr_stack->expr_text[0]);
        acl_to_str_append(str_t, ";\n");
        PERM_FREE(expr_stack->expr_text[0]);
    }

    return(rv);
}

static int
acl_decompose(acl_string_t *str_t, ACLListHandle_t *acl_list)
{
ACLWrapper_t 	*wrap;
ACLHandle_t 	*acl;
ACLExprHandle_t *expr;
int 		rv = 0;
ACLExprStack_t 	*expr_stack;

    expr_stack = (ACLExprStack_t *) PERM_MALLOC(sizeof(ACLExprStack_t));
    if ( expr_stack == NULL )
        return(ACLERRNOMEM);

    if ( acl_list->acl_list_head == NULL ) {
        PERM_FREE(expr_stack);
        return(0);
    }

    acl_to_str_append(str_t, "version 3.0;");
    for (wrap = acl_list->acl_list_head; wrap && !rv; 
         wrap = wrap->wrap_next ) {
        acl = wrap->acl;
        if ( acl->tag ) {
            acl_to_str_append(str_t, "\nname \"");
            acl_to_str_append(str_t, acl->tag);
            acl_to_str_append(str_t, "\";\n");
        } else {
            acl_to_str_append(str_t, "\nname;\n");
        }
            
        for (expr = acl->expr_list_head; expr && rv == 0; 
             expr = expr->expr_next ) {

            switch (expr->expr_type) {
            case ACL_EXPR_TYPE_DENY:
                acl_to_str_append(str_t, "type deny;\nrights ");
                if ( (rv = acl_decompose_expr_arg(str_t, expr)) < 0 )
                    break;
                if ( IS_ABSOLUTE(expr->expr_flags) )
                    acl_to_str_append(str_t, "absolute true;\n");
                rv = acl_decompose_expr_logic(str_t, expr, expr_stack);
                break;
            case ACL_EXPR_TYPE_ALLOW:
                acl_to_str_append(str_t, "type allow;\nrights ");
                if ( (rv = acl_decompose_expr_arg(str_t, expr)) < 0 )
                    break;
                if ( IS_ABSOLUTE(expr->expr_flags) )
                    acl_to_str_append(str_t, "absolute true;\n");
                rv = acl_decompose_expr_logic(str_t, expr, expr_stack);
                break;
            case ACL_EXPR_TYPE_AUTH:
                acl_to_str_append(str_t, "type authenticate;\nattrs ");
                if ( (rv = acl_decompose_expr_arg(str_t, expr)) < 0 )
                    break;
                if ( IS_ABSOLUTE(expr->expr_flags) )
                    acl_to_str_append(str_t, "absolute true;\n");
                rv = acl_decompose_auth_logic(str_t, expr);
                break;
            case ACL_EXPR_TYPE_RESPONSE:
                acl_to_str_append(str_t, "type response;\nattrs ");
                rv = acl_decompose_expr_arg(str_t, expr);
                break;
	    }
        }
    }

    PERM_FREE(expr_stack);
    return(rv);
}

/*
 * Converts an ACLListHandle_t to a parameter list suitable for passing
 * to the ACL UI.  
 *
 * Input:
 *	errp		error stack	
 *	acl		a pointer to a string, holds the result of the
 *			decomposition.
 *	acl_list	Target ACL list handle
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_Decompose(NSErr_t *errp, char **acl, ACLListHandle_t *acl_list)
{
    int rv ;
    acl_string_t aclstr={NULL,0,0};

    if ( acl_list == NULL || acl == NULL )
        return(ACLERRUNDEF);
   
    rv = acl_decompose(&aclstr, acl_list);
    *acl = aclstr.str;

    return ( rv );
}

/*
 * Destroy a NameList 
 *
 * Input:
 *	name_list	a dynamically allocated array of strings	
 * Returns:
 *    0			success
 *    < 0		failure
 */

NSAPI_PUBLIC int
ACL_NameListDestroy(NSErr_t *errp, char **name_list)
{
    int			list_index;

    if ( name_list == NULL )
        return(ACLERRUNDEF);

    for ( list_index = 0; name_list[list_index]; list_index++ ) {
        PERM_FREE(name_list[list_index]);
    }
    PERM_FREE(name_list);
    return(0);
}

static PList_t ACLAttr2IndexPList = NULL;

int
ACL_InitAttr2Index(void)
{
    int i;

    if (ACLAttr2IndexPList) return 0;

    ACLAttr2IndexPList = PListNew(NULL);
    for (i = 1; i < ACL_ATTR_INDEX_MAX; i++) {
        PListInitProp(ACLAttr2IndexPList, NULL, ACLAttrTable[i], (const void *)i, NULL);
    }
 
    return 0;
}

/*
 *	Attempt to locate the index number for one of the known attribute names
 *	that are stored in plists.  If we can't match it, just return 0.
 */
int
ACL_Attr2Index(const char *attrname)
{
    long index = 0;

    PR_ASSERT(sizeof(index) >= sizeof(void *));

    if ( ACLAttr2IndexPList ) {
        PListFindValue(ACLAttr2IndexPList, attrname, (void **)&index, NULL);
        if (index < 0) index = 0;
    }
    return index;
}

