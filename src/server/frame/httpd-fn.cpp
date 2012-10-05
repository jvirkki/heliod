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
 * httpd-fn.c: Server application functions for the MCOM httpd
 *
 * Rob McCool
 */


#include "base/pblock.h"
#include "base/session.h"
#include "frame/req.h"
#include "frame/func.h"
#include "safs/auth.h"
#include "safs/ntrans.h"
#include "safs/otype.h"
#include "safs/acl.h"
#include "safs/pcheck.h"
#include "safs/perf.h"
#include "safs/service.h"
#include "safs/cgi.h"
#include "safs/qos.h"
#include "safs/preencrypted.h"
#include "safs/flexlog.h"
#include "safs/addlog.h"
#include "safs/headerfooter.h"
#include "safs/nstpsafs.h"
#include "safs/ntconsafs.h"
#include "safs/init_fn.h"
#include "safs/init.h"
#include "safs/nsapicachesaf.h"
#include "safs/index.h"
#include "safs/upload.h"
#include "safs/reconfig.h"
#include "safs/debug.h"
#include "safs/deprecated.h"
#include "safs/clauth.h"
#include "safs/cond.h"
#include "safs/filtersafs.h"
#include "safs/trace.h"
#include "safs/httpcompression.h"
#include "safs/reqlimit.h"
#include "safs/dump.h"
#include "safs/logsafs.h"
#include "safs/errorbong.h"
#include "libproxy/reverse.h"
#include "libproxy/stuff.h"
#include "libproxy/route.h"
#include "libproxy/channel.h"
#include "libproxy/httpclient.h"
#include "libproxy/proxyerror.h"
#include "shtml/ShtmlSaf.h"
#ifdef FEAT_SECRULE
#include "libsecrule/sec_filter.h"
#endif

#ifdef NET_SSL
Func record_keysize;
Func service_keytoosmall;
#endif

#ifdef DLL_CAPABLE
#include "safs/dl.h"
#endif

#ifdef DEBUG_CACHES
Func cache_service_debug;
#endif
#ifdef DNS_CACHE
#include "base/dns_cache.h"
#endif 

#include "safs/nsfcsafs.h"
#include "safs/poolsafs.h"

struct FuncStruct func_standard[] = {
    {"qos-handler", qos_handler, NULL, 0},
    {"qos-error", qos_error, NULL, 0},
    {"basic-ncsa", auth_basic1, NULL, 0},
    {"basic-auth", auth_basic, NULL, 0},
    {"get-sslid", auth_get_sslid, NULL, 0},
    {"simple-userdb", simple_user, NULL, 0},
    {"simple-groupdb", simple_group, NULL, 0},
    {"dbm-userdb", nsapi_deprecated, NULL, 0},
    {"strip-params", ntrans_strip_params, NULL, 0},
    {"pfx2dir", ntrans_pfx2dir, NULL, 0},
#ifdef XP_UNIX 
    {"unix-home", ntrans_unix_home, NULL, 0},
    {"init-uhome", ntrans_uhome_init, NULL, 0},
#endif /* XP_UNIX */
    {"document-root", ntrans_docroot, NULL, 0},
    {"user2name", ntrans_user2name, NULL, 0},
    {"redirect", ntrans_redirect, NULL, 0},
    {"home-page", ntrans_homepage, NULL, 0},
    {"assign-name", ntrans_assign_name, NULL, 0},
    {"mozilla-redirect", ntrans_mozilla_redirect, NULL, 0},
    {"detect-vulnerable", pcheck_detect_vulnerable, NULL, 0},

    {"set-cache-control", pcheck_set_cache_control, NULL, 0},
    {"uri-clean", pcheck_uri_clean, NULL, 0},
#ifdef XP_UNIX
    {"unix-uri-clean", pcheck_uri_clean, NULL, 0},
#else /* WIN32 */
    {"nt-uri-clean", pcheck_uri_clean, NULL, 0},
    {"ntcgicheck", pcheck_ntcgicheck, NULL, 0},
#endif /* WIN32 */
    {"find-index", pcheck_find_index, NULL, 0},
    {"require-auth", pcheck_require_auth, NULL, 0},
    {"deny-existence", pcheck_deny_existence, NULL, 0},
#ifdef XP_UNIX
    {"find-links", pcheck_find_links, NULL, 0},
#endif /* XP_UNIX */
    {"find-pathinfo", pcheck_find_path, NULL, 0},
    {"load-config", pcheck_nsconfig, NULL, 0},
    {"check-acl", pcheck_check_acl, NULL, 0},
    {"acl-state", pcheck_acl_state, NULL, 0},
    {"ssl-check", pcheck_ssl_check, NULL, 0},
    {"ssl-logout", pcheck_ssl_logout, NULL, 0},

