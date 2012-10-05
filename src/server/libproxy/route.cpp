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
 * route.cpp: Proxy server routing
 *
 * Chris Elving
 */

#include "pk11func.h"
#include "base/util.h"
#include "base/url64.h"
#include "base/vs.h"
#include "base/pool.h"
#include "base/systhr.h"
#include "frame/conf.h"
#include "frame/func.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/httpact.h"
#include "frame/httpdir.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/WebServer.h"
#include "NsprWrap/NsprSink.h"
#include "time/nstime.h"
#include "libproxy/url.h"
#include "libproxy/reverse.h"
#include "libproxy/httpclient.h"
#include "libproxy/dbtlibproxy.h"
#include "libproxy/route.h"


/*
 * SAF names
 */
#define SET_ORIGIN_SERVER ("set-origin-server")
#define SET_PROXY_SERVER ("set-proxy-server")
#define SET_SOCKS_SERVER ("set-socks-server")

#define SERVER "server"
#define HTTP_PREFIX "http://"
#define HTTP_PREFIX_LEN (sizeof(HTTP_PREFIX) - 1)
#define HTTPS_PREFIX "https://"
#define HTTPS_PREFIX_LEN (sizeof(HTTPS_PREFIX) - 1)
#define TEXT_PLAIN "text/plain"
#define TEXT_PLAIN_LEN (sizeof(TEXT_PLAIN) - 1)
#define PROXY_AGENT PRODUCT_HEADER_ID"/"PRODUCT_VERSION_ID
#define PROXY_AGENT_LEN (sizeof(PROXY_AGENT) - 1)
#define DIRECT "DIRECT"
#define DIRECT_LEN (sizeof(DIRECT) - 1)
#define SOCKS "SOCKS"
#define SOCKS_LEN (sizeof(SOCKS) - 1)
#define PROXY "PROXY"
#define PROXY_LEN (sizeof(PROXY) - 1)
#define ORIGIN "ORIGIN"
#define ORIGIN_LEN (sizeof(ORIGIN) - 1)
#define MAGNUS_INTERNAL_FORCE_HTTP_ROUTE "magnus-internal/force-http-route"
#define MAGNUS_INTERNAL_FORCE_HTTP_ROUTE_LEN (sizeof(MAGNUS_INTERNAL_FORCE_HTTP_ROUTE) - 1)
#define CHECK_HTTP_SERVER ("check-http-server")

/*
 * Number of US-ASCII characters (bytes) in a jroute (must be evenly divisible
 * into machine words)
 */
#define JROUTE_LEN (16)

/*
 * Number of machine words in a jroute
 */
#define JROUTE_WORDS (JROUTE_LEN / sizeof(unsigned))

/*
 * Size of an MD5 hash in bytes
 */
#define MD5_SIZE (16)

/*
 * Number of bytes from an MD5 hash used to generate a jroute
 */
#define MD5_BYTES_FOR_JROUTE (JROUTE_LEN * 3 / 4)

/*
 * Jroute is a string of printable characters that uniquely identifies a
 * Gateway
 */
union Jroute {
    unsigned words[JROUTE_WORDS];
    char chars[JROUTE_LEN];
}; 

/*
 * Gateway records information about an adjacent, explicitly configured proxy
 * or origin server.  Gateway objects are always allocated from the permanent
 * heap.
 *
 * (Each Gateway will have at least one corresponding channel.cpp Daemon, but
 * not every daemon is a gateway; in general, Internet origin servers are not
 * gateways.)
 */
struct Gateway {
    struct Gateway *next; // _gateway_url_ht hash chain

    char *url; // canonical URL for this Gateway
    int url_len;

    char *addr; // value for proxy-addr/origin-addr in rq->vars
    int addr_len;

    char *host_port; // value for Host: header
    int host_port_len;

    Jroute jroute; // unique ID for jroute in rq->vars

    PRLock *offline_lock; // serializes access to offline_vsid
    char *offline_vsid; // VS that discovered Gateway was offline
};

/*
 * Size of the  _gateway_url_ht hash table.  Should probably be prime and
 * greater than the number of CPUs and gateways.
 */
#define GATEWAY_URL_HT_SIZE (31)

/*
 * The _gateway_url_ht hash table tracks the proxy's neighbours (i.e. all the
 * servers mentioned in Route directives), hashed by canonical URL
 */
static struct {
    PRLock *lock;
    Gateway *head;
} _gateway_url_ht[GATEWAY_URL_HT_SIZE];

/*
 * ForeachParamFn * can be passed to foreach() to iterate over pb_params
 */
typedef int (ForeachParamFn)(pblock *pb, pb_param *pp, void *data);

/*
 * Fragment is the portion of a string defined by a particular pointer and
 * length
 */
struct Fragment {
    const char *p;
    int len;
}; 

/*
 * RouteConfig records the different gateways configured for a single Route
 * directive and various associated parameters.  RouteConfig objects may be
 * allocated from the permanent heap (when they're constructed during VS init)
 * or from the session pool (when they're constructed in response to a
 * func_exec() call).
 */
struct RouteConfig {
    pool_handle_t *pool; // pool RouteConfig was allocated from

    Gateway **gateways;
    int num_gateways;

    unsigned choose_gateway_counter;

    int num_sticky_cookies;
    char **sticky_cookies;

    int num_sticky_params;
    char **sticky_params;

    char *route_hdr;

    char *route_cookie;
    int route_cookie_len;

    PRBool rewrite_host;
};

/*
 * RouteCookieStatus records route cookie processing status during
 * route_process_cookies().
 */
struct RouteCookieStatus {
    RouteConfig *route;
    const char *jroute; // downstream Gateway's jroute from rq->vars
    int jroute_len; // length of jroute
    pblock *attributes; // attributes (e.g. ";Path=/") for route cookies
    int num_cookies; // number of cookies in response
    int num_route_cookies; // number of route cookies in response
};

/*
 * request_get_data/request_set_data slot that holds a RouteConfig *
 */
static int _route_request_slot = -1;

/*
 * Thread that performs health checks of offline Gateways
 */
static PRThread *_offline_thread;

/*
 * Set on each call to set_http_server() to indicate routing activity
 */
static unsigned _set_http_server_time = 0;

/*
 * _init_status is PR_SUCCESS if route_init() completed successfully
 */
static PRStatus _init_status = PR_FAILURE;


/* ------------------------------- foreach -------------------------------- */

static inline int foreach(pblock *pb,
                          const pb_key *key,
                          ForeachParamFn *fn = NULL,
                          void *data = NULL)
{
    int num = 0;

    for (int i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        while (p) {
            if (param_key(p->param) == key) {
                if (fn) {
                    num += (*fn)(pb, p->param, data);
                } else {
                    num++;
                }
            }
            p = p->next;
        }
    }

    return num;
}


/* --------------------------- fragmentcasecmp ---------------------------- */

static inline int fragmentcasecmp(Fragment *f1, Fragment *f2)
{
    if (f1->len != f2->len)
        return f1->len - f2->len;

    return strncasecmp(f1->p, f2->p, f1->len);
}


/* ----------------------------- parse_scheme ----------------------------- */

static inline Fragment parse_scheme(const char *p)
{
    Fragment scheme;

    scheme.p = p;
    while (*p && *p != ':')
        p++;
    scheme.len = p - scheme.p;

    PR_ASSERT(scheme.len > 0);
    PR_ASSERT(scheme.p[scheme.len] == ':');

    return scheme;
}


/* ------------------------------ build_url ------------------------------- */

static inline char *build_url(Fragment *scheme, const char *host)
{
    int host_len = strlen(host);
    int url_len = scheme->len + 3 + host_len;

    char *url = (char *)MALLOC(url_len + 1);
    if (url) {
        char *p = url;

        memcpy(p, scheme->p, scheme->len);
        p += scheme->len;

        *p++ = ':';
        *p++ = '/';
        *p++ = '/';

        memcpy(p, host, host_len);
        p += host_len;

        *p = '\0';
    }

    return url;
}


/* --------------------------- hash_gateway_url --------------------------- */

static inline unsigned hash_gateway_url(const char *url)
{
    unsigned hash = 0;

    PR_ASSERT(!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8));

    while (*url) {
        hash = (hash << 5) ^ hash ^ ((unsigned char) *url);
        url++;
    }

    return hash;
}


