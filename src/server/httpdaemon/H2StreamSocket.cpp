/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright (c) 2026, The Heliod Web Server Project. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "H2StreamSocket.h"
#include "H2Stream.h"
#include "base/net.h"
#include "private/pprio.h"

#include <cstring>
#include <cerrno>

#ifdef XP_UNIX
#include <unistd.h>
#endif

/*
 * A stream socket is the *bottom* of the filter stack, and it has no lower
 * layer: PR_CreateIOLayerStub() leaves fd->lower NULL.  PR_GetDefaultIOMethods()
 * does not return no-op methods, it returns *layered* ones -- every default
 * delegates to fd->lower.  So any method left at its default dereferences NULL
 * the first time something calls it.
 *
 * That is not a theoretical hazard.  HttpFilterContext::writevRaw() uses send()
 * when the vector has one element and writev() when it has more, so small
 * responses always worked and anything that took two iovecs crashed the worker;
 * NSFC_PR_SendFile() calls sendfile() for every static file with a non-zero
 * length.  Both were reached by ordinary requests.
 *
 * Everything the response path can reach is therefore implemented here, and
 * everything else is stubbed to fail, so that a caller which finds a new way
 * down here gets an error it can report rather than a signal.
 */

extern "C" {

static PRInt32 h2ss_read(PRFileDesc* fd, void* buf, PRInt32 amount)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream)
        return -1;
    return stream->readRequestBody(buf, amount);
}

static PRInt32 h2ss_recv(PRFileDesc* fd,
                         void* buf,
                         PRInt32 amount,
                         PRIntn /*flags*/,
                         PRIntervalTime /*timeout*/)
{
    return h2ss_read(fd, buf, amount);
}

static PRInt32 h2ss_write(PRFileDesc* fd, const void* buf, PRInt32 amount)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream)
        return -1;
    return stream->writeResponseBody(buf, amount);
}

static PRInt32 h2ss_send(PRFileDesc* fd,
                         const void* buf,
                         PRInt32 amount,
                         PRIntn /*flags*/,
                         PRIntervalTime /*timeout*/)
{
    return h2ss_write(fd, buf, amount);
}

static PRInt32 h2ss_writev(PRFileDesc* fd,
                           const PRIOVec* iov,
                           PRInt32 iov_size,
                           PRIntervalTime /*timeout*/)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream || (iov_size > 0 && !iov))
        return -1;

    // All-or-nothing, like PR_Writev on a blocking socket -- and like
    // writeResponseBody(), which either takes the whole buffer or fails.  A
    // short return would be read as "this many bytes were sent" by
    // HttpFilterContext::writev(), which would then carry on from the wrong
    // place; failing outright puts the pipeline into its existing error path.
    PRInt32 total = 0;
    for (PRInt32 i = 0; i < iov_size; i++) {
        if (iov[i].iov_len <= 0)
            continue;
        if (stream->writeResponseBody(iov[i].iov_base, iov[i].iov_len)
                != iov[i].iov_len)
            return -1;
        total += iov[i].iov_len;
    }
    return total;
}

/*
 * How much file data the sendfile/transmitfile shims move at a time.  Matched to
 * the response ring downstream: the whole point of this path is that a response
 * does not cost memory proportional to its size, so a shim that read a 64 MB
 * file into one buffer to hand it over would reintroduce exactly the buffering
 * this code exists to avoid.  A larger buffer would also only delay the first
 * DATA frame, since the ring cannot take more than this anyway.
 */
static const PRInt32 H2SS_FILE_CHUNK = 32 * 1024;

/*
 * Read from `file` at an absolute offset, without touching its file position.
 *
 * The positional part is not a nicety.  The file cache hands out shared, cached
 * descriptors -- NSFC_PR_SendFile() passes an explicit file_offset and never
 * seeks, precisely because of that -- so seeking here would corrupt the reads of
 * every other thread sending the same file.
 */
static PRInt32 h2ss_pread(PRFileDesc* file, void* buf, PRInt32 len,
                          PRInt64 offset)
{
#ifdef XP_UNIX
    PROsfd osfd = PR_FileDesc2NativeHandle(file);
    if (osfd < 0) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return -1;
    }

    ssize_t got;
    do {
        got = pread((int)osfd, buf, (size_t)len, (off_t)offset);
    } while (got < 0 && errno == EINTR);

    if (got < 0) {
        PR_SetError(PR_IO_ERROR, errno);
        return -1;
    }
    return (PRInt32)got;
