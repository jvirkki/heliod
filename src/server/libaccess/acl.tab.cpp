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


# line 8 "acltext.y"
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


# line 48 "acltext.y"
typedef union
#ifdef __cplusplus
	ACLSTYPE
#endif
 {
	char	*string;
	int	ival;
} ACLSTYPE;
# define ACL_ABSOLUTE_TOK 257
# define ACL_ACL_TOK 258
# define ACL_ALLOW_TOK 259
# define ACL_ALWAYS_TOK 260
# define ACL_AND_TOK 261
# define ACL_AT_TOK 262
# define ACL_AUTHENTICATE_TOK 263
# define ACL_CONTENT_TOK 264
# define ACL_DEFAULT_TOK 265
# define ACL_DENY_TOK 266
# define ACL_GROUP_TOK 267
# define ACL_IN_TOK 268
# define ACL_INHERIT_TOK 269
# define ACL_NOT_TOK 270
# define ACL_NULL_TOK 271
# define ACL_OR_TOK 272
# define ACL_QSTRING_TOK 273
# define ACL_READ_TOK 274
# define ACL_TERMINAL_TOK 275
# define ACL_VARIABLE_TOK 276
# define ACL_VERSION_TOK 277
# define ACL_WRITE_TOK 278
# define ACL_WITH_TOK 279
# define ACL_EQ_TOK 280
# define ACL_GE_TOK 281
# define ACL_GT_TOK 282
# define ACL_LE_TOK 283
# define ACL_LT_TOK 284
# define ACL_NE_TOK 285

#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#else
#include <netsite.h>
#include <memory.h>
#endif


#ifdef __cplusplus

#ifndef aclerror
	void aclerror(const char *);
#endif

#ifndef acllex
#ifdef __EXTERN_C__
	extern "C" { int acllex(void); }
#else
	int acllex(void);
#endif
#endif
	int acl_Parse(void);

#endif
#define aclclearin aclchar = -1
#define aclerrok aclerrflag = 0
extern int aclchar;
extern int aclerrflag;
ACLSTYPE acllval;
ACLSTYPE aclval;
typedef int acltabelem;
#ifndef ACLMAXDEPTH
#define ACLMAXDEPTH 150
#endif
#if ACLMAXDEPTH > 0
int acl_acls[ACLMAXDEPTH], *acls = acl_acls;
ACLSTYPE acl_aclv[ACLMAXDEPTH], *aclv = acl_aclv;
#else	/* user does initial allocation */
int *acls;
ACLSTYPE *aclv;
#endif
static int aclmaxdepth = ACLMAXDEPTH;
# define ACLERRCODE 256

# line 455 "acltext.y"

static const acltabelem aclexca[] ={
-1, 1,
	0, -1,
	-2, 0,
	};
# define ACLNPROD 73
# define ACLLAST 247
static const acltabelem aclact[]={

    99,   100,   101,   103,   102,   104,    83,    62,    38,    30,
    87,   108,    92,    44,   109,    93,    14,     2,    78,    13,
    45,    68,    41,    48,    57,    54,     3,    71,    69,    94,
    75,    46,    23,    73,    21,    32,    55,    61,    20,    72,
    95,    22,    58,    74,    60,    70,    10,    81,    64,    84,
    85,    76,    67,    82,    27,    26,    25,    12,     5,    40,
    47,    79,    50,   107,    80,    51,    52,    33,    34,    15,
     8,    98,    86,    59,    56,    53,    66,    43,    11,    42,
    35,    31,    29,    77,    39,    28,    24,    36,    49,    19,
    18,    17,    16,     9,     7,     6,     4,     1,     0,     0,
    37,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    63,    65,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,    90,    91,     0,     0,    96,     0,    97,
     0,     0,     0,     0,   105,   106,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    88,     0,     0,     0,     0,     0,    89 };
static const acltabelem aclpact[]={

  -260,-10000000,  -250,-10000000,    -1,  -212,-10000000,  -212,-10000000,    -2,
  -257,-10000000,  -225,-10000000,-10000000,-10000000,  -225,    -3,    -4,    -5,
-10000000,-10000000,  -270,  -231,-10000000,-10000000,-10000000,-10000000,    27,-10000000,
-10000000,-10000000,  -271,  -254,-10000000,  -244,  -253,-10000000,-10000000,    21,
-10000000,-10000000,-10000000,    26,  -239,  -233,  -220,-10000000,  -273,  -253,
   -75,  -254,  -255,-10000000,  -247,  -219,-10000000,  -248,  -218,-10000000,
  -224,  -221,  -243,-10000000,  -258,-10000000,    20,-10000000,-10000000,-10000000,
-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,   -78,    -6,  -274,   -30,
  -255,-10000000,  -258,  -261,-10000000,  -232,-10000000,   -30,   -30,  -280,
-10000000,-10000000,-10000000,-10000000,   -30,   -30,    22,-10000000,  -262,-10000000,
-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000 };
static const acltabelem aclpgo[]={

     0,    97,    96,    95,    94,    70,    93,    69,    92,    91,
    90,    89,    88,    60,    87,    85,    84,    51,    59,    83,
    82,    68,    81,    80,    79,    77,    76,    49,    75,    74,
    73,    52,    50,    72,    71 };
