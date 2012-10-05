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


typedef union
#ifdef __cplusplus
	YYSTYPE
#endif
 {
    const char *string;
    Expression *expr;
    Arguments *args;
} YYSTYPE;
# define EXPR_TOKEN_COMMA 257
# define EXPR_TOKEN_NUMBER 258
# define EXPR_TOKEN_IDENTIFIER 259
# define EXPR_TOKEN_SINGLE_QUOTE_STRING 260
# define EXPR_TOKEN_DOUBLE_QUOTE_STRING 261
# define EXPR_TOKEN_DOLLAR_AMP 262
# define EXPR_TOKEN_AMP 263
# define EXPR_TOKEN_O_R 264
# define EXPR_TOKEN_X_O_R 265
# define EXPR_TOKEN_A_N_D 266
# define EXPR_TOKEN_N_O_T 267
# define EXPR_TOKEN_QUESTION 268
# define EXPR_TOKEN_COLON 269
# define EXPR_TOKEN_PIPE_PIPE 270
# define EXPR_TOKEN_AMP_AMP 271
# define EXPR_TOKEN_CARAT 272
# define EXPR_TOKEN_EQUALS_EQUALS 273
# define EXPR_TOKEN_BANG_EQUALS 274
# define EXPR_TOKEN_E_Q 275
# define EXPR_TOKEN_N_E 276
# define EXPR_TOKEN_LEFTANGLE 277
# define EXPR_TOKEN_LEFTANGLE_EQUALS 278
# define EXPR_TOKEN_RIGHTANGLE 279
# define EXPR_TOKEN_RIGHTANGLE_EQUALS 280
# define EXPR_TOKEN_L_T 281
# define EXPR_TOKEN_L_E 282
# define EXPR_TOKEN_G_T 283
# define EXPR_TOKEN_G_E 284
# define EXPR_TOKEN_NAMED_OP 285
# define EXPR_TOKEN_PLUS 286
# define EXPR_TOKEN_MINUS 287
# define EXPR_TOKEN_DOT 288
# define EXPR_TOKEN_STAR 289
# define EXPR_TOKEN_SLASH 290
# define EXPR_TOKEN_PERCENT 291
# define EXPR_TOKEN_EQUALS 292
# define EXPR_TOKEN_EQUALS_TILDE 293
# define EXPR_TOKEN_BANG_TILDE 294
# define EXPR_TOKEN_BANG 295
# define EXPR_PRECEDENCE_UNARY_PLUS 296
# define EXPR_PRECEDENCE_UNARY_MINUS 297
# define EXPR_TOKEN_DOLLAR 298
# define EXPR_TOKEN_RIGHTPAREN 299
# define EXPR_TOKEN_RIGHTBRACE 300
# define EXPR_TOKEN_LEFTPAREN 301
# define EXPR_TOKEN_LEFTBRACE 302
