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
 * filter.cpp: NSAPI filter support
 * 
 * Chris Elving
 */

#include "private/pprio.h"
#include "ssl.h"

#include "netsite.h"
#include "base/util.h"
#include "base/pool.h"
#include "base/ereport.h"
#include "frame/log.h"
#include "frame/func.h"
#include "frame/httpact.h"
#include "frame/dbtframe.h"
#include "NsprWrap/NsprError.h"
#include "support/SimpleHash.h"
#include "frame/filter.h"
#include "filter_pvt.h"


// Number of filter method pointers in the FilterMethods struct
#define METHODS_COUNT(m) (((m)->size - sizeof((m)->size)) / sizeof(void *))

// Array of filter method pointers from the FilterMethods struct
#define METHODS_ARRAY(m) ((void **)&(m)->insert)

// Filter names for pseudo-filters
#define FILTER_NAME_TOP "top"
#define FILTER_NAME_BOTTOM "bottom"
#define FILTER_NAME_NETWORK "network"
#define FILTER_NAME_SSL "ssl"

// Reserved filter name prefixes
#define FILTER_PREFIX_SERVER "server-"
#define FILTER_PREFIX_MAGNUS "magnus-"

static Filter *_filter_list = NULL;
SimplePtrStringHash _filter_hash(11);
static PRLock *_filter_list_lock = NULL;
static PRDescIdentity _filter_identity = PR_INVALID_IO_LAYER;
static PRDescIdentity _filter_ssl_identity = PR_INVALID_IO_LAYER;
static PRDescIdentity _filter_null_identity = PR_INVALID_IO_LAYER;
static PRIOMethods _filter_null_priomethods;
static Filter _filter_top;
static Filter _filter_bottom;
static Filter _filter_network;
static Filter _filter_ssl;
static const Filter *_filter_callback;
static pblock *_empty_pb;
static int _filter_prfiledesc_slot;
static PRBool _filter_logged_native_thread_issue = PR_FALSE;

static FilterInsertFunc filtermethod_always_insert;
static FilterInsertFunc filtermethod_never_insert;
static FilterRemoveFunc filtermethod_default_remove;

enum FilterContextInternalFlag {
    FILTERCONTEXTINTERNAL_MARKED_FOR_DEATH = 0x1
};

struct FilterContextInternal : public FilterContext {
    int flags; /* bitwise OR of FilterContextInternalFlags */
};


/* ------------------------ filter_create_context ------------------------- */

static inline FilterContext *filter_create_context(Session *sn, Request *rq, void *data, const Filter *filter, FilterLayer *layer)
{
    pool_handle_t *pool = sn ? sn->pool : NULL;
    FilterContextInternal *context = (FilterContextInternal *)pool_malloc(pool, sizeof(FilterContextInternal));
    if (!context)
        return NULL;

    context->pool = pool;
    context->sn = sn;
    context->rq = rq;
    context->data = data;
    context->flags = 0;

    layer->context = context;

    return layer->context;
}


/* ------------------------ filter_destroy_context ------------------------ */

static inline void filter_destroy_context(FilterLayer *layer)
{
    if (layer->context) {
        // Blow away the filter context
        pool_free(layer->context->pool, layer->context);
        layer->context = NULL;
    }
}


/* --------------------- filter_alloc_filter_iolayer ---------------------- */

static inline PRFileDesc *filter_alloc_filter_iolayer(Session *sn, const Filter *filter)
{
    // Get a cached PRFileDesc or allocate a new one
    PRFileDesc *iolayer = (PRFileDesc *)session_get_thread_data(sn, _filter_prfiledesc_slot);
    if (iolayer) {
        session_set_thread_data(sn, _filter_prfiledesc_slot, iolayer->lower);
        iolayer->identity = _filter_identity;
        iolayer->methods = &filter->priomethods;
    } else {
        iolayer = PR_CreateIOLayerStub(_filter_identity, &filter->priomethods);
    }
    return iolayer;
}


/* ---------------------- filter_alloc_null_iolayer ----------------------- */

static inline PRFileDesc *filter_alloc_null_iolayer(Session *sn, const PRIOMethods *priomethods)
{
    // Get a cached PRFileDesc or allocate a new one
    PRFileDesc *iolayer = (PRFileDesc *)session_get_thread_data(sn, _filter_prfiledesc_slot);
    if (iolayer) {
        session_set_thread_data(sn, _filter_prfiledesc_slot, iolayer->lower);
        iolayer->identity = _filter_null_identity;
        iolayer->methods = priomethods;
        iolayer->secret = (PRFilePrivate *)sn;
    } else {
        iolayer = PR_CreateIOLayerStub(_filter_null_identity, priomethods);
    }
    return iolayer;
}


/* ------------------------- filter_free_iolayer -------------------------- */

static inline void filter_free_iolayer(Session *sn, PRFileDesc *iolayer)
{
    // Cache the PRFileDesc for later use
    if (iolayer) {
        PR_ASSERT(iolayer->identity == _filter_identity || iolayer->identity == _filter_null_identity);
        PR_ASSERT(iolayer->secret == NULL);
        iolayer->lower = (PRFileDesc *)session_get_thread_data(sn, _filter_prfiledesc_slot);
        session_set_thread_data(sn, _filter_prfiledesc_slot, iolayer);
    }
}


/* ------------------------ filter_insert_iolayer ------------------------- */

