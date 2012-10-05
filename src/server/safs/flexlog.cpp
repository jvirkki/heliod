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
 * flexlog.cpp: Code to handle flexible log file format
 *
 * Don Eastman, imported by Mike McCool 
 */

#include "base/ereport.h"
#include "base/vs.h"
#include "frame/accel.h"
#include "frame/conf.h"
#include "safs/dbtsafs.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/connqueue.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/logmanager.h"
#include "generated/ServerXMLSchema/AccessLogMode.h"
#include "flexlogcommon.h"

/* Check binlog.cpp for binary mode file format */

/*
 * DEFAULT_LOG_NAME is the log name used when none is explicitly specified.
 */
#define DEFAULT_LOG_NAME "default"

/*
 * DOT_DOT_DOT_LEN is the length of the "..." suffix we attach to truncated
 * formatted tokens.
 */
#define DOT_DOT_DOT_LEN 3

/*
 * COMMON_FORMAT is the default magnus.conf flex-init format string.
 */
#define COMMON_FORMAT "%Ses->client.ip% - %Req->vars.auth-user% [%SYSDATE%] \"%Req->reqpb.clf-request%\" %Req->srvhdrs.clf-status% %Req->srvhdrs.content-length%"

/* 
 * TOKENS_ARRAY_INCSIZE is the allocation quantum for the tokens[] array.
 */
#define TOKENS_ARRAY_INCSIZE 8

/*
 * FlexTemplate is a log file template defined by the flex-init SAF.
 */
struct FlexTemplate {
    FlexTemplate *next; // next FlexTemplate in flex_template_list
    char *name;         // name of the log file (e.g. "access")
    char *filename;     // filename template (may contain $variables)
    FlexFormat *format; // pointer to a reference counted FlexFormat
};

/*
 * FlexLog defines a VS-specific instance of a log file.
 */
struct FlexLog {
    FlexLog *next;      // next FlexLog in flex_vs_slot's list
    char *name;         // name of the log file (e.g. "access")
    FlexFormat *format; // pointer to a reference counted FlexFormat
    time_t epoch;       // base time for %RELATIVETIME% values
    PRBool enabled;     // whether the log should be written to
    LogFile *file;      // pointer to a reference counted LogFile
};

/*
 * FlexFormatted is the result of evaluating a FlexToken.
 */
struct FlexFormatted {
    const char *p;
    int len;
    PRBool appendNull;
};

/*
 * flex_format_lock serializes construction of FlexFormats.
 */
static PRLock *flex_format_lock;

/*
 * flex_format_list is the head of the linked list of FlexFormats.
 */
static FlexFormat *flex_format_list;

/*
 * flex_template_list is the head of the linked list of FlexTemplates defined
 * by the flex-init SAF.
 */
static FlexTemplate *flex_template_list;

/*
 * flex_vs_lock serializes the creation of FlexLogs.
 */
static PRLock *flex_vs_lock;

/*
 * flex_vs_slot is a vs_get_data/vs_set_data slot that stores a pointer to the
 * head of a linked list of FlexLogs.
 */
static int flex_vs_slot = -1;

/*
 * flex_max_seconds is the maximum number of seconds that will fit in a
 * PR_IntervalTime.
 */
static const int flex_max_seconds = PR_IntervalToSeconds(0x7fffffff);

/*
 * flex_us_per_tick is the number of microseconds per PRIntervalTime.
 */
static const int flex_us_per_tick = PR_IntervalToMicroseconds(1);

static VSInitFunc flex_init_vs;
static VSDestroyFunc flex_destroy_vs;
static VSDirectiveInitFunc flex_init_vs_directive;


/* ---------------------------- flex_init_late ---------------------------- */

void flex_init_late(void)
{
    PR_ASSERT(flex_vs_slot == -1);

    if (flex_vs_slot == -1) {
        flex_vs_slot = vs_alloc_slot();
        flex_vs_lock = PR_NewLock();
        flex_format_lock = PR_NewLock();
        vs_register_cb(flex_init_vs, flex_destroy_vs);
        vs_directive_register_cb(flex_log, flex_init_vs_directive, 0);
    }
}


/* --------------------------- flex_token_accel --------------------------- */

static inline PRBool flex_token_accel(FlexTokenType type)
{
    // N.B. this list must be consistent with flex_format_accel

    switch (type) {
    case TOKEN_TEXT_OFFSET:
    case TOKEN_TEXT_POINTER:
    case TOKEN_SYSDATE:
    case TOKEN_LOCALEDATE:
    case TOKEN_TIME:
    case TOKEN_RELATIVETIME:
    case TOKEN_IP:
    case TOKEN_METHOD_NUM:
    case TOKEN_PROTV_NUM:
    case TOKEN_REQUEST_LINE:
    case TOKEN_REQUEST_LINE_URI:
    case TOKEN_REQUEST_LINE_URI_ABS_PATH:
    case TOKEN_REQUEST_LINE_URI_QUERY:
    case TOKEN_REQUEST_LINE_PROTOCOL:
    case TOKEN_REQUEST_LINE_PROTOCOL_NAME:
    case TOKEN_REQUEST_LINE_PROTOCOL_VERSION:
    case TOKEN_METHOD:
    case TOKEN_STATUS_CODE:
    case TOKEN_CONTENT_LENGTH:
    case TOKEN_REFERER:
    case TOKEN_USER_AGENT:
    case TOKEN_AUTH_USER:
    case TOKEN_VSID:
    case TOKEN_SUBSYSTEM:
    case TOKEN_RECORD_SIZE:
        return PR_TRUE;

    default:
        return PR_FALSE;
    }
}


/* ---------------------------- flex_add_token ---------------------------- */

static FlexToken *flex_add_token(FlexFormat *f, FlexTokenType type)
{
    if ((f->ntokens % TOKENS_ARRAY_INCSIZE) == 0) {
        int n = f->ntokens + TOKENS_ARRAY_INCSIZE;
        FlexToken *t = (FlexToken *) PERM_REALLOC(f->tokens, n * sizeof(FlexToken));
        if (!t)
            return NULL;
        f->tokens = t;
    }

    FlexToken *t = &f->tokens[f->ntokens++];
    t->type = type;
    t->p = NULL;
    t->offset = 0;
    t->len = 0;
    t->model = NULL;
    t->key = NULL;

    if (!flex_token_accel(type))
        f->accel = PR_FALSE;

    return t;
}


/* ------------------------- flex_add_text_token -------------------------- */

static void flex_add_text_token(FlexFormat *f, const char *p)
{
    char *found = strstr(f->format, p);
    if (found) {
        FlexToken *t = flex_add_token(f, TOKEN_TEXT_OFFSET);
        if (t) {
            t->offset = found - f->format;
            t->len = strlen(p);
        }
    } else {
        FlexToken *t = flex_add_token(f, TOKEN_TEXT_POINTER);
        if (t) {
            t->p = PERM_STRDUP(p);
            t->len = t->p ? strlen(t->p) : 0;
        }
    }
}


/* -------------------------- flex_add_pb_token --------------------------- */

static PRBool flex_add_pb_token(FlexFormat *f, const char *p)
{
    struct Prefix {
        const char *p;
        int len;
        FlexTokenType key;
        FlexTokenType name;
    };

    Prefix prefixes[] = {
#define PREFIX(s, t) { s, sizeof(s) - 1, t##_KEY, t##_NAME }
        PREFIX("pblock.", TOKEN_PB),
        PREFIX("Ses->client.", TOKEN_SN_CLIENT),
        PREFIX("Req->vars.", TOKEN_RQ_VARS),
        PREFIX("Req->reqpb.", TOKEN_RQ_REQPB),
        PREFIX("Req->headers.", TOKEN_RQ_HEADERS),
        PREFIX("Req->srvhdrs.", TOKEN_RQ_SRVHDRS)
#undef PREFIX
    };

    for (int i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        if (!strncmp(p, prefixes[i].p, prefixes[i].len)) {
            const char *name = p + prefixes[i].len;
            const pb_key *key = pblock_key(name);

            if (key) {
                FlexToken *t = flex_add_token(f, prefixes[i].key);
                if (t)
                    t->key = key;
            } else {
                FlexToken *t = flex_add_token(f, prefixes[i].name);
                if (t)
                    t->p = PERM_STRDUP(name);
            }

            return PR_TRUE;
        }
    }

    return PR_FALSE;
}


/* ----------------------- flex_scan_percent_token ------------------------ */

