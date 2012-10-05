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


#include "expr_yy.h"


#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#define	YYCONST	const
#else
#include <malloc.h>
#include <memory.h>
#define	YYCONST
#endif


#if defined(__cplusplus) || defined(__STDC__)

#if defined(__cplusplus) && defined(__EXTERN_C__)
#endif
#ifndef yyerror
#if defined(__cplusplus)
	#define yyerror(msg) expr_yy_error(yydata, msg)
#endif
#endif
#ifndef yylex
	#define yylex() expr_yy_lex(yydata)
#endif
	int expr_yy_parse(YYDATA *yydata);
#if defined(__cplusplus) && defined(__EXTERN_C__)
}
#endif

#endif

#define yyclearin (yydata->yychar) = -1
#define yyerrok (yydata->yyerrflag) = 0
typedef int yytabelem;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
#if YYMAXDEPTH > 0
#else	/* user does initial allocation */
#endif
# define YYERRCODE 256


static YYCONST yytabelem yyexca[] ={
-1, 1,
	0, -1,
	-2, 0,
-1, 80,
	273, 0,
	274, 0,
	275, 0,
	276, 0,
	-2, 29,
-1, 81,
	273, 0,
	274, 0,
	275, 0,
	276, 0,
	-2, 30,
-1, 82,
	273, 0,
	274, 0,
	275, 0,
	276, 0,
	-2, 31,
-1, 83,
	273, 0,
	274, 0,
	275, 0,
	276, 0,
	-2, 32,
-1, 84,
	277, 0,
	278, 0,
	279, 0,
	280, 0,
	281, 0,
	282, 0,
	283, 0,
	284, 0,
	-2, 33,
-1, 85,
	277, 0,
	278, 0,
	279, 0,
	280, 0,
	281, 0,
	282, 0,
	283, 0,
	284, 0,
	-2, 34,
-1, 86,
	277, 0,
	278, 0,
	279, 0,
	280, 0,
	281, 0,
	282, 0,
	283, 0,
	284, 0,
	-2, 35,
-1, 87,
	277, 0,
	278, 0,
	279, 0,
	280, 0,
	281, 0,
	282, 0,
	283, 0,
	284, 0,
	-2, 36,
-1, 88,
	277, 0,
	278, 0,
	279, 0,
	280, 0,
	281, 0,
	282, 0,
	283, 0,
	284, 0,
	-2, 37,
-1, 89,
	277, 0,
	278, 0,
	279, 0,
	280, 0,
	281, 0,
	282, 0,
	283, 0,
	284, 0,
	-2, 38,
-1, 90,
	277, 0,
	278, 0,
	279, 0,
	280, 0,
	281, 0,
	282, 0,
	283, 0,
	284, 0,
	-2, 39,
-1, 91,
	277, 0,
	278, 0,
	279, 0,
	280, 0,
	281, 0,
	282, 0,
	283, 0,
	284, 0,
	-2, 40,
-1, 98,
	292, 0,
	293, 0,
	294, 0,
	-2, 48,
-1, 99,
	292, 0,
	293, 0,
	294, 0,
	-2, 49,
-1, 100,
	292, 0,
	293, 0,
	294, 0,
	-2, 50,
	};
# define YYNPROD 71
# define YYLAST 446
static YYCONST yytabelem yyact[]={

    34,    35,    36,   104,    37,   116,    38,    39,    40,    41,
    42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
    52,   115,    53,    54,    55,    56,    57,    58,    59,    60,
    61,    34,    35,    36,   114,    37,   119,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
    51,    52,   101,    53,    54,    55,    56,    57,    58,    59,
    60,    61,    34,    35,    36,    21,    37,   112,    38,    39,
    40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
    50,    51,    52,    20,    53,    54,    55,    56,    57,    58,
    59,    60,    61,    19,    34,    35,    36,   108,    37,   109,
    38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
    48,    49,    50,    51,    52,   111,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    34,    35,    36,    18,    37,
    17,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    51,    52,    16,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    36,   110,    37,    15,
    38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
    48,    49,    50,    51,    52,    14,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    37,    13,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
    51,    52,    12,    53,    54,    55,    56,    57,    58,    59,
    60,    61,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    51,    52,    11,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    40,    41,    42,    43,
    44,    45,    46,    47,    48,    49,    50,    51,    52,    10,
    53,    54,    55,    56,    57,    58,    59,    60,    61,    41,
    42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
    52,     9,    53,    54,    55,    56,    57,    58,    59,    60,
    61,    30,    27,    31,    32,    29,    67,    68,     8,     7,
    22,    56,    57,    58,    59,    60,    61,     6,     5,    71,
    69,    59,    60,    61,     4,     3,     2,     0,    23,    25,
    26,   107,   105,     0,     0,     0,   106,     0,    24,     0,
     0,    28,     0,     0,    33,    45,    46,    47,    48,    49,
    50,    51,    52,     1,    53,    54,    55,    56,    57,    58,
    59,    60,    61,    70,    53,    54,    55,    56,    57,    58,
    59,    60,    61,     0,     0,     0,    62,    63,    64,    65,
    66,     0,     0,     0,     0,     0,     0,    72,    73,    74,
    75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
    85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
    95,    96,    97,    98,    99,   100,     0,     0,     0,     0,
     0,   102,   103,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,   113,     0,
     0,     0,     0,   117,     0,   118 };
