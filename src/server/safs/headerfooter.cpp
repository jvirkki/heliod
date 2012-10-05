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
 * headerfooter.cpp: SAFs and filters for header and footer files
 * 
 * Chris Elving
 */

#include "base/net.h"
#include "base/util.h"
#include "base/pool.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/filter.h"
#include "frame/httpact.h"
#include "frame/httpfilter.h"
#include "safs/nsfcsafs.h"
#include "safs/dbtsafs.h"
#include "safs/headerfooter.h"


typedef struct HeaderFooter {
    void (*fn)(FilterLayer *layer, char *value);
    char *value;
    PRBool flagCheckSrvhdrs;
    PRBool flagSendHeaderFooter;
} HeaderFooter;

static FilterInsertFunc header_insert;
static FilterWriteFunc header_write;
static FilterWritevFunc header_writev;
static FilterSendfileFunc header_sendfile;
static FilterRemoveFunc header_remove;
static FilterInsertFunc footer_insert;
static FilterRemoveFunc footer_remove;

static const Filter *_header_filter;
static const Filter *_footer_filter;


/* ------------------------------ add-header ------------------------------ */

int pcheck_add_header(pblock *pb, Session *sn, Request *rq)
{
    if (!INTERNAL_REQUEST(rq)) {
        if (!_header_filter)
            return REQ_ABORTED;

        if (filter_insert(NULL, pb, sn, rq, NULL, _header_filter) != REQ_PROCEED)
            return REQ_ABORTED;
    }

    return REQ_NOACTION;
}


/* ------------------------------ add-footer ------------------------------ */

int pcheck_add_footer(pblock *pb, Session *sn, Request *rq)
{
    if (!INTERNAL_REQUEST(rq)) {
        if (!_footer_filter)
            return REQ_ABORTED;

        if (filter_insert(NULL, pb, sn, rq, NULL, _footer_filter) != REQ_PROCEED)
            return REQ_ABORTED;
    }

    return REQ_NOACTION;
}


/* ---------------------- headerfooter_check_srvhdrs ---------------------- */

static inline void headerfooter_check_srvhdrs(FilterLayer *layer)
{
    HeaderFooter *headerfooter = (HeaderFooter *)layer->context->data;

    // Remove Content-length, etc. on the first write
    if (headerfooter->flagCheckSrvhdrs) {
        headerfooter->flagCheckSrvhdrs = PR_FALSE;

        // We can't do ranges when there's a header/footer
        param_free(pblock_removekey(pb_key_accept_ranges, layer->context->rq->srvhdrs));
        if (layer->context->rq->status_num == PROTOCOL_PARTIAL_CONTENT) {
            headerfooter->flagSendHeaderFooter = PR_FALSE;
        }

        // The requested URI's Etag doesn't uniquely identify the response
        http_weaken_etag(layer->context->sn, layer->context->rq);

        // The presence of a header/footer mucks up the content-length
        param_free(pblock_removekey(pb_key_content_length, layer->context->rq->srvhdrs));
    }
}


/* ---------------------- headerfooter_send_absolute ---------------------- */

static void headerfooter_send_absolute(FilterLayer *layer, char *filename)
{
    NSFC_TransmitFile(layer->lower, filename, NULL, 0, NULL, 0,
                      PR_INTERVAL_NO_TIMEOUT,
                      GetServerFileCache(),
                      NULL);
}


/* ---------------------- headerfooter_send_relative ---------------------- */

static void headerfooter_send_relative(FilterLayer *layer, char *filename)
{
    const char* ntrans_base;

    ntrans_base = pblock_findval("ntrans-base", layer->context->rq->vars);
    if (ntrans_base) {
        int ntrans_base_len = strlen(ntrans_base);
        int filename_len = strlen(filename);
        char *path;

        path = (char *)pool_malloc(layer->context->pool, ntrans_base_len + 1 + filename_len + 1);
        if (path) {
            int pos = 0;

            memcpy(path, ntrans_base, ntrans_base_len);
            pos += ntrans_base_len;

            if (*filename != '/')
                path[pos++] = '/';

            strcpy(&path[pos], filename);

            NSFC_TransmitFile(layer->lower, path, NULL, 0, NULL, 0,
                              PR_INTERVAL_NO_TIMEOUT, 
                              GetServerFileCache(), 
                              NULL);

            pool_free(layer->context->pool, path);
        }
    }
}


/* ------------------------ headerfooter_send_uri ------------------------- */

static void headerfooter_send_uri(FilterLayer *layer, char *uri)
{
    servact_include_virtual(layer->context->sn, layer->context->rq, uri, NULL);
}


/* -------------------------- headerfooter_send --------------------------- */

static inline void headerfooter_send(FilterLayer *layer)
{
    HeaderFooter *headerfooter = (HeaderFooter *)layer->context->data;

    // Call the header/footer function once per response
    if (headerfooter->flagSendHeaderFooter) {
        headerfooter->flagSendHeaderFooter = PR_FALSE;
        headerfooter->fn(layer, headerfooter->value);
    }
}


/* ------------------------- headerfooter_insert -------------------------- */

