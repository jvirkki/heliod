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

#ifndef FCGIPARSER_H
#define FCGIPARSER_H

#include <nspr.h>
#include "fastcgi.h"


class FcgiRequest;
class CircularBuffer;

class FcgiParser {
public:
    FcgiParser(FcgiRequest *req, PRUint8 role);
    void reset();
    PRStatus formatRequest(CircularBuffer& to, CircularBuffer& from, PRUint8 streamType = FCGI_STDIN);
    PRStatus parse(CircularBuffer& from, CircularBuffer& to);
    PluginError getLastError() { return lastError; }
    NSString getServerError() { return serverError; }
    PRStatus parseHttpHeader(CircularBuffer& to);
    PRStatus makePacketHeader(int type, int len, CircularBuffer& buf, PRBool managementRecord=PR_FALSE);

    PRBool waitingForDataParse;
    PRUint32 exitStatus;
    PRBool exitStatusSet;
    PRBool authorized;  //used incase of Authorizer app

private:
    PRStatus parseAuthHeaders(pblock *pb);

    PRBool waitingForBeginRequest;
    PRBool waitingForEndRequest;
    PluginError lastError;
    FCGI_Header header;
    FCGI_EndRequestBody endRequestBody;
    PRUint64 dataLen;
    NSString serverError;
    NSString httpHeader;
    FcgiRequest *request;
    PRUint8 fcgiRole;
    PRUint32 authHeaderPrefixLen;

};
#endif //FCGIPARSER_H