static YYCONST yytabelem yypact[]={

    23,  -139,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,
-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,-10000000,
-10000000,-10000000,    23,    23,    23,    23,    23,   -15,    41,-10000000,
-10000000,-10000000,-10000000,    23,    23,    23,    23,    23,    23,    23,
    23,    23,    23,    23,    23,    23,    23,    23,    23,    23,
    23,    23,    23,    23,    23,    23,    23,    23,    23,    23,
    23,    23,   -83,    58,-10000000,-10000000,-10000000,    23,    23,  -299,
    53,-10000000,  -202,  -110,  -110,   -83,  -170,   -59,   -36,   -14,
    48,    48,    48,    48,    58,    58,    58,    58,    58,    58,
    58,    58,     2,     2,     2,     9,     9,     9,-10000000,-10000000,
-10000000,  -142,  -139,  -233,    23,  -266,  -279,  -295,-10000000,    23,
-10000000,    23,-10000000,  -264,-10000000,-10000000,-10000000,   -83,  -139,-10000000 };
static YYCONST yytabelem yypgo[]={

     0,   333,   306,   305,   304,   298,   297,   289,   288,   271,
   249,   226,   202,   186,   175,   159,   146,   130,   128,    93,
    83,    65,    52 };
static YYCONST yytabelem yyr1[]={

     0,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     2,     2,     3,     4,     5,     6,     7,     8,     9,
     9,     9,     9,    10,    10,    10,    10,    10,    10,    10,
    10,    11,    12,    12,    12,    13,    13,    13,    14,    14,
    14,    15,    15,    15,    16,    22,    22,    22,    17,    17,
    18,    18,    18,    18,    18,    18,    19,    20,    20,    20,
    21 };
static YYCONST yytabelem yyr2[]={

     0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     7,     7,     7,     5,    11,     7,     7,     7,     7,
     7,     7,     7,     7,     7,     7,     7,     7,     7,     7,
     7,     5,     7,     7,     7,     7,     7,     7,     7,     7,
     7,     5,     5,     5,     9,     1,     3,     7,    11,     9,
     5,     9,     3,     9,     5,     9,     3,     3,     3,     3,
     7 };
static YYCONST yytabelem yychk[]={

-10000000,    -1,    -2,    -3,    -4,    -5,    -6,    -7,    -8,    -9,
   -10,   -11,   -12,   -13,   -14,   -15,   -16,   -17,   -18,   -19,
   -20,   -21,   267,   285,   295,   286,   287,   259,   298,   262,
   258,   260,   261,   301,   264,   265,   266,   268,   270,   271,
   272,   273,   274,   275,   276,   277,   278,   279,   280,   281,
   282,   283,   284,   286,   287,   288,   289,   290,   291,   292,
   293,   294,    -1,    -1,    -1,    -1,    -1,   301,   302,   259,
   302,   258,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,   -22,    -1,    -1,   302,   259,   263,   258,   299,   269,
   299,   257,   300,    -1,   300,   300,   300,    -1,    -1,   300 };
static YYCONST yytabelem yydef[]={

     0,    -2,     1,     2,     3,     4,     5,     6,     7,     8,
     9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
    19,    20,     0,     0,     0,     0,     0,    66,     0,    62,
    67,    68,    69,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    24,    41,    51,    52,    53,    55,     0,    60,
     0,    64,     0,    21,    22,    23,     0,    26,    27,    28,
    -2,    -2,    -2,    -2,    -2,    -2,    -2,    -2,    -2,    -2,
    -2,    -2,    42,    43,    44,    45,    46,    47,    -2,    -2,
    -2,     0,    56,     0,     0,     0,     0,     0,    70,     0,
    54,     0,    59,     0,    61,    63,    65,    25,    57,    58 };
typedef struct
#ifdef __cplusplus
	yytoktype
#endif
{
#ifdef __cplusplus
const
#endif
char *t_name; int t_val; } yytoktype;
#ifndef YYDEBUG
#	define YYDEBUG	0	/* don't allow debugging */
#endif

#if YYDEBUG

