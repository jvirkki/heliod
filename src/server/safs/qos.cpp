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

// qos.cpp
//
// Author : Julien Pierre
//
// built-in sample NSAPI handlers for Quality of Service
//

#include <signal.h>

#include "frame/log.h"
#include "frame/http.h"
#include "safs/dbtsafs.h"
#include "safs/qos.h"
#include "support/NSString.h"

//-----------------------------------------------------------------------------
// decode : internal function used for parsing of QOS values in pblock
//-----------------------------------------------------------------------------

void decode(const char* val, PRInt32& var, pblock* pb)
{
    char* pbval = pblock_findval(val, pb);
    if (!pbval)
        return;

    var = atoi(pbval);
};

//-----------------------------------------------------------------------------
// qos_error
//
// This function is meant to be an error handler for an HTTP 503 error code,
// which is return by qos_handler when QOS limits are exceeded and enforced
//
// This sample function just prints out a message about which limits were exceeded.
//
//-----------------------------------------------------------------------------

int qos_error(pblock *pb, Session *sn, Request *rq)
{
    PRBool ok = PR_FALSE;

    PR_ASSERT(rq);
    PR_ASSERT(sn);
    PR_ASSERT(pb);

    PRInt32 vs_bw = 0, vs_bwlim = 0, vs_bw_ef = 0,
            vs_conn = 0, vs_connlim = 0, vs_conn_ef = 0,
            vsc_bw = 0, vsc_bwlim = 0, vsc_bw_ef = 0,
            vsc_conn = 0, vsc_connlim = 0, vsc_conn_ef = 0,
            srv_bw = 0, srv_bwlim = 0, srv_bw_ef = 0,
            srv_conn = 0, srv_connlim = 0, srv_conn_ef = 0;

    pblock* apb = rq->vars;

    decode("vs_bandwidth", vs_bw, apb);
    decode("vs_connections", vs_conn, apb);

    decode("vs_bandwidth_limit", vs_bwlim, apb);
    decode("vs_bandwidth_enforced", vs_bw_ef, apb);

    decode("vs_connections_limit", vs_connlim, apb);
    decode("vs_connections_enforced", vs_conn_ef, apb);
    
    decode("vsclass_bandwidth", vsc_bw, apb);
    decode("vsclass_connections", vsc_conn, apb);
    
    decode("vsclass_bandwidth_limit", vsc_bwlim, apb);
    decode("vsclass_bandwidth_enforced", vsc_bw_ef, apb);

    decode("vsclass_connections_limit", vsc_connlim, apb);
    decode("vsclass_connections_enforced", vsc_conn_ef, apb);
    
    decode("server_bandwidth", srv_bw, apb);
    decode("server_connections", srv_conn, apb);
    
    decode("server_bandwidth_limit", srv_bwlim, apb);
    decode("server_bandwidth_enforced", srv_bw_ef, apb);

    decode("server_connections_limit", srv_connlim, apb);
    decode("server_connections_enforced", srv_conn_ef, apb);

    NSString error;

    if ((vs_bwlim) && (vs_bw>vs_bwlim))
    {
        ok = PR_TRUE;
        char abuf[1024];
        // bandwidth limit was exceeded, display it
        error.append("<P>Virtual server bandwidth limit of ");
        sprintf(abuf, "%d", vs_bwlim);
        error.append(abuf);
        error.append(" . Current VS bandwidth : ");
        sprintf(abuf, "%d", vs_bw);
        error.append(abuf);
        error.append(" .<P>");
    };

    if ((vs_connlim) && (vs_conn>vs_connlim))
    {
        ok = PR_TRUE;
        char abuf[1024];
        // connection limit was exceeded, display it
        error.append("<P>Virtual server connection limit of ");
        sprintf(abuf, "%d", vs_connlim);
        error.append(abuf);
        error.append(" . Current VS connections : ");
        sprintf(abuf, "%d", vs_conn);
        error.append(abuf);
        error.append(" .<P>");
    };

    if ((vsc_bwlim) && (vsc_bw>vsc_bwlim))
    {
        ok = PR_TRUE;
        char abuf[1024];
        // bandwidth limit was exceeded, display it
        error.append("<P>Virtual server class bandwidth limit of ");
        sprintf(abuf, "%d", vsc_bwlim);
        error.append(abuf);
        error.append(" . Current VSCLASS bandwidth : ");
        sprintf(abuf, "%d", vsc_bw);
        error.append(abuf);
        error.append(" .<P>");
    };

    if ((vsc_connlim) && (vsc_conn>vsc_connlim))
    {
        ok = PR_TRUE;
        char abuf[1024];
        // connection limit was exceeded, display it
        error.append("<P>Virtual server class connection limit of ");
        sprintf(abuf, "%d", vsc_connlim);
        error.append(abuf);
        error.append(" . Current VScLASS connections : ");
        sprintf(abuf, "%d", vsc_conn);
        error.append(abuf);
        error.append(" .<P>");
    };

    if ((srv_bwlim) && (srv_bw>srv_bwlim))
    {
        ok = PR_TRUE;
        char abuf[1024];
        // bandwidth limit was exceeded, display it
        error.append("<P>Global bandwidth limit of ");
        sprintf(abuf, "%d", srv_bwlim);
        error.append(abuf);
        error.append(" . Current global bandwidth : ");
        sprintf(abuf, "%d", srv_bw);
        error.append(abuf);
        error.append(" .<P>");
    };

    if ((srv_connlim) && (srv_conn>srv_connlim))
    {
        ok = PR_TRUE;
        char abuf[1024];
        // connection limit was exceeded, display it
        error.append("<P>Global connection limit of ");
        sprintf(abuf, "%d", srv_connlim);
        error.append(abuf);
        error.append(" . Current global connections : ");
        sprintf(abuf, "%d", srv_conn);
        error.append(abuf);
        error.append(" .<P>");
    };

    if (ok)
    {
        // this was really a QOS failure, send the error page

        //protocol_status(sn, rq, PROTOCOL_SERVICE_UNAVAILABLE, NULL);

        pblock_nvreplace("content-type", "text/html", rq->srvhdrs);

        protocol_start_response(sn, rq);
        net_write(sn->csd, (char*)error.data(), error.length());
        return REQ_PROCEED;
    }
    else
    {
        // this 503 didn't come from a QOS SAF failure
        return REQ_PROCEED;
    };
};

