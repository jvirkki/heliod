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

#include "drcache.h"

DrHashNode::~DrHashNode()
{
	lastAge = maxAge = 0;
	entry   = 0;
	next    = prev   = 0;
	keyLen  = 0;
	delete key;
	PR_DestroyLock(exAgeLock);
	PR_DestroyRWLock(rwNodeLock);
}

DrHashNode::DrHashNode()
{
	lastAge = maxAge = 0;
	entry   = 0;
	next    = prev   = 0;
	key     = 0;
	keyLen  = 0;
	exAgeLock  = 0;
	rwNodeLock = 0;
	
}

DrHashNode::DrHashNode(Entry *&newEntry, const char *newKey, PRUint32 klen)
{
	next       = prev = 0;
	maxAge     = 0;
	entry      = newEntry;
	exAgeLock  = PR_NewLock();
	rwNodeLock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "rwNodeLock");
	lastAge    = PR_IntervalNow();
	keyLen     = klen;
	key        = new char[keyLen + 1];
	memcpy((void *)key, (const void *)newKey, keyLen);
}

DrHashNode::DrHashNode(Entry *&newEntry, const char *newKey, PRUint32 klen,
						PRIntervalTime timeout)
{
	next       = prev = 0;
	maxAge     = timeout;
	entry      = newEntry;
	exAgeLock  = PR_NewLock();
	rwNodeLock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "rwNodeLock");
	lastAge    = PR_IntervalNow();
	keyLen     = klen;
	key        = new char[keyLen + 1];
	memcpy((void *)key, (const void *)newKey, keyLen);
}

inline Entry *DrHashNode::replaceEntry(Entry *&newEntry)
{
/*Lock it in write mode - so no one is reading from it*/
/*Assumes lock on ageLock*/

	Entry *old = nodeLockWrite();
	entry      = newEntry;
	lastAge    = PR_IntervalNow();
	nodeUnlock();
	return old;
}

inline Entry *DrHashNode::replaceEntry(Entry *&newEntry, PRIntervalTime timeout)
{
/*Lock it in write mode - so no one is reading from it*/
/*Assumes lock on ageLock*/

	Entry *old = nodeLockWrite();
	entry      = newEntry;
	lastAge    = PR_IntervalNow();
	maxAge     = timeout;
	nodeUnlock();
	return old;
}

inline void DrHashNode::linkNode(DrHashNode &newNode)
{
	next         = &newNode;
	newNode.prev      = this;
}

void DrHashNode::ageLock(void)
{
	PR_Lock(exAgeLock);
}

void DrHashNode::ageUnlock(void)
{
	PR_Unlock(exAgeLock);
}

Entry *DrHashNode::nodeLockRead(void)
{
	PR_RWLock_Rlock(rwNodeLock);
	return(entry);
}

inline Entry *DrHashNode::nodeLockWrite(void)
{
	PR_RWLock_Wlock(rwNodeLock);
	return(entry);
}

void DrHashNode::nodeUnlock(void)
{
	PR_RWLock_Unlock(rwNodeLock);
}

inline PRBool DrHashNode::isExpired(PRIntervalTime range)
{
	return(((PR_IntervalNow() - lastAge) > range) ? PR_TRUE : PR_FALSE);
}

inline PRBool DrHashNode::isExpired()
{
	return(((PR_IntervalNow() - lastAge) > maxAge) ? PR_TRUE : PR_FALSE);
}

DrHashList::~DrHashList()
{
	DrHashNode *iter = 0;
	PR_RWLock_Wlock(rwListLock);
	while(head)
	{
		iter      = head;
	 	head = iter->next;
		fnFre(iter->entry);
		delete iter;
	}
	PR_RWLock_Unlock(rwListLock);
	PR_DestroyRWLock(rwListLock);
	head       = tail = 0;
	ctr        = 0;
	rwListLock = 0;
	fnCmp      = 0;
	fnRef      = 0;
	fnFre      = 0;
}

DrHashList::DrHashList()
{
	head       = tail = 0;
	ctr        = 0;
	rwListLock = 0;
	fnCmp      = 0;
	fnRef      = 0;
	fnFre      = 0;
}

