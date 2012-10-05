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


#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include "http.h"
#include "request.h"
#include "support/SimpleHash.h"

class __EXPORT RecvBuf
{
public:
    RecvBuf(const PRFileDesc *socket, int size);
    ~RecvBuf();

    char getChar();
    void putBack();

    void setChunkedMode();

    class EndOfFile {};
    class EndOfChunking {};

private:
    char _getChar();
    PRBool _getBytes(int size);

    const PRFileDesc *_socket;
    int _allocSize;
    char *_buf;
    int _curPos;
    int _curSize;

    PRBool _chunkedMode;
    int _currentChunkSize;
    int _currentChunkBytesRead;
};

//
// DynamicResponse 
//
class __EXPORT DynamicResponse
{
public:
    DynamicResponse(RecvBuf &buf);
    ~DynamicResponse();

    void parse();
 
    PRBool lookupSection(char *sectionName);
    char  *lookupValue(char *sectionName, char *key);

private:
    typedef struct section {
        char *name;
        SimpleStringHash *table;
        struct section *next;
    } section;

    section *_createSection(char *name);

private:
    RecvBuf    &_buf;
    section    *_sectionList;
};

class __EXPORT Response
{
    public:
        Response(const PRFileDesc *sock, Request *request);

    protected:
        const PRFileDesc   *_socket;
        Request		 *_request;
};

class __EXPORT SunResponse: public Response
{
    public:
        SunResponse(const PRFileDesc *sock, Request *request);
        ~SunResponse();
        virtual PRBool processResponse();
        void getData(void*& buf, PRInt32& len);
        void setPerformance(PRBool perf);

    protected:
        PRBool performance;
        void* data;
        PRInt32 length;
        PRIntervalTime timeout;
};

class __EXPORT HttpResponse: public Response
{
    public:
        HttpResponse(const PRFileDesc *sock, HttpRequest *request);
        ~HttpResponse();
        virtual PRBool        processResponse();

        long          getStatus();
        char         *getStatusString();
        HttpProtocol  getProtocol();
        char         *getHeader(char *name);
        char        **getHeaders();

        PRBool        checkKeepAlive(); // return true if we *expect* keepalive based on request
        PRBool        checkConnection();  // return true if connection is open

        long          getBodyLength();

        DynamicResponse &getDynamicResponse();

    protected:
        HttpRequest   *_request;
        int           _verifyStandardBody(RecvBuf &, int, PRBool);
        int           _verifyDynamicBody(RecvBuf &, int);
        PRBool        _handleBody(RecvBuf &buf);
        void          _checkResponseSanity();

        HttpProtocol  _proto;
        char         *_protocol;
        int retcode;
        char         *_statusNum;
        char         *_statusString;

        int           _keepAlive;
        int           _connectionClosed;

        long          _bodyLength;

        PRBool        _chunkedResponse;

        SimpleStringHash _headers;

        DynamicResponse *_dynamicResponse;
};


#endif
