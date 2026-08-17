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

 * Joe McCabe
 * 
 * Determines whether perfdump and the errors log report the raw TCP peer IP
 * or a reverse-proxy-forwarded client IP. See also the per-virtual-server
 * <reverse-proxy> server.xml element,  which can override any of these settings.

 * This does NOT change what is logged in the access log. An administrator
 * must change the access log format to change IP addresses logged there.

 */

#include "httpdaemon/trustedproxylist.h"

#include <string.h>
#include <stdlib.h>

#include "netsite.h"
#include "base/util.h"

//-----------------------------------------------------------------------------
// matchPrefix
//
// Returns PR_TRUE if the first prefixLen bits of the byte strings a and b
// are identical.
//-----------------------------------------------------------------------------

static PRBool matchPrefix(const PRUint8 *a, const PRUint8 *b, int prefixLen)
{
    int fullBytes = prefixLen / 8;
    int remBits = prefixLen % 8;

    if (fullBytes > 0 && memcmp(a, b, fullBytes) != 0)
        return PR_FALSE;

    if (remBits > 0) {
        PRUint8 mask = (PRUint8)(0xFF << (8 - remBits));
        if ((a[fullBytes] & mask) != (b[fullBytes] & mask))
            return PR_FALSE;
    }

    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// collapseV4Mapped
//
// If addr is an IPv4-mapped IPv6 address (::ffff:a.b.c.d), rewrites out as
// the equivalent plain IPv4 address. Otherwise leaves out untouched.
// Returns the address that should be used for comparisons.
//-----------------------------------------------------------------------------

static const PRNetAddr *collapseV4Mapped(const PRNetAddr *addr, PRNetAddr *out)
{
    if (addr->raw.family == PR_AF_INET6 &&
        PR_IsNetAddrType(addr, PR_IpAddrV4Mapped))
    {
        memset(out, 0, sizeof(*out));
        memcpy(&out->inet.ip, addr->ipv6.ip.pr_s6_addr + 12, 4);
        out->inet.family = PR_AF_INET;
        out->raw.family = PR_AF_INET;
        return out;
    }

    return addr;
}

//-----------------------------------------------------------------------------
// TrustedProxyList::TrustedProxyList
//-----------------------------------------------------------------------------

TrustedProxyList::TrustedProxyList()
: entries_(NULL), count_(0), capacity_(0)
{
}

//-----------------------------------------------------------------------------
// TrustedProxyList::TrustedProxyList (copy constructor)
//-----------------------------------------------------------------------------

TrustedProxyList::TrustedProxyList(const TrustedProxyList& other)
: entries_(NULL), count_(0), capacity_(0)
{
    copyFrom(other);
}

//-----------------------------------------------------------------------------
// TrustedProxyList::operator=
//-----------------------------------------------------------------------------

TrustedProxyList& TrustedProxyList::operator=(const TrustedProxyList& other)
{
    if (this != &other) {
        count_ = 0;
        copyFrom(other);
    }
    return *this;
}

//-----------------------------------------------------------------------------
// TrustedProxyList::copyFrom
//-----------------------------------------------------------------------------

void TrustedProxyList::copyFrom(const TrustedProxyList& other)
{
    for (int i = 0; i < other.count_; i++) {
        ensureCapacity();
        entries_[count_++] = other.entries_[i];
    }
}

//-----------------------------------------------------------------------------
// TrustedProxyList::~TrustedProxyList
//-----------------------------------------------------------------------------

TrustedProxyList::~TrustedProxyList()
{
    if (entries_)
        PERM_FREE(entries_);
}

//-----------------------------------------------------------------------------
// TrustedProxyList::ensureCapacity
//-----------------------------------------------------------------------------

void TrustedProxyList::ensureCapacity()
{
    if (count_ < capacity_)
        return;

    int newCapacity = capacity_ ? capacity_ * 2 : 4;
    Entry *newEntries = (Entry *)PERM_MALLOC(sizeof(Entry) * newCapacity);

    if (entries_) {
        memcpy(newEntries, entries_, sizeof(Entry) * count_);
        PERM_FREE(entries_);
    }

    entries_ = newEntries;
    capacity_ = newCapacity;
}

//-----------------------------------------------------------------------------
// TrustedProxyList::addOne
//-----------------------------------------------------------------------------

PRBool TrustedProxyList::addOne(char *token, char *errbuf, int errbuflen)
{
    int prefixLen = -1;

    char *slash = strchr(token, '/');
    if (slash) {
        *slash = '\0';

        char *end = NULL;
        long v = strtol(slash + 1, &end, 10);
        if (*(slash + 1) == '\0' || (end && *end != '\0') || v < 0) {
            if (errbuf)
                util_snprintf(errbuf, errbuflen,
                               "invalid prefix length in \"%s/%s\"",
                               token, slash + 1);
            return PR_FALSE;
        }
        prefixLen = (int)v;
    }

    PRNetAddr net;
    memset(&net, 0, sizeof(net));
    if (PR_StringToNetAddr(token, &net) != PR_SUCCESS) {
        if (errbuf)
            util_snprintf(errbuf, errbuflen, "invalid IP address \"%s\"", token);
        return PR_FALSE;
    }

    PRNetAddr collapsed;
    const PRNetAddr *canonical = collapseV4Mapped(&net, &collapsed);

    int maxPrefix = (canonical->raw.family == PR_AF_INET6) ? 128 : 32;

    if (prefixLen == -1) {
        prefixLen = maxPrefix;
    } else if (prefixLen > maxPrefix) {
        if (errbuf)
            util_snprintf(errbuf, errbuflen,
                           "prefix length %d is out of range for \"%s\"",
                           prefixLen, token);
        return PR_FALSE;
    }

    ensureCapacity();
    entries_[count_].network = *canonical;
    entries_[count_].prefixLen = prefixLen;
    count_++;

    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// TrustedProxyList::add
//-----------------------------------------------------------------------------

PRBool TrustedProxyList::add(const char *spec, char *errbuf, int errbuflen)
{
    if (!spec || !*spec)
        return PR_TRUE;

    char *copy = PERM_STRDUP(spec);
    if (!copy) {
        if (errbuf)
            util_snprintf(errbuf, errbuflen, "out of memory");
        return PR_FALSE;
    }

    PRBool rv = PR_TRUE;
    char *lasts = NULL;
    char *tok = util_strtok(copy, ", \t\r\n", &lasts);
    while (tok) {
        if (!addOne(tok, errbuf, errbuflen)) {
            rv = PR_FALSE;
            break;
        }
        tok = util_strtok(NULL, ", \t\r\n", &lasts);
    }

    PERM_FREE(copy);
    return rv;
}

//-----------------------------------------------------------------------------
// TrustedProxyList::contains
//-----------------------------------------------------------------------------

PRBool TrustedProxyList::contains(const PRNetAddr *addr) const
{
    if (!addr || count_ == 0)
        return PR_FALSE;

    PRNetAddr collapsed;
    const PRNetAddr *canonical = collapseV4Mapped(addr, &collapsed);

    for (int i = 0; i < count_; i++) {
        const Entry &e = entries_[i];

        if (canonical->raw.family != e.network.raw.family)
            continue;

        if (canonical->raw.family == PR_AF_INET) {
            if (matchPrefix((const PRUint8 *)&canonical->inet.ip,
                             (const PRUint8 *)&e.network.inet.ip,
                             e.prefixLen))
                return PR_TRUE;
        } else if (canonical->raw.family == PR_AF_INET6) {
            if (matchPrefix(canonical->ipv6.ip.pr_s6_addr,
                             e.network.ipv6.ip.pr_s6_addr,
                             e.prefixLen))
                return PR_TRUE;
        }
    }

    return PR_FALSE;
}
