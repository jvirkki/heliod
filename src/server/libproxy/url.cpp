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
 * url.h: Simple nondestructive in situ URL parsing
 *
 * Chris Elving
 */

#include "base/util.h"
#include "libproxy/url.h"


/* ------------------------------------------------------------------------ */

Scheme url_get_scheme(const char *scheme, int len)
{
    switch (len) {
    case 0:
        return SCHEME_NONE;

    case 3:
        if (!strncasecmp(scheme, "ftp", 3))
            return SCHEME_FTP;
        break;

    case 4:
        if (!strncasecmp(scheme, "http", 4))
            return SCHEME_HTTP;
        break;

    case 5:
        if (!strncasecmp(scheme, "https", 5))
            return SCHEME_HTTPS;
        break;

    case 6:
        if (!strncasecmp(scheme, "gopher", 6))
            return SCHEME_GOPHER;
        break;
    }

    return SCHEME_UNKNOWN;
}


/* ------------------------------------------------------------------------ */

const char *url_get_scheme_string(Scheme scheme)
{
    switch (scheme) {
    case SCHEME_HTTP:
        return "http";

    case SCHEME_HTTPS:
        return "https";

    case SCHEME_FTP:
        return "ftp";

    case SCHEME_GOPHER:
        return "gopher";
    }

    return "unknown";
}


/* ------------------------------------------------------------------------ */

int url_default_port(Scheme scheme)
{
    switch (scheme) {
    case SCHEME_HTTP:
        return 80;

    case SCHEME_HTTPS:
        return 443;

    case SCHEME_FTP:
        return 21;

    case SCHEME_GOPHER:
        return 70;
    }

    return 0;
}


/* ------------------------------------------------------------------------ */

PRStatus url_parse(char *url, ParsedUrl *parsed)
{
    char *p = url;

    // Optional scheme is terminated by ":/"
    char *scheme = p;
    while (isalpha(*p))
        p++;
    if (p[0] == ':' && p[1] == '/') {
        parsed->scheme = url_get_scheme(scheme, p - scheme);
        p++;
    } else {
        parsed->scheme = SCHEME_NONE;
        p = scheme;
    }

    // Optional authority is preceded by "//"
    char *auth = NULL;
    if (p[0] == '/' && p[1] == '/') {
        p += 2;
        auth = p;
    }

    // Optional user:password in authority is terminated by "@"
    char *colon = NULL;
    if (auth) {
        while (*p && *p != '@' && *p != '/') {
            if (*p == ':')
                colon = p;
            p++;
        }
        if (*p != '@')
            p = auth;
    }
    if (auth && p > auth) {
        parsed->user = auth;
        if (colon) {
            parsed->user_len = colon - auth;
            parsed->password = colon + 1;
            parsed->password_len = p - (colon + 1);
        } else {
            parsed->user_len = p - auth;
            parsed->password = NULL;
            parsed->password_len = 0;
        }
        p++;
    } else {
        parsed->user = NULL;
        parsed->user_len = 0;
        parsed->password = NULL;
        parsed->password_len = 0;
    }

    // Optional host:port in authority or at start of URL is terminated by "/"
    char *host = p;
    if (auth || p == url) {
        char *port_suffix = util_host_port_suffix(host);
        if (port_suffix) {
            parsed->port = atoi(port_suffix + 1);
            if (parsed->port < 1 || parsed->port > 65535)
                return PR_FAILURE;
            p = port_suffix;
        } else {
            parsed->port = url_default_port(parsed->scheme);
            while (*p && *p != '/')
                p++;
        }
    } else {
        parsed->port = url_default_port(parsed->scheme);
    }
    if (p > host) {
        parsed->host = host;
        parsed->host_len = p - host;
        while (*p && *p != '/')
            p++;
        parsed->host_port_len = p - host;
    } else {
        parsed->host = NULL;
        parsed->host_len = 0;
        parsed->host_port_len = 0;
    }

    // Path is remainder of URL
    if (*p == '/') {
        parsed->path = p;
    } else {
        parsed->path = NULL;
    }

    return PR_SUCCESS;
}
