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

#include "request.h"
#include "log.h"
#include "support/NSString.h"
#include "utils.h"
#include "engine.h"

HttpRequest::HttpRequest(const HttpServer* server, const char *uri, HttpProtocol prot, PRIntervalTime to)
: _headers(8), Request(server)
{
    timeout = to;
    _method = strdup("GET");
    _uri = strdup(uri);
    _proto = prot;
    _bodyLength = -1;
    _expectedResponseLength = -1;
    _expectStandardBody = PR_FALSE;
    _expectDynamicBody = PR_FALSE;
    _hangupOk = PR_FALSE;
	_fileFd = NULL;
};

HttpRequest::~HttpRequest()
{
    if (_method)
        free(_method);

    if (_uri)
        free(_uri);

    _headers.removeAll();

    if (_fileFd)
    {
        PR_Close(_fileFd);
        _fileFd = NULL;
    };
};

PRBool HttpRequest::setMethod(char *method)
{
    free(_method);
    _method = strdup(method);
    return PR_TRUE;
};

void HttpRequest::setExpectedResponseLength(int size)
{
    _expectedResponseLength = size;
};

void HttpRequest::setExpectStandardBody()
{
    _expectStandardBody = PR_TRUE;
};

void HttpRequest::setExpectDynamicBody()
{
    _expectDynamicBody = PR_TRUE;
};

PRBool HttpRequest::getExpectStandardBody()
{
    return _expectStandardBody;
};

PRBool HttpRequest::getExpectDynamicBody()
{
    return _expectDynamicBody;
};

int HttpRequest::getExpectedResponseLength()
{
    return _expectedResponseLength;
};

char * HttpRequest::getMethod()
{
    return _method;
};

HttpProtocol HttpMessage::getProtocol() const
{
    return proto;
};

PRBool HttpMessage::addHeader(const char *name, const char *value)
{
    return headers.insert((void *)strdup(name), (void *)strdup(value));
};

const char * HttpMessage::getHeader(const char *name)
{
    return (const char*)headers.lookup((void*)name);
};

PRBool HttpRequest::addRandomBody(int size)
{
    char byteStr[12];

    sprintf(byteStr, "%d", size);
    if (!addHeader("Content-length", byteStr))
        return PR_FALSE;

    _bodyLength = size;

    return PR_TRUE;
};

PRBool HttpRequest::useLocalFileAsBody(const char* fileName)
{
    PRBool res  = PR_FALSE;
    PRFileInfo finfo;
	if (PR_GetFileInfo(fileName, &finfo) == PR_SUCCESS)
	{
	  res = PR_TRUE;
	  char byteStr[25];
	  sprintf(byteStr, "%d", finfo.size);
	  if (!addHeader("Content-length", byteStr))
		return PR_FALSE;
	  _bodyLength = finfo.size;
	  _fileFd = PR_Open(fileName, PR_RDONLY, 0);
	  if (!_fileFd)
		return PR_FALSE;
    };

    return PR_TRUE;
};