#else
    /*
     * No pread() outside Unix.  Seeking is not an acceptable substitute for the
     * reason above, and safs/service.cpp has no fallback for a failed transmit,
     * so a non-Unix port needs a positional read here (ReadFile with an
     * OVERLAPPED offset on Windows) before HTTP/2 can serve large static files.
     * Failing is still strictly better than what the layered default did, which
     * was to crash the worker process.
     */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
#endif
}

/**
 * Copy `nbytes` of `file`, starting at `offset`, into the stream's response.
 * Returns the number of bytes handed over, or -1.
 */
static PRInt64 h2ss_sendFileData(heliod::H2Stream* stream,
                                 PRFileDesc* file,
                                 PRInt64 offset,
                                 PRInt64 nbytes)
{
    if (!file)
        return -1;

    // PRSendFileData: file_nbytes of 0 means "from file_offset to end of file".
    if (nbytes == 0) {
        PRFileInfo64 info;
        if (PR_GetOpenFileInfo64(file, &info) != PR_SUCCESS)
            return -1;
        nbytes = info.size - offset;
        if (nbytes <= 0)
            return 0;
    }

    // One buffer per call, not per chunk, and off the stack: worker stacks are
    // sized for the SAF pipeline, not for a 32 KB frame added underneath it.
    void* buf = PR_Malloc(H2SS_FILE_CHUNK);
    if (!buf) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return -1;
    }

    PRInt64 sent = 0;
    while (sent < nbytes) {
        PRInt64 remain = nbytes - sent;
        PRInt32 want = (remain > H2SS_FILE_CHUNK) ? H2SS_FILE_CHUNK
                                                  : (PRInt32)remain;

        PRInt32 got = h2ss_pread(file, buf, want, offset + sent);
        if (got <= 0) {
            // Fewer bytes than the caller asked for means the file changed
            // underneath the cache, or the read failed.  Either way the response
            // no longer matches its content-length, so fail rather than deliver
            // a truncated body and report success.
            if (got == 0)
                PR_SetError(PR_END_OF_FILE_ERROR, 0);
            PR_Free(buf);
            return -1;
        }

        // This is where back-pressure comes from: the write blocks once the ring
        // is full and does not return until the client has taken delivery.
        if (stream->writeResponseBody(buf, got) != got) {
            PR_Free(buf);
            return -1;
        }
        sent += got;
    }

    PR_Free(buf);
    return sent;
}

static PRInt32 h2ss_sendfile(PRFileDesc* fd,
                             PRSendFileData* sfd,
                             PRTransmitFileFlags flags,
                             PRIntervalTime /*timeout*/)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream || !sfd)
        return -1;

    PRInt64 total = 0;

    if (sfd->header && sfd->hlen > 0) {
        if (stream->writeResponseBody(sfd->header, sfd->hlen) != sfd->hlen)
            return -1;
        total += sfd->hlen;
    }

    PRInt64 body = h2ss_sendFileData(stream, sfd->fd,
                                     (PRInt64)sfd->file_offset,
                                     (PRInt64)sfd->file_nbytes);
    if (body < 0)
        return -1;
    total += body;

    if (sfd->trailer && sfd->tlen > 0) {
        if (stream->writeResponseBody(sfd->trailer, sfd->tlen) != sfd->tlen)
            return -1;
        total += sfd->tlen;
    }

    // The flag is about the socket, not the file: PR_TRANSMITFILE_CLOSE_SOCKET
    // means the transport is finished afterwards, which for a stream means the
    // response is complete.  The file belongs to the caller either way -- the
    // file cache in particular shares and reuses it.
    if (flags == PR_TRANSMITFILE_CLOSE_SOCKET)
        stream->markResponseClosed();

    // Callers cap a single send well below 2 GB -- see the asserts in
    // NSFC_PR_SendFile(), and HttpFilterContext::sendfile(), which refuses
    // anything over INT_MAX -- so the narrowing here cannot lose anything.
    return (PRInt32)total;
}

static PRInt32 h2ss_transmitfile(PRFileDesc* sd,
                                 PRFileDesc* file,
                                 const void* headers,
                                 PRInt32 hlen,
                                 PRTransmitFileFlags flags,
                                 PRIntervalTime timeout)
{
    // PR_TransmitFile is PR_SendFile over the whole file with no trailer.
    PRSendFileData sfd;
    sfd.fd = file;
    sfd.file_offset = 0;
    sfd.file_nbytes = 0;        // to end of file
    sfd.header = headers;
    sfd.hlen = hlen;
    sfd.trailer = NULL;
    sfd.tlen = 0;

    return h2ss_sendfile(sd, &sfd, flags, timeout);
}

