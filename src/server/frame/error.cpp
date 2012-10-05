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
 * error.c: Tries to find an action it should take on errors. Default
 * is to send a short HTML page.
 * 
 * Rob McCool
 */


#include "netsite.h"

#include "base/pblock.h"

#include "base/session.h"
#include "frame/req.h"
#include "base/util.h"
#include "frame/func.h"
#include "frame/http.h"
#include "frame/httpfilter.h"
#include "frame/protocol.h"

#include "frame/servact.h"  /* servact_finderror */
#include "frame/error.h"

#include <stdio.h>   /* sscanf */

#include "frame/dbtframe.h"
#include "httpdaemon/libdaemon.h"
#include "support/NSString.h"
#include "support/SimpleHash.h"
#include "support/GenericVector.h"
#include "httpdaemon/vsconf.h"

/*
 * HeaderSet defines a set of header names and their keys.
 */
class HeaderSet {
public:
    inline HeaderSet() : names(31) { names.setMixCase(); }
    inline PRBool contains(const pb_key *key);
    inline PRBool contains(const char *name);
    inline void insert(const char *name);
    inline void remove(const char *name);

private:
    inline void resolveKeys();

    PtrVector<const pb_key> keys;
    SimpleStringHash names;
};

static PRBool IsRefererGood(const char* url);

static HeaderSet *_suppressed_headers;
static HeaderSet *_suppressed_headers_201;
static HeaderSet *_suppressed_headers_304;
static HeaderSet *_suppressed_headers_416;

static const char _content_type_text_html[] = "text/html";
static const char _content_type_text_html_utf8[] = "text/html;charset=UTF-8";

/* --------------------------- error_check_link --------------------------- */

/*
 * Tells the user to check the link they came from for problems upon not
 * found.
 */

int error_check_link(pblock *param, Session *sn, Request *rq)
{
    char *referer;
    int cl, ret;
    char *buf;
    char digit[16];
    const char* notFoundMsg = XP_GetClientStr(DBT_CheckMsg_);
    const char* refererMsg = XP_GetClientStr(DBT_RefererMsg_);
    int notFoundLen = strlen(notFoundMsg);
    /* Remove two characters of junk from %s on Referer Error Page */
    int refererLen = strlen(refererMsg) - 2;

    /* We're already handling an error. */
    if(request_header("referer", &referer, sn, rq) == REQ_ABORTED)
        referer = NULL;

    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    cl = notFoundLen;
    if(referer)
        cl += refererLen + strlen(referer);
    util_itoa(cl, digit);
    pblock_nvinsert("content-length", digit, rq->srvhdrs);
    PRBool goodReferer = IsRefererGood(referer);
    switch(protocol_start_response(sn, rq)) {
    case REQ_NOACTION:
    case REQ_EXIT:
        return REQ_PROCEED;
    }

    if(referer && (goodReferer == PR_TRUE)) {
        buf = (char *) MALLOC(cl + 1);
        strcpy(buf, notFoundMsg);
        util_sprintf(&buf[notFoundLen], refererMsg, referer);
        ret = net_write(sn->csd, buf, cl);
        FREE(buf);
        if(ret == IO_ERROR)
            return REQ_EXIT;
    }
    else 
        if(net_write(sn->csd, notFoundMsg, notFoundLen) == IO_ERROR)
            return REQ_EXIT;

    return REQ_PROCEED;
}


/* ------------------------ HeaderSet::resolveKeys ------------------------ */

inline void HeaderSet::resolveKeys()
{
    /* Reset the list of keys */
    while(keys.pop());

    /* Lookup the key for each named header */
    SimpleHashUnlockedIterator iterator(&names);
    while(iterator.next()) {
        char *name = STRDUP((const char *) iterator.getKey());
        util_strlower(name);
        const pb_key *key = pblock_key(name);
        FREE(name);
        if(key)
            keys.append(key);
    }
}


/* -------------------------- HeaderSet::insert --------------------------- */

inline void HeaderSet::insert(const char *name)
{
    names.insert((void *) name, (void *) PR_TRUE);
    resolveKeys();
}


/* -------------------------- HeaderSet::remove --------------------------- */

inline void HeaderSet::remove(const char *name)
{
    names.remove((void *) name);
    resolveKeys();
}


/* ------------------------- HeaderSet::contains -------------------------- */

inline PRBool HeaderSet::contains(const pb_key *key)
{
    PR_ASSERT(key != NULL);

    /* When n is small, this is more efficient than hashing */
    int n = keys.length();
    while(n--) { 
        if(keys[n] == key)
            return PR_TRUE;
    }

    return PR_FALSE;
}

