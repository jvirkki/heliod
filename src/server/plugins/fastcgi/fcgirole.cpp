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

#include "base/file.h"
#include "frame/log.h"
#include "constants.h"
#include "fastcgi.h"
#include "circularbuffer.h"
#include "fcgiparser.h"
#include "server.h"
#include "fcgirequest.h"
#include "fcgirole.h"
#include "fastcgii18n.h"

/*************
 * RESPONDER *
 ************/
ResponderRole::ResponderRole(FcgiServer *srvr, FcgiRequest& req)
    : BaseRole(FCGI_RESPONDER, srvr, req)
{
}

PRStatus ResponderRole::process(FcgiServerChannel& channel)
{
    // EndPoint abstractions for client (Session) and server (FcgiServerChannel)
    EndPoint& server = channel;
    CircularBuffer fromClient(sizeBufferFromClient);
    CircularBuffer toClient(sizeBufferToClient);
    EndPoint client(request.getOrigSession()->csd, &fromClient, &toClient, PR_TRUE);
    PRBool clientDataSent = PR_FALSE;
    // We're waiting for the remote server to send its headers
    PRUint64 cl = request.contentLength;
    PRBool clientRead = PR_FALSE;
    PRBool stdinEndSent = PR_FALSE;
    PRBool firstTime = PR_TRUE;

    if (cl > 0) {
        clientRead = PR_TRUE;
    }


resendresponder:
    if (sendPreContentData(server) != PR_SUCCESS) {
        return PR_FAILURE;
    }

    // read the client data that is already available
    if (clientRead) {
       int res = client.recv();
       if (res > 0) {
           cl -= res;
       }
    }

    int initialDataSize = server.to->hasData();

    PRBool serverRead = PR_FALSE;
    PRBool done = PR_FALSE;
    PRBool flagWaitingForResponse = PR_TRUE;

    for (;;) {
        EndPoint *endpoints[2];
        PRPollDesc pd[2];
        int pc = 0;
        PRInt32 rv = 0;
        int i;

        if (firstTime) {
            goto responderSendData;
        }

        // Figure out which endpoints we're waiting for
        if (server.add(&pd[pc])) endpoints[pc++] = &server;
        if (client.add(&pd[pc])) endpoints[pc++] = &client;

        if (!pc) {
            // No endpoints to wait for
            break;
        }

        // Do we have work left to do?
        if (!server.isReadable() && !server.from->hasData()) {
            // Server closed connection or sent terminating chunk
            break;
        }
        if (!server.isWritable() && !client.isWritable()) {
            // Client went away?  In any event, we can't reuse the
            // FcgiServerChannel because it may have unread data
            channel.setClientError();
            break;
        }
        if (done) {
            break;
        }

        // Wait for activity
#ifdef XP_WIN32
        if (fcgiServer->config->udsName) {
            int hCount = 0;
            DWORD avail = 0;

            rv = endpoints[0]->recv();
            if (clientRead)
                rv = PR_Poll(&pd[1], pc-1, fcgiServer->config->poll_timeout);

        } else
            rv = PR_Poll(pd, pc, fcgiServer->config->poll_timeout);
#else
        rv = PR_Poll(pd, pc, fcgiServer->config->poll_timeout);
#endif // XP_WIN32

        if (rv < 1) {
            // Timeout.  Do not reuse the FcgiServerChannel.
            if (client.isReadable()) {
                channel.setClientError();
                request.log(LOG_FAILURE, GetString(DBT_request_body_timeout));
            } else if (flagWaitingForResponse) {
                channel.setServerError();
                request.log(LOG_FAILURE, GetString(DBT_response_header_timeout_X), pblock_findval("path", request.getOrigRequest()->vars));
            } else {
                channel.setServerError();
                request.log(LOG_FAILURE, GetString(DBT_response_body_timeout_X), pblock_findval("path", request.getOrigRequest()->vars));
            }

            break;
        }

        // Get new data
        for (i = 0; i < pc; i++) {
            if (pd[i].out_flags & PR_POLL_READ) {
                int res = endpoints[i]->recv();
                if (endpoints[i]->isClient) { // client
                    if (res > 0)
                        cl -= res;
                }
            }
        }

responderSendData:
        if (clientRead && client.from->hasData()) {
            if (cl <= 0)
                clientRead = PR_FALSE;

            // before formatting check the size of server's to buffer
            int currentSize = server.to->hasData();
            parser->formatRequest(*(client.from), *(server.to));

            if (!clientDataSent && currentSize < server.to->hasData())
                clientDataSent = PR_TRUE;
        }

        if (clientRead == PR_FALSE && !stdinEndSent) {
            if (parser->makePacketHeader(FCGI_STDIN, 0, *(server.to)) == PR_SUCCESS) {
                stdinEndSent = PR_TRUE;
            }
        }

        if (server.to->hasData() && server.isWritable()) {
            server.send();
            if (!clientRead && !clientDataSent)
                clientDataSent = PR_TRUE;
        }

        if (server.from->hasData()) {
            flagWaitingForResponse = PR_FALSE;

            // parse the response from the fcgi server
            parser->parse(*(server.from), *(client.to));
            if (parser->getServerError().length()) {
                request.log(LOG_FAILURE, GetString(DBT_application_stderr_msg), parser->getServerError().data());
            }
            PluginError parseErr = parser->getLastError();
            if (parseErr != NO_FCGI_ERROR) { // error while parsing
               if (!fcgiServer->config->remoteServer &&
                     parseErr == OVER_LOADED && !request.hasContentLength()) {
                   sendRequestToStub(RQ_OVERLOAD, PR_FALSE);
               }

               lastError = parseErr;
               break;
            }

            serverRead = PR_TRUE;
            if (parser->waitingForDataParse) {
               parser->parseHttpHeader(*(client.to));
            }
        }

        if (serverRead == PR_TRUE)
            serverRead = (!parser->exitStatusSet);

        // send the response data to the client
        if (client.to->hasData() && client.isWritable()) {
            client.send();
        }


        if (!firstTime && (clientRead == PR_FALSE) && (serverRead == PR_FALSE) &&
            (!client.to->hasData()) && (!server.to->hasData()) && !flagWaitingForResponse) {
            if (parser->waitingForDataParse == PR_TRUE) {
                lastError = FCGI_INVALID_RESPONSE;
                request.log(LOG_FAILURE, GetString(DBT_invalid_fcgi_response), pblock_findval("path", request.getOrigRequest()->vars));
                break;
            }

            done = PR_TRUE;
        }

        if (firstTime) firstTime = PR_FALSE;

    } // for

    // If there is some unsent data in server channel...
    if (server.from->hasData()) {
        channel.setClientError();  // Do not reuse the FcgiServerChannel
    }

    // If we never got a response...
    if (flagWaitingForResponse) {
        if (!client.isReadable())
            channel.setServerError(); // Do not reuse the FcgiServerChannel
        return PR_FAILURE;
    }

    if (!done) {
        channel.setServerError(); // Do not reuse the FcgiServerChannel
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}


/*************
 * FILTER *
 ************/
FilterRole::FilterRole(FcgiServer *srvr, FcgiRequest& req)
    : BaseRole(FCGI_FILTER, srvr, req),
      filterFileInfo(req.getFilterFileInfo())
{
    filterFd = NULL;
    filterApp = NULL;
}

FilterRole::~FilterRole() {
    if (filterFd) {
        system_fclose(filterFd);
        filterFd = NULL;
    }
}

void FilterRole::initFilter() {
    filterApp = request.getFilterApp();
    filterFd = NULL;

    if (filterApp) {
        filterFd = system_fopenRO(filterApp);
        if (filterFd == SYS_ERROR_FD)
            filterFd = NULL;
        else
            filterFileSize =  filterFileInfo.size;
    }
}

PRStatus FilterRole::process(FcgiServerChannel& channel)
{
    // EndPoint abstractions for client (Session) and server (FcgiServerChannel)
    EndPoint& server = channel;
    CircularBuffer fromClient(sizeBufferFromClient);
    CircularBuffer toClient(sizeBufferToClient);
    EndPoint client(request.getOrigSession()->csd, &fromClient, &toClient, PR_TRUE);
    // We're waiting for the remote server to send its headers
    PRUint64 cl = request.contentLength;
    PRBool clientRead = PR_FALSE;
    PRBool stdinEndSent = PR_FALSE;
    PRUint32 filterFileBytesRead = 0;
    PRBool clientDataSent = PR_FALSE;

    if (cl > 0) {
        clientRead = PR_TRUE;
    }
resendfilter:
    if (sendPreContentData(server) != PR_SUCCESS) {
        return PR_FAILURE;
    }

    initFilter();
    if (!filterFd) {
        lastError = FCGI_FILTER_FILE_OPEN_ERROR;
        request.log(LOG_FAILURE, GetString(DBT_fcgi_filter_file_open_error), pblock_findval("path", request.getOrigRequest()->vars));
        return PR_FAILURE;
    }

    // read the client data that is already available
    if (clientRead) {
       int res = client.recv();
       if (res > 0) {
           cl -= res;
       }
    }

    CircularBuffer fromFilter(sizeBufferFromClient);
    int bytesRead = 0;
    PRBool serverRead = PR_FALSE;
    PRBool done = PR_FALSE;
    PRBool firstTime = PR_TRUE;
    PRBool dataEndSent = PR_FALSE;
    PRBool filterFileRead = PR_TRUE;
    PRBool filterDataSent = PR_FALSE;
    PRBool flagWaitingForResponse = PR_TRUE;

    for (;;) {
        EndPoint *endpoints[2];
        PRPollDesc pd[3];
        int pc = 0;
        PRInt32 rv;
        int i;

        if (firstTime) {
            goto filterSendData;
        }

        // Figure out which endpoints we're waiting for
        if (server.add(&pd[pc])) endpoints[pc++] = &server;
        if (client.add(&pd[pc])) endpoints[pc++] = &client;
        if (!clientRead && filterFd && filterFileRead) {
            pd[pc].fd = filterFd;
            pd[pc].in_flags = (0 | PR_POLL_READ);
            pd[pc].out_flags = 0;
            pc++;
        }

        // Do we have work left to do?
        if (!pc) {
            // No endpoints to wait for
            break;
        }

        if (!server.isReadable() && !server.from->hasData()) {
            // Server closed connection or sent terminating chunk
            break;
        }
        if (!server.isWritable() && !client.isWritable()) {
            // Client went away?  In any event, we can't reuse the
            // FcgiServerChannel because it may have unread data
            channel.setClientError();
            break;
        }
        if (done)
            break;

#ifdef XP_WIN32
        if (fcgiServer->config->udsName) {
            DWORD avail = 0;

            rv = endpoints[0]->recv();
            if (clientRead || filterFileRead)
                rv = PR_Poll(&pd[1], pc-1, fcgiServer->config->poll_timeout);

        } else
            rv = PR_Poll(pd, pc, fcgiServer->config->poll_timeout);
#else
        rv = PR_Poll(pd, pc, fcgiServer->config->poll_timeout);
#endif // XP_WIN32

        if (rv < 1) {
            // Timeout.  Do not reuse the FcgiServerChannel.
            if (client.isReadable()) {
                channel.setClientError();
                request.log(LOG_FAILURE, GetString(DBT_request_body_timeout));
            } else if (flagWaitingForResponse) {
                channel.setServerError();
                request.log(LOG_FAILURE, GetString(DBT_response_header_timeout_X), pblock_findval("path", request.getOrigRequest()->vars));
            } else {
                channel.setServerError();
                request.log(LOG_FAILURE, GetString(DBT_response_body_timeout_X), pblock_findval("path", request.getOrigRequest()->vars));
            }

            break;
        }

        // Get new data
        for (i = 0; i < pc; i++) {
            if (pd[i].out_flags & PR_POLL_READ) {
                if (pd[i].fd != filterFd) {
                    int res = endpoints[i]->recv();
                    if (endpoints[i]->isClient) {
                        if (res > 0)
                            cl -= res;
                    }
                }
            }
        }

filterSendData:
        if (clientRead && client.from->hasData()) {
            if (cl <= 0)
                clientRead = PR_FALSE;

            // before formatting check the size of server's to buffer
            int currentSize = server.to->hasData();
            parser->formatRequest(*(client.from), *(server.to));

            if (!clientDataSent && currentSize < server.to->hasData())
                clientDataSent = PR_TRUE;
        }

        if (!clientRead && !stdinEndSent) {
            if (parser->makePacketHeader(FCGI_STDIN, 0,
                         *(server.to)) == PR_SUCCESS) {
                stdinEndSent = PR_TRUE;
            }
        }

       if (!clientRead && filterFileRead) {
            char *buf;
            int spaceAvailable = 0;
            int available = fromFilter.hasSpace();
            while ((available > 0) && (filterFileRead)) {
                if (fromFilter.requestSpace(buf, spaceAvailable)) {
                    bytesRead = system_fread(filterFd, buf, spaceAvailable);
                    fromFilter.releaseSpace(bytesRead);
                }

                available = fromFilter.hasSpace();
                filterFileBytesRead += bytesRead;
                if (filterFileBytesRead == filterFileSize) {
                    filterFileRead = PR_FALSE;
                }
            }

            int currentSize = server.to->hasData();
            parser->formatRequest(fromFilter, *(server.to), FCGI_DATA);
            if (!filterDataSent && currentSize < server.to->hasData())
                filterDataSent = PR_TRUE;
        }

        if (!clientRead && !filterFileRead && !dataEndSent) {
            // send end of FCGI_DATA stream
            if (parser->makePacketHeader(FCGI_DATA, 0,
                           *(server.to)) == PR_SUCCESS) {
                dataEndSent = PR_TRUE;
            }
        }

        if (server.to->hasData() && server.isWritable())  {
            server.send();
            if (!clientRead && !clientDataSent)
                clientDataSent = PR_TRUE;
        }

        if (server.from->hasData()) {
            flagWaitingForResponse = PR_FALSE;
            serverRead = PR_TRUE;

            // parse the response from the fcgi server
            parser->parse(*(server.from), *(client.to));
            if (parser->getServerError().length()) {
                request.log(LOG_FAILURE, GetString(DBT_application_stderr_msg), parser->getServerError().data());
            }
            PluginError parseErr = parser->getLastError();
            if (parseErr != NO_FCGI_ERROR && !fcgiServer->config->remoteServer &&
                                parseErr == OVER_LOADED && !request.hasContentLength()) {
                if (sendRequestToStub(RQ_OVERLOAD, PR_FALSE) == PR_SUCCESS)
                    goto resendfilter;
            }

            if (parser->waitingForDataParse) {
                parser->parseHttpHeader(*(client.to));
            }
        }

        if (serverRead == PR_TRUE)
            serverRead = (!parser->exitStatusSet);

        // send the response data to the client
        if (client.to->hasData() && client.isWritable()) {
            client.send();
        }

        if (!firstTime && (clientRead == PR_FALSE) && (serverRead == PR_FALSE) &&
            (!client.to->hasData()) && (!server.to->hasData()) && !flagWaitingForResponse) {
            if (parser->waitingForDataParse == PR_TRUE) {
                lastError = FCGI_INVALID_RESPONSE;
                request.log(LOG_FAILURE, GetString(DBT_invalid_fcgi_response), pblock_findval("path", request.getOrigRequest()->vars));
                return PR_FAILURE;
            }

            done = PR_TRUE;
        }

        if (firstTime) firstTime = PR_FALSE;

    } // for

    // If there is some unsent data in server channel...
    if (server.from->hasData()) {
        channel.setClientError();  // Do not reuse the FcgiServerChannel
    }

    // If we never got a response...
    if (flagWaitingForResponse) {
        if (!client.isReadable())
            channel.setServerError(); // Do not reuse the FcgiServerChannel
        return PR_FAILURE;
    }

    if (!done) {
        channel.setServerError(); // Do not reuse the FcgiServerChannel
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

/*************
 * AUTHORIZER *
 ************/
AuthRole::AuthRole(FcgiServer *srvr, FcgiRequest& req)
    : BaseRole(FCGI_AUTHORIZER, srvr, req)
{
}

PRStatus AuthRole::process(FcgiServerChannel& channel)
{
    // EndPoint abstractions for client (Session) and server (FcgiServerChannel)
    EndPoint& server = channel;
    CircularBuffer fromClient(sizeBufferFromClient);
    CircularBuffer toClient(sizeBufferToClient);
    EndPoint client(request.getOrigSession()->csd, &fromClient, &toClient, PR_TRUE);
    PRBool clientRead = PR_FALSE;

resendauth:
    if (sendPreContentData(server) != PR_SUCCESS) {
        return PR_FAILURE;
    }

    // We're waiting for the remote server to send its headers
    PRBool flagWaitingForResponse = PR_TRUE;
    PRBool serverRead = PR_FALSE;
    PRBool done = PR_FALSE;

    for (;;) {
        EndPoint *endpoints[1];
        PRPollDesc pd[1];
        int pc = 0;
        PRInt32 rv;

        // Figure out which endpoints we're waiting for
        if (server.add(&pd[pc])) endpoints[pc++] = &server;

        if (!pc) {
            // No endpoints to wait for
            break;
        }

        // Do we have work left to do?
        if (!server.isReadable() && !server.from->hasData()) {
            // Server closed connection or sent terminating chunk
            break;
        }
        if (!server.isWritable() && !client.isWritable()) {
            // Client went away?  In any event, we can't reuse the
            // FcgiServerChannel because it may have unread data
            channel.setClientError();
            break;
        }
        if (done)
            break;

#ifdef XP_WIN32
        if (fcgiServer->config->udsName) {
            DWORD avail = 0;

            rv = endpoints[0]->recv();

        } else
            rv = PR_Poll(pd, pc, fcgiServer->config->poll_timeout);
#else
        rv = PR_Poll(pd, pc, fcgiServer->config->poll_timeout);
#endif // XP_WIN32

        if (rv < 1) {
            // Timeout.  Do not reuse the FcgiServerChannel.
            if (flagWaitingForResponse) {
                channel.setServerError();
                request.log(LOG_FAILURE, GetString(DBT_response_header_timeout_X), pblock_findval("path", request.getOrigRequest()->vars));
            } else {
                channel.setServerError();
                request.log(LOG_FAILURE, GetString(DBT_response_body_timeout_X), pblock_findval("path", request.getOrigRequest()->vars));
            }

            break;
        }

        // Get new data
        for (int i = 0; i < pc; i++) {
            if (pd[i].out_flags & PR_POLL_READ) {
                endpoints[i]->recv();
            }
        }

        if (server.from->hasData()) {
            flagWaitingForResponse = PR_FALSE;
            serverRead = PR_TRUE;

            // parse the response from the fcgi server
            parser->parse(*(server.from), *(client.to));
            if (parser->getServerError().length()) {
                request.log(LOG_FAILURE, GetString(DBT_application_stderr_msg), parser->getServerError().data());
            }
            PluginError parseErr = parser->getLastError();
            if (parseErr != NO_FCGI_ERROR && !fcgiServer->config->remoteServer && parseErr == OVER_LOADED) {
                if (sendRequestToStub(RQ_OVERLOAD, PR_FALSE) == PR_SUCCESS)
                    goto resendauth;
            }
        }

        if (serverRead == PR_TRUE)
            serverRead = (!parser->exitStatusSet);

        if ((clientRead == PR_FALSE) && (serverRead == PR_FALSE) && (!flagWaitingForResponse) &&
            (!client.to->hasData()) && (!server.to->hasData())) {
            done = PR_TRUE;
        }

    } // for

    // If there is some unsent data in server channel...
    if (server.from->hasData()) {
        channel.setClientError();  // Do not reuse the FcgiServerChannel
    }

    // If we never got a response...
    if (flagWaitingForResponse) {
        if (!client.isReadable())
            channel.setServerError(); // Do not reuse the FcgiServerChannel
        return PR_FAILURE;
    }

    if (!done) {
        channel.setServerError(); // Do not reuse the FcgiServerChannel
        return PR_FAILURE;
    }

    parser->parseHttpHeader(*(client.to));
    if (!parser->authorized) {
        while (client.to->hasData()) {
            client.send();
        }

       lastError = FCGI_NO_AUTHORIZATION;
   }

    return PR_SUCCESS;
}
