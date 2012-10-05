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

#ifndef FRAME_ARGS_PVT_H
#define FRAME_ARGS_PVT_H

/*
 * args_pvt.h: NSAPI expression argument lists private declarations
 * 
 * Chris Elving
 */

#include "support/GenericVector.h"

/*
 * Arguments is an argument list.
 */
class Arguments {
public:
    /*
     * Construct an empty argument list.
     */
    Arguments() { }

    /*
     * Destroy the argument list.
     *
     * Note that any descendant expression nodes (i.e. arguments) are not
     * destroyed.  To destroy an argument list, the arguments, and the
     * arguments' descendants, args->deleteChildren() then delete args.
     */
    ~Arguments() { }

    /*
     * Recursively destroy all descendant expression nodes.
     */
    void deleteChildren();

    /*
     * Add an expression to the argument list.
     */
    void add(Expression *e) { vector.append(e); }

    /*
     * Return the number of expressions in the arguments list.
     */
    int length() const { return vector.length(); }

    /*
     * Get the ith expression from the argument list.  The caller must ensure
     * i < length().
     */
    Expression *get(int i) { return vector[i]; }
    const Expression *get(int i) const { return vector[i]; }

    /*
     * Create a copy of the argument list (that is, of this Arguments object
     * and any descendant expression nodes).
     */
    Arguments *dup() const;

private:
    Arguments(const Arguments&);
    Arguments& operator=(const Arguments&);

    PtrVector<Expression> vector;
};

#endif /* FRAME_ARGS_PVT_H */
