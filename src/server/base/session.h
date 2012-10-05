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

#ifndef BASE_SESSION_H
#define BASE_SESSION_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * session.h: Deals with virtual sessions
 *
 * A session is the time between when a client connects and when it
 * disconnects. Several requests may be handled in one session.
 *
 * Rob McCool
 */

#ifndef NETSITE_H
#include "netsite.h"  /* MALLOC etc */
#endif /* !NETSITE_H */

#ifndef BASE_NET_H
#include "net.h"          /* dns-related stuff */
#endif /* !BASE_NET_H */

#ifndef BASE_BUFFER_H
#include "buffer.h"       /* netbuf */
#endif /* !BASE_BUFFER_H */

#ifdef __cplusplus
class HttpFilterContext;
#else
typedef struct HttpFilterContext HttpFilterContext;
#endif

struct NSAPIRequest;

/*
 * Internal per-DaemonSession storage
 */
typedef struct SessionThreadData SessionThreadData;
struct SessionThreadData {
    void **slots;                  /* array of DaemonSession-specific data */
    int count;                     /* number of elements in slots[] */
};

/*
 *  Define internal NSAPI session structure
 *
 *  This structure encapsulates the older Session structure defined
 *  in include/public/nsapi.h.  It allows internal information to be
 *  added without exposing it to the world.
 */
typedef struct NSAPISession NSAPISession;
struct NSAPISession {

    /* Do not insert fields here */

    Session sn;                     /* public session structure */

    SessionThreadData *thread_data; /* per-DaemonSession storage */

    HttpFilterContext *httpfilter;  /* context for httpfilter */

    PRBool input_done;              /* set if Input should not be run */
    int input_os_pos;               /* lowest object that hasn't run Input */
    int input_rv;                   /* return value from Input stage */

    struct NSAPIRequest *exec_rq;   /* the currently executing Request */
    struct NSAPIRequest *filter_rq; /* the Request the filters are setup for */

    PRBool session_clone;           /* set if created by session_clone() */

    PRInt64 received;               /* number of bytes read */
    PRInt64 transmitted;            /* number of bytes written */
};

/* --- Begin function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

typedef void (SlotDestructorFunc)(void *data);
typedef SlotDestructorFunc *SlotDestructorFuncPtr;

NSAPI_PUBLIC
Session *INTsession_alloc(SYS_NETFD csd, struct sockaddr_in *sac); /* internal */

NSAPI_PUBLIC Session *INTsession_fill(Session *sn); /* internal */
NSAPI_PUBLIC void INTsession_fill_ssl(Session *sn); /* internal */
NSAPI_PUBLIC void INTsession_empty_ssl(Session *sn);

NSAPI_PUBLIC
Session *INTsession_create(SYS_NETFD csd, struct sockaddr_in *sac);

NSAPI_PUBLIC void INTsession_cleanup(Session *sn);
NSAPI_PUBLIC void INTsession_free(Session *sn);

NSAPI_PUBLIC char *INTsession_dns_lookup(Session *sn, int verify);

NSAPI_PUBLIC void *INTsession_get_thread_data(Session *sn, int slot);
NSAPI_PUBLIC void *INTsession_set_thread_data(Session *sn, int slot, void *data);

/*
 * session_alloc_thread_slot allocates a new slot for session_get_thread_data
 * and session_set_thread_data.
 */
NSAPI_PUBLIC int session_alloc_thread_slot(SlotDestructorFuncPtr destructor);

/*
 * session_destroy_thread calls destructors set by session_alloc_thread_slot.
 */ 
NSAPI_PUBLIC void session_destroy_thread(Session *sn);

/*
 * Create a temporary session clone for a child request.  The temporary
 * session clone looks like the original and uses its memory pool, but it has
 * a separate filter stack.
 */
NSAPI_PUBLIC Session *session_clone(Session *orig_sn, Request *child_rq);

/*
 * Return the original SYS_NETFD for this DaemonSession, e.g. the PRFileDesc *
 * that contains the NSS IO layer.  This may be different than sn->csd if
 * someone (e.g. the Java subsystem's filter) is intercepting output.
 */
NSAPI_PUBLIC SYS_NETFD session_get_original_csd(Session *sn);

NSAPI_PUBLIC void session_random(Session *sn, void *buf, size_t sz);

NSPR_END_EXTERN_C

#ifdef __cplusplus

class HttpRequest;

/*
 * session_input_done returns PR_FALSE if Input directives should be run, or
 * PR_TRUE if no more Input directives should be run.
 */
static inline PRBool session_input_done(Session *sn)
{
    return ((NSAPISession *)sn)->input_done;
}

/*
 * session_input_rv returns the result of the Input stage. Only valid when
 * (session_input_done() == PR_TRUE).
 */
static inline int session_input_rv(Session *sn)
{
    return ((NSAPISession *)sn)->input_rv;
}

/*
 * session_get_httpfilter_context returns the HttpFilterContext, if any,
 * associated with this session.
 */
static inline HttpFilterContext *session_get_httpfilter_context(Session *sn)
{
    return ((NSAPISession *)sn)->httpfilter;
}

/*
 * session_set_httpfilter_context sets or clears the HttpFilterContext
 * associated with this session.
 */
static inline void session_set_httpfilter_context(Session *sn, HttpFilterContext *httpfilter)
{
    ((NSAPISession *)sn)->httpfilter = httpfilter;
}

/*
 * session_get_thread_data retrieves per-DaemonSession data.
 */
static inline void *session_get_thread_data(Session *sn, int slot)
{
    NSAPISession *nsapiSession = (NSAPISession *)sn;
    if (nsapiSession && nsapiSession->thread_data && slot < nsapiSession->thread_data->count)
        return nsapiSession->thread_data->slots[slot];
    return INTsession_get_thread_data(sn, slot);
}

/*
 * session_set_thread_data stores per-DaemonSession data.
 */
static inline void *session_set_thread_data(Session *sn, int slot, void *data)
{
    NSAPISession *nsapiSession = (NSAPISession *)sn;
    if (nsapiSession && nsapiSession->thread_data && slot < nsapiSession->thread_data->count) {
        void *old = nsapiSession->thread_data->slots[slot];
        nsapiSession->thread_data->slots[slot] = data;
        return old;
    }
    return INTsession_set_thread_data(sn, slot, data);
}

#else
#define session_get_thread_data INTsession_get_thread_data
#define session_set_thread_data INTsession_set_thread_data
#endif

/* --- End function prototypes --- */

#define session_alloc INTsession_alloc
#define session_fill INTsession_fill
#define session_create INTsession_create
#define session_cleanup INTsession_cleanup
#define session_free INTsession_free
#define session_dns_lookup INTsession_dns_lookup

#endif /* INTNSAPI */

#endif /* !BASE_SESSION_H */
