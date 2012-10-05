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

#include "NsprSink.h"

//-----------------------------------------------------------------------------
// PR_NewSink
//-----------------------------------------------------------------------------

PRFileDesc* PR_NewSink(void)
{
    NsprSink *sink = new NsprSink;
    return (PRFileDesc*)*sink;
}

//-----------------------------------------------------------------------------
// NsprSink::read
//-----------------------------------------------------------------------------

PRInt32 NsprSink::read(void *buf, PRInt32 amount)
{
    return 0;
}

//-----------------------------------------------------------------------------
// NsprSink::write
//-----------------------------------------------------------------------------

PRInt32 NsprSink::write(const void *buf, PRInt32 amount)
{
    return amount;
}

//-----------------------------------------------------------------------------
// NsprSink::writev
//-----------------------------------------------------------------------------

PRInt32 NsprSink::writev(const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    PRInt32 total = 0;
    int i;

    for (i = 0; i < iov_size; i++) {
        total += iov[i].iov_len;
    }

    return total;
}

//-----------------------------------------------------------------------------
// NsprSink::recv
//-----------------------------------------------------------------------------

PRInt32 NsprSink::recv(void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    return 0;
}

//-----------------------------------------------------------------------------
// NsprSink::send
//-----------------------------------------------------------------------------

PRInt32 NsprSink::send(const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    return amount;
}

