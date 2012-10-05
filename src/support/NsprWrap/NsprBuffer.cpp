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

#include "NsprBuffer.h"

//-----------------------------------------------------------------------------
// PR_NewBuffer
//-----------------------------------------------------------------------------

PRFileDesc* PR_NewBuffer(PRInt32 capacity, PRInt32 limit)
{
    NsprBuffer *buffer = new NsprBuffer(capacity, limit);
    return (PRFileDesc*)*buffer;
}

//-----------------------------------------------------------------------------
// NsprBuffer::NsprBuffer
//-----------------------------------------------------------------------------

NsprBuffer::NsprBuffer(PRInt32 capacity, PRInt32 limit)
: _buffer(NULL),
  _size(0),
  _capacity(capacity),
  _position(0),
  _limit(limit)
{
    const int maxallowed = 0x80000000 - 2;
    if (_limit <= 0 || _limit > maxallowed)
        _limit = maxallowed;
    if (_capacity < 0 || _capacity > _limit)
        _capacity = _limit;
}

//-----------------------------------------------------------------------------
// NsprBuffer::~NsprBuffer
//-----------------------------------------------------------------------------

NsprBuffer::~NsprBuffer()
{
    if (_buffer)
        PR_Free(_buffer);
}

//-----------------------------------------------------------------------------
// NsprBuffer::read
//-----------------------------------------------------------------------------

PRInt32 NsprBuffer::read(void *buf, PRInt32 amount)
{
    PRInt32 available = _size - _position;

    if (amount > available)
        amount = available;

    if (amount <= 0)
        return 0;

    memcpy(buf, &_buffer[_position], amount);
    _position += amount;

    if (_position == _size) {
        _position = 0;
        _size = 0;
    }

    return amount;
}

//-----------------------------------------------------------------------------
// NsprBuffer::write
//-----------------------------------------------------------------------------

PRInt32 NsprBuffer::write(const void *buf, PRInt32 amount)
{
    if (amount <= 0)
        return 0;

    require(amount);

    PRInt32 available = _capacity - _size;

    if (amount > available)
        amount = available;

    if (amount <= 0)
        return 0;

    memcpy(&_buffer[_size], buf, amount);
    _size += amount;

    return amount;
}

//-----------------------------------------------------------------------------
// NsprBuffer::seek
//-----------------------------------------------------------------------------

PROffset64 NsprBuffer::seek(PROffset64 offset, PRSeekWhence how)
{
    PRInt32 position = -1;

    switch (how) {
    case PR_SEEK_SET:
        position = offset;
        break;

    case PR_SEEK_CUR:
        position = _position + offset;
        break;

    case PR_SEEK_END:
        position = _size + offset;
        break;
    }

    if (position >= 0 && position <= _size) {
        _position = position;
    } else {
        position = -1;
    }

    return position;
}

//-----------------------------------------------------------------------------
// NsprBuffer::writev
//-----------------------------------------------------------------------------

PRInt32 NsprBuffer::writev(const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    PRInt32 total = 0;
    int i;

    for (i = 0; i < iov_size; i++) {
        PRInt32 rv;

        rv = write(iov[i].iov_base, iov[i].iov_len);

        if (rv < 0) {
            // Error
            total = rv;
            break;
        }

        total += rv;

        if (rv < iov[i].iov_len) {
            // Partial write
            break;
        }
    }

    return total;
}

//-----------------------------------------------------------------------------
// NsprBuffer::recv
//-----------------------------------------------------------------------------

PRInt32 NsprBuffer::recv(void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    return read(buf, amount);
}

//-----------------------------------------------------------------------------
// NsprBuffer::send
//-----------------------------------------------------------------------------

PRInt32 NsprBuffer::send(const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    return write(buf, amount);
}

//-----------------------------------------------------------------------------
// NsprBuffer::operator const char*
//-----------------------------------------------------------------------------

NsprBuffer::operator const char*() const
{
    _buffer[_size] = 0;
    return _buffer;
}

//-----------------------------------------------------------------------------
// NsprBuffer::operator char*
//-----------------------------------------------------------------------------

NsprBuffer::operator char*()
{
    _buffer[_size] = 0;
    return _buffer;
}

//-----------------------------------------------------------------------------
// NsprBuffer::require
//-----------------------------------------------------------------------------

void NsprBuffer::require(PRInt32 amount)
{
    // Note that we always allocate one byte more than _capacity to ease null
    // termination of the buffer

    if (!_buffer) {
        PR_ASSERT(_size == 0);
        PR_ASSERT(_position == 0);
        _capacity = PR_MIN(PR_MAX(_capacity, amount), _limit);
        _buffer = (char*)PR_Malloc(_capacity + 1);
        return;
    }

    if (_capacity < _limit) {
        PRInt32 required = _size + amount;
        if (required > _capacity) {
            _capacity = PR_MIN(PR_MAX(_capacity * 2, required), _limit);
            _buffer = (char*)PR_Realloc(_buffer, _capacity + 1);
        }
    }
}

