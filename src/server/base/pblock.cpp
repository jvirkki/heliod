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
 * pblock.c: Handles Parameter Blocks
 * 
 * See pblock.h for public documentation.
 * 
 * Rob McCool
 * 
 * This code uses property lists to implement pblocks.
 */


#include <limits.h>
#include "base/pblock.h"
#include "plist_pvt.h"
#include "base/plist.h"
#include "support/LinkedList.hh"
#include "base/util.h"   /* util_itoa */
#include "base/pool.h"
#include "base/systhr.h"

#define MALLOC_POOL_HANDLE (thread_malloc_key != -1 ? (pool_handle_t *)systhread_getdata(thread_malloc_key) : getThreadMallocPool())

static int thread_malloc_key = -1;
static int _pblock_str2pblock(const char* str, pblock* pb, PRBool lowerCase);

static pool_handle_t *getThreadMallocPool()
{
    pool_handle_t *thread_malloc_pool = 0;

    thread_malloc_key = getThreadMallocKey();
    if (thread_malloc_key != -1) {
        thread_malloc_pool = (pool_handle_t *)systhread_getdata(thread_malloc_key);
    }

    return thread_malloc_pool;
}

/* ------------------------------- HashList ------------------------------- */

template <class Name, class Value>
class HashList {
public:
    HashList(int mask)
    : _mask(mask),
      _list(new CList<Value>[mask + 1])
    { }

    void Insert(Name name, Value *value)
    {
        _list[name & _mask].Append(value);
    }

    const CList<Value>& Find(Name name)
    {
        return _list[name & _mask];
    }

private:
    CList<Value> *_list;
    unsigned int _mask;
};

/* ---------------------- pb_key static initializers ---------------------- */

/*
 * pb_key
 *
 * Contains a precomputed hash value for a specific pblock variable name.
 */
typedef struct pb_key pb_key;
struct pb_key {
    const char *name;
    int namelen; 
    unsigned int hashval;
    int sizendx;
    int hashndx;
};

static HashList<unsigned int, pb_key> _hashKeys(0x7f);
CList<pb_key> _listKeys;

static const pb_key *const _create_key(const char *name, int sizendx = 0)
{
    /* Create a the new pb_key */
    pb_key *key = (pb_key*)malloc(sizeof(pb_key));
    key->name = STRDUP(name);
    key->namelen = strlen(name);
    key->hashval = PListHash(name);
    key->sizendx = sizendx;
    key->hashndx = key->hashval % PLSIZENDX(sizendx);

    /* Group pb_keys by hashval for later retrieval */
    _hashKeys.Insert(key->hashval, key);

    /* Keep a list of all the registered keys */
    _listKeys.Append(key);

    return key;
}

