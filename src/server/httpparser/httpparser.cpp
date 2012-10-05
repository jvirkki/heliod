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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "netsite.h"

#include "httpparser.h"

//-----------------------------------------------------------------------------
// HttpParser static variables
//-----------------------------------------------------------------------------

const unsigned char HttpParser::CHAR_CR = '\r';
const unsigned char HttpParser::CHAR_LF = '\n';
const unsigned char HttpParser::CHAR_SP = ' ';
const unsigned char HttpParser::CHAR_HT = '\t';
const unsigned char HttpParser::CHAR_COLON = ':';
const unsigned char HttpParser::CHAR_QUOTE = '"';
const unsigned char HttpParser::CHAR_SLASH = '/';
const unsigned char HttpParser::CHAR_BACKSLASH = '\\';
const unsigned char HttpParser::CHAR_QUESTION  = '?';
const unsigned char HttpParser::CHAR_DOT = '.';
const unsigned char HttpParser::CHAR_NUL = '\0';
PRBool HttpParser::flagLutsInitialized = PR_FALSE;
PRPackedBool HttpParser::isSeparator[256];
PRPackedBool HttpParser::isCrLfLwsNul[256];
PRPackedBool HttpParser::isCrLfLwsSlashNul[256];
PRPackedBool HttpParser::isCrLfLwsQuestionNul[256];
PRPackedBool HttpParser::isCrLfQuoteNul[256];
PRPackedBool HttpParser::isCrLfQuoteBackslashNul[256];
PRPackedBool HttpParser::isDigit[256];
unsigned char HttpParser::toLowerExceptCrLfColon[256];

//-----------------------------------------------------------------------------
// Force lookup table initialization through static construction of HttpParser
//-----------------------------------------------------------------------------

static HttpRequestParser parser;

//-----------------------------------------------------------------------------
// hashheader
//-----------------------------------------------------------------------------

inline unsigned long hashheader(HttpString name)
{
    // Simple, quick hash function
    return name.len + *name.p;
}

//-----------------------------------------------------------------------------
// HttpHeaderFieldNode
//-----------------------------------------------------------------------------

struct HttpHeaderFieldNode : public HttpHeaderField {
    int hash;
    HttpHeaderFieldNode *prev;
};

//-----------------------------------------------------------------------------
// HttpHeaderFieldKeyNode
//-----------------------------------------------------------------------------

struct HttpHeaderFieldKeyNode {
    HttpHeaderFieldKeyNode(HttpString name)
    : name(HttpParser::dupString(name)), key(NULL), shortcut(&dummy), next(NULL)
    { }

    ~HttpHeaderFieldKeyNode()
    {
        HttpParser::freeString(name.p);
    }

    HttpString name;
    HttpHeaderFieldKey *key;
    HttpHeaderField *dummy;
    HttpHeaderField **shortcut;
    HttpHeaderFieldKeyNode *next;
};

//-----------------------------------------------------------------------------
// HttpParser::HttpParser
//-----------------------------------------------------------------------------

HttpParser::HttpParser()
: countHeaderFieldNodes(0),
  countHeaderFields(0)
{
    // WorkShop 6.2 generates faster code if these aren't static or const
    maxHeaderFields = 128;
    maskHeaderFieldHash = 0x7F;
    sizeHeaderFieldHash = maskHeaderFieldHash + 1;

    // Allocate the header arrays
    headerFieldNodes = (HttpHeaderFieldNode*)calloc(maxHeaderFields, sizeof(headerFieldNodes[0]));
    headerFieldNodeHash = (HttpHeaderFieldNode**)calloc(sizeHeaderFieldHash, sizeof(headerFieldNodeHash[0]));
    keyNodeHash = (HttpHeaderFieldKeyNode**)calloc(sizeHeaderFieldHash, sizeof(keyNodeHash[0]));
    headerFields = (HttpHeaderField**)malloc(sizeof(headerFields[0]) * maxHeaderFields);
    keys = (HttpHeaderFieldKey**)malloc(sizeof(keys[0]) * maxHeaderFields);

    // Initialize static lookup tables (not MT safe)
    if (!flagLutsInitialized)
        initLuts();
}

//-----------------------------------------------------------------------------
// HttpParser::~HttpParser
//-----------------------------------------------------------------------------

HttpParser::~HttpParser()
{
    // Discard the individual HttpHeaderFieldKeyNodes
    if (keyNodeHash) {
        for (int i = 0; i < sizeHeaderFieldHash; i++) {
            HttpHeaderFieldKeyNode *keynode = keyNodeHash[i];
            while (keynode) {
                HttpHeaderFieldKeyNode *next = keynode->next;
                delete keynode;
                keynode = next;
            }
        }
    }

    free(headerFieldNodes);
    free(headerFieldNodeHash);
    free(keyNodeHash);
    free(headerFields);
    free(keys);
}

//-----------------------------------------------------------------------------
// HttpParser::initLuts
//-----------------------------------------------------------------------------

