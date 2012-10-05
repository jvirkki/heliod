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

#ifndef FRAME_HTTP_H
#define FRAME_HTTP_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * http.h: Deals with HTTP-related issues
 * 
 * Rob McCool
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

#ifndef BASE_SESSION_H
#include "base/session.h"
#endif /* !BASE_SESSION_H */

#ifndef BASE_PBLOCK_H
#include "base/pblock.h"
#endif /* !BASE_PBLOCK_H */

#ifndef FRAME_REQ_H
#include "frame/req.h"               /* REQ_MAX_LINE, Request structure */
#endif /* !FRAME_REQ_H */

#ifdef NS_OLDES3X
#include <libares/arapi.h>
#else
#include "ares/arapi.h"
#endif /* NS_OLDES3X */

/* ------------------------------ Constants ------------------------------- */

/* The maximum number of RFC-822 headers we'll allow */
/* This would be smaller if a certain browser wasn't so damn stupid. */
#define HTTP_MAX_HEADERS 200

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

/*
 * Initializes the NSAPI HTTP subsystem
 */
NSAPI_PUBLIC int http_init(void);

/*
 * Configures the NSAPI HTTP subsystem to use NSFC
 */
void http_init_late(void);

/*
 * Starting with Sun ONE Web Server 6.1, http_parse_request("", NULL, NULL)
 * returns the NSAPI version.
 */
NSAPI_PUBLIC int INThttp_parse_request(char *t, Request *rq, Session *sn);

/*
 * protocol_start_response - start HTTP response
 *
 * Starts the HTTP response by formatting the HTTP headers.  The headers may
 * be sent now or buffered for later transmission depending on the HTTP filter
 * configuration.  It is not necessary to call this function explicitly as the
 * first net_write() performs the same processing implicitly.
 *
 * If this returns REQ_NOACTION, no body should be sent.  Otherwise, it will
 * return REQ_PROCEED.
 */
NSAPI_PUBLIC int INThttp_start_response(Session *sn, Request *rq);

/*
 * Unchunks and buffers a Transfer-encoding: chunked request entity body.
 * Returns REQ_PROCEED if data was successfully unchunked, REQ_NOACTION if
 * there was nothing to unchunk, REQ_ABORTED if there was an error, and
 * REQ_EXIT if the client went away.  To use the default size and timeout
 * values, pass -1.
 */
NSAPI_PUBLIC int http_unchunk_request(Session *sn, Request *rq, int size, int timeout);

/*
 * INThttp_hdrs2env takes the entries from the given pblock and converts them
 * to an environment. 
 *
 * Each name entry will be made uppercase, prefixed with HTTP_ and any
 * occurrence of - will be converted to _.
 */
NSAPI_PUBLIC char **INThttp_hdrs2env(pblock *pb);

/*
 * INThttp_status sets status to the code n, with reason string r. If r is
 * NULL, the server will attempt to find one for the given status code.
 * If it finds none, it will give "Because I felt like it."
 */

NSAPI_PUBLIC void INThttp_status(Session *sn, Request *rq, int n, const char *r);

/**
 * http_status_message - return a standard message for HTTP protocol codes
 *
 * @parame	code	message code
 * @return	(const char *) with the code
 */
NSAPI_PUBLIC const char * INThttp_status_message (int code);

/*
 * INThttp_set_finfo sets content-length and last-modified and checks
 * preconditions such as if-modified-since
 */
NSAPI_PUBLIC int INThttp_set_finfo(Session *sn, Request *rq, struct stat *finfo);

/* INThttp_set_nsfc_finfo is an alternate form of INThttp_set_finfo */
NSAPI_PUBLIC int http_set_nsfc_finfo(Session *sn, Request *rq, NSFCEntry entry, NSFCFileInfo *finfo);

/*
 * http_check_preconditions checks preconditions such as if-modified-since
 */
NSAPI_PUBLIC int http_check_preconditions(Session *sn, Request *rq, struct tm *mtm, const char *etag);

/*
 * http_format_etag formats an Etag given a resource size and modification time
 */
NSAPI_PUBLIC void http_format_etag(Session *sn, Request *rq, char *etagp, int etaglen, PROffset64 size, time_t mtime);

/*
 * http_weaken_etag replaces any existing strong Etag with a weak Etag. This
 * function is useful when a conditionally inserted filter (for example, the
 * http-compression filter) changes the octets of a response but not the
 * semantics.
 */
NSAPI_PUBLIC void http_weaken_etag(Session *sn, Request *rq);

/*
 * http_match_etag does either a weak or strong comparison of the two specified etags
 */
NSAPI_PUBLIC int http_match_etag(const char *header, const char *etag, int strong);

/*
 * Takes the given pblock and prints headers into the given buffer at 
 * position pos. Returns the buffer, reallocated if needed. Modifies pos.
 * Always leaves room at the end of the buffer for caller to append CRLF.
 */
NSAPI_PUBLIC char *INThttp_dump822(pblock *pb, char *t, int *pos, int tsz);

/*
 * http_dump822_with_slack performs the same processing as http_dump822 but
 * allows the caller to specify how many bytes of slack should be left at the
 * end of the buffer.
 */
