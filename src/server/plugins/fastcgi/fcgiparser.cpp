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

#include "frame/http.h"
#include "frame/httpfilter.h"
#include "frame/log.h"
#include "support/NSString.h"
#include "fastcgi.h"
#include "fcgirequest.h"
#include "circularbuffer.h"
#include "fastcgii18n.h"
#include "fcgiparser.h"

FcgiParser::FcgiParser(FcgiRequest *req, PRUint8 role) 
: request(req), fcgiRole(role) {
    authHeaderPrefixLen= strlen(AUTH_HEADER_PREFIX);
    reset();
}

void FcgiParser::reset() {
    waitingForBeginRequest = PR_TRUE;
    waitingForEndRequest = PR_TRUE;
    waitingForDataParse = PR_TRUE;
    lastError = NO_FCGI_ERROR;
    dataLen    = 0;
    exitStatus = 0;
    exitStatusSet = PR_FALSE;
    authorized = PR_FALSE;
    httpHeader.clear();
    serverError.clear();
}

PRStatus FcgiParser::formatRequest(CircularBuffer& from, CircularBuffer& to, PRUint8 streamType) {
    int toLen = 0; 
    int fromLen = 0; 
    char *toBuf = NULL;
    int available = to.requestSpace(toBuf, toLen) - sizeof(FCGI_Header);
    int dataSize = from.hasData();
    int dataToBeMoved = min(dataSize, available);
    if(dataToBeMoved > 0) {
        if(toBuf) {
            //if(makePacketHeader(streamType, maxLen, to) == PR_SUCCESS) {
            if(makePacketHeader(streamType, dataToBeMoved, to) == PR_SUCCESS) {
                if(from.move(to, dataToBeMoved) < dataToBeMoved)
                        return PR_FAILURE;
            } else
                return PR_FAILURE;
        }
    }

    return PR_SUCCESS;
}

PRStatus FcgiParser::parse(CircularBuffer& from, CircularBuffer& to) {
    int len = 0;
    char *endReqBodyPtr = (char *)(&endRequestBody);
    PRBool noDataMoved = PR_FALSE;
    serverError.clear(); //clear logs.

    while((len = from.hasData()) > 0 && !exitStatusSet) {
        if(noDataMoved) {
            //nothing was moved - could be because the to buffer was full
            //so break out and let the buffer be cleared.
            break;
        }
            
        //look for a complete header
        if(waitingForBeginRequest) {
            if(len < sizeof(FCGI_Header))
                return PR_SUCCESS;     // incomplete header  wait till we get the complete header

            int l = from.getData((char *)&header, sizeof(FCGI_Header));
            from.releaseData(l);
            len -= l;

            //check the version
            if(header.version != FCGI_VERSION_1) {
                lastError = INVALID_VERSION;
                request->log(LOG_FAILURE, GetString(DBT_invalid_header_version), header.version);
                return PR_FAILURE;
            }

            //check the header type
            if(header.type > FCGI_MAXTYPE) {
                lastError = INVALID_TYPE;
                request->log(LOG_FAILURE, GetString(DBT_invalid_header_type), header.type);
                return PR_FAILURE;
            }

            dataLen = (header.contentLengthB1 << 8) + header.contentLengthB0;
            waitingForBeginRequest = PR_FALSE;
        }

        //got the header; process the data
        len = min(dataLen, len);

        if(len < 0) {
            lastError = INVALID_RECORD;
            return PR_FAILURE;
        }

        switch(header.type) {
            case FCGI_STDOUT:
                if(len > 0) {
                    if(fcgiRole == FCGI_AUTHORIZER || waitingForDataParse) {
                        char *buf = (char *)MALLOC(len);
                        memset(buf, 0, len);
                        len = from.getData(buf, len);
                        httpHeader.append(buf, len);
                        from.releaseData(len);
                        FREE(buf);
                        buf = NULL;

                    } else {
                        len = from.move(to, len);
                        if(len < 1)
                            noDataMoved = PR_TRUE;
                    }

                    dataLen -= len;
                }
                break;

            case FCGI_STDERR:
                if(len > 0) {
                    char *buf = (char *)MALLOC(len);;
                    memset(buf, 0, len);
                    len = from.getData(buf, len);
                    if(len)
                        serverError.append(buf, len);
                    from.releaseData(len);
                    FREE(buf);
                    dataLen -= len;
                }
                break;

              case FCGI_END_REQUEST:
                if(waitingForEndRequest) {
                    if(dataLen != sizeof(FCGI_EndRequestBody)) {
                        lastError = INVALID_RECORD;
                        request->log(LOG_FAILURE, GetString(DBT_invalid_end_request_record), dataLen);
                        return PR_FAILURE;
                    }

                    len = from.getData((char *)&endRequestBody, sizeof(FCGI_EndRequestBody));
                    if(len < sizeof(FCGI_EndRequestBody)) { 
                        //incomplete FCGI_EndRequestBody data - hence 
                        //return and wait for additional data
                        return PR_SUCCESS;
                    }
                    waitingForEndRequest = PR_FALSE;
                    from.releaseData(len);
                    dataLen -= len;
                }

                switch(endRequestBody.protocolStatus) {
                    case FCGI_REQUEST_COMPLETE:
                        lastError = NO_FCGI_ERROR;
                        break;
                    case FCGI_CANT_MPX_CONN:
                        lastError = CANT_MPX;
                        request->log(LOG_FAILURE, GetString(DBT_cannot_multiplex));
                        break;
                    case FCGI_OVERLOADED:
                        lastError = OVER_LOADED;
                        request->log(LOG_FAILURE, GetString(DBT_overloaded_status));
                        break;
                    case FCGI_UNKNOWN_ROLE:
                        lastError = UNKNOWN_ROLE;
                        request->log(LOG_FAILURE, GetString(DBT_unknown_fcgi_role));
                        break;
                    default:
                        lastError = INVALID_RECORD;
                } //switch protocolstatus

                exitStatus = (endRequestBody.appStatusB3 << 24) +
                             (endRequestBody.appStatusB2 << 16) +
                             (endRequestBody.appStatusB1 << 8)  +
                             (endRequestBody.appStatusB0);

                if(!header.paddingLength) 
                    exitStatusSet = PR_TRUE;

                break;

              case FCGI_GET_VALUES_RESULT:
                /* yet to implement */

              case FCGI_UNKNOWN_TYPE:
                /* yet to implement */
                /*
                 * Ignore unknown packet types from the FastCGI server.
                 */

            default:
                from.releaseData(len);
                dataLen -= len;
        } //switch(header.type)

        if (dataLen == 0) {
          if (header.paddingLength > 0) {
            len = min(header.paddingLength, from.hasData());
            from.releaseData(len);
            header.paddingLength -= len;
          }
          /*
           * If we're done with the data in the packet, then start looking for
           * the next header.
           */
          if (header.paddingLength == 0) {
              waitingForBeginRequest = PR_TRUE;
              if(!waitingForEndRequest && !exitStatusSet)
                  exitStatusSet = PR_TRUE;
          }
        }
    } // while (len > 0)

    return PR_SUCCESS;
}

