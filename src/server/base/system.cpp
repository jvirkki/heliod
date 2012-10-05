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
 * system.c: A grab bag of system-level abstractions
 * 
 * Many authors
 */

#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <new>
using namespace std;
#else
#include <new.h>
#endif
#include "netsite.h"
#include "base/nsassert.h"
#include "base/ereport.h"

#ifdef XP_WIN32
#include <windows.h>
#include <process.h>
#endif

static int thread_malloc_key = -1;

static char* temp_dir = NULL;

#ifdef XP_WIN32
_PNH original_newhandler = 0;
#else
typedef void (newhandler)(void);
static newhandler *original_newhandler = 0;
#endif

#include "base/pool.h"
#include "base/systhr.h"

#define MALLOC_KEY \
    ((pool_handle_t *)(thread_malloc_key != -1 ? systhread_getdata(thread_malloc_key) : NULL))


#ifdef MCC_DEBUG
#define DEBUG_MALLOC
#endif

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

NSAPI_PUBLIC char *system_version()
{
    return PRODUCT_ID"/"PRODUCT_VERSION_ID;
}

NSAPI_PUBLIC pool_handle_t *system_pool(void)
{
    return MALLOC_KEY;
}

NSAPI_PUBLIC void *system_malloc(int size)
{
    void *ret;
    ret = pool_malloc(MALLOC_KEY, size);
    if (!ret) {
        ereport_outofmemory();
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return ret;
}


NSAPI_PUBLIC void *system_calloc(int size)
{
    void *ret;
    ret = pool_malloc(MALLOC_KEY, size);
    if(ret) {
        ZERO(ret, size);
    } else {
        ereport_outofmemory();
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return ret;
}


NSAPI_PUBLIC void *system_realloc(void *ptr, int size)
{
    void *ret;
    ret = pool_realloc(MALLOC_KEY, ptr, size);
    if (!ret) {
        ereport_outofmemory();
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return ret;
}


NSAPI_PUBLIC void system_free(void *ptr)
{
    pool_free(MALLOC_KEY, ptr);
}

NSAPI_PUBLIC char *system_strdup(const char *ptr)
{
    NS_ASSERT(ptr);
    char *ret;
    ret = pool_strdup(MALLOC_KEY, ptr);
    if (!ret) {
        ereport_outofmemory();
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return ret;
}


NSAPI_PUBLIC void *system_malloc_perm(int size)
{
    void *ret;
#ifndef DEBUG_MALLOC
    ret = malloc(size);
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

    ret = real_ptr;
#endif
    if (!ret) {
        ereport_outofmemory();
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return ret;
}

NSAPI_PUBLIC void *system_calloc_perm(int size)
{
    void *ret = system_malloc_perm(size);
    if(ret)
        ZERO(ret, size);
    return ret;
}

NSAPI_PUBLIC void *system_realloc_perm(void *ptr, int size)
{
    void *ret;

#ifndef DEBUG_MALLOC
    ret = realloc(ptr, size);
#else
    int *magic, *length;
    char *baseptr;
    char *cptr;

    /* realloc semantics allow realloc(NULL, size) */
    if (ptr == NULL)
        return system_malloc_perm(size);

    cptr = (char *)ptr - DEBUG_MARGIN - 2 * sizeof(int);
    magic = (int *)cptr;
    if (*magic == DEBUG_MAGIC) {
        cptr += sizeof(int);
        length = (int *)cptr;
        if (*length < size) {
            char *newptr = (char *)system_malloc_perm(size);
            memcpy(newptr, ptr, *length);
            system_free_perm(ptr);

            ret = newptr;
        }else {
            ret = ptr;
        }
    } else {
        ereport(LOG_WARN, XP_GetAdminString(DBT_systemReallocSmallerSize));
        ret = realloc(ptr, size);
    }
#endif

    if (!ret) {
        ereport_outofmemory();
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }

    return ret;
}

NSAPI_PUBLIC void system_free_perm(void *ptr)
{
#ifdef DEBUG_MALLOC
    int *length, *magic;
    char *baseptr, *cptr;
    int index;

    NS_ASSERT(ptr);

    cptr = baseptr = ((char *)ptr) - DEBUG_MARGIN - 2*sizeof(int);

    magic = (int *)cptr;
    if (*magic == DEBUG_MAGIC) {
        cptr += sizeof(int);

        length = (int *)cptr;

        cptr += sizeof(int); 
        for (index=0; index<DEBUG_MARGIN; index++)
            if (cptr[index] != DEBUG_MARGIN_CHAR) {
                ereport(LOG_CATASTROPHE, XP_GetAdminString(DBT_systemRFreeCorruptMemoryPre));
                break;
            }

        cptr += DEBUG_MARGIN + *length;
        for (index=0; index<DEBUG_MARGIN; index++)
            if (cptr[index] != DEBUG_MARGIN_CHAR) {
                ereport(LOG_CATASTROPHE, XP_GetAdminString(DBT_systemRFreeCorruptMemoryPost));
                break;
            }

        memset(baseptr, DEBUG_FREE_CHAR, *length + 2*DEBUG_MARGIN+sizeof(int));
    } else {
        ereport(LOG_CATASTROPHE, XP_GetAdminString(DBT_systemRFreeUnallocatedMem));
    }
    free(baseptr);
#else
    free(ptr);
#endif
}

NSAPI_PUBLIC char *system_strdup_perm(const char *ptr)
{
    char *ret;

#ifndef DEBUG_MALLOC
    NS_ASSERT(ptr);
    ret = strdup(ptr);
#else
    int len = strlen(ptr);
    char *nptr = (char *)system_malloc_perm(len+1);
    memcpy(nptr, ptr, len);
    nptr[len] = '\0';
    ret = nptr;
#endif

    if (!ret) {
        ereport_outofmemory();
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }

    return ret;
}

NSAPI_PUBLIC void system_set_temp_dir(const char *dir)
{
    // Leak any previously-allocated dir in case someone still has a reference
    temp_dir = STRDUP(dir);
}

NSAPI_PUBLIC const char *system_get_temp_dir(void)
{
    char* dir = temp_dir;

    if (!dir) {
#ifdef XP_WIN32
        dir = getenv("TEMP");
        if (!dir) dir = getenv("TMP");
        if (!dir) dir = "C:\\TEMP";
#else
        dir = "/tmp";
#endif
    }

    return dir;
}

NSAPI_PUBLIC int 
getThreadMallocKey(void)
{
    return thread_malloc_key;
}

NSAPI_PUBLIC void
InitThreadMallocKey(void)
{
    PR_NewThreadPrivateIndex((unsigned int *)&thread_malloc_key, NULL);
    PR_ASSERT(thread_malloc_key);
}

#ifdef XP_WIN32
static int _cdecl system_newhandler(unsigned int size)
{
    ereport_outofmemory();

    if (original_newhandler) {
        // Let original handler deal with things
        return (*original_newhandler)(size);
    }

    // Tell new not to retry the allocation
    return 0;
}
#else
static void system_newhandler()
{
    // We want to preserve the original new semantics, but we don't know what
    // those semantics are.  Some platforms throw xalloc while others throw
    // bad_alloc.

    ereport_outofmemory();

    if (original_newhandler) {
        // Let original handler deal with things
        (*original_newhandler)();
    } else {
        // No original handler to call; try to remove all handlers
        static PRBool flagRemovedHandler = PR_FALSE;
        if (flagRemovedHandler) {
            abort();
        }
        set_new_handler(0);
        flagRemovedHandler = PR_TRUE;
    }
}
#endif

NSAPI_PUBLIC void system_setnewhandler(void)
{
#ifdef XP_WIN32
    original_newhandler = _set_new_handler(system_newhandler);
#else
    original_newhandler = set_new_handler(system_newhandler);
#endif
}
