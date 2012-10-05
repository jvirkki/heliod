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
 * expr.cpp: NSAPI expression parsing and evaluation
 * 
 * Chris Elving
 */

#include "plhash.h"
#include "pcre.h"

#include "netsite.h"
#include "NsprWrap/NsprError.h"
#include "base/vs.h"
#include "base/util.h"
#include "base/pool.h"
#include "base/pblock.h"
#include "base/shexp.h"
#include "base/regexp.h"
#include "frame/log.h"
#include "frame/req.h"
#include "frame/model.h"
#include "frame/expr.h"
#include "frame/dbtframe.h"
#include "args_pvt.h"
#include "model_pvt.h"
#include "expr_parse.h"
#include "expr_yy.h"
#include "expr_pvt.h"


/*
 * OperatorStruct specifies the name, expr_yy_lex token, and precedence of an
 * operator.
 */
struct OperatorStruct {
    OperatorStruct(const char *string, int yychar, Precedence precedence);

    const char *string;
    int yychar;
    Precedence precedence;
};

/*
 * op_char_* defines a hash table that contains all OperatorStruct pointers
 * hashed by the leading character of the operator.
 */
static const int op_char_hmask = 0x1f;
static const int op_char_hsize = op_char_hmask + 1;
static PtrVector<const OperatorStruct> op_char_ht[op_char_hsize];

/*
 * The following OperatorStruct definitions, arranged by precedence, define the
 * names, expr_yy_lex tokens, and precedences of all operators.  These
 * definitions must be consistent with the expression grammar defined by
 * expr.y.
 */
static OperatorStruct op_dollar("$", EXPR_TOKEN_DOLLAR, EXPR_PRECEDENCE_DOLLAR);
static OperatorStruct op_left_brace("{", EXPR_TOKEN_LEFTBRACE, EXPR_PRECEDENCE_DOLLAR);
static OperatorStruct op_right_brace("}", EXPR_TOKEN_RIGHTBRACE, EXPR_PRECEDENCE_DOLLAR);
static OperatorStruct op_left_paren("(", EXPR_TOKEN_LEFTPAREN, EXPR_PRECEDENCE_DOLLAR);
static OperatorStruct op_right_paren(")", EXPR_TOKEN_RIGHTPAREN, EXPR_PRECEDENCE_DOLLAR);
static OperatorStruct op_dollar_amp("$&", EXPR_TOKEN_DOLLAR_AMP, EXPR_PRECEDENCE_DOLLAR);
static OperatorStruct op_amp("&", EXPR_TOKEN_AMP, EXPR_PRECEDENCE_DOLLAR);
static OperatorStruct op_c_not("!", EXPR_TOKEN_BANG, EXPR_PRECEDENCE_SIGN);
static OperatorStruct op_wildcard("=", EXPR_TOKEN_EQUALS, EXPR_PRECEDENCE_MATCHING);
static OperatorStruct op_re("=~", EXPR_TOKEN_EQUALS_TILDE, EXPR_PRECEDENCE_MATCHING);
static OperatorStruct op_nre("!~", EXPR_TOKEN_BANG_TILDE, EXPR_PRECEDENCE_MATCHING);
static OperatorStruct op_multiply("*", EXPR_TOKEN_STAR, EXPR_PRECEDENCE_MULTIPLICATIVE);
static OperatorStruct op_divide("/", EXPR_TOKEN_SLASH, EXPR_PRECEDENCE_MULTIPLICATIVE);
static OperatorStruct op_modulo("%", EXPR_TOKEN_PERCENT, EXPR_PRECEDENCE_MULTIPLICATIVE);
static OperatorStruct op_add("+", EXPR_TOKEN_PLUS, EXPR_PRECEDENCE_ADDITIVE);
static OperatorStruct op_subtract("-", EXPR_TOKEN_MINUS, EXPR_PRECEDENCE_ADDITIVE);
static OperatorStruct op_concat(".", EXPR_TOKEN_DOT, EXPR_PRECEDENCE_ADDITIVE);
static OperatorStruct op_dash_d("-d", EXPR_TOKEN_NAMED_OP, EXPR_PRECEDENCE_NAMED_OP);
static OperatorStruct op_dash_e("-e", EXPR_TOKEN_NAMED_OP, EXPR_PRECEDENCE_NAMED_OP);
static OperatorStruct op_dash_f("-f", EXPR_TOKEN_NAMED_OP, EXPR_PRECEDENCE_NAMED_OP);
static OperatorStruct op_dash_l("-l", EXPR_TOKEN_NAMED_OP, EXPR_PRECEDENCE_NAMED_OP);
static OperatorStruct op_dash_r("-r", EXPR_TOKEN_NAMED_OP, EXPR_PRECEDENCE_NAMED_OP);
static OperatorStruct op_dash_s("-s", EXPR_TOKEN_NAMED_OP, EXPR_PRECEDENCE_NAMED_OP);
static OperatorStruct op_dash_U("-U", EXPR_TOKEN_NAMED_OP, EXPR_PRECEDENCE_NAMED_OP);
static OperatorStruct op_defined("defined", EXPR_TOKEN_NAMED_OP, EXPR_PRECEDENCE_NAMED_OP);
static OperatorStruct op_numeric_lt("<", EXPR_TOKEN_LEFTANGLE, EXPR_PRECEDENCE_RELATIONAL);
static OperatorStruct op_numeric_le("<=", EXPR_TOKEN_LEFTANGLE_EQUALS, EXPR_PRECEDENCE_RELATIONAL);
static OperatorStruct op_numeric_gt(">", EXPR_TOKEN_RIGHTANGLE, EXPR_PRECEDENCE_RELATIONAL);
static OperatorStruct op_numeric_ge(">=", EXPR_TOKEN_RIGHTANGLE_EQUALS, EXPR_PRECEDENCE_RELATIONAL);
static OperatorStruct op_string_lt("lt", EXPR_TOKEN_L_T, EXPR_PRECEDENCE_RELATIONAL);
static OperatorStruct op_string_le("le", EXPR_TOKEN_L_E, EXPR_PRECEDENCE_RELATIONAL);
static OperatorStruct op_string_gt("gt", EXPR_TOKEN_G_T, EXPR_PRECEDENCE_RELATIONAL);
static OperatorStruct op_string_ge("ge", EXPR_TOKEN_G_E, EXPR_PRECEDENCE_RELATIONAL);
static OperatorStruct op_numeric_eq("==", EXPR_TOKEN_EQUALS_EQUALS, EXPR_PRECEDENCE_EQUALITY);
static OperatorStruct op_numeric_ne("!=", EXPR_TOKEN_BANG_EQUALS, EXPR_PRECEDENCE_EQUALITY);
static OperatorStruct op_string_eq("eq", EXPR_TOKEN_E_Q, EXPR_PRECEDENCE_EQUALITY);
static OperatorStruct op_string_ne("ne", EXPR_TOKEN_N_E, EXPR_PRECEDENCE_EQUALITY);
static OperatorStruct op_c_xor("^", EXPR_TOKEN_CARAT, EXPR_PRECEDENCE_C_XOR);
static OperatorStruct op_c_and("&&", EXPR_TOKEN_AMP_AMP, EXPR_PRECEDENCE_C_AND);
static OperatorStruct op_c_or("||", EXPR_TOKEN_PIPE_PIPE, EXPR_PRECEDENCE_C_OR);
static OperatorStruct op_question("?", EXPR_TOKEN_QUESTION, EXPR_PRECEDENCE_TERNARY);
static OperatorStruct op_colon(":", EXPR_TOKEN_COLON, EXPR_PRECEDENCE_TERNARY);
static OperatorStruct op_named_not("not", EXPR_TOKEN_N_O_T, EXPR_PRECEDENCE_NAMED_NOT);
static OperatorStruct op_named_and("and", EXPR_TOKEN_A_N_D, EXPR_PRECEDENCE_NAMED_AND);
static OperatorStruct op_named_or("or", EXPR_TOKEN_O_R, EXPR_PRECEDENCE_NAMED_OR);
static OperatorStruct op_named_xor("xor", EXPR_TOKEN_X_O_R, EXPR_PRECEDENCE_NAMED_OR);
static OperatorStruct op_comma(",", EXPR_TOKEN_COMMA, EXPR_PRECEDENCE_NONE);

/*
 * var_get_name_ht hashes predefined variable names to "getter" ExpressionFunc
 * pointers.
 */
static PLHashTable *var_get_name_ht = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL);

/*
 * map_get_name_ht hashes predefined map variable names to "getter"
 * ExpressionFunc pointers.
 */
static PLHashTable *map_get_name_ht = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL);

/*
 * control_name_ht hashes control function names to ExpressionFunc pointers.
 */
static PLHashTable *control_name_ht = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL);

/*
 * Token content we watch for when tracking the number of open/close brackets.
 */
static const TokenContent EXPR_LEFTPAREN(TOKEN_OPERATOR, "(");
static const TokenContent EXPR_RIGHTPAREN(TOKEN_OPERATOR, ")");
static const TokenContent EXPR_LEFTBRACE(TOKEN_OPERATOR, "{");
static const TokenContent EXPR_RIGHTBRACE(TOKEN_OPERATOR, "}");

