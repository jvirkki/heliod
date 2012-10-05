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
 * httpact.h: Defines the API characteristics for HTTP servers
 * 
 * Rob McCool
 */


#ifndef HTTPACT_H
#define HTTPACT_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

#include <libaccess/acl.h>
#include <libaccess/aclproto.h>

#ifndef FRAME_REQ_H
#include "frame/req.h"
#endif /* !FRAME_REQ_H */

#ifndef FRAME_OBJECT_H
#include "frame/object.h"
#endif /* !FRAME_OBJECT_H */


/* -------------------------- Generic Prototypes -------------------------- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

/*
 * servact_finderror looks through the request's object set to find a
 * suitable error function to execute. Returns REQ_PROCEED if a function
 * was found and executed successfully, REQ_NOACTION otherwise.
 */
NSAPI_PUBLIC int INTservact_finderror(Session *sn, Request *rq);

/*
 * Read in and handle one request from the given session
 */
NSAPI_PUBLIC void servact_handle(Session *sn);

NSAPI_PUBLIC int INTservact_handle_processed(Session *sn, Request *rq);
NSAPI_PUBLIC void INTservact_error_processed(Session *sn, Request *rq, int status, const char *reason, const char *url);
NSAPI_PUBLIC int INTservact_lookup(Session *sn, Request *rq);

/*
 * Called after servact_handle returns REQ_NOACTION, servact_nowaytohandle will
 * a) send an OPTIONS response and return REQ_PROCEED or b) set the status to
 * PROTOCOL_NOT_IMPLEMENTED, PROTOCOL_METHOD_NOT_ALLOWED, or
 * PROTOCOL_SERVER_ERROR and return REQ_ABORTED.
 */
NSAPI_PUBLIC int servact_nowaytohandle(Session *sn, Request *rq);

/*
 * Returns the translated path (filename) for the given uri, NULL otherwise.
 * If authentication is required for the given uri, nothing is returned even
 * if the current user has authenticated to that area.
 */
NSAPI_PUBLIC char *INTservact_translate_uri(char *uri, Session *sn);
NSAPI_PUBLIC char *INTservact_translate_uri2(char *uri, Session *sn, Request* rq);

/** This method returns ACLList for a request without a restart **/
NSAPI_PUBLIC int ACL_BuildAclList(char *path, const char *luri, ACLListHandle **acllist, ACLListHandle *aclroot);

/*
 * Resolves the given hostname, first trying to find a resolver
 * function from obj.conf, and if that fails, just calls gethostbyname().
 *
 */
NSAPI_PUBLIC PRHostEnt *servact_gethostbyname(const char *host, Session *sn, Request *rq);

/*
 * Establishes a connection to the specified host and port using
 * a Connect class function from obj.conf.  Returns the sockect
 * descriptor that is connected (and which should be SSL_Import()'ed
 * by the caller).
 *
 * Returns -2 (REQ_NOACTION), if no such Connect class function exists.
 * The caller should use the native connect mechanism in that case.
 *
 * Returns -1 (REQ_ABORT) on failure to connect.  The caller should not
 * attempt to use the native connect.
 *
 */
NSAPI_PUBLIC int servact_connect(const char *host, int port, Session *sn, Request *rq);

NSAPI_PUBLIC int INTservact_uri2path(Session *sn, Request *rq);
NSAPI_PUBLIC int INTservact_objset_uri2path(Session *sn, Request *rq, httpd_objset *vs_os);
NSAPI_PUBLIC int INTservact_pathchecks(Session *sn, Request *rq);
NSAPI_PUBLIC int INTservact_fileinfo(Session *sn, Request *rq);
NSAPI_PUBLIC int INTservact_service(Session *sn, Request *rq);
NSAPI_PUBLIC int servact_errors(Session *sn, Request *rq);
NSAPI_PUBLIC int INTservact_route(Session *sn, Request *rq);
NSAPI_PUBLIC int servact_addlogs(Session *sn, Request *rq);
NSAPI_PUBLIC int INTservact_include_file(Session *sn, Request *rq, const char *path);

/*
 * servact_require_right indicates that this request requires the given custom
 * access right be allowed.  Returns 0 on success or -1 on failure.
 */
NSAPI_PUBLIC int servact_require_right(Request *rq, const char *right, int len);

/*
 * servact_include_virtual parses a URI and query string from the location
 * parameter and includes the result of accessing that URI and query string.
 * The optional param pblock specifies CGI env vars or <SERVLET> parameters.
 * Returns REQ_PROCEED if the include was processed successfully.
 */
NSAPI_PUBLIC int servact_include_virtual(Session *sn, Request *rq, const char *location, pblock *param);

/*
 * servact_input runs the Input directives if they haven't yet been run this
 * session. servact_input returns REQ_NOACTION if the filter stack was not
 * modified or REQ_PROCEED if all configured filters were successfully
 * inserted. Any other return value is an error returned by an Input
 * directive. The caller should verify (session_input_done(sn) == PR_FALSE)
 * before calling servact_input.
 */
NSAPI_PUBLIC int servact_input(Session *sn, Request *rq);

/*
 * servact_output runs the Output directives if they haven't yet been run this
 * request. servact_output returns REQ_NOACTION if the filter stack was not
 * modified or REQ_PROCEED if all configured filters were successfully
 * inserted. Any other return value is an error returned by an Output
 * directive. The caller should verify (request_output_done(rq) == PR_FALSE)
 * before calling servact_output.
 */
NSAPI_PUBLIC int servact_output(Session *sn, Request *rq);

NSPR_END_EXTERN_C

#define servact_finderror INTservact_finderror
#define servact_handle_processed INTservact_handle_processed
#define servact_error_processed INTservact_error_processed
#define servact_lookup INTservact_lookup
#define servact_translate_uri INTservact_translate_uri
#define servact_uri2path INTservact_uri2path
#define servact_objset_uri2path INTservact_objset_uri2path
#define servact_pathchecks INTservact_pathchecks
#define servact_fileinfo INTservact_fileinfo
#define servact_service INTservact_service
#define servact_route INTservact_route
#define servact_include_file INTservact_include_file

#endif /* INTNSAPI */

#endif
