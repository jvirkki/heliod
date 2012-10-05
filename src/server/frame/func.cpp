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

/**
 * funcpool.cpp - multiple pool func.c
 *
 * originally by Rob McCool
 * reimplemented for miltiple pools by Ruslan Belkin <ruslan@netscape.com>
 *
 * @version 2.0
 */

#include "netsite.h"
#include "frame/func.h"
#include "frame/log.h"
#include "frame/conf.h"

#include "base/shexp.h"   /* shexp_casecmp */
#include "base/nsassert.h"
#include "frame/conf.h"
#include "frame/conf_api.h"
#include "frame/http_ext.h"
#include "frame/httpfilter.h"
#include "base/util.h"    /* sprintf and itoa */

#include "frame/dbtframe.h"

#include <prthread.h>
#include <plstr.h>
#include "httpdaemon/httprequest.h"
#include "httpdaemon/daemonsession.h"

/**
 * Put NSAPI function vector pointers here to force them to be linked
 * in on platforms where explicit references control linkage.
 */

/* Set up pointer to static NSAPI 3.0 function vector */
NSAPI_PUBLIC nsapi_dispatch_t *__nsapi30_table = &__nsapi30_init;

/* Set up pointer to static NSAPI 3.2 (Web Server 6.1) function vector */
NSAPI_PUBLIC nsapi302_dispatch_t *__nsapi302_table = &__nsapi302_init;

/* Set up pointer to static NSAPI 3.3 (Proxy Server 4.0) function vector */
NSAPI_PUBLIC nsapi303_dispatch_t *__nsapi303_table = &__nsapi303_init;

/* Function name for log messages */
#define FUNC_EXEC "func_exec"

/* Default flags for new functions; modified by func_set_default_flags() */
static int _func_default_flags = 0;

struct NSTPWorkArg_s {
    FuncPtr func;
    const VirtualServer *vs;
    Session *sn;
    Request *rq;
    pblock *pb;
    unsigned id;
    int rval;
};


/* -------------------------- Static hash table --------------------------- */

struct FuncStructInternal : public FuncStruct {
    FuncStructInternal(const char *fn, FuncPtr fp)
    {
        name = fn;
        func = fp;
        next = 0;
        flags = _func_default_flags;
        poolID = 0;
        pool_resolved = PR_FALSE;
        offset = StatsManager::getFunctionName(name);
    }

    FuncStructInternal(const FuncStruct& fs)
    {
        *(FuncStruct*)this = fs;
        offset = StatsManager::getFunctionName(name);
    }

    ptrdiff_t offset;
};

static const struct FuncStruct *_fsCurrentlyExecuting;

static struct FuncStructInternal *func_tab[NUM_HASH];

NSAPI_PUBLIC  PoolElem poolTable[FUNC_MAXPOOLS + 1];	// pool table

/* ------------------------------ func_init ------------------------------- */

void _add_table (struct FuncStruct tab[])
{
    int x = 0;
    int hn;
    struct FuncStruct *p,*fs;
    struct FuncStructInternal *fsi;

    for(x = 0; (tab[x].name); x++)
	{
        fs = &tab[x];

        hn = FUNC_HASH (fs->name);
        fs -> next = NULL;    /* in case we're restarting */
        fsi = new FuncStructInternal(*fs);

        if(!(p = func_tab[hn]))
            func_tab[hn] = fsi;
        else
		{
            while (p -> next)
                p = p -> next;
            p -> next = fsi;
        }
    }
}

PRInt32 func_native_pool_init ();

