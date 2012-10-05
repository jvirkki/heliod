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

#ifndef FCGIROLE_H
#define FCGIROLE_H

#include <nspr.h>
#include "fastcgistub.h"


class BaseRole {
public:
    BaseRole(PRUint8 role, FcgiServer *server, FcgiRequest& req);
    ~BaseRole();
    PRStatus process();
    PluginError getLastError() { return lastError; }

protected:
    inline PRStatus process(PRBool flagRetry, PRBool flagReusePersistent);
    virtual PRStatus process(FcgiServerChannel& channel) = 0;
    PRStatus sendPreContentData(EndPoint& server);
    void makeBeginRequestBody(FCGI_BeginRequestBody *body);
    PRStatus checkForSpace(EndPoint ep, int size);
    PRStatus makeEnvParams(EndPoint& server);
    PRStatus makeBeginRequest(EndPoint& server);
    PRStatus makeAbortRequestBody(EndPoint& server);
    PRStatus sendRequestToStub(RequestMessageType reqType, PRBool retryOption);
    void buildParams( int nameLen, int valueLen,unsigned char *headerBuffPtr, int *headerLenPtr);

protected:
    PluginError lastError;
    PRUint8 fcgiRole;
    FcgiServer *fcgiServer;
    FcgiRequest& request;
    FcgiParser *parser;
};

class ResponderRole: public BaseRole {
public:
    ResponderRole(FcgiServer *srvr, FcgiRequest& req); //set role = FCGI_RESPONDER;
    ~ResponderRole() {};
    PRStatus process(FcgiServerChannel& channel);
};

class FilterRole : public BaseRole {
public :
    FilterRole(FcgiServer *srvr, FcgiRequest& req);
    ~FilterRole();
    PRStatus process(FcgiServerChannel& channel);

private:
    void initFilter();
    char *filterApp;
    SYS_FILE filterFd;
    PRFileInfo& filterFileInfo;
    PRUint32 filterFileSize;
};

class AuthRole: public BaseRole {
public:
    AuthRole(FcgiServer *srvr, FcgiRequest& req);
    ~AuthRole() {};
    PRStatus process(FcgiServerChannel& channel);
};

#endif // FCGIROLE_H