PRStatus FcgiParser::parseHttpHeader(CircularBuffer& to) {
    if(!waitingForDataParse)
        return PR_SUCCESS;

    const char *data = httpHeader.data();
    int len = httpHeader.length();
    PRUint8 flag = 0;
    while(len-- && flag < 2) {
        switch(*data) {
            case '\r':
                break;
            case '\n':
                flag++;
                break;
            default:
                flag = 0;
                break;
        }

        data++;
    }

    /*
     * Return (to be called later when we have more data)
     */

    if(flag < 2)
        return PR_SUCCESS;

    waitingForDataParse = PR_FALSE;
    Request *rq = request->getOrigRequest();

    pblock *authpb = NULL;
    if(fcgiRole == FCGI_AUTHORIZER) {
        authpb = pblock_create(rq->srvhdrs->hsize);
    }

    register int x ,y;
    register char c;
    int nh;
    char t[REQ_MAX_LINE];
    PRBool headerEnd = PR_FALSE;
    char* statusHeader = pblock_findval("status", rq->srvhdrs);
    char *next = const_cast<char *>(httpHeader.data());

    nh = 0;
    x = 0; y = -1;
 
    for(; !headerEnd;) {
        c = *(next++);
        switch(c) {
        case CR:
            // Silently ignore CRs
            break;

        case LF:
            if (x == 0) {
                headerEnd = PR_TRUE;
                break; 
            }

            t[x] = '\0';
            if(y == -1) {
                request->log(LOG_FAILURE,  "name without value: got line \"%s\"", t);
                return PR_FAILURE;
            }
            while(t[y] && isspace(t[y])) ++y;

            // Do not change the status header to 200 if it was already set
            // This would happen only if it were a cgi error handler
            // and so the status had been already set on the request
            // originally
            if (!statusHeader || // If we don't already have a Status: header
                PL_strcmp(t, "status") || // or this isn't a Status: header
                PL_strncmp(&t[y], "200", 3)) // or this isn't "Status: 200"
            {
                if(!PL_strcmp(t, "content-type")) {
                    pb_param* pParam = pblock_remove ( "content-type", rq->srvhdrs );
                    if ( pParam ) param_free ( pParam );
                }
                if(fcgiRole == FCGI_AUTHORIZER) {
                    pblock_nvinsert(t, &t[y], authpb);
                } else {
                    pblock_nvinsert(t, &t[y], rq->srvhdrs);
                } // !FCGI_AUTHORIZER
            }

            x = 0;
            y = -1;
            ++nh;
            break;

        case ':':
            if(y == -1) {
                y = x+1;
                c = '\0';
            }

        default:
            t[x++] = ((y == -1) && isupper(c) ? tolower(c) : c);

        }
    } // for

    if(fcgiRole == FCGI_AUTHORIZER) {
        if(parseAuthHeaders(authpb) != PR_SUCCESS) {
            pblock_free(authpb);
            return PR_FAILURE;
        }

        pblock_copy(authpb, rq->srvhdrs);
        pblock_free(authpb);

    } else {

        /*
         * We're done scanning the FCGI script's header output.  Now
         * we have to write to the client:  status, FCGI header, and
         * any over-read FCGI output.
         */
        char *s;
        char *l = pblock_findval("location", rq->srvhdrs);

        if((s = pblock_findval("status", rq->srvhdrs))) {
            if((strlen(s) < 3) ||
               (!isdigit(s[0]) || (!isdigit(s[1])) || (!isdigit(s[2])))) {
                s = NULL;
            }
            else {
              char ch = s[3];
              s[3] = '\0';
              int statusNum = atoi(s);
              s[3] = ch;

              rq->status_num = statusNum;
            }
        }

        if(!s) {
            if (l)
                pblock_nvinsert("url", l, rq->vars);
            protocol_status(request->getOrigSession(), request->getOrigRequest(), (l ? PROTOCOL_REDIRECT : PROTOCOL_OK), NULL);
        }
    }

    len = next - httpHeader.data();
    len = httpHeader.length() - len;

    if(len < 0)
        return PR_FAILURE;

    /*
     * Only send the body for methods other than HEAD.
     */
    if(!request->isHead()) {
        if(len > 0) {
            if(to.addData(next, len) != len)
                return PR_FAILURE;
        }
    }

    next = NULL;
    return PR_SUCCESS;
}


