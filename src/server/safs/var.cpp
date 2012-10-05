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
 * var.cpp: Variables for use in NSAPI expressions
 * 
 * Chris Elving
 */

#include <ctype.h>

#include "netsite.h"
#include "base/util.h"
#include "base/pool.h"
#include "base/vs.h"
#include "frame/expr.h"
#include "frame/http.h"
#include "frame/http_ext.h"
#include "frame/cookie.h"
#include "safs/cgi.h"
#include "safs/var.h"
#include "safs/dbtsafs.h"


/*
 * MATCH signals a mismatch, exact match, or inexact match.
 */
enum MATCH {
    MATCH_NONE = 0,
    MATCH_EXACT = 1,
    MATCH_INEXACT = 2
};

/*
 * EXPRESSION_STRING attempts to evaluate an Expression as a constrant string.
 * If the expression is successfully evaluated, the result is assigned to
 * string.  If an error is encountered evaluating the expression, an error is
 * returned.
 */
#define EXPRESSION_STRING(e, sn, rq, string)              \
    string = expr_const_string(e);                        \
    if (!string) {                                        \
        const Result *r = result_expr(sn, rq, e);         \
        if (result_is_error(r))                           \
            return r;                                     \
                                                          \
        result_as_const_string(sn, rq, r, &string, NULL); \
    }

/*
 * SUBSCRIPT_STRING attempts to evaluate an ExpressionFunc's argument as a
 * constant string.  If the argument is successfully evaluated, the result is
 * assigned to const char *subscript.  If the argument is missing or an error
 * is encountered evaluating the argument, an error is returned.
 */
#define SUBSCRIPT_STRING(args, sn, rq, subscript) \
    const char *subscript;                        \
    {                                             \
        const Expression *e = args_get(args, 0);  \
        EXPRESSION_STRING(e, sn, rq, subscript)   \
        PR_ASSERT(subscript);                     \
    }

/*
 * DEFINE_VAR_GET_PB defines an ExpressionFunc that retrieves a variable value
 * from a particular pblock using a particular key.
 */
#define DEFINE_VAR_GET_PB(pblock, key, var, func)                              \
    static ExpressionFunc func;                                                \
    static const Result *func(const Arguments *args, Session *sn, Request *rq) \
    {                                                                          \
        if (const char *value = pblock_findkeyval(key, pblock))                \
            return result_string(sn, rq, value, strlen(value));                \
                                                                               \
        return result_error(sn, rq, XP_GetAdminStr(DBT_noVarX), var);          \
    }

/*
 * DEFINE_MAP_GET_PB defines an ExpressionFunc that retrieves a map variable
 * value from a particular pblock using the map variable subscript.
 */
#define DEFINE_MAP_GET_PB(pblock, map, func)                                   \
    static ExpressionFunc func;                                                \
    static const Result *func(const Arguments *args, Session *sn, Request *rq) \
    {                                                                          \
        const Expression *e = args_get(args, 0);                               \
                                                                               \
        const char *subscript;                                                 \
                                                                               \
        if (const pb_key *key = expr_const_pb_key(e)) {                        \
            if (const char *value = pblock_findkeyval(key, pblock))            \
                return result_string(sn, rq, value, strlen(value));            \
                                                                               \
            subscript = expr_const_string(e);                                  \
        } else {                                                               \
            EXPRESSION_STRING(e, sn, rq, subscript)                            \
                                                                               \
            if (const char *value = pblock_findval(subscript, pblock))         \
                return result_string(sn, rq, value, strlen(value));            \
        }                                                                      \
                                                                               \
        PR_ASSERT(subscript);                                                  \
                                                                               \
        return result_error(sn, rq,                                            \
                            XP_GetAdminStr(DBT_noMapXSubscriptY),              \
                            map, subscript);                                   \
    }

/*
 * DEFINE_MAP_GET_CASE_PB defines an ExpressionFunc that retrieves a map
 * variable value from a particular pblock using the map variable subscript as
 * a case-insensitive parameter name.
 */
