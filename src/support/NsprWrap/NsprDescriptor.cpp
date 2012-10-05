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

#include <string.h>

#include "NsprDescriptor.h"

//-----------------------------------------------------------------------------
// _initialize
//-----------------------------------------------------------------------------

static PRCallOnceType _once;
static PRDescIdentity _identity = -1;

NSPR_BEGIN_EXTERN_C

static PRStatus PR_CALLBACK _initialize(void)
{
    _identity = PR_GetUniqueIdentity("NsprDescriptor-$Revision: 1.1.2.1 $");
    PR_ASSERT(_identity != -1);
    return PR_SUCCESS;
}

NSPR_END_EXTERN_C

//-----------------------------------------------------------------------------
// _verify
//-----------------------------------------------------------------------------

static inline PRBool _verify(PRFileDesc *fd)
{
    PR_ASSERT(fd->identity == _identity);
    if (fd->identity != _identity) {
        PR_SetError(PR_INVALID_METHOD_ERROR, 0);
        return PR_FALSE;
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// _convert
//-----------------------------------------------------------------------------

static inline PRStatus _convert(PRFileInfo *info, PRFileInfo64 *info64)
{
    PRStatus rv = PR_SUCCESS;
    info->type = info64->type;
    info->size = info64->size;
    if (info->size < info64->size) {
        info->size = 0x7fffffff;
        PR_SetError(PR_FILE_TOO_BIG_ERROR, 0);
        rv = PR_FAILURE;
    }
    info->creationTime = info64->creationTime;
    info->modifyTime = info64->modifyTime;
    return rv;
}

//-----------------------------------------------------------------------------
// C-to-C++ adapters for NSPR IO methods
//-----------------------------------------------------------------------------

NSPR_BEGIN_EXTERN_C

static PRStatus PR_CALLBACK _nsprdescriptor_close(PRFileDesc *fd)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->close();
}

static PRInt32 PR_CALLBACK _nsprdescriptor_read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->read(buf, amount);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->write(buf, amount);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_available(PRFileDesc *fd)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->available();
}

static PRInt64 PR_CALLBACK _nsprdescriptor_available64(PRFileDesc *fd)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->available();
}

static PRStatus PR_CALLBACK _nsprdescriptor_fsync(PRFileDesc *fd)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->fsync();
}

static PROffset32 PR_CALLBACK _nsprdescriptor_seek(PRFileDesc *fd, PROffset32 offset, PRSeekWhence how)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->seek(offset, how);
}

static PROffset64 PR_CALLBACK _nsprdescriptor_seek64(PRFileDesc *fd, PROffset64 offset, PRSeekWhence how)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->seek(offset, how);
}

static PRStatus PR_CALLBACK _nsprdescriptor_fileinfo(PRFileDesc *fd, PRFileInfo *info)
{
    if (!_verify(fd)) return PR_FAILURE;

    PRFileInfo64 info64;
    PRStatus rv;

    rv = ((NsprDescriptor*)fd->secret)->fileinfo(&info64);
    if (rv == PR_SUCCESS) {
        rv = _convert(info, &info64);
    }

    return rv;
}

static PRStatus PR_CALLBACK _nsprdescriptor_fileinfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->fileinfo(info);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_writev(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->writev(iov, iov_size, timeout);
}

static PRStatus PR_CALLBACK _nsprdescriptor_connect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->connect(addr, timeout);
}

static PRFileDesc* PR_CALLBACK _nsprdescriptor_accept(PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    if (!_verify(fd)) return NULL;
    return ((NsprDescriptor*)fd->secret)->accept(addr, timeout);
}

static PRStatus PR_CALLBACK _nsprdescriptor_bind(PRFileDesc *fd, const PRNetAddr *addr)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->bind(addr);
}

static PRStatus PR_CALLBACK _nsprdescriptor_listen(PRFileDesc *fd, PRIntn backlog)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->listen(backlog);
}

static PRStatus PR_CALLBACK _nsprdescriptor_shutdown(PRFileDesc *fd, PRIntn how)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->shutdown(how);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->recv(buf, amount, flags, timeout);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_send(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->send(buf, amount, flags, timeout);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->recvfrom(buf, amount, flags, addr, timeout);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_sendto(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->sendto(buf, amount, flags, addr, timeout);
}

static PRInt16 PR_CALLBACK _nsprdescriptor_poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    if (!_verify(fd)) return -1;
    return ((NsprDescriptor*)fd->secret)->poll(in_flags, out_flags);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_transmitfile(PRFileDesc *sd, PRFileDesc *fd, const void *headers, PRInt32 hlen, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    if (!_verify(sd)) return -1;
    return ((NsprDescriptor*)sd->secret)->transmitfile(fd, headers, hlen, flags, timeout);
}

static PRStatus PR_CALLBACK _nsprdescriptor_getsockname(PRFileDesc *fd, PRNetAddr *addr)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->getsockname(addr);
}