/*
 * LexerData is used to pass tokens from expr_parse to expr_yy_lex.
 */
struct LexerData {
    const Token * const *tokens;
    int ntokens;
    int i;
};

/*
 * ParserData is used to pass information from expr_yy_expr, expr_yy_args, and
 * expr_yy_error back to expr_parse.
 */
struct ParserData {
    PtrVector<Expression> expressions;
    PtrVector<Arguments> args;
    NSString error;
};

/*
 * expr_request_backref_slot is a request_get_data/request_set_data slot that
 * holds a RequestBackrefData *.
 */
static int expr_request_backref_slot = request_alloc_slot(NULL);


/* ----------------------------- op_char_hash ----------------------------- */

static inline unsigned op_char_hash(char c)
{
    // Hash operator to an op_char_ht[] index based on leading character
    return ((unsigned char)c) & op_char_hmask;
}


/* -------------------- OperatorStruct::OperatorStruct -------------------- */

OperatorStruct::OperatorStruct(const char *string, int yychar, Precedence precedence)
: string(string), yychar(yychar), precedence(precedence)
{
    int h = op_char_hash(*this->string);
    op_char_ht[h].append(this);
}


/* ------------------------------- op_find -------------------------------- */

const OperatorStruct *op_find(const char *s, int len)
{
    if (len > 0) {
        // Is there a hash entry for s?
        int h = op_char_hash(s[0]);
        for (int i = 0; i < op_char_ht[h].length(); i++) {
            const OperatorStruct *o = op_char_ht[h][i];
            int j;
            for (j = 0; (j < len) && (s[j] == o->string[j]); j++);
            if (j == len && o->string[j] == '\0')
                return o;
        }
    }

    return NULL;
}


/* ------------------------ expr_could_be_operator ------------------------ */

PRBool expr_could_be_operator(const char *s, int len)
{
    PR_ASSERT(len > 0);
    if (len < 1)
        return PR_TRUE;

    // Is there a hash entry that begins with s?
    int h = op_char_hash(s[0]);
    for (int i = 0; i < op_char_ht[h].length(); i++) {
        const OperatorStruct *o = op_char_ht[h][i];
        int j;
        for (j = 0; (j < len) && (s[j] == o->string[j]); j++);
        if (j == len)
            return PR_TRUE;
    }

    return PR_FALSE;
}


/* --------------------------- expr_is_operator --------------------------- */

PRBool expr_is_operator(const char *s, int len)
{
    return (op_find(s, len) != NULL);
}


/* -------------------------- expr_is_identifier -------------------------- */

PRBool expr_is_identifier(const char *s, int len)
{
    if (len < 1)
        return PR_FALSE;

    if (!expr_leading_identifier_char(s[0]))
        return PR_FALSE;

    for (int i = 1; i < len; i++) {
        if (!expr_nonleading_identifier_char(s[i]))
            return PR_FALSE;
    }

    return PR_TRUE;
}


/* --------------------- expr_leading_identifier_char --------------------- */

PRBool expr_leading_identifier_char(char c)
{
    // N.B. identifiers CANNOT begin with '$'
    return (isalpha(c) || c == '_');
}


/* ------------------- expr_nonleading_identifier_char -------------------- */

PRBool expr_nonleading_identifier_char(char c)
{
    return (isalnum(c) || c == '_');
}


/* -------------------------- expr_*_func_insert -------------------------- */

void expr_var_get_func_insert(const char *name, ExpressionFunc *func)
{
    PR_ASSERT(expr_is_identifier(name, strlen(name)));
    PL_HashTableAdd(var_get_name_ht, name, (void *) func);
}

void expr_map_get_func_insert(const char *name, ExpressionFunc *func)
{
    PR_ASSERT(expr_is_identifier(name, strlen(name)));
    PL_HashTableAdd(map_get_name_ht, name, (void *) func);
}

void expr_control_func_insert(const char *name, ExpressionFunc *func)
{
    PR_ASSERT(expr_is_identifier(name, strlen(name)) ||
              expr_is_operator(name, strlen(name)));
    PL_HashTableAdd(control_name_ht, name, (void *) func);
}


/* --------------------------- expr_*_func_find --------------------------- */

ExpressionFunc *expr_var_get_func_find(const char *name)
{
    return (ExpressionFunc *) PL_HashTableLookupConst(var_get_name_ht, name);
}

ExpressionFunc *expr_map_get_func_find(const char *name)
{
    return (ExpressionFunc *) PL_HashTableLookupConst(map_get_name_ht, name);
}

ExpressionFunc *expr_control_func_find(const char *name)
{
    return (ExpressionFunc *) PL_HashTableLookupConst(control_name_ht, name);
}


/* ---------------------------- backref_clear ----------------------------- */

static inline void backref_clear(Request *rq)
{
    RequestBackrefData *rb = (RequestBackrefData *) request_get_data(rq, expr_request_backref_slot);
    if (rb != NULL)
        rb->nbackrefs = 0;
}


/* ----------------------------- backref_data ----------------------------- */

static inline RequestBackrefData *backref_data(Session *sn, Request *rq)
{
    RequestBackrefData *rb = (RequestBackrefData *) request_get_data(rq, expr_request_backref_slot);

    if (rb == NULL) {
        // Allocate, initialize, and store a new RequestBackrefData
        rb = (RequestBackrefData *) pool_malloc(sn->pool, sizeof(RequestBackrefData));
        if (rb != NULL) {
            rb->nbackrefs = 0;
            request_set_data(rq, expr_request_backref_slot, rb);
        }
    }

    return rb;
}


/* ----------------------------- backref_add ------------------------------ */

static inline void backref_add(Session *sn, Request *rq, int n, const char *p, int len)
{
    RequestBackrefData *rb = backref_data(sn, rq);
    if (rb != NULL) {
        // n = 0 indicates we should set backrefs[0].  n != 0 indicates a
        // capturing subpattern match that should be appended to backrefs[].
        if (n != 0)
            n = rb->nbackrefs;

        if (n < EXPR_MAX_BACKREFS) {
            rb->backrefs[n].p = (char *) pool_malloc(sn->pool, len + 1);
            if (rb->backrefs[n].p != NULL) {
                memcpy(rb->backrefs[n].p, p, len);
                rb->backrefs[n].p[len] = '\0';
                rb->backrefs[n].len = len;
                if (n == rb->nbackrefs)
                    rb->nbackrefs++;
            }
        }
    }
}


/* ----------------------------- backref_get ------------------------------ */

static inline PRStatus backref_get(Request *rq, int n, const char **p, int *len)
{
    RequestBackrefData *rb = (RequestBackrefData *) request_get_data(rq, expr_request_backref_slot);
    if (rb == NULL || n >= rb->nbackrefs)
        return PR_FAILURE;

    *p = rb->backrefs[n].p;
    *len = rb->backrefs[n].len;

    return PR_SUCCESS;
}


/* ----------------------------- backref_copy ----------------------------- */

static inline void backref_copy(RequestBackrefData *dst, const RequestBackrefData *src)
{
    int nbackrefs = 0;

    if (src != NULL) {
        nbackrefs = src->nbackrefs;
        for (int bi = 0; bi < nbackrefs; bi++) {
            dst->backrefs[bi].p = src->backrefs[bi].p;
            dst->backrefs[bi].len = src->backrefs[bi].len;
        }
    }

    dst->nbackrefs = nbackrefs;
}


/* -------------------------- expr_get_backrefs --------------------------- */

void expr_get_backrefs(RequestBackrefData *rb, Session *sn, Request *rq)
{
    RequestBackrefData *src = (RequestBackrefData *) request_get_data(rq, expr_request_backref_slot);

    backref_copy(rb, src);
}


/* -------------------------- expr_set_backrefs --------------------------- */

void expr_set_backrefs(const RequestBackrefData *rb, Session *sn, Request *rq)
{
    RequestBackrefData *dst;

    if (rb != NULL && rb->nbackrefs > 0) {
        // Caller is trying to set backrefs, so we need a non-NULL dst
        dst = backref_data(sn, rq);
    } else {
        // Caller is trying to clear backrefs, so it's okay for dst to be NULL
        dst = (RequestBackrefData *) request_get_data(rq, expr_request_backref_slot);
        if (dst == NULL)
            return;
    }

    backref_copy(dst, rb);
}


/* ---------------------- Expression::setConstString ---------------------- */

void Expression::setConstString(const char *s)
{
    string = s;
    key = pblock_key(s);
}


/* --------------------------- ExpressionUnary ---------------------------- */

/*
 * ExpressionUnary is the abstract base class for expressions with a single
 * operand.
 */
class ExpressionUnary : public Expression {
public:
    ExpressionUnary(Expression *e);
    void deleteChildren();
    int getExpressionNodeCount() const;

protected:
    void formatUnaryOperator(NSString& formatted, Precedence parent, Precedence us, const char *op) const;

    Expression *operand;
};

ExpressionUnary::ExpressionUnary(Expression *e)
: operand(e)
{ }

