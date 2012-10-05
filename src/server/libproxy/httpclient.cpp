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
 * httpclient.cpp: Proxy server HTTP client
 *
 * Chris Elving
 */

#include <stdlib.h>
#include <limits.h>
#include "netsite.h"
#include "base/pool.h"
#include "base/util.h"
#include "base/shexp.h"
#include "frame/log.h"
#include "frame/req.h"
#include "frame/conf.h"
#include "frame/http.h"
#include "frame/httpact.h"
#include "frame/httpfilter.h"
#include "httpparser/httpparser.h"
#include "time/nstime.h"
#include "libproxy/url.h"
#include "libproxy/route.h"
#include "libproxy/channel.h"
#include "libproxy/reverse.h"
#include "libproxy/proxyerror.h"
#include "libproxy/dbtlibproxy.h"
#include "libproxy/httpclient.h"
#include "libproxy/util.h"
#include "httpdaemon/HttpMethodRegistry.h"


/*
 * SAF names
 */
#define HTTP_CLIENT_CONFIG ("http-client-config")
#define SERVICE_HTTP ("service-http")

#define CRLF "\x0d\x0a"
#define PROXY_AGENT PRODUCT_HEADER_ID"/"PRODUCT_VERSION_ID
#define PROXY_AGENT_PREFIX "Proxy-agent: "
#define PROXY_AGENT_PREFIX_LEN (sizeof(PROXY_AGENT_PREFIX) - 1)
#define CONTENT_LENGTH_PREFIX "Content-length: "
#define CONTENT_LENGTH_PREFIX_LEN (sizeof(CONTENT_LENGTH_PREFIX) - 1)
#define HOST_PREFIX "Host: "
#define HOST_PREFIX_LEN (sizeof(HOST_PREFIX) - 1)
#define CACHE_INFO_PREFIX "Cache-info: "
#define CACHE_INFO_PREFIX_LEN (sizeof(CACHE_INFO_PREFIX) -1)
#define VIA_PREFIX "Via: "
#define VIA_PREFIX_LEN (sizeof(VIA_PREFIX) - 1)
#define CONNECTION_CLOSE_LINE "Connection: close"CRLF
#define CONNECTION_CLOSE_LINE_LEN (sizeof(CONNECTION_CLOSE_LINE) - 1)
#define CONNECTION_KEEP_ALIVE_LINE "Connection: keep-alive"CRLF
#define CONNECTION_KEEP_ALIVE_LINE_LEN (sizeof(CONNECTION_KEEP_ALIVE_LINE) - 1)
#define TRANSFER_ENCODING_CHUNKED_LINE "Transfer-encoding: chunked"CRLF
#define TRANSFER_ENCODING_CHUNKED_LINE_LEN (sizeof(TRANSFER_ENCODING_CHUNKED_LINE) - 1)
#define LAST_CHUNK "0"CRLF""CRLF
#define LAST_CHUNK_LEN (sizeof(LAST_CHUNK) - 1)
#define HTTP_TOKEN_SEPARATORS "()<>@,;:\\\"/[]?={} \t"
#define FIN "FIN"
#define INTR "INTR"
#define TIMEOUT "TIMEOUT"

#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))

static const HttpString _httpstring_connection((char *)"connection");
static const HttpString _httpstring_content_length((char *)"content-length");
static const HttpString _httpstring_close((char *)"close");
static const HttpString _httpstring_keep_alive((char *)"keep-alive");
static const HttpString _httpstring_transfer_encoding((char *)"transfer-encoding");
static const HttpString _httpstring_warning((char *)"warning");
static const HttpString _httpstring_status((char *)"status");

/*
 * Guess for the maximum segment size for TCP (i.e. the Ethernet MTU - 40).
 * Used as a heuristic to decide whether to memcpy() request entity body onto
 * the end of the HTTP request header.
 */
#define TCP_MSS_GUESS 1460

/*
 * Number of elements in the iovec array we use to parse the full-headers
 * request headers
 */
#define FULL_HEADERS_IOMAX 32

/*
 * Maximum number of headers matching Connection: header tokens
 */
#define MAX_CONNECTION_TOKENS 32

/*
 * Header fields that can be configured in HttpClientHeaders
 */
enum {
    CLIENT_HEADER_CLIENT_IP = 0,
    CLIENT_HEADER_PROXY_AUTHORIZATION,
    CLIENT_HEADER_PROXY_CIPHER,
    CLIENT_HEADER_PROXY_KEYSIZE,
    CLIENT_HEADER_PROXY_SECRET_KEYSIZE,
    CLIENT_HEADER_PROXY_SSL_ID,
    CLIENT_HEADER_PROXY_ISSUER_DN,
    CLIENT_HEADER_PROXY_USER_DN,
    CLIENT_HEADER_PROXY_AUTH_CERT,
    CLIENT_HEADER_PROXY_JROUTE,
    NUM_CLIENT_HEADERS
};

/*
 * Default HttpClientHeaders header field names.  Must be lowercase as we use
 * these values to access the pblocks.
 */
static const char *_default_header_names[NUM_CLIENT_HEADERS] = {
    "client-ip",
    "proxy-authorization",
    "proxy-cipher",
    "proxy-keysize",
    "proxy-secret-keysize",
    "proxy-ssl-id",
    "proxy-issuer-dn",
    "proxy-user-dn",
    "proxy-auth-cert",
    "proxy-jroute"
};

/*
 * Lengths of the header names in _default_header_names[]
 */
static int _default_header_lens[NUM_CLIENT_HEADERS];

/*
 * pb_key *s for the header names in _default_header_names[]
 */
static const pb_key *_default_header_keys[NUM_CLIENT_HEADERS];

/*
 * sn->client pblock parameter to HttpClientHeaders header index map
 */
static struct {
    const char *name; // sn->client parameter name
    const pb_key *key; // sn->client parameter key
    int h; // HttpClientHeaders header_names[] index
} _sn_client_map[] = {
    { "ip", NULL, CLIENT_HEADER_CLIENT_IP },
    { "cipher", NULL, CLIENT_HEADER_PROXY_CIPHER },
    { "keysize", NULL, CLIENT_HEADER_PROXY_KEYSIZE },
    { "secret-keysize", NULL, CLIENT_HEADER_PROXY_SECRET_KEYSIZE },
    { "ssl-id", NULL, CLIENT_HEADER_PROXY_SSL_ID },
    { "issuer_dn", NULL, CLIENT_HEADER_PROXY_ISSUER_DN },
    { "user_dn", NULL, CLIENT_HEADER_PROXY_USER_DN }    
};

/*
 * HttpClientHeaders records HTTP client header configuration (settings
 * controlled by the forward-ip SAF, etc.)
 */
struct HttpClientHeaders {
    pool_handle_t *pool;
    PRBool set[NUM_CLIENT_HEADERS];
    char *names[NUM_CLIENT_HEADERS];
    int lens[NUM_CLIENT_HEADERS];
    const pb_key *keys[NUM_CLIENT_HEADERS];
    unsigned char chars[256];
};

/*
 * HttpClientConfig records HTTP client configuration (settings controlled
 * by the http-client-config SAF, etc.)
 */
struct HttpClientConfig {
    pool_handle_t *pool;
    PRBool keep_alive_set;
    PRBool keep_alive;
    PRBool keep_alive_timeout_set;
    PRIntervalTime keep_alive_timeout;
    PRBool always_use_keep_alive_set;
    PRBool always_use_keep_alive;
    PRBool reformat_request_headers_set;
    PRBool reformat_request_headers;
    // XXX PRBool reformat_response_headers;
    PRBool tunnel_non_http_response_set;
    PRBool tunnel_non_http_response; // pass non-HTTP responses to client
    PRBool always_allow_chunked_set;
    PRBool always_allow_chunked; // allow chunked request to HTTP/1.0 server
    PRBool cache_info_set;
    PRBool cache_info; // send Cache-info: header field
    PRBool via_set;
    PRBool via; // send Via: header field
    PRBool dest_ip; // send Dest-ip: in response to Pragma: dest-ip
    PRBool timeout_set;
    PRIntervalTime timeout; // maximum time both connections can sit idle
    PRBool connect_timeout_set;
    PRIntervalTime connect_timeout; // maximum DNS+connect() time
    PRBool transmit_timeout_set;
    PRIntervalTime transmit_timeout; // maximum time for send() to block
    PRBool parse_timeout_set;
    PRIntervalTime parse_timeout; // maximum time between header packets
    PRBool poll_interval_set;
    PRIntervalTime poll_interval; // time between poll() wakeups
    PRBool retries_set;
    int retries; // maximum number of times to retry a request
    PRBool protocol_set;
    char *protocol;
    int protocol_len;
    int protv_num;
    PRBool proxy_agent_set;
    char *proxy_agent;
    int proxy_agent_len;
    char *proxy_agent_line;
    int proxy_agent_line_len;
};

/*
 * Default HTTP client headers (may be customized during Init)
 */
static HttpClientHeaders _init_headers;

/*
 * Default HTTP client configuration (may be customized during Init)
 */
static HttpClientConfig _init_config;

/*
 * request_get_data/request_set_data slot that holds an HttpClientHeaders *
 */
static int _headers_request_slot = -1;

/*
 * request_get_data/request_set_data slot that holds an HttpClientConfig *
 */
static int _config_request_slot = -1;

/*
 * Request headers we suppress
 */
static char **_suppressed_request_header_names;

/*
 * Lengths of the header names in _suppressed_request_header_names[]
 */
static int *_suppressed_request_header_lens;

/*
 * pb_key *s for the header names in _suppressed_request_header_names[]
 */
static const pb_key **_suppressed_request_header_keys;

/*
 * Number of elements in _suppressed_request_header_names[]
 */
static int _num_suppressed_request_headers;

/*
 * Array where _suppressed_request_header_chars[c] is non-zero if the c is the
 * first character in the name of a suppressed request header
 */
static unsigned char _suppressed_request_header_chars[256];

/*
 * Response headers we suppress.  Note that Connection and Transfer-encoding
 * are not in this list; because of their special meanings, they are always
 * suppressed.
 */
static HttpString *_suppressed_response_headers;

/*
 * Number of elements in _suppressed_response_headers[]
 */
static int _num_suppressed_response_headers;

/*
 * HttpProcessorClientStatus records information about the connection from the
 * client to us (i.e. sn->csd)
 */
struct HttpProcessorClientStatus {
    PRBool readable;
    PRBool eof;
    PRInt64 request_entity_received;
    PRInt64 response_entity_sent;
};

/*
 * HttpProcessorServerStatusrecords information about the connection from us
 * to the origin server (i.e. channel->fd)
 */
struct HttpProcessorServerStatus {
    PRBool readable;
    PRBool eof;
    int request_header_sent;
    PRInt64 request_entity_sent;
    PRInt64 response_received;
    int response_header_received;
    PRInt64 response_entity_received;
};

/*
 * HttpProcessorMessageState tracks message state
 */
enum HttpProcessorMessageState {
    STATE_HEADER,            // Waiting for header
    STATE_ENTITY,            // Waiting for entity body
    STATE_UNCHUNK_BEGIN,     // Waiting for start of chunked body
    STATE_UNCHUNK_LENGTH,    // Waiting for chunk length
    STATE_UNCHUNK_LF_CHUNK,  // Waiting for LF before chunk
    STATE_UNCHUNK_CHUNK,     // Processing chunk contents
    STATE_UNCHUNK_LF_LENGTH, // Waiting for LF before chunk length
    STATE_UNCHUNK_LF_DONE,   // Waiting for LF before EOR
    STATE_UNCHUNK_TRAILER,   // Waiting for trailer header
    STATE_UNCHUNK_DONE,      // End of chunked message body
    STATE_UNCHUNK_ERROR      // Error unchunking message body
};

/*
 * HttpProcessorRequest contains input parameters to http_processor()
 */ 
struct HttpProcessorRequest {
    PRBool can_retry; // set if request can be retried
    PRBool reusing_persistent; // set when reusing a peristent connection

    const char *protocol; // protocol version string
    int protocol_len; // length of protocol version string
    int protv_num; // protocol version number

    PRBool full_headers; // set if full_headers_ioc is valid
    NSAPIIOVec full_headers_iov[FULL_HEADERS_IOMAX];
    int full_headers_ioc;

    const char *host; // Host: header value
    int host_len; // length of Host: header value

    PRBool absolute_uri; // set if talking to a proxy
    PRBool expect_100_continue; // set if client expects 100 Continue
    PRBool has_body; // set if there's a body (note that body may be 0-length)
    PRInt64 content_length; // -1 = unknown length
};

/*
 * HttpProcessorResponse contains working variables used by http_processor()
 */
struct HttpProcessorResponse {
    HttpProcessorMessageState state; // response header/body state
    int header_length; // size of response header
    PRBool keep_alive; // set if channel can be reused
    PRBool has_body; // set if there's a body (note that body may be 0-length)
    PRInt64 content_length; // -1 = unknown length
    int chunk_length; // length of current chunk
    int chunk_remaining; // byes remaining in current chunk
};

/*
 * HttpProcessorResult is returned by http_processor() to describe the result
 * of an HTTP transaction
 */
enum HttpProcessorResult {
    HTTP_PROCESSOR_KEEP_ALIVE, // success, channel can be kept open
    HTTP_PROCESSOR_CLOSE, // success, but channel cannot be kept open
    HTTP_PROCESSOR_CLIENT_FAILURE, // transaction failed due to client error
    HTTP_PROCESSOR_SERVER_FAILURE, // transaction failed due to server error
    HTTP_PROCESSOR_RETRY // server error, but request can be retried
};

/*
 * Size of the HTTP IO buffer.  Also, the maximum allowed request header size.
 */
static int _buffer_size = 16384;

/*
 * session_get_thread_data/session_set_thread_data slot that holds an
 * HttpResponseParser *
 */
static int _parser_thread_slot = -1;

/*
 * _init_status is PR_SUCCESS if httpclient_init() completed successfully
 */
static PRStatus _init_status = PR_FAILURE;


/* ----------------------- free_client_header_name ------------------------ */

static inline void free_client_header_name(HttpClientHeaders *headers, int h)
{
    // Free the header name unless we inherited it from _init_headers or got
    // it from _default_header_names[]
    char *p = headers->names[h];
    if (p != _init_headers.names[h] && p != _default_header_names[h])
        pool_free(headers->pool, p);
}


/* -------------- httpclient_headers_request_slot_destructor -------------- */

extern "C" void httpclient_headers_request_slot_destructor(void *data)
{
    HttpClientHeaders *headers = (HttpClientHeaders *)data;

    PR_ASSERT(headers != &_init_headers);
    PR_ASSERT(headers != NULL);

    // Free any strings we didn't inherit from _init_headers
    for (int i = 0; i < NUM_CLIENT_HEADERS; i++)
        free_client_header_name(headers, i);

    pool_free(headers->pool, headers);
}


/* -------------- httpclient_config_request_slot_destructor --------------- */

extern "C" void httpclient_config_request_slot_destructor(void *data)
{
    HttpClientConfig *config = (HttpClientConfig *)data;

    PR_ASSERT(config != &_init_config);
    PR_ASSERT(config != NULL);

    // Free any strings we didn't inherit from _init_config
    if (config->protocol != _init_config.protocol)
        pool_free(config->pool, config->protocol);
    if (config->proxy_agent != _init_config.proxy_agent)
        pool_free(config->pool, config->proxy_agent);
    if (config->proxy_agent_line != _init_config.proxy_agent_line)
        pool_free(config->pool, config->proxy_agent_line);

    pool_free(config->pool, config);
}


/* ------------------ httpclient_suppress_request_header ------------------ */

