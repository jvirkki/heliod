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

%{
#include "expr_yy.h"
%}

%union {
    const char *string;
    Expression *expr;
    Arguments *args;
}

%type <expr> expr 
%type <expr> named_or_expr
%type <expr> named_and_expr
%type <expr> named_not_expr
%type <expr> ternary_expr
%type <expr> c_or_expr
%type <expr> c_and_expr
%type <expr> c_xor_expr
%type <expr> equality_expr
%type <expr> relational_expr
%type <expr> named_op_expr
%type <expr> additive_expr
%type <expr> multiplicative_expr
%type <expr> matching_expr
%type <expr> sign_expr
%type <expr> call
%type <expr> access
%type <expr> variable
%type <expr> identifier
%type <expr> literal
%type <expr> paren
%type <args> args

%left <string> EXPR_TOKEN_COMMA
%nonassoc <string> EXPR_TOKEN_NUMBER EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_SINGLE_QUOTE_STRING EXPR_TOKEN_DOUBLE_QUOTE_STRING EXPR_TOKEN_DOLLAR_AMP EXPR_TOKEN_AMP
%left <string> EXPR_TOKEN_O_R EXPR_TOKEN_X_O_R
%left <string> EXPR_TOKEN_A_N_D
%right <string> EXPR_TOKEN_N_O_T
%right <string> EXPR_TOKEN_QUESTION EXPR_TOKEN_COLON
%left <string> EXPR_TOKEN_PIPE_PIPE
%left <string> EXPR_TOKEN_AMP_AMP
%left <string> EXPR_TOKEN_CARAT
%nonassoc <string> EXPR_TOKEN_EQUALS_EQUALS EXPR_TOKEN_BANG_EQUALS EXPR_TOKEN_E_Q EXPR_TOKEN_N_E
%nonassoc <string> EXPR_TOKEN_LEFTANGLE EXPR_TOKEN_LEFTANGLE_EQUALS EXPR_TOKEN_RIGHTANGLE EXPR_TOKEN_RIGHTANGLE_EQUALS EXPR_TOKEN_L_T EXPR_TOKEN_L_E EXPR_TOKEN_G_T EXPR_TOKEN_G_E
%right <string> EXPR_TOKEN_NAMED_OP
%left <string> EXPR_TOKEN_PLUS EXPR_TOKEN_MINUS EXPR_TOKEN_DOT
%left <string> EXPR_TOKEN_STAR EXPR_TOKEN_SLASH EXPR_TOKEN_PERCENT
%nonassoc <string> EXPR_TOKEN_EQUALS EXPR_TOKEN_EQUALS_TILDE EXPR_TOKEN_BANG_TILDE
%right <string> EXPR_TOKEN_BANG EXPR_PRECEDENCE_UNARY_PLUS EXPR_PRECEDENCE_UNARY_MINUS
%nonassoc <string> EXPR_TOKEN_DOLLAR EXPR_TOKEN_RIGHTPAREN EXPR_TOKEN_RIGHTBRACE EXPR_TOKEN_LEFTPAREN EXPR_TOKEN_LEFTBRACE

%%

expr : named_or_expr
     | named_and_expr
     | named_not_expr
     | ternary_expr
     | c_or_expr
     | c_and_expr
     | c_xor_expr
     | equality_expr
     | relational_expr
     | named_op_expr
     | additive_expr
     | multiplicative_expr
     | matching_expr
     | sign_expr
     | call
     | access
     | variable
     | identifier
     | literal
     | paren
     ;

named_or_expr : expr EXPR_TOKEN_O_R expr
                { $$ = EXPR_YY_NAMED_OR($1, $3); }
              | expr EXPR_TOKEN_X_O_R expr
                { $$ = EXPR_YY_NAMED_XOR($1, $3); }
              ;

named_and_expr : expr EXPR_TOKEN_A_N_D expr
                 { $$ = EXPR_YY_NAMED_AND($1, $3); }
               ;

named_not_expr : EXPR_TOKEN_N_O_T expr
                 { $$ = EXPR_YY_NAMED_NOT($2); }
               ;

ternary_expr : expr EXPR_TOKEN_QUESTION expr EXPR_TOKEN_COLON expr
               { $$ = EXPR_YY_TERNARY($1, $3, $5); }
             ;

c_or_expr : expr EXPR_TOKEN_PIPE_PIPE expr
            { $$ = EXPR_YY_C_OR($1, $3); }
          ;

c_and_expr : expr EXPR_TOKEN_AMP_AMP expr
             { $$ = EXPR_YY_C_AND($1, $3); }
           ;

c_xor_expr : expr EXPR_TOKEN_CARAT expr
             { $$ = EXPR_YY_C_XOR($1, $3); }
           ;

equality_expr : expr EXPR_TOKEN_EQUALS_EQUALS expr
                { $$ = EXPR_YY_NUMERIC_EQ($1, $3); }
              | expr EXPR_TOKEN_BANG_EQUALS expr
                { $$ = EXPR_YY_NUMERIC_NE($1, $3); }
              | expr EXPR_TOKEN_E_Q expr
                { $$ = EXPR_YY_STRING_EQ($1, $3); }
              | expr EXPR_TOKEN_N_E expr
                { $$ = EXPR_YY_STRING_NE($1, $3); }
              ;