extern "C" NSAPI_PUBLIC
const char *flex_scan_percent_token(FlexFormat *f, const char *p)
{
    PR_ASSERT(*p == '%');

    if (p[1] == '%') { // "%%" is an escaped '%'
        flex_add_text_token(f, "%");
        return p + 2;
    }
    
    const char *end = strchr(p + 1, '%');
    if (!end) { // missing closing '%'
        flex_add_text_token(f, "%");
        return p + 1;
    }

    NSString token(p + 1, end - p - 1);

    if (!strcmp(token, SYSDATE)) {
        flex_add_token(f, TOKEN_SYSDATE);
    } else if (!strcmp(token, LOCALEDATE)) {
        flex_add_token(f, TOKEN_LOCALEDATE);
    } else if (!strcmp(token, TIME)) {
        flex_add_token(f, TOKEN_TIME);
    } else if (!strcmp(token, RELATIVETIME)) {
        flex_add_token(f, TOKEN_RELATIVETIME);
    } else if (!strcmp(token, SES_CLIENT_IP)) {
        flex_add_token(f, net_enabledns ? TOKEN_DNS : TOKEN_IP);
    } else if (!strcmp(token, REQ_METHOD_NUM)) {
        flex_add_token(f, TOKEN_METHOD_NUM);
    } else if (!strcmp(token, REQ_PROTV_NUM)) {
        flex_add_token(f, TOKEN_PROTV_NUM);
    } else if (!strcmp(token, REQ_REQPB_CLF_REQUEST)) {
        flex_add_token(f, TOKEN_REQUEST_LINE);
    } else if (!strcmp(token, REQ_REQPB_CLF_REQUEST_METHOD)) {
        flex_add_token(f, TOKEN_METHOD);
    } else if (!strcmp(token, REQ_REQPB_CLF_REQUEST_URI)) {
        flex_add_token(f, TOKEN_REQUEST_LINE_URI);
    } else if (!strcmp(token, REQ_REQPB_CLF_REQUEST_URI_ABS_PATH)) {
        flex_add_token(f, TOKEN_REQUEST_LINE_URI_ABS_PATH);
    } else if (!strcmp(token, REQ_REQPB_CLF_REQUEST_URI_QUERY)) {
        flex_add_token(f, TOKEN_REQUEST_LINE_URI_QUERY);
    } else if (!strcmp(token, REQ_REQPB_CLF_REQUEST_PROTOCOL)) {
        flex_add_token(f, TOKEN_REQUEST_LINE_PROTOCOL);
    } else if (!strcmp(token, REQ_REQPB_CLF_REQUEST_PROTOCOL_NAME)) {
        flex_add_token(f, TOKEN_REQUEST_LINE_PROTOCOL_NAME);
    } else if (!strcmp(token, REQ_REQPB_CLF_REQUEST_PROTOCOL_VERSION)) {
        flex_add_token(f, TOKEN_REQUEST_LINE_PROTOCOL_VERSION);
    } else if (!strcmp(token, REQ_REQPB_METHOD)) {
        flex_add_token(f, TOKEN_METHOD);
    } else if (!strcmp(token, REQ_SRVHDRS_CLF_STATUS)) {
        flex_add_token(f, TOKEN_STATUS_CODE);
    } else if (!strcmp(token, REQ_SRVHDRS_STATUS_CODE)) {
        flex_add_token(f, TOKEN_STATUS_CODE);
    } else if (!strcmp(token, REQ_SRVHDRS_STATUS_REASON)) {
        flex_add_token(f, TOKEN_STATUS_REASON);
    } else if (!strcmp(token, REQ_SRVHDRS_CONTENT_LENGTH)) {
        flex_add_token(f, TOKEN_CONTENT_LENGTH);
    } else if (!strcmp(token, REQ_HEADERS_REFERER)) {
        flex_add_token(f, TOKEN_REFERER);
    } else if (!strcmp(token, REQ_HEADERS_USER_AGENT)) {
        flex_add_token(f, TOKEN_USER_AGENT);
    } else if (!strcmp(token, REQ_VARS_AUTH_USER)) {
        flex_add_token(f, TOKEN_AUTH_USER);
    } else if (!strcmp(token, VSID)) {
        flex_add_token(f, TOKEN_VSID);
    } else if (!strcmp(token, SUBSYSTEM)) {
        flex_add_token(f, TOKEN_SUBSYSTEM);
    } else if (!strcmp(token, DURATION)) {
        flex_add_token(f, TOKEN_DURATION);
    } else if (!strncmp(token, REQ_HEADERS_COOKIE_PREFIX, REQ_HEADERS_COOKIE_PREFIX_LEN)) {
        FlexToken *t = flex_add_token(f, TOKEN_COOKIE);
        if (t) {
            t->p = PERM_STRDUP(token + REQ_HEADERS_COOKIE_PREFIX_LEN);
            t->len = t->p ? strlen(t->p) : 0;
        }
    } else {
        if (!flex_add_pb_token(f, token)) {
            flex_add_text_token(f, "%");
            return p + 1;
        }
    }

    return end + 1;
}


/* ------------------------ flex_scan_dollar_token ------------------------ */

extern "C" NSAPI_PUBLIC
const char *flex_scan_dollar_token(FlexFormat *f, const char *p)
{
    PR_ASSERT(*p == '$');

    if (p[1] == '$') { // "$$" is an escaped '$'
        flex_add_text_token(f, "$");
        return p + 2;
    }

    int len = model_fragment_scan(p);
    if (len < 1) {
        flex_add_text_token(f, "$");
        return p + 1;
    }

    NSString interpolative(p, len);

    FlexToken *t = flex_add_token(f, TOKEN_MODEL);
    if (t)
        t->model = model_str_create(interpolative);

    return p + len;
}


/* ------------------------- flex_scan_text_token ------------------------- */

extern "C" NSAPI_PUBLIC
const char *flex_scan_text_token(FlexFormat *f, const char *p)
{
    NSString text;

    while (*p) {
        if (*p == '%')
            break;
        if (*p == '$')
            break;
        text.append(*p);
        p++;
    }

    if (text.length() > 0)
        flex_add_text_token(f, text.data());

    return p;
}


/* ------------------------- flex_acquire_format -------------------------- */

static FlexFormat *flex_acquire_format(const char *format,
                                       PRBool binary = PR_FALSE)
{
    PR_Lock(flex_format_lock);

    // Look for an existing FlexFormat with the requested format string
    FlexFormat *f = flex_format_list;
    while (f) {
        if (!strcmp(f->format, format)) {
            f->refcount++; // reference for caller
            break;
        }
        f = f->next;
    }

    if (!f) {
        // Create a new FlexFormat for the requested format string
        f = (FlexFormat *) PERM_MALLOC(sizeof(FlexFormat));
        if (f) {
            f->format = PERM_STRDUP(format);
            f->tokens = NULL;
            f->ntokens = 0;
            f->accel = PR_TRUE;
            f->refcount = 1; // reference for caller

            // In binary log first write record length and then the line
            if (binary)
                flex_add_token(f, TOKEN_RECORD_SIZE);

            // Parse the format string into tokens
            const char *p = f->format;
            while (*p) {
                if (*p == '%') {
                    p = flex_scan_percent_token(f, p);
                } else if (*p == '$') {
                    p = flex_scan_dollar_token(f, p);
                } else {
                    p = flex_scan_text_token(f, p);
                }
            }

            // Add this new FlexFormat to the server-wide list
            f->next = flex_format_list;
            flex_format_list = f;
        }
    }

    PR_Unlock(flex_format_lock);

    return f;
}


/* ------------------------- flex_release_format -------------------------- */

static void flex_release_format(FlexFormat *f)
{
    PR_Lock(flex_format_lock);

    // Release caller's reference
    f->refcount--;

    // If the caller had the last reference...
    if (f->refcount == 0) {
        // Remove the FlexFormat from flex_format_list
        FlexFormat *parent = flex_format_list;
        while (parent && parent->next != f)
            parent = parent->next;
        if (parent) {
            parent->next = f->next;
        } else {
            flex_format_list = f->next;
        }

        // Destroy the FlexFormat
        for (int ti = 0; ti < f->ntokens; ti++) {
            PERM_FREE(f->tokens[ti].p);
            model_str_free(f->tokens[ti].model);
        }
        PERM_FREE(f->tokens);
        PERM_FREE(f->format);
        PERM_FREE(f);
    }

    PR_Unlock(flex_format_lock);
}


/* ------------------------------ flex_init ------------------------------- */

