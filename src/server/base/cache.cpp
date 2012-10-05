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
 * cache.c
 *
 * Abstract cache class.
 *
 * Warning:	procedures needing to lock the cache_crit and a cache_entry lock
 *		must always lock the cache_crit first in order to avoid 
 *		deadlocks.
 *
 * Note:	When inserting a new entry into a full cache where all 
 *		entries are currently in use (access_count >0), new entries 
 *		will be inserted.  Entries at the LRU will be marked as
 *		delete_pending.  Basically this means the cache will grow
 *		indefinitely up to the working set size. XXXMB
 *
 * ------------------
 *
 * Mike Belshe
 * 01-01-96
 */

#ifdef SOLARIS_TNF
#include <tnf/probe.h>
#endif

#include "netsite.h"
#include "base/systems.h"

#include <sys/types.h>
#include <errno.h>
#include "base/ereport.h"
#include "base/util.h"
#include "base/cache.h"
#include "base/session.h"
#include "frame/req.h"
#include "frame/http.h"
#include "base/crit.h"
#include "base/daemon.h"  /* daemon_atrestart */
#include "nsassert.h"
#include "base/dbtbase.h"
#ifdef IRIX
#include "time/nstime.h"
#endif

/* The cache_list is really only here for debugging the multiple caches which
 * may be present in the system.  cache_crit is only used to protect access
 * to the cache_list.
 */
static cache_t *cache_list = NULL;
static CRITICAL cache_crit = NULL;

#define NSCACHESTATUS_OK             0 
#define NSCACHESTATUS_DELETEPENDING  1 

static
cache_entry_t * _cache_entry_lookup(cache_t *cache, void *key, int *rcs);

NSAPI_PUBLIC cache_t *
cache_create(unsigned int cache_max, unsigned int hash_size,
	public_cache_functions_t *fnlist)
{
	cache_t *newentry;
	unsigned int index;

SOLARIS_PROBE(cache_create_start, "cache");

	NS_ASSERT(cache_max > 0);
	NS_ASSERT(hash_size > 0);
	NS_ASSERT(fnlist);
	if ( (newentry = (cache_t *)PERM_MALLOC(sizeof(cache_t))) == NULL) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_insufficientMemoryToCreateHashTa_));
SOLARIS_PROBE(cache_create_end, "cache");
		return NULL;
	}

#ifdef CACHE_DEBUG
	newentry->magic = CACHE_MAGIC;
#endif
	newentry->cache_size	= 0;
	newentry->hash_size	= hash_size;
	newentry->max_size	= cache_max;
    newentry->insert_ok = 0;
    newentry->insert_fail = 0;
    newentry->deletes = 0;
	newentry->cache_hits	= 0;
	newentry->cache_lookups = 0;
	newentry->lock 	= crit_init();
	newentry->virtual_fn	= fnlist;
	if ( (newentry->table = (cache_entry_t **)
		PERM_MALLOC((sizeof(cache_entry_t *)) * (newentry->hash_size)))
		== NULL) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_insufficientMemoryToCreateHashTa_1));
SOLARIS_PROBE(cache_create_end, "cache");
		return NULL;
	}
	for (index=0; index<newentry->hash_size; index++) 
		newentry->table[index] = NULL;
	newentry->mru_head = newentry->lru_head = NULL;

#ifdef IRIX
	newentry->fast_mode = 0;
	newentry->garbage_list_head = NULL;
	newentry->gc_time = 0;
#endif

	if (!cache_crit) 
		cache_crit = crit_init();
	
	crit_enter(cache_crit);
	newentry->next = cache_list;
	cache_list = newentry;
	crit_exit(cache_crit);

SOLARIS_PROBE(cache_create_end, "cache");
	return newentry;
}

