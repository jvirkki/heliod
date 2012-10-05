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
 * result.cpp: NSAPI expression result processing
 * 
 * Chris Elving
 */

#include <limits.h>

#include "netsite.h"
#include "NsprWrap/NsprError.h"
#include "base/util.h"
#include "base/pool.h"
#include "frame/log.h"
#include "frame/result.h"
#include "frame/dbtframe.h"
#include "expr_pvt.h"
#include "result_pvt.h"


/*
 * Result::out_of_memory is returned when an expression cannot be evaluated due
 * to a lack of memory.
 */
const Result Result::out_of_memory = Result(RESULT_ERROR, PR_FALSE, -1, NULL, "", 0);


/* --------------------------- Context::Context --------------------------- */

Context::Context(Session *snArg,
                 Request *rqArg,
                 pool_handle_t *poolArg)
: sn(snArg),
  rq(rqArg),
  pool(poolArg)
{ }


/* ---------------------- Context::createErrorResult ---------------------- */

Result Context::createErrorResult(const char *s)
{
    if (s == NULL)
        return Result::out_of_memory;

    return createErrorResultf("%s", s);
}


/* --------------------- Context::createErrorResultf ---------------------- */

Result Context::createErrorResultf(const char *fmt, ...)
{
    if (fmt == NULL)
        return Result::out_of_memory;

    va_list args;
    va_start(args, fmt);
    Result r = createErrorResultv(fmt, args);
    va_end(args);

    return r;
}


/* --------------------- Context::createErrorResultv ---------------------- */

Result Context::createErrorResultv(const char *fmt, va_list args)
{
    if (fmt == NULL)
        return Result::out_of_memory;

    char *s = PR_vsmprintf(fmt, args);
    if (s == NULL)
        return Result::out_of_memory;

    int len = strlen(s);

    char *p = (char *) pool_malloc(pool, len + 1);
    if (p == NULL) {
        PR_Free(s);
        return Result::out_of_memory;
    }

    memcpy(p, s, len);
    p[len] = '\0';

    PR_Free(s);

    return Result(RESULT_ERROR, PR_FALSE, -1, pool, p, len);
}


/* -------------------- Context::createNsprErrorResult -------------------- */

Result Context::createNsprErrorResult()
{
    PRInt32 prerr = PR_GetError();

    PR_ASSERT(prerr != 0);

    if (prerr == PR_OUT_OF_MEMORY_ERROR)
        return Result::out_of_memory;

    return createErrorResult(system_errmsg());
}


/* ---------------- Context::createOutOfMemoryErrorResult ----------------- */

Result Context::createOutOfMemoryErrorResult()
{
    return Result::out_of_memory;
}


/* --------------------- Context::createBooleanResult --------------------- */

Result Context::createBooleanResult(PRBool b)
{
    return Result(RESULT_BOOLEAN, b, b, NULL, b ? "1" : "0", 1);
}


/* --------------------- Context::createIntegerResult --------------------- */

Result Context::createIntegerResult(PRInt64 i)
{
    if (i == 0)
        return Result(RESULT_INTEGER, PR_FALSE, 0, NULL, "0", 1);

    if (i == 1)
        return Result(RESULT_INTEGER, PR_TRUE, 1, NULL, "1", 1);

    const int size = sizeof("-9223372036854775808");
    char *p = (char *) pool_malloc(pool, size);
    if (p == NULL)
        return Result::out_of_memory;

    int len;
    if (i < INT_MAX || i > INT_MAX) {
        len = PR_snprintf(p, size, "%lld", i);
    } else {
        len = util_itoa(i, p);
    }

    return Result(RESULT_INTEGER, (i != 0), i, pool, p, len);
}


/* ----------------- Context::createStringConstantResult ------------------ */

Result Context::createStringConstantResult(const char *s, int len)
{
    if (s == NULL || len == 0)
        return Result(RESULT_STRING, PR_FALSE, 0, NULL, "", 0);

    if (len == -1)
        len = strlen(s);

    PRBool b = (len > 1 || len == 1 && *s != '0');
    PRInt64 i = getIntegerValue(s);

    return Result(RESULT_STRING, b, i, NULL, s, len);
}