int flex_init(pblock *pb, Session *sn, Request *rq)
{
    if (!conf_is_late_init(pb)) {
        // We want to run LateInit (after the setuid)
        pblock_nvinsert("LateInit", "yes", pb);
        return REQ_PROCEED;
    }

    PR_ASSERT(flex_vs_slot != -1);

    param_free(pblock_remove("server-version", pb));
    param_free(pblock_remove("LateInit", pb));

    LogManager::setParams(pb);

    for (int i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        while (p) {
            const char *t = p->param->name;

            // because the "name" of the log is specified by the user, we don't know what it is
            // except for that it isn't one of the well-known tokens.  This sucks!!!!
            if (strcmp(t, "fn") && 
                strncmp(t, "format.", strlen("format.")) &&
                strncmp(t, "delimit.", strlen("delimit.")) &&
                strncmp(t, "relaxed", strlen("relaxed")) &&
                !LogManager::isParam(t))
            {
                NSString format_name;
                format_name.printf("format.%s", t);
                const char *format = pblock_findval(format_name, pb);

                FlexTemplate *ft = (FlexTemplate *) PERM_MALLOC(sizeof(FlexTemplate));
                ft->name = PERM_STRDUP(t);
                ft->filename = PERM_STRDUP(p->param->value);
                ft->format = flex_acquire_format(format ? format : COMMON_FORMAT);

                ft->next = flex_template_list;
                flex_template_list = ft;
            }

            p = p->next;
        }
    }

    return REQ_PROCEED;
}


/* --------------------------- flex_skip_prefix --------------------------- */

static inline const char *flex_skip_prefix(const char *p, char separator)
{
    if (!p)
        return NULL;

    while (*p && *p != separator)
        p++;

    if (*p != separator)
        return NULL;

    return p + 1;
}


/* --------------------------- flex_skip_method --------------------------- */

static inline const char *flex_skip_method(const char *line)
{
    if (!line)
        return NULL;

    const char *method = line;
    while (isspace(*method))
        method++;

    const char *space = method;
    while (*space && !isspace(*space))
        space++;

    if (!isspace(*space))
        return NULL;

    const char *abs_path = space;
    while (isspace(*abs_path))
        abs_path++;

    return abs_path;
}


/* -------------------- flex_get_request_line_protocol -------------------- */

static inline const char *flex_get_request_line_protocol(Request *rq)
{
    const char *line = pblock_findkeyval(pb_key_clf_request, rq->reqpb);

    const char *abs_path = flex_skip_method(line);
    if (!abs_path)
        return NULL;

    const char *space = abs_path;
    while (*space && !isspace(*space))
        space++;

    if (!isspace(*space))
        return NULL;

    const char *protocol = space;
    while (isspace(*protocol))
        protocol++;

    return protocol;
}

/* -------------------------- flex_format_binary -------------------------- */
/* T can be anything like short or PRInt64 */
template <class T>
static inline int flex_format_binary(pool_handle_t *pool, T i, const char **p)
{
    int len = sizeof(T);
    char *buf = (char *)pool_malloc(pool, len);
    if (!buf)
        return -1;

    memcpy((void *)buf, &i, len);
    *p = buf;
    return len;
}

/* --------------------------- flex_format_date --------------------------- */

static inline int flex_format_date(pool_handle_t *pool, date_format_t *format, const char **p)
{
    const int size = 64; // big enough for CLF or any "reasonable" locale
    char *buf = (char *) pool_malloc(pool, size);
    if (!buf)
        return -1;

    int len = date_current_formatted(format, buf, size);
    if (len > 0) {
        *p = buf;
        return len;
    }

    return -1;
}

/* --------------------------- flex_format_time --------------------------- */

extern "C" NSAPI_PUBLIC
int flex_format_time(pool_handle_t *pool, time_t tim, int type, const char **p)
{
    const int size = 64; // big enough for CLF or any "reasonable" locale
    char *buf = (char *) pool_malloc(pool, size);
    if (!buf)
        return -1;

    int len = 0;
    switch(type) {
        case TOKEN_SYSDATE :
            len = date_format_time(tim, date_format_clf, buf, size);
            break;
        case TOKEN_LOCALEDATE :
            len = date_format_time(tim, date_format_locale, buf, size);
            break;
        default :
            break;
    }

    if (len <= 0) {
        pool_free(pool, (void *)buf);
        buf = NULL;
    }

    *p = buf;
    return len;
}


/* ---------------------------- flex_format_64 ---------------------------- */

static inline int flex_format_64(pool_handle_t *pool, PRInt64 v, const char **p)
{
    char *buf = (char *) pool_malloc(pool, UTIL_I64TOA_SIZE);
    if (!buf)
        return -1;

    *p = buf;

    return util_i64toa(v, buf);
}

/* ------------------------ flex_format_64_wrapper ------------------------ */
extern "C" NSAPI_PUBLIC
int flex_format_64_wrapper(pool_handle_t *pool, PRInt64 v, const char **p)
{
    return flex_format_64(pool, v, p);
}


/* ------------------------ flex_format_method_num ------------------------ */

static inline int flex_format_method_num(pool_handle_t *pool, int method_num, const char **p)
{
    PR_ASSERT(METHOD_GET == 1);

    if (method_num == 1) {
        *p = "1";
        return 1;
    }

    char *buf = (char *) pool_malloc(pool, UTIL_ITOA_SIZE);
    if (!buf)
        return -1;

    *p = buf;

    return util_itoa(method_num, buf);
}


/* ------------------------ flex_format_protv_num ------------------------- */

static inline int flex_format_protv_num(pool_handle_t *pool, int protv_num, const char **p)
{
    PR_ASSERT(PROTOCOL_VERSION_HTTP11 == 101);

    if (protv_num == 101) {
        *p = "101";
        return 3;
    }

    if (protv_num == 100) {
        *p = "100";
        return 3;
    }

    char *buf = (char *) pool_malloc(pool, UTIL_ITOA_SIZE);
    if (!buf)
        return -1;

    *p = buf;

    return util_itoa(protv_num, buf);
}


/* ------------------------- flex_format_protocol ------------------------- */

static inline int flex_format_protocol(pool_handle_t *pool, int protv_num, const char **p)
{
    if (protv_num == PROTOCOL_VERSION_HTTP11) {
        *p = "HTTP/1.1";
        return 8;
    }

    if (protv_num == PROTOCOL_VERSION_HTTP10) {
        *p = "HTTP/1.0";
        return 8;
    }

    char *buf = (char *) pool_malloc(pool, 18);
    if (!buf)
        return -1;

    *p = buf;

    return util_snprintf(buf, 18, "HTTP/%d.%u", protv_num / 100, protv_num % 100);
}


/* ------------------------- flex_format_version -------------------------- */

static inline int flex_format_version(pool_handle_t *pool, int protv_num, const char **p)
{
    if (protv_num == PROTOCOL_VERSION_HTTP11) {
        *p = "1.1";
        return 3;
    }

    if (protv_num == PROTOCOL_VERSION_HTTP10) {
        *p = "1.0";
        return 3;
    }

    char *buf = (char *) pool_malloc(pool, 13);
    if (!buf)
        return -1;

    *p = buf;

    return util_snprintf(buf, 13, "%d.%u", protv_num / 100, protv_num % 100);
}


/* --------------------- flex_format_duration_binary --------------------- */

static inline int flex_format_duration_binary(Session *sn, Request *rq,
                                              const char **p)
{
    PRIntervalTime start_ticks;
    if (request_get_start_interval(rq, &start_ticks) != PR_SUCCESS)
        return -1;

    const int size = sizeof(PRUint64);
    char *buf = (char *) pool_malloc(sn->pool, size);
    if (!buf)
        return -1;

    *p = buf;

    if (rq->req_start) {
        int elapsed_s = ft_time() - rq->req_start;
        if (elapsed_s > flex_max_seconds / 2) {
            // Format duration as s*1000000 to avoid PRIntervalTime overflow
            PRUint64 t = elapsed_s*1000000;
            memcpy(buf, &t, size);
            return size;
        }
    }

    PRUint64 elapsed_ticks = PR_IntervalNow() - start_ticks;
    PRUint64 elapsed_us = elapsed_ticks * flex_us_per_tick;

    memcpy(buf, &elapsed_us, size);
    return size;
}

/* ------------------------- flex_format_duration ------------------------- */

static inline int flex_format_duration(Session *sn, Request *rq, const char **p)
{
    PRIntervalTime start_ticks;
    if (request_get_start_interval(rq, &start_ticks) != PR_SUCCESS)
        return -1;

    const int size = 15; // big enough for a year
    char *buf = (char *) pool_malloc(sn->pool, size);
    if (!buf)
        return -1;

    *p = buf;

    if (rq->req_start) {
        int elapsed_s = ft_time() - rq->req_start;
        if (elapsed_s > flex_max_seconds / 2) {
            // Format duration as s*1000000 to avoid PRIntervalTime overflow
            return PR_snprintf(buf, size, "%d000000", elapsed_s);
        }
    }

    PRUint64 elapsed_ticks = PR_IntervalNow() - start_ticks;
    PRUint64 elapsed_us = elapsed_ticks * flex_us_per_tick;

    return PR_snprintf(buf, size, "%llu", elapsed_us);
}


/* -------------------------- flex_format_cookie -------------------------- */

static inline int flex_format_cookie(Session *sn, Request *rq, const char *name, const char **p)
{
    const char *cookie = pblock_findkeyval(pb_key_cookie, rq->headers);
    if (!cookie)
        return -1;

    char *copy = pool_strdup(sn->pool, cookie);
    if (!copy)
        return -1;

    *p = util_cookie_find(copy, name);

    return -1;
}


