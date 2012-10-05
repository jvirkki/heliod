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

#ifndef BASE_PBLOCK_H
#define BASE_PBLOCK_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * pblock.h: Header for Parameter Block handling functions
 * 
 *
 * A parameter block is a set of name=value pairs which are generally used 
 * as parameters, but can be anything. They are kept in a hash table for 
 * reasonable speed, but if you are doing any intensive modification or
 * access of them you should probably make a local copy of each parameter
 * while working.
 *
 * Rob McCool
 * 
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

#ifdef XP_WIN32
#ifdef BUILD_DLL
#define BASE_DLL _declspec(dllexport)
#else
#define BASE_DLL _declspec(dllimport)
#endif
#else
#define BASE_DLL
#endif

#ifdef INTNSAPI

/* --- Begin function prototypes --- */

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC pb_param *INTparam_create(const char *name, const char *value);

NSAPI_PUBLIC int INTparam_free(pb_param *pp);

NSAPI_PUBLIC pblock *INTpblock_create(int n);

NSAPI_PUBLIC void INTpblock_free(pblock *pb);

NSAPI_PUBLIC char *INTpblock_findval(const char *name, const pblock *pb);

NSAPI_PUBLIC pb_param *INTpblock_nvinsert(const char *name, const char *value, pblock *pb);

NSAPI_PUBLIC pb_param *INTpblock_nninsert(const char *name, int value, pblock *pb);

NSAPI_PUBLIC void INTpblock_pinsert(pb_param *pp, pblock *pb);

NSAPI_PUBLIC int INTpblock_str2pblock(const char *str, pblock *pb);

NSAPI_PUBLIC char *INTpblock_pblock2str(const pblock *pb, char *str);

NSAPI_PUBLIC int INTpblock_copy(const pblock *src, pblock *dst);

NSAPI_PUBLIC pblock *INTpblock_dup(const pblock *src);

NSAPI_PUBLIC char **INTpblock_pb2env(const pblock *pb, char **env);

NSAPI_PUBLIC void pblock_nvreplace (const char *name, const char *value, pblock *pb);

/* --------------------------- Internal things ---------------------------- */

typedef struct pb_key pb_key;