static PRStatus h2ss_close(PRFileDesc* fd)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (stream)
        stream->markResponseClosed();
    fd->secret = NULL;
    // Do not call PR_Free on the descriptor; NSPR will clean up the stub.
    return PR_SUCCESS;
}

static PRInt16 h2ss_poll(PRFileDesc* fd, PRInt16 in_flags, PRInt16* out_flags)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    *out_flags = 0;
    if (!stream)
        return in_flags;
    if ((in_flags & PR_POLL_READ) && stream->requestBodyAvailable() > 0)
        *out_flags |= PR_POLL_READ;
    if (in_flags & PR_POLL_WRITE)
        *out_flags |= PR_POLL_WRITE;
    return *out_flags;
}

static PRInt32 h2ss_available(PRFileDesc* fd)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream)
        return -1;
    return stream->requestBodyAvailable();
}

static PRInt64 h2ss_available64(PRFileDesc* fd)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream)
        return -1;
    return (PRInt64)stream->requestBodyAvailable();
}

static PRStatus h2ss_fsync(PRFileDesc* /*fd*/)
{
    // Nothing to force: the write path already pushes each response write toward
    // the wire, and HttpFilterContext::flush() does not propagate fsync down the
    // stack anyway (it used to, and was changed for an NSS assertion).
    return PR_SUCCESS;
}

static PRStatus h2ss_shutdown(PRFileDesc* fd, PRIntn how)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream)
        return PR_FAILURE;

    // Shutting down the send side is the socket way of saying the response is
    // finished, which is what close() means here too.
    if (how == PR_SHUTDOWN_SEND || how == PR_SHUTDOWN_BOTH)
        stream->markResponseClosed();
    return PR_SUCCESS;
}

//
// Methods a stream socket has no meaning for.  These exist only so that nothing
// is left pointing at a layered default; see the note at the top of the file.
//

static PROffset32 h2ss_seek(PRFileDesc* /*fd*/, PROffset32 /*offset*/,
                           PRSeekWhence /*how*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PROffset64 h2ss_seek64(PRFileDesc* /*fd*/, PROffset64 /*offset*/,
                             PRSeekWhence /*how*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRStatus h2ss_fileInfo(PRFileDesc* /*fd*/, PRFileInfo* /*info*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRStatus h2ss_fileInfo64(PRFileDesc* /*fd*/, PRFileInfo64* /*info*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRStatus h2ss_connect(PRFileDesc* /*fd*/, const PRNetAddr* /*addr*/,
                             PRIntervalTime /*timeout*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRStatus h2ss_connectcontinue(PRFileDesc* /*fd*/, PRInt16 /*out_flags*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRFileDesc* h2ss_accept(PRFileDesc* /*fd*/, PRNetAddr* /*addr*/,
                               PRIntervalTime /*timeout*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return NULL;
}

static PRStatus h2ss_bind(PRFileDesc* /*fd*/, const PRNetAddr* /*addr*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRStatus h2ss_listen(PRFileDesc* /*fd*/, PRIntn /*backlog*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRInt32 h2ss_recvfrom(PRFileDesc* /*fd*/, void* /*buf*/,
                             PRInt32 /*amount*/, PRIntn /*flags*/,
                             PRNetAddr* /*addr*/, PRIntervalTime /*timeout*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRInt32 h2ss_sendto(PRFileDesc* /*fd*/, const void* /*buf*/,
                           PRInt32 /*amount*/, PRIntn /*flags*/,
                           const PRNetAddr* /*addr*/,
                           PRIntervalTime /*timeout*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRInt32 h2ss_acceptread(PRFileDesc* /*sd*/, PRFileDesc** /*nd*/,
                               PRNetAddr** /*raddr*/, void* /*buf*/,
                               PRInt32 /*amount*/, PRIntervalTime /*t*/)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRStatus h2ss_getpeername(PRFileDesc* fd, PRNetAddr* addr)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream || !addr)
        return PR_FAILURE;
    return stream->getRemoteAddress(addr);
}

static PRStatus h2ss_getsockname(PRFileDesc* fd, PRNetAddr* addr)
{
    heliod::H2Stream* stream = reinterpret_cast<heliod::H2Stream*>(fd->secret);
    if (!stream || !addr)
        return PR_FAILURE;
    return stream->getLocalAddress(addr);
}

