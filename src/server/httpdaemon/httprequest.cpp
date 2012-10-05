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
** HttpRequest.cpp
*/

#include <stdio.h>
#include <nss.h>

#include "httpdaemon/httprequest.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/HttpMethodRegistry.h"
#include <base/plist.h>
#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include "libaccess/nsauth.h"

#include "frame/conf.h"
#include "frame/conf_api.h"
#include "frame/dbtframe.h"
#include "frame/http.h"
#include "frame/http_ext.h"
#include "frame/httpact.h"
#include "frame/httpfilter.h"
#include "frame/log.h"
#include "safs/flexlog.h"
#include "safs/addlog.h"
#include "base/servnss.h"
#include "base/util.h"
#include "base/date.h"
#include "prio.h"
#include "prerror.h"
#include "private/pprio.h"

#include "i18n.h"
#include "support/NSString.h"
#include "time/nstime.h"

#include "base64.h"

#include "httpdaemon/vsconf.h"            // VirtualServer class

#ifndef in_addr_t
#define in_addr_t unsigned int
#endif

/* From frame/error.c */
NSAPI_PUBLIC int error_report(Session *sn, Request *rq);

/* These definitions should match the NSHttpHeader enum in nsapi.h */
/* Both lchdrs and lduphdrs should match each other */
/* If you add an entry to one, be sure to add an entry to the other */
static const char *lchdrs[NSHttpHeaderMax] = {
    "unrecognized",
    "accept",
    "accept-charset",
    "accept-encoding",
    "accept-language",
    "accept-ranges",
    "age",
    "allow",
    "authorization",
    "cache-control",
    "connection",
    "content-encoding",
    "content-language",
    "content-length",
    "content-location",
    "content-md5",
    "content-range",
    "content-type",
    "cookie",
    "date",
    "etag",
    "expect",
    "expires",
    "from",
    "host",
    "if-match",
    "if-modified-since",
    "if-none-match",
    "if-range",
    "if-unmodified-since",
    "last-modified",
    "location",
    "max-forwards",
    "pragma",
    "proxy-authenticate",
    "proxy-authorization",
    "range",
    "referer",
    "retry-after",
    "server",
    "te",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "user-agent",
    "vary",
    "via",
    "warning",
    "www-authenticate"
};

static const int lduphdrs[NSHttpHeaderMax] = {
    0, //"unrecognized",
    1, //"accept",
    1, //"accept-charset",
    1, //"accept-encoding",
    1, //"accept-language",
    1, //"accept-ranges",
    0, //"age",
    1, //"allow",
    0, //"authorization",
    0, //"cache-control",
    0, //"connection",
    0, //"content-encoding",
    0, //"content-language",
    0, //"content-length",
    0, //"content-location",
    0, //"content-md5",
    0, //"content-range",
    0, //"content-type",
    1, //"cookie",
    0, //"date",
    0, //"etag",
    0, //"expect",
    0, //"expires",
    0, //"from",
    0, //"host",
    1, //"if-match",
    0, //"if-modified-since",
    1, //"if-none-match",
    0, //"if-range",
    1, //"if-unmodified-since",
    0, //"last-modified",
    0, //"location",
    0, //"max-forwards",
    0, //"pragma",
    0, //"proxy-authenticate",
    0, //"proxy-authorization",
    0, //"range",
    0, //"referer",
    0, //"retry-after",
    0, //"server",
    0, //"te",
    0, //"trailer",
    0, //"transfer-encoding",
    0, //"upgrade",
    0, //"user-agent",
    0, //"vary",
    0, //"via",
    0, //"warning",
    0, //"www-authenticate"
};

/* pb_key values for known headers */
static const pb_key *pbkeyhdrs[NSHttpHeaderMax];

PRBool HttpRequest::fStrictHttpHeaders = PR_FALSE;
PRBool HttpRequest::fDiscardMisquotedCookies = PR_TRUE;

// Enable URI canonicalization by default
PRBool HttpRequest::fCanonicalizeURI = PR_TRUE;

const Filter *HttpRequest::httpfilter = NULL;

/* TCP_NODELAY socket option inherited from listen socket */
PRBool       HttpRequest::fTCPDelayInherited = PR_FALSE;

/* Thread private data index of pointer to HttpRequest instance */
PRIntn       HttpRequest::myTpdIndex = -1;

static const char *client_language_callback(void *data)
{
    HttpRequest *hrq = (HttpRequest *) data;

    const VirtualServer *vs = hrq->getVS();
    if (vs && vs->localization.negotiateClientLanguage) {
        const NSAPIRequest *nrq = hrq->GetNSAPIRequest();
        if (nrq && nrq->rq.headers)
            return pblock_findkeyval(pb_key_accept_language, nrq->rq.headers);
    }

    return NULL;
}