static const acltabelem aclr1[]={

     0,     1,     2,     1,     3,     4,     4,     5,     5,     6,
     6,     7,     7,     8,     8,     8,    12,    11,    14,    11,
    13,    15,     9,    16,    16,    18,    17,    17,    19,    19,
    20,    10,    22,    10,    23,    21,    24,    25,    25,    25,
    25,    25,    25,    25,    30,    30,    30,    30,    28,    28,
    28,    28,    29,    29,    29,    29,    26,    26,    31,    27,
    27,    27,    32,    32,    32,    33,    33,    34,    34,    34,
    34,    34,    34 };
static const acltabelem aclr2[]={

     0,     0,     1,    10,     2,     2,     4,     6,     4,     5,
     5,     2,     4,     4,     4,     4,     1,    10,     1,     8,
     7,     1,    17,     2,     6,     3,     4,     6,     7,     7,
     1,     6,     1,     6,     1,     5,    10,     0,     3,     5,
     3,     5,     3,     5,     3,     3,     5,     5,     3,     3,
     5,     5,     3,     3,     5,     5,     2,     6,     3,     2,
     7,     7,     2,     6,     5,     7,     7,     2,     2,     2,
     2,     2,     2 };
static const acltabelem aclchk[]={

-10000000,    -1,   277,   276,    -2,    59,    -3,    -4,    -5,    -6,
   258,    -5,    59,   276,   273,    -7,    -8,    -9,   -10,   -11,
   263,   259,   266,   257,    -7,    59,    59,    59,   -15,   -20,
   279,   -22,   266,    40,   -21,   -23,   -14,   -21,   279,   -16,
   -18,   276,   -24,   -25,   257,   264,   275,   -13,   276,   -12,
    41,    44,    40,   -28,   264,   275,   -29,   257,   275,   -30,
   264,   257,   280,   -13,   123,   -18,   -26,   -31,   276,   275,
   264,   275,   257,   257,   264,   273,   -17,   -19,   276,    41,
    44,   125,    59,   280,   -27,   -32,   -33,    40,   270,   276,
   -31,   -17,   273,   276,   261,   272,   -27,   -32,   -34,   280,
   281,   282,   284,   283,   285,   -27,   -27,    41,   273,   276 };
static const acltabelem acldef[]={

     1,    -2,     0,     2,     0,     0,     3,     4,     5,     0,
     0,     6,     8,     9,    10,     7,    11,     0,     0,     0,
    21,    30,    32,     0,    12,    13,    14,    15,     0,    34,
    18,    34,     0,     0,    31,    37,     0,    33,    16,     0,
    23,    25,    35,     0,    38,    40,    42,    19,     0,     0,
     0,     0,     0,    39,    48,    49,    41,    52,    53,    43,
    44,    45,     0,    17,     0,    24,     0,    56,    58,    50,
    51,    54,    55,    46,    47,    20,     0,     0,     0,     0,
     0,    22,    26,     0,    36,    59,    62,     0,     0,     0,
    57,    27,    28,    29,     0,     0,     0,    64,     0,    67,
    68,    69,    70,    71,    72,    60,    61,    63,    65,    66 };
typedef struct
#ifdef __cplusplus
	acltoktype
#endif
{ char *t_name; int t_val; } acltoktype;
#ifndef ACLDEBUG
#	define ACLDEBUG	0	/* don't allow debugging */
#endif

#if ACLDEBUG

acltoktype acltoks[] =
{
	"ACL_ABSOLUTE_TOK",	257,
	"ACL_ACL_TOK",	258,
	"ACL_ALLOW_TOK",	259,
	"ACL_ALWAYS_TOK",	260,
	"ACL_AND_TOK",	261,
	"ACL_AT_TOK",	262,
	"ACL_AUTHENTICATE_TOK",	263,
	"ACL_CONTENT_TOK",	264,
	"ACL_DEFAULT_TOK",	265,
	"ACL_DENY_TOK",	266,
	"ACL_GROUP_TOK",	267,
	"ACL_IN_TOK",	268,
	"ACL_INHERIT_TOK",	269,
	"ACL_NOT_TOK",	270,
	"ACL_NULL_TOK",	271,
	"ACL_OR_TOK",	272,
	"ACL_QSTRING_TOK",	273,
	"ACL_READ_TOK",	274,
	"ACL_TERMINAL_TOK",	275,
	"ACL_VARIABLE_TOK",	276,
	"ACL_VERSION_TOK",	277,
	"ACL_WRITE_TOK",	278,
	"ACL_WITH_TOK",	279,
	"ACL_EQ_TOK",	280,
	"ACL_GE_TOK",	281,
	"ACL_GT_TOK",	282,
	"ACL_LE_TOK",	283,
	"ACL_LT_TOK",	284,
	"ACL_NE_TOK",	285,
	"-unknown-",	-1	/* ends search */
};

