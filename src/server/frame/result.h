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

#ifndef FRAME_RESULT_H
#define FRAME_RESULT_H

/*
 * result.h: NSAPI expression result processing
 * 
 * Chris Elving
 */

/*
 * Result is the result of evaluating an expression.
 */
typedef struct Result Result;

#ifndef FRAME_EXPR_H
#include "frame/expr.h"
#endif

PR_BEGIN_EXTERN_C

/*
 * result_expr evaluates an expression and returns a pointer to the result.
 */
NSAPI_PUBLIC const Result *INTresult_expr(Session *sn, Request *rq, const Expression *expr);

/*
 * result_error constructs a result from a localized error string.
 */
NSAPI_PUBLIC const Result *INTresult_error(Session *sn, Request *rq, const char *fmt, ...);

/*
 * result_not_enough_args constructs a result that indicates too few arguments
 * were passed.
 */
NSAPI_PUBLIC const Result *INTresult_not_enough_args(Session *sn, Request *rq);

/*
 * result_too_many_args constructs a result that indicates too many arguments
 * were passed.
 */
NSAPI_PUBLIC const Result *INTresult_too_many_args(Session *sn, Request *rq);

/*
 * result_out_of_memory constructs a result that indicates an out of memory
 * error occurred.
 */
NSAPI_PUBLIC const Result *INTresult_out_of_memory(Session *sn, Request *rq);

/*
 * result_bool constructs a result from a boolean.
 */
NSAPI_PUBLIC const Result *INTresult_bool(Session *sn, Request *rq, PRBool b);

/*
 * result_integer constructs a result from an integer.
 */
NSAPI_PUBLIC const Result *INTresult_integer(Session *sn, Request *rq, PRInt64 i);

/*
 * result_string constructs a result from a string.
 */
NSAPI_PUBLIC const Result *INTresult_string(Session *sn, Request *rq, const char *s, int len);

/*
 * result_is_error returns PR_TRUE if the passed result is an error;
 * system_errmsg can be used to retrieve a localized description of the error.
 */
NSAPI_PUBLIC PRBool INTresult_is_error(const Result *result);

/*
 * result_is_bool returns PR_TRUE if the passed result is a boolean.
 */
NSAPI_PUBLIC PRBool INTresult_is_bool(const Result *result);

/*
 * result_is_integer returns PR_TRUE if the passed result is an integer.
 */
NSAPI_PUBLIC PRBool INTresult_is_integer(const Result *result);

/*
 * result_is_string returns PR_TRUE if the passed result is a string.
 */
NSAPI_PUBLIC PRBool INTresult_is_string(const Result *result);

/*
 * result_as_bool converts the passed result to a boolean.
 */
NSAPI_PUBLIC PRBool INTresult_as_bool(Session *sn, Request *rq, const Result *result);

/*
 * result_as_integer converts the passed result to an integer.
 */
NSAPI_PUBLIC PRInt64 INTresult_as_integer(Session *sn, Request *rq, const Result *result);

/*
 * result_as_string converts the passed result to a string.
 */
NSAPI_PUBLIC void INTresult_as_string(Session *sn, Request *rq, const Result *result, char **pp, int *plen);

/*
 * result_as_const_string converts the passed result to a string.
 */
NSAPI_PUBLIC void INTresult_as_const_string(Session *sn, Request *rq, const Result *result, const char **pp, int *len);

PR_END_EXTERN_C

#define result_expr INTresult_expr
#define result_error INTresult_error
#define result_not_enough_args INTresult_not_enough_args
#define result_too_many_args INTresult_too_many_args
#define result_out_of_memory INTresult_out_of_memory
#define result_bool INTresult_bool
#define result_integer INTresult_integer
#define result_string INTresult_string
#define result_is_error INTresult_is_error
#define result_is_bool INTresult_is_bool
#define result_is_integer INTresult_is_integer
#define result_is_string INTresult_is_string
#define result_as_bool INTresult_as_bool
#define result_as_integer INTresult_as_integer
#define result_as_string INTresult_as_string
#define result_as_const_string INTresult_as_const_string

#endif /* FRAME_RESULT_H */