/* ------------------ Context::createPooledStringResult ------------------- */

Result Context::createPooledStringResult(char *s, int len)
{
    POOL_ASSERT(pool, s);

    if (len == -1)
        len = strlen(s);

    PRBool b = (len > 1 || len == 1 && *s != '0');
    PRInt64 i = getIntegerValue(s);

    return Result(RESULT_STRING, b, i, pool, s, len);
}


/* -------------------- Context::createNewStringResult -------------------- */

Result Context::createNewStringResult(const char *s, int len)
{
    if (s == NULL || len == 0)
        return Result(RESULT_STRING, PR_FALSE, 0, NULL, "", 0);

    if (len == -1)
        len = strlen(s);

    char *p = (char *) pool_malloc(pool, len + 1);
    if (p == NULL)
        return Result::out_of_memory;

    memcpy(p, s, len);
    p[len] = '\0';

    PRBool b = (len > 1 || len == 1 && *p != '0');
    PRInt64 i = getIntegerValue(p);

    return Result(RESULT_STRING, b, i, pool, p, len);
}


/* ----------------------- Context::getIntegerValue ----------------------- */

PRInt64 Context::getIntegerValue(const char *s)
{
    PRInt64 i = 0;

    const char *p = s;
    while (isspace(*p))
        p++;
    PRBool sign = PR_FALSE;
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        sign = PR_TRUE;
        p++;
    }
    while (*p != '\0') {
        if (isdigit(*p)) {
            i = i * 10 + *p - '0';
        } else if (strcspn(p, NUMERIC_IGNORED)) {
            break;
        }
        p++;
    }
    if (sign)
        i = -i;

    return i;
}


/* ---------------------------- Result::Result ---------------------------- */

Result::Result(ResultType typeArg,
               PRBool bArg,
               PRInt64 iArg,
               pool_handle_t *poolArg,
               const char *sArg,
               int lenArg)
: type(typeArg),
  b(bArg),
  i(iArg),
  pool(poolArg),
  s(sArg),
  len(lenArg)
{
#ifdef DEBUG
    // If a pool is specified, s must have been allocated from that pool
    if (pool != NULL)
        POOL_ASSERT(pool, s);

    // If the type is boolean, the integer value must be 0 or 1
    PR_ASSERT(type != RESULT_BOOLEAN || i == 0 || i == 1 || b == i);

    // The passed string must be nul-terminated
    PR_ASSERT(strlen(s) == len);
#endif
}


/* ------------------------- Result::setNsprError ------------------------- */

