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

#include "netsite.h"
#include "NsprWrap/NsprError.h"
#include "time/nstime.h"
#include "base/pblock.h"
#include "base/pool.h"
#include "base/util.h"
#include "frame/cookie.h"
#include "frame/dbtframe.h"

#define COOKIE_DATE_FMT "%a, %d-%b-%Y %T GMT"
#define COOKIE_DATE_END_OF_TIME "Tue, 19-Jan-2038 03:14:07 GMT"
#define COOKIE_DATE_END_OF_TIME_LEN (sizeof(COOKIE_DATE_END_OF_TIME) - 1)
#define COOKIE_PATH_PREFIX "; path="
#define COOKIE_PATH_PREFIX_LEN (sizeof(COOKIE_PATH_PREFIX) - 1)
#define COOKIE_DOMAIN_PREFIX "; domain="
#define COOKIE_DOMAIN_PREFIX_LEN (sizeof(COOKIE_DOMAIN_PREFIX) - 1)
#define COOKIE_EXPIRES_PREFIX "; expires="
#define COOKIE_EXPIRES_PREFIX_LEN (sizeof(COOKIE_EXPIRES_PREFIX) - 1)
#define COOKIE_SECURE "; secure"
#define COOKIE_SECURE_LEN (sizeof(COOKIE_SECURE) - 1)

#define APPEND(buf, s, len) \
        memcpy(buf, s, len); \
        buf += len;

#define APPEND_NAME_VALUE(buf, PREFIX, suffix, nv_len) \
        if (nv_len) { \
            int prefix_len = PREFIX##_LEN; \
            int suffix_len = nv_len - prefix_len; \
            APPEND(buf, PREFIX, prefix_len); \
            APPEND(buf, suffix, suffix_len); \
        }


/* -------------------------- cookie_format_date -------------------------- */

int cookie_format_date(char *buf, int size, time_t t)
{
    PR_ASSERT(size >= COOKIE_DATE_SIZE);

    struct tm tm;
    util_gmtime(&t, &tm);
    int rv = util_strftime(buf, COOKIE_DATE_FMT, &tm);

    PR_ASSERT(rv == COOKIE_DATE_LEN);

    return rv;
}


/* ------------------------------ cookie_set ------------------------------ */

int cookie_set(Session *sn, Request *rq,
               const char *name,
               const char *value,
               const char *path,
               const char *domain,
               const char *expires,
               PRInt64 max_age,
               PRBool secure)
{
    int name_len = strcspn(name, "\"=;, \v\f\t\r\n");
    if (name[name_len] != '\0') {
        NsprError::setError(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_badCookieName));
        return REQ_ABORTED;
    }

    int value_len;
    if (value) {
        value_len = strcspn(value, "\";\r\n");
        if (value[value_len] != '\0') {
            NsprError::setError(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_badCookieValue));
            return REQ_ABORTED;
        }
    } else {
        value = "null";
        value_len = 4;
    }

    int path_nv_len;
    if (path) {
        path_nv_len = COOKIE_PATH_PREFIX_LEN + strlen(path);
    } else {
        path = "/";
        path_nv_len = COOKIE_PATH_PREFIX_LEN + 1;
    }

    int domain_nv_len = domain ? COOKIE_DOMAIN_PREFIX_LEN + strlen(domain) : 0;

    int expires_nv_len;
    if (expires) {
        expires_nv_len = COOKIE_EXPIRES_PREFIX_LEN + strlen(expires);
    } else if (max_age != COOKIE_MAX_AGE_SESSION) {
        expires_nv_len = COOKIE_EXPIRES_PREFIX_LEN + COOKIE_DATE_LEN;
    } else {
        expires_nv_len = 0;
    }

    int secure_len = secure ? COOKIE_SECURE_LEN : 0;

    int cookie_len = name_len + 1 + value_len +
                     path_nv_len + 
                     domain_nv_len + 
                     expires_nv_len +
                     secure_len;

    pb_param *pp = pblock_key_param_create(rq->srvhdrs,
                                           pb_key_set_cookie,
                                           NULL, cookie_len);
    if (!pp)
        return REQ_ABORTED;
    char *buf = pp->value;

    APPEND(buf, name, name_len);
    *buf++ = '=';
    APPEND(buf, value, value_len);

    APPEND_NAME_VALUE(buf, COOKIE_PATH_PREFIX, path, path_nv_len);

    APPEND_NAME_VALUE(buf, COOKIE_DOMAIN_PREFIX, domain, domain_nv_len);

    if (expires) {
        APPEND_NAME_VALUE(buf, COOKIE_EXPIRES_PREFIX, expires, expires_nv_len);
    } else if (max_age != COOKIE_MAX_AGE_SESSION) {
        // N.B. some browsers won't grok time_t values > 31 bits
        PRInt64 t;
        if (max_age == COOKIE_MAX_AGE_INFINITE) {
            t = PR_INT32_MAX;
        } else {
            t = ft_time() + max_age;
        }
        if (t < PR_INT32_MAX) {
            APPEND(buf, COOKIE_EXPIRES_PREFIX, COOKIE_EXPIRES_PREFIX_LEN);
            buf += cookie_format_date(buf, COOKIE_DATE_SIZE, t);
        } else {
            APPEND(buf, COOKIE_EXPIRES_PREFIX, COOKIE_EXPIRES_PREFIX_LEN);
            APPEND(buf, COOKIE_DATE_END_OF_TIME, COOKIE_DATE_END_OF_TIME_LEN);
        }
    } else {
        PR_ASSERT(expires_nv_len == 0);
    }

    APPEND(buf, COOKIE_SECURE, secure_len);

    PR_ASSERT(buf - pp->value == cookie_len);
    *buf = '\0';

    pblock_kpinsert(pb_key_set_cookie, pp, rq->srvhdrs);

    return REQ_PROCEED;
}
