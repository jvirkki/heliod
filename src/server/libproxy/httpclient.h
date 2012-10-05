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

#ifndef LIBPROXY_HTTPCLIENT_H
#define LIBPROXY_HTTPCLIENT_H

/*
 * httpclient.h: Proxy server HTTP client
 *
 * Chris Elving
 */

#include "netsite.h"

NSPR_BEGIN_EXTERN_C

/*
 * SAFs
 */
Func httpclient_forward_ip;
Func httpclient_block_ip;
Func httpclient_forward_proxy_auth;
Func httpclient_block_proxy_auth;
Func httpclient_forward_cipher;
Func httpclient_block_cipher;
Func httpclient_forward_keysize;
Func httpclient_block_keysize;
Func httpclient_forward_secret_keysize;
Func httpclient_block_secret_keysize;
Func httpclient_forward_ssl_id;
Func httpclient_block_ssl_id;
Func httpclient_forward_issuer_dn;
Func httpclient_block_issuer_dn;
Func httpclient_forward_user_dn;
Func httpclient_block_user_dn;
Func httpclient_forward_auth_cert;
Func httpclient_block_auth_cert;
Func httpclient_forward_jroute;
Func httpclient_block_jroute;
Func httpclient_forward_cache_info;
Func httpclient_block_cache_info;
Func httpclient_forward_via;
Func httpclient_block_via;
Func httpclient_forward_proxy_agent;
Func httpclient_block_proxy_agent;
Func httpclient_http_client_config;
Func httpclient_service_http;

/*
 * httpclient_init initializes the proxy server HTTP client.
 */
PRStatus httpclient_init(void);

/*
 * httpclient_process is equivalent to httpclient_service_http (i.e. the
 * service-http Service SAF), but it assumes that servact_route has already
 * been called.
 */
int httpclient_process(pblock *pb, Session *sn, Request *rq, char *path, char *proxy_addr, char *origin_addr);

/*
 * httpclient_suppress_request_header specifies an rq->headers header that the
 * HTTP client should omit when it makes an HTTP request.
 */
void httpclient_suppress_request_header(const char *hdr);

/*
 * httpclient_suppress_response_header specifies an rq->srvhdrs header that the
 * HTTP client should omit when it receives an HTTP response.
 */
void httpclient_suppress_response_header(const char *hdr);

/*
 * httpclient_set_dest_ip indicates whether the HTTP client should send a
 * Dest-ip: header in response to a request that contains Pragma: dest-ip.
 */
void httpclient_set_dest_ip(Session *sn, Request *rq, PRBool b);

/*
 * httpclient_set_connect_timeout specifies the HTTP client connect() timeout.
 */
void httpclient_set_connect_timeout(Session *sn, Request *rq, PRIntervalTime timeout);

/*
 * httpclient_set_timeout specifies the HTTP client idle timeout.
 */
void httpclient_set_timeout(Session *sn, Request *rq, PRIntervalTime timeout);

/*
 * httpclient_set_retries sets the maximum number of times the HTTP client will
 * retry an HTTP request.
 */
void httpclient_set_retries(Session *sn, Request *rq, int retries);

/*
 * httpclient_set_jroute_header configures the name of the jroute header.  The
 * default is "Proxy-jroute".
 */
void httpclient_set_jroute_header(Session *sn, Request *rq, const char *hdr);

/*
 * httpclient_reformat_request_headers instructs the HTTP client to reformat
 * the request headers (using rq->headers), ignoring full-headers in rq->reqpb.
 */
void httpclient_reformat_request_headers(Session *sn, Request *rq);

NSPR_END_EXTERN_C

#endif /* LIBPROXY_HTTPCLIENT_H */