static const char *default_language_callback(void *data)
{
    HttpRequest *hrq = (HttpRequest *) data;

    const VirtualServer *vs = hrq->getVS();
    if (vs)
        return vs->localization.defaultLanguage;

    return NULL;
}

HttpRequest::HttpRequest (DaemonSession *_pSession) 
{
    pSession = _pSession;
    rqHdr = NULL;
    lsc = NULL;
    vs = NULL;
    serverHostname = NULL;
    fn = NULL;

    fRedirectToSSL = PR_FALSE;
    fClientAuthRequired = PR_FALSE;
    fClientAuthRequested = PR_FALSE;
    fSSLEnabled = PR_FALSE;
    fKeepAliveRequested = PR_FALSE;
    fAcceleratable = PR_FALSE;
    fNSAPIActive = PR_FALSE;
    fPipelined = PR_FALSE;
    iStatus = 0;

    currRecursionDepth = 0;
    memset((void *)&rqSn, 0, sizeof(NSAPISession));
    memset((void *)&rqRq, 0, sizeof(NSAPIRequest));

    // initialize header to pb_key mappings (only needs to be done once)
    for (int i = 0; i < NSHttpHeaderMax; i++) {
        pbkeyhdrs[i] = pblock_key(lchdrs[i]);
    }

    // Create an HTTP server filter context object
    httpfilter_context = httpfilter_create_context(_pSession);

    // Get an accelerator cache handle
    accel = accel_handle_create();

    // We haven't yet seen our first Configuration *
    idConfiguration = Configuration::idInvalid;
    fQOS = PR_FALSE;
    fLogFinest = PR_TRUE;
}

HttpRequest::~HttpRequest ()
{
    vs = NULL;
    /*
       we don't need to do the following, as this destructor doesn't seem to be ever called,
       except in the case where the DaemonSession fails to start, in which case the
       conf_reset_globals will assert trying to get the uninitialized thread-private data
    
    conf_reset_globals();
    */
}

//
// HttpRequest::StartSession - init (or re-init) the session-specific data
//
PRBool
HttpRequest::StartSession(netbuf *buf)
{
    PRNetAddr * remoteaddr;
    PRNetAddr * localaddr;

    // Perform first-time thread-specific initialization
    if (PR_GetThreadPrivate(myTpdIndex) == NULL) {
        PR_SetThreadPrivate(myTpdIndex, (void *)this);

        pool = pSession->GetThreadPool();

        // Register a callback to retrieve the request's Accept-language: header
        XP_RegisterGetLanguageRangesCallback(XP_LANGUAGE_SCOPE_THREAD,
                                             XP_LANGUAGE_AUDIENCE_CLIENT,
                                             &client_language_callback,
                                             this);

        // Register a callback to retrieve the VS's default language
        XP_RegisterGetLanguageRangesCallback(XP_LANGUAGE_SCOPE_THREAD,
                                             XP_LANGUAGE_AUDIENCE_DEFAULT,
                                             &default_language_callback,
                                             this);
    }

    if (fTCPDelayInherited == PR_FALSE)
        fTCPDelayInherited = pSession->DisableTCPDelay();

    rqHdr = &pSession->conn->httpHeader;
    lsc = pSession->conn->lsConfig;

    if (pSession->conn->fSSLEnabled) {
        // Connection is using SSL
        const SSLSocketConfiguration *sslc = lsc->getSSLParams();
        fRedirectToSSL = PR_FALSE;
        fClientAuthRequired = (sslc->clientAuth.getEnumValue() == sslc->clientAuth.CLIENTAUTH_REQUIRED);
        fClientAuthRequested = (sslc->clientAuth.getEnumValue() != sslc->clientAuth.CLIENTAUTH_FALSE);
        fSSLEnabled = PR_TRUE;
    } else {
        // Connection isn't using SSL
        fRedirectToSSL = lsc->ssl.enabled;
        fClientAuthRequired = PR_FALSE;
        fClientAuthRequested = PR_FALSE;
        fSSLEnabled = PR_FALSE;
    }

    fKeepAliveRequested = PR_FALSE;
    fAcceleratable = PR_FALSE;
    fNSAPIActive = PR_FALSE;
    fPipelined = PR_FALSE;
    iStatus = 0;

    /* Set up some initial things in the session */
    rqSn.sn.csd = buf->sd;
    rqSn.sn.inbuf = buf;
    rqSn.sn.csd_open = 1;
    rqSn.sn.pool = pool;
    remoteaddr = pSession->GetRemoteAddress();
    localaddr  = pSession->GetLocalAddress();

    /* set these regardless of network type */
    rqSn.sn.pr_client_addr = remoteaddr;
    rqSn.sn.pr_local_addr = localaddr;

    rqSn.thread_data = &pSession->thread_data;

    if ((remoteaddr->raw.family == PR_AF_INET6) &&
	!PR_IsNetAddrType(remoteaddr, PR_IpAddrV4Mapped))
    {
	// Mark IPv4 structure invalid
	rqSn.sn.iaddr.s_addr =  (in_addr_t)-1;
    } else
	rqSn.sn.iaddr = ((struct sockaddr_in *)remoteaddr)->sin_addr;

    if ((localaddr->raw.family == PR_AF_INET6) &&
        !PR_IsNetAddrType(localaddr,PR_IpAddrV4Mapped)) {
        // Mark IPv4 structure invalid
        rqSn.sn.local_addr.sin_addr.s_addr = (in_addr_t)-1;
    } else
	rqSn.sn.local_addr = *(struct sockaddr_in *)localaddr;

    return PR_TRUE;
}

