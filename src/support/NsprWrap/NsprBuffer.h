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

#ifndef NSPRBUFFER_H
#define NSPRBUFFER_H

#include <nspr.h>

#include "NsprDescriptor.h"

#ifdef XP_PC 
#ifdef BUILD_NSPRWRAP_DLL
#define NSPRBUFFER_DLL_API _declspec(dllexport)
#else
#define NSPRBUFFER_DLL_API _declspec(dllimport)
#endif
#else
#define NSPRBUFFER_DLL_API 
#endif

#ifdef __cplusplus

//-----------------------------------------------------------------------------
// NsprBuffer - An NsprDescriptor that's actually an in-memory buffer
//-----------------------------------------------------------------------------

class NSPRBUFFER_DLL_API NsprBuffer : public NsprDescriptor {
public:
    NsprBuffer(PRInt32 capacity = 1024, PRInt32 limit = 1048576);
    ~NsprBuffer();

    PRInt32 read(void *buf, PRInt32 amount);
    PRInt32 write(const void *buf, PRInt32 amount);
    PROffset64 seek(PROffset64 offset, PRSeekWhence how);
    PRInt32 writev(const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout);
    PRInt32 recv(void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
    PRInt32 send(const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
    // XXX transmitfile()/sendfile() is not implemented

    PRInt32 size() const { return _size; }
    const void* data() const { return _buffer; }
    void* data() { return _buffer; }

    operator const char*() const;
    operator char*();

protected:
    void require(PRInt32 size);

    char *_buffer;
    PRInt32 _size;
    PRInt32 _capacity;
    PRInt32 _position;
    PRInt32 _limit;
};

#endif /* __cplusplus */

NSPR_BEGIN_EXTERN_C

PRFileDesc* PR_NewBuffer(PRInt32 capacity, PRInt32 limit);

NSPR_END_EXTERN_C

#endif /* NSPRBUFFER_H */
