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


#include <ctype.h>
#include "response.h"
#include "log.h"
#include "private/pprio.h"
#include "utils.h"
#include "engine.h"

RecvBuf::RecvBuf(const PRFileDesc *socket, int size)
{
    _socket = socket;
    _allocSize = size;
    _buf = (char *)malloc(size);
    _curPos = 0;
    _curSize = 0;
    _chunkedMode = PR_FALSE;
    _currentChunkSize = _currentChunkBytesRead = 0;
};

RecvBuf::~RecvBuf()
{
	if (_buf)
		free(_buf);
};

PRBool RecvBuf::_getBytes(int size)
{
    _curPos = 0;
    _curSize = PR_Recv((PRFileDesc*)_socket, 
                 _buf, 
                 _allocSize, 
                 0, 
                 Engine::globaltimeout);
    if (_curSize <= 0)
    {
#ifdef DEBUG
        printerror("PR_Recv");
#endif
        return PR_FALSE;
    };

    Logger::logError(LOGTRACE, "RECV %s", _buf);
    return PR_TRUE;
};

char RecvBuf::_getChar()
{
    if (_curPos >= _curSize)
        if (!_getBytes(_allocSize))
            throw RecvBuf::EndOfFile();

    return _buf[_curPos++];
};

char RecvBuf::getChar()
{
    if (!_chunkedMode)
        return _getChar();
    else
	{
        if (_currentChunkSize == 0)
		{
            // read the chunk header
            char ch, chunkStr[20];
            int index = 0;
          
            while (!isspace(ch = _getChar()) )
                chunkStr[index++] = ch;
            chunkStr[index] = '\0';

            sscanf(chunkStr, "%x", &_currentChunkSize);

            if (ch != '\n')
			{
                char ch2 = _getChar();
                if (ch != '\r' || ch2 != '\n')
				{
                    Logger::logError(LOGERROR, "did not find CRLF after chunk");
                }
            }
       
            if (_currentChunkSize == 0)
                throw EndOfChunking();
 
            _currentChunkBytesRead = 1;
            return _buf[_curPos++];
        }
        else
			if (_currentChunkBytesRead < _currentChunkSize)
			{
				// read a byte from the chunk
				_currentChunkBytesRead++;
				return _getChar();
			}
			else
			{
				// read the chunk trailer
				char ch1 = _getChar();
				char ch2 = _getChar();
				if (ch1 != '\r' || ch2 != '\n')
				{
					Logger::logError(LOGERROR, "did not find CRLF after chunk");
				};
				_currentChunkSize = _currentChunkBytesRead = 0;
				return getChar();
			};
    };
};

void RecvBuf::putBack()
{
    if (_curPos > 0)
	{
        _curPos--;
        if (_chunkedMode)
            _currentChunkBytesRead--;
    }
};

void RecvBuf::setChunkedMode()
{
    _chunkedMode = PR_TRUE;
    _currentChunkSize = _currentChunkBytesRead = 0;
};

Response::Response(const PRFileDesc *sock, Request *request)
{
    _socket = sock;
    _request = request;
};

HttpResponse::HttpResponse(const PRFileDesc *sock, HttpRequest *request) :
    _headers(8), Response(sock, request)
{
    _request = request;
    _proto = HTTPNA;
    _protocol = NULL;
	 retcode =0 ;
    _statusNum = NULL;
    _statusString = NULL;
    _keepAlive = -1;
    _connectionClosed = 0;
    _bodyLength = -1;
    _chunkedResponse = PR_FALSE;
    _dynamicResponse = NULL;
    _headers.setMixCase();
};

HttpResponse::~HttpResponse()
{
    if (_protocol)
        free(_protocol);
    if (_statusString)
        free(_statusString);
    if (_statusNum)
        free(_statusNum);
    if (_dynamicResponse)
        delete _dynamicResponse;
    _socket = 0;
};

long HttpResponse::getStatus()
{
    if (_statusNum)
        return atoi(_statusNum);
    return 0;
};

char * HttpResponse::getStatusString()
{
    return _statusString?_statusString:(char*)"";
};