BASE_DLL extern const pb_key *const pb_key_accept;
BASE_DLL extern const pb_key *const pb_key_accept_charset;
BASE_DLL extern const pb_key *const pb_key_accept_encoding;
BASE_DLL extern const pb_key *const pb_key_accept_language;
BASE_DLL extern const pb_key *const pb_key_accept_ranges;
BASE_DLL extern const pb_key *const pb_key_actual_route;
BASE_DLL extern const pb_key *const pb_key_age;
BASE_DLL extern const pb_key *const pb_key_always_allow_chunked;
BASE_DLL extern const pb_key *const pb_key_always_use_keep_alive;
BASE_DLL extern const pb_key *const pb_key_auth_cert;
BASE_DLL extern const pb_key *const pb_key_auth_expiring;
BASE_DLL extern const pb_key *const pb_key_auth_group;
BASE_DLL extern const pb_key *const pb_key_auth_type;
BASE_DLL extern const pb_key *const pb_key_auth_user;
BASE_DLL extern const pb_key *const pb_key_authorization;
BASE_DLL extern const pb_key *const pb_key_browser;
BASE_DLL extern const pb_key *const pb_key_c2p_cl;
BASE_DLL extern const pb_key *const pb_key_c2p_hl;
BASE_DLL extern const pb_key *const pb_key_cache_info;
BASE_DLL extern const pb_key *const pb_key_charset;
BASE_DLL extern const pb_key *const pb_key_check_http_server;
BASE_DLL extern const pb_key *const pb_key_ChunkedRequestBufferSize;
BASE_DLL extern const pb_key *const pb_key_ChunkedRequestTimeout;
BASE_DLL extern const pb_key *const pb_key_cipher;
BASE_DLL extern const pb_key *const pb_key_clf_request;
BASE_DLL extern const pb_key *const pb_key_cli_status;
BASE_DLL extern const pb_key *const pb_key_client_cert_nickname;
BASE_DLL extern const pb_key *const pb_key_client_ip;
BASE_DLL extern const pb_key *const pb_key_close;
BASE_DLL extern const pb_key *const pb_key_connect_timeout;
BASE_DLL extern const pb_key *const pb_key_connection;
BASE_DLL extern const pb_key *const pb_key_cont;
BASE_DLL extern const pb_key *const pb_key_content_encoding;
BASE_DLL extern const pb_key *const pb_key_content_language;
BASE_DLL extern const pb_key *const pb_key_content_length;
BASE_DLL extern const pb_key *const pb_key_content_location;
BASE_DLL extern const pb_key *const pb_key_content_md5;
BASE_DLL extern const pb_key *const pb_key_content_range;
BASE_DLL extern const pb_key *const pb_key_content_type;
BASE_DLL extern const pb_key *const pb_key_cookie;
BASE_DLL extern const pb_key *const pb_key_date;
BASE_DLL extern const pb_key *const pb_key_DATE_GMT;
BASE_DLL extern const pb_key *const pb_key_DATE_LOCAL;
BASE_DLL extern const pb_key *const pb_key_dir;
BASE_DLL extern const pb_key *const pb_key_Directive;
BASE_DLL extern const pb_key *const pb_key_dns;
BASE_DLL extern const pb_key *const pb_key_DOCUMENT_NAME;
BASE_DLL extern const pb_key *const pb_key_DOCUMENT_URI;
BASE_DLL extern const pb_key *const pb_key_domain;
BASE_DLL extern const pb_key *const pb_key_enc;
BASE_DLL extern const pb_key *const pb_key_engine;
BASE_DLL extern const pb_key *const pb_key_error_action;
BASE_DLL extern const pb_key *const pb_key_error_desc;
BASE_DLL extern const pb_key *const pb_key_error_fn;
BASE_DLL extern const pb_key *const pb_key_escape;
BASE_DLL extern const pb_key *const pb_key_escaped;
BASE_DLL extern const pb_key *const pb_key_etag;
BASE_DLL extern const pb_key *const pb_key_expect;
BASE_DLL extern const pb_key *const pb_key_expires;
BASE_DLL extern const pb_key *const pb_key_expr;
BASE_DLL extern const pb_key *const pb_key_filter;
BASE_DLL extern const pb_key *const pb_key_find_pathinfo_forward;
BASE_DLL extern const pb_key *const pb_key_flushTimer;
BASE_DLL extern const pb_key *const pb_key_fn;
BASE_DLL extern const pb_key *const pb_key_from;
BASE_DLL extern const pb_key *const pb_key_full_headers;
BASE_DLL extern const pb_key *const pb_key_hdr;
BASE_DLL extern const pb_key *const pb_key_host;
BASE_DLL extern const pb_key *const pb_key_hostname;
BASE_DLL extern const pb_key *const pb_key_if_match;
BASE_DLL extern const pb_key *const pb_key_if_modified_since;
BASE_DLL extern const pb_key *const pb_key_if_none_match;
BASE_DLL extern const pb_key *const pb_key_if_range;
BASE_DLL extern const pb_key *const pb_key_if_unmodified_since;
BASE_DLL extern const pb_key *const pb_key_ip;
BASE_DLL extern const pb_key *const pb_key_iponly;
BASE_DLL extern const pb_key *const pb_key_issuer_dn;
BASE_DLL extern const pb_key *const pb_key_jroute;
BASE_DLL extern const pb_key *const pb_key_keep_alive;
BASE_DLL extern const pb_key *const pb_key_keep_alive_timeout;
BASE_DLL extern const pb_key *const pb_key_keysize;
BASE_DLL extern const pb_key *const pb_key_lang;
BASE_DLL extern const pb_key *const pb_key_LAST_MODIFIED;
BASE_DLL extern const pb_key *const pb_key_last_modified;
BASE_DLL extern const pb_key *const pb_key_level;
BASE_DLL extern const pb_key *const pb_key_location;
BASE_DLL extern const pb_key *const pb_key_lock_owner;
BASE_DLL extern const pb_key *const pb_key_magnus_charset;
BASE_DLL extern const pb_key *const pb_key_magnus_internal;
BASE_DLL extern const pb_key *const pb_key_magnus_internal_dav_src;
BASE_DLL extern const pb_key *const pb_key_magnus_internal_default_acls_only;
BASE_DLL extern const pb_key *const pb_key_magnus_internal_error_j2ee;
BASE_DLL extern const pb_key *const pb_key_magnus_internal_j2ee_nsapi;
BASE_DLL extern const pb_key *const pb_key_magnus_internal_preserve_srvhdrs;
BASE_DLL extern const pb_key *const pb_key_magnus_internal_set_request_status;
BASE_DLL extern const pb_key *const pb_key_magnus_internal_set_response_status;
BASE_DLL extern const pb_key *const pb_key_magnus_internal_webapp_errordesc;
BASE_DLL extern const pb_key *const pb_key_matched_browser;
BASE_DLL extern const pb_key *const pb_key_max_age;
BASE_DLL extern const pb_key *const pb_key_max_forwards;
BASE_DLL extern const pb_key *const pb_key_message;
BASE_DLL extern const pb_key *const pb_key_method;
BASE_DLL extern const pb_key *const pb_key_name;
BASE_DLL extern const pb_key *const pb_key_nocache;
BASE_DLL extern const pb_key *const pb_key_nostat;
BASE_DLL extern const pb_key *const pb_key_ntrans_base;
BASE_DLL extern const pb_key *const pb_key_offline_origin_addr;
BASE_DLL extern const pb_key *const pb_key_offline_proxy_addr;
BASE_DLL extern const pb_key *const pb_key_origin_addr;
BASE_DLL extern const pb_key *const pb_key_p2c_cl;
BASE_DLL extern const pb_key *const pb_key_p2c_hl;
BASE_DLL extern const pb_key *const pb_key_p2r_cl;
BASE_DLL extern const pb_key *const pb_key_p2r_hl;
BASE_DLL extern const pb_key *const pb_key_parse_timeout;
BASE_DLL extern const pb_key *const pb_key_password;
BASE_DLL extern const pb_key *const pb_key_path;
BASE_DLL extern const pb_key *const pb_key_PATH_INFO;
BASE_DLL extern const pb_key *const pb_key_path_info;
BASE_DLL extern const pb_key *const pb_key_pblock;
BASE_DLL extern const pb_key *const pb_key_poll_interval;
BASE_DLL extern const pb_key *const pb_key_port;
BASE_DLL extern const pb_key *const pb_key_ppath;
BASE_DLL extern const pb_key *const pb_key_pragma;
BASE_DLL extern const pb_key *const pb_key_process_request_body;
BASE_DLL extern const pb_key *const pb_key_process_response_body;
BASE_DLL extern const pb_key *const pb_key_protocol;
BASE_DLL extern const pb_key *const pb_key_proxy_addr;
BASE_DLL extern const pb_key *const pb_key_proxy_agent;
BASE_DLL extern const pb_key *const pb_key_proxy_auth_cert;
BASE_DLL extern const pb_key *const pb_key_proxy_authorization;
BASE_DLL extern const pb_key *const pb_key_proxy_cipher;
BASE_DLL extern const pb_key *const pb_key_proxy_issuer_dn;
BASE_DLL extern const pb_key *const pb_key_proxy_jroute;
BASE_DLL extern const pb_key *const pb_key_proxy_keysize;
BASE_DLL extern const pb_key *const pb_key_proxy_ping;
BASE_DLL extern const pb_key *const pb_key_proxy_request;
BASE_DLL extern const pb_key *const pb_key_proxy_secret_keysize;
BASE_DLL extern const pb_key *const pb_key_proxy_ssl_id;
BASE_DLL extern const pb_key *const pb_key_proxy_user_dn;
BASE_DLL extern const pb_key *const pb_key_query;
BASE_DLL extern const pb_key *const pb_key_QUERY_STRING;
BASE_DLL extern const pb_key *const pb_key_QUERY_STRING_UNESCAPED;
BASE_DLL extern const pb_key *const pb_key_r2p_cl;
BASE_DLL extern const pb_key *const pb_key_r2p_hl;
BASE_DLL extern const pb_key *const pb_key_range;
BASE_DLL extern const pb_key *const pb_key_referer;
BASE_DLL extern const pb_key *const pb_key_reformat_request_headers;
BASE_DLL extern const pb_key *const pb_key_remote_status;
BASE_DLL extern const pb_key *const pb_key_request_jroute;
BASE_DLL extern const pb_key *const pb_key_required_rights;
BASE_DLL extern const pb_key *const pb_key_retries;
BASE_DLL extern const pb_key *const pb_key_rewrite_content_location;
BASE_DLL extern const pb_key *const pb_key_rewrite_host;
BASE_DLL extern const pb_key *const pb_key_rewrite_location;
BASE_DLL extern const pb_key *const pb_key_rewrite_set_cookie;
BASE_DLL extern const pb_key *const pb_key_root;
BASE_DLL extern const pb_key *const pb_key_route;
BASE_DLL extern const pb_key *const pb_key_route_cookie;
BASE_DLL extern const pb_key *const pb_key_route_hdr;
BASE_DLL extern const pb_key *const pb_key_route_offline;
BASE_DLL extern const pb_key *const pb_key_script_name;
BASE_DLL extern const pb_key *const pb_key_secret_keysize;
BASE_DLL extern const pb_key *const pb_key_secure;
BASE_DLL extern const pb_key *const pb_key_server;
BASE_DLL extern const pb_key *const pb_key_set_cookie;
BASE_DLL extern const pb_key *const pb_key_socks_addr;
BASE_DLL extern const pb_key *const pb_key_ssl_id;
BASE_DLL extern const pb_key *const pb_key_ssl_unclean_shutdown;
BASE_DLL extern const pb_key *const pb_key_status;
BASE_DLL extern const pb_key *const pb_key_sticky_cookie;
BASE_DLL extern const pb_key *const pb_key_sticky_param;
BASE_DLL extern const pb_key *const pb_key_suppress_request_headers;
BASE_DLL extern const pb_key *const pb_key_svr_status;
BASE_DLL extern const pb_key *const pb_key_timeout;
BASE_DLL extern const pb_key *const pb_key_to;
BASE_DLL extern const pb_key *const pb_key_transfer_encoding;
BASE_DLL extern const pb_key *const pb_key_transmit_timeout;
BASE_DLL extern const pb_key *const pb_key_tunnel_non_http_response;
BASE_DLL extern const pb_key *const pb_key_type;
BASE_DLL extern const pb_key *const pb_key_upstream_jroute;
BASE_DLL extern const pb_key *const pb_key_uri;
BASE_DLL extern const pb_key *const pb_key_url;
BASE_DLL extern const pb_key *const pb_key_url_prefix;
BASE_DLL extern const pb_key *const pb_key_UseOutputStreamSize;
BASE_DLL extern const pb_key *const pb_key_user;
BASE_DLL extern const pb_key *const pb_key_user_agent;
BASE_DLL extern const pb_key *const pb_key_user_dn;
BASE_DLL extern const pb_key *const pb_key_validate_server_cert;
BASE_DLL extern const pb_key *const pb_key_value;
BASE_DLL extern const pb_key *const pb_key_vary;
BASE_DLL extern const pb_key *const pb_key_via;
BASE_DLL extern const pb_key *const pb_key_warning;

