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
 * dns_cache.h
 *
 * Cache of DNS entries
 *
 * Mike Belshe
 * 01-08-96
 */

#ifndef _DNS_CACHE_H_
#define _DNS_CACHE_H_

#include <errno.h>
#include "base/cache.h"
#include "base/buffer.h"
#include "base/session.h"
#include "frame/req.h"
#include "base/crit.h"

/* dns_cache_entry_t
 * An object in the file cache.  
 */
typedef struct dns_cache_entry_t {
	cache_entry_t	cache;		/* we are a subclass of cache */
	char		*host;
	unsigned int	ip;
	unsigned int	verified;	/* 0 if not reverse dns done, 1 otherwise */
	time_t		last_access;	/* time this entry was cached */
} dns_cache_entry_t;

NSPR_BEGIN_EXTERN_C

/* dns_cache_init()
 * Initializes the DNS cache.  Must be initialized before use.
 *
 * Returns 0 on success, -1 on failure.
 */
NSAPI_PUBLIC int dns_cache_init(int dns_cache_size, int timeout);

/* dns_cache_destroy()
 * Cleans up and deletes all entries in the cache.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC void dns_cache_destroy(cache_t *);

/* dns_cache_insert()
 * Inserts a new cache entry.  access_count is initialized to 1, so the
 * caller must call dns_cache_use_decrement() for this entry when 
 * done using the entry.
 * Returns non-NULL on success, NULL on failure.
 */
NSAPI_PUBLIC dns_cache_entry_t *dns_cache_insert(char *host, unsigned int ip,
		unsigned int verified);

/* dns_cache_delete()
 * Attempts to manually delete an entry from the cache.  Normally, the 
 * cache is capable of maintaining itself.  This routine should only be
 * called if the data in the cache has somehow become invalid.
 *
 * Caller must increment the use count for this entry before attempting to
 * delete it.  If the access_count is > 1, then this routine returns -1.
 * <enhancement- mark the entry as delete-pending and allow the caller to 
 * move on>
 *
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int dns_cache_delete(void *entry);

/* dns_cache_use_increment()
 * Increments the use count for this entry.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int dns_cache_use_increment(void *entry);

/* dns_cache_use_decrement
 * Decrements the use count for this entry.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int dns_cache_use_decrement(void *entry);


/* dns_cache_lookup_ip()
 * Search for an IP in the cache.  If the entry is found, the use count
 * is automatically incremented and it is the caller's responsibility to
 * decrement it.
 *
 * Return non-NULL if found, NULL if not found.
 */
NSAPI_PUBLIC dns_cache_entry_t *dns_cache_lookup_ip(unsigned int ip);

/* dns_cache_valid()
 * Chacks to see if an entry in the cache is valid.  To do so, it checks
 * That the entry has not been in the cache for more than <expire> seconds.  
 * The caller MUST have already incremented the use count for this entry 
 * before calling this routine.
 * 
 * Returns 0 if an entry is still "valid",  less-than-zero on error.
 */
NSAPI_PUBLIC int dns_cache_valid(dns_cache_entry_t *entry);

/* dns_cache_touch()
 * Moves a cache entry to the top of the MRU chain.  The caller MUST have
 * already incremented the use count for this entry before calling this
 * routine.
 *
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int dns_cache_touch(dns_cache_entry_t *entry);

NSPR_END_EXTERN_C

#endif /* _DNS_CACHE_H_ */