const pb_key *const pb_key_accept = _create_key("accept");
const pb_key *const pb_key_accept_charset = _create_key("accept-charset");
const pb_key *const pb_key_accept_encoding = _create_key("accept-encoding");
const pb_key *const pb_key_accept_language = _create_key("accept-language");
const pb_key *const pb_key_accept_ranges = _create_key("accept-ranges");
const pb_key *const pb_key_actual_route = _create_key("actual-route");
const pb_key *const pb_key_age = _create_key("age");
const pb_key *const pb_key_always_allow_chunked = _create_key("always-allow-chunked");
const pb_key *const pb_key_always_use_keep_alive = _create_key("always-use-keep-alive");
const pb_key *const pb_key_auth_cert = _create_key("auth-cert");
const pb_key *const pb_key_auth_expiring = _create_key("auth-expiring");
const pb_key *const pb_key_auth_group = _create_key("auth-group");
const pb_key *const pb_key_auth_type = _create_key("auth-type");
const pb_key *const pb_key_auth_user = _create_key("auth-user");
const pb_key *const pb_key_authorization = _create_key("authorization");
const pb_key *const pb_key_browser = _create_key("browser");
const pb_key *const pb_key_c2p_cl = _create_key("c2p-cl");
const pb_key *const pb_key_c2p_hl = _create_key("c2p-hl");
const pb_key *const pb_key_cache_info = _create_key("cache-info");
const pb_key *const pb_key_charset = _create_key("charset");
const pb_key *const pb_key_check_http_server = _create_key("check-http-server");
const pb_key *const pb_key_ChunkedRequestBufferSize = _create_key("ChunkedRequestBufferSize");
const pb_key *const pb_key_ChunkedRequestTimeout = _create_key("ChunkedRequestTimeout");
const pb_key *const pb_key_cipher = _create_key("cipher");
const pb_key *const pb_key_clf_request = _create_key("clf-request");
const pb_key *const pb_key_cli_status = _create_key("cli-status");
const pb_key *const pb_key_client_cert_nickname = _create_key("client-cert-nickname");
const pb_key *const pb_key_client_ip = _create_key("client-ip");
const pb_key *const pb_key_close = _create_key("close");
const pb_key *const pb_key_connect_timeout = _create_key("connect-timeout");
const pb_key *const pb_key_connection = _create_key("connection");
const pb_key *const pb_key_cont = _create_key("cont");
const pb_key *const pb_key_content_encoding = _create_key("content-encoding");
const pb_key *const pb_key_content_language = _create_key("content-language");
const pb_key *const pb_key_content_length = _create_key("content-length");
const pb_key *const pb_key_content_location = _create_key("content-location");
const pb_key *const pb_key_content_md5 = _create_key("content-md5");
const pb_key *const pb_key_content_range = _create_key("content-range");
const pb_key *const pb_key_content_type = _create_key("content-type");
const pb_key *const pb_key_cookie = _create_key("cookie");
const pb_key *const pb_key_date = _create_key("date");
const pb_key *const pb_key_DATE_GMT = _create_key("DATE_GMT");
const pb_key *const pb_key_DATE_LOCAL = _create_key("DATE_LOCAL");
const pb_key *const pb_key_dir = _create_key("dir");
const pb_key *const pb_key_Directive = _create_key("Directive");
const pb_key *const pb_key_dns = _create_key("dns");
const pb_key *const pb_key_DOCUMENT_NAME = _create_key("DOCUMENT_NAME");
const pb_key *const pb_key_DOCUMENT_URI = _create_key("DOCUMENT_URI");
const pb_key *const pb_key_domain = _create_key("domain");
const pb_key *const pb_key_enc = _create_key("enc");
const pb_key *const pb_key_engine = _create_key("engine");
const pb_key *const pb_key_error_action = _create_key("error-action");
const pb_key *const pb_key_error_desc = _create_key("error-desc");
const pb_key *const pb_key_error_fn = _create_key("error-fn");
const pb_key *const pb_key_escape = _create_key("escape");
const pb_key *const pb_key_escaped = _create_key("escaped");
const pb_key *const pb_key_etag = _create_key("etag");
const pb_key *const pb_key_expect = _create_key("expect");
const pb_key *const pb_key_expires = _create_key("expires");
const pb_key *const pb_key_expr = _create_key("expr");
const pb_key *const pb_key_filter = _create_key("filter");
const pb_key *const pb_key_find_pathinfo_forward = _create_key("find-pathinfo-forward");
const pb_key *const pb_key_flushTimer = _create_key("flushTimer");
const pb_key *const pb_key_fn = _create_key("fn");
const pb_key *const pb_key_from = _create_key("from");
const pb_key *const pb_key_full_headers = _create_key("full-headers");
const pb_key *const pb_key_hdr = _create_key("hdr");
const pb_key *const pb_key_host = _create_key("host");
const pb_key *const pb_key_hostname = _create_key("hostname");
const pb_key *const pb_key_if_match = _create_key("if-match");
const pb_key *const pb_key_if_modified_since = _create_key("if-modified-since");
const pb_key *const pb_key_if_none_match = _create_key("if-none-match");
const pb_key *const pb_key_if_range = _create_key("if-range");
const pb_key *const pb_key_if_unmodified_since = _create_key("if-unmodified-since");
const pb_key *const pb_key_ip = _create_key("ip");
const pb_key *const pb_key_iponly = _create_key("iponly");
const pb_key *const pb_key_issuer_dn = _create_key("issuer_dn");
const pb_key *const pb_key_jroute = _create_key("jroute");
const pb_key *const pb_key_keep_alive = _create_key("keep-alive");
const pb_key *const pb_key_keep_alive_timeout = _create_key("keep-alive-timeout");
const pb_key *const pb_key_keysize = _create_key("keysize");
const pb_key *const pb_key_lang = _create_key("lang");
const pb_key *const pb_key_LAST_MODIFIED = _create_key("LAST_MODIFIED");
const pb_key *const pb_key_last_modified = _create_key("last-modified");
const pb_key *const pb_key_level = _create_key("level");
const pb_key *const pb_key_location = _create_key("location");
const pb_key *const pb_key_lock_owner = _create_key("lock-owner");
const pb_key *const pb_key_magnus_charset = _create_key("magnus-charset");
const pb_key *const pb_key_magnus_internal = _create_key("magnus-internal");
const pb_key *const pb_key_magnus_internal_dav_src = _create_key("magnus-internal/dav-src");
const pb_key *const pb_key_magnus_internal_default_acls_only = _create_key("magnus-internal/default-acls-only");
const pb_key *const pb_key_magnus_internal_error_j2ee = _create_key("magnus-internal/error-j2ee");
const pb_key *const pb_key_magnus_internal_j2ee_nsapi = _create_key("magnus-internal/j2ee-nsapi");
const pb_key *const pb_key_magnus_internal_preserve_srvhdrs = _create_key("magnus-internal/preserve-srvhdrs-after-req-restart");
const pb_key *const pb_key_magnus_internal_set_request_status = _create_key("magnus-internal/set-request-status");
const pb_key *const pb_key_magnus_internal_set_response_status = _create_key("magnus-internal/set-response-status");
const pb_key *const pb_key_magnus_internal_webapp_errordesc = _create_key("magnus-internal/webapp-errordesc");
const pb_key *const pb_key_matched_browser = _create_key("matched-browser");
const pb_key *const pb_key_max_age = _create_key("max-age");
const pb_key *const pb_key_max_forwards = _create_key("max-forwards");
const pb_key *const pb_key_message = _create_key("message");
const pb_key *const pb_key_method = _create_key("method");
const pb_key *const pb_key_name = _create_key("name");
const pb_key *const pb_key_nocache = _create_key("nocache");
const pb_key *const pb_key_nostat = _create_key("nostat");
const pb_key *const pb_key_ntrans_base = _create_key("ntrans-base");
const pb_key *const pb_key_offline_origin_addr = _create_key("offline-origin-addr");
const pb_key *const pb_key_offline_proxy_addr = _create_key("offline-proxy-addr");
const pb_key *const pb_key_origin_addr = _create_key("origin-addr");
const pb_key *const pb_key_p2c_cl = _create_key("p2c-cl");
const pb_key *const pb_key_p2c_hl = _create_key("p2c-hl");
const pb_key *const pb_key_p2r_cl = _create_key("p2r-cl");
const pb_key *const pb_key_p2r_hl = _create_key("p2r-hl");
const pb_key *const pb_key_parse_timeout = _create_key("parse-timeout");
const pb_key *const pb_key_password = _create_key("password");
const pb_key *const pb_key_path = _create_key("path");
const pb_key *const pb_key_PATH_INFO = _create_key("PATH_INFO");
const pb_key *const pb_key_path_info = _create_key("path-info");
const pb_key *const pb_key_pblock = _create_key("pblock");
const pb_key *const pb_key_poll_interval = _create_key("poll-interval");
const pb_key *const pb_key_port = _create_key("port");
const pb_key *const pb_key_ppath = _create_key("ppath");
const pb_key *const pb_key_pragma = _create_key("pragma");
const pb_key *const pb_key_process_request_body = _create_key("process-request-body");
const pb_key *const pb_key_process_response_body = _create_key("process-response-body");
const pb_key *const pb_key_protocol = _create_key("protocol");
const pb_key *const pb_key_proxy_addr = _create_key("proxy-addr");
const pb_key *const pb_key_proxy_agent = _create_key("proxy-agent");
const pb_key *const pb_key_proxy_auth_cert = _create_key("proxy-auth-cert");
const pb_key *const pb_key_proxy_authorization = _create_key("proxy-authorization");
const pb_key *const pb_key_proxy_cipher = _create_key("proxy-cipher");
const pb_key *const pb_key_proxy_issuer_dn = _create_key("proxy-issuer-dn");
const pb_key *const pb_key_proxy_jroute = _create_key("proxy-jroute");
const pb_key *const pb_key_proxy_keysize = _create_key("proxy-keysize");
const pb_key *const pb_key_proxy_ping = _create_key("proxy-ping");
const pb_key *const pb_key_proxy_request = _create_key("proxy-request");
const pb_key *const pb_key_proxy_secret_keysize = _create_key("proxy-secret-keysize");
const pb_key *const pb_key_proxy_ssl_id = _create_key("proxy-ssl-id");
const pb_key *const pb_key_proxy_user_dn = _create_key("proxy-user-dn");
const pb_key *const pb_key_query = _create_key("query");
const pb_key *const pb_key_QUERY_STRING = _create_key("QUERY_STRING");
const pb_key *const pb_key_QUERY_STRING_UNESCAPED = _create_key("QUERY_STRING_UNESCAPED");
const pb_key *const pb_key_r2p_cl = _create_key("r2p-cl");
const pb_key *const pb_key_r2p_hl = _create_key("r2p-hl");
const pb_key *const pb_key_range = _create_key("range");
const pb_key *const pb_key_referer = _create_key("referer");
const pb_key *const pb_key_reformat_request_headers = _create_key("reformat-request-headers");
const pb_key *const pb_key_remote_status = _create_key("remote-status");
const pb_key *const pb_key_request_jroute = _create_key("request-jroute");
const pb_key *const pb_key_required_rights = _create_key("required-rights");
const pb_key *const pb_key_retries = _create_key("retries");
const pb_key *const pb_key_rewrite_content_location = _create_key("rewrite-content-location");
const pb_key *const pb_key_rewrite_host = _create_key("rewrite-host");
const pb_key *const pb_key_rewrite_location = _create_key("rewrite-location");
const pb_key *const pb_key_rewrite_set_cookie = _create_key("rewrite-set-cookie");
const pb_key *const pb_key_root = _create_key("root");
const pb_key *const pb_key_route = _create_key("route");
const pb_key *const pb_key_route_cookie = _create_key("route-cookie");
const pb_key *const pb_key_route_hdr = _create_key("route-hdr");
const pb_key *const pb_key_route_offline = _create_key("route-offline");
const pb_key *const pb_key_script_name = _create_key("script-name");
const pb_key *const pb_key_secret_keysize = _create_key("secret-keysize");
const pb_key *const pb_key_secure = _create_key("secure");
const pb_key *const pb_key_server = _create_key("server");
const pb_key *const pb_key_set_cookie = _create_key("set-cookie");
const pb_key *const pb_key_socks_addr = _create_key("socks_addr");
const pb_key *const pb_key_ssl_id = _create_key("ssl-id");
const pb_key *const pb_key_ssl_unclean_shutdown = _create_key("ssl-unclean-shutdown");
const pb_key *const pb_key_status = _create_key("status");
const pb_key *const pb_key_sticky_cookie = _create_key("sticky-cookie");
const pb_key *const pb_key_sticky_param = _create_key("sticky-param");
const pb_key *const pb_key_suppress_request_headers = _create_key("suppress-request-headers");
const pb_key *const pb_key_svr_status = _create_key("svr-status");
const pb_key *const pb_key_timeout = _create_key("timeout");
const pb_key *const pb_key_to = _create_key("to");
const pb_key *const pb_key_transfer_encoding = _create_key("transfer-encoding");
const pb_key *const pb_key_transmit_timeout = _create_key("transmit-timeout");
const pb_key *const pb_key_tunnel_non_http_response = _create_key("tunnel-non-http-response");
const pb_key *const pb_key_type = _create_key("type");
const pb_key *const pb_key_upstream_jroute = _create_key("upstream-jroute");
const pb_key *const pb_key_uri = _create_key("uri");
const pb_key *const pb_key_url = _create_key("url");
const pb_key *const pb_key_url_prefix = _create_key("url-prefix");
const pb_key *const pb_key_UseOutputStreamSize = _create_key("UseOutputStreamSize");
const pb_key *const pb_key_user = _create_key("user");
const pb_key *const pb_key_user_agent = _create_key("user-agent");
const pb_key *const pb_key_user_dn = _create_key("user_dn");
const pb_key *const pb_key_validate_server_cert = _create_key("validate-server-cert");
const pb_key *const pb_key_value = _create_key("value");
const pb_key *const pb_key_vary = _create_key("vary");
const pb_key *const pb_key_via = _create_key("via");
const pb_key *const pb_key_warning = _create_key("warning");


