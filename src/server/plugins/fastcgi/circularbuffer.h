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

#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#ifdef DEBUG
#include <stdlib.h>
#endif

#include <nspr.h>
#include <plstr.h>
#include "base/util.h"
#include "frame/log.h"
#include "constants.h"
//-----------------------------------------------------------------------------
// CircularBuffer
//-----------------------------------------------------------------------------

class CircularBuffer {
public:
    inline CircularBuffer();
    inline CircularBuffer(int size);
    inline CircularBuffer(char *buffer, int size);
    inline ~CircularBuffer();

    // Empty the buffer and start again
    inline void reset();

    // Add data to the circular buffer
    inline int hasSpace() { return _writable ? _size - _used : 0; }
    inline int requestSpace(char* &buffer, int &size);
    inline void releaseSpace(int size);
    inline int addData(char *data, int size);

    // Remove data from the circular buffer
    inline int hasData() { return _used; }
    inline int requestData(char* &buffer, int &size);
    inline int getData(char* buffer, int size);
    inline void releaseData(int size);
    inline int peekc(char *c = NULL);
    inline int getc(char *c = NULL);

    inline int move(CircularBuffer &toBuffer, int size, PRBool totalMove=PR_TRUE);

    // Conversion to/from NSAPI netbuf
    inline void getNetbuf(netbuf *buf);
    inline void setNetbuf(netbuf *buf);

    // Mirror the contents of another buffer
    inline int mirror(const CircularBuffer &buffer, int size);

private:
    CircularBuffer& operator=(const CircularBuffer &buffer);

    int _i;
    int _o;
    int _used;
    int _size;
    char *_buffer;
    PRBool _allocated;
    PRBool _writable;
};

//-----------------------------------------------------------------------------
// CircularBuffer::CircularBuffer
//-----------------------------------------------------------------------------

CircularBuffer::CircularBuffer()
: _i(0), _o(0), _used(0), _size(0), _buffer(NULL), _allocated(PR_FALSE), _writable(PR_FALSE)
{
}

CircularBuffer::CircularBuffer(int size)
: _i(0), _o(0), _used(0), _size(size), _allocated(PR_TRUE), _writable(PR_TRUE)
{
    _buffer = new char[_size];
}

CircularBuffer::CircularBuffer(char *buffer, int size)
: _i(0), _o(0), _used(0), _size(size), _buffer(buffer), _allocated(PR_FALSE), _writable(PR_TRUE)
{
}

//-----------------------------------------------------------------------------
// CircularBuffer::~CircularBuffer
//-----------------------------------------------------------------------------

CircularBuffer::~CircularBuffer()
{
    if (_allocated) {
        delete _buffer;
        _buffer = NULL;
    }
}

//-----------------------------------------------------------------------------
// CircularBuffer::reset
//-----------------------------------------------------------------------------

void CircularBuffer::reset()
{
    _i = 0;
    _o = 0;
    _used = 0;
}

//-----------------------------------------------------------------------------
// CircularBuffer::requestSpace
//-----------------------------------------------------------------------------

int CircularBuffer::requestSpace(char* &buffer, int &size)
{
    size = hasSpace();
    if (size > _size - _i)
        size = _size - _i;

    buffer = size ? &_buffer[_i] : NULL;

    return size;
}

//-----------------------------------------------------------------------------
// CircularBuffer::releaseSpace
//-----------------------------------------------------------------------------

void CircularBuffer::releaseSpace(int size)
{
    if (size > 0) {
        _i = (_i + size) % _size;
        _used += size;
        PR_ASSERT(_used >= 0);
    }

    PR_ASSERT(_i == (_o + _used) % _size);
}

//-----------------------------------------------------------------------------
// CircularBuffer::requestData
//-----------------------------------------------------------------------------

int CircularBuffer::requestData(char* &buffer, int &size)
{
    size = hasData();
    if (size > _size - _o)
        size = _size - _o;

    buffer = size ? &_buffer[_o] : NULL;

    return size;
}

int CircularBuffer::getData(char* buffer, int size)
{
    int len = hasData();
    len = min(len, size);
    if((len > 0) && (buffer)) {
        memcpy(buffer, &(_buffer[_o]), len);
    }

    if(!buffer)
        len = 0;

    return len;
}