void
HttpRequest::EndSession()
{
    // free resources, etc.
    rqSn.sn.inbuf = 0;
    INTsession_cleanup(&rqSn.sn);
}

PRBool
HttpRequest::HandleRequest(netbuf *buf, PRIntervalTime timeout)
{
    void *poolMark = pool_mark(pool);   // set the pool mark so we can roll back to this point later
 
    /* First request on a session or after keepalive is not pipelined */
    fPipelined = PR_FALSE;

    /* Loop while there is still data from the client */
    while (rqSn.sn.csd_open && (buf->pos < buf->cursize)) {
        // Track session statistics
        pSession->beginRequest();
        rqSn.received = 0;
        rqSn.transmitted = 0;

        // Record the original input buffer state
        unsigned char *origBufInbuf = buf->inbuf;
        int origBufMaxsize = buf->maxsize;

        // Start with the default VS for this socket
        VirtualServer *defaultVS = lsc->getDefaultVS();
        vs = defaultVS;
        serverHostname = lsc->getExternalServerName().getHostname();
        PR_ASSERT(vs);
        PR_ASSERT(serverHostname);

        // Get updated settings from the current configuration
        const Configuration *configuration = vs->getConfiguration();
        if (configuration->getID() != idConfiguration) {
            fQOS = configuration->qos.enabled;
            idConfiguration = configuration->getID();
            fLogFinest = ereport_can_log(LOG_FINEST);
        }

        // Parse the next request
        switch (rqHdr->ParseRequest(buf, timeout)) {
        case HHSTATUS_SUCCESS:
            iStatus = 0;
            break;
        case HHSTATUS_VERSION_NOT_SUPPORTED:
            iStatus = PROTOCOL_VERSION_NOT_SUPPORTED;
            break;
        case HHSTATUS_IO_TIMEOUT:
            iStatus = PROTOCOL_REQUEST_TIMEOUT;
            break;
        default:
            iStatus = PROTOCOL_BAD_REQUEST;
            break;
        }

        // We only honour requests for known methods
        if (rqHdr->GetMethodNumber() == -1) {
            if (!iStatus) {
                ereport(LOG_VERBOSE,
                        "Rejecting %.*s request from %s (Method not implemented)",
                        rqHdr->GetMethod().len,
                        rqHdr->GetMethod().ptr,
                        pSession->GetClientIP());
                iStatus = PROTOCOL_NOT_IMPLEMENTED;
            }
        }

        // Make sure the URI path isn't too long for the ACL subsystem
        const HHString& hsAbsPath = rqHdr->GetRequestAbsPath();
        if (hsAbsPath.len > ACL_PATH_MAX) {
            if (!iStatus) {
                ereport(LOG_VERBOSE,
                        "Received malformed request from %s (%d byte URI)",
                        pSession->GetClientIP(),
                        hsAbsPath.len);
                iStatus = PROTOCOL_URI_TOO_LARGE;
            }
        }

        // Record length of the request header
        rqSn.received += buf->pos;
        if (fQOS)
            vs->getTrafficStats().recordBytes(buf->pos);

        // Did the client request keep-alive and is there room in the
        // keep-alive subsystem for another connection?
        fKeepAliveRequested = pSession->RequestKeepAlive(rqHdr->IsKeepAliveRequested());

        // Get the host, either from Host: header or URL
        const HHString *hsHost = rqHdr->GetHost();

        // Host must be specified somehow if this is HTTP/1.1
        if (rqHdr->GetNegotiatedProtocolVersion() >= PROTOCOL_VERSION_HTTP11 && !hsHost) {
            if (!iStatus) {
                ereport(LOG_VERBOSE,
                        "Received malformed request from %s (missing Host: header)",
                        pSession->GetClientIP());
                iStatus = PROTOCOL_BAD_REQUEST;
            }
        }

        // Get the hostname (without any trailing port number) and port the
        // client thinks it's talking to
        char *host = HttpHeader::HHDup(pool, hsHost);
        if (host) {
            // Client specified host it thinks it's talking to
            serverHostname = host;

            // Remove any trailing port number
            char *port = util_host_port_suffix(host);
            if (port) {
                *port = '\0';
                port++;
            }

            // Determine the VS that should handle this request taking both
            // the listen socket address and the Host: header into account
            vs = lsc->findVS(host);
            if (!vs)
                vs = defaultVS;
        }

        // If the VS state is disabled then return 404 Not Found
        if (!vs->enabled && vs != defaultVS) {
            vs = defaultVS;
            if (!iStatus) {
                ereport(LOG_VERBOSE,
                        "Received request for disabled virtual server %s from %s",
                        vs->name.getStringValue(),
                        pSession->GetClientIP());
                iStatus = PROTOCOL_NOT_FOUND;
            }
        }

        // Track session statistics
        pSession->beginProcessing(vs, rqHdr->GetMethod(), rqHdr->GetRequestAbsPath());

        // Try to process the request using the accelerator cache, falling back
        // to NSAPI if necessary
        if (!AcceleratedRespond())
            UnacceleratedRespond();

        PR_ASSERT(iStatus >= 100 && iStatus <= 599);
        if (iStatus < 100 || iStatus > 999)
            iStatus = PROTOCOL_SERVER_ERROR;

        // Recycle all pool memory back to where we marked at the start of the function
        // this gets rid of all allocations that were done during the request.
        pool_recycle(pool, poolMark);

        // Tidy up the input buffer
        if (RestoreInputBuffer(buf, origBufInbuf, origBufMaxsize) != PR_SUCCESS)
            fKeepAliveRequested = PR_FALSE;

        // Track session statistics
        pSession->endProcessing(vs, iStatus, rqSn.received, rqSn.transmitted);

        // Count next request, if any, as pipelined
        fPipelined = PR_TRUE;

        if (!fKeepAliveRequested)
            break;
    }

    // Let DaemonSession know our current intentions with respect to keepalive
    pSession->RequestKeepAlive(fKeepAliveRequested);

    return rqSn.sn.csd_open;
}