static inline int filter_insert_iolayer(PRFileDesc *position, PRFileDesc **piolayer)
{
    /*
     * Based on NSPR 4.2.1's PR_PushIOLayer()
     */

    // Never insert above the head of a new-style stack
    if (position->identity == PR_IO_LAYER_HEAD)
        position = position->lower;

    if (!position->higher) {
        /*
         * Going on top of old-style stack
         */

        PRFileDesc copy = *position;
        *position = **piolayer;
        **piolayer = copy;

        if ((*piolayer)->lower)
            (*piolayer)->lower->higher = *piolayer;
        (*piolayer)->higher = position;

        position->higher = NULL;
        position->lower = *piolayer;

        *piolayer = position;

    } else {
        /*
         * Going somewhere in the middle of the stack for both old- and new-
         * style stacks, or going on top of stack for new-style stack
         */

         (*piolayer)->higher = position->higher;
         (*piolayer)->lower = position;

         position->higher->lower = *piolayer;
         position->higher = *piolayer;
     }
 
     return REQ_PROCEED;
}


/* ------------------------ filter_extract_iolayer ------------------------ */

static inline PRFileDesc *filter_extract_iolayer(PRFileDesc *iolayer)
{
    /*
     * Based on NSPR 4.2.1's PR_PopIOLayer()
     */

    PR_ASSERT(iolayer->identity == _filter_identity || iolayer->identity == _filter_null_identity);

    if (!iolayer->higher && iolayer->lower) {
        /*
         * Extracting top layer of old-style stack
         */

        PRFileDesc *extract = iolayer->lower;
        PRFileDesc copy = *iolayer;
        *iolayer = *extract;
        *extract = copy;

        iolayer->higher = NULL;
        if (iolayer->lower)
            iolayer->lower->higher = iolayer;

        iolayer = extract;

    } else {
        /*
         * Removing only layer or removing from middle or bottom
         */

        if (iolayer->lower)
            iolayer->lower->higher = iolayer->higher;
        if (iolayer->higher)
            iolayer->higher->lower = iolayer->lower;
    }

    return iolayer;
}


/* ------------------------- filter_close_iolayer ------------------------- */

static inline PRStatus filter_close_iolayer(Session *sn, PRFileDesc *iolayer)
{
    // The filter or NSPR IO layer should already have cleaned up
    PR_ASSERT(iolayer->secret == NULL);

    // Based on NSPR 4.2.1's pl_TopClose()
    if (iolayer->higher && iolayer->higher->identity == PR_IO_LAYER_HEAD) {
        // Lower (non-PR_IO_LAYER_HEAD) layers of new style stack
        PRFileDesc *lower = iolayer->lower;
        PRFileDesc *popped = filter_extract_iolayer(iolayer);
        filter_free_iolayer(sn, popped);
        if (lower)
            return (lower->methods->close(lower));

    } else {
        // Old style stack
        PRFileDesc *lower = iolayer->lower;
        PRFileDesc *popped = filter_extract_iolayer(iolayer);
        filter_free_iolayer(sn, popped);
        if (lower)
            return (iolayer->methods->close)(iolayer);
    }

    return PR_SUCCESS;
}


/* ---------------------------- filter_iolayer ---------------------------- */

static PRFileDesc *filter_iolayer(SYS_NETFD fd, const Filter *filter)
{
    // Like filter_layer(), but will also return pseudo-filters

    // If this looks like a pseudo-filter...
    if (filter->insert == &filtermethod_never_insert) {
        PRFileDesc *layer = fd;

        if (filter == &_filter_top) {
            // Return top layer
            if (layer) {
                while (layer->higher)
                    layer = layer->higher;
            }
            if (layer->identity == PR_IO_LAYER_HEAD)
                layer = layer->lower;
            return layer;

        } else if (filter == &_filter_bottom) {
            // Return bottom layer
            if (layer) {
                while (layer->lower)
                    layer = layer->lower;
            }
            return layer;

        } if (filter == &_filter_network) {
            // Return NSPR IO layer
            while (layer) {
                if (layer->identity == PR_NSPR_IO_LAYER)
                    return layer;
                layer = layer->lower;
            }

        } else if (filter == &_filter_ssl) {
            // Return NSS IO layer
            while (layer) {
                if (layer->identity == _filter_ssl_identity)
                    return layer;
                layer = layer->lower;
            }
        }
    }

    return (PRFileDesc *)filter_layer(fd, filter);
}


/* ------------------------- filter_iolayer_order ------------------------- */

static inline int filter_iolayer_order(PRFileDesc *iolayer)
{
    if (iolayer->identity == _filter_identity) {
        // Filter
        return ((FilterLayer *)iolayer)->filter->order;

    } else if (iolayer->identity == PR_NSPR_IO_LAYER) {
        // NSPR pseudo-filter
        return FILTER_NETWORK;

    } else if (iolayer->identity == _filter_ssl_identity) {
        // NSS pseudo-filter
        return FILTER_TRANSPORT_CODING;
    }

    // Unknown IO layer
    return -1;
}


/* ------------------------ default filter methods ------------------------ */

static int filtermethod_always_insert(FilterLayer *layer, pblock *pb)
{
    // Aways insert
    return REQ_PROCEED;
}

static int filtermethod_never_insert(FilterLayer *layer, pblock *pb)
{
    // never insert
    return REQ_NOACTION;
}

static void filtermethod_default_remove(FilterLayer *layer)
{
    return;
}

PR_BEGIN_EXTERN_C