inline PRBool HeaderSet::contains(const char *name)
{
    return (PRBool) (size_t) names.lookup((void *) name);
}


/* ----------------------------- parse_headers ---------------------------- */

/*
 * Parse a list of headers into a hash table.
 */

static HeaderSet *parse_headers(const char *list)
{
    HeaderSet *hash = new HeaderSet();

    /* Parse list into hash */
    char *listcopy = STRDUP(list);
    char *lasts = NULL;
    char *name = util_strtok(listcopy, ", \r\n\t", &lasts);
    while(name) {
        hash->insert(name);
        name = util_strtok(NULL, ", \r\n\t", &lasts);
    }
    FREE(listcopy);

    return hash;
}


/* ---------------------- error_set_content_headers ----------------------- */

/*
 * Indicate which headers that are considered ContentHeaders.  In general,
 * ContentHeaders - headers that describe the entity body - are removed from
 * srvhdrs before sending an error response.
 */

NSAPI_PUBLIC void error_set_content_headers(const char *headers)
{
    /* Most error responses suppress all the ContentHeaders */
    if(_suppressed_headers)
        delete _suppressed_headers;
    _suppressed_headers = parse_headers(headers);

    /* 201s include Etag */
    if(_suppressed_headers_201)
        delete _suppressed_headers_201;
    _suppressed_headers_201 = parse_headers(headers);
    _suppressed_headers_201->remove("etag");

    /* 304s include Etag, Content-location, Expires, Cache-control, and Vary */
    if(_suppressed_headers_304)
        delete _suppressed_headers_304;
    _suppressed_headers_304 = parse_headers(headers);
    _suppressed_headers_304->remove("etag");
    _suppressed_headers_304->remove("content-location");
    _suppressed_headers_304->remove("expires");
    _suppressed_headers_304->remove("cache-control");
    _suppressed_headers_304->remove("vary");

    /* 416 include Content-range */
    if(_suppressed_headers_416)
        delete _suppressed_headers_416;
    _suppressed_headers_416 = parse_headers(headers);
    _suppressed_headers_416->remove("content-range");
}


/* --------------------------- suppress_headers --------------------------- */

/*
 * Remove the headers named in suppressed from pb.
 */

static inline void suppress_headers(HeaderSet *suppressed, pblock *pb)
{
    for(int i = 0; i < pb->hsize; i++) {
    restart_bucket:
        pb_entry *p = pb->ht[i];
        while(p) {
            /* Check if the header in this pb_param should be suppressed */
            if(const pb_key *key = param_key(p->param)) {
                if(suppressed->contains(key)) {
                    /* Remove this instance of the header */
                    pblock_removekey(key, pb);
                
                    /* Bucket's linked list has changed; start from the top */
                    goto restart_bucket;
                }
            }
            else {
                if(suppressed->contains(p->param->name)) {
                    /* Remove this instance of the header */
                    pblock_remove(p->param->name, pb);
                
                    /* Bucket's linked list has changed; start from the top */
                    goto restart_bucket;
                }
            }

            p = p->next;
        }
    }
}


/* ------------------------------ error_init ------------------------------ */

NSAPI_PUBLIC void error_init(void)
{
    if(!_suppressed_headers) {
        /* Initialize the default set of ContentHeaders headers */
        error_set_content_headers("Age,"
                                  "Content-encoding,"
                                  "Content-language,"
                                  "Content-length,"
                                  "Content-location,"
                                  "Content-md5,"
                                  "Content-range,"
                                  "Content-type,"
                                  "Etag,"
                                  "Expires,"
                                  "Last-modified");
    }
}

/* ----------------------------- error_report ----------------------------- */

/* Special cased to preserve WWW-authenticate and Status */