char * aclreds[] =
{
	"-no such reduction-",
	"start : /* empty */",
	"start : ACL_VERSION_TOK ACL_VARIABLE_TOK",
	"start : ACL_VERSION_TOK ACL_VARIABLE_TOK ';' start_acl_v3",
	"start_acl_v3 : acl_list",
	"acl_list : acl",
	"acl_list : acl_list acl",
	"acl : named_acl ';' body_list",
	"acl : named_acl ';'",
	"named_acl : ACL_ACL_TOK ACL_VARIABLE_TOK",
	"named_acl : ACL_ACL_TOK ACL_QSTRING_TOK",
	"body_list : body",
	"body_list : body body_list",
	"body : authenticate_statement ';'",
	"body : authorization_statement ';'",
	"body : deny_statement ';'",
	"deny_statement : ACL_ABSOLUTE_TOK ACL_DENY_TOK ACL_WITH_TOK",
	"deny_statement : ACL_ABSOLUTE_TOK ACL_DENY_TOK ACL_WITH_TOK deny_common",
	"deny_statement : ACL_DENY_TOK ACL_WITH_TOK",
	"deny_statement : ACL_DENY_TOK ACL_WITH_TOK deny_common",
	"deny_common : ACL_VARIABLE_TOK ACL_EQ_TOK ACL_QSTRING_TOK",
	"authenticate_statement : ACL_AUTHENTICATE_TOK",
	"authenticate_statement : ACL_AUTHENTICATE_TOK '(' attribute_list ')' '{' parameter_list '}'",
	"attribute_list : attribute",
	"attribute_list : attribute_list ',' attribute",
	"attribute : ACL_VARIABLE_TOK",
	"parameter_list : parameter ';'",
	"parameter_list : parameter ';' parameter_list",
	"parameter : ACL_VARIABLE_TOK ACL_EQ_TOK ACL_QSTRING_TOK",
	"parameter : ACL_VARIABLE_TOK ACL_EQ_TOK ACL_VARIABLE_TOK",
	"authorization_statement : ACL_ALLOW_TOK",
	"authorization_statement : ACL_ALLOW_TOK auth_common_action",
	"authorization_statement : ACL_DENY_TOK",
	"authorization_statement : ACL_DENY_TOK auth_common_action",
	"auth_common_action : /* empty */",
	"auth_common_action : auth_common",
	"auth_common : flag_list '(' args_list ')' expression",
	"flag_list : /* empty */",
	"flag_list : ACL_ABSOLUTE_TOK",
	"flag_list : ACL_ABSOLUTE_TOK content_static",
	"flag_list : ACL_CONTENT_TOK",
	"flag_list : ACL_CONTENT_TOK absolute_static",
	"flag_list : ACL_TERMINAL_TOK",
	"flag_list : ACL_TERMINAL_TOK content_absolute",
	"content_absolute : ACL_CONTENT_TOK",
	"content_absolute : ACL_ABSOLUTE_TOK",
	"content_absolute : ACL_CONTENT_TOK ACL_ABSOLUTE_TOK",
	"content_absolute : ACL_ABSOLUTE_TOK ACL_CONTENT_TOK",
	"content_static : ACL_CONTENT_TOK",
	"content_static : ACL_TERMINAL_TOK",
	"content_static : ACL_CONTENT_TOK ACL_TERMINAL_TOK",
	"content_static : ACL_TERMINAL_TOK ACL_CONTENT_TOK",
	"absolute_static : ACL_ABSOLUTE_TOK",
	"absolute_static : ACL_TERMINAL_TOK",
	"absolute_static : ACL_ABSOLUTE_TOK ACL_TERMINAL_TOK",
	"absolute_static : ACL_TERMINAL_TOK ACL_ABSOLUTE_TOK",
	"args_list : arg",
	"args_list : args_list ',' arg",
	"arg : ACL_VARIABLE_TOK",
	"expression : factor",
	"expression : factor ACL_AND_TOK expression",
	"expression : factor ACL_OR_TOK expression",
	"factor : base_expr",
	"factor : '(' expression ')'",
	"factor : ACL_NOT_TOK factor",
	"base_expr : ACL_VARIABLE_TOK relop ACL_QSTRING_TOK",
	"base_expr : ACL_VARIABLE_TOK relop ACL_VARIABLE_TOK",
	"relop : ACL_EQ_TOK",
	"relop : ACL_GE_TOK",
	"relop : ACL_GT_TOK",
	"relop : ACL_LT_TOK",
	"relop : ACL_LE_TOK",
	"relop : ACL_NE_TOK",
};
#endif /* ACLDEBUG */
# line	1 "/usr/ccs/bin/yaccpar"
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */


/*
** Skeleton parser driver for yacc output
*/