void httpclient_suppress_request_header(const char *hdr)
{
    // Suppressed headers must be configured before we go threaded
    PR_ASSERT(!request_is_server_active());

    int n = _num_suppressed_request_headers + 1;

    _suppressed_request_header_names = (char **)
        PERM_REALLOC(_suppressed_request_header_names,
                     n * sizeof(_suppressed_request_header_names[0]));

    _suppressed_request_header_lens = (int *)
        PERM_REALLOC(_suppressed_request_header_lens,
                     n * sizeof(_suppressed_request_header_lens[0]));

    _suppressed_request_header_keys = (const pb_key **)
        PERM_REALLOC(_suppressed_request_header_keys,
                     n * sizeof(_suppressed_request_header_keys[0]));

    char *name = PERM_STRDUP(hdr);
    util_strlower(name);
    _suppressed_request_header_names[_num_suppressed_request_headers] = name;
    _suppressed_request_header_lens[_num_suppressed_request_headers] = strlen(name);
    _suppressed_request_header_keys[_num_suppressed_request_headers] = pblock_key(name);

    _suppressed_request_header_chars[(unsigned char)tolower(*name)] = 1;
    _suppressed_request_header_chars[(unsigned char)toupper(*name)] = 1;

    _num_suppressed_request_headers = n;
}


/* ----------------- httpclient_suppress_response_header ------------------ */

void httpclient_suppress_response_header(const char *hdr)
{
    // Suppressed headers must be configured before we go threaded
    PR_ASSERT(!request_is_server_active());

    int n = _num_suppressed_response_headers + 1;

    _suppressed_response_headers = (HttpString *)
        PERM_REALLOC(_suppressed_response_headers,
                     n * sizeof(_suppressed_response_headers[0]));

    char *name = PERM_STRDUP(hdr);
    util_strlower(name);
    _suppressed_response_headers[_num_suppressed_response_headers] = HttpString(name);

    _num_suppressed_response_headers = n;
}


/* --------------------------- set_proxy_agent ---------------------------- */

static void set_proxy_agent(HttpClientConfig *config, const char *proxy_agent)
{
    if (config->proxy_agent != _init_config.proxy_agent)
        pool_free(config->pool, config->proxy_agent);

    config->proxy_agent = NULL;
    config->proxy_agent_len = 0;

    if (config->proxy_agent_line != _init_config.proxy_agent_line)
        pool_free(config->pool, config->proxy_agent_line);

    config->proxy_agent_line = NULL;
    config->proxy_agent_line_len = 0;

    if (proxy_agent && *proxy_agent) {
        int len = strlen(proxy_agent);

        config->proxy_agent = (char *)pool_strdup(config->pool, proxy_agent);
        if (config->proxy_agent)
            config->proxy_agent_len = len;

        int line_len = PROXY_AGENT_PREFIX_LEN + len + 2;

        config->proxy_agent_line = (char *)
            pool_malloc(config->pool, line_len + 1);
        if (config->proxy_agent_line) {
            strcpy(config->proxy_agent_line, PROXY_AGENT_PREFIX);
            strcat(config->proxy_agent_line, proxy_agent);
            strcat(config->proxy_agent_line, CRLF);
            config->proxy_agent_line_len = line_len;
        }
    }    
}


/* ------------------------ forward_client_header ------------------------- */

static void forward_client_header(HttpClientHeaders *headers,
                                  int h,
                                  const char *hdr = NULL)
{
    char first_char = '\0';

    // Discard the old header name
    free_client_header_name(headers, h);

    // Check for a user-specified header name, falling back to the default
    if (hdr && *hdr) {
        // Use a copy of the header name the user specified.  We'll need to
        // free it later on with free_config_header_name().
        int len = strlen(hdr);
        char *name = (char *)pool_malloc(headers->pool, len + 1);
        if (name) {
            first_char = *hdr;
            strcpy(name, hdr);
            util_strlower(name);
            headers->names[h] = name;
            headers->lens[h] = len;
            headers->keys[h] = pblock_key(name);
        }
    } else {
        // Use the default header name.  Note that we don't make a copy;
        // free_client_header_name() will know not to attempt to free it.
        first_char = *_default_header_names[h];
        headers->names[h] = (char *)_default_header_names[h];
        headers->lens[h] = _default_header_lens[h];
        headers->keys[h] = _default_header_keys[h];
    }

    // Track which chars appear as the first char of a header name
    headers->chars[(unsigned char)tolower(first_char)] = 1;
    headers->chars[(unsigned char)toupper(first_char)] = 1;
}


/* ------------------------- block_client_header -------------------------- */

static void block_client_header(HttpClientHeaders *headers, int h)
{
    // Discard the old header name
    free_client_header_name(headers, h);

    // Block the header
    headers->names[h] = NULL;
    headers->lens[h] = 0;
    headers->keys[h] = NULL;
}


/* --------------------------- httpclient_init ---------------------------- */

PRStatus httpclient_init(void)
{
    int i;

    PR_ASSERT(_init_status == PR_FAILURE);

    if (_init_status != PR_SUCCESS) {
        // Allocate slots to store HttpClientConfig and HttpClientHeaders
        // per-Request
        _config_request_slot =
            request_alloc_slot(&httpclient_config_request_slot_destructor);
        _headers_request_slot =
            request_alloc_slot(&httpclient_headers_request_slot_destructor);

        // Initialize _default_header_lens[] and _default_header_keys[]
        for (i = 0; i < NUM_CLIENT_HEADERS; i++) {
            PR_ASSERT(_default_header_names[i] != NULL);
            _default_header_lens[i] = strlen(_default_header_names[i]);
            _default_header_keys[i] = pblock_key(_default_header_names[i]);
            PR_ASSERT(_default_header_keys[i] != NULL);
        }

        // Initialize _sn_client_map[]
        for (i = 0; i < NUM_ELEMENTS(_sn_client_map); i++) {
            _sn_client_map[i].key = pblock_key(_sn_client_map[i].name);
            PR_ASSERT(_sn_client_map[i].key != NULL);
        }

        // Initialize _suppressed_request_header_names[], etc.  Common header
        // names should appear first.
        httpclient_suppress_request_header("connection");
        httpclient_suppress_request_header("proxy-connection");
        httpclient_suppress_request_header("proxy-authorization");
        httpclient_suppress_request_header("keep-alive"); // see remove_connection_headers()
        httpclient_suppress_request_header("close"); // see remove_connection_headers()
        httpclient_suppress_request_header("transfer-encoding");
        httpclient_suppress_request_header("te");
        httpclient_suppress_request_header("trailer");
        httpclient_suppress_request_header("upgrade");

        // Initialize _suppressed_response_header.  Note that some additional
        // headers not in this list are suppressed by begin_response().
        httpclient_suppress_response_header("proxy-authenticate");
        httpclient_suppress_response_header("keep-alive");
        httpclient_suppress_response_header("trailer");
        httpclient_suppress_response_header("upgrade");

        // Initialize _init_headers and _init_config
        _init_config.pool = NULL;
        _init_config.keep_alive = PR_TRUE;
        _init_config.keep_alive_timeout = PR_SecondsToInterval(29);
        _init_config.always_use_keep_alive = PR_FALSE;
        _init_config.reformat_request_headers = PR_FALSE;
        // XXX _init_config.reformat_response_headers = PR_TRUE;
        _init_config.tunnel_non_http_response = PR_TRUE;
        _init_config.always_allow_chunked = PR_FALSE;
        _init_config.cache_info = PR_FALSE;
        _init_config.via = PR_TRUE;
        _init_config.dest_ip = PR_FALSE;
        _init_config.timeout = PR_SecondsToInterval(300);
        _init_config.connect_timeout = PR_SecondsToInterval(20);
        _init_config.transmit_timeout = PR_SecondsToInterval(60);
        _init_config.parse_timeout = PR_SecondsToInterval(10);
        _init_config.poll_interval = PR_SecondsToInterval(5);
        _init_config.retries = 3;
        _init_config.protocol = (char *)"HTTP/1.1";
        _init_config.protocol_len = strlen(_init_config.protocol);
        _init_config.protv_num = PROTOCOL_VERSION_HTTP11;
        set_proxy_agent(&_init_config, PROXY_AGENT);

        // Initialize _init_headers
        _init_headers.pool = NULL;
        httpclient_forward_ip(NULL, NULL, NULL);
        httpclient_block_proxy_auth(NULL, NULL, NULL);
        httpclient_forward_cipher(NULL, NULL, NULL);
        httpclient_forward_keysize(NULL, NULL, NULL);
        httpclient_forward_secret_keysize(NULL, NULL, NULL);
        httpclient_forward_ssl_id(NULL, NULL, NULL);
        httpclient_forward_issuer_dn(NULL, NULL, NULL);
        httpclient_forward_user_dn(NULL, NULL, NULL);
        httpclient_forward_auth_cert(NULL, NULL, NULL);
        httpclient_forward_jroute(NULL, NULL, NULL);

        // Allocate a slot to store a HttpResponseParser per-DaemonSession
        _parser_thread_slot = session_alloc_thread_slot(NULL);

        // Make sure our buffer is as big as the frontend's HeaderBufferSize
        int HeaderBufferSize = conf_getinteger("HeaderBufferSize",
                                               _buffer_size);
        if (HeaderBufferSize > _buffer_size)
            _buffer_size = HeaderBufferSize;

        _init_status = PR_SUCCESS;
    }

    return _init_status;
}


/* ----------------------------- get_headers ------------------------------ */

static inline HttpClientHeaders * get_headers(Session *sn, Request *rq)
{
    HttpClientHeaders *headers;

    if (rq) {
        // If there's already a per-Request HttpClientHeaders, use it.  If
        // not, create a new per-Request HttpClientHeaders based on the
        // default.
        headers = (HttpClientHeaders *)request_get_data(rq,
                                                        _headers_request_slot);
        if (!headers) {
            headers = (HttpClientHeaders *)
                pool_malloc(sn->pool, sizeof(HttpClientHeaders));
            if (!headers)
                return NULL;

            // Copy the default headers.  Note that we do a shallow copy even
            // though _init_headers contains a number of char *'s; we don't
            // own the storage pointed to by these char *'s, so we need to be
            // careful not to free it in the slot destructor.
            *headers = _init_headers;
            headers->pool = sn->pool;

            request_set_data(rq, _headers_request_slot, headers);
        }
    } else {
        // No Request active, must be Init time.  We'll update the default
        // headers.
        headers = &_init_headers;
    }

    return headers;
}


/* ------------------------------ get_config ------------------------------ */

static inline HttpClientConfig * get_config(Session *sn, Request *rq)
{
    HttpClientConfig *config;

    if (rq) {
        // If there's already a per-Request HttpClientConfig, use it.  If not,
        // create a new per-Request HttpClientConfig based on the default.
        config = (HttpClientConfig *)request_get_data(rq,
                                                      _config_request_slot);
        if (!config) {
            config = (HttpClientConfig *)pool_malloc(sn->pool,
                                                     sizeof(HttpClientConfig));
            if (!config)
                return NULL;

            // Copy the default config.  Note that we do a shallow copy even
            // though _init_config contains a number of char *'s; we don't own
            // the storage pointed to by these char *'s, so we need to be
            // careful not to free it in the slot destructor.
            *config = _init_config;
            config->pool = sn->pool;

            request_set_data(rq, _config_request_slot, config);
        }
    } else {
        // No Request active, must be Init time.  We'll update the default
        // config.
        config = &_init_config;
    }

    return config;
}


/* ------------------------ httpclient_set_dest_ip ------------------------ */

void httpclient_set_dest_ip(Session *sn, Request *rq, PRBool b)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (config)
        config->dest_ip = b;
}


/* -------------------- httpclient_set_connect_timeout -------------------- */

void httpclient_set_connect_timeout(Session *sn,
                                    Request *rq,
                                    PRIntervalTime connect_timeout)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (config)
        config->connect_timeout = connect_timeout;
}


/* ------------------------ httpclient_set_timeout ------------------------ */

void httpclient_set_timeout(Session *sn, Request *rq, PRIntervalTime timeout)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (config)
        config->timeout = timeout;
}


/* ------------------------ httpclient_set_retries ------------------------ */

void httpclient_set_retries(Session *sn, Request *rq, int retries)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (config)
        config->retries = retries;
}


/* --------------------- httpclient_set_jroute_header --------------------- */

void httpclient_set_jroute_header(Session *sn, Request *rq, const char *hdr)
{
    HttpClientHeaders *headers = get_headers(sn, rq);
    if (headers) {
        if (hdr && *hdr) {
            forward_client_header(headers, CLIENT_HEADER_PROXY_JROUTE, hdr);
        } else {
            block_client_header(headers, CLIENT_HEADER_PROXY_JROUTE);
        }
    }
}


/* ----------------- httpclient_reformat_request_headers ------------------ */

void httpclient_reformat_request_headers(Session *sn, Request *rq)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (config)
        config->reformat_request_headers = PR_TRUE;
}


/* -------------------------- get_config_boolean -------------------------- */

static inline int get_config_boolean(pblock *pb,
                                     Session *sn,
                                     Request *rq,
                                     const pb_key *key,
                                     PRBool *set,
                                     PRBool *b)
{
    if (*set)
        return REQ_NOACTION;

    pb_param *pp = pblock_findkey(key, pb);
    if (!pp)
        return REQ_NOACTION;

    int t = util_getboolean(pp->value, -1);
    if (t == -1) {
        log_error(LOG_MISCONFIG, HTTP_CLIENT_CONFIG, sn, rq,
                  XP_GetAdminStr(DBT_invalid_X_value_Y_expected_boolean),
                  pp->name, pp->value);
        return REQ_ABORTED;
    }

    *set = PR_TRUE;
    *b = t;

    return REQ_PROCEED;
}


/* ------------------------- get_config_interval -------------------------- */

static inline int get_config_interval(pblock *pb,
                                      Session *sn,
                                      Request *rq,
                                      const pb_key *key,
                                      PRBool *set,
                                      PRIntervalTime *i)
{
    if (*set)
        return REQ_NOACTION;

    pb_param *pp = pblock_findkey(key, pb);
    if (!pp)
        return REQ_NOACTION;

    PRIntervalTime t = util_getinterval(pp->value, PR_INTERVAL_NO_WAIT);
    if (t == PR_INTERVAL_NO_WAIT) {
        log_error(LOG_MISCONFIG, HTTP_CLIENT_CONFIG, sn, rq,
                  XP_GetAdminStr(DBT_invalid_X_value_Y_expected_seconds),
                  pp->name, pp->value);
        return REQ_ABORTED;
    }

    *set = PR_TRUE;
    *i = t;

    return REQ_PROCEED;
}


/* -------------------- httpclient_http_client_config --------------------- */

