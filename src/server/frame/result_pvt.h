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

#ifndef FRAME_RESULT_PVT_H
#define FRAME_RESULT_PVT_H

/*
 * result_pvt.h: NSAPI expression result processing private declarations
 * 
 * Chris Elving
 */

/*
 * NUMERIC_IGNORED contains the characters we ignore in numeric values.
 */
#define NUMERIC_IGNORED ".:/, \t\r\n"

/*
 * NUMERIC_CHARSET contains the characters we accept in numeric values.
 */
#define NUMERIC_CHARSET "+-0123456789"NUMERIC_IGNORED

/*
 * ResultType identifies the type of result that resulted from an Expression
 * evaluation.
 */
enum ResultType {
    RESULT_ERROR = -1,
    RESULT_BOOLEAN = 0,
    RESULT_INTEGER = 1,
    RESULT_STRING = 2
};

/*
 * Result is the result of evaluating an Expression in a Context.
 */
class Result {
public:
    PRBool isError() const { return type == RESULT_ERROR; }
    PRBool isBoolean() const { return type == RESULT_BOOLEAN; }
    PRBool isInteger() const { return type == RESULT_INTEGER; }
    PRBool isString() const { return type == RESULT_STRING; }
    void setNsprError() const;
    PRBool getBoolean() const { return b; }
    PRInt64 getInteger() const { return i; }
    const char *getConstString() const { return isError() ? "" : s; }
    const char *getConstErrorString() const { return isError() ? s : NULL; }
    int getStringLength() const { return isError() ? 0 : len; }
    char *getPooledString(pool_handle_t *pool) const;

    static const Result out_of_memory;

private:
    inline Result(ResultType type, PRBool b, PRInt64 i, pool_handle_t *pool, const char *s, int len);

    ResultType type;
    PRBool b;
    PRInt64 i;
    pool_handle_t *pool;
    const char *s;
    int len;

friend class Context;
};

/*
 * Context defines an environment in which an Expression may be evaluated.
 */
class Context {
public:
    /*
     * Define a context for evaluating Expressions.
     */
    Context(Session *sn, Request *rq, pool_handle_t *pool);

    /*
     * Create a Result from a localized error string.
     */
    Result createErrorResult(const char *s);

    /*
     * Create a Result from a localized error string format and arguments.
     */
    Result createErrorResultf(const char *fmt, ...);

    /*
     * Create a Result from a localized error string format and arguments.
     */
    Result createErrorResultv(const char *fmt, va_list args);

    /*
     * Create a Result from the current NSPR error state.
     */
    Result createNsprErrorResult();

    /*
     * Create a Result that indicates an out of memory error.
     */
    Result createOutOfMemoryErrorResult();

    /*
     * Create a Result from a boolean.
     */
    Result createBooleanResult(PRBool b);

    /*
     * Create a Result from an integer.
     */
    Result createIntegerResult(PRInt64 i);

    /*
     * Create a Result from a string constant.  The contents of the string are
     * not copied.
     */
    Result createStringConstantResult(const char *s, int len = -1);

    /*
     * Create a Result from a string that was previously allocated from this
     * Context's pool.  The contents of the string are not copied.
     */
    Result createPooledStringResult(char *s, int len = -1);

    /*
     * Create a Result from a string.  A new copy of the value is allocated
     * from the Context's pool.
     *
     * If s is a constant, you should call createStringConstantResult instead.
     *
     * If s was previously allocated from this Context's pool, you should call
     * createPooledStringResult instead.
     */
    Result createNewStringResult(const char *s, int len = -1);

    Session * const sn;
    Request * const rq;
    pool_handle_t * const pool;

private:
    static inline PRInt64 getIntegerValue(const char *s);
};

#endif /* FRAME_RESULT_PVT_H */
