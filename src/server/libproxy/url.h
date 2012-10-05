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

#ifndef LIBPROXY_URL_H
#define LIBPROXY_URL_H

/*
 * url.h: Simple nondestructive in situ URL parsing
 *
 * Chris Elving
 */

#include "netsite.h"

/*
 * URL schemes
 */
enum Scheme {
    SCHEME_UNKNOWN = -1,
    SCHEME_NONE = 0,
    SCHEME_HTTP,
    SCHEME_HTTPS,
    SCHEME_FTP,
    SCHEME_GOPHER
};

/*
 * ParsedUrl represents a (partially) parsed URL
 */
struct ParsedUrl {
    enum Scheme scheme;
    char *user; /* May be NULL */
    int user_len;
    char *password; /* May be NULL */
    int password_len;
    char *host; /* May be NULL */
    int host_len;
    int host_port_len; /* Length of host:port */
    int port; /* May be 0 */
    char *path; /* Actually path, query, and fragment */
};

NSPR_BEGIN_EXTERN_C

/*
 * url_parse parses an URL into a ParsedUrl.  The contents of the ParsedUrl are
 * only valid when url_parse returns PR_SUCCESS.  Returns PR_FAILURE if url
 * specified an invalid port.  Otherwise, returns PR_SUCCESS.
 */
PRStatus url_parse(char *url, ParsedUrl *parsed);

/*
 * url_get_scheme returns the Scheme constant for the given scheme string.
 * Returns SCHEME_NONE if len is 0 or SCHEME_UNKNOWN if the scheme is not
 * recognized.
 */
Scheme url_get_scheme(const char *scheme, int len);

/*
 * url_default_port returns the default port for the given scheme.  Returns 0
 * if the scheme is SCHEME_UNKNOWN, SCHEME_NONE, or invalid.
 */
int url_default_port(Scheme scheme);

/*
 * url_get_scheme_string returns the Scheme string for the given the Scheme constant.
 * Returns "unknown" if scheme is SCHEME_UNKNOWN, SCHEME_NONE, or invalid.
 */
const char *url_get_scheme_string(Scheme scheme);

NSPR_END_EXTERN_C

#endif /* LIBPROXY_URL_H */
