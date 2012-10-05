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

/*
 * HTTP/1.1 TESTS
 * These test HTTP/1.1 functionality
 *
 * 1. OPTIONS method
 * 1A. OPTIONS *
 * 1B. OPTIONS /index.html
 *
 * 2. connection header
 * 2A. no connection header
 */

#include "engine.h"
#include "testthread.h"
#include "log.h"
#include "time/nstime.h"

#define SMALLFILE "/upload/http11/small.html"
#define SMALLSIZE 1024

#ifdef WIN32
#define strncasecmp(s,d,l) strnicmp(s,d,l)
#endif

void
http11_1A(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, "*", arg->proto, Engine::globaltimeout);
    HttpResponse *response;

    arg->returnVal = 0;

    request.setMethod("OPTIONS");
    response = engine.makeRequest(request, *arg->server);

    if (response && response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
}

void
http11_1B(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
    HttpResponse *response;

    arg->returnVal = 0;

    request.setMethod("OPTIONS");
    response = engine.makeRequest(request, *arg->server);

    if (response && response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
}

void
http11_2A(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    // retrieve the file 
    {
        request = new HttpRequest(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
        response = engine.makeRequest(*request, *arg->server);

        if (response && response->getStatus() != 200) {
            Logger::logError(LOGERROR, "server could not get %s (%d)", 
                SMALLFILE, response->getStatus());
            arg->returnVal = -1;
        } 

        if (response->checkConnection() == PR_FALSE)
            Logger::logError(LOGWARNING, "server closed connection when it shouldn't have");
        
        delete request;
        delete response;
    }
}
test_t http11t1A = {HTTP11, "HTTP11 1A- OPTIONS *", http11_1A};
test_t http11t1B = {HTTP11, "HTTP11 1B- OPTIONS /index.html", http11_1B};
test_t http11t2A = {HTTP11, "HTTP11 2A- no connection header", http11_2A};
