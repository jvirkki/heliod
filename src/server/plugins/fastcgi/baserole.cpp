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

#include "frame/log.h"
#include "support/NSString.h"
#include "fastcgi.h"
#include "circularbuffer.h"
#include "endpoint.h"
#include "fcgiparser.h"
#include "server.h"
#include "fcgirequest.h"
#include "fcgiparser.h"
#include "fcgirole.h"
#include "stubexec.h"
#include "fastcgii18n.h"

BaseRole::BaseRole(PRUint8 role, FcgiServer* server, FcgiRequest& req)
: fcgiRole(role),
  fcgiServer(server),
  request(req)
{
    lastError = NO_FCGI_ERROR;
    parser = new FcgiParser(&request, fcgiRole);
}

BaseRole::~BaseRole() {
    delete parser;
    parser = NULL;
    fcgiServer = NULL;
}
//-----------------------------------------------------------------------------
// BaseRole::sendPreContentData
//-----------------------------------------------------------------------------
PRStatus BaseRole::sendPreContentData(EndPoint& server) {
    PRStatus rv = PR_FAILURE;

    if(makeBeginRequest(server) == PR_SUCCESS)
        rv = makeEnvParams(server);

    return rv;
}

void BaseRole::buildParams( int nameLen, int valueLen,
            unsigned char *headerBuffPtr, int *headerLenPtr) {
    unsigned char *startHeaderBuffPtr = headerBuffPtr;

   PR_ASSERT(nameLen >= 0);
    if(nameLen < 0x80) {
        *headerBuffPtr++ = nameLen;
    } else {
        *headerBuffPtr++ = (nameLen >> 24) | 0x80;
        *headerBuffPtr++ = (nameLen >> 16);
        *headerBuffPtr++ = (nameLen >> 8);
        *headerBuffPtr++ = nameLen;
    }
   PR_ASSERT(valueLen >= 0);
    if(valueLen < 0x80) {
        *headerBuffPtr++ = valueLen;
    } else {
        *headerBuffPtr++ = (valueLen >> 24) | 0x80;
        *headerBuffPtr++ = (valueLen >> 16);
        *headerBuffPtr++ = (valueLen >> 8);
        *headerBuffPtr++ = valueLen;
    }
    *headerLenPtr = headerBuffPtr - startHeaderBuffPtr;
}

void BaseRole::makeBeginRequestBody(FCGI_BeginRequestBody *body) {
    body->roleB1 = (fcgiRole >>  8) & 0xff;
    body->roleB0 = (fcgiRole      ) & 0xff;
    body->flags = (fcgiServer->config->keepAliveConnection) ? FCGI_KEEP_CONN : 0;
    memset(body->reserved, 0, sizeof(body->reserved));
}

PRStatus BaseRole::checkForSpace(EndPoint ep, int size) {
    //check if the buffer has the required space
    if((size < 0) || (ep.to->hasSpace() < size)) {
        if(ep.to->hasData()) {
            //try to create space by sending the existing data
            ep.send();
        } else {
            //not enough buffer space
            return PR_FAILURE;
        }
    }

    return ((ep.to->hasSpace() >= size) ? PR_SUCCESS : PR_FAILURE);
}


PRStatus BaseRole::makeEnvParams(EndPoint& server) {
    int headerLen, nameLen, valueLen;
    char *equalPtr;
    unsigned char headerBuff[8];
    int i = 0;
    int totalDataLen = 0;
    char **env = request.getEnvironment();

    i=0;
    while(env && env[i]) {
        equalPtr = strchr(env[i], '=');
        PR_ASSERT(equalPtr != NULL);
        nameLen = equalPtr - env[i];
        valueLen = strlen(equalPtr + 1);
        buildParams(nameLen, valueLen, &headerBuff[0], &headerLen);
        totalDataLen = nameLen + valueLen + headerLen;
        if(parser->makePacketHeader(FCGI_PARAMS, totalDataLen,
                        *(server.to)) != PR_SUCCESS) {
            if(checkForSpace(server, -1) == PR_SUCCESS)
                continue;
            else
                return PR_FAILURE;
        }

        if(checkForSpace(server, totalDataLen) == PR_FAILURE)
            return PR_FAILURE;

        if((server.to->addData((char *) &headerBuff[0], headerLen) >=
                                headerLen) &&
           (server.to->addData(env[i], nameLen) >= nameLen) &&
           (server.to->addData(equalPtr + 1, valueLen) >= valueLen))
            ; //added the env data
        else {
            return PR_FAILURE;
        }

        i++;
    }

    util_env_free(env);

    if(parser->makePacketHeader(FCGI_PARAMS, 0, *(server.to)) != PR_SUCCESS)
        return PR_FAILURE;

    if(fcgiRole == FCGI_AUTHORIZER)
        server.send();

    return PR_SUCCESS;
}

PRStatus BaseRole::makeBeginRequest(EndPoint& server) {
    FCGI_BeginRequestBody body;
    PRUint32 bodySize;

    bodySize = sizeof(FCGI_BeginRequestBody);
    makeBeginRequestBody(&body);
    if(parser->makePacketHeader(FCGI_BEGIN_REQUEST, bodySize, *(server.to))
                                        == PR_SUCCESS) {
        if(server.to->addData((char *)&body, bodySize) >= bodySize) {
            return PR_SUCCESS;
        }
    }

    return PR_FAILURE;
}