void ExpressionUnary::deleteChildren()
{
    operand->deleteChildren();
    delete operand;
    operand = NULL;
}

int ExpressionUnary::getExpressionNodeCount() const
{
    return 1 + operand->getExpressionNodeCount();
}

void ExpressionUnary::formatUnaryOperator(NSString& formatted, Precedence parent, Precedence us, const char *op) const
{
    if (parent > us)
        formatted.append('(');

    formatted.append(op);
    if (isalpha(*op))
        formatted.append(' ');

    operand->format(formatted, us);

    if (parent > us)
        formatted.append(')');
}


/* --------------------------- ExpressionBinary --------------------------- */

/*
 * ExpressionBinary is the abstract base class for expressions with two
 * operands.
 */
class ExpressionBinary : public Expression {
public:
    ExpressionBinary(Expression *e0, Expression *e1);
    void deleteChildren();
    int getExpressionNodeCount() const;

protected:
    void formatBinaryOperator(NSString& formatted, Precedence parent, Precedence us, const char *op) const;

    Expression *operands[2];
};

ExpressionBinary::ExpressionBinary(Expression *e0, Expression *e1)
{
    operands[0] = e0;
    operands[1] = e1;
}

void ExpressionBinary::deleteChildren()
{
    for (int i = 0; i < sizeof(operands) / sizeof(operands[0]); i++) {
        operands[i]->deleteChildren();
        delete operands[i];
        operands[i] = NULL;
    }
}

int ExpressionBinary::getExpressionNodeCount() const
{
    return 1 +
           operands[0]->getExpressionNodeCount() +
           operands[1]->getExpressionNodeCount();
}

void ExpressionBinary::formatBinaryOperator(NSString& formatted, Precedence parent, Precedence us, const char *op) const
{
    if (parent > us)
        formatted.append('(');

    operands[0]->format(formatted, us);

    formatted.append(' ');
    formatted.append(op);
    formatted.append(' ');

    operands[1]->format(formatted, us);

    if (parent > us)
        formatted.append(')');
}


/* -------------------------- ExpressionTernary --------------------------- */

/*
 * ExpressionTernary conditionally evalutes one of two expressions.
 */
class ExpressionTernary : public Expression {
public:
    ExpressionTernary(Expression *e0, Expression *e1, Expression *e2);
    void deleteChildren();
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
    int getExpressionNodeCount() const;

protected:
    Expression *operands[3];
};

ExpressionTernary::ExpressionTernary(Expression *e0, Expression *e1, Expression *e2)
{
    operands[0] = e0;
    operands[1] = e1;
    operands[2] = e2;
}

void ExpressionTernary::deleteChildren()
{
    for (int i = 0; i < sizeof(operands) / sizeof(operands[0]); i++) {
        operands[i]->deleteChildren();
        delete operands[i];
        operands[i] = NULL;
    }
}

Expression *ExpressionTernary::dup() const
{
    return new ExpressionTernary(operands[0]->dup(), operands[1]->dup(), operands[2]->dup());
}

Result ExpressionTernary::evaluate(Context& context) const
{
    Result x = operands[0]->evaluate(context);

    int i = 2 - x.getBoolean();

    return operands[i]->evaluate(context);
}

void ExpressionTernary::format(NSString& formatted, Precedence parent) const
{
    if (parent > EXPR_PRECEDENCE_TERNARY)
        formatted.append('(');

    operands[0]->format(formatted, EXPR_PRECEDENCE_TERNARY);
    formatted.append(" ? ");
    operands[1]->format(formatted, EXPR_PRECEDENCE_TERNARY);
    formatted.append(" : ");
    operands[2]->format(formatted, EXPR_PRECEDENCE_TERNARY);

    if (parent > EXPR_PRECEDENCE_TERNARY)
        formatted.append(')');
}

int ExpressionTernary::getExpressionNodeCount() const
{
    return 1 +
           operands[0]->getExpressionNodeCount() +
           operands[1]->getExpressionNodeCount() +
           operands[2]->getExpressionNodeCount();
}


/* -------------------------- ExpressionBoolean --------------------------- */

/*
 * ExpressionBoolean always evaluates to a particular boolean value.
 */
class ExpressionBoolean : public ExpressionNullary {
public:
    ExpressionBoolean(PRBool v);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    PRBool value;
};

ExpressionBoolean::ExpressionBoolean(PRBool v)
: value(v)
{ }

Expression *ExpressionBoolean::dup() const
{
    return new ExpressionBoolean(value);
}

Result ExpressionBoolean::evaluate(Context& context) const
{
    return context.createBooleanResult(value);
}

void ExpressionBoolean::format(NSString& formatted, Precedence parent) const
{
    formatted.append(value ? "true" : "false");
}


/* -------------------------- ExpressionInteger --------------------------- */

/*
 * ExpressionInteger always evaluates to a particular integral value.
 */
class ExpressionInteger : public ExpressionNullary {
public:
    ExpressionInteger(PRInt64 v);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    PRInt64 value;
};

ExpressionInteger::ExpressionInteger(PRInt64 v)
: value(v)
{ }

Expression *ExpressionInteger::dup() const
{
    return new ExpressionInteger(value);
}

Result ExpressionInteger::evaluate(Context& context) const
{
    return context.createIntegerResult(value);
}

void ExpressionInteger::format(NSString& formatted, Precedence parent) const
{
    formatted.printf("%lld", value);
}


/* --------------------------- ExpressionString --------------------------- */

/*
 * ExpressionString always evaluates to a particular noninterpolative string.
 */
class ExpressionString : public ExpressionNullary {
public:
    ExpressionString(const char *s);
    ~ExpressionString();
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    char *p;
    int len;
};

ExpressionString::ExpressionString(const char *s)
{
    p = PERM_STRDUP(s);
    len = strlen(p);
    setConstString(p);
}

ExpressionString::~ExpressionString()
{
    PERM_FREE(p);
}

Expression *ExpressionString::dup() const
{
    return new ExpressionString(p);
}

Result ExpressionString::evaluate(Context& context) const
{
    return context.createStringConstantResult(p, len);
}

void ExpressionString::format(NSString& formatted, Precedence parent) const
{
    formatted.append('\'');
    for (int i = 0; i < len; i++) {
        if (p[i] == '\'')
            formatted.append('\\');
        formatted.append(p[i]);
    }
    formatted.append('\'');
}


/* ----------------------------- ExpressionOr ----------------------------- */

/*
 * ExpressionOr computes the shortcircuit logical OR of its operands.
 */
class ExpressionOr : public ExpressionBinary {
public:
    ExpressionOr(const OperatorStruct& op, Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    const OperatorStruct& op;
};

ExpressionOr::ExpressionOr(const OperatorStruct& op, Expression *l, Expression *r)
: ExpressionBinary(l, r), op(op)
{ }

Expression *ExpressionOr::dup() const
{
    return new ExpressionOr(op, operands[0]->dup(), operands[1]->dup());
}

Result ExpressionOr::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    if (l.getBoolean())
        return context.createBooleanResult(PR_TRUE);

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    if (r.getBoolean())
        return context.createBooleanResult(PR_TRUE);

    return context.createBooleanResult(PR_FALSE);
}

void ExpressionOr::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, op.precedence, op.string);
}


/* ---------------------------- ExpressionXor ----------------------------- */

/*
 * ExpressionXor computes the logical XOR of its operands.
 */
class ExpressionXor : public ExpressionBinary {
public:
    ExpressionXor(const OperatorStruct& op, Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    const OperatorStruct& op;
};

ExpressionXor::ExpressionXor(const OperatorStruct& op, Expression *l, Expression *r)
: ExpressionBinary(l, r), op(op)
{ }

Expression *ExpressionXor::dup() const
{
    return new ExpressionXor(op, operands[0]->dup(), operands[1]->dup());
}

Result ExpressionXor::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    return context.createBooleanResult(l.getBoolean() ^ r.getBoolean());
}

void ExpressionXor::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, op.precedence, op.string);
}


/* ---------------------------- ExpressionAnd ----------------------------- */

/*
 * ExpressionAnd computes the shortcircuit logical AND of its operands.
 */
class ExpressionAnd : public ExpressionBinary {
public:
    ExpressionAnd(const OperatorStruct& op, Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    const OperatorStruct& op;
};

ExpressionAnd::ExpressionAnd(const OperatorStruct& op, Expression *l, Expression *r)
: ExpressionBinary(l, r), op(op)
{ }

Expression *ExpressionAnd::dup() const
{
    return new ExpressionAnd(op, operands[0]->dup(), operands[1]->dup());
}

Result ExpressionAnd::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    if (!l.getBoolean())
        return context.createBooleanResult(PR_FALSE);

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    if (!r.getBoolean())
        return context.createBooleanResult(PR_FALSE);

    return context.createBooleanResult(PR_TRUE);
}

void ExpressionAnd::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, op.precedence, op.string);
}


/* ---------------------------- ExpressionNot ----------------------------- */

/*
 * ExpressionNot computes the logical NOT of its operand.
 */