/* ----------------------------- get_gateway ------------------------------ */

static Gateway * get_gateway(const char *host, int port, PRBool secure)
{
    Gateway *gateway = NULL;
    char *url = NULL;
    char *addr = NULL;
    char *host_port = NULL;
    Jroute jroute;
    PRLock *offline_lock = NULL;
    int default_port;
    int max_url_size;
    const char *prefix;
    unsigned hash;
    int i;
    unsigned char md5[MD5_SIZE];

    default_port = secure ? HTTPS_PORT : HTTP_PORT;

    if (port == 0)
        port = default_port;

    max_url_size = HTTPS_PREFIX_LEN + strlen(host) + sizeof(":65535");

    /*
     * Build the canonical URL
     */

    url = (char *)PERM_MALLOC(max_url_size);
    if (!url)
        goto get_gateway_short_circuit;

    prefix = secure ? HTTPS_PREFIX : HTTP_PREFIX;
    if (port == default_port) {
        util_snprintf(url, max_url_size, "%s%s", prefix, host);
    } else {
        util_snprintf(url, max_url_size, "%s%s:%d", prefix, host, port);
    }
    util_strlower(url);

    /*
     * Check for an existing Gateway object for this URL
     */

    hash = hash_gateway_url(url);
    i = hash % GATEWAY_URL_HT_SIZE;

    PR_Lock(_gateway_url_ht[i].lock);
    gateway = _gateway_url_ht[i].head;
    while (gateway) {
        if (!strcmp(url, gateway->url))
            break;
        gateway = gateway->next;
    }
    PR_Unlock(_gateway_url_ht[i].lock);

    if (gateway)
        goto get_gateway_short_circuit;

    /*
     * For consistency with Proxy 3.6, we never set a proxy-addr with an
     * http:// prefix and we always include the port number
     */

    addr = (char *)PERM_MALLOC(max_url_size);
    if (!addr)
        goto get_gateway_short_circuit;

    prefix = secure ? HTTPS_PREFIX : "";
    util_snprintf(addr, max_url_size, "%s%s:%d", prefix, host, port);

    /*
     * Format a value for the Host: header
     */

    host_port = (char *)PERM_MALLOC(max_url_size);
    if (!host_port)
        goto get_gateway_short_circuit;

    if (port == default_port) {
        util_snprintf(host_port, max_url_size, "%s", host);
    } else {
        util_snprintf(host_port, max_url_size, "%s:%d", host, port);
    }

    /*
     * Assign a unique ID based on the URL
     */

    PK11_HashBuf(SEC_OID_MD5, md5, (unsigned char *)url, strlen(url));
    url64_encode(md5, MD5_BYTES_FOR_JROUTE, jroute.chars, JROUTE_LEN);

    // XXX check for unique ID collisions

    /*
     * Allocate a lock to serialize access to offline_vsid
     */

    offline_lock = PR_NewLock();
    if (!offline_lock)
        goto get_gateway_short_circuit;

    /*
     * Create the Gateway object
     */

    gateway = (Gateway *)PERM_MALLOC(sizeof(Gateway));
    if (!gateway)
        goto get_gateway_short_circuit;

    gateway->url = url;
    gateway->url_len = strlen(url);
    gateway->addr = addr;
    gateway->addr_len = strlen(addr);
    gateway->host_port = host_port;
    gateway->host_port_len = strlen(host_port);
    gateway->jroute = jroute;
    gateway->offline_lock = offline_lock;
    gateway->offline_vsid = NULL;

    PR_Lock(_gateway_url_ht[i].lock);
    gateway->next = _gateway_url_ht[i].head;
    _gateway_url_ht[i].head = gateway;
    PR_Unlock(_gateway_url_ht[i].lock);

    log_error(LOG_VERBOSE, NULL, NULL, NULL,
              "configured server %s",
              gateway->addr);

    return gateway;

get_gateway_short_circuit:
    PERM_FREE(url);
    PERM_FREE(addr);
    PERM_FREE(host_port);
    if (offline_lock)
        PR_DestroyLock(offline_lock);

    return gateway;
}


/* -------------------------- add_server_gateway -------------------------- */

static int add_server_gateway(pblock *pb, pb_param *pp, void *data)
{
    Gateway *gateway = NULL;

    char *server = STRDUP(pp->value);
    if (!server)
        return -1;

    ParsedUrl url;
    if (url_parse(server, &url) == PR_SUCCESS &&
        (url.scheme == SCHEME_NONE ||
         url.scheme == SCHEME_HTTP ||
         url.scheme == SCHEME_HTTPS) &&
        url.host &&
        (!url.path || !strcmp(url.path, "/")) &&
        !url.user &&
        !url.password)
    {
        PRBool secure = (url.scheme == SCHEME_HTTPS);
        url.host[url.host_len] = '\0';
        gateway = get_gateway(url.host, url.port, secure);
    }

    FREE(server);

    if (!gateway) {
        log_error(LOG_FAILURE, pblock_findkeyval(pb_key_fn, pb), NULL, NULL,
                  XP_GetAdminStr(DBT_invalid_url_X), pp->value);
        return -1;
    }

    RouteConfig *route = (RouteConfig *)data;
    if (!route->gateways)
        return -1;

    route->gateways[route->num_gateways] = gateway;
    route->num_gateways++;

    return 0;
}


/* -------------------------- add_sticky_cookie --------------------------- */

static int add_sticky_cookie(pblock *pb, pb_param *pp, void *data)
{
    if (*pp->value) {
        char *sticky_cookie = STRDUP(pp->value);
        if (!sticky_cookie)
            return -1;

        RouteConfig *route = (RouteConfig *)data;
        if (!route->sticky_cookies)
            return -1;

        route->sticky_cookies[route->num_sticky_cookies] = sticky_cookie;
        route->num_sticky_cookies++;
    }

    return 0;
}


/* --------------------------- add_sticky_param --------------------------- */

static int add_sticky_param(pblock *pb, pb_param *pp, void *data)
{
    if (*pp->value) {
        char *sticky_param = STRDUP(pp->value);
        if (!sticky_param)
            return -1;

        RouteConfig *route = (RouteConfig *)data;
        if (!route->sticky_params)
            return -1;

        route->sticky_params[route->num_sticky_params] = sticky_param;
        route->num_sticky_params++;
    }

    return 0;
}


/* ------------------------- destroy_route_config ------------------------- */

static void destroy_route_config(RouteConfig *route)
{
    FREE(route->gateways);
    FREE(route->sticky_cookies);
    FREE(route->sticky_params);
    FREE(route->route_hdr);
    FREE(route->route_cookie);
    FREE(route);
}


/* ------------------------- create_route_config -------------------------- */