PRStatus BaseRole::makeAbortRequestBody(EndPoint& server) {
    if(parser->makePacketHeader(FCGI_ABORT_REQUEST, sizeof(FCGI_Header), *(server.to)) != PR_SUCCESS)
        return PR_FAILURE;

    server.send();
    return PR_SUCCESS;
}

PRStatus BaseRole::process() {
    if (!fcgiServer) {
        lastError = FCGI_INVALID_SERVER;
        request.log(LOG_FAILURE, GetString(DBT_no_server_available));
        return PR_FAILURE;
    }

    PRBool flagReusePersistent = fcgiServer->config->keepAliveConnection;
    if (process(PR_TRUE, flagReusePersistent) == PR_SUCCESS)
        return PR_SUCCESS;

    return PR_FAILURE;
}

inline PRStatus BaseRole::process(PRBool flagRetry, PRBool flagReusePersistent) {
     lastError = NO_FCGI_ERROR;
    // We won't retry if we were asked not to or this is a POST
    int retries = 0;
    if (flagRetry )
        retries = fcgiServer->config->retries;

    // Attempt to process the request (with retries as appropriate)
    PRIntervalTime epoch = PR_IntervalNow();
    PRIntervalTime previous = epoch;
    int tries = 0;
    PRBool firstTime = PR_TRUE;
    PRBool firstStubPidFileError = PR_FALSE;

    for (;;) {
        // Get a FcgiServerChannel to the selected Daemon
        FcgiServerChannel *channel = fcgiServer->getChannel(fcgiServer->config->connect_timeout, flagReusePersistent);
        if (channel) {
            //reset the parser
            parser->reset();

            // Pass the FcgiServerChannel to the derived class's process() function
            // (which implements the NSAPI/ISAPI-specific processing)
            PRStatus status = process(*channel);

            // Return the FcgiServerChannel to the Daemon
            fcgiServer->addChannel(channel);

            // Get out if we're done
            if (status == PR_SUCCESS)
                return PR_SUCCESS;

        } else if(!fcgiServer->config->remoteServer) {
            PRStatus result = sendRequestToStub(RQ_START, firstTime);

            if (result == PR_SUCCESS) {
                fcgiServer->setActive(PR_TRUE);
            }

            if((result == PR_SUCCESS) && (firstTime || firstStubPidFileError)) {
                firstStubPidFileError = PR_FALSE;
                firstTime = PR_FALSE;
                continue;
            } else if(((result == PR_FAILURE) && (lastError == STUB_PID_FILE_CREATE_FAILURE)) && firstTime) {
                // wait for 5 seconds before retrying the stub connection
                PR_Sleep(PR_SecondsToInterval(DEFAULT_SLEEP_TIME));
                firstStubPidFileError = PR_TRUE;
                firstTime = PR_FALSE;
                continue;
            }
        } else { // remote server
            request.log(LOG_FAILURE, GetString(DBT_remote_connection_failure), fcgiServer->config->procInfo.bindPath);
        }

        // Get out if we've spent too long trying
        PRIntervalTime now = PR_IntervalNow();
        if ((PRIntervalTime)(now - epoch) > fcgiServer->config->connect_timeout)
            break;

        // Get out if we've tried too many times
        // or if the processing of the POST request failed
        tries++;
        if(channel && request.hasBody()) // error during processing of POST request
            break;

        if (tries > retries) {
            if (retries > 0)
                request.log(LOG_FAILURE, GetString(DBT_exceeded_number_of_retries), retries);
            break;
        }

        // Don't reuse persistent connections on retries if processing failed
        if(channel)
            flagReusePersistent = PR_FALSE;

        // Throttle back our connection attempts
        PRIntervalTime elapsed = now - previous;
        if (elapsed < fcgiServer->config->connect_interval) {
            PRIntervalTime delay = fcgiServer->config->connect_interval - elapsed;
            PR_Sleep(delay);
            now += delay;
        }
        previous = now;
    }

    return PR_FAILURE;
}

PRStatus BaseRole::sendRequestToStub(RequestMessageType reqType, PRBool retryOption) {
    PRBool flagError = PR_FALSE;
    StubExec *stubExec = new StubExec(fcgiServer->config, request.getOrigSession(), request.getOrigRequest());

    //check if there was any error while getting the stub socket address
    lastError = stubExec->getLastError();
    if(lastError == STUB_CONNECT_FAILURE) {
        request.log(LOG_FAILURE, GetString(DBT_stub_socket_connect_error));
        flagError = PR_TRUE;
    }

    //try to connect to stub
    if(!flagError && stubExec->connect() == PR_FAILURE) {
        //start Fastcgistub
        if(stubExec->startStub(retryOption) == PR_FAILURE) {
            lastError = stubExec->getLastError();
            if(lastError == STUB_CONNECT_FAILURE)
                request.log(LOG_FAILURE, GetString(DBT_stub_socket_connect_error));
            flagError = PR_TRUE;
        }
    }

    //Ask stub to spawn Fastcgi application processes
    if(!flagError && (stubExec->sendRequest(reqType) != PR_SUCCESS)) {
        lastError = stubExec->getLastError();
        if(lastError == STUB_CONNECT_FAILURE)
            request.log(LOG_FAILURE, GetString(DBT_stub_socket_connect_error));
        flagError = PR_TRUE;
    }

    lastError = stubExec->getLastError();
    delete stubExec;
    stubExec = NULL;
    return ( flagError ? PR_FAILURE : PR_SUCCESS );
}