class ExpressionNot : public ExpressionUnary {
public:
    ExpressionNot(const OperatorStruct& op, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    const OperatorStruct& op;
};

ExpressionNot::ExpressionNot(const OperatorStruct& op, Expression *r)
: ExpressionUnary(r), op(op)
{ }

Expression *ExpressionNot::dup() const
{
    return new ExpressionNot(op, operand->dup());
}

Result ExpressionNot::evaluate(Context& context) const
{
    Result r = operand->evaluate(context);
    if (r.isError())
        return r;

    return context.createBooleanResult(!r.getBoolean());
}

void ExpressionNot::format(NSString& formatted, Precedence parent) const
{
    formatUnaryOperator(formatted, parent, op.precedence, op.string);
}


/* ------------------------- ExpressionNumericOp -------------------------- */

/*
 * ExpressionNumericOp is the abstract base class for expressions that operate
 * on two integers.
 */
class ExpressionNumericOp : public ExpressionBinary {
public:
    ExpressionNumericOp(Expression *l, Expression *r);

protected:
    void warnOfNonNumericOperands(Context& context, const char *fn, Result& l, Result& r) const;
};

ExpressionNumericOp::ExpressionNumericOp(Expression *l, Expression *r)
: ExpressionBinary(l, r)
{ }

void ExpressionNumericOp::warnOfNonNumericOperands(Context& context, const char *fn, Result& l, Result& r) const
{
    if (ereport_can_log(LOG_VERBOSE)) {
        const char *operand;

        operand = l.getConstString();
        if (strcspn(operand, NUMERIC_CHARSET)) {
            log_error(LOG_VERBOSE, fn, context.sn, context.rq,
                      "Numeric operator operand \"%s\" contains non-numeric characters",
                      operand);
        }

        operand = r.getConstString();
        if (strcspn(operand, NUMERIC_CHARSET)) {
            log_error(LOG_VERBOSE, fn, context.sn, context.rq,
                      "Numeric operator operand \"%s\" contains non-numeric characters",
                      operand);
        }
    }
}


/* ------------------------- ExpressionNumericCmp ------------------------- */

/*
 * ExpressionNumericCmp performs an integer comparison of its operands.
 */
class ExpressionNumericCmp : public ExpressionNumericOp {
public:
    ExpressionNumericCmp(const OperatorStruct& op, Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    const OperatorStruct& op;
    int yychar;
};

ExpressionNumericCmp::ExpressionNumericCmp(const OperatorStruct& op, Expression *l, Expression *r)
: ExpressionNumericOp(l, r), op(op), yychar(op.yychar)
{ }

Expression *ExpressionNumericCmp::dup() const
{
    return new ExpressionNumericCmp(op, operands[0]->dup(), operands[1]->dup());
}

Result ExpressionNumericCmp::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    if (l.isString() || r.isString())
        warnOfNonNumericOperands(context, op.string, l, r);

    PRInt64 cmp = l.getInteger() - r.getInteger();

    PRBool result;
    switch (yychar) {
    case EXPR_TOKEN_EQUALS_EQUALS: result = (cmp == 0); break;
    case EXPR_TOKEN_BANG_EQUALS: result = (cmp != 0); break;
    case EXPR_TOKEN_LEFTANGLE: result = (cmp < 0); break;
    case EXPR_TOKEN_LEFTANGLE_EQUALS: result = (cmp <= 0); break;
    case EXPR_TOKEN_RIGHTANGLE: result = (cmp > 0); break;
    case EXPR_TOKEN_RIGHTANGLE_EQUALS: result = (cmp >= 0); break;
    default: PR_ASSERT(0);
    }

    return context.createBooleanResult(result);
}

void ExpressionNumericCmp::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, op.precedence, op.string);
}


/* ------------------------- ExpressionStringCmp -------------------------- */

/*
 * ExpressionStringCmp performs a string-wise comparison of its operands.
 */
class ExpressionStringCmp : public ExpressionBinary {
public:
    ExpressionStringCmp(const OperatorStruct& op, Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    const OperatorStruct& op;
    int yychar;
};

ExpressionStringCmp::ExpressionStringCmp(const OperatorStruct& op, Expression *l, Expression *r)
: ExpressionBinary(l, r), op(op), yychar(op.yychar)
{ }

Expression *ExpressionStringCmp::dup() const
{
    return new ExpressionStringCmp(op, operands[0]->dup(), operands[1]->dup());
}

Result ExpressionStringCmp::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    int cmp = strcmp(l.getConstString(), r.getConstString());

    PRBool result;
    switch (yychar) {
    case EXPR_TOKEN_E_Q: result = (cmp == 0); break;
    case EXPR_TOKEN_N_E: result = (cmp != 0); break;
    case EXPR_TOKEN_L_T: result = (cmp < 0); break;
    case EXPR_TOKEN_L_E: result = (cmp <= 0); break;
    case EXPR_TOKEN_G_T: result = (cmp > 0); break;
    case EXPR_TOKEN_G_E: result = (cmp >= 0); break;
    default: PR_ASSERT(0);
    }

    return context.createBooleanResult(result);
}

void ExpressionStringCmp::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, op.precedence, op.string);
}


/* ---------------------------- ExpressionAdd ----------------------------- */

/*
 * ExpressionAdd returns the sum of two integers.
 */
class ExpressionAdd : public ExpressionNumericOp {
public:
    ExpressionAdd(Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
};

ExpressionAdd::ExpressionAdd(Expression *l, Expression *r)
: ExpressionNumericOp(l, r)
{ }

Expression *ExpressionAdd::dup() const
{
    return new ExpressionAdd(operands[0]->dup(), operands[1]->dup());
}

Result ExpressionAdd::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    if (l.isString() || r.isString())
        warnOfNonNumericOperands(context, "+", l, r);

    return context.createIntegerResult(l.getInteger() + r.getInteger());
}

void ExpressionAdd::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_ADDITIVE, "+");
}


/* -------------------------- ExpressionSubtract --------------------------- */

/*
 * ExpressionSubtract returns the difference between two integers.
 */
class ExpressionSubtract : public ExpressionNumericOp {
public:
    ExpressionSubtract(Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
};

ExpressionSubtract::ExpressionSubtract(Expression *l, Expression *r)
: ExpressionNumericOp(l, r)
{ }

Expression *ExpressionSubtract::dup() const
{
    return new ExpressionSubtract(operands[0]->dup(), operands[1]->dup());
}

Result ExpressionSubtract::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    if (l.isString() || r.isString())
        warnOfNonNumericOperands(context, "-", l, r);

    return context.createIntegerResult(l.getInteger() - r.getInteger());
}

void ExpressionSubtract::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_ADDITIVE, "-");
}


/* --------------------------- ExpressionConcat --------------------------- */

/*
 * ExpressionConcat returns the concatenation of two strings.
 */
class ExpressionConcat : public ExpressionBinary {
public:
    ExpressionConcat(Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
};

ExpressionConcat::ExpressionConcat(Expression *l, Expression *r)
: ExpressionBinary(l, r)
{ }

Expression *ExpressionConcat::dup() const
{
    return new ExpressionConcat(operands[0]->dup(), operands[1]->dup());
}

Result ExpressionConcat::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    int len = l.getStringLength() + r.getStringLength();

    char *p = (char *) pool_malloc(context.pool, len + 1);
    if (p == NULL)
        return context.createOutOfMemoryErrorResult();

    memcpy(p, l.getConstString(), l.getStringLength());
    memcpy(p + l.getStringLength(), r.getConstString(), r.getStringLength());
    p[len] = '\0';

    return context.createPooledStringResult(p, len);
}

void ExpressionConcat::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_ADDITIVE, ".");
}


/* -------------------------- ExpressionMultiply -------------------------- */

/*
 * ExpressionMultiply returns the product of two integers.
 */
class ExpressionMultiply : public ExpressionNumericOp {
public:
    ExpressionMultiply(Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
};

ExpressionMultiply::ExpressionMultiply(Expression *l, Expression *r)
: ExpressionNumericOp(l, r)
{ }

Expression *ExpressionMultiply::dup() const
{
    return new ExpressionMultiply(operands[0]->dup(), operands[1]->dup());
}

Result ExpressionMultiply::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    if (l.isString() || r.isString())
        warnOfNonNumericOperands(context, "*", l, r);

    return context.createIntegerResult(l.getInteger() * r.getInteger());
}

void ExpressionMultiply::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_MULTIPLICATIVE, "*");
}


/* --------------------------- ExpressionDivide --------------------------- */

/*
 * ExpressionDivide returns the dividend of two integers.
 */
class ExpressionDivide : public ExpressionNumericOp {
public:
    ExpressionDivide(Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
};

ExpressionDivide::ExpressionDivide(Expression *l, Expression *r)
: ExpressionNumericOp(l, r)
{ }

Expression *ExpressionDivide::dup() const
{
    return new ExpressionDivide(operands[0]->dup(), operands[1]->dup());
}

Result ExpressionDivide::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    if (l.isString() || r.isString())
        warnOfNonNumericOperands(context, "/", l, r);