NSAPI_PUBLIC void
func_init (struct FuncStruct *func_standard)
{
    int rv;
    int x;

    for(x = 0; x < NUM_HASH; x++)
        func_tab[x] = NULL;

    _add_table (func_standard);

    unsigned stackSize  = 0;
    unsigned queueSize  = 0;
    unsigned minThreads = 0;
    unsigned maxThreads = 0;

    const char *envStr;

    if ((envStr = conf_findGlobal ("NativePoolStackSize")) != NULL)
	stackSize  = atoi (envStr);

    if ((envStr = getenv ("NSCP_POOL_STACKSIZE"))    != NULL)
	stackSize  = atoi (envStr);

    if ((envStr = conf_findGlobal ("NativePoolMaxThreads"))!= NULL)
	maxThreads = atoi (envStr);

    if ((envStr = getenv ("NSCP_POOL_THREADMAX"))    != NULL)
	maxThreads = atoi (envStr);

    if ((envStr = conf_findGlobal ("NativePoolQueueSize")) != NULL)
	queueSize  = atoi (envStr);

    if ((envStr = getenv ("NSCP_POOL_WORKQUEUEMAX")) != NULL)
	queueSize  = atoi (envStr);

    if ((envStr = conf_findGlobal ("NativePoolMinThreads"))!= NULL)
	minThreads = atoi (envStr);

    memset (poolTable, 0, sizeof (poolTable));

    rv = func_addPool ("NativePool", minThreads, maxThreads, queueSize, stackSize, PR_FALSE);

    NS_ASSERT (rv == REQ_PROCEED);

#if defined (XP_PC)
    // Windows needs to still initialize here.
    // UNIX moves this initialization to be after the forking...
	rv = func_native_pool_init ();
    NS_ASSERT (rv == 0);
#endif
}


/* ------------------------------ func_find ------------------------------- */

NSAPI_PUBLIC struct FuncStruct *
func_find_str (const char *name)
{
    int hv = FUNC_HASH(name);
    struct FuncStruct *p;

    p = func_tab[hv];
    while (p) 
	{
        if(!PL_strcmp (name, p->name))
            return p;
        p = p -> next;
    }
    return NULL;
}

NSAPI_PUBLIC FuncPtr 
func_find (char *name)
{
    int hv = FUNC_HASH (name);
    struct FuncStruct *p;
    p = func_tab[hv];
    
    while(p) {
        if(!PL_strcmp (name, p -> name))
            return p->func;
        p = p->next;
    }
    return NULL;
}

NSAPI_PUBLIC FuncPtr 
func_replace (char *name, FuncPtr newfunc)
{
    int hv = FUNC_HASH(name);
    struct FuncStruct *p;
    FuncPtr rv = NULL;

    p = func_tab[hv];
    while(p) {
        if(!strcmp(name, p->name)) {
            FuncPtr oldfunc = p -> func;
            p -> func = newfunc;
            rv = oldfunc;
            break;
        }
        p = p->next;
    }

    return rv;
}

static inline int _func_exec_str(struct FuncStruct *f,
                                 pblock *pb, Session *sn, Request *rq)
{
    char *fname;
    int rv;

    int old_directive_is_cacheable;
    if (rq) {
        old_directive_is_cacheable = rq->directive_is_cacheable;
        rq->directive_is_cacheable = 0;
    }

    if (f -> flags & FUNC_USE_NATIVE_THREAD)
	{
        rv = func_native_pool_wait_work (f -> func, f -> poolID, pb, sn, rq);

        if (rv == REQ_TOOBUSY)
		{
            //
            // Too many requests stacked up.
            // We need to execute a busy function
            // but which one?
            if((fname = pblock_findval("busy", pb)))
			{
                if(!(f = func_find_str(fname)))
				{
                    log_error(LOG_MISCONFIG, FUNC_EXEC, sn, rq, 
                              XP_GetAdminStr(DBT_cannotFindFunctionNamedS_),
                              fname);
                    return REQ_ABORTED;
                }
                rv = (f->func)(pb, sn, rq);
            }
			else
			{
                if(!(f = func_find_str("service-toobusy")))
				{
                    log_error(LOG_MISCONFIG, FUNC_EXEC, sn, rq, 
                              XP_GetAdminStr(DBT_cannotFindFunctionNamedS_),
                              "service-toobusy");
                    return REQ_ABORTED;
                }
                rv = (f -> func)(pb, sn, rq);
            }
        }
    }
	else
        rv = (f -> func)(pb, sn, rq);

    if (rq) {
        rq -> request_is_cacheable &= rq -> directive_is_cacheable;
        rq->directive_is_cacheable = old_directive_is_cacheable;
    }

    return rv;
}

