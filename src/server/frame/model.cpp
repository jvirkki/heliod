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
 * model.cpp: String and pblock interpolation
 * 
 * Chris Elving
 */

#include "netsite.h"
#include "base/pool.h"
#include "base/ereport.h"
#include "frame/expr.h"
#include "frame/model.h"
#include "frame/dbtframe.h"
#include "expr_parse.h"
#include "expr_pvt.h"
#include "model_pvt.h"


/*
 * FragmentType names the different types of fragments that may be encountered
 * while scanning an interpolative string.
 */
enum FragmentType {
    FRAGMENT_TYPE_NONESCAPED,
    FRAGMENT_TYPE_ESCAPED_DOLLAR,
    FRAGMENT_TYPE_VARIABLE,
    FRAGMENT_TYPE_MAP,
    FRAGMENT_TYPE_EXPRESSION
};

/*
 * ScannedFragment contains information collected while scanning a string
 * fragment.
 */
struct ScannedFragment {
    FragmentType type;

    // Valid for FRAGMENT_TYPE_NONESCAPED and FRAGMENT_TYPE_ESCAPED_DOLLAR
    struct {
        const char *p;
        int len;
    } text;

    // Valid for FRAGMENT_TYPE_VARIABLE and FRAGMENT_TYPE_MAP
    struct {
        const char *p;
        int len;
    } identifier;

    // Valid for FRAGMENT_TYPE_MAP
    struct {
        const char *p;
        int len;
    } subscript;

    // Valid for FRAGMENT_TYPE_EXPRESSION
    struct {
        const char *p;
        int len;
    } expression;
};

/*
 * Interpolator provides an environment in which a synthetic string may be
 * constructed.
 */
class Interpolator {
public:
    /*
     * Prepare to construct a synthetic string in the given Context.
     */
    inline Interpolator(Context& context);

    /*
     * Return a pointer where len bytes of a synthetic string may be stored.
     * Returns NULL on out of memory errors.
     */
    inline char *require(int len);
    
    /*
     * Indicate that len bytes have been stored at the address most recently
     * returned by require.
     */
    inline void advance(int len);
    
    /*
     * Nul-terminate the synthetic string and return a pointer to it.  The
     * returned string will have been allocated from the pool of the Context
     * specified when this Interpolator was constructed.  The returned string
     * is not freed when the context is destroyed.  Returns NULL on out of
     * memory errors.
     */
    inline char *terminate();

    /*
     * Return the current length of the synthetic string, excluding any
     * terminating nul.
     */
    inline int length() const { return pos; }

    /*
     * The context (sn, rq, and pool) the synthetic string is being constructed
     * in.
     */
    Context& context;

private:
    Interpolator(const Interpolator&);
    Interpolator& operator=(const Interpolator&);

    char *p;
    int pos;
    int size;
};

/*
 * Fragment is the abstract base class for the constituent fragments of string
 * models.
 */
class Fragment {
public:
    Fragment() { }
    virtual ~Fragment() { }
    virtual Fragment *dup() const = 0;
    virtual PRStatus interpolate(Interpolator& interpolator) const = 0;

private:
    Fragment(const Fragment&);
    Fragment& operator=(const Fragment&);
};

/*
 * Parameter describes a name-value pair from a pblock model.
 */
class Parameter {
public:
    /*
     * Construct a parameter from the given name and value.  The Parameter
     * creates a copy of the passed name but assumes ownership of the passed
     * string model value.
     */
    Parameter(const char *name, ModelString *value);

    /*
     * Destroy the parameter, its name string, and its value string model.
     */
    ~Parameter();

    /*
     * Create a copy of the parameter.
     */
    Parameter *dup() const;

    /*
     * Interpolate the parameter's value and add the resulting synthetic
     * parameter to the specified pblock.
     */
    inline PRStatus interpolate(Context& context, pblock *pb) const;

    char * const name;
    const pb_key * const key;
    ModelString * const value;

private:
    Parameter(const Parameter&);
    Parameter& operator=(const Parameter&);
};

/*
 * ModelPblock is a model from which a synthetic pblock may be constructed.
 */
class ModelPblock {
public:
    /*
     * Construct an empty pblock model.
     */
    ModelPblock();