void HttpParser::initLuts()
{
    // RFC 2616 2.2
    isSeparator['('] = PR_TRUE;
    isSeparator[')'] = PR_TRUE;
    isSeparator['<'] = PR_TRUE;
    isSeparator['>'] = PR_TRUE;
    isSeparator['@'] = PR_TRUE;
    isSeparator[','] = PR_TRUE;
    isSeparator[';'] = PR_TRUE;
    isSeparator[':'] = PR_TRUE;
    isSeparator['\\'] = PR_TRUE;
    isSeparator['"'] = PR_TRUE;
    isSeparator['/'] = PR_TRUE;
    isSeparator['['] = PR_TRUE;
    isSeparator[']'] = PR_TRUE;
    isSeparator['?'] = PR_TRUE;
    isSeparator['='] = PR_TRUE;
    isSeparator['{'] = PR_TRUE;
    isSeparator['}'] = PR_TRUE;
    isSeparator[CHAR_SP] = PR_TRUE;
    isSeparator[CHAR_HT] = PR_TRUE;

    // We treat the following as RFC 2616 2.2 token separators due to the way
    // we handle RFC 2616 4.2 multiple line field values
    isSeparator[CHAR_CR] = PR_TRUE;
    isSeparator[CHAR_LF] = PR_TRUE;

    isCrLfLwsNul[CHAR_CR] = PR_TRUE;
    isCrLfLwsNul[CHAR_LF] = PR_TRUE;
    isCrLfLwsNul[CHAR_SP] = PR_TRUE;
    isCrLfLwsNul[CHAR_HT] = PR_TRUE;
    isCrLfLwsNul[CHAR_NUL] = PR_TRUE;

    isCrLfLwsSlashNul[CHAR_CR] = PR_TRUE;
    isCrLfLwsSlashNul[CHAR_LF] = PR_TRUE;
    isCrLfLwsSlashNul[CHAR_SP] = PR_TRUE;
    isCrLfLwsSlashNul[CHAR_HT] = PR_TRUE;
    isCrLfLwsSlashNul[CHAR_SLASH] = PR_TRUE;
    isCrLfLwsSlashNul[CHAR_NUL] = PR_TRUE;

    isCrLfLwsQuestionNul[CHAR_CR] = PR_TRUE;
    isCrLfLwsQuestionNul[CHAR_LF] = PR_TRUE;
    isCrLfLwsQuestionNul[CHAR_SP] = PR_TRUE;
    isCrLfLwsQuestionNul[CHAR_HT] = PR_TRUE;
    isCrLfLwsQuestionNul[CHAR_QUESTION] = PR_TRUE;
    isCrLfLwsQuestionNul[CHAR_NUL] = PR_TRUE;

    isCrLfQuoteNul[CHAR_CR] = PR_TRUE;
    isCrLfQuoteNul[CHAR_LF] = PR_TRUE;
    isCrLfQuoteNul[CHAR_QUOTE] = PR_TRUE;
    isCrLfQuoteNul[CHAR_NUL] = PR_TRUE;

    isCrLfQuoteBackslashNul[CHAR_CR] = PR_TRUE;
    isCrLfQuoteBackslashNul[CHAR_LF] = PR_TRUE;
    isCrLfQuoteBackslashNul[CHAR_QUOTE] = PR_TRUE;
    isCrLfQuoteBackslashNul[CHAR_BACKSLASH] = PR_TRUE;
    isCrLfQuoteBackslashNul[CHAR_NUL] = PR_TRUE;

    isDigit['0'] = PR_TRUE;
    isDigit['1'] = PR_TRUE;
    isDigit['2'] = PR_TRUE;
    isDigit['3'] = PR_TRUE;
    isDigit['4'] = PR_TRUE;
    isDigit['5'] = PR_TRUE;
    isDigit['6'] = PR_TRUE;
    isDigit['7'] = PR_TRUE;
    isDigit['8'] = PR_TRUE;
    isDigit['9'] = PR_TRUE;

    int i;
    for (i = 0; i < sizeof(toLowerExceptCrLfColon); i++) {
        switch (i) {
        case CHAR_CR:
        case CHAR_LF:
        case CHAR_COLON:
        case CHAR_NUL:
            toLowerExceptCrLfColon[i] = CHAR_NUL;
            break;
        default:
            toLowerExceptCrLfColon[i] = tolower(i);
            break;
        }
    }

    flagLutsInitialized = PR_TRUE;
}

//-----------------------------------------------------------------------------
// HttpParser::setHeaderFieldKey
//-----------------------------------------------------------------------------

void HttpParser::setHeaderFieldKey(HttpString name, HttpHeaderFieldKey *key)
{
    HttpHeaderFieldKeyNode *keynode = getHeaderFieldKeyNode(name);
    keynode->key = key;
}

//-----------------------------------------------------------------------------
// HttpParser::setHeaderFieldShortcut
//-----------------------------------------------------------------------------

void HttpParser::setHeaderFieldShortcut(HttpString name, HttpHeaderField **shortcut)
{
    HttpHeaderFieldKeyNode *keynode = getHeaderFieldKeyNode(name);
    keynode->shortcut = shortcut;
}

//-----------------------------------------------------------------------------
// HttpParser::getHeaderFieldKeyNode
//-----------------------------------------------------------------------------

HttpHeaderFieldKeyNode* HttpParser::getHeaderFieldKeyNode(HttpString name)
{
    int hash = hashheader(name) & maskHeaderFieldHash;

    // Look for an existing key node
    HttpHeaderFieldKeyNode *keynode = keyNodeHash[hash];
    while (keynode && keynode->name != name)
        keynode = keynode->next;

    // If no existing key node was found...
    if (!keynode) {
        // Create a new key node
        keynode = new HttpHeaderFieldKeyNode(name);
        keynode->next = keyNodeHash[hash];
        keyNodeHash[hash] = keynode;
    }

    return keynode;
}

//-----------------------------------------------------------------------------
// HttpRequestParser::HttpRequestParser
//-----------------------------------------------------------------------------

HttpRequestParser::HttpRequestParser()
{
    // Register shortcuts to common headers
    setHeaderFieldShortcut(HttpString((char*)"host"), &host);
    setHeaderFieldShortcut(HttpString((char*)"connection"), &connection);
}

//-----------------------------------------------------------------------------
// HttpParser::reset
//-----------------------------------------------------------------------------

void HttpParser::reset()
{
    // Clear the header node hash table
    int countDirtyHeaderFieldNodes = countHeaderFieldNodes + 1;
    if (countDirtyHeaderFieldNodes > maxHeaderFields)
        countDirtyHeaderFieldNodes = maxHeaderFields;
    for (int i = 0; i < countDirtyHeaderFieldNodes; i++)
        headerFieldNodeHash[headerFieldNodes[i].hash] = NULL;

    state = STATE_STARTLINE;
    startline.p = NULL;
    startline.len = 0;
    countHeaderFieldNodes = 0;
    countHeaderFields = 0;
}

