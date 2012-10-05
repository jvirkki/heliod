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
 * ntpipe.c: Code for dealing with I/O buffers
 * 
 * See buffer.h for public documentation.
 *
 * 2/5/95 aruna
 * 
 */


#ifndef NS_OLDES3X
#include "windows.h"
#endif
#include "buffer.h"
#include "ereport.h"      /* ereport */
#include "prio.h"
#include "private/pprio.h"

#include "base/dbtbase.h"

#define PIPE_BUFFERSIZE 8192

/* ----------------------------- buffer_open ------------------------------ */

NSAPI_PUBLIC filebuf_t *pipebuf_open(SYS_FILE fd, int sz, struct stat *finfo) {
    filebuf_t *buf = (filebuf_t *) MALLOC(sizeof(filebuf_t));

    buf->pos = 0;
	if (finfo) {
    	buf->len = finfo->st_size;
		buf->cursize = finfo->st_size;
	} else {
    	buf->len = buf->cursize = 0;
	}
    buf->fd = fd;
    buf->inbuf = NULL;
    buf->errmsg = NULL;

    return buf;
}


NSAPI_PUBLIC void pipebuf_close(filebuf_t *buf) 
{
	SYS_FILE fd = buf->fd;

    HANDLE hpipe = (HANDLE) PR_FileDesc2NativeHandle(fd);
    BOOL RC = DisconnectNamedPipe(hpipe);

    PR_Close(fd);
	if(buf->inbuf)
		FREE(buf->inbuf);
    FREE(buf);
}

NSAPI_PUBLIC int pipebuf_next(filebuf_t *buf, int advance) {
    int n;

    if(!buf->inbuf)
        buf->inbuf = (unsigned char *) MALLOC(PIPE_BUFFERSIZE);

    switch(n = system_pread(buf->fd, (char *)(buf->inbuf), PIPE_BUFFERSIZE)) {
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
NSAPI_PUBLIC int pipebuf_grab(filebuf_t *buf, int sz) {
    int n;

    if(!buf->inbuf) {
        buf->inbuf = (unsigned char *) MALLOC(sz);
    }
    else if(sz > PIPE_BUFFERSIZE) {
        buf->inbuf = (unsigned char *) REALLOC(buf->inbuf, sz);
    }
    buf->cursize = 0;
    buf->pos = 0;
    
    switch((n = system_pread(buf->fd, (char *)(buf->inbuf), sz))) {
        case IO_EOF:
            buf->pos = 0;
            buf->cursize = 0;
            return IO_EOF;
        case IO_ERROR:
            buf->errmsg = system_errmsg();
            return IO_ERROR;
    }
    buf->cursize = n; 
    return n;
}

NSAPI_PUBLIC int pipebuf_buf2sd(filebuf_t *buf, SYS_NETFD sd, int len)
{
    register int n = len, t, ns;

    ns = 0;

    /* First, flush the current buffer */
    t = buf->cursize - buf->pos;

    if((n != -1) && (t > n))
        t = n;
    if((t) && (sd != SYS_NET_ERRORFD)) {
        if( net_write(sd, (char *)&buf->inbuf[buf->pos], t) == IO_ERROR) {
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
        t = PIPE_BUFFERSIZE;

    /* Now, keep blasting until done */
    while(1) {
        if(n != -1)
            t = (n < PIPE_BUFFERSIZE ? n : PIPE_BUFFERSIZE);
        
        switch(pipebuf_grab(buf, t)) {
          
          case IO_ERROR:
              ereport(LOG_FAILURE, XP_GetAdminStr(DBT_pipebufBuf2sdPipebufGrabIoErrorD_),GetLastError());
              return IO_ERROR;
          
          case IO_EOF:
	      if (buf->cursize) {
                  if(sd != SYS_NET_ERRORFD) {
                      if( net_write(sd, (char *)(buf->inbuf), buf->cursize) == IO_ERROR) {
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
              }
              if(n == -1) {
                  return ns;
              }
              else {
                  buf->errmsg = "premature EOF";
                  return IO_ERROR;
              }
              break;
          default:
              if(sd != SYS_NET_ERRORFD) {
                  if( net_write(sd, (char *)(buf->inbuf), buf->cursize) == IO_ERROR) {
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

NSAPI_PUBLIC int pipebuf_netbuf2sd(netbuf *buf, SYS_FILE sd, int len)
{
    register int n = len, t, ns;
    ns = 0;

    /* First, flush the current buffer */
    t = buf->cursize - buf->pos;

    if((n != -1) && (t > n))
        t = n;
    if((t) && ((int)sd != -1)) {
        if(system_fwrite(sd, (char *)&buf->inbuf[buf->pos], t) == IO_ERROR) {
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

        switch(netbuf_grab(buf, t)) {
          case IO_ERROR:
            return IO_ERROR;

          case IO_EOF:
            if(n == -1)
                return ns;
            else {
                buf->errmsg = "premature EOF";
                return IO_ERROR;
            }
          default:
            if((int)sd != -1) {
                if(system_fwrite(sd, (char *)(buf->inbuf), buf->cursize) == IO_ERROR) {
                    buf->errmsg = system_errmsg();
                    return IO_ERROR;
                }
                ns += buf->cursize;
            }
            if(n != -1) {
                n -= buf->cursize;
                if(!n) {
                    return ns;
				}
            }
            break;
        }
    }
}

NSAPI_PUBLIC int new_pipebuf_netbuf2pipe(netbuf *buf, SYS_FILE sd, int len, int& ns)
{
    register int n = len, t;
    ns = 0;

    /* First, flush the current buffer */
    t = buf->cursize - buf->pos;

    if((n != -1) && (t > n))
        t = n;
    if((t) && ((int)sd != -1)) {
        if(system_pwrite(sd, (char *)&buf->inbuf[buf->pos], t) == IO_ERROR) {
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

        switch(netbuf_grab(buf, t)) {
          case IO_ERROR:
            return IO_ERROR;

          case IO_EOF:
            if(n == -1)
                return ns;
            else {
                buf->errmsg = "premature EOF";
                return IO_ERROR;
            }
          default:
            if((int)sd != -1) {
                if(system_pwrite(sd, (char *)(buf->inbuf), buf->cursize) == IO_ERROR) {
                    buf->errmsg = system_errmsg();
                    return IO_ERROR;
                }
                ns += buf->cursize;
		buf->pos = buf->cursize;
            }
            if(n != -1) {
                n -= buf->cursize;
                if(!n) {
                    return ns;
				}
            }
            break;
        }
    }
}

NSAPI_PUBLIC int pipebuf_netbuf2pipe(netbuf *buf, SYS_FILE sd, int len)
{
    int ns;
    return new_pipebuf_netbuf2pipe(buf, sd, len, ns);
};