#define DEFINE_MAP_GET_CASE_PB(pblock, map, func)                              \
    static ExpressionFunc func;                                                \
    static const Result *func(const Arguments *args, Session *sn, Request *rq) \
    {                                                                          \
        SUBSCRIPT_STRING(args, sn, rq, subscript);                             \
                                                                               \
        const char *value = find(pblock, subscript);                           \
        if (value)                                                             \
            return result_string(sn, rq, value, strlen(value));                \
                                                                               \
        return result_error(sn, rq,                                            \
                            XP_GetAdminStr(DBT_noMapXSubscriptY),              \
                            map, subscript);                                   \
    }

static ExpressionFunc var_get_browser;
static ExpressionFunc var_get_chunked;
static ExpressionFunc var_get_code;
static ExpressionFunc var_get_id;
static ExpressionFunc var_get_internal;
static ExpressionFunc var_get_keep_alive;
static ExpressionFunc var_get_path;
static ExpressionFunc var_get_reason;
static ExpressionFunc var_get_restarted;
static ExpressionFunc var_get_security;
static ExpressionFunc var_get_senthdrs;
static ExpressionFunc var_get_server_url;
static ExpressionFunc var_get_time;
static ExpressionFunc var_get_time_day;
static ExpressionFunc var_get_time_hour;
static ExpressionFunc var_get_time_min;
static ExpressionFunc var_get_time_mon;
static ExpressionFunc var_get_time_sec;
static ExpressionFunc var_get_time_wday;
static ExpressionFunc var_get_time_year;
static ExpressionFunc var_get_url;
static ExpressionFunc var_get_urlhost;
static ExpressionFunc map_get_env;
static ExpressionFunc map_get_cookie;


/* -------------------------------- match --------------------------------- */

static inline MATCH match(const char *s1, const char *s2)
{
    while (*s1 == *s2) {
        if (*s1 == '\0')
            return MATCH_EXACT;
        s1++;
        s2++;
    }

    while (tolower(*s1) == tolower(*s2)) {
        if (*s1 == '\0')
            return MATCH_INEXACT;
        s1++;
        s2++;
    }

    return MATCH_NONE;
}


/* --------------------------------- find --------------------------------- */

static inline const char *find(pblock *pblock, const char *name)
{
    const char *inexact = NULL;

    for (int i = 0; i < pblock->hsize; i++) {
        for (struct pb_entry *p = pblock->ht[i]; p; p = p->next) {
            switch (match(p->param->name, name)) {
            case MATCH_EXACT:
                return p->param->value;
            case MATCH_INEXACT:
                inexact = p->param->value;
                break;
            }
        }
    }

    return inexact;
}


/* ---------------------------- var_get_pblock ---------------------------- */

pblock *var_get_pblock(Session *sn, Request *rq, const char *pbname)
{
    pblock *pb = NULL;

    // XXX implement $env{'...'}? Does it map to nrq->param?

    if (pbname) {
        switch (*pbname) {
        case 'c': if (!strcmp(pbname + 1, "client" + 1)) pb = sn->client; break;
        case 'v': if (!strcmp(pbname + 1, "vars" + 1)) pb = rq->vars; break;
        case 'r': if (!strcmp(pbname + 1, "reqpb" + 1)) pb = rq->reqpb; break;
        case 'h': if (!strcmp(pbname + 1, "headers" + 1)) pb = rq->headers; break;
        case 's': if (!strcmp(pbname + 1, "srvhdrs" + 1)) pb = rq->srvhdrs; break;

        case 'S':
            if (!strcmp(pbname + 1, "Ses->client" + 1)) pb = sn->client;
            break;

        case 'R':
            if (!strcmp(pbname + 1, "Req->vars" + 1)) pb = rq->vars;
            else if (!strcmp(pbname + 1, "Req->reqpb" + 1)) pb = rq->reqpb;
            else if (!strcmp(pbname + 1, "Req->headers" + 1)) pb = rq->headers;
            else if (!strcmp(pbname + 1, "Req->srvhdrs" + 1)) pb = rq->srvhdrs;
            break;
        }
    }

    return pb;
}