yytoktype yytoks[] =
{
	"EXPR_TOKEN_COMMA",	257,
	"EXPR_TOKEN_NUMBER",	258,
	"EXPR_TOKEN_IDENTIFIER",	259,
	"EXPR_TOKEN_SINGLE_QUOTE_STRING",	260,
	"EXPR_TOKEN_DOUBLE_QUOTE_STRING",	261,
	"EXPR_TOKEN_DOLLAR_AMP",	262,
	"EXPR_TOKEN_AMP",	263,
	"EXPR_TOKEN_O_R",	264,
	"EXPR_TOKEN_X_O_R",	265,
	"EXPR_TOKEN_A_N_D",	266,
	"EXPR_TOKEN_N_O_T",	267,
	"EXPR_TOKEN_QUESTION",	268,
	"EXPR_TOKEN_COLON",	269,
	"EXPR_TOKEN_PIPE_PIPE",	270,
	"EXPR_TOKEN_AMP_AMP",	271,
	"EXPR_TOKEN_CARAT",	272,
	"EXPR_TOKEN_EQUALS_EQUALS",	273,
	"EXPR_TOKEN_BANG_EQUALS",	274,
	"EXPR_TOKEN_E_Q",	275,
	"EXPR_TOKEN_N_E",	276,
	"EXPR_TOKEN_LEFTANGLE",	277,
	"EXPR_TOKEN_LEFTANGLE_EQUALS",	278,
	"EXPR_TOKEN_RIGHTANGLE",	279,
	"EXPR_TOKEN_RIGHTANGLE_EQUALS",	280,
	"EXPR_TOKEN_L_T",	281,
	"EXPR_TOKEN_L_E",	282,
	"EXPR_TOKEN_G_T",	283,
	"EXPR_TOKEN_G_E",	284,
	"EXPR_TOKEN_NAMED_OP",	285,
	"EXPR_TOKEN_PLUS",	286,
	"EXPR_TOKEN_MINUS",	287,
	"EXPR_TOKEN_DOT",	288,
	"EXPR_TOKEN_STAR",	289,
	"EXPR_TOKEN_SLASH",	290,
	"EXPR_TOKEN_PERCENT",	291,
	"EXPR_TOKEN_EQUALS",	292,
	"EXPR_TOKEN_EQUALS_TILDE",	293,
	"EXPR_TOKEN_BANG_TILDE",	294,
	"EXPR_TOKEN_BANG",	295,
	"EXPR_PRECEDENCE_UNARY_PLUS",	296,
	"EXPR_PRECEDENCE_UNARY_MINUS",	297,
	"EXPR_TOKEN_DOLLAR",	298,
	"EXPR_TOKEN_RIGHTPAREN",	299,
	"EXPR_TOKEN_RIGHTBRACE",	300,
	"EXPR_TOKEN_LEFTPAREN",	301,
	"EXPR_TOKEN_LEFTBRACE",	302,
	"-unknown-",	-1	/* ends search */
};

