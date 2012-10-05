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
 * simple hash algorithm.
 *
 * Mike Belshe
 * 09-21-97
 */

#include "SimpleHash.h"

/*

//The following is a test showing how to use the SimpleHashTemplate

class anint: public KeyBase
{
    public:
        anint(int val)
        {
            value = val;
        };

        anint(anint& src)
        {
            value = src.hash();
        };

        unsigned long hash()
        {
            return value;
        };

        operator == (anint& src)
        {
            if (src.hash() == value)
                return 1;
            else
                return 0;
        };

    protected:
        unsigned long value;
};

void test()
{
    SimpleHashTemplate<void,anint> inttable(100, 5);
};

*/

// SimpleIntHash

SimpleIntHash :: SimpleIntHash(unsigned long size) : SimpleHash(size, 32)
{
};

SimpleIntHash :: ~SimpleIntHash()
{
};

unsigned long
SimpleIntHash::keyhash(void *_key)
{
    unsigned long key = (unsigned long)_key;

    return key;
};

PRBool
SimpleIntHash::keycompare(void *_key1, void *_key2)
{
    return (_key1 == _key2)?PR_TRUE:PR_FALSE;
};

void *
SimpleIntHash::keycopy(void *_key)
{
    return _key;
};

// SimpleTwoIntHash

unsigned long
SimpleTwoIntHash::keyhash(void *_key)
{
    keytype *key = (keytype *)_key;

    return key->key1 + key->key2;
};

PRBool
SimpleTwoIntHash::keycompare(void *_key1, void *_key2)
{
    keytype *key1 = (keytype *)_key1;
    keytype *key2 = (keytype *)_key2;
    return (key1->key1 == key2->key1 && key1->key2 == key2->key2)?PR_TRUE:PR_FALSE;
};

void *
SimpleTwoIntHash::keycopy(void *_key)
{
    keytype *rv;
    keytype *srckey = (keytype *)_key;

    rv = (keytype *)malloc(sizeof(keytype));
    rv->key1 = srckey->key1;
    rv->key2 = srckey->key2;

    return rv;
};

SimpleTwoIntHash :: SimpleTwoIntHash(unsigned long size) : SimpleHash(size, 32)
{
};

SimpleTwoIntHash :: ~SimpleTwoIntHash()
{
    // free memory allocated (if any) for keys/data
    removeAll();
};

SimpleTwoIntHash :: keytype :: keytype(unsigned long _key1, unsigned long _key2) : key1(_key1), key2(_key2)
{
};

// SimpleStringHash

SimpleStringHash :: SimpleStringHash(unsigned long size) : SimpleHash(size, 32)
{
};

SimpleStringHash :: ~SimpleStringHash()
{
    // free memory allocated (if any) for keys/data
    removeAll();
};

unsigned long
SimpleStringHash::keyhash(void *_key)
{
    char *key = (char *)_key;
    register unsigned long hash = 0;

    if (isMixedCase()) {
        while(*key) {
            hash = (hash << 5) + hash + TOLOWER(*key);
            key++;
        }
    } else {
        while(*key) {
            hash = (hash << 5) + hash + *key;
            key++;
        }
    }

    return hash;
};

PRBool
SimpleStringHash::keycompare(void *_key1, void *_key2)
{
    char *key1 = (char *)_key1;
    char *key2 = (char *)_key2;

    if (isMixedCase()) {
        if (TOLOWER(key1[0]) != TOLOWER(key2[0]))
            return PR_FALSE;

        return (!strcasecmp(key1, key2))?PR_TRUE:PR_FALSE;
    } else {
        if (key1[0] != key2[0])
            return PR_FALSE;

        return (!strcmp(key1, key2))?PR_TRUE:PR_FALSE;
    }
};

void *
SimpleStringHash::keycopy(void *_key)
{
    char *key = (char *)_key;
    return strdup(key);
};

void
SimpleStringHash::keydelete(void *_key)
{
    free(_key);
};

// SimplePtrStringHash

SimplePtrStringHash :: SimplePtrStringHash(unsigned long size) : SimpleHash(size, 32)
{
};

SimplePtrStringHash :: ~SimplePtrStringHash()
{
};

unsigned long
SimplePtrStringHash::keyhash(void *_key)
{
    char *key = (char *)_key;
    register unsigned long hash = 0;

    if (isMixedCase()) {
        while(*key) {
            hash = (hash << 5) + hash + TOLOWER(*key);
            key++;
        }
    } else {
        while(*key) {
            hash = (hash << 5) + hash + *key;
            key++;
        }
    }

    return hash;
};

