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
** HttpHeader.cpp
*/

#include "httpdaemon/libdaemon.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/HttpMethodRegistry.h"
#include "base/nsassert.h"
#include "base/systems.h"
#include "base/util.h"
#include "frame/log.h"
#include "frame/conf.h"
#include "prerror.h"

/* The maximum HTTP version number supported */
#define HTTP_VERSION_MAX 199

/* Space characters are SP and HT */
#define HHCHSPACE(c) (httpCharMask[c] & HHCM_SPACE)

/* Separators are ( ) < > @ , ; : \ " / [ ] ? = { }  SP and HT */
#define HHCHSEP(c) (httpCharMask[c] & HHCM_SEP)

/* Macro to look for CR or LF */
#define HHCHEOL(c) (httpCharMask[c] & HHCM_EOL)

/* Macro to look for digits 0-9 */
#define HHCHDIGIT(c) (httpCharMask[c] & HHCM_DIGIT)

/* Macro to look for letters A-Z, a-z */
#define HHCHALPHA(c) (httpCharMask[c] & HHCM_ALPHA)

/* Macro to look for letters A-Z, a-z, or digits 0-9 */
#define HHCHALPHANUM(c) (httpCharMask[c] & (HHCM_ALPHA|HHCM_DIGIT))

/* Macro to look for CTL, space, or eol */
#define HHCHSTOP(c) (httpCharMask[c] & (HHCM_CTL|HHCM_SPACE|HHCM_EOL))

/* token is any CHAR except CTL or <separators> */
#define HHCHTOKEN(c) \
    ((httpCharMask[c] & (HHCM_CHAR|HHCM_CTL|HHCM_SEP)) == HHCM_CHAR)


typedef struct header_entry header_entry;
struct header_entry {
    const char * header;
    NSHttpHeader index;
};


static const header_entry http_headers[] = { 
    { "ACCEPT", NSHttpHeader_Accept },
    { "ACCEPT-CHARSET", NSHttpHeader_Accept_Charset },
    { "ACCEPT-ENCODING", NSHttpHeader_Accept_Encoding },
    { "ACCEPT-LANGUAGE", NSHttpHeader_Accept_Language },
    { "ACCEPT-RANGES", NSHttpHeader_Accept_Ranges },
    { "AGE", NSHttpHeader_Age },
    { "ALLOW", NSHttpHeader_Allow },
    { "AUTHORIZATION", NSHttpHeader_Authorization },
    { "CACHE-CONTROL", NSHttpHeader_Cache_Control },
    { "CONNECTION", NSHttpHeader_Connection },
    { "CONTENT-ENCODING", NSHttpHeader_Content_Encoding },
    { "CONTENT-LANGUAGE", NSHttpHeader_Content_Language },
    { "CONTENT-LENGTH", NSHttpHeader_Content_Length },
    { "CONTENT-LOCATION", NSHttpHeader_Content_Location },
    { "CONTENT-MD5", NSHttpHeader_Content_MD5 },
    { "CONTENT-RANGE", NSHttpHeader_Content_Range },
    { "CONTENT-TYPE", NSHttpHeader_Content_Type },
    { "COOKIE", NSHttpHeader_Cookie },
    { "DATE", NSHttpHeader_Date },
    { "ETAG", NSHttpHeader_ETag },
    { "EXPECT", NSHttpHeader_Expect },
    { "EXPIRES", NSHttpHeader_Expires },
    { "FROM", NSHttpHeader_From },
    { "HOST", NSHttpHeader_Host },
    { "IF-MATCH", NSHttpHeader_If_Match },
    { "IF-MODIFIED-SINCE", NSHttpHeader_If_Modified_Since },
    { "IF-NONE-MATCH", NSHttpHeader_If_None_Match },
    { "IF-RANGE", NSHttpHeader_If_Range },
    { "IF-UNMODIFIED-SINCE", NSHttpHeader_If_Unmodified_Since },
    { "LAST-MODIFIED", NSHttpHeader_Last_Modified },
    { "LOCATION", NSHttpHeader_Location },
    { "MAX-FORWARDS", NSHttpHeader_Max_Forwards },
    { "PRAGMA", NSHttpHeader_Pragma },
    { "PROXY-AUTHENTICATE", NSHttpHeader_Proxy_Authenticate },
    { "PROXY-AUTHORIZATION", NSHttpHeader_Proxy_Authorization },
    { "RANGE", NSHttpHeader_Range },
    { "REFERER", NSHttpHeader_Referer },
    { "RETRY-AFTER", NSHttpHeader_Retry_After },
    { "SERVER", NSHttpHeader_Server },
    { "TE", NSHttpHeader_TE },
    { "TRAILER", NSHttpHeader_Trailer },
    { "TRANSFER-ENCODING", NSHttpHeader_Transfer_Encoding },
    { "UPGRADE", NSHttpHeader_Upgrade },
    { "USER-AGENT", NSHttpHeader_User_Agent },
    { "VARY", NSHttpHeader_Vary },
    { "VIA", NSHttpHeader_Via },
    { "WARNING", NSHttpHeader_Warning },
    { "WWW-AUTHENTICATE", NSHttpHeader_WWW_Authenticate },
    { NULL, NSHttpHeaderMax }
};

// default maximum number of headers value
int HttpHeader::maxRqHeaders = 64;

/* Table of character class masks */
char HttpHeader::httpCharMask[256];

/* Method and header keyword lookup tables */
NSKWSpace *HttpHeader::httpMethods = NULL;
NSKWSpace *HttpHeader::httpHeaders = NULL;

HttpHeader::HttpHeader()
{
    rqHeaders = new HHHeader [maxRqHeaders];
}

HttpHeader::~HttpHeader()
{
    delete [] rqHeaders;
}

void HttpHeader::SetMaxRqHeaders(int n)
{
    maxRqHeaders = n;
}