int httpclient_http_client_config(pblock *pb, Session *sn, Request *rq)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (!config)
        return REQ_ABORTED;

    if (get_config_boolean(pb, sn, rq,
                           pb_key_keep_alive,
                           &config->keep_alive_set,
                           &config->keep_alive) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_interval(pb, sn, rq,
                            pb_key_keep_alive_timeout,
                            &config->keep_alive_timeout_set,
                            &config->keep_alive_timeout) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_boolean(pb, sn, rq,
                           pb_key_always_use_keep_alive,
                           &config->always_use_keep_alive_set,
                           &config->always_use_keep_alive) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_boolean(pb, sn, rq,
                           pb_key_reformat_request_headers,
                           &config->reformat_request_headers_set,
                           &config->reformat_request_headers) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_boolean(pb, sn, rq,
                           pb_key_tunnel_non_http_response,
                           &config->tunnel_non_http_response_set,
                           &config->tunnel_non_http_response) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_boolean(pb, sn, rq,
                           pb_key_always_allow_chunked,
                           &config->always_allow_chunked_set,
                           &config->always_allow_chunked) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_interval(pb, sn, rq,
                            pb_key_timeout,
                            &config->timeout_set,
                            &config->timeout) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_interval(pb, sn, rq,
                            pb_key_connect_timeout,
                            &config->connect_timeout_set,
                            &config->connect_timeout) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_interval(pb, sn, rq,
                            pb_key_transmit_timeout,
                            &config->transmit_timeout_set,
                            &config->transmit_timeout) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_interval(pb, sn, rq,
                            pb_key_parse_timeout,
                            &config->parse_timeout_set,
                            &config->parse_timeout) == REQ_ABORTED)
        return REQ_ABORTED;

    if (get_config_interval(pb, sn, rq,
                            pb_key_poll_interval,
                            &config->poll_interval_set,
                            &config->poll_interval) == REQ_ABORTED)
        return REQ_ABORTED;

    if (!config->retries_set) {
        if (pb_param *pp = pblock_findkey(pb_key_retries, pb)) {
            int t = strtol(pp->value, NULL, 0);
            if (t < 0 || t > 100) {
                log_error(LOG_MISCONFIG, HTTP_CLIENT_CONFIG, sn, rq,
                          XP_GetAdminStr(DBT_invalid_X_value_Y),
                          pp->name, pp->value);
                return REQ_ABORTED;
            } else {
                config->retries_set = PR_TRUE;
                config->retries = t;
            }
        }
    }

    if (!config->protocol_set) {
        if (pb_param *pp = pblock_findkey(pb_key_protocol, pb)) {
            config->protocol_set = PR_TRUE;
            config->protocol = NULL;
            config->protocol_len = 0;
            config->protv_num = PROTOCOL_VERSION_HTTP09;

            if (*pp->value) {
                int size = sizeof("HTTP/") + strlen(pp->value);

                config->protocol = (char *)pool_malloc(config->pool, size);
                if (!config->protocol)
                    return REQ_ABORTED;

                if (!util_format_http_version(pp->value,
                                              &config->protv_num,
                                              config->protocol,
                                              size))
                {
                    log_error(LOG_MISCONFIG, HTTP_CLIENT_CONFIG, sn, rq,
                              XP_GetAdminStr(DBT_invalid_X_value_Y),
                              pp->name, pp->value);
                    return REQ_ABORTED;
                }

                config->protocol_len = strlen(config->protocol);
            }
        }
    }

    if (!config->proxy_agent_set) {
        if (pb_param *pp = pblock_findkey(pb_key_proxy_agent, pb)) {
            config->proxy_agent_set = PR_TRUE;
            set_proxy_agent(config, pp->value);
        }
    }

    return REQ_NOACTION;
}


/* ------------------------------- forward -------------------------------- */

static int forward(pblock *pb, Session *sn, Request *rq, int h)
{
    HttpClientHeaders *headers = get_headers(sn, rq);
    if (!headers)
        return REQ_ABORTED;

    // If "hdr" is set in pb, use that as the header name
    const char *hdr;
    if (pb) {
        hdr = pblock_findkeyval(pb_key_hdr, pb);
    } else {
        hdr = NULL;
    }

    // Indicate that the header h should be forwarded
    forward_client_header(headers, h, hdr);

    return REQ_NOACTION;
}


/* ------------------------ httpclient_forward_ip ------------------------- */

int httpclient_forward_ip(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_CLIENT_IP);
}


/* -------------------- httpclient_forward_proxy_auth --------------------- */

int httpclient_forward_proxy_auth(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_AUTHORIZATION);
}


/* ---------------------- httpclient_forward_cipher ----------------------- */

int httpclient_forward_cipher(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_CIPHER);
}


/* ---------------------- httpclient_forward_keysize ---------------------- */

int httpclient_forward_keysize(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_KEYSIZE);
}


/* ------------------ httpclient_forward_secret_keysize ------------------- */

int httpclient_forward_secret_keysize(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_SECRET_KEYSIZE);
}


/* ---------------------- httpclient_forward_ssl_id ----------------------- */

int httpclient_forward_ssl_id(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_SSL_ID);
}


/* --------------------- httpclient_forward_issuer_dn --------------------- */

int httpclient_forward_issuer_dn(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_ISSUER_DN);
}


/* ---------------------- httpclient_forward_user_dn ---------------------- */

int httpclient_forward_user_dn(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_USER_DN);
}


/* --------------------- httpclient_forward_auth_cert --------------------- */

int httpclient_forward_auth_cert(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_AUTH_CERT);
}


/* ---------------------- httpclient_forward_jroute ----------------------- */

int httpclient_forward_jroute(pblock *pb, Session *sn, Request *rq)
{
    return forward(pb, sn, rq, CLIENT_HEADER_PROXY_JROUTE);
}


/* -------------------- httpclient_forward_cache_info --------------------- */

int httpclient_forward_cache_info(pblock *pb, Session *sn, Request *rq)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (!config)
        return REQ_ABORTED;
    if (!config->cache_info_set) {
        config->cache_info_set = PR_TRUE;
        config->cache_info = PR_TRUE;
    }
    return REQ_NOACTION;
}


/* ------------------------ httpclient_forward_via ------------------------ */

int httpclient_forward_via(pblock *pb, Session *sn, Request *rq)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (!config)
        return REQ_ABORTED;
    if (!config->via_set) {
        config->via_set = PR_TRUE;
        config->via = PR_TRUE;
    }
    return REQ_NOACTION;
}


/* -------------------- httpclient_forward_proxy_agent -------------------- */

int httpclient_forward_proxy_agent(pblock *pb, Session *sn, Request *rq)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (!config)
        return REQ_ABORTED;
    if (!config->proxy_agent_set) {
        config->proxy_agent_set = PR_TRUE;
        if (!config->proxy_agent_len)
            set_proxy_agent(config, PROXY_AGENT);
    }
    return REQ_NOACTION;
}


/* -------------------------------- block --------------------------------- */

static int block(Session *sn, Request *rq, int h)
{
    HttpClientHeaders *headers = get_headers(sn, rq);
    if (!headers)
        return REQ_ABORTED;

    // Block the header
    block_client_header(headers, h);

    return REQ_NOACTION;
}


/* ------------------------- httpclient_block_ip -------------------------- */

int httpclient_block_ip(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_CLIENT_IP);
}


/* --------------------- httpclient_block_proxy_auth ---------------------- */

int httpclient_block_proxy_auth(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_AUTHORIZATION);
}


/* ----------------------- httpclient_block_cipher ------------------------ */

int httpclient_block_cipher(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_CIPHER);
}


/* ----------------------- httpclient_block_keysize ----------------------- */

int httpclient_block_keysize(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_KEYSIZE);
}


/* ------------------- httpclient_block_secret_keysize -------------------- */

int httpclient_block_secret_keysize(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_SECRET_KEYSIZE);
}


/* ----------------------- httpclient_block_ssl_id ------------------------ */

int httpclient_block_ssl_id(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_SSL_ID);
}


/* ---------------------- httpclient_block_issuer_dn ---------------------- */

int httpclient_block_issuer_dn(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_ISSUER_DN);
}


/* ----------------------- httpclient_block_user_dn ----------------------- */

int httpclient_block_user_dn(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_USER_DN);
}


/* ---------------------- httpclient_block_auth_cert ---------------------- */

int httpclient_block_auth_cert(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_AUTH_CERT);
}


/* ----------------------- httpclient_block_jroute ------------------------ */

int httpclient_block_jroute(pblock *pb, Session *sn, Request *rq)
{
    return block(sn, rq, CLIENT_HEADER_PROXY_JROUTE);
}


/* --------------------- httpclient_block_cache_info ---------------------- */

int httpclient_block_cache_info(pblock *pb, Session *sn, Request *rq)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (!config)
        return REQ_ABORTED;
    if (!config->cache_info_set) {
        config->cache_info_set = PR_TRUE;
        config->cache_info = PR_FALSE;
    }
    return REQ_NOACTION;
}


/* ------------------------- httpclient_block_via ------------------------- */

int httpclient_block_via(pblock *pb, Session *sn, Request *rq)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (!config)
        return REQ_ABORTED;
    if (!config->via_set) {
        config->via_set = PR_TRUE;
        config->via = PR_FALSE;
    }
    return REQ_NOACTION;
}


/* --------------------- httpclient_block_proxy_agent --------------------- */

int httpclient_block_proxy_agent(pblock *pb, Session *sn, Request *rq)
{
    HttpClientConfig *config = get_config(sn, rq);
    if (!config)
        return REQ_ABORTED;
    if (!config->proxy_agent_set) {
        config->proxy_agent_set = PR_TRUE;
        if (config->proxy_agent_len)
            set_proxy_agent(config, NULL);
    }
    return REQ_NOACTION;
}


/* ----------------------- format_request_startline ----------------------- */

static inline int format_request_startline(Session *sn,
                                           Request *rq,
                                           HttpProcessorRequest *request,
                                           char *buffer,
                                           int size)
{
    int pos = 0;

    const char *method = pblock_findkeyval(pb_key_method, rq->reqpb);
    if (!method)
        return -1;

    char *dynamic_escaped = NULL;
    char *escaped = pblock_findkeyval(pb_key_escaped, rq->vars);
    if (!escaped) {
        escaped = pblock_findkeyval(pb_key_path, rq->vars);
        if (!escaped)
            return -1;
        dynamic_escaped = util_url_escape(NULL, escaped);
        if (!dynamic_escaped)
            return -1;
        escaped = dynamic_escaped;
    }

    const char *uri = escaped;
    const char *uri_prefix = NULL;
    if (request->absolute_uri) {
        // We'll be talking to a proxy
        if (*escaped == '/' || *escaped == '*')
        {
            // Need to prepend scheme://hostname
            uri_prefix = http_uri2url_dynamic("", "", sn, rq);
        }
        else if (!strncasecmp(uri, "connect://", 10)) {
             // We'll be  talking to a proxy and we are about to forward
             // a connect request. strip off the preceding "connect://"
             // so that we build a line of the form "CONNECT host:port ..."
             // instead of "CONNECT connect://host:port ..."
             uri += 10;
       }
    } else {
        // We'll be talking to an origin server
        if (isalpha(*escaped)) {
            // Need to remove scheme://hostname
            ParsedUrl url;
            if (url_parse(escaped, &url) == PR_SUCCESS) {
                if (url.path) {
                    uri = url.path;
                } else {
                    uri = "/";
                }
            }
        }
    }

    const char *query = pblock_findkeyval(pb_key_query, rq->reqpb);

    // Do we have enough space?
    int method_len = strlen(method);
    int uri_prefix_len = uri_prefix ? strlen(uri_prefix) : 0;
    int uri_len = strlen(uri);
    int query_len = query ? strlen(query) : 0;
    int startline_len = method_len + 1 + uri_prefix_len + uri_len +
                        query_len + 1 + request->protocol_len + 2;
    if (startline_len > size) {
        if (dynamic_escaped)
            pool_free(sn->pool, dynamic_escaped);
        return -1;
    }

    // Method
    memcpy(buffer + pos, method, method_len);
    pos += method_len;

    buffer[pos++] = ' ';

    // URI
    if (uri_prefix_len) {
        memcpy(buffer + pos, uri_prefix, uri_prefix_len);
        pos += uri_prefix_len;
    }
    memcpy(buffer + pos, uri, uri_len);
    pos += uri_len;

    // Query
    if (query) {
        buffer[pos++] = '?';
        memcpy(buffer + pos, query, query_len);
        pos += query_len;
    }

    // Protocol
    if (request->protocol_len) {
        buffer[pos++] = ' ';
        memcpy(buffer + pos, request->protocol, request->protocol_len);
        pos += request->protocol_len;
    }

    buffer[pos++] = CR;
    buffer[pos++] = LF;

    if (dynamic_escaped)
        pool_free(sn->pool, dynamic_escaped);

    return pos;
}


/* ---------------------------- format_header ----------------------------- */

static inline int format_header(const char *name,
                                const char *value,
                                char *buffer,
                                int size)
{
    char *end = buffer + size;
    char *out = buffer;
    const char *in;

    // Make sure first 22 bytes of name fit with room left for ": ", 32 bytes
    // of value, and "\r\n"
    if (out + 22 + 2 + 32 + 2 > end)
        return -1;

    // Unrolled for 22 bytes
    in = name;
    if (!(*out = toupper(*in))) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;
    if (!(*out = *in)) goto done_name; out++; in++;

    // Copy rest of name
    while (*in) {
        *out++ = *in++;
        if (out + 2 + 32 + 2 > end)
            return -1;
    }

done_name:
    *out++ = ':';
    *out++ = ' ';

    // Make sure first 32 bytes of value fit with room left for "\r\n"
    if (out + 32 + 2 > end)
        return -1;

    // Unrolled for 32 bytes
    in = value;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;
    if (!(*out = *in)) goto done_value; out++; in++;

    // Copy rest of value
    while (*in) {
        *out++ = *in++;
        if (out + 2 > end)
            return -1;
    }

done_value:
    *out++ = CR;
    *out++ = LF;

    return (out - buffer);
}


/* -------------------------- format_rq_headers --------------------------- */

static inline int format_rq_headers(pblock *pb, char *buffer, int size)
{
    int pos = 0;
    
    for (int x = 0; x < pb->hsize; x++) {
        pb_entry *p = pb->ht[x];
        while (p) {
            int rv = format_header(p->param->name,
                                   p->param->value,
                                   buffer + pos,
                                   size - pos);
            if (rv == -1)
                return -1;
            pos += rv;
            p = p->next;
        }
    }

    return pos;
}


/* ------------------------- format_unsafe_header ------------------------- */

static int format_unsafe_header(pool_handle_t *pool,
                                const char *name,
                                const char *value,
                                char *buffer,
                                int size)
{
    char *stripped = pool_strdup(pool, value);
    if (!stripped)
        return -1;

    // Remove embedded CRs/LFs
    char *in = stripped;
    char *out = stripped;
    while (*in) {
        if (*in != CR && *in != LF) {
            *out = *in;
            out++;
        }
        in++;
    }
    *out = '\0';

    int rv = format_header(name, stripped, buffer, size);

    pool_free(pool, stripped);

    return rv;
}


/* ---------------------------- format_request ---------------------------- */