static RouteConfig * create_route_config(pblock *pb)
{
    const char *fn = pblock_findkeyval(pb_key_fn, pb);

    PR_ASSERT(_init_status == PR_SUCCESS);

    // Allocate the RouteConfig.  Note that we allocate from the permanent heap
    // when we're called from route_init_set_http_server() and from the session
    // pool when we're called from set_http_server().
    RouteConfig *route = (RouteConfig *)CALLOC(sizeof(RouteConfig));
    if (!route)
        return NULL;

    // pool should be non-NULL when the RouteConfig was allocated specifically
    // for this request
    route->pool = system_pool();

    // Count the number of "server" parameters
    int num_servers = foreach(pb, pb_key_server);

    // Legacy support for "hostname" and "port" parameters
    const char *hostname = pblock_findkeyval(pb_key_hostname, pb);
    const char *port = pblock_findkeyval(pb_key_port, pb);
    if (hostname && port)
        num_servers++;

    if (num_servers == 0) {
        // No route for you
        log_error(LOG_MISCONFIG, fn, NULL, NULL,
                  XP_GetAdminStr(DBT_need_server_or_hostname_and_port));
        destroy_route_config(route);
        return NULL;
    }

    route->gateways = (Gateway **)MALLOC(num_servers * sizeof(Gateway *));
    route->num_gateways = 0;

    // Lookup the Gateway * for each "server" parameter
    if (foreach(pb, pb_key_server, &add_server_gateway, route) != 0) {
        destroy_route_config(route);
        return NULL;
    }

    // Lookup the Gateway * for "hostname" and "port"
    if (hostname && port) {
        Gateway *gateway = get_gateway(hostname, atoi(port), PR_FALSE);
        if (!gateway) {
            destroy_route_config(route);
            return NULL;
        }
        route->gateways[route->num_gateways] = gateway;
        route->num_gateways++;
    }

    PR_ASSERT(route->num_gateways > 0);

    // Process "sticky-cookie" parameters
    int num_sticky_cookies = foreach(pb, pb_key_sticky_cookie);
    if (num_sticky_cookies > 0) {
        route->sticky_cookies = (char **)
            MALLOC(num_sticky_cookies * sizeof(char *));
        foreach(pb, pb_key_sticky_cookie, add_sticky_cookie, route);
    } else {
        route->sticky_cookies = (char **)MALLOC(2 * sizeof(char *));
        if (route->sticky_cookies) {
            char *cookie;
            if (cookie = STRDUP("JSESSIONID"))
                route->sticky_cookies[route->num_sticky_cookies++] = cookie;
            if (cookie = STRDUP("JSESSIONIDSSO"))
                route->sticky_cookies[route->num_sticky_cookies++] = cookie;
        }
    }
    
    // Process "sticky-param" parameters
    int num_sticky_params = foreach(pb, pb_key_sticky_param);
    if (num_sticky_params > 0) {
        route->sticky_params = (char **)
            MALLOC(num_sticky_params * sizeof(char *));
        foreach(pb, pb_key_sticky_param, add_sticky_param, route);
    } else {
        route->sticky_params = (char **)MALLOC(1 * sizeof(char *));
        if (route->sticky_params) {
            char *param;
            if (param = STRDUP("jsessionid"))
                route->sticky_params[route->num_sticky_params++] = param;
        }
    }

    // Process the "route-hdr" parameter
    if (char *route_hdr = pblock_findkeyval(pb_key_route_hdr, pb))
        route->route_hdr = STRDUP(route_hdr);

    // Process the "route-cookie" parameter
    if (char *route_cookie = pblock_findkeyval(pb_key_route_cookie, pb)) {
        route->route_cookie = STRDUP(route_cookie);
    }  else {
        route->route_cookie = STRDUP("JROUTE");
    }
    if (route->route_cookie)
        route->route_cookie_len = strlen(route->route_cookie);

    // Process the "rewrite-host" parameter
    if (pb_param *pp = pblock_findkey(pb_key_rewrite_host, pb)) {
        int t = util_getboolean(pp->value, -1);
        if (t == -1) {
            log_error(LOG_MISCONFIG, fn, NULL, NULL,
                      XP_GetAdminStr(DBT_invalid_X_value_Y_expected_boolean),
                      pp->name, pp->value);
            destroy_route_config(route);
            return NULL;
        }
        route->rewrite_host = t;
    }
    
    return route;
}


/* ---------------------- route_init_set_http_server ---------------------- */

extern "C" int route_init_set_http_server(const directive *dir,
                                          VirtualServer *incoming,
                                          const VirtualServer *current)
{
    // Preparse the route configuration
    RouteConfig *route = create_route_config(dir->param.pb);
    if (!route)
        return REQ_ABORTED;

    // Cache a pointer to the preparsed route configuration
    pblock_kvinsert(pb_key_magnus_internal,
                    (const char *)&route,
                    sizeof(route),
                    dir->param.pb);

    return REQ_PROCEED;
}


/* --------------------------- get_route_config --------------------------- */

static inline RouteConfig * get_route_config(pblock *pb)
{
    RouteConfig **proute;

    // Check for a preparsed route configuration
    proute = (RouteConfig **)pblock_findkeyval(pb_key_magnus_internal, pb);
    if (proute)
        return *proute;

    return NULL;
}


/* -------------------- route_destroy_set_http_server --------------------- */

extern "C" void route_destroy_set_http_server(const directive *dir,
                                              VirtualServer *outgoing)
{
    // Destroy the preparsed configuration we allocated earlier
    RouteConfig *route = get_route_config(dir->param.pb);
    if (route)
        destroy_route_config(route);
}


/* -------------------- route_request_slot_destructor --------------------- */

extern "C" void route_request_slot_destructor(void *data)
{
    // If the RouteConfig was allocated specifically for this request, free it
    RouteConfig *route = (RouteConfig *)data;
    if (route && route->pool)
        destroy_route_config(route);
}


/* ------------------------------ route_init ------------------------------ */

PRStatus route_init(void)
{
    PR_ASSERT(_init_status == PR_FAILURE);

    if (_init_status != PR_SUCCESS) {
        int i;

        _init_status = PR_SUCCESS;

        // We want a chance to preparse route configuration
        vs_directive_register_cb(route_set_origin_server,
                                 route_init_set_http_server,
                                 route_destroy_set_http_server);
        vs_directive_register_cb(route_set_proxy_server,
                                 route_init_set_http_server,
                                 route_destroy_set_http_server);

        // Allocate a slot to store a RouteConfig * per-Request
        _route_request_slot =
            request_alloc_slot(&route_request_slot_destructor);

        // Initialize the _gateway_url_ht hash table
        for (i = 0; i < GATEWAY_URL_HT_SIZE; i++) {
            _gateway_url_ht[i].lock = PR_NewLock();
            if (!_gateway_url_ht[i].lock)
                _init_status = PR_FAILURE;
        }
    }

    return _init_status;
}


/* --------------------------- gateway_offline ---------------------------- */

static PRBool gateway_offline(Session *sn, Request *rq, Gateway *gateway)
{
    if (gateway->offline_vsid)
        return PR_FALSE;

    const VirtualServer *vs = request_get_vs(rq);
    PR_ASSERT(vs != NULL);
    if (!vs)
        return PR_FALSE;

    char *vsid = PERM_STRDUP(vs_get_id(vs));
    if (!vsid)
        return PR_FALSE;

    PR_Lock(gateway->offline_lock);
    if (!gateway->offline_vsid) {
        gateway->offline_vsid = vsid;
        vsid = NULL;
    }
    PR_Unlock(gateway->offline_lock);

    if (vsid) {
        // Someone else noticed the server was offline first
        PERM_FREE(vsid);
        return PR_FALSE;
    }

    return PR_TRUE; // changed Gateway online/offline status
}


/* ---------------------------- gateway_online ---------------------------- */

static PRBool gateway_online(Gateway *gateway)
{
    if (!gateway->offline_vsid)
        return PR_FALSE;

    char *vsid;

    PR_Lock(gateway->offline_lock);
    vsid = gateway->offline_vsid;
    gateway->offline_vsid = NULL;
    PR_Unlock(gateway->offline_lock);

    if (!vsid)
        return PR_FALSE;

    PERM_FREE(vsid);

    return PR_TRUE; // changed Gateway online/offline status
}


/* ---------------------------- route_offline ----------------------------- */

int route_offline(Session *sn,
                  Request *rq,
                  char **proxy_addr,
                  char **origin_addr)
{
    if (proxy_addr && *proxy_addr) {
        // Route previously told caller to talk to an (offline) proxy
        PR_ASSERT(strcmp(*proxy_addr, ROUTE_PROXY_ADDR_DIRECT));
        pblock_kvinsert(pb_key_offline_proxy_addr,
                        *proxy_addr,
                        strlen(*proxy_addr),
                        rq->vars);

    } else if (origin_addr && *origin_addr) {
        // Route previously told caller to talk to an (offline) origin server
        pblock_kvinsert(pb_key_offline_origin_addr,
                        *origin_addr,
                        strlen(*origin_addr),
                        rq->vars);

    } else {
        // Route didn't select any server, so there's no need to tell Route
        // that the server is offline
        return REQ_NOACTION;
    }

    while (pb_param *pp = pblock_removekey(pb_key_proxy_addr, rq->vars))
        param_free(pp);

    while (pb_param *pp = pblock_removekey(pb_key_origin_addr, rq->vars))
        param_free(pp);

    // Rerun Route.  This gives the guilty Route SAF a chance to 1. mark the
    // offline server as offline and 2. suggest a server that's online.
    int res = servact_route(sn, rq);
    if (res != REQ_PROCEED)
        return res;

    *proxy_addr = route_get_proxy_addr(sn, rq);
    *origin_addr = route_get_origin_addr(sn, rq);

    return REQ_PROCEED;
}


