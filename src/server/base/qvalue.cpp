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
 * qvalue.cpp: manipulate HTTP quality qvalues
 * 
 * Rob McCool
 */

#include "netsite.h"
#include "base/util.h"


/* ------------------------------ util_qtoi ------------------------------- */

/*
 * RFC 2616
 *
 * Optional whitespace can appear almost anywhere:
 *
 *     The grammar described by this specification is word-based. Except
 *     where noted otherwise, linear white space (LWS) can be included
 *     between any two adjacent words (token or quoted-string), and
 *     between adjacent words and separators, without changing the
 *     interpretation of a field.
 *
 * Elements in a list are separted by commas:
 *
 *     1#element - ( *LWS element *( *LWS "," *LWS element ))
 *
 * qvalues are introduced by *LWS ";" *LWS "q" *LWS "=" *LWS:
 *
 *     accept-params  = ";" "q" "=" qvalue *( accept-extension )
 *
 * qvalues follow a specific format:
 *
 *     qvalue = ( "0" [ "." 0*3DIGIT ] )
 *            | ( "1" [ "." 0*3("0") ] )
 */

static inline int parse_qvalue(const char *q, const char **p)
{
    int rv = 0;

    if (*q == '1') {
        q++;
        if (*q == '.') {
            q++;
            if (*q == '0') {
                q++;
                if (*q == '0') {
                    q++;
                    if (*q == '0')
                        q++;
                }
            }
        }
        rv = 1000;
    } else if (*q == '0') {
        q++;
        if (*q == '.') {
            q++;
            if (*q >= '0' && *q <= '9') {
                rv += (*q++ - '0') * 100;
                if (*q >= '0' && *q <= '9') {
                    rv += (*q++ - '0') * 10;
                    if (*q >= '0' && *q <= '9')
                        rv += (*q++ - '0');
                }
            }
        }
    }

    if (p)
        *p = q;

    return rv;
}

NSAPI_PUBLIC int util_qtoi(const char *q, const char **p)
{
    int rv = 1000;

    while (isspace(*q))
        q++;

    if (*q == ';') {
        q++;
        while (isspace(*q))
            q++;
        if (*q == 'q') {
            q++;
            while (isspace(*q))
                q++;
            if (*q == '=') {
                q++;
                while (isspace(*q))
                    q++;
                rv = parse_qvalue(q, p);
            }
        }
    }

    return rv;
}
