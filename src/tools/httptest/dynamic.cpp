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
 * DYNAMIC HTTP TESTS
 * These test dynamic content generators for HTTP (WAI, CGI, Servlets, etc)
 *
 * 1. GET requests
 * 1A. GET
 * 1B. GET w/ query
 * 1C. GET w/ headers
 * 1D. GET w/ long headers
 * 1E. GET w/ 1000's of headers
 *
 * URL encoding?
 *
 *
 *
 */

#include "engine.h"
#include "testthread.h"
#include "log.h"
#include "nstime/nstime.h"
#include "NsprWrap/ThreadSafe.h"

#ifdef WIN32
#define strncasecmp(s,d,l) strnicmp(s,d,l)
#endif

void
dynamic_1A(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, arg->uri, arg->proto);
    HttpResponse *response;

    arg->returnVal = 0;

    request.setExpectDynamicBody();
    response = engine.makeRequest(request);

    if (response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
}

#define TEST_QUERY "this+is+a+really+really+really+long+query_string"

void
dynamic_1B(TestArg *arg)
{
    char *uri;
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    uri = (char *)malloc(sizeof(TEST_QUERY) + strlen(arg->uri) + 2);
    sprintf(uri, "%s?%s", arg->uri, TEST_QUERY);

    request = new HttpRequest(arg->server, uri, arg->proto);
    request->setExpectDynamicBody();
    response = engine.makeRequest(*request);

    if (response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    char *query = response->getDynamicResponse().lookupValue("BASIC", "queryString");
    if (!query || strcmp(query, TEST_QUERY)) {
        Logger::logError(LOGERROR, "query string mismatch! (\"%s\")", query);
        arg->returnVal = -1;
    }

    free(uri);
    delete request;
    delete response;
}

#define HEADER1 "uniqueheader1"
#define HEADER2 "uniqueheader2"
#define HEADER3 "uniqueheader3"
#define HEADER4 "uniqueheader4"
#define VALUE1 "value1"
#define VALUE2 "value2"
#define VALUE3 "value3"
#define VALUE4 "value4"

void
dynamic_1C(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, arg->uri, arg->proto);
    HttpResponse *response;

    arg->returnVal = 0;

    request.addHeader(HEADER1, VALUE1);
    request.addHeader(HEADER2, VALUE2);
    request.addHeader(HEADER3, VALUE3);
    request.addHeader(HEADER4, VALUE4);
    request.setExpectDynamicBody();
    response = engine.makeRequest(request);

    if (response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    char *header = response->getDynamicResponse().lookupValue("REQUEST HEADERS", HEADER1);
    if (!header || strcmp(header, VALUE1)) {
        Logger::logError(LOGERROR, "header1 mismatch! (\"%s\")", header);
        arg->returnVal = -1;
    }

    header = response->getDynamicResponse().lookupValue("REQUEST HEADERS", HEADER2);
    if (!header || strcmp(header, VALUE2)) {
        Logger::logError(LOGERROR, "header2 mismatch! (\"%s\")", header);
        arg->returnVal = -1;
    }

    header = response->getDynamicResponse().lookupValue("REQUEST HEADERS", HEADER3);
    if (!header || strcmp(header, VALUE3)) {
        Logger::logError(LOGERROR, "header3 mismatch! (\"%s\")", header);
        arg->returnVal = -1;
    }

    header = response->getDynamicResponse().lookupValue("REQUEST HEADERS", HEADER4);
    if (!header || strcmp(header, VALUE4)) {
        Logger::logError(LOGERROR, "header4 mismatch! (\"%s\")", header);
        arg->returnVal = -1;
    }

    delete response;
}

void
dynamic_1D(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, arg->uri, arg->proto);
    HttpResponse *response;
    char header[1024], value[1024];

    arg->returnVal = 0;

    memset(header, 'X', 1024);
    header[1024-1] = '\0';
    memset(value, 'Y', 1024);
    value[1024-1] = '\0';

    request.addHeader(header,value);
    request.setExpectDynamicBody();
    response = engine.makeRequest(request);

    if (response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    char *rvheader = response->getDynamicResponse().lookupValue("REQUEST HEADERS", header);
    if (!rvheader || strcmp(rvheader, value)) {
fprintf(stdout, "strlen(rvheader) is %d\n", strlen(rvheader));
fprintf(stdout, "strlen(header) is %d\n", strlen(header));
        Logger::logError(LOGERROR, "long header mismatch! (\"%s\")", rvheader);
        arg->returnVal = -1;
    }

    delete response;
}

void
dynamic_1E(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, arg->uri, arg->proto);
    HttpResponse *response;

    arg->returnVal = 0;

    request.setExpectDynamicBody();
    response = engine.makeRequest(request);

    if (response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
}

test_t dynamic1A = {HTTP10|HTTP11, "DYNAMIC1A- GET", dynamic_1A};
test_t dynamic1B = {HTTP10|HTTP11, "DYNAMIC1B- GET w/ query", dynamic_1B};
test_t dynamic1C = {HTTP10|HTTP11, "DYNAMIC1C- GET w/ headers", dynamic_1C};
test_t dynamic1D = {HTTP10|HTTP11, "DYNAMIC1D- GET w/ long headers", dynamic_1D};
test_t dynamic1E = {HTTP10|HTTP11, "DYNAMIC1E- GET w/ 1000's of headers", dynamic_1E};