static PRStatus filtermethod_close(PRFileDesc *fd)
{
    PR_ASSERT(fd->identity == _filter_identity);
    FilterLayer *layer = (FilterLayer *)fd;
    Session *sn = layer->context->sn;

    // N.B. this duplicates code in filter_remove_layer()

    // Bail if we're already in the process of removing this layer
    FilterContextInternal *context = (FilterContextInternal *)layer->context;
    if (context->flags & FILTERCONTEXTINTERNAL_MARKED_FOR_DEATH)
        return PR_SUCCESS;
    context->flags |= FILTERCONTEXTINTERNAL_MARKED_FOR_DEATH;

    // Give the filter a chance to tidy up
    layer->filter->remove(layer);

    // Destroy the filter's context
    filter_destroy_context(layer);

    // Destroy the IO layer and propagate the close down the stack
    return filter_close_iolayer(sn, fd);
}

static PRStatus filtermethod_default_fsync(PRFileDesc *fd)
{
    return fd->lower->methods->fsync(fd->lower);
}

static PRInt32 filtermethod_default_read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    return fd->lower->methods->read(fd->lower, buf, amount);
}

static PRInt32 filtermethod_default_write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    return fd->lower->methods->write(fd->lower, buf, amount);
}

static PRInt32 filtermethod_default_writev(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    return fd->lower->methods->writev(fd->lower, iov, iov_size, timeout);
}

static PRInt32 filtermethod_default_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    return fd->lower->methods->recv(fd->lower, buf, amount, flags, timeout);
}

static PRInt32 filtermethod_default_send(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    return fd->lower->methods->send(fd->lower, buf, amount, flags, timeout);
}

static PRInt32 filtermethod_default_sendfile(PRFileDesc *fd, PRSendFileData *data, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    return fd->lower->methods->sendfile(fd->lower, data, flags, timeout);
}

PR_END_EXTERN_C


/* ----------------------- filter method emulation ------------------------ */

static PRInt32 XP_EmulateWritev(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    PRInt32 nsent = 0;
    int i;

    for (i = 0; i < iov_size; i++) {
        PRInt32 rv;

        rv = PR_Send(fd, iov[i].iov_base, iov[i].iov_len, 0, timeout);
        if (rv == PR_FAILURE) {
            return PR_FAILURE;
        } else if (rv != iov[i].iov_len) {
            return nsent + rv;
        }

        nsent += rv;
    }

    return nsent;
}

PR_BEGIN_EXTERN_C

static PRInt32 filtermethod_emulate_read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PR_ASSERT(fd->identity == _filter_identity);
    FilterLayer *layer = (FilterLayer *)fd;

    return layer->filter->read(layer, buf, amount, NET_INFINITE_TIMEOUT);
}

static PRInt32 filtermethod_emulate_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    PR_ASSERT(fd->identity == _filter_identity);
    FilterLayer *layer = (FilterLayer *)fd;

    int net_timeout;
    if (timeout == PR_INTERVAL_NO_WAIT) {
        net_timeout = NET_ZERO_TIMEOUT;
    } else if (timeout == PR_INTERVAL_NO_TIMEOUT) {
        net_timeout = NET_INFINITE_TIMEOUT;
    } else {
        net_timeout = PR_IntervalToSeconds(timeout);
    }

    return layer->filter->read(layer, buf, amount, net_timeout);
}

static PRInt32 filtermethod_emulate_writev(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    return XP_EmulateWritev(fd, iov, iov_size, timeout);
}

static PRInt32 filtermethod_emulate_sendfile(PRFileDesc *fd, PRSendFileData *data, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    // XXX Uses mmap(), so it can exhaust address space.  Roll our own?
    // XXX How can we tell NSFC to cache file content instead of the fd?
    // XXX Maybe if (filter_has_sendfile(sn->csd)) ... ?
    return PR_EmulateSendFile(fd, data, flags, timeout);
}

NSAPI_PUBLIC int filter_emulate_writev(FilterLayer *layer, const NSAPIIOVec *iov, int iov_size)
{
    return filtermethod_emulate_writev((PRFileDesc *)layer, (const PRIOVec *)iov, iov_size, PR_INTERVAL_NO_TIMEOUT);
}

NSAPI_PUBLIC int filter_emulate_sendfile(FilterLayer *layer, sendfiledata *sfd)
{
    return filtermethod_emulate_sendfile((PRFileDesc *)layer, (PRSendFileData *)sfd, PR_TRANSMITFILE_KEEP_OPEN, PR_INTERVAL_NO_TIMEOUT);
}

PR_END_EXTERN_C


/* ------------------------ null IO layer methods ------------------------- */

PR_BEGIN_EXTERN_C

PRStatus iolayer_null_close(PRFileDesc *fd)
{ 
    // N.B. secret must be NULL or a valid Session *
    Session *sn = (Session *)fd->secret;
    fd->secret = NULL;
    return filter_close_iolayer(sn, fd);
}

PRInt32 iolayer_null_read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PR_SetError(PR_END_OF_FILE_ERROR, 0);
    return 0;
}

PRInt32 iolayer_null_write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    PR_SetError(PR_NO_DEVICE_SPACE_ERROR, 0);
    return -1;
}

PRInt32 iolayer_null_available(PRFileDesc *fd)
{
    return 0;
}

PRInt64 iolayer_null_available64(PRFileDesc *fd)
{
    return 0;
}

PRStatus iolayer_null_fsync(PRFileDesc *fd)
{
    PR_SetError(PR_NO_DEVICE_SPACE_ERROR, 0);
    return PR_FAILURE;
}

