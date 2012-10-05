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
 * NEGATIVE HTTP TESTS
 * These tests are negative tests; checking for appropriate errors and 
 * boundary conditions
 *
 * 1. Normal errors
 * 1A. Not Found Error
 * 1B. Invalid method
 * 1C. Invalid Protocol
 * 1D. Binary URL (embedded NULL)
 * 1E. Binary URL (no embedded NULL)
 * 1F. Binary Headers (binary header)
 * 1G. Binary Headers (binary value)
 * 
 * 2. Large requests
 * 2A. 8K method
 * 2B. 2K URI
 * 2C. 4K URI
 * 2D. 8K URI
 * 2E. 2K header
 * 2F. 4K header
 * 2G. 8K header
 * 2H. 8K headers
 *
 */
#include "engine.h"
#include "testthread.h"
#include "log.h"
#include "tests.h"
#include "NsprWrap/ThreadSafe.h"


#define DNEFILE "/unlikely/to/exist/on/any/server"

#define SMALLFILE "/upload/negative/small.html"
#define SMALLSIZE 3891


void
negative_1A(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, DNEFILE, arg->proto, Engine::globaltimeout);
    HttpResponse *response;

    arg->returnVal = 0;

    response = engine.makeRequest(request, *arg->server);

    if (response && response->getStatus() != 404) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
}

void
negative_1B(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
    HttpResponse *response;

    arg->returnVal = 0;

    arg->server->putFile(SMALLFILE, SMALLSIZE);

    request.setMethod("LUDICROUS");
    response = engine.makeRequest(request, *arg->server);

    if (response && response->getStatus() != 500) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
}

void
negative_1C(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, SMALLFILE, HTTPBOGUS, Engine::globaltimeout);
    HttpResponse *response;

    arg->returnVal = 0;

    response = engine.makeRequest(request, *arg->server);

    if (response && response->getStatus() != 400) { 
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
}

void
negative_1D(TestArg *arg)
{
    char binaryURL[25];
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    // This URL has embedded NULLs
    sprintf(binaryURL, "/upload/");
    binaryURL[8] = (char)1;
    binaryURL[9] = (char)1;
    binaryURL[10] = (char)2;
    binaryURL[11] = (char)0;
    binaryURL[12] = (char)100;
    binaryURL[13] = (char)244;
    binaryURL[14] = (char)255;
    binaryURL[15] = '\0';
    
    request = new HttpRequest(arg->server, binaryURL, arg->proto, Engine::globaltimeout);
    
    arg->returnVal = 0;

    response = engine.makeRequest(*request, *arg->server);

    if (response && response->getStatus() != 500 && response->getStatus() != 404) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
    delete request;
}

void
negative_1E(TestArg *arg)
{
    char binaryURL[25];
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    // This URL does not have embedded NULLs
    sprintf(binaryURL, "/upload/");
    binaryURL[8] = (char)4;
    binaryURL[9] = (char)1;
    binaryURL[10] = (char)2;
    binaryURL[11] = (char)3;
    binaryURL[12] = (char)100;
    binaryURL[13] = (char)244;
    binaryURL[14] = (char)255;
    binaryURL[15] = '\0';
    
    request = new HttpRequest(arg->server, binaryURL, arg->proto, Engine::globaltimeout);
    
    arg->returnVal = 0;

    response = engine.makeRequest(*request, *arg->server);

    if (response && response->getStatus() != 500 && response->getStatus() != 404) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
    delete request;
}


class Negative1F: public NetscapeTest
{
	public:
		Negative1F(const char* tname = "NEGATIVE1F Binary Headers (binary name)",
			const char* filename = SMALLFILE,
			const unsigned long filesize = SMALLSIZE)
			: NetscapeTest(tname, HTTP10 | HTTP11)  { };

		virtual PRBool run()
		{

                    char binaryHeader[25];
                    HttpEngine engine;
                    const HttpServer& server = instance->myServer();
                    request = new HttpRequest(&server, SMALLFILE, instance->myProtocol(), Engine::globaltimeout);

                    sprintf(binaryHeader, "BinaryHeader");
                    binaryHeader[8] = (char)4;
                    binaryHeader[9] = (char)1;
                    binaryHeader[10] = (char)2;
                    binaryHeader[11] = (char)3;
                    binaryHeader[12] = (char)100;
                    binaryHeader[13] = (char)244;
                    binaryHeader[14] = (char)255;
                    binaryHeader[15] = '\0';

                    request->addHeader(binaryHeader, "value");

                    response = engine.makeRequest(*request, server);

                    if (response && response->getStatus() != 200 && response->getStatus() != 500 && response->getStatus() != 400)
                    {
                        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
                    }
                    else
                        instance->setStatus(PR_SUCCESS);	// success

                    // XXXMB - can we check that it actually received the headers okay?
                    if (response)
                    {
                        delete response;
                        response = NULL;
                    };

                    if (request)
                    {
                        delete request;
                        request= NULL;
                    };
                    return PR_TRUE;
                };
};

