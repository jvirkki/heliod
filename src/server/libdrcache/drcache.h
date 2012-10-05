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

#ifndef __DRCACHE_H__
#define __DRCACHE_H__

#include "drnsapi.h"

#define MIN_SIZE 16

class DrHashList;

class DrHashNode
{
public:
	~DrHashNode();
	DrHashNode();
	DrHashNode(Entry *&entry, const char *newKey, PRUint32 klen);
	DrHashNode(Entry *&entry, const char *newKey, PRUint32 klen,
						PRIntervalTime timeout);
	Entry *replaceEntry(Entry *&entry);
	Entry *replaceEntry(Entry *&entry, PRIntervalTime timeout);
	void linkNode(DrHashNode &newNode);
	void ageLock(void);
	void ageUnlock(void);
	Entry *nodeLockRead(void);
	Entry *nodeLockWrite(void);
	void nodeUnlock(void);
	PRBool  isExpired(PRIntervalTime range);
	PRBool  isExpired(void);
private:
	Entry          *entry;
	char           *key;
	DrHashNode     *next;
	DrHashNode     *prev;
	PRLock         *exAgeLock;
	PRRWLock       *rwNodeLock;
	PRUint32        keyLen;
	PRIntervalTime lastAge; 
	PRIntervalTime maxAge; 
	friend class DrHashList;
};

class DrHashList
{
public:
	~DrHashList();
	DrHashList();
	DrHashList(CompareFunc_t cmp, RefreshFunc_t ref, FreeFunc_t fre);
	DrHashNode *getEntry(const char *&key, PRUint32 len,
							Request * rq, Session *sn);
	DrHashNode *tryGetEntry(const char *&key, PRUint32 len,
							Request * rq, Session *sn);
	DrHashNode *getUnexpiredEntry(const char *&key, PRUint32 len,
							PRIntervalTime range, Request * rq, Session *sn);
	DrHashNode *forceRefresh(const char *&key, PRUint32 len,
							Request * rq, Session *sn);
	DrHashNode *refreshEntry(const char *&key, PRUint32 len,
							PRIntervalTime timeout, Entry *entry);
	CompareFunc_t  fnCmp;
	RefreshFunc_t  fnRef;
	FreeFunc_t     fnFre;
private:
	DrHashNode *head;
	DrHashNode *tail;
	PRRWLock   *rwListLock;
	PRUint32   ctr;
	DrHashNode *putEntry(const char *&key, PRUint32 len,
							PRBool ifForce, Request * rq, Session *sn);
	DrHashNode *putEntry(const char *&key, PRUint32 len, PRIntervalTime timeout,
							Entry *entry);
};

class DrHashTable
{
public:
	~DrHashTable();
	DrHashTable();
	DrHashTable(PRUint32 maxEntries, PRIntervalTime maxAge, CompareFunc_t cmp,
							RefreshFunc_t ref, FreeFunc_t fre);
	DrHashNode *getEntry(const char *&key, PRUint32 len,
							Request * rq, Session *sn);
	DrHashNode *tryGetEntry(const char *&key, PRUint32 len,
							Request * rq, Session *sn);
	DrHashNode *getUnexpiredEntry(const char *&key, PRUint32 len,
							Request * rq, Session *sn);
	DrHashNode *forceRefresh(const char *&key, PRUint32 len,
							Request * rq, Session *sn);
	DrHashNode *putEntry(const char *&key, PRUint32 len, PRIntervalTime timeout,
							Entry *entry);
	CompareFunc_t  fnCmp;
	RefreshFunc_t  fnRef;
	FreeFunc_t     fnFre;
protected:
	DrHashList     **hashTable;
	PRUint32       hashSize;
	PRUint32       hashMask;
	PRIntervalTime maxEntryAge;
private:
	PRRWLock       *rwTableLock;
	PRUint32       hashIt(const char *&key, PRUint32 len);
	DrHashNode     *putEntry(const char *&key, PRUint32 len,
							PRBool ifForce, Request * rq, Session *sn);
};
#endif
