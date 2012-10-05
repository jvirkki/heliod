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

#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include <string.h>

#include "nspr.h"
#include "base/buffer.h"

//-----------------------------------------------------------------------------
// HttpString
//-----------------------------------------------------------------------------

struct HttpString {
    HttpString() { }
    HttpString(char *p) : p(p), len(strlen(p)) { } 
    HttpString(char *p, int len) : p(p), len(len) { } 
    char *p;
    int len;
};

inline int operator==(const HttpString &l, const HttpString &r)
{
    return (l.len == r.len) && (!l.len || !memcmp(l.p, r.p, l.len));
}

inline int operator!=(const HttpString &l, const HttpString &r)
{
    return (l.len != r.len) || (l.len && memcmp(l.p, r.p, l.len));
}

inline int atoi(const HttpString &string)
{
    // This assumes ASCII, but that's fine according to RFC 2616
    int i = 0;
    while (i < string.len && (string.p[i] == ' ' || string.p[i] == '\t'))
        i++;
    int value = 0;
    while (i < string.len && string.p[i] >= '0' && string.p[i] <= '9')
        value = value * 10 + (string.p[i++] - '0');
    return value;
}

//-----------------------------------------------------------------------------
// HttpStringNode
//-----------------------------------------------------------------------------

struct HttpStringNode : public HttpString {
    HttpStringNode *next;
};

//-----------------------------------------------------------------------------
// HttpHeaderField
//-----------------------------------------------------------------------------

struct HttpHeaderField {
    HttpString name;
    HttpStringNode value;
};

//-----------------------------------------------------------------------------
// HttpHeaderFieldNode
//-----------------------------------------------------------------------------

struct HttpHeaderFieldNode;

//-----------------------------------------------------------------------------
// HttpHeaderFieldKey
//-----------------------------------------------------------------------------

struct HttpHeaderFieldKey;

//-----------------------------------------------------------------------------
// HttpHeaderFieldKeyNode
//-----------------------------------------------------------------------------

struct HttpHeaderFieldKeyNode;

//-----------------------------------------------------------------------------
// HttpTokenizer
//-----------------------------------------------------------------------------

class HttpTokenizer {
public:
    HttpTokenizer(HttpString string);
    HttpTokenizer(HttpStringNode string);
    HttpString* operator*() { return current.len > 0 ? &current : NULL; }
    HttpString* operator++();
    HttpString* operator++(int);
    PRBool matches(HttpString token);

private:
    inline void getFirstToken();
    inline void getNextToken();

    HttpStringNode tokens;
    HttpString current;
    HttpString previous;
};

//-----------------------------------------------------------------------------
// HttpParser
//-----------------------------------------------------------------------------

class HttpParser {
public:
    HttpParser();
    ~HttpParser();

    void setHeaderFieldKey(HttpString name, HttpHeaderFieldKey *key);

    int parse(char *string);
    int parse(netbuf *buf);

    PRBool isProtocol(const char *string);
    virtual HttpString getProtocol() = 0;
    HttpString getStartline() { return startline; }
    int getHeaderFieldCount() { return countHeaderFields; }
    HttpHeaderField* getHeaderField(int i) { return headerFields[i]; }
    HttpHeaderField* getHeaderField(HttpString name);
    HttpHeaderFieldKey* getHeaderFieldKey(int i) { return keys[i]; }

    static void getToken(HttpString &string);
    static char* dupHeaderField(HttpHeaderField header);
    static char* dupHeaderFieldValue(HttpStringNode value);
    static char* dupString(HttpString string);
    static void freeString(char *string);

    // Character constants
    static const unsigned char CHAR_CR;
    static const unsigned char CHAR_LF;
    static const unsigned char CHAR_SP;
    static const unsigned char CHAR_HT;
    static const unsigned char CHAR_COLON;
    static const unsigned char CHAR_QUOTE;
    static const unsigned char CHAR_SLASH;
    static const unsigned char CHAR_BACKSLASH;
    static const unsigned char CHAR_QUESTION;
    static const unsigned char CHAR_DOT;
    static const unsigned char CHAR_NUL;

protected:
    virtual void reset();
    virtual unsigned char* parseStartline(unsigned char *c) = 0;

    void setHeaderFieldShortcut(HttpString name, HttpHeaderField **shortcut);

    // Request/response state
    enum {
        STATE_STARTLINE = 0,
        STATE_LF_HEADER,
        STATE_HEADER,
        STATE_NAME,
        STATE_LWS_VALUE,
        STATE_VALUE,
        STATE_QUOTEDVALUE,
        STATE_ESCAPEDQUOTEDVALUE,
        STATE_LF_DONE,
        STATE_DONE,
        STATE_ERROR = -1
    } state;

