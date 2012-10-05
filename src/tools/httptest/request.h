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


#ifndef _REQUEST_H_
#define _REQUEST_H_

#include "http.h"
#include "support/SimpleHash.h"

// abstract request class
class Request
{
    public:
        Request(const HttpServer* server);
        PRBool         isSSL() const;
        void           setSSL(PRBool SSLstate);
        virtual PRBool send(PRFileDesc *sock) = 0;
        void           getAddr(PRNetAddr *addr);
        const char*    getAddr();
        const char*    getHost();
        const          HttpServer * getServer();
        void           setServer(HttpServer* _server);
        PRIntervalTime getTimeout() const;
        const PRInt32* cipherSet;
        PRInt32        cipherCount;
        PRBool         handshake;
        SecurityProtocols secprots;
        const char*    nickName;
        const char*    mutexName;
        const char*    testName;

    protected:
        PRBool SSLOn;
        const HttpServer * _server;
        PRIntervalTime timeout;
        
};

// Sun-style request
class SunRequest: public Request
{
	public:
		SunRequest(const HttpServer* server, const void* inbuf, 
                   const PRInt32 len, PRIntervalTime to, 
                   const PRInt32* cipherSet, PRInt32 cipherCount, 
                   const char *nickname, const char *mutexname,
                   const PRBool noSSL, const PRBool hshake,
                   const SecurityProtocols& secp, const char *tName);
		~SunRequest();
		virtual PRBool send(PRFileDesc *sock);
		void setSplit(PRInt32 offset, PRInt32 delay);

	protected:
		const void* input;
		PRInt32 length;
		PRInt32 delay; // delay between each PR_Send
		PRInt32 split; // offset at which to split requests
			       // into separate PR_Send
};

// Netscape-style request
class HttpRequest: public HttpMessage, public Request
{
public:
    HttpRequest(const HttpServer* server, const char *uri, HttpProtocol proto, PRIntervalTime to);
    ~HttpRequest();

    // connection related stuff

    // set data on the request
    PRBool         setMethod(char *method);
    //PRBool         addHeader(const char *name, const char *value);
    PRBool         addRandomBody(int size);
    PRBool         useLocalFileAsBody(const char* fileName);
    void           setExpectedResponseLength(int size);
    void           setExpectStandardBody();
    void           setExpectDynamicBody();
    void           setHangupOk();
    PRBool         isHangupOk();

    // get data about the request
    char          *getMethod();
    //HttpProtocol   getProtocol();
    //char          *getHeader(char *name);
    int            getExpectedResponseLength();
    PRBool         getExpectStandardBody();
    PRBool         getExpectDynamicBody();

    virtual PRBool send(PRFileDesc *sock);

private:
    char            *_method;
    char            *_uri;
    HttpProtocol     _proto;
    int              _bodyLength;
    SimpleStringHash _headers;
    int              _expectedResponseLength;
    PRBool           _expectStandardBody;
    PRBool           _expectDynamicBody;
    PRBool           _hangupOk;
    PRFileDesc*      _fileFd;
};

#endif