NSAPI_PUBLIC void 
cache_destroy(void *cache_ptr)
{
	cache_t *cache = (cache_t *)cache_ptr;
	cache_t *search, *last;
	cache_entry_t *ptr;
SOLARIS_PROBE(cache_destroy_start, "cache");

#ifdef IRIX
        NS_ASSERT(!cache->fast_mode);
#endif

	NS_ASSERT(cache_crit);
	NS_ASSERT(cache_ptr);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
#endif

	crit_enter(cache_crit);
	crit_enter(cache->lock);

	ptr = cache->lru_head;
	while(ptr) {
		/* Caller MUST bump the access_count before calling delete
		 * We can do this since we hold the cache lock.  
		 */
		cache_use_increment(cache, ptr);
		cache_delete(cache, ptr, 0);
		ptr = cache->lru_head;
	}

	PERM_FREE(cache->table);

	cache->max_size = 0;
	cache->hash_size = 0;

	for ( last = NULL, search = cache_list; search; last = search,
		search = search->next)
		if (search == cache)
			break;

	if (search) {
		if (last) 
			last->next = search->next;
		else
			cache_list = search->next;
	}
	else {
		ereport(LOG_WARN, XP_GetAdminStr(DBT_cacheDestroyCacheTablesAppearCor_));
	}
	crit_exit(cache_crit);
	crit_exit(cache->lock);

	crit_terminate(cache->lock);

	PERM_FREE(cache);
SOLARIS_PROBE(cache_destroy_end, "cache");
}

/* _cache_make_mru
 * Put this element at the MRU element.
 * Caller must hold cache lock.
 */
static int
_cache_make_mru(cache_t *cache, cache_entry_t *entry) 
{
SOLARIS_PROBE(cache_make_mru_start, "cache");
	NS_ASSERT(cache);
	NS_ASSERT(entry);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
	NS_ASSERT(entry->magic == CACHE_ENTRY_MAGIC);
#endif

	if (cache->mru_head == NULL)
		cache->mru_head = cache->lru_head = entry;
	else {
		if (cache->mru_head != entry) {
			if (cache->lru_head == entry)
				cache->lru_head = entry->mru;
			/* only check entry->lru if we aren't the lru_head */
			else if (entry->lru) 
				entry->lru->mru = entry->mru;
			if (entry->mru)
				entry->mru->lru = entry->lru;
			entry->lru = cache->mru_head;
			entry->mru = NULL;
			/* cache->mru_head cannot be NULL here */
			cache->mru_head->mru = entry;
			cache->mru_head = entry;
		}
	}

SOLARIS_PROBE(cache_make_mru_end, "cache");
	return 0;
}

/* _cache_remove_mru()
 * Remove an entry from the MRU list. 
 * Caller MUST hold the cache->lock in order to call this routine.
 * Caller MUST be on the MRU list; otherwise this routine will segfault.
 */
static int
_cache_remove_mru(cache_t *cache, cache_entry_t *entry)
{

SOLARIS_PROBE(cache_remove_mru_start, "cache");
	NS_ASSERT(cache);
	NS_ASSERT(cache->mru_head);
	NS_ASSERT(cache->lru_head);
	NS_ASSERT(entry);
	NS_ASSERT((cache->mru_head == cache->lru_head) || 
		(entry->mru || entry->lru));
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
	NS_ASSERT(entry->magic == CACHE_ENTRY_MAGIC);
#endif

	if (cache->mru_head == entry) 
		cache->mru_head = entry->lru;
	else 
		/* if we are not the mru_head, entry->mru cannot be NULL */
		entry->mru->lru = entry->lru;
	if (cache->lru_head == entry) 
		cache->lru_head = entry->mru;
	else 
		/* if we are not the lru_head, entry->lru cannot be NULL */
		entry->lru->mru = entry->mru;

	entry->mru = entry->lru = NULL;

#ifdef CACHE_CHECK_LIST
	{
		cache_entry_t *ptr;
		for (ptr = cache->mru_head; ptr; ptr = ptr->lru)
			NS_ASSERT(ptr != entry);
		for (ptr = cache->lru_head; ptr; ptr = ptr->mru)
			NS_ASSERT(ptr != entry);
	}
#endif /* CACHE_CHECK_LIST */

SOLARIS_PROBE(cache_remove_mru_end, "cache");
	return 0;
}

