/**
 * PROPRIETARY/CONFIDENTIAL. Use of this product is subject to
 * license terms. Copyright © 2001 Sun Microsystems, Inc.
 * Some preexisting portions Copyright © 2001 Netscape Communications Corp.
 * All rights reserved.
 */
/*
 * stuff.h: Functions to be used in the proxy configuration.
 * 
 * These actually belong to their respective modules, ntrans.c
 * and pcheck.c, but they are here for now because they belong
 * only to the proxy version of the server.
 *
 *
 *
 * Ari Luotonen
 * Copyright (c) 1995 Netscape Communcations Corporation
 *
 */

#ifndef PROXY_STUFF_H
#define PROXY_STUFF_H

#include "frame/func.h"

NSPR_BEGIN_EXTERN_C

Func ntrans_host_map;
Func ntrans_pac_map;
Func ntrans_pat_map;
Func ntrans_regexp_map;
Func ntrans_map;
Func ntrans_regexp_redirect;

Func pcheck_deny_service;
Func pcheck_url_check;
Func pcheck_user_agent_check;
Func pcheck_block_multipart_posts;

Func otype_set_proxy_server;
Func otype_unset_proxy_server;

Func otype_set_socks_server;
Func otype_unset_socks_server;
Func proxy_otype_init;

Func otype_cache_enable;
Func otype_cache_disable;
Func otype_cache_setting;

Func otype_forward_ip;
Func otype_block_ip;
Func otype_ftp_config;
Func otype_http_config;
Func otype_java_ip_check;
Func otype_set_basic_auth;

Func dns_config;

Func route_set_proxy_server;
NSAPI_PUBLIC Func route_unset_proxy_server;

Func route_set_socks_server;
Func route_unset_socks_server;

Func general_append_header;
Func general_set_or_replace_header;
Func general_fix_host_header;

NSPR_END_EXTERN_C

int general_append_header2(const char *name, const char *value, Session *sn, Request *rq);
int general_set_or_replace_header2(const char *name, const char *value, Session *sn, Request *rq);
int general_set_host_header(const char *host, Session *sn, Request *rq);
int general_set_host_header_from_url(char *url, Session *sn, Request *rq);

#endif