//-----------------------------------------------------------------------------
// HttpRequestParser::reset
//-----------------------------------------------------------------------------

void HttpRequestParser::reset()
{
    state = STATE_REQUEST_BEGIN;
    memset(&request, 0, sizeof(request));
    host = NULL;
    connection = NULL;
    HttpParser::reset();
}

//-----------------------------------------------------------------------------
// HttpResponseParser::reset
//-----------------------------------------------------------------------------

void HttpResponseParser::reset()
{
    state = STATE_RESPONSE_BEGIN;
    memset(&response, 0, sizeof(response));
    HttpParser::reset();
}

//-----------------------------------------------------------------------------
// HttpRequestParser::parseStartline
//-----------------------------------------------------------------------------

unsigned char* HttpRequestParser::parseStartline(unsigned char *c)
{
    // Until we hit a nul...
    while (*c) {
        // Process one or more bytes of data according to our current state
        switch (state) {
        case STATE_REQUEST_BEGIN:
            // Optionally skip CR/LF and advance to STATE_REQUEST_METHOD
            while (*c == CHAR_CR || *c == CHAR_LF) c++;
            if (*c)
                state = STATE_REQUEST_METHOD;
            break;

        case STATE_REQUEST_METHOD:
            // Parse the method
            if (!startline.p)
                startline.p = (char*)c;
            if (!request.method.p)
                request.method.p = (char*)c;
            while (!isCrLfLwsNul[*c]) c++;
            if (*c) {
                switch (*c) {
                case CHAR_SP:
                case CHAR_HT:
                    // Found end of method
                    request.method.len = (char*)c - request.method.p;
                    c++;
                    state = STATE_REQUEST_LWS_URL;
                    break;

                case CHAR_CR:
                case CHAR_LF:
                    // Unexpected CR/LF
                    state = STATE_REQUEST_ERROR;
                    break;
                }
            }
            break;

        case STATE_REQUEST_LWS_URL:
            // Optionally skip LWS and advance to STATE_REQUEST_URL
            while (*c == CHAR_SP || *c == CHAR_HT) c++;
            if (*c)
                state = STATE_REQUEST_URL;
            break;

        case STATE_REQUEST_URL:
            // Beginning of the URL; what type of URL is it?
            request.url.p = (char*)c;
            switch (*c) {
            case CHAR_SLASH:
                // Beginning of path (e.g. "/path")
                state = STATE_REQUEST_PATH;
                break;

            default:
                // Beginning of scheme (e.g. "http" in "http://host/path")
                state = STATE_REQUEST_SCHEME;
                break;
            }
            break;

        case STATE_REQUEST_SCHEME:
            // Parse the scheme, converting to lowercase
            if (!request.scheme.p)
                request.scheme.p = (char*)c;
            for (;;) {
                unsigned char x = toLowerExceptCrLfColon[*c];
                if (!x) break;
                *c++ = x;
            }
            if (*c) {
                // Found end of scheme
                request.scheme.len = (char*)c - request.scheme.p;
                switch (*c) {
                case CHAR_COLON:
                    // Look for slash followed by host/path
                    state = STATE_REQUEST_SLASH_HOSTPATH;
                    break;

                default:
                    // Scheme ended unexpectedly
                    state = STATE_REQUEST_ERROR;
                    break;
                }
                c++;
            }
            break;

        case STATE_REQUEST_SLASH_HOSTPATH:
            // Consume a slash and advance to STATE_REQUEST_HOSTPATH
            switch (*c) {
            case CHAR_SLASH:
                // Got first slash following scheme (e.g. "http:/")
                state = STATE_REQUEST_HOSTPATH;
                break;

            default:
                // Missing slash after scheme (e.g. "http:path")
                state = STATE_REQUEST_ERROR;
                break;
            }
            c++;
            break;

        case STATE_REQUEST_HOSTPATH:
            // Advance to STATE_REQUEST_HOST or STATE_REQUEST_PATH
            switch (*c) {
            case CHAR_SLASH:
                // Look for host (e.g. URL of the form "http://host/path")
                state = STATE_REQUEST_HOST;
                break;

            default:
                // Look for path (e.g. URL of the form "http:/path")
                state = STATE_REQUEST_PATH;
                break;
            }
            c++;
            break;

        case STATE_REQUEST_HOST:
            // Parse the host
            if (!request.host.p)
                request.host.p = (char*)c;
            while (!isCrLfLwsSlashNul[*c]) c++;
            if (*c) {
                // Found end of host
                request.host.len = (char*)c - request.host.p;
                switch (*c) {
                case CHAR_SLASH:
                    // Look for path (e.g. "/path")
                    state = STATE_REQUEST_PATH;
                    break;

                default:
                    // No path supplied
                    state = STATE_REQUEST_ERROR;
                    break;
                }
            }
            break;

        case STATE_REQUEST_PATH:
            // Parse the path
            if (!request.path.p)
                request.path.p = (char*)c;
            while (!isCrLfLwsQuestionNul[*c]) c++;
            if (*c) {
                // Found end of path and possibly end of url
                request.path.len = (char*)c - request.path.p;
                request.url.len = (char*)c - request.url.p;
                startline.len = (char*)c - startline.p;
                switch (*c) {
                case CHAR_SP:
                case CHAR_HT:
                    // Look for protocol (e.g. " HTTP/1.0")
                    state = STATE_REQUEST_LWS_PROTOCOL;
                    break;

                case CHAR_QUESTION:
                    // Look for query (e.g. "?foo=bar")
                    state = STATE_REQUEST_QUERY;
                    break;

                case CHAR_CR:
                    // No protocol/query supplied (HTTP/0.9)
                    state = STATE_REQUEST_LF_DONE;
                    break;

                case CHAR_LF:
                    // No protocol/query supplied (HTTP/0.9)
                    state = STATE_REQUEST_DONE;
                    break;
                }
                c++;
            }
            break;

        case STATE_REQUEST_QUERY:
            // Parse the query
            if (!request.query.p)
                request.query.p = (char*)c;
            while (!isCrLfLwsNul[*c]) c++;
            if (*c) {
                // Found end of query and url
                request.query.len = (char*)c - request.query.p;
                request.url.len = (char*)c - request.url.p;
                startline.len = (char*)c - startline.p;
                switch (*c) {
                case CHAR_SP:
                case CHAR_HT:
                    // Look for protocol (e.g. " HTTP/1.0")
                    state = STATE_REQUEST_LWS_PROTOCOL;
                    break;

                case CHAR_CR:
                    // No protocol/query supplied (HTTP/0.9)
                    state = STATE_REQUEST_LF_DONE;
                    break;

                case CHAR_LF:
                    // No protocol/query supplied (HTTP/0.9)
                    state = STATE_REQUEST_DONE;
                    break;
                }
                c++;
            }
            break;

        case STATE_REQUEST_LWS_PROTOCOL:
            // Optionally skip LWS and advance to STATE_REQUEST_PROTOCOL
            while (*c == CHAR_SP || *c == CHAR_HT) c++;
            if (*c)
                state = STATE_REQUEST_PROTOCOL;
            break;

        case STATE_REQUEST_PROTOCOL:
            // Parse the protocol
            if (!request.protocol.p)
                request.protocol.p = (char*)c;
            while (!isCrLfLwsSlashNul[*c]) c++;
            if (*c) {
                // Found end of protocol name
                request.protocol.len = (char*)c - request.protocol.p;
                startline.len = (char*)c - startline.p;
                switch (*c) {
                case CHAR_SLASH:
                    // Look for protocol major version
                    state = STATE_REQUEST_PROTOCOLMAJOR;
                    break;

                case CHAR_SP:
                    // XXX Space after protocol; perhaps a brain dead browser
                    // (i.e. Navigator 4.x) sent a URI with unencoded spaces?
                    if (request.path.p) {
                        // Turns out we weren't parsing protocol after all
                        request.protocol.p = NULL;
                        request.protocol.len = 0;

                        if (request.query.p) {
                            // Resume parsing query
                            c = (unsigned char*)request.query.p + request.query.len;
                            state = STATE_REQUEST_QUERY;
                        } else {
                            // Resume parsing path
                            c = (unsigned char*)request.path.p + request.path.len;
                            state = STATE_REQUEST_PATH;
                        }
                    }
                    break;

                case CHAR_CR:
                    // No version supplied
                    state = STATE_REQUEST_LF_DONE;
                    break;

                case CHAR_LF:
                    // No version supplied
                    state = STATE_REQUEST_DONE;
                    break;
                }
                c++;
            }
            break;

        case STATE_REQUEST_PROTOCOLMAJOR:
            // Parse protocol major version
            while (isDigit[*c]) {
                request.major = request.major * 10 + (*c - '0');
                c++;
            }
            if (*c) {
                // Found end of major version
                request.protocol.len = (char*)c - request.protocol.p;
                startline.len = (char*)c - startline.p;
                switch (*c) {
                case CHAR_DOT:
                    // Minor version comes next
                    state = STATE_REQUEST_PROTOCOLMINOR;
                    break;

                case CHAR_CR:
                    // No minor version supplied
                    state = STATE_REQUEST_LF_DONE;
                    break;

                case CHAR_LF:
                    // No minor version supplied
                    state = STATE_REQUEST_DONE;
                    break;

                default:
                    // Oddly formatted protocol name/version
                    request.major = 0;
                    state = STATE_REQUEST_PROTOCOL;
                    break;
                }
                c++;
            }
            break;

        case STATE_REQUEST_PROTOCOLMINOR:
            // Parse protocol minor version
            while (isDigit[*c]) {
                request.minor = request.minor * 10 + (*c - '0');
                c++;
            }
            if (*c) {
                // Found end of minor version
                request.protocol.len = (char*)c - request.protocol.p;
                startline.len = (char*)c - startline.p;
                switch (*c) {
                case CHAR_CR:
                    // No minor version supplied
                    state = STATE_REQUEST_LF_DONE;
                    break;

                case CHAR_LF:
                    // No minor version supplied
                    state = STATE_REQUEST_DONE;
                    break;

                default:
                    // Oddly formatted protocol name/version
                    request.major = 0;
                    request.minor = 0;
                    state = STATE_REQUEST_PROTOCOL;
                    break;
                }
                c++;
            }
            break;

        default:
            // STATE_REQUEST_DONE or STATE_REQUEST_ERROR
            goto done;
        }
    }

    if (state == STATE_REQUEST_DONE || state == STATE_REQUEST_ERROR) {
    done:
        // End of request startline
        doneStartline(c);
    }

    return c;
}

