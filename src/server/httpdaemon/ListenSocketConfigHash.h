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

#ifndef _ListenSocketConfigHash_h_
#define _ListenSocketConfigHash_h_

#include "nspr.h"
#include "support/SimpleHash.h"
#include "support/GenericVector.h"
#include "httpdaemon/ListenSocketConfig.h"

/**
 * Maintains a mapping between network addresses and listen socket
 * configurations.
 */

class ListenSocketConfigHash
{
    public:

        /**
         * Create an empty hash table.
         */
        ListenSocketConfigHash();

        /**
         * Destroy the hash table and release references to any
         * <code>ListenSocketConfig</code> pointers contained in it.
         */
        ~ListenSocketConfigHash();

        /**
         * Add a new <code>ListenSocketConfig</code> to the hash table. The
         * <code>ListenSocketConfigHash</code> increments <code>config</code>'s
         * refcount before returning.
         *
         * @param config Specifies the network address-specific
         *               <code>ListenSocketConfig</code> to add.
         */
        PRBool addConfig(ListenSocketConfig* config);

        /**
         * Return the number of <code>ListenSocketConfig</code> pointers stored
         * in the hash table.
         */
        int getConfigCount() const;

        /**
         * Return the <code>i</code>th <code>ListenSocketConfig</code> pointer.
         * The refcount of the returned <code>ListenSocketConfig</code> is not
         * incremented.
         *
         * @returns A pointer to a <code>ListenSocketConfig</code> stored in
         *          the hash table.
         */
        ListenSocketConfig* getConfig(int i) const;

        /**
         * Return a pointer to the <code>ListenSocketConfig</code> associated
         * with the specified network address or NULL if the specified network
         * address is not present in the hash table. The refcount of the
         * returned <code>ListenSocketConfig</code> is not incremented.
         *
         * @param addr Specifies the network address to lookup.
         * @returns    The <code>ListenSocketConfig*</code> associated with
         *             <code>addr</code>.
         */
        ListenSocketConfig* getConfig(PRNetAddr& addr);

    private:

        /**
         * Objects of this class cannot be copied.
         */
        ListenSocketConfigHash(const ListenSocketConfigHash& source);

        /**
         * Objects of this class cannot be copied.
         */
        ListenSocketConfigHash& operator=(const ListenSocketConfigHash& source);

        /**
         * Hashes IP addresses to <code>ListenSocketConfig</code> pointers.
         * The <code>ListenSocketConfigHash</code> holds a reference to each
         * <code>ListenSocketConfig</code> contained in the hash table.
         */
        SimpleNetAddrHash hash_;

        /**
         * A list of all the <code>ListenSocketConfig</code> pointers that are
         * stored in hash_.
         */
        GenericVector vector_;
};

inline
int
ListenSocketConfigHash::getConfigCount() const
{
    return vector_.length();
}

inline
ListenSocketConfig*
ListenSocketConfigHash::getConfig(int i) const
{
    return (ListenSocketConfig*)vector_[i];

}

inline
ListenSocketConfig*
ListenSocketConfigHash::getConfig(PRNetAddr& addr)
{
    return (ListenSocketConfig*)hash_.lookup(&addr);
}

#endif /* _ListenSocketConfigHash_h_ */
