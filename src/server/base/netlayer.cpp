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
 * netlayer.cpp: NSPR IO layer useful for networking
 *
 * Chris Elving
 */

#include "nspr.h"
#include "base/net.h"

struct NetLayer {
    int pos;
    int cursize;
    int maxsize;
    char *inbuf;
};

extern "C" {
PRInt32 _netlayer_method_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
PRInt32 _netlayer_method_read(PRFileDesc *fd, void *buf, PRInt32 amount);
PRInt32 _netlayer_method_available(PRFileDesc *fd);
PRInt64 _netlayer_method_available64(PRFileDesc *fd);
PROffset32 _netlayer_method_seek(PRFileDesc *fd, PROffset32 offset, PRSeekWhence how);
PROffset64 _netlayer_method_seek64(PRFileDesc *fd, PROffset64 offset, PRSeekWhence how);
PRInt32 _netlayer_method_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout);
PRInt16 _netlayer_method_poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags);
PRInt32 _netlayer_method_acceptread(PRFileDesc *fd, PRFileDesc **nd, PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime t);
PRStatus _netlayer_method_close(PRFileDesc *fd);
}

static PRDescIdentity _netlayer_identity = PR_INVALID_IO_LAYER;
static PRIOMethods _netlayer_methods;
static PRCloseFN _netlayer_default_close_method;


/* -------------------------- _netlayer_retrieve -------------------------- */

static inline PRInt32 _netlayer_retrieve(NetLayer *nl, void *buf, PRInt32 amount, PRBool peek)
{
    PRInt32 available = nl->cursize - nl->pos;

    if (amount > available)
        amount = available;

    memcpy(buf, nl->inbuf + nl->pos, amount);

    if (!peek)
        nl->pos += amount;

    return amount;
}


/* ------------------------ _netlayer_method_recv ------------------------- */

PRInt32 _netlayer_method_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    PRBool peek;
    switch (flags) {
    case 0:
        peek = PR_FALSE;
        break;
    case PR_MSG_PEEK:
        peek = PR_TRUE;
        break;
    default:
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
        
    NetLayer *nl = (NetLayer *) fd->secret;

    if (nl->pos >= nl->cursize) {
        if (!peek && amount >= nl->maxsize)
            return fd->lower->methods->recv(fd->lower, buf, amount, 0, timeout);

        PRInt32 nrecv = fd->lower->methods->recv(fd->lower, nl->inbuf, nl->maxsize, 0, timeout);
        if (nrecv == -1)
            return -1;

        nl->cursize = nrecv;
        nl->pos = 0;
    }

    return _netlayer_retrieve(nl, buf, amount, peek);
}


/* ------------------------ _netlayer_method_read ------------------------- */

PRInt32 _netlayer_method_read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    NetLayer *nl = (NetLayer *) fd->secret;

    if (nl->pos >= nl->cursize) {
        if (amount >= nl->maxsize)
            return fd->lower->methods->read(fd->lower, buf, amount);

        PRInt32 nread = fd->lower->methods->read(fd->lower, nl->inbuf, nl->maxsize);
        if (nread == -1)
            return -1;

        nl->cursize = nread;
        nl->pos = 0;
    }

    return _netlayer_retrieve(nl, buf, amount, PR_FALSE);
}


/* ---------------------- _netlayer_method_available ---------------------- */

PRInt32 _netlayer_method_available(PRFileDesc *fd)
{
    PRInt32 rv = fd->lower->methods->available(fd->lower);

    if (rv == -1 && PR_GetError() == PR_INVALID_METHOD_ERROR)
        rv = 0;

    if (rv != -1) {
        NetLayer *nl = (NetLayer *) fd->secret;
        PRInt32 sum = rv + nl->cursize - nl->pos;
        if (rv < sum)
            rv = sum;
    }

    return rv;
}


/* --------------------- _netlayer_method_available64 --------------------- */

PRInt64 _netlayer_method_available64(PRFileDesc *fd)
{
    PRInt64 rv = fd->lower->methods->available64(fd->lower);

    if (rv == -1 && PR_GetError() == PR_INVALID_METHOD_ERROR)
        rv = 0;

    if (rv != -1) {
        NetLayer *nl = (NetLayer *) fd->secret;
        PRInt64 sum = rv + nl->cursize - nl->pos;
        if (rv < sum)
            rv = sum;
    }

    return rv;
}


/* ------------------------ _netlayer_method_seek ------------------------- */

PROffset32 _netlayer_method_seek(PRFileDesc *fd, PROffset32 offset, PRSeekWhence how)
{
    PROffset32 rv = fd->lower->methods->seek(fd->lower, offset, how);

    if (offset != 0 || how != PR_SEEK_CUR) {
        NetLayer *nl = (NetLayer *) fd->secret;
        nl->cursize = 0;
        nl->pos = 0;
    }

    return rv;
}


/* ----------------------- _netlayer_method_seek64 ------------------------ */