DrHashList::DrHashList(CompareFunc_t fnCompare, RefreshFunc_t fnRefresh,
                        FreeFunc_t fnFree)
{
	head       = tail = 0;
	ctr        = 0;
	rwListLock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "rwListLock");
	fnCmp      = fnCompare;
	fnRef      = fnRefresh;
	fnFre      = fnFree;
}

inline DrHashNode *DrHashList::getEntry(const char *&key, PRUint32 len,
							Request *rq, Session *sn)
{
	DrHashNode *node = head;
	PR_RWLock_Rlock(rwListLock);
	while(node)
	{
		if(fnCmp(key, node->key, len, node->keyLen))
		{
			break;
		}
		node = node->next;
	}
	PR_RWLock_Unlock(rwListLock);
	if(!node)
	{
		node = putEntry(key, len, PR_FALSE, rq, sn);
	}
	return node;
}

inline DrHashNode *DrHashList::tryGetEntry(const char *&key, PRUint32 len,
							Request *rq, Session *sn)
{
	DrHashNode *node = head;
	PR_RWLock_Rlock(rwListLock);
	while(node)
	{
		if(fnCmp(key, node->key, len, node->keyLen))
		{
			break;
		}
		node = node->next;
	}
	PR_RWLock_Unlock(rwListLock);
	if(node)
	{
		node->ageLock();
		if(node->isExpired())
		{
/*Send NULL so that caller refreshes it himself*/
			node = 0;
		}
		node->ageUnlock();
	}
	return node;
}

inline DrHashNode *DrHashList::getUnexpiredEntry(const char *&key, PRUint32 len,
							PRIntervalTime range, Request *rq, Session *sn)
{
	DrHashNode *node = head;
	PR_RWLock_Rlock(rwListLock);
	while(node)
	{
		if(fnCmp(key, node->key, len, node->keyLen))
		{
			break;
		}
		node = node->next;
	}
	PR_RWLock_Unlock(rwListLock);
	if(!node)
	{
		node = putEntry(key, len, PR_FALSE, rq, sn);
	}
	if(node)
	{
/*Check for Expiry, if Expired then we replace it*/
/*Get A lock on the agelock, this makes sure no one is writing to it*/
		node->ageLock();
		if(node->isExpired(range))
		{
/*Call The refresh Function to Add one for this key*/
			Entry *entry = fnRef(key, len, 0, rq, sn);
			Entry *old   = node->replaceEntry(entry);
/*Delete the old entry*/
			fnFre(old);
		}
		node->ageUnlock();
	}
	return node;
}

inline DrHashNode *DrHashList::forceRefresh(const char *&key, PRUint32 len,
							Request *rq, Session *sn)
{
	DrHashNode *node = head;

	PR_RWLock_Rlock(rwListLock);
/*First Try to locate it*/
	while(node)
	{
		if(fnCmp(key, node->key, len, node->keyLen))
		{
			break;
		}
		node = node->next;
	}
	PR_RWLock_Unlock(rwListLock);
	if(!node)
	{
		node = putEntry(key, len, PR_TRUE, rq, sn);
	}
	else
	{
/*Call The refresh Function to Add one for this key*/
		Entry *entry = fnRef(key, len, 0, rq, sn);

/*Get A lock on the agelock, this makes sure no one is writing to it*/
		node->ageLock();
		Entry *old   = node->replaceEntry(entry);
		node->ageUnlock();

/*Delete the old entry*/
		fnFre(old);
	}
	return node;
}

inline DrHashNode *DrHashList::refreshEntry(const char *&key, PRUint32 len,
								PRIntervalTime timeout, Entry *entry)
{
	DrHashNode *node = head;

	PR_RWLock_Rlock(rwListLock);
/*First Try to locate it*/
	while(node)
	{
		if(fnCmp(key, node->key, len, node->keyLen))
		{
			break;
		}
		node = node->next;
	}
	PR_RWLock_Unlock(rwListLock);
	if(!node)
	{
		node = putEntry(key, len, timeout, entry);
	}
	else
	{
/*Get A lock on the agelock, this makes sure no one is writing to it*/
		node->ageLock();
		Entry *old   = node->replaceEntry(entry, timeout);
		node->ageUnlock();
/*Delete the old entry*/
		fnFre(old);
	}
	return node;
}