NSAPI_PUBLIC int 
cache_touch(cache_t *cache, cache_entry_t *entry)
{
SOLARIS_PROBE(cache_touch_start, "cache");
	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
	NS_ASSERT(entry);
	NS_ASSERT(entry->access_count > 0);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
	NS_ASSERT(entry->magic == CACHE_ENTRY_MAGIC);
#endif

	/* Since entries which have the access count > 0 (and the caller
	 * must hold the access count by definition, does this routine
	 * make sense anymore? XXXMB
	 */

SOLARIS_PROBE(cache_touch_end, "cache");
	return 0;
}

cache_entry_t *
cache_create_entry(void)
{
	cache_entry_t *newentry;

SOLARIS_PROBE(cache_create_entry_start, "cache");
	if ( (newentry = (cache_entry_t *)PERM_MALLOC(sizeof(cache_entry_t))) == NULL) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_unableToAllocateHashEntry_));
SOLARIS_PROBE(cache_create_entry_end, "cache");
		return NULL;
	}

#ifdef CACHE_DEBUG
	newentry->magic = CACHE_ENTRY_MAGIC;
#endif

SOLARIS_PROBE(cache_create_entry_end, "cache");
	return newentry;
}

/* cache_free_entry()
 * free a cache entry created by cache_create_entry()
 * which has not been added to the cache. if the entry
 * already been added to the cache, do not use this function
 * (use cache_delete() instead). this function is added here
 * to help free a newly created cache entry which for some
 * reason is not going to be added to the cache.
 */
void
cache_free_entry(cache_entry_t * entry)
{
        NS_ASSERT(entry);
#ifdef CACHE_DEBUG
	NS_ASSERT(entry->magic == CACHE_ENTRY_MAGIC);
#endif
        PERM_FREE(entry);
    
}

NSAPI_PUBLIC int
cache_insert_p(cache_t *cache, cache_entry_t *entry, void *key, void *data,
	cache_entry_functions_t *fn)
{
	cache_entry_t *tmp;
	int bucket;
	cache_entry_t *delete_ptr;
        int rcs = NSCACHESTATUS_OK;

SOLARIS_PROBE(cache_insert_p_start, "cache");
	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
	NS_ASSERT(entry);
	NS_ASSERT((cache->mru_head && cache->lru_head) || 
		(!cache->mru_head && !cache->lru_head));
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
	NS_ASSERT(entry->magic == CACHE_ENTRY_MAGIC);
#endif

	crit_enter(cache->lock);
	/* Reserve a space in the cache if possible; if not; try to delete
	 * the oldest guy...  Note that since we increment to reserve
	 * space; it is possible that we have maxed the cache when there
	 * are no entries yet added; so we need to check the lru_head ptr
	 * to see if the list is empty.  If the list is empty, there is 
	 * nothing we can delete, so just fail the insert request.  This
	 * condition should go away momentarily.
	 */
	if (cache->cache_size >= cache->max_size) {

		if (!cache->lru_head) {
			/* No space in the cache */
            cache->insert_fail++;
			crit_exit(cache->lock);
SOLARIS_PROBE(cache_insert_p_end, "cache");
			return -1;
		}

		/* Caller MUST bump the access_count before calling delete
		 * We can do this since we hold the cache lock.
		 */
		delete_ptr = cache->lru_head;
		cache_use_increment(cache, delete_ptr);
		if ( cache_delete(cache, delete_ptr, 0) < 0 ) {
			cache_use_decrement(cache, delete_ptr);
			NS_ASSERT(delete_ptr->access_count > 0);
            cache->insert_fail++;
			crit_exit(cache->lock);
SOLARIS_PROBE(cache_insert_p_end, "cache");
			return -1;
		}
	}
	cache->cache_size++;
	crit_exit(cache->lock);

	entry->key = key;
	entry->data = data;
	entry->access_count = 1;
	entry->delete_pending = 0;
	entry->fn_list = fn;
	entry->next = NULL;
	entry->lru = NULL;
	entry->mru = NULL;
	bucket = cache->virtual_fn->hash_fn(cache->hash_size, key);

#ifdef IRIX
	entry->next_deleted = NULL;
#endif

	crit_enter(cache->lock);
	/* Don't add duplicate entries in the cache */
	if ( (tmp = _cache_entry_lookup(cache, key, &rcs)) ) {
                /* Try to delete original element */
                if ( cache_delete(cache, tmp, 0) < 0) {
			cache->cache_size--;
			cache_use_decrement(cache, tmp);
			NS_ASSERT(tmp->access_count > 0);
            cache->insert_fail++;
			crit_exit(cache->lock);
SOLARIS_PROBE(cache_insert_p_end, "cache");
			return -1;
		}
	}
        else if (rcs == NSCACHESTATUS_DELETEPENDING) {
            cache->cache_size--;
            cache->insert_fail++;
            crit_exit(cache->lock);
            return -1;
        }

	/* Insert in hash table */
	entry->next = cache->table[bucket];
	cache->table[bucket] = entry;

    cache->insert_ok++;
	crit_exit(cache->lock);

SOLARIS_PROBE(cache_insert_p_end, "cache");
	return 0;
}