/* ------------------------- flex_format_hhstring ------------------------- */

static inline int flex_format_hhstring(const HHString *hs, const char **p)
{
    int len;

    if (hs) {
        *p = hs->ptr;
        len = hs->len;
    } else {
        *p = NULL;
        len = 0;
    }

    return len;
}


/* -------------------------- flex_format_nsapi --------------------------- */

static inline int flex_format_nsapi(pblock *pb, Session *sn, Request *rq, FlexLog *log, FlexFormatted *formatted)
{
    const FlexFormat *f = log->format;
    int pos = 0;

    // Format tokens for a request served by NSAPI
    for (int ti = 0; ti < f->ntokens; ti++) {
        const FlexToken *t = &f->tokens[ti];

        const char *p = NULL;
        int len = -1;
        formatted[ti].appendNull = PR_FALSE;

        switch (t->type) {
        case TOKEN_TEXT_OFFSET:
            p = f->format + t->offset;
            len = t->len;
            break;

        case TOKEN_TEXT_POINTER:
            p = t->p;
            len = t->len;
            break;

        case TOKEN_MODEL:
            if (model_str_interpolate(t->model, sn, rq, &p, &len))
                log_error(LOG_VERBOSE, "flex-log", sn, rq, "error interpolating format (%s)", system_errmsg());
            break;

        case TOKEN_SYSDATE:
            len = flex_format_date(sn->pool, date_format_clf, &p);
            break;

        case TOKEN_LOCALEDATE:
            len = flex_format_date(sn->pool, date_format_locale, &p);
            break;

        case TOKEN_TIME:
            len = flex_format_64(sn->pool, ft_time(), &p);
            break;

        case TOKEN_RELATIVETIME:
            len = flex_format_64(sn->pool, (PRInt64) ft_time() - log->epoch, &p);
            break;

        case TOKEN_IP:
            p = pblock_findkeyval(pb_key_ip, sn->client);
            break;

        case TOKEN_DNS:
            if (!pblock_findkeyval(pb_key_iponly, pb))
                p = session_dns(sn);
            if (!p)
                p = pblock_findkeyval(pb_key_ip, sn->client);
            break;

        case TOKEN_METHOD_NUM:
            len = flex_format_method_num(sn->pool, rq->method_num, &p);
            break;

        case TOKEN_PROTV_NUM:
            len = flex_format_protv_num(sn->pool, rq->protv_num, &p);
            break;

        case TOKEN_REQUEST_LINE:
            p = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
            break;

        case TOKEN_REQUEST_LINE_URI:
            p = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
            p = flex_skip_method(p);
            if (p)
                for (len = 0; p[len] && !isspace(p[len]); len++);
            break;

        case TOKEN_REQUEST_LINE_URI_ABS_PATH:
            p = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
            p = flex_skip_method(p);
            if (p)
                for (len = 0; p[len] && p[len] != '?' && !isspace(p[len]); len++);
            break;

        case TOKEN_REQUEST_LINE_URI_QUERY:
            p = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
            p = flex_skip_prefix(p, '?');
            if (p)
                for (len = 0; p[len] && !isspace(p[len]); len++);
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL:
            p = flex_get_request_line_protocol(rq);
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL_NAME:
            p = flex_get_request_line_protocol(rq);
            if (p)
                for (len = 0; p[len] && p[len] != '/'; len++);
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL_VERSION:
            p = flex_get_request_line_protocol(rq);
            p = flex_skip_prefix(p, '/');
            break;

        case TOKEN_METHOD:
            p = pblock_findkeyval(pb_key_method, rq->reqpb);
            break;

        case TOKEN_STATUS_CODE:
            p = pblock_findkeyval(pb_key_status, rq->srvhdrs);
            if (p && p[0] && p[1] && p[2])
                len = 3;
            break;

        case TOKEN_STATUS_REASON:
            p = pblock_findkeyval(pb_key_status, rq->srvhdrs);
            if (p && p[0] && p[1] && p[2] && p[3]) {
                p += 4;
            } else {
                p = NULL;
            }
            break;

        case TOKEN_CONTENT_LENGTH:
            p = pblock_findkeyval(pb_key_content_length, rq->srvhdrs);
            break;

        case TOKEN_REFERER:
            p = pblock_findkeyval(pb_key_referer, rq->headers);
            break;

        case TOKEN_USER_AGENT:
            p = pblock_findkeyval(pb_key_user_agent, rq->headers);
            break;

        case TOKEN_AUTH_USER:
            p = pblock_findkeyval(pb_key_auth_user, rq->vars);
            break;

        case TOKEN_VSID:
            p = vs_get_id(request_get_vs(rq));
            break;

        case TOKEN_DURATION:
            len = flex_format_duration(sn, rq, &p);
            break;

        case TOKEN_SUBSYSTEM:
            p = "NSAPI";
            len = 5;
            break;

        case TOKEN_COOKIE:
            len = flex_format_cookie(sn, rq, t->p, &p);
            break;

        case TOKEN_PB_KEY:
            p = pblock_findkeyval(t->key, pb);
            break;

        case TOKEN_PB_NAME:
            p = pblock_findval(t->p, pb);
            break;

        case TOKEN_SN_CLIENT_KEY:
            p = pblock_findkeyval(t->key, sn->client);
            break;

        case TOKEN_SN_CLIENT_NAME:
            p = pblock_findval(t->p, sn->client);
            break;

        case TOKEN_RQ_VARS_KEY:
            p = pblock_findkeyval(t->key, rq->vars);
            break;

        case TOKEN_RQ_VARS_NAME:
            p = pblock_findval(t->p, rq->vars);
            break;

        case TOKEN_RQ_REQPB_KEY:
            p = pblock_findkeyval(t->key, rq->reqpb);
            break;

        case TOKEN_RQ_REQPB_NAME:
            p = pblock_findval(t->p, rq->reqpb);
            break;

        case TOKEN_RQ_HEADERS_KEY:
            p = pblock_findkeyval(t->key, rq->headers);
            break;

        case TOKEN_RQ_HEADERS_NAME:
            p = pblock_findval(t->p, rq->headers);
            break;

        case TOKEN_RQ_SRVHDRS_KEY:
            p = pblock_findkeyval(t->key, rq->srvhdrs);
            break;

        case TOKEN_RQ_SRVHDRS_NAME:
            p = pblock_findval(t->p, rq->srvhdrs);
            break;

        default:
            PR_ASSERT(0);
            break;
        }

        if (p) {
            if (len == -1)
                len = strlen(p);
        } else {
            p = "-";
            len = 1;
        }

        formatted[ti].p = p;
        formatted[ti].len = len;

        pos += len;
    }

    return pos;
}

/* ---------------------- flex_format_nsapi_binary ------------------------ */

