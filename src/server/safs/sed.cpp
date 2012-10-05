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
 * sed.cpp: Filters for sed-like functionality
 * 
 * Chris Elving
 */

#include "base/util.h"
#include "base/pool.h"
#include "frame/log.h"
#include "frame/filter.h"
#include "frame/httpfilter.h"
#include "libsed/libsed.h"
#include "NsprWrap/NsprError.h"
#include "NsprWrap/NsprBuffer.h"
#include "safs/dbtsafs.h"
#include "safs/sed.h"


#define SED_REQUEST "sed-request"
#define SED_RESPONSE "sed-response"
#define IDENTITY "identity"

typedef struct SedData {
    const char *name;
    Session *sn;
    Request *rq;
    sed_commands_t commands; // XXX should be done at config time
    sed_eval_t eval; // XXX should be cached
    NsprBuffer *fifo;
} SedData;

static sed_err_fn_t log_sed_errf;
static FilterInsertFunc sed_request_insert;
static FilterInsertFunc sed_response_insert;
static FilterRemoveFunc sed_request_remove;
static FilterRemoveFunc sed_response_remove;
static FilterReadFunc sed_request_read;
static FilterWriteFunc sed_response_write;

static const Filter *_sed_request_filter;
static const Filter *_sed_response_filter;


/* ----------------------------- log_sed_errf ----------------------------- */
static void log_sed_errf(void *data, const char *fmt, va_list args)
{
    SedData *seddata = (SedData *)data;
    log_error_v(LOG_FAILURE, seddata->name, seddata->sn, seddata->rq, fmt, args);
}

/* ---------------------------- init_sed_data ----------------------------- */

static SedData *init_sed_data(pblock *pb, Session *sn, Request *rq, pool_handle_t *pool, const char *name)
{
    PRStatus rv;

    SedData *seddata = (SedData *)pool_malloc(pool, sizeof(SedData));
    if (!seddata)
        return NULL;

    seddata->name = name;
    seddata->sn = sn;
    seddata->rq = rq;
    seddata->fifo = NULL;

    rv = sed_init_commands(&seddata->commands, log_sed_errf, seddata, pool);
    if (rv != PR_SUCCESS) {
        pool_free(pool, seddata);
        return NULL;
    }

    // Compile sed commands (XXX should be done at config time)
    int numcommands = 0;
    int i;
    for (i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        while (p) {
            if (!strcmp(p->param->name, "sed")) {
                rv = sed_compile_string(&seddata->commands, p->param->value);
                if (rv != PR_SUCCESS) {
                    sed_destroy_commands(&seddata->commands);
                    pool_free(pool, seddata);
                    return NULL;
                }
                numcommands++;
            }
            p = p->next;
        }
    }

    if (numcommands == 0) {
        log_error(LOG_MISCONFIG, name, sn, rq, XP_GetAdminStr(DBT_NeedSed));
        sed_destroy_commands(&seddata->commands);
        pool_free(pool, seddata);
        return NULL;
    }
    else {
        if (sed_finalize_commands(&seddata->commands) != PR_SUCCESS) {
            sed_destroy_commands(&seddata->commands);
            pool_free(pool, seddata);
            return NULL;
        }
    }

    rv = sed_init_eval(&seddata->eval, &seddata->commands, log_sed_errf, seddata, 
                       pool);
    if (rv != PR_SUCCESS) {
        sed_destroy_commands(&seddata->commands);
        pool_free(pool, seddata);
        return NULL;
    }

    return seddata;
}


/* --------------------------- destroy_sed_data --------------------------- */

static void destroy_sed_data(pool_handle_t *pool, SedData *seddata)
{
    if (seddata->fifo)
        delete seddata->fifo;

    // XXX the evaluation context should be cached)
    sed_destroy_eval(&seddata->eval);

    // XXX the sed commands should be destroyed at reconfig time
    sed_destroy_commands(&seddata->commands);

    pool_free(pool, seddata);
}


/* -------------------------- sed_request_insert -------------------------- */