NSAPI_PUBLIC cache_entry_t *
cache_insert(cache_t *cache, void *key, void *data, cache_entry_functions_t *fn)
{
	cache_entry_t *newentry;

SOLARIS_PROBE(cache_insert_start, "cache");
	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
#endif

	if ( (newentry = cache_create_entry()) == NULL) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_cacheInsertUnableToCreateCacheEn_));
SOLARIS_PROBE(cache_insert_end, "cache");
		return NULL;
	}

	if ( cache_insert_p(cache, newentry, key, data, fn) < 0) {
		PERM_FREE(newentry);
SOLARIS_PROBE(cache_insert_end, "cache");
		return NULL;
	}


SOLARIS_PROBE(cache_insert_end, "cache");
	return newentry;
}

/*
 * Caller can pass dec_hits as 1 or 0:
 * If dec_hits is non-zero, the cache_hits will be decremented on return.
 * Caller should only specify non-zero dec_hits when the caller has 
 * incremented the cache_hits while the entry handle is obtained (eg. by 
 * doing lookup). Only use non-zero dec_hits when caller found the entry
 * data become invalid and cannot count as a cache hit
 */
NSAPI_PUBLIC int
cache_delete(cache_t *cache, cache_entry_t *entry, int dec_hits)
{
	int bucket;
	cache_entry_t *ptr, *last;

SOLARIS_PROBE(cache_delete_try_start, "cache");
	NS_ASSERT(cache);
	NS_ASSERT(entry);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
	NS_ASSERT(entry->magic == CACHE_ENTRY_MAGIC);
#endif
	NS_ASSERT(cache_crit);

	/* the caller is supposed to bump access count to before calling us.  
	 * If the count is less than 1, something is wrong...
	 */
	NS_ASSERT(entry->access_count >= 1);

	crit_enter(cache->lock);

	entry->delete_pending = 1;
        if (dec_hits) {
            NS_ASSERT(cache->cache_hits > 0);
            cache->cache_hits--;
        }

	/* Check the access_count now that we have the lock.  If
	 * we got context switched just after we checked, it is possible
	 * that someone else is now using the entry.
	 */
	if (entry->access_count > 1) {
		crit_exit(cache->lock);
SOLARIS_PROBE(cache_delete_try_end, "cache");
		return -1;
	}

SOLARIS_PROBE(cache_delete_start, "cache");
	/* Delete from the hash table */
	bucket = cache->virtual_fn->hash_fn(cache->hash_size, entry->key);
	for (last = NULL, ptr = cache->table[bucket]; ptr; 
		last = ptr, ptr= ptr->next)
		if (entry == ptr)
			break;
	NS_ASSERT(ptr);
	if (ptr) {
		if (last)
			last->next = ptr->next;
		else
			cache->table[bucket] = ptr->next;
	}
	cache->cache_size--;

	/* don't need to remove from the MRU/LRU list because
	 * the access count is > 0 in order to call delete 
	 */

	/* Technically, someone else could kill the whole cache
	 * right now, and we would be left with a bum pointer.   XXXMB
	 */
	entry->fn_list->cleanup_fn(entry->data);

    cache->deletes++;
	crit_exit(cache->lock);

	PERM_FREE(entry);

SOLARIS_PROBE(cache_delete_end, "cache");
	return 0;
}