PROffset32 iolayer_null_seek(PRFileDesc *fd, PROffset32 offset, PRSeekWhence how)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PROffset64 iolayer_null_seek64(PRFileDesc *fd, PROffset64 offset, PRSeekWhence how)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRStatus iolayer_null_fileinfo(PRFileDesc *fd, PRFileInfo *info)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus iolayer_null_fileinfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 iolayer_null_writev(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    PR_SetError(PR_NO_DEVICE_SPACE_ERROR, 0);
    return -1;
}

PRStatus iolayer_null_connect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRFileDesc *iolayer_null_accept(PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return NULL;
}

PRStatus iolayer_null_bind(PRFileDesc *fd, const PRNetAddr *addr)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus iolayer_null_listen(PRFileDesc *fd, PRIntn backlog)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus iolayer_null_shutdown(PRFileDesc *fd, PRIntn how)
{
    return PR_SUCCESS;
}

PRInt32 iolayer_null_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    PR_SetError(PR_END_OF_FILE_ERROR, 0);
    return 0;
}

PRInt32 iolayer_null_send(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    PR_SetError(PR_NO_DEVICE_SPACE_ERROR, 0);
    return -1;
}

PRInt32 iolayer_null_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 iolayer_null_sendto(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt16 iolayer_null_poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    *out_flags = 0;
    return in_flags;
}

PRInt32 iolayer_null_acceptread(PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime t)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

PRInt32 iolayer_null_transmitfile(PRFileDesc *sd, PRFileDesc *fd, const void *headers, PRInt32 hlen, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    PR_SetError(PR_NO_DEVICE_SPACE_ERROR, 0);
    return -1;
}

PRStatus iolayer_null_getsockname(PRFileDesc *fd, PRNetAddr *addr)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus iolayer_null_getpeername(PRFileDesc *fd, PRNetAddr *addr)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus iolayer_null_getsocketoption(PRFileDesc *fd, PRSocketOptionData *data)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRStatus iolayer_null_setsocketoption(PRFileDesc *fd, const PRSocketOptionData *data)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PRInt32 iolayer_null_sendfile(PRFileDesc *sd, PRSendFileData *sendData, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    PR_SetError(PR_NO_DEVICE_SPACE_ERROR, 0);
    return -1;
}