PRBool HttpRequest::send(PRFileDesc *sock)
{
    PRBool rv = PR_FALSE;
	if (!sock)
		return rv;

    NSString data;

    // set host header if needed
    if (_proto == HTTP11)
    {
        addHeader("host", _server->getAddrString());
    };

    // Send request line
    {
        data.append(_method);
        data.append(" ");
        data.append(_uri);
        data.append(" ");
        data.append(HttpProtocolToString(_proto));
        data.append("\r\n");
    };

    // Send HTTP headers
    {
        SimpleHashIterator it(&_headers);
        char *headerKey;
        char *headerValue;

        while( (headerValue = (char *)it.next()) !=  NULL )
		{
            headerKey = (char *)it.getKey();
            data.append(headerKey);
            data.append(": ");
            data.append(headerValue);
            data.append("\r\n");
        };
    };

    // Send terminator
    {
        data.append("\r\n");
    };

    if (PR_Send(sock, data, data.length(), 0, timeout) != (PRInt32) data.length())
    {
#ifdef DEBUG
        printerror("PR_Send");
#endif
        Logger::logError(LOGERROR, "Error sending request\n");
        return PR_FALSE;
    };

    // Send body
    if (_fileFd)
	{
      PRInt32 bytesSent = PR_TransmitFile(sock, _fileFd, 0, 0, 
                                          PR_TRANSMITFILE_KEEP_OPEN, 
                                          timeout);
      if (bytesSent < 0)
	  {
          Logger::logError(LOGERROR, "Error sending request\n");
          return PR_FALSE;
      };
    }
    else
	{
#define BODYBUFFER (16*1024)
        if (_bodyLength >= 0)
		{
            char body[BODYBUFFER];
            int sentBytes, bytesToSend;
            int index;

            bytesToSend = (_bodyLength > BODYBUFFER)?BODYBUFFER:_bodyLength;

            for (index=0; index<bytesToSend; index++)
			{
                    body[index] = (unsigned char)(index %256);
            };

            // send in 1k chunks
            for (; _bodyLength > 0; _bodyLength -= sentBytes)
			{
                bytesToSend = (_bodyLength > BODYBUFFER)?BODYBUFFER:_bodyLength;
                sentBytes = PR_Send(sock, body, bytesToSend, 0, timeout);
                if (sentBytes < 0)
		{
#ifdef DEBUG
    printerror("PR_Send");
#endif
                    Logger::logError(LOGERROR, "Error sending request\n");
                    return PR_FALSE;
                };
            };
        };
    };

    return PR_TRUE;
};

void HttpRequest::setHangupOk()
{
    _hangupOk = PR_TRUE;
};

PRBool HttpRequest::isHangupOk()
{
    return(_hangupOk);
};

PRBool Request::isSSL() const
{
  return SSLOn;
};

void Request::setSSL(PRBool SSLstate)
{
  SSLOn=SSLstate;
};

/* void Request :: getAddr(PRNetAddr *addr)
{
    _server->getAddr(addr);
};

const char* Request :: getAddr()
{
    return _server->getAddr();
}; */

Request :: Request(const HttpServer* server)
{
    _server = server;
    timeout = Engine::globaltimeout;
    SSLOn=PR_FALSE;
    if (server)
        SSLOn=server->isSSL();
    nickName = NULL;
    mutexName = NULL;
    handshake = PR_FALSE;
    cipherCount = 0;
    cipherSet = NULL;

};

SunRequest :: SunRequest(const HttpServer* server, const void* inbuf, const
                         PRInt32 len, PRIntervalTime to, const PRInt32* cset,
                         PRInt32 count, const char *certname,
                         const char *mutexname, const PRBool noSSL,
                         const PRBool hshake, const SecurityProtocols& secp,
                         const char *tName) : Request(server)
{
    secprots = secp;
    handshake = hshake;
    timeout = to;
    input = inbuf;
    length = len;
    cipherSet = cset;
    cipherCount = count;
    nickName = certname;
    mutexName = mutexname;
    if (noSSL)
        setSSL(PR_FALSE);
    delay = 0; // no delay between send
    split = 0; // disable packet splitting
    testName = tName;
};

SunRequest :: ~SunRequest()
{
};

PRBool SunRequest :: send(PRFileDesc *sock)
{
    PRBool rv = PR_FALSE;
    PRErrorCode error = 0;
    if (!sock)
        return rv;

    PRInt32 tosend = length;
    rv = PR_TRUE;
    PRIntervalTime tps = PR_TicksPerSecond();

    while (tosend)
    {
       PRInt32 sent = PR_Send(sock, 
			      (char *)input+length-tosend, 
			      split>0?(tosend>split?split:tosend):tosend, 
			      0, 
			      timeout
                              );
       if (sent>0)
       {
           rv = PR_TRUE;
	   tosend = tosend - sent;
	   if (delay)
	      PR_Sleep((tps*delay)/1000);
       }
       else
       {
	   rv = PR_FALSE;
#ifdef DEBUG
    printerror("PR_Send");
#endif
	   break;
       };
    };

    return rv;
};

void SunRequest :: setSplit(PRInt32 offset, PRInt32 pdelay)
{
   split = offset;
   delay = pdelay;
};

/*const HttpServer * Request :: getServer()
{
    return _server;
};

void Request :: setServer(HttpServer* server)
{
    _server = server;
};
*/

PRIntervalTime Request :: getTimeout() const
{
    return timeout;
};
