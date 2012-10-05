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

#if !defined (FUNC_POOL_HXX)
#define FUNC_POOL_HXX	1

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

#include "netsite.h"
#include "base/pblock.h"
#include "base/session.h"		/* Session structure */
#include "frame/req.h"			/* Request structure */

#define	FUNC_MAXPOOLS	10	/* maximum number of pools allowed */
#define	poolId2handle(id)	((unsigned)(((unsigned char)id - '0'))

#include "threadpool/nstp.h"

typedef struct {
    const char *name;
    NSTPPool pool;
    NSTPPoolConfig config;
} PoolElem;

/* --------------------------- Hash definitions --------------------------- */

/* 
 * This is a primitive hash function. Once more is known about the names of
 * the functions, this will be optimized.
 */

#define NUM_HASH 20
#define FUNC_HASH(s) (s[0] % NUM_HASH)

NSAPI_PUBLIC struct FuncStruct *func_insert2  (const char *name, FuncPtr fn, int flags, const char *poolName);
NSAPI_PUBLIC struct FuncStruct *func_find_str (const char *);

/* flags for the flags field of the FuncStruct */
typedef enum NSFuncFlags {
    FUNC_USE_NATIVE_THREAD = 0x1       /* run function on native thread */
} NSFuncFlags;

/* -------------------------Native Pool Functions-------------------------- */
NSAPI_PUBLIC PRInt32 func_native_pool_init ();
NSAPI_PUBLIC int func_addPool (const char *poolName, unsigned minThreads, unsigned maxThreads, unsigned queueSize, unsigned stackSize, PRBool startPool);
NSAPI_PUBLIC PRBool func_is_native_thread ();
NSAPI_PUBLIC NSTPPool func_get_native_pool ();
NSAPI_PUBLIC PRInt32 func_native_pool_wait_work (FuncPtr fn, unsigned poolID, pblock *pb, Session *sn, Request *rq);

#ifdef INTNSAPI
NSPR_BEGIN_EXTERN_C

/* NSAPI 3.0 function vector */
extern nsapi_dispatch_t __nsapi30_init;

/* NSAPI 3.2 (Web Server 6.1) function vector */
extern nsapi302_dispatch_t __nsapi302_init;

/* NSAPI 3.3 (Proxy Server 4.0) function vector */
extern nsapi303_dispatch_t __nsapi303_init;

/*
 * INTfunc_init reads the static FuncStruct arrays and creates the global 
 * function table from them.
 *
 * INTfunc_init will only read from the static arrays defined in func.c.
 */

NSAPI_PUBLIC void INTfunc_init (struct FuncStruct *func_standard);

/*
 * INTfunc_find returns a pointer to the function named name, or NULL if none
 * exists.
 */

NSAPI_PUBLIC FuncPtr INTfunc_find	(char *name);

/*
 * INTfunc_find_str returns a pointer to the function table entry
 * for the named function.
 */
NSAPI_PUBLIC struct FuncStruct *INTfunc_find_str(const char *name);

NSAPI_PUBLIC FuncPtr INTfunc_replace(char *name, FuncPtr newfunc);

/*
 * INTfunc_exec will try to execute the function whose name is the "fn" entry
 * in the given pblock. If name is not found, it will log a misconfig of
 * missing fn parameter. If it can't find it, it will log that. In these
 * cases it will return REQ_ABORTED. Otherwise, it will return what the 
 * function being executed returns.
 */

NSAPI_PUBLIC int INTfunc_exec (pblock *pb, Session *sn, Request *rq);

NSAPI_PUBLIC int func_exec_str(struct FuncStruct *f,
                               pblock *pb, Session *sn, Request *rq);

/*
 * INTfunc_insert dynamically inserts a named function into the server's
 * table of functions. Returns the FuncStruct it keeps in internal 
 * databases, because on server restart you are responsible for freeing 
 * (or not) its contents.
 */

NSAPI_PUBLIC struct FuncStruct *INTfunc_insert  (char *name, FuncPtr fn);

NSAPI_PUBLIC int INTfunc_set_native_thread_flag (char *name,  int flags);

/*
 * func_set_default_flag controls the default flags for new named functions.
 */
NSAPI_PUBLIC PRBool func_set_default_flag(NSFuncFlags flag, PRBool on);
NSAPI_PUBLIC PRBool func_get_default_flag(NSFuncFlags flag);

NSAPI_PUBLIC int INTfunc_exec_directive(directive *d, pblock *pb, Session *sn, Request *rq);

NSAPI_PUBLIC void INTprepare_nsapi_thread (Request * rq, Session * sn);

NSAPI_PUBLIC const char *INTfunc_current(void);

NSAPI_PUBLIC struct FuncStruct *INTfunc_resolve(pblock *pb, Session *sn, Request *rq);

NSPR_END_EXTERN_C

#define func_init		INTfunc_init
#define func_find		INTfunc_find
#define func_find_str	INTfunc_find_str
#define func_replace	INTfunc_replace
#define func_exec		INTfunc_exec
#define func_insert		INTfunc_insert
#define func_set_native_thread_flag	INTfunc_set_native_thread_flag
#define func_exec_directive INTfunc_exec_directive
#define prepare_nsapi_thread	INTprepare_nsapi_thread
#define func_current            INTfunc_current
#define func_resolve            INTfunc_resolve

#endif /* INTNSAPI */

#endif
