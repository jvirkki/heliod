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

#ifndef ENDPOINT_H
#define ENDPOINT_H

#include <nspr.h>
#include "private/pprio.h"
#include "circularbuffer.h"

//-----------------------------------------------------------------------------
// EndPoint
//-----------------------------------------------------------------------------

class EndPoint {
public:
    inline EndPoint(PRUint16 family,
                    CircularBuffer *from,
                    CircularBuffer *to,
                    PRBool isClient = PR_FALSE,
                    PRBool isNamedPipe = PR_FALSE,
                    PRBool flagReadable = PR_TRUE,
                    PRBool flagWritable = PR_TRUE)
    : from(from),
      to(to),
      countBytesSent(0),
      isClient(isClient),
#ifdef XP_WIN32
      namedPipe(isNamedPipe),
#endif // XP_WIN32
      flagReadable(flagReadable),
      flagWritable(flagWritable)
    {
        fd = PR_Socket(family, SOCK_STREAM, 0);
    }

    inline EndPoint(PRFileDesc *fd,
                    CircularBuffer *from,
                    CircularBuffer *to,
                    PRBool isClient = PR_FALSE,
                    PRBool isNamedPipe = PR_FALSE,
                    PRBool flagReadable = PR_TRUE,
                    PRBool flagWritable = PR_TRUE)
    : fd(fd),
      from(from),
      to(to),
      countBytesSent(0),
      isClient(isClient),
#ifdef XP_WIN32
      namedPipe(isNamedPipe),
#endif // XP_WIN32
      flagReadable(flagReadable),
      flagWritable(flagWritable)
    { }

    inline PRBool add(PRPollDesc *pd);
    inline int recv();
    inline int send();
    inline PRInt64 getCountBytesSent() { return countBytesSent; }
    inline PRBool isReadable() { return flagReadable; }
    inline PRBool isWritable() { return flagWritable; }

    CircularBuffer *from;
    CircularBuffer *to;

    PRFileDesc *fd;
    PRBool isClient;

protected:
    PRBool flagReadable;
    PRBool flagWritable;
    PRInt64 countBytesSent;
#ifdef XP_WIN32
    PRBool namedPipe;
#endif // XP_WIN32
};

//-----------------------------------------------------------------------------
// EndPoint::add
//-----------------------------------------------------------------------------

inline PRBool EndPoint::add(PRPollDesc *pd)
{
    // Add the endpoint to a PRPollDesc
    pd->fd = fd;
    pd->in_flags = 0;
    pd->out_flags = 0;
    if (flagReadable && from->hasSpace()) {
        pd->in_flags |= PR_POLL_READ;
    }
    if (flagWritable && to->hasData()) {
        pd->in_flags |= PR_POLL_WRITE;
    }
    return (pd->in_flags != 0);
}

//-----------------------------------------------------------------------------
// EndPoint::recv
//-----------------------------------------------------------------------------

inline int EndPoint::recv()
{
    int rv = 0;

    // Receive into from
    if (flagReadable) {
        char *buffer;
        int size;
        if (from->requestSpace(buffer, size)) {
            PR_SetError(PR_CONNECT_RESET_ERROR, 0);
#ifdef XP_WIN32
            OVERLAPPED gOverlapped;
            // Set up overlapped structure fields.
            gOverlapped.Offset = 0;
            gOverlapped.OffsetHigh = 0;
            gOverlapped.hEvent = CreateEvent( NULL, // default security attribute
                                              TRUE, // manual-reset event
                                              TRUE, // initial state = signaled
                                              NULL); // unnamed event object
            if (namedPipe && !isClient) {
                BOOL fsuccess = ReadFile((HANDLE)PR_FileDesc2NativeHandle(fd), buffer, size, (unsigned long *)&rv, &gOverlapped);
                DWORD r = GetLastError();
                if (!fsuccess && r != ERROR_IO_PENDING && r != ERROR_MORE_DATA) {
                    flagReadable = PR_FALSE;
                } else if (r == ERROR_IO_PENDING) {
                    DWORD vl = WaitForSingleObject(gOverlapped.hEvent, INFINITE);
                    if (!vl) {
                        BOOL v = GetOverlappedResult( (HANDLE)PR_FileDesc2NativeHandle(fd), &gOverlapped, (unsigned long *)&rv, TRUE );
                    }
                }
            } else {
                // XXX PR_Recv() with a 0 timeout is filling buffer with the read
                // data but returning PR_FAILURE with PR_IO_TIMEOUT_ERROR on NT
                rv = net_read(fd, buffer, size, 1);
            }
#else
            rv = net_read(fd, buffer, size, NET_ZERO_TIMEOUT);
#endif
            if (rv < 1) {
#ifdef XP_WIN32
                if (!namedPipe || isClient) {
#endif
                // XXX kludge around the potential for NSS sockets to return 0
                // bytes even after polling ready
                PRInt32 err = PR_GetError();
                if (err == PR_SOCKET_SHUTDOWN_ERROR || err == PR_CONNECT_RESET_ERROR) {
                    flagReadable = PR_FALSE;
                }
#ifdef XP_WIN32
                }
#endif
            }
            if (rv > 0)
                from->releaseSpace(rv);
        }
    }

    return rv;
}

//-----------------------------------------------------------------------------
// EndPoint::send
//-----------------------------------------------------------------------------

inline int EndPoint::send()
{
    int rv = 0;

    // Send from to
    if (flagWritable) {
        char *buffer;
        int size;
        if (to->requestData(buffer, size)) {
#ifdef XP_WIN32
            OVERLAPPED gOverlapped;
            // Set up overlapped structure fields.
            gOverlapped.Offset = 0;
            gOverlapped.OffsetHigh = 0;
            gOverlapped.hEvent = CreateEvent( NULL, // default security attribute
                                              TRUE, // manual-reset event
                                              TRUE, // initial state = signaled
                                              NULL); // unnamed event object

            if (namedPipe && !isClient) {
                BOOL fsuccess = WriteFile((HANDLE)PR_FileDesc2NativeHandle(fd), buffer, size, (unsigned long *)&rv, &gOverlapped);
                DWORD r = GetLastError();
                if (!fsuccess && r != ERROR_IO_PENDING && r != ERROR_MORE_DATA) {
                    flagWritable = PR_FALSE;
                } else if (r == ERROR_IO_PENDING) {
                    DWORD vl = WaitForSingleObject(gOverlapped.hEvent, INFINITE);
                    if (!vl) {
                        BOOL v = GetOverlappedResult( (HANDLE)PR_FileDesc2NativeHandle(fd), &gOverlapped, (unsigned long *)&rv, TRUE );
                    }
                }

            } else
                rv = net_write(fd, buffer, size);
#else
            rv = net_write(fd, buffer, size);
#endif // XP_WIN32
            if (rv < 0) {
#ifndef XP_WIN32
                PRInt32 err = PR_GetError();
#endif // XP_WIN32
                flagWritable = PR_FALSE;
            }
            if (rv > 0) {
                countBytesSent += rv;
                to->releaseData(rv);
            }
        }
    }

    return rv;
}

#endif // ENDPOINT_H