PRBool HttpHeader::Initialize()
{
    const struct header_entry *he;
    char *cp;
    int rv;
    int cc;

    if (!httpMethods) {
        httpMethods = (NSKWSpace *)HttpMethodRegistry::GetRegistry().GetMethodsTable();
    }

    if (!httpHeaders) {
        httpHeaders = NSKW_CreateNamespace(NSHttpHeaderMax);
        for (he = http_headers; he->header != NULL; ++he) {
            rv = NSKW_DefineKeyword(he->header, he->index, httpHeaders);
            PR_ASSERT(rv == he->index);
            if (rv != he->index) {
                return PR_FALSE;
            }
        }
        NSKW_Optimize(httpHeaders);
    }

    /* Initialize character class mask table */
    /* XXX hep - could be done at compile time */
    memset(httpCharMask, 0, 256);
    for (cc = 0; cc <= 255; ++cc) {
        httpCharMask[cc] = HHCM_CHAR;
    }
    for (cc = 0; cc <= 31; ++cc) {
        httpCharMask[cc] |= HHCM_CTL;
    }
    httpCharMask[127] |= HHCM_CTL;
    for (cc = '0'; cc <= '9'; ++cc) {
        httpCharMask[cc] |= HHCM_DIGIT;
    }
    for (cc = 'A'; cc <= 'Z'; ++cc) {
        httpCharMask[cc] |= HHCM_ALPHA;
        httpCharMask[cc + ('a' - 'A')] |= HHCM_ALPHA;
    }
    for (cp = "()<>@,;:\\\"/[]?={} \t"; *cp; ++cp) {
        httpCharMask[*cp] |= HHCM_SEP;
    }
    httpCharMask[' '] |= HHCM_SPACE;
    httpCharMask['\t'] |= HHCM_SPACE;
    httpCharMask['\r'] |= HHCM_EOL;
    httpCharMask['\n'] |= HHCM_EOL;

    return PR_TRUE;
}

/*
 * ParseCSList - parse a comma-separated list of items
 *
 * This function parses a comma-separated list of values contained
 * in a string described by "instr".  It starts at the position in
 * "instr" given by "startpos", skips any leading spaces, marks the
 * start of a token, scans to the end of token, marks the length,
 * and skips any spaces up a comma or EOL.  If the token is quoted,
 * the quotes are not included in the result string, and "wasQuoted"
 * is returned as PR_TRUE.  The "outstr" descriptor is modified to
 * describe the result string, and the position from which to start
 * parsing the next token is returned as the function value.  If
 * no token is found, the function return value is -1.
 */
int HttpHeader::ParseCSList(HHString *outstr, const HHString *instr,
                            int startpos, PRBool &wasQuoted)
{
    char *cp = instr->ptr;
    int i;
    int vstart = -1;
    int len = -1;
    int inqs = 0;
    int nextpos = -1;

    outstr->ptr = cp;
    outstr->len = 0;
    wasQuoted = PR_FALSE;

    i = startpos;
    while (i < instr->len) {

        /* Space? */
        if (HHCHSPACE(cp[i])) {

            /*
             * If not in quoted string and value has been started,
             * this terminates the value.  But we still need to
             * advance to the next comma or end of string.
             */
            if (!inqs && (vstart >= 0)) {
                len = i - vstart;
            }

            /* Skip spaces */
            ++i;
            continue;
        }

        /* Find some kind of separator? */
        if (HHCHSEP(cp[i])) {

            /* Check for comma */
            if (!inqs && (cp[i] == ',')) {

                ++i;

                /* Got anything yet? */
                if (vstart >= 0) {

                    /* Yes, done */
                    if (len < 0) {
                        len = (i-1) - vstart;
                    }
                    break;
                }

                /* Ignore empty field in list */
                continue;
            }

            /* Check for beginning or end of quoted string */
            if (cp[i] == '"') {
                if (inqs) {
                    inqs = 0;
                    len = (i++) - vstart;
                    continue;
                }
                if (vstart < 0) {
                    vstart = ++i;
                    wasQuoted = PR_TRUE;
                }
                inqs = 1;
                continue;
            }

            /* Check for quoted pair in quoted string */
            if (inqs && (cp[i] == '\\')) {
                ++i;
                if (i < instr->len) {
                    ++i;
                }
                continue;
            }
        }

        /* Stop at EOL */
        if (HHCHEOL(cp[i])) {
            break;
        }

        /* Begin value if we haven't yet */
        if (vstart < 0) {
            vstart = i;
        }

        ++i;
    }

    if (vstart >= 0) {
        if (len < 0) {
            len = i - vstart;
        }

        outstr->ptr = &cp[vstart];
        outstr->len = len;
        nextpos = i;
    }

    return nextpos;
}

/*
 * ParseRequest - parse an HTTP request from a netbuf
 *
 * This function parses an HTTP request, given a netbuf for the source
 * of the request.  It requires that the request line and all of the
 * associated HTTP headers will fit entirely within the inbuf buffer
 * of the netbuf.  If the remaining data in inbuf is not at the beginning
 * of the buffer, it is copied there before parsing begins.  If the
 * data in inbuf is exhausted before the parse is complete, the socket
 * associated with the netbuf will be read, repeatedly if necessary,
 * until inbuf is full or the parse is complete.
 *
 * On return, the class instance contains descriptors for components
 * of the request that were found by the parse.  These descriptors
 * refer to strings in the netbuf inbuf, and the netbuf pos is updated
 * to the index of the first character beyond the parsed text.  If
 * additional reads were necessary, the netbuf cursize may have also
 * increased to reflect the additional data.
 *
 * The implementation has a limit on the total size of the request
 * line and headers, which is the netbuf maxsize value.  It also has
 * an internal limit on the number of HTTP header lines it can
 * handle in a request.  This limit is set by the constant, maxRqHeaders.
 */
