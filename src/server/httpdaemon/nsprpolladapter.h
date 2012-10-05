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
 * prpollsupp.h
 * NSPR PollSupp implementation.
 */

#ifndef _NSPR_POLL_ADAPTER_H_
#define _NSPR_POLL_ADAPTER_H_

#include "frame/log.h"

// Provide PollAdapter functions using NSPR PR_Poll() interface.
class NSPRPollAdapter {
public:
    // Event flags for setupPollDesc and setPollDescEvents
    enum Events {
        READABLE = PR_POLL_READ,
        WRITABLE = PR_POLL_WRITE
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

    inline void NSPRPollAdapter::setupPollDesc(void *descArray, PRInt32 index, PRFileDesc *socket, NSPRPollAdapter::Events events)
    {
        PRPollDesc *pa = (PRPollDesc *)descArray;

        pa[index].fd = socket;
        pa[index].in_flags = events;
        pa[index].out_flags = 0;
    }

    // Get the size of a single underlying poll descriptor
    inline size_t NSPRPollAdapter::descSize()
    {
        return sizeof(PRPollDesc);
    }

    inline void NSPRPollAdapter::setPollDescEvents(void *descArray, PRInt32 index, NSPRPollAdapter::Events events)
    {
        PRPollDesc *pa = (PRPollDesc *)descArray;

        PR_ASSERT(pa[index].fd != NULL);
        pa[index].in_flags = events;
        pa[index].out_flags = 0;
    }

    inline int NSPRPollAdapter::poll(void *descArray, PRInt32 numDesc, PRIntervalTime timeout)
    {
        PRPollDesc *pa = (PRPollDesc *)descArray;

        return PR_Poll(pa, numDesc, timeout);
    }

    inline int NSPRPollAdapter::isPollDescReady(void *descArray, PRInt32 index)
    {
        PRPollDesc *pa = (PRPollDesc *)descArray;

        return (pa[index].out_flags & (PR_POLL_READ| PR_POLL_WRITE));
    }

    inline int NSPRPollAdapter::isPollDescWritable(void *descArray, PRInt32 index)
    {
        PRPollDesc *pa = (PRPollDesc *)descArray;

        return (pa[index].out_flags &  PR_POLL_WRITE);
    }

    inline int NSPRPollAdapter::isPollDescReadable(void *descArray, PRInt32 index)
    {
        PRPollDesc *pa = (PRPollDesc *)descArray;

        return (pa[index].out_flags & PR_POLL_READ);
    }

    inline void NSPRPollAdapter::resetDesc(void *descArray, PRInt32 index)
    {
        PRPollDesc *pa = (PRPollDesc *)descArray;

        pa[index].fd = NULL;
        pa[index].in_flags = 0;
        pa[index].out_flags = 0;
    }

    inline void NSPRPollAdapter::swapDesc(void *descArray, PRInt32 dst, PRInt32 src)
    {
        PRPollDesc *pa = (PRPollDesc *)descArray;

        pa[dst] = pa[src];
        resetDesc(descArray, src);   
    }

#endif /* _NSPR_POLL_ADAPTER_H_ */
