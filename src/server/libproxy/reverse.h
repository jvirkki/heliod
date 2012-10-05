/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef LIBPROXY_REVERSE_H
#define LIBPROXY_REVERSE_H

/*
 * reverse.h: Reverse proxying
 *
 * Ari Luotonen
 * Copyright (c) 1995 Netscape Communcations Corporation
 */

#include "netsite.h"

NSPR_BEGIN_EXTERN_C

/*
 * SAFs
 */
Func ntrans_reverse_map;

/*
 * reverse_init initializes the reverse mapping subsystem.
 */
PRStatus reverse_init(void);

/*
 * reverse_map_set_headers configures which headers will be rewritten by
 * inspecting rewrite-* parameters in pb.
 */
PRStatus reverse_map_set_headers(pblock *pb, Session *sn, Request *rq);

/*
 * reverse_map_add adds a new reverse mapping.
 */
PRStatus reverse_map_add(Session *sn, Request *rq, const char *src, const char *dst);

/*
 * reverse_map_for returns the reverse mapping for the specified URL (allocated
 * from the sn->pool pool) or NULL if there is no reverse mapping.
 */
char *reverse_map_for(Session *sn, Request *rq, const char *url);

/*
 * reverse_map_rewrite rewrites the headers in rq->srvhdrs.
 */
PRBool reverse_map_rewrite(Session *sn, Request *rq);

NSPR_END_EXTERN_C

#endif/* LIBPROXY_REVERSE */
