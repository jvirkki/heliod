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

#include <sys/types.h>
#ifdef XP_WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include "netsite.h"
#include "xp/xpunit.h"
#include "NsprWrap/NsprBuffer.h"
#include "frame/http.h"
#include "safs/debug.h"

int service_debug(pblock *pb, Session *sn, Request *rq)
{
    // Service SAF for miscellaneous internal debug/test functionality

    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    protocol_start_response(sn, rq);

    PR_fprintf(sn->csd, "<html><head><title>service-debug</title></head><body>\n");

    char *query = pblock_findval("query", rq->reqpb);
    if (query) {
        if (!strcmp(query, "dereference-null")) {
            *(int*)0 = 1;
        } else if (!strcmp(query, "dereference-1")) {
            *(int*)1 = 1;
        } else if (!strcmp(query, "malloc-all")) {
            while (MALLOC(1048576));
            while (MALLOC(16384));
            while (MALLOC(1));
        } else if (!strcmp(query, "perm-malloc-all")) {
            while (PERM_MALLOC(1048576));
            while (PERM_MALLOC(16384));
            while (PERM_MALLOC(1));
        } else if (!strcmp(query, "new-all")) {
            while (new char[1048576]);
            while (new char[16384]);
            while (new char[1]);
        } else if (!strncmp(query, "utime=", 6)) {
            time_t t = strtol(query + 6, NULL, 0);
            char *path = pblock_findval("path", rq->vars);
            struct utimbuf ut;
            ut.actime = t;
            ut.modtime = t;
            if (utime(path, &ut)) {
                PR_fprintf(sn->csd, "Failed to set file access and modification times for %s\n", path);
            } else {
                PR_fprintf(sn->csd, "Set file access and modification times to %d\n", t);
            }
        } else {
            PR_fprintf(sn->csd, "Unrecognized option\n");
        }
    } else {
        PR_fprintf(sn->csd,
                   "Specify option in query string:\n"
                   "<ul>"
                   "<li>dereference-null</li>\n"
                   "<li>dereference-1</li>\n"
                   "<li>malloc-all</li>\n"
                   "<li>perm-malloc-all</li>\n"
                   "<li>new-all</li>\n"
                   "<li>utime=n</li>\n"
                   "</ul>\n");
    }

    PR_fprintf(sn->csd, "</body></html>\n");

    return REQ_PROCEED;
}

int service_unit_tests(pblock *pb, Session *sn, Request *rq)
{
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/plain", rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    protocol_start_response(sn, rq);

    char *query = pblock_findval("query", rq->reqpb);
    PRBool verbose = (query && strstr(query, "verbose"));

    NsprBuffer buffer;

    int failed = XP_RunUnitTests(buffer);

    if (failed || verbose) {
        PR_Write(sn->csd, buffer.data(), buffer.size());
    } else {
        PR_fprintf(sn->csd, "(Diagnostic messages suppressed.  Include verbose in the query string to display diagnostic messages.)\n");
    }

    if (failed) {
        PR_fprintf(sn->csd, "service-unit-tests reports some tests failed\n");
    } else {
        PR_fprintf(sn->csd, "service-unit-tests reports all tests passed\n");
    }

    return REQ_PROCEED;
}