HHStatus HttpHeader::ParseRequest(netbuf *buf, PRIntervalTime timeout)
{
    int csize = buf->cursize;
    int i = buf->pos;
    char *cp = (char *)(buf->inbuf);

    sRequest.ptr = NULL;
    sRequest.len = 0;
    sMethod.ptr = NULL;
    sMethod.len = 0;
    sReqURI.ptr = NULL;
    sReqURI.len = 0;
    sProtocol.ptr = NULL;
    sProtocol.len = 0;
    sHost.ptr = NULL;
    sHost.len = 0;
    sAbsPath.ptr = NULL;
    sAbsPath.len = 0;
    sQuery.ptr = NULL;
    sQuery.len = 0;
    iMethod = -1;
    nClientProtocolVersion = HTTPprotv_num;

    nRqHeaders = 0;

    hCacheControl = NULL;
    hConnection = NULL;
    hContentLength = NULL;
    hHost = NULL;
    hIfMatch = NULL;
    hIfModifiedSince = NULL;
    hIfNoneMatch = NULL;
    hIfRange = NULL;
    hIfUnmodifiedSince = NULL;
    hPragma = NULL;
    hRange = NULL;
    hReferer = NULL;
    hTransferEncoding = NULL;
    hUserAgent = NULL;

    fAbsoluteURI = PR_FALSE;
    fKeepAliveRequested = PR_FALSE;

    timeRemaining = timeout;

    /* If the data is not at the beginning of the buffer, copy it */
    if (i > 0) {
        int rlen = csize - i;
#ifdef NO_MEMMOVE
        char *dp = cp;
        char *sp = cp + i;

        /* Can't trust this to memcpy() */
        for ( ; rlen > 0; --rlen) {
            *dp++ = *sp++;
        }
#else
        if (rlen > 0) {
            memmove(cp, cp + i, rlen);
        }
#endif /* NO_MEMMOVE */
        csize -= i;
        i = 0;
        buf->cursize = csize;
        buf->pos = 0;
    }

    /* Make sure we can store a terminating byte */
    PR_ASSERT(buf->maxsize >= csize);
    if (csize >= buf->maxsize) {
        return HHSTATUS_TOO_LARGE;
    }
 
    /* Store terminating byte */
    cp[csize] = '\0';

    HHStatus rv = ScanRequestLine(buf);
    if (rv == HHSTATUS_SUCCESS) {
        /*
         * The next major version of the HTTP protocol may or may not have
         * headers in the sense we understand them
         */
        if (nClientProtocolVersion > HTTP_VERSION_MAX)
            return HHSTATUS_VERSION_NOT_SUPPORTED;

        /* HTTP/1.x requests have headers */
        if (nClientProtocolVersion >= PROTOCOL_VERSION_HTTP10) {
            rv = ScanHeaders(buf);
            if (rv == HHSTATUS_SUCCESS)
                ParseConnectionHeader();
        }
    }

    return rv;
}

int HttpHeader::NetbufAppend(netbuf *buf)
{
    int rlen = buf->maxsize - buf->cursize - 1;

    /* If there's space in the buffer, read more data */
    if (rlen > 0) {
        PRIntervalTime timeout;
        PRInt32 rv;

        /*
         * Wait no longer than the lesser of the netbuf's IO timeout or the
         * HttpHeader::ParseRequest() request header timeout
         */
        timeout = net_nsapi_timeout_to_nspr_interval(buf->rdtimeout);
        if (timeout > timeRemaining)
            timeout = timeRemaining;

        PRIntervalTime epoch;
        if (timeRemaining != PR_INTERVAL_NO_TIMEOUT)
            epoch = PR_IntervalNow();

        rv = PR_Recv(buf->sd, (void *)(buf->inbuf + buf->cursize), rlen,
                     0, timeout);
        if (rv > 0) {

            /* Update the total size of data in the buffer */
            rv += buf->cursize;
            buf->inbuf[rv] = '\0';
            buf->cursize = rv;

            /* Subtract the time we spent in IO from the time remaining */
            if (timeRemaining != PR_INTERVAL_NO_TIMEOUT) {
                PRIntervalTime elapsed = PR_IntervalNow() - epoch;
                if (timeRemaining > elapsed) {
                    timeRemaining -= elapsed;
                }
                else {
                    timeRemaining = 0;
                }
            }

            /* Successful completion */
            return rv;
        }
        else if (rv < 0) {
            // Cancel io only on error
            HHStatus status;
            if (PR_GetError() == PR_IO_TIMEOUT_ERROR) {
                status = HHSTATUS_IO_TIMEOUT;
            } else {
                status = HHSTATUS_IO_ERROR;
            }
            INTnet_cancelIO(buf->sd);
            return status;
        }
        else {
            return HHSTATUS_IO_EOF;
        }
    }

    return HHSTATUS_TOO_LARGE;
}

PRBool HttpHeader :: ScanVersion(int& i, unsigned char*& cp, char*& str, netbuf*& buf)
{
    PRBool res = PR_FALSE;

    i += 5;

    if (HHCHDIGIT(cp[i]))
    {
        int nMajor = (cp[i] - '0');
        int nMinor = 0;

        PRInt32 majorlen = 1;
        while (majorlen < 9)
        {
            i++;

            if (HHCHDIGIT(cp[i]))
            {
                nMajor = nMajor * 10 + (cp[i] - '0');
                majorlen++;
                continue;
            };

            break; // not a digit
        }

        if ('.' == cp[i])
        {
            i++;

            if (HHCHDIGIT(cp[i]))
            {
                nMinor = (cp[i] - '0');

                PRInt32 minorlen = 1;
                while (minorlen < 9)
                {
                    i++;

                    if (HHCHDIGIT(cp[i]))
                    {
                        nMinor = nMinor * 10 + (cp[i] - '0');
                        minorlen ++;
                        continue;
                    };

                    break; // not a digit
                }

                if (minorlen != 9)
                {
                    sProtocol.len = (char *)&cp[i] - str;
                    sRequest.len = (char *)&cp[i] - sRequest.ptr;

                    res = PR_TRUE;
                }
            }
        }

        if (nMinor > 99)
            nMinor = 99;

        nClientProtocolVersion = nMajor * 100 + nMinor;
    }
    return res;
}