    return context.createIntegerResult(l.getInteger() / r.getInteger());
}

void ExpressionDivide::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_MULTIPLICATIVE, "/");
}


/* --------------------------- ExpressionModulo --------------------------- */

/*
 * ExpressionModulo returns the modulo of two integers.
 */
class ExpressionModulo : public ExpressionNumericOp {
public:
    ExpressionModulo(Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
};

ExpressionModulo::ExpressionModulo(Expression *l, Expression *r)
: ExpressionNumericOp(l, r)
{ }

Expression *ExpressionModulo::dup() const
{
    return new ExpressionModulo(operands[0]->dup(), operands[1]->dup());
}

Result ExpressionModulo::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    if (l.isString() || r.isString())
        warnOfNonNumericOperands(context, "%", l, r);

    return context.createIntegerResult(l.getInteger() % r.getInteger());
}

void ExpressionModulo::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_MULTIPLICATIVE, "%");
}


/* -------------------------- ExpressionWildcard -------------------------- */

/*
 * ExpressionWildcard returns a boolean that indicates whether the given
 * subject l matches the wildcard pattern r.
 */
class ExpressionWildcard : public ExpressionBinary {
public:
    ExpressionWildcard(Expression *l, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
};

ExpressionWildcard::ExpressionWildcard(Expression *l, Expression *r)
: ExpressionBinary(l, r)
{ }

Expression *ExpressionWildcard::dup() const
{
    return new ExpressionWildcard(operands[0]->dup(), operands[1]->dup());
}

Result ExpressionWildcard::evaluate(Context& context) const
{
    Result l = operands[0]->evaluate(context);
    if (l.isError())
        return l;

    Result r = operands[1]->evaluate(context);
    if (r.isError())
        return r;

    // <Client> tag compatibility: if the left operand is a boolean and the
    // right operand is a string that contains a boolean, do a simple boolean
    // equality test.  For example, <Client security="on">.
    if (l.isBoolean() && r.isBoolean()) {
        return context.createBooleanResult(l.getBoolean() == r.getBoolean());
    } else if (l.isBoolean() && r.isString()) {
        int rb = util_getboolean(r.getConstString(), -1);
        if (rb != -1)
            return context.createBooleanResult(l.getBoolean() == rb);
    } else if (r.isBoolean() && l.isString()) {
        int lb = util_getboolean(l.getConstString(), -1);
        if (lb != -1)
            return context.createBooleanResult(r.getBoolean() == lb);
    }

    int cmp = shexp_match(l.getConstString(), r.getConstString());

    return context.createBooleanResult(cmp == 0);
}

void ExpressionWildcard::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_MATCHING, "=");
}


/* ----------------------------- ExpressionRe ----------------------------- */

/*
 * ExpressionRe returns a boolean that indicates whether the given subject l
 * matches the regular expression r.
 */
class ExpressionRe : public ExpressionBinary {
public:
    ExpressionRe(Expression *l, Expression *r, PRBool m);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    PRBool match;
};

ExpressionRe::ExpressionRe(Expression *l, Expression *r, PRBool m)
: ExpressionBinary(l, r), match(m)
{ }

Expression *ExpressionRe::dup() const
{
    return new ExpressionRe(operands[0]->dup(), operands[1]->dup(), match);
}

Result ExpressionRe::evaluate(Context& context) const
{
    Result subject = operands[0]->evaluate(context);
    if (subject.isError())
        return subject;

    Result pattern = operands[1]->evaluate(context);
    if (pattern.isError())
        return pattern;

    const char *p = pattern.getConstString();
    const char *error = NULL;
    int erroroffset;

    pcre *re = pcre_compile(p, 0, &error, &erroroffset, NULL);
    if (re == NULL) {
        if (error != NULL) {
            return context.createErrorResultf(XP_GetAdminStr(DBT_badRegexBecauseX), error); // XXX PCRE l10n
        } else {
            return context.createErrorResult(XP_GetAdminStr(DBT_badRegex));
        }
    }

    const char *s = subject.getConstString();
    int len = subject.getStringLength();
    int ovector[EXPR_MAX_BACKREFS * 3];

    int rv = pcre_exec(re, NULL, s, len, 0, 0, ovector, EXPR_MAX_BACKREFS * 3);

    pcre_free(re);

    if (rv == PCRE_ERROR_NOMATCH) {
        log_error(LOG_FINEST, match ? "=~" : "!~", context.sn, context.rq,
                  "Subject \"%s\" does not match pattern \"%s\"",
                  s, p);
        return context.createBooleanResult(!match);
    }

    if (rv < 0)
        return context.createErrorResult(XP_GetAdminStr(DBT_badRegex));

    PR_ASSERT(rv <= EXPR_MAX_BACKREFS);
    for (int i = 0; i < rv; i++) {
        backref_add(context.sn,
                    context.rq,
                    i,
                    s + ovector[i * 2],
                    ovector[i * 2 + 1] - ovector[i * 2]);
    }

    log_error(LOG_FINEST, match ? "=~" : "!~", context.sn, context.rq,
              "Subject \"%s\" matches pattern \"%s\" with %d capturing subpattern(s)",
              s, p, rv - 1);

    return context.createBooleanResult(match);
}

void ExpressionRe::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_MATCHING, match ? "=~" : "!~");
}


/* ------------------------- ExpressionCompiledRe ------------------------- */

/*
 * ExpressionCompiledRe returns a boolean that indicates whether the given
 * subject l matches the compiled regular expression defined by r and re.
 */
class ExpressionCompiledRe : public ExpressionBinary {
public:
    ExpressionCompiledRe(Expression *l, Expression *r, pcre *re, PRBool m);
    ~ExpressionCompiledRe();
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    pcre *re;
    pcre_extra *pe;
    PRBool match;
};

ExpressionCompiledRe::ExpressionCompiledRe(Expression *l, Expression *r, pcre *p, PRBool m)
: ExpressionBinary(l, r), re(p), match(m)
{
    PR_ASSERT(r->getConstString() != NULL);
    const char *error;
    pe = pcre_study(re, 0, &error);
}

ExpressionCompiledRe::~ExpressionCompiledRe()
{
    pcre_free(pe);
    pcre_free(re);
}

Expression *ExpressionCompiledRe::dup() const
{
    const char *pattern = operands[1]->getConstString();
    const char *error;
    int erroroffset;

    pcre *dre = pcre_compile(pattern, 0, &error, &erroroffset, NULL);

    return new ExpressionCompiledRe(operands[0]->dup(), operands[1]->dup(), dre, match);
}

Result ExpressionCompiledRe::evaluate(Context& context) const
{
    Result subject = operands[0]->evaluate(context);
    if (subject.isError())
        return subject;

    const char *s = subject.getConstString();
    int len = subject.getStringLength();
    int ovector[EXPR_MAX_BACKREFS * 3];

    int rv = pcre_exec(re, pe, s, len, 0, 0, ovector, EXPR_MAX_BACKREFS * 3);

    if (rv == PCRE_ERROR_NOMATCH) {
        log_error(LOG_FINEST, match ? "=~" : "!~", context.sn, context.rq,
                  "Subject \"%s\" does not match pattern \"%s\"",
                  s, operands[1]->getConstString());
        return context.createBooleanResult(!match);
    }

    if (rv < 0)
        return context.createErrorResult(XP_GetAdminStr(DBT_badRegex));

    PR_ASSERT(rv <= EXPR_MAX_BACKREFS);
    for (int i = 0; i < rv; i++) {
        backref_add(context.sn,
                    context.rq,
                    i,
                    s + ovector[i * 2],
                    ovector[i * 2 + 1] - ovector[i * 2]);
    }

    log_error(LOG_FINEST, match ? "=~" : "!~", context.sn, context.rq,
              "Subject \"%s\" matches pattern \"%s\" with %d capturing subpattern(s)",
              s, operands[1]->getConstString(), rv - 1);

    return context.createBooleanResult(match);
}

void ExpressionCompiledRe::format(NSString& formatted, Precedence parent) const
{
    formatBinaryOperator(formatted, parent, EXPR_PRECEDENCE_MATCHING, match ? "=~" : "!~");
}


/* -------------------------- ExpressionBackref --------------------------- */

/*
 * ExpressionBackref returns the value of a regular expression backreference.
 */
class ExpressionBackref : public ExpressionNullary {
public:
    ExpressionBackref(int n);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    int n;
};

ExpressionBackref::ExpressionBackref(int n)
: n(n)
{ }

Expression *ExpressionBackref::dup() const
{
    return new ExpressionBackref(n);
}

Result ExpressionBackref::evaluate(Context& context) const
{
    const char *p;
    int len;
    if (backref_get(context.rq, n, &p, &len) == PR_SUCCESS)
        return context.createStringConstantResult(p, len);

    if (n > 0)
        return context.createErrorResultf(XP_GetAdminStr(DBT_noBackrefNumX), n);

    return context.createErrorResultf(XP_GetAdminStr(DBT_noBackref));
}

void ExpressionBackref::format(NSString& formatted, Precedence parent) const
{
    if (n > 0) {
        formatted.printf("$%d", n);
    } else {
        formatted.append("$&");
    }
}


