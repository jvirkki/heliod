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

#ifndef LIBPROXY_ROUTE_H
#define LIBPROXY_ROUTE_H

/*
 * route.h: Proxy server routing
 *
 * Chris Elving
 */

#include "netsite.h"

NSPR_BEGIN_EXTERN_C

/*
 * SAFs
 */
Func route_set_origin_server;
Func route_set_proxy_server;
NSAPI_PUBLIC Func route_unset_proxy_server;
Func route_set_socks_server;
Func route_unset_socks_server;
Func route_route_offline;
Func route_service_gateway_dump;
Func route_magnus_internal_force_http_route;

/*
 * Setting proxy-addr in rq->vars to "DIRECT" indicates that a request should
 * not be sent through a proxy server
 */
#define ROUTE_PROXY_ADDR_DIRECT ("DIRECT")
#define ROUTE_PROXY_ADDR_DIRECT_LEN (sizeof(ROUTE_PROXY_ADDR_DIRECT) - 1)

/*
 * Setting socks-addr in rq->vars to "DIRECT" indicates that a request should
 * not be sent through a SOCKS server
 */
#define ROUTE_SOCKS_ADDR_DIRECT ("DIRECT")
#define ROUTE_SOCKS_ADDR_DIRECT_LEN (sizeof(ROUTE_SOCKS_ADDR_DIRECT) - 1)

/*
 * route_init initializes the proxy server HTTP routing subsystem.
 */
PRStatus route_init(void);

/*
 * route_init_late creates a thread to monitor offline servers.
 */
PRStatus route_init_late(void);

/*
 * route_get_proxy_addr returns the proxy server address set by a previous call
 * to servact_route.
 */
char * route_get_proxy_addr(Session *sn, Request *rq);

/*
 * route_get_origin_addr returns the origin server address set by a previous
 * call to servact_route.
 */
char * route_get_origin_addr(Session *sn, Request *rq);

/*
 * route_get_socks_addr returns the SOCKS server address set by a previous call
 * to servact_route.
 */
char * route_get_socks_addr(Session *sn, Request *rq);

/*
 * route_offline reruns Route, indicating that the route specified by
 * *proxy_addr and *origin_addr is unusable by setting offline-proxy-addr or
 * offline-origin-addr in rq->vars as appropriate.
 */
int route_offline(Session *sn, Request *rq, char **proxy_addr, char **origin_addr);

/*
 * route_set_actual_route records information about the current route by
 * setting actual-route in rq->vars.  This information may later be inspected
 * during logging.
 */
void route_set_actual_route(Session *sn, Request *rq);

/*
 * route_process_cookies manages route cookies in rq->srvhdrs when the response
 * contains a sticky cookie.
 */
PRBool route_process_cookies(Session *sn, Request *rq);

NSPR_END_EXTERN_C

#endif /* LIBPROXY_ROUTE_H */