//-----------------------------------------------------------------------------
// qos_handler
//
// This is an NSAPI AuthTrans function
//
// It examines the QOS values in the request and compare them to the QOS limits.
//
// It does several things :
// 1) It will log errors if the QOS limits are exceeded. 
// 2) It will return REQ_ABORTED with a 503 error code if the QOS limits are exceeded,
//    and the QOS limits are set to be enforced. Otherwise it will return REQ_PROCEED
//
//-----------------------------------------------------------------------------

int qos_handler(pblock *pb, Session *sn, Request *rq)
{
    PRBool ok = PR_TRUE;

    PR_ASSERT(rq);
    PR_ASSERT(sn);
    PR_ASSERT(pb);

    PRInt32 vs_bw = 0, vs_bwlim = 0, vs_bw_ef = 0,
            vs_conn = 0, vs_connlim = 0, vs_conn_ef = 0,
            vsc_bw = 0, vsc_bwlim = 0, vsc_bw_ef = 0,
            vsc_conn = 0, vsc_connlim = 0, vsc_conn_ef = 0,
            srv_bw = 0, srv_bwlim = 0, srv_bw_ef = 0,
            srv_conn = 0, srv_connlim = 0, srv_conn_ef = 0;

    pblock* apb = rq->vars;

    decode("vs_bandwidth", vs_bw, apb);
    decode("vs_connections", vs_conn, apb);

    decode("vs_bandwidth_limit", vs_bwlim, apb);
    decode("vs_bandwidth_enforced", vs_bw_ef, apb);

    decode("vs_connections_limit", vs_connlim, apb);
    decode("vs_connections_enforced", vs_conn_ef, apb);
    
    decode("vsclass_bandwidth", vsc_bw, apb);
    decode("vsclass_connections", vsc_conn, apb);
    
    decode("vsclass_bandwidth_limit", vsc_bwlim, apb);
    decode("vsclass_bandwidth_enforced", vsc_bw_ef, apb);

    decode("vsclass_connections_limit", vsc_connlim, apb);
    decode("vsclass_connections_enforced", vsc_conn_ef, apb);
    
    decode("server_bandwidth", srv_bw, apb);
    decode("server_connections", srv_conn, apb);
    
    decode("server_bandwidth_limit", srv_bwlim, apb);
    decode("server_bandwidth_enforced", srv_bw_ef, apb);

    decode("server_connections_limit", srv_connlim, apb);
    decode("server_connections_enforced", srv_conn_ef, apb);

    if ((vs_bwlim) && (vs_bw>vs_bwlim))
    {
        // bandwidth limit was exceeded, log it
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_QOS_VSBandwidthX), vs_bwlim, vs_bw);

        if (vs_bw_ef)
        {
            // and enforce it
            ok = PR_FALSE;
        };
    };

    if ((vs_connlim) && (vs_conn>vs_connlim))
    {
        // connection limit was exceeded, log it
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_QOS_VSConnX), vs_connlim, vs_conn);

        if (vs_conn_ef)
        {
            // and enforce it
            ok = PR_FALSE;
        };
    };

    if ((vsc_bwlim) && (vsc_bw>vsc_bwlim))
    {
        // bandwidth limit was exceeded, log it
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_QOS_VSClassBandwidthX), vsc_bwlim, vsc_bw);

        if (vsc_bw_ef)
        {
            // and enforce it
            ok = PR_FALSE;
        };
    };

    if ((vsc_connlim) && (vsc_conn>vsc_connlim))
    {
        // connection limit was exceeded, log it
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_QOS_VSClassConnX), vsc_connlim, vsc_conn);

        if (vsc_conn_ef)
        {
            // and enforce it
            ok = PR_FALSE;
        };
    };


    if ((srv_bwlim) && (srv_bw>srv_bwlim))
    {
        // bandwidth limit was exceeded, log it
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_QOS_GlobalBandwidthX), srv_bwlim, srv_bw);

        if (srv_bw_ef)
        {
            // and enforce it
            ok = PR_FALSE;
        };
    };

    if ((srv_connlim) && (srv_conn>srv_connlim))
    {
        // connection limit was exceeded, log it
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_QOS_GlobalConnX), srv_connlim, srv_conn);

        if (srv_conn_ef)
        {
            // and enforce it
            ok = PR_FALSE;
        };
    };

    if (ok)
    {
        return REQ_PROCEED;
    }
    else
    {
        // one of the limits was exceeded
        // therefore, we set HTTP error 503 "server too busy"
        protocol_status(sn, rq, PROTOCOL_SERVICE_UNAVAILABLE, NULL);
        return REQ_ABORTED;
    };
};