static inline int flex_format_nsapi_binary(pblock *pb, Session *sn, Request *rq, 
                                           FlexLog *log, FlexFormatted *formatted)
{
    const FlexFormat *f = log->format;
    int pos = 0;
    int nullCount = 0;

    // Format tokens for a request served by NSAPI
    for (int ti = 0; ti < f->ntokens; ti++) {
        const FlexToken *t = &f->tokens[ti];

        const char *p = NULL;
        int len = -1;
        formatted[ti].appendNull = PR_FALSE;

        switch (t->type) {
        case TOKEN_TEXT_OFFSET:
            p = f->format + t->offset;
            // not writing beautification stuff in binary log
            len = 0;
            break;

        case TOKEN_TEXT_POINTER:
            p = t->p;
            len = t->len;
            break;

        case TOKEN_MODEL:
            if (model_str_interpolate(t->model, sn, rq, &p, &len))
                log_error(LOG_VERBOSE, "flex-log", sn, rq, "error interpolating format (%s)", system_errmsg());
            else if (p && (len > 0)) {  // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_SYSDATE:
                len = flex_format_binary <PRInt64> (sn->pool, (PRInt64)
                                                    ft_time(), &p);
            break;

        case TOKEN_LOCALEDATE:
                len = flex_format_binary <PRInt64> (sn->pool, (PRInt64)
                                                    ft_time(), &p);
            break;

        case TOKEN_TIME:
                len = flex_format_binary <PRInt64> (sn->pool, (PRInt64)
                                                    ft_time(), &p);
            break;

        case TOKEN_RELATIVETIME:
                len = flex_format_binary <PRInt64> (sn->pool, (PRInt64) 
                                            (ft_time() - log->epoch), &p);
            break;

        case TOKEN_IP:
            p = pblock_findkeyval(pb_key_ip, sn->client);
            break;

        case TOKEN_DNS:
            if (!pblock_findkeyval(pb_key_iponly, pb))
                p = session_dns(sn);
            if (!p)
                p = pblock_findkeyval(pb_key_ip, sn->client);
            break;

        case TOKEN_METHOD_NUM:
                len = flex_format_binary <short> (sn->pool, (short)
                                                  rq->method_num, &p);
            break;

        case TOKEN_PROTV_NUM:
                len = flex_format_binary <short>(sn->pool, (short)
                                                 rq->protv_num, &p);
            break;

        case TOKEN_REQUEST_LINE:
            p = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
            break;

        case TOKEN_REQUEST_LINE_URI:
            p = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
            p = flex_skip_method(p);
            if (p)
                for (len = 0; p[len] && !isspace(p[len]); len++);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_REQUEST_LINE_URI_ABS_PATH:
            p = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
            p = flex_skip_method(p);
            if (p)
                for (len = 0; p[len] && p[len] != '?' && !isspace(p[len]); len++);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_REQUEST_LINE_URI_QUERY:
            p = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
            p = flex_skip_prefix(p, '?');
            if (p)
                for (len = 0; p[len] && !isspace(p[len]); len++);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL:
            p = flex_get_request_line_protocol(rq);
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL_NAME:
            p = flex_get_request_line_protocol(rq);
            if (p)
                for (len = 0; p[len] && p[len] != '/'; len++);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL_VERSION:
            p = flex_get_request_line_protocol(rq);
            p = flex_skip_prefix(p, '/');
            break;

        case TOKEN_METHOD:
            p = pblock_findkeyval(pb_key_method, rq->reqpb);
            break;

        case TOKEN_STATUS_CODE:
            p = pblock_findkeyval(pb_key_status, rq->srvhdrs);
            if (p && p[0] && p[1] && p[2]) {
                short s = atoi(p);
                len = flex_format_binary <short> (sn->pool, s, &p);
            }
            break;

        case TOKEN_STATUS_REASON:
            p = pblock_findkeyval(pb_key_status, rq->srvhdrs);
            if (p && p[0] && p[1] && p[2] && p[3]) {
                p += 4;
            } else {
                p = NULL;
            }
            break;

        case TOKEN_CONTENT_LENGTH:
            p = pblock_findkeyval(pb_key_content_length, rq->srvhdrs);
            if (p) {
                PRInt64 cl = util_atoi64(p);
                len = flex_format_binary <PRInt64> (sn->pool, cl, &p);
            }
            break;

        case TOKEN_REFERER:
            p = pblock_findkeyval(pb_key_referer, rq->headers);
            break;

        case TOKEN_USER_AGENT:
            p = pblock_findkeyval(pb_key_user_agent, rq->headers);
            break;

        case TOKEN_AUTH_USER:
            p = pblock_findkeyval(pb_key_auth_user, rq->vars);
            break;

        case TOKEN_VSID:
            p = vs_get_id(request_get_vs(rq));
            break;

        case TOKEN_DURATION:
            len = flex_format_duration_binary(sn, rq, &p);
            break;

        case TOKEN_SUBSYSTEM:
            p = "NSAPI";
            len = 5;
            // for '\0'
            formatted[ti].appendNull = PR_TRUE;
            nullCount++;
            break;

        case TOKEN_COOKIE:
            len = flex_format_cookie(sn, rq, t->p, &p);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_PB_KEY:
            p = pblock_findkeyval(t->key, pb);
            break;

        case TOKEN_PB_NAME:
            p = pblock_findval(t->p, pb);
            break;

        case TOKEN_SN_CLIENT_KEY:
            p = pblock_findkeyval(t->key, sn->client);
            break;

        case TOKEN_SN_CLIENT_NAME:
            p = pblock_findval(t->p, sn->client);
            break;

        case TOKEN_RQ_VARS_KEY:
            p = pblock_findkeyval(t->key, rq->vars);
            break;

        case TOKEN_RQ_VARS_NAME:
            p = pblock_findval(t->p, rq->vars);
            break;

        case TOKEN_RQ_REQPB_KEY:
            p = pblock_findkeyval(t->key, rq->reqpb);
            break;

        case TOKEN_RQ_REQPB_NAME:
            p = pblock_findval(t->p, rq->reqpb);
            break;

        case TOKEN_RQ_HEADERS_KEY:
            p = pblock_findkeyval(t->key, rq->headers);
            break;

        case TOKEN_RQ_HEADERS_NAME:
            p = pblock_findval(t->p, rq->headers);
            break;

        case TOKEN_RQ_SRVHDRS_KEY:
            p = pblock_findkeyval(t->key, rq->srvhdrs);
            break;

        case TOKEN_RQ_SRVHDRS_NAME:
            p = pblock_findval(t->p, rq->srvhdrs);
            break;
        case TOKEN_RECORD_SIZE:
            // Field to add buffer length
            len = sizeof(PRInt32);
            p = (char *)pool_malloc(sn->pool, len);
            break;

        default:
            PR_ASSERT(0);
            break;
        }

        if (p) {
            if (len == -1) {
                len = strlen(p);
                // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
        } else {
            // for '\0'
            len = 0;
            formatted[ti].appendNull = PR_TRUE;
            nullCount++;
        }

        formatted[ti].p = p;
        formatted[ti].len = len;

        pos += len;
    }

    pos += nullCount;
    // We now have buffer length we add copy it in the first field
    memcpy((void *)(formatted[0].p), (void *)&pos, sizeof(PRInt32));

    return pos;
}



/* -------------------------- flex_format_accel --------------------------- */

static inline int flex_format_accel(FlexLog *log, pool_handle_t *pool, Connection *connection, const char *status, int status_len, const char *vsid, int vsid_len, PRInt64 cl, FlexFormatted *formatted)
{
    const FlexFormat *f = log->format;
    int pos = 0;

    // Format tokens for a request served from the accelerator cache
    for (int ti = 0; ti < f->ntokens; ti++) {
        const FlexToken *t = &f->tokens[ti];

        PR_ASSERT(flex_token_accel(t->type));
              
        const char *p = NULL;
        int len = -1;
        formatted[ti].appendNull = PR_FALSE;

        switch (t->type) {
        case TOKEN_TEXT_OFFSET:
            p = f->format + t->offset;
            len = t->len;
            break;

        case TOKEN_TEXT_POINTER:
            p = t->p;
            len = t->len;
            break;

        case TOKEN_SYSDATE:
            len = flex_format_date(pool, date_format_clf, &p);
            break;

        case TOKEN_LOCALEDATE:
            len = flex_format_date(pool, date_format_locale, &p);
            break;

        case TOKEN_TIME:
            len = flex_format_64(pool, ft_time(), &p);
            break;

        case TOKEN_RELATIVETIME:
            len = flex_format_64(pool, (PRInt64) ft_time() - log->epoch, &p);
            break;

        case TOKEN_IP:
            p = connection->remoteIP.buf;
            len = connection->remoteIP.len;
            break;

        case TOKEN_METHOD_NUM:
            PR_ASSERT(METHOD_GET == 1);
            p = "1";
            len = 1;
            break;

        case TOKEN_PROTV_NUM:
            len = flex_format_protv_num(pool, connection->httpHeader.GetNegotiatedProtocolVersion(), &p);
            break;

        case TOKEN_REQUEST_LINE:
            p = connection->httpHeader.GetRequestLine().ptr;
            len = connection->httpHeader.GetRequestLine().len;
            break;

        case TOKEN_REQUEST_LINE_URI:
            p = connection->httpHeader.GetRequestAbsPath().ptr;
            len = connection->httpHeader.GetRequestAbsPath().len;
            break;

        case TOKEN_REQUEST_LINE_URI_ABS_PATH:
            p = connection->httpHeader.GetRequestAbsPath().ptr;
            len = connection->httpHeader.GetRequestAbsPath().len;
            break;

        case TOKEN_REQUEST_LINE_URI_QUERY:
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL:
            len = flex_format_protocol(pool, connection->httpHeader.GetClientProtocolVersion(), &p);
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL_NAME:
            p = "HTTP";
            len = 4;
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL_VERSION:
            len = flex_format_version(pool, connection->httpHeader.GetClientProtocolVersion(), &p);
            break;

        case TOKEN_METHOD:
            p = "GET";
            len = 3;
            break;

        case TOKEN_STATUS_CODE:
            p = status;
            len = status_len;
            break;

        case TOKEN_CONTENT_LENGTH:
            len = flex_format_64(pool, cl, &p);
            break;

        case TOKEN_REFERER:
            len = flex_format_hhstring(connection->httpHeader.GetReferer(), &p);
            break;

        case TOKEN_USER_AGENT:
            len = flex_format_hhstring(connection->httpHeader.GetUserAgent(), &p);
            break;

        case TOKEN_AUTH_USER:
            break;

        case TOKEN_VSID:
            p = vsid;
            len = vsid_len;
            break;

        case TOKEN_SUBSYSTEM:
            // XXX elving differentiate between sync and async in log
            p = "cache";
            len = 5;
            break;

        default:
            // XXX elving add support for getting arbitrary headers form HttpHeader
            PR_ASSERT(0);
            break;
        }

        if (p) {
            if (len == -1)
                len = strlen(p);
        } else {
            p = "-";
            len = 1;
        }

        formatted[ti].p = p;
        formatted[ti].len = len;

        pos += len;
    }

    return pos;
}

/* ---------------------- flex_format_accel_binary ------------------------ */

static inline int flex_format_accel_binary(FlexLog *log, pool_handle_t *pool,
                                           Connection *connection, const char *status,
                                           int status_len, const char *vsid, int vsid_len,
                                           PRInt64 cl, FlexFormatted *formatted)
{
    const FlexFormat *f = log->format;
    int pos = 0;
    int nullCount = 0;

    // Format tokens for a request served from the accelerator cache
    for (int ti = 0; ti < f->ntokens; ti++) {
        const FlexToken *t = &f->tokens[ti];

        PR_ASSERT(flex_token_accel(t->type));
              
        const char *p = NULL;
        int len = -1;
        formatted[ti].appendNull = PR_FALSE;

        switch (t->type) {
        case TOKEN_TEXT_OFFSET:
            p = f->format + t->offset;
            len = 0;
            break;

        case TOKEN_TEXT_POINTER:
            p = t->p;
            len = t->len;
            break;

        case TOKEN_SYSDATE:
            len = flex_format_binary <PRInt64> (pool, (PRInt64)
                                                    ft_time(), &p);
            break;

        case TOKEN_LOCALEDATE:
            len = flex_format_binary <PRInt64> (pool, (PRInt64)
                                                    ft_time(), &p);
            break;

        case TOKEN_TIME:
            len = flex_format_binary <PRInt64> (pool, (PRInt64)
                                                    ft_time(), &p);
            break;

        case TOKEN_RELATIVETIME:
            len = flex_format_binary <PRInt64> (pool, (PRInt64) (ft_time()
                                                    - log->epoch), &p);
            break;

        case TOKEN_IP:
            p = connection->remoteIP.buf;
            len = connection->remoteIP.len;
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_METHOD_NUM:
            PR_ASSERT(METHOD_GET == 1);
            len = flex_format_binary <short> (pool, (short)1, &p);
            break;

        case TOKEN_PROTV_NUM:
            len = flex_format_binary <short> (pool, (short)
                    connection->httpHeader.GetNegotiatedProtocolVersion(), &p);
            break;

        case TOKEN_REQUEST_LINE:
            p = connection->httpHeader.GetRequestLine().ptr;
            len = connection->httpHeader.GetRequestLine().len;
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_REQUEST_LINE_URI:
            p = connection->httpHeader.GetRequestAbsPath().ptr;
            len = connection->httpHeader.GetRequestAbsPath().len;
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_REQUEST_LINE_URI_ABS_PATH:
            p = connection->httpHeader.GetRequestAbsPath().ptr;
            len = connection->httpHeader.GetRequestAbsPath().len;
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_REQUEST_LINE_URI_QUERY:
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL:
            len = flex_format_protocol(pool, connection->httpHeader.GetClientProtocolVersion(), &p);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL_NAME:
            p = "HTTP";
            len = 4;
            // for '\0'
            formatted[ti].appendNull = PR_TRUE;
            nullCount++;
            break;

        case TOKEN_REQUEST_LINE_PROTOCOL_VERSION:
            len = flex_format_version(pool, connection->httpHeader.GetClientProtocolVersion(), &p);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_METHOD:
            p = "GET";
            len = 3;
            // for '\0'
            formatted[ti].appendNull = PR_TRUE;
            nullCount++;
            break;

        case TOKEN_STATUS_CODE:
            if (status) {
                short s = atoi(status);
                len = flex_format_binary <short>(pool, s, &p);
            }
            break;

        case TOKEN_CONTENT_LENGTH:
            len = flex_format_binary <PRInt64> (pool, cl, &p);
            break;

        case TOKEN_REFERER:
            len = flex_format_hhstring(connection->httpHeader.GetReferer(), &p);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_USER_AGENT:
            len = flex_format_hhstring(connection->httpHeader.GetUserAgent(), &p);
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_AUTH_USER:
            break;

        case TOKEN_VSID:
            p = vsid;
            len = vsid_len;
            if (p && (len > 0)) { // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
            break;

        case TOKEN_SUBSYSTEM:
            // XXX elving differentiate between sync and async in log
            p = "cache";
            len = 5;
            // for '\0'
            formatted[ti].appendNull = PR_TRUE;
            nullCount++;
            break;

        case TOKEN_RECORD_SIZE:
            // Field for buffer length
            len = sizeof(PRInt32);
            p = (char *)pool_malloc(pool, len);
            break;

        default:
            // XXX elving add support for getting arbitrary headers form HttpHeader
            PR_ASSERT(0);
            break;
        }

        if (p) {
            if (len == -1) {
                len = strlen(p);
                // for '\0'
                formatted[ti].appendNull = PR_TRUE;
                nullCount++;
            }
        } else {
            // for '\0'
            len = 0;
            formatted[ti].appendNull = PR_TRUE;
            nullCount++;
        }

        formatted[ti].p = p;
        formatted[ti].len = len;

        pos += len;
    }

    pos += nullCount;
    // We now have buffer length we add copy it in the first field
    memcpy((void *)(formatted[0].p), (void *)&pos, sizeof(PRInt32));

    return pos;
}


/* ----------------------------- flex_memcpy ------------------------------ */

static inline void flex_memcpy(char *buf, int& pos, const char *p, int len,
                               PRBool appendNull)
{
    // Access log format strings tend to contain a number of extremely short
    // strings, so skip the memcpy call when it makes sense
    switch (len) {
    case 4: buf[pos++] = *p++;
    case 3: buf[pos++] = *p++;
    case 2: buf[pos++] = *p++;
    case 1: buf[pos++] = *p++;
    case 0: break;
    default:
        memcpy(buf + pos, p, len);
        pos += len;
        break;
    }
    if (appendNull)
        buf[pos++] = '\0';
}


/* ------------------------ flex_parse_header_line ------------------------ */

static inline const char *flex_parse_header_line(const char *line, const char *prefix, int prefix_len, NSString& value)
{
    if (!strncmp(line, prefix, prefix_len)) {
        const char *eol = strstr(line, ENDLINE);
        if (eol) {
            const char *suffix = line + prefix_len;
            int suffix_len = eol - suffix;

            value.append(suffix, suffix_len);

            line += prefix_len;
            line += suffix_len;
            line += ENDLINE_LEN;
        }
    }

    return line;
}


/* --------------------------- flex_read_header --------------------------- */

static int flex_read_header(const char *filename, NSString& format,
                            NSString& time, NSString& version) 
{
    char buf[FORMAT_LINE_LEN + TIME_LINE_LEN + BIN_LOG_VERSION_LINE_LEN + 1];

    // Open the log file
    SYS_FILE fd = system_fopenRO(filename);
    if (fd == SYS_ERROR_FD)
        return -1;

    // Get the first few lines
    int len = system_fread(fd, buf, sizeof(buf) - 1);
    system_fclose(fd);
    if (len < 0)
        return -1;

    // Parse any format= and time= lines from the buffer
    buf[len] = '\0';
    const char *p = buf;
    p = flex_parse_header_line(p, FORMAT_PREFIX, FORMAT_PREFIX_LEN, format);
    p = flex_parse_header_line(p, TIME_PREFIX, TIME_PREFIX_LEN, time);
    p = flex_parse_header_line(p, BIN_LOG_VERSION_PREFIX, 
                               BIN_LOG_VERSION_PREFIX_LEN, version);

    return len;
}


/* --------------------------- flex_create_log ---------------------------- */

static FlexLog *flex_create_log(const VirtualServer *vs, const char *name,
                                const char *filename, const char *format,
                                PRBool enabled, PRBool binary = PR_FALSE)
{
    PR_ASSERT(vs);
    PR_ASSERT(name);
    PR_ASSERT(filename);
    PR_ASSERT(format);

    // The preferred epoch for %RELATIVETIME% is right now (this will be
    // overridden below if the log file already exists)
    time_t epoch = ft_time();

    // If the log file exists and is a plain file...
    PRFileInfo64 finfo;
    if (PR_GetFileInfo64(filename, &finfo) == PR_SUCCESS) { 
        if (finfo.type == PR_FILE_FILE) {
            // Compare the old header with the new header
            NSString old_format;
            NSString old_time;
            NSString old_binversion;
            int rv = flex_read_header(filename, old_format, old_time,
                                      old_binversion);
            if (rv > 0) {
                if (strncmp(format, old_format, FORMAT_LEN))
                    ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_flexLogError3), filename);
                epoch = util_atoi64(old_time);

                if (binary) {
                    // older log was not binary but this one is
                    if (old_binversion.length() <= 0) {
                        ereport(LOG_MISCONFIG,
                                XP_GetAdminStr(DBT_flexLogError4), filename);
                        // Do not start the server
                        return NULL;
                    } else if (strncmp(BIN_LOG_VERSION, old_binversion.data(),
                             strlen(BIN_LOG_VERSION))) {
                        // both are binary but their versions didnt match
                        ereport(LOG_MISCONFIG,
                                XP_GetAdminStr(DBT_flexLogError5));
                        // Do not start the server
                        return NULL;
                    } // else if both the versions are same - ok
                } else if (old_binversion.length() > 0) {
                        // If this log is NOT binary and older log is binary
                        ereport(LOG_MISCONFIG,
                                XP_GetAdminStr(DBT_flexLogError4), filename);
                        // Do not start the server
                        return NULL;
                }
                    // else both are non-binary access logs - ok 
            }
        }
    }

    // Allocate a new FlexLog
    FlexLog *log = (FlexLog *) PERM_MALLOC(sizeof(FlexLog));
    if (!log)
        return NULL;

    // Initialize the FlexLog
    log->next = NULL;
    log->name = PERM_STRDUP(name);
    log->format = flex_acquire_format(format, binary);
    log->format->binary = binary;
    log->epoch = epoch;
    log->enabled = enabled;
    log->file = LogManager::getFile(filename);

    // Prepare header line(s)
    NSString header;
    header.printf("%s%.*s%s", FORMAT_PREFIX, FORMAT_LEN, format, ENDLINE);
    for (int i = 0; i < log->format->ntokens; i++) {
        if (log->format->tokens[i].type == TOKEN_RELATIVETIME) {
            // The time line is needed only when %RELATIVETIME% is used
            header.printf("%s%lu%s", TIME_PREFIX, (unsigned long) epoch, ENDLINE);
            break;
        }
    }
    if (binary)
        header.printf("%s%s%s", BIN_LOG_VERSION_PREFIX, BIN_LOG_VERSION,
                      ENDLINE);
            
    // Open the log file
    LogManager::setHeader(log->file, header);
    LogManager::openFile(log->file);

    // Add the FlexLog to the VS's list
    log->next = (FlexLog *) vs_get_data(vs, flex_vs_slot);
    vs_set_data(vs, &flex_vs_slot, log);

    return log;
}

static FlexLog *flex_create_log(const VirtualServer *vs, const char *name)
{
    int i;

    // Look for a <virtual-server> <access-log>
    for (i = 0; i < vs->getAccessLogCount(); i++) {
        const ServerXMLSchema::AccessLog *al = vs->getAccessLog(i);
        if (!strcmp(al->name, name))
            return flex_create_log(vs, name, al->file, al->format, al->enabled,
                          al->mode.getEnumValue() == 
                          ServerXMLSchema::AccessLogMode::ACCESSLOGMODE_BINARY);
    }

    // Look for a <server> <access-log>
    const Configuration *configuration = vs->getConfiguration();
    for (i = 0; i < configuration->getAccessLogCount(); i++) {
        const ServerXMLSchema::AccessLog *al = configuration->getAccessLog(i);
        if (!strcmp(al->name, name))
            return flex_create_log(vs, name, al->file, al->format, al->enabled,
                          al->mode.getEnumValue() == 
                          ServerXMLSchema::AccessLogMode::ACCESSLOGMODE_BINARY);
    }

    // Look for a magnus.conf flex-init log template
    for (FlexTemplate *ft = flex_template_list; ft; ft = ft->next) {
        if (!strcmp(ft->name, name)) {
            char *filename = vs_substitute_vars(vs, ft->filename);
            if (!filename) {
                ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_flexLogError2), name, ft->filename, vs_get_id(vs));
                return NULL;
            }
            FlexLog *log = flex_create_log(vs, name, filename, ft->format->format, PR_TRUE);
            FREE(filename);
            return log;
        }
    }

    return NULL;
}