    {"type-by-extension", otype_ext2type, NULL, 0},
    {"type-by-exp", otype_typebyexp , NULL, 0},
    {"force-type" , otype_forcetype , NULL, 0},
    {"change-type", otype_changetype, NULL, 0},
    {"shtml-hacktype", otype_shtmlhacks, NULL, 0},
    {"image-switch", otype_imgswitch, NULL, 0},
    {"html-switch", otype_htmlswitch, NULL, 0},

    {"load-types", nsapi_deprecated, NULL, 0},

    {"send-file", service_plain_file, NULL, 0},
    {"send-range", service_plain_range, NULL, 0},
    {"send-preencrypted", service_preencrypted, NULL,0},

    {"send-error", service_send_error, NULL, 0}, 
    {"append-trailer", service_trailer, NULL, 0},
    {"disable-types", service_disable_type, NULL, 0},
#ifdef XP_WIN32
    {"send-cgi", cgi_send, NULL, FUNC_USE_NATIVE_THREAD},
    {"query-handler", cgi_query, NULL, FUNC_USE_NATIVE_THREAD},
#else
    {"send-cgi", cgi_send, NULL, 0},
    {"query-handler", cgi_query, NULL, 0},
#endif
#ifdef XP_WIN32
    {"send-wincgi", wincgi_send, NULL, FUNC_USE_NATIVE_THREAD},
    {"send-shellcgi", shellcgi_send, NULL, FUNC_USE_NATIVE_THREAD},
#endif /* XP_WIN32 */
#ifndef MCC_ADMSERV
    {"init-cgi", cgi_init, NULL, 0},
#endif
    {"thread-pool-init", nstp_init_saf,   NULL, 0},
    {"imagemap", service_imagemap, NULL, 0},
    {"index-simple", index_simple, NULL, 0},
    {"index-common", cindex_send, NULL, 0},
    {"cindex-init", cindex_init, NULL, 0},

    {"parse-html", shtml_send, NULL, 0},
    {"shtml_send", shtml_send, NULL, 0},
    {"shtml-send", shtml_send, NULL, 0},
    {"shtml_init", shtml_init, NULL, 0},
    {"shtml-init", shtml_init, NULL, 0},
    {"upload-file", upload_file, NULL, 0},
    {"delete-file", remove_file, NULL, 0},
    {"list-dir", list_dir, NULL, 0},
    {"make-dir", make_dir, NULL, 0},
    {"remove-dir", remove_dir, NULL, 0},
    {"rename-file", rename_file, NULL, 0},

    {"common-log", clf_record, NULL, 0},
    {"record-useragent", record_useragent, NULL, 0},
    {"init-clf", clf_init, NULL, 0},

    {"flex-init", flex_init, NULL, 0},
    {"flex-log", flex_log, NULL, 0},

    {"get-client-cert", CA_getcert, NULL, 0},
    {"cert2user", nsapi_deprecated, NULL, 0},    

    {"key-toosmall", service_keytoosmall, NULL, 0},
    {"record-keysize", record_keysize, NULL, 0},
#ifdef DLL_CAPABLE
    {"load-modules", load_modules, NULL, 0},
#endif

    {"cache-init", nsapi_deprecated, NULL, 0},
    {"accel-cache-init", nsapi_deprecated, NULL, 0},
    {"nsapi-cache-init", nsapi_deprecated, NULL, 0},
#ifdef DEBUG_CACHES
    {"cache-debug", cache_service_debug, NULL, 0},
#endif
    {"time-cache-init", nsapi_deprecated, NULL, 0},
    {"pool-init", pool_init, NULL, 0},
#ifdef DEBUG_CACHES
    {"pool-debug", pool_service_debug, NULL, 0},
#endif

    {"acl-register-module", register_module, NULL, 0},
    {"acl-register-dbtype", register_database_type, NULL, 0},
    {"acl-register-dbname", register_database_name, NULL, 0},
    {"acl-register-method", register_method, NULL, 0},
    {"acl-register-getter", register_attribute_getter, NULL, 0},
    {"acl-set-default-method", set_default_method, NULL, 0},
    {"acl-set-default-database", set_default_database, NULL, 0},

    {"service-dump", service_dumpstats, NULL, 0},
    {"service-toobusy", service_toobusy, NULL, 0},
    {"service-nsfc-dump", nsfc_cache_list, NULL, 0},
    {"service-pool-dump", service_pool_dump, NULL, 0},

    {"add-header", pcheck_add_header, NULL, 0},
    {"add-footer", pcheck_add_footer, NULL, 0},