    /*
     * Destroy a pblock model and its constituent parameters.
     */
    ~ModelPblock();

    /*
     * Create a copy of the pblock model.
     */
    ModelPblock *dup() const;

    /*
     * Add a parameter to the pblock model.  The pblock model assumes ownership
     * of the passed string model.
     */
    void addParameter(const char *name, ModelString *value);

    /*
     * Indicate whether the pblock model requires interpolation.
     */
    PRBool isInterpolative() const { return interpolative; }

    /*
     * Construct a synthetic pblock, allocated from the passed Context's pool,
     * based on the pblock model.
     */
    inline pblock *interpolate(Context& context) const;

private:
    ModelPblock(const ModelPblock&);
    ModelPblock& operator=(const ModelPblock&);

    PtrVector<Parameter> parameters;
    PRBool interpolative;
};


/* ------------------------------ backslash ------------------------------- */

static inline PRBool backslash(char c, NSString& result)
{
    switch (c) {
    case '\\':
        result.append("\\\\");
        return PR_TRUE;
    case '"':
        result.append("\\\"");
        return PR_TRUE;
    case '\n':
        result.append("\\n");
        return PR_TRUE;
    case '\r':
        result.append("\\r");
        return PR_TRUE;
    case '\t':
        result.append("\\t");
        return PR_TRUE;
    case '\f':
        result.append("\\f");
        return PR_TRUE;
    case '\b':
        result.append("\\b");
        return PR_TRUE;
    case '\a':
        result.append("\\a");
        return PR_TRUE;
    case '\x01b':
        result.append("\\e");
        return PR_TRUE;
    case '\0':
        result.append("\\0");
        return PR_TRUE;
    default:
        if (!isprint(c)) {
            result.printf("\\x%02x", c);
            return PR_TRUE;
        }
        return PR_FALSE;
    }
}


/* ----------------------------- unbackslash ------------------------------ */

static inline int unbackslash(const char *p, NSString& result)
{
    char *endptr;
    int n = 0;

    switch (*p) {
    case '\\':
        result.append('\\');
        n = 1;
        break;
    case '"':
        result.append('"');
        n = 1;
        break;
    case '\'':
        result.append('\'');
        n = 1;
        break;
    case 'n':
        result.append('\n');
        n = 1;
        break;
    case 'r':
        result.append('\r');
        n = 1;
        break;
    case 't':
        result.append('\t');
        n = 1;
        break;
    case 'f':
        result.append('\f');
        n = 1;
        break;
    case 'b':
        result.append('\b');
        n = 1;
        break;
    case 'a':
        result.append('\a');
        n = 1;
        break;
    case 'e': // esc
        result.append(0x1b);
        n = 1;
        break;
    case 'c': // "c@" ... "cZ" -> 0 ... 26
        if (p[1] >= 64 && p[1] <= 90) {
            result.append(p[1] - 64);
            n = 2;
        }
        break;
    case 'x':
        if (isxdigit(p[1])) {
            result.append((char) strtoul(p + 1, &endptr, 16));
            n = endptr - p;
        }
        break;
    case '0':
        result.append((char) strtoul(p, &endptr, 8));
        n = endptr - p;
        break;
    }

    return n;
}


/* ---------------------- Interpolator::Interpolator ---------------------- */

Interpolator::Interpolator(Context& contextArg)
: context(contextArg),
  p(NULL),
  pos(0),
  size(0)
{ }


/* ------------------------ Interpolator::require ------------------------- */

char *Interpolator::require(int len)
{
    // Ensure we have space for len bytes plus a trailing nul
    int required = pos + len + 1;
    if (size < required) {
        size = required;
        char *np = (char *) pool_realloc(context.pool, p, size);
        if (np == NULL) {
            size = 0;
            return NULL;
        }
        p = np;
    }
    return p + pos;
}

/* ------------------------ Interpolator::advance ------------------------- */

void Interpolator::advance(int len)
{
    pos += len;
    PR_ASSERT(pos < size);
}


/* ----------------------- Interpolator::terminate ------------------------ */

char *Interpolator::terminate()
{
    if (require(0) == NULL)
        return NULL;
    p[pos] = '\0';
    return p;
}


/* -------------------------- FragmentInvariant --------------------------- */

