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

#ifndef FRAME_EXPR_H
#define FRAME_EXPR_H

/*
 * expr.h: NSAPI expression evaluation
 * 
 * Chris Elving
 */

/*
 * Expression is a node in an expression tree.
 */
typedef struct Expression Expression;

#ifndef BASE_PBLOCK_H
#include "base/pblock.h"
#endif

#ifndef FRAME_ARGS_H
#include "frame/args.h"
#endif

#ifndef FRAME_RESULT_H
#include "frame/result.h"
#endif

PR_BEGIN_EXTERN_C

/*
 * EXPR_MAX_BACKREFS is the maximum number of regular expression backreference
 * values that can be defined.
 */
#define EXPR_MAX_BACKREFS 10

/*
 * RequestBackrefData records the values of regular expression backreferences.
 */
typedef struct RequestBackrefData RequestBackrefData;
struct RequestBackrefData {
    struct {
        char *p;
        int len;
    } backrefs[EXPR_MAX_BACKREFS];
    int nbackrefs;
};

/*
 * ExpressionFunc is the function prototype for pluggable functions that
 * participate in expression evaluation.
 *
 * An ExpressionFunc may optionally evaluate its arguments.  To evaluate an
 * argument, the ExpressionFunc first calls args_get to retrieve an expression
 * from its argument list.  The ExpressionFunc then passes this expression to
 * to result_expr and checks the result with result_is_error.  If
 * result_is_error indicates that a given result is an error, the
 * ExpressionFunc should abort its normal processing and return that result.
 *
 * After completing its processing, an ExpressionFunc must return a result
 * it received from a result_* function.  Note that it is illegal for an
 * ExpressionFunc to return NULL.
 */
typedef const Result *(ExpressionFunc)(const Arguments *args, Session *sn, Request *rq);

/*
 * expr_var_get_func_insert registers the function responsible for retrieving
 * the value of the named predefined variable.
 */
NSAPI_PUBLIC void INTexpr_var_get_func_insert(const char *name, ExpressionFunc *func);

/*
 * expr_var_get_func_find returns the function responsible for retrieving the
 * value of the named predefined variable.  Returns NULL if name is not the
 * name of a predefined variable.
 */
NSAPI_PUBLIC ExpressionFunc *INTexpr_var_get_func_find(const char *name);

/*
 * expr_map_get_func_insert registers the function responsible for retrieving
 * values from the named predefined map variable.
 */
NSAPI_PUBLIC void INTexpr_map_get_func_insert(const char *name, ExpressionFunc *func);

/*
 * expr_map_get_func_find returns the function responsible for retrieving
 * values from the named predefined map variable.  Returns NULL if name is not
 * the name of a predefined map variable.
 */
NSAPI_PUBLIC ExpressionFunc *INTexpr_map_get_func_find(const char *name);

/*
 * expr_control_func_insert registers the function that implements the named
 * control function.
 */
NSAPI_PUBLIC void INTexpr_control_func_insert(const char *name, ExpressionFunc *func);

/*
 * expr_control_func_find returns the function that implements the named
 * control function.  Returns NULL if name is not the name of a predefined
 * control function.
 */
NSAPI_PUBLIC ExpressionFunc *INTexpr_control_func_find(const char *name);

/*
 * expr_create creates an expression from a string.  Returns NULL on error;
 * system_errmsg can be used to retrieve a localized escription of the error.
 */
NSAPI_PUBLIC Expression *INTexpr_create(const char *s);

/*
 * expr_evaluate evaluates an expression in the specified request context.
 * Returns REQ_PROCEED if the expression evaluated to logical true or was NULL
 * or REQ_NOACTION if the expression evaluated to logical false.  Returns
 * REQ_ABORTED on error; system_errmsg can be used to retrieve a localized
 * description of the error.
 */
NSAPI_PUBLIC int INTexpr_evaluate(const Expression *expr, Session *sn, Request *rq);

/*
 * expr_get_backrefs retrieves the regular expression backreference values
 * determined by the most recent call to expr_evaluate for the specified
 * request context.
 */
NSAPI_PUBLIC void INTexpr_get_backrefs(RequestBackrefData *rb, Session *sn, Request *rq);

/*
 * expr_set_backrefs restores the regular expression backreference values
 * previously returned by expr_get_backrefs for the specified request context.
 */
NSAPI_PUBLIC void INTexpr_set_backrefs(const RequestBackrefData *rb, Session *sn, Request *rq);

/*
 * expr_const_string returns a const char * if the passed expression will
 * always evaluate to that particular string.
 */
NSAPI_PUBLIC const char *INTexpr_const_string(const Expression *expr);

/*
 * expr_const_pb_key returns a const pb_key * if the passed expression will
 * always evaluate to a particular string with that pb_key.
 */
NSAPI_PUBLIC const pb_key *INTexpr_const_pb_key(const Expression *expr);

/*
 * expr_format formats the expression as a string.  The caller should free the
 * returned string using FREE().
 */
NSAPI_PUBLIC char *INTexpr_format(const Expression *expr);

/*
 * expr_dup creates a deep copy of an expression that includes copies of any
 * child expressions.
 */
NSAPI_PUBLIC Expression *INTexpr_dup(const Expression *expr);

/*
 * expr_free destroys an expression, including any child expressions.
 */
NSAPI_PUBLIC void INTexpr_free(Expression *expr);

/*
 * expr_new_* can be used to construct an expression tree.
 */