static inline int format_request(Session *sn,
                                 Request *rq,
                                 HttpClientHeaders *headers,
                                 HttpClientConfig *config,
                                 HttpProcessorRequest *request,
                                 char *buffer,
                                 int size)
{
    int pos = 0;
    int rv;

    // Save room for trailing CRLF
    size -= 2;

    // First is the startline
    rv = format_request_startline(sn, rq, request, buffer + pos, size - pos);
    if (rv == -1)
        return -1;
    pos += rv;

    // Proxy-agent:
    if (config->proxy_agent_line_len) {
        if (config->proxy_agent_line_len > size - pos)
            return -1;
        memcpy(buffer + pos,
               config->proxy_agent_line,
               config->proxy_agent_line_len);
        pos += config->proxy_agent_line_len;
    }

    // Host: when sending an HTTP/1.1 request on behalf of an HTTP/1.0 client
    if (request->host_len) {
        if (HOST_PREFIX_LEN + request->host_len + 2 > size - pos)
            return -1;
        memcpy(buffer + pos, HOST_PREFIX, HOST_PREFIX_LEN);
        pos += HOST_PREFIX_LEN;
        memcpy(buffer + pos, request->host, request->host_len);
        pos += request->host_len;
        buffer[pos++] = CR;
        buffer[pos++] = LF;
    }

    // Include the original header fields
    if (request->full_headers) {
        // XXX It'd be nice to use use writev() to send the request.
        // Unfortunately, writev() sucks on Solaris 9; for a typical request,
        // it ends up being cheaper to memcpy() and write().
        for (int i = 0; i < request->full_headers_ioc; i++) {
            if (request->full_headers_iov[i].iov_len > size - pos)
                return -1;
            memcpy(buffer + pos,
                   request->full_headers_iov[i].iov_base,
                   request->full_headers_iov[i].iov_len);
            pos += request->full_headers_iov[i].iov_len;
        }
    } else {
        // Format rq->headers into buffer
        rv = format_rq_headers(rq->headers, buffer + pos, size - pos);
        if (rv == -1)
            return -1;
        pos += rv;
    }

    // Proxy-authorization:
    if (headers->lens[CLIENT_HEADER_PROXY_AUTHORIZATION]) {
        const char *v =
            pblock_findkeyval(pb_key_proxy_authorization, rq->vars);
        if (v) {
            const char *n = headers->names[CLIENT_HEADER_PROXY_AUTHORIZATION];
            rv = format_header(n, v, buffer + pos, size - pos);
            if (rv == -1)
                return -1;
            pos += rv;
        }
    }

    // Format sn->client pblock parameters as HttpClientConfig headers (i.e.
    // add Client-ip:, Proxy-cipher, Proxy-user-dn, etc. header fields)
    for (int i = 0; i < NUM_ELEMENTS(_sn_client_map); i++) {
        int h = _sn_client_map[i].h;
        if (headers->lens[h]) {
            const char *v = pblock_findkeyval(_sn_client_map[i].key,
                                              sn->client);
            if (v) {
                const char *n = headers->names[h];
                rv = format_header(n, v, buffer + pos, size - pos);
                if (rv == -1)
                    return -1;
                pos += rv;
            }
        }
    }

    // Proxy-auth-cert:
    if (headers->lens[CLIENT_HEADER_PROXY_AUTH_CERT]) {
        if (const char *v = pblock_findkeyval(pb_key_auth_cert, rq->vars)) {
            const char *n = headers->names[CLIENT_HEADER_PROXY_AUTH_CERT];
            rv = format_unsafe_header(sn->pool,
                                      n,
                                      v,
                                      buffer + pos,
                                      size - pos);
            if (rv == -1)
                return -1;
            pos += rv;
        }
    }

    // Proxy-jroute:
    if (headers->lens[CLIENT_HEADER_PROXY_JROUTE]) {
        if (char *suffix = pblock_findkeyval(pb_key_jroute, rq->vars)) {
            char *prefix = pblock_findkeyval(pb_key_upstream_jroute, rq->vars);
            const char *n = headers->names[CLIENT_HEADER_PROXY_JROUTE];
            if (prefix) {
                // Proxy-jroute: value is the downstream server's jroute
                // appended to the list of upstream servers' jroutes
                int prefix_len = strlen(prefix);
                int suffix_len = strlen(suffix);
                char *v = (char *)pool_malloc(sn->pool, prefix_len + 1 +
                                              suffix_len + 1);
                if (v) {
                    strcpy(v, prefix);
                    v[prefix_len] = '.';
                    strcpy(v + prefix_len + 1, suffix);
                    rv = format_header(n, v, buffer + pos, size - pos);
                    pool_free(sn->pool, v);
                    if (rv == -1)
                        return -1;
                    pos += rv;
                }
            } else {
                // Proxy-jroute: value is the downstream server's jroute
                rv = format_header(n, suffix, buffer + pos, size - pos);
                if (rv == -1)
                    return -1;
                pos += rv;
            }
        }
    }

    // Cache-info:
    if (config->cache_info) {
        char *report_buf = pblock_findval("report-buf", rq->vars);
        char *rep_http   = pblock_findval("log-report", rq->vars);
        if (report_buf) {
            rv = PR_snprintf(buffer + pos, 
                             size - pos, 
                             CACHE_INFO_PREFIX"%s%s"CRLF,
                             server_hostname,
                             report_buf);
            if (rv == -1 || rv >= size - pos - 1)
                return -1;
            pos += rv;
 
        } else if (rep_http) {
          if (!strcmp(rep_http, "hits") ) {
                rv = PR_snprintf(buffer + pos, 
                                 size - pos, 
                                 CACHE_INFO_PREFIX"%s; get-count=0"CRLF,
                                 server_hostname);
                if (rv == -1 || rv >= size - pos - 1)
                    return -1;
                pos += rv;
          }
        }
    }

    // Via:
    if (config->via) {
        // XXX append to existing Via: header field instead
        // XXX detect and log loops by inspecting Via:?
        rv = PR_snprintf(buffer + pos,
                         size - pos,
                         VIA_PREFIX"%d.%d %s"CRLF,
                         rq->protv_num / 100,
                         rq->protv_num % 100,
                         server_id);
        if (rv == -1 || rv >= size - pos - 1)
            return -1;
        pos += rv;
    }

    // Transfer-encoding: chunked
    if (request->content_length == -1) {
        if (TRANSFER_ENCODING_CHUNKED_LINE_LEN > size - pos)
            return -1;
        memcpy(buffer + pos,
               TRANSFER_ENCODING_CHUNKED_LINE,
               TRANSFER_ENCODING_CHUNKED_LINE_LEN);
        pos += TRANSFER_ENCODING_CHUNKED_LINE_LEN;
    }
            
    // Connection:
    if (config->keep_alive) {
        if (CONNECTION_KEEP_ALIVE_LINE_LEN > size - pos)
            return -1;
        memcpy(buffer + pos,
               CONNECTION_KEEP_ALIVE_LINE,
               CONNECTION_KEEP_ALIVE_LINE_LEN);
        pos += CONNECTION_KEEP_ALIVE_LINE_LEN;
    } else {
        if (CONNECTION_CLOSE_LINE_LEN > size - pos)
            return -1;
        memcpy(buffer + pos,
               CONNECTION_CLOSE_LINE,
               CONNECTION_CLOSE_LINE_LEN);
        pos += CONNECTION_CLOSE_LINE_LEN;
    }

    // End of HTTP header (we saved room for this earlier)
    buffer[pos++] = CR;
    buffer[pos++] = LF;

    return pos;
}


/* ---------------------- remove_connection_headers ----------------------- */

static int remove_connection_headers(pblock *headers,
                                     pb_param **tokens = NULL,
                                     int max_tokens = 0)
{
    int num_tokens = 0;
    pb_param *ppc;

    // Check for Connection: headers
    while (ppc = pblock_removekey(pb_key_connection, headers)) {
        // Parse tokens from the Connection: header
        // XXX this tokenization does not correctly handle quoted spans
        char *prev;
        char *token = util_strtok(ppc->value, HTTP_TOKEN_SEPARATORS, &prev);
        while (token) {
            // We need to remove headers that match this Connection: token
            if (!strcasecmp(token, "keep-alive")) {
                // Don't add keep-alive to tokens[] as it should appear in
                // _suppressed_request_header_names[] anyway
                while (pb_param *ppt = pblock_removekey(pb_key_keep_alive, headers))
                    param_free(ppt);

            } else if (!strcasecmp(token, "close")) {
                // Don't add close to tokens[] as it should appear in
                // _suppressed_request_header_names[] anyway
                while (pb_param *ppt = pblock_removekey(pb_key_close, headers))
                    param_free(ppt);

            } else {
                // Some weird Connection: token.  Remove it from headers and
                // tell our caller about it so he can remove it from
                // full-headers.
                util_strlower(token);
                while (pb_param *ppt = pblock_remove(token, headers)) {
                    if (num_tokens < max_tokens) {
                        tokens[num_tokens] = ppt;
                        num_tokens++;
                    } else {
                        param_free(ppt);
                    }
                }
            }

            token = util_strtok(NULL, HTTP_TOKEN_SEPARATORS, &prev);
        }

        param_free(ppc);
    }

    return num_tokens;
}


/* ------------------------------ find_match ------------------------------ */

static inline const char * find_match(const char *h,
                                      const char * const *names,
                                      const int * lens,
                                      int n)
{
    // Check if h points to the beginning of a header named in names[].  (When
    // n is small and the contents of names[] is dynamic, iterative
    // strcasecmp'ing is cheaper than using a pblock hash table, etc.)
    for (int i = 0; i < n; i++) {
        if (!strncasecmp(h, names[i], lens[i])) {
            if (h[lens[i]] == ':' || isspace(h[lens[i]])) {
                // Found a match.  (We expect a ':' as the terminator, but we
                // look for spaces, too, as some HTTP parsers allow space to
                // appear before the ':')
                return names[i];
            }
        }
    }

    return NULL;
}


/* -------------------- suppressed_request_header_name -------------------- */

static inline const char * suppressed_request_header_name(const char *h)
{
    const char *match = NULL;

    if (_suppressed_request_header_chars[(unsigned char)*h]) {
        match = find_match(h,
                           _suppressed_request_header_names,
                           _suppressed_request_header_lens,
                           _num_suppressed_request_headers);
    }

    return match;
}


/* -------------------------- client_header_name -------------------------- */

static inline const char * client_header_name(HttpClientHeaders *headers,
                                              const char *h,
                                              const char * const *names,
                                              const int * lens,
                                              int n)
{
    const char *match = NULL;

    if (headers->chars[(unsigned char)*h])
        match = find_match(h, names, lens, n);

    return match;
}


/* ------------------------ suppress_full_headers ------------------------- */

static int suppress_full_headers(Session *sn,
                                 Request *rq,
                                 HttpClientHeaders *headers,
                                 const char **dynamic_suppressed_header_names,
                                 int *dynamic_suppressed_header_lens,
                                 int num_dynamic_suppressed_headers,
                                 char *full_headers,
                                 NSAPIIOVec *iov,
                                 int iomax)
{
    int ioc = 0;
    int i;

    /*
     * N.B. if you modify this function, make sure to preserve symmetry with
     * suppress_rq_headers()
     *
     * We remove headers from both rq->headers and full-headers.  We assume
     * that any header in full-headers will also be present in rq->headers.
     * (This is not necessarily the case if someone gets under the covers and
     * manipulates rq->headers directly.  Don't do that.)
     */

    // Build a list of header names that were specified in HttpClientHeaders
    // (we suppress these to prevent spoofing)
    char *client_header_names[NUM_CLIENT_HEADERS];
    int client_header_lens[NUM_CLIENT_HEADERS];
    int num_client_headers = 0;
    for (i = 0; i < NUM_CLIENT_HEADERS; i++) {
        if (headers->lens[i]) {
            client_header_names[num_client_headers] = headers->names[i];
            client_header_lens[num_client_headers] = headers->lens[i];
            num_client_headers++;
        }
    }

    // Traverse full-headers, looking for headers that should be suppressed
    // (that is, removed from the iov and rq->headers)
    char *h = full_headers;
    iov[ioc].iov_base = h;
    while (*h) {
        // Check if the header name matches a suppressed header
        const char *match;
        if (match = suppressed_request_header_name(h)) {
            param_free(pblock_remove(match, rq->headers));
        } else if (match = client_header_name(headers,
                                              h,
                                              client_header_names,
                                              client_header_lens,
                                              num_client_headers))
        {
            param_free(pblock_remove(match, rq->headers));
        } else if (num_dynamic_suppressed_headers) {
            match = find_match(h,
                               dynamic_suppressed_header_names,
                               dynamic_suppressed_header_lens,
                               num_dynamic_suppressed_headers);
        }

        // If the header should be suppressed, leave a hole in the iov
        if (match && ioc < iomax) {
            // We want the stuff before the suppressed header
            iov[ioc].iov_len = h - (char *)iov[ioc].iov_base;
            if (iov[ioc].iov_len)
                ioc++;
        }

        // Skip past the header name and value
        while (*h) {
            // XXX we don't properly handle quoted spans with embedded LFs
            if (*h++ == LF) {
                if (!isspace(*h)) // line folding
                    break;
            }
        }

        // If the header was suppressed, we want the stuff after the header
        if (match && ioc < iomax)
            iov[ioc].iov_base = h;
    }
    if (ioc < iomax) {
        // We want the stuff at the end of full-headers
        iov[ioc].iov_len = h - (char *)iov[ioc].iov_base;
        if (iov[ioc].iov_len)
            ioc++;
    }

    PR_ASSERT(ioc <= iomax);

    return ioc;
}


/* ------------------------- suppress_rq_headers -------------------------- */

static void suppress_rq_headers(Session *sn,
                                Request *rq,
                                HttpClientHeaders *headers,
                                const char **dynamic_suppressed_header_names,
                                int num_dynamic_suppressed_headers)
{
    int i;

    /*
     * N.B. if you modify this function, make sure to preserve symmetry with
     * suppress_full_headers()
     */

    // Remove the _suppressed_request_header_names[] headers from rq->headers
    for (i = 0; i < _num_suppressed_request_headers; i++) {
        pb_param *pp;
        if (_suppressed_request_header_keys[i]) {
            pp = pblock_removekey(_suppressed_request_header_keys[i], rq->headers);
        } else {
            pp = pblock_remove(_suppressed_request_header_names[i], rq->headers);
        }
        if (pp)
            param_free(pp);
    }

    // Remove the HttpClientHeaders headers from rq->headers (we suppress
    // these to prevent spoofing)
    for (i = 0; i < NUM_CLIENT_HEADERS; i++) {
        if (headers->lens[i]) {
            pb_param *pp;
            if (headers->keys[i]) {
                pp = pblock_removekey(headers->keys[i], rq->headers);
            } else {
                pp = pblock_remove(headers->names[i], rq->headers);
            }
            if (pp)
                param_free(pp);
        }
    }

    // Remove the dynamic_suppressed_header_names[] headers from rq->headers
    for (i = 0; i < num_dynamic_suppressed_headers; i++) {
        pb_param *pp;
        pp = pblock_remove(dynamic_suppressed_header_names[i], rq->headers);
        if (pp)
            param_free(pp);
    }
}


/* ----------------------------- add_warnings ----------------------------- */

static void add_warnings(HttpStringNode warnings, pblock *headers)
{
    /*
     * RFC 2616 14.46
     *
     * Warning = "Warning" ":" 1#warning-value
     * warning-value = warn-code SP warn-agent SP warn-text [SP warn-date]
     *
     * If an implementation receives a message with a warning-value that
     * includes a warn-date, and that warn-date is different from the Date
     * value in the response, then that warning-value MUST be deleted from
     * the message before storing, forwarding, or using it.
     */

    HttpTokenizer tokenizer(warnings);

    for (;;) {
        // Parse warn-code, warn-agent, and warn-text
        HttpString *token;
        if ((token = tokenizer++) == NULL)
            break;
        HttpString warn_code = *token;
        if ((token = tokenizer++) == NULL)
            break;
        HttpString warn_agent = *token;
        if ((token = tokenizer++) == NULL)
            break;
        HttpString warn_text = *token;

        // Is there a trailing token?
        HttpString warn_date(NULL, 0);
        token = *tokenizer;
        if (token) {
            // Is the trailing token a warn-date or the next warn-code?
            int nondigits = 0;
            for (int i = 0; i < token->len && i < 3; i++)
                nondigits += !isdigit(token->p[i]);
            if (nondigits) {
                // Found warn-date
                warn_date = *token;
                tokenizer++;

                // Compare warn-date with Date: header
                warn_date.p[warn_date.len] = '\0';
                char *date = pblock_findkeyval(pb_key_date, headers);
                if (date && util_str_time_equal(date, warn_date.p) == -1)
                    continue; // dates differ, omit warning
            }
        }

        // Calculate length required for the Warning: header field
        int len = warn_code.len + 2 + warn_agent.len + 3 + warn_text.len + 1;
        if (warn_date.len)
            len += 2 + warn_date.len + 1;

        pb_param *pp = pblock_key_param_create(headers,
                                               pb_key_warning,
                                               NULL,
                                               len);
        if (!pp)
            break;

        PR_ASSERT(pp->value);

        // Format the Warning: header field
        char *p = pp->value;
        memcpy(p, warn_code.p, warn_code.len);
        p += warn_code.len;
        *p++ = ' ';
        *p++ = '"';
        memcpy(p, warn_agent.p, warn_agent.len);
        p += warn_agent.len;
        *p++ = '"';
        *p++ = ' ';
        *p++ = '"';
        memcpy(p, warn_text.p, warn_text.len);
        p += warn_text.len;
        *p++ = '"';
        if (warn_date.len) {
            *p++ = ' ';
            *p++ = '"';
            memcpy(p, warn_date.p, warn_date.len);
            p += warn_date.len;
            *p++ = '"';
        }
        *p = '\0';

        // Add the Warning: header
        pblock_kpinsert(pb_key_warning, pp, headers);
    }
}


/* ---------------------------- begin_response ---------------------------- */

