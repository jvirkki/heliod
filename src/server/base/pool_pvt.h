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

#ifndef BASE_POOL_PVT_H
#define BASE_POOL_PVT_H

#ifndef BASE_POOL_H
#include "pool.h"
#endif /* BASE_POOL_H */

/*
 * pool_pvt.h - private definitions for memory pools
 */

/* Definitions */

/*
 * Define PER_POOL_STATISTICS to get detailed statistics for each
 * pool.
 */
#ifdef DEBUG
#define PER_POOL_STATISTICS
#endif

/* Define POOL_GLOBAL_STATISTICS to get global pool statistics */
#define POOL_GLOBAL_STATISTICS

/*
 * When POOL_ZERO_DEBUG is defined, overwrite the contents of freed
 * pool data structures and memory with the POOL_ZERO_DEBUG byte value.
 */
#ifdef DEBUG
#define POOL_ZERO_DEBUG 0xa
#endif

/*
 * DEFAULT_BLOCK_SIZE specifies the minimum granularity, in bytes, used
 * in allocating memory from the heap for use with a pool.  A request
 * for more than DEFAULT_BLOCK_SIZE bytes from a pool will cause a block
 * of that size to be allocated from the heap.
 */
#define DEFAULT_BLOCK_SIZE      (32 * 1024)

/*
 * The pool_recycle() mechanism keeps a list of free blocks associated
 * with each pool, in order to avoid global locking when doing the
 * pool_recycle() operation, or when subsequently allocating memory
 * from the pool.  DEFAULT_RETENTION_SIZE and DEFAULT_RETENTION_NUM
 * specify the maximum number of bytes and blocks, respectively, that
 * will be kept on a per-pool free list.
 */
#define DEFAULT_RETENTION_SIZE  (DEFAULT_BLOCK_SIZE * 2)
#define DEFAULT_RETENTION_NUM   2

/* WORD_SIZE 8 sets us up for 8 byte alignment. */
#define WORD_SIZE       8   
#undef  ALIGN
#define ALIGN(x)        ( (x + WORD_SIZE-1) & (~(WORD_SIZE-1)) )

/* Types */

/*
 * pool_stats_t
 * This structure contains per pool statistics.
 */
#ifdef PER_POOL_STATISTICS
typedef struct pool_stats_t pool_stats_t;
struct pool_stats_t {
    PRUint32  poolId;      /* pool id */
    PRUint32  maxAlloc;    /* maximum bytes ever used from pool */
    PRUint32  allocCnt;    /* count of memory allocations from pool */
    PRUint32  freeCnt;     /* count of pool memory free operations */
    PRUint32  blkAlloc;    /* count of block allocations */
    PRUint32  blkFree;     /* count of blocks freed (on pool_recycle) */
    PRThread *thread;      /* last thread to use pool */
    PRTime    created;     /* time of pool creation */
};
#endif /* PER_POOL_STATISTICS */

typedef struct pool_config_t pool_config_t;
struct pool_config_t {
    PRUint32 block_size;   /* size of blocks to allocate */
    PRUint32 retain_size;  /* maximum bytes kept on per-pool free list */
    PRUint32 retain_num;   /* maximum blocks kept on per-pool free list */
};

#define POOL_CONFIG_INIT { \
                           DEFAULT_BLOCK_SIZE,     /* block_size */ \
                           DEFAULT_RETENTION_SIZE, /* retain_size */ \
                           DEFAULT_RETENTION_NUM,  /* retain_num */ \
                         }

/*
 * block_t
 * When the user allocates space, a DEFAULT_BLOCK_SIZE (or larger) block
 * is created in the pool.  This block is used until all the space is
 * eaten within it.  When all the space is gone, a new block is created.
 */
typedef struct block_t block_t;
struct block_t {
    char    *data;              /* the real alloc'd space */
    char    *start;             /* first free byte in block */
    char    *end;               /* ptr to end of block */
    block_t *next;              /* ptr to next block */
};

#define POOL_PTR_IN_BLOCK(blk, ptr) \
    (((char *)(ptr) < (blk)->end) && ((char *)(ptr) >= (blk)->data))

/*
 * pool_t
 * A pool is a collection of blocks.  The blocks consist of multiple 
 * allocations of memory, but a single allocation cannot be freed by
 * itself.  Once the memory is allocated it is allocated until the
 * entire pool is freed.
 */
typedef struct pool_t pool_t;
struct pool_t {
    block_t  *curr_block;       /* current block being used */
    block_t  *used_blocks;      /* blocks that are all used up */
    block_t  *free_blocks;      /* blocks that are free */
    PRUint32  free_size;        /* number of bytes in free_blocks */
    PRUint32  free_num;         /* number of blocks in free_blocks */
    size_t    size;             /* size of memory in pool */
    pool_t   *next;             /* known_pools list */
#ifdef PER_POOL_STATISTICS
    pool_stats_t stats;         /* statistics for this pool */
#endif /* PER_POOL_STATISTICS */
};

typedef struct pool_global_stats_t pool_global_stats_t;
struct pool_global_stats_t {
    PRLock   *lock;        /* lock for access to poolList */
    pool_t   *poolList;    /* list of known pools */
    PRUint32  createCnt;   /* count of pools created */
    PRUint32  destroyCnt;  /* count of pools destroyed */
#ifdef POOL_GLOBAL_STATISTICS
    PRUint32  blkAlloc;    /* count of block allocations from heap */
    PRUint32  blkFree;     /* count of blocks freed to heap */
#endif /* POOL_GLOBAL_STATISTICS */
};

/* Private functions for inspecting pool configuration/statistics */

NSAPI_PUBLIC pool_config_t *pool_getConfig(void);

NSAPI_PUBLIC pool_global_stats_t *pool_getGlobalStats(void);

#ifdef PER_POOL_STATISTICS
NSAPI_PUBLIC pool_stats_t *pool_getPoolStats(pool_handle_t *pool_handle);
#endif

#endif /* BASE_POOL_PVT_H */