/* -------------------------- var_get_name_value -------------------------- */

PRStatus var_get_name_value(char *nv, const char **pname, const char **pvalue)
{
    char *n;
    char *v;

    // Get the "name=value" or "Name: value" pair from nv
    n = nv;
    if (!n || !*n) {
        return PR_FAILURE;
    }
    v = n;
    while (*v && *v != '=' && *v != ':') v++;
    if (*v == '=') {
        // Separate name and value from "name=value"
        *v = '\0';
        v++;
    } else if (*v == ':') {
        // Separate Name and value from "Name: value"
        *v = '\0';
        do v++; while (isspace(*v));

        // Convert Name to lowercase
        char *t = n;
        while (*t) {
            *t = tolower(*t);
            t++;
        }
    } else {
        v = NULL;
    }

    *pname = n;
    *pvalue = v;

    return PR_SUCCESS;
}


/* ---------------------- var_get_pblock_name_value ----------------------- */

PRStatus var_get_pblock_name_value(Session *sn, Request *rq, const char *pbname, char *nv, pblock **ppb, const char **pname, const char **pvalue)
{
    // If caller didn't tell us the pblock name up front...
    if (!pbname || !*pbname) {
        // Split namevalue into a pblock name and name=value pair
        pbname = nv;
        nv = strchr(nv, '.');
        if (nv) {
            *nv = '\0';
            nv++;
        }
    }

    // Figure out which pblock we're modifying
    *ppb = var_get_pblock(sn, rq, pbname);
    if (!*ppb)
        return PR_FAILURE;

    // Get the name and value from the name=value pair
    return var_get_name_value(nv, pname, pvalue);
}


/* ------------------------------ set_header ------------------------------ */

static void set_header(Session *sn, Request *rq, pblock *pblock, const char *n, const char *v)
{
    // Remove any existing parameters for the named header
    for (int i = 0; i < pblock->hsize; i++) {
for_pb_entry:
        for (struct pb_entry *p = pblock->ht[i]; p; p = p->next) {
            if (match(p->param->name, n) != MATCH_NONE) {
                pblock_remove(p->param->name, pblock);
                goto for_pb_entry;
            }
        }
    }

    if ((*n == 's' || *n == 'S') && !strcasecmp(n + 1, "status" + 1)) {
        // XXX NSAPI reserves "status" for the HTTP status
        n = "Status";
    } else {
        // Normalize header parameter name
        for (const char *p = n; *p != '\0'; p++) {
            if (isupper(*p)) {
                char *lower = pool_strdup(sn->pool, n);
                if (lower) {
                    util_strlower(lower);
                    n = lower;
                }
                break;
            }
        }
    }

    // Add the header to the pblock
    pblock_nvinsert(n, v, pblock);
}


/* --------------------------- set_internal_map --------------------------- */

static PRStatus set_internal_map(Session *sn, Request *rq, const char *n, const char *subscript, const char *v)
{
    switch (n[0]) {
    case 'c': // cookie
        if (!strcmp(n + 1, "cookie" + 1)) {
            cookie_set(sn, rq, subscript, v, NULL, NULL, NULL, COOKIE_MAX_AGE_SESSION, PR_FALSE);
            return PR_SUCCESS;
        }
        break;

    case 'h': // headers
        if (!strcmp(n + 1, "headers" + 1)) {
            set_header(sn, rq, rq->headers, subscript, v);
            return PR_SUCCESS;
        }
        break;

    case 's': // srvhdrs
        if (!strcmp(n + 1, "srvhdrs" + 1)) {
            set_header(sn, rq, rq->srvhdrs, subscript, v);
            return PR_SUCCESS;
        }
        break;
    }

    return PR_FAILURE;
}


/* ------------------------- set_internal_scalar -------------------------- */

