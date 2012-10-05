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
 * http.c: Deal with HTTP protocol related things
 * 
 * Rob McCool
 */


#define XP_GetError() PR_GetError()

#include "frame/http.h"

#include "netsite.h"         /* MALLOC, FREE */
#include "frame/log.h"       /* log_error */
#include "base/pblock.h"     /* parameter structure */
#include "base/cinfo.h"      /* cinfo_find */
#include "base/util.h"       /* env. functions, sprintf */
#include "frame/req.h"       /* request_handle */
#include "frame/conf.h"      /* globals for server port and name */
#include "frame/http_ext.h"   /* c++ based functions */
#include "frame/conf_api.h"   /* conf_findglobal */
#include "frame/otype_helper.h"   /* OtypeHelperApplyDefaults */
#include "frame/httpfilter.h"
#include "base/date.h"

#include "frame/dbtframe.h"

#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/HttpMethodRegistry.h"
#include "safs/nsfcsafs.h"
#include "httpdaemon/vsconf.h"

#include "definesEnterprise.h"

#include <stdarg.h>          /* Heh. */
#include <ctype.h>           /* toupper */
#include <time.h>            /* struct tm */

#include "time/nstime.h"

#ifndef XP_UNIX
#include <errno.h>
#endif /* XP_UNIX */

#define http_assbackwards(rq) (rq->protv_num < 100)

#define TOKEN_CLOSE ("close")
#define TOKEN_CLOSE_SIZE (sizeof(TOKEN_CLOSE) - 1)

#define MAX_STATUS_SIZE 256

/*
 * HttpCacheEntry
 * Used to associate headers, etc. with an NSFC cache entry.
 */
typedef struct HttpCacheEntry HttpCacheEntry;
struct HttpCacheEntry {
    struct tm *mtm;
    struct tm mtms;

    int last_modified_len;
    char last_modified[HTTP_DATE_LEN];

    int content_length_len;
    char content_length[21];

    int etag_len;
    char etag[MAX_ETAG];
};

char HTTPsvs[80] = "HTTP/1.1 ";
int HTTPsvs_len = strlen(HTTPsvs);
int HTTPprotv_num = CURRENT_PROTOCOL_VERSION;

char *ServercsMVS = NULL;
int ServercsMVS_len = 0;

static NSFCPrivDataKey http_nsfc_key;
static const Filter *http_httpfilter;
static PRBool http_etag;
static int _ChunkedRequestBufferSize;
static int _ChunkedRequestTimeout;

/* ------------------------ http_set_server_header ------------------------ */

void http_set_server_header(const char *server)
{
   char tmpServer[256];
   if (server == NULL) {
       ServercsMVS = NULL;
       ServercsMVS_len = 0;
       return;
   }
   ServercsMVS_len = strlen("Server: ") + strlen(server);

   if ((ServercsMVS = (char *)malloc(ServercsMVS_len + 1)) == NULL) 
      return;
   PL_strcpy(ServercsMVS, "Server: ");
   PL_strcat(ServercsMVS, server); 
}


/* -------------------------- http_set_protocol --------------------------- */

int http_set_protocol(const char *version)
{
    if (!util_format_http_version(version, &HTTPprotv_num, HTTPsvs, sizeof(HTTPsvs) - 1))
        return -1;

    strcat(HTTPsvs, " ");
    HTTPsvs_len = strlen(HTTPsvs);

    return 0;
}


/* --------------------------- http_enable_etag --------------------------- */

void http_enable_etag(PRBool b)
{
    http_etag = b;
}


/* ---------------------- http_set_max_unchunk_size ----------------------- */

void http_set_max_unchunk_size(int size)
{
    _ChunkedRequestBufferSize = size;
}


/* ----------------------- http_set_unchunk_timeout ----------------------- */

void http_set_unchunk_timeout(PRIntervalTime timeout)
{
    _ChunkedRequestTimeout = net_nspr_interval_to_nsapi_timeout(timeout);
}


/* ---------------------- http_set_keepalive_timeout ---------------------- */

/*
 * http_set_keepalive_timeout() is explicitly exposed through NSAPI
 * as protocol_set_keepalive_timeout().
 */
NSAPI_PUBLIC
void http_set_keepalive_timeout(int secs)
{
    DaemonSession::SetKeepAliveTimeout(secs);
}


/* -------------------------- http_cache_evictor -------------------------- */

NSPR_BEGIN_EXTERN_C
static void http_cache_evictor(NSFCCache cache, const char *filename, NSFCPrivDataKey key, void *data)
{
    /* Free the HttpCacheEntry */
    free(data);
}
NSPR_END_EXTERN_C


/* ------------------------------ http_init ------------------------------- */

NSAPI_PUBLIC int http_init(void)
{
    if (httpfilter_init() != PR_SUCCESS)
        return -1;

    http_httpfilter = httpfilter_get_filter();

    return 0;
}


/* ---------------------------- http_init_late ---------------------------- */

void http_init_late(void)
{
    PR_ASSERT(!http_nsfc_key);
 
    if (nsapi_nsfc_cache) {
        /* Key we use to associate headers, etc. with a cache entry */
        http_nsfc_key = NSFC_NewPrivateDataKey(nsapi_nsfc_cache,
                                               http_cache_evictor);
    }
}


/* ----------------------- http_dump822_with_slack ------------------------ */

