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

#ifndef FRAME_REQ_H
#define FRAME_REQ_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * req.h: Request-specific data structures and functions
 * 
 * Rob McCool
 */

/*
 * REQUESTS, REQUEST RESTARTS, AND CHILD REQUESTS
 *
 * An NSAPIRequest's orig_rq pointer is the head of a linked list of
 * NSAPIRequests.  The orig_rq linked list always ends with an NSAPIRequest
 * whose orig_rq pointer points to itself.  The orig_rq linked list can be
 * walked to retrieve information about the original HTTP request:
 *
 *     Request *orig_rq = rq;
 *     while (orig_rq->orig_rq != orig_rq)
 *         orig_rq = orig_rq->orig_rq;
 *     const char *orig_uri = pblock_findkeyval(pb_key_uri, orig_rq->reqpb);
 *
 * When a new HTTP request arrives, an initial NSAPIRequest is created.  This
 * NSAPIRequest will have internal_req = 0 and its orig_rq pointer will point
 * to itself.
 *
 * While processing an HTTP request, additional internal NSAPIRequests may be
 * constructed as a result of a restart or a child request.
 *
 * An NSAPIRequest is restarted when a SAF decides that NSAPI processing
 * should be performed as though the request had arrived with a different
 * method, URI, etc.  For example, the find-index SAF might restart a request
 * for / as a request for /index.html.  After an NSAPIRequest is restarted, it
 * will have req_restarted = 1 and its orig_rq pointer will point to a newly
 * created internal NSAPIRequest with internal_req = 1.  This newly created
 * internal NSAPIRequest saves information such as the pre-restart method and
 * URI in its reqpb and headers pblocks for posterity.
 *
 * Child NSAPIRequests are created when multiple NSAPIRequests must be
 * processed to satisfy one HTTP request.  For example, a WebDAV PROPFIND
 * request on a directory might require processing a separate child
 * NSAPIRequest for each contained file.  Each child NSAPIRequest will have
 * internal_req = 1 and its orig_rq pointer will point to its parent.
 *
 * Sometimes internal NSAPIRequests are constructed without any corresponding
 * HTTP request.  These NSAPIRequests will have internal_req = 1.
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

#ifndef BASE_PBLOCK_H
#include "../base/pblock.h"
#endif /* !BASE_PBLOCK_H */

#ifndef BASE_SESSION_H
#include "base/session.h"       /* Session structure */
#endif /* !BASE_SESSION_H */

#ifndef FRAME_OBJSET_H
#include "objset.h"
#endif /* !FRAME_OBJSET_H */

#ifndef BASE_FILE_H
#include "../base/file.h"   /* struct stat */
#endif /* !BASE_FILE_H */

#ifndef SAFS_FLEXLOG_H
#include "../safs/flexlog.h"
#endif /* !SAFS_FLEXLOG_H */

#ifndef __nsfc_h_
#include "filecache/nsfc.h"
#endif /* !__nsfc_h_ */

#ifdef __cplusplus
class HttpRequest;
#else
typedef void HttpRequest;
#endif /* __cplusplus */

/* ------------------------------ Constants ------------------------------- */

#define REQ_HASHSIZE 10

#define MAX_ETAG		80

/*
 * MAX_REQUEST_CHILDREN is the maximum number of child requests to allow.  It's
 * set low because we'd like to catch infinite recursion before the thread
 * stack overflows.
 */
#define MAX_REQUEST_CHILDREN 20

/*
 * MAX_REQUEST_RESTARTS is the maximum number of restarts to allow per request.
 */
#define MAX_REQUEST_RESTARTS 50

#ifdef INTNSAPI

/*
 *  Define internal NSAPI request structure
 *
 *  This structure encapsulates the older Request structure defined
 *  in include/public/nsapi.h.  It allows internal information to be
 *  added without exposing it to the world.
 */
typedef struct NSAPIRequest NSAPIRequest;
struct NSAPIRequest {

    /* Do not insert fields here */

    Request       rq;           /* public request structure */

    HttpRequest  *hrq;          /* pointer to original HttpRequest */

    char         *nsfcpath;     /* last path passed to request_info_path */
    NSFCFileInfo *nsfcinfo;     /* cached info about nsfcpath */

    pblock       *param;          /* CGI env vars or <SERVLET> parameters */
    void         *webModule;      /* WebModule for this request */
    void         *servletResource;/* ServletResource for this request */
    void         *davCollection;  /* DAVCollection for this request */

    Session      *filter_sn;      /* Session we installed a filter in (may be NULL) */
    Request      *output_next_rq; /* next Request to run Output stage for */
    PRBool        output_done;    /* set if Output stage has been run */
    int           output_rv;      /* return value from Output stage */
    PRBool        session_clone;  /* set if using a different Session and filter stack than its parent */