static PRStatus set_internal_scalar(Session *sn, Request *rq, const char *n, const char *v)
{
    switch (n[0]) {
    case 's': // security, senthdrs
        switch (n[1]) {
        case 'e': // security, senthdrs
            switch (n[2]) {
            case 'c': // security
                if (!strcmp(n + 3, "security" + 3)) {
                    sn->ssl = util_getboolean(v, PR_FALSE);
                    if (sn->ssl) {
                        INTsession_fill_ssl(sn);
                    } else {
                        INTsession_empty_ssl(sn);
                    }
                    return PR_SUCCESS;
                }
                break;
            case 'n': // senthdrs
                if (!strcmp(n + 3, "senthdrs" + 3)) {
                    rq->senthdrs = util_getboolean(v, PR_FALSE);
                    return PR_SUCCESS;
                }
                break;
            }
            break;
        }
        break;

    case 'k': // keep_alive
        if (!strcmp(n + 1, "keep_alive" + 1)) {
            KEEP_ALIVE(rq) &= util_getboolean(v, PR_FALSE);
            return PR_SUCCESS;
        }
        break;
    }

    return PR_FAILURE;
}


/* ----------------------------- set_variable ----------------------------- */

static PRStatus set_variable(Session *sn, Request *rq, const char *n, const char *v, PRBool internal_only)
{
    // Does this look like a bracketed ${variable}?
    const char *closing = NULL;
    if (*n == '{') {
        closing = "}";
        n++;
    }

    // Extract the identifier from $identifier, ${identifier},
    // $identifier{'...'}, or ${identifier{'...'}}
    const char *p = n;
    while (isalpha(*p) || *p == '_')
        p++;

    // Allocate a copy of the identifier prefixed with $
    char *dollarname = (char *) pool_malloc(sn->pool, 1 + p - n + 1);
    if (!dollarname)
        return PR_FAILURE;
    dollarname[0] = '$';
    memcpy(dollarname + 1, n, p - n);
    dollarname[1 + p - n] = '\0';

    // Look for a subscript
    char *subscript = NULL;
    if (*p == '{') {
        // $map{'subscript'} or ${map{'subscript'}}
        // XXX We choke if the subscript is anything other than a simple
        // literal, but there's no real reason we couldn't parse a full blown
        // expression when necessary...
        p++;
        if (*p != '\'')
            return PR_FAILURE;
        p++;
        subscript = (char *) pool_malloc(sn->pool, strlen(p) + 1);
        if (!subscript)
            return PR_FAILURE;
        char *s = subscript;
        for (;;) {
            if (*p == '\0')
                return PR_FAILURE;
            if (*p == '\'')
                break;
            if (*p == '\\' && p[1] != '\0')
                p++;
            *s++ = *p++;
        }
        *s = '\0';
        p++;
        if (*p != '}')
            return PR_FAILURE;
        p++;
    }

    // Look for closing '}' in bracketed ${variables}s
    if (closing) {
        if (*p != *closing)
            return PR_FAILURE;
        p++;
    }

    // We found the end of the $variable; that should be the end of the string
    if (*p != '\0')
        return PR_FAILURE;

    // Now try to set the variable
    if (subscript) {
        // Check if this is a settable internal map variable
        if (set_internal_map(sn, rq, dollarname + 1, subscript, v) != PR_SUCCESS) {
            // Not a settable internal map variable.  If the $map{'...'}
            // identifier doesn't specify a pblock name, it's not a valid map
            // variable.
            pblock *pb = var_get_pblock(sn, rq, dollarname + 1);
            if (!pb)
                return PR_FAILURE;

            // Simple pblock-based $map{'...'} variable
            if (internal_only)
                return PR_FAILURE;
            pblock_nvreplace(subscript, v, pb);
        }
    } else {
        // Check if this is a settable internal variable
        if (set_internal_scalar(sn, rq, dollarname + 1, v) != PR_SUCCESS) {
            // Not a settable internal veriable.  If the $variable has a
            // specialized getter, treat it as read only.
            if (expr_var_get_func_find(dollarname + 1))
                return PR_FAILURE;

            // Simple user-defined $variable
            if (internal_only)
                return PR_FAILURE;
            pblock_nvreplace(dollarname, v, rq->vars);
        }
    }

    return PR_SUCCESS;
}