/* ------------------------------ _find_key ------------------------------- */

static inline const pb_key *_find_key(const char *name, unsigned int hashval)
{
    /* Check to see if name corresponds to a pb_key */
    CListConstIterator<pb_key> iter(&_hashKeys.Find(hashval));
    const pb_key *key;
    while (key = ++iter) {
        if (key->hashval == hashval && !strcmp(key->name, name))
            return key;
    }
    return NULL;
}


/* --------------------------- _get_hash_index ---------------------------- */

static inline int _get_hash_index(const PListStruct_t *pl, const pb_key *key)
{
    /* Get the hash index from the key.  Requires a symbol table. */
    int i;
    if (key->sizendx == pl->pl_symtab->pt_sizendx)
        i = key->hashndx;
    else
        i = key->hashval % PLSIZENDX(pl->pl_symtab->pt_sizendx);
    return i;
}


/* ---------------------------- _param_create ----------------------------- */

static inline pb_param *_param_create(pool_handle_t *pool_handle, const char *name, int namelen, const char *value, int valuelen)
{
    PLValueStruct_t *ret;

    ret = (PLValueStruct_t *)pool_malloc(pool_handle, sizeof(PLValueStruct_t));

    ret->pv_pbentry.param = &ret->pv_pbparam;
    ret->pv_pbentry.next = 0;
    ret->pv_next = 0;
    ret->pv_type = 0;
    ret->pv_mempool = pool_handle;

    if (name || namelen) {
        ret->pv_name = (char*)pool_malloc(pool_handle, namelen + 1);
        if (name) {
            memcpy(ret->pv_name, name, namelen);
            ret->pv_name[namelen] = '\0';
        } else {
            ret->pv_name[0] = '\0';
        }
    } else {
        ret->pv_name = 0;
    }

    if (value || valuelen) {
        ret->pv_value = (char*)pool_malloc(pool_handle, valuelen + 1);
        if (value) {
            memcpy(ret->pv_value, value, valuelen);
            ret->pv_value[valuelen] = '\0';
        } else {
            ret->pv_value[0] = '\0';
        }
    } else {
        ret->pv_value = 0;
    }

    return &ret->pv_pbparam;
}