//-----------------------------------------------------------------------------
// HttpRequestParser::doneStartline
//-----------------------------------------------------------------------------

inline void HttpRequestParser::doneStartline(unsigned char *c)
{
    // Bail if the startline didn't parse ok
    if (state != STATE_REQUEST_DONE) {
        HttpParser::state = STATE_ERROR;
        return;
    }

    // Set HttpParser's state.  If no protocol was specified, we assume
    // HTTP/0.9 (i.e. no headers will follow)
    if (request.protocol.len)
        HttpParser::state = STATE_HEADER;
    else
        HttpParser::state = STATE_DONE;
}

//-----------------------------------------------------------------------------
// HttpResponseParser::parseStartline
//-----------------------------------------------------------------------------

unsigned char* HttpResponseParser::parseStartline(unsigned char *c)
{
    // Until we hit a nul...
    while (*c) {
        // Process one or more bytes of data according to our current state
        switch (state) {
        case STATE_RESPONSE_BEGIN:
            // Optionally skip CR/LF and advance to STATE_RESPONSE_PROTOCOL
            while (*c == CHAR_CR || *c == CHAR_LF) c++;
            if (*c)
                state = STATE_RESPONSE_PROTOCOL;
            break;

        case STATE_RESPONSE_PROTOCOL:
            // Parse the protocol
            if (!startline.p)
                startline.p = (char*)c;
            if (!response.protocol.p)
                response.protocol.p = (char*)c;
            while (!isCrLfLwsSlashNul[*c]) c++;
            if (*c) {
                // Found end of protocol name
                response.protocol.len = (char*)c - response.protocol.p;
                switch (*c) {
                case CHAR_SLASH:
                    // Look for protocol major version
                    state = STATE_RESPONSE_PROTOCOLMAJOR;
                    break;

                case CHAR_SP:
                case CHAR_HT:
                    // Look for code
                    state = STATE_RESPONSE_LWS_CODE;
                    break;

                default:
                    // Unexpected CR/LF
                    state = STATE_RESPONSE_ERROR;
                    break;
                }
                c++;
            }
            break;

        case STATE_RESPONSE_PROTOCOLMAJOR:
            // Parse protocol major version
            while (isDigit[*c]) {
                response.major = response.major * 10 + (*c - '0');
                c++;
            }
            if (*c) {
                // Found end of major version
                response.protocol.len = (char*)c - response.protocol.p;
                switch (*c) {
                case CHAR_DOT:
                    // Minor version comes next
                    state = STATE_RESPONSE_PROTOCOLMINOR;
                    break;

                case CHAR_SP:
                case CHAR_HT:
                    // Look for code
                    state = STATE_RESPONSE_LWS_CODE;
                    break;

                case CHAR_CR:
                case CHAR_LF:
                    // Unexpected CR/LF
                    state = STATE_RESPONSE_ERROR;
                    break;

                default:
                    // Oddly formatted protocol name/version
                    state = STATE_RESPONSE_PROTOCOL;
                    break;
                }
                c++;
            }
            break;

        case STATE_RESPONSE_PROTOCOLMINOR:
            // Parse protocol minor version
            while (isDigit[*c]) {
                response.minor = response.minor * 10 + (*c - '0');
                c++;
            }
            if (*c) {
                // Found end of minor version
                response.protocol.len = (char*)c - response.protocol.p;
                switch (*c) {
                case CHAR_SP:
                case CHAR_HT:
                    // Look for code
                    state = STATE_RESPONSE_LWS_CODE;
                    break;

                case CHAR_CR:
                case CHAR_LF:
                    // Unexpected CR/LF
                    state = STATE_RESPONSE_ERROR;
                    break;

                default:
                    // Oddly formatted protocol name/version
                    state = STATE_RESPONSE_PROTOCOL;
                    break;
                }
                c++;
            }
            break;

        case STATE_RESPONSE_LWS_CODE:
            // Optionally skip LWS and advance to STATE_RESPONSE_CODE
            while (*c == CHAR_SP || *c == CHAR_HT) c++;
            if (*c)
                state = STATE_RESPONSE_CODE;
            break;

        case STATE_RESPONSE_CODE:
            // Parse the code
            if (!response.code.p)
                response.code.p = (char*)c;
            while (!isCrLfLwsNul[*c]) c++;
            if (*c) {
                // Found end of code
                response.code.len = (char*)c - response.code.p;
                startline.len = (char*)c - startline.p;
                switch (*c) {
                case CHAR_SP:
                case CHAR_HT:
                    // Look for reason (e.g. " OK")
                    state = STATE_RESPONSE_LWS_REASON;
                    break;

                case CHAR_CR:
                    // No reason supplied, start on headers
                    state = STATE_RESPONSE_LF_DONE;
                    break;

                case CHAR_LF:
                    // No reason supplied, start on headers
                    state = STATE_RESPONSE_DONE;
                    break;
                }
                c++;
            }
            break;

        case STATE_RESPONSE_LWS_REASON:
            // Optionally skip LWS and advance to STATE_RESPONSE_REASON
            while (*c == CHAR_SP || *c == CHAR_HT) c++;
            if (*c)
                state = STATE_RESPONSE_REASON;
            break;

        case STATE_RESPONSE_REASON:
            // Parse the reason
            if (!response.reason.p)
                response.reason.p = (char*)c;
            while (*c && *c != CHAR_CR && *c != CHAR_LF) c++;
            if (*c) {
                // Found end of reason
                response.reason.len = (char*)c - response.reason.p;
                startline.len = (char*)c - startline.p;
                switch (*c) {
                case CHAR_CR:
                    // End of startline, start on headers
                    state = STATE_RESPONSE_LF_DONE;
                    break;

                case CHAR_LF:
                    // End of startline, start on headers
                    state = STATE_RESPONSE_DONE;
                    break;
                }
                c++;
            }
            break;

        case STATE_RESPONSE_LF_DONE:
            // Consume a LF then advance to STATE_RESPONSE_DONE
            if (*c != CHAR_LF) {
                state = STATE_RESPONSE_ERROR;
                break;
            }
            c++;
            state = STATE_RESPONSE_DONE;
            break;

        default:
            // STATE_RESPONSE_DONE or STATE_RESPONSE_ERROR
            goto done;
        }
    }

    if (state == STATE_RESPONSE_DONE || state == STATE_RESPONSE_ERROR) {
    done:
        // End of response startline
        doneStartline(c);
    }

    return c;
}

