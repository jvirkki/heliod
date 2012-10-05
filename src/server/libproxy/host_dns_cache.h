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
 * host_dns_cache.h
 *
 * Proxy DNS Cache - Cache of DNS host-ip entries
 * Note that this is a host->ip DNS cache
 * unlike ip->host DNS cache in dns_cache.h
 *
 */

#ifndef _PROXY_DNS_CACHE_H_
#define _PROXY_DNS_CACHE_H_

#include <errno.h>
#include "base/cache.h"
#include "base/buffer.h"
#include "base/session.h"
#include "frame/req.h"
#include "base/crit.h"

/* host_dns_cache_entry_t
 * An object in the file cache.  
 */
typedef struct host_dns_cache_entry_t {
    cache_entry_t   cache;       /* we are a subclass of cache */
    char            *host;
    PRHostEnt       *hostent;
    unsigned int    verified;    /* 0 if not reverse dns done, 1 otherwise */
    time_t          last_access; /* time this entry was cached */
} host_dns_cache_entry_t;

NSPR_BEGIN_EXTERN_C

/* host_dns_cache_init()
 * Initializes the DNS cache.  Must be initialized before use.
 *
 * This is just another NSAPI initializer function
 *
 * Returns REQ_PROCEED on success, REQ_ABORTED on failure.
 */
NSAPI_PUBLIC int host_dns_cache_init(pblock *pb, Session *sn, Request *rq);

/* host_dns_cache_destroy()
 * Cleans up and deletes all entries in the cache.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC void host_dns_cache_destroy(cache_t *);

/* host_dns_cache_insert()
 * Inserts a new cache entry.  access_count is initialized to 1, so the
 * caller must call host_dns_cache_use_decrement() for this entry when 
 * done using the entry.
 * Returns non-NULL on success, NULL on failure.
 */
NSAPI_PUBLIC host_dns_cache_entry_t *host_dns_cache_insert(const char *host, PRHostEnt *hostent, unsigned int verified);

/* host_dns_cache_delete()
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
NSAPI_PUBLIC int host_dns_cache_delete(void *entry);

/* host_dns_cache_use_increment()
 * Increments the use count for this entry.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int host_dns_cache_use_increment(void *entry);

/* host_dns_cache_use_decrement
 * Decrements the use count for this entry.
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int host_dns_cache_use_decrement(void *entry);


/* host_dns_cache_lookup_host_name()
 * Search for an host name in the cache. If the entry is found, the use count
 * is automatically incremented and it is the caller's responsibility to
 * decrement it.
 *
 * Return non-NULL if found, NULL if not found.
 */
NSAPI_PUBLIC host_dns_cache_entry_t *host_dns_cache_lookup_host_name(const char *name);

/* host_dns_cache_valid()
 * Chacks to see if an entry in the cache is valid.  To do so, it checks
 * That the entry has not been in the cache for more than <expire> seconds.  
 * The caller MUST have already incremented the use count for this entry 
 * before calling this routine.
 * 
 * Returns 0 if an entry is still "valid",  less-than-zero on error.
 */
NSAPI_PUBLIC int host_dns_cache_valid(host_dns_cache_entry_t *entry);

/* host_dns_cache_touch()
 * Moves a cache entry to the top of the MRU chain.  The caller MUST have
 * already incremented the use count for this entry before calling this
 * routine.
 *
 * Returns 0 on success, less-than-zero on failure.
 */
NSAPI_PUBLIC int host_dns_cache_touch(host_dns_cache_entry_t *entry);

NSAPI_PUBLIC PRHostEnt *host_dns_cache_lookup(const char *host, Session *sn, Request *rq);

NSAPI_PUBLIC PRBool host_dns_cache_is_negative_dns_cache_enabled();

/*
 * INTdns_set_hostent sets the hostent to be used
 */

NSAPI_PUBLIC int INTdns_set_hostent(struct hostent *he, Session *sn, Request *rq);

/*
 * INTdns_get_hostent returns the host entry
 */

NSAPI_PUBLIC PRHostEnt *INTdns_get_hostent(Request *rq);

/*
 * host_dns_cache_hostent_init initializes host entry slot
 */
NSAPI_PUBLIC PRStatus host_dns_cache_hostent_init(void);


NSPR_END_EXTERN_C

#define dns_set_hostent INTdns_set_hostent
#define dns_get_hostent INTdns_get_hostent

#endif /* _PROXY_DNS_CACHE_H_ */
