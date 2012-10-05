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
 * Use this hash table if you don't trust roguewave or other prebuilt hashes.
 * This one lets you control the locks; rwlocks are employed throughout.
 *
 * Mike Belshe
 * 09-21-97
 */

#ifndef _SIMPLEHASH_H_
#define _SIMPLEHASH_H_

#include "nspr.h"
#include "NsprWrap/ReadWriteLock.h"
#ifdef AIX
#include <strings.h>
#endif
#include <string.h>

#ifdef WIN32
#ifdef BUILD_HASH_DLL
#define HASH_EXPORT __declspec(dllexport)
#else
#define HASH_EXPORT __declspec(dllimport)
#endif
#else
#define HASH_EXPORT
#endif

#ifdef WIN32
#ifndef strcasecmp
#define strcasecmp(x,y) stricmp(x,y)
#endif
#endif
#define TOLOWER(x) (x>='A'&&x<='Z'?x|0x20:x)

/***************************************
 The read-write lock is faster when doing 100% reads.
 But if we're doing mostly native threads, the insert/delete time is
 so high it makes this prohibitively expensive.  For now, use 
 normal locks.

 Times are in ms.
	 NATIVE NATIVE  FIBERS  FIBERS
	 NORMAL	RWLOCK	NORMAL	RWLOCK
 inserts    418  14899     234     199
 lookups    331     69     119     106
 deletes    504  14878     251     205
 ***************************************/

// ruslan: since it's not Fargo anymore - user RW locks since we're doing mostly reads
#define USE_READ_WRITE_LOCK

class HASH_EXPORT SimpleHashIterator;
class HASH_EXPORT SimpleHashUnlockedIterator;

template <class Data, class HashKey> class hashEntry
{
    public:
        HashKey* key;
        Data*data;
        hashEntry<Data, HashKey> *prev, *next;
};

/*
 * Template abstract base class for hash tables
 */
template <class Data, class HashKey> class HASH_EXPORT SimpleHashTemplateBase
{
    public:
        SimpleHashTemplateBase(unsigned long maxSize, unsigned long expectedSize);
        ~SimpleHashTemplateBase();

        unsigned long size();
        unsigned long numEntries();

        void setMixCase();
        PRBool isMixedCase();

        virtual unsigned long keyhash(HashKey* key) = 0;
        virtual PRBool keycompare(HashKey* key1, HashKey* key2) = 0;
        virtual HashKey* keycopy(HashKey* key) = 0;
        virtual void keydelete(HashKey* key);
        virtual void datadelete(HashKey* key);

        void lock  ();
        void unlock();
        void readLock ();

        unsigned long hash(HashKey* key);

        // Caller is responsible for doing locking
        Data* lookup(HashKey* key);
        Data* lookup(unsigned long bucket, HashKey* key);
        PRBool insert(HashKey* key, Data*data);
        Data* remove(HashKey* key);
        void removeAll();

        // These do internal locking; caller should not hold lock
        Data* lockedLookup(HashKey* key);
        PRBool lockedInsert(HashKey* key, Data*data);
        Data* lockedRemove(HashKey* key);

    private:
        SimpleHashTemplateBase(const SimpleHashTemplateBase< Data, HashKey >&);
        SimpleHashTemplateBase< Data, HashKey >& operator=(const SimpleHashTemplateBase< Data, HashKey >&);

        friend class SimpleHashUnlockedIterator;

        PRBool _mixCase;

        typedef struct poolMemory
        {
            void *memory;
            struct poolMemory *next;
        } poolMemory;

        void _createFreePool(unsigned long num);
        void _destroyFreePool();
        void _putFreePool(hashEntry<Data,HashKey> *);
        hashEntry<Data,HashKey> *_freePool;
        unsigned long _freePoolIncrement;
        poolMemory *_poolMemory;

#ifdef USE_READ_WRITE_LOCK
        ReadWriteLock _lock;
#else
        CriticalSection _lock;
#endif
        hashEntry<Data,HashKey> **_hashTable;
        unsigned long _hashTableSize;
        unsigned long _numHashEntries;

        void _resize();
        void _insert(unsigned long bucket, hashEntry<Data,HashKey> *entry);
        hashEntry<Data,HashKey> *_lookup(unsigned long bucket, HashKey* key);
        hashEntry<Data,HashKey> *_getFreePool();
};