/* ------------------------ flex_find_existing_log ------------------------ */

static inline FlexLog *flex_find_existing_log(const VirtualServer *vs, const char *name)
{
    // Look for an existing FlexLog with the given name
    FlexLog *log = (FlexLog *) vs_get_data(vs, flex_vs_slot);
    while (log) {
        if (!strcmp(log->name, name))
            break;
        log = log->next;
    }

    return log;
}


/* ----------------------------- flex_get_log ----------------------------- */

static inline FlexLog *flex_get_log(const VirtualServer *vs, const char *name)
{
    // Get an existing FlexLog or create a new one as necessary
    FlexLog *log = flex_find_existing_log(vs, name);
    if (!log) {
        PR_Lock(flex_vs_lock);

        // If this is the first time this VS has heard about the log, create it
        log = flex_find_existing_log(vs, name);
        if (!log) {
            // XXX Even with the lock, this is not MT safe in theory because
            // another CPU could see the new FlexLog * before it sees the
            // contents of the new FlexLog (as there's no guarantee of cache
            // coherency or ordered memory updates)
            log = flex_create_log(vs, name);
        }

        PR_Unlock(flex_vs_lock);
    }

    return log;
}

/* ----------------------------- flex_get_logfile ----------------------------- */

LogFile *flex_get_logfile(FlexLog *log)
{
    if (log == NULL)
        return NULL;
    return log->file;
}