static PRStatus begin_response(Session *sn,
                               Request *rq,
                               HttpClientConfig *config,
                               HttpResponseParser *parser,
                               HttpProcessorRequest *request,
                               HttpProcessorResponse *response,
                               Channel *channel,
                               int *response_header_received,
                               netbuf *inbuf)
{
    int code;
    int rv;

    // We shouldn't be called until there's data ready
    PR_ASSERT(inbuf->cursize > 0);

begin_response_parse:

    PR_ASSERT(response->state == STATE_HEADER);
    PR_ASSERT(response->has_body == PR_TRUE);
    PR_ASSERT(response->content_length == -1);
    PR_ASSERT(inbuf->pos == 0);

    // Try to parse a complete response header
    rv = parser->parse(inbuf); 
    if (rv) {
        // Since we're in begin_response(), we know that the remote server
        // started its response header.  Something went wrong, though.
        if (rv == PROTOCOL_ENTITY_TOO_LARGE) {
            proxyerror_agent_errorf(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                    channel_get_remote_host(channel),
                                    channel_get_remote_port(channel),
                                    PROTOCOL_BAD_GATEWAY, 
                                    XP_GetAdminStr(DBT_res_hdr_max_X),
                                    inbuf->maxsize);
        } else if (rv == PROTOCOL_REQUEST_TIMEOUT) {
            proxyerror_agent_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                   channel_get_remote_host(channel),
                                   channel_get_remote_port(channel),
                                   PROTOCOL_BAD_GATEWAY, 
                                   XP_GetAdminStr(DBT_incomplete_res_hdr));
        } else {
            proxyerror_agent_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                   channel_get_remote_host(channel),
                                   channel_get_remote_port(channel),
                                   PROTOCOL_BAD_GATEWAY, 
                                   XP_GetAdminStr(DBT_invalid_res_hdr));
        }

        if (!config->tunnel_non_http_response) {
            // Abort transaction
            return PR_FAILURE;
        }

        // Assume the remote server is talking something other than HTTP and
        // just pass the data through raw.  Perhaps the client will be able to
        // make sense of it.
        response->state = STATE_ENTITY;
        inbuf->pos = 0;
        rq->senthdrs = PR_TRUE;
        return PR_SUCCESS;
    }

    // Remember size of the HTTP response header
    *response_header_received += inbuf->pos;

    // Get HTTP response code
    code = atoi(parser->getCode());

    // If this was just an interim response...
    if (code >= 100 && code <= 199) {
        // Relay the interim response to the client
        if (rq->protv_num >= PROTOCOL_VERSION_HTTP11) {
            PRFileDesc *fd = sn->csd;
            if (FilterLayer *layer = filter_layer(fd, httpfilter_get_filter()))
                fd = layer->lower;
            net_write(fd, inbuf->inbuf, inbuf->pos);
        }

        // Remove the interim response from the buffer
        int remaining = inbuf->cursize - inbuf->pos;
        memcpy(inbuf->inbuf, inbuf->inbuf + inbuf->pos, remaining);
        inbuf->pos = 0;
        inbuf->cursize = remaining;

        // If there's more data to parse, parse it now
        if (inbuf->cursize)
            goto begin_response_parse;

        // Wait for more data
        return PR_SUCCESS;
    }

    // Remember the remote status code for logging
    pblock_kninsert(pb_key_remote_status, code, rq->vars);

    // We've received the response header
    response->state = STATE_ENTITY;

    // HTTP/1.1 uses persistent connections by default
    int protv_num = parser->getVersion();
    if (protv_num >= PROTOCOL_VERSION_HTTP11)
        response->keep_alive = PR_TRUE;

    // If the client is sending chunked data to a server that won't grok it...
    if (request->content_length == -1 &&
        !config->always_allow_chunked &&
        protv_num < PROTOCOL_VERSION_HTTP11)
    {
        log_error(LOG_FAILURE, SERVICE_HTTP, sn, rq, 
                  XP_GetAdminStr(DBT_chunked_to_pre_http11));

        // Abort transaction
        protocol_status(sn, rq, PROTOCOL_LENGTH_REQUIRED, NULL);
        return PR_FAILURE;
    }

    // Save Warning: header field values for post processing
    HttpHeaderField *warnings = NULL;

    // Inspect each header from the remote server
    int count = parser->getHeaderFieldCount();
    for (int i = 0; i < count; i++) {
        HttpHeaderField *hhf = parser->getHeaderField(i);

        // Check for headers we're interested in
        if (hhf->name == _httpstring_transfer_encoding) {
            // Don't pass Transfer-encoding: on to client
            response->state = STATE_UNCHUNK_BEGIN;
            continue;

        } else if (hhf->name == _httpstring_connection) {
            // We'll remove this Connection: header later on
            HttpTokenizer tokenizer(hhf->value);
            do {
                if (tokenizer.matches(_httpstring_close)) {
                    // Connection: close
                    response->keep_alive = PR_FALSE;
                } else if (tokenizer.matches(_httpstring_keep_alive)) {
                    // Connection: keep-alive
                    response->keep_alive = PR_TRUE;
                }
            } while (++tokenizer);

        } else {
            // Peek at the Content-length:
            if (hhf->name == _httpstring_content_length) {
                if (hhf->value.next && *hhf->value.next != hhf->value) {
                    // Always reject conflicting Content-length: headers to
                    // preempt HTTP response smuggling attacks
                    proxyerror_agent_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                           channel_get_remote_host(channel),
                                           channel_get_remote_port(channel),
                                           PROTOCOL_BAD_GATEWAY,
                                           XP_GetAdminStr(DBT_dup_cl_res_header));
                    return PR_FAILURE;
                }
                response->content_length = util_atoi64(hhf->value.p);
                if (response->content_length < 0) {
                    // Don't pass along bogus Content-length:
                    response->content_length = -1;
                    continue;
                }
            }

            // Don't pass along headers in _suppressed_response_headers[]
            int j;
            for (j = 0; j < _num_suppressed_response_headers; j++) {
                if (hhf->name == _suppressed_response_headers[j])
                    break;
            }
            if (j < _num_suppressed_response_headers)
                continue;

            // Save Warning: header field values for post processing
            if (hhf->name == _httpstring_warning) {
                warnings = hhf;
                continue;
            }

            // Rewrite "status" to "Status" so as not to confuse NSAPI
            if (hhf->name == _httpstring_status)
                hhf->value.p[0] = 'S';
        }

        // Pass this header on to the client.  Note that unlike the frontend
        // parser, we create multiple pb_params when a given header name
        // appears multiple times.  Combining headers would be bad for
        // not-strictly-HTTP-compliant header fields like Set-cookie:.
        HttpStringNode *hsn = &hhf->value;
        while (hsn) {
            // XXX for efficiency, we could tell the HttpResponseParser about
            // well-known headers' pb_keys beforehand
            hhf->name.p[hhf->name.len] = '\0';
            hsn->p[hsn->len] = '\0';
            pblock_nvinsert(hhf->name.p, hsn->p, rq->srvhdrs);
            hsn = hsn->next;
        }
    }

    // Add only up-to-date Warning: headers
    if (warnings)
        add_warnings(warnings->value, rq->srvhdrs);
    
    // Remove Connection: headers from the response
    remove_connection_headers(rq->srvhdrs);

    // If remote server sent both Content-length: and Transfer-encoding:,
    // discard Content-length: (RFC 2616 4.4)
    if (response->state == STATE_UNCHUNK_BEGIN &&
        response->content_length != -1)
    {
        param_free(pblock_removekey(pb_key_content_length, rq->srvhdrs));
        response->content_length = -1;
    }

    // Via:
    if (config->via) {
        // XXX append to existing Via: header field instead
        int via_size = 6 /* "99.99 " */ + strlen(server_id) + 1;
        char *via = (char *)pool_malloc(sn->pool, via_size);
        if (via) {
            int via_len = PR_snprintf(via, via_size,
                                      "%d.%d %s",
                                      protv_num / 100,
                                      protv_num % 100,
                                      server_id);
            pblock_kvinsert(pb_key_via, via, via_len, rq->srvhdrs);
            pool_free(sn->pool, via);
        }

    }

    // Proxy-agent:
    if (config->proxy_agent_len) {
        pblock_kvinsert(pb_key_proxy_agent,
                        config->proxy_agent, config->proxy_agent_len,
                        rq->srvhdrs);
    }

    // Dest-IP:
    if (config->dest_ip) {
        // XXX this is disgusting
        char *pragma;
        PRNetAddr addr;
        if ((pragma = pblock_findkeyval(pb_key_pragma, rq->headers)) &&
            util_strcasestr(pragma, "dest-ip") &&
            !route_get_proxy_addr(sn, rq) &&
            !route_get_origin_addr(sn, rq) &&
            PR_GetPeerName(inbuf->sd, &addr) == PR_SUCCESS)
        {
            // No need to do IPv6 support for this lame legacy hack
            char ip[sizeof("255.255.255.255")];
            if (net_addr_to_string(&addr, ip, sizeof(ip)) != -1)
                pblock_nvinsert("dest-ip", ip, rq->srvhdrs);
        }
    }

    // As per RFC2616 4.3, there's never any content sent over the channel on a
    // HEAD, 1xx, 204, or 304 response
    if (rq->method_num == METHOD_HEAD || code == 204 || code == 304) {
        response->has_body = PR_FALSE;
        response->content_length = 0;
    }

    // Do we have enough information to maintain a persistent connection?
    if (response->content_length == -1 && response->state == STATE_ENTITY)
        response->keep_alive = PR_FALSE;

    // Don't reuse the connection if we see strange status codes
    if (code < 100 || code > 999) {
        proxyerror_agent_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                               channel_get_remote_host(channel),
                               channel_get_remote_port(channel),
                               PROTOCOL_BAD_GATEWAY, 
                               XP_GetAdminStr(DBT_invalid_res_hdr));
        code = PROTOCOL_BAD_GATEWAY;
        response->keep_alive = PR_FALSE;
    } else if (code < 200 || code == 400 || code == 408 || code == 413 || code >= 500) {
        response->keep_alive = PR_FALSE;
    }

    // Handle reverse mapping for URLs in Location:, etc. headers
    // XXX this should really be done as an Output directive
    reverse_map_rewrite(sn, rq);
    
    // Add a route cookie when we see a sticky cookie in the response headers
    // XXX this should really be done as an Output directive
    route_process_cookies(sn, rq);
    
    // Get reason given by remote server (e.g. "OK")
    char *reason;
    HttpString hs = parser->getReason();
    if (hs.len) {
        hs.p[hs.len] = '\0';
        reason = hs.p;
    } else {
        reason = NULL;
    }

    // Start the response
    protocol_status(sn, rq, code, reason);
    protocol_start_response(sn, rq);

    return PR_SUCCESS;
}


/* --------------------------- request_complete --------------------------- */

static PRBool request_complete(HttpProcessorRequest *request,
                               HttpProcessorClientStatus *client)
{
    // No body?
    if (request->content_length == 0)
        return PR_TRUE;

    // Complete request body w/Content-length?
    if (client->request_entity_received == request->content_length)
        return PR_TRUE;

    // Complete chunked request body?
    if (request->content_length == -1 && client->eof)
        return PR_TRUE;

    // Still waiting for body or error reading body
    return PR_FALSE;
}


/* -------------------------- response_complete --------------------------- */

static PRBool response_complete(HttpProcessorResponse *response,
                                HttpProcessorServerStatus *server)
{
    // Still waiting for the header?
    if (response->state == STATE_HEADER)
        return PR_FALSE;

    // No body?
    if (response->content_length == 0)
        return PR_TRUE;

    // Complete response body w/Content-length?
    if (server->response_entity_received == response->content_length)
        return PR_TRUE;

    // Complete chunked response body?
    if (response->state == STATE_UNCHUNK_DONE)
        return PR_TRUE;
           
    // Complete response body w/o Content-length or Transfer-encoding?
    if (response->state == STATE_ENTITY &&
        response->content_length == -1 &&
        server->eof)
    {
        return PR_TRUE;
    }

    // Still waiting for body or error reading body
    return PR_FALSE;
}


/* ----------------------------- client_read ------------------------------ */

static inline int client_read(netbuf *client,
                              char *buffer,
                              int size,
                              PRIntervalTime timeout)
{
    int available = client->cursize - client->pos;
    if (available > size)
        available = size;
    if (available > 0) {
        // Oops, data was already buffered, but not where we want it
        memcpy(buffer, client->inbuf + client->pos, available);
        client->pos += available;
        return available;
    }

    return net_read(client->sd, buffer, size, timeout);
}


/* ---------------------- client_write_error_degree ----------------------- */

static inline int client_write_error_degree(PRErrorCode prerr)
{
    int degree;

    if (prerr == PR_CONNECT_RESET_ERROR) {
        // Peer disconnecting isn't very interesting
        degree = LOG_VERBOSE;
    } else {
        // This looks interesting
        degree = LOG_FAILURE;
    }

    return degree;
}


/* ------------------------- server_error_degree -------------------------- */

static inline int server_error_degree(HttpProcessorRequest *request,
                                      PRErrorCode prerr)
{
    int degree;

    /*
     * We got an error a) attempting to send the request or b) attempting to
     * read the first byte of the response
     */

    if (request->can_retry &&
        request->reusing_persistent &&
        prerr == PR_CONNECT_RESET_ERROR)
    {
        // Probably a stale keep-alive connection (i.e. allowed by HTTP)
        degree = LOG_VERBOSE;
    } else {
        // Actual error
        degree = LOG_FAILURE;
    }

    return degree;
}


/* -------------------------- server_eof_degree --------------------------- */

static inline int server_eof_degree(HttpProcessorRequest *request)
{
    int degree;

    /*
     * We saw EOF attempting to read the first byte of the response
     */

    if (request->can_retry && request->reusing_persistent) {
        // Probably a stale keep-alive connection (i.e. allowed by HTTP)
        degree = LOG_VERBOSE;
    } else {
        // Actual error
        degree = LOG_FAILURE;
    }

    return degree;
}

/* --------------------------- retry_or_failure --------------------------- */

static inline HttpProcessorResult retry_or_failure(
    Session *sn,
    Request *rq,
    HttpProcessorRequest *request,
    HttpProcessorResponse *response)
{
    HttpProcessorResult result;

    PR_ASSERT(response->state == STATE_HEADER);

    if (request->can_retry) {
        PR_ASSERT(request->content_length == 0);
        result = HTTP_PROCESSOR_RETRY;
    } else {
        result = HTTP_PROCESSOR_SERVER_FAILURE;
    }

    return result;
}


/* -------------------------- get_buffered_char --------------------------- */

static inline int get_buffered_char(netbuf *inbuf)
{
    return (inbuf->pos < inbuf->cursize) ? inbuf->inbuf[inbuf->pos++] : -1;
}


/* ------------------------------- unchunk -------------------------------- */

