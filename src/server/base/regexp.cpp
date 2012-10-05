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
 * regexp.cpp: NSAPI compatibility wrappers for PCRE
 */

#include "pcre.h"
#include "base/regexp.h"
#include "base/ereport.h"
#include "base/dbtbase.h"


/* ---------------------------- _regexp_match ----------------------------- */

static int _regexp_match(const char *str, const char *exp, int options)
{
    const char *error;
    int erroroffset;
    pcre *re = pcre_compile(exp, options, &error, &erroroffset, NULL);
    if (!re) {
        ereport(LOG_MISCONFIG,
                XP_GetAdminStr(DBT_regexErrorSRegexS_),
                exp,
                error);
        return -1;
    }

    int rv = pcre_exec(re, NULL, str, strlen(str), 0, 0, NULL, 0);

    pcre_free(re);

    if (rv == PCRE_ERROR_NOMATCH)
        return 1;
    if (rv < 0)
        return -1;

    return 0;
}


/* ----------------------------- regexp_valid ----------------------------- */

NSAPI_PUBLIC int regexp_valid(const char *exp)
{
    if (!strpbrk(exp, "\\^$.[|()?*+{"))
        return NON_SXP;

    const char *error;
    int erroroffset;
    pcre *re = pcre_compile(exp, 0, &error, &erroroffset, NULL);
    if (!re) {
        ereport(LOG_MISCONFIG,
                XP_GetAdminStr(DBT_regexErrorSRegexS_),
                exp,
                error);
        return INVALID_SXP;
    }

    pcre_free(re);

    return VALID_SXP;
}


/* ----------------------------- regexp_match ----------------------------- */

NSAPI_PUBLIC int regexp_match(const char *str, const char *exp)
{
    return _regexp_match(str, exp, 0);
}


/* ------------------------------ regexp_cmp ------------------------------ */

NSAPI_PUBLIC int regexp_cmp(const char *str, const char *exp)
{
    // Historically regexp_cmp would evaluate a regular expression without
    // first validating it (i.e. regexp_cmp assumed the caller had already
    // called regexp_valid)
    return _regexp_match(str, exp, 0);
}


/* ---------------------------- regexp_casecmp ---------------------------- */

NSAPI_PUBLIC int regexp_casecmp(const char *str, const char *exp)
{
    return _regexp_match(str, exp, PCRE_CASELESS);
}
