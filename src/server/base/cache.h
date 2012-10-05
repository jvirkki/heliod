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
 * cache.h
 *
 * Abstract class for caching objects.
 *
 * Mike Belshe
 * 01-08-96
 *
 */

#ifndef _CACHE_H_
#define _CACHE_H_


#include <errno.h>
#include "base/crit.h"
#include "base/buffer.h"
#include "base/session.h"
#include "frame/req.h"

NSPR_BEGIN_EXTERN_C

#ifdef DEBUG
#ifdef SOLARIS_TNF
#include "tnf/probe.h"
#include "sys/tnf_probe.h"
#endif
#endif

#ifdef _DEBUG
#define CACHE_DEBUG
#endif


#define CACHE_MAGIC		0x11111111
#define CACHE_ENTRY_MAGIC	0xffffffff

#ifdef SOLARIS_TNF
#define SOLARIS_PROBE(x, y) TNF_PROBE_0(x, y, "netscape")
#else
#define SOLARIS_PROBE(x, y)
#endif

/* cache_entry_functions_t 
 * These are the "virtual" functions for the cache element.
 */
typedef int (*key_compare_function)(void *, void *);
typedef int (*cleanup_function)(void *);
typedef int (*print_function)(void *data, SYS_NETFD fd);

typedef struct cache_entry_functions_t {
	key_compare_function	key_cmp_fn;
	cleanup_function	cleanup_fn;
	print_function		print_fn;
} cache_entry_functions_t;

/* cache_entry_t
 * An object in the cache.  
 */
typedef struct cache_entry_t {
#ifdef CACHE_DEBUG
	unsigned long magic;
#endif
	void	*key;			/* key for doing lookups */
	void	*data;			/* data in the cache */
	int	access_count;		/* use count for this entry */
	int	delete_pending;		/* 0 normally, 1 if a this 
					 * request is pending delete.
					 * requests pending delete are
					 * effectively hidden from 
					 * lookups.
					 */
	cache_entry_functions_t	*fn_list;
	struct cache_entry_t	*next;	/* internal linked list */
	struct cache_entry_t	*lru;	/* lru and mru pointers */
	struct cache_entry_t	*mru;

#ifdef IRIX
        unsigned int            delete_time;
        struct cache_entry_t    *next_deleted;
#endif

} cache_entry_t;

typedef unsigned int (*hash_function)(unsigned int, void *);
typedef int (*debug_function)(pblock *pb, Session *sn, Request *rq);

typedef struct public_cache_functions_t {
	hash_function		hash_fn;
	debug_function		debug_fn;
} public_cache_functions_t;

/* cache_t
 * An instance of a cache.  
 *
 * About locking:  Currently each cache gets one lock.  The lock protects
 * all reads/writes to the cache_t structure, and also protects the
 * access_count field of every instance in the cache_entry_t.  It does not
 * protect other fields in the cache_entry_t as they are READ ONLY after
 * they are created.
 */
typedef struct cache_t {
#ifdef CACHE_DEBUG
	unsigned long magic;
#endif
	/* PUBLIC */
	unsigned int 		cache_size;	/* current size of cache */
	unsigned int		hash_size;	/* size of hash table */
	unsigned int		max_size;	/* max size of cache */

	int			cache_hits;	/* num cache hits */
	int			cache_lookups;	/* num cache lookups */

	/* VIRTUAL FUNCTIONS */
	public_cache_functions_t *virtual_fn;

	/* PRIVATE */
	CRITICAL		lock;		/* Must be held to make any
						 * changes to this cache_t
						 * structure.
						 */
	cache_entry_t		**table;	/* hash table for this cache */
	cache_entry_t		*lru_head;	/* least-recently-used list */
	cache_entry_t		*mru_head;	/* most-recently-used list */
	struct cache_t		*next;		/* next cache - for debugging */

    unsigned int         insert_ok;   /* num successful inserts */
    unsigned int         insert_fail; /* num failed inserts */
    unsigned int         deletes;     /* num deletes */

#ifdef IRIX
    int                  fast_mode;
    cache_entry_t        *garbage_list_head;
    unsigned int         gc_time;
#endif

} cache_t;


/* cache_create()
 * Creates/initializes the cache.  Must be initialized before use.
 *
 * This is just another NSAPI initializer function
 *
 * Returns REQ_PROCEED on success, REQ_ABORTED on failure.
 */