/* -------------------------- check_http_server --------------------------- */

static void check_http_server(const VirtualServer *vs, Gateway *gateway)
{
    PRFileDesc *csd = NULL;
    Session *sn = NULL;
    Request *rq = NULL;
    pblock *opb = NULL;
    httpd_object *obj = NULL;
    pblock *dpb = NULL;
    pblock *fpb = NULL;
    PRBool online;
    int res;
    int rv;

    // We don't expect to be called from a DaemonSession thread
    PR_ASSERT(system_pool() == NULL);

    // Setup the thread-specific "globals" for this VS
    conf_set_thread_vs_globals(vs);

    // Create a dummy client socket descriptor
    csd = PR_NewSink();

    // Create a dummy Session
    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    sn = session_create(csd, &address);
    if (!sn)
        goto check_http_server_done;

    // Create a dummy Request
    // XXX make default method configurable
    // XXX make default URL configurable
    // XXX make default protocol version configurable
    rq = request_restart_internal(gateway->url, NULL);
    if (!rq)
        goto check_http_server_done;
    rq->method_num = METHOD_OPTIONS;
    rq->protv_num = PROTOCOL_VERSION_HTTP11;
    pblock_kvreplace(pb_key_method, "OPTIONS", 7, rq->reqpb);
    pblock_kvreplace(pb_key_protocol, "HTTP/1.1", 8, rq->reqpb);
    pblock_kvreplace(pb_key_host,
                     gateway->host_port,
                     gateway->host_port_len,
                     rq->headers);

    // Provide hints to interested parties that this is a health check request
    PR_ASSERT(INTERNAL_REQUEST(rq));
    pblock_kvinsert(pb_key_check_http_server, "1", 1, rq->reqpb);
    pblock_kvinsert(pb_key_proxy_ping, "true", 4, rq->headers);

    log_error(LOG_VERBOSE, CHECK_HTTP_SERVER, sn, rq,
              "checking server %s",
              gateway->addr);

    // Default to quick service-http timeouts (do this before running
    // ObjectType so the administrator can override)
    httpclient_set_connect_timeout(sn, rq, PR_SecondsToInterval(5));
    httpclient_set_timeout(sn, rq, PR_SecondsToInterval(5));

    // There's no client, so don't send a Client-ip: header
    httpclient_block_ip(NULL, sn, rq);

    // Run through AuthTrans, NameTrans, PathCheck, and ObjectType to give the
    // administrator a chance to customize the health check.  We're only doing
    // this to pick up options (e.g. ssl-client-config client-cert-nickname),
    // so don't give up if any individual stage reports an error.
    rv = servact_uri2path(sn, rq);
    if (!rq->os || rv == REQ_ABORTED) {
        log_error(LOG_VERBOSE, CHECK_HTTP_SERVER, sn, rq,
                 "checking server %s failed.", gateway->url);
        goto check_http_server_done;
    }

    servact_pathchecks(sn, rq);
    servact_fileinfo(sn, rq);

    // Force service-http not to retry
    httpclient_set_retries(sn, rq, 0);

    // Create a new object
    opb = pblock_create(1);
    if (!opb)
        goto check_http_server_done;
    pblock_kvinsert(pb_key_name,
                    MAGNUS_INTERNAL_FORCE_HTTP_ROUTE,
                    MAGNUS_INTERNAL_FORCE_HTTP_ROUTE_LEN,
                    opb);
    obj = object_create(NUM_DIRECTIVES, opb);
    if (!obj)
        goto check_http_server_done;
    opb = NULL;

    // Add a Route fn="magnus-internal/force-http-route" directive that will
    // force service-http to route the request to the server we want to check
    dpb = pblock_create(3);
    if (!dpb)
        goto check_http_server_done;
    pblock_kvinsert(pb_key_fn,
                    MAGNUS_INTERNAL_FORCE_HTTP_ROUTE,
                    MAGNUS_INTERNAL_FORCE_HTTP_ROUTE_LEN,
                    dpb);
    pblock_kvinsert(pb_key_proxy_addr,
                    ROUTE_PROXY_ADDR_DIRECT,
                    ROUTE_PROXY_ADDR_DIRECT_LEN,
                    dpb);
    pblock_kvinsert(pb_key_origin_addr,
                    gateway->addr,
                    gateway->addr_len,
                    dpb);
    object_add_directive(NSAPIRoute, dpb, NULL, obj);
    dpb = NULL;

    // Add our object to the original request's objset
    if (!rq->tmpos)
        rq->tmpos = objset_create();
    if (!rq->tmpos)
        goto check_http_server_done;
    objset_add_object(obj, rq->tmpos);
    objset_add_object(obj, rq->os);
    obj = NULL;

    // Run service-http
    fpb = pblock_create(3);
    if (!fpb)
        goto check_http_server_done;
    pblock_kvinsert(pb_key_fn, "service-http", 12, fpb);
    res = func_exec(fpb, sn, rq);

    // Run Output to give the administrator a chance to influence the offline
    // server determination, for example:
    // <Client code="500">
    // Output fn="route-offline"
    // </Client>
    servact_output(sn, rq);

    // Did service-http successfully contact the Gateway?
    online = PR_FALSE;
    if (res == REQ_PROCEED) {
        if (rq->status_num < 200 || rq->status_num > 999) {
            log_error(LOG_VERBOSE, CHECK_HTTP_SERVER, sn, rq,
                      "received invalid HTTP response code %d from server %s",
                      rq->status_num, gateway->addr);
        } else if (rq->status_num == PROTOCOL_BAD_GATEWAY ||
                   rq->status_num == PROTOCOL_REQUEST_TIMEOUT)
        {
            log_error(LOG_VERBOSE, CHECK_HTTP_SERVER, sn, rq,
                      "received unexpected HTTP response code %d from server %s",
                      rq->status_num, gateway->addr);
        } else if (!pblock_findkey(pb_key_offline_origin_addr, rq->vars) &&
                   !pblock_findkey(pb_key_offline_proxy_addr, rq->vars) &&
                   !pblock_findkey(pb_key_route_offline, rq->vars))
        {
            log_error(LOG_VERBOSE, CHECK_HTTP_SERVER, sn, rq,
                      "received HTTP response code %d from server %s",
                      rq->status_num, gateway->addr);
            online = PR_TRUE;
        }
    }

    if (online) {
        int degree = LOG_VERBOSE;
        if (gateway_online(gateway))
            degree = LOG_INFORM;
        log_error(degree, CHECK_HTTP_SERVER, sn, rq,
                  XP_GetAdminStr(DBT_server_X_online),
                  gateway->addr);
    } else {
        int degree = LOG_VERBOSE;
        if (gateway_offline(sn, rq, gateway))
            degree = LOG_INFORM;
        log_error(degree, CHECK_HTTP_SERVER, sn, rq,
                  XP_GetAdminStr(DBT_server_X_offline),
                  gateway->addr);
    }

check_http_server_done:
    if (fpb)
        pblock_free(fpb);
    if (dpb)
        pblock_free(dpb);
    if (obj)
        object_free(obj);
    if (opb)
        pblock_free(opb);

    // Discard the request
    if (rq)
        request_free(rq);

    // Discard the session
    if (sn) {
        if (sn->csd && sn->csd_open)
            PR_Close(sn->csd);
        csd = NULL;
        session_free(sn);
    }

    if (csd)
        PR_Close(csd);
}


/* ---------------------- check_offline_http_server ----------------------- */