static inline int unchunk(HttpProcessorResponse *response,
                          netbuf *inbuf,
                          NSAPIIOVec *iov)
{
    // If the response message body isn't chunked...
    if (response->state == STATE_ENTITY) {
        int amount = inbuf->cursize - inbuf->pos;
        if (amount > 0) {
            iov->iov_base = (char *)inbuf->inbuf + inbuf->pos;
            iov->iov_len = amount;
            inbuf->pos += amount;
        }
        return amount;
    }

    // Unchunk buffered data from inbuf
    for (;;) {
        int c;

        switch (response->state) {
        case STATE_UNCHUNK_BEGIN:
            // Beginning of a chunked message body
            response->chunk_length = 0;
            response->state = STATE_UNCHUNK_LENGTH;
            break;

        case STATE_UNCHUNK_LENGTH:
            // Consume the chunk length
            for (;;) {
                c = get_buffered_char(inbuf);
                if (c == -1) {
                    goto unchunk_need_input;
                } else if (c >= '0' && c <= '9') {
                    response->chunk_length <<= 4;
                    response->chunk_length += (c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    response->chunk_length <<= 4;
                    response->chunk_length += (c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    response->chunk_length <<= 4;
                    response->chunk_length += (c - 'A' + 10);
                } else {
                    break;
                }
            }
            if (response->chunk_length >= 0) {
                response->chunk_remaining = response->chunk_length;
                if (c == '\n') {
                    response->state = STATE_UNCHUNK_CHUNK;
                } else {
                    response->state = STATE_UNCHUNK_LF_CHUNK;
                }
            } else {
                response->state = STATE_UNCHUNK_ERROR;
            }
            break;

        case STATE_UNCHUNK_LF_CHUNK:
            // Consume any chunk extensions and the LF before the chunk data
            for (;;) {
                c = get_buffered_char(inbuf);
                if (c == -1) {
                    goto unchunk_need_input;
                } else if (c == '\n') {
                    break;
                }
            }
            response->state = STATE_UNCHUNK_CHUNK;
            break;

        case STATE_UNCHUNK_CHUNK:
            // Consume the chunk data
            if (response->chunk_length == 0) {
                response->state = STATE_UNCHUNK_LF_DONE;
            } else if (response->chunk_remaining == 0) {
                response->chunk_length = 0;
                response->state = STATE_UNCHUNK_LF_LENGTH;
            } else {
                int amount = inbuf->cursize - inbuf->pos;
                if (amount == 0)
                    goto unchunk_need_input;
                if (amount > response->chunk_remaining)
                    amount = response->chunk_remaining;
                iov->iov_base = (char *)inbuf->inbuf + inbuf->pos;
                iov->iov_len = amount;
                inbuf->pos += amount;
                response->chunk_remaining -= amount;
                return amount;
            }
            break;

        case STATE_UNCHUNK_LF_LENGTH:
            // Consume the LF before the next chunk's length
            for (;;) {
                c = get_buffered_char(inbuf);
                if (c == -1) {
                    goto unchunk_need_input;
                } else if (c != CR) {
                    break;
                }
            }
            if (c == LF) {
                response->state = STATE_UNCHUNK_LENGTH;
            } else {
                response->state = STATE_UNCHUNK_ERROR;
            }
            break;

        case STATE_UNCHUNK_LF_DONE:
            // Consume any trailer headers and the final LF
            for (;;) {
                c = get_buffered_char(inbuf);
                if (c == -1) {
                    goto unchunk_need_input;
                } else if (c != CR) {
                    break;
                }
            }
            if (c == LF) {
                response->state = STATE_UNCHUNK_DONE;
            } else {
                response->state = STATE_UNCHUNK_TRAILER;
            }
            break;

        case STATE_UNCHUNK_TRAILER:
            // Consume a trailer header
            for (;;) {
                c = get_buffered_char(inbuf);
                if (c == -1) {
                    goto unchunk_need_input;
                } else if (c == LF) {
                    break;
                }
            }
            response->state = STATE_UNCHUNK_LF_DONE;
            break;

        case STATE_UNCHUNK_DONE:
            // End of chunked message body
            return 0;

        case STATE_UNCHUNK_ERROR:
            // Server sent something bogus
            return -1;

        default:
            // Unreachable (I hope)
            PR_ASSERT(0);
            return -1;
        }
    }

unchunk_need_input:
    PR_ASSERT(inbuf->pos == inbuf->cursize);
    return 0;
}


/* ------------------------------- coalesce ------------------------------- */

int coalesce(NSAPIIOVec *iov, int ioc)
{
    // XXX coalesce small chunks
    return ioc;
}


/* ---------------------------- timeout_error ----------------------------- */

static inline PRBool timeout_error(PRErrorCode prerr)
{
    return (prerr == PR_IO_TIMEOUT_ERROR || prerr == PR_WOULD_BLOCK_ERROR);
}


/* ---------------------------- http_processor ---------------------------- */

static HttpProcessorResult http_processor(Session *sn,
                                          Request *rq,
                                          HttpClientHeaders *headers,
                                          HttpClientConfig *config,
                                          HttpResponseParser *parser,
                                          HttpProcessorRequest *request,
                                          char *buffer,
                                          int size,
                                          Channel *channel)
{
    NSAPIIOVec iov[PR_MAX_IOVECTOR_SIZE];
    PRInt32 rv;

    // Under WINNT NSPR, we can't use PR_INTERVAL_NO_WAIT (or small timeouts in
    // general) without risking data loss.  See net_is_timeout_safe().
    PRIntervalTime recv_timeout;
    if (net_is_timeout_safe()) {
        recv_timeout = PR_INTERVAL_NO_WAIT;
    } else {
        recv_timeout = config->timeout;
    }

    // HttpProcessorResponse tracks response status.  It will be updated by
    // begin_response().
    HttpProcessorResponse response;
    response.header_length = 0;
    response.keep_alive = PR_FALSE;
    response.state = STATE_HEADER;
    response.has_body = PR_TRUE;
    response.content_length = -1;

    // HttpProcessorClientStatus tracks client connection status
    HttpProcessorClientStatus client;
    if (request->content_length == 0) {
        // There's no request body to read
        client.readable = PR_FALSE;
    } else if (sn->inbuf->cursize > sn->inbuf->pos ||
               httpfilter_request_body_buffered(sn, rq))
    {
        // There's buffered request body data that can be read without blocking
        // XXX see httpfilter_request_body_buffered()
        client.readable = PR_TRUE;
    } else {
        // We need to poll before recv because we don't know whether the
        // request body or response header will arrive first.  (sn->csd is
        // typically a nonblocking socket, but that's not guaranteed.  We err
        // on the side of caution.)
        client.readable = PR_FALSE;
    }
    client.eof = (request->content_length == 0);
    client.request_entity_received = 0;
    client.response_entity_sent = 0;

    // HttpProcessorServerStatus tracks server connection status
    HttpProcessorServerStatus server;
    if (recv_timeout == PR_INTERVAL_NO_WAIT) {
        // We'll do PR_INTERVAL_NO_WAIT PR_Recv()s, and it's safe to recv
        // without first polling
        server.readable = PR_TRUE; // XXX would poll before recv be faster?
    } else {
        // We're using WINNT NSPR, so timing out during PR_Recv() is terminal.
        // Reads from channel->fd will need to block.
        if (request_complete(request, &client)) {
            // There's no request body so it's okay for us to block waiting for
            // the server to respond
            server.readable = PR_TRUE;
        } else {
            // We need to poll before recv because we don't know whether the
            // request body or response header will arrive first
            server.readable = PR_FALSE;
        }
    }
    server.eof = PR_FALSE;
    server.request_header_sent = 0;
    server.request_entity_sent = 0;
    server.response_received = 0;
    server.response_header_received = 0;
    server.response_entity_received = 0;

    // Remember when we started the transaction
    PRIntervalTime transaction_epoch = ft_timeIntervalNow();
    int first_response = 0;

    /*
     * Build the request header
     */

    int request_header_size = format_request(sn,
                                             rq,
                                             headers,
                                             config,
                                             request,
                                             buffer,
                                             size);
    if (request_header_size == -1) {
        // Startline and headers wouldn't fit in buffer.  Blame the client.
        log_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                  XP_GetAdminStr(DBT_req_hdr_too_large_max_X),
                  size);
        protocol_status(sn, rq, PROTOCOL_BAD_REQUEST, NULL);
        return HTTP_PROCESSOR_CLIENT_FAILURE;
    }

    /*
     * Optionally tack on some request entity body
     */

    int request_packet_size = request_header_size;

    // If 1. the request has an non-encoded body, 2. we haven't filled the
    // first packet, and 3. the client isn't expecting a 100 Continue before
    // sending the body...
    if (request->content_length > 0 &&
        request_packet_size < TCP_MSS_GUESS &&
        !request->expect_100_continue &&
        client.readable)
    {
        // Try to append some request entity body data
        rv = client_read(sn->inbuf,
                         buffer + request_packet_size,
                         size - request_packet_size,
                         recv_timeout);

        if (rv > 0) {
            client.request_entity_received += rv;
            request_packet_size += rv;
        }
    }

    /*
     * Send the request header (and perhaps some request body)
     */

    pblock_nninsert("iwait-start", (int)PR_IntervalToMilliseconds(PR_IntervalNow()), rq->vars);

    rv = PR_Send(channel->fd,
                 buffer,
                 request_packet_size,
                 0,
                 config->transmit_timeout);

    if (rv != request_packet_size) {
        // Error sending request.  Complain, but not too loudly.
        proxyerror_agent_errorf(server_error_degree(request, PR_GetError()),
                                SERVICE_HTTP, sn, rq,
                                channel_get_remote_host(channel),
                                channel_get_remote_port(channel),
                                PROTOCOL_GATEWAY_TIMEOUT,
                                XP_GetAdminStr(DBT_error_send_req_because_X),
                                system_errmsg());
        return retry_or_failure(sn, rq, request, &response);
    }

    // Sent request header
    server.request_header_sent = request_header_size;

    // May have sent some request entity body
    server.request_entity_sent += (request_packet_size - request_header_size);

    /*
     * Send/receive loop
     */

    enum {
        STATUS_SUCCESS,
        STATUS_BEGIN_RESPONSE_ERROR,
        STATUS_UNCHUNK_ERROR,
        STATUS_UNCHUNK_TRAILING_DATA,
        STATUS_CLIENT_BODY_EOF,
        STATUS_CLIENT_BODY_READ_ERROR,
        STATUS_CLIENT_BODY_WRITE_ERROR,
        STATUS_CLIENT_DISCONNECTED_WAITING_FOR_HEADER,
        STATUS_CLIENT_DISCONNECTED_WAITING_FOR_BODY,
        STATUS_SERVER_HEADER_EOF,
        STATUS_SERVER_HEADER_READ_ERROR,
        STATUS_SERVER_BODY_EOF,
        STATUS_SERVER_BODY_READ_ERROR,
        STATUS_SERVER_BODY_WRITE_ERROR,
        STATUS_POLL_ERROR,
        STATUS_UNEXPECTED_ERROR
    } status = STATUS_UNEXPECTED_ERROR;

    for (;;) {
        /*
         * Read from client
         */

        if (client.readable) {
            // Read request body from client (N.B. this may block)
            rv = client_read(sn->inbuf,
                             buffer,
                             size,
                             recv_timeout);

            if (rv == 0) {
                // EOF from client
                client.readable = PR_FALSE;
                client.eof = PR_TRUE;

                /*
                 * Send last chunk to server
                 */

                if (request->content_length == -1) {
                    PR_Send(channel->fd,
                            LAST_CHUNK,
                            LAST_CHUNK_LEN,
                            0,
                            config->transmit_timeout);
                }

            } else if (rv < 0) {
                // Error reading from client
                status = STATUS_CLIENT_BODY_READ_ERROR;
                goto http_processor_finished;

            } else {
                // Got some request entity body
                int entity_size = rv;
                client.request_entity_received += entity_size;

                /*
                 * Send to server
                 */

                if (request->content_length == -1) {
                    char prefix[sizeof("7fffffff"CRLF)];
                    int prefix_len = PR_snprintf(prefix, sizeof(prefix) - 2, "%x", entity_size);
                    prefix[prefix_len++] = CR;
                    prefix[prefix_len++] = LF;

                    iov[0].iov_base = prefix;
                    iov[0].iov_len = prefix_len;
                    iov[1].iov_base = buffer;
                    iov[1].iov_len = entity_size;
                    iov[2].iov_base = (char *)CRLF;
                    iov[2].iov_len = 2;

                    // Send chunked request message body to server
                    rv = PR_Writev(channel->fd,
                                   (const PRIOVec *)iov,
                                   3,
                                   config->transmit_timeout);

                    if (rv != prefix_len + entity_size + 2) {
                        // Error sending to server
                        status = STATUS_SERVER_BODY_WRITE_ERROR;
                        goto http_processor_finished;
                    }

                } else {
                    // Send request entity body to server
                    rv = PR_Send(channel->fd,
                                 buffer,
                                 entity_size,
                                 0,
                                 config->transmit_timeout);

                    if (rv != entity_size) {
                        // Error sending to server
                        status = STATUS_SERVER_BODY_WRITE_ERROR;
                        goto http_processor_finished;
                    }
                }

                // Sent some request entity body
                server.request_entity_sent += entity_size;
            }
        }

        /*
         * Read from server
         */

        if (server.readable) {
            // If server reads block and we're not done sending the request...
            if (recv_timeout != PR_INTERVAL_NO_WAIT &&
                !request_complete(request, &client))
            {
                // The server fd must poll ready before we'll try to read from
                // it again.  We do this to avoid blocking on the server while
                // the client is trying to send us its request body.
                server.readable = PR_FALSE;
            }

            // Read response header or response body from server
            rv = PR_Recv(channel->fd,
                         buffer,
                         size,
                         0,
                         recv_timeout);

            if (rv == 0) {
                // EOF from server
                server.readable = PR_FALSE;
                server.eof = PR_TRUE;

            } else if (rv < 0) {
                // Error reading from server
                if (timeout_error(PR_GetError())) {
                    // poll() server fd before trying to recv again
                    server.readable = PR_FALSE;
                } else {
                    // Weird error
                    if (response.state == STATE_HEADER) {
                        status = STATUS_SERVER_HEADER_READ_ERROR;
                    } else {
                        status = STATUS_SERVER_BODY_READ_ERROR;
                    }
                    goto http_processor_finished;
                }

            } else {

                if (!first_response) {
                    pblock_nninsert("iwait-end", (int)PR_IntervalToMilliseconds(PR_IntervalNow()), rq->vars);
                    first_response++;
                }

                // Got some response header and/or response message body
                server.response_received += rv;

                netbuf buf;
                buf.sd = channel->fd;
                buf.rdtimeout = NET_ZERO_TIMEOUT;
                buf.inbuf = (unsigned char *)buffer;
                buf.pos = 0;
                buf.cursize = rv;
                buf.maxsize = size;

                /*
                 * Parse response header
                 */

                if (response.state == STATE_HEADER) {
                    // Got the first byte of the response header.  We'll now
                    // synchronously parse the entire response header.  Since
                    // we don't check net_isalive(sn->csd) while we're reading
                    // the headers, we want to timeout relatively quickly.
                    PRIntervalTime parse_timeout = config->parse_timeout;
                    if (parse_timeout > config->timeout)
                        parse_timeout = config->timeout;

                    int seconds = PR_IntervalToSeconds(parse_timeout);
                    if (seconds < 1)
                        seconds = 1;

                    buf.rdtimeout = seconds;

                    // Parse the response
                    if (begin_response(sn, rq,
                                       config,
                                       parser,
                                       request,
                                       &response,
                                       channel,
                                       &server.response_header_received,
                                       &buf) == PR_FAILURE)
                    {
                        status = STATUS_BEGIN_RESPONSE_ERROR;
                        goto http_processor_finished;
                    }
                }

                /*
                 * Send to client
                 */

                if (response.state != STATE_HEADER) {
                    while (buf.pos < buf.cursize) {
                        int ioc;

                        // Unchunk response message body as appropriate
                        for (ioc = 0; ioc < PR_MAX_IOVECTOR_SIZE; ioc++) {
                            rv = unchunk(&response, &buf, &iov[ioc]);
                            if (rv < 0) {
                                status = STATUS_UNCHUNK_ERROR;
                                goto http_processor_finished;
                            }
                            if (rv == 0) {
                                if (buf.pos != buf.cursize) {
                                    // Unchunking is done, but there is trailing
                                    // data. We will ignore it and close the
                                    // connection.
                                    status = STATUS_UNCHUNK_TRAILING_DATA;
                                    goto http_processor_finished;
                                }
                                break;
                            }
                        }

                        // If there's response entity body data available...
                        if (ioc > 0) {
                            // Coalesce small chunks
                            ioc = coalesce(iov, ioc);

                            // Got some response entity body
                            int entity_size = 0;
                            for (int i = 0; i < ioc; i++)
                                entity_size += iov[i].iov_len;
                            server.response_entity_received += entity_size;

                            // Send response entity body data to client
                            rv = net_writev(sn->csd, iov, ioc);
                            if (rv != entity_size) {
                                // Error sending to client
                                status = STATUS_CLIENT_BODY_WRITE_ERROR;
                                goto http_processor_finished;
                            }

                            // Sent some response entity body
                            client.response_entity_sent += entity_size;
                        }
                    }
                }
            }
        }

        /*
         * poll() as necessary
         */

        PRPollDesc pd[2];
        PRBool *readable[2];
        int pc = 0;

        // Is there work left to do on the request body?
        if (request_complete(request, &client)) {
            // We're done reading from the client
            client.readable = PR_FALSE;

        } else if (client.readable) {
            // No need to poll
            continue;

        } else if (client.eof) {
            // Client closed connection prematurely
            status = STATUS_CLIENT_BODY_EOF;
            goto http_processor_finished;

        } else {
            // We need to wait for more data from the client before we can make
            // any progress on the request body
            readable[pc] = &client.readable;
            pd[pc].fd = sn->csd;
            pd[pc].in_flags = PR_POLL_READ;
            pd[pc].out_flags = 0;
            pc++;
        }

        // Is there work left to do on the response header/body?
        if (response_complete(&response, &server)) {
            // We're done (There may be unread request body, but the HTTP
            // transaction is complete)
            status = STATUS_SUCCESS;
            goto http_processor_finished;

        } else if (server.readable) {
            // No need to poll
            continue;

        } else if (server.eof) {
            // Server closed connection prematurely
            if (response.state == STATE_HEADER) {
                status = STATUS_SERVER_HEADER_EOF;
            } else {
                status = STATUS_SERVER_BODY_EOF;
            }
            goto http_processor_finished;

        } else {
            // We need to wait for more data from the server before we can make
            // any progress on the response header/body
            readable[pc] = &server.readable;
            pd[pc].fd = channel->fd;
            pd[pc].in_flags = PR_POLL_READ;
            pd[pc].out_flags = 0;
            pc++;
        }

        if (pc == 0) {
            // Unreachable (I hope)
            PR_ASSERT(0);
            status = STATUS_UNEXPECTED_ERROR;
            goto http_processor_finished;
        }

        // Remember when we started poll()'ing
        PRIntervalTime poll_epoch = ft_timeIntervalNow();

        // Don't wait too long.  We want to wake up periodically.
        PRIntervalTime poll_interval = config->timeout;
        if (poll_interval > config->poll_interval)
            poll_interval = config->poll_interval;

http_processor_repoll:

        // poll() the client/server as appropriate
        rv = PR_Poll(pd, pc, poll_interval);

        // If poll() timed out...
        if (rv == 0) {
            // How long have we been poll()'ing?
            PRIntervalTime poll_elapsed = ft_timeIntervalNow() - poll_epoch;

            // Have we exceeded the timeout?
            if (poll_elapsed >= config->timeout) {
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                if (!request_complete(request, &client)) {
                    status = STATUS_CLIENT_BODY_READ_ERROR;
                } else if (response.state == STATE_HEADER) {
                    status = STATUS_SERVER_HEADER_READ_ERROR;
                } else {
                    status = STATUS_SERVER_BODY_READ_ERROR;
                }
                goto http_processor_finished;
            }

            // If it's been a while...
            if (poll_interval >= config->poll_interval) {
                // Force any buffered data to the client
                if (response.state != STATE_HEADER)
                    net_flush(sn->csd);

                // Did the client go away?
                if (!net_isalive(sn->csd)) {
                    if (!request_complete(request, &client)) {
                        status = STATUS_CLIENT_BODY_EOF;
                    } else if (response.state == STATE_HEADER) {
                        status = STATUS_CLIENT_DISCONNECTED_WAITING_FOR_HEADER;
                    } else {
                        status = STATUS_CLIENT_DISCONNECTED_WAITING_FOR_BODY;
                    }
                    goto http_processor_finished;
                }
            }

            // XXX is the server shutting down?

            // Everything looks cool, keep poll()'ing
            PR_ASSERT(poll_elapsed < config->timeout);
            PRIntervalTime poll_remaining = config->timeout - poll_elapsed;
            if (poll_interval > poll_remaining)
                poll_interval = poll_remaining;
            goto http_processor_repoll;
        }

        if (rv == -1) {
            // We don't expect poll() to fail
            PR_ASSERT(0);
            status = STATUS_POLL_ERROR;
            goto http_processor_finished;
        }

        // Set client.readable and/or server.readable as appropriate
        for (int i = 0; i < pc; i++) {
            if (pd[i].out_flags & PR_POLL_READ)
                *readable[i] = PR_TRUE;
        }
    }

http_processor_finished:

    pblock_nninsert("fwait-end", (int)PR_IntervalToMilliseconds(PR_IntervalNow()), rq->vars);

    /*
     * How'd it go?
     */

    HttpProcessorResult result;
    const char *cli_status = FIN;
    const char *svr_status = FIN;
    
    if (status == STATUS_SUCCESS) {
        // Everything turned out okay.  Can caller keep the connection open?
        if (response.keep_alive) {
            result = HTTP_PROCESSOR_KEEP_ALIVE;
        } else {
            result = HTTP_PROCESSOR_CLOSE;
        }

    } else if (status == STATUS_UNCHUNK_TRAILING_DATA) {
        result = HTTP_PROCESSOR_CLOSE;
    } else if (status == STATUS_BEGIN_RESPONSE_ERROR) {
        // begin_response() already logged an error message
        svr_status = INTR;
        result = HTTP_PROCESSOR_SERVER_FAILURE;

    } else {
        // Some error condition we need to log an error message for
        PRErrorCode prerr = PR_GetError();

        switch (status) {
        case STATUS_UNCHUNK_ERROR:
            proxyerror_agent_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                   channel_get_remote_host(channel),
                                   channel_get_remote_port(channel),
                                   PROTOCOL_BAD_GATEWAY, 
                                   XP_GetAdminStr(DBT_bogus_chunked_res_body));
            svr_status = INTR;
            result = HTTP_PROCESSOR_SERVER_FAILURE;
            break;

        case STATUS_CLIENT_BODY_EOF:
            log_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                      XP_GetAdminStr(DBT_error_read_req_body_client_closed));
            protocol_status(sn, rq, PROTOCOL_BAD_REQUEST, NULL);
            cli_status = INTR;
            result = HTTP_PROCESSOR_CLIENT_FAILURE;
            break;

        case STATUS_CLIENT_BODY_READ_ERROR:
            log_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                      XP_GetAdminStr(DBT_error_read_req_body_because_X),
                      system_errmsg());
            if (timeout_error(prerr)) {
                protocol_status(sn, rq, PROTOCOL_REQUEST_TIMEOUT, NULL);
                cli_status = TIMEOUT;
            } else {
                protocol_status(sn, rq, PROTOCOL_BAD_REQUEST, NULL);
                cli_status = INTR;
            }
            result = HTTP_PROCESSOR_CLIENT_FAILURE;
            break;

        case STATUS_CLIENT_BODY_WRITE_ERROR:
            log_error(client_write_error_degree(prerr), SERVICE_HTTP, sn, rq,
                      XP_GetAdminStr(DBT_error_send_res_because_X),
                      system_errmsg());
            cli_status = INTR;
            result = HTTP_PROCESSOR_CLIENT_FAILURE;
            break;

        case STATUS_CLIENT_DISCONNECTED_WAITING_FOR_HEADER:
            log_error(LOG_VERBOSE, SERVICE_HTTP, sn, rq,
                      XP_GetAdminStr(DBT_client_disconn_before_res_begin));
            cli_status = INTR;
            result = HTTP_PROCESSOR_CLIENT_FAILURE;
            break;

        case STATUS_CLIENT_DISCONNECTED_WAITING_FOR_BODY:
            log_error(LOG_VERBOSE, SERVICE_HTTP, sn, rq,
                      XP_GetAdminStr(DBT_client_disconn_before_res_done));
            cli_status = INTR;
            result = HTTP_PROCESSOR_CLIENT_FAILURE;
            break;

        case STATUS_SERVER_HEADER_EOF:
            proxyerror_agent_error(server_eof_degree(request),
                                   SERVICE_HTTP, sn, rq,
                                   channel_get_remote_host(channel),
                                   channel_get_remote_port(channel),
                                   PROTOCOL_BAD_GATEWAY, 
                                   XP_GetAdminStr(DBT_res_hdr_server_closed));
            svr_status = INTR;
            result = retry_or_failure(sn, rq, request, &response);
            break;

        case STATUS_SERVER_HEADER_READ_ERROR:
            if (timeout_error(prerr)) {
                proxyerror_agent_errorf(server_eof_degree(request),
                                        SERVICE_HTTP, sn, rq,
                                        channel_get_remote_host(channel),
                                        channel_get_remote_port(channel),
                                        PROTOCOL_GATEWAY_TIMEOUT,
                                        XP_GetAdminStr(DBT_res_hdr_because_X),
                                        system_errmsg());
                svr_status = TIMEOUT;
            } else {
                proxyerror_agent_errorf(server_eof_degree(request),
                                        SERVICE_HTTP, sn, rq,
                                        channel_get_remote_host(channel),
                                        channel_get_remote_port(channel),
                                        PROTOCOL_BAD_GATEWAY,
                                        XP_GetAdminStr(DBT_res_hdr_because_X),
                                        system_errmsg());
                svr_status = INTR;
            }
            result = retry_or_failure(sn, rq, request, &response);
            break;

        case STATUS_SERVER_BODY_EOF:
            proxyerror_agent_error(server_eof_degree(request),
                                   SERVICE_HTTP, sn, rq,
                                   channel_get_remote_host(channel),
                                   channel_get_remote_port(channel),
                                   PROTOCOL_BAD_GATEWAY,
                                   XP_GetAdminStr(DBT_res_body_server_closed));
            svr_status = INTR;
            result = HTTP_PROCESSOR_SERVER_FAILURE;
            break;

        case STATUS_SERVER_BODY_READ_ERROR:
            if (timeout_error(prerr)) {
                proxyerror_agent_errorf(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                        channel_get_remote_host(channel),
                                        channel_get_remote_port(channel),
                                        PROTOCOL_GATEWAY_TIMEOUT,
                                        XP_GetAdminStr(DBT_res_body_because_X),
                                        system_errmsg());
                svr_status = TIMEOUT;
            } else {
                proxyerror_agent_errorf(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                        channel_get_remote_host(channel),
                                        channel_get_remote_port(channel),
                                        PROTOCOL_BAD_GATEWAY,
                                        XP_GetAdminStr(DBT_res_body_because_X),
                                        system_errmsg());
                svr_status = INTR;
            }
            result = HTTP_PROCESSOR_SERVER_FAILURE;
            break;

        case STATUS_SERVER_BODY_WRITE_ERROR:
            proxyerror_agent_errorf(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                    channel_get_remote_host(channel),
                                    channel_get_remote_port(channel),
                                    PROTOCOL_BAD_GATEWAY,
                                    XP_GetAdminStr(DBT_req_body_because_X),
                                    system_errmsg());
            svr_status = INTR;
            result = HTTP_PROCESSOR_SERVER_FAILURE;
            break;

        case STATUS_POLL_ERROR:
            proxyerror_agent_errorf(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                    channel_get_remote_host(channel),
                                    channel_get_remote_port(channel),
                                    PROTOCOL_BAD_GATEWAY,
                                    XP_GetAdminStr(DBT_poll_error_because_X),
                                    system_errmsg());
            cli_status = INTR;
            svr_status = INTR;
            result = HTTP_PROCESSOR_SERVER_FAILURE;
            break;

        default:
            PR_ASSERT(status == STATUS_UNEXPECTED_ERROR);
            proxyerror_agent_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                                   channel_get_remote_host(channel),
                                   channel_get_remote_port(channel),
                                   PROTOCOL_BAD_GATEWAY,
                                   XP_GetAdminStr(DBT_unexpected_error));
            cli_status = INTR;
            svr_status = INTR;
            result = HTTP_PROCESSOR_SERVER_FAILURE;
            break;
        }
    }

    // Do the following only if we are not blocking this mime type
    if (result != HTTP_PROCESSOR_RETRY) {
        // Get Session byte counts
        net_flush(sn->csd);
        PRInt64 c2p_hl = ((NSAPISession *)sn)->received;
        if (c2p_hl > client.request_entity_received) {
            c2p_hl -= client.request_entity_received;
        } else {
            c2p_hl = 0;
        }
        PRInt64 p2c_hl = ((NSAPISession *)sn)->transmitted;
        if (p2c_hl > client.response_entity_sent) {
            p2c_hl -= client.response_entity_sent;
        } else {
            p2c_hl = 0;
        }

        // Populate rq->vars for logging
        pblock_kllinsert(pb_key_c2p_hl,
                         c2p_hl,
                         rq->vars);
        if (request->has_body) {
            pblock_kllinsert(pb_key_c2p_cl,
                             client.request_entity_received,
                             rq->vars);
        }
        pblock_kninsert(pb_key_p2r_hl,
                        server.request_header_sent,
                        rq->vars);
        if (request->has_body) {
            pblock_kllinsert(pb_key_p2r_cl,
                             server.request_entity_sent,
                             rq->vars);
        }
        pblock_kninsert(pb_key_r2p_hl,
                        server.response_header_received,
                        rq->vars);
        if (response.has_body) {
            pblock_kllinsert(pb_key_r2p_cl,
                             server.response_entity_received,
                             rq->vars);
        }
        pblock_kllinsert(pb_key_p2c_hl,
                         p2c_hl,
                         rq->vars);
        if (response.has_body) {
            pblock_kllinsert(pb_key_p2c_cl,
                             client.response_entity_sent,
                             rq->vars);
        }
        pblock_kvinsert(pb_key_cli_status,
                        cli_status,
                        strlen(cli_status),
                        rq->vars);
        pblock_kvinsert(pb_key_svr_status,
                        svr_status,
                        strlen(svr_status),
                        rq->vars);
        route_set_actual_route(sn, rq);

        // XXX_MCC_PROXY timers?
    }

    return result;
}