//-----------------------------------------------------------------------------
// CircularBuffer::releaseData
//-----------------------------------------------------------------------------

void CircularBuffer::releaseData(int size)
{
    if (size > 0) {
        _o = (_o + size) % _size;
        PR_ASSERT(_used >= size);
        _used -= size;
        PR_ASSERT(_used >= 0);
    }

    if (!_used)
        reset();

    PR_ASSERT(_i == (_o + _used) % _size);
}

//-----------------------------------------------------------------------------
// CircularBuffer::peekc
//-----------------------------------------------------------------------------

int CircularBuffer::peekc(char *c)
{
    int size = hasData();
    if (size < 1)
        return 0;

    if (c)
        *c = _buffer[_o];

    return 1;
}

//-----------------------------------------------------------------------------
// CircularBuffer::getc
//-----------------------------------------------------------------------------

int CircularBuffer::getc(char *c)
{
    int size = hasData();
    if (size < 1)
        return 0;

    if (c)
        *c = _buffer[_o];

    releaseData(1);

    return 1;
}


//-----------------------------------------------------------------------------
// CircularBuffer::getNetbuf
//-----------------------------------------------------------------------------

void CircularBuffer::getNetbuf(netbuf *buf)
{
    buf->inbuf = (unsigned char*)_buffer;
    buf->pos = _o;
    buf->cursize = _o + _used;
    buf->maxsize = _size;
}

//-----------------------------------------------------------------------------
// CircularBuffer::setNetbuf
//-----------------------------------------------------------------------------

void CircularBuffer::setNetbuf(netbuf *buf)
{
    PR_ASSERT(buf->inbuf == (unsigned char*)_buffer);
    PR_ASSERT(buf->maxsize == _size);
    PR_ASSERT(buf->cursize >= buf->pos);

    _buffer = (char*)buf->inbuf;
    _o = buf->pos;
    _i = buf->cursize;
    if (_i >= buf->maxsize)
        _i = 0;
    _used = buf->cursize - buf->pos;
    _size = buf->maxsize;

    PR_ASSERT(_i == (_o + _used) % _size);
}

//-----------------------------------------------------------------------------
// CircularBuffer::mirror
//-----------------------------------------------------------------------------

int CircularBuffer::mirror(const CircularBuffer &buffer, int size)
{
    // Mirror up to size data (not space) bytes from buffer for reading
    _buffer = buffer._buffer;
    _i = buffer._i;
    _o = buffer._o;
    _used = buffer._used > size ? size : buffer._used;
    _size = buffer._size;
    _allocated = PR_FALSE;
    _writable = PR_FALSE;

    return _used;
}


int CircularBuffer::move(CircularBuffer &toBuffer, int size, PRBool totalMove) {
    char *fromBuf;
    char *toBuf;
    int fromLen;
    int toLen;
    int len;
    int dataToBeMoved = size;
    int dataMoved = 0;

    do {
    fromBuf = NULL;
    toBuf = NULL;
    toLen = 0;
    fromLen = 0;
    this->requestData(fromBuf, fromLen);
    toBuffer.requestSpace(toBuf, toLen);
    len = min((min(toLen, fromLen)), dataToBeMoved);
    if((len > 0) && (dataToBeMoved > 0) && toBuf && fromBuf) {
        memcpy(toBuf, fromBuf, len);
        toBuffer.releaseSpace(len);
        this->releaseData(len);
        dataMoved += len;
        dataToBeMoved -= len;
    }

    if(!toBuf || !fromBuf)
        len = 0;

    } while(totalMove && (len > 0) && (dataToBeMoved > 0)); 

    return dataMoved;
}

int CircularBuffer::addData(char *data, int size) {
    char *buf;
    int len;
    requestSpace(buf, len);

    if(len < size)
        len = 0;

    len = min(len, size);

    if(len > 0 && buf) {
        memcpy(buf, data, len);
        releaseSpace(len);
    }

    if(!buf)
        len = 0;

    return len;
}

#endif // CIRCULARBUFFER_H
