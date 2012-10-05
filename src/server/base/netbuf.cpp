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
 * netbuf.c: Handles buffered I/O on network sockets
 * 
 * Rob McCool
 */


#include "nspr.h"

#include "buffer.h"
#include "ereport.h"      /* ereport */


#define NETBUF_RDTIMEOUT 120


/* ----------------------------- netbuf_open ------------------------------ */


NSAPI_PUBLIC netbuf *netbuf_open(SYS_NETFD sd, int sz) {
    netbuf *buf = (netbuf *) MALLOC(sizeof(netbuf));

    buf->rdtimeout = NETBUF_RDTIMEOUT;
    buf->pos = sz;
    buf->cursize = sz; /* force buffer_next on first getc */
    buf->maxsize = sz;
    buf->sd = sd;

    buf->inbuf = NULL;
    buf->errmsg = NULL;

    return buf;
}


/* ----------------------------- netbuf_close ----------------------------- */


NSAPI_PUBLIC void netbuf_close(netbuf *buf) {
    if(buf->inbuf)
        FREE(buf->inbuf);
    FREE(buf);
}


/* ----------------------------- netbuf_replace --------------------------- */


NSAPI_PUBLIC unsigned char * netbuf_replace(netbuf *buf,
    unsigned char *inbuf, int pos, int cursize, int maxsize)
{
    unsigned char *oldbuf = buf->inbuf;

    buf->inbuf = inbuf;
    buf->pos = pos;
    buf->cursize = cursize;
    buf->maxsize = maxsize;

    return oldbuf;
}


/* ----------------------------- netbuf_next ------------------------------ */


NSAPI_PUBLIC int netbuf_next(netbuf *buf, int advance) {
    int n;

    if(!buf->inbuf)
        buf->inbuf = (unsigned char *) MALLOC(buf->maxsize);

    while(1) {
        switch(n = net_read(buf->sd,(char *)(buf->inbuf),buf->maxsize,buf->rdtimeout)) {
        case IO_EOF:
            return IO_EOF;
        case IO_ERROR:
            buf->errmsg = system_errmsg();
            return IO_ERROR;
        default:
            buf->pos = advance;
            buf->cursize = n;
            return buf->inbuf[0];
        }
    }
}

NSAPI_PUBLIC int netbuf_getbytes(netbuf *buf, char *buffer, int size)
{
    int bytes;

    if (!buf->inbuf) {
        buf->inbuf = (unsigned char *) MALLOC(buf->maxsize);
    } else {
        if (buf->pos < buf->cursize) {
            int bytes_in_buffer = buf->cursize - buf->pos;

            if (bytes_in_buffer > size)
                bytes_in_buffer = size;

            memcpy(buffer, &(buf->inbuf[buf->pos]), bytes_in_buffer);

            buf->pos += bytes_in_buffer;
            return bytes_in_buffer;
        }
    }

    /* The netbuf is empty.  Read data directly into the caller's buffer */
    bytes = net_read(buf->sd, buffer, size, buf->rdtimeout);
    if (bytes == 0)
        return NETBUF_EOF;
    if (bytes < 0) {
        buf->errmsg = system_errmsg();
        return NETBUF_ERROR;
    }
    return bytes;
}


/* ----------------------------- netbuf_grab ------------------------------ */


NSAPI_PUBLIC int netbuf_grab(netbuf *buf, int sz) {
    int n;

    if(!buf->inbuf) {
        buf->inbuf = (unsigned char *) MALLOC(sz);
        buf->maxsize = sz;
    }
    else if(sz > buf->maxsize) {
        buf->inbuf = (unsigned char *) REALLOC(buf->inbuf, sz);
        buf->maxsize = sz;
    }

    PR_ASSERT(buf->pos == buf->cursize);
    buf->pos = 0;
    buf->cursize = 0;

    while(1) {
        switch(n = net_read(buf->sd,(char *)(buf->inbuf),sz,buf->rdtimeout)) {
        case IO_EOF:
            return IO_EOF;
        case IO_ERROR: {
            buf->errmsg = system_errmsg();
            return IO_ERROR;
        }
        default:
            buf->cursize = n;
            return n;
        }
    }
}


/* ---------------------------- netbuf_buf2sd ----------------------------- */


NSAPI_PUBLIC int netbuf_buf2sd(netbuf *buf, SYS_NETFD sd, int len)
{
  return netbuf_buf2sd_timed(buf, sd, len, PR_INTERVAL_NO_TIMEOUT);  
}

/*
 * perform netbuf_buf2sd in a timely manner enforcing a timeout handling
 * the netbuf_grab() that can potentially receive just one char at a time
 * it can go on for a long time -- potentially leading to tying the thread 
 * resources to the request forever.
 *
 * timeout can be PR_INTERVAL_NO_TIMEOUT -- no timeout will be enforced.
 */