NSAPI_PUBLIC int func_exec_str(struct FuncStruct *f,
                               pblock *pb, Session *sn, Request *rq)
{
    NSAPISession *nsn = (NSAPISession *)sn;

    // Track the function this thread is in
    DaemonSession *ds;
    const char *namePrev;
    ptrdiff_t offsetPrev;
    const FuncStruct *fsPrev = NULL;
    PRBool flagPushed = PR_FALSE;
    if (rq) {
        HttpRequest *hrq = ((NSAPIRequest*)rq)->hrq;
        if (hrq) {
            ds = &hrq->GetDaemonSession();
            offsetPrev = ds->inFunction(((FuncStructInternal*)f)->offset);
            namePrev = hrq->GetFunction();
            hrq->SetFunction(f->name);
            flagPushed = PR_TRUE;
        }
    } else {
        fsPrev = _fsCurrentlyExecuting;
        _fsCurrentlyExecuting = f;
    }

    // Track the Request currently active for this Session
    NSAPIRequest *prev_rq;
    if (nsn) {
        prev_rq = nsn->exec_rq;
        nsn->exec_rq = (NSAPIRequest *)rq;
    }

    // Execute this function
    int rv = _func_exec_str(f, pb, sn, rq);

    // Track the Request currently active for this Session
    if (nsn)
        nsn->exec_rq = prev_rq;

    // Track the function this thread is in
    if (flagPushed) {
        HttpRequest *hrq = ((NSAPIRequest*)rq)->hrq;
        ds->inFunction(offsetPrev);
        hrq->SetFunction(namePrev);
    } else {
        _fsCurrentlyExecuting = fsPrev;
    }

    return rv;
}

NSAPI_PUBLIC int 
func_exec (pblock *pb, Session *sn, Request *rq)
{
    struct FuncStruct *f = func_resolve(pb, sn, rq);
    if (f == NULL)
        return REQ_ABORTED;

    return func_exec_str(f, pb, sn, rq);
}


/* ----------------------------- func_insert ------------------------------ */


NSAPI_PUBLIC struct FuncStruct *
func_insert (char *name, FuncPtr fn)
{
    int hn;
    struct FuncStructInternal *fsi;
    struct FuncStruct *p;

    hn = FUNC_HASH (name);

    // Create a PERM_FREE()-able FuncStructInternal
    FuncStructInternal temp(name, fn);
    fsi = (FuncStructInternal*)PERM_MALLOC(sizeof(temp));
    *fsi = temp;

    if(!(p = func_tab[hn]))
        func_tab[hn] = fsi;
    else
	{
        while(p -> next)
            p = p -> next;
        p -> next = fsi;
    }
    return fsi;
}

NSAPI_PUBLIC struct FuncStruct *
func_insert2 (const char *name, FuncPtr fn, int flags, const char *poolName)
{
    struct FuncStruct *fs;

    fs = func_insert ((char *)name, fn);
    if (fs)
	{
        fs -> flags  = flags;
		fs -> poolID = 0;
		
		if (poolName != NULL)
		{
			int i = 0;
			for ( ; i < FUNC_MAXPOOLS; i++)
				if (poolTable[i].name != NULL
					&& !PL_strcasecmp (poolName, poolTable[i].name))
				{
					fs -> poolID = i;
					break;
				}

			if (i == FUNC_MAXPOOLS)
				log_ereport (LOG_MISCONFIG, 
                     XP_GetAdminStr(DBT_funcThreadPoolNotFound), poolName);
		}
	}

    return fs;
}


static	PRBool func_native_pool_initialized = PR_FALSE;
static	NSTPGlobalConfig poolConfig;
static	unsigned poolIndex;

