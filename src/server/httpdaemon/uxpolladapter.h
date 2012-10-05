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
 * PROPRIETARY/CONFIDENTIAL.  Use of this product is subject to license terms.
 * Copyright  1999 Sun Microsystems, Inc. Some preexisting portions Copyright
 * 1999 Netscape Communications Corp. All rights reserved.
 *
 * uxpolladapter.h
 * Native poll adapter for Unix.
 */

#ifndef _PR_UXPOLL_ADAPTER_H_
#define _PR_UXPOLL_ADAPTER_H_

#ifndef Linux
#include <poll.h>
#else
/* Needed for extra defines */
#define _BSD_SOURCE 1
#define _XOPEN_SOURCE 1
#include <sys/poll.h>
#endif

#include "private/pprio.h"
#include "frame/log.h"

// Provide PollAdapter functions using native Unix poll() interface.
class UxPollAdapter {
public:
    // Event flags for setupPollDesc and setPollDescEvents
    enum Events {
        READABLE = POLLIN,
        WRITABLE = POLLOUT
    };

    // The size of a single underlying poll descriptor
    static size_t descSize();

    // Setup the underluing poll descritptor
    static void setupPollDesc(void *descArray, PRInt32 index, PRFileDesc *socket, Events events);

    // Change the events we're interested in
    static void setPollDescEvents(void *descArray, PRInt32 index, Events events);

    // Poll the given descriptor array, and return the number of descriptors ready
    static int poll(void *descArray, PRInt32 numDesc, PRIntervalTime timeout);

    // Is the underlying poll descriptor ready?
    static int isPollDescReady(void *descArray, PRInt32 index);

    // Is the underlying poll descriptor readable?
    static int isPollDescReadable(void *descArray, PRInt32 index);

    // Is the underlying poll descriptor writable?
    static int isPollDescWritable(void *descArray, PRInt32 index);

    // reset the poll descriptor state
    static void resetDesc(void *descArray, PRInt32 index);

    // Swap underlying descriptor
    static void swapDesc(void *descArray, PRInt32 dst, PRInt32 src);
};

    // Get the size of a single underlying poll descriptor
    inline size_t UxPollAdapter::descSize()
    {
        return sizeof(struct pollfd);
    }

    inline void UxPollAdapter::setupPollDesc(void *descArray, PRInt32 index, PRFileDesc *socket, UxPollAdapter::Events events)
    {
        struct pollfd *pa = (struct pollfd *)descArray;

        pa[index].fd      = PR_FileDesc2NativeHandle(socket);
        pa[index].events  = events;
        pa[index].revents = 0;
    }

    inline void UxPollAdapter::setPollDescEvents(void *descArray, PRInt32 index, UxPollAdapter::Events events)
    {
        struct pollfd *pa = (struct pollfd *)descArray;

        PR_ASSERT(pa[index].fd != -1);
        pa[index].events  = events;
        pa[index].revents = 0;
    }

    inline int UxPollAdapter::isPollDescReady(void *descArray, PRInt32 index)
    {
        struct pollfd *pa = (struct pollfd *)descArray;

        return (pa[index].revents != 0);
    }

    inline int UxPollAdapter::isPollDescWritable(void *descArray, PRInt32 index)
    {
        struct pollfd *pa = (struct pollfd *)descArray;

        return (pa[index].revents & POLLOUT);
    }

    inline int UxPollAdapter::isPollDescReadable(void *descArray, PRInt32 index)
    {
        struct pollfd *pa = (struct pollfd *)descArray;

        return (pa[index].revents & POLLIN);
    }

    inline void UxPollAdapter::resetDesc(void *descArray, PRInt32 index)
    {
        struct pollfd *pa = (struct pollfd *)descArray;

        pa[index].fd = -1;
        pa[index].events = 0;
        pa[index].revents = 0;
    }

    inline void UxPollAdapter::swapDesc(void *descArray, PRInt32 dst, PRInt32 src)
    {
        struct pollfd *pa = (struct pollfd *)descArray;

        pa[dst] = pa[src];
        resetDesc(descArray, src);
    }

#endif /* _PR_UXPOLL_ADAPTER_H_ */
