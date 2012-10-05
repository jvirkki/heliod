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
 * shmem.h: Portable abstraction for memory shared among a server's workers
 * 
 * Rob McCool
 */


#include "shmem.h"
#include "NsprWrap/NsprError.h"

#ifndef NS_OLDES3X
#include "private/pprio.h"
#endif /* NS_OLDES3X */

#ifdef XP_UNIX

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

NSPR_BEGIN_EXTERN_C
#include <sys/mman.h>
NSPR_END_EXTERN_C

NSAPI_PUBLIC shmem_s *shmem_alloc(char *name, int size, int expose)
{
    shmem_s *ret = (shmem_s *) PERM_MALLOC(sizeof(shmem_s));
    char *growme = NULL;

    if( (ret->fd = PR_Open(name, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0666)) == NULL) {
        PERM_FREE(ret);
        return NULL;
    }
    growme = (char *) PERM_MALLOC(size);
    if (growme == NULL) {
        PR_Close(ret->fd);
        PERM_FREE(ret);
        return NULL;
    }
    ZERO(growme, size);
    if(PR_Write(ret->fd, (char *)growme, size) < 0) {
        PR_Close(ret->fd);
        PERM_FREE(growme);
        PERM_FREE(ret);
        return NULL;
    }
    PERM_FREE(growme);
#ifdef NS_OLDES3X
    PR_Seek(ret->fd, 0, SEEK_SET);
#else
    PR_Seek(ret->fd, 0, PR_SEEK_SET);
#endif /* NS_OLDES3X */
    if( (ret->data = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE,
                          SHMEM_MMAP_FLAGS, PR_FileDesc2NativeHandle(ret->fd), 0)) == (caddr_t) -1)
    {
        NsprError::mapUnixErrno();
        PR_Close(ret->fd);
        PERM_FREE(ret);
        return NULL;
    }
    if(!expose) {
        ret->name = NULL;
        unlink(name);
    }
    else
        ret->name = STRDUP(name);
    ret->size = size;
    return ret;
}


NSAPI_PUBLIC void shmem_free(shmem_s *region)
{
    if(region->name) {
        unlink(region->name);
        FREE(region->name);
    }
    munmap((char *)region->data, region->size);  /* CLEARLY, C++ SUCKS */
    PR_Close(region->fd);
    PERM_FREE(region);
}

#endif /* XP_UNIX */

#ifdef XP_WIN32

#define PAGE_SIZE	(1024*8)
#define ALIGN(x)	( (x+PAGE_SIZE-1) & (~(PAGE_SIZE-1)) )
NSAPI_PUBLIC shmem_s *shmem_alloc(char *name, int size, int expose)
{
    shmem_s *ret = (shmem_s *) PERM_MALLOC(sizeof(shmem_s));
    HANDLE fHandle;

    ret->fd = 0; /* not used on NT */
  
    size = ALIGN(size);
    if( !(ret->fdmap = CreateFileMapping(
                           (HANDLE)0xffffffff,
                           NULL, 
                           PAGE_READWRITE,
                           0, 
                           size, 
                           name)) )
    {
        NsprError::mapWin32Error();
        PERM_FREE(ret);
        return NULL;
    }
    if( !(ret->data = (char *)MapViewOfFile (
                               ret->fdmap, 
                               FILE_MAP_ALL_ACCESS,
                               0, 
                               0, 
                               0)) )
    {
        NsprError::mapWin32Error();
        CloseHandle(ret->fdmap);
        PERM_FREE(ret);
        return NULL;
    }
    ret->size = size;
    ret->name = NULL;

    return ret;
}


NSAPI_PUBLIC void shmem_free(shmem_s *region)
{
    if(region->name) {
        DeleteFile(region->name);
        PERM_FREE(region->name);
    }
    UnmapViewOfFile(region->data);
    CloseHandle(region->fdmap);
    PERM_FREE(region);
}

#endif /* XP_WIN32 */
