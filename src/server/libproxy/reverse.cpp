/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * reverse.c: Reverse proxying
 *
 * Ari Luotonen
 * Copyright (c) 1995 Netscape Communcations Corporation
 */

#include "netsite.h"
#include "base/pool.h"
#include "base/shexp.h"
#include "base/session.h"
#include "base/util.h"            /* util_sprintf */
#include "frame/log.h"            /* log_error */
#include "frame/req.h"
#include "libproxy/url.h"
#include "libproxy/util.h"
#include "libproxy/dbtlibproxy.h"
#include "libproxy/reverse.h"


/*
 * SAF names
 */
#define REVERSE_MAP ("reverse-map")

#define REWRITE_PREFIX "rewrite-"
#define REWRITE_PREFIX_LEN (sizeof(REWRITE_PREFIX) - 1)

/*
 * ReverseMapping describes an individual reverse mapping
 */
struct ReverseMapping {
    char *src; // "from" URL prefix
    int src_len; // length of "from" URL prefix
    char *dst; // "to" URL prefix
    int dst_len; // length of "to" URL prefix
};

/*
 * ReverseConfig tracks reverse mapping settings per-Request
 */
struct ReverseConfig {
    pool_handle_t *pool;

    int num_mappings; // number of initialize elements in mappings[]
    int mappings_capacity; // number of mappings[] elements allocated
    ReverseMapping *mappings; // reverse mappings

    PRBool rewrite_location;
    PRBool rewrite_content_location;
    PRBool rewrite_set_cookie;

    int num_headers; // number of initialized elements in headers[]
    int headers_capacity; // number of headers[] elements allocated
    char **headers; // names of headers to rewrite
};

/*
 * Allocation quantum for the ReverseConfig mappings array
 */
#define DEFAULT_MAPPINGS_CAPACITY 8

/*
 * Allocation quantum for the ReverseConfig headers array
 */
#define DEFAULT_HEADERS_CAPACITY 4

/*
 * request_get_data/request_set_data slot that holds a ReverseConfig *
 */
static int _config_request_slot = -1;


/* ----------------------------- free_config ------------------------------ */

static void free_config(ReverseConfig *config)
{
    int i;

    PR_ASSERT(config != NULL);

    for (i = 0; i < config->num_mappings; i++) {
        pool_free(config->pool, config->mappings[i].src);
        pool_free(config->pool, config->mappings[i].dst);
    }
    pool_free(config->pool, config->mappings);

    for (i = 0; i < config->num_headers; i++) {
        if (config->headers[i])
            pool_free(config->pool, config->headers[i]);
    }
    pool_free(config->pool, config->headers);

    pool_free(config->pool, config);
}


/* ---------------- reverse_config_request_slot_destructor ---------------- */

extern "C" void reverse_config_request_slot_destructor(void *data)
{
    free_config((ReverseConfig *)data);
}


/* ----------------------------- reverse_init ----------------------------- */

PRStatus reverse_init(void)
{
    // Allocate a slot to store ReverseConfig per-Request
    _config_request_slot =
        request_alloc_slot(&reverse_config_request_slot_destructor);

    return PR_SUCCESS;
}


/* ------------------------------ get_config ------------------------------ */

static inline ReverseConfig * get_config(Session *sn, Request *rq)
{
    ReverseConfig *config;

    if (!rq)
        return NULL;

    config = (ReverseConfig *)request_get_data(rq, _config_request_slot);
    if (!config) {
        // Create a new per-Request ReverseConfig
        config = (ReverseConfig *)pool_malloc(sn->pool, sizeof(ReverseConfig));
        if (!config)
            return NULL;

        config->pool = sn->pool;
        config->num_mappings = 0;
        config->mappings_capacity = 0;
        config->mappings = NULL;
        config->rewrite_location = PR_TRUE;
        config->rewrite_content_location = PR_TRUE;
        config->rewrite_set_cookie = PR_TRUE;
        config->num_headers = 0;
        config->headers_capacity = 0;
        config->headers = NULL;

        request_set_data(rq, _config_request_slot, config);
    }

    return config;
}