/* ----------------------- pblock_key_param_create  ----------------------- */

NSAPI_PUBLIC pb_param *pblock_key_param_create(pblock *pb, const pb_key *key, const char *value, int valuelen)
{
    /*
     * Allocate a PLValueStruct_t from the property list's memory pool.
     */
    PListStruct_t *pl = PBTOPL(pb);
    return _param_create(pl->pl_mempool, key->name, key->namelen, value, valuelen);
}


/* ------------------------- pblock_param_create -------------------------- */

NSAPI_PUBLIC pb_param *pblock_param_create(pblock *pb, const char *name, const char *value)
{
    /*
     * Allocate a PLValueStruct_t from the property list's memory pool.
     */
    PListStruct_t *pl = PBTOPL(pb);
    return _param_create(pl->pl_mempool, name, name ? strlen(name) : 0, value, value ? strlen(value) : 0);
}


/* ----------------------------- param_create ----------------------------- */

NSAPI_PUBLIC pb_param *param_create(const char *name, const char *value)
{
    /*
     * Allocate a PLValueStruct_t containing the pb_param that will
     * be returned.  Normally PLValueStruct_ts are allocated from the
     * memory pool associated with a property list, but we don't have
     * that here, so we just use the thread's pool and indicate we were
     * allocated from a specific pool.
     */
    return _param_create(system_pool(), name, name ? strlen(name) : 0, value, value ? strlen(value) : 0);
}