static PRStatus PR_CALLBACK _nsprdescriptor_getpeername(PRFileDesc *fd, PRNetAddr *addr)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->getpeername(addr);
}

static PRStatus PR_CALLBACK _nsprdescriptor_getsocketoption(PRFileDesc *fd, PRSocketOptionData *data)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->getsocketoption(data);
}

static PRStatus PR_CALLBACK _nsprdescriptor_setsocketoption(PRFileDesc *fd, const PRSocketOptionData *data)
{
    if (!_verify(fd)) return PR_FAILURE;
    return ((NsprDescriptor*)fd->secret)->setsocketoption(data);
}

static PRInt32 PR_CALLBACK _nsprdescriptor_sendfile(PRFileDesc *sd, PRSendFileData *sendData, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    if (!_verify(sd)) return -1;
    return ((NsprDescriptor*)sd->secret)->sendfile(sendData, flags, timeout);
}

NSPR_END_EXTERN_C

//-----------------------------------------------------------------------------
// _nsprdescriptor_methods
//-----------------------------------------------------------------------------

static PRIOMethods _methods = { PR_DESC_FILE,
                                &_nsprdescriptor_close,
                                &_nsprdescriptor_read,
                                &_nsprdescriptor_write,
                                &_nsprdescriptor_available,
                                &_nsprdescriptor_available64,
                                &_nsprdescriptor_fsync,
                                &_nsprdescriptor_seek,
                                &_nsprdescriptor_seek64,
                                &_nsprdescriptor_fileinfo,
                                &_nsprdescriptor_fileinfo64,
                                &_nsprdescriptor_writev,
                                &_nsprdescriptor_connect,
                                &_nsprdescriptor_accept,
                                &_nsprdescriptor_bind,
                                &_nsprdescriptor_listen,
                                &_nsprdescriptor_shutdown,
                                &_nsprdescriptor_recv,
                                &_nsprdescriptor_send,
                                &_nsprdescriptor_recvfrom,
                                &_nsprdescriptor_sendto,
                                &_nsprdescriptor_poll,
                                NULL,
                                &_nsprdescriptor_transmitfile,
                                &_nsprdescriptor_getsockname,
                                &_nsprdescriptor_getpeername,
                                NULL,
                                NULL,
                                &_nsprdescriptor_getsocketoption,
                                &_nsprdescriptor_setsocketoption,
                                &_nsprdescriptor_sendfile,
                                NULL, 
                                NULL, 
                                NULL, 
                                NULL, 
                                NULL };

//-----------------------------------------------------------------------------
// NsprDescriptor::NsprDescriptor
//-----------------------------------------------------------------------------

NsprDescriptor::NsprDescriptor(PRFileDesc *lower)
{
    if (_identity == -1) {
        PR_CallOnce(&_once, &_initialize);
    }

    _fd.methods = &_methods;
    _fd.secret = (PRFilePrivate*)this;
    _fd.lower = lower;
    _fd.higher = NULL;
    _fd.dtor = NULL;
    _fd.identity = _identity;

    if (lower) {
        lower->higher = &_fd;
    }
}

//-----------------------------------------------------------------------------
// NsprDescriptor::~NsprDescriptor
//-----------------------------------------------------------------------------

NsprDescriptor::~NsprDescriptor()
{
    PR_ASSERT(_fd.methods == &_methods);
    PR_ASSERT(_fd.secret == (PRFilePrivate*)this);
    PR_ASSERT(_fd.identity == _identity);

    memset(&_fd, 0, sizeof(_fd));
}

//-----------------------------------------------------------------------------
// NsprDescriptor::getNsprDescriptor
//-----------------------------------------------------------------------------