//----------------------------------------------------------------------------

#define SimpleHash SimpleHashTemplateBase<void,void>

class HASH_EXPORT SimpleIntHash : public SimpleHash
{
    public:
        SimpleIntHash(unsigned long size);
        ~SimpleIntHash();

        unsigned long keyhash(void *key);
        PRBool keycompare(void *key1, void *key2);
        void *keycopy(void *key);
};

//----------------------------------------------------------------------------

class HASH_EXPORT SimpleTwoIntHash : public SimpleHash
{
    public:
        SimpleTwoIntHash(unsigned long size);
        ~SimpleTwoIntHash();

        class keytype
        {
            public:
                keytype(unsigned long _key1, unsigned long _key2);
                unsigned long key1, key2;
        };

        unsigned long keyhash(void *key);
        PRBool keycompare(void *key1, void *key2);
        void *keycopy(void *key);
};

//----------------------------------------------------------------------------

/*
 * Keys are strdup()ed and their memory is managed by the hash table.
 * Memory for the data (values) is NOT managed by the hash table. It is
 * the user's responsiblity to release any memory allocated for values
 * inserted into the hash table.
 *
 * Alternatively, one can create a subclass and provide an 
 * implementation for the datadelete() method. In this method, you
 * must cast the void* pointer to the appropriate class pointer and then
 * invoke delete on that pointer.
 */
class HASH_EXPORT SimpleStringHash : public SimpleHash
{
    public:
        SimpleStringHash(unsigned long size);
        ~SimpleStringHash();

        unsigned long keyhash(void *key);
        PRBool keycompare(void *key1, void *key2);
        void *keycopy(void *key);
        void keydelete(void *key);
};

//----------------------------------------------------------------------------
/*
 * Memory for both keys and datavalues are NOT managed by the table. It
 * is the user's responsibility to ensure that these memory pointers remain
 * valid for the lifetime of the hashtable and that they get freed.
 */
class HASH_EXPORT SimplePtrStringHash : public SimpleHash
{
    public:
        SimplePtrStringHash(unsigned long size);
        ~SimplePtrStringHash();

        unsigned long keyhash(void *key);
        PRBool keycompare(void *key1, void *key2);
        void *keycopy(void *key);
        void keydelete(void *key);
};

//----------------------------------------------------------------------------

/*
 * Keys are malloc()ed and their memory is managed by the hash table.
 * Memory for the data (values) is NOT managed by the hash table. It is
 * the user's responsiblity to release any memory allocated for values
 * inserted into the hash table.
 */
class HASH_EXPORT SimpleNetAddrHash : public SimpleHash
{
    public:
        SimpleNetAddrHash(unsigned long size = 32);
        ~SimpleNetAddrHash();

        unsigned long keyhash(void *key);
        PRBool keycompare(void *key1, void *key2);
        void *keycopy(void *key);
        void keydelete(void *key);
};

//----------------------------------------------------------------------------

// this hash iterator doesn't lock the table
class HASH_EXPORT SimpleHashUnlockedIterator
{
    public:
        SimpleHashUnlockedIterator(SimpleHash *table);
        ~SimpleHashUnlockedIterator();

        // Find the next item in the hash table
        inline void *next();

        // Get the hash table key for the current item
        inline void *getKey();

        void **getValPtr();
        void **getKeyPtr();
    protected:
        SimpleHash *_hash;
    private:
        unsigned long _bucket;
        hashEntry<void,void> *_bucketPtr;
};

