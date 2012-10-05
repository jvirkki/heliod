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
 * These routines reduce contention on the heap and guard against
 * memory leaks.
 *
 * Thread warning:
 *     This implementation is thread safe.  However, simultaneous 
 *     mallocs/frees to the same "pool" are not safe.  Do not share
 *     pools across multiple threads without providing your own
 *     synchronization.
 *
 * Mike Belshe
 * 11-20-95
 *
 */

#include "netsite.h"
#include "base/systems.h"
#include "base/systhr.h"
#include "base/pool_pvt.h"
#include "base/ereport.h"
#include "base/session.h"
#include "frame/req.h"
#include "frame/http.h"
#include "base/util.h"
#include "base/crit.h"

#include "base/dbtbase.h"

/* Pool configuration parameters */
static pool_config_t pool_config = POOL_CONFIG_INIT;

/* Pool global statistics */
static pool_global_stats_t pool_global_stats;

static int 
pool_internal_init()
{
    if (pool_global_stats.lock == NULL) {
        pool_global_stats.lock = PR_NewLock();
    }

    if (pool_config.block_size == 0) {
        ereport(LOG_INFORM, XP_GetAdminStr(DBT_poolInitInternalAllocatorDisabled_));
    }

    return 0;
}

NSAPI_PUBLIC int
pool_init(pblock *pb, Session *sn, Request *rq)
{
    char *str_block_size = pblock_findval("block-size", pb);
    char *str_pool_disable = pblock_findval("disable", pb);
    int n;

    if (str_block_size != NULL) {
        n = atoi(str_block_size);
        if (n > 0)
            pool_config.block_size = n;
    }

    if (str_pool_disable && util_getboolean(str_pool_disable, PR_TRUE)) {
        /* We'll call PERM_MALLOC() on each pool_malloc() call */
        pool_config.block_size = 0;
        pool_config.retain_size = 0;
        pool_config.retain_num = 0;
    }

    pool_internal_init();

    return REQ_PROCEED;
}

static block_t *
_create_block(pool_t *pool, int size)
{
    block_t *newblock;
    char *newdata;
    block_t **blk_ptr;
    long blen;

    /* Does the pool have any retained blocks on its free list? */
    for (blk_ptr = &pool->free_blocks;
         (newblock = *blk_ptr) != NULL; blk_ptr = &newblock->next) {

        /* Yes, is this block large enough? */
        blen = newblock->end - newblock->data;
        if (blen >= size) {

            /* Yes, take it off the free list */
            *blk_ptr = newblock->next;
            pool->free_size -= blen;
            --pool->free_num;

            /* Give the block to the caller */
            newblock->start = newblock->data;
            goto done;
        }
    }

    newblock = (block_t *)PERM_MALLOC(sizeof(block_t));
    newdata = (char *)PERM_MALLOC(size);
    if (newblock == NULL || (newdata == NULL && size != 0)) {
        ereport(LOG_CATASTROPHE,
                XP_GetAdminStr(DBT_poolCreateBlockOutOfMemory_));
        PERM_FREE(newblock);
        PERM_FREE(newdata);
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }
    newblock->data = newdata;
    newblock->start = newblock->data;
    newblock->end   = newblock->data + size;
    newblock->next  = NULL;
    blen = size;

#ifdef POOL_GLOBAL_STATISTICS
    PR_AtomicIncrement((PRInt32 *)&pool_global_stats.blkAlloc);
#endif /* POOL_GLOBAL_STATISTICS */

  done:

#ifdef PER_POOL_STATISTICS
    ++pool->stats.blkAlloc;
#endif /* PER_POOL_STATISTICS */

    return newblock;
}

static void 
_free_block(block_t *block)
{
    long blen = block->end - block->data;

#ifdef POOL_ZERO_DEBUG
    memset(block->data, POOL_ZERO_DEBUG, blen);
#endif /* POOL_ZERO_DEBUG */

    PERM_FREE(block->data);

#ifdef POOL_ZERO_DEBUG
    memset(block, POOL_ZERO_DEBUG, sizeof(block));
#endif /* POOL_ZERO_DEBUG */

    PERM_FREE(block);

#ifdef POOL_GLOBAL_STATISTICS
    PR_AtomicIncrement((PRInt32 *)&pool_global_stats.blkFree);
#endif /* POOL_GLOBAL_STATISTICS */
}

/* ptr_in_pool()
 * Checks to see if the given pointer is in the given pool.
 * If true, returns a ptr to the block_t containing the ptr;
 * otherwise returns NULL
 */