/*
 * FragmentInvariant is a Fragment whose value is a simple string.
 */
class FragmentInvariant : public Fragment {
public:
    FragmentInvariant(const NSString& s);
    Fragment *dup() const;
    PRStatus interpolate(Interpolator& interpolator) const;

private:
    NSString s;
};

FragmentInvariant::FragmentInvariant(const NSString& sArg)
: s(sArg.data(), sArg.length())
{ }

Fragment *FragmentInvariant::dup() const
{
    return new FragmentInvariant(s);
}

PRStatus FragmentInvariant::interpolate(Interpolator& interpolator) const
{
    int len = s.length();

    char *p = interpolator.require(len);
    if (p == NULL)
        return PR_FAILURE;

    memcpy(p, s.data(), len);
    interpolator.advance(len);

    return PR_SUCCESS;
}


/* -------------------------- FragmentExpression -------------------------- */

/*
 * FragmentExpression is a Fragment whose value is the result of evaluating an
 * expression.
 */
class FragmentExpression : public Fragment {
public:
    FragmentExpression(Expression *e);
    ~FragmentExpression();
    Fragment *dup() const;
    PRStatus interpolate(Interpolator& interpolator) const;

private:
    Expression *e;
};

FragmentExpression::FragmentExpression(Expression *eArg)
: e(eArg)
{ }

FragmentExpression::~FragmentExpression()
{
    expr_free(e);
}

Fragment *FragmentExpression::dup() const
{
    return new FragmentExpression(expr_dup(e));
}

PRStatus FragmentExpression::interpolate(Interpolator& interpolator) const
{
    Result result = e->evaluate(interpolator.context);
    if (result.isError()) {
        result.setNsprError();
        return PR_FAILURE;
    }
        
    const char *s = result.getConstString();
    int len = result.getStringLength();

    char *p = interpolator.require(len);
    if (p == NULL)
        return PR_FAILURE;

    memcpy(p, s, len);
    interpolator.advance(len);

    return PR_SUCCESS;
}


/* ----------------------- ModelString::ModelString ----------------------- */

ModelString::ModelString()
: invariant(PR_TRUE),
  interpolative(PR_FALSE),
  estimate(0)
{
    unescaped.setGrowthSize(NSString::SMALL_STRING);
    uninterpolated.setGrowthSize(NSString::SMALL_STRING);
}


/* ---------------------- ModelString::~ModelString ----------------------- */

ModelString::~ModelString()
{
    for (int i = 0; i < fragments.length(); i++)
        delete fragments[i];
}


/* --------------------------- ModelString::dup --------------------------- */

Expression *ModelString::dup() const
{
    return dupModelString();
}


/* --------------------- ModelString::dupModelString ---------------------- */

ModelString *ModelString::dupModelString() const
{
    ModelString *model = new ModelString();

    model->invariant = invariant;
    model->interpolative = interpolative;
    model->unescaped = unescaped;
    model->uninterpolated = uninterpolated;
    model->estimate = estimate;

    for (int i = 0; i < fragments.length(); i++)
        model->fragments.append(fragments[i]->dup());

    model->complete();

    return model;
}


/* ---------------------- ModelString::addNonescaped ---------------------- */

void ModelString::addNonescaped(const NSString& s)
{
    unescaped.append(s);
    uninterpolated.append(s);
    fragments.append(new FragmentInvariant(s));
}


/* -------------------- ModelString::addEscapedDollar --------------------- */

void ModelString::addEscapedDollar()
{
    interpolative = PR_TRUE;
    unescaped.append('$');
    uninterpolated.append("$$");
    fragments.append(new FragmentInvariant("$"));
}


/* ---------------------- ModelString::addExpression ---------------------- */

void ModelString::addExpression(const NSString& s, Expression *e)
{
    invariant = PR_FALSE;
    interpolative = PR_TRUE;
    unescaped.append(s);
    uninterpolated.append(s);
    fragments.append(new FragmentExpression(e));
}


/* ------------------------ ModelString::complete ------------------------- */

void ModelString::complete()
{
    if (invariant)
        setConstString(unescaped);
    estimate = unescaped.length();
}


/* ----------------------- ModelString::interpolate ----------------------- */