NSAPI_PUBLIC int 
cache_use_increment(cache_t *cache, cache_entry_t *entry)
{
	int ret;
SOLARIS_PROBE(cache_use_increment_start, "cache");
	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
	NS_ASSERT(entry);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
	NS_ASSERT(entry->magic == CACHE_ENTRY_MAGIC);
#endif

	/* 
	 * Why do we do a cache_do_lookup()? Solves the problem where one
	 * thread calls delete() while another called increment().  If
	 * the delete happened first, then we need to lookup the element
	 * before we try to bump the access count.
	 */
	ret = cache_do_lookup(cache, entry->key)?0:-1;
SOLARIS_PROBE(cache_use_increment_end, "cache");
	return ret;
}

NSAPI_PUBLIC int
cache_use_decrement(cache_t *cache, cache_entry_t *entry)
{
SOLARIS_PROBE(cache_use_decrement_start, "cache");
	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
	NS_ASSERT(entry);
	NS_ASSERT(entry->access_count > 0);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
	NS_ASSERT(entry->magic == CACHE_ENTRY_MAGIC);
#endif

	crit_enter(cache->lock);
	/* If we are the last user of this entry and the delete
	 * is pending, cleanup now!
	 */
    int res = 0;
	if ((entry->access_count == 1) && (entry->delete_pending)) {
		if (cache_delete(cache, entry, 0) < 0) {
			/* can't delete right now; should never happen
			 * since we have the cache lock and we know the
			 * access count is 1
			 */
			entry->access_count--;
			(void)_cache_make_mru(cache, entry);
		}
        res = -1;
	} else {
		if (entry->access_count == 1)
			(void)_cache_make_mru(cache, entry);
		entry->access_count--;
	}
	crit_exit(cache->lock);

SOLARIS_PROBE(cache_use_decrement_end, "cache");
	return res;
}

static cache_entry_t *
_cache_entry_lookup(cache_t *cache, void *key, int *rcs)
{
	int bucket;
	cache_entry_t *ptr;
        *rcs = NSCACHESTATUS_OK;

#if 0
TNF_PROBE_0(cache_entry_lookup_start, "", "cache");
#endif
	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
#endif

	/* move outside the crit section */
	bucket = cache->virtual_fn->hash_fn(cache->hash_size, key);
#if 0
TNF_PROBE_0(cache_hash_end, "", "cache_hash_end");
#endif

	crit_enter(cache->lock);
	cache->cache_lookups++;

#if 0
TNF_PROBE_0(cache_crit_enter_end, "", "cache_crit_enter_end");
#endif

	for (ptr = cache->table[bucket]; ptr; ptr = ptr->next) {
          if (!ptr->fn_list->key_cmp_fn(key, ptr->key))
	    break;
	}
#if 0
TNF_PROBE_0(cache_search_end, "", "cache_hash_end");
#endif

	if (ptr) {
	   if (!ptr->delete_pending) {
		cache->cache_hits++;
		if (ptr->access_count == 0) 
			(void)_cache_remove_mru(cache, ptr);
		ptr->access_count++;
           }
           else {
               NS_ASSERT(ptr->access_count > 0);
               ptr = NULL;
               *rcs = NSCACHESTATUS_DELETEPENDING;
           }
        }
	crit_exit(cache->lock);

#if 0
TNF_PROBE_0(cache_entry_lookup_end, "", "cache_entry_lookup_end");
#endif

SOLARIS_PROBE(cache_entry_lookup_end, "cache");
	return ptr;
}