// this hash iterator locks the table
class HASH_EXPORT SimpleHashIterator : public SimpleHashUnlockedIterator
{
    public:
        SimpleHashIterator(SimpleHash *table);
        ~SimpleHashIterator();
};

template <class Data, class HashKey> class HASH_EXPORT SimpleHashTemplate: public SimpleHashTemplateBase<Data, HashKey>
{
    public:
        SimpleHashTemplate(unsigned long maxSize, unsigned long expectedSize);
        ~SimpleHashTemplate();
        virtual unsigned long keyhash(HashKey* key);
        virtual PRBool keycompare(HashKey* key1, HashKey* key2);
        virtual HashKey* keycopy(HashKey* key);
        virtual void keydelete(HashKey* key);
        virtual void datadelete(HashKey* key);
};

// you must derive from this to create a key class
class KeyBase
{
    public:
        virtual unsigned long hash() = 0; // hash key to int
        // a copy constructor is also needed, as well as an equality operator (==)
};

// SimpleHashTemplateBase

template <class Data, class HashKey> SimpleHashTemplateBase<Data,HashKey>::SimpleHashTemplateBase(unsigned long maxSize, unsigned long expectedSize)
{
    if (maxSize <= 0)   // sanity - avoid a crash when doing a modulus 0
        maxSize = 1;
    _hashTableSize = maxSize;
    _numHashEntries = 0;
    _hashTable = (hashEntry<Data,HashKey> **)calloc(1, sizeof(hashEntry<Data,HashKey> *) * _hashTableSize);

    _freePool = 0;
    _poolMemory = 0;
    _freePoolIncrement = expectedSize;
    _createFreePool(expectedSize);
 
    _mixCase = PR_FALSE;
};

template <class Data, class HashKey> SimpleHashTemplateBase<Data,HashKey>::~SimpleHashTemplateBase()
{
    // free memory allocated (if any) for keys/data
	removeAll();

    // to find memory in use; just delete the free pool.

    _destroyFreePool();

    free(_hashTable);
};

template <class Data, class HashKey> PRBool SimpleHashTemplateBase<Data,HashKey>::insert(HashKey* key, Data* data)
{
    unsigned long bucket = hash(key);
    PRBool rv;

    if (!lookup(bucket, key)) {
        hashEntry<Data,HashKey> *entry = _getFreePool();
        entry->key = keycopy(key);
        entry->data = data;

        _insert(bucket, entry);

        _numHashEntries++;

        if (_numHashEntries > _hashTableSize)
            _resize();

        rv = PR_TRUE;
    } else {
        rv = PR_FALSE;
    }
  
    return rv;
};

