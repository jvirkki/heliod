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
 * host_dns_cache.cpp
 *
 * Proxy DNS Cache - Cache of DNS entries
 * Note that this is a host->ip DNS cache
 * unlike ip->host DNS cache in dns_cache.cpp
 */

#include "base/cache.h"
#include "base/util.h"
#include "base/pool.h"
#include "frame/log.h"
#include "libproxy/host_dns_cache.h"
#include "libproxy/util.h"

#include "libproxy/dbtlibproxy.h"

/* ----- Macros ------------------------------------------------------- */
#define DEFAULT_EXPIRE_TIME    (20*60)        /* 20 minutes */
#define ABSOLUTE_EXPIRE_MAX    (60*60*24*365)    /* 1 year */

#define DEFAULT_CACHE_MAX          1024
#define ABSOLUTE_CACHE_MIN         32    /* disallow smaller sizes */
#define ABSOLUTE_CACHE_MAX         32768    /* disallow larger sizes */
#define DEFAULT_HASH_SIZE          (2*DEFAULT_CACHE_MAX)

#define DNS_CACHE_INIT         "dns-cache-init"
#define DNS_CACHE_INSERT       "dns-cache-insert"

/* ----- Forwards ----------------------------------------------------- */
static unsigned int dns_cache_hash_host_name(unsigned int, void *);
static int dns_cache_compare_keys(void *, void *);
static int dns_cache_cleanup(void *);
static int dns_cache_debug(pblock *pb, Session *sn, Request *rq);
static int dns_cache_print(void *data, SYS_NETFD fd);

/* ----- Static globals ----------------------------------------------- */
static cache_t          *dns_cache = NULL;
static unsigned long    dns_expire_time = DEFAULT_EXPIRE_TIME;
static PRBool           negative_dns_cache = PR_TRUE;

static public_cache_functions_t dns_cache_functions = {
    dns_cache_hash_host_name,
    dns_cache_debug
};

static cache_entry_functions_t dns_cache_entry_functions = {
    dns_cache_compare_keys,
    dns_cache_cleanup,
    dns_cache_print
};

/*
 * request_get_data/request_set_data slot that holds a PRHostEnt *
 */
static int _dns_request_slot = -1;

NSAPI_PUBLIC PRStatus host_dns_cache_hostent_init(void)
{
    // Allocate a slot to store a struct hostent * per-Request
    _dns_request_slot = request_alloc_slot(NULL);

    return PR_SUCCESS;
}

/*
 * Used to duplicate a PRHostEnt structure stored in the dns cache
 * onto a sessions memory pool.
 */
static PRHostEnt *hostent_dup(PRHostEnt *hostent, pool_handle_t *pool = NULL)
{
    PRHostEnt *newent = NULL;
    int len = 0, na = 0;
    char **ap = NULL;

    if (pool)
        newent = (PRHostEnt *) pool_malloc(pool, sizeof(PRHostEnt));
    else
        newent = (PRHostEnt *) PERM_MALLOC(sizeof(PRHostEnt));

    // copy the easy stuff
    newent->h_addrtype = hostent->h_addrtype;
    newent->h_length = hostent->h_length;

    // allocate and copy official name
    len = strlen(hostent->h_name) + 1;
    if (pool)
        newent->h_name = (char *) pool_malloc(pool, sizeof(char)*len);
    else
        newent->h_name = (char *) PERM_MALLOC(sizeof(char)*len);
    memcpy(newent->h_name, hostent->h_name, len);

    // copy aliases and addresses
    if (pool) {
        newent->h_aliases = util_strlist_pool_dup(hostent->h_aliases, pool);
        newent->h_addr_list = util_strlist_pool_dup(hostent->h_addr_list, pool);
    } else {
        newent->h_aliases = util_strlist_dup(hostent->h_aliases);
        newent->h_addr_list = util_strlist_dup(hostent->h_addr_list);
    }

    return newent;
}

static void hostent_free(PRHostEnt *hp)
{
    char **s = NULL;

    if (hp->h_name)
        PERM_FREE(hp->h_name);

    util_strlist_free(hp->h_aliases);
    util_strlist_free(hp->h_addr_list);
    PERM_FREE(hp);
}

static unsigned int 
dns_cache_hash_host_name(unsigned int hash_size, void *voidcharptr)
{
    char *host = (char *)voidcharptr;
    long h = 0;

    while (*host) {
        h = (h >> 28) ^ (h << 4) ^ (*host);
        host++;
    }

    if (h < 0)
    h = -h;

    return h % hash_size;
}

static int
dns_cache_compare_keys(void *voidcharptr1, void *voidcharptr2)
{
    return  strcmp((char *)voidcharptr1, (char *)voidcharptr2);
}

static int 
dns_cache_cleanup(void *voiddata)
{
    host_dns_cache_entry_t *data = (host_dns_cache_entry_t *)voiddata;

    if (!dns_cache || !data)
        return -1;

    if (data->host)
        PERM_FREE(data->host);

    if (data->hostent)
        hostent_free(data->hostent);

    /* It is imperative that we do not FREE the host_dns_cache_entry_t (data)
     * structure here.  cache_delete() will do that for us.
     */

    return 0;
}

