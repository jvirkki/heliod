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

#ifndef XP_WIN32
  #include <sys/un.h>
#endif  // XP_WIN32

#include <nspr.h>
#include <plstr.h>
#include "base/util.h"
#include "util.h"


char errorMsg[256];

char *getError() { return errorMsg; }

char *host_port_suffix(char *h) {
    /* Return a pointer to the colon preceding the port number in a hostname.
     *
     * host_port_suffix("foo.com:80") = ":80"
     * host_port_suffix("foo.com") = NULL
     * host_port_suffix("[::]:80") = ":80"
     * host_port_suffix("[::]") = NULL
     */

    if (h == NULL)
        return h;

    for (;;) {
        /* Find end of host, beginning of ":port", or an IPv6 address */
        for (;;) {
            register char c = *h;

            if (c == '\0')
                return NULL; /* end of host, no port found */

            if (c == '/')
                return NULL; /* end of host, no port found */

            if (c == ':')
                return h; /* found port */

            if (c == '[')
                break; /* skip IPv6 address */

            h++;
        }

        /* Skip IPv6 address */
        while (*h != '\0' && *h != ']')
            h++;
    }
}

//-----------------------------------------------------------------------------
// getNetAddr
//-----------------------------------------------------------------------------

PRStatus getNetAddr(const char *host, PRNetAddr *addr) {
    PRStatus rv = PR_SUCCESS;
    memset(addr, 0, sizeof(*addr));

    // Extract the port number
    int port = 0;
    char *h = PL_strdup(host);
    char * p = host_port_suffix(h);

    // uds name
    if(!p) {
#ifndef XP_WIN32
        addr->local.family = PR_AF_LOCAL;
        strcpy(addr->local.path, h);
#else
        addr = NULL;
        return PR_SUCCESS;
#endif  // XP_WIN32
    } else {
        if (p) {
            *p = '\0';
            p++;

            // validate the port number
            if(*p == '\0') {
                // port is empty
                PL_strcpy(errorMsg, "port is missing in the bind-path");
                return PR_FAILURE;
            }

            // check if port has only digits
            PRBool invalidPort = PR_FALSE;

            char *startP = p;
            while(p && *p) {
                if(!isdigit(*p)) {
                    invalidPort = PR_TRUE;
                    break;
                }
                p++;
            }

            if(invalidPort) {
                PR_snprintf(errorMsg, 256, "invalid port %s specified in the bind-path", startP);
                return PR_FAILURE;
            }

            port = atoi(startP);
            if(port < 1 || port > 65535) {
                PR_snprintf(errorMsg, 256, "port %s specified in the bind-path is out of range", startP);
                return PR_FAILURE;
            }
        }

        // try to resolve the hostname
        PRHostEnt he;
        char buffer[PR_NETDB_BUF_SIZE];

        if(h && *h != '\0') {
            // Format a PRNetAddr
            rv = PR_GetHostByName(h, buffer, sizeof(buffer), &he);
        } else { // no host specified, consider it as localhost
            rv = PR_GetHostByName("localhost", buffer, sizeof(buffer), &he);
        }

        if (rv == PR_SUCCESS) {
            if (PR_EnumerateHostEnt(0, &he, port, addr) < 0) {
                rv = PR_FAILURE;
                PR_snprintf(errorMsg, 256, "invalid host %s specified in the bind-path", h);
                printf("invalid host is specified in the bind-path\n");
            }
        }
    }

    PL_strfree(h);

    return rv;
}

PRStatus getSockAddr(const char *host, struct sockaddr **addr, int *localAddr ) {
    PRStatus rv = PR_SUCCESS;
     *localAddr = 0;
    // Extract the port number
    int port = 0;
    char *h = PL_strdup(host);
    char * p = host_port_suffix(h);

    // uds name
    if(!p) {
        *localAddr = 1;
#ifndef XP_WIN32
        struct sockaddr_un *udsSock = (struct sockaddr_un *)malloc(sizeof(struct sockaddr_un));
        memset((char *)udsSock, 0, sizeof(struct sockaddr_un));
        udsSock->sun_family = PR_AF_LOCAL;
        strcpy(udsSock->sun_path, h);
        *addr = (struct sockaddr *)udsSock;
#else
        PL_strcpy(errorMsg, "UDS is not supported on Windows");
        return PR_FAILURE;  // no UDS on Windows
#endif  // XP_WIN32

    } else {
        if (p) {
            *p = '\0';
            p++;

            // validate the port number
            if(*p == '\0') {
                // port is empty
                PL_strcpy(errorMsg, "port is missing in the bind-path");
                return PR_FAILURE;
            }

            // check if port has only digits
            PRBool invalidPort = PR_FALSE;
            char *startP = p;
            while(p && *p) {
                if(!isdigit(*p)) {
                    invalidPort = PR_TRUE;
                    break;
                }
                p++;
            }

            if(invalidPort) {
                PR_snprintf(errorMsg, 256, "invalid port %s specified in the bind-path", startP);
                return PR_FAILURE;
            }

            port = atoi(startP);
            if(port < 1 || port > 65535) {
                PR_snprintf(errorMsg, 256, "port %s specified in the bind-path is out of range", startP);
                return PR_FAILURE;
            }
        }

        struct sockaddr_in *servAddr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
        memset((char*)servAddr, 0, sizeof(struct sockaddr_in));
        servAddr->sin_family = PF_INET;

        struct hostent *pHostEnt;
        if(h && *h != '\0') {  // if hostname is specified
           pHostEnt = gethostbyname( h );
        } else { // if no hostname specified, consider "localhost"
           pHostEnt = gethostbyname( "localhost" );
        }

        if ( !pHostEnt ) {
            PR_snprintf(errorMsg, 256, "invalid host %s specified in the bind-path", h);
            return PR_FAILURE;
        }

        memcpy ( &(servAddr->sin_addr), pHostEnt->h_addr, pHostEnt->h_length );
        servAddr->sin_port = htons(port);
        *addr = (struct sockaddr *)servAddr;
    }

    PL_strfree(h);

    return rv;
}