static PRStatus h2ss_getsocketoption(PRFileDesc* /*fd*/,
                                     PRSocketOptionData* data)
{
    if (!data)
        return PR_FAILURE;
    switch (data->option) {
    case PR_SockOpt_Nonblocking:
        data->value.non_blocking = PR_TRUE;
        break;
    case PR_SockOpt_NoDelay:
        data->value.no_delay = PR_TRUE;
        break;
    case PR_SockOpt_Reuseaddr:
        data->value.reuse_addr = PR_FALSE;
        break;
    case PR_SockOpt_Keepalive:
        data->value.keep_alive = PR_FALSE;
        break;
    case PR_SockOpt_Linger:
        data->value.linger.polarity = PR_FALSE;
        data->value.linger.linger = 0;
        break;
    default:
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

static PRStatus h2ss_setsocketoption(PRFileDesc* /*fd*/,
                                     const PRSocketOptionData* data)
{
    if (!data)
        return PR_FAILURE;
    // Ignore socket options on the synthetic stream socket.
    return PR_SUCCESS;
}

} // extern "C"

namespace heliod {

PRDescIdentity H2StreamSocket::identity_ = PR_INVALID_IO_LAYER;
PRIOMethods H2StreamSocket::methods_;
PRBool H2StreamSocket::initialized_ = PR_FALSE;

PRStatus H2StreamSocket::initialize()
{
    if (initialized_)
        return PR_SUCCESS;

    identity_ = PR_GetUniqueIdentity("heliod-h2-stream");
    if (identity_ == PR_INVALID_IO_LAYER)
        return PR_FAILURE;

    // Start from the defaults for file_type and the reserved slots only.  Every
    // callable method is replaced below, because the defaults are layered and
    // this descriptor has no lower layer to delegate to -- see the note at the
    // top of this file.  Nothing here may be left out.
    const PRIOMethods* defaults = PR_GetDefaultIOMethods();
    methods_ = *defaults;

    // The response path.
    methods_.write = h2ss_write;
    methods_.send = h2ss_send;
    methods_.writev = h2ss_writev;
    methods_.sendfile = h2ss_sendfile;
    methods_.transmitfile = h2ss_transmitfile;

    // The request path.
    methods_.read = h2ss_read;
    methods_.recv = h2ss_recv;
    methods_.available = h2ss_available;
    methods_.available64 = h2ss_available64;

    // Lifecycle and introspection.
    methods_.close = h2ss_close;
    methods_.shutdown = h2ss_shutdown;
    methods_.fsync = h2ss_fsync;
    methods_.poll = h2ss_poll;
    methods_.getpeername = h2ss_getpeername;
    methods_.getsockname = h2ss_getsockname;
    methods_.getsocketoption = h2ss_getsocketoption;
    methods_.setsocketoption = h2ss_setsocketoption;

    // Meaningless for a stream socket: fail rather than crash.
    methods_.seek = h2ss_seek;
    methods_.seek64 = h2ss_seek64;
    methods_.fileInfo = h2ss_fileInfo;
    methods_.fileInfo64 = h2ss_fileInfo64;
    methods_.connect = h2ss_connect;
    methods_.connectcontinue = h2ss_connectcontinue;
    methods_.accept = h2ss_accept;
    methods_.bind = h2ss_bind;
    methods_.listen = h2ss_listen;
    methods_.recvfrom = h2ss_recvfrom;
    methods_.sendto = h2ss_sendto;
    methods_.acceptread = h2ss_acceptread;

    initialized_ = PR_TRUE;
    return PR_SUCCESS;
}

PRFileDesc* H2StreamSocket::create(H2Stream* stream)
{
    if (initialize() != PR_SUCCESS)
        return NULL;

    PRFileDesc* fd = PR_CreateIOLayerStub(identity_, &methods_);
    if (!fd)
        return NULL;

    fd->secret = reinterpret_cast<PRFilePrivate*>(stream);
    return fd;
}

H2Stream* H2StreamSocket::fromFD(PRFileDesc* fd)
{
    if (!initialized_ || !fd)
        return NULL;

    PRFileDesc* layer = PR_GetIdentitiesLayer(fd, identity_);
    if (!layer)
        return NULL;

    return reinterpret_cast<H2Stream*>(layer->secret);
}

} // namespace heliod