/* ------------------------------ add_header ------------------------------ */

static inline PRStatus add_header(ReverseConfig *config, const char *name)
{
    PRStatus rv = PR_FAILURE;

    // Grow headers[] as necessary
    if (config->num_headers >= config->headers_capacity) {
        int capacity = config->headers_capacity + DEFAULT_HEADERS_CAPACITY;
        char **headers = (char **)
            pool_realloc(config->pool,
                         config->headers,
                         capacity * sizeof(config->headers[0]));
        if (headers) {
            config->headers = headers;
            config->headers_capacity = capacity;
        }
    }

    // Add the new name/key to the headers[] array
    if (config->num_headers < config->headers_capacity) {
        config->headers[config->num_headers] = pool_strdup(config->pool, name);
        config->num_headers++;
        rv = PR_SUCCESS;
    }

    return rv;
}


/* ------------------------------ set_header ------------------------------ */

static PRStatus set_header(ReverseConfig *config, const char *name, PRBool b)
{
    PRBool found = PR_FALSE;

    for (int i = 0; i < config->num_headers; i++) {
        if (config->headers[i] && !strcasecmp(config->headers[i], name)) {
            found = PR_TRUE;
            if (!b)
                config->headers[i] = NULL;
        }
    }

    if (b) {
        if (found) {
            return PR_SUCCESS;
        } else {
            return add_header(config, name);
        }
    }

    return PR_SUCCESS;
}


/* ----------------------- reverse_map_set_headers ------------------------ */

PRStatus reverse_map_set_headers(pblock *pb, Session *sn, Request *rq)
{
    ReverseConfig *config = get_config(sn, rq);
    if (!config)
        return PR_FAILURE;

    // Process "rewrite-" parameters
    for (int i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        while (p) {
            if (*p->param->name == 'r' &&
                !strncmp(p->param->name, REWRITE_PREFIX, REWRITE_PREFIX_LEN))
            {
                int t = util_getboolean(p->param->value, -1);
                if (t == -1) {
                    const char *fn = pblock_findkeyval(pb_key_fn, pb);
                    log_error(LOG_MISCONFIG, fn, sn, rq,
                              "invalid %s value: %s (expected boolean)",
                              p->param->name, p->param->value);
                    return PR_FAILURE;
                }

                const pb_key *key = param_key(p->param);
                if (key == pb_key_rewrite_location) {
                    config->rewrite_location = t;
                } else if (key == pb_key_rewrite_content_location) {
                    config->rewrite_content_location = t;
                } else if (key == pb_key_rewrite_set_cookie) {
                    config->rewrite_set_cookie = t;
                } else if (key == pb_key_rewrite_host) {
                    // Ignore.  set-origin-server knows what to do with this.
                } else {
                    set_header(config, p->param->name + REWRITE_PREFIX_LEN, t);
                }
            }
            p = p->next;
        }
    }

    return PR_SUCCESS;
}


/* --------------------------- reverse_map_add ---------------------------- */

PRStatus reverse_map_add(Session *sn,
                         Request *rq,
                         const char *src,
                         const char *dst)
{
    PRStatus rv = PR_FAILURE;

    if (!src || !dst)
	return PR_FAILURE;

    ReverseConfig *config = get_config(sn, rq);
    if (!config)
        return PR_FAILURE;

    // Grow mappings[] as necessary
    if (config->num_mappings >= config->mappings_capacity) {
        int capacity = config->mappings_capacity + DEFAULT_MAPPINGS_CAPACITY;
        ReverseMapping *mappings = (ReverseMapping *)
            pool_realloc(config->pool,
                         config->mappings,
                         capacity * sizeof(config->mappings[0]));
        if (mappings) {
            config->mappings = mappings;
            config->mappings_capacity = capacity;
        }
    }

    // Add src and dst to mappings[]
    if (config->num_mappings < config->mappings_capacity) {
        int src_len = strlen(src);
        char *src_copy = (char *)pool_malloc(config->pool, src_len + 1);
        int dst_len = strlen(dst);
        char *dst_copy = (char *)pool_malloc(config->pool, dst_len + 1);
        if (src_copy && dst_copy) {
            strcpy(src_copy, src);
            config->mappings[config->num_mappings].src = src_copy;
            config->mappings[config->num_mappings].src_len = src_len;
            strcpy(dst_copy, dst);
            config->mappings[config->num_mappings].dst = dst_copy;
            config->mappings[config->num_mappings].dst_len = dst_len;
            config->num_mappings++;
            rv = PR_SUCCESS;
        } else {
            pool_free(config->pool, src_copy);
            pool_free(config->pool, dst_copy);
        }
    }

    return PR_SUCCESS;
}