//-----------------------------------------------------------------------------
// HttpResponseParser::doneStartline
//-----------------------------------------------------------------------------

inline void HttpResponseParser::doneStartline(unsigned char *c)
{
    // Bail if the startline didn't parse ok
    if (state != STATE_RESPONSE_DONE) {
        HttpParser::state = STATE_ERROR;
        return;
    }

    // Set HttpParser's state
    HttpParser::state = STATE_HEADER;
}

//-----------------------------------------------------------------------------
// HttpParser::parseHeaderFields
//-----------------------------------------------------------------------------

inline unsigned char* HttpParser::parseHeaderFields(unsigned char *c)
{
    // Until we hit a nul...
    while (*c) {
        // Process one or more bytes of data according to our current state
        switch (state) {
        case STATE_LF_HEADER:
            // Consume a LF then advance to STATE_HEADER
            if (*c != CHAR_LF) {
                state = STATE_ERROR;
                break;
            }
            c++;
            state = STATE_HEADER;
            break;

        case STATE_HEADER:
            // Parse the beginning of a header line
            switch (*c) {
            case CHAR_SP:
            case CHAR_HT:
                // LWS following LF is a header value continuation
                if (countHeaderFieldNodes) {
                    // We weren't done with the previous header node after all
                    countHeaderFieldNodes--;

                    // Continue with header value
                    state = STATE_VALUE;
                } else {
                    // There's no header value to continue
                    state = STATE_ERROR;
                }
                break;

            case CHAR_CR:
                // End of headers
                c++;
                state = STATE_LF_DONE;
                break;

            case CHAR_LF:
                // End of headers
                c++;
                state = STATE_DONE;
                break;

            default:
                // Beginning of header name
                if (countHeaderFieldNodes < maxHeaderFields) {
                    headerFieldNodes[countHeaderFieldNodes].name.p = (char*)c;
                    state = STATE_NAME;
                } else {
                    state = STATE_ERROR;
                }
                break;
            }
            break;

        case STATE_NAME:
            // Parse name from "name: value", converting to lowercase
            for (;;) {
                unsigned char x = toLowerExceptCrLfColon[*c];
                if (!x) break;
                *c++ = x;
            }
            switch (*c) {
            case CHAR_CR:
            case CHAR_LF:
                // Unexpected CR/LF
                state = STATE_ERROR;
                break;

            case CHAR_COLON:
                // End of header name
                HttpHeaderFieldNode *node = &headerFieldNodes[countHeaderFieldNodes];
                node->name.len = (char*)c - node->name.p;
                if (node->name.len) {
                    // Add header name to hash table
                    node->hash = hashheader(node->name) & maskHeaderFieldHash;
                    node->prev = headerFieldNodeHash[node->hash];
                    headerFieldNodeHash[node->hash] = node;

                    // We now expect a header value
                    c++;
                    state = STATE_LWS_VALUE;
                } else {
                    // Zero length header name
                    state = STATE_ERROR;
                }
                break;
            }
            break;

        case STATE_LWS_VALUE:
            // Optionally skip LWS and advance to STATE_VALUE
            while (*c == CHAR_SP || *c == CHAR_HT) c++;
            if (*c) {
                // Beginning of header value
                headerFieldNodes[countHeaderFieldNodes].value.p = (char*)c;
                state = STATE_VALUE;
            }
            break;

        case STATE_VALUE:
            // Parse the value of a "name: value" header
            while (!isCrLfQuoteNul[*c]) c++;
            switch (*c) {
            case CHAR_CR:
                // End of header
                headerFieldNodes[countHeaderFieldNodes].value.len = (char*)c - headerFieldNodes[countHeaderFieldNodes].value.p;
                countHeaderFieldNodes++;
                c++;
                state = STATE_LF_HEADER;
                break;

            case CHAR_LF:
                // End of header
                headerFieldNodes[countHeaderFieldNodes].value.len = (char*)c - headerFieldNodes[countHeaderFieldNodes].value.p;
                countHeaderFieldNodes++;
                c++;
                state = STATE_HEADER;
                break;

            case CHAR_QUOTE:
                // Beginning of a quoted value
                c++;
                state = STATE_QUOTEDVALUE;
                break;
            }
            break;

        case STATE_QUOTEDVALUE:
            // Consume quoted string then return to STATE_VALUE
            while (!isCrLfQuoteBackslashNul[*c]) c++;
            switch (*c) {
            case CHAR_CR:
                // End of header
                headerFieldNodes[countHeaderFieldNodes].value.len = (char*)c - headerFieldNodes[countHeaderFieldNodes].value.p;
                countHeaderFieldNodes++;
                c++;
                state = STATE_LF_HEADER;
                break;

            case CHAR_LF:
                // End of header
                headerFieldNodes[countHeaderFieldNodes].value.len = (char*)c - headerFieldNodes[countHeaderFieldNodes].value.p;
                countHeaderFieldNodes++;
                c++;
                state = STATE_HEADER;
                break;

            case CHAR_QUOTE:
                // End of quoted string
                c++;
                state = STATE_VALUE;
                break;

            case CHAR_BACKSLASH:
                // Consume an escaped character
                c++;
                state = STATE_ESCAPEDQUOTEDVALUE;
                break;
            }
            break;

        case STATE_ESCAPEDQUOTEDVALUE:
            // Consume an escaped character then return to STATE_QUOTEDVALUE
            c++;
            state = STATE_QUOTEDVALUE;
            break;

        case STATE_LF_DONE:
            // Consume a LF then advance to STATE_DONE
            if (*c != CHAR_LF) {
                state = STATE_ERROR;
                break;
            }
            c++;
            state = STATE_DONE;
            break;

        default:
            // STATE_DONE, STATE_ERROR
            return c;
        }
    }

    return c;
}

