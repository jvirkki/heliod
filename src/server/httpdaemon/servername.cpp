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

#include "netsite.h"
#include "base/util.h"
#include "httpdaemon/servername.h"

//-----------------------------------------------------------------------------
// ServerName::ServerName
//-----------------------------------------------------------------------------

ServerName::ServerName(const char *serverName,
                       const char *defaultScheme,
                       PRUint16 defaultPort)
{
    char *url = strdup(serverName);

    // Check whether the server name begins with a scheme or hostname
    char *p = url;
    while (isalpha(*p))
        p++;
    if (p[0] == ':' && p[1] == '/' && p[2] == '/') {
        // Separate scheme from the server name
        *p = '\0';
        p += 3;
        scheme_ = url;
        hostname_ = strdup(p);
        flagExplicitScheme_ = PR_TRUE;
    } else {
        // Use the default scheme
        scheme_ = strdup(defaultScheme);
        hostname_ = url;
        flagExplicitScheme_ = PR_FALSE;
    }

    // Get the default port for the specified scheme
    if (!strcasecmp(scheme_, HTTPS_URL)) {
        schemePort_ = HTTPS_PORT;
    } else if (!strcasecmp(scheme_, HTTP_URL)) {
        schemePort_ = HTTP_PORT;
    } else {
        schemePort_ = 0;
    }

    // Check for explicit port
    char *portSuffix = util_host_port_suffix(hostname_);
    if (portSuffix && isdigit(portSuffix[1])) {
        *portSuffix = '\0';
        port_ = atoi(&portSuffix[1]);
        flagExplicitPort_ = PR_TRUE;
    } else if (flagExplicitScheme_ && schemePort_ != 0) {
        port_ = schemePort_;
        flagExplicitPort_ = PR_FALSE;
    } else {
        port_ = defaultPort;
        flagExplicitPort_ = PR_FALSE;
    }
}

//-----------------------------------------------------------------------------
// ServerName::~ServerName
//-----------------------------------------------------------------------------

ServerName::~ServerName()
{
    free(scheme_);
    free(hostname_);
}