HHStatus HttpHeader::ScanRequestLine(netbuf *buf)
{
    int i = 0;                       /* current position in buffer */
    unsigned char *cp = buf->inbuf;  /* pointer to start of buffer */
    int csize = buf->cursize;
    char *str;
    int len;
    unsigned long hash;

    sRequest.ptr = (char *)cp;
    sRequest.len = 0;

    /*
     * Special case "GET" method
     *
     * If the method can be readily identified as a GET, we attempt
     * a fairly aggressive parse of the request line.  If it doesn't
     * work out, we reset the position to just after "GET " and try
     * a more conservative approach.
     */
    if (cp[0] == 'G' && cp[1] == 'E' && cp[2] == 'T' && cp[3] == ' ') {

        sMethod.ptr = (char *)cp;
        sMethod.len = 3;
        iMethod = METHOD_GET;
        i += 4;

        /* Skip additional spaces after method */
        /* NB: Strictly speaking there shouldn't be more than the one */
        while (1) {

            /* Need more data? */
            if (!cp[i]) {
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    sRequest.len = (char *)&cp[i] - sRequest.ptr;
                    buf->pos = i;
                    return (HHStatus)csize;
                }
            }

            if (!HHCHSPACE(cp[i])) {
                /* Got a non-space character */
                break;
            }

            ++i;
        }

        /* Look for the abs_path first, since it usually is */
        if (cp[i] == '/')
        {

            /* Remember start of Request-URI and abs_path */
            str = (char *)&cp[i];
            sReqURI.ptr = sAbsPath.ptr = str;
            ++i;

            /* Assume everything up to the next space or eol is part of it */
            while (1)
            {

                /* Read more when necessary */
                if (!cp[i])
                {
                    if ((csize = NetbufAppend(buf)) < 0)
                    {
                        sRequest.len = (char *)&cp[i] - sRequest.ptr;
                        buf->pos = i;
                        return (HHStatus)csize;
                    }
                }

                /* Start of query string? */
                if ((cp[i] == '?') && !sQuery.ptr)
                {

                    /* Yes, save length of abs_path */
                    sAbsPath.len = (char *)&cp[i] - str;

                    /* Continue loop, now scanning query string */
                    ++i;
                    str = (char *)&cp[i];
                    sQuery.ptr = str;
                    continue;
                }

                /* Exit loop at space or eol */
                if (HHCHSTOP(cp[i]))
                {
                    break;
                }

                ++i;
            }

            sReqURI.len = (char *)&cp[i] - sReqURI.ptr;

            /* Are we terminating the query, or abs_path? */
            if (sQuery.ptr)
            {
                sQuery.len = (char *)&cp[i] - str;
            }
            else
            {
                sAbsPath.len = sReqURI.len;
            }

            if (HHCHSPACE(cp[i]))
            {
                ++i;

                /* Skip additional spaces after Request-URI */
                /*
                 * NB: Strictly speaking there shouldn't be more
                 * than the one.
                 */
                while (1)
                {

                    if (!cp[i])
                    {
                        if ((csize = NetbufAppend(buf)) < 0)
                        {
                            /* EOF or error */
                            sRequest.len = (char *)&cp[i] - sRequest.ptr;
                            buf->pos = i;
                            return (HHStatus)csize;
                        }
                    }

                    if (!HHCHSPACE(cp[i]))
                    {
                        /* Got a non-space character */
                        break;
                    }

                    ++i;
                }

                sProtocol.ptr = str = (char *)&cp[i];
                if (((csize - i) > 8) && str[0] == 'H' && str[1] == 'T' &&
                    str[2] == 'T' && str[3] == 'P' && str[4] == '/')
                {
                    if (PR_TRUE == ScanVersion(i, cp, str, buf))
                    {
                        if (((cp[i] == '\r') && (cp[++i] == '\n')) ||
                            (cp[i] == '\n'))
                        {
                            buf->pos = ++i;

                            return HHSTATUS_SUCCESS;
                        }
                    }
                }
            }
        }

        /* Reset to right after "GET " */
        i = 4;

        /* Reset these since they might have been changed */
        sAbsPath.ptr = NULL;
        sAbsPath.len = 0;
        sReqURI.ptr = NULL;
        sReqURI.len = 0;
        sQuery.ptr = NULL;
        sQuery.len = 0;
        sProtocol.ptr = NULL;
        sProtocol.len = 0;
        sRequest.len = 0;
    }

    /* 
     * If i==0, we have not parsed the special case GET method above.  It
     *   is here that we will throw away any leading CRLFs before the method.
     *   We do this because it is specified in section 4.1 of the HTTP/1.1
     *   specification (to be robust in the face of buggy browsers).  
     *   Microsoft also has a test tool that will add an extra newline between
     *   pipelined requests, and we do not want NES to fail in this scenario.
     *   We will limit the number that we accept to 2 CRLFs -- an arbitrary 
     *   value that should be good enough (Microsoft IIS supports one). 
     */
    if (i == 0) {
	int throwaway = 4; /* leading '\r' or '\n', any order, for simplicity */

        /* Compute hash of method string */
        hash = 0;
        str = (char *)(cp);

        while (1) {

            /* Need more data? */
            if (!cp[i]) {
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    sRequest.len = (char *)&cp[i] - sRequest.ptr;
                    buf->pos = i;
                    return (HHStatus)csize;
                }
            }

	    if (throwaway > 0 && (str[0] == '\r' || str[0] == '\n')) {
		throwaway--;
		str++;
		i++;
		continue;
	    }

            if (!HHCHTOKEN(cp[i])) {
                break;
            }

            /* Hash next character of method */
            NSKW_HASHNEXT(hash, cp[i]);
            ++i;
        }

        len = (char *)&cp[i] - str;

        if (len <= 0) {
            /* No method found */
            sRequest.len = (char *)&cp[i] - sRequest.ptr;
            buf->pos = i;
            return HHSTATUS_BAD_REQUEST;
        }

        sRequest.ptr = str;

        sMethod.ptr = str;
        sMethod.len = len;