/* ----------------------------- flex_init_vs ----------------------------- */

static int flex_init_vs(VirtualServer *incoming, const VirtualServer *current)
{
    // For simplicity, start out with a fresh list of FlexLogs each time
    vs_set_data(incoming, &flex_vs_slot, NULL);

    return REQ_PROCEED;
}


/* ------------------------ flex_init_vs_directive ------------------------ */

static int flex_init_vs_directive(const directive* dir, VirtualServer* incoming, const VirtualServer* current)
{
    // Get the directive's log name
    const char *name = pblock_findkeyval(pb_key_name, dir->param.pb);
    if (!name)
        name = DEFAULT_LOG_NAME;

    // Create this directive's FlexLog
    FlexLog *log = flex_get_log(incoming, name);

    // If the log couldn't be created... (in order to accomodate interpolated
    // log names, we don't complain if the name looks a $variable)
    if (!log && !strchr(name, '$')) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_flexLogError1), name);
        return REQ_ABORTED;
    }

    return REQ_PROCEED;
}


/* --------------------------- flex_destroy_vs ---------------------------- */

static void flex_destroy_vs(VirtualServer *outgoing)
{
    // For each of this VS's FlexLogs...
    FlexLog *log = (FlexLog *) vs_get_data(outgoing, flex_vs_slot);
    while (log) {
        FlexLog *next = log->next;

        // Destroy the FlexLog
        PERM_FREE(log->name);
        flex_release_format(log->format);
        LogManager::unref(log->file);
        PERM_FREE(log);

        log = next;
    }

    vs_set_data(outgoing, &flex_vs_slot, NULL);
}


/* ------------------------------ flex_write ------------------------------ */

