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


#ifndef _HTTP_SERVER_
#define _HTTP_SERVER_

#include <stdlib.h>
#include <prnetdb.h>
#include <prio.h>
#include "support/SimpleHash.h"
#include "ptrlist.h"

#ifdef WIN32
#define __EXPORT __declspec(dllexport)
#else
#define __EXPORT
#endif

class HttpRequest;

class __EXPORT HttpServer
{
public:
    HttpServer(const char *addr, PRUint16 af);
    ~HttpServer();

    const char *getAddrString() const;
    const PRNetAddr * getAddr() const;
    void setSSL(PRBool SSLstate);
    PRBool isSSL() const;

    // put a file on the server of size bytes
    PRBool putFile(const char *uri, int size) const;
    PRBool putFile(const char* uri, const char* localFile) const;

private:
    char *_addr;
    PRNetAddr *_netAddr;
    PRBool SSLOn;
    PRBool _putFile(HttpRequest& rq) const;
};

typedef __EXPORT enum HttpProtocol_e { HTTPNA    = 0x0, 
                                       HTTP09    = 0x1, 
                                       HTTP10    = 0x2, 
                                       HTTP11    = 0x4, 
                                       HTTPBOGUS = 0x8 } HttpProtocol;

#define NUM_PROTOS 5 // needed for arrays of tests

__EXPORT char *HttpProtocolToString(HttpProtocol);

class HttpMessage
{
    public:
        HttpMessage(long len = 0, const char* buf = NULL);
        ~HttpMessage();

        PRBool          operator == (const HttpMessage& rhs);

        void addData(long len, const void* buf);

        // set data on the message
        void            setProtocol(HttpProtocol prot);
        PRBool          addHeader(const char *name, const char *value);

        // get data about the message
        HttpProtocol    getProtocol() const;
        const char*     getHeader(const char *name);


    protected:
        char*               firstline; // first line - may be the request-line or server status
        HttpProtocol        proto;
        SimpleStringHash    headers;
        long                cl;
};

class SecurityProtocols
{
    public:
        SecurityProtocols(PRBool s2 = PR_TRUE, PRBool s3 = PR_TRUE, PRBool t = PR_TRUE);
        const SecurityProtocols& operator = (const SecurityProtocols& rhs);
        const SecurityProtocols& operator = (const PtrList<char>& protlist);

        PRBool ssl2, ssl3, tls;
};

#endif
