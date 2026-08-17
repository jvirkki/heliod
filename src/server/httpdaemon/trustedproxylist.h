/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright (c) 2026, The Heliod Web Server Project. All rights reserved.
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

/* Joe McCabe
 *
 * trustedproxylist.h
 *
 * TrustedProxyList holds a set of IP/CIDR network ranges and answers
 * whether a given peer address falls within any of them. It's used to
 * decide whether a reverse proxy's forwarded-for header may be trusted
 * for perfdump/errors log attribution purposes (see the ReverseProxy*
 * magnus.conf directives and the <reverse-proxy> server.xml element).
 */

#ifndef TRUSTEDPROXYLIST_H
#define TRUSTEDPROXYLIST_H

#include <nspr.h>

class TrustedProxyList {
public:
    TrustedProxyList();
    TrustedProxyList(const TrustedProxyList& other);
    TrustedProxyList& operator=(const TrustedProxyList& other);
    ~TrustedProxyList();

    /**
     * Parses a whitespace/comma-separated list of "ip" or "ip/prefixlen"
     * entries (IPv4 or IPv6) from spec and appends them to this list.
     *
     * Returns PR_TRUE on success. On failure returns PR_FALSE, and, if
     * errbuf is non-NULL, writes a human-readable message (truncated to
     * fit) into errbuf.
     */
    PRBool add(const char *spec, char *errbuf = NULL, int errbuflen = 0);

    /**
     * Returns PR_TRUE if addr falls within any of the configured networks.
     */
    PRBool contains(const PRNetAddr *addr) const;

    /**
     * Returns PR_TRUE if no networks have been configured.
     */
    PRBool isEmpty() const { return count_ == 0; }

private:
    struct Entry {
        PRNetAddr network;
        int prefixLen;
    };

    PRBool addOne(char *token, char *errbuf, int errbuflen);
    void ensureCapacity();
    void copyFrom(const TrustedProxyList& other);

    Entry *entries_;
    int count_;
    int capacity_;
};

#endif // TRUSTEDPROXYLIST_H