block_t * 
_ptr_in_pool(pool_t *pool, const void *ptr)
{
    block_t *block_ptr = NULL;

    /* try to find a block which contains this ptr */

    if (POOL_PTR_IN_BLOCK(pool->curr_block, ptr)) {
        block_ptr = pool->curr_block;
    }
    else {
        for (block_ptr = pool->used_blocks; block_ptr; block_ptr = block_ptr->next) {
            if (POOL_PTR_IN_BLOCK(block_ptr, ptr))
                break;
        }
    }
    return block_ptr;
}


NSAPI_PUBLIC pool_handle_t *
pool_create()
{
    pool_t *newpool;

    newpool = (pool_t *)PERM_MALLOC(sizeof(pool_t));

    if (newpool) {
        /* Have to initialize now, as pools get created sometimes
         * before pool_init can be called...
         */
        if (pool_global_stats.lock == NULL) {
            pool_internal_init();
        }

        newpool->used_blocks = NULL;
        newpool->free_blocks = NULL;
        newpool->free_size = 0;
        newpool->free_num = 0;
        newpool->size = 0;
        newpool->next = NULL;

#ifdef PER_POOL_STATISTICS
        /* Initial per pool statistics */
        memset((void *)(&newpool->stats), 0, sizeof(newpool->stats));
        newpool->stats.thread = PR_GetCurrentThread();
        newpool->stats.created = PR_Now();
#endif /* PER_POOL_STATISTICS */

        /* No need to lock, since pool has not been exposed yet */
        newpool->curr_block =_create_block(newpool, pool_config.block_size);
        if (newpool->curr_block == NULL) {
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_poolCreateOutOfMemory_));
            pool_destroy((pool_handle_t *)newpool);
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return NULL;
        }

        /* Add to known pools list */
        PR_Lock(pool_global_stats.lock);
        newpool->next = pool_global_stats.poolList;
        pool_global_stats.poolList = newpool;
        ++pool_global_stats.createCnt;
#ifdef PER_POOL_STATISTICS
        newpool->stats.poolId = pool_global_stats.createCnt;
#endif /* PER_POOL_STATISTICS */
        PR_Unlock(pool_global_stats.lock);

    }
    else {
        ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_poolCreateOutOfMemory_1));
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }

    return (pool_handle_t *)newpool;
}

/*
 * pool_mark - get mark for subsequent recycle
 *
 * This function returns a value that can be used to free all pool
 * memory which is subsequently allocated, without freeing memory
 * that has already been allocated when pool_mark() is called.
 * The pool_recycle() function is used to free the memory allocated
 * since pool_mark() was called.
 *
 * This function may be called several times before pool_recycle()
 * is called, but some care must be taken not to pass an invalid
 * mark value to pool_recycle(), which would cause all pool memory
 * to be freed.  A mark value becomes invalid when pool_recycle is
 * called with a previously returned mark value.
 */
NSAPI_PUBLIC void *
pool_mark(pool_handle_t *pool_handle)
{
    pool_t *pool = (pool_t *)pool_handle;

    PR_ASSERT(pool != NULL);

    if (pool == NULL)
        return NULL;

#ifdef PER_POOL_STATISTICS
    pool->stats.thread = PR_GetCurrentThread();
#endif /* PER_POOL_STATISTICS */

    /* Never return end as it points outside the block */
    if (pool->curr_block->start == pool->curr_block->end)
        return pool->curr_block;

    return (void *)(pool->curr_block->start);
}

/*
 * pool_recycle - recycle memory in a pool
 *
 * This function returns all the allocated memory for a pool back to
 * a free list associated with the pool. It is like pool_destroy() in
 * the sense that all data structures previously allocated from the
 * pool are freed, but it keeps the memory associated with the pool,
 * and doesn't actually destroy the pool.
 *
 * The "mark" argument can be a value previously returned by
 * pool_mark(), in which case the pool is returned to the state it
 * was in when pool_mark() was called, or it can be NULL, in which
 * case the pool is completely recycled.
 */