PRStatus iolayer_null_connectcontinue(PRFileDesc *fd, PRInt16 out_flags)
{
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

PR_END_EXTERN_C


/* ----------------------- callback filter methods ------------------------ */

PR_BEGIN_EXTERN_C

static int filtermethod_callback_flush(FilterLayer *layer)
{
    PRFileDesc *fd = (PRFileDesc *)layer;
 
    filter_generic_callback(layer->context->sn);

    return fd->lower->methods->fsync(fd->lower);
}

static int filtermethod_callback_read(FilterLayer *layer, void *buf, int amount, int timeout)
{
    PRFileDesc *fd = (PRFileDesc *)layer;
 
    filter_read_callback(layer->context->sn);

    return net_read(fd->lower, buf, amount, timeout);
}

static int filtermethod_callback_write(FilterLayer *layer, const void *buf, int amount)
{
    PRFileDesc *fd = (PRFileDesc *)layer;
    Session *sn = layer->context->sn;

    int rv = filter_output_callback(sn);
    if (rv == REQ_PROCEED) {
        // Added new filters, start again from the top of the stack
        return sn->csd->methods->write(sn->csd, buf, amount);
    }
    if (rv != REQ_NOACTION)
        return -1;

    return fd->lower->methods->write(fd->lower, buf, amount);
}

static PRInt32 filtermethod_callback_writev(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    FilterLayer *layer = (FilterLayer *)fd;
    Session *sn = layer->context->sn;

    int rv = filter_output_callback(sn);
    if (rv == REQ_PROCEED) {
        // Added new filters, start again from the top of the stack
        return sn->csd->methods->writev(sn->csd, iov, iov_size, timeout);
    }
    if (rv != REQ_NOACTION)
        return -1;

    return fd->lower->methods->writev(fd->lower, iov, iov_size, timeout);
}

static PRInt32 filtermethod_callback_sendfile(PRFileDesc *fd, PRSendFileData *data, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    FilterLayer *layer = (FilterLayer *)fd;
    Session *sn = layer->context->sn;

    int rv = filter_output_callback(sn);
    if (rv == REQ_PROCEED) {
        // Added new filters, start again from the top of the stack
        return sn->csd->methods->sendfile(sn->csd, data, flags, timeout);
    }
    if (rv != REQ_NOACTION)
        return -1;

    return fd->lower->methods->sendfile(fd->lower, data, flags, timeout);
}

PR_END_EXTERN_C


/* ----------------------------- filter_fill ------------------------------ */

static Filter *filter_fill(const char *name, int order, const FilterMethods *methods, int flags, Filter *filter)
{
    filter->name = PERM_STRDUP(name);
    filter->flags = flags;
    filter->order = order & FILTER_MASK; // Reserve topmost bits for future use

    /*
     * Setup NSAPI-specific filter methods
     */

    if (methods->insert) {
        filter->insert = methods->insert;
    } else {
        filter->insert = &filtermethod_always_insert;
    }

    if (methods->remove) {
        filter->remove = methods->remove;
    } else {
        filter->remove = &filtermethod_default_remove;
    }

    if (methods->read) {
        filter->read = methods->read;
    } else {
        filter->read = NULL;
    }

    /*
     * Setup NSPR filter methods
     */

    filter->priomethods = *PR_GetDefaultIOMethods();
    filter->priomethods.file_type = PR_DESC_LAYERED;

    // Assert NSPR IO layer and NSAPI filter binary compatibility
    PR_ASSERT(sizeof(PRSendFileData) == sizeof(sendfiledata));
    PR_ASSERT(sizeof(PRIOVec) == sizeof(NSAPIIOVec));
    PR_ASSERT(sizeof(PRStatus) == sizeof(int));
    PR_ASSERT(sizeof(PRInt32) == sizeof(int));
    PR_ASSERT(sizeof(PRSize) == sizeof(size_t));

    filter->priomethods.close = &filtermethod_close;

    if (methods->flush) {
        filter->priomethods.fsync = (PRFsyncFN)methods->flush;
    } else {
        filter->priomethods.fsync = &filtermethod_default_fsync;
    }

    if (methods->read) {
        filter->priomethods.read = &filtermethod_emulate_read;
        filter->priomethods.recv = &filtermethod_emulate_recv;
    } else {
        filter->priomethods.read = &filtermethod_default_read;
        filter->priomethods.recv = &filtermethod_default_recv;
    }

    if (methods->write) {
        filter->priomethods.write = (PRWriteFN)methods->write;
        filter->priomethods.send = (PRSendFN)methods->write;
    } else {
        filter->priomethods.write = &filtermethod_default_write;
        filter->priomethods.send = &filtermethod_default_send;
    }

    if (methods->writev) {
        filter->priomethods.writev = (PRWritevFN)methods->writev;
    } else if (methods->write) {
        ereport(LOG_VERBOSE, "Emulating writev for filter %s", filter->name);
        filter->priomethods.writev = &filtermethod_emulate_writev;
    } else {
        filter->priomethods.writev = &filtermethod_default_writev;
    }

    if (methods->sendfile) {
        filter->priomethods.sendfile = (PRSendfileFN)methods->sendfile;
    } else if (methods->write) {
        ereport(LOG_VERBOSE, "Emulating sendfile for filter %s", filter->name);
        filter->priomethods.sendfile = &filtermethod_emulate_sendfile;
    } else {
        filter->priomethods.sendfile = &filtermethod_default_sendfile;
    }

    /*
     * Add the filter to the front of the server-wide list
     */

    PR_Lock(_filter_list_lock);

    filter->next = _filter_list;
    _filter_list = filter;

    _filter_hash.remove((void *)filter->name);
    _filter_hash.insert((void *)filter->name, filter);

    PR_Unlock(_filter_list_lock);

    return filter;
}


/* ------------------------ filter_create_internal ------------------------ */

NSAPI_PUBLIC const Filter *filter_create_internal(const char *name, int order, const FilterMethods *methods, int flags)
{
    /*
     * Deal with the extensible, variable-sized FilterMethods struct
     */

    FilterMethods ourmethods = FILTER_METHODS_INITIALIZER;
    int supplied = METHODS_COUNT(methods);
    int supported = METHODS_COUNT(&ourmethods);
    int unsupported = 0;
    int i;

    // Check for unsupported methods
    for (i = supported; i < supplied; i++) {
        if (METHODS_ARRAY(methods)[i] != NULL)
            unsupported++;
    }
    if (unsupported) {
        ereport(LOG_WARN, XP_GetAdminStr(DBT_FilterXDefinesYUnsupportedMethods),
                name, unsupported);
    }

    // Copy the caller's method pointers to our initialized method array
    if (supported > supplied)
        supported = supplied;
    memcpy(METHODS_ARRAY(&ourmethods), METHODS_ARRAY(methods), supported * sizeof(void *));

    /*
     * Create the filter
     */

    Filter *filter = (Filter *)PERM_CALLOC(sizeof(Filter));
    if (filter)
        filter_fill(name, order, &ourmethods, flags, filter);

    return filter;
}


/* ---------------------------- filter_create ----------------------------- */

NSAPI_PUBLIC const Filter *filter_create(const char *name, int order, const FilterMethods *methods)
{
    /*
     * Validate parameters
     */

    if (!name || !*name || !methods)
        return NULL;

    // Reserve part of the filter namespace for internal use
    if (isdigit(name[0]) ||
        !strcasecmp(name, FILTER_NAME_TOP) ||
        !strcasecmp(name, FILTER_NAME_BOTTOM) ||
        !strncasecmp(name, FILTER_PREFIX_SERVER, strlen(FILTER_PREFIX_SERVER)) ||
        !strncasecmp(name, FILTER_PREFIX_MAGNUS, strlen(FILTER_PREFIX_MAGNUS)))
    {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_FilterNameXIsReserved), name);
        return NULL;
    }

    // Users can't override the NSPR/NSS pseudo-filters... maybe later?
    if (!strcasecmp(name, FILTER_NAME_NETWORK) || !strcasecmp(name, FILTER_NAME_SSL)) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_FilterXCannotBeOverriden), name);
        return NULL;
    }

    // Determine flags to use for filter
    int flags = 0;
    if (func_get_default_flag(FUNC_USE_NATIVE_THREAD)) {
        // We're being called from a NativeThread="yes" module's Init function
        flags |= FILTER_USE_NATIVE_THREAD;
    }

    return filter_create_internal(name, order, methods, flags);
}


/* ----------------------------- filter_name ------------------------------ */

NSAPI_PUBLIC const char *filter_name(const Filter *filter)
{
    if (filter)
        return filter->name;

    return NULL;
}


/* ----------------------------- filter_find ------------------------------ */

NSAPI_PUBLIC const Filter *filter_find(const char *name)
{
    PR_Lock(_filter_list_lock);

    const Filter *filter = (const Filter *)_filter_hash.lookup((void *)name);

    PR_Unlock(_filter_list_lock);

    PR_ASSERT(!filter || !strcmp(filter->name, name));

    return filter;
}