    {"perf-init", perf_init, NULL, 0},
    {"define-perf-bucket", perf_define_bucket , NULL, 0	},
    {"nt-console-init"	 , nt_console_init_saf, NULL, 0 },
    {"register-http-method", register_http_method, NULL, 0 },
    {"set-virtual-index", pcheck_set_virtual_index, NULL, 0 },
    {"set-default-type", otype_setdefaulttype, NULL, 0 },
    {"stats-xml", stats_xml, NULL, 0 },
    {"service-reconfig", service_reconfig, NULL, 0 },
    {"service-debug", service_debug, NULL, 0 },
    {"service-unit-tests", service_unit_tests, NULL, 0 },
    {"set-variable", ntrans_set_variable, NULL, 0 },
    {"match-browser", ntrans_match_browser, NULL, 0 },
    {"cond-match-variable", cond_match_variable, NULL, 0 },
    {"insert-filter", insert_filter, NULL, 0 },
    {"remove-filter", remove_filter, NULL, 0 },
    {"init-filter-order", init_filter_order, NULL, 0 },
    {"service-trace", service_trace, NULL, 0 },
    {"find-compressed", find_compressed, NULL, 0 },
    {"compress-file", compress_file, NULL, 0 },
    {"reverse-map", ntrans_reverse_map, NULL },
    {"map", ntrans_map, NULL },
    {"regexp-map", ntrans_regexp_map, NULL },
    {"regexp-redirect", ntrans_regexp_redirect, NULL, 0},
    {"set-basic-auth", otype_set_basic_auth, NULL, 0},
    {"magnus-internal/force-http-route", route_magnus_internal_force_http_route, NULL, 0},
    {"route-offline", route_route_offline, NULL, 0},
    {"set-origin-server", route_set_origin_server, NULL, 0},
    {"set-proxy-server", route_set_proxy_server, NULL, 0},
    {"ssl-client-config", channel_ssl_client_config, NULL, 0},
    {"block-auth-cert", httpclient_block_auth_cert, NULL, 0},
    {"block-cache-info", httpclient_block_cache_info, NULL, 0},
    {"block-cipher", httpclient_block_cipher, NULL, 0},
    {"block-ip", httpclient_block_ip, NULL, 0},
    {"block-issuer-dn", httpclient_block_issuer_dn, NULL, 0},
    {"block-jroute", httpclient_block_jroute, NULL, 0},
    {"block-keysize", httpclient_block_keysize, NULL, 0},
    {"block-proxy-agent", httpclient_block_proxy_agent, NULL, 0},
    {"block-proxy-auth", httpclient_block_proxy_auth, NULL, 0},
    {"block-secret-keysize", httpclient_block_secret_keysize, NULL, 0},
    {"block-ssl-id", httpclient_block_ssl_id, NULL, 0},
    {"block-user-dn", httpclient_block_user_dn, NULL, 0},
    {"block-via", httpclient_block_via, NULL, 0},
    {"forward-auth-cert", httpclient_forward_auth_cert, NULL, 0},
    {"forward-cache-info", httpclient_forward_cache_info, NULL, 0},
    {"forward-cipher", httpclient_forward_cipher, NULL, 0},
    {"forward-ip", httpclient_forward_ip, NULL, 0},
    {"forward-issuer-dn", httpclient_forward_issuer_dn, NULL, 0},
    {"forward-jroute", httpclient_forward_jroute, NULL, 0},
    {"forward-keysize", httpclient_forward_keysize, NULL, 0},
    {"forward-proxy-agent", httpclient_forward_proxy_agent, NULL, 0},
    {"forward-proxy-auth", httpclient_forward_proxy_auth, NULL, 0},
    {"forward-secret-keysize", httpclient_forward_secret_keysize, NULL, 0},
    {"forward-ssl-id", httpclient_forward_ssl_id, NULL, 0},
    {"forward-user-dn", httpclient_forward_user_dn, NULL, 0},
    {"forward-via", httpclient_forward_via, NULL, 0},
    {"http-client-config", httpclient_http_client_config, NULL, 0},
    {"service-http", httpclient_service_http, NULL, 0},
    {"proxy-retrieve", httpclient_service_http, NULL, 0},
    {"magnus-internal/send-proxy-error", proxyerror_magnus_internal_send_proxy_error, NULL, 0},
    {"check-request-limits", check_request_limits, NULL, 0},
    {"init-request-limits", init_request_limits, NULL, 0},
    {"rewrite", ntrans_rewrite, NULL, 0},
    {"restart", ntrans_restart, NULL, 0},
    {"set-cookie", otype_setcookie, NULL, 0},
    {"log", logsafs_log, NULL, 0},
    {"send-bong-file", send_bong_file, NULL, 0 },
#ifdef FEAT_SECRULE
    {"secrule-config", auth_secrule_config, NULL, 0},
#endif

    {NULL,NULL,NULL, 0}
};