NSAPI_PUBLIC void
pool_recycle(pool_handle_t *pool_handle, void *mark)
{
    pool_t *pool = (pool_t *)pool_handle;
    block_t *tmp_blk;
    unsigned long blen;

    PR_ASSERT(pool != NULL);

    if (pool == NULL)
        return;

    /* Fix up curr_block.  There should always be a curr_block. */
    tmp_blk = pool->curr_block;
    PR_ASSERT(tmp_blk != NULL);

    /* Start with curr_block, then scan blocks on used_blocks list */
    for (;;) {

        /* Check if the mark is at the end of this block */
        if (tmp_blk == mark) {
            pool->curr_block = tmp_blk;
            break;
        }

        /* Look for a block containing the mark */
        if (POOL_PTR_IN_BLOCK(tmp_blk, mark)) {

            /* Reset block start pointer to marked spot */
            if (tmp_blk == pool->curr_block) {
                blen = tmp_blk->start - (char *)mark;
            } else {
                blen = tmp_blk->end - (char *)mark;
            }
            pool->size -= blen;
            PR_ASSERT(pool->size >= 0);
            tmp_blk->start = (char *)mark;
            pool->curr_block = tmp_blk;
            break;
        }

        /* Reset block start pointer to base of block */
        if (tmp_blk == pool->curr_block) {
            /* Count just the allocated length in the current block */
            blen = tmp_blk->start - tmp_blk->data;
        }
        else {
            /* Count the entire size of a used_block */
            blen = tmp_blk->end - tmp_blk->data;
        }
        tmp_blk->start = tmp_blk->data;
        pool->size -= blen;
        PR_ASSERT(pool->size >= 0);

        /*
         * If there are no more used blocks after this one, then set
         * this block up as the current block and return.
         */
        if (pool->used_blocks == NULL) {
            PR_ASSERT(mark == NULL);
            pool->curr_block = tmp_blk;
            break;
        }

        /* Otherwise free this block one way or another */

        /* Add block length to total retained length and check limit */
        if ((pool->free_size + blen) <= pool_config.retain_size &&
            pool->free_num < pool_config.retain_num) {

            /* Retain block on pool free list */
            /*
             * XXX hep - could sort blocks on free list in order
             * ascending size to get "best fit" allocation in
             * _create_block(), but the default block size is large
             * enough that fit should rarely be an issue.
             */
            tmp_blk->next = pool->free_blocks;
            pool->free_blocks = tmp_blk;
            pool->free_size += blen;
            ++pool->free_num;
        }
        else {
            /* Limit exceeded - free block */
            _free_block(tmp_blk);
        }

#ifdef PER_POOL_STATISTICS
        ++pool->stats.blkFree;
#endif /* PER_POOL_STATISTICS */

        /* Remove next block from used blocks list */
        tmp_blk = pool->used_blocks;
        pool->used_blocks = tmp_blk->next;
    }
}

NSAPI_PUBLIC void
pool_destroy(pool_handle_t *pool_handle)
{
    pool_t *pool = (pool_t *)pool_handle;
    block_t *tmp_blk;

    PR_ASSERT(pool != NULL);

    if (pool == NULL)
        return;

    if (pool->curr_block)
        _free_block(pool->curr_block);

    while(pool->used_blocks) {
        tmp_blk = pool->used_blocks;
        pool->used_blocks = tmp_blk->next;
        _free_block(tmp_blk);
    }

    while(pool->free_blocks) {
        tmp_blk = pool->free_blocks;
        pool->free_blocks = tmp_blk->next;
        _free_block(tmp_blk);
    }

    {
        pool_t **ppool;

        /* Remove from the known pools list */
        PR_Lock(pool_global_stats.lock);
        for (ppool = &pool_global_stats.poolList;
             *ppool; ppool = &(*ppool)->next) {
            if (*ppool == pool) {
                ++pool_global_stats.destroyCnt;
                *ppool = pool->next;
                break;
            }
        }
        PR_Unlock(pool_global_stats.lock);
    }

#ifdef POOL_ZERO_DEBUG
    memset(pool, POOL_ZERO_DEBUG, sizeof(pool));
#endif /* POOL_ZERO_DEBUG */

    PERM_FREE(pool);

    return;
}


NSAPI_PUBLIC void *
pool_malloc(pool_handle_t *pool_handle, size_t size)
{
    pool_t *pool = (pool_t *)pool_handle;
    block_t *curr_block;
    long reqsize, blocksize;
    char *ptr;

    if (pool == NULL)
        return PERM_MALLOC(size);

    reqsize = ALIGN(size);
    if (reqsize == 0) {
        /* Assign a unique address to each 0-byte allocation */
        reqsize = WORD_SIZE;
    }

    curr_block = pool->curr_block;
    ptr = curr_block->start;
    curr_block->start += reqsize;

    /* does this fit into the last allocated block? */
    if (curr_block->start > curr_block->end) {

        /* Did not fit; time to allocate a new block */

        curr_block->start -= reqsize;  /* keep structs in tact */

        /* Count unallocated bytes in current block in pool size */
        pool->size += curr_block->end - curr_block->start;
        PR_ASSERT(pool->size >= 0);
#ifdef PER_POOL_STATISTICS
        if (pool->size > pool->stats.maxAlloc) {
            pool->stats.maxAlloc = pool->size;
        }
#endif /* PER_POOL_STATISTICS */

        /* Move current block to used block list */
        curr_block->next = pool->used_blocks;
        pool->used_blocks = curr_block;

        /* Allocate a chunk of memory which is at least block_size bytes */
        blocksize = reqsize;
        if (blocksize < pool_config.block_size)
            blocksize = pool_config.block_size;

        curr_block = _create_block(pool, blocksize);
        pool->curr_block = curr_block;

        if (curr_block == NULL) {
            ereport(LOG_CATASTROPHE,
                    XP_GetAdminStr(DBT_poolMallocOutOfMemory_));
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return NULL;
        }

        ptr = curr_block->start;
        curr_block->start += reqsize;
    }

    pool->size += reqsize;
    PR_ASSERT(pool->size >= 0);

#ifdef PER_POOL_STATISTICS
    if (pool->size > pool->stats.maxAlloc) {
        pool->stats.maxAlloc = pool->size;
    }
    ++pool->stats.allocCnt;
    pool->stats.thread = PR_GetCurrentThread();
#endif /* PER_POOL_STATISTICS */

    return ptr;
}