/*
** yacc user known macros and defines
*/
#define ACLERROR		goto aclerrlab
#define ACLACCEPT	return(0)
#define ACLABORT		return(1)
#define ACLBACKUP( newtoken, newvalue )\
{\
	if ( aclchar >= 0 || ( aclr2[ acltmp ] >> 1 ) != 1 )\
	{\
		aclerror( "syntax error - cannot backup" );\
		goto aclerrlab;\
	}\
	aclchar = newtoken;\
	aclstate = *aclps;\
	acllval = newvalue;\
	goto aclnewstate;\
}
#define ACLRECOVERING()	(!!aclerrflag)
#define ACLNEW(type)	PERM_MALLOC(sizeof(type) * aclnewmax)
#define ACLCOPY(to, from, type) \
	(type *) memcpy(to, (char *) from, aclmaxdepth * sizeof (type))
#define ACLENLARGE( from, type) \
	(type *) PERM_REALLOC((char *) from, aclnewmax * sizeof(type))
#ifndef ACLDEBUG
#	define ACLDEBUG	1	/* make debugging available */
#endif

/*
** user known globals
*/
int acldebug;			/* set to 1 to get debugging */

/*
** driver internal defines
*/
#define ACLFLAG		(-10000000)

/*
** global variables used by the parser
*/
ACLSTYPE *aclpv;			/* top of value stack */
int *aclps;			/* top of state stack */

int aclstate;			/* current state */
int acltmp;			/* extra var (lasts between blocks) */

int aclnerrs;			/* number of errors */
int aclerrflag;			/* error recovery flag */
int aclchar;			/* current input token number */



#ifdef ACLNMBCHARS
#define ACLLEX()		aclcvtok(acllex())
/*
** aclcvtok - return a token if i is a wchar_t value that exceeds 255.
**	If i<255, i itself is the token.  If i>255 but the neither 
**	of the 30th or 31st bit is on, i is already a token.
*/
#if defined(__STDC__) || defined(__cplusplus)
int aclcvtok(int i)
#else
int aclcvtok(i) int i;
#endif
{
	int first = 0;
	int last = ACLNMBCHARS - 1;
	int mid;
	wchar_t j;

	if(i&0x60000000){/*Must convert to a token. */
		if( aclmbchars[last].character < i ){
			return i;/*Giving up*/
		}
		while ((last>=first)&&(first>=0)) {/*Binary search loop*/
			mid = (first+last)/2;
			j = aclmbchars[mid].character;
			if( j==i ){/*Found*/ 
				return aclmbchars[mid].tvalue;
			}else if( j<i ){
				first = mid + 1;
			}else{
				last = mid -1;
			}
		}
		/*No entry in the table.*/
		return i;/* Giving up.*/
	}else{/* i is already a token. */
		return i;
	}
}
#else/*!ACLNMBCHARS*/
#define ACLLEX()		acllex()
#endif/*!ACLNMBCHARS*/