PRStatus ModelString::interpolate(Context& context, const char **pp, int *plen, pool_handle_t **ppool) const
{
    PR_ASSERT(invariant == (getConstString() != NULL));

    // If the string model is invariant...
    if (invariant) {
        // The interpolated version is just a simple unescaped string.  Yay!
        *pp = unescaped.data();
        *plen = unescaped.length();
        *ppool = NULL;
        return PR_SUCCESS;
    }

    // We need to actually interpolate things.  Start with a buffer we think
    // will be big enough.
    Interpolator interpolator(context);
    interpolator.require(estimate);

    // Interpolate the individual fragments
    NsprError error;
    int nerrors = 0;
    int nfragments = fragments.length();
    for (int i = 0; i < nfragments; i++) {
        if (fragments[i]->interpolate(interpolator) == PR_FAILURE) {
            // Record the error but keep on trucking
            if (nerrors == 0)
                error.save();
            nerrors++;
        }
    }

    // We're done.  Get a pointer to the complete interpolated result.
    char *p = interpolator.terminate();
    int len = interpolator.length();

    // Remember how big a buffer the interpolated result required so that we
    // can avoid future reallocs
    if (len > estimate)
        estimate = len;

    if (p == NULL) {
        // Out of memory
        *pp = NULL;
        *plen = 0;
        *ppool = NULL;
        return PR_FAILURE;        
    }

    if (nerrors == nfragments) {
        // The string was a total write-off, e.g. "$undefined"
        PR_ASSERT(nerrors > 0);
        *pp = NULL;
        *plen = 0;
        *ppool = NULL;
    } else {
        // Some of the string fragments were okay, e.g. "[$undefined]"
        *pp = p;
        *plen = len;
        *ppool = context.pool;
    }

    if (nerrors > 0) {
        error.restore();
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}


/* ------------------------ ModelString::evaluate ------------------------- */

Result ModelString::evaluate(Context& context) const
{
    const char *p;
    int len;
    pool_handle_t *pool;
    PRStatus rv = interpolate(context, &p, &len, &pool);

    if (rv == PR_FAILURE) {
        // Convert the NSPR error to an expression Result
        return context.createNsprErrorResult();
    } else if (pool != NULL) {
        // Convert the pooled string to an expression Result
        PR_ASSERT(pool == context.pool);
        return context.createPooledStringResult((char *) p, len);
    } else {
        // Convert the non-pooled string to an expression Result
        return context.createStringConstantResult(p, len);
    }
}


/* ------------------------- ModelString::format -------------------------- */

void ModelString::format(NSString& formatted, Precedence precedence) const
{
    formatted.append('"');
    for (int i = 0; i < uninterpolated.length(); i++) {
        char c = uninterpolated.data()[i];
        if (!backslash(c, formatted))
            formatted.append(c);
    }
    formatted.append('"');
}


/* ---------------------------- model_unescape ---------------------------- */

static char *model_unescape(const char *s, PRBool interpolative)
{
    NSString unescaped;

    while (*s != '\0') {
        if (*s == '\\') {
            s++;
            if (*s == '$' && interpolative) {
                unescaped.append("$$");
                s++;
                continue;
            }
            int n = unbackslash(s, unescaped);
            if (n > 0) {
                s += n;
                continue;
            }
            if (isprint(*s)) {
                NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_badEscapeCharX), *s);
            } else {
                NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_badEscapeIntX), *s);
            }
            return NULL;
        }
        unescaped.append(*s);
        s++;
    }

    return STRDUP(unescaped);
}


/* --------------------- model_unescape_interpolative --------------------- */

char *model_unescape_interpolative(const char *s)
{
    return model_unescape(s, PR_TRUE);
}


/* ------------------- model_unescape_noninterpolative -------------------- */

char *model_unescape_noninterpolative(const char *s)
{
    return model_unescape(s, PR_FALSE);
}


/* --------------------------- model_scan_expr ---------------------------- */

static const char *model_scan_expr(const char *s, ScannedFragment *scanned)
{
    PR_ASSERT(*s == '(');

    TokenizerStringCharSource source(s, strlen(s));

    Expression *expr = expr_scan_inclusive(source, ")");
    if (expr == NULL)
        return NULL;

    expr_free(expr);

    PR_ASSERT(s[source.getOffset() - 1] == ')');

    scanned->type = FRAGMENT_TYPE_EXPRESSION;
    scanned->expression.p = s;
    scanned->expression.len = source.getOffset();

    return s + source.getOffset();
}