NSAPI_PUBLIC void
pool_free(pool_handle_t *pool_handle, void *ptr)
{
    pool_t *pool = (pool_t *)pool_handle;

    if (ptr == NULL)
        return;

    if (pool == NULL) {
        PERM_FREE(ptr);
        return;
    }

    PR_ASSERT(_ptr_in_pool(pool, ptr));

#ifdef PER_POOL_STATISTICS

    ++pool->stats.freeCnt;
    pool->stats.thread = PR_GetCurrentThread();

#endif /* PER_POOL_STATISTICS */

    return;
}

NSAPI_PUBLIC void *
pool_calloc(pool_handle_t *pool_handle, size_t nelem, size_t elsize)
{
    void *ptr;

    if (pool_handle == NULL)
        return PERM_CALLOC(elsize * nelem);

    ptr = pool_malloc(pool_handle, elsize * nelem);
    if (ptr)
        memset(ptr, 0, elsize * nelem);
    return ptr;
}

NSAPI_PUBLIC void *
pool_realloc(pool_handle_t *pool_handle, void *ptr, size_t size)
{
    pool_t *pool = (pool_t *)pool_handle;
    void *newptr;
    block_t *block_ptr;
    size_t oldsize;

    if (pool == NULL)
        return PERM_REALLOC(ptr, size);

    if ( (newptr = pool_malloc(pool_handle, size)) == NULL) 
        return NULL;

    /* With our structure we don't know exactly where the end
     * of the original block is.  But we do know an upper bound
     * which is a valid ptr.  Search the outstanding blocks
     * for the block which contains this ptr, and copy...
     */

    if ( !(block_ptr = _ptr_in_pool(pool, ptr)) ) {
        /* User is trying to realloc nonmalloc'd space! */
        return newptr;
    }

    oldsize = block_ptr->end - (char *)ptr ;
    if (oldsize > size)
        oldsize = size;
    memmove((char *)newptr, (char *)ptr, oldsize);

    return newptr;
}

NSAPI_PUBLIC char *
pool_strdup(pool_handle_t *pool_handle, const char *orig_str)
{
    char *new_str;
    int len = strlen(orig_str);

    if (pool_handle == NULL)
        return PERM_STRDUP(orig_str);

    new_str = (char *)pool_malloc(pool_handle, len+1);

    if (new_str) 
        memcpy(new_str, orig_str, len+1);

    return new_str;
}

NSAPI_PUBLIC long
pool_space(pool_handle_t *pool_handle)
{
    pool_t *pool = (pool_t *)pool_handle;

    return pool->size;
}

NSAPI_PUBLIC int pool_enabled()
{
    if (getThreadMallocKey() == -1)
        return 0;

    if (!systhread_getdata(getThreadMallocKey()))
        return 0;

    return 1;
}

#ifdef DEBUG
NSAPI_PUBLIC void INTpool_assert(pool_handle_t *pool_handle, const void *ptr)
{
    pool_t *pool = (pool_t *)pool_handle;

    if (pool == NULL)
        return;

    PR_ASSERT(_ptr_in_pool(pool, ptr));
}
#endif

NSAPI_PUBLIC pool_config_t *pool_getConfig(void)
{
    return &pool_config;
}

#ifdef POOL_GLOBAL_STATISTICS
NSAPI_PUBLIC pool_global_stats_t *pool_getGlobalStats(void)
{
    return &pool_global_stats;
}
#endif /* POOL_GLOBAL_STATISTICS */

#ifdef PER_POOL_STATISTICS
NSAPI_PUBLIC pool_stats_t *pool_getPoolStats(pool_handle_t *pool_handle)
{
    pool_t *pool = (pool_t *)pool_handle;

    if (pool == NULL)
        return NULL;

    return &pool->stats;
}
#endif /* PER_POOL_STATISTICS */