static int
dns_cache_debug(pblock *pb, Session *sn, Request *rq)
{
    if (!dns_cache)
        return 0;

    return cache_dump(dns_cache, "Forward DNS Cache", sn->csd);
}

#define MAX_DEBUG_LINE    1024
static int
dns_cache_print(void *voiddata, SYS_NETFD fd)
{
#ifdef DEBUG_CACHES    /* XXXhep this causes ball of string effect */
    host_dns_cache_entry_t *data = (host_dns_cache_entry_t *)voiddata;
    char buf[MAX_DEBUG_LINE];
    int len;

    if (!dns_cache || !data)
        return 0;

    len = util_snprintf(buf, MAX_DEBUG_LINE,  "%s %d\n",
        data->host?data->host:"null", data->hostent->h_addr_list[0]);
    net_write(fd, buf,len);
#endif /* DEBUG_CACHES */

    return 0;
}

NSAPI_PUBLIC int
host_dns_cache_init(pblock *pb, Session *sn, Request *rq)
{
    char *str_hash_size = pblock_findval("hash-size", pb);
    char *str_cache_size = pblock_findval("cache-size", pb);
    char *str_expire_time = pblock_findval("expire", pb);
    char *str_disable = pblock_findval("disable", pb);
    char *str_negative_dns_cache = pblock_findval("negative-dns-cache", pb);
    int hash_size;
    int cache_size;

    /* Check if cache already initialized */
    if (dns_cache)
        return REQ_PROCEED;

    if (str_disable)
        return REQ_PROCEED;

    if (str_hash_size == NULL)
        hash_size = DEFAULT_HASH_SIZE;
    else 
        if ( (hash_size = atoi(str_hash_size)) <= 0) {
            log_error(LOG_WARN, DNS_CACHE_INIT, sn, rq,
                      XP_GetAdminStr(DBT_hostDnsCache_init_hashSize_lt_zero), 
                      DEFAULT_HASH_SIZE);
            hash_size = DEFAULT_HASH_SIZE;
        }

    if (str_cache_size == NULL) 
        cache_size = DEFAULT_CACHE_MAX;
    else {
        if ( (cache_size = atoi(str_cache_size)) < ABSOLUTE_CACHE_MIN) {
            log_error(LOG_WARN, DNS_CACHE_INIT, sn, rq,
                      XP_GetAdminStr(DBT_hostDnsCache_init_caheSize_lt_zero),
                      ABSOLUTE_CACHE_MIN, DEFAULT_CACHE_MAX);
            cache_size = ABSOLUTE_CACHE_MIN;
        } else {
            if (cache_size > ABSOLUTE_CACHE_MAX) {
                log_error(LOG_WARN, DNS_CACHE_INIT, sn, rq,
                          XP_GetAdminStr(DBT_hostDnsCache_init_cacheSizeTooLarge),
                          cache_size, ABSOLUTE_CACHE_MAX);
                cache_size = ABSOLUTE_CACHE_MAX;
            }
        }
    }

    if (str_expire_time == NULL) 
        dns_expire_time = DEFAULT_EXPIRE_TIME;
    else 
        if ( (dns_expire_time = atoi(str_expire_time)) <= 0) {
            log_error(LOG_WARN, DNS_CACHE_INIT, sn, rq,
                      XP_GetAdminStr(DBT_hostDnsCache_init_expireTime_lt_zero),
                      DEFAULT_EXPIRE_TIME);
            dns_expire_time = DEFAULT_EXPIRE_TIME;
        } else {
            if (dns_expire_time > ABSOLUTE_EXPIRE_MAX) {
                log_error(LOG_WARN, DNS_CACHE_INIT, sn, rq,
                          XP_GetAdminStr(DBT_hostDnsCache_init_expireTimeTooLarge),
                          dns_expire_time, ABSOLUTE_EXPIRE_MAX);
                dns_expire_time = ABSOLUTE_EXPIRE_MAX;
            }
        }

    if (str_negative_dns_cache) {
        int ret = util_getboolean(str_negative_dns_cache, -1);
        if ( ret == -1 ) {
            log_error(LOG_WARN, DNS_CACHE_INIT, sn, rq,
                      XP_GetAdminStr(DBT_hostDnsCache_init_invalidNegativeDnsCache_value));
        }
        else
            negative_dns_cache = ret;
    }

    if ( (dns_cache = cache_create(cache_size, hash_size, 
        &dns_cache_functions)) ==NULL){
        log_error(LOG_FAILURE, DNS_CACHE_INIT, sn, rq,
                  XP_GetAdminStr(DBT_hostDnsCache_init_errorCreatingDnsCache));
        return REQ_ABORTED;
    }

    return REQ_PROCEED;
}

NSAPI_PUBLIC void 
host_dns_cache_destroy(cache_t *cache)
{
    /* cache module automatically cleans up the cache itself */
    dns_cache = NULL;
}