/* --------------------------- ExpressionSigned --------------------------- */

/*
 * ExpressionSigned multiplies its operand by +1 or -1.
 */
class ExpressionSigned : public ExpressionUnary {
public:
    ExpressionSigned(int sign, Expression *r);
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    int sign;
};

ExpressionSigned::ExpressionSigned(int sign, Expression *r)
: ExpressionUnary(r), sign(sign)
{ }

Expression *ExpressionSigned::dup() const
{
    return new ExpressionSigned(sign, operand->dup());
}

Result ExpressionSigned::evaluate(Context& context) const
{
    Result r = operand->evaluate(context);
    if (r.isError())
        return r;

    return context.createIntegerResult(sign * r.getInteger());
}

void ExpressionSigned::format(NSString& formatted, Precedence parent) const
{
    formatUnaryOperator(formatted, parent, EXPR_PRECEDENCE_SIGN, (sign > 0) ? "+" : "-");
}


/* -------------------------- ExpressionCallFunc -------------------------- */

/*
 * ExpressionCallFunc returns the result of a call to a control function.
 */
class ExpressionCallFunc : public Expression {
public:
    ExpressionCallFunc(const char *n, ExpressionFunc *f, Arguments *a);
    ~ExpressionCallFunc();
    void deleteChildren();
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;
    int getExpressionNodeCount() const;

private:
    char *name;
    ExpressionFunc *func;
    Arguments *args;
};

ExpressionCallFunc::ExpressionCallFunc(const char *n, ExpressionFunc *f, Arguments *a)
: func(f), args(a)
{
    name = PERM_STRDUP(n);
}

ExpressionCallFunc::~ExpressionCallFunc()
{
    PERM_FREE(name);
}

void ExpressionCallFunc::deleteChildren()
{
    args->deleteChildren();
    delete args;
    args = NULL;
}

Expression *ExpressionCallFunc::dup() const
{
    return new ExpressionCallFunc(name, func, args->dup());
}

Result ExpressionCallFunc::evaluate(Context& context) const
{
    const Result *result = (*func)(args, context.sn, context.rq);
    if (result == NULL) {
        log_error(LOG_CATASTROPHE,
                  "ExpressionCallFunc::evaluate",
                  context.sn, context.rq,
                  "ExpressionFunc for %s (%p) returned NULL",
                  name, func);
        abort();
    }
    return *result;
}

void ExpressionCallFunc::format(NSString& formatted, Precedence parent) const
{
    formatted.append(name);
    formatted.append('(');
    for (int i = 0; i < args->length(); i++) {
        Expression *e = args->get(i);
        if (i != 0)
            formatted.append(", ");
        e->format(formatted, EXPR_PRECEDENCE_NONE);
    }
    formatted.append(')');
}

int ExpressionCallFunc::getExpressionNodeCount() const
{
    int count = 1;
    for (int i = 0; i < args->length(); i++)
        count += args->get(i)->getExpressionNodeCount();
    return count;
}


/* ------------------------ ExpressionNamedOpFunc ------------------------- */

/*
 * ExpressionNamedOpFunc returns the result of a call to a control function
 * that implements a file operator.
 */
class ExpressionNamedOpFunc : public ExpressionUnary {
public:
    ExpressionNamedOpFunc(const char *n, ExpressionFunc *f, Expression *r);
    ~ExpressionNamedOpFunc();
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    char *name;
    ExpressionFunc *func;
    Arguments *args;
};

ExpressionNamedOpFunc::ExpressionNamedOpFunc(const char *n, ExpressionFunc *f, Expression *r)
: ExpressionUnary(r), func(f)
{
    name = PERM_STRDUP(n);

    args = new Arguments();
    args->add(r);
}

ExpressionNamedOpFunc::~ExpressionNamedOpFunc()
{
    PERM_FREE(name);

    // N.B. the contained expression may only be destroyed by deleteChildren()
    delete args;
}

Expression *ExpressionNamedOpFunc::dup() const
{
    return new ExpressionNamedOpFunc(name, func, operand->dup());
}

Result ExpressionNamedOpFunc::evaluate(Context& context) const
{
    const Result *result = (*func)(args, context.sn, context.rq);
    if (result == NULL) {
        log_error(LOG_CATASTROPHE,
                  "ExpressionNamedOpFunc::evaluate",
                  context.sn, context.rq,
                  "ExpressionFunc for %s (%p) returned NULL",
                  name, func);
        abort();
    }
    return *result;
}

void ExpressionNamedOpFunc::format(NSString& formatted, Precedence parent) const
{
    formatUnaryOperator(formatted, parent, EXPR_PRECEDENCE_NAMED_OP, name);
}


/* -------------------------- ExpressionMapFunc --------------------------- */

/*
 * ExpressionMapFunc returns the result of a call to a map variable getter
 * function
 */
class ExpressionMapFunc : public ExpressionUnary {
public:
    ExpressionMapFunc(const char *i, ExpressionFunc *f, Expression *s);
    ~ExpressionMapFunc();
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    char *identifier;
    ExpressionFunc *func;
    Arguments *args;
};

ExpressionMapFunc::ExpressionMapFunc(const char *i, ExpressionFunc *f, Expression *s)
: ExpressionUnary(s), func(f)
{
    identifier = PERM_STRDUP(i);

    args = new Arguments();
    args->add(s);
}

ExpressionMapFunc::~ExpressionMapFunc()
{
    PERM_FREE(identifier);

    // N.B. the contained expression may only be destroyed by deleteChildren()
    delete args;
}

Expression *ExpressionMapFunc::dup() const
{
    return new ExpressionMapFunc(identifier, func, operand->dup());
}

Result ExpressionMapFunc::evaluate(Context& context) const
{
    const Result *result = (*func)(args, context.sn, context.rq);
    if (result == NULL) {
        log_error(LOG_CATASTROPHE,
                  "ExpressionMapFunc::evaluate",
                  context.sn, context.rq,
                  "ExpressionFunc for %s (%p) returned NULL",
                  identifier, func);
        abort();
    }
    return *result;
}

void ExpressionMapFunc::format(NSString& formatted, Precedence parent) const
{
    formatted.append('$');
    formatted.append(identifier);
    formatted.append('{');
    operand->format(formatted, EXPR_PRECEDENCE_NONE);
    formatted.append('}');
}


/* --------------------------- ExpressionVarGet --------------------------- */

/*
 * ExpressionVarGet looks up the value of a variable.
 */
class ExpressionVarGet : public ExpressionNullary {
public:
    ExpressionVarGet(const char *v);
    ~ExpressionVarGet();
    Expression *dup() const;
    Result evaluate(Context& context) const;
    void format(NSString& formatted, Precedence parent) const;

private:
    char *name;
    const pb_key *key;
    ExpressionFunc *func;
    Arguments *args;
};

ExpressionVarGet::ExpressionVarGet(const char *v)
{
    PR_ASSERT(*v == '$');
    name = PERM_STRDUP(v);
    key = pblock_key(v);
    func = expr_var_get_func_find(v + 1);
}

ExpressionVarGet::~ExpressionVarGet()
{
    PERM_FREE(name);
}

Expression *ExpressionVarGet::dup() const
{
    return new ExpressionVarGet(name);
}

Result ExpressionVarGet::evaluate(Context& context) const
{
    // 1. Is the variable a predefined variable?
    if (func) {
        const Result *result = (*func)(NULL, context.sn, context.rq);
        if (result == NULL) {
            log_error(LOG_CATASTROPHE,
                      "ExpressionVarGet::evaluate",
                      context.sn, context.rq,
                      "ExpressionFunc for %s (%p) returned NULL",
                      name, func);
            abort();
        }
        return *result;
    }

    // 2. Was the variable defined at request time?
    if (key) {
        if (char *value = pblock_findkeyval(key, context.rq->vars))
            return context.createPooledStringResult(value);
    } else {
        if (char *value = pblock_findval(name, context.rq->vars))
            return context.createPooledStringResult(value);
    }

    // 3. Was the variable defined in server.xml?
    const VirtualServer *vs = request_get_vs(context.rq);
    if (vs) {
        if (char *value = vs_resolve_config_var(vs, name + 1))
            return context.createPooledStringResult(value);
    }

    // 4. Undefined variable
    return context.createErrorResultf(XP_GetAdminStr(DBT_noVarX), name);
}

void ExpressionVarGet::format(NSString& formatted, Precedence parent) const
{
    formatted.append(name);
}


/* ----------------------------- expr_format ------------------------------ */

char *expr_format(const Expression *expr)
{
    NSString formatted;

    if (expr != NULL)
        expr->format(formatted, EXPR_PRECEDENCE_NONE);

    return STRDUP(formatted);
}


/* ------------------------------- expr_dup ------------------------------- */

Expression *expr_dup(const Expression *expr)
{
    if (expr == NULL)
        return NULL;

    return expr->dup();
}


/* ------------------------------ expr_free ------------------------------- */