enableTest<Negative1F> negative1f;

void
negative_1F(TestArg *arg)
{
    char binaryHeader[25];
    HttpEngine engine;
    HttpRequest request(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
    HttpResponse *response;

    arg->returnVal = 0;

    sprintf(binaryHeader, "BinaryHeader");
    binaryHeader[8] = (char)4;
    binaryHeader[9] = (char)1;
    binaryHeader[10] = (char)2;
    binaryHeader[11] = (char)3;
    binaryHeader[12] = (char)100;
    binaryHeader[13] = (char)244;
    binaryHeader[14] = (char)255;
    binaryHeader[15] = '\0';
    
    request.addHeader(binaryHeader, "value");

    response = engine.makeRequest(request, *arg->server);

    if (response && response->getStatus() != 200 && response->getStatus() != 500 && response->getStatus() != 400) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    // XXXMB - can we check that it actually received the headers okay?
    delete response;
}


class Negative1G: public NetscapeTest
{
	public:
		Negative1G(const char* tname = "NEGATIVE1G Binary Headers (binary value)",
			const char* filename = SMALLFILE,
			const unsigned long filesize = SMALLSIZE)
			: NetscapeTest(tname, HTTP10 | HTTP11)  { };

		virtual PRBool run()
		{
                    char binaryHeader[25];
                    HttpEngine engine;
                    const HttpServer& server = instance->myServer();
                    request = new HttpRequest(&server, SMALLFILE, instance->myProtocol(), Engine::globaltimeout);

                    sprintf(binaryHeader, "BinaryHeader");
                    binaryHeader[8] = (char)4;
                    binaryHeader[9] = (char)1;
                    binaryHeader[10] = (char)2;
                    binaryHeader[11] = (char)3;
                    binaryHeader[12] = (char)100;
                    binaryHeader[13] = (char)244;
                    binaryHeader[14] = (char)255;
                    binaryHeader[15] = '\0';

                    request->addHeader("header", binaryHeader);

                    response = engine.makeRequest(*request, server);

                    if (response && response->getStatus() != 200 && response->getStatus() != 500 && response->getStatus() != 400)
                    {
                        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
                    }
                    else
                        instance->setStatus(PR_SUCCESS);	// success

                    // XXXMB - can we check that it actually received the headers okay?
                    if (response)
                    {
                        delete response;
                        response = NULL;
                    };

                    if (request)
                    {
                        delete request;
                        request= NULL;
                    };
                    return PR_TRUE;
                };

};

enableTest<Negative1G> negative1g;


void
negative_1G(TestArg *arg)
{
    char binaryHeader[25];
    HttpEngine engine;
    HttpRequest request(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
    HttpResponse *response;

    arg->returnVal = 0;

    sprintf(binaryHeader, "BinaryHeader");
    binaryHeader[8] = (char)4;
    binaryHeader[9] = (char)1;
    binaryHeader[10] = (char)2;
    binaryHeader[11] = (char)3;
    binaryHeader[12] = (char)100;
    binaryHeader[13] = (char)244;
    binaryHeader[14] = (char)255;
    binaryHeader[15] = '\0';
    
    request.addHeader("header", binaryHeader);

    response = engine.makeRequest(request, *arg->server);

    if (response && response->getStatus() != 200 && response->getStatus() != 500 && response->getStatus() != 400) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    // XXXMB - can we check that it actually received the headers okay?

    delete response;
}

#define SIZE_1K (1*1024)
#define SIZE_2K (2*1024)
#define SIZE_4K (4*1024)
#define SIZE_8K (8*1024)
void
negative_2A(TestArg *arg)
{
    char longMethod[SIZE_8K+1];
    HttpEngine engine;
    HttpRequest request(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
    HttpResponse *response;

    arg->returnVal = 0;

    for (int index=0; index< SIZE_8K; index++)
        longMethod[index] = 'X';
    longMethod[SIZE_8K] = '\0';

    request.setMethod(longMethod);

    response = engine.makeRequest(request, *arg->server);

    if (response && response->getStatus() != 403 && response->getStatus() != 500) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
}

class Negative2B: public NetscapeTest
{
	public:
		Negative2B(const char* tname = "Negative2B - 2K URI",
			const unsigned long filesize = SIZE_2K)
			: NetscapeTest(tname, HTTP10 | HTTP11)  , urlsize(filesize)
                        { };

		virtual PRBool run()
		{
			if (!instance)
				return PR_FALSE;

                        char* longUri= (char*)malloc(urlsize+1);
                        HttpEngine engine;

                        for (unsigned long index=0; index < urlsize; index++)
                            longUri[index] = 'X';

                        longUri[0] = '/';
                        longUri[urlsize] = '\0';

                        const HttpServer& server = instance->myServer();
                        request = new HttpRequest(&server, longUri, instance->myProtocol(), Engine::globaltimeout);

                        response = engine.makeRequest(*request, server);

                        if (response && response->getStatus() != 404)
                            Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
                        else
                            instance->setStatus(PR_SUCCESS);	// success

                        if (response)
                        {
                            delete response;
                            response = NULL;
                        }
                        if (request)
                        {
                            delete request;
                            request = NULL;
                        };
                        if (longUri)
                        {
                            free(longUri);
                            longUri = NULL;
                        };

                        return PR_TRUE;
                };

        protected:
                const unsigned long urlsize;

};

enableTest<Negative2B> negative2b;

class Negative2C: public Negative2B
{
    public:
        Negative2C()
            : Negative2B("Negative2C - 4K URI", SIZE_4K) { };
};

enableTest<Negative2C> negative2c;

class Negative2D: public Negative2B
{
    public:
        Negative2D()
            : Negative2B("Negative2D - 8K URI", SIZE_8K) { };
};

enableTest<Negative2D> negative2d;

void
negative_2E(TestArg *arg)
{
    char longHeader[SIZE_2K+1];
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    for (int index=0; index< SIZE_2K; index++)
        longHeader[index] = 'X';
    longHeader[SIZE_2K] = '\0';

    arg->server->putFile(SMALLFILE, SMALLSIZE);

    request = new HttpRequest(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
   
    request->addHeader(longHeader, "value");

    response = engine.makeRequest(*request, *arg->server);

    if (response && response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
    delete request;
}

void
negative_2F(TestArg *arg)
{
    char longHeader[SIZE_4K+1];
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    arg->server->putFile(SMALLFILE, SMALLSIZE);

    for (int index=0; index< SIZE_4K; index++)
        longHeader[index] = 'X';
    longHeader[SIZE_4K - 20] = '\0';  // make it so that header + value will be less than 4k

    request = new HttpRequest(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
   
    request->addHeader(longHeader, "value");

    response = engine.makeRequest(*request, *arg->server);

    if (response && response->getStatus() != 200) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
    delete request;
}

void
negative_2G(TestArg *arg)
{
    char longHeader[SIZE_8K+1];
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    arg->server->putFile(SMALLFILE, SMALLSIZE);

    for (int index=0; index< SIZE_8K; index++)
        longHeader[index] = 'X';
    longHeader[SIZE_8K] = '\0';

    request = new HttpRequest(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);
   
    request->addHeader(longHeader, "value");
    request->setHangupOk();

    response = engine.makeRequest(*request, *arg->server);

    if (response && response->getStatus() != 400 && response->getStatus() != 500 ) {
        Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
        arg->returnVal = -1;
    }

    delete response;
    delete request;
}

void
negative_2H(TestArg *arg)
{
    char header[128];
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    arg->server->putFile(SMALLFILE, SMALLSIZE);

    request = new HttpRequest(arg->server, SMALLFILE, arg->proto, Engine::globaltimeout);

    for (int index=0; index< SIZE_8K; index++) {
        sprintf(header, "Header%d", index);
        request->addHeader(header, "value");
    }

    request->setHangupOk();
    response = engine.makeRequest(*request, *arg->server);

	if (response)
	{ 
		if (response->getStatus() != 400 && response->getStatus() != 500)
		{
			Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
			arg->returnVal = -1;
		}
	} else
	{
	// On several platforms, response comes back null, so this avoids the core
	// dump trying to getStatus.
		Logger::logError(LOGERROR, "server responded with null response %d", 0);
        arg->returnVal = -1;
	};

    delete response;
    delete request;
}

test_t negative1A = {HTTP10|HTTP11, "NEGATIVE1A Not Found Error", negative_1A};
test_t negative1B = {HTTP10|HTTP11, "NEGATIVE1B Invalid method", negative_1B};
test_t negative1C = {HTTP10|HTTP11, "NEGATIVE1C Invalid protocol", negative_1C};
test_t negative1D = {HTTP10|HTTP11, "NEGATIVE1D Binary URL (embedded NULLs)", negative_1D};
test_t negative1E = {HTTP10|HTTP11, "NEGATIVE1E Binary URL (no embedded NULLs)", negative_1E};
test_t negative1F = {HTTP10|HTTP11, "NEGATIVE1F Binary Headers (binary name)", negative_1F};
test_t negative1G = {HTTP10|HTTP11, "NEGATIVE1G Binary Headers (binary value)", negative_1G};
test_t negative2A = {HTTP10|HTTP11, "NEGATIVE2A 8K method", negative_2A};
test_t negative2E = {HTTP10|HTTP11, "NEGATIVE2E 2K header", negative_2E};
test_t negative2F = {HTTP10|HTTP11, "NEGATIVE2F 4K header", negative_2F};
test_t negative2G = {HTTP10|HTTP11, "NEGATIVE2G 8K header", negative_2G};
test_t negative2H = {HTTP10|HTTP11, "NEGATIVE2H 8K headers", negative_2H};