/* ------------------------- model_scan_subscript ------------------------- */

static const char *model_scan_subscript(const char *s, ScannedFragment *scanned)
{
    TokenizerStringCharSource source(s, strlen(s));

    Expression *subscript = expr_scan_exclusive(source, "}");
    if (subscript == NULL)
        return NULL;

    expr_free(subscript);

    PR_ASSERT(s[source.getOffset() - 1] == '}');

    PR_ASSERT(scanned->type == FRAGMENT_TYPE_MAP);
    scanned->subscript.p = s;
    scanned->subscript.len = source.getOffset() - 1;

    return s + source.getOffset();
}


/* ------------------------ model_scan_var_or_map ------------------------- */

static const char *model_scan_var_or_map(const char *s, const char *closing, ScannedFragment *scanned)
{
    // Skip whitespace following "${" in bracketed ${fragment}s
    if (closing) {
        while (isspace(*s))
            s++;
    }

    // Extract the identifier from "$identifier", "${identifier}",
    // "$identifier{expr}", or "${identifier{expr}}"
    const char *p = s;
    if (*p == '&') {
        p++;
    } else if (isdigit(*p)) {
        do p++; while (isdigit(*p));
    } else if (expr_leading_identifier_char(*p)) {
        do p++; while (expr_nonleading_identifier_char(*p));
    } else {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_syntaxErrorNearX), "$");
        return NULL;
    }

    scanned->identifier.p = s;
    scanned->identifier.len = p - s;

    // Skip whitespace following identifier in bracketed ${fragment}s
    if (closing) {
        while (isspace(*p))
            p++;        
    }

    if (*p == '{' && expr_is_identifier(scanned->identifier.p, scanned->identifier.len)) {
        // "$map{expr}" or "${map{expr}}"
        scanned->type = FRAGMENT_TYPE_MAP;
        p = model_scan_subscript(p + 1, scanned);
        if (p == NULL)
            return NULL;
    } else {
        // "$var" or "${var}"
        scanned->type = FRAGMENT_TYPE_VARIABLE;
    }

    // Look for closing '}' in bracketed ${fragment}s
    if (closing) {
        while (isspace(*p))
            p++;
        if (*p != *closing) {
            if (isprint(*p)) {
                NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_unexpectedCharX), *p);
            } else {
                NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_missingClosingCharX), *closing);
            }
            return NULL;
        }
        p++;
    }

    return p;
}


/* ------------------------------ model_scan ------------------------------ */

static const char *model_scan(const char *s, ScannedFragment *scanned)
{
    PR_ASSERT(*s != '\0');

    if (*s != '$') {
        // Scan the invariant fragment
        const char *p = s;
        do p++; while (*p != '\0' && *p != '$');

        // Found simple text (no $fragments or "$$" sequences)
        scanned->type = FRAGMENT_TYPE_NONESCAPED;
        scanned->text.p = s;
        scanned->text.len = p - s;

        return p;
    }

    if (s[1] == '\0') {
        // Found trailing '$'
        scanned->type = FRAGMENT_TYPE_NONESCAPED;
        scanned->text.p = s;
        scanned->text.len = 1;

        return s + 1;
    }

    if (s[1] == '$') {
        // Found "$$" escape sequence
        scanned->type = FRAGMENT_TYPE_ESCAPED_DOLLAR;
        scanned->text.p = s;
        scanned->text.len = 1;

        return s + 2;
    }

    // Scan $fragment
    switch (s[1]) {
    case '(': return model_scan_expr(s + 1, scanned);
    case '{': return model_scan_var_or_map(s + 2, "}", scanned);
    default: return model_scan_var_or_map(s + 1, NULL, scanned);
    }
}


/* ------------------------- model_fragment_scan -------------------------- */

int model_fragment_scan(const char *f)
{
    const char *p = f;
    if (*p != '\0') {
        ScannedFragment scanned;
        p = model_scan(p, &scanned);
        if (p == NULL)
            return -1;
    }

    return p - f;
}


