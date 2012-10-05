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
 * Copyright  2000 Sun Microsystems, Inc. Some preexisting portions Copyright
 * 1999 Netscape Communications Corp. All rights reserved.
 *
 * poll_array.cpp
 *
 * Poll array maintenance functions.
 *
 * This module maintains the generic code for handling connection polling 
 * arrays.  Currently it is used both for the keepalive and the readahead
 * polls. 
 *
 * There is no user-data in the poll array, just a file-descriptor,
 * but we need to map those file descriptors back to connection objects.
 * We maintain tables so that, for a given index (< poll_count) in poll_array:
 *
 *        poll_array[index].fd == poll_data[index]->fd
 *
 * OR
 *
 *        poll_occupied[index] == PR_FALSE
 *    and
 * 	  poll_array[index].fd = -1;
 *    and 
 *	  index is listed in 'free_slots'
 *
 * So given an index in the poll array, we can find the connection.
 *
 * This helps meet several requirements for connection polling:
 *
 * 1) 'poll()' takes an array of fd's and masks and writes into the 
 *    entries to tell us when they fire.  The number of connections can 
 *    be large so we want a fast mapping from the poll-array entry to 
 *    a connection object.
 *	
 * 2) Subsequent 'poll()' calls in a thread are more efficient
 *    when the same poll_array and length are repeated, so we arrange
 *    to leave the poll_array sparse... we mark deletions with -1,
 *    and keep a free list.
 *
 * 3) But we don't want the poll array to become too sparse.
 *    So we pick a threshold and say that when the number of live
 *    entries in the array is half of the total, we'll compact it.
 *    Meanwhile, as connections enter or leave the keepalive polling set,
 *    we use the free_list to assign and reuse positions.
 *
 * 4) Building the poll argument from the global connections table,
 *    although it seems simple (a loop over the connection table once a 
 *    second) becomes a huge bottleneck when the # connections grows.
 *    So we use the events of entering and leaving the keepalive state
 *    to maintain the poll array.
 *
 * Each poll array is only accessed from a single KAPollThread.
 */

#include "time/nstime.h"
#include "pollarray.h"
#include "httpdaemon/WebServer.h"         // WebServer::isTerminating()
#include "frame/log.h"                    // ereport()

PollArray::PollArray(PRInt32 numDesc)
: poll_array(NULL),
  poll_occupied(NULL),
  poll_data(NULL),
  poll_config(NULL),
  poll_expiration(NULL),
  poll_count(0),
  active(0),
  array_size(0),
  free_slots(0),
  free_count(0)
{
    Resize(numDesc);
}

PollArray::~PollArray()
{
    PERM_FREE(poll_array);
    PERM_FREE(poll_occupied);
    PERM_FREE(poll_data);
    PERM_FREE(poll_config);
    PERM_FREE(poll_expiration);
    PERM_FREE(free_slots);
}