static void check_offline_http_server(Gateway *gateway)
{
    // Check if the Gateway is offline
    char *offline_vsid;
    PR_Lock(gateway->offline_lock);
    if (gateway->offline_vsid) {
        offline_vsid = PERM_STRDUP(gateway->offline_vsid);
    } else {
        offline_vsid = NULL;
    }
    PR_Unlock(gateway->offline_lock);

    // If the Gateway is offline...
    if (offline_vsid) {
        Configuration *configuration = ConfigurationManager::getConfiguration();
        if (configuration) {
            const VirtualServer *vs = configuration->getVS(offline_vsid);
            if (vs) {
                // Check the Gateway
                check_http_server(vs, gateway);
            } else {
                // Uh, the VS disappeared.  Mark the Gateway as online.  If
                // it's actually still offline, it will probably get marked
                // offline in another VS.
                gateway_online(gateway);
            }
            configuration->unref();
        }
        PERM_FREE(offline_vsid);
    }
}    


/* ------------------------- route_offline_thread ------------------------- */

extern "C" void route_offline_thread(void *arg)
{
    // XXX make interval configurable
    PRIntervalTime offline_check_interval = PR_SecondsToInterval(30);

    time_t prev_set_http_server_time = (time_t) -1;

    for (;;) {
        PRIntervalTime epoch = ft_timeIntervalNow();

        if (prev_set_http_server_time != _set_http_server_time) {
            prev_set_http_server_time = _set_http_server_time;

            for (int i = 0; i < GATEWAY_URL_HT_SIZE; i++) {
                Gateway *gateway;

                PR_Lock(_gateway_url_ht[i].lock);
                gateway = _gateway_url_ht[i].head;
                PR_Unlock(_gateway_url_ht[i].lock);

                while (gateway) {
                    if (gateway->offline_vsid)
                        check_offline_http_server(gateway);

                    if (WebServer::isTerminating())
                        break;

                    PR_Lock(_gateway_url_ht[i].lock);
                    gateway = gateway->next;
                    PR_Unlock(_gateway_url_ht[i].lock);
                }
            }
        }

        PRIntervalTime elapsed = (ft_timeIntervalNow() - epoch);
        if (elapsed < offline_check_interval) {
            PRIntervalTime remaining = offline_check_interval - elapsed;
            systhread_sleep(PR_IntervalToMilliseconds(remaining));
        }
    }
}


/* --------------------------- route_init_late ---------------------------- */

PRStatus route_init_late(void)
{
    _offline_thread = PR_CreateThread(PR_SYSTEM_THREAD,
                                      route_offline_thread,
                                      0,
                                      PR_PRIORITY_NORMAL,
                                      PR_GLOBAL_THREAD,
                                      PR_UNJOINABLE_THREAD,
                                      0);
    if (!_offline_thread) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_error_creating_thread_because_X),
                system_errmsg());
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}


/* ------------------------- route_get_proxy_addr ------------------------- */

char * route_get_proxy_addr(Session *sn, Request *rq)
{
    char *proxy_addr;

    // Should the request be routed through a proxy?
    proxy_addr = pblock_findkeyval(pb_key_proxy_addr, rq->vars);
    if (proxy_addr && !strcmp(proxy_addr, ROUTE_PROXY_ADDR_DIRECT))
        proxy_addr = NULL;

    return proxy_addr;
}


/* ------------------------ route_get_origin_addr ------------------------- */

char * route_get_origin_addr(Session *sn, Request *rq)
{
    char *origin_addr;

    // Should the request be routed directly to a specific origin server?
    origin_addr = pblock_findkeyval(pb_key_origin_addr, rq->vars);

    PR_ASSERT(!origin_addr || !route_get_proxy_addr(sn, rq));

    return origin_addr;
}


/* ------------------------- route_get_socks_addr ------------------------- */

char * route_get_socks_addr(Session *sn, Request *rq)
{
    char *socks_addr;

    // Should the request be routed through a SOCKS server?
    socks_addr = pblock_findkeyval(pb_key_socks_addr, rq->vars);
    if (socks_addr && !strcmp(socks_addr, ROUTE_SOCKS_ADDR_DIRECT))
        socks_addr = NULL;

    return socks_addr;
}


/* ------------------------- route_route_offline -------------------------- */

int route_route_offline(pblock *pb, Session *sn, Request *rq)
{
    char *proxy_addr = route_get_proxy_addr(sn, rq);
    char *origin_addr = route_get_origin_addr(sn, rq);

    route_offline(sn, rq, &proxy_addr, &origin_addr);

    return REQ_NOACTION;
}


/* ------------------------ route_set_actual_route ------------------------ */

void route_set_actual_route(Session *sn, Request *rq)
{
    char *socks_addr = pblock_findkeyval(pb_key_socks_addr, rq->vars);
    char *proxy_addr = route_get_proxy_addr(sn, rq);
    char *origin_addr = route_get_origin_addr(sn, rq);

    if (!socks_addr && !proxy_addr && !origin_addr) {
        pblock_kvinsert(pb_key_actual_route, DIRECT, DIRECT_LEN, rq->vars);
        return;
    }

    int len = 0;
    if (socks_addr)
        len += SOCKS_LEN + 1 + strlen(socks_addr) + 2; // "SOCKS(x)-"
    if (proxy_addr)
        len += PROXY_LEN + 1 + strlen(proxy_addr) + 1; // "PROXY(x)"
    if (origin_addr)
        len += ORIGIN_LEN + 1 + strlen(origin_addr) + 1; // "ORIGIN(x)"

    pb_param *pp = pblock_key_param_create(rq->vars,
                                           pb_key_actual_route,
                                           NULL,
                                           len);
    if (pp) {
        int size = len + 1;
        if (socks_addr) {
            if (proxy_addr) {
                // SOCKS(x)-PROXY(x)
                PR_snprintf(pp->value,
                            size,
                            SOCKS"(%s)-"PROXY"(%s)",
                            socks_addr,
                            proxy_addr);
            } else if (origin_addr) {
                // SOCKS(x)-ORIGIN(x)
                PR_snprintf(pp->value,
                            size,
                            SOCKS"(%s)-"ORIGIN"(%s)",
                            socks_addr,
                            origin_addr);
            } else {
                // SOCKS(x)
                PR_snprintf(pp->value, size, SOCKS"(%s)", socks_addr);
            }
        } else if (proxy_addr) {
            // PROXY(x)
            PR_snprintf(pp->value, size, PROXY"(%s)", proxy_addr);
        } else {
            // ORIGIN(x)
            PR_snprintf(pp->value, size, ORIGIN"(%s)", origin_addr);
        }
        pblock_kpinsert(pb_key_actual_route, pp, rq->vars);
    }
}


/* ------------------------------- matches -------------------------------- */

static inline PRBool matches(const char *name, char **names, int num_names)
{
    for (int i = 0; i < num_names; i++) {
        const char *expected = names[i];
        const char *provided = name;
        while (*expected && *provided == *expected) {
            expected++;
            provided++;
        }
        if (!*expected && *provided == '=')
            return PR_TRUE;
    }
    return PR_FALSE;
}


/* ------------------------- rewrite_route_cookie ------------------------- */

static int rewrite_route_cookie(pblock *pb, pb_param *pp, void *data)
{
    RouteCookieStatus *status = (RouteCookieStatus *)data;
    RouteConfig *route = status->route;

    // Get the cookie name
    char *cookie_name = pp->value;
    while (isspace(*cookie_name))
        cookie_name++;

    status->num_cookies++;

    // Skip non-route cookies
    if (strncmp(cookie_name, route->route_cookie, route->route_cookie_len) ||
        cookie_name[route->route_cookie_len] != '=')
    {
        return 0;
    }

    status->num_route_cookies++;

    pool_handle_t *pool = pblock_pool(pb);

    // Get the route cookie value and attributes
    char *cookie_suffix = cookie_name + route->route_cookie_len + 1;

    // Remember that we have a route cookie for these attributes (e.g. path)
    const char *cookie_attributes = strchr(cookie_suffix, ';');
    if (!cookie_attributes)
        cookie_attributes = "";
    if (!status->attributes) {
        status->attributes = pblock_create_pool(pool, 7);
        if (!status->attributes)
            return -1;
    }
    pblock_nvinsert(cookie_attributes, "", status->attributes);

    // Allocate a new cookie buffer where we can prepend the downstream
    // server's jroute
    char *new_cookie = (char *)pool_malloc(pool,
                                           route->route_cookie_len + 1 +
                                           status->jroute_len + 1 +
                                           strlen(cookie_suffix) + 1);
    if (!new_cookie)
        return -1;

    // Reformat the route cookie
    int pos = 0;
    memcpy(new_cookie + pos, route->route_cookie, route->route_cookie_len);
    pos += route->route_cookie_len;
    new_cookie[pos++] = '=';
    memcpy(new_cookie + pos, status->jroute, status->jroute_len);
    pos += status->jroute_len;
    new_cookie[pos++] = '.';
    strcpy(new_cookie + pos, cookie_suffix);

    log_error(LOG_VERBOSE, NULL, NULL, NULL,
              "rewrote \"%s: %s\" to \"%s: %s\"",
              pp->name, pp->value, pp->name, new_cookie);

    // Replace the route cookie
    pool_free(pool, pp->value);
    pp->value = new_cookie;

    return 0;
}