NSAPI_PUBLIC char **fcgi_util_env_create(char **env, int n, int *pos) {
    int x;

    if(!env) {
        *pos = 0;
        return (char **) PR_Malloc((n + 1)*sizeof(char *));
    }
    else {
        for(x = 0; (env[x]); x++);
        env = (char **) PR_Realloc(env, (n + x + 1)*(sizeof(char *)));
        *pos = x;
        return env;
    }
}

NSAPI_PUBLIC void fcgi_util_env_free(char **env)
{
    register char **ep = env;

    for(ep = env; *ep; ep++)
        PR_Free(*ep);

    PR_Free(env);
}

NSAPI_PUBLIC char *fcgi_util_arg_str(char *name) {
    char *t;

    t = (char *) PR_Malloc(strlen(name)+1); /* 1 = '\0' */

    PL_strcpy(t, name);

    return t;
}

/* ----------------------------- util_env_str ----------------------------- */


NSAPI_PUBLIC char *fcgi_util_env_str(char *name, char *value) {
    char *t;

    t = (char *) PR_Malloc(strlen(name)+strlen(value)+2); /* 2: '=' and '\0' */

    sprintf(t, "%s=%s", name, value);

    return t;
}

NSAPI_PUBLIC char **fcgi_util_env_copy(char **src, char **dst)
{
    char **src_ptr;
    int src_cnt;
    int index;

    if (!src)
        return NULL;

    for (src_cnt = 0, src_ptr = src; *src_ptr; src_ptr++, src_cnt++);

    if (!src_cnt)
        return NULL;

    dst = fcgi_util_env_create(dst, src_cnt, &index);

    for (src_ptr = src; *src_ptr; index++, src_ptr++)
        dst[index] = PL_strdup(*src_ptr);
    dst[index] = NULL;

    return dst;
}

NSAPI_PUBLIC PRBool fcgi_util_env_match(char **src, char **dst) {
    char **src_ptr;
    char **dst_ptr;
    int index;

    if (!src || !dst)
        return PR_FALSE;

    for (index = 0, src_ptr = src, dst_ptr = dst; *src_ptr; src_ptr++, dst_ptr++, index++) {
        if(!strmatch(src[index], dst[index]))
            return PR_FALSE;
    }

    if (!(src[index] && dst[index]))
        return PR_FALSE;

    return PR_TRUE;
}

NSAPI_PUBLIC char * baseName (const char *fileName) {
  char *base = NULL;
  char *fname = NULL;
  base = PL_strdup(fileName);
  fname = base;
  int len = PL_strlen(fname);
  char *p = PL_strrchr(base, '/');

#ifdef XP_WIN32
    if (!p) {  // if no forward slashes check for backward slashes
        p = PL_strrchr(base, '\\');
    }
#endif // XP_WIN32

  if (!p) {  // if no slashes
#ifdef XP_WIN32
    char *driveLetter = strchr(base, ':');
    if (driveLetter) {   // specified only the drive letter
        base = NULL;
    }
#endif // XP_WIN32
  } else if (p == base)  { // only one slash
    if (len > 1) { // more chars after '/'
        base = p + 1;
    } else {
        base = NULL;
    }
  } else if (p == (base + (len - 1))) {
      // name is ending with a slash..set base to NULL
      base = NULL;
  } else {
      base = p + 1;
  }

  if (!base) {
     // set the passed name as the base name
     base = fname;
  }

  char *bname = PL_strdup(base);
  PL_strfree(fname);

  return bname;
}

#ifdef XP_WIN32
NSAPI_PUBLIC PRStatus createPipe(const char *host, const char *bindPathPrefix, HANDLE *hListenPipe) {
        *hListenPipe = INVALID_HANDLE_VALUE;

        *hListenPipe = CreateNamedPipe(host,
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_READMODE_BYTE,
                PIPE_UNLIMITED_INSTANCES,
                4096, 4096, 0, NULL);

        if (*hListenPipe == INVALID_HANDLE_VALUE) {
            return PR_FAILURE;
        }

        if (! SetHandleInformation(*hListenPipe, HANDLE_FLAG_INHERIT, TRUE)) {
            return PR_FAILURE;
        }

    return PR_SUCCESS;
}
#endif // XP_WIN32