NSAPI_PUBLIC char *http_dump822_with_slack(pblock *pb, char *t, int *ip, int tsz, int slack)
{
    struct pb_entry *p;
    register int x, y, z;
    int pos = *ip;
    
    /* Leave room for stuff we tack on after length checks */
    slack += 3;

    for(x = 0; x < pb->hsize; x++) {
        p = pb->ht[x];
        while(p) {
            const pb_key *key = param_key(p->param);
            if (key == pb_key_status || key == pb_key_server || key == pb_key_date) {
                /* Skip internal Status:, Server:, and Date: information */
                p = p->next;
                continue;
            }

            /* 
             * If worst case p->param->name[1] is 0, REALLOC inside next for
             * loop will not be invoked so we need to make sure 3 more bytes at
             * pointer (t + pos) are available before next place condition
             * check + REALLOC is invoked.  Note that Slack is incremented to 3
             * in the beginning.
             */
            if (pos >= (tsz - slack)) {
                tsz += REQ_MAX_LINE;
                t = (char *) REALLOC(t, tsz + 1);
            }

            if (p->param->name[0]) {
                t[pos] = toupper(p->param->name[0]);
                for(y = pos+1, z = 1; (t[y] = p->param->name[z]); y++, z++) {
                    if(y >= (tsz - slack)) {
                        tsz += REQ_MAX_LINE;
                        t = (char *) REALLOC(t, tsz + 1);
                    }
                }
            } else {
                y = pos;
            }

            t[y++] = ':';
            t[y++] = ' ';

            /* 
             * Suppose REALLOC happened in previous for loop and loop exited
             * with y == (tsz - slack) And suppose p->param->value[0] is 0 then
             * we must check the available buffer in t here to make sure we
             * have at least 3 more bytes available to avoid overwriting the
             * allocated buffer. (Note that slack was incremented to 3 in
             * beginning). If p->param->value[0] is not 0 then REALLOC inside
             * next for loop will make sure that we have 3 bytes available
             * before next REALLOC is required.
             */
            if (y >= (tsz - slack)) {
                tsz += REQ_MAX_LINE;
                t = (char *) REALLOC(t, tsz + 1);
            }

            for(z = 0; (t[y] = p->param->value[z]); ++y, ++z) {
                if(y >= (tsz - slack)) {
                    tsz += REQ_MAX_LINE;
                    t = (char *) REALLOC(t, tsz + 1);
                }
            }

            t[y++] = CR;
            t[y++] = LF;

            pos = y;
            p = p->next;
        }
    }
    t[pos] = '\0';
    *ip = pos;
    return t;
}


/* ----------------------------- http_dump822 ----------------------------- */

NSAPI_PUBLIC char *http_dump822(pblock *pb, char *t, int *ip, int tsz)
{
    return http_dump822_with_slack(pb, t, ip, tsz, 2 /* CRLF */);
}


/* -------------------------- http_format_status -------------------------- */

NSAPI_PUBLIC int http_format_status(Session *sn, Request *rq, char *buf, int sz)
{
    int pos = 0;

    const char *protocol = pblock_findkeyval(pb_key_protocol, rq->vars);
    if (protocol) {
        pos += util_strlcpy(buf + pos, protocol, sz - pos - 1);
        buf[pos++] = ' ';
    } else {
        memcpy(buf + pos, HTTPsvs, HTTPsvs_len);
        pos += HTTPsvs_len;
    }

    // ruslan: whenever status is not set - it's an error in user's plugin
    const char *status = pblock_findkeyval(pb_key_status, rq->srvhdrs);
    if (status == NULL) {
        http_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        status = pblock_findkeyval(pb_key_status, rq->srvhdrs);
    }

    int statuslen = strlen(status);
    if (pos + statuslen + 2 > sz)
        return -1;

    memcpy(buf + pos, status, statuslen);
    pos += statuslen;

    buf[pos++] = CR;
    buf[pos++] = LF;

    return pos;
}


/* -------------------------- http_format_server -------------------------- */

NSAPI_PUBLIC int http_format_server(Session *sn, Request *rq, char *buf, int sz)
{
    if (!ServercsMVS)
        return 0;

    if (ServercsMVS_len + 2 > sz)
        return -1;

    memcpy(buf, ServercsMVS, ServercsMVS_len);
    buf[ServercsMVS_len] = CR;
    buf[ServercsMVS_len + 1] = LF;

    return ServercsMVS_len + 2;
}


/* -------------------------- http_make_headers --------------------------- */

char *http_make_headers(Session *sn, Request *rq, char *t, int *ppos, int maxlen, int slack)
{
    pb_param *charset;
    pb_param *ct;
    int pos = *ppos;
    int l;
    int rv;

    /* Generate the status line */
    pos += http_format_status(sn, rq, t + pos, maxlen - pos);

    /* Generate the "Server: " header */
    pos += http_format_server(sn, rq, t + pos, maxlen - pos);

    /* Generate "Date: " header */
    t[pos++] = 'D';
    t[pos++] = 'a';
    t[pos++] = 't';
    t[pos++] = 'e';
    t[pos++] = ':';
    t[pos++] = ' ';
    pos += date_current_formatted(date_format_http, t + pos, 30);
    t[pos++] = CR;
    t[pos++] = LF;
    t[pos] = 0;

    /* Apply any defults on content-type, charset etc that have been set */
    OtypeHelperApplyDefaults(rq);

    /* 
     * Handle charset. Problem is that most browsers don't grok 
     * parameters on content-types. So we send it only to Mozilla/1.1 
     * or browsers that send accept-charset
     */
    if ((charset = pblock_removekey(pb_key_magnus_charset, rq->srvhdrs)) &&
        (ct = pblock_findkey(pb_key_content_type, rq->srvhdrs))) {
        if (!strstr(ct->value, "; charset=")) {
            l = strlen(ct->value);
            ct->value = (char *) pool_realloc(sn->pool, ct->value, l + 10 + 
                                              strlen(charset->value) + 1);
            util_sprintf(&ct->value[l], "; charset=%s", charset->value);
        }
        param_free(charset);
    }

    *ppos = pos;

    return http_dump822_with_slack(rq->srvhdrs, t, ppos, maxlen, slack);
}


/* ------------------------- http_start_response -------------------------- */

