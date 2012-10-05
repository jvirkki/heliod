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

#ifndef FCGI_UTIL_H
#define FCGI_UTIL_H

#ifndef ISSLASH
# define ISSLASH(C) ((C) == '/')
#endif

//-----------------------------------------------------------------------------
// strmatch
//-----------------------------------------------------------------------------

inline PRBool strmatch(const char *a, const char *b)
{
    if (a == b)
        return PR_TRUE;
    if (!a || !b)
        return PR_FALSE;
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

inline PRBool isUds(const char *bPath) {
    PRBool rv = PR_FALSE;
    char *h = PL_strdup(bPath);
    char *p = PL_strrchr(h, ':');
    if(!p) {
        rv = PR_TRUE;
    }
    PL_strfree(h);
    return rv;
}

//-----------------------------------------------------------------------------
// Prototypes for utility functions
//-----------------------------------------------------------------------------
PRStatus getNetAddr(const char *host, PRNetAddr *addr);
PRStatus getSockAddr(const char *host, struct sockaddr **addr, int *localAddr);
char *getError();
char *host_port_suffix(char *h);
NSAPI_PUBLIC char **fcgi_util_env_create(char **env, int n, int *pos);
NSAPI_PUBLIC void fcgi_util_env_free(char **env);
NSAPI_PUBLIC char *fcgi_util_env_str(char *name, char *value);
NSAPI_PUBLIC char **fcgi_util_env_copy(char **src, char **dst);
NSAPI_PUBLIC PRBool fcgi_util_env_match(char **src, char **dst);
NSAPI_PUBLIC char *fcgi_util_arg_str(char *name);
NSAPI_PUBLIC char *baseName(const char *name);
#ifdef XP_WIN32
NSAPI_PUBLIC PRStatus createPipe(const char *host, const char *bindPathPrefix, HANDLE *hListenPipe);
#endif //XP_WIN32
#endif // FCGI_UTIL_H
