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

#ifndef OUTPUTVECTOR_H
#define OUTPUTVECTOR_H

#include "nspr.h"


/* ----------------------------- OutputVector ----------------------------- */

struct OutputVector {
    /**
     * Construct an empty output vector. Note that adding data to the output
     * vector does not create a copy of the data.
     */
    OutputVector();

    /**
     * Mark the output vector as empty.
     */
    inline void reset();

    /**
     * Add data to the output vector. Returns 1 on success or 0 if there was
     * no room for the data in the iov[] array.
     */
    inline int addBuffer(const void *base, int len, int iov_slack = 0);

    /**
     * Add data to the output vector. The contents of the source iov[] array
     * are not modified. Returns the number of PRIOVec entries that were
     * accepted. This may be less than iov_size if there was not enough room in
     * the desintation iov[] array. iov_slack specifies the number of PRIOVec
     * entries to leave unused in the destination iov[] array.
     */
    inline int addIov(const PRIOVec *iov, int iov_size, int iov_slack = 0);

    /**
     * Add data to the output vector. The contents of the source iov[] array
     * are not modified. Returns the number of PRIOVec entries that were
     * accepted. This may be less than the source output vector's iov_size
     * if there was not enough room in the desintation iov[] array. iov_slack
     * specifies the number of PRIOVec entries to leave unused in the
     * destination iov[] array.
     */
    inline int add(const OutputVector &source, int iov_slack = 0);

    /**
     * Move data from the source iov[] array into the output vector. Returns
     * the number of PRIOVec entries that were moved. This may be less than the
     * source output vector's iov_size if there was not enough room in the
     * destination iov[] array. iov_slack specifies the number of PRIOVec
     * entries to leave unused in the destination iov[] array.
     */
    inline int moveFrom(OutputVector &source, int iov_slack = 0);

    /**
     * An array of entries suitable for passing to PR_Writev().
     */
    struct PRIOVec iov[PR_MAX_IOVECTOR_SIZE];

    /**
     * Number of valid entries in the iov[] array.
     */
    int iov_size;

    /**
     * Number of bytes of data in the iov[] array.
     */
    int amount;
};


/* ---------------------- OutputVector::OutputVector ---------------------- */

OutputVector::OutputVector()
: iov_size(0),
  amount(0)
{ }


/* ------------------------- OutputVector::reset -------------------------- */

inline void OutputVector::reset()
{
    iov_size = 0;
    amount = 0;
}


/* ----------------------- OutputVector::addBuffer ------------------------ */

inline int OutputVector::addBuffer(const void *base, int len, int iov_slack)
{
    PR_ASSERT(iov_size + iov_slack <= PR_MAX_IOVECTOR_SIZE);

    if (len > 0) {
        if (iov_size > 0 && (char *)iov[iov_size - 1].iov_base + iov[iov_size - 1].iov_len == (char *)base) {
            // Grow previous entry
            iov[iov_size - 1].iov_len += len;

        } else if (iov_size + iov_slack < PR_MAX_IOVECTOR_SIZE) {
            // Add new entry
            iov[iov_size].iov_base = (char *)base;
            iov[iov_size].iov_len = len;
            iov_size++;

        } else {
            // No room for a new entry
            return 0;
        }

        amount += len;
    }

    return 1;
}


/* ------------------------- OutputVector::addIov ------------------------- */

inline int OutputVector::addIov(const PRIOVec *incoming_iov, int incoming_iov_size, int iov_slack)
{
    PR_ASSERT(iov_size + iov_slack <= PR_MAX_IOVECTOR_SIZE);

    for (int i = 0; i < incoming_iov_size; i++) {
        if (incoming_iov[i].iov_len > 0) {
            if (iov_size > 0 && iov[iov_size - 1].iov_base + iov[iov_size - 1].iov_len == incoming_iov[i].iov_base) {
                // Grow previous entry
                iov[iov_size - 1].iov_len += incoming_iov[i].iov_len;

            } else if (iov_size + iov_slack < PR_MAX_IOVECTOR_SIZE) {
                // Add new entry
                iov[iov_size] = incoming_iov[i];
                iov_size++;

            } else {
                // No room for a new entry
                return i;
            }

            amount += incoming_iov[i].iov_len;
        }
    }

    return incoming_iov_size;
}


/* -------------------------- OutputVector::add --------------------------- */

inline int OutputVector::add(const OutputVector &source, int iov_slack)
{
    return addIov(source.iov, source.iov_size, iov_slack);
}


/* ------------------------ OutputVector::moveFrom ------------------------ */

inline int OutputVector::moveFrom(OutputVector &source, int iov_slack)
{
    int moved = addIov(source.iov, source.iov_size, iov_slack);

    if (moved > 0) {
        if (moved == source.iov_size) {
            // Source vector is now empty
            source.iov_size = 0;
            source.amount = 0;

        } else {
            // Source vector needs to be compacted (we do this rather than
            // maintain a pointer or offset into iov[] because web server will
            // rarely invoke this code but will often dereference the iov
            // pointer)
            source.iov_size -= moved;
            int i;
            for (i = 0; i < source.iov_size && i < moved; i++) {
                source.amount -= source.iov[i].iov_len;
                source.iov[i] = source.iov[i + moved];
            }
            for (; i < source.iov_size; i++)
                source.iov[i] = source.iov[i + moved];
            for (; i < moved; i++)
                source.amount -= source.iov[i].iov_len;
        }
    }

    return moved;
}

#endif /* OUTPUTVECTOR_H */