inline DrHashNode *DrHashList::putEntry(const char *&key, PRUint32 len,
								PRBool ifForce, Request * rq, Session *sn)
{
	DrHashNode *node = head;

	PR_RWLock_Wlock(rwListLock);
/*Try locating the node one more time*/
	while(node)
	{
		if(fnCmp(key, node->key, len, node->keyLen))
		{
			break;
		}
		node = node->next;
	}
	if(!node)
	{
/*Still not there, I will add one*/
/*Call The refresh Function to Add one for this key*/
		Entry *entry = fnRef(key, len, 0, rq, sn);
		node = new DrHashNode(entry, key, len);
		if(tail)
		{
			tail->linkNode(*node);
			tail = node;
		}
		else
		{
			head = tail = node;
		}
		ctr++;
	}
	PR_RWLock_Unlock(rwListLock);
	if(node && ifForce)
	{
/*Requested a force Replace*/
		Entry *entry = fnRef(key, len, 0, rq, sn);

/*Get A lock on the agelock, this makes sure no one is writing to it*/
		node->ageLock();
		Entry *old   = node->replaceEntry(entry);
		node->ageUnlock();

/*Delete the old entry*/
		fnFre(old);
	}
	return(node);
}

inline DrHashNode *DrHashList::putEntry(const char *&key, PRUint32 len,
		PRIntervalTime timeout, Entry *entry)
{
	DrHashNode *node = head;

	PR_RWLock_Wlock(rwListLock);
/*Try locating the node one more time*/
	while(node)
	{
		if(fnCmp(key, node->key, len, node->keyLen))
		{
			break;
		}
		node = node->next;
	}
	if(!node)
	{
/*Still not there, I will add one*/
		node = new DrHashNode(entry, key, len, timeout);
		if(tail)
		{
			tail->linkNode(*node);
			tail = node;
		}
		else
		{
			head = tail = node;
		}
		ctr++;
		PR_RWLock_Unlock(rwListLock);
	}
	else
	{
		PR_RWLock_Unlock(rwListLock);

/*Get A lock on the agelock, this makes sure no one is writing to it*/

		node->ageLock();
		Entry *old   = node->replaceEntry(entry, timeout);
		node->ageUnlock();

/*Delete the old entry*/
		fnFre(old);
	}
	return(node);
}

DrHashTable::~DrHashTable()
{
	for(PRUint32 iCtr = 0; iCtr < hashSize; iCtr++)
	{
		delete hashTable[iCtr];
	}
	delete hashTable;
}

DrHashTable::DrHashTable()
{
	rwTableLock = 0;
	maxEntryAge = 0;
	hashSize    = 0;
	hashMask    = 0;
	hashTable   = 0;
	fnCmp       = 0;
	fnRef       = 0;
	fnFre       = 0;
}

DrHashTable::DrHashTable(PRUint32 maxEntries, PRIntervalTime maxAge, 
						CompareFunc_t fnCompare, RefreshFunc_t fnRefresh,
						FreeFunc_t fnFree)
{
	rwTableLock = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "rwTableLock");
	maxEntryAge = maxAge;
	hashSize    = (!(maxEntries % MIN_SIZE) ?
		maxEntries : (maxEntries
										+ (MIN_SIZE - (maxEntries % MIN_SIZE))));
	hashMask    = hashSize - 1;
	hashTable   = new DrHashList*[hashSize];
	for(PRUint32 iCtr = 0; iCtr < hashSize; iCtr++)
	{
		hashTable[iCtr] = 0;
	}
	fnCmp      = fnCompare;
	fnRef      = fnRefresh;
	fnFre      = fnFree;
}

PRUint32 DrHashTable::hashIt(const char *&key, PRUint32 len)
{
	PRUint32  iLen    = len;
	PRUint32  iScatter = 0;
	for(PRUint32 iCtr = 0; iCtr < iLen; iCtr++)
	{
		iScatter += (key[iCtr] + iCtr + hashSize);
	}
	return(((iScatter >> 3) ^ (iScatter << 3)) & hashMask);
}

