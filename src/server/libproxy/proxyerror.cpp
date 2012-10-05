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
 * proxyerror.cpp: Proxy server error messages
 *
 * Chris Elving
 */

#include "private/pprio.h"
#include "netsite.h"
#include "base/pool.h"
#include "base/pblock.h"
#include "base/util.h"
#include "frame/log.h"
#include "frame/conf.h"
#include "frame/http.h"
#include "frame/httpfilter.h"
#include "NsprWrap/NsprError.h"
#include "libproxy/route.h"
#include "libproxy/dbtlibproxy.h"
#include "libproxy/proxyerror.h"


#define MAX_DESC_LEN 2048
#define MAX_MSG_LEN 1024
#define MAGNUS_INTERNAL_SEND_PROXY_ERROR "magnus-internal/send-proxy-error"
#define MAGNUS_INTERNAL_SEND_PROXY_ERROR_LEN (sizeof(MAGNUS_INTERNAL_SEND_PROXY_ERROR) - 1)

#ifdef FEAT_PROXY
extern char *proxy_custom_signature;
#endif

/* --------------------------- server_generated --------------------------- */

static inline PRBool server_generated(Session *sn, Request *rq)
{
    while (rq != rq->orig_rq)
        rq = rq->orig_rq;
    PRBool internal_request = INTERNAL_REQUEST(rq);

    PRBool internal_session = (sn->csd == NULL ||
                               sn->csd == SYS_NET_ERRORFD ||
                               sn->csd_open != 1 ||
                               PR_FileDesc2NativeHandle(sn->csd) == -1);

    return internal_request && internal_session;
}


/* ---------------------------- user_typed_url ---------------------------- */

static inline PRBool user_typed_url(Session *sn, Request *rq)
{
    return (!INTERNAL_REQUEST(rq) &&
            !pblock_findkey(pb_key_referer, rq->headers) &&
            !route_get_proxy_addr(sn, rq) &&
            !route_get_origin_addr(sn, rq));
}


/* ------------------------- set_action_check_url ------------------------- */

static inline void set_action_check_url(Session *sn, Request *rq)
{
    const char *error_action = XP_GetClientStr(DBT_check_url);
    pblock_kvinsert(pb_key_error_action,
                    error_action,
                    strlen(error_action),
                    rq->vars);
}


/* ------------------------- set_action_try_later ------------------------- */

static inline void set_action_try_later(Session *sn, Request *rq)
{
    const char *error_action = XP_GetClientStr(DBT_try_later);
    pblock_kvinsert(pb_key_error_action,
                    error_action,
                    strlen(error_action),
                    rq->vars);
}

/* --------------------- set_action_mime_type_blocked --------------------- */

static inline void set_action_mime_type_blocked(Session *sn, Request *rq)
{
    const char *error_action = XP_GetClientStr(DBT_mime_type_blocked);
    pblock_kvinsert(pb_key_error_action,
                    error_action,
                    strlen(error_action),
                    rq->vars);
}


/* -------------------------- set_action_default -------------------------- */

static inline void set_action_default(Session *sn, Request *rq)
{
    if (user_typed_url(sn, rq)) {
        set_action_check_url(sn, rq);
    } else {
        set_action_try_later(sn, rq);
    }
}


/* ------------------------------- set_desc ------------------------------- */

static void set_desc(Session *sn, Request *rq, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    pb_param *pp = pblock_key_param_create(rq->vars,
                                           pb_key_error_desc,
                                           NULL,
                                           MAX_DESC_LEN);
    if (pp) {
        PR_vsnprintf(pp->value, MAX_DESC_LEN, fmt, args);
        pblock_kpinsert(pb_key_error_desc, pp, rq->vars);
    }

    va_end(args);
}


/* ------------------------- set_desc_agent_error ------------------------- */

static void set_desc_agent_error(Session *sn,
                                 Request *rq,
                                 const char *host,
                                 int port,
                                 const char *msg)
{
    char *html_host = util_html_escape(host);
    if (!html_host)
        return;

    set_desc(sn, rq,
             XP_GetClientStr(DBT_unexpected_error_X_Y_X_Y_because_Z),
             html_host, port, html_host, port, msg);

    pool_free(sn->pool, html_host);
}


/* ----------------------------- agent_error ------------------------------ */

static void agent_error(int degree,
                        const char *fn,
                        Session *sn,
                        Request *rq,
                        const char *host,
                        int port,
                        int code,
                        const char *fmt,
                        va_list args)
{
    PR_ASSERT(code == PROTOCOL_BAD_GATEWAY ||
              code == PROTOCOL_GATEWAY_TIMEOUT);

    char *msg = (char *)pool_malloc(sn->pool, MAX_MSG_LEN);
    if (!msg)
        return;

    PR_vsnprintf(msg, MAX_MSG_LEN, fmt, args);
    set_desc_agent_error(sn, rq, host, port, msg);
    set_action_default(sn, rq);
    pblock_kvinsert(pb_key_error_fn,
                    MAGNUS_INTERNAL_SEND_PROXY_ERROR,
                    MAGNUS_INTERNAL_SEND_PROXY_ERROR_LEN,
                    rq->vars);

    pool_free(sn->pool, msg);

    log_error_v(degree, fn, sn, rq, fmt, args);

    protocol_status(sn, rq, code, NULL);
}