#ifdef DIE
        if (strncmp(sMethod.ptr, "DIE", 3) == 0) {
          exit(0);
        }
#endif

        /* Look up the method string */
        iMethod = NSKW_LookupKeyword(str, len, PR_TRUE, hash, httpMethods);
        if (iMethod > 0)
            --iMethod;

        /*
         * There should be one space between the method and the URI, but
         * we'll take one or more spaces or tabs.
         */
        if (!HHCHSPACE(cp[i])) {
            sRequest.len = (char *)&cp[i] - sRequest.ptr;
            buf->pos = i;
            return HHSTATUS_BAD_REQUEST;
        }

        ++i;

        while (1) {

            /* Need more data? */
            if (!cp[i]) {
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    sRequest.len = (char *)&cp[i] - sRequest.ptr;
                    buf->pos = i;
                    return (HHStatus)csize;
                }
            }

            /* Exit loop when no more spaces */
            if (!HHCHSPACE(cp[i])) {
                break;
            }

            ++i;
        }
    }

    /* Start URI */

    str = (char *)&cp[i];
    sReqURI.ptr = str;
    sAbsPath.ptr = str;

    /* Ordinarily we expect the URI to start with '/' */
    if (cp[i] != '/') {

        /* Maybe "http://" */
#ifdef MCC_PROXY
        /*, or in the case of CONNECT could be an ip address" */
        if (HHCHALPHANUM(cp[i])) {
#else
        if (HHCHALPHA(cp[i])) {
#endif

            /* Start of absoluteURI */

            ++i;

            /* Get the rest of "http://" or "https://" */
            while ((csize - i) < 7) {
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    sRequest.len = (char *)&cp[i] - sRequest.ptr;
                    buf->pos = i;
                    return (HHStatus)csize;
                }
            }

            if (!strncasecmp((char *)&cp[i], "ttp://", 6)) {
                i += 6;
            } else if (!strncasecmp((char *)&cp[i], "ttps://", 7)) {
                i += 7;
            } else {
                /* URI starts badly */
                sRequest.len = (char *)&cp[i] - sRequest.ptr;
                buf->pos = i;
                return HHSTATUS_BAD_REQUEST;
            }

            fAbsoluteURI = PR_TRUE;

            /* Now look for the host:port part */
            sHost.ptr = (char *)&cp[i];

            PRBool fIPv6Address = PR_FALSE;

            /* Loop to look for end of host */
            while (1) {
                
                if (!cp[i]) {
                    if ((csize = NetbufAppend(buf)) < 0) {
                        /* EOF or error */
                        sRequest.len = (char *)&cp[i] - sRequest.ptr;
                        buf->pos = i;
                        return (HHStatus)csize;
                    }
                }

                /*
                 * Normally we expect the host part to be terminated by '/',
                 * but RFC 2396 seems to say that the abs_path part is
                 * optional.  As a result, things that could follow the
                 * abs_path are potentially valid as well, such as space,
                 * query, or with HTTP/0.9-like syntax, maybe even EOL.
                 */
                if ((cp[i] == '/') || HHCHSPACE(cp[i]) ||
                    (cp[i] == '?') || HHCHEOL(cp[i])) {

                    sHost.len = (char *)&cp[i] - sHost.ptr;
                    sAbsPath.ptr = (char *)&cp[i];
                    break;
                }

                if (!HHCHALPHANUM(cp[i]) && (cp[i] != ':') &&
                    (cp[i] != '.') && (cp[i] != '-')) {

                    /* Bad character in host part */
                    sRequest.len = (char *)&cp[i] - sRequest.ptr;
                    buf->pos = i;
                    return HHSTATUS_BAD_REQUEST;
                }

                ++i;
            }
        }
        else if (cp[i] == '*') {

            sAbsPath.ptr = str;
            sAbsPath.len = 1;
            ++i;

            if (!cp[i]) {
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    sRequest.len = (char *)&cp[i] - sRequest.ptr;
                    buf->pos = i;
                    return (HHStatus)csize;
                }
            }

            /* Need to see space or EOL next */
            if (!HHCHSPACE(cp[i]) && !HHCHEOL(cp[i])) {
                sRequest.len = (char *)&cp[i] - sRequest.ptr;
                buf->pos = i;
                return HHSTATUS_BAD_REQUEST;
            }
        }
        else {
            /* Doesn't look like the start of a URI */
            sRequest.len = (char *)&cp[i] - sRequest.ptr;
            buf->pos = i;
            return HHSTATUS_BAD_REQUEST;
        }
    }

    /* Now are we at the abs_path or query part? */
    if ((cp[i] == '/') || (cp[i] == '?')) {

        /* Set start of appropriate component */
        if (cp[i] == '/') {
            str = (char *)&cp[i];
            sAbsPath.ptr = str;
            ++i;
        }
        else {
            ++i;
            str = (char *)&cp[i];
            sQuery.ptr = str;
        }

        /* Begin scanning abs_path and/or query after the 1st character */
        while (1) {

            /* Need more data? */
            if (!cp[i]) {
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    buf->pos = i;
                    return (HHStatus)csize;
                }
            }

            /* Start of query string? */
            if ((cp[i] == '?') && !sQuery.ptr) {

                /* Yes, save length of abs_path */
                sAbsPath.len = (char *)&cp[i] - str;

                /* Continue loop, now scanning query string */
                ++i;
                str = (char *)&cp[i];
                sQuery.ptr = str;
                continue;
            }

            /*
             * We're hoping for a space to terminate the URI,
             * but support for HTTP/0.9 permits a CRLF, which
             * often appears as just a LF, and loose syntax
             * permits a TAB.
             */
            if (HHCHSTOP(cp[i])) {
                break;
            }

            ++i;
        }


        /* Are we terminating the query, or abs_path? */
        if (sQuery.ptr) {
            sQuery.len = (char *)&cp[i] - str;
        }
        else {
            sAbsPath.len = (char *)&cp[i] - str;
        }
    }

    str = (char *)&cp[i];
    sReqURI.len = str - sReqURI.ptr;

    /* Request URI terminated by space or tab? */
    if (HHCHSPACE(cp[i])) {

        ++i;

        /* Skip additional spaces after Request-URI */
        /* NB: Strictly speaking there shouldn't be more than the one */
        while (1) {

            if (!cp[i]) {
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    sRequest.len = (char *)&cp[i] - sRequest.ptr;
                    buf->pos = i;
                    return (HHStatus)csize;
                }
            }

            if (!HHCHSPACE(cp[i])) {
                /* Got a non-space character */
                break;
            }

            ++i;
        }

        /* Protocol should be coming up */

        /* Ensure 9 more characters */
        while ((csize - i) < 9) {
            if ((csize = NetbufAppend(buf)) < 0) {
                sRequest.len = (char *)&cp[i] - sRequest.ptr;
                buf->pos = i;
                return (HHStatus)csize;
            }
        }

        /* The way I read the spec, it's case insensitive */
        if (!strncasecmp((char *)&cp[i], "HTTP/", 5))
        {

            sProtocol.ptr = str = (char *)&cp[i];

            ScanVersion(i, cp, str, buf);

            if (sProtocol.len == 0)
            {
                /* Syntax error in protocol specification */
                sRequest.len = (char *)&cp[i] - sRequest.ptr;
                buf->pos = i;
                return HHSTATUS_BAD_REQUEST;
            }
        }
    } else {
        /* Assume protocol is HTTP/0.9 if it's not there */
        nClientProtocolVersion = PROTOCOL_VERSION_HTTP09;
    }

    /* Set length of request line */
    sRequest.len = (char *)&cp[i] - sRequest.ptr;

    /* Now we want to see EOL, but we'll accept spaces before that */
    while (1) {

        /* Need more data? */
        if (!cp[i]) {
            if ((csize = NetbufAppend(buf)) < 0) {
                /* EOF or error */
                sRequest.len = (char *)&cp[i] - sRequest.ptr;
                buf->pos = i;
                return (HHStatus)csize;
            }
        }

        if (cp[i-1] != '\r') {
            if (cp[i] == '\r') {
                ++i;
                continue;
            }
            else if (HHCHSPACE(cp[i])) {
                ++i;
                continue;
            }
        }

        if (cp[i] == '\n') {
            ++i;
            break;
        }

        sRequest.len = (char *)&cp[i] - sRequest.ptr;
        buf->pos = i;
        return HHSTATUS_BAD_REQUEST;
    }

    buf->pos = i;
    return HHSTATUS_SUCCESS;
}

