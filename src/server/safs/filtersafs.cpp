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
 * filtersafs.cpp: SAFs related to NSAPI filters
 * 
 * Chris Elving
 */

#include "base/util.h"
#include "base/vs.h"
#include "frame/log.h"
#include "frame/object.h"
#include "frame/filter.h"
#include "safs/dbtsafs.h"
#include "safs/filtersafs.h"


// Names of the SAFs as they should appear in log messages
#define INSERT_FILTER "insert-filter"
#define REMOVE_FILTER "remove-filter"
#define INIT_FILTER_ORDER "init-filter-order"


/* ------------------------ filter_init_directive ------------------------- */

static int filter_init_directive(const directive *dir, VirtualServer *incoming, const VirtualServer *current)
{
    // Cache a const Filter * in the pblock if possible
    const char *name = pblock_findkeyval(pb_key_filter, dir->param.pb);
    if (name) {
        const Filter *filter = filter_find(name);
        if (filter)
            pblock_kvinsert(pb_key_magnus_internal, (const char *)&filter, sizeof(filter), dir->param.pb);
    }
    return REQ_PROCEED;
}


/* ----------------------------- find_filter ------------------------------ */

static inline const Filter *find_filter(pblock *pb, Session *sn, Request *rq)
{
    const Filter *filter;

    // Look for a cached const Filter *
    const Filter **pfilter = (const Filter **)pblock_findkeyval(pb_key_magnus_internal, pb);
    if (pfilter) {
        filter = *pfilter;
        PR_ASSERT(filter != NULL);
        return filter;
    }

    // No cached const Filter *, find the filter name
    const char *name = pblock_findkeyval(pb_key_filter, pb);
    if (!name) {
        log_error(LOG_MISCONFIG, pblock_findkeyval(pb_key_fn, pb), sn, rq,
                  XP_GetAdminStr(DBT_NeedFilter));
        return NULL;
    }

    // Find the const Filter * with the specified name (this is slow)
    filter = filter_find(name);
    if (!filter) {
        log_error(LOG_MISCONFIG, pblock_findkeyval(pb_key_fn, pb), sn, rq,
                  XP_GetAdminStr(DBT_CantFindFilterX), name);
        return NULL;
    }

    return filter;
}


/* ---------------------------- insert_filter ----------------------------- */

int insert_filter(pblock *pb, Session *sn, Request *rq)
{
    const Filter *filter = find_filter(pb, sn, rq);
    if (!filter)
        return REQ_ABORTED;

    int rv = filter_insert(sn->csd, pb, sn, rq, NULL, filter);
    if (rv == REQ_PROCEED) {
        ereport(LOG_VERBOSE, "inserted filter %s", filter_name(filter));
    } else if (rv == REQ_NOACTION) {
        ereport(LOG_VERBOSE, "did not insert filter %s", filter_name(filter));
    } else {
        log_error(LOG_WARN, INSERT_FILTER, sn, rq,
                  XP_GetAdminStr(DBT_ErrorInsertingFilterX),
                  filter_name(filter));
    }
        
    return rv;
}


/* ---------------------------- remove_filter ----------------------------- */

int remove_filter(pblock *pb, Session *sn, Request *rq)
{
    const Filter *filter = find_filter(pb, sn, rq);
    if (!filter)
        return REQ_ABORTED;

    int rv = REQ_NOACTION;
    if (sn->csd && sn->csd_open == 1)
        rv = filter_remove(sn->csd, filter);

    return rv;
}


/* ------------------------ validate_filter_order ------------------------- */

static void validate_filter_order(const Filter *filter, int order)
{
    if (FILTER_ORDER_CLASS(filter->order) != FILTER_ORDER_CLASS(order)) {
        log_error(LOG_WARN, INIT_FILTER_ORDER, NULL, NULL,
                  XP_GetAdminStr(DBT_InvalidOrderForX), filter->name);
    }
}


/* -------------------------- init_filter_order --------------------------- */

int init_filter_order(pblock *pb, Session *sn, Request *rq)
{
    // Get the list of filters to define the filter order for
    char *filters = pblock_findval("filters", pb);
    if (!filters) {
        pblock_nvinsert("error", XP_GetAdminStr(DBT_NeedFilters), pb);
        return REQ_ABORTED;
    }

    // Parse the list of filters
    int order = FILTER_TOPMOST;
    char *lasts;
    char *name = util_strtok(filters, ", \t", &lasts);
    while (name) {
        Filter *filter;

        if (*name == '(') {
            // Handle filter group.  The relative order of filters within the
            // '('- and ')'-delimited filter group doesn't matter.

            // Skip past the opening '('
            name++;

            // Handle all the filters in the group
            int group_order = order;
            while (name) {
                if (*name) {
                    // Remove any closing ')' from the filter name
                    char *paren = NULL;
                    if (name[strlen(name) - 1] == ')') {
                        paren = &name[strlen(name) - 1];
                        *paren = '\0';
                    }

                    // Handle a filter within the group
                    filter = (Filter *)filter_find(name);
                    if (filter) {
                        if (filter->order >= order) {
                            validate_filter_order(filter, order - 1);
                            filter->order = order - 1;
                        }
                        if (filter->order < group_order)
                            group_order = filter->order;
                    } else {
                        log_error(LOG_MISCONFIG, INIT_FILTER_ORDER, sn, rq,
                                  XP_GetAdminStr(DBT_CannotOrderNonexistentFilterX),
                                  name);
                    }

                    // Check for end of filter group
                    if (paren) {
                        *paren = ')';
                        break;
                    }
                }

                name = util_strtok(NULL, ", \t", &lasts);
            }
            if (!name) {
                log_error(LOG_MISCONFIG, INIT_FILTER_ORDER, sn, rq,
                          XP_GetAdminStr(DBT_FiltersMissingClosingParen));
            }
            order = group_order;

        } else {
            // Handle individual filter
            filter = (Filter *)filter_find(name);
            if (filter) {
                if (filter->order >= order) {
                    validate_filter_order(filter, order - 1);
                    filter->order = order - 1;
                }
                if (filter->order < order)
                    order = filter->order;
            } else {
                log_error(LOG_MISCONFIG, INIT_FILTER_ORDER, sn, rq,
                          XP_GetAdminStr(DBT_CannotOrderNonexistentFilterX),
                          name);
            }
        }

        // Next filter
        name = util_strtok(NULL, ", \t", &lasts);
    }

    return REQ_PROCEED;
}


/* --------------------------- filtersafs_init ---------------------------- */

PRStatus filtersafs_init(void)
{
    // Call filter_init_vs_directive() to cache the const Filter * for each
    // insert-filter and remove-filter directive
    vs_directive_register_cb(insert_filter, filter_init_directive, 0);
    vs_directive_register_cb(remove_filter, filter_init_directive, 0);

    return PR_SUCCESS;
}

