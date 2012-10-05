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
 * malloc.cpp: debug memory allocation routines
 * 
 * 
 */

#ifdef _DEBUG
#define DEBUG_MALLOC
#endif

#include "nspr.h"
#include "nsmalloc.h"
#include "nspool.h"
#include <string.h>

#ifdef DEBUG_MALLOC

/* The debug malloc routines provide several functions:
 *
 *  - detect allocated memory overflow/underflow
 *  - detect multiple frees
 *  - intentionally clobbers malloc'd buffers
 *  - intentionally clobbers freed buffers
 */
#define DEBUG_MAGIC 0x12345678
#define DEBUG_MARGIN 32
#define DEBUG_MARGIN_CHAR '*'
#define DEBUG_MALLOC_CHAR '.'
#define DEBUG_FREE_CHAR   'X'
#endif /* DEBUG_MALLOC */

int thread_key = -1;

#define MALLOC_KEY (thread_key == -1?NULL:(nspool_handle_t*)PR_GetThreadPrivate(thread_key))

MALLOC_EXPORT void *
NSMALLOC(int size)
{
    return nspool_malloc(MALLOC_KEY, size);
}


MALLOC_EXPORT void *
NSCALLOC(int size)
{
    void *ret;
    ret = nspool_malloc(MALLOC_KEY, size);
    if(ret)
        memset(ret, 0, size);
    return ret;
}


MALLOC_EXPORT void *
NSREALLOC(void *ptr, int size)
{
    return nspool_realloc(MALLOC_KEY, ptr, size);
}


MALLOC_EXPORT void 
NSFREE(void *ptr)
{
    nspool_free(MALLOC_KEY, ptr);
}

MALLOC_EXPORT char *
NSSTRDUP(const char *ptr)
{
    PR_ASSERT(ptr);
    return nspool_strdup(MALLOC_KEY, ptr);
}


MALLOC_EXPORT void *
NSPERM_MALLOC(int size)
{
#ifndef DEBUG_MALLOC
    return malloc(size);
#else
    char *ptr = (char *)malloc(size + 2*DEBUG_MARGIN+2*sizeof(int));
    char *real_ptr;
    int *magic;
    int *length;
  
    magic = (int *)ptr;
    *magic = DEBUG_MAGIC;
    ptr += sizeof(int);
    length = (int *)ptr;
    *length = size;
    ptr += sizeof(int);
    memset(ptr, DEBUG_MARGIN_CHAR, DEBUG_MARGIN);
    ptr += DEBUG_MARGIN;
    memset(ptr, DEBUG_MALLOC_CHAR, size);
    real_ptr = ptr;
    ptr += size;
    memset(ptr, DEBUG_MARGIN_CHAR, DEBUG_MARGIN);

    return real_ptr;
#endif
}

MALLOC_EXPORT void *
NSPERM_CALLOC(int size)
{
    void *ret = NSPERM_MALLOC(size);
    if(ret)
        memset(ret, 0, size);
    return ret;
}

MALLOC_EXPORT void *
NSPERM_REALLOC(void *ptr, int size)
{
#ifndef DEBUG_MALLOC
    return realloc(ptr, size);
#else
    int *magic, *length;
    char *baseptr;
    char *cptr;

    cptr = (char *)ptr - DEBUG_MARGIN - 2 * sizeof(int);
    magic = (int *)cptr;
    if (*magic == DEBUG_MAGIC) {
        cptr += sizeof(int);
        length = (int *)cptr;
        if (*length < size) {
            char *newptr = (char *)NSPERM_MALLOC(size);
            memcpy(newptr, ptr, *length);
            NSPERM_FREE(ptr);

            return newptr;
        }else {
            return ptr;
        }
    } else {
        // XXXMB - log error
        fprintf(stderr, "realloc: attempt to realloc to smaller size");
        return realloc(ptr, size);
    }

#endif
}

MALLOC_EXPORT void 
NSPERM_FREE(void *ptr)
{
#ifdef DEBUG_MALLOC
    int *length, *magic;
    char *baseptr, *cptr;
    int index;

    PR_ASSERT(ptr);

    cptr = baseptr = ((char *)ptr) - DEBUG_MARGIN - 2*sizeof(int);

    magic = (int *)cptr;
    if (*magic == DEBUG_MAGIC) {
        cptr += sizeof(int);

        length = (int *)cptr;

        cptr += sizeof(int); 
        for (index=0; index<DEBUG_MARGIN; index++)
            if (cptr[index] != DEBUG_MARGIN_CHAR) {
                // XXXMB - log error
                fprintf(stderr, "free: corrupt memory (prebounds overwrite)");
                break;
            }

        cptr += DEBUG_MARGIN + *length;
        for (index=0; index<DEBUG_MARGIN; index++)
            if (cptr[index] != DEBUG_MARGIN_CHAR) {
                // XXXMB - log error
                fprintf(stderr, "free: corrupt memory (prebounds overwrite)");
                break;
            }

        memset(baseptr, DEBUG_FREE_CHAR, *length + 2*DEBUG_MARGIN+sizeof(int));
    } else {
        // XXXMB - log error
        fprintf(stderr, "free: freeing unallocated memory");
    }
    free(baseptr);
#else
    free(ptr);
#endif
}

MALLOC_EXPORT char *
NSPERM_STRDUP(const char *ptr)
{
#ifdef DEBUG_MALLOC
    int len = strlen(ptr);
    char *nptr = (char *)NSPERM_MALLOC(len+1);
    memcpy(nptr, ptr, len);
    nptr[len] = '\0';
    return nptr;
#else
    PR_ASSERT(ptr);
    return strdup(ptr);
#endif
}