NSAPI_PUBLIC int http_start_response(Session *sn, Request *rq)
{
    if (INTERNAL_REQUEST(rq))
        return REQ_PROCEED;

    /* We want the httpfilter to send the headers if possible */
    if (session_get_httpfilter_context(sn))
        return httpfilter_start_response(sn, rq);

    if (ISMHEAD(rq))
        return REQ_NOACTION;

    if (!rq->senthdrs && !http_assbackwards(rq)) {
        /*
         * If we get here, there's no httpfilter installed (e.g. we're handling
         * an error) so we just want to close the connection when we're done.
         */
        KEEP_ALIVE(rq) = PR_FALSE;
        if (rq->protv_num >= PROTOCOL_VERSION_HTTP11) {
            if (!pblock_findkey(pb_key_connection, rq->srvhdrs)) {
                pblock_kvinsert(pb_key_connection, TOKEN_CLOSE,
                                TOKEN_CLOSE_SIZE, rq->srvhdrs);
            }
        }

        /* Allocate a buffer for the headers */
        int pos = 0;
        char *t = (char *) pool_malloc(sn->pool, REQ_MAX_LINE);

        t = http_make_headers(sn, rq, t, &pos, REQ_MAX_LINE, 2 /* CRLF */);

        if (pos < 0) {
            pool_free(sn->pool, t);
            return REQ_ABORTED;
        }

        /* Generate an empty line after the headers */
        t[pos++] = CR;
        t[pos++] = LF;

        int rv = net_write(sn->csd, t, pos);

        pool_free(sn->pool, t);

        rq->senthdrs = PR_TRUE;

        if (rv != pos)
            return REQ_EXIT;
    }

    return REQ_PROCEED;
}


/* ---------------------------- http_hdrs2env ----------------------------- */

NSAPI_PUBLIC char **http_hdrs2env(pblock *pb)
{
    char *t, *n, **env;
    struct pb_entry *p;
    pb_param *pp;
    register int x, y, z;
    int pos, ts, ln;

    /* Find out how many there are. */
    for(x = 0, y = 0; x < pb->hsize; x++) {
        p = pb->ht[x];
        while(p) {
            ++y;
            p = p->next;
        }
    }

    env = util_env_create(NULL, y, &pos);

    ts = 1024;
    t = (char *) MALLOC(ts);

    for(x = 0; x < pb->hsize; x++) {
        p = pb->ht[x];

        while(p) {
            pp = p->param;

            ln = strlen(pp->name) + 6;

            if(ln >= ts) {
                ts = ln;
                t = (char *) REALLOC(t, ts);
            }
            n = pp->name;

            /* Skip authorization for CGI */
            if(strcasecmp(n, "authorization")) {
                if(strcasecmp(n, "content-type") &&
                   strcasecmp(n, "content-length")) 
                {
                    strncpy(t, "HTTP_", 5);
                    z = 5;
                }
                else
                    z = 0;

                for(y = 0; n[y]; ++y, ++z)
                    t[z] = (n[y] == '-' ? '_' : toupper(n[y]));
                t[z] = '\0';

                env[pos++] = util_env_str(t, pp->value);
            }
            p = p->next;
        }
    }
    env[pos] = NULL;
    FREE(t);
    return env;
}


/* ---------------------------- convert_prtime ---------------------------- */

static inline time_t convert_prtime(PRTime t)
{
    const PRInt64 million = LL_INIT(0, 1000000);
    PRInt64 result;
    time_t tt;

    /* Convert a PRTime value to a time_t value */
    LL_DIV(result, t, million);
    LL_L2UI(tt, result);

    return tt;
}


/* --------------------------- http_format_etag --------------------------- */

NSAPI_PUBLIC void http_format_etag(Session *sn, Request *rq, char *etagp, int etaglen, PROffset64 size, time_t mtime)
{
    /*
     * Heuristic for weak/strong validator: if the request was made within
     * a second of the last-modified date, then we send a weak Etag.
     */
    if (rq->req_start - mtime <= 1) {
        /* Send a weak Etag */
        *etagp++ = 'W';
        *etagp++ = '/';
        etaglen -= 2;
    }

    /*
     * Create an Etag out of the metadata (size, mtime) to obtain Etag
     * uniqueness.
     */
    util_snprintf(etagp, etaglen, "\"%x-%x\"", (int)size, (int)mtime);
}


/* --------------------------- http_weaken_etag --------------------------- */

NSAPI_PUBLIC void http_weaken_etag(Session *sn, Request *rq)
{
    /* Replace any existing strong Etag with a weak Etag */
    pb_param *pp = pblock_findkey(pb_key_etag, rq->srvhdrs);
    if (pp) {
        if (pp->value[0] != 'W' || pp->value[1] != '/') {
            char *weak = (char *) pool_malloc(sn->pool, 2 + strlen(pp->value) + 1);

            weak[0] = 'W';
            weak[1] = '/';
            strcpy(weak + 2, pp->value);

            pool_free(sn->pool, pp->value);
            pp->value = weak;
        }
    }
}


/* ------------------------------ match_etag ------------------------------ */

NSAPI_PUBLIC int http_match_etag(const char *header, const char *etag, int strong)
{
    if (!etag)
        return 0; /* mismatch */

    if (header[0] == '*')
        return 1; /* match */

    if (etag[0] == 'W' && etag[1] == '/') {
        /* Weak Etags never match when using the strong validator */
        if (strong)
            return 0; /* mismatch */

        etag += 2;
    }

    /* Look for Etag in header */
    const char *found = strstr(header, etag);
    if (!found)
        return 0; /* mismatch */

    /* Weak Etags never match when using the strong validator */
    if (strong && found >= header + 2 && found[-2] == 'W' && found[-1] == '/')
        return 0; /* mismatch */

    return 1; /* match */
}


/* ----------------------- http_check_preconditions ----------------------- */

