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
 * BASIC HTTP TESTS
 * These test basic HTTP functionality
 *
 * 1. GET requests
 * 1A. GET small file
 * 1B. GET large file
 * 1C. GET zero length file
 *
 * 2. If-modified-since
 * 2A. Exact modified date
 * 2B. Later modified date
 * 2C. Earlier modified date
 *
 * 3. Byte Ranges for a 256 byte file
 * 3A. Byte Range 0-127
 * 3B. Byte Range 100-200
 * 3C. Byte Range 128-255
 * 3D. Byte Range -128
 * 3E. Byte Range 0-0, -1 (first and last bytes)
 * 3F. Byte Range -256
 * 3G. Byte Range 512-800
 * 3H. Byte Range 0-9,10-19,30-39,100-200,-250
 * *** Add PUT w/ byte ranges tests
 * *** Add checking to request for valid multipart/byteranges responses
 * 
 * 4. Connection header
 * 4A. connection: close
 * 4B. connection: keep-alive
 * 4C. no connection header
 *
 * 5. PUT requests
 * 5A. PUT zero length file
 * 5B. PUT small file
 * 5C. PUT large file
 * 5D. PUT on top of existing file
 * 5E. PUT to several directories deep
 *
 */

#include "engine.h"
#include "log.h"
#include <time/nstime.h>
#include "NsprWrap/ThreadSafe.h"
#include "tests.h"

#define SMALLFILE "/upload/basic/small.html"
#define SMALLSIZE 1024
#define LARGEFILE "/upload/basic/large.html"
#define LARGESIZE 1024000
#define BYTEFILE "/upload/basic/b256.html"
#define BYTESIZE 256
#define ZEROFILE "/upload/basic/zero.html"
#define ZEROSIZE 0
#define DEEPFILE "/upload/basic/deep/deep/deep/deep/deep/deep.html"
#define DEEPSIZE 32000

#ifdef WIN32
#define strncasecmp(s,d,l) strnicmp(s,d,l)
#endif

class Test1A: public NetscapeTest
{
	public:
		Test1A(const char* tname = "BASIC1A- GET small file",
			const char* filename = SMALLFILE,
			const unsigned long filesize = SMALLSIZE)
			: NetscapeTest(tname, HTTP10 | HTTP11), fname(filename), fsize(filesize) { };

		virtual PRBool setup()
		{
			if (!instance)
				return PR_FALSE;

			const HttpServer& server = instance->myServer();
			// first upload the file
			ready = server.putFile(fname, fsize);
			return ready;
		};

		virtual PRBool run()
		{
			if (!instance)
				return PR_FALSE;

			const HttpServer& server = instance->myServer();
/*			// first upload the file
			server.putFile(fname, fsize); */

			// simple GET
			request = new HttpRequest(&server, fname, instance->myProtocol(), Engine::globaltimeout);

			request->setExpectedResponseLength(fsize);
			request->setExpectStandardBody();
			HttpEngine engine;
			response = engine.makeRequest(*request, server);

			if (response)
			{
				if (response->getStatus() != 200)
					Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
				else
					instance->setStatus(PR_SUCCESS);	// success

				delete response;
				response = NULL;
			};
			return PR_TRUE;
		};

	protected:
		const char* fname;
		const unsigned long fsize;
};

enableTest<Test1A> test1a;

class Test1B: public Test1A
{
	public:
		Test1B() : Test1A("BASIC1B- GET large file", LARGEFILE, LARGESIZE) {};
};

enableTest<Test1B> test1b;

class Test1C: public Test1A
{
	public:
		Test1C() : Test1A("BASIC1C- GET zero length file", ZEROFILE, ZEROSIZE) {};
};

enableTest<Test1C> test1c;

class Test2A: public NetscapeTest
{
	public:
		Test2A(char* desc = "BASIC2A- Exact modified date", long prot = HTTP10 | HTTP11) : NetscapeTest(desc, prot)
		{
			expected = PR_TRUE;
			lastModified = NULL;
		};

	protected:

		PRBool expected;
		char* lastModified;

		void lastmodified()
		{
			if (!instance)
				return;

			// retrieve the file and save the last modified date
			char *lm=NULL;

			request = new HttpRequest(&instance->myServer(), SMALLFILE, instance->myProtocol(), Engine::globaltimeout);
			HttpEngine engine;
			response = engine.makeRequest(*request, instance->myServer());

			if (response && (response->getStatus() != 200))
				Logger::logError(LOGERROR, "server could not get %s (%d)", 
					SMALLFILE, response->getStatus());

			if (response)
			{
				lm = response->getHeader("last-modified");
				delete response;
				response = NULL;
			};

			if (!lm)
				Logger::logError(LOGWARNING, "server did not respond with last-modified header");
			else 
				lastModified = strdup(lm);

			delete request;
			request = NULL;
		};