/* ----------------------------- filter_layer ----------------------------- */

NSAPI_PUBLIC FilterLayer *filter_layer(SYS_NETFD fd, const Filter *filter)
{
    if (!filter)
        return NULL;

    while (fd) {
        if (fd->methods == &filter->priomethods)
            return (FilterLayer *)fd;

        fd = fd->lower;
    }
    
    return NULL;
}


/* ---------------------------- filter_insert ----------------------------- */

NSAPI_PUBLIC int filter_insert(SYS_NETFD fd, pblock *pb, Session *sn, Request *rq, void *data, const Filter *filter)
{
    // Use session's csd if caller didn't specify an fd
    if (!fd && sn && sn->csd_open == 1)
        fd = sn->csd;

    if (!fd || !filter)
        return REQ_NOACTION;

    PR_ASSERT((void *)filter == (void *)&filter->priomethods);

    // Check Request-based filter preconditions
    if (rq) {
        // Request-based filters must be associated with a session
        if (rq && !sn) {
            NsprError::setError(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_MissingSession));
            return REQ_ABORTED;
        }

        // Don't insert FILTER_CONTENT_CODING and lower filters on child
        // requests
        if (INTERNAL_REQUEST(rq) && filter->order < FILTER_SUBREQUEST_BOUNDARY)
            return REQ_NOACTION;
    }

    // Check for a NativeThread="yes" filter running on a local thread 
    if (filter->flags & FILTER_USE_NATIVE_THREAD) {
        if (!_filter_logged_native_thread_issue) {
            // XXX this is inefficient
            if (PR_GetThreadScope(PR_GetCurrentThread()) == PR_LOCAL_THREAD) {
                _filter_logged_native_thread_issue = PR_TRUE;
                log_error(LOG_MISCONFIG, "filter-insert", sn, rq, XP_GetAdminStr(DBT_FilterXRequiresNativeThreads), filter->name);
            }
        }
    }

    // Find where the filter belongs in the filter stack
    PRFileDesc *bottom = NULL;
    PRFileDesc *unknown = NULL;
    PRFileDesc *position;
    for (position = fd; position; position = position->lower) {
        // Find a layer we can determine the order of
        int order = filter_iolayer_order(position);
        if (order < 0) {
            if (!unknown)
                unknown = position;

            continue;
        }

        // Does the new layer belong above this layer?
        if (filter->order >= order) {
            // If there are non-filter IO layers directly above this layer,
            // the filter belongs above them
            if (unknown)
                position = unknown;

            break;
        }

        unknown = NULL;

        // This may be the bottom-most layer with a known order
        bottom = position;
    }
    if (!position) {
        if (!bottom) {
            // We're the first layer, we go on top
            position = fd;
        } else if (bottom->lower) {
            // Insert below the lowest layer with a known order
            position = bottom->lower;
        } else {
            // Nowhere to install the filter
            NsprError::setError(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_InvalidFilterStack));
            return REQ_ABORTED;
        }
    }

    // Get an IO layer for the filter
    PRFileDesc *iolayer = filter_alloc_filter_iolayer(sn, filter);
    if (!iolayer)
        return REQ_ABORTED;

    // Initialize filter layer context
    FilterContext *context = filter_create_context(sn, rq, data, filter, (FilterLayer *)iolayer);
    if (!context) {
        // Dispose of the IO layer
        filter_free_iolayer(sn, iolayer);

        return REQ_ABORTED;
    }

    // Install the filter's IO layer
    if (filter_insert_iolayer(position, &iolayer) != REQ_PROCEED) {
        // Destroy the filter's context
        filter_destroy_context((FilterLayer *)iolayer);

        // Dispose of the IO layer
        filter_free_iolayer(sn, iolayer);

        return REQ_ABORTED;
    }
    PR_ASSERT(iolayer->methods == &filter->priomethods);

    // Give the filter a chance to initialize
    int rv = filter->insert((FilterLayer *)iolayer, pb ? pb : _empty_pb);
    if (rv != REQ_PROCEED) {
        // Destroy the filter's context
        filter_destroy_context((FilterLayer *)iolayer);

        // Remove the IO layer from the stack
        iolayer = filter_extract_iolayer(iolayer);

        // Dispose of the IO layer
        filter_free_iolayer(sn, iolayer);

        return rv;
    }

    // Let the NSAPIRequest know it needs to clean up after a filter
    if (rq) {
        NSAPIRequest *nrq = (NSAPIRequest *)rq;
        NSAPISession *nsn = (NSAPISession *)sn;
        PR_ASSERT(nrq->filter_sn == NULL || nrq->filter_sn == sn);
        nrq->filter_sn = sn;
    }

    // If the new filter doesn't call the callbacks...
    if (!(filter->flags & FILTER_CALLS_CALLBACKS)) {
        // If the new filter was added to the top of the stack...
        if (position == fd && position->lower && position->lower->identity == _filter_identity) {
            // If the new filter is masking a filter that does call the
            // callbacks (i.e. httpfilter)...
            FilterLayer *lower = (FilterLayer *)position->lower;
            if (lower->filter->flags & FILTER_CALLS_CALLBACKS) {
                // Add a callback filter to the top of the stack
                filter_insert(fd, NULL, lower->context->sn, NULL, NULL, _filter_callback);
            }
        }
    }

    return rv;
}


/* ------------------------- filter_remove_layer -------------------------- */