NSAPI_PUBLIC int http_check_preconditions(Session *sn, Request *rq, struct tm *mtm, const char *etag)
{
    char *header;

    /* If-modified-since */
    header = pblock_findkeyval(pb_key_if_modified_since, rq->headers);
    if (header) {
        if (mtm && util_later_than(mtm, header)) {
            http_status(sn, rq, PROTOCOL_NOT_MODIFIED, NULL);
            return REQ_ABORTED;
        }
    }

    /* If-unmodified-since */
    header = pblock_findkeyval(pb_key_if_unmodified_since, rq->headers);
    if (header) {
        if (mtm && !util_later_than(mtm, header)) {
            PRTime temptime;
            PRStatus status = PR_ParseTimeString(header, PR_TRUE, &temptime);
            if (status == PR_SUCCESS) {
                http_status(sn, rq, PROTOCOL_PRECONDITION_FAIL, NULL);
                return REQ_ABORTED;
            }
        }
    }

    /* If-none-match */
    header = pblock_findkeyval(pb_key_if_none_match, rq->headers);
    if (header) {
        /* If the If-none-match header matches the current Etag... */
        if (ISMGET(rq) || ISMHEAD(rq)) {
            if (http_match_etag(header, etag, PR_FALSE)) {
                http_status(sn, rq, PROTOCOL_NOT_MODIFIED, NULL);
                return REQ_ABORTED;
            }
        } else {
            if (http_match_etag(header, etag, PR_TRUE)) {
                http_status(sn, rq, PROTOCOL_PRECONDITION_FAIL, NULL);
                return REQ_ABORTED;
            }
        }
    }

    /* If-match */
    header = pblock_findkeyval(pb_key_if_match, rq->headers);
    if (header) {
        /* If the If-match header matches the current Etag... */
        if (!http_match_etag(header, etag, PR_TRUE)) {
            http_status(sn, rq, PROTOCOL_PRECONDITION_FAIL, NULL);
            return REQ_ABORTED;
        }
    }

    return REQ_PROCEED;
}


/* ---------------------------- http_set_finfo ---------------------------- */

static inline int set_finfo(Session *sn, Request *rq, PROffset64 size, time_t mtime)
{
    struct tm mtms;
    struct tm *mtm = system_gmtime(&mtime, &mtms);
    pb_param *pp;

    /* Insert Last-modified */
    if (mtm) {
        pp = pblock_key_param_create(rq->srvhdrs, pb_key_last_modified, NULL,
                                     HTTP_DATE_LEN);
        if (!pp || !pp->value)
            return REQ_ABORTED;
        util_strftime(pp->value, HTTP_DATE_FMT, mtm);
        pblock_kpinsert(pb_key_last_modified, pp, rq->srvhdrs);
    }

    /* Insert Content-length */
    const int content_length_size = 21;
    pp = pblock_key_param_create(rq->srvhdrs, pb_key_content_length, NULL,
                                 content_length_size);
    if (!pp || !pp->value)
        return REQ_ABORTED;
    PR_snprintf(pp->value, content_length_size, "%lld", size);
    pblock_kpinsert(pb_key_content_length, pp, rq->srvhdrs);

    char *etag;
    if (http_etag) {
        /* Insert Etag */
        pp = pblock_key_param_create(rq->srvhdrs, pb_key_etag, NULL, MAX_ETAG);
        if (!pp || !pp->value)
            return REQ_ABORTED;
        http_format_etag(sn, rq, pp->value, MAX_ETAG, size, mtime);
        pblock_kpinsert(pb_key_etag, pp, rq->srvhdrs);
        etag = pp->value;
    } else {
        etag = NULL;
    }

    /* Check If-modified-since, etc. */
    return http_check_preconditions(sn, rq, mtm, etag);
}

NSAPI_PUBLIC int http_set_finfo(Session *sn, Request *rq, struct stat *finfo)
{
    return set_finfo(sn, rq, finfo->st_size, finfo->st_mtime);
}

NSAPI_PUBLIC int http_set_nspr_finfo(Session *sn, Request *rq, PRFileInfo64 *finfo)
{
    return set_finfo(sn, rq, finfo->size, convert_prtime(finfo->modifyTime));
}


/* ------------------------- http_set_nsfc_finfo -------------------------- */

NSAPI_PUBLIC int http_set_nsfc_finfo(Session *sn, Request *rq, NSFCEntry entry, NSFCFileInfo *finfo)
{
    PR_ASSERT(finfo);

    if (!NSFCENTRY_ISVALID(&entry))
        return http_set_nspr_finfo(sn, rq, &finfo->pr);

    PR_ASSERT(nsapi_nsfc_cache);
    PR_ASSERT(http_nsfc_key);

    /* Try to get cached information from NSFC */
    HttpCacheEntry *cached = NULL;
    PRBool isFreeRequired = PR_FALSE;
    NSFCStatus rfc = NSFC_GetEntryPrivateData(entry,
                                              http_nsfc_key,
                                              (void **)&cached,
                                              nsapi_nsfc_cache);
    if (rfc == NSFC_BUSY) {
        /* NSFC contention, use the non-cached NSPR path */
        return http_set_nspr_finfo(sn, rq, &finfo->pr);

    } else if (rfc == NSFC_NOTFOUND) {
        /* Create a new HttpCacheEntry for this NSFCEntry */
        cached = (HttpCacheEntry *)PERM_MALLOC(sizeof(*cached));
        if (!cached)
            return REQ_ABORTED;

        time_t mtime = convert_prtime(finfo->pr.modifyTime);
        cached->mtm = system_gmtime(&mtime, &cached->mtms);

        /* Format Last-modified */
        if (cached->mtm) {
            util_strftime(cached->last_modified, HTTP_DATE_FMT, cached->mtm);
            cached->last_modified_len = strlen(cached->last_modified);
        } else {
            cached->last_modified_len = 0;
        }

        /* Format Content-length */
        PR_snprintf(cached->content_length, sizeof(cached->content_length),
                    "%lld", finfo->pr.size);
        cached->content_length_len = strlen(cached->content_length);

        if (http_etag) {
            /* Format Etag */
            http_format_etag(sn, rq, cached->etag, sizeof(cached->etag),
                             finfo->pr.size, mtime);
            cached->etag_len = strlen(cached->etag);
        } else {
            cached->etag_len = 0;
        }

        /* Cache the newly created HttpCacheEntry */
        rfc = NSFC_SetEntryPrivateData(entry, http_nsfc_key, cached,
                                       nsapi_nsfc_cache);
        if (rfc != NSFC_OK) {
            /* Entry wasn't added to cache, so we need to free it ourselves */
            isFreeRequired = PR_TRUE;
        }

    } else if (rfc != NSFC_OK) {
        /* Unknown error */
        return REQ_ABORTED;
    }

    /* Insert cached headers */
    if (cached->last_modified_len > 0) {
        pblock_kvinsert(pb_key_last_modified, cached->last_modified,
                        cached->last_modified_len, rq->srvhdrs);
    }
    pblock_kvinsert(pb_key_content_length, cached->content_length,
                    cached->content_length_len, rq->srvhdrs);
    if (cached->etag_len > 0) {
        pblock_kvinsert(pb_key_etag, cached->etag, cached->etag_len,
                        rq->srvhdrs);
    }

    /* Check If-modified-since, etc. */
    int rv = http_check_preconditions(sn, rq,
                                      cached->mtm,
                                      cached->etag);

    /* Free the HttpCacheEntry if we own it */
    if (isFreeRequired)
        free(cached);

    return rv;
}


