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
 * buffer.c: Code for dealing with I/O buffers
 * 
 * See buffer.h for public documentation.
 *
 * Rob McCool
 * 
 */


#include "base/buffer.h"
#include "base/ereport.h"      /* ereport */
#include "base/dbtbase.h"
#include "NsprWrap/NsprError.h"
#ifndef NS_OLDES3X
#include "private/pprio.h"
#endif /* !NS_OLDES3X */


/* ----------------------------- buffer_open ------------------------------ */

#ifdef XP_UNIX
#include <sys/mman.h>           /* mmap, munmap */
#endif

/* FUNCTION: filebuf_create
 * DESCRIPTION:
 *    A convenience function to create a file buffer if the file is already
 *    open or mmap'd.  It creates a new filebuf for use with the mmap'd file.
 *    If mmap_ptr is NULL, it creates a buffer with size bufsz.
 * INPUTS:
 *    fd - the file to buffer
 *    mmap_ptr - a pointer to the mmap'd region for this file.
 *    mmap_len - the length of the mmap'd region
 *    bufsz    - buffer size to use.
 * OUTPUTS:
 *    none
 * RETURNS:
 *    NULL on failure
 *    a valid pointer on success
 * RESTRICTIONS:
 */
filebuf_t *filebuf_create(SYS_FILE fd, caddr_t mmap_ptr, int mmap_len, int bufsz)
{
    filebuf_t *buf = (filebuf_t *)MALLOC(sizeof(filebuf_t));

    if (buf) {
        buf->fd = fd;
        buf->pos = 0;
#ifdef XP_WIN32
        buf->fdmap = NULL;
#endif
        buf->fp = mmap_ptr;
        buf->len = mmap_len;
        buf->cursize = mmap_len;
        buf->inbuf = NULL;
        buf->errmsg = NULL;
    }

    return buf;
}

/* FUNCTION: filebuf_close_buffer
 * DESCRIPTION:
 *    Deallocates a buffer but does not close the underlying file.  If 
 *    clean_mmap is set to 1, and the file is mmap'd, the mmap'd region
 *    will also be closed.  But the file itself will remain open.
 * INPUTS:
 *    buf - the buffer to close
 *    clean_mmap - a flag specifying whether or not the mmap'd region should
 *                 be closed.
 * OUTPUTS:
 *    none
 * RETURNS:
 *    none
 * RESTRICTIONS:
 */
void filebuf_close_buffer(filebuf_t *buf, int clean_mmap)
{
#ifdef XP_UNIX
    if ( clean_mmap )
        if(munmap(buf->fp,buf->len) == -1) {
            NsprError::mapUnixErrno();
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_munmapFailedS_), system_errmsg());
        }
#else
    if ( clean_mmap ) {
        UnmapViewOfFile(buf->fp);
        CloseHandle(buf->fdmap);
    }
#endif
    FREE(buf);
}


#ifdef XP_UNIX

/* FUNCTION: filebuf_open_nostat
 * DESCRIPTION:
 *    A slightly cheaper version of filebuf_open for if the stat structure
 *    is already available.
 * INPUTS:
 *    fd - the file to open a buffer for
 *    sz - the size of the buffer
 *    finfo - the stat structure for this file, with the length
 *            of the file already known
 * OUTPUTS:
 *    none
 * RETURNS:
 *    NULL on failure
 *    a valid pointer on success
 * RESTRICTIONS:
 */
NSAPI_PUBLIC filebuf_t *filebuf_open_nostat(SYS_FILE fd, int sz, 
                                          struct stat *finfo) 
{
    filebuf_t *buf = (filebuf_t *) MALLOC(sizeof(filebuf_t));

    buf->pos = 0;
    buf->len = finfo->st_size;
    buf->cursize = finfo->st_size;
    buf->fd = fd;

    if(buf->len) {
        buf->fp = (caddr_t)mmap(NULL, buf->len, PROT_READ, FILE_MMAP_FLAGS, PR_FileDesc2NativeHandle(fd), 0);
        if(buf->fp == (caddr_t) -1) {
            NsprError::mapUnixErrno();
            return NULL;
        }
    } else {
        buf->fp = NULL;
    }
    buf->inbuf = NULL;
    buf->errmsg = NULL;

    return buf;
}

