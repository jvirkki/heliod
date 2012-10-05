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

#include <stdlib.h>
#include "crit.h"
#include "rwlock.h"

/*
 * rwLock.c
 *    Implements a shared/exclusive lock package atop the
 * critical section/condition variables. It allows multiple
 * shared lock holders and only one exclusive lock holder
 * on a lock variable.
 *
 * NOTE : It currently favors writers over readers and writers
 *  may starve readers. It is usually preferable to allow updates
 *  if they are not as frequent. We may have to change this if
 *  the common usage pattern differs from this.
 */

typedef struct {
    CRITICAL crit;           /* Short term crit to synchronize lock ops */
    CONDVAR  readFree;       /* Indicates lock is free for readers */
    CONDVAR  writeFree;      /* Indicates lock is free for the writer */
    int      numReaders;     /* Number of read locks held */
    int      write;          /* Flag to indicate write lock held */
    int      numWriteWaiters;/* Number of threads waiting for write lock */
} rwLock_t;

/* 
 * rwlock_init()
 *   Allocate and initialize the rwlock structure and return
 *  to the caller.
 */
RWLOCK rwlock_Init()
{
    rwLock_t *rwLockP;

    rwLockP = (rwLock_t *)PERM_MALLOC(sizeof(rwLock_t));
    rwLockP->numReaders = 0;
    rwLockP->write = 0;
    rwLockP->numWriteWaiters = 0;
    rwLockP->crit = crit_init();
    rwLockP->readFree = condvar_init(rwLockP->crit);
    rwLockP->writeFree = condvar_init(rwLockP->crit);
    return((RWLOCK)rwLockP);
}

/* 
 * rwlock_terminate()
 *   Terminate the associated condvars and critical sections
 */
void rwlock_Terminate(RWLOCK lockP)
{
    rwLock_t   *rwLockP = (rwLock_t *)lockP;
    
    crit_terminate(rwLockP->crit);
    condvar_terminate(rwLockP->readFree);
    condvar_terminate(rwLockP->writeFree);
    PERM_FREE(rwLockP);
}

/*
 * rwlock_ReadLock -- Obtain a shared lock. The caller would
 *  block if there are writers or writeWaiters.
 */
void rwlock_ReadLock(RWLOCK lockP)
{
    rwLock_t *rwLockP = (rwLock_t *)lockP;

    crit_enter(rwLockP->crit);                 
    while (rwLockP->write || rwLockP->numWriteWaiters != 0)
        condvar_wait(rwLockP->readFree);
    rwLockP->numReaders++;                              
    crit_exit(rwLockP->crit);                 
}

/*
 * rwlock_writeLock -- Obtain an exclusive lock. The caller would
 * block if there are other readers or a writer.
 */
void rwlock_WriteLock(RWLOCK lockP)
{
    rwLock_t *rwLockP = (rwLock_t *)lockP;

    crit_enter(rwLockP->crit);                 
    rwLockP->numWriteWaiters++;                                
    while (rwLockP->numReaders != 0 || rwLockP->write)             
        condvar_wait(rwLockP->writeFree);      
    rwLockP->numWriteWaiters--;                                
    rwLockP->write = 1;                                   
    crit_exit(rwLockP->crit);                 
}

/*
 * rw_Unlock -- Releases the lock. 
 */
void rwlock_Unlock(RWLOCK lockP)
{
    rwLock_t *rwLockP = (rwLock_t *)lockP;
                                                         
    crit_enter(rwLockP->crit);                 
    if (rwLockP->write)                                      
        rwLockP->write = 0;                              
    else                                                 
        rwLockP->numReaders--;                                 
    if (rwLockP->numReaders == 0)                              
        if (rwLockP->numWriteWaiters != 0)                     
            condvar_notify(rwLockP->writeFree);             
        else                                             
            condvar_notifyAll(rwLockP->readFree);           
    crit_exit(rwLockP->crit);                 
}
                                                         
/*
 * rwlock_DemoteLock -- Change an exclusive lock on the given lock
 * variable into a shared lock.  
 */
void rwlock_DemoteLock(RWLOCK lockP)
{
    rwLock_t *rwLockP = (rwLock_t *)lockP;
                                                         
    crit_enter(rwLockP->crit);                 
    rwLockP->numReaders = 1;
    rwLockP->write = 0;
    if (rwLockP->numWriteWaiters == 0)
        condvar_notifyAll(rwLockP->readFree);
    crit_exit(rwLockP->crit);                 
}