static inline PRBool filter_remove_layer(PRFileDesc *fd)
{
    PR_ASSERT(fd->identity == _filter_identity);
    FilterLayer *layer = (FilterLayer *)fd;
    Session *sn = layer->context->sn;

    // N.B. this code is duplicated in filtermethod_close()

    // Bail if we're already in the process of removing this layer
    FilterContextInternal *context = (FilterContextInternal *)layer->context;
    if (context->flags & FILTERCONTEXTINTERNAL_MARKED_FOR_DEATH)
        return PR_FALSE;
    context->flags |= FILTERCONTEXTINTERNAL_MARKED_FOR_DEATH;

    // Give the filter a chance to tidy up
    layer->filter->remove(layer);

    // Destroy the filter's context
    filter_destroy_context(layer);

    // Remove the IO layer from the stack
    fd = filter_extract_iolayer(fd);

    // Dispose of the IO layer
    filter_free_iolayer(sn, fd);

    return PR_TRUE;
}


/* ---------------------------- filter_remove ----------------------------- */

NSAPI_PUBLIC int filter_remove(SYS_NETFD fd, const Filter *filter)
{
    // Pseudo-filters cannot be removed
    if (filter->insert == &filtermethod_never_insert) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_FilterXCannotBeRemoved), filter->name);
        return REQ_ABORTED;
    }

    while (fd) {
        // If this is the requested filter...
        if (fd->methods == &filter->priomethods) {
            if (filter_remove_layer(fd))
                return REQ_PROCEED;
        }

        fd = fd->lower;
    }

    return REQ_NOACTION;
}


/* -------------------------- filter_set_request -------------------------- */

NSAPI_PUBLIC void filter_set_request(Session *sn, Request *rq)
{
filter_set_request:
    for (PRFileDesc *fd = sn->csd; fd; fd = fd->lower) {
        // Skip non-filter IO layers
        if (fd->identity != _filter_identity)
            continue;

        FilterLayer *layer = (FilterLayer *)fd;

        // Skip layers not associated with a Request
        if (!layer->context->rq)
            continue;

        // Skip layers associated with the current Request
        if (layer->context->rq == rq)
            continue;

        // Skip layers associated with a "lower" Request
        Request *curr_rq = rq;
        while (curr_rq->orig_rq != curr_rq) {
            curr_rq = curr_rq->orig_rq;
            if (layer->context->rq == curr_rq)
                break;
        }
        if (layer->context->rq == curr_rq)
            continue;

        // Skip layers associated with the original Request, even if the
        // child request doesn't contain an orig_rq pointer to it
        PR_ASSERT(INTERNAL_REQUEST(layer->context->rq));
        if (!INTERNAL_REQUEST(layer->context->rq))
            continue;

        // This layer is from a "higher" Request; it's now inactive
        NSAPIRequest *inactive_nrq = (NSAPIRequest *)layer->context->rq;
        PR_ASSERT(inactive_nrq->filter_sn == sn);
        inactive_nrq->output_done = PR_TRUE;
        inactive_nrq->output_rv = REQ_ABORTED;

        // Remove the inactive layer
        if (filter_remove_layer(fd)) {
            // Resume search from the top of the stack; any PRFileDesc * below
            // the top of the stack could now be invalid
            goto filter_set_request;
        }
    }

    // Record the currently active Request
    NSAPISession *nsn = (NSAPISession *)sn;
    nsn->filter_rq = (NSAPIRequest *)rq;

    PR_ASSERT(nsn->filter_rq == nsn->exec_rq);
}


/* ------------------------ filter_finish_request ------------------------- */

NSAPI_PUBLIC int filter_finish_request(Session *sn, Request *rq, const Filter *above)
{
    if (!sn->csd_open)
        return REQ_EXIT;

    int rv = REQ_NOACTION;

    PRFileDesc *fd = sn->csd;
    while (fd) {
        // If this is a filter...
        if (fd->identity == _filter_identity) {
            FilterLayer *layer = (FilterLayer *)fd;

            if (layer->filter == above)
                break;

            if (layer->context->rq == rq) {
                // We did something
                rv = REQ_PROCEED;

                // Remove the inactive layer
                if (filter_remove_layer(fd)) {
                    // Resume search from the top of the stack; any
                    // PRFileDesc * below the top of the stack could now be
                    // invalid
                    fd = sn->csd;
                    continue;
                }
            }
        }
 
        fd = fd->lower;
    }

    return rv;
}


/* ------------------------ filter_finish_response ------------------------ */

NSAPI_PUBLIC void filter_finish_response(Session *sn)
{
    if (!sn->csd_open)
        return;

    PRFileDesc *fd = sn->csd;
    while (fd) {
        // If this is a filter...
        if (fd->identity == _filter_identity) {
            FilterLayer *layer = (FilterLayer *)fd;

            // If the filter is associated with a Session...
            if (layer->context->sn) {
                PR_ASSERT(layer->context->sn == sn);

                // Break the association between the Request and this filter
                // stack; we'll remove all the Request's filters
                if (layer->context->rq) {
                    NSAPIRequest *nrq = (NSAPIRequest *)layer->context->rq;
                    PR_ASSERT(nrq->filter_sn == sn || !nrq->filter_sn);
                    nrq->filter_sn = NULL;
                }

                // Remove the filter from the stack
                if (filter_remove_layer(fd)) {
                    // Resume search from the top of the stack; any
                    // PRFileDesc * below the top of the stack could now be
                    // invalid
                    fd = sn->csd;
                    continue;
                }
            }

            // Request-based filters must also have a Session
            PR_ASSERT(!layer->context->rq || layer->context->sn);
        }
 
        fd = fd->lower;
    }
}


/* --------------------------- filter_create_stack --------------------------- */

