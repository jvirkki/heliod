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

#ifndef _VAR_H
#define _VAR_H

PR_BEGIN_EXTERN_C

PRStatus var_init(void);

/*
 * var_get_pblock returns a pblock* for the pblock named pbname.  Returns NULL
 * if pbname is not a a valid pblock name.
 */
pblock *var_get_pblock(Session *sn, Request *rq, const char *pbname);

/*
 * var_get_name_value extracts name and value strings from the supplied nv
 * string.  nv can be in of two forms:
 *
 *     1. "name=value"
 *     2. "Name: value"
 *
 * If nv is given in form 2, name will be converted to lowercase.  Note that
 * this function will modify nv and that *pvalue may be NULL even if this
 * function returns PR_SUCCESS.
 */
PRStatus var_get_name_value(char *nv, const char **pname, const char **pvalue);

/*
 * var_get_pblock_name_value extracts a pblock *, name, and value from the
 * supplied pbname and nv strings.  If pbname is NULL or empty, nv is assumed
 * to begin with a pblock name followed by '.'.
 */
PRStatus var_get_pblock_name_value(Session *sn, Request *rq, const char *pbname, char *nv, pblock **ppb, const char **pname, const char **pvalue);

/*
 * var_set_variable assigns a value to a $variable.
 */
PRStatus var_set_variable(Session *sn, Request *rq, const char *n, const char *v);

/*
 * var_set_internal_variable assigns a value to a $variable, provided the
 * named variable is a predefined internal variable.
 */
PRStatus var_set_internal_variable(Session *sn, Request *rq, const char *n, const char *v);

PR_END_EXTERN_C

#endif /* _VAR_H */