/* ----------------------------- http_status ------------------------------ */

NSAPI_PUBLIC void http_status(Session *sn, Request *rq, int n, const char *r)
{
    if ((n < 100) || (n > 999))
    {
        log_error(LOG_MISCONFIG, XP_GetAdminStr(DBT_httpStatus_), sn, rq, 
            XP_GetAdminStr(DBT_DIsNotAValidHttpStatusCode_), n);
        n = PROTOCOL_SERVER_ERROR;
    }

    rq->status_num = n;

    const char *msg = (r ? r : http_status_message(n));

    pb_param *pp = pblock_removekey(pb_key_status, rq->srvhdrs);
    if (pp != NULL)
        param_free(pp);

    pp = pblock_key_param_create(rq->srvhdrs, pb_key_status, NULL, MAX_STATUS_SIZE - 1);
    if (pp != NULL) {
        int len = util_itoa(n, pp->value);
        pp->value[len++] = ' ';
        util_strlcpy(pp->value + len, msg, MAX_STATUS_SIZE - len);

        pblock_kpinsert(pb_key_status, pp, rq->srvhdrs);
    }
}

/**
 * http_status_message - return a standard message for HTTP protocol codes
 *
 * @parame  code    message code
 * @return  (const char *0 with the code
 */

NSAPI_PUBLIC const char * http_status_message (int code)
{
    const char *r;

    switch (code)
    {
        case PROTOCOL_CONTINUE : // 100
            r = "Continue";
            break;
        case PROTOCOL_SWITCHING: //101
            r = "Switching Protocols";
            break;
        case PROTOCOL_OK: // 200
            r = "OK";
            break;
        case PROTOCOL_CREATED: // 201
            r = "Created";
            break;
        case PROTOCOL_ACCEPTED: // 202
            r = "Accepted";
            break;
        case PROTOCOL_NONAUTHORITATIVE: // 203
            r = "Non-Authoritative Information";
            break;
        case PROTOCOL_NO_CONTENT: //204
        /* There is another define to PROTOCOL_NO_RESPONSE for 204 in nsapi.h 
           The spec maps this to No Content.
           Hence cahnging this to No Content
        */
            r = "No Content";
            break;
        case PROTOCOL_RESET_CONTENT: // 205
            r = "Reset Content";
            break;
        case PROTOCOL_PARTIAL_CONTENT: // 206
            r = "Partial Content";
            break;
        case PROTOCOL_MULTI_STATUS: // 207
            r = "Multi Status";
            break;
        case PROTOCOL_MULTIPLE_CHOICES: // 300
            r = "Multiple Choices";
            break;
        case PROTOCOL_MOVED_PERMANENTLY: // 301
            r = "Moved Permanently";
            break;
        case PROTOCOL_REDIRECT:          // 302
            r = "Moved Temporarily"; /* The spec actually says "Found" */
            break;
        case PROTOCOL_SEE_OTHER:         // 303
            r = "See Other";
            break;
        case PROTOCOL_NOT_MODIFIED:      // 304
            r = "Use local copy";    /* The spec actually says "Not Modified" */
            break;
        case PROTOCOL_USE_PROXY:         // 305
            r = "Use Proxy";
            break;
        case PROTOCOL_TEMPORARY_REDIRECT: // 307
            r = "Temporary Redirect"; 
            break;
        case PROTOCOL_BAD_REQUEST:        // 400
            r = "Bad request";
            break;
        case PROTOCOL_UNAUTHORIZED:       // 401
            r = "Unauthorized";
            break;
        case PROTOCOL_PAYMENT_REQUIRED:   // 402
            r = "Payment Required";
            break;
        case PROTOCOL_FORBIDDEN:          // 403
            r = "Forbidden";
            break;
        case PROTOCOL_NOT_FOUND:         // 404
            r = "Not found";
            break;
        case PROTOCOL_METHOD_NOT_ALLOWED: // 405                /* HTTP/1.1 */
            r = "Method Not Allowed";
            break;
        case PROTOCOL_NOT_ACCEPTABLE: // 406                /* HTTP/1.1 */
            r = "Not Acceptable";
            break;
        case PROTOCOL_PROXY_UNAUTHORIZED: // 407
            r = "Proxy Authentication Required";
            break;
        case PROTOCOL_REQUEST_TIMEOUT:    // 408            /* HTTP/1.1 */
            r = "Request Timeout";
            break;
        case PROTOCOL_CONFLICT:           // 409
            r = "Conflict";                         /* HTTP/1.1 */
            break;
        case PROTOCOL_GONE:           // 410
            r = "Gone";                         /* HTTP/1.1 */
            break;
        case PROTOCOL_LENGTH_REQUIRED:    // 411                /* HTTP/1.1 */
            r = "Length Required";
            break;
        case PROTOCOL_PRECONDITION_FAIL:  // 412                /* HTTP/1.1 */
            r = "Precondition Failed";
            break;
        case PROTOCOL_ENTITY_TOO_LARGE:   // 413                /* HTTP/1.1 */
            r = "Request Entity Too Large";
            break;
        case PROTOCOL_URI_TOO_LARGE:      // 414                /* HTTP/1.1 */
            r = "Request-URI Too Large";
            break;
        case PROTOCOL_UNSUPPORTED_MEDIA_TYPE: // 415
            r = "Unsupported Media Type";
            break;
        case PROTOCOL_REQUESTED_RANGE_NOT_SATISFIABLE: // 416
            r = "Requested range not satisfiable";
            break;
        case PROTOCOL_EXPECTATION_FAILED:     // 417
            r = "Expectation Failed";
            break;
        case PROTOCOL_LOCKED:     // 423
            r = "Locked";
            break;
        case PROTOCOL_FAILED_DEPENDENCY:     // 424
            r = "Failed Dependency";
            break;
        case PROTOCOL_SERVER_ERROR:           // 500
            r = "Server Error";           /* The spec actually says "Internal Server Error" */
            break;
        case PROTOCOL_NOT_IMPLEMENTED:        // 501
            r = "Not Implemented";
            break;
        case PROTOCOL_BAD_GATEWAY:            // 502
            r = "Bad Gateway";
            break;
        case PROTOCOL_SERVICE_UNAVAILABLE:    // 503
            r = "Service Unavailable";
            break;
        case PROTOCOL_GATEWAY_TIMEOUT:        // 504            /* HTTP/1.1 */
            r = "Gateway Timeout";
            break;
        case PROTOCOL_VERSION_NOT_SUPPORTED:  // 505            /* HTTP/1.1 */
            r = "HTTP Version Not Supported";
            break;
        case PROTOCOL_INSUFFICIENT_STORAGE:  // 507            
            r = "Insufficient Storage";
            break;
        default:
            switch (code / 100)
            {
                case 1:
                    r = "Information";
                    break;
                case 2:
                    r = "Success";
                    break;
                case 3:
                    r = "Redirect";
                    break;
                case 4:
                    r = "Client error";
                    break;
                case 5:
                    r = "Server error";
                    break;
                default:
                    r = "Unknown reason";
                    break;
            }
            break;
    }

    return r;
}