    struct {
        void **slots;           /* array of Request-specific data */
        int count;              /* number of elements in slots[] */
    } data;

    int           nuri2path;    /* number of times servact_uri2path has run */

    NSFCEntry accel_nsfc_entry;
    FlexLog *accel_flex_log;
    char *accel_ssl_unclean_shutdown_browser;
};

/* --- Begin function prototypes --- */

NSPR_BEGIN_EXTERN_C

/*
 * INTrequest_initialize initializes an NSAPIRequest structure.
 */
NSAPI_PUBLIC PRStatus INTrequest_initialize(pool_handle_t *pool, HttpRequest *hrq, const char *hostname, NSAPIRequest *nrq);

/*
 * INTrequest_create creates a new request structure.
 */
NSAPI_PUBLIC Request *INTrequest_create(void);

/*
 * INTrequest_free destroys a request structure.
 */
NSAPI_PUBLIC void INTrequest_free(Request *req);

/*
 * request_create_child constructs a child request which can be used for SHTML
 * virtual includes, path resolution, etc.  Returns the new Request * on
 * success.  Returns NULL and logs an error on error.
 *
 * If method is NULL, the child method defaults to HEAD if the parent method
 * was HEAD or to GET in all other cases.  uri must be non-NULL and may be an
 * URL, absolute URI, or URI relative to the parent.  A NULL query indicates
 * the child will not have a query string; to use the parent query string, pass
 * the value of "query" from the parent rq->reqpb.
 */
NSAPI_PUBLIC Request *request_create_child(Session *sn, Request *parent_rq,
                                           const char *method, const char *uri,
                                           const char *query);

/*
 * request_create_virtual works like request_create_child but parses the URI
 * and query string from its location parameter and accepts an optional param
 * pblock that specifies CGI env vars or <SERVLET> parameters.  Returns the new
 * Request * on success.  Returns NULL and logs an error on error.
 */
NSAPI_PUBLIC Request *request_create_virtual(Session *sn, Request *parent_rq,
                                             const char *location,
                                             pblock *param);

/*
 * INTrequest_restart_internal is a legacy NSAPI function that constructs a
 * new request or prepares an request for a restart.
 *
 * request_create_child or request_restart_location should be used instead.
 */
NSAPI_PUBLIC Request *INTrequest_restart_internal(const char *location,
                                                  Request *rq);

/*
 * request_restart prepares a request for a restart, changing the URI and query
 * string.  Returns REQ_RESTART on success.  Returns REQ_ABORTED and logs an
 * error on error.
 *
 * If method is NULL, the request method is unchanged.  uri must be non-NULL
 * and may be an URL, absolute URI, or URI relative to the parent.  A NULL
 * query indicates the query string should be removed; to use the original
 * query string, pass the value of "query" from rq->reqpb.
 */
NSAPI_PUBLIC int request_restart(Session *sn, Request *rq, const char *method,
                                 const char *uri, const char *query);

/*
 * request_restart_location prepares a request for a restart, changing the URI
 * and query string.  Returns REQ_RESTART on success.  Returns REQ_ABORTED and
 * logs an error on error.
 */
NSAPI_PUBLIC int request_restart_location(Session *sn, Request *rq,
                                          const char *location);

/*
 * request_header finds the named header depending on the requesting 
 * protocol.  If possible, it will not load headers until the first is 
 * requested.  You have to watch out because this can return REQ_ABORTED.
 */
NSAPI_PUBLIC int INTrequest_header(char *name, char **value, 
                                   Session *sn, Request *rq);

/*
 * request_stat_path tries to stat path.  If path is NULL, it will look in
 * the vars pblock for "path".  If the stat is successful, it returns the stat 
 * structure.  If not, returns NULL and leaves a message in rq->staterr.  If a 
 * previous call to this function was successful, and path is the same, the 
 * function will simply return the previously found value.
 *
 * User functions should not free this structure.
 */
NSAPI_PUBLIC struct stat *INTrequest_stat_path(const char *path, Request *rq);

/*
 * INTrequest_info_path is like INTrequest_stat_path, except that it uses
 * the NSFC cache, and returns NSFCFileInfo instead of struct stat.
 */
NSAPI_PUBLIC PRStatus INTrequest_info_path(const char *path, Request *rq,
                                           NSFCFileInfo **finfo);

NSAPI_PUBLIC int INTrequest_is_internal(Request *rq);

NSAPI_PUBLIC PRBool INTrequest_is_restarted(Request *rq);

NSAPI_PUBLIC PRBool INTrequest_is_default_type_set(const Request *rq);

NSAPI_PUBLIC void INTrequest_has_default_type(Request *rq);

/* Returns the VirtualServer instance associated with this request */
NSAPI_PUBLIC const VirtualServer* INTrequest_get_vs(Request *rq);