PRBool
SimplePtrStringHash::keycompare(void *_key1, void *_key2)
{
    char *key1 = (char *)_key1;
    char *key2 = (char *)_key2;

    if (isMixedCase()) {
        if (TOLOWER(key1[0]) != TOLOWER(key2[0]))
            return PR_FALSE;

        return (!strcasecmp(key1, key2))?PR_TRUE:PR_FALSE;
    } else {
        if (key1[0] != key2[0])
            return PR_FALSE;

        return (!strcmp(key1, key2))?PR_TRUE:PR_FALSE;
    }
};

void *
SimplePtrStringHash::keycopy(void *_key)
{
    return _key;
};

void
SimplePtrStringHash::keydelete(void *_key)
{
};

// SimpleNetAddrHash

SimpleNetAddrHash :: SimpleNetAddrHash(unsigned long size) : SimpleHash(size, 32)
{
};

SimpleNetAddrHash :: ~SimpleNetAddrHash()
{
    // free memory allocated (if any) for keys/data
    removeAll();
};

unsigned long
SimpleNetAddrHash::keyhash(void *_key)
{
    PRNetAddr *key = (PRNetAddr *)_key;

    switch (key->raw.family) {
    case PR_AF_INET:
        return key->inet.ip ^ (key->inet.port << 16);

#if defined(PR_AF_INET6)
    case PR_AF_INET6:
        return key->ipv6.ip.pr_s6_addr32[0] ^
               key->ipv6.ip.pr_s6_addr32[3] ^
               (key->ipv6.port << 16);
#endif
    }

    register unsigned long hash = 0;
    for (int i = 0; i < sizeof(key->raw.data); i++) {
        hash = (hash << 5) + hash + key->raw.data[i];
    }

    return hash;
};

PRBool
SimpleNetAddrHash::keycompare(void *_key1, void *_key2)
{
    PRNetAddr *key1 = (PRNetAddr *)_key1;
    PRNetAddr *key2 = (PRNetAddr *)_key2;

    if (key1->raw.family != key2->raw.family)
        return PR_FALSE;

    switch (key1->raw.family) {
    case PR_AF_INET:
        return key1->inet.ip == key2->inet.ip &&
               key1->inet.port == key2->inet.port;

#if defined(PR_AF_INET6)
    case PR_AF_INET6:
        return key1->ipv6.ip.pr_s6_addr32[0] == key2->ipv6.ip.pr_s6_addr32[0] &&
               key1->ipv6.ip.pr_s6_addr32[1] == key2->ipv6.ip.pr_s6_addr32[1] &&
               key1->ipv6.ip.pr_s6_addr32[2] == key2->ipv6.ip.pr_s6_addr32[2] &&
               key1->ipv6.ip.pr_s6_addr32[3] == key2->ipv6.ip.pr_s6_addr32[3] &&
               key1->ipv6.port == key2->ipv6.port;
#endif

#if defined(XP_UNIX)
    case PR_AF_LOCAL:
        return !strcmp(key1->local.path, key2->local.path);
#endif
    }

    for (int i = 0; i < sizeof(key1->raw.data); i++) {
        if (key1->raw.data[i] != key2->raw.data[i])
            return PR_FALSE;
    }

    return PR_TRUE;
};

void *
SimpleNetAddrHash::keycopy(void *_key)
{
    PRNetAddr *key = (PRNetAddr *)malloc(sizeof(PRNetAddr));
    if (key)
        memcpy(key, _key, sizeof(PRNetAddr));
    return key;
};

void
SimpleNetAddrHash::keydelete(void *_key)
{
    free(_key);
};

// SimpleHashUnlockedIterator

SimpleHashUnlockedIterator :: SimpleHashUnlockedIterator(SimpleHash *table) : _hash(table), _bucket(0), _bucketPtr(0)
{
};

SimpleHashUnlockedIterator :: ~SimpleHashUnlockedIterator()
{
};

void **
SimpleHashUnlockedIterator::getKeyPtr()
{
    if (_bucketPtr)
        return &(_bucketPtr->key);
    return NULL;
};

void **
SimpleHashUnlockedIterator::getValPtr()
{
    if (_bucketPtr)
        return &(_bucketPtr->data);
    return NULL;
};

// SimpleHashIterator

SimpleHashIterator :: SimpleHashIterator(SimpleHash *table) : SimpleHashUnlockedIterator(table)
{
    _hash->readLock();
};

SimpleHashIterator :: ~SimpleHashIterator()
{
    _hash->unlock();
};