/* ------------------------------ param_free ------------------------------ */

NSAPI_PUBLIC int param_free(pb_param *pp)
{
    if (pp) {
        PLValueStruct_t *pv = PATOPV(pp);

        /* Don't bother if the pblock was allocated from a pool */
        if (!pv->pv_mempool) {
            pool_free(pv->pv_mempool, pv->pv_name);
            pool_free(pv->pv_mempool, pv->pv_value);
            pool_free(pv->pv_mempool, pv);
        }

        return 1;
    }

    return 0;
}


/* -------------------------- pblock_create_pool -------------------------- */

NSAPI_PUBLIC pblock *pblock_create_pool(pool_handle_t *pool_handle, int n)
{
    /* Create a property list with n property indices */
    PListStruct_t *plist = (PListStruct_t *)PListCreate(pool_handle, n, 0, 0);
    if (!plist)
        return NULL;

    plist->pl_resvpi = 0;

    return &plist->pl_pb;
}


/* ----------------------------- pblock_pool ------------------------------ */

NSAPI_PUBLIC pool_handle_t *pblock_pool(pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);
    return pl->pl_mempool;
}


/* ---------------------------- pblock_create ----------------------------- */

NSAPI_PUBLIC pblock *pblock_create(int n)
{
    return pblock_create_pool(MALLOC_POOL_HANDLE, n);
}


/* ----------------------------- pblock_free ------------------------------ */

NSAPI_PUBLIC void pblock_free(pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);
    PLValueStruct_t **ppval;
    PLValueStruct_t *pv;
    int i;

    if (!pb) {
        return;
    }

    /* If the pools are enabled, this routine has no effect anyway, so 
     * just return. 
     */
    if (pl->pl_mempool || pool_enabled()) {
        return;
    }

    /* Free the property name symbol table if any */
    if (pl->pl_symtab) {
        pool_free(pl->pl_mempool, (void *)(pl->pl_symtab));
    }

    ppval = (PLValueStruct_t **)(pl->pl_ppval);

    /* Loop over the initialized property indices */
    for (i = 0; i < pl->pl_initpi; ++i) {

        /* Got a property here? */
        pv = ppval[i];
        if (pv) {

            param_free(&pv->pv_pbparam);
        }
    }

    /* Free the array of pointers to property values */
    pool_free(pl->pl_mempool, (void *)ppval);

    /* Free the property list head */
    pool_free(pl->pl_mempool, (void *)pl);
}


/* ------------------------------ pblock_key ------------------------------ */

NSAPI_PUBLIC const pb_key *pblock_key(const char *name)
{
    if (!name)
        return NULL;

    return _find_key(name, PListHash(name));
}


/* --------------------------- pblock_kpinsert ---------------------------- */

NSAPI_PUBLIC void pblock_kpinsert(const pb_key *key, pb_param *pp, pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);
    PLValueStruct_t *pv = PATOPV(pp);

    PR_ASSERT(pv->pv_mempool == pl->pl_mempool);

    /* Check to see if the name corresponds to a pb_key */
    unsigned int hashval;
    if (!key) {
        hashval = PListHash(pv->pv_name);
        key = _find_key(pv->pv_name, hashval);
    }

    /* Find property index */
    int pindex = PListGetFreeIndex(pl);
    if (pindex < 1) {
        /* Error - invalid property index */
        return;
    }

    /* Allocate/grow the symbol table as needed */
    PLSymbolTable_t *pt = PListSymbolTable(pl);
    if (!pt) {
        return;
    }

    /* Add PLValueStruct_t to the property list */
    PLValueStruct_t **ppval = (PLValueStruct_t **)(pl->pl_ppval);
    pv->pv_pbkey = key;
    pv->pv_pi = pindex;
    ppval[pv->pv_pi - 1] = pv;

    /* Add name to symbol table */
    int i = key ? _get_hash_index(pl, key) : (hashval % PLSIZENDX(pt->pt_sizendx));
    pv->pv_next = pt->pt_hash[i];
    pt->pt_hash[i] = pv;
    pt->pt_nsyms++;

    PR_ASSERT(param_key(pp) == key);
}


/* ---------------------------- pblock_pinsert ---------------------------- */

NSAPI_PUBLIC void pblock_pinsert(pb_param *pp, pblock *pb)
{
    pblock_kpinsert(NULL, pp, pb);
}


/* --------------------------- pblock_nvinsert ---------------------------- */

NSAPI_PUBLIC pb_param *pblock_nvinsert(const char *name, const char *value, pblock *pb)
{
    pb_param *pp = pblock_param_create(pb, name, value);
    if (pp)
        pblock_kpinsert(NULL, pp, pb);
    return pp;
}


/* --------------------------- pblock_kvinsert ---------------------------- */

