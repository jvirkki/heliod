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
 * rwlock.h: Shared/Exclusive lock abstraction. 
 * 
 * Sanjay Krishnamurthi
 */
#ifndef _BASE_RWLOCK_H_
#define _BASE_RWLOCK_H_

#include "netsite.h"
#include "crit.h"

NSPR_BEGIN_EXTERN_C

typedef void* RWLOCK;

/*
 * rwlock_Init()
 *  creates and returns a new readwrite lock variable. 
 */
NSAPI_PUBLIC RWLOCK rwlock_Init(void);

/*
 * rwlock_ReadLock()
 */
NSAPI_PUBLIC void rwlock_ReadLock(RWLOCK lock);

/*
 * rwlock_WriteLock()
 */
NSAPI_PUBLIC void rwlock_WriteLock(RWLOCK lock);

/*
 * rwlock_Unlock()
 */
NSAPI_PUBLIC void rwlock_Unlock(RWLOCK lock);

/*
 * rwlock_DemoteLock()
 */
NSAPI_PUBLIC void rwlock_DemoteLock(RWLOCK lock);

/*
 * rwlock_terminate removes a previously allocated RWLOCK variable.
 */
NSAPI_PUBLIC void rwlock_Terminate(RWLOCK lock);

NSPR_END_EXTERN_C

#endif /* _BASE_RWLOCK_H_ */