PRStatus
PollArray::Resize(PRInt32 numDesc)
{
    PRInt32 i;

    PR_ASSERT(numDesc >= array_size);

    // Attempt to allocate bigger arrays
    void *new_poll_array = PERM_MALLOC(numDesc * PollAdapter::descSize());
    PRBool *new_poll_occupied = (PRBool *) PERM_MALLOC(numDesc * sizeof(PRBool));
    void **new_poll_data = (void **) PERM_MALLOC(numDesc * sizeof(void *));
    PRInt32 *new_poll_config = (PRInt32 *) PERM_MALLOC(numDesc * sizeof(PRInt32));
    time_t *new_poll_expiration = (time_t *) PERM_MALLOC(numDesc * sizeof(time_t));
    PRInt32 *new_free_slots = (PRInt32 *) PERM_MALLOC(numDesc * sizeof(PRInt32));
    if (!new_poll_array ||
        !new_poll_occupied ||
        !new_poll_data ||
        !new_poll_config ||
        !new_poll_expiration ||
        !new_free_slots)
    {
        // Failed to allocate space for at least one of the arrays
        PERM_FREE(new_poll_array);
        PERM_FREE(new_poll_occupied);
        PERM_FREE(new_poll_data);
        PERM_FREE(new_poll_config);
        PERM_FREE(new_poll_expiration);
        PERM_FREE(new_free_slots);
        return PR_FAILURE;
    }

    // Initialize the new arrays
    memcpy(new_poll_array, poll_array, PollAdapter::descSize() * array_size);
    for (i = array_size; i < numDesc; i++)
        PollAdapter::resetDesc(new_poll_array, i);
    memcpy(new_poll_occupied, poll_occupied, sizeof(poll_occupied[0]) * array_size);
    for (i = array_size; i < numDesc; i++)
        new_poll_occupied[i] = PR_FALSE;
    memcpy(new_poll_data, poll_data, sizeof(poll_data[0]) * array_size);
    memcpy(new_poll_config, poll_config, sizeof(poll_config[0]) * array_size);
    memcpy(new_poll_expiration, poll_expiration, sizeof(poll_expiration[0]) * array_size);
    memcpy(new_free_slots, free_slots, sizeof(free_slots[0]) * array_size);

    // Discard the old arrays
    PERM_FREE(poll_array);
    PERM_FREE(poll_occupied);
    PERM_FREE(poll_data);
    PERM_FREE(poll_config);
    PERM_FREE(poll_expiration);
    PERM_FREE(free_slots);

    // Use the new arrays
    poll_array = new_poll_array;
    poll_occupied = new_poll_occupied;
    poll_data = new_poll_data;
    poll_config = new_poll_config;
    poll_expiration = new_poll_expiration;
    free_slots = new_free_slots;
    array_size = numDesc;

    return PR_SUCCESS;
}

PRStatus
PollArray::Grow()
{
    // We should only be called to grow a completely full poll array
    PR_ASSERT(poll_count == array_size);

    // Since the poll arrays are initially set to accomodate the number of
    // connections allowed, we only need to grow them when new acceptors are
    // configured.  We don't expect many acceptors, so grow only by a small
    // amount: the greater of 10 poll descriptors or 1% of the existing poll
    // array size.
    PRInt32 delta = array_size / 100;
    if (delta < 10)
        delta = 10;

    // Attempt to grow the poll array
    if (Resize(array_size + delta) != PR_SUCCESS)
        return PR_FAILURE;

    return PR_SUCCESS;
}

// Compact
//
// Compact the keepalive poll array.   After this routine,
// there is no free list, and no padding in the poll_array.
void 
PollArray::Compact()
{
    // If the poll array is less than 80% full, compact it.  There is a
    // benefit to using sparse poll arrays on Solaris where reusing the same
    // pollset pointer and length allows the implementation to reuse data.
    //
    // The > 10 (arbitrary) check keeps us from thrashing on low numbers,
    // where the size of the poll array won't make any difference.
    if (poll_count > 10 && active < (poll_count*8/10)) {
	PRInt32 curr_count = 0;
	PRInt32 i; 

	// Straightforward in-place compaction of the arrays;
	// keep poll_array and poll_conns parallel.
	ereport(LOG_VERBOSE, "PollArray compact (%d %d)", active, poll_count);
	for (i = 0; i < poll_count; i++) {

		if (!poll_occupied[i]) {	// Slot empty 
			continue;
		} 

		if (i == curr_count) {		// No movement yet 
			curr_count++;	
			continue;
		}

		// Pull 'i' forward to curr_count 
        PR_ASSERT(!poll_occupied[curr_count]);
        poll_occupied[curr_count] = poll_occupied[i];
        poll_data[curr_count] = poll_data[i];
        poll_config[curr_count] = poll_config[i];
        poll_expiration[curr_count] = poll_expiration[i];
        poll_occupied[i] = PR_FALSE;

        PollAdapter::swapDesc(poll_array, curr_count, i);
		curr_count++;
	}

	poll_count = curr_count;
	free_count = 0;
    }
}
