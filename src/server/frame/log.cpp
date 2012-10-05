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
 * log.c:  Records transactions, reports errors to administrators, etc.
 * 
 * Rob McCool
 */


#include "frame/log.h"

#include "netsite.h"
#include "base/util.h"       /* util_sprintf */
#include "frame/conf.h"
#include "frame/dbtframe.h"
#include <stdarg.h>
#include <stdio.h>           /* vsprintf */


NSAPI_PUBLIC int log_error_v(int degree, const char *func,
                             Session *sn, Request *rq, const char *fmt, va_list args)
{
    char errstr[MAX_ERROR_LEN], *rhost;
    int pos;

    if (!ereport_can_log(degree))
        return IO_OKAY;

    pos = 0;
    if(sn && sn->iaddr.s_addr != 0 /* internal Sessions have iaddr == 0 */) {
        if(!(rhost = session_dns(sn))) 
            rhost = pblock_findkeyval(pb_key_ip, sn->client);
        pos += util_snprintf(&errstr[pos], sizeof(errstr) - pos,
                             XP_GetAdminStr(DBT_logForHostX),
                             ERROR_CUTOFF, rhost);
    }
    if(rq) {
        const char *fmt = XP_GetAdminStr(DBT_logTryingToMethodXUriY);
        Request *curr_rq = rq;
        char *prev_method = NULL;
        char *prev_uri = NULL;
        for(;;) {
            char *method = pblock_findkeyval(pb_key_method, curr_rq->reqpb);
            char *uri = pblock_findkeyval(pb_key_uri, curr_rq->reqpb);

            if(method && uri &&
               (!prev_method || strcmp(method, prev_method) ||
                !prev_uri || strcmp(uri, prev_uri)))
            {
                if(pos > 0 && pos < sizeof(errstr) - 1)
                    errstr[pos++] = ' ';
                pos += util_snprintf(&errstr[pos], sizeof(errstr) - pos, fmt,
                                     ERROR_CUTOFF, method, ERROR_CUTOFF, uri);
                fmt = XP_GetAdminStr(DBT_logWhileTryingToMethodXUriY);
            }

            prev_method = method;
            prev_uri = uri;

            if(curr_rq->orig_rq == curr_rq)
                break;

            curr_rq = curr_rq->orig_rq;
        }
    }
    if(sn || rq)
        pos += util_snprintf(&errstr[pos], sizeof(errstr) - pos,
                             XP_GetAdminStr(DBT_logCommaSpace));
    if(func)
        pos += util_snprintf(&errstr[pos], sizeof(errstr) - pos, 
                             XP_GetAdminStr(DBT_logFunctionXReports), func);

    pos += util_vsnprintf(&errstr[pos], sizeof(errstr) - pos, fmt, args);

    errstr[pos] = '\0';

    return ereport_request(rq, degree, "%s", errstr);
}

NSAPI_PUBLIC int log_error(int degree, const char *func,
                           Session *sn, Request *rq, const char *fmt, ...)
{
    va_list args;
    int rv;
    va_start(args, fmt);
    rv = log_error_v(degree, func, sn, rq, fmt, args);
    va_end(args);
    return rv;
}

NSAPI_PUBLIC int log_ereport_v(int degree, const char *fmt, va_list args)
{
    int rv;

    rv = ereport_v(degree, fmt, args);
    return rv;      
}

NSAPI_PUBLIC int log_ereport(int degree, const char *fmt, ...)
{
    int rv;
    va_list args;

    va_start(args, fmt);
    rv = ereport_v(degree, fmt, args);
    va_end(args);

    return rv;
}
