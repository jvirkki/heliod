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

#ifndef BASE_BUFFER_H
#define BASE_BUFFER_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * buffer.h: For performing buffered I/O on a file or socket descriptor.
 * 
 * This is an abstraction to allow I/O to be performed regardless of the
 * current system. That way, an integer file descriptor can be used under 
 * UNIX but a stdio FILE structure could be used on systems which don't
 * support that or don't support it as efficiently.
 * 
 * Two abstractions are defined: A file buffer, and a network buffer. A
 * distinction is made so that mmap() can be used on files (but is not
 * required). Also, the file buffer takes a file name as the object to 
 * open instead of a file descriptor. A lot of the network buffering
 * is almost an exact duplicate of the non-mmap file buffering.
 * 
 * If an error occurs, system-independent means to obtain an error string
 * are also provided. However, if the underlying system is UNIX the error
 * may not be accurate in a threaded environment.
 * 
 * Rob McCool
 * 
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

#ifndef BASE_FILE_H
#include "file.h"
#endif /* !BASE_FILE_H */

#ifndef BASE_NET_H
#include "net.h"
#endif /* !BASE_NET_H */

#ifdef INTNSAPI

/* --- Begin function prototypes --- */

NSPR_BEGIN_EXTERN_C

/*
 * buffer_open opens a new buffer reading the specified file, with an I/O
 * buffer of size sz, and returns a new buffer structure which will hold
 * the data.
 *
 * This may return NULL.  If it does, check system_errmsg to get a message
 * about the error.
 */

NSAPI_PUBLIC filebuf_t *INTfilebuf_open(SYS_FILE fd, int sz);
NSAPI_PUBLIC netbuf *INTnetbuf_open(SYS_NETFD sd, int sz);

/*
 * filebuf_open_nostat is a convenience function for mmap() buffer opens,
 * if you happen to have the stat structure already.
 */

/*
 * filebuf_create is a convenience function if the file is already open
 * or mmap'd.  It creates a new filebuf for use with the mmap'd file.
 * If mmap_ptr is NULL, or MMAP is not supported on this system, it 
 * creates a buffer with buffer size bufsz.
 */
NSAPI_PUBLIC
filebuf_t *INTfilebuf_create(SYS_FILE fd, caddr_t mmap_ptr, int mmap_len, 
                             int bufsz);

/* 
 * filebuf_close_buffer is provided to cleanup a filebuf without closing
 * the underlying file.  If clean_mmap is 1, and the file is memory mapped,
 * the file will be unmapped.  If clean_mmap is 0, the file will not
 * be unmapped.
 */
NSAPI_PUBLIC void INTfilebuf_close_buffer(filebuf_t *buf, int clean_mmap);

NSAPI_PUBLIC 
filebuf_t *INTfilebuf_open_nostat(SYS_FILE fd, int sz, struct stat *finfo);
filebuf_t *INTfilebuf_open_nostat_nommap(SYS_FILE fd, int sz, struct stat *finfo);

#ifdef XP_WIN32
NSAPI_PUBLIC 
filebuf_t *INTpipebuf_open(SYS_FILE fd, int sz, struct stat *finfo);
#endif /* XP_WIN32 */

/*
 * buffer_next loads size more bytes into the given buffer and returns the
 * first one, or BUFFER_EOF on EOF and BUFFER_ERROR on error.
 */

NSAPI_PUBLIC int INTnetbuf_next(netbuf *buf, int advance);
#ifdef XP_WIN32 
NSAPI_PUBLIC int INTpipebuf_next(filebuf_t *buf, int advance);
#endif /* XP_WIN32 */

/*
 * netbuf_getbytes will read bytes from the netbuf into the user
 * supplied buffer.  Up to size bytes will be read.
 * If the call is successful, the number of bytes read is returned.  
 * NETBUF_EOF is returned when no more data will arrive on the socket,
 * and NETBUF_ERROR is returned in the event of an error.
 *
 */
NSAPI_PUBLIC int INTnetbuf_getbytes(netbuf *buf, char *buffer, int size);

/*
 * buffer_close deallocates a buffer and closes its associated files
 * (does not close a network socket).
 */

NSAPI_PUBLIC void INTfilebuf_close(filebuf_t *buf);
NSAPI_PUBLIC void INTfilebuf_close_nommap(filebuf_t *buf);
NSAPI_PUBLIC void INTnetbuf_close(netbuf *buf);
#ifdef XP_WIN32
NSAPI_PUBLIC void	INTpipebuf_close(filebuf_t *buf);
#endif /* XP_WIN32 */