NSAPI_PUBLIC pool_handle_t *pblock_pool(pblock *pb);

NSAPI_PUBLIC pb_param *pblock_param_create(pblock *pb, const char *name, const char *value);

NSAPI_PUBLIC pblock *pblock_create_pool(pool_handle_t *pool_handle, int n);

NSAPI_PUBLIC pb_param *INTpblock_fr(const char *name, pblock *pb, int remove);

NSAPI_PUBLIC char *INTpblock_replace(const char *name,char * new_value,pblock *pb);

NSAPI_PUBLIC int INTpblock_str2pblock_lowercasename(const char *str, pblock *pb);

NSAPI_PUBLIC pb_param *pblock_removeone(pblock *pb);

NSAPI_PUBLIC const pb_key *pblock_key(const char *name);

NSAPI_PUBLIC pb_param *pblock_key_param_create(pblock *pb, const pb_key *key, const char *value, int valuelen);

NSAPI_PUBLIC char *pblock_findkeyval(const pb_key *key, const pblock *pb);

NSAPI_PUBLIC pb_param *pblock_findkey(const pb_key *key, const pblock *pb);

NSAPI_PUBLIC pb_param *pblock_removekey(const pb_key *key, pblock *pb);

NSAPI_PUBLIC pb_param *pblock_kvinsert(const pb_key *key, const char *value, int valuelen, pblock *pb);