/*
** acl_Parse - return 0 if worked, 1 if syntax error not recovered from
*/
#if defined(__STDC__) || defined(__cplusplus)
int acl_Parse(void)
#else
int acl_Parse()
#endif
{
	register ACLSTYPE *aclpvt = 0;	/* top of value stack for $vars */

#if defined(__cplusplus) || defined(lint)
/*
	hacks to please C++ and lint - goto's inside
	switch should never be executed
*/
	static int __yaccpar_lint_hack__ = 0;
	switch (__yaccpar_lint_hack__)
	{
		case 1: goto aclerrlab;
		case 2: goto aclnewstate;
	}
#endif

	/*
	** Initialize externals - acl_Parse may be called more than once
	*/
	aclpv = &aclv[-1];
	aclps = &acls[-1];
	aclstate = 0;
	acltmp = 0;
	aclnerrs = 0;
	aclerrflag = 0;
	aclchar = -1;

#if ACLMAXDEPTH <= 0
	if (aclmaxdepth <= 0)
	{
		if ((aclmaxdepth = ACLEXPAND(0)) <= 0)
		{
			aclerror("yacc initialization error");
			ACLABORT;
		}
	}
#endif

	{
		register ACLSTYPE *acl_pv;	/* top of value stack */
		register int *acl_ps;		/* top of state stack */
		register int acl_state;		/* current state */
		register int  acl_n;		/* internal state number info */
	goto aclstack;	/* moved from 6 lines above to here to please C++ */

		/*
		** get globals into registers.
		** branch to here only if ACLBACKUP was called.
		*/
	aclnewstate:
		acl_pv = aclpv;
		acl_ps = aclps;
		acl_state = aclstate;
		goto acl_newstate;

		/*
		** get globals into registers.
		** either we just started, or we just finished a reduction
		*/
	aclstack:
		acl_pv = aclpv;
		acl_ps = aclps;
		acl_state = aclstate;

		/*
		** top of for (;;) loop while no reductions done
		*/
	acl_stack:
		/*
		** put a state and value onto the stacks
		*/
#if ACLDEBUG
		/*
		** if debugging, look up token value in list of value vs.
		** name pairs.  0 and negative (-1) are special values.
		** Note: linear search is used since time is not a real
		** consideration while debugging.
		*/
		if ( acldebug )
		{
			register int acl_i;

			printf( "State %d, token ", acl_state );
			if ( aclchar == 0 )
				printf( "end-of-file\n" );
			else if ( aclchar < 0 )
				printf( "-none-\n" );
			else
			{
				for ( acl_i = 0; acltoks[acl_i].t_val >= 0;
					acl_i++ )
				{
					if ( acltoks[acl_i].t_val == aclchar )
						break;
				}
				printf( "%s\n", acltoks[acl_i].t_name );
			}
		}
#endif /* ACLDEBUG */
		if ( ++acl_ps >= &acls[ aclmaxdepth ] )	/* room on stack? */
		{
			/*
			** reallocate and recover.  Note that pointers
			** have to be reset, or bad things will happen
			*/
			int aclps_index = (acl_ps - acls);
			int aclpv_index = (acl_pv - aclv);
			int aclpvt_index = (aclpvt - aclv);
			int aclnewmax;
#ifdef ACLEXPAND
			aclnewmax = ACLEXPAND(aclmaxdepth);
#else
			aclnewmax = 2 * aclmaxdepth;	/* double table size */
			if (aclmaxdepth == ACLMAXDEPTH)	/* first time growth */
			{
				char *newacls = (char *)ACLNEW(int);
				char *newaclv = (char *)ACLNEW(ACLSTYPE);
				if (newacls != 0 && newaclv != 0)
				{
					acls = ACLCOPY(newacls, acls, int);
					aclv = ACLCOPY(newaclv, aclv, ACLSTYPE);
				}
				else
					aclnewmax = 0;	/* failed */
			}
			else				/* not first time */
			{
				acls = ACLENLARGE(acls, int);
				aclv = ACLENLARGE(aclv, ACLSTYPE);
				if (acls == 0 || aclv == 0)
					aclnewmax = 0;	/* failed */
			}
#endif
			if (aclnewmax <= aclmaxdepth)	/* tables not expanded */
			{
				aclerror( "yacc stack overflow" );
				ACLABORT;
			}
			aclmaxdepth = aclnewmax;

			acl_ps = acls + aclps_index;
			acl_pv = aclv + aclpv_index;
			aclpvt = aclv + aclpvt_index;
		}
		*acl_ps = acl_state;
		*++acl_pv = aclval;

		/*
		** we have a new state - find out what to do
		*/
	acl_newstate:
		if ( ( acl_n = aclpact[ acl_state ] ) <= ACLFLAG )
			goto acldefault;		/* simple state */
#if ACLDEBUG
		/*
		** if debugging, need to mark whether new token grabbed
		*/
		acltmp = aclchar < 0;
#endif
		if ( ( aclchar < 0 ) && ( ( aclchar = ACLLEX() ) < 0 ) )
			aclchar = 0;		/* reached EOF */
#if ACLDEBUG
		if ( acldebug && acltmp )
		{
			register int acl_i;

			printf( "Received token " );
			if ( aclchar == 0 )
				printf( "end-of-file\n" );
			else if ( aclchar < 0 )
				printf( "-none-\n" );
			else
			{
				for ( acl_i = 0; acltoks[acl_i].t_val >= 0;
					acl_i++ )
				{
					if ( acltoks[acl_i].t_val == aclchar )
						break;
				}
				printf( "%s\n", acltoks[acl_i].t_name );
			}
		}
#endif /* ACLDEBUG */
		if ( ( ( acl_n += aclchar ) < 0 ) || ( acl_n >= ACLLAST ) )
			goto acldefault;
		if ( aclchk[ acl_n = aclact[ acl_n ] ] == aclchar )	/*valid shift*/
		{
			aclchar = -1;
			aclval = acllval;
			acl_state = acl_n;
			if ( aclerrflag > 0 )
				aclerrflag--;
			goto acl_stack;
		}

	acldefault:
		if ( ( acl_n = acldef[ acl_state ] ) == -2 )
		{
#if ACLDEBUG
			acltmp = aclchar < 0;
#endif
			if ( ( aclchar < 0 ) && ( ( aclchar = ACLLEX() ) < 0 ) )
				aclchar = 0;		/* reached EOF */
#if ACLDEBUG
			if ( acldebug && acltmp )
			{
				register int acl_i;

				printf( "Received token " );
				if ( aclchar == 0 )
					printf( "end-of-file\n" );
				else if ( aclchar < 0 )
					printf( "-none-\n" );
				else
				{
					for ( acl_i = 0;
						acltoks[acl_i].t_val >= 0;
						acl_i++ )
					{
						if ( acltoks[acl_i].t_val
							== aclchar )
						{
							break;
						}
					}
					printf( "%s\n", acltoks[acl_i].t_name );
				}
			}
#endif /* ACLDEBUG */
			/*
			** look through exception table
			*/
			{
				register const int *aclxi = aclexca;

				while ( ( *aclxi != -1 ) ||
					( aclxi[1] != acl_state ) )
				{
					aclxi += 2;
				}
				while ( ( *(aclxi += 2) >= 0 ) &&
					( *aclxi != aclchar ) )
					;
				if ( ( acl_n = aclxi[1] ) < 0 )
					ACLACCEPT;
			}
		}

		/*
		** check for syntax error
		*/
		if ( acl_n == 0 )	/* have an error */
		{
			/* no worry about speed here! */
			switch ( aclerrflag )
			{
			case 0:		/* new error */
				aclerror( "syntax error" );
				goto skip_init;
			aclerrlab:
				/*
				** get globals into registers.
				** we have a user generated syntax type error
				*/
				acl_pv = aclpv;
				acl_ps = aclps;
				acl_state = aclstate;
			skip_init:
				aclnerrs++;
				/* FALLTHRU */
			case 1:
			case 2:		/* incompletely recovered error */
					/* try again... */
				aclerrflag = 3;
				/*
				** find state where "error" is a legal
				** shift action
				*/
				while ( acl_ps >= acls )
				{
					acl_n = aclpact[ *acl_ps ] + ACLERRCODE;
					if ( acl_n >= 0 && acl_n < ACLLAST &&
						aclchk[aclact[acl_n]] == ACLERRCODE)					{
						/*
						** simulate shift of "error"
						*/
						acl_state = aclact[ acl_n ];
						goto acl_stack;
					}
					/*
					** current state has no shift on
					** "error", pop stack
					*/
#if ACLDEBUG
#	define _POP_ "Error recovery pops state %d, uncovers state %d\n"
					if ( acldebug )
						printf( _POP_, *acl_ps,
							acl_ps[-1] );
#	undef _POP_
#endif
					acl_ps--;
					acl_pv--;
				}
				/*
				** there is no state on stack with "error" as
				** a valid shift.  give up.
				*/
				ACLABORT;
			case 3:		/* no shift yet; eat a token */
#if ACLDEBUG
				/*
				** if debugging, look up token in list of
				** pairs.  0 and negative shouldn't occur,
				** but since timing doesn't matter when
				** debugging, it doesn't hurt to leave the
				** tests here.
				*/
				if ( acldebug )
				{
					register int acl_i;

					printf( "Error recovery discards " );
					if ( aclchar == 0 )
						printf( "token end-of-file\n" );
					else if ( aclchar < 0 )
						printf( "token -none-\n" );
					else
					{
						for ( acl_i = 0;
							acltoks[acl_i].t_val >= 0;
							acl_i++ )
						{
							if ( acltoks[acl_i].t_val
								== aclchar )
							{
								break;
							}
						}
						printf( "token %s\n",
							acltoks[acl_i].t_name );
					}
				}
#endif /* ACLDEBUG */
				if ( aclchar == 0 )	/* reached EOF. quit */
					ACLABORT;
				aclchar = -1;
				goto acl_newstate;
			}
		}/* end if ( acl_n == 0 ) */
		/*
		** reduction by production acl_n
		** put stack tops, etc. so things right after switch
		*/
#if ACLDEBUG
		/*
		** if debugging, print the string that is the user's
		** specification of the reduction which is just about
		** to be done.
		*/
		if ( acldebug )
			printf( "Reduce by (%d) \"%s\"\n",
				acl_n, aclreds[ acl_n ] );
#endif
		acltmp = acl_n;			/* value to switch over */
		aclpvt = acl_pv;			/* $vars top of value stack */
		/*
		** Look in goto table for next state
		** Sorry about using acl_state here as temporary
		** register variable, but why not, if it works...
		** If aclr2[ acl_n ] doesn't have the low order bit
		** set, then there is no action to be done for
		** this reduction.  So, no saving & unsaving of
		** registers done.  The only difference between the
		** code just after the if and the body of the if is
		** the goto acl_stack in the body.  This way the test
		** can be made before the choice of what to do is needed.
		*/
		{
			/* length of production doubled with extra bit */
			register int acl_len = aclr2[ acl_n ];

			if ( !( acl_len & 01 ) )
			{
				acl_len >>= 1;
				aclval = ( acl_pv -= acl_len )[1];	/* $$ = $1 */
				acl_state = aclpgo[ acl_n = aclr1[ acl_n ] ] +
					*( acl_ps -= acl_len ) + 1;
				if ( acl_state >= ACLLAST ||
					aclchk[ acl_state =
					aclact[ acl_state ] ] != -acl_n )
				{
					acl_state = aclact[ aclpgo[ acl_n ] ];
				}
				goto acl_stack;
			}
			acl_len >>= 1;
			aclval = ( acl_pv -= acl_len )[1];	/* $$ = $1 */
			acl_state = aclpgo[ acl_n = aclr1[ acl_n ] ] +
				*( acl_ps -= acl_len ) + 1;
			if ( acl_state >= ACLLAST ||
				aclchk[ acl_state = aclact[ acl_state ] ] != -acl_n )
			{
				acl_state = aclact[ aclpgo[ acl_n ] ];
			}
		}
					/* save until reenter driver code */
		aclstate = acl_state;
		aclps = acl_ps;
		aclpv = acl_pv;
	}
	/*
	** code supplied by user is placed in this switch
	*/
	switch( acltmp )
	{
		
case 2:
# line 90 "acltext.y"
{
		PERM_FREE(aclpvt[-0].string);
	} break;
case 9:
# line 114 "acltext.y"
{
		curr_acl = ACL_AclNew(NULL, aclpvt[-0].string);
		PERM_FREE(aclpvt[-0].string);
		if ( ACL_ListAppend(NULL, curr_acl_list, curr_acl, 0) < 0 ) {
			aclerror("Couldn't add ACL to list.");
			return(-1);
		}
	} break;
case 10:
# line 123 "acltext.y"
{
		curr_acl = ACL_AclNew(NULL, aclpvt[-0].string);
		PERM_FREE(aclpvt[-0].string);
		if ( ACL_ListAppend(NULL, curr_acl_list, curr_acl, 0) < 0 ) {
			aclerror("Couldn't add ACL to list.");
			return(-1);
		}
	} break;
case 16:
# line 144 "acltext.y"
{
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_RESPONSE) ;
		if ( curr_expr == NULL ) {
			aclerror("ACL_ExprNew(deny) failed");
			return(-1);
		}
		if ( ACL_ExprAppend(NULL, curr_acl, curr_expr) < 0 ) {
			aclerror("Could not add authorization");
			return(-1);
		}
		if ( ACL_ExprSetPFlags(NULL, curr_expr,
                                        ACL_PFLAG_ABSOLUTE) < 0 ) {
                        aclerror("Could not set deny processing flags");
                        return(-1);
                }
	} break;
case 18:
# line 162 "acltext.y"
{
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_RESPONSE) ;
		if ( curr_expr == NULL ) {
			aclerror("ACL_ExprNew(deny) failed");
			return(-1);
		}
		if ( ACL_ExprAppend(NULL, curr_acl, curr_expr) < 0 ) {
			aclerror("Could not add authorization");
			return(-1);
		}
	} break;