/* --------------------- model_fragment_is_invariant ---------------------- */

PRBool model_fragment_is_invariant(const char *f, const char **ptext, int *plen)
{
    const char *p = f;
    if (*p != '\0') {
        ScannedFragment scanned;
        p = model_scan(p, &scanned);
        if (p == NULL)
            return PR_FALSE;
        if (scanned.type != FRAGMENT_TYPE_NONESCAPED && scanned.type != FRAGMENT_TYPE_ESCAPED_DOLLAR)
            return PR_FALSE;
    }

    if (ptext)
        *ptext = f;
    if (plen)
        *plen = p - f;

    return PR_TRUE;
}


/* ---------------------- model_fragment_is_var_ref ----------------------- */

PRBool model_fragment_is_var_ref(const char *f, const char **pname, int *plen)
{
    if (*f != '$')
        return PR_FALSE;

    ScannedFragment scanned;
    const char *p = model_scan(f, &scanned);
    if (p == NULL)
        return PR_FALSE;
    if (scanned.type != FRAGMENT_TYPE_VARIABLE)
        return PR_FALSE;

    if (pname)
        *pname = scanned.identifier.p;
    if (plen)
        *plen = scanned.identifier.len;

    return PR_TRUE;
}


/* -------------------------- model_str_fragment -------------------------- */

static PRStatus model_str_fragment(ModelString *model, const char *p, int len, ScannedFragment *scanned)
{
    if (scanned->type == FRAGMENT_TYPE_NONESCAPED) {
        model->addNonescaped(NSString(scanned->text.p, scanned->text.len));

        return PR_SUCCESS;
    }

    if (scanned->type == FRAGMENT_TYPE_ESCAPED_DOLLAR) {
        model->addEscapedDollar();

        return PR_SUCCESS;
    }

    if (scanned->type == FRAGMENT_TYPE_VARIABLE) {
        NSString n(scanned->identifier.p, scanned->identifier.len);
        Expression *expr = expr_new_variable(n);
        if (expr == NULL)
            return PR_FAILURE;

        model->addExpression(NSString(p, len), expr);

        return PR_SUCCESS;
    }

    if (scanned->type == FRAGMENT_TYPE_MAP) {
        NSString s(scanned->subscript.p, scanned->subscript.len);
        Expression *subscript = expr_create(s);
        if (subscript == NULL)
            return PR_FAILURE;

        NSString n(scanned->identifier.p, scanned->identifier.len);
        Expression *expr = expr_new_access(n, subscript);
        if (expr == NULL) {
            expr_free(subscript);
            return PR_FAILURE;
        }

        model->addExpression(NSString(p, len), expr);

        return PR_SUCCESS;
    }

    if (scanned->type == FRAGMENT_TYPE_EXPRESSION) {
        NSString e(scanned->expression.p, scanned->expression.len);
        Expression *expr = expr_create(e);
        if (expr == NULL)
            return PR_FAILURE;

        model->addExpression(NSString(p, len), expr);

        return PR_SUCCESS;
    }

    PR_ASSERT(0);

    return PR_FAILURE;
}


/* --------------------------- model_str_create --------------------------- */

ModelString *model_str_create(const char *s)
{
    ModelString *model = new ModelString();

    while (*s != '\0') {
        ScannedFragment scanned;

        const char *f = s;
        s = model_scan(f, &scanned);
        if (s == NULL) {
            delete model;
            return NULL;
        }

        PR_ASSERT(s > f);

        PRStatus rv = model_str_fragment(model, f, s - f, &scanned);
        if (rv != PR_SUCCESS) {
            delete model;
            return NULL;          
        }
    }

    model->complete();

    return model;
}


/* ---------------------------- model_str_dup ----------------------------- */

ModelString *model_str_dup(const ModelString *model)
{
    return model->dupModelString();
}


/* ---------------------------- model_str_free ---------------------------- */

void model_str_free(ModelString *model)
{
    delete model;
}


/* ------------------------ model_str_interpolate ------------------------- */

int model_str_interpolate(const ModelString *model, Session *sn, Request *rq, const char **pp, int *plen)
{
    pool_handle_t *pool = request_pool(rq);
    Context context(sn, rq, pool);
    return model->interpolate(context, pp, plen, &pool);
}


