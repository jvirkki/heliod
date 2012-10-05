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

#ifndef FRAME_ARGS_H
#define FRAME_ARGS_H

/*
 * args.h: NSAPI expression argument lists
 * 
 * Chris Elving
 */

/*
 * Arguments is an argument list.
 */
typedef struct Arguments Arguments;

#ifndef FRAME_EXPR_H
#include "frame/expr.h"
#endif

PR_BEGIN_EXTERN_C

/*
 * args_create creates an argument list.
 */
NSAPI_PUBLIC Arguments *INTargs_create(void);

/*
 * args_append adds an expression to the end of an agument list.  If args is
 * NULL, a new argument list is created.
 */
NSAPI_PUBLIC Arguments *INTargs_append(Arguments *args, Expression *expr);

/*
 * args_dup creates a deep copy of an argument list that includes copies of any
 * contained expressions.
 */
NSAPI_PUBLIC Arguments *INTargs_dup(const Arguments *args);

/*
 * args_free destroys an argument list, including any contained expression.
 */
NSAPI_PUBLIC void INTargs_free(Arguments *args);

/*
 * args_length returns the number of expressions in an argument list.
 */
NSAPI_PUBLIC int INTargs_length(const Arguments *args);

/*
 * args_get returns the ith expression from an argument list.
 */
NSAPI_PUBLIC const Expression *INTargs_get(const Arguments *args, int i);

PR_END_EXTERN_C

#define args_create INTargs_create
#define args_append INTargs_append
#define args_dup INTargs_dup
#define args_free INTargs_free
#define args_length INTargs_length
#define args_get INTargs_get

#endif /* FRAME_ARGS_H */