/* ------------------------- http_finish_request -------------------------- */

NSAPI_PUBLIC void http_finish_request(Session *sn, Request *rq) 
{
    httpfilter_finish_request(sn, rq);
}


/* ----------------------------- http_uri2url ----------------------------- */

NSAPI_PUBLIC char *http_uri2url_dynamic(const char *prefix, const char *suffix, Session *sn, Request *rq)
{
    const char* httpx_url;
    const char* host;
    PRUint16 port;

    GetUrlComponents(rq, &httpx_url, &host, &port);

    register char *u;
    int httpx_url_len, host_len, prefix_len, suffix_len;
    int len;

    httpx_url_len = strlen(httpx_url);
    host_len = strlen(host);
    prefix_len = strlen(prefix);
    suffix_len = strlen(suffix);

    u = (char *) MALLOC(httpx_url_len + 3 + host_len + 17 + prefix_len +
    suffix_len + 1);

    memcpy(u, httpx_url, httpx_url_len);
    len = httpx_url_len + 3;
    u[len - 3] = ':';
    u[len - 2] = '/';
    u[len - 1] = '/';
    memcpy(&u[len], host, host_len);
    len += host_len;
    if (port != 0) {
        u[len++] = ':';
        len += util_itoa(port, &u[len]);
    }
    memcpy(&u[len], prefix, prefix_len);
    len += prefix_len;
    memcpy(&u[len], suffix, suffix_len);
    u[len + suffix_len] = 0;
    return u;
}

NSAPI_PUBLIC char *http_uri2url(const char *prefix, const char *suffix)
{
    return http_uri2url_dynamic(prefix, suffix, NULL, NULL);
}

NSAPI_PUBLIC HttpRequest* GetHrq(const Request* rq)
{
    if (!rq)
        return HttpRequest::CurrentRequest();

    Request* origRq = rq->orig_rq;
    PR_ASSERT(origRq);

    NSAPIRequest* nsapiRq = (NSAPIRequest*)origRq;
    PR_ASSERT(nsapiRq);

    HttpRequest* hrq = nsapiRq->hrq;

    return hrq;
};

NSAPI_PUBLIC PRBool GetSecurity(const Session* sn)
{
    // We allow NSAPI plugins to fake secure mode by inserting "keysize"
    return (sn->ssl || pblock_findkey(pb_key_keysize, sn->client));
}

NSAPI_PUBLIC const SSLSocketConfiguration* GetSecurityParams(const Request* rq)
{
    const SSLSocketConfiguration* sslc = NULL;

    const HttpRequest* hrq = GetHrq(rq);

    if (hrq)
    {
        const DaemonSession& ds = hrq->GetDaemonSession();
    
        // we first need to check if SSL was enabled on the listen socket
    
        PR_ASSERT(ds.conn->lsConfig);
        ListenSocketConfig* lsc = ds.conn->lsConfig;
        if (lsc->ssl.enabled)
        {
            // then we need to get the SSL parameters for the DaemonSession
            sslc = ds.conn->sslconfig;
        };
    };
    return sslc;
}


/* -------------------- http_get_scheme_hostname_ports -------------------- */