/* ------------------------- Parameter::Parameter ------------------------- */

Parameter::Parameter(const char *nameArg, ModelString *valueArg)
: name(PERM_STRDUP(nameArg)),
  key(pblock_key(name)),
  value(valueArg)
{ }


/* ------------------------ Parameter::~Parameter ------------------------- */

Parameter::~Parameter()
{
    PERM_FREE(name);
    delete value;
}


/* ---------------------------- Parameter::dup ---------------------------- */

Parameter *Parameter::dup() const
{
    return new Parameter(name, value->dupModelString());
}


/* ------------------------ Parameter::interpolate ------------------------ */

inline PRStatus Parameter::interpolate(Context& context, pblock *pb) const
{
    pb_param *pp;
    if (key) {
        pp = pblock_key_param_create(pb, key, NULL, 0);
    } else {
        pp = pblock_param_create(pb, name, NULL);
    }
    if (!pp)
        return PR_FAILURE;

    Result result = value->evaluate(context);
    if (result.isError()) {
        result.setNsprError();
        return PR_FAILURE;
    }

    pp->value = result.getPooledString(context.pool);
    if (!pp->value)
        return PR_FAILURE;

    if (key) {
        pblock_kpinsert(key, pp, pb);
    } else {
        pblock_pinsert(pp, pb);
    }

    return PR_SUCCESS;
}


/* ----------------------- ModelPblock::ModelPblock ----------------------- */

ModelPblock::ModelPblock()
: interpolative(PR_FALSE)
{ }


/* ---------------------- ModelPblock::~ModelPblock ----------------------- */

ModelPblock::~ModelPblock()
{
    for (int i = 0; i < parameters.length(); i++)
        delete parameters[i];
}


/* --------------------------- ModelPblock::dup --------------------------- */

ModelPblock *ModelPblock::dup() const
{
    ModelPblock *model = new ModelPblock();

    model->interpolative = interpolative;

    for (int i = 0; i < parameters.length(); i++)
        model->parameters.append(parameters[i]->dup());

    return model;
}

inline pblock *ModelPblock::interpolate(Context& context) const
{
    pblock *result = pblock_create_pool(context.pool, parameters.length());
    if (result != NULL) {
        for (int i = 0; i < parameters.length(); i++) {
            if (parameters[i]->interpolate(context, result) == PR_FAILURE)
                return NULL;
        }
    }
    return result;
}


/* ---------------------- ModelPblock::addParameter ----------------------- */

void ModelPblock::addParameter(const char *name, ModelString *value)
{
    if (value->isInterpolative())
        interpolative = PR_TRUE;

    parameters.append(new Parameter(name, value));
}


/* --------------------------- model_pb_create ---------------------------- */

ModelPblock *model_pb_create(const pblock *pb)
{
    if (pb == NULL)
        return NULL;

    ModelPblock *model = new ModelPblock();

    for (int hi = 0; hi < pb->hsize; hi++) {
        for (pb_entry *p = pb->ht[hi]; p != NULL; p = p->next) {
            if (param_key(p->param) != pb_key_magnus_internal) {
                ModelString *value = model_str_create(p->param->value);
                if (value == NULL) {
                    NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_paramNameXErrorY), p->param->name, system_errmsg());
                    delete model;
                    return NULL;
                }

                model->addParameter(p->param->name, value);
            }
        }
    }

    return model;
}


/* ----------------------------- model_pb_dup ----------------------------- */

ModelPblock *model_pb_dup(const ModelPblock *model)
{
    if (model == NULL)
        return NULL;

    return model->dup();
}


/* ---------------------------- model_pb_free ----------------------------- */

void model_pb_free(ModelPblock *model)
{
    delete model;
}


/* --------------------- model_pb_is_noninterpolative --------------------- */

PRBool model_pb_is_noninterpolative(const ModelPblock *model)
{
    if (model == NULL)
        return PR_TRUE;

    return !model->isInterpolative();
}


/* ------------------------- model_pb_interpolate ------------------------- */

pblock *model_pb_interpolate(const ModelPblock *model, Session *sn, Request *rq)
{
    Context context(sn, rq, request_pool(rq));
    return model->interpolate(context);
}