void Result::setNsprError() const
{
    PR_ASSERT(type == RESULT_ERROR);
    if (len > 0) {
        NsprError::setError(PR_INVALID_ARGUMENT_ERROR, s);
    } else {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
}


/* ----------------------- Result::getPooledString ------------------------ */

char *Result::getPooledString(pool_handle_t *dst) const
{
    if (type == RESULT_ERROR)
        return pool_strdup(dst, "");

    // If the caller's pool is the pool the string was allocated from...
    if (dst == pool && pool != NULL) {
        // We're cool with giving the caller a non-const pointer into his own
        // pool.  This eliminates an unnecessary copy.
        return (char *) s;
    }

    // The string is a constant or was allocated from some other pool.  Create
    // a copy in the caller's pool.
    char *p = (char *) pool_malloc(dst, len + 1);
    if (p != NULL) {
        memcpy(p, s, len);
        p[len] = '\0';
    }

    return p;
}


/* ----------------------------- result_expr ------------------------------ */

const Result *result_expr(Session *sn, Request *rq, const Expression *expr)
{
    pool_handle_t *pool = request_pool(rq);

    Result *result = (Result *) pool_malloc(pool, sizeof(Result));
    if (result == NULL)
        return &Result::out_of_memory;

    Context context(sn, rq, pool);
    *result = expr->evaluate(context);

    return result;
}


/* ----------------------------- result_error ----------------------------- */

const Result *result_error(Session *sn, Request *rq, const char *fmt, ...)
{
    pool_handle_t *pool = request_pool(rq);

    Result *result = (Result *) pool_malloc(pool, sizeof(Result));
    if (result == NULL)
        return &Result::out_of_memory;

    Context context(sn, rq, pool);
    va_list args;
    va_start(args, fmt);
    *result = context.createErrorResultv(fmt, args);
    va_end(args);

    log_error(LOG_FINEST, "result_error", sn, rq, "%s", result->getConstErrorString());

    return result;
}


/* ------------------------ result_not_enough_args ------------------------ */

const Result *result_not_enough_args(Session *sn, Request *rq)
{
    const char *msg = XP_GetAdminStr(DBT_notEnoughArgs);
    if (msg == NULL)
        return &Result::out_of_memory;

    return result_error(sn, rq, "%s", msg);
}


/* ------------------------- result_too_many_args ------------------------- */

const Result *result_too_many_args(Session *sn, Request *rq)
{
    const char *msg = XP_GetAdminStr(DBT_tooManyArgs);
    if (msg == NULL)
        return &Result::out_of_memory;

    return result_error(sn, rq, "%s", msg);
}


/* ------------------------- result_out_of_memory ------------------------- */

const Result *result_out_of_memory(Session *sn, Request *rq)
{
    return &Result::out_of_memory;
}


/* ---------------------------- result_boolean ---------------------------- */

const Result *result_bool(Session *sn, Request *rq, PRBool b)
{
    pool_handle_t *pool = request_pool(rq);

    Result *result = (Result *) pool_malloc(pool, sizeof(Result));
    if (result == NULL)
        return &Result::out_of_memory;

    Context context(sn, rq, pool);
    *result = context.createBooleanResult(b == PR_TRUE);

    return result;
}


/* ---------------------------- result_integer ---------------------------- */

const Result *result_integer(Session *sn, Request *rq, PRInt64 i)
{
    pool_handle_t *pool = request_pool(rq);

    Result *result = (Result *) pool_malloc(pool, sizeof(Result));
    if (result == NULL)
        return &Result::out_of_memory;

    Context context(sn, rq, pool);
    *result = context.createIntegerResult(i);

    return result;
}


/* ---------------------------- result_string ----------------------------- */

const Result *result_string(Session *sn, Request *rq, const char *s, int len)
{
    pool_handle_t *pool = request_pool(rq);

    Result *result = (Result *) pool_malloc(pool, sizeof(Result));
    if (result == NULL)
        return &Result::out_of_memory;

    Context context(sn, rq, pool);
    *result = context.createNewStringResult(s, len);

    return result;
}


/* --------------------------- result_is_error ---------------------------- */

PRBool result_is_error(const Result *result)
{
    if (result->isError()) {
        result->setNsprError();
        return PR_TRUE;
    }

    return PR_FALSE;
}


/* ---------------------------- result_is_bool ---------------------------- */

PRBool result_is_bool(const Result *result)
{
    return result->isBoolean();
}


/* -------------------------- result_is_integer --------------------------- */

PRBool result_is_integer(const Result *result)
{
    return result->isInteger();
}


/* --------------------------- result_is_string --------------------------- */

PRBool result_is_string(const Result *result)
{
    return result->isString();
}


/* ---------------------------- result_as_bool ---------------------------- */

PRBool result_as_bool(Session *sn, Request *rq, const Result *result)
{
    return result->getBoolean();
}


/* -------------------------- result_as_integer --------------------------- */

PRInt64 result_as_integer(Session *sn, Request *rq, const Result *result)
{
    return result->getInteger();
}


/* --------------------------- result_as_string --------------------------- */

void result_as_string(Session *sn, Request *rq, const Result *result, char **pp, int *plen)
{
    int len = 0;

    if (pp != NULL) {
        *pp = result->getPooledString(request_pool(rq));
        if (*pp != NULL)
            len = result->getStringLength();
    }

    if (plen != NULL)
        *plen = len;
}


/* ------------------------ result_as_const_string ------------------------ */

void result_as_const_string(Session *sn, Request *rq, const Result *result, const char **pp, int *plen)
{
    if (pp != NULL)
        *pp = result->getConstString();
    if (plen != NULL)
        *plen = result->getStringLength();
}
