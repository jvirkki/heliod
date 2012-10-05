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
 * dns_cache.c
 *
 * Cache of DNS entries
 *
 * Mike Belshe
 * 02-03-96
 */

#include "netsite.h"
#include "time/nstime.h"
#include "base/cache.h"
#include "base/dns_cache.h"
#include "base/buffer.h"
#include "base/ereport.h"
#include "base/util.h"
#include "base/daemon.h"
#include "base/dbtbase.h"

/* ----- Forwards ----------------------------------------------------- */
static unsigned int dns_cache_hash_ip(unsigned int, void *);
static int dns_cache_compare_keys(void *, void *);
static int dns_cache_cleanup(void *);
static int dns_cache_debug(pblock *pb, Session *sn, Request *rq);
static int dns_cache_print(void *data, SYS_NETFD fd);

/* ----- Static globals ----------------------------------------------- */
static cache_t			*dns_cache = NULL;
static unsigned long		dns_expire_time = 0;

static	public_cache_functions_t dns_cache_functions = {
		dns_cache_hash_ip,
		dns_cache_debug
	};

static	cache_entry_functions_t dns_cache_entry_functions = {
		dns_cache_compare_keys,
		dns_cache_cleanup,
		dns_cache_print
	};

/* ----- Functions ----------------------------------------------------- */

static unsigned int 
dns_cache_hash_ip(unsigned int hash_size, void *voidint)
{
	register unsigned int *intptr = (unsigned int *)voidint;

	return (*intptr) % hash_size;
}

static int
dns_cache_compare_keys(void *voidint1, void *voidint2)
{
	return  !( *(unsigned int *)voidint1 == *(unsigned int *)voidint2);
}

static int 
dns_cache_cleanup(void *voiddata)
{
	dns_cache_entry_t *data = (dns_cache_entry_t *)voiddata;

	if (!dns_cache || !data)
		return -1;

	if (data->host)
		PERM_FREE(data->host);

	/* It is imperative that we do not FREE the dns_cache_entry_t (data)
     * structure here.  cache_delete() will do that for us.
     */

	return 0;
}

static int
dns_cache_debug(pblock *pb, Session *sn, Request *rq)
{
	if (!dns_cache)
		return 0;

	return cache_dump(dns_cache, "DNS Cache", sn->csd);
}

#define MAX_DEBUG_LINE	1024
static int
dns_cache_print(void *voiddata, SYS_NETFD fd)
{
#ifdef DEBUG_CACHES	/* XXXhep this causes ball of string effect */
	dns_cache_entry_t *data = (dns_cache_entry_t *)voiddata;
	char buf[MAX_DEBUG_LINE];
	int len;

	if (!dns_cache || !data)
		return 0;

	len = util_snprintf(buf, MAX_DEBUG_LINE,  "%s %d\n",
		data->host?data->host:"null", data->ip);
	net_write(fd, buf,len);
#endif /* DEBUG_CACHES */

	return 0;
}

NSAPI_PUBLIC int
dns_cache_init(int dns_cache_size, int timeout)
{
	/* Check if cache already initialized */
	if (dns_cache)
		return -1;

	dns_expire_time = timeout;

	if ( (dns_cache = cache_create(dns_cache_size, 2*dns_cache_size, 
		&dns_cache_functions)) ==NULL){
		return -1;
	}

	return 0;
}

NSAPI_PUBLIC void 
dns_cache_destroy(cache_t *cache)
{
	/* cache module automatically cleans up the cache itself */
	dns_cache = NULL;
}


NSAPI_PUBLIC dns_cache_entry_t *
dns_cache_insert(char *host, unsigned int ip, unsigned int verified)
{
	dns_cache_entry_t *newentry;

	if ( !dns_cache || !ip)  {
		return NULL;
    }

	if ( (newentry = (dns_cache_entry_t *)PERM_MALLOC(sizeof(dns_cache_entry_t))) == NULL) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_dnsCacheInsertErrorAllocatingEnt_));
		goto error;
	}

	if (host) {
		if ( (newentry->host = PERM_STRDUP(host)) == NULL) {
			ereport(LOG_FAILURE, XP_GetAdminStr(DBT_dnsCacheInsertMallocFailure_));
			goto error;
		}
	} else
		newentry->host = NULL;

#ifdef CACHE_DEBUG
	newentry->cache.magic = CACHE_ENTRY_MAGIC;
#endif

	newentry->ip = ip;
	newentry->verified = verified;

	newentry->last_access = ft_time();

	if ( cache_insert_p(dns_cache, &(newentry->cache), (void *)&(newentry->ip), 
		(void *)newentry, &dns_cache_entry_functions) < 0) {
		/* Not a bad error; it just means the cache is full */
		goto error;
	}

	return newentry;

error:
	if (newentry) {
		if (newentry->host)
			PERM_FREE(newentry->host);
		PERM_FREE(newentry);
	}
	return NULL;
}

NSAPI_PUBLIC int
dns_cache_delete(void *entry)
{
	if ( !dns_cache || !entry ) 
		return -1;

	return cache_delete(dns_cache, (cache_entry_t *)entry, 0);
}


NSAPI_PUBLIC int 
dns_cache_use_increment(void *entry)
{
	if ( !dns_cache || !entry ) 
		return -1;

	return cache_use_increment(dns_cache, (cache_entry_t *)entry);
}

NSAPI_PUBLIC int
dns_cache_use_decrement(void *entry)
{
	if ( !dns_cache || !entry) 
		return -1;

	return cache_use_decrement(dns_cache, (cache_entry_t *)entry);
}

NSAPI_PUBLIC dns_cache_entry_t *
dns_cache_lookup_ip(unsigned int ip)
{
	dns_cache_entry_t *ptr;

	if (!dns_cache || !ip ) 
		return NULL;

	ptr = (dns_cache_entry_t *)cache_do_lookup(dns_cache, (void *)&ip);

	if (ptr) {
		if (dns_cache_valid(ptr) < 0) {
			if (dns_cache_delete(ptr) < 0)
				/* couldn't delete right now */
				dns_cache_use_decrement(ptr);
			return NULL;
		}

		(void)dns_cache_touch(ptr);
	}

	return ptr;
}

NSAPI_PUBLIC int
dns_cache_valid(dns_cache_entry_t *entry)
{
	time_t now;

	now = ft_time();

	if (now > (time_t)(entry->last_access + dns_expire_time))
		return -1;

	return cache_valid(dns_cache, (cache_entry_t *)entry);
}

NSAPI_PUBLIC int
dns_cache_touch(dns_cache_entry_t *entry)
{
	if (!entry)
		return -1;

	return cache_touch(dns_cache, (cache_entry_t *)entry);
}

#include "httpdaemon/internalstats.h"

PRBool
GetDNSCacheInfo(DNSCacheInfo *info)
{
    if (!info)
        return PR_FALSE;

    memset(info, 0, sizeof(DNSCacheInfo));

    info->enabled = (dns_cache == NULL)?PR_FALSE:PR_TRUE;

    if (info->enabled) {
        info->maxCacheEntries = dns_cache->max_size;
        info->numCacheEntries = dns_cache->cache_size;

        info->numCacheHits = dns_cache->cache_hits;
        info->numCacheMisses = dns_cache->cache_lookups - dns_cache->cache_hits;

        info->numCacheInsertsOk = dns_cache->insert_ok;
        info->numCacheInsertsFail = dns_cache->insert_fail;
        info->numCacheDeletes = dns_cache->deletes;
    }

    return PR_TRUE;
}