NSAPI_PUBLIC char *http_dump822_with_slack(pblock *pb, char *t, int *ip, int tsz, int slack);

/*
 * http_format_status formats an HTTP response status line, including the
 * terminating CRLF, and returns its length.  The line is not nul-terminated.
 */
NSAPI_PUBLIC int http_format_status(Session *sn, Request *rq, char *buf, int sz);

/*
 * http_format_server formats a Server: header field line, including the
 * terminating CRLF, and returns its length.  The line is not nul-terminated.
 */
NSAPI_PUBLIC int http_format_server(Session *sn, Request *rq, char *buf, int sz);

/*
 * Finishes a request. For HTTP, this just closes the socket.
 */
NSAPI_PUBLIC void INThttp_finish_request(Session *sn, Request *rq);

/*
 * INThttp_handle_session processes each request generated by Session
 */
NSAPI_PUBLIC void INThttp_handle_session(Session *sn);

/*
 * INThttp_uri2url takes the give URI prefix and URI suffix and creates a 
 * newly-allocated full URL from them of the form
 * http://(server):(port)(prefix)(suffix)
 * 
 * If you want either prefix or suffix to be skipped, use "" instead of NULL.
 *
 * Normally, the server hostname is taken from the ServerName parameter in
 * magnus.conf. The newer function INThttp_uri2url_dynamic should be used when 
 * a Session and Request structure are available, to ensure that the browser
 * gets redirected to the exact host they were originally referencing.
 */
NSAPI_PUBLIC char *INThttp_uri2url(const char *prefix, const char *suffix);
NSAPI_PUBLIC char *INThttp_uri2url_dynamic(const char *prefix,
                                           const char *suffix,
                                           Session *sn, Request *rq);

/*
 * http_canonical_redirect prepares to redirect the client to the canonical
 * server name. Returns REQ_NOACTION if the client used the canonical server
 * name and no redirection is necessary. Otherwise, returns the value that
 * should be returned from NameTrans to effect the redirect.
 */
NSAPI_PUBLIC int http_canonical_redirect(Session *sn, Request *rq);

/*
 * INThttp_set_keepalive_timeout sets the number of seconds to wait for a new
 * request to come from a persistent connection. Returns nothing. Intended
 * to be called at server startup only.
 *
 * Specifying a timeout of zero will disable persistent connections and allow
 * browsers to request only one file per connection.
 */
NSAPI_PUBLIC void INThttp_set_keepalive_timeout(int secs);

/*
 * http_set_server_header sets the value of the Server: header used in HTTP
 * responses.
 */
void http_set_server_header(const char *server);

/*
 * http_set_protocol_version sets the protocol used in HTTP responses.
 */
int http_set_protocol(const char *version);

/*
 * http_enable_etag enables (if b is PR_TRUE) or disables (if b is PR_FALSE)
 * support for Etags.
 */
void http_enable_etag(PRBool b);

/*
 * http_set_max_unchunk_size sets the maximum amount of a chunked request
 * entity body to unchunk.  (It is necessary to unchunk request entity bodies
 * and compute a content-length for compatibility with legacy SAFs.)
 */
void http_set_max_unchunk_size(int size);

/*
 * http_set_max_unchunk_size sets the maximum amount of time to wait for
 * a chunked request body to arrive.
 */
void http_set_unchunk_timeout(PRIntervalTime timeout);

/*
 * http_make_headers - generate HTTP response headers
 *
 * This function generates the HTTP response headers for a request into
 * a buffer provided by the caller.  This function leaves slack number of
 * unused bytes at the end of the formatted headers.  If the headers are
 * generated successfully, the function return value is the total length
 * of the generated headers.  Otherwise -1 is returned.
 */
char *http_make_headers(Session *sn, Request *rq, char *t, int *ppos, int maxlen, int slack);

/*
 * http_get_method_num returns the method_num for a given method.  Returns -1
 * if the method is unknown.
 */
NSAPI_PUBLIC int http_get_method_num(const char *method);

/*
 * The server HTTP version string, followed by a space (e.g. "HTTP/1.1 "), as
 * set by http_set_protocol_version, and the corresponding version number.
 */
extern char HTTPsvs[80];
extern int HTTPsvs_len;
extern int HTTPprotv_num;

NSPR_END_EXTERN_C

#define http_find_request INThttp_find_request
#define http_parse_request INThttp_parse_request
#define http_scan_headers INThttp_scan_headers
#define http_start_response INThttp_start_response
#define http_hdrs2env INThttp_hdrs2env
#define http_status INThttp_status
#define http_status_message INThttp_status_message
#define http_set_finfo INThttp_set_finfo
#define http_dump822 INThttp_dump822
#define http_finish_request INThttp_finish_request
#define http_handle_session INThttp_handle_session
#define http_uri2url INThttp_uri2url
#define http_uri2url_dynamic INThttp_uri2url_dynamic
#define http_set_keepalive_timeout INThttp_set_keepalive_timeout

#endif /* INTNSAPI */

#endif /* !FRAME_HTTP_H */