HttpProtocol HttpResponse::getProtocol()
{
    // first check the response protocol
    if (_proto == HTTPNA)
	{
        if (_protocol)
		{
            int major, minor;

            sscanf(_protocol, "HTTP/%d.%d", &major, &minor);

            switch(major)
			{
                case 1:
                    switch(minor)
					{
                        case 0:
                            _proto = HTTP10;
                            break;
                        case 1:
                            _proto = HTTP11;
                            break;
                    };
                    break;
            };
        }
		else
		{
            _proto = HTTP09;
        };
    };

    if (_proto == HTTP11)
	{
        // A 1.1 compliant server response shows the protocol as HTTP/1.1 even
        // for a HTTP/1.0 request,  but it promises to only use HTTP/1.0 syntax.
        if (_request->getProtocol() == HTTP10)
            _proto = HTTP10;
    };

    return _proto;
};

char * HttpResponse::getHeader(char *name)
{
    return (char *)_headers.lookup(name);
};

char ** HttpResponse::getHeaders()
{
    SimpleHashIterator it(&_headers);
    int numHeaders = _headers.numEntries();
    char **rv = NULL;
    int index;

    if (numHeaders)
	{
        rv = (char **)malloc((numHeaders+1) * sizeof(char *));
        index = 0;
        while ( it.next() != NULL )
		{
            rv[index++] = (char *)it.getKey();
        };
        rv[index] = 0;
    };
    return rv;
};

long HttpResponse::getBodyLength()
{
    return _bodyLength;
};

PRBool HttpResponse::checkKeepAlive()
{
    HttpProtocol proto;
    const char *connectionHeader;

    if (_keepAlive < 0) {
        proto = getProtocol();
        if (proto == HTTP11) {
            // default is connection: keep-alive
            _keepAlive = 1;
        } else {
            // default is connection: close
            _keepAlive = 0;
        }

        connectionHeader = _request->getHeader("connection");
        if (connectionHeader) {
            if (!strcasecmp(connectionHeader, "keep-alive"))
                _keepAlive = 1;
            else if (!strcasecmp(connectionHeader, "close"))
                _keepAlive = 0;
            else
                Logger::logError(LOGERROR, "unknown connection header");
        }
    }

    return (_keepAlive == 0?PR_FALSE:PR_TRUE);
};

PRBool HttpResponse::checkConnection()
{
    // return true if the connection is OPEN
    return (_connectionClosed == 0?PR_TRUE:PR_FALSE);
};

int HttpResponse::_verifyStandardBody(RecvBuf &buf, int expectedBytes, PRBool check)
{
    int bytesRead = 0; 
    int curPos = 0;
    char ch;

    try
	{
        while(expectedBytes)
		{
            ch = buf.getChar();
            
            // if check is true, we think we know what the content looks like
            if (check)
			{
                if (ch != (char) curPos%256)
				{
                    Logger::logError(LOGERROR, "response data corrupt at byte %d (%d, %d)", curPos, ch, curPos % 256);
                    check = PR_FALSE;
                    break;
                }
                curPos++;
            };

            bytesRead++;

            if (expectedBytes > 0)
                expectedBytes--;
        };
    }
	catch (RecvBuf::EndOfChunking &)
	{
        // okay
        if (expectedBytes != -1)
            Logger::logError(LOGERROR, "expectedBytes known when chunking enabled?!", bytesRead, expectedBytes);
    }
	catch (RecvBuf::EndOfFile &)
	{
        if (expectedBytes >= 0)
            Logger::logError(LOGERROR, "unexpected EOF after %d bytes (expected %d)", bytesRead, expectedBytes);
    }

    return bytesRead;
};