NSAPI_PUBLIC int
func_addPool (const char *poolName, unsigned minThreads, unsigned maxThreads, unsigned queueSize, unsigned stackSize, PRBool startPool)
{
    int i = 0;
    for ( ; i < FUNC_MAXPOOLS; i++)
	if (poolTable[i].name != NULL) {
	    if (!PL_strcasecmp (poolName, poolTable[i].name)) {
		    log_ereport (LOG_MISCONFIG, XP_GetAdminStr(DBT_funcThreadPoolAlreadyDecl), poolName);
		    return REQ_ABORTED;
	    }
	} else
	    break;

    if (i == FUNC_MAXPOOLS) {
	log_ereport (LOG_MISCONFIG, XP_GetAdminStr(DBT_funcThreadMaxExceeded), poolName, FUNC_MAXPOOLS);
	return REQ_ABORTED;
    }

    if (stackSize > 0 && stackSize < (64 * 1024)) {
	stackSize = 0;	// Stack size can never be less for all practical purposes
	log_ereport (LOG_WARN, XP_GetAdminStr(DBT_funcThreadMinStackError), (64 * 1024));
    }

    if (minThreads == 0)
	minThreads = 1;

    if (maxThreads == 0)
	maxThreads = 128;	// just some default limit

    poolTable[i].name              = poolName;
    poolTable[i].config.maxThread  = maxThreads;
    poolTable[i].config.initThread = minThreads;
    poolTable[i].config.maxQueue   = queueSize;
    poolTable[i].config.stackSize  = stackSize;
    poolTable[i].config.version	   = NSTP_API_VERSION;
    poolTable[i].config.defTimeout = PR_INTERVAL_NO_TIMEOUT;
    poolTable[i].pool              = NULL;

    if (startPool)
	func_native_pool_init ();

    return REQ_PROCEED;
}

NSAPI_PUBLIC int
func_native_pool_init ()
{
	if (func_native_pool_initialized == PR_FALSE)
	{
		if (PR_NewThreadPrivateIndex (&poolIndex, NULL)
			!= PR_SUCCESS)
		{
			log_ereport (LOG_MISCONFIG, 
                   XP_GetAdminStr(DBT_funcThreadPoolIndexError));
			return -1;
		}

		poolConfig.version   = NSTP_API_VERSION;
		poolConfig.tmoEnable = PR_FALSE;
	
		PRIntn vmin;
		PRIntn vmax;

		if (NSTP_Initialize (&poolConfig, &vmin, &vmax)
			!= PR_SUCCESS)
		{
			log_ereport (LOG_MISCONFIG, 
                   XP_GetAdminStr(DBT_funcGlobalThreadPoolInitError));
			return -1;
		}
		func_native_pool_initialized = PR_TRUE;
	}

	for (int i = 0; i < FUNC_MAXPOOLS; i++)
		if (poolTable[i].name != NULL && poolTable[i].pool == NULL)
		{
			if (NSTP_CreatePool (&poolTable[i].config, &poolTable[i].pool)
				!= PR_SUCCESS)
			{
				poolTable[i].pool = NULL;
				log_ereport (LOG_MISCONFIG, XP_GetAdminStr(DBT_funcPoolInitFailure),
                     poolTable[i].name);
				return -1;
			}
		}
	
	return 0;
}

NSAPI_PUBLIC int 
func_set_native_thread_flag (char *name, int flags)
{
    FuncStruct *f = func_find_str (name);
    if (f == NULL)
        return 1;

    f->flags = flags;
    return 0;
}

PR_CALLBACK void
func_native_pool_thread_main (NSTPWorkArg work_item)
{
    // ruslan: set pool id, so we don't need to go into the pool
    // on the next function
    PR_SetThreadPrivate (poolIndex, (void *)(work_item -> id + 1));

    pool_handle_t *pool;
    if (work_item->sn) {
        pool = work_item->sn->pool;
    } else {
        pool = NULL;
    }

    PR_SetThreadPrivate(getThreadMallocKey(), pool);

    HttpRequest *hrq;
    if (work_item->rq) {
        hrq = ((NSAPIRequest *) work_item->rq)->hrq;
    } else {
        hrq = NULL;
    }

    HttpRequest::SetCurrentRequest(hrq);

    if (hrq) {
        PR_ASSERT(hrq->getVS() == work_item->vs);
        conf_set_thread_globals(hrq);
    } else {
        conf_set_thread_vs_globals(work_item->vs);
    }

    /* do work */
    work_item->rval = work_item->func(work_item->pb, work_item->sn, work_item->rq);

    // ruslan: do not re-set thread local storage
}