#ifdef __cplusplus
const
#endif
char * yyreds[] =
{
	"-no such reduction-",
	"expr : named_or_expr",
	"expr : named_and_expr",
	"expr : named_not_expr",
	"expr : ternary_expr",
	"expr : c_or_expr",
	"expr : c_and_expr",
	"expr : c_xor_expr",
	"expr : equality_expr",
	"expr : relational_expr",
	"expr : named_op_expr",
	"expr : additive_expr",
	"expr : multiplicative_expr",
	"expr : matching_expr",
	"expr : sign_expr",
	"expr : call",
	"expr : access",
	"expr : variable",
	"expr : identifier",
	"expr : literal",
	"expr : paren",
	"named_or_expr : expr EXPR_TOKEN_O_R expr",
	"named_or_expr : expr EXPR_TOKEN_X_O_R expr",
	"named_and_expr : expr EXPR_TOKEN_A_N_D expr",
	"named_not_expr : EXPR_TOKEN_N_O_T expr",
	"ternary_expr : expr EXPR_TOKEN_QUESTION expr EXPR_TOKEN_COLON expr",
	"c_or_expr : expr EXPR_TOKEN_PIPE_PIPE expr",
	"c_and_expr : expr EXPR_TOKEN_AMP_AMP expr",
	"c_xor_expr : expr EXPR_TOKEN_CARAT expr",
	"equality_expr : expr EXPR_TOKEN_EQUALS_EQUALS expr",
	"equality_expr : expr EXPR_TOKEN_BANG_EQUALS expr",
	"equality_expr : expr EXPR_TOKEN_E_Q expr",
	"equality_expr : expr EXPR_TOKEN_N_E expr",
	"relational_expr : expr EXPR_TOKEN_LEFTANGLE expr",
	"relational_expr : expr EXPR_TOKEN_LEFTANGLE_EQUALS expr",
	"relational_expr : expr EXPR_TOKEN_RIGHTANGLE expr",
	"relational_expr : expr EXPR_TOKEN_RIGHTANGLE_EQUALS expr",
	"relational_expr : expr EXPR_TOKEN_L_T expr",
	"relational_expr : expr EXPR_TOKEN_L_E expr",
	"relational_expr : expr EXPR_TOKEN_G_T expr",
	"relational_expr : expr EXPR_TOKEN_G_E expr",
	"named_op_expr : EXPR_TOKEN_NAMED_OP expr",
	"additive_expr : expr EXPR_TOKEN_PLUS expr",
	"additive_expr : expr EXPR_TOKEN_MINUS expr",
	"additive_expr : expr EXPR_TOKEN_DOT expr",
	"multiplicative_expr : expr EXPR_TOKEN_STAR expr",
	"multiplicative_expr : expr EXPR_TOKEN_SLASH expr",
	"multiplicative_expr : expr EXPR_TOKEN_PERCENT expr",
	"matching_expr : expr EXPR_TOKEN_EQUALS expr",
	"matching_expr : expr EXPR_TOKEN_EQUALS_TILDE expr",
	"matching_expr : expr EXPR_TOKEN_BANG_TILDE expr",
	"sign_expr : EXPR_TOKEN_BANG expr",
	"sign_expr : EXPR_TOKEN_PLUS expr",
	"sign_expr : EXPR_TOKEN_MINUS expr",
	"call : EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_LEFTPAREN args EXPR_TOKEN_RIGHTPAREN",
	"args : /* empty */",
	"args : expr",
	"args : args EXPR_TOKEN_COMMA expr",
	"access : EXPR_TOKEN_DOLLAR EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_LEFTBRACE expr EXPR_TOKEN_RIGHTBRACE",
	"access : EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_LEFTBRACE expr EXPR_TOKEN_RIGHTBRACE",
	"variable : EXPR_TOKEN_DOLLAR EXPR_TOKEN_IDENTIFIER",
	"variable : EXPR_TOKEN_DOLLAR EXPR_TOKEN_LEFTBRACE EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_RIGHTBRACE",
	"variable : EXPR_TOKEN_DOLLAR_AMP",
	"variable : EXPR_TOKEN_DOLLAR EXPR_TOKEN_LEFTBRACE EXPR_TOKEN_AMP EXPR_TOKEN_RIGHTBRACE",
	"variable : EXPR_TOKEN_DOLLAR EXPR_TOKEN_NUMBER",
	"variable : EXPR_TOKEN_DOLLAR EXPR_TOKEN_LEFTBRACE EXPR_TOKEN_NUMBER EXPR_TOKEN_RIGHTBRACE",
	"identifier : EXPR_TOKEN_IDENTIFIER",
	"literal : EXPR_TOKEN_NUMBER",
	"literal : EXPR_TOKEN_SINGLE_QUOTE_STRING",
	"literal : EXPR_TOKEN_DOUBLE_QUOTE_STRING",
	"paren : EXPR_TOKEN_LEFTPAREN expr EXPR_TOKEN_RIGHTPAREN",
};
#endif /* YYDEBUG */
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)yaccpar	6.16	99/01/20 SMI"

/*
** Skeleton parser driver for yacc output
*/

/*
** yacc user known macros and defines
*/
#define YYERROR		goto yyerrlab
#define YYACCEPT	return(0)
#define YYABORT		return(1)
#define YYBACKUP( newtoken, newvalue )\
{\
	if ( (yydata->yychar) >= 0 || ( yyr2[ (yydata->yytmp) ] >> 1 ) != 1 )\
	{\
		yyerror( "syntax error - cannot backup" );\
		goto yyerrlab;\
	}\
	(yydata->yychar) = newtoken;\
	(yydata->yystate) = *(yydata->yyps);\
	(yydata->yylval) = newvalue;\
	goto yynewstate;\
}
#define YYRECOVERING()	(!!(yydata->yyerrflag))
#define YYNEW(type)	malloc(sizeof(type) * yynewmax)
#define YYCOPY(to, from, type) \
	(type *) memcpy(to, (char *) from, (yydata->yymaxdepth) * sizeof (type))
#define YYENLARGE( from, type) \
	(type *) realloc((char *) from, yynewmax * sizeof(type))
#ifndef YYDEBUG
#	define YYDEBUG	1	/* make debugging available */
#endif

/*
** user known globals
*/

/*
** driver internal defines
*/
#define YYFLAG		(-10000000)

/*
** global variables used by the parser
*/





