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
 * poll_array.h
 * Keepalive poll array maintenance.
 */

#ifndef _POLL_ARRAY_H_
#define _POLL_ARRAY_H_

#include "nspr.h"
#include "prthread.h"
#include "polladapter.h"
#include "httpdaemon/configuration.h"

class Configuration;

class PollArray {
public:
    PollArray(PRInt32 numDesc);
    ~PollArray();

    //
    // Add this connection to the list of connections waiting to be added to
    // the keepalive poll array.
    //
    PRStatus AddDescriptor(PRFileDesc *fd, PollAdapter::Events events, const Configuration *config, time_t expirationTime, void *data);

    //
    // Change the events in which we're interested.
    //
    void SetDescriptorEvents(PRInt32 index, PollAdapter::Events events);

    //
    // Change the time at which a polled file descriptor will expire.
    //
    void SetDescriptorExpirationTime(PRInt32 index, time_t expirationTime);

    //
    // Indicate whether the poll descriptor polled ready to read.
    //
    PRBool IsDescriptorReadable(PRInt32 index);

    //
    // Indicate whether the poll descriptor polled ready to write.
    //
    PRBool IsDescriptorWritable(PRInt32 index);

    //
    // Return the user-defined data associated with the poll descriptor.
    //
    void *GetDescriptorData(int index);

#ifdef DEBUG
    //
    // Indicate whether any poll descriptor is associated with the specified
    // user-defined data.
    //
    PRBool IsDataPresent(void *data);
#endif

    //
    // Remove a poll descriptor from the poll array.
    //
    void RemoveDescriptor(PRInt32 index);

    //
    // Check the underlying platform-specific poll set using the poll adapter.
    // Returns the number of ready poll descriptors.  The set of ready poll
    // descriptors may be examined with FindReadyDescriptor.
    //
    PRInt32 Poll(PRIntervalTime timeout);

    // 
    // Find the index of a poll descriptor that polled ready.  Pass an index
    // of 0 to start looking from the beginning of the poll array.  This
    // function must not be called more times than the number of ready poll
    // descriptors indicated by the most recent Poll call.
    // 
    PRInt32 FindReadyDescriptor(PRInt32 index);

    // 
    // Find the index of an expired poll descriptor.  Pass an index of 0 to
    // start looking from the beginning of the poll array.  Returns -1 if there
    // are no expired descriptors.
    // 
    PRInt32 FindExpiredDescriptor(const Configuration *config, time_t now, PRInt32 index);

    //
    // Compact the poll array if it is too sparse.
    //
    void Compact();

    //
    // Get the number of live entries either in the poll array or waiting to
    // be added to the poll array
    //
    PRInt32 GetNumDescriptors() { return active; }

private:

    void    *poll_array;        // platform-specific poll descriptor array
    PRBool  *poll_occupied;     // poll_occuped[i] indicates whether the 
                                // poll_array[i] poll descriptor is in use
    void    **poll_data;        // poll_data[i] contains user-supplied data
                                // for the poll_array[i] poll descriptor
    PRInt32 *poll_config;       // poll_config[i] contains the configuration
                                // ID for the poll_array[i] poll descriptor
    time_t  *poll_expiration;   // poll_expiration[i] contains the expiration
                                // time for the poll_array[i] poll descriptor
    PRInt32 poll_count;         // #entries in use, in poll_array 
    PRInt32 active;             // #active entries in poll_array
    PRInt32 array_size;         // size of the poll array: poll_count limit     
    PRInt32 *free_slots;        // Unused indeces in "poll_array"
    PRInt32 free_count;         // Number of listed free slots 

    PRStatus Resize(PRInt32 numDesc);
    PRStatus Grow();
};

inline PRStatus
PollArray::AddDescriptor(PRFileDesc *fd, PollAdapter::Events events, const Configuration *config, time_t expirationTime, void *data)
{
    PRInt32 index;
    if (free_count > 0) {
        index = free_slots[--free_count];
    } else {
        if (poll_count == array_size) {
            if (Grow() == PR_FAILURE)
                return PR_FAILURE;
        }
        index = poll_count++;
    }

    PR_ASSERT(index >= 0 && index < array_size);

    PollAdapter::setupPollDesc(poll_array, index, fd, events);

    PR_ASSERT(!poll_occupied[index]);
    poll_occupied[index] = PR_TRUE;
    poll_config[index] = config ? config->getID() : Configuration::idInvalid;
    poll_expiration[index] = expirationTime;
    poll_data[index] = data;

    active++;

    return PR_SUCCESS;
}

inline void
PollArray::SetDescriptorEvents(PRInt32 index, PollAdapter::Events events)
{
    PR_ASSERT(poll_occupied[index]);

    PollAdapter::setPollDescEvents(poll_array, index, events);
}

inline void
PollArray::SetDescriptorExpirationTime(PRInt32 index, time_t expirationTime)
{
    PR_ASSERT(poll_occupied[index]);

    poll_expiration[index] = expirationTime;
}

inline PRBool
PollArray::IsDescriptorReadable(PRInt32 index)
{
    PR_ASSERT(poll_occupied[index]);

    return PollAdapter::isPollDescReadable(poll_array, index);
}

inline PRBool
PollArray::IsDescriptorWritable(PRInt32 index)
{
    PR_ASSERT(poll_occupied[index]);

    return PollAdapter::isPollDescWritable(poll_array, index);
}

inline void *
PollArray::GetDescriptorData(int index)
{
    PR_ASSERT(poll_occupied[index]);

    return poll_data[index];
}

#ifdef DEBUG
inline PRBool
PollArray::IsDataPresent(void *data)
{
    for (PRInt32 index = 0; index < poll_count; index++) {
        if (poll_data[index] == data && poll_occupied[index])
            return PR_TRUE;
    }

    return PR_FALSE;
}
#endif

inline void 
PollArray::RemoveDescriptor(PRInt32 index)
{
    PR_ASSERT(index >= 0 && index < array_size);
    PR_ASSERT(poll_occupied[index]);

    free_slots[free_count++] = index;

    PollAdapter::resetDesc(poll_array, index);

    poll_occupied[index] = PR_FALSE;

    active--;
}

inline PRInt32
PollArray::Poll(PRIntervalTime timeout)
{
    return PollAdapter::poll(poll_array, poll_count, timeout);
}

inline PRInt32
PollArray::FindReadyDescriptor(PRInt32 index)
{
    PR_ASSERT(index >= 0 && index < poll_count);

    while (!PollAdapter::isPollDescReady(poll_array, index))
        index++;

    PR_ASSERT(index >= 0 && index < poll_count);
    PR_ASSERT(poll_occupied[index]);
    
    return index;
}

inline PRInt32
PollArray::FindExpiredDescriptor(const Configuration *config, time_t now, PRInt32 index)
{
    PR_ASSERT(index >= 0 && index <= poll_count);

    PRInt32 config_invalid = Configuration::idInvalid;
    PRInt32 config_current = config ? config->getID() : config_invalid;

    while (index < poll_count) {
        if (poll_occupied[index] &&
            ((poll_expiration[index] != -1 &&
              poll_expiration[index] <= now) ||
             (poll_config[index] != config_invalid &&
              poll_config[index] != config_current)))
        {
            return index;
        }

        index++;
    }

    return -1;
}

#endif /* _POLL_ARRAY_H_ */