static void http_get_scheme_hostname_ports(const Request *rq, const char **scheme, const char **hostname, PRUint16 *port, PRUint16 *scheme_port)
{
    const HttpRequest* hrq = GetHrq(rq);
    if (!hrq) {
        conf_global_vars_s* vars = conf_get_true_globals();

        *scheme = vars->Vsecurity_active ? HTTPS_URL : HTTP_URL;

        *hostname = vars->Vserver_hostname;

        *scheme_port = vars->Vsecurity_active ? HTTPS_PORT : HTTP_PORT;
        *port = vars->Vport;

        return;
    }

    const Session *sn = &hrq->GetNSAPISession()->sn;
    if (!rq)
        rq = &hrq->GetNSAPIRequest()->rq;

    const DaemonSession& ds = hrq->GetDaemonSession();
    ListenSocketConfig *lsc = ds.conn->lsConfig;
    const ServerName& server_name = lsc->getExternalServerName();

    if (server_name.hasExplicitScheme()) {
        *scheme = server_name.getScheme();
        *scheme_port = server_name.getSchemePort();
    } else if (GetSecurity(sn)) {
        *scheme = HTTPS_URL;
        *scheme_port = HTTPS_PORT;
    } else {
        *scheme = HTTP_URL;
        *scheme_port = HTTP_PORT;
    }

    const char *host_header = pblock_findkeyval(pb_key_host, rq->headers);
    if (host_header) {
        // Client sent a Host: header.  Use it to get hostname and port.
        const char *port_suffix = util_host_port_suffix(host_header);
        if (port_suffix) {
            char *host_copy = pool_strdup(sn->pool, host_header);
            host_copy[port_suffix - host_header] = '\0';
            *hostname = host_copy;
            *port = atoi(port_suffix + 1);
        } else {
            *hostname = host_header;
            *port = *scheme_port;
        }
    } else {
        // Client didn't send a Host: header.  Get values from the listener.
        *hostname = server_name.getHostname();
        *port = server_name.getPort();
    }
}

NSAPI_PUBLIC int GetServerPort(const Request* rq)
{
    const char *s;
    const char *h;
    PRUint16 p;
    PRUint16 sp;
    http_get_scheme_hostname_ports(rq, &s, &h, &p, &sp);

    return p;
}

NSAPI_PUBLIC const char* GetServerHostname(const Request* rq)
{
    const char *s;
    const char *h;
    PRUint16 p;
    PRUint16 sp;
    http_get_scheme_hostname_ports(rq, &s, &h, &p, &sp);

    return h;
}

NSAPI_PUBLIC void GetUrlComponents(const Request* rq, const char** scheme, const char** hostname, PRUint16* port)
{
    // Retrieve the scheme, hostname, and port to use in self-referencing URLs.
    // Note that *port will be 0 if the URL should omit the port number (e.g.
    // http on port 80).
    PRUint16 scheme_port;
    http_get_scheme_hostname_ports(rq, scheme, hostname, port, &scheme_port);
    if (*port == scheme_port)
        *port = 0;
}

NSAPI_PUBLIC void
GetServerHostnameAndPort(const Request& rq, const Session& sn, 
                         NSString& srvName, NSString& port)
{
    const char *s;
    const char *h;
    PRUint16 p;
    PRUint16 sp;
    http_get_scheme_hostname_ports(&rq, &s, &h, &p, &sp);

    srvName.clear();
    srvName.append(h);

    port.clear();
    port.printf("%d", p);
}


/* ----------------------- http_canonical_redirect ------------------------ */

NSAPI_PUBLIC int http_canonical_redirect(Session *sn, Request *rq)
{
    const VirtualServer *vs = request_get_vs(rq);

    // Most virtual servers won't enforce a canonical server name, so take the
    // fast path out in those cases
    const ServerName *server_name = vs->getNormalizedCanonicalServerName();
    if (!server_name)
        return REQ_NOACTION;

    // Don't try to redirect requests that weren't generated by a client
    if (INTERNAL_REQUEST(rq))
        return REQ_NOACTION;

    // Responses that vary based on the the value of the Host: header cannot
    // be cached by the accelerator cache
    rq->request_is_cacheable = 0;

    // Don't try to redirect clients that don't know how to say which hostname
    // they're talking to
    if (!pblock_findkey(pb_key_host, rq->headers))
        return REQ_NOACTION;

    PRBool need_redirect = PR_FALSE;

    // Get the URL the client used
    const char *scheme;
    const char *hostname;
    PRUint16 port;
    PRUint16 scheme_port;
    http_get_scheme_hostname_ports(rq, &scheme, &hostname, &port, &scheme_port);

    // Check the scheme the client used
    if (server_name->hasExplicitScheme()) {
        if (strcasecmp(scheme, server_name->getScheme())) {
            // Client used noncanonical scheme
            scheme = server_name->getScheme();
            port = server_name->getPort();
            scheme_port = server_name->getSchemePort();
            need_redirect = PR_TRUE;
        }
    }

    // Check the hostname the client used
    if (strcasecmp(hostname, server_name->getHostname())) {
        // Client used noncanonical hostname
        hostname = server_name->getHostname();
        need_redirect = PR_TRUE;
    }

    // Check the port the client used
    if (server_name->hasExplicitPort()) {
        if (port != server_name->getPort()) {
            // Client used noncanonical port
            port = server_name->getPort();
            need_redirect = PR_TRUE;
        }
    }

    if (!need_redirect)
        return REQ_NOACTION;
    
    // Get the path and query string the client used
    const char *uri = pblock_findkeyval(pb_key_escaped, rq->vars);
    const char *query = NULL;
    if (!uri) {
        uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
        if (uri)
            uri = util_uri_escape(NULL, uri);
        query = pblock_findkeyval(pb_key_query, rq->reqpb);
    }

    int scheme_len = strlen(scheme);
    int hostname_len = strlen(hostname);
    int uri_len = uri ? strlen(uri) : 0;
    int query_len = query ? strlen(query) : 0;
    int max_len = scheme_len + 3 /* "://" */ +
                  hostname_len + 6 /* ":65535" */ +
                  uri_len + 1 /* "?" */ + query_len;

    char *url = (char *) pool_malloc(sn->pool, max_len);
    if (!url)
        return REQ_ABORTED;

    // Format the canonical URL based on both the canonical server name and the
    // URL the client used
    int pos = 0;
    memcpy(&url[pos], scheme, scheme_len);
    pos += scheme_len;
    url[pos++] = ':';
    url[pos++] = '/';
    url[pos++] = '/';
    memcpy(&url[pos], hostname, hostname_len);
    pos += hostname_len;
    if (port != scheme_port) {
        url[pos++] = ':';
        pos += util_itoa(port, &url[pos]);
    }
    memcpy(&url[pos], uri, uri_len);
    pos += uri_len;
    if (query) {
        url[pos++] = '?';
        memcpy(&url[pos], query, query_len);
        pos += query_len;
    }

    // Tell the Error stage to redirect the client to the canonical URL
    protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
    pblock_kvinsert(pb_key_location, url, pos, rq->srvhdrs);

    return REQ_ABORTED;
}