NSAPI_PUBLIC int error_report(Session *sn, Request *rq)
{
    int rc = REQ_PROCEED;
    const char *msg = NULL;

    const char *s;
    int code, cl;
#define MAX_ERROR_LINE  (REQ_MAX_LINE+512)
    char buf[MAX_ERROR_LINE];
    char *expire;
    char *errorDesc;

    // Get the servlet-specified custom error description (if any)
    errorDesc = pblock_findkeyval(pb_key_magnus_internal_webapp_errordesc,
                                  rq->vars);

    pb_param* pp = pblock_findkey(pb_key_status, rq->srvhdrs);
    if (pp && strncmp(pp->value, "200", 3)) {
        code = atoi(pp->value);
        s = pp->value;
        while (*s && *s != ' ')
            s++;
        if (*s == ' ')
            s++;
    }
    else {
        http_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        code = PROTOCOL_SERVER_ERROR;
        s = http_status_message(PROTOCOL_SERVER_ERROR);
    }

    /*
     * Remove specific headers from srvhdrs before sending the error response.
     * In general, ContentHeaders - headers that describe the entity body - are
     * removed from srvhdrs before sending the error response.
     */
    switch(code) {
    case PROTOCOL_CREATED:
        suppress_headers(_suppressed_headers_201, rq->srvhdrs);
        break;

    case PROTOCOL_NOT_MODIFIED:
        suppress_headers(_suppressed_headers_304, rq->srvhdrs);
        break;

    case PROTOCOL_REQUESTED_RANGE_NOT_SATISFIABLE:
        suppress_headers(_suppressed_headers_416, rq->srvhdrs);
        break;

    default:
        suppress_headers(_suppressed_headers, rq->srvhdrs);
        break;
    }

    if(rq->os) {
        int res = servact_finderror(sn, rq);
        if(res != REQ_NOACTION)
            return res;
    }

    if(const char *fn = pblock_findkeyval(pb_key_error_fn, rq->vars)) {
        pblock *fpb = pblock_create(1);
        if(fpb) {
            pblock_kvinsert(pb_key_fn, fn, strlen(fn), fpb);
            int res = func_exec(fpb, sn, rq);
            pblock_free(fpb);
            if(res != REQ_NOACTION)
                return res;
        }
    }

    /* Failed to find custom function. Do what we can. */

    /* 
     * These should be re-visited to provide more information. Also, the
     * strlen() call at the end on a constant string is dumb.
     */

    switch(code) {
    case PROTOCOL_REDIRECT:
        if (pblock_findkeyval(pb_key_location, rq->srvhdrs) == NULL) {
          if (const char *url = pblock_findkeyval(pb_key_url, rq->vars)) {
            /*
             * The value of "url" in rq->vars is always escaped before
             * using it to set "location" in rq->srvhdrs.  As a result, "url"
             * should only be used when constructing an URL from values that
             * were known to not be escaped.  If a SAF constructs an URL that
             * is already escaped, it should add that value directly to
             * rq->srvhdrs.
             */
            char *p = util_url_escape(NULL, url);
            pblock_kvinsert(pb_key_location, p, strlen(p), rq->srvhdrs);
          }
        }

        /*
         * XXXMB -  Bug 312888
         * There is a MSIE bug that if you send content with a redirect 
         * response, MSIE will not do the redirect correctly.  The reason
         * for us sending content is from the really really old days where
         * some browsers didn't understand redirects.  This is no longer 
         * necessary.  We need to fix on the server side since there are
         * so many broken IE clients already out there.
         */
        break;

    case PROTOCOL_NO_CONTENT:
        protocol_start_response(sn, rq);
        return rc;

    case PROTOCOL_MULTI_STATUS:
        protocol_start_response(sn, rq);
        return rc;

    case PROTOCOL_NOT_MODIFIED:
        protocol_start_response(sn, rq);
        return rc;

    case PROTOCOL_BAD_REQUEST:
        rc = REQ_EXIT;
        msg = XP_GetClientStr(DBT_ProtocolBadRequestMsg_2);
        break;

    case PROTOCOL_UNAUTHORIZED:
        expire = pblock_findval("password-policy", rq->vars);
        if (expire && (strcmp(expire, "expired") == 0))
            msg = XP_GetClientStr(DBT_ProtocolUnauthorizedPWExpiredMsg_2);
        else
            msg = XP_GetClientStr(DBT_ProtocolUnauthorizedMsg_2);
        break;

    case PROTOCOL_FORBIDDEN:
        msg = XP_GetClientStr(DBT_ProtocolForbiddenMsg_2);
        break;

    case PROTOCOL_NOT_FOUND:
        if (errorDesc == NULL)
            return error_check_link(NULL, sn, rq);
        break;

    case PROTOCOL_SERVER_ERROR:
        msg = XP_GetClientStr(DBT_ProtocolServerErrorMsg_2);
        break;

    case PROTOCOL_NOT_IMPLEMENTED:
        rc = REQ_EXIT;
        msg = XP_GetClientStr(DBT_ProtocolNotImplementedMsg_2);
        break;

    case PROTOCOL_REQUEST_TIMEOUT:
        rc = REQ_EXIT;
        msg = XP_GetClientStr(DBT_ProtocolRequestTimeoutMsg_2);
        break;

    case PROTOCOL_LENGTH_REQUIRED:
        rc = REQ_EXIT;
        msg = XP_GetClientStr(DBT_ProtocolLengthRequiredMsg_2);
        break;

    case PROTOCOL_URI_TOO_LARGE:
        rc = REQ_EXIT;
        msg = XP_GetClientStr(DBT_ProtocolURITooLargeMsg_2);
        break;

    case PROTOCOL_ENTITY_TOO_LARGE:
        rc = REQ_EXIT;
        msg = XP_GetClientStr(DBT_ProtocolEntityTooLargeMsg_2);
        break;

    case PROTOCOL_UNSUPPORTED_MEDIA_TYPE:
        rc = REQ_EXIT;
        msg = XP_GetClientStr(DBT_ProtocolUnsupportedMediaTypeMsg_2);
        break;

    case PROTOCOL_REQUESTED_RANGE_NOT_SATISFIABLE:
        msg = XP_GetClientStr(DBT_ProtocolRequestedRangeNotSatisfiableMsg_2);
        break;

    case PROTOCOL_BAD_GATEWAY:
        msg = XP_GetClientStr(DBT_ProtocolBadGatewayMsg_2);
        break;

    case PROTOCOL_GATEWAY_TIMEOUT:
        msg = XP_GetClientStr(DBT_ProtocolGatewayTimeoutMsg_2);
        break;

    case PROTOCOL_LOCKED:
        msg = XP_GetClientStr(DBT_ProtocolLockedMsg_2);
        break;

    case PROTOCOL_FAILED_DEPENDENCY:
        msg = XP_GetClientStr(DBT_ProtocolFailedDependencyMsg_2);
        break;

    case PROTOCOL_INSUFFICIENT_STORAGE:
        msg = XP_GetClientStr(DBT_ProtocolInsufficientStorageMsg_2);
        break;

    case PROTOCOL_SERVICE_UNAVAILABLE:
        msg = XP_GetClientStr(DBT_ProtocolServiceUnavailableMsg_2);
        break;

    default:
        switch (code / 100) {
        case 1:
            /* 1xx responses never have a body */
            protocol_start_response(sn, rq);
            return REQ_PROCEED;

        case 2:
            /* Send an empty response body for unknown 2xx codes */
            break;

        case 3:
            /* Send an empty response body for unknown 3xx codes */
            break;

        case 4:
            /* Send a default response body for unknown 4xx codes */
            msg = XP_GetClientStr(DBT_ProtocolDefault4xxMsg_2);
            break;

        case 5:
            /* Send a default response body for unknown 5xx codes */
            msg = XP_GetClientStr(DBT_ProtocolDefault5xxMsg_2);
            break;

        default:
            /* Invalid response status code */
            rc = REQ_EXIT;
            msg = XP_GetClientStr(DBT_ProtocolDefaultErrorMsg_2);
            break;
        }
        break;
    }

    if (errorDesc != NULL) {
     /* if there is a user specified error message, dont format the
        status using <H1></H1>. Dont set Content-Type META tag either.
        Both these are not i18n compliant */ 
        cl = util_snprintf(buf, MAX_ERROR_LINE,
                  XP_GetClientStr(DBT_HtmlNoContentTypeTitleBody),
                  s, errorDesc);
        pblock_kninsert(pb_key_content_length, cl, rq->srvhdrs);
        pblock_kvreplace(pb_key_content_type, _content_type_text_html_utf8,
                        sizeof(_content_type_text_html_utf8) - 1, rq->srvhdrs);
    }
    else if (msg != NULL) {
     /* This is a standard message that came out of ns-httpd.db. Use the
        appropriate format  for the message */
        cl = util_snprintf(buf, MAX_ERROR_LINE,
                  XP_GetClientStr(DBT_HtmlHeadTitleSTitleHeadNBodyH1SH_),
                  s, s, msg);
        pblock_kninsert(pb_key_content_length, cl, rq->srvhdrs);
        pblock_kvreplace(pb_key_content_type, _content_type_text_html,
                         sizeof(_content_type_text_html) - 1, rq->srvhdrs);
    }
    else {
        cl = 0;
        pblock_kninsert(pb_key_content_length, cl, rq->srvhdrs);
    }


    switch(protocol_start_response(sn, rq)) {
    case REQ_NOACTION:
        return rc;
    case REQ_EXIT:
        return REQ_EXIT;
    }

    if(net_write(sn->csd, buf, cl) == IO_ERROR)
        return REQ_EXIT;
    return rc;
}

PRBool
IsRefererGood(const char* referer)
{
    static char* EVIL_CHARS = "<>%\"'+";
    PRBool res = PR_FALSE;
    if (referer) {
        if (strpbrk(referer, EVIL_CHARS) == NULL) {
            res = PR_TRUE;
        }
    }
    return res;
}