NSAPI_PUBLIC filebuf_t* filebuf_open_nostat_nommap(SYS_FILE fd, int sz,
                                                struct stat *finfo)
{
    filebuf_t *buf = (filebuf_t *) MALLOC(sizeof(filebuf_t));
    off_t n = finfo->st_size;
    buf->pos = 0;
    buf->len = finfo->st_size;
    buf->cursize = finfo->st_size;
    buf->fd = fd;
    buf->inbuf = NULL;
    buf->errmsg = NULL;
    buf->fp = 0;
    if(buf->len) {
        buf->fp = (char*) PERM_MALLOC(buf->len);
        if(!(buf->fp)) {
            FREE(buf);
            return NULL;
        }
        int bytesRead = 0;
        char* p = buf->fp;
        while (((bytesRead = system_fread(fd, p, buf->len)) > 0) && (n > 0)) {
           p += bytesRead;
           n =  n - bytesRead;
        }
        if (n != 0) {
          PERM_FREE(buf->fp);
          buf->fp = 0;
          FREE(buf);
          return NULL;
        }
    }
 
    return buf;
}

#endif /* XP_UNIX */

#ifdef XP_WIN32
NSAPI_PUBLIC filebuf_t *filebuf_open_nostat(SYS_FILE fd, int sz, 
                                          struct stat *finfo) 
{
    filebuf_t *buf = (filebuf_t *) MALLOC(sizeof(filebuf_t));

    buf->pos = 0;
    if (finfo) {
        buf->len = finfo->st_size;
        buf->cursize = finfo->st_size;
    } else {
    	buf->len = buf->cursize = 0;
    }
    buf->fd = fd;
   
    if(buf->len) {
        buf->fdmap = CreateFileMapping((HANDLE)PR_FileDesc2NativeHandle(fd), NULL, PAGE_READONLY, 0, 0, NULL);
        if(!buf->fdmap) {
            NsprError::mapWin32Error();
            return NULL;
        }
        if(!(buf->fp = (char *)MapViewOfFile(buf->fdmap, FILE_MAP_READ, 0, 0, 0))) {
            NsprError::mapWin32Error();
            return NULL;
        }
    } else {
        buf->fdmap = NULL;
        buf->fp = NULL;
    }
    buf->inbuf = NULL;
    buf->errmsg = NULL;

    return buf;
}
#endif /* XP_WIN32 */


NSAPI_PUBLIC filebuf_t *filebuf_open(SYS_FILE fd, int sz) 
{
    struct stat finfo;
    PRFileInfo pr_info;

    if (PR_GetOpenFileInfo(fd, &pr_info) != -1){
		finfo.st_mode = 0;
		finfo.st_size = pr_info.size;
		LL_L2I(finfo.st_ctime, pr_info.creationTime);
		LL_L2I(finfo.st_mtime, pr_info.modifyTime);
    
		return filebuf_open_nostat(fd, sz, &finfo);
	}else
		return NULL;
}


/* ----------------------------- buffer_close ----------------------------- */

#ifdef XP_UNIX

NSAPI_PUBLIC void filebuf_close(filebuf_t *buf) 
{
    if(buf->len) {
        if(munmap(buf->fp,buf->len) == -1) {
            NsprError::mapUnixErrno();
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_munmapFailedS_1), system_errmsg());
            return;
        }
    }
    if(system_fclose(buf->fd) == -1) {
        ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_closeFailedS_), system_errmsg());
    }
    FREE(buf);
}

NSAPI_PUBLIC void filebuf_close_nommap(filebuf_t *buf)
{
    if(buf->fp) {
        PERM_FREE(buf->fp);
        buf->fp = 0;
    }
    if(system_fclose(buf->fd) == -1) {
        ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_closeFailedS_), 
                system_errmsg());
    }
    FREE(buf);
}

#endif /* XP_UNIX */

#ifdef XP_WIN32

NSAPI_PUBLIC void filebuf_close(filebuf_t *buf) 
{
    if(buf->len) {
        UnmapViewOfFile(buf->fp);
        CloseHandle(buf->fdmap);
    }
    if(system_fclose(buf->fd) == -1) {
        ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_closeFailedS_), system_errmsg());
    }

    FREE(buf);
}

#endif /* XP_WIN32 */


/* ----------------------------- buffer_grab ------------------------------ */


NSAPI_PUBLIC int filebuf_grab(filebuf_t *buf, int sz) {
    register int nsz;

    if(buf->len == buf->pos)
        return IO_EOF;

    buf->inbuf = (unsigned char *)&buf->fp[buf->pos];

    nsz = buf->len - buf->pos;
    nsz = (nsz > sz ? sz : nsz);
    
    buf->cursize = nsz;
    buf->pos += nsz;

    return nsz;
}


/* ---------------------------- filebuf_buf2sd ---------------------------- */

/*
 * MOVED TO NETBUF.C TO AVOID ENTANGLEMENT IN ADMIN FORMS
 */