NSAPI_PUBLIC host_dns_cache_entry_t *
host_dns_cache_insert(const char *host, PRHostEnt *hostent, unsigned int verified)
{
    host_dns_cache_entry_t *newentry;

    if ( !dns_cache || !hostent)  {
        return NULL;
    }

    if ( (newentry = (host_dns_cache_entry_t *)PERM_MALLOC(sizeof(host_dns_cache_entry_t))) == NULL) {
        log_error(LOG_FAILURE, DNS_CACHE_INSERT, NULL, NULL,
                  XP_GetAdminStr(DBT_hostDnsCache_insert_errorAllocatingEnt));
        goto error;
    }

    if (host) {
        if ( (newentry->host = PERM_STRDUP(host)) == NULL) {
            log_error(LOG_FAILURE, DNS_CACHE_INSERT, NULL, NULL,
                      XP_GetAdminStr(DBT_hostDnsCache_insert_mallocFailure));
            goto error;
        }
    } else
        newentry->host = NULL;

#ifdef CACHE_DEBUG
    newentry->cache.magic = CACHE_ENTRY_MAGIC;
#endif

    /* create and copy the host entry */
    newentry->hostent = hostent_dup(hostent);

    newentry->verified = verified;

    newentry->last_access = time(NULL);

    if ( cache_insert_p(dns_cache, &(newentry->cache), (void *)(newentry->host), 
        (void *)newentry, &dns_cache_entry_functions) < 0) {
        /* Not a bad error; it just means the cache is full */
        goto error;
    }
    /*
     * cache_insert_p() sets the access count of the cached item to 1. bring it
     * down. otherwise this can never be deleted because it will always appear
     * to be "in use".
     */
    host_dns_cache_use_decrement(newentry);
    return newentry;

error:
    if (newentry) {
        if (newentry->host)
            PERM_FREE(newentry->host);
        if (newentry->hostent)
            hostent_free(newentry->hostent);
        PERM_FREE(newentry);
    }
    return NULL;
}

NSAPI_PUBLIC int
host_dns_cache_delete(void *entry)
{
    if ( !dns_cache || !entry ) 
        return -1;

    return cache_delete(dns_cache, (cache_entry_t *)entry, 0);
}


NSAPI_PUBLIC int 
host_dns_cache_use_increment(void *entry)
{
    if ( !dns_cache || !entry ) 
        return -1;

    return cache_use_increment(dns_cache, (cache_entry_t *)entry);
}

NSAPI_PUBLIC int
host_dns_cache_use_decrement(void *entry)
{
    if ( !dns_cache || !entry) 
        return -1;

    return cache_use_decrement(dns_cache, (cache_entry_t *)entry);
}

NSAPI_PUBLIC host_dns_cache_entry_t *
host_dns_cache_lookup_host_name(const char *name)
{
    host_dns_cache_entry_t *ptr;

    if (!dns_cache || !name ) 
        return NULL;

    ptr = (host_dns_cache_entry_t *)cache_do_lookup(dns_cache, (void *)name);

    if (ptr) {
        if (host_dns_cache_valid(ptr) < 0) {
            if (host_dns_cache_delete(ptr) < 0)
                /* couldn't delete right now */
                host_dns_cache_use_decrement(ptr);
            return NULL;
        }

        (void)host_dns_cache_touch(ptr);
    }

    return ptr;
}

NSAPI_PUBLIC int
host_dns_cache_valid(host_dns_cache_entry_t *entry)
{
    time_t now;

    now = time(NULL);

    if (now > (time_t)(entry->last_access + dns_expire_time))
        return -1;

    return cache_valid(dns_cache, (cache_entry_t *)entry);
}

NSAPI_PUBLIC int
host_dns_cache_touch(host_dns_cache_entry_t *entry)
{
    if (!entry)
        return -1;

    return cache_touch(dns_cache, (cache_entry_t *)entry);
}

NSAPI_PUBLIC PRHostEnt *
host_dns_cache_lookup(const char *host, Session *sn, Request *rq)
{
    host_dns_cache_entry_t *cache_entry = NULL;

    cache_entry = host_dns_cache_lookup_host_name(host);
    if (cache_entry) {
        PRHostEnt *p = hostent_dup(cache_entry->hostent, sn->pool);
        /*
         * once we have the cached item duplicated, bring down its
         * access count. 
         */
        host_dns_cache_use_decrement(cache_entry);
        return p;
    }

    return NULL;
}

NSAPI_PUBLIC PRBool host_dns_cache_is_negative_dns_cache_enabled()
{
    return negative_dns_cache;
}

/* ---------------------------- dns_set_hostent ---------------------------- */

NSAPI_PUBLIC int dns_set_hostent(struct hostent *hostent, Session *sn, Request *rq)
{
    request_set_data(rq, _dns_request_slot, hostent);

    return REQ_PROCEED;
}

/* ---------------------------- dns_get_hostent ---------------------------- */

NSAPI_PUBLIC PRHostEnt *dns_get_hostent(Request *rq)
{
    PRHostEnt *hostent = NULL;

    hostent = (PRHostEnt *) request_get_data(rq, _dns_request_slot);

    return hostent;
}