/* --------------------------- var_set_variable --------------------------- */

PRStatus var_set_variable(Session *sn, Request *rq, const char *n, const char *v)
{
    return set_variable(sn, rq, n, v, PR_FALSE);
}


/* ---------------------- var_set_internal_variable ----------------------- */

PRStatus var_set_internal_variable(Session *sn, Request *rq, const char *n, const char *v)
{
    return set_variable(sn, rq, n, v, PR_TRUE);
}


/* --------------------------- var_get_browser ---------------------------- */

static const Result *var_get_browser(const Arguments *args, Session *sn, Request *rq)
{
    if (char *browser = pblock_findkeyval(pb_key_user_agent, rq->headers))
        return result_string(sn, rq, browser, strlen(browser));

    return result_string(sn, rq, "", 0);
}


/* --------------------------- var_get_chunked ---------------------------- */

static const Result *var_get_chunked(const Arguments *args, Session *sn, Request *rq)
{
    return result_bool(sn, rq, CHUNKED(rq));
}


/* ----------------------------- var_get_code ----------------------------- */

static const Result *var_get_code(const Arguments *args, Session *sn, Request *rq)
{
    if (char *status = pblock_findkeyval(pb_key_status, rq->srvhdrs))
        return result_integer(sn, rq, atoi(status));

    return result_string(sn, rq, "", 0);
}


/* ------------------------------ var_get_id ------------------------------ */

static const Result *var_get_id(const Arguments *args, Session *sn, Request *rq)
{
    const char *id = vs_get_id(request_get_vs(rq));
    return result_string(sn, rq, id, strlen(id));
}


/* --------------------------- var_get_internal --------------------------- */

static const Result *var_get_internal(const Arguments *args, Session *sn, Request *rq)
{
    return result_bool(sn, rq, INTERNAL_REQUEST(rq));
}


/* -------------------------- var_get_keep_alive -------------------------- */

static const Result *var_get_keep_alive(const Arguments *args, Session *sn, Request *rq)
{
    return result_bool(sn, rq, KEEP_ALIVE(rq));
}


/* ----------------------------- var_get_path ----------------------------- */

static const Result *var_get_path(const Arguments *args, Session *sn, Request *rq)
{
    const char *path = pblock_findkeyval(pb_key_path, rq->vars);
    if (!path) {
        path = pblock_findkeyval(pb_key_ppath, rq->vars);
        if (!path) {
            path = pblock_findkeyval(pb_key_uri, rq->reqpb);
            if (!path)
                path = "";
        }
    }

    return result_string(sn, rq, path, strlen(path));
}


/* ---------------------------- var_get_reason ---------------------------- */

static const Result *var_get_reason(const Arguments *args, Session *sn, Request *rq)
{
    if (char *status = pblock_findkeyval(pb_key_status, rq->srvhdrs)) {
        if (char *reason = strchr(status, ' '))
            return result_string(sn, rq, reason + 1, strlen(reason + 1));
    }

    return result_string(sn, rq, "", 0);
}


/* -------------------------- var_get_restarted --------------------------- */

static const Result *var_get_restarted(const Arguments *args, Session *sn, Request *rq)
{
    return result_bool(sn, rq, RESTARTED_REQUEST(rq));
}


/* --------------------------- var_get_security --------------------------- */

static const Result *var_get_security(const Arguments *args, Session *sn, Request *rq)
{
    return result_bool(sn, rq, GetSecurity(sn));
}


/* --------------------------- var_get_senthdrs --------------------------- */

static const Result *var_get_senthdrs(const Arguments *args, Session *sn, Request *rq)
{
    return result_bool(sn, rq, rq->senthdrs);
}


/* -------------------------- var_get_server_url -------------------------- */