/* --------------------------- add_route_cookie --------------------------- */

static int add_route_cookie(pblock *pb, pb_param *pp, void *data)
{
    RouteCookieStatus *status = (RouteCookieStatus *)data;
    RouteConfig *route = status->route;

    // Get the cookie name
    char *cookie_name = pp->value;
    while (isspace(*cookie_name))
        cookie_name++;

    // Skip non-sticky cookies
    if (!matches(cookie_name,
                 route->sticky_cookies,
                 route->num_sticky_cookies))
    {
        return 0;
    }

    // Get the sticky cookie's attributes (e.g. path)
    const char *cookie_attributes = strchr(cookie_name + 1, ';');
    if (!cookie_attributes)
        cookie_attributes = "";

    // Skip sticky cookies that already have a corresponding route cookie
    if (status->attributes &&
        pblock_find(cookie_attributes, status->attributes))
    {
        return 0;
    }

    /*
     * At this point, we've found a fresh sticky cookie (that is, a sticky
     * cookie without a corresponding route cookie).  We need to generate a
     * route cookie.
     */

    // Allocate a new Set-cookie: header parameter for the route cookie
    pp = pblock_key_param_create(pb,
                                 pb_key_set_cookie,
                                 NULL, 
                                 route->route_cookie_len + 1 +
                                 status->jroute_len + 1 +
                                 strlen(cookie_attributes));
    if (!pp)
        return -1;

    // Format the new route cookie
    int pos = 0;
    memcpy(pp->value, route->route_cookie, route->route_cookie_len);
    pos += route->route_cookie_len;
    pp->value[pos++] = '=';
    memcpy(pp->value + pos, status->jroute, status->jroute_len);
    pos += status->jroute_len;
    strcpy(pp->value + pos, cookie_attributes);

    // Add the route cookie to the response
    pblock_kpinsert(pb_key_set_cookie, pp, pb);

    status->num_route_cookies++;

    return 0;
}


/* ------------------------ route_process_cookies ------------------------- */

/*
 * XXX
 *
 * This function is called by the Service SAF to add a route cookie to
 * rq->srvhdrs based on the sticky cookie settings established earlier by
 * set-origin-server.  That sucks.
 *
 * Instead, an Output SAF - its directive implicitly added by set-origin-server
 * - could perform this function.
 */

PRBool route_process_cookies(Session *sn, Request *rq)
{
    RouteConfig *route = (RouteConfig *)
        request_get_data(rq, _route_request_slot);
    if (!route ||
        route->num_gateways < 2 ||
        route->route_cookie_len == 0 ||
        route->num_sticky_cookies == 0)
        return PR_FALSE;

    const char *jroute = pblock_findkeyval(pb_key_jroute, rq->vars);
    if (!jroute)
        return PR_FALSE;

    // Initialize route cookie processing status
    RouteCookieStatus status;
    status.route = route;
    status.jroute = jroute;
    status.jroute_len = strlen(jroute);
    status.attributes = NULL;
    status.num_cookies = 0;
    status.num_route_cookies = 0;

    // Rewrite existing route cookies
    foreach(rq->srvhdrs, pb_key_set_cookie, rewrite_route_cookie, &status);

    // Add new route cookies for fresh sticky cookies
    if (status.num_cookies > status.num_route_cookies)
        foreach(rq->srvhdrs, pb_key_set_cookie, add_route_cookie, &status);

    if (status.attributes)
        pblock_free(status.attributes);

    return (status.num_route_cookies > 0);
}


/* ----------------------------- parse_jroute ----------------------------- */

static inline PRStatus parse_jroute(const char *string, Jroute *jroute)
{
    int j;

    for (j = 0; j < JROUTE_LEN; j++) {
        if (isprint(*string)) {
            jroute->chars[j] = *string;
            string++;
        } else {
            jroute->chars[j] = '\0';
        }
    }

    return (j == JROUTE_LEN) ? PR_SUCCESS : PR_FAILURE;
}


/* -------------------------- extract_uri_jroute -------------------------- */