DrHashNode *DrHashTable::getEntry(const char *&key, PRUint32 len, Request *rq,
								Session *sn)
{
	DrHashNode    *out   = 0;
	PRUint32 keyVal = hashIt(key, len);
	
/*Must Have a refresh Callback*/
	if(!fnRef)
	{
		return(0);
	}
	if(hashTable[keyVal])
	{
		out = hashTable[keyVal]->getEntry(key, len, rq, sn);
	}
	else
	{
		out = putEntry(key, len, PR_FALSE, rq, sn);
	}
	return(out);
}

DrHashNode *DrHashTable::tryGetEntry(const char *&key, PRUint32 len,
				Request *rq, Session *sn)
{
	DrHashNode    *out   = 0;
	PRUint32 keyVal = hashIt(key, len);
/*Need not check for the Refresh Function - when in this path*/	
/*Implementor may just be checking for expiry - he manages freshness himself*/
	if(hashTable[keyVal])
	{
		out = hashTable[keyVal]->tryGetEntry(key, len, rq, sn);
	}
	return(out);
}

DrHashNode *DrHashTable::getUnexpiredEntry(const char *&key, PRUint32 len,
								Request *rq, Session *sn)
{
	DrHashNode    *out   = 0;
	PRUint32 keyVal = hashIt(key, len);
	
/*Must Have a refresh Callback*/
	if(!fnRef)
	{
		return(0);
	}
	if(hashTable[keyVal])
	{
		out = hashTable[keyVal]->getUnexpiredEntry(key, len,
						maxEntryAge, rq, sn);
	}
	else
	{
		out = putEntry(key, len, PR_FALSE, rq, sn);
	}
	return(out);
}

DrHashNode *DrHashTable::forceRefresh(const char *&key, PRUint32 len,
								Request *rq, Session *sn)
{
	DrHashNode    *out   = 0;
	PRUint32 keyVal = hashIt(key, len);
	
/*Must Have a refresh Callback*/
	if(!fnRef)
	{
		return(0);
	}
	if(hashTable[keyVal])
	{
		out = hashTable[keyVal]->forceRefresh(key, len, rq, sn);
	}
	else
	{
		out = putEntry(key, len, PR_TRUE, rq, sn);
	}
	return(out);
}

DrHashNode *DrHashTable::putEntry(const char *&key, PRUint32 len,
								PRIntervalTime timeout, Entry *entry)
{
	PRUint32 keyVal = hashIt(key, len);

/*this method is called by outsider on forcefully dr_cache_refresh*/
	if(!hashTable[keyVal])
	{
		PR_RWLock_Wlock(rwTableLock);
		if(!hashTable[keyVal])
		{
/*Still not there, I will add one*/
			hashTable[keyVal] =
						new DrHashList(fnCmp, fnRef, fnFre);
		}
		PR_RWLock_Unlock(rwTableLock);
	}
	DrHashNode *out = hashTable[keyVal]->refreshEntry(key, len, ((timeout == 0) ?
							maxEntryAge : timeout), entry);
	return(out);
}

DrHashNode *DrHashTable::putEntry(const char *&key, PRUint32 len,
								PRBool ifForce, Request *rq, Session *sn)
{
	PRUint32 keyVal = hashIt(key, len);

/*this method is not called by outsiders - so we do not check for fnRef != 0*/
/*We have taken care of fnRef already*/
	if(!hashTable[keyVal])
	{
		PR_RWLock_Wlock(rwTableLock);
		if(!hashTable[keyVal])
		{
/*Still not there, I will add one*/
			hashTable[keyVal] =
						new DrHashList(fnCmp, fnRef, fnFre);
		}
		PR_RWLock_Unlock(rwTableLock);
	}
	DrHashNode *out;
	if(ifForce)
	{
		out = hashTable[keyVal]->forceRefresh(key, len, rq, sn);
	}
	else
	{
		out = hashTable[keyVal]->getEntry(key, len, rq, sn);
	}
	return(out);
}