//-----------------------------------------------------------------------------
// HttpParser::parseString
//-----------------------------------------------------------------------------

inline unsigned char* HttpParser::parseString(unsigned char *c)
{
    while (*c && state != STATE_ERROR && state != STATE_DONE) {
        if (state == STATE_STARTLINE)
            c = parseStartline(c);
        else
            c = parseHeaderFields(c);
    }
    return c;
}

//-----------------------------------------------------------------------------
// HttpParser::parse
//-----------------------------------------------------------------------------

int HttpParser::parse(netbuf *buf)
{
    int rv = PROTOCOL_BAD_REQUEST;

    // Make sure there's no unused space at the front of the buffer
    if (buf->pos > 0) {
        int size = buf->cursize - buf->pos;
        if (size > 0) {
            memmove(buf->inbuf, &buf->inbuf[buf->pos], size);
            buf->cursize = size;
        } else {
            buf->cursize = 0;
        }
        buf->pos = 0;
    }

    // Save room for a trailing nul
    buf->maxsize--;
    int oldpos = 0;
    char oldchar;
    if (buf->cursize > buf->maxsize) {
        buf->cursize--;
        oldpos = buf->cursize;
        oldchar = buf->inbuf[oldpos];
    }
    buf->inbuf[buf->cursize] = CHAR_NUL;

    // c is the current pointer into buf
    unsigned char *c = buf->inbuf;

    // Initialize the parser
    reset();
    PR_ASSERT(state == STATE_STARTLINE);

    // While there's something left to do...
    while (state != STATE_ERROR && state != STATE_DONE) {
        // Make sure there is data to process
        if (!*c) {
            int available = &buf->inbuf[buf->cursize] - c;
            int room = &buf->inbuf[buf->maxsize] - c;

            if (available) {
                // Embedded nul
                rv = PROTOCOL_BAD_REQUEST;
                state = STATE_ERROR;                
            } else if (room < 1) {
                // Buffer full
                rv = PROTOCOL_ENTITY_TOO_LARGE;
                state = STATE_ERROR;
            } else {
                // Read into buffer
                int count = net_read(buf->sd, (char*)c, room, buf->rdtimeout);
                if (count > 0) {
                    // Read okay
                    buf->cursize += count;
                    buf->inbuf[buf->cursize] = CHAR_NUL;
                } else {
                    // Timeout or EOF
                    rv = PROTOCOL_REQUEST_TIMEOUT;
                    state = STATE_ERROR;
                }
            }
        }

        // Process the data
        c = parseString(c);
    }

    // Record how much data we consumed from buf
    buf->pos = c - buf->inbuf;

    // Restore trailing character
    buf->maxsize++;
    if (oldpos) {
        buf->inbuf[oldpos] = oldchar;
        PR_ASSERT(oldpos == buf->cursize);
        buf->cursize++;
    }

    // Did everything parse okay?
    if (state != STATE_DONE) {
        if (countHeaderFieldNodes >= maxHeaderFields)
            return PROTOCOL_ENTITY_TOO_LARGE;
        return rv;
    }

    // Build the header list
    doneHeaderFields();

    // Success
    return 0;
}