PRStatus FcgiParser::parseAuthHeaders(pblock *pb) {
    Request *rq = request->getOrigRequest();
    authHeaderPrefixLen = strlen(AUTH_HEADER_PREFIX);
    char *s;

    if(pb) {
        if((s = pblock_findval("status", pb))) {
            if((strlen(s) < 3) ||
               (!isdigit(s[0]) || (!isdigit(s[1])) || (!isdigit(s[2])))) {
                s = NULL;
            }
            else {
              char ch = s[3];
              s[3] = '\0';
              int statusNum = atoi(s);
              s[3] = ch;

              rq->status_num = statusNum;
              if(statusNum == 200)
                authorized = PR_TRUE;
            }
        }

        if(!authorized) {
            protocol_status(request->getOrigSession(), request->getOrigRequest(), PROTOCOL_UNAUTHORIZED, NULL);
        } else {
            //retain only the headers starting with "Varaiable-"
            for (int i = 0; i < pb->hsize; i++) {
                pb_entry *p = pb->ht[i];
                while (p) {
                    const char *name = PL_strdup(p->param->name);
                    const char *value = PL_strdup(p->param->value);

                    if (*name == 'v' && (!PL_strcmp(name, "variable-"))) {
                        pb_param *pparam = pblock_fr((char *)name, pb, PR_TRUE);
                        param_free(pparam);
                        name += authHeaderPrefixLen;   // remove the prefix from name
                        if(*name == 'r' && (!PL_strcmp(name, "remote_user"))) {
                            pblock_nvinsert("auth-user", value, rq->vars);
                        }
            if(*name == 'a' && (!PL_strcmp(name, "auth_type"))) {
                            pblock_nvinsert("auth-type", value, rq->vars);
                        }

                        pblock_nvinsert(name, value, rq->headers);
                    }

                   PL_strfree((char *)name);
                   PL_strfree((char *)value);

                   p = p->next;
                } //while
            } //for
        } //if

    } else {
        lastError = INVALID_HTTP_HEADER;
        request->log(LOG_FAILURE, GetString(DBT_invalid_response));
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

PRStatus FcgiParser::makePacketHeader(int type, int len, CircularBuffer& buf, PRBool managementRecord) {
    int headerSize = sizeof(FCGI_Header);
    PR_ASSERT(type > 0 && type <= FCGI_MAXTYPE);

    /*
     * Assemble and queue the packet header.
     */
    header.version = FCGI_VERSION_1;
    header.type = type;

    if(!managementRecord) {
        header.requestIdB1 = (request->getRequestId() >> 8) & 0xff;
        header.requestIdB0 = (request->getRequestId()) & 0xff;
    } else {
        header.requestIdB1 = 0x00;
        header.requestIdB0 = 0x00;
    }

    header.contentLengthB1 = MSB(len);
    header.contentLengthB0 = LSB(len);
    header.paddingLength = 0;
    header.reserved = 0;

    int l = buf.addData((char *)&header, headerSize);
    return ((l < headerSize) ? PR_FAILURE : PR_SUCCESS);
}
