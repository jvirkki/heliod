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
 * Generic pool handling routines.
 *
 * Hopefully these reduce the number of malloc/free calls.
 *
 *
 * Thread warning:
 *	This implementation is thread safe.  However, simultaneous 
 *	mallocs/frees to the same "pool" are not safe.  If you wish to
 *	use this module across multiple threads, you should define
 *	POOL_LOCKING which will make the malloc pools safe. 
 *
 * Mike Belshe
 * 11-20-95
 *
 */

#ifdef WIN32
#include <windows.h>
#else
#include <time.h>
#include <string.h>
#endif
#include "nspr.h"
#include "nspool.h"
#include "nsmalloc.h"

#undef POOL_LOCKING

#define BLOCK_SIZE		(32 * 1024)
#define MAX_FREELIST_SIZE	(BLOCK_SIZE * 128)

/* WORD SIZE 8 sets us up for 8 byte alignment. */
#define WORD_SIZE	8   
#undef	ALIGN
#define ALIGN(x)	( (x + WORD_SIZE-1) & (~(WORD_SIZE-1)) )

/* block_t
 * When the user allocates space, a BLOCK_SIZE (or larger) block is created in 
 * the pool.  This block is used until all the space is eaten within it.
 * When all the space is gone, a new block is created.
 *
 */
typedef struct block_t {
	char		*data;		/* the real alloc'd space */
	char		*start;		/* first free byte in block */
	char		*end;		/* ptr to end of block */
	struct block_t	*next;		/* ptr to next block */
} block_t;

/* pool_t
 * A pool is a collection of blocks.  The blocks consist of multiple 
 * allocations of memory, but a single allocation cannot be freed by
 * itself.  Once the memory is allocated it is allocated until the
 * entire pool is freed.
 */
typedef struct pool_t {
#ifdef POOL_LOCKING
	PRLock *	lock;		/* lock for modifying the pool */
#endif
	block_t		*curr_block;	/* current block being used */
	block_t		*used_blocks;	/* blocks that are all used up */
	long		size;		/* size of memory in pool */
	struct pool_t	*next;		/* known_pools list */
} pool_t;

/* known_pools
 * Primarily for debugging, keep a list of all active malloc pools.
 */
static pool_t *known_pools = NULL;
static PRLock * known_pools_lock = NULL;
static unsigned long pool_blocks_created = 0;
static unsigned long pool_blocks_freed = 0;

/* freelist
 * Internally we maintain a list of free blocks which we try to pull from
 * whenever possible.  This list will never have more than MAX_FREELIST_SIZE
 * bytes within it.
 */
static PRLock * freelist_lock = NULL;
static block_t	*freelist = NULL;
static unsigned long	freelist_size = 0;
static unsigned long	freelist_max = MAX_FREELIST_SIZE;
static int pool_disable = 0;

int 
nspool_init(int free_size, int disable)
{
        pool_disable = disable;

	if (free_size > 0)
	        freelist_max = free_size;

	if (pool_disable == 0) {
		if (known_pools_lock == NULL) {
			known_pools_lock = PR_NewLock();
			freelist_lock = PR_NewLock();
		}
	} 

	return 0;
}

static block_t *
_create_block(int size)
{
	block_t *newblock = NULL;
	long bytes = ALIGN(size);
	block_t	*free_ptr,
		*last_free_ptr = NULL;

	/* check freelist for large enough block first */

	PR_Lock(freelist_lock);
	free_ptr = freelist;
	while(free_ptr && ((free_ptr->end - free_ptr->data) < bytes)) {
		last_free_ptr = free_ptr;
		free_ptr = free_ptr->next;
	}

	if (free_ptr) {
		newblock = free_ptr;
		if (last_free_ptr)
			last_free_ptr->next = free_ptr->next;
		else
			freelist = free_ptr->next;
		freelist_size -= (newblock->end - newblock->data);
		PR_Unlock(freelist_lock);
		bytes = free_ptr->end - free_ptr->data;
	}
	else {
                pool_blocks_created++;
		PR_Unlock(freelist_lock);
		if (((newblock = (block_t *)NSPERM_MALLOC(sizeof(block_t))) == NULL) || 
		    ((newblock->data = (char *)NSPERM_MALLOC(bytes)) == NULL)) {
                        // XXXMB - log error message
			if (newblock)
				NSPERM_FREE(newblock);
			return NULL;
		}
	}
	newblock->start	= newblock->data;
	newblock->end	= newblock->data + bytes;
	newblock->next	= NULL;

	return newblock;
}