static int sed_request_insert(FilterLayer *layer, pblock *pb)
{
    SedData *seddata = (SedData *)layer->context->data;

    if (!seddata) {
        seddata = init_sed_data(pb, layer->context->sn, layer->context->rq, layer->context->pool, SED_REQUEST);
        if (!seddata)
            return REQ_ABORTED;
        layer->context->data = seddata;
    }

    // We use an NsprBuffer to capture sed output
    if (!seddata->fifo) {
        seddata->fifo = new NsprBuffer();
        if (!seddata->fifo) {
            destroy_sed_data(layer->context->pool, seddata);
            return REQ_ABORTED;
        }
    }

    // We modify the request entity body length
    if (!pblock_findkey(pb_key_transfer_encoding, layer->context->rq->headers)) {
        pb_param *pp = pblock_removekey(pb_key_content_length, layer->context->rq->headers);
        if (pp) {
            param_free(pp);
            pblock_kvinsert(pb_key_transfer_encoding, IDENTITY, sizeof(IDENTITY) - 1, layer->context->rq->headers);
        }
    }

    return REQ_PROCEED;
}


/* ------------------------- sed_response_insert -------------------------- */

static int sed_response_insert(FilterLayer *layer, pblock *pb)
{
    SedData *seddata = (SedData *)layer->context->data;

    if (!seddata) {
        seddata = init_sed_data(pb, layer->context->sn, layer->context->rq, layer->context->pool, SED_RESPONSE);
        if (!seddata)
            return REQ_ABORTED;
        layer->context->data = seddata;
    }

    // We modify the response entity body length
    param_free(pblock_removekey(pb_key_content_length, layer->context->rq->srvhdrs));

    // sed splits the input into separate lines, so enable buffering to
    // reduce system call overhead
    httpfilter_buffer_output(layer->context->sn, layer->context->rq, PR_TRUE);

    return REQ_PROCEED;
}


/* -------------------------- sed_request_remove -------------------------- */

static void sed_request_remove(FilterLayer *layer)
{
    SedData *seddata = (SedData *)layer->context->data;

    destroy_sed_data(layer->context->pool, seddata);
}


/* ------------------------- sed_response_remove -------------------------- */

static void sed_response_remove(FilterLayer *layer)
{
    SedData *seddata = (SedData *)layer->context->data;

    // Flush any pending data
    sed_finalize_eval(&seddata->eval, layer->lower);

    destroy_sed_data(layer->context->pool, seddata);
}


/* --------------------------- sed_request_read --------------------------- */

static int sed_request_read(FilterLayer *layer, void *buf, int amount, int rdtimeout)
{
    SedData *seddata = (SedData *)layer->context->data;
    PRStatus rv;

    for (;;) {
        int n;

        n = seddata->fifo->read(buf, amount);
        if (n > 0)
            return n;

        n = net_read(layer->lower, buf, amount, rdtimeout);
        if (n == 0)
            break;
        if (n < 0)
            return n;

        rv = sed_eval_buffer(&seddata->eval, (const char *)buf, n, (PRFileDesc *)*seddata->fifo);
        if (rv != PR_SUCCESS) {
            NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_FnError), seddata->name);
            return -1;
        }
    }

    rv = sed_finalize_eval(&seddata->eval, (PRFileDesc *)*seddata->fifo);
    if (rv != PR_SUCCESS) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_FnError), seddata->name);
        return -1;
    }

    return seddata->fifo->read(buf, amount);
}


/* -------------------------- sed_response_write -------------------------- */

static int sed_response_write(FilterLayer *layer, const void *buf, int amount)
{
    SedData *seddata = (SedData *)layer->context->data;
    PRStatus rv;

    rv = sed_eval_buffer(&seddata->eval, (const char *)buf, amount, layer->lower);
    if (rv != PR_SUCCESS) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_FnError), seddata->name);
        return -1;
    }

    return amount;
}


/* ------------------------------- sed_init ------------------------------- */

PRStatus sed_init(void)
{
    FilterMethods sed_request_methods = FILTER_METHODS_INITIALIZER;
    FilterMethods sed_response_methods = FILTER_METHODS_INITIALIZER;

    // Create the sed-request filter
    sed_request_methods.insert = &sed_request_insert;
    sed_request_methods.remove = &sed_request_remove;
    sed_request_methods.read = &sed_request_read;
    _sed_request_filter = filter_create_internal(SED_REQUEST, FILTER_CONTENT_TRANSLATION, &sed_request_methods, 0);
    if (!_sed_request_filter)
        return PR_FAILURE;

    // Create the sed-response filter
    sed_response_methods.insert = &sed_response_insert;
    sed_response_methods.remove = &sed_response_remove;
    sed_response_methods.write = &sed_response_write;
    _sed_response_filter = filter_create_internal(SED_RESPONSE, FILTER_CONTENT_TRANSLATION, &sed_response_methods, 0);
    if (!_sed_response_filter)
        return PR_FAILURE;

    return PR_SUCCESS;
}