NSAPI_PUBLIC void
prepare_nsapi_thread (Request * rq, Session * sn)
{
    NSAPIRequest * nRq = (NSAPIRequest *) rq;

    PRUint32 malloc_idx = getThreadMallocKey ();
    PRUint32 httprq_idx = HttpRequest::getTpdIndex ();

    if (malloc_idx != -1 && sn->pool != NULL) {
	PR_SetThreadPrivate (malloc_idx, sn -> pool);
    }

    if (httprq_idx != -1 && nRq->hrq != NULL) {
	PR_SetThreadPrivate (httprq_idx, nRq -> hrq);
    }

    if (nRq->hrq != NULL) {
        conf_set_thread_globals(nRq->hrq);
    }
}

NSAPI_PUBLIC PRBool
func_is_native_thread ()
{
#ifdef XP_WIN32
    // NSPR threads can be Win32 threads or Win32 fibers on Windows
    PRThread *thread = PR_CurrentThread ();

    int scope = PR_GetThreadScope (thread);

    return (scope != PR_LOCAL_THREAD);
#else
    // We use native threads on all platforms except Windows
    return PR_TRUE;
#endif
}

NSAPI_PUBLIC NSTPPool
func_get_native_pool ()
{
    return poolTable[0].pool;
}

NSAPI_PUBLIC PRInt32
func_native_pool_wait_work (FuncPtr fn, unsigned poolID, pblock *pb, Session *sn, Request *rq)
{
    PRBool need_thread = PR_FALSE;
	PRInt32 rval;
	
    if (func_native_pool_initialized == PR_TRUE)
	{
		if (func_is_native_thread ())
		{
			if (poolID > 0)
			{
				unsigned indx = (unsigned)(size_t)PR_GetThreadPrivate (poolIndex);

				if (indx == 0 || indx != poolID + 1)
					need_thread = PR_TRUE;
			}
		}
		else
			need_thread = PR_TRUE;
	}

	if (need_thread == PR_TRUE)
	{
		if (poolTable[poolID].pool == NULL)	// new pools are not yet initialized, porbbaly EarlyInit
			poolID = 0;

		NSTPWorkArg_s	work_item;

	    work_item.func	= fn;
		work_item.vs	= request_get_vs(rq);
		work_item.sn	= sn;
		work_item.rq	= rq;
		work_item.pb	= pb;
		work_item.id	= poolID;

		if (NSTP_QueueWorkItem (poolTable[poolID].pool, func_native_pool_thread_main, &work_item, PR_INTERVAL_NO_TIMEOUT)
			!= NSTP_STATUS_WORK_DONE)
		{
			return REQ_TOOBUSY;
		}
		else
			rval = work_item.rval;
	}
	else
        rval = fn (pb, sn, rq);

	return rval;
}


/* ----------------------------- func_resolve ----------------------------- */

NSAPI_PUBLIC struct FuncStruct *
func_resolve(pblock *pb, Session *sn, Request *rq)
{
    const char *fname = pblock_findkeyval(pb_key_fn, pb);
    if (!fname) {
        log_error(LOG_MISCONFIG, FUNC_EXEC, sn, rq,
                  XP_GetAdminStr(DBT_noHandlerFunctionGivenForDirecti_));
        return NULL;
    }

    struct FuncStruct *f = func_find_str(fname);
    if (!f) {
        log_error(LOG_MISCONFIG, FUNC_EXEC, sn, rq,
                  XP_GetAdminStr(DBT_cannotFindFunctionNamedS_),
                  fname);
        return NULL;
    }

    return f;
}


/* ------------------------------- _rv2str -------------------------------- */

static const char *_rv2str(int rv)
{
    switch (rv) {
    case REQ_PROCEED:
        return "REQ_PROCEED";
    case REQ_ABORTED:
        return "REQ_ABORTED";
    case REQ_NOACTION:
        return "REQ_NOACTION";
    case REQ_EXIT:
        return "REQ_EXIT";
    case REQ_RESTART:
        return "REQ_RESTART";
    case REQ_TOOBUSY:
        return "REQ_TOOBUSY";
    default:
        return "Unknown return code";
    }
}