PRBool
HttpRequest::AcceleratedRespond()
{
    // Is it safe to service this request using the accelerator cache?
    fAcceleratable = (!iStatus &&
                      !fQOS &&
                      !fClientAuthRequired &&
                      !fRedirectToSSL &&
                      accel_is_eligible(pSession->conn));
    if (!fAcceleratable)
        return PR_FALSE;

    const HHString &hsAbsPath = rqHdr->GetRequestAbsPath();

    if (fLogFinest) {
        ereport(LOG_FINEST,
                "Checking accelerator cache for %.*s",
                hsAbsPath.len,
                hsAbsPath.ptr);
    }

    int status;
    PRInt64 transmitted;

    pSession->beginFunction();

    // Try to service the request using a previously cached response
    if (!accel_process_http(accel,
                            pool,
                            pSession->conn,
                            vs,
                            &status,
                            &transmitted)) {
        pSession->abortFunction();
        return PR_FALSE;
    }

    // Update STATS_PROFILE_CACHE profile bucket with time spent in accelerator
    pSession->endFunction(STATS_PROFILE_CACHE);
    
    if (fLogFinest) {
        ereport(LOG_FINEST,
                "Accelerator cache serviced %.*s",
                hsAbsPath.len,
                hsAbsPath.ptr);
    }

    iStatus = status;

    rqSn.transmitted += transmitted;

    return PR_TRUE;
}

PRStatus
HttpRequest::RestoreInputBuffer(netbuf *buf, unsigned char *inbuf, int maxsize)
{
    PRStatus rv = PR_SUCCESS;

    // The story so far:
    // - buf is the netbuf we received from our DaemonSession
    // - inbuf is the original input buffer from buf
    // - maxsize is the original maxsize from buf
    // - rqSn.sn.inbuf SHOULD be buf (i.e. no one should change the netbuf*)
    // - buf->inbuf MIGHT NOT be inbuf (e.g. it may have been REALLOC()'d)

    // Check if someone changed the netbuf* itself
    PR_ASSERT(rqSn.sn.inbuf == buf);
    if (rqSn.sn.inbuf != buf) {
        // This should not occur
        ereport(LOG_CATASTROPHE, "rqSn.sn.inbuf changed unexpectedly from %p to %p", buf, rqSn.sn.inbuf);
        netbuf_close(rqSn.sn.inbuf);
        rv = PR_FAILURE;

        // Discard the data and cary on with the original netbuf
        rqSn.sn.inbuf = buf;
        rqSn.sn.inbuf->pos = 0;
        rqSn.sn.inbuf->cursize = 0;
    }

    // If we can't fit all the data back into the original input buffer, we'll
    // be forced to discard the data and close the connection.  Note that we
    // leave room for a trailing null for HttpHeader::ParseRequest().
    int size = buf->cursize - buf->pos;
    if (size >= maxsize) {
        size = 0;
        rv = PR_FAILURE;
    }

    // Move data to the front of the input buffer
    if (size && &buf->inbuf[buf->pos] != inbuf) {
        // Allow for overlapping regions
        memmove(inbuf, &buf->inbuf[buf->pos], size);
    }

    // Restore input buffer
    buf->inbuf = inbuf;
    buf->pos = 0;
    buf->cursize = size;
    buf->maxsize = maxsize;

    return rv;
}