Expression *INTexpr_new_named_or(Expression *l, Expression *r);
Expression *INTexpr_new_named_xor(Expression *l, Expression *r);
Expression *INTexpr_new_named_and(Expression *l, Expression *r);
Expression *INTexpr_new_named_not(Expression *r);
Expression *INTexpr_new_ternary(Expression *x, Expression *y, Expression *z);
Expression *INTexpr_new_c_or(Expression *l, Expression *r);
Expression *INTexpr_new_c_and(Expression *l, Expression *r);
Expression *INTexpr_new_c_xor(Expression *l, Expression *r);
Expression *INTexpr_new_numeric_eq(Expression *l, Expression *r);
Expression *INTexpr_new_numeric_ne(Expression *l, Expression *r);
Expression *INTexpr_new_string_eq(Expression *l, Expression *r);
Expression *INTexpr_new_string_ne(Expression *l, Expression *r);
Expression *INTexpr_new_numeric_lt(Expression *l, Expression *r);
Expression *INTexpr_new_numeric_le(Expression *l, Expression *r);
Expression *INTexpr_new_numeric_gt(Expression *l, Expression *r);
Expression *INTexpr_new_numeric_ge(Expression *l, Expression *r);
Expression *INTexpr_new_string_lt(Expression *l, Expression *r);
Expression *INTexpr_new_string_le(Expression *l, Expression *r);
Expression *INTexpr_new_string_gt(Expression *l, Expression *r);
Expression *INTexpr_new_string_ge(Expression *l, Expression *r);
Expression *INTexpr_new_named_op(const char *n, Expression *r);
Expression *INTexpr_new_add(Expression *l, Expression *r);
Expression *INTexpr_new_subtract(Expression *l, Expression *r);
Expression *INTexpr_new_concat(Expression *l, Expression *r);
Expression *INTexpr_new_multiply(Expression *l, Expression *r);
Expression *INTexpr_new_divide(Expression *l, Expression *r);
Expression *INTexpr_new_modulo(Expression *l, Expression *r);
Expression *INTexpr_new_wildcard(Expression *l, Expression *r);
Expression *INTexpr_new_re(Expression *l, Expression *r, PRBool match);
Expression *INTexpr_new_c_not(Expression *r);
Expression *INTexpr_new_positive(Expression *r);
Expression *INTexpr_new_negative(Expression *r);
Expression *INTexpr_new_call(const char *f, Arguments *a);
Expression *INTexpr_new_access(const char *i, Expression *s);
Expression *INTexpr_new_variable(const char *i);
Expression *INTexpr_new_identifier(const char *i);
Expression *INTexpr_new_true(void);
Expression *INTexpr_new_false(void);
Expression *INTexpr_new_number(const char *n);
Expression *INTexpr_new_string(const char *s);
Expression *INTexpr_new_interpolative(const char *s);

PR_END_EXTERN_C

#define expr_var_get_func_insert INTexpr_var_get_func_insert
#define expr_var_get_func_find INTexpr_var_get_func_find
#define expr_map_get_func_insert INTexpr_map_get_func_insert
#define expr_map_get_func_find INTexpr_map_get_func_find
#define expr_control_func_insert INTexpr_control_func_insert
#define expr_control_func_find INTexpr_control_func_find
#define expr_create INTexpr_create
#define expr_evaluate INTexpr_evaluate
#define expr_get_backrefs INTexpr_get_backrefs
#define expr_set_backrefs INTexpr_set_backrefs
#define expr_const_string INTexpr_const_string
#define expr_const_pb_key INTexpr_const_pb_key
#define expr_format INTexpr_format
#define expr_dup INTexpr_dup
#define expr_free INTexpr_free
#define expr_new_named_or INTexpr_new_named_or
#define expr_new_named_xor INTexpr_new_named_xor
#define expr_new_named_and INTexpr_new_named_and
#define expr_new_named_not INTexpr_new_named_not
#define expr_new_ternary INTexpr_new_ternary
#define expr_new_c_or INTexpr_new_c_or
#define expr_new_c_and INTexpr_new_c_and
#define expr_new_c_xor INTexpr_new_c_xor
#define expr_new_numeric_eq INTexpr_new_numeric_eq
#define expr_new_numeric_ne INTexpr_new_numeric_ne
#define expr_new_string_eq INTexpr_new_string_eq
#define expr_new_string_ne INTexpr_new_string_ne
#define expr_new_numeric_lt INTexpr_new_numeric_lt
#define expr_new_numeric_le INTexpr_new_numeric_le
#define expr_new_numeric_gt INTexpr_new_numeric_gt
#define expr_new_numeric_ge INTexpr_new_numeric_ge
#define expr_new_string_lt INTexpr_new_string_lt
#define expr_new_string_le INTexpr_new_string_le
#define expr_new_string_gt INTexpr_new_string_gt
#define expr_new_string_ge INTexpr_new_string_ge
#define expr_new_named_op INTexpr_new_named_op
#define expr_new_add INTexpr_new_add
#define expr_new_subtract INTexpr_new_subtract
#define expr_new_concat INTexpr_new_concat
#define expr_new_multiply INTexpr_new_multiply
#define expr_new_divide INTexpr_new_divide
#define expr_new_modulo INTexpr_new_modulo
#define expr_new_wildcard INTexpr_new_wildcard
#define expr_new_re INTexpr_new_re
#define expr_new_c_not INTexpr_new_c_not
#define expr_new_positive INTexpr_new_positive
#define expr_new_negative INTexpr_new_negative
#define expr_new_call INTexpr_new_call
#define expr_new_access INTexpr_new_access
#define expr_new_variable INTexpr_new_variable
#define expr_new_identifier INTexpr_new_identifier
#define expr_new_true INTexpr_new_true
#define expr_new_false INTexpr_new_false
#define expr_new_number INTexpr_new_number
#define expr_new_string INTexpr_new_string
#define expr_new_interpolative INTexpr_new_interpolative

#endif /* FRAME_EXPR_H */