/* Caller must hold lock for the pool */
static void 
_free_block(block_t *block)
{

#ifdef _DEBUG
	memset(block->data, 0xa, block->end-block->data);
#endif /* _DEBUG */

	if ((freelist_size + block->end - block->data) > freelist_max) {
		/* Just have to delete the whole block! */

		PR_Lock(freelist_lock);
                pool_blocks_freed++;
		PR_Unlock(freelist_lock);

		NSPERM_FREE(block->data);
#ifdef _DEBUG
		memset(block, 0xa, sizeof(block));
#endif /* _DEBUG */

		NSPERM_FREE(block);
		return;
	}
	PR_Lock(freelist_lock);
	freelist_size += (block->end - block->data);
	block->start = block->data;

	block->next = freelist;
	freelist = block;
	PR_Unlock(freelist_lock);
}

/* ptr_in_pool()
 * Checks to see if the given pointer is in the given pool.
 * If true, returns a ptr to the block_t containing the ptr;
 * otherwise returns NULL
 */
block_t * 
_ptr_in_pool(pool_t *pool, void *ptr)
{
	block_t *block_ptr = NULL;

	/* try to find a block which contains this ptr */

	if (	((char *)ptr < (char *)pool->curr_block->end) && 
		((char *)ptr >= (char *)pool->curr_block->data) ) 
		block_ptr = pool->curr_block;
	else 
		for(	block_ptr = pool->used_blocks; 
			block_ptr && 
			(((char *)ptr >= (char *)block_ptr->end) && 
			 ((char *)ptr < (char *)block_ptr->data)); 
			block_ptr = block_ptr->next);

	return block_ptr;
}


POOL_EXPORT nspool_handle_t *
nspool_create()
{
	pool_t *newpool;

	if (pool_disable)
		return NULL;

	newpool = (pool_t *)NSPERM_MALLOC(sizeof(pool_t));

	if (newpool) {
		/* Have to initialize now, as pools get created sometimes
		 * before pool_init can be called...
		 */
		if (known_pools_lock == NULL) {
			known_pools_lock = PR_NewLock();
			freelist_lock = PR_NewLock();
		}

		if ( (newpool->curr_block =_create_block(BLOCK_SIZE)) == NULL) {
                        // XXXMB - log error
			NSPERM_FREE(newpool);
			return NULL;
		}
		newpool->used_blocks = NULL;
		newpool->size = 0;
		newpool->next = NULL;
#ifdef POOL_LOCKING
		newpool->lock = PR_NewLock();
#endif

		/* Add to known pools list */
		PR_Lock(known_pools_lock);
		newpool->next = known_pools;
		known_pools = newpool;
		PR_Unlock(known_pools_lock);
	}
	else  {
		// XXXMB - log error
	}

	return (nspool_handle_t *)newpool;
}

POOL_EXPORT void
nspool_destroy(nspool_handle_t *pool_handle)
{
	pool_t *pool = (pool_t *)pool_handle;
	block_t *tmp_blk;
	pool_t *last, *search;

	if (pool_disable)
		return;

	PR_Lock(known_pools_lock);
#ifdef POOL_LOCKING
	PR_Lock(pool->lock);
#endif

	if (pool->curr_block)
		_free_block(pool->curr_block);

	while(pool->used_blocks) {
		tmp_blk = pool->used_blocks;
		pool->used_blocks = pool->used_blocks->next;
		_free_block(tmp_blk);
	}

	/* Remove from the known pools list */
	for (last = NULL, search = known_pools; search; 
		last = search, search = search->next)
		if (search == pool) 
			break;
	if (search) {
		if(last) 
			last->next = search->next;
		else
			known_pools = search->next;
	
	}

#ifdef POOL_LOCKING
	PR_Unlock(pool->lock);
	PR_DestroyLock(pool->lock);
#endif
	PR_Unlock(known_pools_lock);

#ifdef _DEBUG
	memset(pool, 0xa, sizeof(pool));
#endif /* _DEBUG */

	NSPERM_FREE(pool);

	return;
}