    // Generic request/response startline
    HttpString startline;

    // Character lookup tables
    static PRBool flagLutsInitialized;
    static PRPackedBool isSeparator[256];
    static PRPackedBool isCrLfLwsNul[256];
    static PRPackedBool isCrLfLwsSlashNul[256];
    static PRPackedBool isCrLfLwsQuestionNul[256];
    static PRPackedBool isCrLfQuoteNul[256];
    static PRPackedBool isCrLfQuoteBackslashNul[256];
    static PRPackedBool isDigit[256];
    static unsigned char toLowerExceptCrLfColon[256];

private:
    static void initLuts();
    HttpHeaderFieldKeyNode* getHeaderFieldKeyNode(HttpString name);
    inline unsigned char* parseString(unsigned char *c);
    inline unsigned char* parseHeaderFields(unsigned char *c);
    inline void doneHeaderFields();

    // Header field information
    int maxHeaderFields;
    int maskHeaderFieldHash;
    int sizeHeaderFieldHash;
    int countHeaderFieldNodes;
    HttpHeaderFieldNode *headerFieldNodes;
    HttpHeaderFieldNode **headerFieldNodeHash;
    HttpHeaderFieldKeyNode **keyNodeHash;
    int countHeaderFields;
    HttpHeaderField **headerFields;
    HttpHeaderFieldKey **keys;

friend class HttpTokenizer;
};

//-----------------------------------------------------------------------------
// HttpRequestParser
//-----------------------------------------------------------------------------

class HttpRequestParser : public HttpParser {
public:
    HttpRequestParser();
    int getVersion();
    HttpString getProtocol();
    HttpString getMethod() { return request.method; }
    HttpString getUrl() { return request.url; }
    HttpString getScheme() { return request.scheme; }
    HttpString getHost() { return request.host; }
    HttpString getPath() { return request.path; }
    HttpString getQuery() { return request.query; }
    HttpHeaderField* getHostHeaderField() { return host; }
    HttpHeaderField* getConnectionHeaderField() { return connection; }

protected:
    void reset();
    unsigned char* parseStartline(unsigned char *c);
    inline void doneStartline(unsigned char *c);

    // Request startline state
    enum {
        STATE_REQUEST_BEGIN = 0,
        STATE_REQUEST_METHOD,
        STATE_REQUEST_LWS_URL,
        STATE_REQUEST_URL,
        STATE_REQUEST_SCHEME,
        STATE_REQUEST_SLASH_HOSTPATH,
        STATE_REQUEST_HOSTPATH,
        STATE_REQUEST_HOST,
        STATE_REQUEST_PATH,
        STATE_REQUEST_QUERY,
        STATE_REQUEST_LWS_PROTOCOL,
        STATE_REQUEST_PROTOCOL,
        STATE_REQUEST_PROTOCOLMAJOR,
        STATE_REQUEST_PROTOCOLMINOR,
        STATE_REQUEST_LF_DONE,
        STATE_REQUEST_DONE,
        STATE_REQUEST_ERROR = -1
    } state;

    // Request startline
    struct _HttpRequestStartLine {
        HttpString method;
        HttpString url;
        HttpString scheme;
        HttpString host;
        HttpString path;
        HttpString query;
        HttpString protocol;
        int major;
        int minor;
    } request;

    // Shortcuts to common headers
    HttpHeaderField *host;
    HttpHeaderField *connection;
};

//-----------------------------------------------------------------------------
// HttpResponseParser
//-----------------------------------------------------------------------------

class HttpResponseParser : public HttpParser {
public:
    int getVersion() { return response.major * 100 + response.minor; }
    HttpString getProtocol() { return response.protocol; }
    HttpString getCode() { return response.code; }
    HttpString getReason() { return response.reason; }

protected:
    void reset();
    unsigned char* parseStartline(unsigned char *c);
    inline void doneStartline(unsigned char *c);

    // Response startline state
    enum {
        STATE_RESPONSE_BEGIN = 0,
        STATE_RESPONSE_PROTOCOL,
        STATE_RESPONSE_PROTOCOLMAJOR,
        STATE_RESPONSE_PROTOCOLMINOR,
        STATE_RESPONSE_LWS_CODE,
        STATE_RESPONSE_CODE,
        STATE_RESPONSE_LWS_REASON,
        STATE_RESPONSE_REASON,
        STATE_RESPONSE_LF_DONE,
        STATE_RESPONSE_DONE,
        STATE_RESPONSE_ERROR = -1
    } state;

    // Response startline
    struct _HttpResponseStartLine {
        // Parsed values
        HttpString protocol;
        HttpString code;
        HttpString reason;
        int major;
        int minor;
    } response;
};

#endif // HTTPPARSER_H