void expr_free(Expression *expr)
{
    if (expr) {
        // Delete the root node's children (and grandchildren, etc.)
        expr->deleteChildren();

        // Delete the root node itself
        delete expr;
    }
}


/* ---------------------------- expr_evaluate ----------------------------- */

int expr_evaluate(const Expression *expr, Session *sn, Request *rq)
{
    backref_clear(rq);

    if (expr == NULL)
        return REQ_PROCEED;

    Context context(sn, rq, request_pool(rq));

    Result result = expr->evaluate(context);
    if (result.isError()) {
        result.setNsprError();
        return REQ_ABORTED;
    }

    return result.getBoolean() ? REQ_PROCEED : REQ_NOACTION;
}


/* ---------------------------- expr_yy_expand ---------------------------- */

int expr_yy_expand(YYDATA *yydata, int depth)
{
    PR_ASSERT(depth == yydata->yymaxdepth);

    depth *= 2;
    if (depth < 16)
        depth = 16;

    // Allocate (or grow) value and state stacks for expr_yy_parse
    yydata->yyv = (YYSTYPE *) REALLOC(yydata->yyv, depth * sizeof(YYSTYPE));
    yydata->yys = (int *) REALLOC(yydata->yys, depth * sizeof(int));
    if (yydata->yyv == NULL || yydata->yys == NULL) {
        FREE(yydata->yyv);
        yydata->yyv = NULL;
        FREE(yydata->yys);
        yydata->yys = NULL;
        return 0;
    }

    return depth;
}


/* ----------------------------- yydata_init ------------------------------ */

static void yydata_init(YYDATA *yydata, LexerData *lexer, ParserData *parser)
{
    memset(yydata, 0, sizeof(*yydata));
    yydata->lexer = lexer;
    yydata->parser = parser;
    yydata->yymaxdepth = expr_yy_expand(yydata, yydata->yymaxdepth);
}


/* ---------------------------- yydata_destroy ---------------------------- */

static void yydata_destroy(YYDATA *yydata)
{
    FREE(yydata->yyv);
    FREE(yydata->yys);
}


/* -------------------------- bracket_imbalance --------------------------- */

static PRBool bracket_imbalance(int parens, int braces)
{
    if (parens > 0) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_missingClosingCharX), ')');
        return PR_TRUE;
    } else if (parens < 0) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_unmatchedCharX), ')');
        return PR_TRUE;
    } else if (braces > 0) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_missingClosingCharX), '}');
        return PR_TRUE;
    } else if (braces < 0) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_unmatchedCharX), '}');
        return PR_TRUE;
    }
    return PR_FALSE;
}


/* ------------------------------ expr_scan ------------------------------- */

static Expression *expr_scan(TokenizerCharSource& source, const char *include, const char *exclude)
{
    ExpressionTokenizer tokenizer(source);
    PtrVector<const Token> tokens(16);

    // The expression ends at the first appearance of the inclusive end string
    // (e.g. ")") or exclusive end string (e.g. ">" or "}") that's not quoted
    // or bracketed
    const char *end = NULL;
    if (include)
        end = include;
    if (exclude)
        end = exclude;
    PR_ASSERT(include == NULL || exclude == NULL);

    // Tokenize the expression (e.g. the "foo" in <If "foo">)
    int firstLine = source.getLine();
    int parens = 0;
    int braces = 0;
    try {
        for (;;) {
            const Token& token = tokenizer.getToken();

            if (token.type == TOKEN_EOF) {
                if (end) {
                    NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_expectedX), end);
                    return NULL;
                }
                if (bracket_imbalance(parens, braces))
                    return NULL;
                break;
            }

            if (exclude && !strcmp(token.value, exclude)) {
                if (parens < 1 && braces < 1) {
                    if (bracket_imbalance(parens, braces))
                        return NULL;
                    break;
                }
            }

            if (token == EXPR_LEFTPAREN) {
                parens++;
            } else if (token == EXPR_RIGHTPAREN) {
                parens--;
            } else if (token == EXPR_LEFTBRACE) {
                braces++;
            } else if (token == EXPR_RIGHTBRACE) {
                braces--;
            }

            tokens.append(&token);

            if (include && !strcmp(token.value, include)) {
                if (parens < 1 && braces < 1) {
                    if (bracket_imbalance(parens, braces))
                        return NULL;
                    break;
                }
            }
        }
    }
    catch (const TokenizerIOErrorException& e) {
        e.error.restore();
        return NULL;
    }
    catch (const TokenizerUnclosedException& e) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_missingClosingCharX), e.closing);
        return NULL;
    }
    catch (const TokenizerCharException& e) {
        if (source.getLine() != firstLine) {
            if (bracket_imbalance(parens, braces))
                return NULL;
            if (end) {
                NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_expectedX), end);
                return NULL;
            }
        }
        if (isprint(e.c)) {
            NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_unexpectedCharX), e.c);
        } else {
            NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_unexpectedIntX), e.c);
        }
        return NULL;
    }

    if (tokens.length() == 0) {
        NsprError::setError(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_unexpectedEndOfExpression));
        return NULL;
    }

    return expr_parse(&tokens[0], tokens.length());
}


/* ------------------------- expr_scan_inclusive -------------------------- */

Expression *expr_scan_inclusive(TokenizerCharSource& source, const char *end)
{
    return expr_scan(source, end, NULL);
}


/* ------------------------- expr_scan_exclusive -------------------------- */

Expression *expr_scan_exclusive(TokenizerCharSource& source, const char *end)
{
    return expr_scan(source, NULL, end);
}


/* ----------------------------- expr_create ------------------------------ */

Expression *expr_create(const char *s)
{
    TokenizerStringCharSource source(s, strlen(s));

    return expr_scan(source, NULL, NULL);
}


/* ------------------------------ expr_parse ------------------------------ */

Expression *expr_parse(const Token * const *tokens, int ntokens)
{
    // LexerData communicates tokens to expr_yy_lex
    LexerData lexer;
    lexer.tokens = tokens;
    lexer.ntokens = ntokens;
    lexer.i = 0;

    // ParserData collects status from expr_yy_expr, expr_yy_args, and
    // expr_yy_error
    ParserData parser;

    // YYDATA provides context for expr_yy_parse
    YYDATA yydata;
    yydata_init(&yydata, &lexer, &parser);
    
    // Parse the tokens into an expression tree.  expr_yy_parse will call into
    // expr_yy_lex to retrieve each token in turn.
    Expression *expr;
    if (expr_yy_parse(&yydata) || parser.error.length() > 0) {
        // Error during parse
        expr = NULL;
        if (parser.error.length() > 0) {
            NsprError::setError(PR_INVALID_ARGUMENT_ERROR, parser.error.data());
        } else {
            PR_ASSERT(0);
            NsprError::setError(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_unexpectedEndOfExpression));
        }

        // Delete any expressions and expression lists that were created
        while (Expression *e = parser.expressions.pop())
            delete e;
        while (Arguments *a = parser.args.pop())
            delete a;
    } else {
        // Successfully parsed expression tree
        expr = parser.expressions.pop();

#ifdef DEBUG
        // Assert that the number of nodes actually in the expression tree
        // matches the number of expressions the parser reported to us.  If
        // these don't match, it means we'll leak memory on syntax errors.
        int tree_count = expr->getExpressionNodeCount();
        int parser_count = parser.expressions.length() + 1;
        PR_ASSERT(tree_count == parser_count);
#endif
    }

    // Free expr_yy_parse's stacks
    yydata_destroy(&yydata);

    return expr;
}


/* ----------------------------- expr_yy_lex ------------------------------ */

int expr_yy_lex(YYDATA *yydata)
{
    LexerData *lexer = yydata->lexer;
    YYSTYPE& yylval = yydata->yylval;

    // Return the next token from the tokens vector that expr_parse built
    for (;;) {
        if (lexer->i >= lexer->ntokens)
            return -1;

        const Token *token = lexer->tokens[lexer->i];
        lexer->i++;

        yylval.string = token->value;

        switch (token->type) {
        case TOKEN_OPERATOR:
            return op_find(token->value, strlen(token->value))->yychar;

        case TOKEN_NUMBER:
            return EXPR_TOKEN_NUMBER;

        case TOKEN_IDENTIFIER:
            return EXPR_TOKEN_IDENTIFIER;

        case TOKEN_SINGLE_QUOTE_STRING:
            return EXPR_TOKEN_SINGLE_QUOTE_STRING;

        case TOKEN_DOUBLE_QUOTE_STRING:
            return EXPR_TOKEN_DOUBLE_QUOTE_STRING;

        default:
            PR_ASSERT(0);
        }
    }
}


/* ---------------------------- expr_yy_error ----------------------------- */

void expr_yy_error(YYDATA *yydata, const char *msg)
{
    ParserData *parser = yydata->parser;

    if (parser->error.length() == 0) {
        // yacc produces non-localized error messages
        if (strstr(msg, "syntax error")) {
            if (yydata->yylval.string && strlen(yydata->yylval.string)) {
                parser->error.printf(XP_GetAdminStr(DBT_syntaxErrorNearX), yydata->yylval.string);
            } else {
                parser->error.append(XP_GetAdminStr(DBT_syntaxError));
            }
        } else {
            parser->error.append(msg);
        }
    }
}