NSAPI_PUBLIC int netbuf_buf2sd_timed(netbuf *buf, 
                        SYS_NETFD sd, int len, PRIntervalTime timeout)
{
    register int n = len, t, ns;

    ns = 0;

    register PRIntervalTime start = 0;

    /* Note the starting time */
    if (timeout != PR_INTERVAL_NO_TIMEOUT)
        start = PR_IntervalNow();

    /* First, flush the current buffer */
    t = buf->cursize - buf->pos;

    if((n != -1) && (t > n))
        t = n;
    if((t) && (sd != SYS_NET_ERRORFD)) {
#if defined(OSF1) || defined(HPUX) || defined(SNI)
      OSF_label1:
#endif
        if(net_write(sd, (char *)&buf->inbuf[buf->pos], t) == IO_ERROR) {
#if defined(OSF1) || defined(HPUX) || defined(SNI)
            if(errno == EINTR) goto OSF_label1;
#endif
            buf->errmsg = system_errmsg();
            return IO_ERROR;
        }
        ns += t;
    }
    buf->pos += t;

    if(n != -1) {
        n -= t;
        if(!n)
            return ns;
    }
    else 
        t = buf->maxsize;

    /* Now, keep blasting until done */

    while(1) {
        /* Check to see if this client is taking too long to process */
        if (timeout != PR_INTERVAL_NO_TIMEOUT && 
                    PR_IntervalNow() > (start + timeout)) {
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
            buf->errmsg = system_errmsg();
            return IO_ERROR;
        }

        if(n != -1)
            t = (n < buf->maxsize ? n : buf->maxsize);

#if defined(OSF1) || defined(HPUX) || defined(SNI)
      OSF_label2:
#endif
        switch(netbuf_grab(buf, t)) {
          case IO_ERROR:
#if defined(OSF1) || defined(HPUX) || defined(SNI)
            if(errno == EINTR) goto OSF_label2;
#endif
            return IO_ERROR;
          case IO_EOF:
            if(n == -1)
                return ns;
            else {
                PR_SetError(PR_END_OF_FILE_ERROR, 0);
                buf->errmsg = system_errmsg();
                return IO_ERROR;
            }
          default:
            if(sd != SYS_NET_ERRORFD) {
#if defined(OSF1) || defined(HPUX)
              OSF_label3:
#endif
                if(net_write(sd, (char *)(buf->inbuf), buf->cursize) == IO_ERROR) {
#if defined(OSF1) || defined(HPUX)
                    if(errno == EINTR) goto OSF_label3;
#endif
                    buf->errmsg = system_errmsg();
                    return IO_ERROR;
                }
                buf->pos += buf->cursize;
                ns += buf->cursize;
            }
            if(n != -1) {
                n -= buf->cursize;
                if(!n)
                    return ns;
            }
            break;
        }
    }
}


NSAPI_PUBLIC int netbuf_buf2file(netbuf *buf, SYS_FILE sd, int len)
{
    register int n = len, t, ns;

    ns = 0;

    /* First, flush the current buffer */
    t = buf->cursize - buf->pos;

    if((n != -1) && (t > n))
        t = n;
    if((t) && (sd != SYS_NET_ERRORFD)) {
#if defined(OSF1) || defined(HPUX) || defined(SNI)
      OSF_label4:
#endif
        if(system_fwrite(sd, (char *)&buf->inbuf[buf->pos], t) == IO_ERROR) {
#if defined(OSF1) || defined(HPUX) || defined(SNI)
            if(errno == EINTR) goto OSF_label4;
#endif
            buf->errmsg = system_errmsg();
            return IO_ERROR;
        }
        ns += t;
    }
    buf->pos += t;

    if(n != -1) {
        n -= t;
        if(!n)
            return ns;
    }
    else 
        t = buf->maxsize;

    /* Now, keep blasting until done */

    while(1) {
        if(n != -1)
            t = (n < buf->maxsize ? n : buf->maxsize);

#if defined(OSF1) || defined(HPUX) || defined(SNI)
      OSF_label5:
#endif
        switch(netbuf_grab(buf, t)) {
          case IO_ERROR:
#if defined(OSF1) || defined(HPUX) || defined(SNI)
            if(errno == EINTR) goto OSF_label5;
#endif
            return IO_ERROR;
          case IO_EOF:
            if(n == -1)
                return ns;
            else {
                buf->errmsg = "premature EOF";
                return IO_ERROR;
            }
          default:
            if(sd != SYS_NET_ERRORFD) {
#if defined(OSF1) || defined(HPUX)
              OSF_label6:
#endif
                if(system_fwrite(sd, (char *)(buf->inbuf), buf->cursize) == IO_ERROR) {
#if defined(OSF1) || defined(HPUX)
                    if(errno == EINTR) goto OSF_label6;
#endif
                    buf->errmsg = system_errmsg();
                    return IO_ERROR;
                }
                ns += buf->cursize;
            }
            if(n != -1) {
                n -= buf->cursize;
                if(!n)
                    return ns;
            }
            break;
        }
    }
}



/* ---------------------------- buffer_buf2sd ----------------------------- */


NSAPI_PUBLIC int filebuf_buf2sd(filebuf_t *buf, SYS_NETFD sd)
{
#ifndef FILE_MMAP
    int n = 0;
    while(filebuf_grab(buf, NET_BUFFERSIZE) != IO_EOF) {
        if(net_write(sd, (char *)(buf->inbuf), buf->cursize) == IO_ERROR)
            return IO_ERROR;
        n += buf->cursize;
    }
    return n;
#else
#define CHUNKSIZE (64*1024)
    /* Write out in 64K chunks */
    int buflen = buf->len;
    char *bp = (char *)buf->fp;

    if (buf->pos > 0) {
        bp += buf->pos;
        buflen -= buf->pos;
    }

#ifdef IRIX
    /* let the system chunk it as necessary; don't force it */
    if (net_write(sd, bp, buflen) == IO_ERROR)
      return IO_ERROR;
#else /* IRIX */
    while (buflen > 0) {
        int bytes_written;
        int chunksize;

        if (buflen > CHUNKSIZE)
            chunksize = CHUNKSIZE;
        else
            chunksize = buflen;

        if((bytes_written = net_write(sd, bp, chunksize)) == IO_ERROR)
            return IO_ERROR;

        buflen -= bytes_written;
        bp += bytes_written;
    }
#endif /* IRIX */
    return buf->len;
#endif
}