NSAPI_PUBLIC void *
cache_do_lookup(cache_t *cache, void *key)
{
	cache_entry_t *ptr;
        int rcs = NSCACHESTATUS_OK;

SOLARIS_PROBE(cache_do_lookup_start, "cache");
	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
#ifdef CACHE_DEBUG
	NS_ASSERT(cache->magic == CACHE_MAGIC);
#endif

	ptr = _cache_entry_lookup(cache, key, &rcs);

SOLARIS_PROBE(cache_do_lookup_end, "cache");
	if (ptr) 
		return ptr->data;
	else
		return NULL;
    
}

NSAPI_PUBLIC int 
cache_valid(cache_t *cache, cache_entry_t *entry)
{
	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
	NS_ASSERT(entry);
	NS_ASSERT(entry->access_count > 0);

	return entry->delete_pending?-1:0;
}

NSAPI_PUBLIC void
cache_lock(cache_t *cache)
{
SOLARIS_PROBE(cache_lock_start, "cache");
	crit_enter(cache->lock);
SOLARIS_PROBE(cache_lock_end, "cache");
}

NSAPI_PUBLIC void
cache_unlock(cache_t *cache)
{
SOLARIS_PROBE(cache_unlock_start, "cache");
	crit_exit(cache->lock);
SOLARIS_PROBE(cache_unlock_end, "cache");
}

NSAPI_PUBLIC unsigned int
cache_get_use_count(cache_t *cache, cache_entry_t *entry)
{
	unsigned int result;

#ifdef IRIX
	result = entry->access_count;
#else
	crit_enter(cache->lock);
	result = entry->access_count;
	crit_exit(cache->lock);
#endif

	return result;
}


#define MAX_DEBUG_LINE	1024
#ifdef DEBUG_CACHES
/* cache_service_debug()
 * To force the creation of good debugging tools, all known caches must
 * have a debugging routine.  The cache_service_debug() routine is a common
 * routine that can be inserted via NSAPI to view the status of all
 * known caches.  This is a hidden entry point which can be enabled when
 * debugging is needed.
 *
 */
NSAPI_PUBLIC int 
cache_service_debug(pblock *pb, Session *sn, Request *rq)
{
	cache_t *ptr;
	char buf[MAX_DEBUG_LINE];
	int len;

	NS_ASSERT(cache_crit);

	param_free(pblock_removekey(pb_key_content_type, rq->srvhdrs));
	pblock_nvinsert("content-type", "text/html", rq->srvhdrs);

	len = util_sprintf(buf, XP_GetClientStr(DBT_http10200OkNcontentTypeTextHtmlN_));
	net_write(sn->csd, buf, len);

	len = util_sprintf(buf, XP_GetClientStr(DBT_H2NetscapeCacheStatusReportH2N_));
	net_write(sn->csd, buf, len);

	crit_enter(cache_crit);
	if (cache_list) {
		len = util_sprintf(buf, "<HR>");
		net_write(sn->csd, buf, len);
		for (ptr = cache_list; ptr; ptr = ptr->next) {
			if (ptr->virtual_fn->debug_fn)
				if ( ptr->virtual_fn->debug_fn(pb, sn, rq) == REQ_ABORTED ) 
					return REQ_ABORTED;
		}
	} else {
		len = util_sprintf(buf, XP_GetClientStr(DBT_noCachesOnSystemP_));
		net_write(sn->csd, buf, len);
	}
	crit_exit(cache_crit);

	return REQ_PROCEED;
}

#endif /* 0 */

/* cache_dump()
 * A generic routine to dump a cache to a socket
 */