		void cause304()
		{
			if (!instance)
				return;

			// try to cause a 304 request
			if (lastModified)
			{
				request = new HttpRequest(&instance->myServer(), SMALLFILE, instance->myProtocol(), Engine::globaltimeout);

				request->addHeader("If-modified-since", lastModified);
				HttpEngine engine;
				response = engine.makeRequest(*request, instance->myServer());

				if (response)
				{
					if  (PR_TRUE == expected)
					{
						if (response->getStatus() != 304)
							Logger::logError(LOGERROR, "server did not respond with 304 response (%d)", response->getStatus());
						else
							instance->setStatus(PR_SUCCESS);	// success
					}
					else
					{
						if (response->getStatus() != 200)
							Logger::logError(LOGERROR, "server did not respond with 200 response (%d)", response->getStatus());
						else
							instance->setStatus(PR_SUCCESS);	// success
					}

					delete response;
					response=NULL;
				};

				delete request;
				request=NULL;
			};
		};

		virtual void setlastmod()
		{
		};

		virtual PRBool run()
		{
			// 304 with exact date
			// retrieve the file and save the last modified date
			lastmodified();

			// change last modified
			setlastmod();

			// try to cause a 304 request
			cause304();

			return PR_TRUE;
		};

		void update_stamp(char*& stamp, int change) const
		{
			if (!stamp)
				return;

			time_t lastModifiedStamp;
			char newStamp[64];
			struct tm tm;
			lastModifiedStamp = time_parse_string(stamp, 0);
			lastModifiedStamp += change;
			ThreadSafe::gmtime(&lastModifiedStamp, &tm);
			ThreadSafe::strftime(newStamp, "%a, %d %b %Y %H:%M:%S GMT", tm);
			free((void*)stamp);
			stamp = strdup(newStamp);
		};

};

enableTest<Test2A> test2a;

class Test2B: public Test2A
{
	public:
		Test2B() : Test2A("BASIC2B- Later modified date", HTTP10 | HTTP11) {};

		virtual void setlastmod()
		{
			if (lastModified)
				update_stamp(lastModified, 3600);						// increment by an hour
		};
};

enableTest<Test2B> test2b;

class Test2C: public Test2A
{
	public:
		Test2C() : Test2A("BASIC2C- Earlier modified date", HTTP10 | HTTP11) {};

		virtual void setlastmod()
		{
			if (lastModified)
			{
				update_stamp(lastModified, -3600);						// decrement by an hour
				expected = PR_FALSE;
			};
		};
};

enableTest<Test2C> test2c;

class Test3A: public NetscapeTest
{
	public:
		Test3A() : NetscapeTest("BASIC3A- Byte Range 0-127", HTTP10 | HTTP11) {};

		virtual PRBool run()
		{
			PRBool supportsRanges = PR_FALSE;

			if (!instance)
				return PR_FALSE;

			// put the file on the server
			instance->myServer().putFile(BYTEFILE, BYTESIZE);

			// retrieve the file 
			{
				request = new HttpRequest(&instance->myServer(), BYTEFILE, instance->myProtocol(), Engine::globaltimeout);
				request->addHeader("range", "bytes=0-127");
				request->setExpectedResponseLength(128);
				request->setExpectStandardBody();
				HttpEngine engine;
				response = engine.makeRequest(*request, instance->myServer());

				if (response && (response->getStatus() == 206))
					instance->setStatus(PR_SUCCESS);	// success

				else

					if (response)
					{
						if (response->getStatus() == 200)
						{
							Logger::logError(LOGWARNING, "server does not support byte ranges");
						}
						else
						{
							Logger::logError(LOGERROR, "server could not get %s (%d)", 
								BYTEFILE, response->getStatus());
						};

						char *contentRangeHeader = response->getHeader("Content-range");
						if (!contentRangeHeader)
							Logger::logError(LOGERROR, "server did not send content range header");
					};

				delete request;
				request = NULL;
				delete response;
				response=NULL;
			};
			return PR_TRUE;
		};
};

enableTest<Test3A> test3a;

