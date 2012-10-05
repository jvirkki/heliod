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

#ifndef HTTPDAEMON_SERVERNAME_H
#define HTTPDAEMON_SERVERNAME_H

//-----------------------------------------------------------------------------
// ServerName
//-----------------------------------------------------------------------------

/** 
 * ServerName is the parsed representation of a server name.  A server name
 * consists of a hostname, scheme, and port.
 */
class ServerName {
public:
    /**
     * Construct a server name from the given string.  The string must include
     * a hostname.  If the string does not specify a scheme, assume
     * defaultScheme.  If the string does not specify a port, assume
     * defaultPort.
     */
    ServerName(const char *serverName,
               const char *defaultScheme = HTTP_URL,
               PRUint16 defaultPort = HTTP_PORT);

    /**
     * Destroy the server name.
     */
    ~ServerName();

    /**
     * Return the scheme.
     */
    const char *getScheme() const { return scheme_; }

    /**
     * Return the hostname.
     */
    const char *getHostname() const { return hostname_; }

    /**
     * Return the port.
     */
    PRUint16 getPort() const { return port_; }

    /**
     * Return the default port for the scheme.  Returns 0 if the scheme is
     * unknown (i.e. something other than http or https).  This function is
     * useful when constructing URLs; if (getPort() != getSchemePort()), the
     * port should be specified in the URL.
     */
    PRUint16 getSchemePort() const { return schemePort_; }

    /**
     * Indicate whether the server name explicitly specified the scheme.
     */
    PRBool hasExplicitScheme() const { return flagExplicitScheme_; }

    /**
     * Indicate whether the server name explicitly specified the port.
     */
    PRBool hasExplicitPort() const { return flagExplicitPort_; }

private:
    ServerName(const ServerName&);
    ServerName& operator=(const ServerName&);

    char *scheme_;
    char *hostname_;
    PRUint16 port_;
    PRUint16 schemePort_;
    PRBool flagExplicitScheme_;
    PRBool flagExplicitPort_;
};

#endif // HTTPDAEMON_SERVERNAME_H