/* ------------------------- http_unchunk_request ------------------------- */

NSAPI_PUBLIC int http_unchunk_request(Session *sn, Request *rq, int size, int timeout)
{
    if (sn->csd_open != 1)
        return REQ_EXIT;

    // There's nothing to do if there's already a Content-length header
    if (pblock_findkeyval(pb_key_content_length, rq->headers))
        return REQ_NOACTION;

    // There's nothing to do if there's no Transfer-encoding header (Note that
    // httpfilter will set transfer-encoding="identity" if there was any
    // Transfer-encoding header because it removes chunk separators)
    if (!pblock_findkeyval(pb_key_transfer_encoding, rq->headers))
        return REQ_NOACTION;

    // Set defaults for size and timeout if necessary
    if (size < 0)
        size = _ChunkedRequestBufferSize;
    if (timeout < 0)
        timeout = _ChunkedRequestTimeout;

    // Don't calculate Content-length if ChunkedRequestBufferSize=0
    if (size < 1)
        return REQ_NOACTION;

    // Tell httpfilter we don't want more than size bytes.  This will help it
    // reject bogus chunks early.
    httpfilter_set_request_body_limit(sn, rq, size);

    // Reset pos and cursize to avoid overallocating when the inbuf is empty
    if (sn->inbuf->pos == sn->inbuf->cursize) {
        sn->inbuf->pos = 0;
        sn->inbuf->cursize = 0;
    }

    // Resize the Session inbuf if it doesn't have room for size bytes
    int maxsize = sn->inbuf->cursize + size;
    if (!sn->inbuf->inbuf || maxsize > sn->inbuf->maxsize) {
        unsigned char *inbuf = (unsigned char *)pool_realloc(sn->pool, sn->inbuf->inbuf, maxsize);
        if (!inbuf) {
            protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
            return REQ_ABORTED;
        }
        sn->inbuf->inbuf = inbuf;
        sn->inbuf->maxsize = maxsize;
    }

    // rdtimeout is the per-net_read() timeout, timeout is the timeout for the
    // entire body
    int epoch = ft_time();
    int rdtimeout = sn->inbuf->rdtimeout;
    if (sn->inbuf->rdtimeout > timeout)
        sn->inbuf->rdtimeout = timeout;

    // Read data into the buffer.  httpfilter will consume the chunk separators
    // for us.
    unsigned char *buffer = sn->inbuf->inbuf + sn->inbuf->cursize;
    int content_length = 0;
    int rv = -1;
    do {
        rv = net_read(sn->csd, buffer + content_length, size - content_length, sn->inbuf->rdtimeout);
        if (rv < 1)
            break;

        if (ft_time() - epoch > timeout) {
            // We've been in here too long, bail out
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
            rv = -1;
            break;
        }

        content_length += rv;
    } while (content_length < size);

    // Restore original rdtimeout
    sn->inbuf->rdtimeout = rdtimeout;

    // If there was an error...
    if (rv == -1) {
        if (PR_GetError() == PR_IO_TIMEOUT_ERROR) {
            protocol_status(sn, rq, PROTOCOL_REQUEST_TIMEOUT, NULL);
        } else {
            protocol_status(sn, rq, PROTOCOL_LENGTH_REQUIRED, NULL);
        }
        return REQ_ABORTED;
    }

    // If we completely filled the buffer...
    if (content_length > 0 && content_length == size) {
        rv = net_read(sn->csd, buffer, 1, sn->inbuf->rdtimeout);
        if (rv != 0) {
            // We couldn't fit the entire request entity body in our buffer
            protocol_status(sn, rq, PROTOCOL_LENGTH_REQUIRED, NULL);
            return REQ_ABORTED;
        }
    }

    // Generate a content-length header
    char content_length_buffer[sizeof("18446744073709551615")];
    int content_length_len = util_itoa(content_length, content_length_buffer);
    pblock_kvinsert(pb_key_content_length, content_length_buffer,
                    content_length_len, rq->headers);

    // Let others (e.g. the Service SAF) know about the unchunked request
    // entity body data
    sn->inbuf->cursize += content_length;

    return REQ_PROCEED;
}


/* -------------------------- http_parse_request -------------------------- */

NSAPI_PUBLIC int http_parse_request(char *t, Request *rq, Session *sn)
{
    /*
     * Starting with Sun ONE Web Server 6.1, http_parse_request("", NULL, NULL)
     * returns the NSAPI version.  Previous web server releases return
     * REQ_ABORTED.
     */
    if (!*t)
        return NSAPI_VERSION;

    PR_ASSERT(0);
    return REQ_ABORTED;
}


/* ------------------------- http_get_method_num -------------------------- */

NSAPI_PUBLIC int http_get_method_num(const char *method)
{
    int method_num = -1;

    if (method)
        method_num = HttpMethodRegistry::GetRegistry().GetMethodIndex(method);

    return method_num;
}