int HttpParser::parse(char *string)
{
    // Initialize the parser
    reset();
    PR_ASSERT(state == STATE_STARTLINE);

    // Parse string
    parseString((unsigned char*)string);

    // Did everything parse okay?
    if (state != STATE_DONE) {
        if (countHeaderFieldNodes >= maxHeaderFields)
            return PROTOCOL_ENTITY_TOO_LARGE;
        return PROTOCOL_BAD_REQUEST;
    }

    // Build the header list
    doneHeaderFields();

    // Success
    return 0;
}

//-----------------------------------------------------------------------------
// HttpParser::doneHeaderFields
//-----------------------------------------------------------------------------

inline void HttpParser::doneHeaderFields()
{
    if (countHeaderFieldNodes && !countHeaderFields) {
        for (int i = 0; i < countHeaderFieldNodes; i++) {
            HttpHeaderFieldNode *node = &headerFieldNodes[i];

            // Have we already seen this header name?
            HttpHeaderFieldNode *old = node->prev;
            while (old && old->name != node->name)
                old = old->prev;
            node->value.next = NULL;
            if (old) {
                // New value for existing header
                PR_ASSERT(countHeaderFields);
                old->value.next = &node->value;
            } else {
                // New header
                headerFields[countHeaderFields] = node;

                // Check to see if there's a key registered for this header
                HttpHeaderFieldKeyNode *keynode = keyNodeHash[node->hash];
                while (keynode && keynode->name != node->name)
                    keynode = keynode->next;
                if (keynode) {
                    // Found a key for this header
                    keys[countHeaderFields] = keynode->key;

                    // Remember a shortcut to this header
                    *keynode->shortcut = node;
                } else {
                    // No key for this header
                    keys[countHeaderFields] = NULL;
                }

                countHeaderFields++;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// HttpRequestParser::getVersion
//-----------------------------------------------------------------------------

int HttpRequestParser::getVersion()
{
    // HTTP/0.9 doesn't explicity specify the protocol
    const int http09 = 9;
    return request.protocol.len ? request.major * 100 + request.minor : http09;
}

//-----------------------------------------------------------------------------
// HttpRequestParser::getProtocol
//-----------------------------------------------------------------------------

HttpString HttpRequestParser::getProtocol()
{
    // HTTP/0.9 doesn't explicitly specify the protocol
    const HttpString http09((char*)"HTTP/0.9");
    return request.protocol.len ? request.protocol : http09;
}

//-----------------------------------------------------------------------------
// HttpParser::isProtocol
//-----------------------------------------------------------------------------

PRBool HttpParser::isProtocol(const char *string)
{
    HttpString protocol = getProtocol();
    if (protocol.len) {
        int len = strlen(string);
        if (len == protocol.len || (protocol.len > len && protocol.p[len] == '/')) {
            return !memcmp(string, protocol.p, len);
        }
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// HttpParser::getHeaderField
//-----------------------------------------------------------------------------

HttpHeaderField* HttpParser::getHeaderField(HttpString name)
{
    HttpHeaderField *header = NULL;

    // Hash the header name to a list of HttpHeaderFieldNodes
    HttpHeaderFieldNode *node = headerFieldNodeHash[hashheader(name) & maskHeaderFieldHash];
    while (node) {
        // We want the last HttpHeaderFieldNode in the list (i.e. the first in the
        // message) that matches name
        if (node->name == name)
            header = node;
        node = node->prev;
    }

    return header;
}

//-----------------------------------------------------------------------------
// HttpParser::dupHeaderField
//-----------------------------------------------------------------------------

char* HttpParser::dupHeaderField(HttpHeaderField header)
{
    const HttpStringNode *node;

    // Find length of header name and value(s)
    int len = header.name.len;
    node = &header.value;
    while (node) {
        len += node->len + 1;
        node = node->next;
    }

    char *buffer = (char*)MALLOC(len + 2);
    if (buffer) {
        int pos = 0;

        // Get header name
        memcpy(buffer, header.name.p, header.name.len);
        buffer[0] = toupper(buffer[0]);
        pos += header.name.len;
        buffer[pos++] = ':';
        buffer[pos++] = ' ';

        // Get header value(s)
        node = &header.value;
        while (node) {
            memcpy(&buffer[pos], node->p, node->len);
            pos += node->len;
            buffer[pos++] = ','; // RFC 2616 4.2
            node = node->next;
        }

        // End of string (overwrites last space or comma)
        buffer[--pos] = '\0';
    }

    return buffer;
}

//-----------------------------------------------------------------------------
// HttpParser::dupHeaderFieldValue
//-----------------------------------------------------------------------------

char* HttpParser::dupHeaderFieldValue(HttpStringNode value)
{
    const HttpStringNode *node;

    // Find length of string(s)
    int len = 0;
    node = &value;
    while (node) {
        len += node->len + 1;
        node = node->next;
    }

    char *buffer = (char*)MALLOC(len + 1);
    if (buffer) {
        int pos = 0;

        if (len) {
            // Get string(s)
            node = &value;
            while (node) {
                memcpy(&buffer[pos], node->p, node->len);
                pos += node->len;
                buffer[pos++] = ','; // RFC 2616 4.2
                node = node->next;
            }
            --pos;
        }

        // End of string (overwrites last comma)
        buffer[pos] = '\0';
    }

    return buffer;
}

//-----------------------------------------------------------------------------
// HttpParser::dupString
//-----------------------------------------------------------------------------

char* HttpParser::dupString(HttpString string)
{
    char *buffer = (char*)MALLOC(string.len + 1);
    memcpy(buffer, string.p, string.len);
    buffer[string.len] = '\0';
    return buffer;
}

//-----------------------------------------------------------------------------
// HttpParser::freeString
//-----------------------------------------------------------------------------

void HttpParser::freeString(char *string)
{
    FREE(string);
}

//-----------------------------------------------------------------------------
// HttpTokenizer::HttpTokenizer
//-----------------------------------------------------------------------------

HttpTokenizer::HttpTokenizer(HttpString string)
{
    tokens.p = string.p;
    tokens.len = string.len;
    tokens.next = NULL;
    getFirstToken();
}

HttpTokenizer::HttpTokenizer(HttpStringNode string)
{
    tokens = string;
    getFirstToken();
}

//-----------------------------------------------------------------------------
// HttpTokenizer::operator++
//-----------------------------------------------------------------------------

HttpString* HttpTokenizer::operator++()
{
    // Prefix increment
    getNextToken();
    return current.len > 0 ? &current : NULL;
}

HttpString* HttpTokenizer::operator++(int)
{
    // Postfix increment
    previous = current;
    getNextToken();
    return previous.len > 0 ? &previous : NULL;
}

//-----------------------------------------------------------------------------
// HttpTokenizer::matches
//-----------------------------------------------------------------------------

PRBool HttpTokenizer::matches(HttpString token)
{
    if (current.len != token.len)
        return PR_FALSE;

    int count = current.len;
    while (count-- > 0) {
        if (HttpParser::toLowerExceptCrLfColon[current.p[count]] != token.p[count])
            return PR_FALSE;
    }

    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// HttpTokenizer::getFirstToken
//-----------------------------------------------------------------------------

inline void HttpTokenizer::getFirstToken()
{
    current.p = tokens.p;
    current.len = -1;
    getNextToken();
}

//-----------------------------------------------------------------------------
// HttpTokenizer::getNextToken
//-----------------------------------------------------------------------------

inline void HttpTokenizer::getNextToken()
{
    do {
        if (current.p + current.len + 1 >= tokens.p + tokens.len) {
            if (!tokens.next) {
                // End of the line
                current.len = -1;
                break;
            }
                
            // Advance to the next HttpStringNode
            tokens = *tokens.next;
            current.p = tokens.p;
            current.len = -1;
        }

        // Advance to the next token within this HttpStringNode
        current.p += current.len + 1;
        current.len = tokens.p + tokens.len - current.p;
        HttpParser::getToken(current);
    } while (!current.len);
}

//-----------------------------------------------------------------------------
// HttpParser::getToken
//-----------------------------------------------------------------------------

inline void HttpParser::getToken(HttpString &string)
{
    char *p = string.p;
    int size = string.len;

    // Skip past leading separators
    const char *end = p + size;
    while (p < end && isSeparator[*p] && *p != CHAR_QUOTE)
        p++;
    size = end - p;

    // Find end of token
    int len;
    if (*p == CHAR_QUOTE) {
        // Handle quoted string
        for (len = 1; len < size && p[len] != CHAR_QUOTE; len++) {
            // Skip over escaped characters
            if (p[len] == CHAR_BACKSLASH)
                len++;
        }
    } else {
        // Handle unquoted string
        for (len = 0; len < size && !isSeparator[p[len]]; len++);
    }

    // Certain bogus token values could take us past the end
    if (len > size)
        len = size;

    // Don't include opening quote in the token
    if (len && *p == CHAR_QUOTE) {
        p++;
        len--;
    }

    // Tell caller about the token
    string.p = p;
    string.len = len;
}