NSAPI_PUBLIC pb_param *pblock_kvinsert(const pb_key *key, const char *value, int valuelen, pblock *pb)
{
    pb_param *pp = pblock_key_param_create(pb, key, value, valuelen);
    if (pp)
        pblock_kpinsert(key, pp, pb);
    return pp;
}


/* --------------------------- pblock_nninsert ---------------------------- */

NSAPI_PUBLIC pb_param *pblock_nninsert(const char *name, int value, pblock *pb)
{
    char num[UTIL_ITOA_SIZE];

    util_itoa(value, num);
    return pblock_nvinsert(name, num, pb);
}


/* --------------------------- pblock_kninsert ---------------------------- */

NSAPI_PUBLIC pb_param *pblock_kninsert(const pb_key *key, int value, pblock *pb)
{
    pb_param *pp = pblock_key_param_create(pb, key, NULL, UTIL_ITOA_SIZE);
    if (pp) {
        util_itoa(value, pp->value);
        pblock_kpinsert(key, pp, pb);
    }
    return pp;
}


/* --------------------------- pblock_kllinsert --------------------------- */

NSAPI_PUBLIC pb_param *pblock_kllinsert(const pb_key *key, PRInt64 value, pblock *pb)
{
    pb_param *pp = pblock_key_param_create(pb, key, NULL, UTIL_I64TOA_SIZE);
    if (pp) {
        util_i64toa(value, pp->value);
        pblock_kpinsert(key, pp, pb);
    }
    return pp;
}


/* ---------------------------- pblock_findkey ---------------------------- */

NSAPI_PUBLIC pb_param *pblock_findkey(const pb_key *key, const pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);

    /* Lookup key by examining symbol table */
    if (pl->pl_symtab) {
        int i = _get_hash_index(pl, key);
        PLValueStruct_t *pv;

        /* Search hash collision list for matching name */
        for (pv = pl->pl_symtab->pt_hash[i]; pv; pv = pv->pv_next) {
            if (pv->pv_pbkey == key)
                return &pv->pv_pbparam;
        }
    }

    return NULL;
}


/* -------------------------- pblock_findkeyval --------------------------- */

NSAPI_PUBLIC char *pblock_findkeyval(const pb_key *key, const pblock *pb)
{
    pb_param *pp = pblock_findkey(key, pb);
    return pp ? pp->value : NULL;
}


/* ---------------------------- pblock_findval ---------------------------- */

NSAPI_PUBLIC char *pblock_findval(const char *name, const pblock *pb)
{
    void *pvalue = 0;

    (void)PListFindValue((PList_t)(PBTOPL(pb)), name, &pvalue, 0);

    return (char *)pvalue;
}


/* ------------------------------ pblock_fr ------------------------------ */

NSAPI_PUBLIC pb_param *pblock_fr(const char *name, pblock *pb, int remove)
{
    PListStruct_t *pl = PBTOPL(pb);
    PLValueStruct_t **ppval;
    PLValueStruct_t **pvp;
    PLValueStruct_t *pv = NULL;
    int pindex;
    int i;

    if (pl->pl_symtab) {

        /* Compute hash index of specified property name */
        i = PListHashName(pl->pl_symtab, name);

        /* Search hash collision list for matching name */
        for (pvp = &pl->pl_symtab->pt_hash[i];
             (pv = *pvp); pvp = &(*pvp)->pv_next) {

            if (!strcmp(name, pv->pv_name)) {

                if (remove) {
                    /* Remove PLValueStruct_t from symbol table */
                    *pvp = pv->pv_next;
                    pl->pl_symtab->pt_nsyms--;

                    /* Remove it from pl_ppval too */
                    ppval = (PLValueStruct_t **)(pl->pl_ppval);
                    pindex = pv->pv_pi;
                    ppval[pindex - 1] = 0;
                }
                break;
            }
        }
    }

    return (pv) ? &pv->pv_pbparam : NULL;
}


/* --------------------------- pblock_removekey --------------------------- */

NSAPI_PUBLIC pb_param *pblock_removekey(const pb_key *key, pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);
    PLValueStruct_t **ppval;
    PLValueStruct_t **pvp;
    PLValueStruct_t *pv = NULL;
    int pindex;
    int i;

    if (pl->pl_symtab) {
        /* Lookup hash index for specified property key */
        i = _get_hash_index(pl, key);

        /* Search hash collision list for matching key */
        for (pvp = &pl->pl_symtab->pt_hash[i]; (pv = *pvp); pvp = &pv->pv_next) {
            /* If this value has the requested key... */
            if (pv->pv_pbkey == key) {
                /* Remove PLValueStruct_t from symbol table */
                *pvp = pv->pv_next;
                pl->pl_symtab->pt_nsyms--;

                /* Remove it from pl_ppval too */
                ppval = (PLValueStruct_t **)(pl->pl_ppval);
                pindex = pv->pv_pi;
                ppval[pindex - 1] = 0;

                break;
            }
        }
    }

    return (pv) ? &pv->pv_pbparam : NULL;
}


/* -------------------------- pblock_removeone --------------------------- */