//
// Work detail for background threads
//

void HttpRequest::processQOS()
{
    PR_ASSERT(fQOS);
    PR_ASSERT(vs != NULL);

    const Configuration *configuration = vs->getConfiguration();

    // insert the right values for QOS stats into vars

    // first get the VS stats
    TrafficStats& vs_stats = vs->getTrafficStats();

    pblock_nninsert("vs_bandwidth", vs_stats.getBandwidth(), rqRq.rq.vars);
    pblock_nninsert("vs_connections", vs_stats.getConnections(), rqRq.rq.vars);

    PRInt32 vsMaxBps = PR_INT32_MAX;
    if (vs->qosLimits.enabled)
        vsMaxBps = *vs->qosLimits.getMaxBps();
    pblock_nninsert("vs_bandwidth_limit", vsMaxBps, rqRq.rq.vars);
    pblock_nninsert("vs_bandwidth_enforced", (vsMaxBps != PR_INT32_MAX), rqRq.rq.vars);

    PRInt32 vsMaxConnections = PR_INT32_MAX;
    if (vs->qosLimits.enabled)
        vsMaxConnections = *vs->qosLimits.getMaxConnections();
    pblock_nninsert("vs_connections_limit", vsMaxConnections, rqRq.rq.vars);
    pblock_nninsert("vs_connections_enforced", (vsMaxConnections != PR_INT32_MAX), rqRq.rq.vars);

    // then get the global SERVER stats
    TrafficStats& srv_stats = configuration->getTrafficStats();

    pblock_nninsert("server_bandwidth", srv_stats.getBandwidth(), rqRq.rq.vars);
    pblock_nninsert("server_connections", srv_stats.getConnections(), rqRq.rq.vars);

    PRInt32 serverMaxBps = PR_INT32_MAX;
    if (configuration->qosLimits.enabled)
        serverMaxBps = *configuration->qosLimits.getMaxBps();
    pblock_nninsert("server_bandwidth_limit", serverMaxBps, rqRq.rq.vars);
    pblock_nninsert("server_bandwidth_enforced", (serverMaxBps != PR_INT32_MAX), rqRq.rq.vars);

    PRInt32 serverMaxConnections = PR_INT32_MAX;
    if (configuration->qosLimits.enabled)
        serverMaxConnections = *configuration->qosLimits.getMaxConnections();
    pblock_nninsert("server_connections_limit", serverMaxConnections, rqRq.rq.vars);
    pblock_nninsert("server_connections_enforced", (serverMaxConnections != PR_INT32_MAX), rqRq.rq.vars);
}

