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
 * wdutil.cpp
 * 
 */

#include <ctype.h>
#include <stdlib.h>

/* --------------------------- util_env_create ---------------------------- */

char **util_env_create(char **env, int n, int *pos)
{
    int x;

    if(!env) {
        *pos = 0;
        return (char **) malloc((n + 1)*sizeof(char *));
    }
    else {
        for(x = 0; (env[x]); x++);
        env = (char **) realloc(env, (n + x + 1)*(sizeof(char *)));
        *pos = x;
        return env;
    }
}


/* ---------------------------- util_env_free ----------------------------- */

void util_env_free(char **env)
{
    register char **ep = env;

    for(ep = env; *ep; ep++)
        free(*ep);
    free(env);
}


/* --------------------------- util_argv_parse ---------------------------- */

static inline const char *parse_arg(const char *src, char *dst)
{
    while (isspace(*src))
        src++;

    if (!*src)
        return NULL;

    for (char quote = '\0'; *src; src++) {
        if (quote) {
            if (*src == quote) {
                quote = '\0'; // end of quoted span
                continue;
            }
        } else {
            if (*src == '"' || *src == '\'') {
                quote = *src; // beginning of quoted span
                continue;
            } else if (isspace(*src)) {
                break;
            } else if (*src == '\\') {
                if (src[1] == '"' || src[1] == '\'' || isspace(src[1]))
                    src++; // escaped quote or space
            }
        }
        if (dst)
            *dst++ = *src;
    }

    if (dst)
        *dst = '\0';

    return src;
}

static inline int parse_args(const char *args, char **argv)
{
    const char *p;
    int i;

    for (i = 0; p = parse_arg(args, NULL); i++) {
        if (argv) {
            argv[i] = (char *) malloc(p - args + 1);
            if (!argv[i])
                break;
            parse_arg(args, argv[i]);
        }
        args = p;
    }

    return i;
}

char **util_argv_parse(const char *cmdline)
{
    int n = parse_args(cmdline, NULL);

    int i;
    char **argv = util_env_create(NULL, n, &i);
    if (argv) {
        parse_args(cmdline, &argv[i]);
        argv[i + n] = NULL;
    }

    return argv;
}