static PRBool extract_uri_jroute(RouteConfig *route,
                                 const char *uri,
                                 const char *upstream_jroute,
                                 Jroute *extracted_jroute)
{
    // Look for a sticky parameter in the URI
    const char *request_jroute = NULL;
    while (uri && (uri = strchr(uri, ';')) != NULL) {
        uri++;
        if (matches(uri, route->sticky_params, route->num_sticky_params)) {
            // The jroute follows the colon in a sticky parameter
            while (*uri && *uri != '=' && *uri != ';' && *uri != '/')
                uri++;
            while (*uri && *uri != ':' && *uri != ';' && *uri != '/')
                uri++;
            if (*uri == ':')
                request_jroute = uri + 1;
        }
    }

    // If there was a jroute encoded in the sticky parameter...
    if (request_jroute) {
        // How many jroute-aware proxies has the request passed through so far?
        int jroute_depth = 0;
        if (upstream_jroute) {
            const char *p = upstream_jroute;
            jroute_depth++;
            while (*p) {
                if (*p == '.')
                    jroute_depth++;
                p++;
            }
        }

        // Traverse the requested jroute (obtained from a sticky parameter) to
        // figure out the next server the request should be routed to
        const char *next_jroute = request_jroute;
        int i;
        for (i = 0; i < jroute_depth; i++) {
            while (*next_jroute &&
                   *next_jroute != '.' &&
                   *next_jroute != ';' &&
                   *next_jroute != '/')
            {
                next_jroute++;
            }
            if (*next_jroute != '.')
                break;
            next_jroute++;
        }
        if (i == jroute_depth) {
            // Extract the jroute for the next hop
            parse_jroute(next_jroute, extracted_jroute);
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}


/* ------------------------ extract_cookie_jroute ------------------------- */

static PRBool extract_cookie_jroute(Session *sn,
                                    Request *rq,
                                    RouteConfig *route,
                                    char *cookie,
                                    Jroute *extracted_jroute)
{
    // Look for a route cookie
    while (cookie) {
        // Advance to cookie name
        while (isspace(*cookie))
            cookie++;

        // Nul terminate cookie value
        char *next = strpbrk(cookie, ";,");
        char separator;
        if (next) {
            separator = *next;
            *next = '\0';
        }

        // If this is a valid route cookie...
        char *equalsign = strchr(cookie, '=');
        if (equalsign &&
            equalsign - cookie == route->route_cookie_len &&
            !memcmp(cookie, route->route_cookie, route->route_cookie_len))
        {
            // Extract the jroute for the next hop
            if (parse_jroute(equalsign + 1, extracted_jroute) == PR_SUCCESS) {
                // If there is more than one jroute in this cookie...
                char *dot = strchr(equalsign, '.');
                if (dot) {
                    // Rearrange the cookie: JROUTE=aa.bb.cc -> JROUTE=bb.cc.aa
                    char *value = equalsign + 1;
                    int value_len = strlen(value);
                    if (dot - value == JROUTE_LEN) {
                        memcpy(value, dot + 1, value_len - JROUTE_LEN - 1);
                        value[value_len - JROUTE_LEN - 1] = '.';
                        memcpy(value + value_len - JROUTE_LEN,
                               extracted_jroute,
                               JROUTE_LEN);

                        // Force the HTTP client to use our reformatted cookie
                        httpclient_reformat_request_headers(sn, rq);
                    }
                }

                // Restore cookie separator
                if (next)
                    *next = separator;

                return PR_TRUE;
            }
        }

        // Restore cookie separator
        if (next) {
            *next = separator;
            next++;
        }

        cookie = next;
    }

    return PR_FALSE;
}


/* -------------------------- get_request_jroute -------------------------- */

static inline PRBool get_request_jroute(Session *sn,
                                        Request *rq,
                                        RouteConfig *route,
                                        const char *upstream_jroute,
                                        Jroute *extracted_jroute)
{
    if (route->num_gateways < 2)
        return PR_FALSE;

    // Check to see if someone already extracted the requested jroute
    char *request_jroute = pblock_findkeyval(pb_key_request_jroute, rq->vars);
    if (request_jroute) {
        parse_jroute(request_jroute, extracted_jroute);
        return PR_TRUE;
    }

    PRBool extracted = PR_FALSE;

    // Look for a sticky parameter in the URI
    if (route->num_sticky_params) {
        const char *uri = pblock_findkeyval(pb_key_escaped, rq->vars);
        if (!uri)
            uri = pblock_findkeyval(pb_key_path, rq->vars);
        if (extract_uri_jroute(route, uri, upstream_jroute, extracted_jroute))
            extracted = PR_TRUE;
    }

    // Look for a route cookie in the headers
    if (route->route_cookie_len) {
        if (char *cookie = pblock_findkeyval(pb_key_cookie, rq->headers)) {
            if (extract_cookie_jroute(sn, rq, route, cookie, extracted_jroute))
                extracted = PR_TRUE;
        }
    }

    // Remember the requested jroute (in case we need to rerun Route later)
    if (extracted) {
        pblock_kvinsert(pb_key_request_jroute,
                        extracted_jroute->chars,
                        JROUTE_LEN,
                        rq->vars);
    }

    return extracted;
}


/* ------------------------------ jroutecmp ------------------------------- */

static inline int jroutecmp(Jroute &a, Jroute &b)
{
    int j;

    for (j = 0; j < JROUTE_WORDS; j++) {
        if (int diff = a.words[j] - b.words[j])
            return diff;
    }

    return 0;
}


/* ---------------------------- choose_gateway ---------------------------- */

static inline Gateway * choose_gateway(Session *sn,
                                       RouteConfig *route,
                                       Gateway *preferred_offline_gateway)
{
    if (route->num_gateways == 0)
        return NULL;

    // Look for an online gateway
    unsigned counter = route->choose_gateway_counter++;
    unsigned noise = (unsigned) (size_t) sn; // "noise" that varies by CPU
    int i = (counter + noise) % route->num_gateways;
    int num_gateways_remaining = route->num_gateways;
    while (num_gateways_remaining--) {
        if (!route->gateways[i]->offline_vsid)
            return route->gateways[i];
        i--;
        if (i < 0)
            i = route->num_gateways - 1;
    }

    // No online gateways, use an offline gateway
    if (preferred_offline_gateway)
        return preferred_offline_gateway;
    return route->gateways[i];
}


/* --------------------------- set_http_server ---------------------------- */

static int set_http_server(pblock *pb,
                           Session *sn,
                           Request *rq,
                           const char *fn,
                           const pb_key *addr_key,
                           const pb_key *offline_addr_key)
{
    // Let route_offline_thread() know that the server isn't idle
    _set_http_server_time = ft_time();

    // Get the preparsed route configuration
    RouteConfig *non_preparsed_route = NULL;
    RouteConfig *route = get_route_config(pb);
    if (!route) {
        // No preparsed route configuration; someone called us directly?
        non_preparsed_route = create_route_config(pb);
        route = non_preparsed_route;
    }
    if (!route)
        return REQ_ABORTED;

    PR_ASSERT(route->num_gateways > 0);

    // offline-(proxy|origin)-addr is set when a server previously specified by
    // (proxy|origin)-addr is found to be offline during the Service stage
    const char *offline_addr = pblock_findkeyval(offline_addr_key, rq->vars);
    if (offline_addr) {
        // Look for a gateway that matches the offline-(proxy|origin)-addr
        for (int i = 0; i < route->num_gateways; i++) {
            if (!strcmp(route->gateways[i]->addr, offline_addr)) {
                // Mark gateway offline
                if (gateway_offline(sn, rq, route->gateways[i])) {
                    log_error(LOG_INFORM, fn, sn, rq,
                              XP_GetAdminStr(DBT_server_X_offline),
                              route->gateways[i]->addr);
                }

                // The offline-proxy-addr is one of ours
                param_free(pblock_removekey(offline_addr_key, rq->vars));
                break;
            }
        }
    }

    // Check for a Proxy-jroute value from an upstream proxy
    const char *upstream_jroute = pblock_findkeyval(pb_key_upstream_jroute,
                                                    rq->vars);
    if (!upstream_jroute) {
        // Inspect rq->headers for a Proxy-jroute header
        if (route->route_hdr) {
            upstream_jroute = pblock_findval(route->route_hdr, rq->headers);
        } else {
            upstream_jroute = pblock_findkeyval(pb_key_proxy_jroute, rq->headers);
        }

        // Save the old Proxy-jroute header for later (in case we need to rerun
        // Route after someone's removed Proxy-jroute from rq->headers)
        if (upstream_jroute) {
            pblock_kvinsert(pb_key_upstream_jroute,
                            upstream_jroute,
                            strlen(upstream_jroute),
                            rq->vars);
        }
    }
        
    Gateway *gateway = NULL;
    Gateway *sticky_gateway = NULL;

    // If this is a sticky request...
    Jroute request_jroute;
    if (get_request_jroute(sn, rq, route, upstream_jroute, &request_jroute)) {
        // Look for a gateway that matches the request's jroute
        for (int i = 0; i < route->num_gateways; i++) {
            if (!jroutecmp(request_jroute, route->gateways[i]->jroute)) {
                log_error(LOG_VERBOSE, fn, sn, rq,
                          "received sticky request for server %s",
                          route->gateways[i]->addr);
                if (!route->gateways[i]->offline_vsid)
                    gateway = route->gateways[i];
                sticky_gateway = route->gateways[i];
                break;
            }
        }
        if (!sticky_gateway) {
            log_error(LOG_WARN, fn, sn, rq,
                      XP_GetAdminStr(DBT_sticky_req_for_unknown_jroute_X),
                      JROUTE_LEN, request_jroute.chars);
        }
    }

    // If we haven't decided on a gateway yet...
    if (!gateway) {
        Gateway *preferred_offline_gateway = NULL;
        if (!offline_addr) {
            // On the first attempt (offline_addr == NULL), we give preference
            // to the sticky gateway when all gateways are offline.  On any
            // subsequent attempts (offline_addr != NULL), we'll try any
            // gateway because we know that the sticky gateway really is still
            // offline.
            preferred_offline_gateway = sticky_gateway;
        }
        gateway = choose_gateway(sn, route, preferred_offline_gateway);
    }

    // Set things up for the Service SAF
    if (gateway) {
        if (sticky_gateway && gateway != sticky_gateway) {
            log_error(LOG_WARN, fn, sn, rq,
                      XP_GetAdminStr(DBT_sticky_req_for_offline_X_using_Y),
                      sticky_gateway->addr,
                      gateway->addr);
        } else {
            log_error(LOG_VERBOSE, fn, sn, rq,
                      "using server %s",
                      gateway->addr);
        }

        // Set proxy-addr or origin-addr
        PR_ASSERT(!pblock_findkey(addr_key, rq->vars));
        pblock_kvinsert(addr_key, gateway->addr, gateway->addr_len, rq->vars);

        // Stash a pointer to the RouteConfig, discarding any old RouteConfig
        if (void *old = request_set_data(rq, _route_request_slot, route))
            route_request_slot_destructor(old);
        non_preparsed_route = NULL;

        // Record jroute for the request's Proxy-jroute: header and/or the
        // response's route cookie
        if (upstream_jroute || route->num_gateways > 1) {
            if (route->route_hdr)
                httpclient_set_jroute_header(sn, rq, route->route_hdr);
            pblock_kvinsert(pb_key_jroute,
                            gateway->jroute.chars,
                            JROUTE_LEN,
                            rq->vars);
        }

        // Add reverse mappings from gateway->url to our URL
        char *our_url = http_uri2url_dynamic("", "", sn, rq);
        reverse_map_add(sn, rq, gateway->url, our_url);

        // If we're going to tell the gateway the original Host: header...
        if (!route->rewrite_host) {
            // If we'll use a different protocol to talk to the gateway than
            // the client is using to talk to us...
            Fragment gateway_scheme = parse_scheme(gateway->url);
            Fragment our_scheme = parse_scheme(our_url);
            if (fragmentcasecmp(&gateway_scheme, &our_scheme)) {
                // In case the origin server uses the original Host: header to
                // construct self-referencing URLs, add a reverse mapping that
                // rewrites the gateway's scheme to our own
                const char *host = pblock_findkeyval(pb_key_host, rq->headers);
                if (host) {
                    char *wrong_scheme_url = build_url(&gateway_scheme, host);
                    if (wrong_scheme_url)
                        reverse_map_add(sn, rq, wrong_scheme_url, our_url);
                }
            }
        }

        // Process the generic "rewrite-" parameters
        reverse_map_set_headers(pb, sn, rq);

        // Rewrite Host:
        if (route->rewrite_host) {
            param_free(pblock_removekey(pb_key_host, rq->headers));
            pblock_kvinsert(pb_key_host,
                            gateway->host_port,
                            gateway->host_port_len,
                            rq->headers);
            httpclient_reformat_request_headers(sn, rq);
        }
    }

    if (non_preparsed_route)
        destroy_route_config(non_preparsed_route);

    return REQ_PROCEED;
}


/* ----------------------- route_set_origin_server ------------------------ */

int route_set_origin_server(pblock *pb, Session *sn, Request *rq)
{
    // Because the server begins executing Route SAFs in the most specific
    // object and works its way down to the least specific, we should not
    // override an existing route
    if (pblock_findkey(pb_key_origin_addr, rq->vars))
        return REQ_NOACTION; // origin-addr already set
    if (char *proxy_addr = pblock_findkeyval(pb_key_proxy_addr, rq->vars)) {
        if (strcmp(proxy_addr, ROUTE_PROXY_ADDR_DIRECT))
            return REQ_NOACTION; // proxy-addr already set
    } else {
        // Set proxy-addr to "DIRECT" since we're going to talk directly to an
        // origin server
        pblock_kvinsert(pb_key_proxy_addr,
                        ROUTE_PROXY_ADDR_DIRECT,
                        ROUTE_PROXY_ADDR_DIRECT_LEN,
                        rq->vars);
    }

    // Set origin-addr
    return set_http_server(pb, sn, rq,
                           SET_ORIGIN_SERVER,
                           pb_key_origin_addr,
                           pb_key_offline_origin_addr);
}


/* ------------------------ route_set_proxy_server ------------------------ */

int route_set_proxy_server(pblock *pb, Session *sn, Request *rq)
{
    // Because the server begins executing Route SAFs in the most specific
    // object and works its way down to the least specific, we should not
    // override an existing route
    if (pblock_findkey(pb_key_proxy_addr, rq->vars))
        return REQ_NOACTION; // proxy-addr already set

    // Set proxy-addr
    return set_http_server(pb, sn, rq,
                           SET_PROXY_SERVER,
                           pb_key_proxy_addr,
                           pb_key_offline_proxy_addr);
}


/* ----------------------- route_unset_proxy_server ----------------------- */

NSAPI_PUBLIC int route_unset_proxy_server(pblock *pb, Session *sn, Request *rq)
{
    if (pblock_findkey(pb_key_proxy_addr, rq->vars))
        return REQ_NOACTION;

    pblock_kvinsert(pb_key_proxy_addr,
                    ROUTE_PROXY_ADDR_DIRECT,
                    ROUTE_PROXY_ADDR_DIRECT_LEN,
                    rq->vars);

    return REQ_PROCEED;
}


/* ------------------------ route_set_socks_server ------------------------ */

int route_set_socks_server(pblock *pb, Session *sn, Request *rq)
{
    // Because the server begins executing Route SAFs in the most specific
    // object and works its way down to the least specific, we should not
    // override an existing route
    if (pblock_findkey(pb_key_socks_addr, rq->vars))
        return REQ_NOACTION; // socks-addr already set

    const char *hostname = pblock_findkeyval(pb_key_hostname, pb);
    const char *port = pblock_findkeyval(pb_key_port, pb);

    if (!hostname || !port) {
        log_error(LOG_MISCONFIG, SET_SOCKS_SERVER, sn, rq,
                  XP_GetAdminStr(DBT_need_hostname_and_port));
        return REQ_ABORTED;
    }

    int hostname_len = strlen(hostname);
    int port_len = strlen(port);
    int socks_addr_len = hostname_len + 1 + port_len;

    pb_param *pp = pblock_key_param_create(rq->vars, pb_key_socks_addr,
                                           NULL, socks_addr_len);
    if (!pp)
        return REQ_ABORTED;

    strcpy(pp->value, hostname);
    pp->value[hostname_len] = ':';
    strcpy(pp->value + hostname_len + 1, port);
    pblock_kpinsert(pb_key_socks_addr, pp, rq->vars);

    return REQ_PROCEED;
}


/* ----------------------- route_unset_socks_server ----------------------- */

int route_unset_socks_server(pblock *pb, Session *sn, Request *rq)
{
    if (pblock_findkey(pb_key_socks_addr, rq->vars))
        return REQ_NOACTION;

    pblock_kvinsert(pb_key_socks_addr,
                    ROUTE_SOCKS_ADDR_DIRECT,
                    ROUTE_SOCKS_ADDR_DIRECT_LEN,
                    rq->vars);

    return REQ_PROCEED;
}


/* ---------------- route_magnus_internal_force_http_route ---------------- */

int route_magnus_internal_force_http_route(pblock *pb, Session *sn, Request *rq)
{
    PR_ASSERT(INTERNAL_REQUEST(rq));
    if (!INTERNAL_REQUEST(rq))
        return REQ_ABORTED;

    while (pb_param *pp = pblock_removekey(pb_key_proxy_addr, rq->vars))
        param_free(pp);

    if (const char *a = pblock_findkeyval(pb_key_proxy_addr, pb)) {
        pblock_kvinsert(pb_key_proxy_addr, a, strlen(a), rq->vars);

        char *o = pblock_findkeyval(pb_key_offline_proxy_addr, rq->vars);
        if (o && !strcmp(a, o))
            pblock_kvinsert(pb_key_route_offline, "1", 1, rq->vars);
    }

    while (pb_param *pp = pblock_removekey(pb_key_origin_addr, rq->vars))
        param_free(pp);

    if (const char *a = pblock_findkeyval(pb_key_origin_addr, pb)) {
        pblock_kvinsert(pb_key_origin_addr, a, strlen(a), rq->vars);

        char *o = pblock_findkeyval(pb_key_offline_origin_addr, rq->vars);
        if (o && !strcmp(a, o))
            pblock_kvinsert(pb_key_route_offline, "1", 1, rq->vars);
    }

    return REQ_PROCEED;
}


/* ---------------------- route_service_gateway_dump ---------------------- */

int route_service_gateway_dump(pblock *pb, Session *sn, Request *rq)
{
    param_free(pblock_removekey(pb_key_content_type, rq->srvhdrs));
    pblock_kvinsert(pb_key_content_type, TEXT_PLAIN, TEXT_PLAIN_LEN, rq->srvhdrs);

    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    protocol_start_response(sn, rq);

    int num_gateways = 0;

    for (int i = 0; i < GATEWAY_URL_HT_SIZE; i++) {
        PR_Lock(_gateway_url_ht[i].lock);
        Gateway *gateway = _gateway_url_ht[i].head;
        while (gateway) {
            num_gateways++;

            PR_fprintf(sn->csd,
                       "url = %s, "
                       "addr = %s, "
                       "jroute = %s, "
                       "offline = %s\n",
                       gateway->url,
                       gateway->addr,
                       gateway->jroute,
                       gateway->offline_vsid ? "true" : "false");

            gateway = gateway->next;
        }
        PR_Unlock(_gateway_url_ht[i].lock);
    }

    PR_fprintf(sn->csd, "%d gateway(s)\n", num_gateways);

    return REQ_PROCEED;
}
