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
 * args.cpp: NSAPI expression argument lists
 * 
 * Chris Elving
 */

#include "netsite.h"
#include "frame/args.h"
#include "args_pvt.h"


/* ------------------------------ Arguments ------------------------------- */

void Arguments::deleteChildren()
{
    for (int i = 0; i < length(); i++)
        expr_free(get(i));
}

Arguments *Arguments::dup() const
{
    Arguments *l = new Arguments();
    for (int i = 0; i < length(); i++)
        l->add(expr_dup(get(i)));
    return l;
}


/* ----------------------------- args_create ------------------------------ */

Arguments *args_create(void)
{
    return new Arguments();
}


/* ----------------------------- args_append ------------------------------ */

Arguments *args_append(Arguments *args, Expression *expr)
{
    PR_ASSERT(expr != NULL);

    if (args == NULL)
        args = new Arguments();

    if (expr != NULL)
        args->add(expr);

    return args;
}


/* ------------------------------- args_dup ------------------------------- */

Arguments *args_dup(const Arguments *args)
{
    if (args == NULL)
        return NULL;

    return args->dup();
}


/* ------------------------------ args_free ------------------------------- */

void args_free(Arguments *args)
{
    if (args == NULL)
        return;

    // Delete the argument list's children (and grandchildren, etc.)
    args->deleteChildren();

    // Delete the argument list itself
    delete args;
}


/* ----------------------------- args_length ------------------------------ */

int args_length(const Arguments *args)
{
    if (args == NULL)
        return 0;

    return args->length();
}


/* ------------------------------- args_get ------------------------------- */

const Expression *args_get(const Arguments *args, int i)
{
    if (args == NULL || i < 0 || i > args->length())
        return NULL;

    return args->get(i);
}