case 20:
# line 177 "acltext.y"
{
		acl_string_lower(aclpvt[-2].string);
                if ( ACL_ExprSetDenyWith(NULL, curr_expr, 
                                         aclpvt[-2].string, aclpvt[-0].string) < 0 ) {
                        aclerror("ACL_ExprSetDenyWith() failed");
                        return(-1);
                }
                PERM_FREE(aclpvt[-2].string);
                PERM_FREE(aclpvt[-0].string);
	} break;
case 21:
# line 190 "acltext.y"
{
		pflags = 0;
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_AUTH) ;
		if ( curr_expr == NULL ) {
			aclerror("ACL_ExprNew(allow) failed");
			return(-1);
		}
                curr_auth_info = PListCreate(NULL, ACL_ATTR_INDEX_MAX, NULL, NULL);
		if ( ACL_ExprAddAuthInfo(curr_expr, curr_auth_info) < 0 ) {
			aclerror("Could not set authorization info");
			return(-1);
		}
	} break;
case 22:
# line 204 "acltext.y"
{
		if ( ACL_ExprAppend(NULL, curr_acl, curr_expr) < 0 ) {
			aclerror("Could not add authorization");
			return(-1);
		}
	} break;
case 25:
# line 216 "acltext.y"
{
		acl_string_lower(aclpvt[-0].string);
		if ( ACL_ExprAddArg(NULL, curr_expr, aclpvt[-0].string) < 0 ) {
			aclerror("ACL_ExprAddArg() failed");
			return(-1);
		}
		PERM_FREE(aclpvt[-0].string);
	} break;