#ifdef YYNMBCHARS
#define YYLEX()		yycvtok(yylex())
/*
** yycvtok - return a token if i is a wchar_t value that exceeds 255.
**	If i<255, i itself is the token.  If i>255 but the neither 
**	of the 30th or 31st bit is on, i is already a token.
*/
#if defined(__STDC__) || defined(__cplusplus)
#else
#endif
{
	int first = 0;
	int last = YYNMBCHARS - 1;
	int mid;
	wchar_t j;

	if(i&0x60000000){/*Must convert to a token. */
		if( yymbchars[last].character < i ){
			return i;/*Giving up*/
		}
		while ((last>=first)&&(first>=0)) {/*Binary search loop*/
			mid = (first+last)/2;
			j = yymbchars[mid].character;
			if( j==i ){/*Found*/ 
				return yymbchars[mid].tvalue;
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
#else/*!YYNMBCHARS*/
#define YYLEX()		yylex()
#endif/*!YYNMBCHARS*/

/*
** yyparse - return 0 if worked, 1 if syntax error not recovered from
*/
#if defined(__STDC__) || defined(__cplusplus)
int expr_yy_parse(YYDATA *yydata)
#else
#endif
{
	register YYSTYPE *yypvt = 0;	/* top of value stack for $vars */

#if defined(__cplusplus) || defined(lint)
/*
	hacks to please C++ and lint - goto's inside
	switch should never be executed
*/
	static int __yaccpar_lint_hack__ = 0;
	switch (__yaccpar_lint_hack__)
	{
		case 1: goto yyerrlab;
		case 2: goto yynewstate;
	}
#endif

	/*
	** Initialize externals - yyparse may be called more than once
	*/
	(yydata->yypv) = &(yydata->yyv)[-1];
	(yydata->yyps) = &(yydata->yys)[-1];
	(yydata->yystate) = 0;
	(yydata->yytmp) = 0;
	(yydata->yynerrs) = 0;
	(yydata->yyerrflag) = 0;
	(yydata->yychar) = -1;

#if YYMAXDEPTH <= 0
	if ((yydata->yymaxdepth) <= 0)
	{
		if (((yydata->yymaxdepth) = YYEXPAND(0)) <= 0)
		{
			yyerror("yacc initialization error");
			YYABORT;
		}
	}
#endif

	{
		register YYSTYPE *yy_pv;	/* top of value stack */
		register int *yy_ps;		/* top of state stack */
		register int yy_state;		/* current state */
		register int  yy_n;		/* internal state number info */
	goto yystack;	/* moved from 6 lines above to here to please C++ */

		/*
		** get globals into registers.
		** branch to here only if YYBACKUP was called.
		*/
	yynewstate:
		yy_pv = (yydata->yypv);
		yy_ps = (yydata->yyps);
		yy_state = (yydata->yystate);
		goto yy_newstate;

		/*
		** get globals into registers.
		** either we just started, or we just finished a reduction
		*/
	yystack:
		yy_pv = (yydata->yypv);
		yy_ps = (yydata->yyps);
		yy_state = (yydata->yystate);

		/*
		** top of for (;;) loop while no reductions done
		*/
	yy_stack:
		/*
		** put a state and value onto the stacks
		*/
#if YYDEBUG
		/*
		** if debugging, look up token value in list of value vs.
		** name pairs.  0 and negative (-1) are special values.
		** Note: linear search is used since time is not a real
		** consideration while debugging.
		*/
		if ( yydebug )
		{
			register int yy_i;

			printf( "State %d, token ", yy_state );
			if ( (yydata->yychar) == 0 )
				printf( "end-of-file\n" );
			else if ( (yydata->yychar) < 0 )
				printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == (yydata->yychar) )
						break;
				}
				printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif /* YYDEBUG */
		if ( ++yy_ps >= &(yydata->yys)[ (yydata->yymaxdepth) ] )	/* room on stack? */
		{
			/*
			** reallocate and recover.  Note that pointers
			** have to be reset, or bad things will happen
			*/
			long yyps_index = (yy_ps - (yydata->yys));
			long yypv_index = (yy_pv - (yydata->yyv));
			long yypvt_index = (yypvt - (yydata->yyv));
			int yynewmax;
#ifdef YYEXPAND
			yynewmax = YYEXPAND((yydata->yymaxdepth));
#else
			yynewmax = 2 * (yydata->yymaxdepth);	/* double table size */
			if ((yydata->yymaxdepth) == YYMAXDEPTH)	/* first time growth */
			{
				char *newyys = (char *)YYNEW(int);
				char *newyyv = (char *)YYNEW(YYSTYPE);
				if (newyys != 0 && newyyv != 0)
				{
					(yydata->yys) = YYCOPY(newyys, (yydata->yys), int);
					(yydata->yyv) = YYCOPY(newyyv, (yydata->yyv), YYSTYPE);
				}
				else
					yynewmax = 0;	/* failed */
			}
			else				/* not first time */
			{
				(yydata->yys) = YYENLARGE((yydata->yys), int);
				(yydata->yyv) = YYENLARGE((yydata->yyv), YYSTYPE);
				if ((yydata->yys) == 0 || (yydata->yyv) == 0)
					yynewmax = 0;	/* failed */
			}
#endif
			if (yynewmax <= (yydata->yymaxdepth))	/* tables not expanded */
			{
				yyerror( "yacc stack overflow" );
				YYABORT;
			}
			(yydata->yymaxdepth) = yynewmax;

			yy_ps = (yydata->yys) + yyps_index;
			yy_pv = (yydata->yyv) + yypv_index;
			yypvt = (yydata->yyv) + yypvt_index;
		}
		*yy_ps = yy_state;
		*++yy_pv = (yydata->yyval);

		/*
		** we have a new state - find out what to do
		*/
	yy_newstate:
		if ( ( yy_n = yypact[ yy_state ] ) <= YYFLAG )
			goto yydefault;		/* simple state */
#if YYDEBUG
		/*
		** if debugging, need to mark whether new token grabbed
		*/
		(yydata->yytmp) = (yydata->yychar) < 0;
#endif
		if ( ( (yydata->yychar) < 0 ) && ( ( (yydata->yychar) = YYLEX() ) < 0 ) )
			(yydata->yychar) = 0;		/* reached EOF */
#if YYDEBUG
		if ( yydebug && (yydata->yytmp) )
		{
			register int yy_i;

			printf( "Received token " );
			if ( (yydata->yychar) == 0 )
				printf( "end-of-file\n" );
			else if ( (yydata->yychar) < 0 )
				printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == (yydata->yychar) )
						break;
				}
				printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif /* YYDEBUG */
		if ( ( ( yy_n += (yydata->yychar) ) < 0 ) || ( yy_n >= YYLAST ) )
			goto yydefault;
		if ( yychk[ yy_n = yyact[ yy_n ] ] == (yydata->yychar) )	/*valid shift*/
		{
			(yydata->yychar) = -1;
			(yydata->yyval) = (yydata->yylval);
			yy_state = yy_n;
			if ( (yydata->yyerrflag) > 0 )
				(yydata->yyerrflag)--;
			goto yy_stack;
		}

	yydefault:
		if ( ( yy_n = yydef[ yy_state ] ) == -2 )
		{
#if YYDEBUG
			(yydata->yytmp) = (yydata->yychar) < 0;
#endif
			if ( ( (yydata->yychar) < 0 ) && ( ( (yydata->yychar) = YYLEX() ) < 0 ) )
				(yydata->yychar) = 0;		/* reached EOF */
#if YYDEBUG
			if ( yydebug && (yydata->yytmp) )
			{
				register int yy_i;

				printf( "Received token " );
				if ( (yydata->yychar) == 0 )
					printf( "end-of-file\n" );
				else if ( (yydata->yychar) < 0 )
					printf( "-none-\n" );
				else
				{
					for ( yy_i = 0;
						yytoks[yy_i].t_val >= 0;
						yy_i++ )
					{
						if ( yytoks[yy_i].t_val
							== (yydata->yychar) )
						{
							break;
						}
					}
					printf( "%s\n", yytoks[yy_i].t_name );
				}
			}
#endif /* YYDEBUG */
			/*
			** look through exception table
			*/
			{
				register YYCONST int *yyxi = yyexca;

				while ( ( *yyxi != -1 ) ||
					( yyxi[1] != yy_state ) )
				{
					yyxi += 2;
				}
				while ( ( *(yyxi += 2) >= 0 ) &&
					( *yyxi != (yydata->yychar) ) )
					;
				if ( ( yy_n = yyxi[1] ) < 0 )
					YYACCEPT;
			}
		}

		/*
		** check for syntax error
		*/
		if ( yy_n == 0 )	/* have an error */
		{
			/* no worry about speed here! */
			switch ( (yydata->yyerrflag) )
			{
			case 0:		/* new error */
				yyerror( "syntax error" );
				goto skip_init;
			yyerrlab:
				/*
				** get globals into registers.
				** we have a user generated syntax type error
				*/
				yy_pv = (yydata->yypv);
				yy_ps = (yydata->yyps);
				yy_state = (yydata->yystate);
			skip_init:
				(yydata->yynerrs)++;
				/* FALLTHRU */
			case 1:
			case 2:		/* incompletely recovered error */
					/* try again... */
				(yydata->yyerrflag) = 3;
				/*
				** find state where "error" is a legal
				** shift action
				*/
				while ( yy_ps >= (yydata->yys) )
				{
					yy_n = yypact[ *yy_ps ] + YYERRCODE;
					if ( yy_n >= 0 && yy_n < YYLAST &&
						yychk[yyact[yy_n]] == YYERRCODE)					{
						/*
						** simulate shift of "error"
						*/
						yy_state = yyact[ yy_n ];
						goto yy_stack;
					}
					/*
					** current state has no shift on
					** "error", pop stack
					*/
#if YYDEBUG
#	define _POP_ "Error recovery pops state %d, uncovers state %d\n"
					if ( yydebug )
						printf( _POP_, *yy_ps,
							yy_ps[-1] );
#	undef _POP_
#endif
					yy_ps--;
					yy_pv--;
				}
				/*
				** there is no state on stack with "error" as
				** a valid shift.  give up.
				*/
				YYABORT;
			case 3:		/* no shift yet; eat a token */
#if YYDEBUG
				/*
				** if debugging, look up token in list of
				** pairs.  0 and negative shouldn't occur,
				** but since timing doesn't matter when
				** debugging, it doesn't hurt to leave the
				** tests here.
				*/
				if ( yydebug )
				{
					register int yy_i;

					printf( "Error recovery discards " );
					if ( (yydata->yychar) == 0 )
						printf( "token end-of-file\n" );
					else if ( (yydata->yychar) < 0 )
						printf( "token -none-\n" );
					else
					{
						for ( yy_i = 0;
							yytoks[yy_i].t_val >= 0;
							yy_i++ )
						{
							if ( yytoks[yy_i].t_val
								== (yydata->yychar) )
							{
								break;
							}
						}
						printf( "token %s\n",
							yytoks[yy_i].t_name );
					}
				}
#endif /* YYDEBUG */
				if ( (yydata->yychar) == 0 )	/* reached EOF. quit */
					YYABORT;
				(yydata->yychar) = -1;
				goto yy_newstate;
			}
		}/* end if ( yy_n == 0 ) */
		/*
		** reduction by production yy_n
		** put stack tops, etc. so things right after switch
		*/
#if YYDEBUG
		/*
		** if debugging, print the string that is the user's
		** specification of the reduction which is just about
		** to be done.
		*/
		if ( yydebug )
			printf( "Reduce by (%d) \"%s\"\n",
				yy_n, yyreds[ yy_n ] );
#endif
		(yydata->yytmp) = yy_n;			/* value to switch over */
		yypvt = yy_pv;			/* $vars top of value stack */
		/*
		** Look in goto table for next state
		** Sorry about using yy_state here as temporary
		** register variable, but why not, if it works...
		** If yyr2[ yy_n ] doesn't have the low order bit
		** set, then there is no action to be done for
		** this reduction.  So, no saving & unsaving of
		** registers done.  The only difference between the
		** code just after the if and the body of the if is
		** the goto yy_stack in the body.  This way the test
		** can be made before the choice of what to do is needed.
		*/
		{
			/* length of production doubled with extra bit */
			register int yy_len = yyr2[ yy_n ];

			if ( !( yy_len & 01 ) )
			{
				yy_len >>= 1;
				(yydata->yyval) = ( yy_pv -= yy_len )[1];	/* $$ = $1 */
				yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
					*( yy_ps -= yy_len ) + 1;
				if ( yy_state >= YYLAST ||
					yychk[ yy_state =
					yyact[ yy_state ] ] != -yy_n )
				{
					yy_state = yyact[ yypgo[ yy_n ] ];
				}
				goto yy_stack;
			}
			yy_len >>= 1;
			(yydata->yyval) = ( yy_pv -= yy_len )[1];	/* $$ = $1 */
			yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
				*( yy_ps -= yy_len ) + 1;
			if ( yy_state >= YYLAST ||
				yychk[ yy_state = yyact[ yy_state ] ] != -yy_n )
			{
				yy_state = yyact[ yypgo[ yy_n ] ];
			}
		}
					/* save until reenter driver code */
		(yydata->yystate) = yy_state;
		(yydata->yyps) = yy_ps;
		(yydata->yypv) = yy_pv;
	}
	/*
	** code supplied by user is placed in this switch
	*/
	switch( (yydata->yytmp) )
	{
		
case 21:
{ (yydata->yyval).expr = EXPR_YY_NAMED_OR(yypvt[-2].expr, yypvt[-0].expr); } break;
case 22:
{ (yydata->yyval).expr = EXPR_YY_NAMED_XOR(yypvt[-2].expr, yypvt[-0].expr); } break;
case 23:
{ (yydata->yyval).expr = EXPR_YY_NAMED_AND(yypvt[-2].expr, yypvt[-0].expr); } break;
case 24:
{ (yydata->yyval).expr = EXPR_YY_NAMED_NOT(yypvt[-0].expr); } break;
case 25:
{ (yydata->yyval).expr = EXPR_YY_TERNARY(yypvt[-4].expr, yypvt[-2].expr, yypvt[-0].expr); } break;
case 26:
{ (yydata->yyval).expr = EXPR_YY_C_OR(yypvt[-2].expr, yypvt[-0].expr); } break;
case 27:
{ (yydata->yyval).expr = EXPR_YY_C_AND(yypvt[-2].expr, yypvt[-0].expr); } break;
case 28:
{ (yydata->yyval).expr = EXPR_YY_C_XOR(yypvt[-2].expr, yypvt[-0].expr); } break;
case 29:
{ (yydata->yyval).expr = EXPR_YY_NUMERIC_EQ(yypvt[-2].expr, yypvt[-0].expr); } break;
case 30:
{ (yydata->yyval).expr = EXPR_YY_NUMERIC_NE(yypvt[-2].expr, yypvt[-0].expr); } break;
case 31:
{ (yydata->yyval).expr = EXPR_YY_STRING_EQ(yypvt[-2].expr, yypvt[-0].expr); } break;
case 32:
{ (yydata->yyval).expr = EXPR_YY_STRING_NE(yypvt[-2].expr, yypvt[-0].expr); } break;
case 33:
{ (yydata->yyval).expr = EXPR_YY_NUMERIC_LT(yypvt[-2].expr, yypvt[-0].expr); } break;
case 34:
{ (yydata->yyval).expr = EXPR_YY_NUMERIC_LE(yypvt[-2].expr, yypvt[-0].expr); } break;
case 35:
{ (yydata->yyval).expr = EXPR_YY_NUMERIC_GT(yypvt[-2].expr, yypvt[-0].expr); } break;
case 36:
{ (yydata->yyval).expr = EXPR_YY_NUMERIC_GE(yypvt[-2].expr, yypvt[-0].expr); } break;
case 37:
{ (yydata->yyval).expr = EXPR_YY_STRING_LT(yypvt[-2].expr, yypvt[-0].expr); } break;
case 38:
{ (yydata->yyval).expr = EXPR_YY_STRING_LE(yypvt[-2].expr, yypvt[-0].expr); } break;
case 39:
{ (yydata->yyval).expr = EXPR_YY_STRING_GT(yypvt[-2].expr, yypvt[-0].expr); } break;
case 40:
{ (yydata->yyval).expr = EXPR_YY_STRING_GE(yypvt[-2].expr, yypvt[-0].expr); } break;
case 41:
{ (yydata->yyval).expr = EXPR_YY_NAMED_OP(yypvt[-1].string, yypvt[-0].expr); } break;
case 42:
{ (yydata->yyval).expr = EXPR_YY_ADD(yypvt[-2].expr, yypvt[-0].expr); } break;
case 43:
{ (yydata->yyval).expr = EXPR_YY_SUBTRACT(yypvt[-2].expr, yypvt[-0].expr); } break;
case 44:
{ (yydata->yyval).expr = EXPR_YY_CONCAT(yypvt[-2].expr, yypvt[-0].expr); } break;
case 45:
{ (yydata->yyval).expr = EXPR_YY_MULTIPLY(yypvt[-2].expr, yypvt[-0].expr); } break;
case 46:
{ (yydata->yyval).expr = EXPR_YY_DIVIDE(yypvt[-2].expr, yypvt[-0].expr); } break;
case 47:
{ (yydata->yyval).expr = EXPR_YY_MODULO(yypvt[-2].expr, yypvt[-0].expr); } break;
case 48:
{ (yydata->yyval).expr = EXPR_YY_WILDCARD(yypvt[-2].expr, yypvt[-0].expr); } break;
case 49:
{ (yydata->yyval).expr = EXPR_YY_RE(yypvt[-2].expr, yypvt[-0].expr, PR_TRUE); } break;
case 50:
{ (yydata->yyval).expr = EXPR_YY_RE(yypvt[-2].expr, yypvt[-0].expr, PR_FALSE); } break;
case 51:
{ (yydata->yyval).expr = EXPR_YY_C_NOT(yypvt[-0].expr); } break;
case 52:
{ (yydata->yyval).expr = EXPR_YY_POSITIVE(yypvt[-0].expr); } break;
case 53:
{ (yydata->yyval).expr = EXPR_YY_NEGATIVE(yypvt[-0].expr); } break;
case 54:
{ (yydata->yyval).expr = EXPR_YY_CALL(yypvt[-3].string, yypvt[-1].args); } break;
case 55:
{ (yydata->yyval).args = EXPR_YY_ARGS_EMPTY(); } break;
case 56:
{ (yydata->yyval).args = EXPR_YY_ARGS_CREATE(yypvt[-0].expr); } break;
case 57:
{ (yydata->yyval).args = EXPR_YY_ARGS_APPEND(yypvt[-2].args, yypvt[-0].expr); } break;
case 58:
{ (yydata->yyval).expr = EXPR_YY_ACCESS(yypvt[-3].string, yypvt[-1].expr); } break;
case 59:
{ (yydata->yyval).expr = EXPR_YY_ACCESS(yypvt[-3].string, yypvt[-1].expr); } break;
case 60:
{ (yydata->yyval).expr = EXPR_YY_VARIABLE(yypvt[-0].string); } break;
case 61:
{ (yydata->yyval).expr = EXPR_YY_VARIABLE(yypvt[-1].string); } break;
case 62:
{ (yydata->yyval).expr = EXPR_YY_VARIABLE("&"); } break;
case 63:
{ (yydata->yyval).expr = EXPR_YY_VARIABLE("&"); } break;
case 64:
{ (yydata->yyval).expr = EXPR_YY_VARIABLE(yypvt[-0].string); } break;
case 65:
{ (yydata->yyval).expr = EXPR_YY_VARIABLE(yypvt[-1].string); } break;
case 66:
{ (yydata->yyval).expr = EXPR_YY_IDENTIFIER(yypvt[-0].string); } break;
case 67:
{ (yydata->yyval).expr = EXPR_YY_NUMBER(yypvt[-0].string); } break;
case 68:
{ (yydata->yyval).expr = EXPR_YY_STRING(yypvt[-0].string); } break;
case 69:
{ (yydata->yyval).expr = EXPR_YY_INTERPOLATIVE(yypvt[-0].string); } break;
case 70:
{ (yydata->yyval).expr = yypvt[-1].expr; } break;
	}
	goto yystack;		/* reset registers in driver code */
}