NSAPI_PUBLIC 
cache_t *cache_create(unsigned int cache_max, unsigned int hash_size, 
	public_cache_functions_t *fnlist);

/* cache_destroy()
 * Cleans up and deletes all entries in the cache.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC void cache_destroy(void *cache);

/* cache_create_entry()
 * Creates a new cache entry; but does not add it to the cache.
 * Can use the result of this routine to pass into cache_insert_p();
 */
NSAPI_PUBLIC cache_entry_t *cache_create_entry(void);

/* cache_free_entry()
 * free a cache entry created by cache_create_entry() 
 * which has not been added to the cache. if the entry
 * already been added to the cache, do not use this function
 * (use cache_delete() instead). this function is added here
 * to help free a newly created cache entry which for some 
 * reason is not going to be added to the cache.
 */ 
NSAPI_PUBLIC void cache_free_entry(cache_entry_t * entry);


/* cache_insert()
 * Inserts a new cache entry.  access_count is initialized to 1, so the
 * caller must call cache_use_decrement() for this entry when 
 * done using the entry.
 * Returns non-NULL on success, NULL on failure.
 */
NSAPI_PUBLIC
cache_entry_t *cache_insert(cache_t *cache, void *key, void *data,
	cache_entry_functions_t *fn);

/* cache_insert_p()
 * Inserts a new cache entry.  The cache_entry has been preallocated by
 * the caller.  This is done to allow better subclassing.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC
int cache_insert_p(cache_t *cache, cache_entry_t *ent, void *key, void *data,
	cache_entry_functions_t *fn);

/* cache_delete()
 * Attempts to manually delete an entry from the cache.  Normally, the 
 * cache is capable of maintaining itself.  This routine should only be
 * called if the data in the cache has somehow become invalid.
 *
 * Caller must increment the use count for this entry before attempting to
 * delete it.  If the access_count is > 1, then this routine returns -1.
 * <enhancement- mark the entry as delete-pending and allow the caller to 
 * move on>
 *
 * Caller can pass dec_hits as 1 or 0:
 * If dec_hits is non-zero, the cache_hits will be decremented on return.
 * Caller should only specify non-zero dec_hits when the caller has 
 * incremented the cache_hits while the entry handle is obtained (eg. by
 * doing lookup)
 *
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int cache_delete(cache_t *cache, cache_entry_t *entry, int dec_hits);

/* cache_use_increment()
 * Increments the use count for this entry.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int cache_use_increment(cache_t *cache, cache_entry_t *entry);

/* cache_use_decrement()
 * Decrements the use count for this entry.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int cache_use_decrement(cache_t *cache, cache_entry_t *entry);

/* cache_do_lookup()
 * Looks up "key" within the cache. If found, increments the access
 * count and returns the element.
 */
NSAPI_PUBLIC void *cache_do_lookup(cache_t *cache, void *key);

/* cache_touch()
 * "Touches" an entry and moves it to the head of the MRU chain.
 * Caller must have previously incremented the access count.
 *
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int cache_touch(cache_t *cache, cache_entry_t *entry);

/* cache_valid
 * Checks if a cache entry is valid (has someone else set the delete_pending
 * bit).  Caller must have previously incremented the access count.
 *
 * Returns 0 if valid or less-than-zero if it is invalid.
 */
NSAPI_PUBLIC int cache_valid(cache_t *cache, cache_entry_t *entry);

/* cache_lock
 * Locks the entire cache.  May be called recursively.  No changes can
 * be made to the cache while it is locked.
 */
NSAPI_PUBLIC void cache_lock(cache_t *cache);

/* cache_unlock
 * Unlocks the cache.
 */
NSAPI_PUBLIC void cache_unlock(cache_t *cache);

/* cache_get_use_count
 * Return the use count for an entry
 */
NSAPI_PUBLIC unsigned int cache_get_use_count(cache_t *cache, cache_entry_t *entry);

/* cache_dump
 * Dumps an HTTP-format document containing info about the cache
 * to the file.
 */
NSAPI_PUBLIC int cache_dump(cache_t *cache, char *cache_name, SYS_NETFD fd);

#ifdef IRIX
/* cache_collect_garbage
 * Deletes entries marked for deletion in fast mode.
 */
NSAPI_PUBLIC void cache_collect_garbage(cache_t *cache);
#endif

NSPR_END_EXTERN_C

#endif /* _CACHE_H_ */