void HttpRequest::UnacceleratedRespond()
{
    const char *clientIP = pSession->GetClientIP();

    /*
     * Initialize request-specific NSAPI Session data.
     *
     * Because we allow plugins to modify sn (notably sn->client) but recycle
     * pool memory between requests, this must happen once per request, not
     * once per session.
     *
     * Note that the request-invariant NSAPI Session data was initialized in
     * HttpRequest::StartSession().
     */

    strcpy(rqSn.sn.inbuf->address, clientIP);

    rqSn.sn.client = NULL;
    rqSn.sn.next = NULL;
    rqSn.sn.fill = 1;
    rqSn.sn.subject = NULL;
    rqSn.sn.ssl = fSSLEnabled;
    if (fClientAuthRequired) {
        rqSn.sn.clientauth = 1;
    } else if (fClientAuthRequested) {
        rqSn.sn.clientauth = -1;
    } else {
        rqSn.sn.clientauth = 0;
    }

    rqSn.httpfilter = NULL;
    rqSn.input_done = PR_FALSE;
    rqSn.input_os_pos = 0;
    rqSn.exec_rq = &rqRq;
    rqSn.filter_rq = &rqRq;
    rqSn.session_clone = PR_FALSE;

    ClAuth_t *cla = (ClAuth_t *) pool_malloc(pool, sizeof(ClAuth_t));
    rqSn.sn.clauth = (void *) cla;
    cla->cla_realm = 0;
    cla->cla_ipaddr = 0;
    cla->cla_dns = 0;
    cla->cla_uoptr = 0;
    cla->cla_goptr = 0;
    cla->cla_cert = 0;

    rqSn.sn.client = pblock_create_pool(pool, SESSION_HASHSIZE);

    pblock_kvinsert(pb_key_ip, clientIP, strlen(clientIP), rqSn.sn.client);

    INTsession_fill_ssl(&rqSn.sn);

    /* Check the security  - side effect: cla->cla_cert may get set */
    if (servssl_check_session(&rqSn.sn) != PR_SUCCESS) {
        if (!iStatus) {
            ereport(LOG_VERBOSE,
                    "Error receiving client certificate from %s",
                    clientIP);
            iStatus = PROTOCOL_FORBIDDEN;
        }
    }

    /*
     * Initialize the NSAPI Request structure
     */

    request_initialize(pool, this, serverHostname, &rqRq);
    PERM_REQUEST(&rqRq.rq) = 1;
    KEEP_ALIVE(&rqRq.rq) = fKeepAliveRequested;
    PIPELINED(&rqRq.rq) = fPipelined;
    ABS_URI(&rqRq.rq) = rqHdr->IsAbsoluteURI();

    /* Pass request line as "clf-request" */
    const HHString& hsRequestLine = rqHdr->GetRequestLine();
    pblock_kvinsert(pb_key_clf_request, hsRequestLine.ptr, hsRequestLine.len, rqRq.rq.reqpb);

    /* Pass method as "method" in reqpb, and also as method_num */
    const HHString& hsMethod = rqHdr->GetMethod();
    pblock_kvinsert(pb_key_method, hsMethod.ptr, hsMethod.len, rqRq.rq.reqpb);
    rqRq.rq.method_num = rqHdr->GetMethodNumber();
    PR_ASSERT(rqRq.rq.method_num != -1 || iStatus);

    /* Pass protocol as "protocol" in reqpb, and also in protv_num */
    if (const HHString *hsProtocol = rqHdr->GetProtocol()) {
        pblock_kvinsert(pb_key_protocol, hsProtocol->ptr, hsProtocol->len, rqRq.rq.reqpb);
    } else {
        pblock_kvinsert(pb_key_protocol, "HTTP/0.9", 8, rqRq.rq.reqpb);
    }
    rqRq.rq.protv_num = rqHdr->GetNegotiatedProtocolVersion();
    PR_ASSERT(rqRq.rq.protv_num > 0);

    /* Pass any query as "query" in reqpb */
    if (const HHString *hsQuery = rqHdr->GetQuery())
        pblock_kvinsert(pb_key_query, hsQuery->ptr, hsQuery->len, rqRq.rq.reqpb);

    /* Get abs_path part of request URI, and canonicalize the path */
    char *absPath;
    const HHString& hsAbsPath = rqHdr->GetRequestAbsPath();
    if (hsAbsPath.len > 0) {
        if (fCanonicalizeURI) {
            absPath = util_canonicalize_uri(pool, hsAbsPath.ptr, hsAbsPath.len, NULL);
            if (!absPath) {
                if (!iStatus) {
                    ereport(LOG_VERBOSE,
                            "Received malformed request from %s (URI points outside document root)",
                            clientIP);
                    iStatus = PROTOCOL_FORBIDDEN;
                }
            }
        } else {
            absPath = HttpHeader::HHDup(pool, &hsAbsPath);
        }
    } else {
        absPath = NULL;
    }

    /* Decode the abs_path */
    if (absPath) {
#ifdef XP_WIN32
        char *unmpath = (char *) pool_malloc(pool, strlen(absPath) + 1);
        if (!util_uri_unescape_and_normalize(pool, absPath, unmpath))
            absPath = NULL;
        pblock_nvinsert("unmuri", unmpath, rqRq.rq.vars);
#else
        if (!util_uri_unescape_strict(absPath))
            absPath = NULL;
#endif
        if (!absPath) {
            if (!iStatus) {
                ereport(LOG_VERBOSE,
                        "Received malformed request from %s (invalid URI encoding)",
                        clientIP);
                iStatus = PROTOCOL_BAD_REQUEST;
            }
        }
    }

    /* Pass the abs_path as "uri" in reqpb */
    if (absPath) {
        pblock_kvinsert(pb_key_uri, absPath, strlen(absPath), rqRq.rq.reqpb);
    } else {
        if (!iStatus) {
            ereport(LOG_VERBOSE,
                    "Received malformed request from %s (invalid URI)",
                    clientIP);
            iStatus = PROTOCOL_FORBIDDEN;
        }
        pblock_kvinsert(pb_key_uri, "/", 1, rqRq.rq.reqpb);
    }

    rqRq.rq.loadhdrs = 0;

    if (MakeHeadersPblock() != PR_SUCCESS) {
        if (!iStatus) {
            ereport(LOG_VERBOSE,
                    "Received malformed request from %s (invalid header field)",
                    clientIP);
            iStatus = PROTOCOL_BAD_REQUEST;
        }
    }

    /* Add any client certificate to rq->vars */
    if (cla->cla_cert) {
        char *certb64 = BTOA_DataToAscii(cla->cla_cert->derCert.data, cla->cla_cert->derCert.len);
        if (certb64) {
            pblock_removekey(pb_key_auth_cert, rqRq.rq.vars);
            pblock_kvinsert(pb_key_auth_cert, certb64, strlen(certb64), rqRq.rq.vars);
            PORT_Free(certb64);
        }
    }

    /* Add QOS parameters to rq->vars */
    if (fQOS)
        processQOS();

    /*
     * Send the request to NSAPI
     */

    // Initialize the NSAPI "globals" with VS-specific information
    conf_set_thread_globals(this);

    currRecursionDepth = 0;

    fNSAPIActive = PR_TRUE;

    if (fRedirectToSSL) {
        NSString url;
        url.append(lsc->getListenScheme());
        url.append("://");
        url.append(serverHostname);
        url.printf(":%d/", lsc->port.getInt32Value());

        if (fLogFinest)
            ereport(LOG_FINEST, "Redirecting %s to %s using NSAPI", clientIP, url.data());

        /* Redirect to SSL listener */
        servact_error_processed(&rqSn.sn, &rqRq.rq, PROTOCOL_REDIRECT, NULL, url);

        fKeepAliveRequested = PR_FALSE;

    } else if (iStatus) {
        if (fLogFinest)
            ereport(LOG_FINEST, "Processing %d error for %s using NSAPI", iStatus, clientIP);

        /* Handle error response */
        servact_error_processed(&rqSn.sn, &rqRq.rq, iStatus, NULL, NULL);

    } else {
        if (fLogFinest)
            ereport(LOG_FINEST, "Processing %s for %s using NSAPI", absPath, clientIP);

        /* We'd like to store the result in the accelerator cache */
        if (fAcceleratable)
            accel_enable(&rqSn.sn, &rqRq.rq);

        /* Insert the httpfilter that will enforce message boundaries, etc. */
        filter_insert(NULL, NULL, &rqSn.sn, &rqRq.rq, httpfilter_context, httpfilter);

        /* Pass the request on to the NSAPI engine */
        servact_handle_processed(&rqSn.sn, &rqRq.rq);

        /* Remove Session/Request-based filters */
        filter_finish_response(&rqSn.sn);

        /* Store the result in the accelerator cache if possible */
        if (fAcceleratable)
            fAcceleratable = accel_store(&rqSn.sn, &rqRq.rq);
    }

    fNSAPIActive = PR_FALSE;

    PR_ASSERT(currRecursionDepth == 0);

    conf_reset_thread_globals();

    /*
     * Tear down the NSAPI objects
     */

    // Extract information set during NSAPI processing
    iStatus = rqRq.rq.status_num;
    pSession->conn->fUncleanShutdown |= SSL_UNCLEAN_SHUTDOWN(&rqRq.rq);
    fKeepAliveRequested &= KEEP_ALIVE(&rqRq.rq);

    // Free the request.  This also removes Request-oriented filters.
    PERM_REQUEST(&rqRq.rq) = 1;
    request_free(&rqRq.rq);

    // Free rqSn.sn.client, rqSn.sn.clauth and rqSn.sn.subject
    INTsession_cleanup(&rqSn.sn);
}