/* ---------------------------- is_safe_method ---------------------------- */

static inline PRBool is_safe_method(Request *rq)
{
    switch (rq->method_num) {
    case METHOD_HEAD:
    case METHOD_GET:
    case METHOD_OPTIONS:
        return PR_TRUE;

    default:
        return PR_FALSE;
    }
}


/* ----------------------------- get_channel ------------------------------ */

static Channel * get_channel(Session *sn,
                             Request *rq,
                             ParsedUrl *url,
                             PRBool reuse_persistent,
                             PRIntervalTime timeout,
                             PRBool will_retry)
{
    // Get a nul-terminated host string
    const char *host;
    char *dynamic_host = NULL;
    if (url->host[url->host_len] == '\0') {
        host = url->host;
    } else {
        dynamic_host = (char *)pool_malloc(sn->pool, url->host_len + 1);
        if (!dynamic_host)
            return NULL;

        memcpy(dynamic_host, url->host, url->host_len);
        dynamic_host[url->host_len] = '\0';

        host = dynamic_host;
    }

    // If there's no scheme, we assume http
    int port;
    if (url->port != 0) {
        port = url->port;
    } else {
        port = HTTP_PORT;
    }

    // Use SSL if there was an explicit https scheme
    PRBool secure = (url->scheme == SCHEME_HTTPS);

    log_error(LOG_VERBOSE, SERVICE_HTTP, sn, rq,
              "attempting to contact %s:%d",
              host, port);

    // Get a channel to the remote server
    Channel *channel = channel_acquire(sn, rq,
                                       host, port,
                                       secure,
                                       reuse_persistent,
                                       timeout);
    if (!channel) {
        int degree = will_retry ? LOG_VERBOSE : LOG_FAILURE;
        proxyerror_channel_error(degree, SERVICE_HTTP, sn, rq, host, port);
    }

    // If we needed to dynamically allocate the host string, free it now
    if (dynamic_host)
        pool_free(sn->pool, dynamic_host);

    return channel;
}


/* -------------------------- httpclient_process -------------------------- */

