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


#ifndef WIN32
#include <string.h>
#endif

#include "http.h"
#include "engine.h"
#include "request.h"
#include "response.h"
#include "utils.h"

HttpServer::HttpServer(const char *addr, PRUint16 af)
{
    SSLOn = PR_FALSE;
    int port = 80;
    char *pPort;

    _netAddr = (PRNetAddr *)malloc(sizeof(PRNetAddr));

    _addr = NULL;
    if (addr)
    {
        _addr = strdup(addr); 
    };
    
    /* HACK for IPv6 addresses, whatever is specified 
     * after the last colon is the port
     */
    pPort = strrchr(_addr, ':');
    if (pPort)
	{
        *pPort = 0;
        port = atoi(++pPort);
    };

    /* kludge for doing IPv6 tests on localhost */
    if (!strcmp(_addr, "ip6-localhost") && (af == PR_AF_INET6)) {
        if (_addr)
            free(_addr);
        _addr = strdup("::1");
    }

    PR_InitializeNetAddr(PR_IpAddrNull, port, _netAddr);

    if (PR_StringToNetAddr(_addr, _netAddr) ==  PR_FAILURE)
    {
        char buf[256];
        PRHostEnt ent;

        if (PR_GetIPNodeByName(_addr, af, PR_AI_DEFAULT, buf, sizeof(buf), &ent) == PR_SUCCESS)
        {
            PR_EnumerateHostEnt(0, &ent, port, _netAddr);
        } else {
            printerror("PR_GetIPNodeByName");
            exit;
        }
    }
};

HttpServer::~HttpServer()
{
    if (_addr)
        free(_addr);
    if (_netAddr)
        free(_netAddr);
};

void HttpServer::setSSL(PRBool SSLstate)
{
  SSLOn = SSLstate;
};

PRBool HttpServer::isSSL() const
{
  return SSLOn;
};

const char * HttpServer::getAddrString() const
{
    return _addr;
};

const PRNetAddr * HttpServer::getAddr() const
{
    return _netAddr;
};

char *HttpProtocolToString(HttpProtocol proto)
{
    switch(proto)
	{
        case HTTP09:
            return "";
        case HTTP10:
            return "HTTP/1.0";
        case HTTP11:
            return "HTTP/1.1";
        case HTTPBOGUS:
            return "BOGO-PROTO";
    };

    return NULL;
};

HttpMessage :: HttpMessage(long len, const char* buf) : headers(16)
{
    firstline = NULL;
    cl = 0;
    proto = HTTPNA;

    // search for the first line
    int counter=0;
    PRBool found = PR_FALSE;
    while ( ( (counter++<len) && (PR_FALSE == found) ) )
    {
        if (buf[counter] != '\n')
            continue;
        found = PR_TRUE;
    };

    // extract the first line
    if (PR_TRUE == found)
    {
        firstline=new char[counter+1];
        memcpy(firstline, buf, counter);
        firstline[counter] = '\0';
    };
};

HttpMessage :: ~HttpMessage()
{
    if (firstline)
        delete firstline;
};

SecurityProtocols :: SecurityProtocols(PRBool s2, PRBool s3, PRBool t)
{
    ssl2 = s2;
    ssl3 = s3;
    tls = t;
};

const SecurityProtocols& SecurityProtocols :: operator = (const PtrList<char>& protlist)
{
    ssl2 = PR_FALSE;
    ssl3 = PR_FALSE;
    tls = PR_FALSE;
    PRInt32 i;
    for (i = 0;i<protlist.entries();i++)
    {
        if (0 == strcmp(protlist.at(i), "SSL2"))
        {
            ssl2 = PR_TRUE;
        };
        if (0 == strcmp(protlist.at(i), "SSL3"))
        {
            ssl3 = PR_TRUE;
        };
        if (0 == strcmp(protlist.at(i), "TLS"))
        {
            tls = PR_TRUE;
        };
    };
    return *this;
};

const SecurityProtocols& SecurityProtocols :: operator = (const SecurityProtocols& rhs)
{
    ssl2 = rhs.ssl2;
    ssl3 = rhs.ssl3;
    tls = rhs.tls;
    return *this;
};

PRBool HttpServer::putFile(const char* localFile, const char* remoteUri) const
{
    HttpRequest request(this, remoteUri, HTTP10, Engine::globaltimeout);
    request.setMethod("PUT");
    request.useLocalFileAsBody(localFile);

    PRBool rv = _putFile(request);
    return rv;
};

PRBool HttpServer::putFile(const char *uri, int size) const
{
    HttpRequest request(this, uri, HTTP10, Engine::globaltimeout);
    request.setMethod("PUT");
    request.addRandomBody(size);

    PRBool rv = _putFile(request);;
    return rv;
};

PRBool HttpServer::_putFile(HttpRequest& request) const
{
    HttpEngine engine;
    PRBool rv = PR_TRUE;

    HttpResponse* response = engine.makeRequest(request, *this);

    if (response)
      {
        int status = response->getStatus();
        if (status == 200 || status == 201 || status == 204)
            rv = PR_TRUE;
        else
            rv = PR_FALSE;

        delete response;
    }
      else
        rv = PR_FALSE;

    return rv;
};