NSAPI_PUBLIC int
cache_dump(cache_t *cache, char *cache_name, SYS_NETFD fd)
{
#ifdef DEBUG_CACHES /* XXXrobm this causes ball of string effect */
	char buf[MAX_DEBUG_LINE];
	cache_entry_t *ptr;
	int index, len;

	NS_ASSERT(cache_crit);
	NS_ASSERT(cache);
	NS_ASSERT(cache_name);

	if (!cache_crit)
		return -1;

	crit_enter(cache->lock);
	len = util_sprintf(buf, XP_GetClientStr(DBT_H2SCacheH2N_), cache_name);
	net_write(fd, buf, len);
	len = util_sprintf(buf, XP_GetClientStr(DBT_cacheHitRatioDDFPNPN_),
		cache->cache_hits, cache->cache_lookups, 
		(cache->cache_lookups > 0)?
		((cache->cache_hits)/(cache->cache_lookups)):0.0);
	net_write(fd, buf, len);
	len = util_sprintf(buf, XP_GetClientStr(DBT_cacheSizeDDPNPN_),
		cache->cache_size, cache->max_size);
	net_write(fd, buf, len);
	len = util_sprintf(buf, XP_GetClientStr(DBT_hashTableSizeDPNPN_),
		cache->hash_size);
	net_write(fd, buf, len);
	len = util_sprintf(buf, XP_GetClientStr(DBT_mruDPNlruDPN_),
                cache->mru_head, cache->lru_head);
        net_write(fd, buf, len);

	/* Create an HTML table */
	len = util_sprintf(buf, XP_GetClientStr(DBT_UlTableBorder4ThBucketThThAddres_));
	net_write(fd, buf, len);

	for (index = 0; index < cache->hash_size; index++) {
		if (cache->table[index]) {
			for (ptr = cache->table[index]; ptr; ptr = ptr->next) {

				len = util_snprintf(buf, MAX_DEBUG_LINE, "\
<tr align=right> \
<td>%d</td> \
<td>%d</td> \
<td>%d</td> \
<td>%d</td> \
<td>%d</td> \
<td>%d</td> \
<td>%d</td> \
<td>%d</td> <td>",
					index, ptr, ptr->key,ptr->access_count, 
					ptr->delete_pending, ptr->next, 
					ptr->lru, ptr->mru);

				net_write(fd, buf, len);

				(void)ptr->fn_list->print_fn(ptr->data, fd);

				len = util_sprintf(buf, "</td></tr>\n");
				net_write(fd, buf, len);

			}
		}
	}

	len = util_sprintf(buf, "</TABLE></UL>\n");
	net_write(fd, buf, len);
	crit_exit(cache->lock);
	
#endif
	return REQ_PROCEED;
}


#ifdef IRIX

#define SAFE_INTERVAL 3600    /* deleted entry linger time */
#define GC_INTERVAL   120     /* garbage collection period */

NSAPI_PUBLIC void
cache_collect_garbage(cache_t *cache)
{
  unsigned int now;
  cache_entry_t	*ptr, *last;

  NS_ASSERT(cache_crit);
  NS_ASSERT(cache);
#ifdef CACHE_DEBUG
  NS_ASSERT(cache->magic == CACHE_MAGIC);
#endif

  if(!cache->fast_mode)
    return;

  now = ft_time();
  if(now - cache->gc_time < GC_INTERVAL)
    return;

  crit_enter(cache->lock);

  last = NULL;
  ptr = cache->garbage_list_head;
  while(ptr) {
    NS_ASSERT(ptr->delete_pending);
    if((ptr->delete_time + SAFE_INTERVAL < now) && (ptr->access_count == 0)) {
      if(last)
	last->next_deleted = ptr->next_deleted;
      else
	cache->garbage_list_head = ptr->next_deleted;

      ptr->fn_list->cleanup_fn(ptr->data);
      PERM_FREE(ptr);

      ptr = (last) ? (last->next_deleted) : (cache->garbage_list_head);

    } else {
      last = ptr;
      ptr = ptr->next_deleted;
    }
  }

  crit_exit(cache->lock);
  cache->gc_time = now;
}

#endif  /* IRIX */

