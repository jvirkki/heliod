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

#ifndef FRAME_MODEL_PVT_H
#define FRAME_MODEL_PVT_H

/*
 * model_pvt.h: String and pblock interpolation private declarations
 * 
 * Chris Elving
 */

#include "support/NSString.h"
#include "support/GenericVector.h"

#ifndef FRAME_EXPR_PVT_H
#include "expr_pvt.h"
#endif

/*
 * Fragment is the abstract base class for the constituent fragments of string
 * models.
 */
class Fragment;

/*
 * ModelString is a model from which a synthetic string may be constructed.
 */
class ModelString : public ExpressionNullary {
public:
    /*
     * Construct an empty string model.
     */
    ModelString();

    /*
     * Destroy the string model and its constituent fragments.
     */
    ~ModelString();

    /*
     * Create a copy of the string model and its constituent fragments.
     */
    Expression *dup() const;

    /*
     * Create a copy of the string model and its constituent fragments.
     */
    ModelString *dupModelString() const;

    /*
     * Append a simple nonescaped, noninterpolative string to the string model.
     */
    void addNonescaped(const NSString& s);

    /*
     * Append a single escaped '$' to the string model.
     */
    void addEscapedDollar();

    /*
     * Append an expression the string model.  The string model assumes
     * ownership of the passed expression.
     */
    void addExpression(const NSString& s, Expression *e);

    /*
     * Mark the model as complete.
     */
    void complete();

    /*
     * Indicate whether the string model requires interpolation (that is,
     * whether it contains $fragments or "$$" sequences).
     */
    PRBool isInterpolative() const { return interpolative; }

    /*
     * Construct a synthetic string based on the string model.
     */
    PRStatus interpolate(Context& context, const char **pp, int *plen, pool_handle_t **ppool) const;

    /*
     * Construct a synthetic string based on the string model.
     */
    Result evaluate(Context& context) const;

    /*
     * Append a quoted, escaped, uninterpolated version of the string model to
     * the passed buffer.
     */
    void format(NSString& formatted, Precedence precedence) const;

private:
    ModelString(const ModelString&);
    ModelString& operator=(const ModelString&);

    PRBool invariant;
    PRBool interpolative;
    NSString unescaped;
    NSString uninterpolated;
    mutable int estimate;
    PtrVector<Fragment> fragments;
};

#endif /* FRAME_MODEL_PVT_H */