/* Returns the time the server began processing this request */
NSAPI_PUBLIC PRStatus INTrequest_get_start_interval(Request *rq, PRIntervalTime *start);

/* Returns the memory pool currently in use */
NSAPI_PUBLIC pool_handle_t *request_pool(Request *rq);

/*
 * INTrequest_server_active should be called by the daemon immediately before
 * it begins normal request processing.
 */
NSAPI_PUBLIC void INTrequest_server_active(void);

/*
 * INTrequest_is_server_active returns PR_TRUE if INTrequest_server_active has
 * been called and PR_FALSE if it has not.
 */
NSAPI_PUBLIC PRBool INTrequest_is_server_active(void);

/*
 * INTrequest_alloc_slot allocates a new slot for request_get_data and
 * request_set_data.
 */
NSAPI_PUBLIC int INTrequest_alloc_slot(SlotDestructorFuncPtr destructor);

/*
 * INTrequest_get_data is a non-inline version of request_get_data.
 */
NSAPI_PUBLIC void *INTrequest_get_data(Request *rq, int slot);

/*
 * INTrequest_set_data is a non-inline version of request_set_data.
 */
NSAPI_PUBLIC void *INTrequest_set_data(Request *rq, int slot, void *data);

/*
 * INTrequest_set_path sets the ppath partial path in rq->vars to a local file
 * system path. Returns REQ_PROCEED if the ppath was set, REQ_RESTART if name
 * translation must be restarted (e.g. to serve a localized version of a file),
 * or REQ_ABORTED on error.
 */
NSAPI_PUBLIC int INTrequest_set_path(Session *sn,
                                     Request *rq,
                                     const char *root,
                                     int rootlen,
                                     const char *ppath,
                                     int ppathlen);

/*
 * request_get_base_uri returns the base URI for a request.  A base URI always
 * ends with a trailing / and specifies the virtual "directory" in which the
 * requested resource exists.
 *
 * Note that a base URI never includes a path-info suffix, so the base URI for
 * /cgi-bin/foo.pl/bar would typically be /cgi-bin/, not /cgi-bin/foo.pl/.
 */
NSAPI_PUBLIC void request_get_base_uri(Request *rq, const char **pbase,
                                       int *plen);

/* 
 * This is just a hack to get the stuff working on NT 
 * since httpdaemon gets linked as a static library to ns-httpd
 */
NSAPI_PUBLIC PRUint32 GetCurrentRecursionDepth();
NSAPI_PUBLIC void IncrementRecursionDepth();
NSAPI_PUBLIC void DecrementRecursionDepth();

NSPR_END_EXTERN_C

#ifdef __cplusplus

/*
 * request_output_done returns PR_FALSE if the Output directives should be run,
 * (i.e. if servact_output() should be called), or PR_TRUE if the Output
 * directives have already been run.
 */
static inline PRBool request_output_done(Request *rq)
{
    return ((NSAPIRequest *)rq)->output_done;
}

/*
 * request_output_rv returns REQ_NOACTION if the Output directives were
 * successfully run or the error returned from an Output directive if there was
 * an error running the Output stage. Only valid when (request_output_done() ==
 * PR_TRUE).
 */
static inline int request_output_rv(Request *rq)
{
    return ((NSAPIRequest *)rq)->output_rv;
}

/*
 * request_get_data retrieves per-Request data.
 */
static inline void *request_get_data(Request *rq, int slot)
{
    NSAPIRequest *nsapiRequest = (NSAPIRequest *)rq;
    if (slot < nsapiRequest->data.count)
        return nsapiRequest->data.slots[slot];
    return NULL;
}

/*
 * request_set_data stores per-Request data.
 */
static inline void *request_set_data(Request *rq, int slot, void *data)
{
    NSAPIRequest *nsapiRequest = (NSAPIRequest *)rq;
    if (slot < nsapiRequest->data.count) {
        void *old = nsapiRequest->data.slots[slot];
        nsapiRequest->data.slots[slot] = data;
        return old;
    }
    return INTrequest_set_data(rq, slot, data);
}

#else
#define request_get_data INTrequest_get_data
#define request_set_data INTrequest_set_data
#endif

/* --- End function prototypes --- */

#define request_initialize INTrequest_initialize
#define request_create INTrequest_create
#define request_free INTrequest_free
#define request_restart_internal INTrequest_restart_internal
#define request_header INTrequest_header
#define request_stat_path INTrequest_stat_path
#define request_info_path INTrequest_info_path
#define request_get_vs INTrequest_get_vs
#define request_get_start_interval INTrequest_get_start_interval
#define request_server_active INTrequest_server_active
#define request_is_server_active INTrequest_is_server_active
#define request_alloc_slot INTrequest_alloc_slot
#define request_set_path INTrequest_set_path

#endif /* INTNSAPI */

#endif /* !FRAME_REQ_H */
