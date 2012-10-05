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

#ifndef BASE_SYSTHR_H
#define BASE_SYSTHR_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * systhr.h: Abstracted threading mechanisms
 * 
 * Rob McCool
 */

#ifndef NETSITE_H
#include "netsite.h"
#endif /* !NETSITE_H */

#ifdef THREAD_ANY

/* --- Begin function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

typedef void (*thrstartfunc)(void *);
NSAPI_PUBLIC
SYS_THREAD INTsysthread_start(int prio, int stksz, thrstartfunc fn, void *arg);

NSAPI_PUBLIC SYS_THREAD INTsysthread_current(void);

NSAPI_PUBLIC void INTsysthread_yield(void);

NSAPI_PUBLIC SYS_THREAD INTsysthread_attach(void);

NSAPI_PUBLIC void INTsysthread_detach(SYS_THREAD thr);

NSAPI_PUBLIC void INTsysthread_terminate(SYS_THREAD thr);

NSAPI_PUBLIC void INTsysthread_sleep(int milliseconds);

NSAPI_PUBLIC void INTsysthread_init(char *name);

NSAPI_PUBLIC void INTsysthread_timerset(int usec);

NSAPI_PUBLIC int INTsysthread_newkey(void);

NSAPI_PUBLIC void *INTsysthread_getdata(int key);

NSAPI_PUBLIC void INTsysthread_setdata(int key, void *data);

NSAPI_PUBLIC 
void INTsysthread_set_default_stacksize(unsigned long size);

NSPR_END_EXTERN_C

/* --- End function prototypes --- */

#define systhread_start INTsysthread_start
#define systhread_current INTsysthread_current
#define systhread_yield INTsysthread_yield
#define systhread_attach INTsysthread_attach
#define systhread_detach INTsysthread_detach
#define systhread_terminate INTsysthread_terminate
#define systhread_sleep INTsysthread_sleep
#define systhread_init INTsysthread_init
#define systhread_timerset INTsysthread_timerset
#define systhread_newkey INTsysthread_newkey
#define systhread_getdata INTsysthread_getdata
#define systhread_setdata INTsysthread_setdata
#define systhread_set_default_stacksize INTsysthread_set_default_stacksize

#endif /* INTNSAPI */

#endif /* THREAD_ANY */

#endif /* !BASE_SYSTHR_H */