static const Result *var_get_server_url(const Arguments *args, Session *sn, Request *rq)
{
    const char *server_url = http_uri2url_dynamic("", "", sn, rq);
    return result_string(sn, rq, server_url, strlen(server_url));
}


/* ----------------------------- var_get_time ----------------------------- */

static const Result *var_get_time(const Arguments *args, Session *sn, Request *rq)
{
    return result_integer(sn, rq, rq->req_start);
}


/* --------------------------- var_get_time_day --------------------------- */

static const Result *var_get_time_day(const Arguments *args, Session *sn, Request *rq)
{
    struct tm tm;
    util_localtime(&rq->req_start, &tm);
    char buf[sizeof("dd")];
    int len = util_strftime(buf, "%d", &tm);
    return result_string(sn, rq, buf, len);
}


/* -------------------------- var_get_time_hour --------------------------- */

static const Result *var_get_time_hour(const Arguments *args, Session *sn, Request *rq)
{
    struct tm tm;
    util_localtime(&rq->req_start, &tm);
    char buf[sizeof("HH")];
    int len = util_strftime(buf, "%H", &tm);
    return result_string(sn, rq, buf, len);
}


/* --------------------------- var_get_time_min --------------------------- */

static const Result *var_get_time_min(const Arguments *args, Session *sn, Request *rq)
{
    struct tm tm;
    util_localtime(&rq->req_start, &tm);
    char buf[sizeof("MM")];
    int len = util_strftime(buf, "%M", &tm);
    return result_string(sn, rq, buf, len);
}


/* --------------------------- var_get_time_mon --------------------------- */

static const Result *var_get_time_mon(const Arguments *args, Session *sn, Request *rq)
{
    struct tm tm;
    util_localtime(&rq->req_start, &tm);
    char buf[sizeof("mm")];
    int len = util_strftime(buf, "%m", &tm);
    return result_string(sn, rq, buf, len);
}


/* --------------------------- var_get_time_sec --------------------------- */

static const Result *var_get_time_sec(const Arguments *args, Session *sn, Request *rq)
{
    struct tm tm;
    util_localtime(&rq->req_start, &tm);
    char buf[sizeof("SS")];
    int len = util_strftime(buf, "%S", &tm);
    return result_string(sn, rq, buf, len);
}


/* -------------------------- var_get_time_wday --------------------------- */

static const Result *var_get_time_wday(const Arguments *args, Session *sn, Request *rq)
{
    struct tm tm;
    util_localtime(&rq->req_start, &tm);
    char buf[sizeof("w")];
    int len = util_strftime(buf, "%w", &tm);
    return result_string(sn, rq, buf, len);
}


/* -------------------------- var_get_time_year --------------------------- */

static const Result *var_get_time_year(const Arguments *args, Session *sn, Request *rq)
{
    struct tm tm;
    util_localtime(&rq->req_start, &tm);
    char buf[sizeof("YYYY")];
    int len = util_strftime(buf, "%Y", &tm);
    return result_string(sn, rq, buf, len);
}


/* ----------------------------- var_get_url ------------------------------ */

static const Result *var_get_url(const Arguments *args, Session *sn, Request *rq)
{
    const char *uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
    if (!uri)
        uri = "";
       
    char *url = http_uri2url_dynamic(uri, "", sn, rq);
    if (!url)
        return result_out_of_memory(sn, rq);

    int len = strlen(url);
    if (const char *q = pblock_findkeyval(pb_key_query, rq->reqpb)) {
        int qlen = strlen(q);
        url = (char *) pool_realloc(sn->pool, url, len + 1 + qlen + 1);
        if (!url)
            return result_out_of_memory(sn, rq);
        url[len] = '?';
        len++;
        strcpy(url + len, q);
        len += qlen;
    }

    return result_string(sn, rq, url, len);
}


/* --------------------------- var_get_urlhost ---------------------------- */