NsprDescriptor* NsprDescriptor::getNsprDescriptor(PRFileDesc *fd)
{
    if (fd) {
        fd = PR_GetIdentitiesLayer(fd, _identity);
        if (fd) {
            return (NsprDescriptor*)fd->secret;
        }
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// NsprDescriptor::printf
//-----------------------------------------------------------------------------

PRInt32 NsprDescriptor::printf(const char *fmt, ...)
{
    va_list ap;
    PRInt32 rv;

    va_start(ap, fmt);
    rv = vprintf(fmt, ap);
    va_end(ap);

    return rv;
}

//-----------------------------------------------------------------------------
// NsprDescriptor::vprintf
//-----------------------------------------------------------------------------

PRInt32 NsprDescriptor::vprintf(const char *fmt, va_list ap)
{
    return PR_vfprintf((PRFileDesc*)*this, fmt, ap);
}

//-----------------------------------------------------------------------------
// C++ stubs for NSPR IO methods
//-----------------------------------------------------------------------------

PRStatus NsprDescriptor::close()
{ 
    // User must not close NsprDescriptors that are on the stack
    PRStatus rv = PR_SUCCESS;
    if (_fd.lower && _fd.lower->methods->close) {
        rv = (_fd.lower->methods->close)(_fd.lower);
    }
    delete this;
    return rv;
}

PRInt32 NsprDescriptor::read(void *buf, PRInt32 amount)
{
    if (_fd.lower && _fd.lower->methods->read) {
        return (_fd.lower->methods->read)(_fd.lower, buf, amount);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 NsprDescriptor::write(const void *buf, PRInt32 amount)
{
    if (_fd.lower && _fd.lower->methods->write) {
        return (_fd.lower->methods->write)(_fd.lower, buf, amount);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt64 NsprDescriptor::available()
{
    if (_fd.lower && _fd.lower->methods->available64) {
        return (_fd.lower->methods->available64)(_fd.lower);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRStatus NsprDescriptor::fsync()
{
    if (_fd.lower && _fd.lower->methods->fsync) {
        return (_fd.lower->methods->fsync)(_fd.lower);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PROffset64 NsprDescriptor::seek(PROffset64 offset, PRSeekWhence how)
{
    if (_fd.lower && _fd.lower->methods->seek64) {
        return (_fd.lower->methods->seek64)(_fd.lower, offset, how);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRStatus NsprDescriptor::fileinfo(PRFileInfo64 *info)
{
    if (_fd.lower && _fd.lower->methods->fileInfo64) {
        return (_fd.lower->methods->fileInfo64)(_fd.lower, info);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 NsprDescriptor::writev(const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->writev) {
        return (_fd.lower->methods->writev)(_fd.lower, iov, iov_size, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRStatus NsprDescriptor::connect(const PRNetAddr *addr, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->connect) {
        return (_fd.lower->methods->connect)(_fd.lower, addr, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRFileDesc* NsprDescriptor::accept(PRNetAddr *addr, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->accept) {
        return (_fd.lower->methods->accept)(_fd.lower, addr, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return NULL;
}

PRStatus NsprDescriptor::bind(const PRNetAddr *addr)
{
    if (_fd.lower && _fd.lower->methods->bind) {
        return (_fd.lower->methods->bind)(_fd.lower, addr);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus NsprDescriptor::listen(PRIntn backlog)
{
    if (_fd.lower && _fd.lower->methods->listen) {
        return (_fd.lower->methods->listen)(_fd.lower, backlog);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus NsprDescriptor::shutdown(PRIntn how)
{
    if (_fd.lower && _fd.lower->methods->shutdown) {
        return (_fd.lower->methods->shutdown)(_fd.lower, how);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 NsprDescriptor::recv(void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->recv) {
        return (_fd.lower->methods->recv)(_fd.lower, buf, amount, flags, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 NsprDescriptor::send(const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->send) {
        return (_fd.lower->methods->send)(_fd.lower, buf, amount, flags, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 NsprDescriptor::recvfrom(void *buf, PRInt32 amount, PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->recvfrom) {
        return (_fd.lower->methods->recvfrom)(_fd.lower, buf, amount, flags, addr, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 NsprDescriptor::sendto(const void *buf, PRInt32 amount, PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->sendto) {
        return (_fd.lower->methods->sendto)(_fd.lower, buf, amount, flags, addr, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt16 NsprDescriptor::poll(PRInt16 in_flags, PRInt16 *out_flags)
{
    if (_fd.lower && _fd.lower->methods->poll) {
        return (_fd.lower->methods->poll)(_fd.lower, in_flags, out_flags);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 NsprDescriptor::transmitfile(PRFileDesc *fd, const void *headers, PRInt32 hlen, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->transmitfile) {
        return (_fd.lower->methods->transmitfile)(_fd.lower, fd, headers, hlen, flags, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRStatus NsprDescriptor::getsockname(PRNetAddr *addr)
{
    if (_fd.lower && _fd.lower->methods->getsockname) {
        return (_fd.lower->methods->getsockname)(_fd.lower, addr);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus NsprDescriptor::getpeername(PRNetAddr *addr)
{
    if (_fd.lower && _fd.lower->methods->getpeername) {
        return (_fd.lower->methods->getpeername)(_fd.lower, addr);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus NsprDescriptor::getsocketoption(PRSocketOptionData *data)
{
    if (_fd.lower && _fd.lower->methods->getsocketoption) {
        return (_fd.lower->methods->getsocketoption)(_fd.lower, data);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus NsprDescriptor::setsocketoption(const PRSocketOptionData *data)
{
    if (_fd.lower && _fd.lower->methods->setsocketoption) {
        return (_fd.lower->methods->setsocketoption)(_fd.lower, data);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 NsprDescriptor::sendfile(PRSendFileData *sendData, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    if (_fd.lower && _fd.lower->methods->sendfile) {
        return (_fd.lower->methods->sendfile)(_fd.lower, sendData, flags, timeout);
    }
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}
