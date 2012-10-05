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

#ifndef FRAME_EXPR_PVT_H
#define FRAME_EXPR_PVT_H

/*
 * expr_pvt.h: NSAPI expression evaluation private declarations
 * 
 * Chris Elving
 */

#include "support/NSString.h"

#ifndef FRAME_RESULT_PVT_H
#include "result_pvt.h"
#endif

/*
 * The Precedence constants define the operator precedence hierarchy used when
 * formatting an expression.  The precedence rules must be consistent with
 * the expression grammar defined by expr.y.
 */
enum Precedence {
    EXPR_PRECEDENCE_NONE = 0,
    EXPR_PRECEDENCE_NAMED_OR,
    EXPR_PRECEDENCE_NAMED_AND,
    EXPR_PRECEDENCE_NAMED_NOT,
    EXPR_PRECEDENCE_TERNARY,
    EXPR_PRECEDENCE_C_OR,
    EXPR_PRECEDENCE_C_AND,
    EXPR_PRECEDENCE_C_XOR,
    EXPR_PRECEDENCE_EQUALITY,
    EXPR_PRECEDENCE_RELATIONAL,
    EXPR_PRECEDENCE_NAMED_OP,
    EXPR_PRECEDENCE_ADDITIVE,
    EXPR_PRECEDENCE_MULTIPLICATIVE,
    EXPR_PRECEDENCE_MATCHING,
    EXPR_PRECEDENCE_SIGN,
    EXPR_PRECEDENCE_DOLLAR
};

/*
 * Expression is the abstract base class for nodes in an expression tree.
 */
class Expression {
public:
    /*
     * Destroy the expression node.
     *
     * Note that any descendant expression nodes (i.e. operands) are not
     * destroyed.  To destroy an entire expression tree, root->deleteChildren()
     * then delete root.
     */
    virtual ~Expression() { }

    /*
     * Recursively destroy all descendant expression nodes.
     */
    virtual void deleteChildren() = 0;

    /*
     * Create a copy of the expression tree (that is, of this Expression and
     * any descendant expression nodes).
     */
    virtual Expression *dup() const = 0;

    /*
     * Evaluate the expression in a particular context.
     */
    virtual Result evaluate(Context& context) const = 0;

    /*
     * Append a string version of the expression to the passed buffer.
     * precedence specifies the precedence of the parent operator; if it is
     * higher than the precedence of the operator contained in this expression
     * node, format will wrap the string in parentheses.
     */
    virtual void format(NSString& formatted, Precedence precedence) const = 0;

    /*
     * Return the number of nodes in the expression tree that's rooted at this
     * node.
     */
    virtual int getExpressionNodeCount() const = 0;

    /*
     * Return a const char * if the expression will always evaluate to that
     * particular string.
     */
    const char *getConstString() const { return string; }

    /*
     * Return a const pb_key * if the expression will always evaluate to a
     * string with that particular pb_key.
     */
    const pb_key *getConstPbKey() const { return key; }

protected:
    /*
     * Construct an expression node.
     */
    Expression() : string(NULL), key(NULL) { }

    /*
     * Indicate that the expression will always evaluate to a particular
     * string.
     */
    void setConstString(const char *s);

private:
    Expression(const Expression&);
    Expression& operator=(const Expression&);

    const char *string;
    const pb_key *key;
};

/*
 * ExpressionNullary is the abstract base class for expressions without any
 * operands.
 */
class ExpressionNullary : public Expression {
public:
    void deleteChildren() { }
    int getExpressionNodeCount() const { return 1; }
};

#endif /* FRAME_EXPR_PVT_H */