static int headerfooter_insert(FilterLayer *layer, pblock *pb, const char *name)
{
    if (!layer->context->data) {
        HeaderFooter *headerfooter;
        const char *value;

        // Allocate header/footer context
        headerfooter = (HeaderFooter *)pool_malloc(layer->context->pool, sizeof(HeaderFooter));
        if (!headerfooter)
            return REQ_ABORTED;

        headerfooter->flagCheckSrvhdrs = PR_TRUE;
        headerfooter->flagSendHeaderFooter = PR_TRUE;

        // Populate header/footer context with parameters from pblock
        if ((value = pblock_findval("file", pb)) != NULL) {
            if (util_getboolean(pblock_findval("NSIntAbsFilePath", pb), PR_FALSE)) {
                headerfooter->fn = &headerfooter_send_absolute;
            } else {
                headerfooter->fn = &headerfooter_send_relative;
            }
            headerfooter->value = pool_strdup(layer->context->pool, value);

        } else if ((value = pblock_findval("uri", pb)) != NULL) {
            headerfooter->fn = headerfooter_send_uri;
            headerfooter->value = pool_strdup(layer->context->pool, value);

        } else {
            log_error(LOG_MISCONFIG, name, layer->context->sn, layer->context->rq, XP_GetAdminStr(DBT_NeedUriOrFile));
            pool_free(layer->context->pool, headerfooter);
            return REQ_ABORTED;
        }

        // We only want to buffer the data if a generatd Content-length header
        // would be useful
        if (KEEP_ALIVE(layer->context->rq))
            httpfilter_buffer_output(layer->context->sn, layer->context->rq, PR_TRUE);

        layer->context->data = headerfooter;
    }

    return REQ_PROCEED;
}


/* ------------------------- headerfooter_remove -------------------------- */

static void headerfooter_remove(FilterLayer *layer)
{
    HeaderFooter *headerfooter = (HeaderFooter *)layer->context->data;

    if (headerfooter->value)
        pool_free(layer->context->pool, headerfooter->value);

    pool_free(layer->context->pool, headerfooter);
}


/* ---------------------------- header_insert ----------------------------- */

static int header_insert(FilterLayer *layer, pblock *pb)
{
    return headerfooter_insert(layer, pb, "add-header");
}


/* ---------------------------- header_remove ----------------------------- */

static void header_remove(FilterLayer *layer)
{
    headerfooter_remove(layer);
}


/* ----------------------------- header_write ----------------------------- */

static int header_write(FilterLayer *layer, const void *buf, int amount)
{
    headerfooter_check_srvhdrs(layer);
    headerfooter_send(layer);
    return net_write(layer->lower, buf, amount);
}


/* ---------------------------- header_writev ----------------------------- */

static int header_writev(FilterLayer *layer, const NSAPIIOVec *iov, int iov_size)
{
    headerfooter_check_srvhdrs(layer);
    headerfooter_send(layer);
    return net_writev(layer->lower, iov, iov_size);
}


/* --------------------------- header_sendfile ---------------------------- */

static int header_sendfile(FilterLayer *layer, sendfiledata *sfd)
{
    headerfooter_check_srvhdrs(layer);
    headerfooter_send(layer);
    return net_sendfile(layer->lower, sfd);
}


/* ---------------------------- footer_insert ----------------------------- */

static int footer_insert(FilterLayer *layer, pblock *pb)
{
    return headerfooter_insert(layer, pb, "add-footer");
}


/* ---------------------------- footer_remove ----------------------------- */

static void footer_remove(FilterLayer *layer)
{
    headerfooter_send(layer);
    headerfooter_remove(layer);
}


/* ----------------------------- footer_write ----------------------------- */

static int footer_write(FilterLayer *layer, const void *buf, int amount)
{
    headerfooter_check_srvhdrs(layer);
    return net_write(layer->lower, buf, amount);
}


/* ---------------------------- footer_writev ----------------------------- */

static int footer_writev(FilterLayer *layer, const NSAPIIOVec *iov, int iov_size)
{
    headerfooter_check_srvhdrs(layer);
    return net_writev(layer->lower, iov, iov_size);
}


/* --------------------------- footer_sendfile ---------------------------- */

static int footer_sendfile(FilterLayer *layer, sendfiledata *sfd)
{
    headerfooter_check_srvhdrs(layer);
    return net_sendfile(layer->lower, sfd);
}


/* -------------------------- headerfooter_init --------------------------- */

PRStatus headerfooter_init(void)
{
    FilterMethods header_methods = FILTER_METHODS_INITIALIZER;
    FilterMethods footer_methods = FILTER_METHODS_INITIALIZER;

    /* Create the add-header filter */
    header_methods.insert = &header_insert;
    header_methods.remove = &header_remove;
    header_methods.write = &header_write;
    header_methods.writev = &header_writev;
    header_methods.sendfile = &header_sendfile;
    _header_filter = filter_create_internal("magnus-internal/add-header", FILTER_CONTENT_GENERATION, &header_methods, 0);
    if (!_header_filter)
        return PR_FAILURE;

    /* Create the add-footer filter */
    footer_methods.insert = &footer_insert;
    footer_methods.remove = &footer_remove;
    footer_methods.write = &footer_write;
    footer_methods.writev = &footer_writev;
    footer_methods.sendfile = &footer_sendfile;
    _footer_filter = filter_create_internal("magnus-internal/add-footer", FILTER_CONTENT_GENERATION, &footer_methods, 0);
    if (!_footer_filter)
        return PR_FAILURE;

    return PR_SUCCESS;
}