case 28:
# line 231 "acltext.y"
{
		acl_string_lower(aclpvt[-2].string);
		if ( PListInitProp(curr_auth_info, 
                                   ACL_Attr2Index(aclpvt[-2].string), aclpvt[-2].string, aclpvt[-0].string, NULL) < 0 ) {
		}
		PERM_FREE(aclpvt[-2].string);
	} break;
case 29:
# line 239 "acltext.y"
{
		acl_string_lower(aclpvt[-2].string);
		if ( PListInitProp(curr_auth_info, 
                                   ACL_Attr2Index(aclpvt[-2].string), aclpvt[-2].string, aclpvt[-0].string, NULL) < 0 ) {
		}
		PERM_FREE(aclpvt[-2].string);
	} break;
case 30:
# line 249 "acltext.y"
{
		pflags = 0;
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_ALLOW) ;
		if ( curr_expr == NULL ) {
			aclerror("ACL_ExprNew(allow) failed");
			return(-1);
		}
	} break;
case 32:
# line 259 "acltext.y"
{
		pflags = 0;
		curr_expr = ACL_ExprNew(ACL_EXPR_TYPE_DENY) ;
		if ( curr_expr == NULL ) {
			aclerror("ACL_ExprNew(deny) failed");
			return(-1);
		}
	} break;
case 34:
# line 271 "acltext.y"
{
		if ( ACL_ExprAppend(NULL, curr_acl, curr_expr) < 0 ) {
			aclerror("Could not add authorization");
			return(-1);
		}
	} break;
case 35:
# line 278 "acltext.y"
{
		if ( ACL_ExprSetPFlags (NULL, curr_expr, pflags) < 0 ) {
			aclerror("Could not set authorization processing flags");
			return(-1);
		}
#if ACLDEBUG
		if ( ACL_ExprDisplay(curr_expr) < 0 ) {
			aclerror("ACL_ExprDisplay() failed");
			return(-1);
		}
		printf("Parsed authorization.\n");
#endif
	} break;