/* ------------------------- func_exec_directive -------------------------- */

NSAPI_PUBLIC int
func_exec_directive(directive *inst, pblock *pb, Session *sn, Request *rq)
{
    int rv;

    /* Cache pointer to FuncStruct */
    if (inst->f == NULL) {
        inst->f = func_resolve(pb, sn, rq);
        if (inst->f == NULL)
            return REQ_ABORTED;
    }

    /*
     * Existing NSAPI modules can take advantage of HTTP aware buffered
     * streams by configuring "UseOutputStreamSize" and "flushTimer"
     * parameters!
     */
    char *bufsize_str;
    bufsize_str = pblock_findkeyval(pb_key_UseOutputStreamSize, pb);
    if (bufsize_str) {
        int bufsize = atoi(bufsize_str);
        if (bufsize >= 0)
            httpfilter_set_output_buffer_size(sn, rq, bufsize);
    }

    char *timer_str = pblock_findkeyval(pb_key_flushTimer, pb);
    if (timer_str) {
        int flush_timer = atoi(timer_str);
        if (flush_timer >= 0)
            httpfilter_set_output_buffer_timeout(sn, rq, flush_timer);
    }

    if (inst->f->pool_resolved == PR_FALSE)
    {
        char *poolName = pblock_findval("pool", pb);

        if (poolName != NULL)
        {
            int i;
            for (i = 0; i < FUNC_MAXPOOLS; i++)
                if (poolTable[i].name != NULL
                    && !PL_strcasecmp(poolName, poolTable[i].name))
                {
                    inst->f->poolID = i;
                    inst->f->flags |= FUNC_USE_NATIVE_THREAD;
                    break;
                }
            
            if (i == FUNC_MAXPOOLS)
                log_ereport (LOG_MISCONFIG, "Unable to find thread pool %s (fn=%s)", poolName, inst->f->name);
        }
        inst->f-> pool_resolved = PR_TRUE;
    }

    char *log_args = NULL;
    if (ereport_can_log(LOG_FINEST)) {
        log_args = pblock_pblock2str(pb, NULL);
        log_error(LOG_FINEST, FUNC_EXEC, sn, rq,
                  "executing %s",
                  log_args);
    }

    HttpRequest *hrq = ((NSAPIRequest *) rq)->hrq;

    /* Track perf bucket statistics */
    if (hrq)
        hrq->GetDaemonSession().beginFunction();

    /* Execute the SAF */
    rv = func_exec_str(inst->f, pb, sn, rq);

    /* Track perf bucket statistics */
    if (hrq)
        hrq->GetDaemonSession().endFunction(inst->bucket);

    if (rv == REQ_ABORTED && rq->status_num == 0) {
        log_error(LOG_FAILURE, FUNC_EXEC, sn, rq,
                  XP_GetAdminStr(DBT_fnXErrorWithoutStatus),
                  inst->f->name);
    }

    if (log_args != NULL) {
        log_error(LOG_FINEST, FUNC_EXEC, sn, rq,
                  "%s returned %d (%s)",
                  log_args, rv, _rv2str(rv));

    }

    return rv;
}

NSAPI_PUBLIC const char*
func_current(void)
{
    const char *name = NULL;

    // Init SAF
    const FuncStruct *fs = _fsCurrentlyExecuting;
    if (fs && fs->name) {
        name = fs->name;
    }

    // Request processing SAF
    HttpRequest *hrq = HttpRequest::CurrentRequest();
    if (hrq && hrq->GetFunction()) {
        name = hrq->GetFunction();
    }

    return name;
}

NSAPI_PUBLIC PRBool
func_set_default_flag(NSFuncFlags flag, PRBool on)
{
    PRBool old = (_func_default_flags & flag) == flag;

    if (on)
        _func_default_flags |= flag;
    else
        _func_default_flags &= ~flag;

    return old;
}

NSAPI_PUBLIC PRBool
func_get_default_flag(NSFuncFlags flag)
{
    return (_func_default_flags & flag) == flag;
}