PRBool HttpRequest::Initialize ()
{
    PRBool rv = PR_TRUE;
    if (myTpdIndex == -1) {
        if (PR_NewThreadPrivateIndex((PRUintn *)&myTpdIndex, NULL)
            == PR_FAILURE) {
            rv = PR_FALSE;
        }
    }

    httpfilter = httpfilter_get_filter();

    fCanonicalizeURI = conf_getboolean("CanonicalizeURI", fCanonicalizeURI);

    return rv;
}


void HttpRequest::SetStrictHttpHeaders(PRBool f)
{
    fStrictHttpHeaders = f;
}

void HttpRequest::SetDiscardMisquotedCookies(PRBool f)
{
    fDiscardMisquotedCookies = f;
}

PRBool HttpRequest::GetDiscardMisquotedCookies(void)
{
    return fDiscardMisquotedCookies;
}


PRStatus
HttpRequest::MakeHeadersPblock()
{
    int i;
    int j;
    int pvsize;
    char *cp;
    pb_param *pp;
    const HHHeader *hh;
    char seen[NSHttpHeaderMax];
    int rc = 0;

    /* Clear the flags for the known header types */
    memset(seen, 0, sizeof(seen));

    /* Scan all the headers we received */
    for (i = 0; ((hh = rqHdr->GetHeader(i)) != NULL); ++i) {

        /* Is this one of the known headers that has not yet been seen? */
        if ((hh->ix > 0) && (hh->ix < NSHttpHeaderMax) && !seen[hh->ix]) {

            /* Mark this header type as seen */
            seen[hh->ix] = 1;

            /* Scan list of headers of this type to get total length */
            pvsize = hh->val.len + 1;
            j = hh->next;
            while (j >= 0) {
                const HHHeader *hhcont = rqHdr->GetHeader(j);
                if (!lduphdrs[hh->ix]){
                    // This is considered evil
                    // If strict checking is on, return indicating failure
                    if (HttpRequest::fStrictHttpHeaders)
                        return PR_FAILURE;

                    // Always reject conflicting Content-length: headers to
                    // preempt HTTP request smuggling attacks
                    if (hh->ix == NSHttpHeader_Content_Length) {
                        if (hhcont->val.len != hh->val.len ||
                            memcmp(hhcont->val.ptr, hh->val.ptr, hh->val.len))
                            return PR_FAILURE;
                    }
                }
                pvsize += (hhcont->val.len + 2);
                j = hhcont->next;
            }

            /* Create a parameter block with a NULL value */
            const pb_key *key = pbkeyhdrs[hh->ix];
            if (key) {
                pp = pblock_key_param_create(rqRq.rq.headers, key, NULL, 0);
            } else {
                pp = pblock_param_create(rqRq.rq.headers, (char *)lchdrs[hh->ix], NULL);
            }

            /* Allocate space for the value to be constructed */
            cp = (char *)pool_malloc(pool, pvsize);
            pp->value = cp;

            /* Scan the list again, this time constructing the value */
            while (1) {

                if (hh->val.ptr && (hh->val.len > 0)) {

                    /* Append the new value */
                    memcpy(cp, hh->val.ptr, hh->val.len);
                    cp += hh->val.len;
                }

                /* Append ", " if there is another value */
                if (hh->next >= 0) {
                    *cp++ = ',';
                    *cp++ = ' ';
                    hh = rqHdr->GetHeader(hh->next);
                }
                else {
                    /* Otherwise we're done */
                    *cp = '\0';
                    break;
                }
            }

            /* Add the parameter block to the pblock */
            pblock_kpinsert(key, pp, rqRq.rq.headers);
        }
        else if (hh->ix <= 0){ 
            /* Not a standard header, have to do it the hard way */
            pb_param *found;
            char *name = hh->tag.ptr;
            char *val = hh->val.ptr;
            int nlen = hh->tag.len;
            char *nameptr;
            char namebuf[64];             /* should hold longest header name */
            char ntch = '\0';
            char vtch;

            /* If header name exceeds maximum, don't convert case */
            if (nlen >= sizeof(namebuf)) {

                /*
                 * We want to null-terminate name and val, but we need to
                 * restore them when we're done.
                 */
                ntch = name[nlen];
                name[nlen] = '\0';

                nameptr = name;
            }
            else {

                /* Convert name to lower case */
                for (j = 0; j < nlen; ++j) {
                    namebuf[j] = isupper(name[j]) ? tolower(name[j]) : name[j];
                }

                namebuf[j] = '\0';
                nameptr = namebuf;
            }

            if (!(found = pblock_find(nameptr, rqRq.rq.headers))) {
                vtch = val[hh->val.len];
                val[hh->val.len] = '\0';
                pblock_nvinsert(nameptr, val, rqRq.rq.headers);
                val[hh->val.len] = vtch;
            }else { /* Dup header, concatenate values */
                int lv = strlen(found->value);
                int y;

                /* 2 = ", "  (already have space for the '\0') */
                cp = (char *)pool_realloc(pool, found->value, hh->val.len + lv + 3);
                found->value = cp;

                cp += lv;
                *cp++ = ',';
                *cp++ = ' ';

                for(y = 0; y < hh->val.len; ++y) {
                    *cp++ = hh->val.ptr[y];
                }
                *cp = '\0';
            }

            /* Restore original name terminating character, if necessary */
            if (ntch) {
                name[nlen] = ntch;
            }
        }
    }
    return PR_SUCCESS;
}

const NSAPIRequest *HttpRequest::GetNSAPIRequest() const
{
    PR_ASSERT(fNSAPIActive);

    return &rqRq;
}

const NSAPISession *HttpRequest::GetNSAPISession() const
{
    return &rqSn;
}

AcceleratorHandle *HttpRequest::GetAcceleratorHandle() const
{
    return accel;
}

HttpRequest *HttpRequest::CurrentRequest()
{
    HttpRequest *ret = NULL;

    if (myTpdIndex >= 0) {
        ret = (HttpRequest *)PR_GetThreadPrivate((PRUintn)myTpdIndex);
    }

    return ret;
}


void HttpRequest::SetCurrentRequest(HttpRequest *hrq)
{
    PR_SetThreadPrivate(myTpdIndex, hrq);
}


httpd_objset* HttpRequest::getObjset()
{
    if (!vs) return NULL;
    return vs->getObjset();
}