POOL_EXPORT void *
nspool_malloc(nspool_handle_t *pool_handle, size_t size)
{
	pool_t *pool = (pool_t *)pool_handle;
	long reqsize, blocksize;
	char *ptr;

	if (pool == NULL || pool_disable) {
		return NSPERM_MALLOC(size);
	}

#ifdef DEBUG
	if (size == 0)
		return NULL;
#endif

#ifdef POOL_LOCKING
	PR_Lock(pool->lock);
#endif

        reqsize = ALIGN(size);
	ptr = pool->curr_block->start;
	pool->curr_block->start += reqsize;

	/* does this fit into the last allocated block? */
	if (pool->curr_block->start > pool->curr_block->end) {

		/* Did not fit; time to allocate a new block */

		pool->curr_block->start -= reqsize;  /* keep structs in tact */

		pool->curr_block->next = pool->used_blocks;
		pool->used_blocks = pool->curr_block;
	
		/* Allocate a chunk of memory which is a multiple of BLOCK_SIZE
		 * bytes 
		 */
		blocksize = ( (size + BLOCK_SIZE-1) / BLOCK_SIZE ) * BLOCK_SIZE;
		if ( (pool->curr_block = _create_block(blocksize)) == NULL) {
			// XXXMB - log error
#ifdef POOL_LOCKING
			PR_Unlock(pool->lock);
#endif
			return NULL;
		}

                ptr = pool->curr_block->start;
		reqsize = ALIGN(size);
		pool->curr_block->start += reqsize;
	}

	pool->size += reqsize;

#ifdef POOL_LOCKING
	PR_Unlock(pool->lock);
#endif
	return ptr;
}

void _pool_free_error()
{
	// XXXMB - log error
	fprintf(stdout, "free used where perm_free should have been used\n");
	

	return;
}

POOL_EXPORT void
nspool_free(nspool_handle_t *pool_handle, void *ptr)
{
	if (pool_handle == NULL || pool_disable) {
		NSPERM_FREE(ptr);
		return;
	}

#ifdef DEBUG
	/* Just to be nice, check to see if the ptr was allocated in a pool.
	 * If not, issue a warning and do a REAL free just to make sure that
	 * we don't leak memory.
	 */
	if ( !_ptr_in_pool((pool_t *)pool_handle, ptr) ) {
		_pool_free_error();

		NSPERM_FREE(ptr);
	}
#endif
	return;
}

POOL_EXPORT void *
nspool_calloc(nspool_handle_t *pool_handle, size_t nelem, size_t elsize)
{
	void *ptr;

	if (pool_handle == NULL || pool_disable)
		return NSPERM_CALLOC(elsize * nelem);

	ptr = nspool_malloc(pool_handle, elsize * nelem);
	if (ptr)
		memset(ptr, 0, elsize * nelem);
	return ptr;
}

POOL_EXPORT void *
nspool_realloc(nspool_handle_t *pool_handle, void *ptr, size_t size)
{
	pool_t *pool = (pool_t *)pool_handle;
	void *newptr;
	block_t *block_ptr;
	int oldsize;

	if (pool_handle == NULL || pool_disable)
		return NSPERM_REALLOC(ptr, size);

	if ( (newptr = nspool_malloc(pool_handle, size)) == NULL) 
		return NULL;

	/* With our structure we don't know exactly where the end
	 * of the original block is.  But we do know an upper bound
	 * which is a valid ptr.  Search the outstanding blocks
	 * for the block which contains this ptr, and copy...
	 */
#ifdef POOL_LOCKING
	PR_Lock(pool->lock);
#endif

	if ( !(block_ptr = _ptr_in_pool(pool, ptr)) ) {
		/* User is trying to realloc nonmalloc'd space! */
		return newptr;
	}

	oldsize = block_ptr->end - (char *)ptr ;
	if (oldsize > size)
		oldsize = size;
	memmove((char *)newptr, (char *)ptr, oldsize);
#ifdef POOL_LOCKING
	PR_Unlock(pool->lock);
#endif

	return newptr;
}

POOL_EXPORT char *
nspool_strdup(nspool_handle_t *pool_handle, const char *orig_str)
{
	char *new_str;
	int len = strlen(orig_str);

	if (pool_handle == NULL || pool_disable)
		return NSPERM_STRDUP(orig_str);

	new_str = (char *)nspool_malloc(pool_handle, len+1);

	if (new_str) 
		memcpy(new_str, orig_str, len+1);

	return new_str;
}

POOL_EXPORT long
nspool_space(nspool_handle_t *pool_handle)
{
	pool_t *pool = (pool_t *)pool_handle;

	return pool->size;
}

POOL_EXPORT int nspool_enabled()
{
#ifndef THREAD_ANY
	/* we don't have USE_NSPR defined so systhread_getdata is undef'ed */
	return 0;
#else
	if (pool_disable || (getThreadMallocKey() == -1) )
		return 0;

	if (!systhread_getdata(getThreadMallocKey()))
		return 0;

	return 1;
#endif
}
