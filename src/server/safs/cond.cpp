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

#include "base/util.h"
#include "base/session.h"
#include "base/shexp.h"
#include "frame/log.h"
#include "frame/http_ext.h"
#include "safs/dbtsafs.h"
#include "safs/cond.h"
#include "safs/var.h"

//-----------------------------------------------------------------------------
// _match_dns
//-----------------------------------------------------------------------------

static PRBool _match_dns(Session *sn, const char *pattern)
{
    // Get the client's hostname
    char *host = session_maxdns(sn);
    if (!host) host = "";

    // Check if DNS pattern matches
    return !WILDPAT_CASECMP(host, (char*)pattern);
}

//-----------------------------------------------------------------------------
// _match_urlhost
//-----------------------------------------------------------------------------

static PRBool _match_urlhost(Request *rq, const char *pattern)
{
    // Get the host the client says it's connected to
    const char *host = rq->hostname;
    if (!host) host = "";

    // Make a copy with the port stripped out if necessary
    char *hostcopy = NULL;
    if (util_host_port_suffix(host)) {
        hostcopy = STRDUP(host);
        *util_host_port_suffix(hostcopy) = '\0';
        host = hostcopy;
    }

    // Check if host matches pattern
    PRBool rv = !WILDPAT_CASECMP((char*)host, (char*)pattern);

    FREE(hostcopy);

    return rv;
}

//-----------------------------------------------------------------------------
// _match_random
//-----------------------------------------------------------------------------

static int _match_random(const char *odds)
{
    // We understand odds in the form "1/2" and "50%"
    int x = atoi(odds);
    int y = 0;
    const char *slash = strchr(odds, '/');
    if (slash) {
        y = atoi(slash + 1);
    } else if (strchr(odds, '%')) {
        y = 100;
    }

    // Check for invalid odds
    if (x > y || x < 1 || y < 1 || strchr(odds, '.')) {
        return -1;
    }

    // Return PR_TRUE x out of y times
    unsigned short r;
    util_random(&r, sizeof(r));
    return (r % y) < x;
}

//-----------------------------------------------------------------------------
// _match_pblock_variable
//-----------------------------------------------------------------------------

static int _match_pblock_variable(Session *sn, Request *rq, pblock *pb, const char *pbname, const char *nv, const char *pattern = NULL)
{
    char *nvcopy = NULL;
    const char *name = NULL;
    int rv = -1;

    // Get the pblock*, name, and pattern
    if (pb && nv && pattern) {
        name = nv;
    } else if (pbname && *pbname && nv && pattern) {
        pb = var_get_pblock(sn, rq, pbname);
        name = nv;
    } else {
        nvcopy = STRDUP(nv);
        var_get_pblock_name_value(sn, rq, pbname, nvcopy, &pb, &name, &pattern);
    }

    // If things parsed okay...
    if (pb && name) {
        char *current = pblock_findval(name, pb);
        if (pattern) {
            // There is a pattern (which may be "")
            rv = !WILDPAT_CMP(current ? current : (char *)"", (char*)pattern);
        } else {
            // No pattern, so this is simply an existence check
            rv = (current != NULL);
        }
    }

    FREE(nvcopy);

    return rv;
}

//-----------------------------------------------------------------------------
// _match_boolean
//-----------------------------------------------------------------------------

static int _match_boolean(PRBool actual, const char *pattern)
{
    // Get the desired boolean value from pattern
    int desired = util_getboolean(pattern, -1);
    if (desired == -1) {
        if (!*pattern) {
            // Treat "" as true
            desired = PR_TRUE;
        } else {
            // Invalid pattern
            return -1;
        }
    }

    // Compare the actual value with the desired value
    return (actual == desired);
}

//-----------------------------------------------------------------------------
// cond_match_variable
//-----------------------------------------------------------------------------