/*
void basic_3B(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;
    PRBool supportsRanges = PR_FALSE;

    arg->returnVal = 0;

    // put the file on the server
    arg->server->putFile(BYTEFILE, BYTESIZE);

    // retrieve the file 
    {
        char *lm=NULL;

        request = new HttpRequest(arg->server, BYTEFILE, arg->proto);
        request->addHeader("range", "bytes=100-200");
        request->setExpectedResponseLength(101);
        response = engine.makeRequest(*request);

        if (response && (response->getStatus() != 206)) {
            if (response->getStatus() == 200) {
                Logger::logError(LOGWARNING, "server does not support byte ranges");
            } else {
                Logger::logError(LOGERROR, "server could not get %s (%d)", 
                    BYTEFILE, response->getStatus());
                arg->returnVal = -1;
            }
        } else {
            supportsRanges = PR_TRUE;
        }

		if (response)
		{
			char *contentRangeHeader = response->getHeader("Content-range");
			if (!contentRangeHeader)
				Logger::logError(LOGERROR, "server did not send content range header");
		};
       
        delete request;
        delete response;
    }
}

void
basic_3C(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;
    PRBool supportsRanges = PR_FALSE;

    arg->returnVal = 0;

    // put the file on the server
    arg->server->putFile(BYTEFILE, BYTESIZE);

    // retrieve the file 
    {
        char *lm=NULL;

        request = new HttpRequest(arg->server, BYTEFILE, arg->proto);
        request->addHeader("range", "bytes=128-255");
        request->setExpectedResponseLength(128);
        response = engine.makeRequest(*request);

        if (response && (response->getStatus() != 206)) {
            if (response->getStatus() == 200) {
                Logger::logError(LOGWARNING, "server does not support byte ranges");
            } else {
                Logger::logError(LOGERROR, "server could not get %s (%d)", 
                    BYTEFILE, response->getStatus());
                arg->returnVal = -1;
            }
        } else {
            supportsRanges = PR_TRUE;
        }

		if (response)
		{
			char *contentRangeHeader = response->getHeader("Content-range");
			if (!contentRangeHeader)
				Logger::logError(LOGERROR, "server did not send content range header");
		};
       
        delete request;
        delete response;
    }
}

void
basic_3D(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;
    PRBool supportsRanges = PR_FALSE;

    arg->returnVal = 0;

    // put the file on the server
    arg->server->putFile(BYTEFILE, BYTESIZE);

    // retrieve the file 
    {
        request = new HttpRequest(arg->server, BYTEFILE, arg->proto);
        request->addHeader("range", "bytes=-128");
        request->setExpectedResponseLength(128);
        response = engine.makeRequest(*request);

        if (response && (response->getStatus() != 206)) {
            if (response->getStatus() == 200) {
                Logger::logError(LOGWARNING, "server does not support byte ranges");
            } else {
                Logger::logError(LOGERROR, "server could not get %s (%d)", 
                    BYTEFILE, response->getStatus());
                arg->returnVal = -1;
            }
        } else {
            supportsRanges = PR_TRUE;
        }

		if (response)
		{
			char *contentRangeHeader = response->getHeader("Content-range");
			if (!contentRangeHeader)
				Logger::logError(LOGERROR, "server did not send content range header");
		};
       
        delete request;
        delete response;
    }
}

void
basic_3E(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;
    PRBool supportsRanges = PR_FALSE;

    arg->returnVal = 0;

    // put the file on the server
    arg->server->putFile(BYTEFILE, BYTESIZE);

    // retrieve the file 
    {
        char *lm=NULL;

        request = new HttpRequest(arg->server, BYTEFILE, arg->proto);
        request->addHeader("range", "bytes=0-0,-1");
        response = engine.makeRequest(*request);

        if (response && response->getStatus() != 206) {
            if (response->getStatus() == 200) {
                Logger::logError(LOGWARNING, "server does not support byte ranges");
            } else {
                Logger::logError(LOGERROR, "server could not get %s (%d)", 
                    BYTEFILE, response->getStatus());
                arg->returnVal = -1;
            }
        } else {
            supportsRanges = PR_TRUE;
        }

		if (response)
		{
			char *contentTypeHeader = response->getHeader("Content-type");
			if (contentTypeHeader) {
				if (strncasecmp(contentTypeHeader, "multipart/byteranges", 20))
					Logger::logError(LOGERROR, "server did not send multipart/byteranges response (%s)", 
						contentTypeHeader);
			} else {
				Logger::logError(LOGERROR, "server did not send content range header");
			}
		};
       
        delete request;
        delete response;
    }
}

void
basic_3F(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;
    PRBool supportsRanges = PR_FALSE;

    arg->returnVal = 0;

    // put the file on the server
    arg->server->putFile(BYTEFILE, BYTESIZE);

    // retrieve the file 
    {
        request = new HttpRequest(arg->server, BYTEFILE, arg->proto);
        request->addHeader("range", "bytes=-256");
        request->setExpectedResponseLength(256);
        request->setExpectStandardBody();
        response = engine.makeRequest(*request);

        if (response && response->getStatus() != 206) {
            if (response->getStatus() == 200) {
                Logger::logError(LOGWARNING, "server does not support byte ranges");
            } else {
                Logger::logError(LOGERROR, "server could not get %s (%d)", 
                    BYTEFILE, response->getStatus());
                arg->returnVal = -1;
            }
        } else {
            supportsRanges = PR_TRUE;
        }

		if (response)
		{
			char *contentRangeHeader = response->getHeader("Content-range");
			if (!contentRangeHeader)
				Logger::logError(LOGERROR, "server did not send content range header");
		};
       
        delete request;
        delete response;
    }
}

void
basic_3G(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;
    PRBool supportsRanges = PR_FALSE;

    arg->returnVal = 0;

    // put the file on the server
    arg->server->putFile(BYTEFILE, BYTESIZE);

    // retrieve the file 
    {
        request = new HttpRequest(arg->server, BYTEFILE, arg->proto);
        request->addHeader("range", "bytes=512-800");
        response = engine.makeRequest(*request);

        // don't know what to check for 

        delete request;
        delete response;
    }
}

void
basic_3H(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;
    PRBool supportsRanges = PR_FALSE;

    arg->returnVal = 0;

    // put the file on the server
    arg->server->putFile(BYTEFILE, BYTESIZE);

    // retrieve the file 
    {
        request = new HttpRequest(arg->server, BYTEFILE, arg->proto);
        request->addHeader("range", "bytes=0-9,10-19,30-39,100-200,-250");
        response = engine.makeRequest(*request);

        if (response && response->getStatus() != 206) {
            if (response->getStatus() == 200) {
                Logger::logError(LOGWARNING, "server does not support byte ranges");
            } else {
                Logger::logError(LOGERROR, "server could not get %s (%d)", 
                    BYTEFILE, response->getStatus());
                arg->returnVal = -1;
            }
        } else {
            supportsRanges = PR_TRUE;
        }

        // for multipart requests, don't expect a content-range header
		if (response)
		{
			char *contentRangeHeader = response->getHeader("Content-range");
			if (contentRangeHeader)
				Logger::logError(LOGERROR, "server sent content range header");
		};
       
        delete request;
        delete response;
    }
}

void
basic_4A(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    // retrieve the file 
    {
        request = new HttpRequest(arg->server, SMALLFILE, arg->proto);
        request->addHeader("connection", "close");
        response = engine.makeRequest(*request);

        if (response && response->getStatus() != 200) {
            Logger::logError(LOGERROR, "server could not get %s (%d)", 
                SMALLFILE, response->getStatus());
            arg->returnVal = -1;
        } 

        if (response && response->checkConnection() == PR_TRUE) {
            Logger::logError(LOGERROR, "server ignored \"connection: close\" header");
            arg->returnVal = -1;
        }
       
        delete request;
        delete response;
    }
}

void
basic_4B(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    // retrieve the file 
    {
        request = new HttpRequest(arg->server, SMALLFILE, arg->proto);
        request->addHeader("connection", "keep-alive");
        response = engine.makeRequest(*request);

        if (response && response->getStatus() != 200) {
            Logger::logError(LOGERROR, "server could not get %s (%d)", 
                SMALLFILE, response->getStatus());
            arg->returnVal = -1;
        } 

        // its really impossible to detect whether or not the server obeyed
        // hopefully the connection is open, but it doesn't have to be.  
        if (response && response->checkConnection() == PR_FALSE)
            Logger::logError(LOGWARNING, "server ignored \"connection: keep-alive\" header");
        
        delete request;
        delete response;
    }
}
void
basic_4C(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest *request;
    HttpResponse *response;

    arg->returnVal = 0;

    // retrieve the file 
    {
        request = new HttpRequest(arg->server, SMALLFILE, arg->proto);
        response = engine.makeRequest(*request);

        if (response && response->getStatus() != 200) {
            Logger::logError(LOGERROR, "server could not get %s (%d)", 
                SMALLFILE, response->getStatus());
            arg->returnVal = -1;
        } 

        if (response && response->checkConnection() == PR_TRUE)
            Logger::logError(LOGERROR, "server incorrectly kept connection open");
        
        delete request;
        delete response;
    }
}

void
basic_5A(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, ZEROFILE, arg->proto);
    HttpResponse *response;
    int status;

    arg->returnVal = 0;

    request.setMethod("PUT");
    request.addRandomBody(ZEROSIZE);
    response = engine.makeRequest(request);

	if (response)
	{
		status = response->getStatus();
		if (status != 200 && status != 201 && status != 204) {
			Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
			arg->returnVal = -1;
		}
	};

    delete response;
}

void
basic_5B(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, SMALLFILE, arg->proto);
    HttpResponse *response;
    int status;

    arg->returnVal = 0;

    request.setMethod("PUT");
    request.addRandomBody(SMALLSIZE);
    response = engine.makeRequest(request);

	if (response)
	{

		status = response->getStatus();
		if (status != 200 && status != 201 && status != 204) {
			Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
			arg->returnVal = -1;
		}
	};

    delete response;
}

void
basic_5C(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, LARGEFILE, arg->proto);
    HttpResponse *response;
    int status;

    arg->returnVal = 0;

    request.setMethod("PUT");
    request.addRandomBody(LARGESIZE);
    response = engine.makeRequest(request);

	if (response)
	{
		status = response->getStatus();
		if (status != 200 && status != 201 && status != 204) {
			Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
			arg->returnVal = -1;
		}
	};

    delete response;
}

void
basic_5D(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, SMALLFILE, arg->proto);
    HttpResponse *response;
    int status;

    arg->returnVal = 0;

    request.setMethod("PUT");
    request.addRandomBody(SMALLSIZE);
    response = engine.makeRequest(request);

	if (response)
	{
		status = response->getStatus();
		if (status != 200 && status != 201 && status != 204) {
			Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
			arg->returnVal = -1;
		}
	};

    delete response;
}

void
basic_5E(TestArg *arg)
{
    HttpEngine engine;
    HttpRequest request(arg->server, DEEPFILE, arg->proto);
    HttpResponse *response;
    int status;

    arg->returnVal = 0;

    request.setMethod("PUT");
    request.addRandomBody(DEEPSIZE);
    response = engine.makeRequest(request);

	if (response)
	{
		status = response->getStatus();
		if (status != 200 && status != 201 && status != 204) {
			Logger::logError(LOGERROR, "server responded with status code %d", response->getStatus());
			arg->returnVal = -1;
		};
	};

    delete response;
}

test_t basic3A = {HTTP10|HTTP11, "BASIC3A- Byte Range 0-127", basic_3A};
test_t basic3B = {HTTP10|HTTP11, "BASIC3B- Byte Range 100-200", basic_3B};
test_t basic3C = {HTTP10|HTTP11, "BASIC3C- Byte Range 128-255", basic_3C};
test_t basic3D = {HTTP10|HTTP11, "BASIC3D- Byte Range -128", basic_3D};
test_t basic3E = {HTTP10|HTTP11, "BASIC3E- Byte Range 0-0,-1", basic_3E};
test_t basic3F = {HTTP10|HTTP11, "BASIC3F- Byte Range -256", basic_3F};
test_t basic3G = {HTTP10|HTTP11, "BASIC3G- Byte Range 512-800", basic_3G};
test_t basic3H = {HTTP10|HTTP11, "BASIC3H- Byte Range 0-9,10-19,30-39,100-200,-250", basic_3H};
test_t basic4A = {HTTP10|HTTP11, "BASIC4A- connection: close", basic_4A};
test_t basic4B = {HTTP10|HTTP11, "BASIC4B- connection: keep-alive", basic_4B};
test_t basic4C = {HTTP10, "BASIC4C- no connection header", basic_4C};
test_t basic5A = {HTTP10|HTTP11, "BASIC5A- PUT zero length file", basic_5A};
test_t basic5B = {HTTP10|HTTP11, "BASIC5B- PUT small file", basic_5B};
test_t basic5C = {HTTP10|HTTP11, "BASIC5C- PUT large file", basic_5C};
test_t basic5D = {HTTP10|HTTP11, "BASIC5D- PUT existing", basic_5D};
test_t basic5E = {HTTP10|HTTP11, "BASIC5E- PUT several directories", basic_5E}; */
