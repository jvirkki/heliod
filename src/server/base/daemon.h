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

#ifndef BASE_DAEMON_H
#define BASE_DAEMON_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

#ifndef NETSITE_H
#include "netsite.h"
#endif /* !NETSITE_H */

#ifndef HTTPDAEMON_HTTPDAEMON_H
#include "../httpdaemon/httpdaemon.h"
#endif /* !HTTPDAEMON_HTTPDAEMON_H */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

#ifdef XP_UNIX
/* FUNCTION: child_fork
** DESCRIPTION:
** child_fork is a thread-safe wrapper for the system's fork function. 
**/
NSAPI_PUBLIC pid_t INTchild_fork(void);
#endif

/* In lib/base/restart.cpp */
NSAPI_PUBLIC void INTdaemon_dorestart(void);

/*
 * daemon_atrestart registers a function to be called fn, with the given
 * void pointer as an argument, when the server is restarted.
 */
typedef void (*restartfunc)(void *);

NSAPI_PUBLIC void INTdaemon_atrestart(restartfunc fn, void *data);

/* Random number generation */

NSAPI_PUBLIC void *nsapi_random_create(void);
NSAPI_PUBLIC
void nsapi_random_update(void *rctx, unsigned char *inbuf, int length);
NSAPI_PUBLIC
void nsapi_random_generate(void *rctx, unsigned char *outbuf, int length);
NSAPI_PUBLIC void nsapi_random_destroy(void *rctx);

/* MD5 hashing */

NSAPI_PUBLIC void *nsapi_md5hash_create(void);
NSAPI_PUBLIC void *nsapi_md5hash_copy(void *hctx);
NSAPI_PUBLIC void nsapi_md5hash_begin(void *hctx);
NSAPI_PUBLIC
void nsapi_md5hash_update(void *hctx, unsigned char *inbuf, int length);
NSAPI_PUBLIC void nsapi_md5hash_end(void *hctx, unsigned char *outbuf);
NSAPI_PUBLIC void nsapi_md5hash_destroy(void *hctx);
NSAPI_PUBLIC void
nsapi_md5hash_data(unsigned char *outbuf, unsigned char *inbuf, int length);

NSAPI_PUBLIC int nsapi_rsa_set_priv_fn(void *);
NSPR_END_EXTERN_C

#ifdef XP_UNIX
#define child_fork INTchild_fork
#endif /* XP_UNIX */
#define daemon_dorestart INTdaemon_dorestart
#define daemon_atrestart INTdaemon_atrestart

#endif /* INTNSAPI */

#endif /* !BASE_DAEMON_H */