SYS_NETFD filter_create_stack(Session *sn)
{
    // The bottom layer of the stack will be a "null" NSPR IO layer that
    // doesn't actually do anything
    PRFileDesc *fd = filter_alloc_null_iolayer(sn, &_filter_null_priomethods);
    if (!fd)
        return SYS_NET_ERRORFD;

    fd->higher = NULL;
    fd->lower = NULL;

    // Add a callback filter to the top of the stack
    if (sn)
        filter_insert(fd, NULL, sn, NULL, NULL, _filter_callback);

    return fd;
}


/* ----------------------------- filter_init ------------------------------ */

NSAPI_PUBLIC PRStatus filter_init(void)
{
    _filter_list_lock = PR_NewLock();

    // Get an IO layer identity for active NSAPI filters
    _filter_identity = PR_GetUniqueIdentity("NSAPI filter");

    // Figure out the NSS IO layer identity
    PRFileDesc *model = PR_NewTCPSocket();
    if (model) {
        model = SSL_ImportFD(NULL, model);
        _filter_ssl_identity = model->identity;
        PR_Close(model);
    }

    // Get an IO layer identity for our "null" (bottom of stack) IO layer
    _filter_null_identity = PR_GetUniqueIdentity("NSAPI filter stack");

    // Initialize the PRIOMethods for our "null" (bottom of stack) IO layer
    _filter_null_priomethods = *PR_GetDefaultIOMethods();
    _filter_null_priomethods.file_type = PR_DESC_PIPE; // Not socket/file/layer
    _filter_null_priomethods.close = &iolayer_null_close;
    _filter_null_priomethods.read = &iolayer_null_read;
    _filter_null_priomethods.write = &iolayer_null_write;
    _filter_null_priomethods.available = &iolayer_null_available;
    _filter_null_priomethods.available64 = &iolayer_null_available64;
    _filter_null_priomethods.fsync = &iolayer_null_fsync;
    _filter_null_priomethods.seek = &iolayer_null_seek;
    _filter_null_priomethods.seek64 = &iolayer_null_seek64;
    _filter_null_priomethods.fileInfo = &iolayer_null_fileinfo;
    _filter_null_priomethods.fileInfo64 = &iolayer_null_fileinfo64;
    _filter_null_priomethods.writev = &iolayer_null_writev;
    _filter_null_priomethods.connect = &iolayer_null_connect;
    _filter_null_priomethods.accept = &iolayer_null_accept;
    _filter_null_priomethods.bind = &iolayer_null_bind;
    _filter_null_priomethods.listen = &iolayer_null_listen;
    _filter_null_priomethods.shutdown = &iolayer_null_shutdown;
    _filter_null_priomethods.recv = &iolayer_null_recv;
    _filter_null_priomethods.send = &iolayer_null_send;
    _filter_null_priomethods.recvfrom = &iolayer_null_recvfrom;
    _filter_null_priomethods.sendto = &iolayer_null_sendto;
    _filter_null_priomethods.poll = &iolayer_null_poll;
    _filter_null_priomethods.acceptread = &iolayer_null_acceptread;
    _filter_null_priomethods.transmitfile = &iolayer_null_transmitfile;
    _filter_null_priomethods.getsockname = &iolayer_null_getsockname;
    _filter_null_priomethods.getpeername = &iolayer_null_getpeername;
    _filter_null_priomethods.getsocketoption = &iolayer_null_getsocketoption;
    _filter_null_priomethods.setsocketoption = &iolayer_null_setsocketoption;
    _filter_null_priomethods.sendfile = &iolayer_null_sendfile;
    _filter_null_priomethods.connectcontinue = &iolayer_null_connectcontinue;

    // Initialize pseudo-filters
    FilterMethods pseudo_methods = FILTER_METHODS_INITIALIZER;
    pseudo_methods.insert = &filtermethod_never_insert; // user can't insert these
    filter_fill(FILTER_NAME_TOP, 0x7fffffff /* top */, &pseudo_methods, 0, &_filter_top);
    filter_fill(FILTER_NAME_BOTTOM, 0 /* bottom */, &pseudo_methods, 0, &_filter_bottom);
    filter_fill(FILTER_NAME_NETWORK, FILTER_NETWORK, &pseudo_methods, 0, &_filter_network);
    filter_fill(FILTER_NAME_SSL, FILTER_TRANSPORT_CODING, &pseudo_methods, 0, &_filter_ssl);

    // Create a filter to call into Output stage and filter_set_request()
    FilterMethods callback_methods = FILTER_METHODS_INITIALIZER;
    callback_methods.insert = &filtermethod_always_insert;
    callback_methods.flush = &filtermethod_callback_flush;
    callback_methods.read = &filtermethod_callback_read;
    callback_methods.write = &filtermethod_callback_write;
    callback_methods.writev = (FilterWritevFunc*)&filtermethod_callback_writev;
    callback_methods.sendfile = (FilterSendfileFunc*)&filtermethod_callback_sendfile;
    _filter_callback = filter_create_internal("magnus-internal/callback",
                                              FILTER_TOPMOST,
                                              &callback_methods,
                                              FILTER_CALLS_CALLBACKS);
    if (!_filter_callback)
        return PR_FAILURE;

    // We'll use this per-DaemonSession slot to cache unused PRFileDescs
    _filter_prfiledesc_slot = session_alloc_thread_slot(NULL);

    // Will pass this empty pblock (instead of NULL) if filter_insert() is
    // called with a NULL pblock *
    _empty_pb = pblock_create(1);

    return PR_SUCCESS;
}