NSAPI_PUBLIC pb_param *pblock_removeone(pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);

    if (pl && pl->pl_symtab) {
        /* Search hash buckets */
        for (int i = 0; i < PLSIZENDX(pl->pl_symtab->pt_sizendx); i++) {
            /* Search hash collision list */
            PLValueStruct_t *pv = pl->pl_symtab->pt_hash[i];
            if (pv) {
                /* Remove PLValueStruct_t from symbol table */
                pl->pl_symtab->pt_hash[i] = pv->pv_next;
                pl->pl_symtab->pt_nsyms--;

                /* Remove it from pl_ppval too */
                PLValueStruct_t **ppval = (PLValueStruct_t**)pl->pl_ppval;
                ppval[pv->pv_pi - 1] = 0;

                return &pv->pv_pbparam;
            }
        }
    }

    return NULL;
}


/* -------------------------- pblock_str2pblock --------------------------- */


int _verify_pbstr(const char *str)
{
    const char *cp;
    const char *scp;
    int np;
    int state;
    int quote;

    for(cp = str, np = 0, state = 0; *cp; ) {
        switch (state) {
        case 0:                 /* skipping leading spaces */

            while (*cp && isspace(*cp)) ++cp;
            if (*cp == '=') {
                return -1;
            }
            if (*cp) state = 1;
            break;

        case 1:                 /* scanning parameter name */

            scp = cp;
            while (*cp && (*cp != '=') && !isspace(*cp)) ++cp;
            if (*cp == '=') ++cp;
            else cp = scp;
            state = 2;
            break;

        case 2:                 /* scanning parameter value */
            quote = 0;
            if (*cp == '\"') {
                quote = 1;
                ++cp;
            }
            for (;;) {
                if (*cp == '\\') {
                    ++cp;
                    if (*cp == 0) {
                        return -1;
                    }
                }
                else if (*cp == '\"') {
                    if (!quote) {
                        return -1;
                    }
                    ++np;
                    ++cp;
                    quote = 0;
                    state = 0;
                    break;
                }
                else if (!quote && (!*cp || isspace(*cp))) {
                    ++np;
                    if (*cp) ++cp;
                    state = 0;
                    break;
                }
                else if (*cp == 0) {
                    return -1;
                }
                ++cp;
            }
            if (quote) {
                return -1;
            }
            break;
        }
    }

    return (state == 0) ? np : -1;
}

NSAPI_PUBLIC int
INTpblock_str2pblock_lowercasename(const char *str, pblock *pb)
{
    return _pblock_str2pblock(str, pb, PR_TRUE);
}

NSAPI_PUBLIC int pblock_str2pblock(const char *str, pblock *pb)
{
    return _pblock_str2pblock(str, pb, PR_FALSE);
}

int
_pblock_str2pblock(const char* str, pblock* pb, PRBool lowerCase)
{
    char *cpy;
    char *cp;
    char *dp;
    char *pname;
    char *pvalue;
    int np;
    int quote;
    int state;
    char numbuf[UTIL_ITOA_SIZE];

    if((np = _verify_pbstr(str)) < 1)
        return -1;

    while (*str && isspace(*str)) ++str;

    cpy = STRDUP(str);

    for (np = 0, cp = cpy, state = 0; *cp; ) {
        switch (state) {

        case 0:                 /* skipping leading spaces */

            while (*cp && isspace(*cp)) ++cp;
            if (*cp) state = 1;
            break;

        case 1:                 /* scanning parameter name */

            pname = cp;
            while (*cp && (*cp != '=') && !isspace(*cp)) ++cp;
            if (*cp == '=') {
                *cp++ = 0;
            }
            else {
                cp = pname;
                pname = numbuf;
                util_itoa(np+1, numbuf);
            }
            state = 2;
            break;

        case 2:                 /* scanning parameter value */
            quote = 0;
            if (*cp == '\"') {
                quote = 1;
                ++cp;
            }
            for (pvalue = cp, dp = cp; ; ++cp, ++dp) {
                if (*cp == '\\') {
                    ++cp;
                }
                else if (*cp == '\"') {
                    ++np;
                    ++cp;
                    *dp = 0;
                    quote = 0;
                    state = 0;
                    break;
                }
                else if (!quote && ((*cp == 0) || isspace(*cp))) {
                    ++np;
                    if (*cp != 0) {
                        ++cp;
                    }
                    *dp = 0;
                    state = 0;
                    break;
                }
                if (cp != dp) *dp = *cp;
            }
            if (lowerCase == PR_TRUE) {
                for (char* p = pname; *p; p++) {
                    *p = tolower(*p);
                }
            }
            pblock_nvinsert(pname, pvalue, pb);
            break;
        }
    }

    FREE(cpy);

    return np;
}


/* -------------------------- pblock_pblock2str --------------------------- */