int HttpResponse::_verifyDynamicBody(RecvBuf &buf, int expectedBytes)
{
    char *value;

    _dynamicResponse = new DynamicResponse(buf);

    _dynamicResponse->parse();

    // do basic checks here

    value = _dynamicResponse->lookupValue("BASIC", "dispatchMethod");
    if (!value)
        Logger::logError(LOGERROR, "dispatchMethod not set");
    else if (value && strcasecmp(value, _request->getMethod()))
        Logger::logError(LOGERROR, "dispatchMethod does not match request method (%s,%s)", value, _request->getMethod());

    value = _dynamicResponse->lookupValue("BASIC", "method");
    if (!value)
        Logger::logError(LOGERROR, "method not set");
    else if (value && strcasecmp(value, _request->getMethod()))
        Logger::logError(LOGERROR, "method does not match request method (%s,%s)", value, _request->getMethod());

    value = _dynamicResponse->lookupValue("BASIC", "url");
    if (!value)
        Logger::logError(LOGERROR, "url not set");

    value = _dynamicResponse->lookupValue("BASIC", "protocol");
    if (!value)
        Logger::logError(LOGERROR, "protocol not set");

    value = _dynamicResponse->lookupValue("BASIC", "RemoteAddr");
    if (!value)
        Logger::logError(LOGERROR, "RemoteAddr not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "ServerName");
    if (!value)
        Logger::logError(LOGERROR, "ServerName not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "HostName");
    if (!value)
        Logger::logError(LOGERROR, "HostName not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "Port");
    if (!value)
        Logger::logError(LOGERROR, "Port not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "Version");
    if (!value)
        Logger::logError(LOGERROR, "Version not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "Secure");
    if (!value)
        Logger::logError(LOGERROR, "Secure not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "GATEWAY_INTERFACE");
    if (!value)
        Logger::logError(LOGERROR, "GATEWAY_INTERFACE not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "SERVER_SOFTWARE");
    if (!value)
        Logger::logError(LOGERROR, "SERVER_SOFTWARE not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "HTTPS");
    if (!value)
        Logger::logError(LOGERROR, "HTTPS not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "SERVER_NAME");
    if (!value)
        Logger::logError(LOGERROR, "SERVER_NAME not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "SERVER_ID");
    if (!value)
        Logger::logError(LOGERROR, "SERVER_ID not set");

    value = _dynamicResponse->lookupValue("SERVER CONTEXT", "SERVER_PORT");
    if (!value)
        Logger::logError(LOGERROR, "SERVER_PORT not set");

    return 0;
}

PRBool HttpResponse::_handleBody(RecvBuf &buf)
{
    char *clHeader;      // content length header
    char *teHeader;      // transfer-encoding header
    int expected_cl=-1;  // expected content length

    teHeader = getHeader("transfer-encoding");
    if (teHeader && !strcasecmp(teHeader, "chunked"))
	{
        _chunkedResponse = PR_TRUE;
        buf.setChunkedMode();
    }
	else
	{
        _chunkedResponse = PR_FALSE;
        clHeader = getHeader("content-length");
        if (clHeader)
		{
             expected_cl =  atoi(clHeader);
        };
    };

    if (_request->getExpectStandardBody())
	{
        _bodyLength = _verifyStandardBody(buf, expected_cl, PR_TRUE);

    }
	else
		if (_request->getExpectDynamicBody())
		{
			_bodyLength = _verifyDynamicBody(buf, expected_cl);

		}
		else
		{
			_bodyLength = _verifyStandardBody(buf, expected_cl, PR_FALSE);
		};

    if (expected_cl >= 0)
	{
        if (_bodyLength != expected_cl)
		{
            Logger::logError(LOGERROR, "Content length was incorrect (%d/%d bytes)", _bodyLength, expected_cl);
        };
    };

    return PR_TRUE;
};

SunResponse :: SunResponse(const PRFileDesc *sock, Request *request) : Response(sock, request)
{
    performance = PR_FALSE;
    data = NULL;
    length = 0;
    timeout = request->getTimeout();
};

PRBool SunResponse::processResponse()
{
    // receive everything in the socket until it closes
    PRSocket buf(_socket, timeout);
    if (PR_FALSE == performance)
        buf.read(data, length);
    else
        buf.empty(length);
    return PR_TRUE;
};

void SunResponse :: getData(void*& buf, PRInt32& len)
{
    buf = data;
    len = length;
};

SunResponse :: ~ SunResponse()
{
    if (data)
        free(data);
};

PRBool HttpResponse::processResponse()
{
    RecvBuf buf(_socket, 1024);

    try
	{
        char tmp[1024];
        char ch;
        int index;
        PRBool doneParsing = PR_FALSE;
        char name[1024], value[1024];
        PRBool atEOL = PR_FALSE;
        PRBool inName = PR_TRUE;

        // Get protocol string
        index = 0;
        while ( !isspace(ch = buf.getChar()) )
            tmp[index++] = ch;
        tmp[index] = '\0';
        _protocol = strdup(tmp);

        // Get status num
        index = 0;
        while ( !isspace(ch = buf.getChar()) )
            tmp[index++] = ch;
        tmp[index] = '\0';
        _statusNum = strdup(tmp);
		retcode=atoi(tmp);

        // Get status string
        if (ch != '\r')
		{
            index = 0;
            while ( (ch = buf.getChar()) != '\r' ) 
                tmp[index++] = ch;
            tmp[index] = '\0';
            _statusString = strdup(tmp);
        } ;

        // Skip CRLF
        (void)buf.getChar();

        // loop over response headers
        index = 0;
        while (!doneParsing)
		{
            ch = buf.getChar();
            switch(ch)
			{
                case ':':
                      if (inName)
					  {
                          name[index] = '\0';
                          index = 0;
                          inName = PR_FALSE;

                          // skip whitespace
                          while( isspace(ch = buf.getChar()) );
                          value[index++] = ch;
                      }
					  else
					  {
                          value[index++] = ch;
                      };
                      break;
                case '\r':
                      if (inName && !atEOL)
					  {
                           Logger::logError(LOGERROR, "name without value header");
                           return PR_FALSE;
                      };
                      break;
                case '\n':
                      if (atEOL)
					  {
                          doneParsing = PR_TRUE;
                          break;
                      };
                      if (inName)
					  {
                           Logger::logError(LOGERROR, "name without value header");
                           return PR_FALSE;
                      };
                      value[index] = '\0';
                      index = 0;
                      inName = PR_TRUE;
                      _headers.insert((void *)name, (void *)strdup(value));
                      atEOL = PR_TRUE;
                      break;
                default:
                    atEOL = PR_FALSE;
                    if (inName)
                         name[index++] = ch;
                    else
                         value[index++] = ch;
                    break;
            };
        };


    }
	catch (RecvBuf::EndOfFile &)
	{
        if (!_request->isHangupOk())
            Logger::logError(LOGERROR, "Received unexpected end of file from server");
        return PR_FALSE;
    }

    // Read the body (HEAD requests don't have bodies)
    // jpierre 1xx, 204 and 304 don't have bodies either
    if ( 
		strcmp(_request->getMethod(), "HEAD") &&
		(!((retcode>=100) && (retcode<200))) &&
		(retcode!=204) &&
		(retcode!=304)
		)
	{
        if (_handleBody(buf) == PR_FALSE)
            return PR_FALSE;
    }

    if (checkConnection() && !checkKeepAlive())
	{
        // if connection is still open, and we didn't expect a keepalive,
        // read another byte to see if the connection has closed.
        try
		{
            char ch;
            ch = buf.getChar();
            buf.putBack();
            // conflict!
            Logger::logError(LOGERROR, "connection kept alive when it shouldn't");
        }
		catch (RecvBuf::EndOfFile &)
		{
            _connectionClosed = 1;
        };
    };

    _checkResponseSanity();

    return PR_TRUE;
};

void HttpResponse::_checkResponseSanity()
{
    char *clHeader = getHeader("content-length");
    char *teHeader = getHeader("transfer-encoding");

    ///////////////////////////////////////////////////
    // Check items relevant to HTTP/1.0 and HTTP/1.1 //
    ///////////////////////////////////////////////////
    {
        // check for both content-length and chunked
        if (clHeader && teHeader)
            Logger::logError(LOGWARNING, "response contains both content-length and transfer-encoding");

        // check for basic headers
		if (!getHeader("date"))
            Logger::logError(LOGWARNING, "response does not contain a date header");
		if (!getHeader("server"))
            Logger::logError(LOGWARNING, "response does not contain a server header");

        int expectedLength;
        if ((expectedLength = _request->getExpectedResponseLength()) > 0)
		{
            if (expectedLength != _bodyLength)
			{
                Logger::logError(LOGERROR, "response body length does not match expected response length (%d/%d)", _bodyLength, expectedLength);
            };
        };
    };

    ///////////////////////////////////////
    // Check items relevant to HTTP/1.0  //
    ///////////////////////////////////////
    if (getProtocol() == HTTP10)
	{
        if (_chunkedResponse)
            Logger::logError(LOGWARNING, "server sent a chunked HTTP/1.0 response");
    };

    ///////////////////////////////////////
    // Check items relevant to HTTP/1.1  //
    ///////////////////////////////////////
    if (getProtocol() == HTTP11)
	{
        if ((!clHeader && !_chunkedResponse) &&
			(!((retcode>=100) && (retcode<200))) &&
			(retcode!=204) &&
			(retcode!=304))

            Logger::logError(LOGINFO, "server responded with a HTTP/1.1 response without content-length or chunked encoding");

    };
};


DynamicResponse::DynamicResponse(RecvBuf &buf)  : _buf(buf)
{
    _sectionList = 0;
};

DynamicResponse & HttpResponse::getDynamicResponse()
{
    return *_dynamicResponse;
};

DynamicResponse::~DynamicResponse()
{
    section *ptr;

    for (ptr = _sectionList; ptr; )
	{
        section *dead = ptr;
        ptr = ptr->next;

        delete dead->name;
        delete dead->table;
        free(dead);
    };
};

DynamicResponse::section* DynamicResponse::_createSection(char *name)
{
    section *newSection;

    newSection = (section *)malloc(sizeof(section));
    newSection->name = strdup(name);
    newSection->table= new SimpleStringHash(32);
    newSection->table->setMixCase();
    newSection->next = NULL;

    newSection->next = _sectionList;
    _sectionList = newSection;

    return newSection;
};

void DynamicResponse::parse()
{
    PRBool endOfFileOk = PR_FALSE;   // TRUE if EOF is okay at this point in 
                                     // our parsing.
    char ch;
#define BUFLEN 4096
    char tmpStr[BUFLEN], tmpStr2[BUFLEN];
    int index;

    try
	{
        while(1)
		{
            section *newSection;

            // skip whitespace
            while( isspace( (ch = _buf.getChar()) ) );

            // read the section name
            if (ch == '[')
			{
                endOfFileOk = PR_FALSE;
                index = 0;
                while ( (ch = _buf.getChar()) != ']' )
				{
                    tmpStr[index++] = ch;
                };
                tmpStr[index] = '\0';
                newSection = _createSection(tmpStr);
            }
			else
			{
                Logger::logError(LOGERROR, "Expected '[' parsing dynamic response");
            };

            endOfFileOk = PR_TRUE;

            // read the name-value pairs
            while(1)
			{
                // skip whitespace
                while( isspace( (ch = _buf.getChar()) ) );
 
                if (ch == '[')
				{
                    // whoops - this is the start of a new section
                    _buf.putBack();
                    break;
                };

                // read the name
                index = 1;
                tmpStr[0] = ch;
                while ( !isspace( (ch = _buf.getChar()) ) )
				{
                    endOfFileOk = PR_FALSE;
                    if (ch == ':')
                        break;
                    tmpStr[index++] = ch;
                };

                tmpStr[index] = '\0';
                if (ch != ':') {
                    Logger::logError(LOGERROR, "Expected ':' parsing dynamic response");
                };
 
                // skip whitespace
                while( isspace( (ch = _buf.getChar()) ) )
                    if (ch == '\r' || ch == '\n')
					{
                        newSection->table->insert(tmpStr, NULL);
                        break;
                    };

                if (ch == '\r' || ch == '\n')
                    continue;
 
                // read the value
                index = 1;
                tmpStr2[0] = ch;
                while ( 1 )
				{
                    ch = _buf.getChar();
                    if (ch == '\r' || ch == '\n')
                        break;
                    tmpStr2[index++] = ch;
                };

                tmpStr2[index] = '\0';

                if (!strcasecmp(tmpStr, "Error"))
                    Logger::logError(LOGERROR, "Dynamic content reports error: \"%s\"", tmpStr2);
                else 
                    newSection->table->insert(tmpStr, strdup(tmpStr2));

                endOfFileOk = PR_TRUE;
            };
        };
    }
    catch (RecvBuf::EndOfFile &)
	{
        if (!endOfFileOk)
            Logger::logError(LOGERROR, "Unexpected EOF parsing dynamic response");
    }

    catch (RecvBuf::EndOfChunking &)
	{
        if (!endOfFileOk)
            Logger::logError(LOGERROR, "Unexpected EOF parsing dynamic response");
    };
};

PRBool DynamicResponse::lookupSection(char *sectionName)
{
    section *ptr;

    for (ptr = _sectionList; ptr; ptr = ptr->next)
        if (!strcasecmp(ptr->name, sectionName))
            break;

    if (ptr)
        return PR_TRUE;

    return PR_FALSE;
};

char* DynamicResponse::lookupValue(char *sectionName, char *key)
{
    section *ptr;
    char *value = NULL;

    for (ptr = _sectionList; ptr; ptr = ptr->next)
        if (!strcasecmp(ptr->name, sectionName))
            break;

    if (ptr)
	{
        value = (char *)ptr->table->lookup(key);
    };

    return value;
};

void SunResponse :: setPerformance(PRBool perf)
{
    performance = perf;
};