/*
 * buffer_grab will set the buffer's inbuf array to an array of sz bytes 
 * from the buffer's associated object. It returns the number of bytes 
 * actually read (between 1 and sz). It returns IO_EOF upon EOF or IO_ERROR
 * upon error. The cursize entry of the structure will reflect the size
 * of the iobuf array.
 * 
 * The buffer will take care of allocation and deallocation of this array.
 */

NSAPI_PUBLIC int INTfilebuf_grab(filebuf_t *buf, int sz);
NSAPI_PUBLIC int INTnetbuf_grab(netbuf *buf, int sz);
#ifdef XP_WIN32
NSAPI_PUBLIC int INTpipebuf_grab(filebuf_t *buf, int sz);
#endif /* XP_WIN32 */


/*
 * INTnetbuf_buf2sd will send n bytes from the (probably previously read)
 * buffer and send them to sd. If sd is -1, they are discarded. If n is
 * -1, it will continue until EOF is recieved. Returns IO_ERROR on error
 * and the number of bytes sent any other time.
 */

NSAPI_PUBLIC int INTnetbuf_buf2sd(netbuf *buf, SYS_NETFD sd, int len);

/*
 * filebuf_buf2sd assumes that nothing has been read from the filebuf, 
 * and just sends the file out to the given socket. Returns IO_ERROR on error
 * and the number of bytes sent otherwise.
 *
 * Does not currently support you having read from the buffer previously. This
 * can be changed transparently.
 */

NSAPI_PUBLIC int INTfilebuf_buf2sd(filebuf_t *buf, SYS_NETFD sd);

#ifdef XP_WIN32

/*
 * NT pipe version of filebuf_buf2sd.
 */
NSAPI_PUBLIC int INTpipebuf_buf2sd(filebuf_t *buf, SYS_NETFD sd, int len);

/*
 * NT pipe version of INTnetbuf_buf2sd.
 */

NSAPI_PUBLIC int INTpipebuf_netbuf2sd(netbuf *buf, SYS_FILE sd, int len);
NSAPI_PUBLIC int INTpipebuf_netbuf2pipe(netbuf *buf, SYS_NETFD sd, int len);
#ifdef __cplusplus
NSAPI_PUBLIC int INTnew_pipebuf_netbuf2pipe(netbuf *buf, SYS_NETFD sd, int len, int& ns);
#endif
#endif /* XP_WIN32 */

/* added with 3.0 */
NSAPI_PUBLIC int INTnetbuf_buf2file(netbuf *buf, SYS_FILE sd, int len);

/* netbuf_replace : Internal function required by lib/streams/unchunk.cpp */
NSAPI_PUBLIC unsigned char * INTnetbuf_replace(netbuf *buf,
    unsigned char *inbuf, int pos, int cursize, int maxsize);

/* netbuf_buf2sd_timed: internal function required by lib/httpdaemon, cgi */
NSAPI_PUBLIC int INTnetbuf_buf2sd_timed(netbuf *buf,
                        SYS_NETFD sd, int len, PRIntervalTime timeout);
NSPR_END_EXTERN_C

#define filebuf_open_nostat INTfilebuf_open_nostat
#define filebuf_open_nostat_nommap INTfilebuf_open_nostat_nommap
#define filebuf_close_nommap INTfilebuf_close_nommap
#define filebuf_open INTfilebuf_open
#define filebuf_close INTfilebuf_close
#define filebuf_next INTfilebuf_next
#define filebuf_grab INTfilebuf_grab
#define filebuf_create INTfilebuf_create
#define filebuf_close_buffer INTfilebuf_close_buffer
#define filebuf_buf2sd INTfilebuf_buf2sd
#define netbuf_open INTnetbuf_open
#define netbuf_close INTnetbuf_close
#define netbuf_next INTnetbuf_next
#define netbuf_grab INTnetbuf_grab
#define netbuf_getbytes INTnetbuf_getbytes
#define netbuf_buf2sd INTnetbuf_buf2sd
#define netbuf_buf2file INTnetbuf_buf2file
#define netbuf_replace INTnetbuf_replace
#define netbuf_buf2sd_timed INTnetbuf_buf2sd_timed

#ifdef XP_WIN32
#define pipebuf_open INTpipebuf_open
#define pipebuf_close INTpipebuf_close
#define pipebuf_next INTpipebuf_next
#define pipebuf_grab INTpipebuf_grab
#define pipebuf_buf2sd INTpipebuf_buf2sd
#define pipebuf_netbuf2sd INTpipebuf_netbuf2sd
#define pipebuf_netbuf2pipe INTpipebuf_netbuf2pipe
#define new_pipebuf_netbuf2pipe INTnew_pipebuf_netbuf2pipe
#endif /* XP_WIN32 */

#endif /* INTNSAPI */

#endif /* !BASE_BUFFER_H */