int httpclient_process(pblock *pb,
                       Session *sn,
                       Request *rq,
                       char *path,
                       char *proxy_addr,
                       char *origin_addr)
{
    PR_ASSERT(_init_status == PR_SUCCESS);

    // Get the customized HTTP client config for this request, falling back to
    // the default as appropriate
    HttpClientHeaders *headers = (HttpClientHeaders *)
        request_get_data(rq, _headers_request_slot);
    if (!headers)
        headers = &_init_headers;
    HttpClientConfig *config = (HttpClientConfig *)
        request_get_data(rq, _config_request_slot);
    if (!config)
        config = &_init_config;

    // Get the HttpResponseParser that will parse the HTTP response
    HttpResponseParser *parser = (HttpResponseParser *)
        session_get_thread_data(sn, _parser_thread_slot);
    if (!parser) {
        // No cached thread-specific HttpResponseParser, create a new one
        parser = new HttpResponseParser();
        if (!parser)
            return REQ_ABORTED;

        session_set_thread_data(sn, _parser_thread_slot, parser);
    }

    HttpProcessorRequest request;

    // Handle Expect: (RFC 2616 14.20)
    request.expect_100_continue = PR_FALSE;
    if (pb_param *pp = pblock_findkey(pb_key_expect, rq->headers)) {
        // Parse tokens from the Expect: header
        char *prev;
        char *token = util_strtok(pp->value, HTTP_TOKEN_SEPARATORS, &prev);
        while (token) {
            if (!strcasecmp(token, "100-continue")) {
                httpfilter_suppress_100_continue(sn, rq);
                request.expect_100_continue = PR_TRUE;
            } else {
                protocol_status(sn, rq, PROTOCOL_EXPECTATION_FAILED, NULL);
                return REQ_ABORTED;
            }
            token = util_strtok(NULL, HTTP_TOKEN_SEPARATORS, &prev);
        }

        // Expect: 100-continue to known pre-HTTP/1.1 origin (RFC 2616 8.2.3)?
        if (request.expect_100_continue &&
            config->protv_num < PROTOCOL_VERSION_HTTP11)
        {
            protocol_status(sn, rq, PROTOCOL_EXPECTATION_FAILED, NULL);
            return REQ_ABORTED;
        }
    }

    // Determine which HTTP version the request should use
    if (config->protocol == _init_config.protocol &&
        config->protv_num > PROTOCOL_VERSION_HTTP10 &&
        rq->protv_num <= PROTOCOL_VERSION_HTTP10 &&
        pblock_findkey(pb_key_range, rq->headers))
    {
        // Some versions of Navigator send an HTTP/1.1 Range: header but expect
        // pre-HTTP/1.1 Request-Range: semantics.  As a result, we use HTTP/1.0
        // when proxying Range: requests from pre-HTTP/1.1 clients.
        request.protocol = "HTTP/1.0";
        request.protocol_len = 8;
        request.protv_num = PROTOCOL_VERSION_HTTP10;
    } else {
        request.protocol = config->protocol;
        request.protocol_len = config->protocol_len;
        request.protv_num = config->protv_num;
    }

    // Does the client's request have a body?
    if (char *cl = pblock_findkeyval(pb_key_content_length, rq->headers)) {
        request.has_body = PR_TRUE;
        request.content_length = util_atoi64(cl);
    } else if (pblock_findkey(pb_key_transfer_encoding, rq->headers)) {
        request.has_body = PR_TRUE;
        request.content_length = -1;
    } else {
        request.has_body = PR_FALSE;
        request.content_length = 0;
    }

    // Chunked request to known pre-HTTP/1.1 origin?
    if (request.content_length == -1 &&
        !config->always_allow_chunked &&
        config->protv_num < PROTOCOL_VERSION_HTTP11)
    {
        log_error(LOG_FAILURE, SERVICE_HTTP, sn, rq, 
                  XP_GetAdminStr(DBT_chunked_to_pre_http11));
        protocol_status(sn, rq, PROTOCOL_LENGTH_REQUIRED, NULL);
        return REQ_ABORTED;
    }

    // Remove the Connection: header (and any headers that match Connection:
    // header tokens) from rq->headers.  Note that we don't update full-headers
    // yet.
    pb_param *connection_tokens[MAX_CONNECTION_TOKENS];
    int num_connection_tokens = remove_connection_headers(rq->headers,
        connection_tokens, MAX_CONNECTION_TOKENS);

    PR_ASSERT(!pblock_findkey(pb_key_connection, rq->headers));

    // dynamic_suppressed_header_names[] is a list of header names we need to
    // suppress based on the contents of the request.  It is typically an empty
    // list.
    const char *dynamic_suppressed_header_names[MAX_CONNECTION_TOKENS + 1];
    int dynamic_suppressed_header_lens[MAX_CONNECTION_TOKENS + 1];
    int num_dynamic_suppressed_headers = 0;

    // If HTTP/1.1 Range: is present with multiple byte ranges, add Range: to
    // the dynamic list of suppressed headers.  This is necessary because we
    // don't know how to parse multipart/byteranges, but HTTP/1.1 allows
    // origin servers to use the content of a multipart/byteranges response
    // body to define the HTTP message length.
    if (config->protv_num >= PROTOCOL_VERSION_HTTP11) {
        char *range = pblock_findkeyval(pb_key_range, rq->headers);
        if (range && strchr(range, ',')) {
            param_free(pblock_removekey(pb_key_range, rq->headers));
            dynamic_suppressed_header_names[num_dynamic_suppressed_headers] =
                "range";
            dynamic_suppressed_header_lens[num_dynamic_suppressed_headers] = 5;
            num_dynamic_suppressed_headers++;
        }
    }

    // Save Proxy-authorization: in case we need it later
    if (pb_param *pp = pblock_removekey(pb_key_proxy_authorization,
                                        rq->headers))
    {
        pblock_kpinsert(pb_key_proxy_authorization, pp, rq->vars);
    }

    // Should we reformat the request headers (that is, use rq->headers instead
    // of full-headers)?
    PRBool reformat_request_headers = config->reformat_request_headers;
    if (request.content_length > 0) {
        char *te = pblock_findkeyval(pb_key_transfer_encoding, rq->headers);
        if (te && !strcmp(te, "identity")) {
            // The original request was Transfer-encoding: chunked but was
            // unchunked due to ChunkedRequestBufferSize != 0.  As a result,
            // there'll be a content-length in rq->headers by no corresponding
            // Content-length: in full-headers.  We'll use rq->headers, not
            // full-headers.
            reformat_request_headers = PR_TRUE;
        }
    }

    // Process the original request headers
    char *full_headers = pblock_findkeyval(pb_key_full_headers, rq->reqpb);
    if (full_headers && !reformat_request_headers) {
        // Add the header names that 1. appeared as Connection: header tokens,
        // 2. appeared as standalone headers, and 3. weren't Keep-Alive or
        // Close to the dynamic list of suppressed headers.  (We only need to
        // do this for the full-headers case as remove_connection_headers()
        // already removed these headers from rq->headers.)
        for (int i = 0; i < num_connection_tokens; i++) {
            dynamic_suppressed_header_names[num_dynamic_suppressed_headers] =
                connection_tokens[i]->name;
            dynamic_suppressed_header_lens[num_dynamic_suppressed_headers] =
                strlen(connection_tokens[i]->name);
            num_dynamic_suppressed_headers++;
        }

        // Parse full-headers, removing headers that should be suppressed from
        // rq->headers and adding headers that shouldn't be suppressed to
        // request.full_headers_iov[]
        request.full_headers = PR_TRUE;
        request.full_headers_ioc =
            suppress_full_headers(sn,
                                  rq,
                                  headers,
                                  dynamic_suppressed_header_names,
                                  dynamic_suppressed_header_lens,
                                  num_dynamic_suppressed_headers,
                                  full_headers,
                                  request.full_headers_iov,
                                  FULL_HEADERS_IOMAX);
    } else {
        // Parse rq->headers, removing headers that should be suppressed
        request.full_headers = PR_FALSE;
        suppress_rq_headers(sn,
                            rq,
                            headers,
                            dynamic_suppressed_header_names,
                            num_dynamic_suppressed_headers);

    }

    // Free the Connection: token parameters that remove_connection_headers()
    // removed earlier
    while (num_connection_tokens--)
        param_free(connection_tokens[num_connection_tokens]);

    int res = REQ_NOACTION;
    char *buffer = NULL;
    int retry;

    // Buffer for request/response
    int size = _buffer_size;
    buffer = (char *)pool_malloc(sn->pool, size);
    if (!buffer)
        return REQ_ABORTED;

    PRIntervalTime epoch = ft_timeIntervalNow();

    // Attempt the transaction, retrying as appropriate
    for (retry = 0; ; retry++) {
        if (retry) {
            log_error(LOG_VERBOSE, SERVICE_HTTP, sn, rq,
                      "retrying request (retry %d of %d)",
                      retry, config->retries);
        }

        // Can we resend the HTTP request if it fails this time?
        request.can_retry = is_safe_method(rq) &&
                            request.content_length == 0 &&
                            retry < config->retries;

        // Will we attempt to reuse a connection somebody else kept open?
        request.reusing_persistent = config->keep_alive;
        if (retry) {
            // Don't reuse an existing persistent connection for retries
            request.reusing_persistent = PR_FALSE;
        } else if (!request.can_retry && !config->always_use_keep_alive) {
            // Don't reuse an existing persistent connection if we can't retry
            // (The administrator can override this behaviour with
            // always-use-keep-alive="true")
            request.reusing_persistent = PR_FALSE;
        }

        // When present, proxy_addr should contain a port (we expect proxy_addr
        // to be NULL, not "DIRECT", when we're not supposed to use a proxy)
        PR_ASSERT(!proxy_addr || strchr(proxy_addr, ':'));

        // Use the absoluteURI form for Request-URI when talking to a proxy
        request.absolute_uri = (proxy_addr != NULL);

        // Where should we send the request?
        ParsedUrl channel_url;
        char *channel_addr;
        if (proxy_addr) {
            channel_addr = proxy_addr;
        } else if (origin_addr) {
            channel_addr = origin_addr;
        } else if (util_is_url(path)) {
            channel_addr = path;
        } else {
            protocol_status(sn, rq, PROTOCOL_BAD_REQUEST, NULL);
            log_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                      XP_GetAdminStr(DBT_invalid_url_X), path);
            res = REQ_ABORTED;
            goto httpclient_processor_finished;
        }
        if (url_parse(channel_addr, &channel_url) != PR_SUCCESS ||
            !channel_url.host)
        {
            protocol_status(sn, rq, PROTOCOL_BAD_REQUEST, NULL);
            log_error(LOG_FAILURE, SERVICE_HTTP, sn, rq,
                      XP_GetAdminStr(DBT_invalid_url_X), channel_addr);
            res = REQ_ABORTED;
            goto httpclient_processor_finished;
        }

        // Generate a Host: header value if an HTTP/1.0 client didn't provide
        // one
        if (rq->protv_num < PROTOCOL_VERSION_HTTP11 &&
            config->protv_num >= PROTOCOL_VERSION_HTTP11 &&
            !pblock_findkey(pb_key_host, rq->headers))
        {
            ParsedUrl path_url;
            if (url_parse(path, &path_url) == PR_SUCCESS &&
                path_url.host)
            {
                request.host = path_url.host;
                request.host_len = path_url.host_port_len;
            } else {
                request.host = channel_url.host;
                request.host_len = channel_url.host_port_len;
            }
        } else {
            request.host = NULL;
            request.host_len = 0;
        }

        // Acquire a channel to the remote server
        Channel *channel = get_channel(sn, rq,
                                       &channel_url,
                                       request.reusing_persistent,
                                       config->connect_timeout,
                                       (retry < config->retries));
        if (!channel) {
            // Couldn't connect to server
            protocol_status(sn, rq, PROTOCOL_GATEWAY_TIMEOUT, NULL);

            // That server sucked.  We want one that sucks less.
            res = route_offline(sn, rq, &proxy_addr, &origin_addr);
            if (res != REQ_PROCEED && res != REQ_NOACTION)
                goto httpclient_processor_finished;

            // Retry if possible (note that this is a DNS/connect() retry, not
            // an HTTP request retry, so request.can_retry is irrelevant)
            if (retry < config->retries) {
                if (net_isalive(sn->csd))
                    continue;

                log_error(LOG_VERBOSE, SERVICE_HTTP, sn, rq,
                          "client disconnected before connection to remote server could be established");
            }

            res = REQ_ABORTED;
            goto httpclient_processor_finished;
        }

        // Process the HTTP transaction
        HttpProcessorResult result = http_processor(sn, rq,
                                                    headers,
                                                    config,
                                                    parser,
                                                    &request,
                                                    buffer,
                                                    size,
                                                    channel);

        switch (result) {
        case HTTP_PROCESSOR_KEEP_ALIVE:
            // We can keep the channel open.  Should we?
            if (config->keep_alive) {
                channel_release(channel, config->keep_alive_timeout);
            } else {
                channel_release(channel, PR_INTERVAL_NO_WAIT);
            }
            res = REQ_PROCEED;
            goto httpclient_processor_finished;

        case HTTP_PROCESSOR_CLOSE:
            // We can't keep the channel open
            channel_release(channel, PR_INTERVAL_NO_WAIT);
            res = REQ_PROCEED;
            goto httpclient_processor_finished;

        case HTTP_PROCESSOR_CLIENT_FAILURE:
            // Destroy the channel as it may be in an inconsistent state
            channel_release(channel, PR_INTERVAL_NO_WAIT);
            res = REQ_ABORTED;
            goto httpclient_processor_finished;

        case HTTP_PROCESSOR_SERVER_FAILURE:
            // That server sucked
            channel_purge(channel);
            route_offline(sn, rq, &proxy_addr, &origin_addr);
            res = REQ_ABORTED;
            goto httpclient_processor_finished;
        }

        // http_processor() reported a retryable error (most likely a stale
        // keep-alive connection)
        PR_ASSERT(result == HTTP_PROCESSOR_RETRY);
        PR_ASSERT(request.can_retry);

        if (retry == 0 && request.reusing_persistent && config->retries > 1) {
            // This channel is dead, but we'll try again with a new channel to
            // the same server
            channel_release(channel, PR_INTERVAL_NO_WAIT);
        } else {
            // That server sucked.  We want one that sucks less.
            channel_purge(channel);
            res = route_offline(sn, rq, &proxy_addr, &origin_addr);
            if (res != REQ_PROCEED && res != REQ_NOACTION)
                goto httpclient_processor_finished;
        }

        // Start again from the top...
        PR_ASSERT(retry < config->retries);
    }

httpclient_processor_finished:

    PR_ASSERT(res != REQ_NOACTION);

    // Cleanup
    pool_free(sn->pool, buffer);

    return res;
}


/* ----------------------- httpclient_service_http ------------------------ */

int httpclient_service_http(pblock *pb, Session *sn, Request *rq)
{
    pblock_nninsert("xfer-start", (int)PR_IntervalToMilliseconds(PR_IntervalNow()), rq->vars);

    // Handle Max-forwards:
#ifdef FEAT_PROXY
    if (!INTERNAL_REQUEST(rq)) {
        pb_param *pp = pblock_removekey(pb_key_max_forwards, rq->headers);
        if (pp) {
            char *max_forwards = util_decrement_string(pp->value);
            PRBool do_not_forward = (*max_forwards == '\0');
            general_set_or_replace_header2("max-forwards",
                                           max_forwards,
                                           sn, rq);
            param_free(pp);
            if (do_not_forward)
                return REQ_NOACTION;
        }
    }
#endif

    char *path = pblock_findkeyval(pb_key_path, rq->vars);
    if (!path)
        return record_times(REQ_ABORTED, rq);

    // Run Route to see if we should route the request through a proxy or to a
    // specific origin server
    int res = servact_route(sn, rq);
    if (res != REQ_PROCEED)
        return record_times(REQ_ABORTED, rq);

    char *proxy_addr = route_get_proxy_addr(sn, rq);
    char *origin_addr = route_get_origin_addr(sn, rq);

    // The origin server will establish the Content-type:, if any
    param_free(pblock_removekey(pb_key_content_type, rq->srvhdrs));
    res = httpclient_process(pb, sn, rq, path, proxy_addr, origin_addr);

    return record_times(res, rq);
}