/* -------------------------- ntrans_reverse_map -------------------------- */

/*
 * XXX
 *
 * This SAF is called at NameTrans time to add a new reverse mapping.  Later,
 * the Service SAF must call back into reverse_map_rewrite() to actually
 * perform the mapping.  That sucks.
 *
 * Instead, an Output SAF could perform the reverse mapping.
 */

int ntrans_reverse_map(pblock *pb, Session *sn, Request *rq)
{
    char *src = pblock_findkeyval(pb_key_from, pb);
    char *dst = pblock_findkeyval(pb_key_to, pb);

    if (!src || !dst) {
	log_error(LOG_MISCONFIG, REVERSE_MAP, sn, rq,
                  XP_GetAdminStr(DBT_need_from_and_to));
	return REQ_ABORTED;
    }

    if (reverse_map_add(sn, rq, src, dst) == PR_FAILURE)
        return REQ_ABORTED;

    if (reverse_map_set_headers(pb, sn, rq) == PR_FAILURE)
        return REQ_ABORTED;

    return REQ_NOACTION;
}


/* --------------------------- reverse_map_for ---------------------------- */

char *reverse_map_for(Session *sn, Request *rq, const char *url)
{
    ReverseConfig *config;

    if (!url)
	return NULL;

    config = (ReverseConfig *)request_get_data(rq, _config_request_slot);
    if (config) {
        while (isspace(*url))
            url++;

        char *url_copy = pool_strdup(sn->pool, url);
        if (!url_copy)
            return NULL;

        util_url_fix_hostname(url_copy);

        for (int i = 0; i < config->num_mappings; i++) {
            const char *src = config->mappings[i].src;
            int src_len = config->mappings[i].src_len;
            const char *matched_url = NULL;

            if (!strncasecmp(src, url, src_len)) {
                matched_url = url;
            } else if (!strncasecmp(src, url_copy, src_len)) {
                matched_url = url_copy;
            }

            if (matched_url) {
                int old_url_len = strlen(matched_url);
                int new_url_len = old_url_len - src_len +
                                  config->mappings[i].dst_len;

                char *new_url = (char *)pool_malloc(sn->pool, new_url_len + 1);
                if (!new_url)
                    return NULL;

                strcpy(new_url, config->mappings[i].dst);
                strcpy(new_url + config->mappings[i].dst_len,
                       matched_url + src_len);

                pool_free(sn->pool, url_copy);

                return new_url;
            }
        }

        pool_free(sn->pool, url_copy);
    }

    return NULL;
}


/* -------------------------- reverse_map_domain -------------------------- */

static char *reverse_map_domain(Session *sn, Request *rq, const char *domain)
{
    ReverseConfig *config;

    if (!domain)
        return NULL;

    config = (ReverseConfig *)request_get_data(rq, _config_request_slot);
    if (config) {
        while (isspace(*domain))
            domain++;

        int dot;
        if (*domain == '.') {
            domain++;
            dot = 1;
        } else {
            dot = 0;
        }

        int domain_len = strlen(domain);

        for (int i = 0; i < config->num_mappings; i++) {
            ParsedUrl src;
            if (url_parse(config->mappings[i].src, &src) == PR_SUCCESS &&
                src.host_len == domain_len &&
                !memcmp(domain, src.host, domain_len))
            {
                ParsedUrl dst;
                if (url_parse(config->mappings[i].dst, &dst) == PR_SUCCESS) {
                    int len = dot + dst.host_len;
                    char *new_domain = (char *)pool_malloc(sn->pool, len + 1);
                    if (!new_domain)
                        return NULL;

                    if (dot)
                        new_domain[0] = '.';
                    memcpy(new_domain + dot, dst.host, dst.host_len);
                    new_domain[len] = '\0';

                    return new_domain;
                }

            }
        }
    }

    return NULL;
}