static int flex_write(pool_handle_t *pool, FlexLog *log,
                      FlexFormatted *formatted, int desired,
                      LogBuffer* incomingHandle, PRBool binary)
{
    const FlexFormat *f = log->format;

    // If the formatted tokens will fit in a buffer...
    int available = LogManager::sizeMaxLogLine - ((binary) ? 0 : ENDLINE_LEN);
    char* buf = NULL;
    if (desired <= available) {
        // Acquire a log buffer
        LogBuffer *handle = NULL;
        if (incomingHandle == NULL) {
            buf = LogManager::lockBuffer(log->file, handle, desired +
                                         ((binary) ? 0 : ENDLINE_LEN));
            if (!buf)
                return -1;
        }
        else
            buf = LogManager::getBuffer(incomingHandle);

        int pos = 0;

        // Copy the formatted tokens into the buffer
        for (int ti = 0; ti < f->ntokens; ti++)
            flex_memcpy(buf, pos, formatted[ti].p, formatted[ti].len,
                        formatted[ti].appendNull);

        // Append EOL
        if (!binary)
        for (int i = 0; i < ENDLINE_LEN; i++)
            buf[pos++] = ENDLINE[i];

        // Release the log buffer
        if (handle) {
            LogManager::unlockBuffer(handle, pos);
        } else {
            LogManager::updatePosition(incomingHandle, pos);
        }

        return desired;
    }

    // Determine the average space available to each token
    int avg_available = available / f->ntokens;
    if (avg_available < DOT_DOT_DOT_LEN) {
        PR_ASSERT(0);
        return -1;
    }

    int ti;

    // Consider the total lengths of a) greedy tokens that want more than the
    // average and b) ungreedy tokens that only want their fair share
    int greedy_desired = 0;
    int ungreedy_desired = 0;
    for (ti = 0; ti < f->ntokens; ti++) {
        if ((formatted[ti].len + (int)formatted[ti].appendNull)
            > avg_available) {
            greedy_desired += (formatted[ti].len +
                               (int)formatted[ti].appendNull);
        } else {
            ungreedy_desired += (formatted[ti].len +
                                 (int)formatted[ti].appendNull);
        }
    }

    PR_ASSERT(desired == greedy_desired + ungreedy_desired);

    // By what factor do we need to scale the greedy tokens so they'll fit?
    int greedy_available = available - ungreedy_desired;
    int greedy_scale = (greedy_desired * 1024 + greedy_available - 1) / greedy_available;

    // We'll need to track how much space each token is allowed
    int *allowed = (int *) pool_malloc(pool, sizeof(int) * f->ntokens);
    if (!allowed)
        return -1;

    // Limit the amount of space each token is allowed
    int greedy_allowed = 0;
    int ungreedy_allowed = 0;
    for (ti = 0; ti < f->ntokens; ti++) {
        if ((formatted[ti].len + (int)formatted[ti].appendNull)
            > avg_available) {
            allowed[ti] = (formatted[ti].len + (int)formatted[ti].appendNull)
                           * 1024 / greedy_scale;
            greedy_allowed += allowed[ti];
        } else {
            allowed[ti] = formatted[ti].len + (int)formatted[ti].appendNull;
            ungreedy_allowed += allowed[ti];
        }
    }

    // Distribute any leftover space amongst the greedy
    int leftover = available - greedy_allowed - ungreedy_allowed;
    while (leftover != 0) {
        for (ti = 0; ti < f->ntokens; ti++) {
            if (leftover == 0)
                break;
            if ((formatted[ti].len + (int)formatted[ti].appendNull)
                > allowed[ti]) {
                allowed[ti]++;
                leftover--;
            }
        }
    }

    // Acquire a log buffer
    LogBuffer *handle = NULL;
    buf = NULL;
    if (incomingHandle == NULL) {
        buf = LogManager::lockBuffer(log->file, handle, available +
                                     ((binary) ? 0 : ENDLINE_LEN));
        if (!buf)
            return -1;
    } else {
        buf = LogManager::getBuffer(incomingHandle);
    }

    int pos = 0;

    // Copy the formatted tokens into the buffer, truncating tokens as needed
    for (ti = 0; ti < f->ntokens; ti++) {
        if ((formatted[ti].len + (int)formatted[ti].appendNull)
            > allowed[ti]) {
            flex_memcpy(buf, pos, formatted[ti].p, allowed[ti]
                        - DOT_DOT_DOT_LEN - (int)formatted[ti].appendNull, 0);
            buf[pos++] = '.';
            buf[pos++] = '.';
            buf[pos++] = '.';
            if (formatted[ti].appendNull)
                buf[pos++] = '\0';
            // size of actual record written is going to vary
            if ((&f->tokens[0])->type == TOKEN_RECORD_SIZE) {
                PRInt32 i;
                memcpy(&i, buf, sizeof(PRInt32));
                i = i + allowed[ti] - formatted[ti].len 
                    - (int)formatted[ti].appendNull;
                memcpy(buf, &i, sizeof(PRInt32));
            }
        } else {
            flex_memcpy(buf, pos, formatted[ti].p, formatted[ti].len,
                        formatted[ti].appendNull);
        }
    }    

    // Append EOL
    if (!binary)
    for (int i = 0; i < ENDLINE_LEN; i++)
        buf[pos++] = ENDLINE[i];

    // Release the log buffer
    if (handle) {
        LogManager::unlockBuffer(handle, pos);
    } else {
        // Update the position in the logmanager
        LogManager::updatePosition(incomingHandle, pos);
    }

    return available;
}


/* ------------------------------- flex_log ------------------------------- */

int flex_log(pblock *pb, Session *sn, Request *rq)
{
    // Get the directive's log name
    const char *name = pblock_findkeyval(pb_key_name, pb);
    if (!name)
        name = DEFAULT_LOG_NAME;

    // Lookup this directive's FlexLog
    FlexLog *log = flex_get_log(request_get_vs(rq), name);
    PR_ASSERT(log);
    if (!log) {
        log_error(LOG_MISCONFIG, "flex-log", sn, rq, XP_GetAdminStr(DBT_flexLogError1), name);
        return REQ_ABORTED;
    }

    // Don't write to disabled access logs
    if (!log->enabled) {
        log_error(LOG_VERBOSE, "flex-log", sn, rq, "skipping disabled log \"%s\"", name);
        return REQ_NOACTION;
    }

    // Check if the log format can be accomodated by the accelerator cache
    const FlexFormat *f = log->format;
    if (f->accel) {
        // The accelerator cache can call flex_log_accel for this log file
        NSAPIRequest *nrq = (NSAPIRequest *) rq;
        if (!nrq->accel_flex_log) {
            nrq->accel_flex_log = log;
            rq->directive_is_cacheable = 1;
        }
    }

    // Allocate an array to track formatted tokens
    FlexFormatted *formatted = (FlexFormatted *) pool_malloc(sn->pool, sizeof(FlexFormatted) * f->ntokens);
    if (!formatted)
        return REQ_ABORTED;

    // Format the tokens
    int len;
    if (f->binary)
        len = flex_format_nsapi_binary(pb, sn, rq, log, formatted);
    else
        len = flex_format_nsapi(pb, sn, rq, log, formatted);

    // Write the entry to the log file
    int rv = flex_write(sn->pool, log, formatted, len, NULL, f->binary);
    if (rv == -1)
        return REQ_ABORTED;
    if (rv < len)
        log_error(LOG_WARN, "flex-log", sn, rq, XP_GetAdminStr(DBT_flexlogereport8), LogManager::sizeMaxLogLine);

    return REQ_PROCEED;
}


/* ---------------------------- flex_log_accel ---------------------------- */

void flex_log_accel(FlexLog *log, pool_handle_t *pool, Connection *connection, const char *status, int status_len, const char *vsid, int vsid_len, PRInt64 cl)
{
    const FlexFormat *f = log->format;

    // We should only get here if flex-log previously decided that the log
    // was enabled and its format was compatible with the accelerator cache
    PR_ASSERT(log->enabled);
    PR_ASSERT(f->accel);

    // Allocate an array to track formatted tokens
    FlexFormatted *formatted = (FlexFormatted *) pool_malloc(pool, sizeof(FlexFormatted) * f->ntokens);

    // Format the tokens
    int len = 0;
    if (f->binary)
        len = flex_format_accel_binary(log, pool, connection, status, status_len, vsid, vsid_len, cl, formatted);
    else
        len = flex_format_accel(log, pool, connection, status, status_len, vsid, vsid_len, cl, formatted);

    // Write the entry to the log file
    flex_write(pool, log, formatted, len, NULL, f->binary);

}

// Do not acquire and release the LogBuffer for each
// request, even when we're running async on the keep-alive threads.
// Cache LogBuffer in AcceleratorAsync to reduce mutex
// acquisition overhead.  
void flex_log_accel_async(FlexLog *log, pool_handle_t *pool, 
                          struct Connection *connection, 
                          const char *status, 
                          int status_len, 
                          const char *vsid, int vsid_len, PRInt64 cl, 
                          LogBuffer **handle)
{
    const FlexFormat *f = log->format;

    // We should only get here if flex-log previously decided that the log
    // was enabled and its format was compatible with the accelerator cache
    PR_ASSERT(log->enabled);
    PR_ASSERT(f->accel);

    // Allocate an array to track formatted tokens
    FlexFormatted *formatted = (FlexFormatted *) pool_malloc(pool, sizeof(FlexFormatted) * f->ntokens);

    // Format the tokens
    int len = 0;
    if (f->binary)
        len = flex_format_accel_binary(log, pool, connection, status, status_len, vsid, vsid_len, cl, formatted);
    else
        len = flex_format_accel(log, pool, connection, status, status_len, vsid, vsid_len, cl, formatted);
    int desired = len + ((f->binary)?0:ENDLINE_LEN);
    PRBool spaceAvailable = PR_FALSE;

    int available = LogManager::sizeMaxLogLine;

    // If there is already a handle
    if (*handle) {
        // Grab a different buffer if the current one does not have enough 
        // space. flex_log_write formats it to fit into available, so
        // call isSpaceAvailable() accordingly
        spaceAvailable = LogManager::isSpaceAvailable(*handle, 
                                                      desired > available?available:desired);
        if (!spaceAvailable) {
            LogManager::unlockBuffer(*handle, 0);
            *handle = NULL;
        }
    }

    // Either this is the first request or the handle has been
    // released
    if (!spaceAvailable) {
        LogManager::lockBuffer(log->file, *handle, 
                               desired > available?available:desired);
    }

    // Write the entry to the log file
    flex_write(pool, log, formatted, len, *handle, f->binary);
}