/* ----------------------- proxyerror_agent_errorf ------------------------ */

void proxyerror_agent_errorf(int degree,
                             const char *fn,
                             Session *sn,
                             Request *rq,
                             const char *host,
                             int port,
                             int code,
                             const char *fmt,
                             ...)
{
    va_list args;
    va_start(args, fmt);
    NsprError error;
    error.save();
    if (server_generated(sn, rq))
        degree = LOG_VERBOSE;
    error.restore();
    agent_error(degree, fn, sn, rq, host, port, code, fmt, args);
    va_end(args);
}


/* ------------------------ proxyerror_agent_error ------------------------ */

void proxyerror_agent_error(int degree,
                            const char *fn,
                            Session *sn,
                            Request *rq,
                            const char *host,
                            int port,
                            int code,
                            const char *msg)
{
    proxyerror_agent_errorf(degree, fn, sn, rq, host, port, code, "%s", msg);
}


/* ----------------------- proxyerror_channel_error ----------------------- */

void proxyerror_channel_error(int degree,
                              const char *fn,
                              Session *sn,
                              Request *rq,
                              const char *host,
                              int port)
{
    // XXX what about SOCKS connection problems? There's no point in telling
    // the user to check the URL if the real problem was a problem with sockd

    NsprError error;
    error.save();

    char *html_host = util_html_escape(host);
    if (!html_host)
        return;

    error.restore();

    switch (PR_GetError()) {
    case PR_DIRECTORY_LOOKUP_ERROR:
        // DNS lookup failed
        set_desc(sn, rq,
                 XP_GetClientStr(DBT_hostname_X_not_found),
                 html_host);
        set_action_default(sn, rq);
        break;

    case PR_IO_TIMEOUT_ERROR:
        // connect() timed out
        set_desc(sn, rq,
                 XP_GetClientStr(DBT_connect_timeout_X_Y),
                 html_host, port);
        set_action_default(sn, rq);
        break;

    case PR_CONNECT_REFUSED_ERROR:
        // connect() refused
        set_desc(sn, rq,
                 XP_GetClientStr(DBT_connect_refused_X_Y),
                 host, port);
        set_action_default(sn, rq);
        break;

    default:
        // Atypical error
        set_desc(sn, rq,
                 XP_GetClientStr(DBT_connect_X_Y_failed_because_Z),
                 html_host, port, system_errmsg());
        set_action_try_later(sn, rq);
        break;
    }

    pool_free(sn->pool, html_host);

    pblock_kvinsert(pb_key_error_fn,
                    MAGNUS_INTERNAL_SEND_PROXY_ERROR,
                    MAGNUS_INTERNAL_SEND_PROXY_ERROR_LEN,
                    rq->vars);

    if (server_generated(sn, rq))
        degree = LOG_VERBOSE;

    error.restore();

    log_error(degree, fn, sn, rq,
              XP_GetAdminStr(DBT_unable_to_contact_X_Y_because_Z),
              host, port, system_errmsg());

    error.restore();

    // Note that RFC 2616 seems to imply that 504 Gateway Timeout should always
    // be returned when there is a problem contacting an origin server (that
    // is, PROTOCOL_GATEWAY_TIMEOUT isn't just for timeouts)
    protocol_status(sn, rq, PROTOCOL_GATEWAY_TIMEOUT, system_errmsg());
}


/* ------------- proxyerror_magnus_internal_send_proxy_error -------------- */

int proxyerror_magnus_internal_send_proxy_error(pblock *pb,
                                                Session *sn,
                                                Request *rq)
{
#ifdef FEAT_PROXY
/*
    PR_ASSERT(rq->status_num == PROTOCOL_BAD_GATEWAY ||
              rq->status_num == PROTOCOL_GATEWAY_TIMEOUT);
*/

    const char *desc = pblock_findkeyval(pb_key_error_desc, rq->vars);
    if (!desc)
        return REQ_NOACTION;

    const char *action = pblock_findkeyval(pb_key_error_action, rq->vars);
    if (!action)
        return REQ_NOACTION;

    const char *signature;
    char signature_buffer[256];
    if (proxy_custom_signature) {
        signature = proxy_custom_signature;
    } else {
        PR_snprintf(signature_buffer,
                    sizeof(signature_buffer),
                    XP_GetClientStr(DBT_product_X_at_Y_Z),
                    PRODUCT_ID,
                    conf_get_true_globals()->Vserver_hostname,
                    conf_get_true_globals()->Vport);
        signature = signature_buffer;
    }

    const char *type = XP_GetClientStr(DBT_html_error_content_type);
    param_free(pblock_removekey(pb_key_content_type, rq->srvhdrs));

    pblock_kvinsert(pb_key_content_type, type, strlen(type), rq->srvhdrs);

    httpfilter_buffer_output(sn, rq, PR_TRUE);

    protocol_start_response(sn, rq);

    int len = PR_fprintf(sn->csd,
                         XP_GetClientStr(DBT_html_error_X_Y_Z),
                         desc,
                         action,
                         signature);

    if (len > 0)
        pblock_kninsert(pb_key_p2c_cl, len, rq->vars);

    return REQ_PROCEED;
#else
    return REQ_NOACTION;
#endif
}