template <class Data, class HashKey> Data* SimpleHashTemplateBase<Data,HashKey>::remove(HashKey* key)
{
    unsigned long bucket = hash(key);
    Data* rv;
    hashEntry<Data,HashKey> *entry;

    if ((entry = _lookup(bucket, key)) != NULL) {
        if (entry->prev)
            entry->prev->next = entry->next;
        if (entry->next)
            entry->next->prev = entry->prev;
        if (_hashTable[bucket] == entry)
            _hashTable[bucket] = entry->next;

        keydelete(entry->key);
        rv = entry->data;

        _putFreePool(entry);

        _numHashEntries--;
    } else {
        rv = 0;
    } 

    return rv;
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::removeAll()
{
    if (_numHashEntries > 0)
    {
        unsigned long bucket;
        hashEntry<Data,HashKey> *entry;

        for (bucket = 0; bucket < _hashTableSize; bucket++) {
            for (entry = _hashTable[bucket]; entry; ) {
                hashEntry<Data,HashKey> *dead;
                dead = entry;
                entry = entry->next;
                keydelete(dead->key);
                datadelete(dead->data);
                _putFreePool(dead);
            }
            _hashTable[bucket] = NULL;
        }
    }
    _numHashEntries = 0;
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::_createFreePool(unsigned long num)
{
    unsigned long index;
    hashEntry<Data,HashKey> *newEntries;
    poolMemory *newMemory;

    newEntries = (hashEntry<Data,HashKey> *)malloc(sizeof(hashEntry<Data,HashKey>)*num);
    if (newEntries) {

        newMemory = (poolMemory *)malloc(sizeof(poolMemory));
        newMemory->memory = newEntries;
        newMemory->next = _poolMemory;
        _poolMemory = newMemory;

        for (index=0; index<num; index++) {
            _putFreePool(&newEntries[index]);
        }
    }
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::_destroyFreePool()
{
    poolMemory *ptr;

    for (ptr=_poolMemory; ptr; ) {
        poolMemory *dead = ptr;
        ptr = ptr->next;
        free(dead->memory);
        free(dead);
    }
    _poolMemory = NULL;
    _freePool = NULL;
};

template <class Data, class HashKey> unsigned long SimpleHashTemplateBase<Data,HashKey>::size()
{
    return _hashTableSize;
};

template <class Data, class HashKey> unsigned long SimpleHashTemplateBase<Data,HashKey>::numEntries()
{
    return _numHashEntries;
};

template <class Data, class HashKey> 
void SimpleHashTemplateBase<Data,HashKey>::_resize()
{
    // Keep the old hash table around until we're done with it
    unsigned long hashTableSize = _hashTableSize;
    hashEntry<Data,HashKey> **hashTable = _hashTable;

    // Replace the old hash table with a new, bigger (empty) hash table
    _hashTableSize *= 2;
    _hashTable = (hashEntry<Data,HashKey> **)calloc(1, sizeof(hashEntry<Data,HashKey> *) * _hashTableSize);

    // Move entries from the old hash table to the new
    unsigned long bucket;
    for (bucket = 0; bucket < hashTableSize; bucket++) {
        hashEntry<Data,HashKey> *entry;
        hashEntry<Data,HashKey> *next;
        for (entry = hashTable[bucket]; entry; entry = next) {
            next = entry->next;
            _insert(hash(entry->key), entry);
        }
    }

    // Discard the old hash table
    free(hashTable);
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::_insert(unsigned long bucket, hashEntry<Data,HashKey> *entry)
{
    entry->prev = NULL;
    if (_hashTable[bucket]) {
        entry->next = _hashTable[bucket];
        _hashTable[bucket]->prev = entry;
    } else
        entry->next = NULL;
    _hashTable[bucket] = entry;
};

template <class Data, class HashKey> 
//SimpleHashTemplateBase<Data,HashKey>::
hashEntry<Data,HashKey>* SimpleHashTemplateBase<Data,HashKey>::_lookup(unsigned long bucket, HashKey* key)
{
    hashEntry<Data,HashKey> *ptr;
    
    for (ptr = _hashTable[bucket]; ptr; ptr = ptr->next)
        if (keycompare(key, ptr->key))
            break;
    return ptr;
};

template <class Data, class HashKey> hashEntry<Data,HashKey>* SimpleHashTemplateBase<Data,HashKey>::_getFreePool()
{
    hashEntry<Data,HashKey> *entry;
    
    if (!_freePool)
    {
        _createFreePool(_freePoolIncrement);
    };
    
    entry = _freePool;
    _freePool = _freePool->next;
    
    return entry;
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::keydelete(HashKey* key)
{
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::datadelete(HashKey* key)
{
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::setMixCase()
{
        _mixCase = PR_TRUE;
};

template <class Data, class HashKey> PRBool SimpleHashTemplateBase<Data,HashKey>::isMixedCase()
{
    return _mixCase;
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::lock()
{
#ifdef USE_READ_WRITE_LOCK
    _lock.acquireWrite();
#else
    _lock.acquire();
#endif
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::readLock()
{
#ifdef USE_READ_WRITE_LOCK
    _lock.acquireRead ();
#else
    _lock.acquire();
#endif
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::unlock()
{
    _lock.release();
};

template <class Data, class HashKey> unsigned long SimpleHashTemplateBase<Data,HashKey>::hash(HashKey* key)
{
    return keyhash(key) % _hashTableSize;
};

template <class Data, class HashKey> Data* SimpleHashTemplateBase<Data,HashKey>::lookup(unsigned long bucket, HashKey* key)
{
    hashEntry<Data,HashKey> *ptr;
    ptr = _lookup(bucket, key);
    if (ptr)
        return ptr->data;
    return NULL;
};

template <class Data, class HashKey> Data* SimpleHashTemplateBase<Data,HashKey>::lookup(HashKey* key)
{
    unsigned long bucket = hash(key);

    return lookup(bucket, key);
};

template <class Data, class HashKey> void SimpleHashTemplateBase<Data,HashKey>::_putFreePool(hashEntry<Data,HashKey> *entry)
{
    entry->next = _freePool;
    _freePool = entry;
};

template <class Data, class HashKey> Data* SimpleHashTemplateBase<Data,HashKey>::lockedLookup(HashKey* key)
{
    Data* rv;

#ifdef USE_READ_WRITE_LOCK
    _lock.acquireRead();
#else
    _lock.acquire();
#endif
    rv = lookup(key);
    _lock.release();
 
    return rv;
};

template <class Data, class HashKey> PRBool SimpleHashTemplateBase<Data,HashKey>::lockedInsert(HashKey* key, Data* data)
{
    PRBool rv;

    lock();
    rv = insert(key, data);
    unlock();
 
    return rv;
};

template <class Data, class HashKey> Data* SimpleHashTemplateBase<Data,HashKey>::lockedRemove(HashKey* key)
{
    Data* rv;

    lock();
    rv = remove(key);
    unlock();
 
    return rv;
};

// SimpleHashTemplate

template <class Data, class HashKey> SimpleHashTemplate<Data,HashKey>::SimpleHashTemplate(unsigned long maxSize, unsigned long expectedSize) :
    SimpleHashTemplateBase<Data, HashKey> (maxSize, expectedSize)
{
};

template <class Data, class HashKey> SimpleHashTemplate<Data,HashKey>::~SimpleHashTemplate()
{
};

template <class Data, class HashKey> unsigned long SimpleHashTemplate<Data,HashKey>::keyhash(HashKey* key)
{
    return key->hash();
};

template <class Data, class HashKey> PRBool SimpleHashTemplate<Data,HashKey>::keycompare(HashKey* key1, 
                                                                                                HashKey* key2)
{
    if (key1 && key2 && ( (*key1).operator == (*key2) ) )
        return PR_TRUE;
    else
        return PR_FALSE;
};

template <class Data, class HashKey> HashKey* SimpleHashTemplate<Data,HashKey>::keycopy(HashKey* key)
{
    if (key)
        return new HashKey(*key);
    else
        return NULL;
};

template <class Data, class HashKey> void SimpleHashTemplate<Data,HashKey>::keydelete(HashKey* key)
{
    delete key;
};

template <class Data, class HashKey> void SimpleHashTemplate<Data,HashKey>::datadelete(HashKey* key)
{
    Data* data = lookup(key);
    if (data)
    {
        delete data;
    };
};

// SimpleHashUnlockedIterator

void *
SimpleHashUnlockedIterator::next()
{
    if (_bucketPtr) {
        if (_bucketPtr->next) {
            _bucketPtr = _bucketPtr->next;
            return _bucketPtr->data;
        } else
            _bucket++;
    }

    for ( ; _bucket < _hash->_hashTableSize; _bucket++) {
        _bucketPtr = _hash->_hashTable[_bucket];
        if (_bucketPtr)
            break;
    }

    if (_bucket < _hash->_hashTableSize && _bucketPtr)
        return _bucketPtr->data;
    return NULL;
};

void *
SimpleHashUnlockedIterator::getKey()
{
    if (_bucketPtr)
        return _bucketPtr->key;
    return NULL;

};

#endif