PROffset64 _netlayer_method_seek64(PRFileDesc *fd, PROffset64 offset, PRSeekWhence how)
{
    PROffset64 rv = fd->lower->methods->seek64(fd->lower, offset, how);

    if (offset != 0 || how != PR_SEEK_CUR) {
        NetLayer *nl = (NetLayer *) fd->secret;
        nl->cursize = 0;
        nl->pos = 0;
    }

    return rv;
}


/* ---------------------- _netlayer_method_recvfrom ----------------------- */

PRInt32 _netlayer_method_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}


/* ------------------------ _netlayer_method_poll ------------------------- */

PRInt16 _netlayer_method_poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    NetLayer *nl = (NetLayer *) fd->secret;

    if (in_flags & PR_POLL_READ) {
        if (nl->cursize > nl->pos) {
            *out_flags = PR_POLL_READ;
            return in_flags;
        }
    }

    return fd->lower->methods->poll(fd->lower, in_flags, out_flags);
}


/* --------------------- _netlayer_method_acceptread ---------------------- */

PRInt32 _netlayer_method_acceptread(PRFileDesc *fd, PRFileDesc **nd, PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime t)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}


/* ------------------------ _netlayer_method_close ------------------------ */

PRStatus _netlayer_method_close(PRFileDesc *fd)
{
    NetLayer *nl = (NetLayer *) fd->secret;
    fd->secret = NULL;

    PRStatus rv = _netlayer_default_close_method(fd);

    /*
     * Because an uncancelled IO might have been pending on a Win32 IO
     * completion port, we can't free nl->inbuf until after the socket has
     * been closed
     */
    PERM_FREE(nl->inbuf);
    PERM_FREE(nl);

    return rv;
}


/* ---------------------------- _netlayer_find ---------------------------- */

static NetLayer *_netlayer_find(PRFileDesc *fd)
{
    while (fd) {
        if (fd->identity == _netlayer_identity)
            return (NetLayer *) fd->secret;
        fd = fd->lower;
    }

    return NULL;
}


/* --------------------------- net_buffer_input --------------------------- */

NSAPI_PUBLIC int INTnet_buffer_input(SYS_NETFD sd, int sz)
{
    NetLayer *nl = _netlayer_find(sd);
    if (nl) {
        if (sz > nl->maxsize) {
            void *inbuf = PERM_REALLOC(nl->inbuf, sz);
            if (!inbuf)
                return -1;

            nl->inbuf = (char *) inbuf;
        }

        nl->maxsize = sz;

        return 0;
    }

    nl = (NetLayer *) PERM_MALLOC(sizeof(NetLayer));
    if (!nl)
        return -1;

    nl->pos = 0;
    nl->cursize = 0;
    nl->maxsize = sz;
    nl->inbuf = (char *) PERM_MALLOC(sz);
    if (!nl->inbuf) {
        PERM_FREE(nl);
        return -1;
    }

    PRFileDesc *layer = PR_CreateIOLayerStub(_netlayer_identity, &_netlayer_methods);
    if (!layer) {
        PERM_FREE(nl->inbuf);
        PERM_FREE(nl);
        return -1;
    }

    layer->secret = (PRFilePrivate *) nl;

    PRStatus rv = PR_PushIOLayer(sd, PR_NSPR_IO_LAYER, layer);
    if (rv != PR_SUCCESS) {
        PR_Close(layer);
        return -1;
    }

    return 0;
}


/* ------------------------------- net_peek ------------------------------- */

NSAPI_PUBLIC int INTnet_peek(SYS_NETFD sd, void *buf, int sz, int timeout)
{
    PRIntervalTime interval = net_nsapi_timeout_to_nspr_interval(timeout);
    return sd->methods->recv(sd, buf, sz, PR_MSG_PEEK, interval);
}


/* ------------------------------- net_init ------------------------------- */

NSAPI_PUBLIC int net_init(int security_on)
{
    _netlayer_identity = PR_GetUniqueIdentity("netlayer/" PRODUCT_FULL_VERSION_ID);

    const PRIOMethods *default_methods = PR_GetDefaultIOMethods();

    _netlayer_methods = *default_methods;
    _netlayer_methods.recv = _netlayer_method_recv;
    _netlayer_methods.read = _netlayer_method_read;
    _netlayer_methods.available = _netlayer_method_available;
    _netlayer_methods.available64 = _netlayer_method_available64;
    _netlayer_methods.seek = _netlayer_method_seek;
    _netlayer_methods.seek64 = _netlayer_method_seek64;
    _netlayer_methods.recvfrom = _netlayer_method_recvfrom;
    _netlayer_methods.poll = _netlayer_method_poll;
    _netlayer_methods.acceptread = _netlayer_method_acceptread;
    _netlayer_methods.close = _netlayer_method_close;

    _netlayer_default_close_method = default_methods->close;

    return 0;
}