/* ----------------------------- expr_yy_expr ----------------------------- */

Expression *expr_yy_expr(YYDATA *yydata, Expression *e)
{
    ParserData *parser = yydata->parser;

    if (e == NULL) {
        // Last expr_* call failed to produce an Expression *.  Remember why so
        // we can log an error later.
        if (parser->error.length() == 0)
            parser->error.append(system_errmsg());
        e = new ExpressionBoolean(PR_FALSE);
    }

#ifdef DEBUG
    for (int i = 0; i < parser->expressions.length(); i++)
        PR_ASSERT(parser->expressions[i] != e);
#endif

    // Keep track of all expressions created by expr_yy_parse so that
    // expr_parse can deal with them after expr_yy_parse is done
    parser->expressions.append(e);

    return e;
}


/* ----------------------------- expr_yy_args ----------------------------- */

Arguments *expr_yy_args(YYDATA *yydata, Arguments *a)
{
    ParserData *parser = yydata->parser;

    if (a == NULL) {
        // Last args_* call failed to produce an Arguments *.  Remember why so
        // we can log an error later.
        if (parser->error.length() == 0)
            parser->error.append(system_errmsg());
    }

#ifdef DEBUG
    for (int i = 0; i < parser->args.length(); i++)
        PR_ASSERT(parser->args[i] != a);
#endif

    // Keep track of all argument lists created by expr_yy_parse so that
    // expr_parse can deal with them after expr_yy_parse is done
    parser->args.append(a);

    return a;
}


/* -------------------------- expr_const_string --------------------------- */

const char *expr_const_string(const Expression *expr)
{
    if (expr == NULL)
        return NULL;

    return expr->getConstString();
}


/* -------------------------- expr_const_pb_key --------------------------- */

const pb_key *expr_const_pb_key(const Expression *expr)
{
    if (expr == NULL)
        return NULL;

    return expr->getConstPbKey();
}


/* -------------------------------- expr_new_* -------------------------------- */

Expression *expr_new_named_or(Expression *l, Expression *r)
{
    return new ExpressionOr(op_named_or, l, r);
}

Expression *expr_new_named_xor(Expression *l, Expression *r)
{
    return new ExpressionXor(op_named_xor, l, r);
}

Expression *expr_new_named_and(Expression *l, Expression *r)
{
    return new ExpressionAnd(op_named_and, l, r);
}

Expression *expr_new_named_not(Expression *r)
{
    return new ExpressionNot(op_named_not, r);
}

Expression *expr_new_ternary(Expression *x, Expression *y, Expression *z)
{
    return new ExpressionTernary(x, y, z);
}

Expression *expr_new_c_or(Expression *l, Expression *r)
{
    return new ExpressionOr(op_c_or, l, r);
}

Expression *expr_new_c_and(Expression *l, Expression *r)
{
    return new ExpressionAnd(op_c_and, l, r);
}

Expression *expr_new_c_xor(Expression *l, Expression *r)
{
    return new ExpressionXor(op_c_xor, l, r);
}

Expression *expr_new_numeric_eq(Expression *l, Expression *r)
{
    return new ExpressionNumericCmp(op_numeric_eq, l, r);
}

Expression *expr_new_numeric_ne(Expression *l, Expression *r)
{
    return new ExpressionNumericCmp(op_numeric_ne, l, r);
}

Expression *expr_new_string_eq(Expression *l, Expression *r)
{
    return new ExpressionStringCmp(op_string_eq, l, r);
}

Expression *expr_new_string_ne(Expression *l, Expression *r)
{
    return new ExpressionStringCmp(op_string_ne, l, r);
}

Expression *expr_new_numeric_lt(Expression *l, Expression *r)
{
    return new ExpressionNumericCmp(op_numeric_lt, l, r);
}

Expression *expr_new_numeric_le(Expression *l, Expression *r)
{
    return new ExpressionNumericCmp(op_numeric_le, l, r);
}

Expression *expr_new_numeric_gt(Expression *l, Expression *r)
{
    return new ExpressionNumericCmp(op_numeric_gt, l, r);
}

Expression *expr_new_numeric_ge(Expression *l, Expression *r)
{
    return new ExpressionNumericCmp(op_numeric_ge, l, r);
}

Expression *expr_new_string_lt(Expression *l, Expression *r)
{
    return new ExpressionStringCmp(op_string_lt, l, r);
}

Expression *expr_new_string_le(Expression *l, Expression *r)
{
    return new ExpressionStringCmp(op_string_le, l, r);
}

Expression *expr_new_string_gt(Expression *l, Expression *r)
{
    return new ExpressionStringCmp(op_string_gt, l, r);
}

Expression *expr_new_string_ge(Expression *l, Expression *r)
{
    return new ExpressionStringCmp(op_string_ge, l, r);
}

Expression *expr_new_named_op(const char *n, Expression *r)
{
    ExpressionFunc *func = expr_control_func_find(n);
    if (func == NULL) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_noOpX), n);
        return NULL;
    }

    return new ExpressionNamedOpFunc(n, func, r);
}

Expression *expr_new_add(Expression *l, Expression *r)
{
    return new ExpressionAdd(l, r);
}

Expression *expr_new_subtract(Expression *l, Expression *r)
{
    return new ExpressionSubtract(l, r);
}

Expression *expr_new_concat(Expression *l, Expression *r)
{
    return new ExpressionConcat(l, r);
}

Expression *expr_new_multiply(Expression *l, Expression *r)
{
    return new ExpressionMultiply(l, r);
}

Expression *expr_new_divide(Expression *l, Expression *r)
{
    return new ExpressionDivide(l, r);
}

Expression *expr_new_modulo(Expression *l, Expression *r)
{
    return new ExpressionModulo(l, r);
}

Expression *expr_new_wildcard(Expression *l, Expression *r)
{
    return new ExpressionWildcard(l, r);
}

Expression *expr_new_re(Expression *l, Expression *r, PRBool match)
{
    const char *pattern = r->getConstString();
    if (pattern != NULL) {
        const char *error = NULL;
        int erroroffset;

        pcre *re = pcre_compile(pattern, 0, &error, &erroroffset, NULL);
        if (re == NULL) {
            if (error != NULL) {
                NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_badRegexBecauseX), error); // XXX PCRE l10n
            } else {
                NsprError::setError(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_badRegex));
            }
            return NULL;
        }

        return new ExpressionCompiledRe(l, r, re, match);
    }

    return new ExpressionRe(l, r, match);
}

Expression *expr_new_c_not(Expression *r)
{
    return new ExpressionNot(op_c_not, r);
}

Expression *expr_new_positive(Expression *r)
{
    return new ExpressionSigned(+1, r);
}

Expression *expr_new_negative(Expression *r)
{
    return new ExpressionSigned(-1, r);
}

Expression *expr_new_call(const char *f, Arguments *a)
{
    ExpressionFunc *func = expr_control_func_find(f);
    if (func == NULL) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_noControlX), f);
        return NULL;
    }

    return new ExpressionCallFunc(f, func, a);
}

Expression *expr_new_access(const char *i, Expression *s)
{
    // XXX what about bare words in map variable accesses? e.g. $headers{cookie}

    ExpressionFunc *func = expr_map_get_func_find(i);
    if (func == NULL) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_noMapX), i);
        return NULL;
    }

    return new ExpressionMapFunc(i, func, s);
}

Expression *expr_new_variable(const char *i)
{
    if (!strcmp(i, "&"))
        return new ExpressionBackref(0);

    char *endptr;
    unsigned long ul = strtoul(i, &endptr, 10);
    if (*endptr == '\0')
        return new ExpressionBackref(ul);

    PR_ASSERT(expr_is_identifier(i, strlen(i)));

    NSString v;
    v.append('$');
    v.append(i);

    return new ExpressionVarGet(v);
}

Expression *expr_new_identifier(const char *i)
{
    if (!strcmp(i, "true")) {
        return new ExpressionBoolean(PR_TRUE);
    } else if (!strcmp(i, "false")) {
        return new ExpressionBoolean(PR_FALSE);
    }

    if (expr_var_get_func_find(i) != NULL)
        return expr_new_variable(i);

    NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_syntaxErrorNearX), i);
    return NULL;
}

Expression *expr_new_true(void)
{
    return new ExpressionBoolean(PR_TRUE);
}

Expression *expr_new_false(void)
{
    return new ExpressionBoolean(PR_FALSE);
}

Expression *expr_new_number(const char *n)
{
    PRInt64 i = 0;
    PR_sscanf(n, "%lli", &i);
    return new ExpressionInteger(i);
}

Expression *expr_new_string(const char *s)
{
    return new ExpressionString(s);
}

Expression *expr_new_interpolative(const char *s)
{
    char *unescaped = model_unescape_interpolative(s);
    if (!unescaped)
        return NULL;

    ModelString *model = model_str_create(unescaped);

    FREE(unescaped);

    if (model != NULL)
        return model;

    NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, system_errmsg());
    return NULL;
}