HHStatus HttpHeader::ScanHeaders(netbuf *buf)
{
    int i = buf->pos;                /* current position in buffer */
    unsigned char *cp = buf->inbuf;  /* pointer to start of buffer */
    int csize = buf->cursize;
    char *str;
    int len;
    int j;
    unsigned long hash;
    NSHttpHeader hi;
    HHString sTag;
    HHString sValue;

    /* field-value parsing stuff */
    int eov;
    int invalue;
    int tws;
    enum { vs_eovchk, vs_eovchk2, vs_wsskip, vs_inqs, vs_incmt } vState;

    /* Loop while there are more request headers */
    while (1) {

        str = (char *)&cp[i];

        hash = 0;

        sTag.ptr = NULL;
        sTag.len = 0;

        /* Scan header field-name */
        while (1) {

            /* Looking for a token */
            while (HHCHTOKEN(cp[i])) {

                /* Hash next character of header field-name */
                NSKW_HASHNEXT(hash, cp[i]);
                ++i;
            }

            if (!cp[i]) {

                /* Read more data */
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    return (HHStatus)csize;
                }

                if (!cp[i]) {
                    /* null character in data stream */
                    buf->pos = i;
                    return HHSTATUS_BAD_REQUEST;
                }
                continue;
            }

            break;
        }

        len = (char *)&cp[i] - str;

        /* Terminate field-name? */
        if (cp[i] == ':') {
            sTag.ptr = str;
            sTag.len = len;
            ++i;
        }
        else if (len == 0) {

            /* Blank line instead of field-name? */
            if (HHCHEOL(cp[i])) {

                if (cp[i] == '\r') {
                    if (cp[++i] != '\n') {
                        if (!cp[i]) {
                            if ((csize = NetbufAppend(buf)) < 0) {
				return (HHStatus)csize;
                            }
                        }
                        if (cp[i] != '\n') {
                            /* CR without LF */
                            buf->pos = i;
                            return HHSTATUS_BAD_REQUEST;
                        }
                    }
                }

                /* Headers parsed successfully */
                buf->pos = ++i;
                return HHSTATUS_SUCCESS;
            }
            else {
                buf->pos = i;
                return HHSTATUS_BAD_REQUEST;
            }
        }
        else {
            buf->pos = i;
            return HHSTATUS_BAD_REQUEST;
        }

        /* Look up tag as an HTTP header */
        hi = (NSHttpHeader)NSKW_LookupKeyword(str, len, PR_FALSE,
                                              hash, httpHeaders);

        /* Initialize scan of field-value */
        str = (char *)&cp[i];

        eov = 0;
        invalue = 0;
        tws = 0;
        vState = vs_wsskip;

        while (1) {

            if (!cp[i]) {
                if ((csize = NetbufAppend(buf)) < 0) {
                    /* EOF or error */
                    buf->pos = i;
		    return (HHStatus)csize;
                }

                if (!cp[i]) {
                    /* null character in data stream */
                    buf->pos = i;
                    return HHSTATUS_BAD_REQUEST;
                }
            }

            switch (vState) {

            case vs_eovchk:

                /* Handle end of line */
                j = i;
                if (cp[i] == '\r') {
                    if (cp[++i] != '\n') {
                        if (!cp[i]) {
                            if ((csize = NetbufAppend(buf)) < 0) {
				return (HHStatus)csize;
                            }
                        }
                        if (cp[i] != '\n') {
                            /* CR without LF */
                            buf->pos = i;
                            return HHSTATUS_BAD_REQUEST;
                        }
                    }
                    ++tws;
                }
                ++tws;
                ++i;

                /* Go around the loop to make sure we have data */
                vState = vs_eovchk2;
                break;

            case vs_eovchk2:

                /* Check for continuation */
                if (!HHCHSPACE(cp[i])) {

                    /* No, end of field-value */
                    eov = 1;
                    break;
                }

                /* Yes, change EOL to blanks */
                while (j < i) cp[j++] = ' ';

                /* Skip more white space */
                vState = vs_wsskip;
                ++i;

                /* Fall through to vs_wsskip */

            case vs_wsskip:

                /* Skipping space or tab */
                while (1) {
                    while (HHCHSPACE(cp[i])) ++i, ++tws;

                    if (!cp[i]) {
                        if ((csize = NetbufAppend(buf)) < 0) {
                            /* EOF or error */
			    return (HHStatus)csize;
                        }
                        if (!cp[i]) {
                            /* null character in data stream */
                            buf->pos = i;
                            return HHSTATUS_BAD_REQUEST;
                        }
                        continue;
                    }

                    /* Got a non-space character */
                    break;
                }

                /* Check for valid token character */
                if (HHCHTOKEN(cp[i])) {
                    tws = 0;
                    if (!invalue) {
                        invalue = 1;
                        str = (char *)&cp[i];
                    }
                    ++i;
                    while (1) {
                        while (HHCHTOKEN(cp[i])) ++i;
                        if (!cp[i]) {
                            if ((csize = NetbufAppend(buf)) < 0) {
				return (HHStatus)csize;
                            }
                            if (!cp[i]) {
                                /* null character in data stream */
                                buf->pos = i;
                                return HHSTATUS_BAD_REQUEST;
                            }
                            continue;
                        }
                        break;
                    }
                }

                /* End of line? */
                if (HHCHEOL(cp[i])) {
                    /* Got end of line, next check continuation */
                    vState = vs_eovchk;
                    break;
                }

                /* Check for beginning of quoted string */
                if (cp[i] == '"') {
                    vState = vs_inqs;
                    tws = 0;
                    if (!invalue) {
                        invalue = 1;
                        str = (char *)&cp[i];
                    }
                    ++i;
                    break;
                }

                /* Check for beginning of comment */
                if (cp[i] == '(') {
                    vState = vs_incmt;
                    tws = 0;
                    if (!invalue) {
                        invalue = 1;
                        str = (char *)&cp[i];
                    }
                    ++i;
                    break;
                }

                /*
                 * Some field-values allow separators, and some clients
                 * use separators when they are not allowed.
                 */
                if (HHCHSEP(cp[i])) {
                    vState = vs_wsskip;
                    tws = 0;
                    if (!invalue) {
                        invalue = 1;
                        str = (char *)&cp[i];
                    }
                    if ( !(HHCHSPACE(cp[i])) )
                        ++i;
                    break;
                }

                /* What IS this?  It's certainly nothing we want. */
                buf->pos = i;
                return HHSTATUS_BAD_REQUEST;

            case vs_inqs:
            case vs_incmt:

                while (1) {

                    /* Skip spaces internal to quoted string or comment */
                    while (HHCHSPACE(cp[i])) ++i;

                    if (httpCharMask[cp[i]] & (HHCM_CTL|HHCM_SEP)) {

                        /* Need more data? */
                        if (!cp[i]) {

                            /* Read it */
                            if ((csize = NetbufAppend(buf)) < 0) {
                                /* EOF or error */
                                return (HHStatus)csize;
                            }

                            if (!cp[i]) {
                                /* null in data stream */
                                return HHSTATUS_BAD_REQUEST;
                            }
                            continue;
                        }

                        if (vState == vs_inqs) {
                            if (cp[i] == '"') {

                                /*
                                 * End of quoted string,
                                 * check for white space next.
                                 */
                                ++i;
                                vState = vs_wsskip;
                                break;
                            }
                        }
                        else if (cp[i] == ')') {

                            /* End of comment, check for white space next */
                            ++i;
                            vState = vs_wsskip;
                            break;
                        }

                        if (HHCHEOL(cp[i])) {
                            /*
                             * Should begin a continuation, but we'll
                             * assume the end of field-value if not,
                             * even though were missing a closing
                             * quote or right paren.
                             */
                            vState = vs_eovchk;
                            break;
                        }

                        if (httpCharMask[cp[i]] & HHCM_CTL) {
                            /* invalid CTL character */
                            return HHSTATUS_BAD_REQUEST;
                        }
                    }

                    /* Check for quoted-pair */
                    if (cp[i] == '\\') {

                        /* Yes, absorb the next character */
                        if (!cp[++i]) {
                            if ((csize = NetbufAppend(buf)) < 0) {
                                return (HHStatus)csize;
                            }
                        }
                    }

                    ++i;
                }
                break;
            }

            /* Exit loop on end of field-value */
            if (eov) {
                break;
            }
        }

        len = (char *)&cp[i] - str - tws;

        if (eov) {
            sValue.ptr = str;
            sValue.len = len;
        }

        /* Add header entry to instance */
        if (nRqHeaders >= maxRqHeaders)
        {
            return HHSTATUS_TOO_MANY_HEADERS;
        }
        else
        {
            rqHeaders[nRqHeaders].tag = sTag;
            rqHeaders[nRqHeaders].val = sValue;
            rqHeaders[nRqHeaders].ix = hi;
            rqHeaders[nRqHeaders].next = -1;

            /* Process header field-value as indicated by the tag */
            switch (hi) {

            case NSHttpHeader_Cache_Control:
                if (hCacheControl) {
                    AppendHeader(hCacheControl, nRqHeaders);
                }
                else {
                    hCacheControl = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_Connection:
                if (hConnection) {
                    AppendHeader(hConnection, nRqHeaders);
                }
                else {
                    hConnection = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_Content_Length:
                if (hContentLength) {
                    AppendHeader(hContentLength, nRqHeaders);
                }
                else {
                    hContentLength = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_Host:
                hHost = &rqHeaders[nRqHeaders];
                if (sHost.ptr == NULL) {
                    sHost.ptr = str;
                    sHost.len = len;
                }
                break;

            case NSHttpHeader_If_Match:
                if (hIfMatch) {
                    AppendHeader(hIfMatch, nRqHeaders);
                }
                else {
                    hIfMatch = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_If_Modified_Since:
                if (hIfModifiedSince) {
                    AppendHeader(hIfModifiedSince, nRqHeaders);
                }
                else {
                    hIfModifiedSince = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_If_None_Match:
                if (hIfNoneMatch) {
                    AppendHeader(hIfNoneMatch, nRqHeaders);
                }
                else {
                    hIfNoneMatch = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_If_Range:
                if (hIfRange) {
                    AppendHeader(hIfRange, nRqHeaders);
                }
                else {
                    hIfRange = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_If_Unmodified_Since:
                if (hIfUnmodifiedSince) {
                    AppendHeader(hIfUnmodifiedSince, nRqHeaders);
                }
                else {
                    hIfUnmodifiedSince = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_Pragma:
                if (hPragma) {
                    AppendHeader(hPragma, nRqHeaders);
                }
                else {
                    hPragma = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_Range:
                if (hRange) {
                    AppendHeader(hRange, nRqHeaders);
                }
                else {
                    hRange = &rqHeaders[nRqHeaders];
                }
                break;

            case NSHttpHeader_Referer:
                hReferer = &rqHeaders[nRqHeaders];
                break;

            case NSHttpHeader_Transfer_Encoding:
                hTransferEncoding = &rqHeaders[nRqHeaders];
                break;

            case NSHttpHeader_User_Agent:
                hUserAgent = &rqHeaders[nRqHeaders];
                break;

            case NSHttpHeader_Cookie:
               /* If discard-misquoted-cookies is disabled, do nothing.
                * If discard-misquoted-cookies is enabled (default),
                *     If cookie value doesn't start with a quote, do nothing.
                *     If cookie value starts with a quote (single or double)
                *       then look for a next quote of similar type.
                *         If the first matching quote one is found,
                *           its ok as it won't ruin other cookies,
                *           so break let it fall through the default loop.
                *         If no such matching quote is found,
                *           then discard that particular cookie.
                */
                if (HttpRequest::GetDiscardMisquotedCookies()) {
                    PR_ASSERT(rqHeaders[nRqHeaders].val.ptr != NULL);
                    PRBool unmatchedQuote = PR_FALSE;
                    PRBool findBeginQuote = PR_TRUE;
                    /* If the value doesn't start with a quote or init time
                     *    unmatchedQuote is false
                     * If the value starts with a quote
                     *    unmatchedQuote is set to true.
                     * If the value has a matching quote
                     *    unmatchedQuote is set false again.
                     * In the end check the value of unmatchedQuote if it is:
                     * false :
                     *   MEANS either cookie value didn't start with a quote or
                     *   it started with a quote and matching quote was found
                     * ACTION - do what we were already doing in default loop
                     * true :
                     *   MEANS starting quote was found but no matching quote
                     *    was found
                     *  ACTION - discard that cookie
                    */
                    char quote;
                    char prevChar = ' ';
                    for (int i=0; i < rqHeaders[nRqHeaders].val.len; i++) {
                        char currChar = rqHeaders[nRqHeaders].val.ptr[i];
                        if (findBeginQuote) {
                            // cookie value starts after =
                            if (prevChar == '=') {
                                if (currChar != '\"' && currChar != '\'')
                                    break;
                                // if starting character of value is " or '
                                quote = currChar;
                                findBeginQuote = PR_FALSE;
                                unmatchedQuote = PR_TRUE;
                            }
                        } else if (unmatchedQuote && (currChar == quote)) {
                            unmatchedQuote = PR_FALSE;
                            break;
                        }
                        prevChar = currChar;
                    }
                    if (unmatchedQuote) { // ignore Cookie
                        nRqHeaders--;
                        break;
                    }
                }
                // Fall through to the default object if misquoted cookie
                // handling is disabled or if the cookie isn't misquoted
            default:
                /* If this is a known header, maintain a list for each */
                if (hi > 0) {

                    /* Find a previous instance of the same header? */
                    for (j = 0; j < nRqHeaders; ++j) {
                        if (rqHeaders[j].ix == hi) {

                            /* Yes, add this one to end of list */
                            AppendHeader(&rqHeaders[j], nRqHeaders);
                            break;
                        }
                    }
                }
                break;
            }
            ++nRqHeaders;
        }
    }

    /* Huh? */
    return HHSTATUS_BAD_REQUEST;
}

void HttpHeader::ParseConnectionHeader()
{   
    fKeepAliveRequested = (GetNegotiatedProtocolVersion() >= PROTOCOL_VERSION_HTTP11);

    // Inspect the Connection: header for close and keep-alive tokens
    if (hConnection) {
        // Parse the Connection: header.  For speed, we start off assuming the
        // header consists solely of a single close or keep-alive token.  If
        // that assumption proves false, we use ParseCSList() to iterate
        // through the tokens.
        HHString token = hConnection->val;
        int pos = 0;
        do {
            if (token.len == 5 && !strncasecmp(token.ptr, "close", 5)) {
                fKeepAliveRequested = PR_FALSE;
                if (token.len == hConnection->val.len)
                    break;
            } else if (token.len == 10 && !strncasecmp(token.ptr, "keep-alive", 10)) {
                fKeepAliveRequested = PR_TRUE;
                if (token.len == hConnection->val.len)
                    break;
            }

            PRBool quoted;
            pos = HttpHeader::ParseCSList(&token, &hConnection->val, pos, quoted);
        } while (pos != -1);
    }
}