int cond_match_variable(pblock *pb, Session *sn, Request *rq)
{
    enum { MATCH_ALL, MATCH_ANY, MATCH_NONE } match = MATCH_ALL;
    int matches = 0;
    int mismatches = 0;
    int ret = REQ_NOACTION;

    // Iterate over the pblock looking for parameters we recognize
    int i;
    for (i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        while (p) {
            const char *n = p->param->name;
            const char *v = p->param->value;
            p = p->next;

            const char *nv = NULL; // Set to do pblock matching
            pblock *targetpb = NULL; // Optional pblock matching parameter
            const char *pbname = NULL; // Optional pblock matching parameter
            const char *pattern = NULL; // Optional pblock matching parameter
            int boolean = -1; // Set to do generic boolean matching
            int matched = -1; // Match/mismatch/error accounting
 
            // Is this a parameter we recognize?
            switch (*n) {
            case 'D':
                if (!strcmp(n, "Directive")) {
                    // Ignore
                    continue;
                }
                break;

            case 'b':
                if (!strcmp(n, "browser")) {
                    // Shorthand for headers="user-agent=$v"
                    targetpb = rq->headers;
                    nv = "user-agent";
                    pattern = v;
                }
                break;

            case 'c':
                if (!strcmp(n, "chunked")) {
                    // Check whether request is chunked
                    matched = _match_boolean(CHUNKED(rq), v);
                } else if (!strcmp(n, "code")) {
                    // Compare numeric portion of status
                    const char *status = pblock_findval("status", rq->srvhdrs);
                    if (!status) status = "500";
                    char *code = STRDUP(status);
                    if (code) {
                        char *t = strchr(code, ' ');
                        if (t) *t = '\0';
                    }
                    matched = !WILDPAT_CMP(code ? code : (char *)"", (char*)v);
                    FREE(code);
                }
                break;

            case 'd':
                if (!strcmp(n, "dns")) {
                    // Check if client's hostname matches dns pattern
                    matched = _match_dns(sn, v);
                }
                break;

            case 'f':
                if (!strcmp(n, "fn")) {
                    // Ignore
                    continue;
                }
                break;

            case 'i':
                if (!strcmp(n, "internal")) {
                    // Check whether request was internally generated
                    matched = _match_boolean(INTERNAL_REQUEST(rq), v);
                } else if (!strcmp(n, "ip")) {
                    // sn->client shorthand notation
                    targetpb = sn->client;
                    nv = n;
                    pattern = v;
                }
                break;

            case 'k':
                if (!strcmp(n, "keep-alive")) {
                    // Check whether keep-alive is enabled
                    matched = _match_boolean(KEEP_ALIVE(rq), v);
                } else if (!strcmp(n, "keysize")) {
                    // sn->client shorthand notation
                    targetpb = sn->client;
                    nv = n;
                    pattern = v;
                }
                break;

            case 'm':
                if (!strcmp(n, "method")) {
                    // rq->reqpb shorthand notation
                    targetpb = rq->reqpb;
                    nv = n;
                    pattern = v;
                } else if (!strcmp(n, "match")) {
                    // Set match mode
                    if (!strcasecmp(v, "all")) {
                        match = MATCH_ALL;
                    } else if (!strcasecmp(v, "any")) {
                        match = MATCH_ANY;
                    } else if (!strcasecmp(v, "none")) {
                        match = MATCH_NONE;
                    } else {
                        log_error(LOG_MISCONFIG, "<Client>", sn, rq, XP_GetAdminStr(DBT_InvalidExpressionNameValue), n, v);
                    }
                    continue;
                }
                break;

            case 'n':
                if (!strcmp(n, "name")) {
                    // rq->vars shorthand notation
                    targetpb = rq->vars;
                    nv = n;
                    pattern = v;
                }
                break;

            case 'o':
                if (!strcmp(n, "odds")) {
                    // odds="x/y"
                    matched = _match_random(v);
                }
                break;

            case 'p':
                if (!strcmp(n, "path")) {
                    // rq->vars shorthand notation
                    targetpb = rq->vars;
                    nv = n;
                    pattern = v;
                } else if (!strcmp(n, "ppath")) {
                    // rq->vars shorthand notation
                    targetpb = rq->vars;
                    nv = n;
                    pattern = v;
                }
                break;

            case 'q':
                if (!strcmp(n, "query")) {
                    // rq->reqpb shorthand notation
                    targetpb = rq->reqpb;
                    nv = n;
                    pattern = v;
                }
                break;

            case 'r':
                if (!strcmp(n, "reason")) {
                    // Compare text portion of status
                    const char *reason = pblock_findval("status", rq->srvhdrs);
                    if (reason) reason = strchr(reason, ' ');
                    if (reason) reason++;
                    matched = !WILDPAT_CMP((char*)(reason ? reason : ""), (char*)v);
                } else if (!strcmp(n, "refcount")) {
                    // Ignore
                    continue;
                } else if (!strcmp(n, "restarted")) {
                    // Check whether request was restarted
                    matched = _match_boolean(RESTARTED_REQUEST(rq), v);
                }
                break;

            case 's':
                if (!strcmp(n, "secret-keysize")) {
                    // sn->client shorthand notation
                    targetpb = sn->client;
                    nv = n;
                    pattern = v;
                } else if (!strcmp(n, "security")) {
                    // Check whether request was encrypted
                    matched = _match_boolean(GetSecurity(sn), v);
                }
                break;

            case 't':
                if (!strcmp(n, "type")) {
                    // Shorthand for srvhdrs="content-type=$v"
                    targetpb = rq->srvhdrs;
                    nv = "content-type";
                    pattern = v;
                }
                break;

            case 'u':
                if (!strcmp(n, "uri")) {
                    // rq->reqpb shorthand notation
                    targetpb = rq->reqpb;
                    nv = n;
                    pattern = v;
                } else if (!strcmp(n, "urlhost")) {
                    // Check if Host: header matches urlhost pattern
                    matched = _match_urlhost(rq, v);
                }
                break;

            case 'v':
                if (!strcmp(n, "variable")) {
                    // pblock name is encoded in v
                    pbname = "";
                    nv = v;
                } else if (!strncmp(n, "variable-", sizeof("variable-")-1)) {
                    // pblock name is encoded in n
                    pbname = n + sizeof("variable-")-1;
                    nv = v;
                }
                break;
            }

            // If n didn't match anything above...
            if (matched == -1 && !nv) {
                // Is n a pblock name?
                PR_ASSERT(!targetpb && !pbname);
                targetpb = var_get_pblock(sn, rq, n);
                if (targetpb) {
                    pbname = n;
                    nv = v;
                }
            }

            // Perform generic pblock matching operations
            if (nv) {
                PR_ASSERT(matched == -1);
                PR_ASSERT(targetpb || pbname);
                matched = _match_pblock_variable(sn, rq, targetpb, pbname, nv, pattern);
            }

            // Perform match/mismatch/error accounting
            switch (matched) {
            case PR_TRUE:
                matches++;
                break;
            case PR_FALSE:
                mismatches++;
                break;
            default:
                log_error(LOG_MISCONFIG, "<Client>", sn, rq, XP_GetAdminStr(DBT_InvalidExpressionNameValue), n, v);
                break;
            }
        }
    }

    // Set success according to our parameters
    PRBool success = PR_FALSE;
    switch (match) {
    case MATCH_ALL: success = (mismatches == 0); break;
    case MATCH_ANY: success = (matches > 0); break;
    case MATCH_NONE: success = (matches == 0); break;
    }

    return success ? REQ_PROCEED : REQ_NOACTION;
}

