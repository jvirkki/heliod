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

#ifndef NSPRDESCRIPTOR_H
#define NSPRDESCRIPTOR_H

#include <nspr.h>

#ifdef XP_PC 
#ifdef BUILD_NSPRWRAP_DLL
#define NSPRDESCRIPTOR_DLL_API _declspec(dllexport)
#else
#define NSPRDESCRIPTOR_DLL_API _declspec(dllimport)
#endif
#else
#define NSPRDESCRIPTOR_DLL_API 
#endif

#ifdef __cplusplus

//-----------------------------------------------------------------------------
// NsprDescriptor - A C++ wrapper for NSPR's PRFileDesc
//-----------------------------------------------------------------------------

class NSPRDESCRIPTOR_DLL_API NsprDescriptor {
public:
    NsprDescriptor(PRFileDesc *lower = 0);
    virtual ~NsprDescriptor();
    virtual PRStatus close();
    virtual PRInt32 read(void *buf, PRInt32 amount);
    virtual PRInt32 write(const void *buf, PRInt32 amount);
    virtual PRInt64 available();
    virtual PRStatus fsync();
    virtual PROffset64 seek(PROffset64 offset, PRSeekWhence how);
    virtual PRStatus fileinfo(PRFileInfo64 *info);
    virtual PRInt32 writev(const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout);
    virtual PRStatus connect(const PRNetAddr *addr, PRIntervalTime timeout);
    virtual PRFileDesc* accept(PRNetAddr *addr, PRIntervalTime timeout);
    virtual PRStatus bind(const PRNetAddr *addr);
    virtual PRStatus listen(PRIntn backlog);
    virtual PRStatus shutdown(PRIntn how);
    virtual PRInt32 recv(void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
    virtual PRInt32 send(const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
    virtual PRInt32 recvfrom(void *buf, PRInt32 amount, PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout);
    virtual PRInt32 sendto(const void *buf, PRInt32 amount, PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout);
    virtual PRInt16 poll(PRInt16 in_flags, PRInt16 *out_flags);
    virtual PRInt32 transmitfile(PRFileDesc *fd, const void *headers, PRInt32 hlen, PRTransmitFileFlags flags, PRIntervalTime timeout);
    virtual PRStatus getsockname(PRNetAddr *addr);
    virtual PRStatus getpeername(PRNetAddr *addr);
    virtual PRStatus getsocketoption(PRSocketOptionData *data);
    virtual PRStatus setsocketoption(const PRSocketOptionData *data);
    virtual PRInt32 sendfile(PRSendFileData *sendData, PRTransmitFileFlags flags, PRIntervalTime timeout);
    operator PRFileDesc*() { return &_fd; }
    PRInt32 printf(const char *fmt, ...);
    PRInt32 vprintf(const char *fmt, va_list ap);
    static NsprDescriptor* getNsprDescriptor(PRFileDesc* fd);

protected:
    PRFileDesc _fd;
};

#endif /* __cplusplus */

#endif /* NSPRDESCRIPTOR_H */