relational_expr : expr EXPR_TOKEN_LEFTANGLE expr
                  { $$ = EXPR_YY_NUMERIC_LT($1, $3); }
                | expr EXPR_TOKEN_LEFTANGLE_EQUALS expr
                  { $$ = EXPR_YY_NUMERIC_LE($1, $3); }
                | expr EXPR_TOKEN_RIGHTANGLE expr
                  { $$ = EXPR_YY_NUMERIC_GT($1, $3); }
                | expr EXPR_TOKEN_RIGHTANGLE_EQUALS expr
                  { $$ = EXPR_YY_NUMERIC_GE($1, $3); }
                | expr EXPR_TOKEN_L_T expr
                  { $$ = EXPR_YY_STRING_LT($1, $3); }
                | expr EXPR_TOKEN_L_E expr
                  { $$ = EXPR_YY_STRING_LE($1, $3); }
                | expr EXPR_TOKEN_G_T expr
                  { $$ = EXPR_YY_STRING_GT($1, $3); }
                | expr EXPR_TOKEN_G_E expr
                  { $$ = EXPR_YY_STRING_GE($1, $3); }
                ;

named_op_expr : EXPR_TOKEN_NAMED_OP expr
                { $$ = EXPR_YY_NAMED_OP($1, $2); }
              ;

additive_expr : expr EXPR_TOKEN_PLUS expr
                { $$ = EXPR_YY_ADD($1, $3); }
              | expr EXPR_TOKEN_MINUS expr
                { $$ = EXPR_YY_SUBTRACT($1, $3); }
              | expr EXPR_TOKEN_DOT expr
                { $$ = EXPR_YY_CONCAT($1, $3); }
              ;

multiplicative_expr : expr EXPR_TOKEN_STAR expr
                      { $$ = EXPR_YY_MULTIPLY($1, $3); }
                    | expr EXPR_TOKEN_SLASH expr
                      { $$ = EXPR_YY_DIVIDE($1, $3); }
                    | expr EXPR_TOKEN_PERCENT expr
                      { $$ = EXPR_YY_MODULO($1, $3); }
                    ;

matching_expr : expr EXPR_TOKEN_EQUALS expr
                { $$ = EXPR_YY_WILDCARD($1, $3); }
              | expr EXPR_TOKEN_EQUALS_TILDE expr
                { $$ = EXPR_YY_RE($1, $3, PR_TRUE); }
              | expr EXPR_TOKEN_BANG_TILDE expr
                { $$ = EXPR_YY_RE($1, $3, PR_FALSE); }
              ;

sign_expr : EXPR_TOKEN_BANG expr
            { $$ = EXPR_YY_C_NOT($2); }
          | EXPR_TOKEN_PLUS expr %prec EXPR_PRECEDENCE_UNARY_PLUS
            { $$ = EXPR_YY_POSITIVE($2); }
          | EXPR_TOKEN_MINUS expr %prec EXPR_PRECEDENCE_UNARY_MINUS
            { $$ = EXPR_YY_NEGATIVE($2); }
          ;

call : EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_LEFTPAREN args EXPR_TOKEN_RIGHTPAREN
       { $$ = EXPR_YY_CALL($1, $3); }
     ;

args :
       { $$ = EXPR_YY_ARGS_EMPTY(); }
     | expr
       { $$ = EXPR_YY_ARGS_CREATE($1); }
     | args EXPR_TOKEN_COMMA expr
       { $$ = EXPR_YY_ARGS_APPEND($1, $3); }
     ;

access : EXPR_TOKEN_DOLLAR EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_LEFTBRACE expr EXPR_TOKEN_RIGHTBRACE
         { $$ = EXPR_YY_ACCESS($2, $4); }
       | EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_LEFTBRACE expr EXPR_TOKEN_RIGHTBRACE
         { $$ = EXPR_YY_ACCESS($1, $3); }
       ;

variable : EXPR_TOKEN_DOLLAR EXPR_TOKEN_IDENTIFIER
           { $$ = EXPR_YY_VARIABLE($2); }
         | EXPR_TOKEN_DOLLAR EXPR_TOKEN_LEFTBRACE EXPR_TOKEN_IDENTIFIER EXPR_TOKEN_RIGHTBRACE
           { $$ = EXPR_YY_VARIABLE($3); }
         | EXPR_TOKEN_DOLLAR_AMP
           { $$ = EXPR_YY_VARIABLE("&"); }
         | EXPR_TOKEN_DOLLAR EXPR_TOKEN_LEFTBRACE EXPR_TOKEN_AMP EXPR_TOKEN_RIGHTBRACE
           { $$ = EXPR_YY_VARIABLE("&"); }
         | EXPR_TOKEN_DOLLAR EXPR_TOKEN_NUMBER
           { $$ = EXPR_YY_VARIABLE($2); }
         | EXPR_TOKEN_DOLLAR EXPR_TOKEN_LEFTBRACE EXPR_TOKEN_NUMBER EXPR_TOKEN_RIGHTBRACE
           { $$ = EXPR_YY_VARIABLE($3); }
         ;

identifier : EXPR_TOKEN_IDENTIFIER
             { $$ = EXPR_YY_IDENTIFIER($1); }
           ;

literal : EXPR_TOKEN_NUMBER
          { $$ = EXPR_YY_NUMBER($1); }
        | EXPR_TOKEN_SINGLE_QUOTE_STRING
          { $$ = EXPR_YY_STRING($1); }
        | EXPR_TOKEN_DOUBLE_QUOTE_STRING
          { $$ = EXPR_YY_INTERPOLATIVE($1); }
        ;

paren : EXPR_TOKEN_LEFTPAREN expr EXPR_TOKEN_RIGHTPAREN
        { $$ = $2; }
      ;

%%