case 38:
# line 298 "acltext.y"
{
		pflags = ACL_PFLAG_ABSOLUTE;
	} break;
case 39:
# line 302 "acltext.y"
{
		pflags = ACL_PFLAG_ABSOLUTE;
	} break;
case 40:
# line 306 "acltext.y"
{
		pflags = ACL_PFLAG_CONTENT;
	} break;
case 41:
# line 310 "acltext.y"
{
		pflags = ACL_PFLAG_CONTENT;
	} break;
case 42:
# line 314 "acltext.y"
{
		pflags = ACL_PFLAG_TERMINAL;
	} break;
case 43:
# line 318 "acltext.y"
{
		pflags = ACL_PFLAG_TERMINAL;
	} break;
case 44:
# line 324 "acltext.y"
{
		pflags |= ACL_PFLAG_CONTENT;
	} break;
case 45:
# line 328 "acltext.y"
{
		pflags |= ACL_PFLAG_ABSOLUTE;
	} break;
case 46:
# line 332 "acltext.y"
{
		pflags |= ACL_PFLAG_ABSOLUTE | ACL_PFLAG_CONTENT;
	} break;
case 47:
# line 336 "acltext.y"
{
		pflags |= ACL_PFLAG_ABSOLUTE | ACL_PFLAG_CONTENT;
	} break;
case 48:
# line 342 "acltext.y"
{
		pflags |= ACL_PFLAG_CONTENT;
	} break;
case 49:
# line 346 "acltext.y"
{
		pflags |= ACL_PFLAG_TERMINAL;
	} break;
case 50:
# line 350 "acltext.y"
{
		pflags |= ACL_PFLAG_TERMINAL | ACL_PFLAG_CONTENT;
	} break;
case 51:
# line 354 "acltext.y"
{
		pflags |= ACL_PFLAG_TERMINAL | ACL_PFLAG_CONTENT;
	} break;
case 52:
# line 360 "acltext.y"
{
		pflags |= ACL_PFLAG_ABSOLUTE;
	} break;
case 53:
# line 364 "acltext.y"
{
		pflags |= ACL_PFLAG_TERMINAL;
	} break;
case 54:
# line 368 "acltext.y"
{
		pflags |= ACL_PFLAG_TERMINAL | ACL_PFLAG_ABSOLUTE;
	} break;
case 55:
# line 372 "acltext.y"
{
		pflags |= ACL_PFLAG_TERMINAL | ACL_PFLAG_ABSOLUTE;
	} break;
case 58:
# line 382 "acltext.y"
{
		acl_string_lower(aclpvt[-0].string);
		if ( ACL_ExprAddArg(NULL, curr_expr, aclpvt[-0].string) < 0 ) {
			aclerror("ACL_ExprAddArg() failed");
			return(-1);
		}
		PERM_FREE( aclpvt[-0].string );
	} break;
case 60:
# line 394 "acltext.y"
{
                if ( ACL_ExprAnd(NULL, curr_expr) < 0 ) {
                        aclerror("ACL_ExprAnd() failed");
                        return(-1);
                }
        } break;
case 61:
# line 401 "acltext.y"
{
                if ( ACL_ExprOr(NULL, curr_expr) < 0 ) {
                        aclerror("ACL_ExprOr() failed");
                        return(-1);
                }
        } break;
case 64:
# line 412 "acltext.y"
{
                if ( ACL_ExprNot(NULL, curr_expr) < 0 ) {
                        aclerror("ACL_ExprNot() failed");
                        return(-1);
                }
        } break;
case 65:
# line 421 "acltext.y"
{
		acl_string_lower(aclpvt[-2].string);
		if ( ACL_ExprTerm(NULL, curr_expr,
				aclpvt[-2].string, (CmpOp_t) aclpvt[-1].ival, aclpvt[-0].string) < 0 ) {
			aclerror("ACL_ExprTerm() failed");
			PERM_FREE(aclpvt[-2].string);
			PERM_FREE(aclpvt[-0].string);	
			return(-1);
		}
		PERM_FREE(aclpvt[-2].string);
		PERM_FREE(aclpvt[-0].string);	
	} break;
case 66:
# line 434 "acltext.y"
{
		acl_string_lower(aclpvt[-2].string);
		if ( ACL_ExprTerm(NULL, curr_expr,
				aclpvt[-2].string, (CmpOp_t) aclpvt[-1].ival, aclpvt[-0].string) < 0 ) {
			aclerror("ACL_ExprTerm() failed");
			PERM_FREE(aclpvt[-2].string);
			PERM_FREE(aclpvt[-0].string);	
			return(-1);
		}
		PERM_FREE(aclpvt[-2].string);
		PERM_FREE(aclpvt[-0].string);	
	} break;
# line	531 "/usr/ccs/bin/yaccpar"
	}
	goto aclstack;		/* reset registers in driver code */
}