NSAPI_PUBLIC char *pblock_pblock2str(const pblock *pb, char *str)
{
    register char *s = str, *t, *u;
    PListStruct_t *pl = PBTOPL(pb);
    PLValueStruct_t **ppval;
    PLValueStruct_t *pv;
    int i;
    int sl;
    int xlen;

    ppval = (PLValueStruct_t **)(pl->pl_ppval);

    /* Loop over the initialized property indices */
    for (i = 0, xlen = 0; i < pl->pl_initpi; ++i) {

        /* Got a property here? */
        pv = ppval[i];
        if (pv && pv->pv_name) {

            int ln = strlen(pv->pv_name);
            int lv = strlen((char *)(pv->pv_value));

            /* Check for " or \ because we'll have to escape them */
            for (t = (char *)(pv->pv_value); *t; ++t) {
                if ((*t == '\"') || (*t == '\\')) ++lv;
            }

            /* 4: two quotes, =, and a null */
            xlen += (ln + lv + 4);
        }
    }

    /* Allocate string to hold parameter settings, or increase size */
    if (!s) {
        s = (char *)MALLOC(xlen);
        s[0] = '\0';
        t = &s[0];
        sl = xlen;
    }
    else {
        sl = strlen(s);
        t = &s[sl];
        sl += xlen;
        s = (char *)REALLOC(s, sl);
    }

    /* Loop over the initialized property indices */
    for (i = 0; i < pl->pl_initpi; ++i) {

        /* Got a property here? */
        pv = ppval[i];
        if (pv && pv->pv_name) {

            if (t != s) *t++ = ' ';

            for (u = pv->pv_name; *u; ) *t++ = *u++;

            *t++ = '=';
            *t++ = '\"';

            for (u = (char *)(pv->pv_value); *u; ) {
                if ((*u == '\\') || (*u == '\"')) *t++ = '\\';
                *t++ = *u++;
            }

            *t++ = '\"';
            *t = '\0';
        }
    }

    return s;
}


/* ----------------------------- pblock_copy ------------------------------ */


NSAPI_PUBLIC int pblock_copy(const pblock *src, pblock *dst)
{
    PListStruct_t *pl = PBTOPL(src);
    PLValueStruct_t **ppval;
    PLValueStruct_t *pv;
    int rv = 0;
    int i;

    ppval = (PLValueStruct_t **)(pl->pl_ppval);

    for (i = 0; i < pl->pl_initpi; ++i) {
        pv = ppval[i];
        if (pv) {
            if (pv->pv_pbkey) {
                if (pv->pv_pbkey != pb_key_magnus_internal) {
                    if (!pblock_kvinsert(pv->pv_pbkey, (char *)(pv->pv_value), strlen(pv->pv_value), dst))
                        rv = -1;
                }
            } else {
                if (!pblock_nvinsert(pv->pv_name, (char *)(pv->pv_value), dst))
                    rv = -1;
            }
        }
    }

    return rv;
}

/* ---------------------------- pblock_dup -------------------------------- */

NSAPI_PUBLIC pblock *pblock_dup(const pblock *src)
{
    pblock *dst;

    if (!src)
        return NULL;

    if ( (dst = pblock_create(src->hsize)) )
        pblock_copy(src, dst);

    return dst;
}


/* ---------------------------- pblock_pb2env ----------------------------- */


NSAPI_PUBLIC char **pblock_pb2env(const pblock *pb, char **env)
{
    PListStruct_t *pl = PBTOPL(pb);
    PLValueStruct_t **ppval;
    PLValueStruct_t *pv;
    int i;
    int nval;
    int pos;

    /* Find out how many there are. */
    ppval = (PLValueStruct_t **)(pl->pl_ppval);

    for (i = 0, nval = 0; i < pl->pl_initpi; ++i) {
        if (ppval[i]) ++nval;
    }

    env = util_env_create(env, nval, &pos);

    for (i = 0; i < pl->pl_initpi; ++i) {
        pv = ppval[i];
        if (pv) {
            env[pos++] = util_env_str(pv->pv_name, (char *)(pv->pv_value));
        }
    }
    env[pos] = NULL;

    return env;
}


/* ---------------------------- pblock_replace ---------------------------- */

NSAPI_PUBLIC char * pblock_replace(const char *name,
                                   char * new_value, pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);

    /* Replace an existing value */
    pb_param *pp = pblock_find(name, pb);
    if (!pp)
        return NULL;
    pool_free(pl->pl_mempool, pp->value);
    pp->value = new_value;

    return new_value;
}


/* --------------------------- pblock_nvreplace --------------------------- */

NSAPI_PUBLIC void pblock_nvreplace (const char *name, const char *value, pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);

    /* Replace an existing value or insert a new value */
    pb_param *pp = pblock_find(name, pb);
    if (pp) {
        pool_free(pl->pl_mempool, pp->value);
        pp->value = pool_strdup(pl->pl_mempool, value);
    } else {
        pblock_nvinsert(name, value, pb);
    }
}


/* --------------------------- pblock_kvreplace --------------------------- */

NSAPI_PUBLIC void pblock_kvreplace(const pb_key *key, const char *value, int valuelen, pblock *pb)
{
    PListStruct_t *pl = PBTOPL(pb);

    /* Replace an existing value or insert a new value */
    pb_param *pp = pblock_findkey(key, pb);
    if (pp) {
        pool_free(pl->pl_mempool, pp->value);
        pp->value = (char*)pool_malloc(pl->pl_mempool, valuelen + 1);
        memcpy(pp->value, value, valuelen + 1);
    } else {
        pblock_kvinsert(key, value, valuelen, pb);
    }
}