NSAPI_PUBLIC void pblock_kpinsert(const pb_key *key, pb_param *pp, pblock *pb);

NSAPI_PUBLIC void pblock_kvreplace(const pb_key *key, const char *value, int valuelen, pblock *pb);

NSAPI_PUBLIC pb_param *pblock_kninsert(const pb_key *key, int value, pblock *pb);

NSAPI_PUBLIC pb_param *pblock_kllinsert(const pb_key *key, PRInt64 value, pblock *pb);

#ifdef __cplusplus
inline const pb_key *param_key(pb_param *pp)
{
    return *(const pb_key **)(pp + 1); /* XXX see plist_pvt.h */
}
#endif

NSPR_END_EXTERN_C

#define param_create INTparam_create
#define param_free INTparam_free
#define pblock_create INTpblock_create
#define pblock_free INTpblock_free
#define pblock_findval INTpblock_findval
#define pblock_nvinsert INTpblock_nvinsert
#define pblock_nninsert INTpblock_nninsert
#define pblock_pinsert INTpblock_pinsert
#define pblock_str2pblock INTpblock_str2pblock
#define pblock_pblock2str INTpblock_pblock2str
#define pblock_copy INTpblock_copy
#define pblock_dup INTpblock_dup
#define pblock_pb2env INTpblock_pb2env
#define pblock_fr INTpblock_fr
#define pblock_replace INTpblock_replace

#endif /* INTNSAPI */

#endif /* !BASE_PBLOCK_H */