static const Result *var_get_urlhost(const Arguments *args, Session *sn, Request *rq)
{
    PR_ASSERT(rq->hostname);
    PR_ASSERT(!util_host_port_suffix(rq->hostname));

    const char *hostname = rq->hostname;
    if (!hostname)
        hostname = "localhost";
        
    return result_string(sn, rq, hostname, strlen(hostname));
}


/* ----------------------------- map_get_env ------------------------------ */

static const Result *map_get_env(const Arguments *args, Session *sn, Request *rq)
{
    const Result *r = result_expr(sn, rq, args_get(args, 0));
    if (result_is_error(r))
        return r;

    const char *subscript;
    result_as_const_string(sn, rq, r, &subscript, NULL);

    if (char **env = cgi_common_vars(sn, rq, NULL)) {
        if (const char *value = util_env_find(env, subscript))
            return result_string(sn, rq, value, strlen(value));
    }

    if (const char *value = getenv(subscript))
        return result_string(sn, rq, value, strlen(value));

    return result_error(sn, rq, XP_GetAdminStr(DBT_noMapXSubscriptY), "env", subscript);
}


/* ----------------------- check_set_cookie_header ------------------------ */

static const char *check_set_cookie_header(const char *name, const char *header)
{
    while (isspace(*header))
        header++;

    while (*name == *header && *header != '\0' && *header != '=' && *header != ';' && !isspace(*header)) {
        name++;
        header++;
    }

    if (*name != '\0')
        return NULL;

    if (*header == '=')
        header++;

    return header;
}


/* ---------------------------- map_get_cookie ---------------------------- */

static const Result *map_get_cookie(const Arguments *args, Session *sn, Request *rq)
{
    SUBSCRIPT_STRING(args, sn, rq, subscript)

    // Check request cookies
    if (char *cookie = pblock_findkeyval(pb_key_cookie, rq->headers)) {
        char *buf = pool_strdup(sn->pool, cookie);
        if (!buf)
            return result_out_of_memory(sn, rq);

        char *value = util_cookie_find(buf, subscript);
        if (value)
            return result_string(sn, rq, value, strlen(value));
    }

    // Check response cookies
    for (int i = 0; i < rq->srvhdrs->hsize; i++) {
        for (struct pb_entry *p = rq->srvhdrs->ht[i]; p; p = p->next) {
            if (match(p->param->name, "set-cookie") != MATCH_NONE) {
                const char *header = p->param->value;
                const char *value = check_set_cookie_header(subscript, header);
                if (value) {
                    int len;
                    if (const char *semicolon = strchr(value, ';')) {
                        len = semicolon - value;
                    } else {
                        len = strlen(value);
                    }
                    return result_string(sn, rq, value, len);
                }
            }
        }
    }

    return result_error(sn, rq, XP_GetAdminStr(DBT_noMapXSubscriptY), "cookie", subscript);
}


/* ------------------------------- var_init ------------------------------- */

DEFINE_MAP_GET_PB(sn->client, "$client", map_get_client)
DEFINE_MAP_GET_PB(rq->vars, "$vars", map_get_vars)
DEFINE_MAP_GET_PB(rq->reqpb, "$reqpb", map_get_reqpb)
DEFINE_MAP_GET_CASE_PB(rq->headers, "$headers", map_get_headers)
DEFINE_MAP_GET_CASE_PB(rq->srvhdrs, "$srvhdrs", map_get_srvhdrs)
DEFINE_VAR_GET_PB(rq->vars, pb_key_auth_group, "$auth_group", var_get_auth_group)
DEFINE_VAR_GET_PB(rq->vars, pb_key_auth_type, "$auth_type", var_get_auth_type)
DEFINE_VAR_GET_PB(rq->vars, pb_key_auth_user, "$auth_user", var_get_auth_user)
DEFINE_VAR_GET_PB(sn->client, pb_key_dns, "$dns", var_get_dns)
DEFINE_VAR_GET_PB(sn->client, pb_key_ip, "$ip", var_get_ip)
DEFINE_VAR_GET_PB(sn->client, pb_key_keysize, "$keysize", var_get_keysize)
DEFINE_VAR_GET_PB(rq->reqpb, pb_key_method, "$method", var_get_method)
DEFINE_VAR_GET_PB(rq->vars, pb_key_path_info, "$path_info", var_get_path_info)
DEFINE_VAR_GET_PB(rq->vars, pb_key_ppath, "$ppath", var_get_ppath)
DEFINE_VAR_GET_PB(rq->reqpb, pb_key_protocol, "$protocol", var_get_protocol)
DEFINE_VAR_GET_PB(rq->reqpb, pb_key_query, "$query", var_get_query)
DEFINE_VAR_GET_PB(rq->headers, pb_key_referer, "$referer", var_get_referer)
DEFINE_VAR_GET_PB(sn->client, pb_key_secret_keysize, "$secret_keysize", var_get_secret_keysize)
DEFINE_VAR_GET_PB(rq->srvhdrs, pb_key_content_type, "$type", var_get_type)
DEFINE_VAR_GET_PB(rq->reqpb, pb_key_uri, "$uri", var_get_uri)

