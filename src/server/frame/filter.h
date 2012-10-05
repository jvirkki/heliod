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

#ifndef FRAME_FILTER_H
#define FRAME_FILTER_H

/*
 * filter.h: NSAPI filter support
 * 
 * Chris Elving
 */

#define INTNSAPI
#include "netsite.h"

NSPR_BEGIN_EXTERN_C

enum FilterFlag {
    FILTER_USE_NATIVE_THREAD = 0x1,
    FILTER_CALLS_CALLBACKS = 0x2
};

struct Filter {
    PRIOMethods priomethods;
    PRDescIdentity identity;
    const Filter *next; /* next filter in server-wide list, not filter stack */
    int flags; /* bitwise OR of FilterFlags */
    int order;
    const char *name;
    FilterInsertFunc *insert;
    FilterRemoveFunc *remove;
    FilterReadFunc *read;
};

/*
 * Private filter order definitions for filter_create(). See nsapi.h for public
 * filter order definitions.
 */
#define FILTER_MASK                     0xfffff
#define FILTER_TOPMOST                  0xfffff
#define FILTER_SUBREQUEST_BOUNDARY      0x68000

/*
 * FILTER_ORDER_CLASS can be used to determine whether two filters are of the
 * same "class"; For example, a filter with order FILTER_CONTENT_CODING is of
 * the same class as filters with order (FILTER_CONTENT_CODING - 1) or order
 * (FILTER_CONTENT_CODING + 1).
 */
#define FILTER_ORDER_CLASS(order) (((order) + 0x7ff) & (FILTER_MASK & ~0xfff))

/*
 * Public functions declared in nsapi.h. See nsapi.h for descriptions.
 */

NSAPI_PUBLIC const Filter *filter_create(const char *name, int order, const FilterMethods *methods);
NSAPI_PUBLIC const char *filter_name(const Filter *filter);
NSAPI_PUBLIC const Filter *filter_find(const char *name);
NSAPI_PUBLIC FilterLayer *filter_layer(SYS_NETFD fd, const Filter *filter);
NSAPI_PUBLIC int filter_insert(SYS_NETFD fd, pblock *pb, Session *sn, Request *rq, void *data, const Filter *filter);
NSAPI_PUBLIC int filter_remove(SYS_NETFD fd, const Filter *filter);
NSAPI_PUBLIC SYS_NETFD filter_create_stack(Session *sn);

/*
 * filter_init initializes the NSAPI filter subsystem.
 */
NSAPI_PUBLIC PRStatus filter_init(void);

/*
 * filter_create_internal creates a server-internal filter. Unlike
 * filter_create, filter_create_internal does not restrict the filter name.
 * The names of undocumented server-internal filters should begin with the
 * "magnus-internal/" prefix.
 */
NSAPI_PUBLIC const Filter *filter_create_internal(const char *name, int order, const FilterMethods *methods, int flags);

/*
 * filter_set_request removes filters associated with inactive subrequests.
 */
NSAPI_PUBLIC void filter_set_request(Session *sn, Request *rq);

/*
 * filter_finish_request removes filters associated with the specified Request.
 * If above is specified, that filter and the filters below it are not removed.
 * Returns REQ_PROCEED if one or more filters were removed, REQ_NOACTION if no
 * filters were removed, or REQ_EXIT if sn->csd has already been closed.
 */
NSAPI_PUBLIC int filter_finish_request(Session *sn, Request *rq, const Filter *above);

/*
 * filter_finish_response removes all filters associated with Sessions.
 */
NSAPI_PUBLIC void filter_finish_response(Session *sn);

/*
 * filter_emulate_writev implements FilterWritevFunc using the layer's write
 * filter method.
 */ 
NSAPI_PUBLIC FilterWritevFunc filter_emulate_writev;

/*
 * filter_emulate_sendfile implements FilterSendfileFunc using the layer's
 * write filter method.
 */
NSAPI_PUBLIC FilterSendfileFunc filter_emulate_sendfile;

NSPR_END_EXTERN_C

#endif /* !FRAME_FILTER_H */