/* ---------------------------- rewrite_param ----------------------------- */

static void rewrite_param(Session *sn, Request *rq, pb_param *pp, char *value)
{
    log_error(LOG_VERBOSE, NULL, sn, rq,
              "mapped \"%s: %s\" to \"%s: %s\"",
              pp->name,
              pp->value,
              pp->name,
              value);

    pool_free(sn->pool, pp->value);
    pp->value = value;
}


/* ----------------------------- rewrite_url ------------------------------ */

static PRBool rewrite_url(Session *sn, Request *rq, pb_param *pp)
{
    char *url = reverse_map_for(sn, rq, pp->value);
    if (!url)
        return PR_FALSE;

    rewrite_param(sn, rq, pp, url);

    return PR_TRUE;
}


/* -------------------------- rewrite_set_cookie -------------------------- */

static PRBool rewrite_set_cookie(Session *sn, Request *rq, pb_param *pp)
{
    char *copy = pool_strdup(sn->pool, pp->value);
    if (!copy)
        return PR_FALSE;

    PRBool rewritten = PR_FALSE;

    char *old_domain = util_cookie_find(copy, "domain");
    if (old_domain) {
        char *new_domain = reverse_map_domain(sn, rq, old_domain);
        if (new_domain) {
            int prefix_len = old_domain - copy;
            const char *suffix = pp->value + prefix_len + strlen(old_domain);
            int suffix_len = strlen(suffix);
            int new_domain_len = strlen(new_domain);
            int len = prefix_len + new_domain_len + suffix_len;
            char *p = (char *)pool_malloc(sn->pool, len + 1);
            if (p) {
                memcpy(p, pp->value, prefix_len);
                memcpy(p + prefix_len, new_domain, new_domain_len);
                memcpy(p + prefix_len + new_domain_len, suffix, suffix_len);
                p[len] = '\0';

                rewrite_param(sn, rq, pp, p);

                rewritten = PR_TRUE;
            }
        }
    }

    pool_free(sn->pool, copy);

    return rewritten;
}


/* ------------------------- reverse_map_rewrite -------------------------- */

PRBool reverse_map_rewrite(Session *sn, Request *rq)
{
    ReverseConfig *config;
    int rewritten = 0;

    config = (ReverseConfig *)request_get_data(rq, _config_request_slot);
    if (config) {
        pb_param *pp;

        if (config->rewrite_location) {
            if (pp = pblock_findkey(pb_key_location, rq->srvhdrs))
                rewritten += rewrite_url(sn, rq, pp);
        }

        if (config->rewrite_content_location) {
            if (pp = pblock_findkey(pb_key_content_location, rq->srvhdrs))
                rewritten += rewrite_url(sn, rq, pp);
        }

        if (config->rewrite_set_cookie) {
            if (pblock_findkey(pb_key_set_cookie, rq->srvhdrs)) {
                for (int i = 0; i < rq->srvhdrs->hsize; i++) {
                    for (pb_entry *p = rq->srvhdrs->ht[i]; p; p = p->next) {
                        if (param_key(p->param) == pb_key_set_cookie)
                            rewritten += rewrite_set_cookie(sn, rq, p->param);
                    }
                }
            }
        }

        for (int i = 0; i < config->num_headers; i++) {
            if (config->headers[i]) {
                if (pp = pblock_find(config->headers[i], rq->srvhdrs))
                    rewritten += rewrite_url(sn, rq, pp);
            }
        }
    }

    return rewritten ? PR_TRUE : PR_FALSE;
}
