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
 * atomic.c
 *
 * Initial implementation for WinNT
 * XXXMB - need implementation for various platforms
 *
 *
 */

#include "atomic.h"

// not used on all platforms, but must be declared anyway:
CriticalSection Atomic::_lock;

#ifdef WIN32

#include "windows.h"

//////////////////////////////////////////////////////////////////////
//////////////////// Windows NT //////////////////////////////////////
//////////////////////////////////////////////////////////////////////

PRInt32
Atomic::Increment(PRInt32 *val)
{
    return (PRInt32)InterlockedIncrement((long *)val)-1;
}

PRInt32
Atomic::Decrement(PRInt32 *val)
{
    return (PRInt32)InterlockedDecrement((long *)val)+1;
}

PRInt32
Atomic::TestAndSet(PRInt32 *val, PRInt32 oldval, PRInt32 newval)
{
    return (PRInt32)InterlockedCompareExchange((void **)val, (void *)newval, (void *)oldval);
}

PRInt32
Atomic::Add(PRInt32 *val, PRInt32 increment)
{
    return (PRInt32)InterlockedExchangeAdd((long *)val, (unsigned int)increment);
}

#else // end NT
#ifdef AIX

/////////////////////////////////////////////////////////////////////////
///////////////////////////// AIX ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

// These routines will only work as long as "int" maps to 32 bits on AIX.

#include <memory.h>
#include <sys/atomic_op.h>

PRInt32 Atomic::Increment(PRInt32 *val) {
  return fetch_and_add(val, 1);
}

PRInt32 Atomic::Decrement(PRInt32 *val) {
  return fetch_and_add(val, -1);
}

PRInt32 Atomic::TestAndSet(PRInt32 *val, PRInt32 oldval, PRInt32 newval) {
  PRInt32 temp = oldval;
  if (compare_and_swap(val, &temp, newval)) {
    // success.
    return oldval;
  } else {
    // failure
   return temp;
  }
}

PRInt32 Atomic::Add(PRInt32 *val, PRInt32 increment) {
  return fetch_and_add(val, increment);
}

#else // end AIX


PRInt32
Atomic::Increment(PRInt32 *val)
{
    PRInt32 rv;

    _lock.acquire();
    rv = *val;
    (*val)++;
    _lock.release();

    return rv;
}

PRInt32
Atomic::Decrement(PRInt32 *val)
{
    PRInt32 rv;

    _lock.acquire();
    rv = *val;
    (*val)--;
    _lock.release();
  
    return rv;
}

PRInt32
Atomic::TestAndSet(PRInt32 *val, PRInt32 oldval, PRInt32 newval)
{
    PRInt32 rv;

    _lock.acquire();
    rv = *val;
    if (*val == oldval)
        (*val)++;
    _lock.release();

    return rv;
}

PRInt32
Atomic::Add(PRInt32 *val, PRInt32 increment)
{
    PRInt32 rv;

    _lock.acquire();
    rv = *val;
    (*val)+=increment;
    _lock.release();

    return rv;
}

#endif // AIX
#endif // NT