PRStatus var_init(void)
{
    expr_map_get_func_insert("client", &map_get_client);
    expr_map_get_func_insert("vars", &map_get_vars);
    expr_map_get_func_insert("reqpb", &map_get_reqpb);
    expr_map_get_func_insert("headers", &map_get_headers);
    expr_map_get_func_insert("srvhdrs", &map_get_srvhdrs);
    expr_map_get_func_insert("env", &map_get_env);
    expr_map_get_func_insert("cookie", &map_get_cookie);

    expr_var_get_func_insert("auth_group", &var_get_auth_group);
    expr_var_get_func_insert("auth_type", &var_get_auth_type);
    expr_var_get_func_insert("auth_user", &var_get_auth_user);
    expr_var_get_func_insert("browser", &var_get_browser);
    expr_var_get_func_insert("chunked", &var_get_chunked);
    expr_var_get_func_insert("code", &var_get_code);
    expr_var_get_func_insert("dns", &var_get_dns);
    expr_var_get_func_insert("id", &var_get_id);
    expr_var_get_func_insert("internal", &var_get_internal);
    expr_var_get_func_insert("ip", &var_get_ip);
    expr_var_get_func_insert("keep_alive", &var_get_keep_alive);
    expr_var_get_func_insert("keysize", &var_get_keysize);
    expr_var_get_func_insert("method", &var_get_method);
    expr_var_get_func_insert("path", &var_get_path);
    expr_var_get_func_insert("path_info", &var_get_path_info);
    expr_var_get_func_insert("ppath", &var_get_ppath);
    expr_var_get_func_insert("protocol", &var_get_protocol);
    expr_var_get_func_insert("query", &var_get_query);
    expr_var_get_func_insert("reason", &var_get_reason);
    expr_var_get_func_insert("referer", &var_get_referer);
    expr_var_get_func_insert("restarted", &var_get_restarted);
    expr_var_get_func_insert("secret_keysize", &var_get_secret_keysize);
    expr_var_get_func_insert("security", &var_get_security);
    expr_var_get_func_insert("senthdrs", &var_get_senthdrs);
    expr_var_get_func_insert("server_url", &var_get_server_url);
    expr_var_get_func_insert("time", &var_get_time);
    expr_var_get_func_insert("time_day", &var_get_time_day);
    expr_var_get_func_insert("time_hour", &var_get_time_hour);
    expr_var_get_func_insert("time_min", &var_get_time_min);
    expr_var_get_func_insert("time_mon", &var_get_time_mon);
    expr_var_get_func_insert("time_sec", &var_get_time_sec);
    expr_var_get_func_insert("time_wday", &var_get_time_wday);
    expr_var_get_func_insert("time_year", &var_get_time_year);
    expr_var_get_func_insert("type", &var_get_type);
    expr_var_get_func_insert("uri", &var_get_uri);
    expr_var_get_func_insert("url", &var_get_url);
    expr_var_get_func_insert("urlhost", &var_get_urlhost);

    return PR_SUCCESS;
}
