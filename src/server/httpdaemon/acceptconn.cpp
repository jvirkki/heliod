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

#include <nspr.h>
#include "base/ereport.h"            // ereport()
#include "base/systhr.h"             // systhread_sleep()
#include "secerr.h"                  // IS_SEC_ERROR()
#include "sslerr.h"                  // IS_SSL_ERROR()

#include "httpdaemon/ListenSocket.h" // ListenSocket class
#include "httpdaemon/connqueue.h"    // ConnectionQueue class
#include "httpdaemon/acceptconn.h"   // Acceptor class
#include "httpdaemon/dbthttpdaemon.h"


Acceptor::Acceptor(ConnectionQueue* connQ, ListenSocket* ls) :
    Thread("Acceptor"), _connQ(connQ), _listenSocket(ls)
{
    /* Create one thread per acceptor constructor */

    if (_connQ==NULL)
    {
        ereport(LOG_FAILURE, "No Connection Queue created");
        return;
    }

    if (_listenSocket==NULL)
    {
        ereport(LOG_FAILURE, "Invalid listen socket");
        return;
    }
}

Acceptor::~Acceptor()
{
    this->setTerminatingFlag();
    /*  socket should already be closed, PR_Join thread to end */
    this->join();
}

/*
   Loops doing accept on listen socket. If connection is
   made, creates Connection object, Adds it to the queue,
   and continues looping.

   Returns TRUE when terminated normally
   Returns FALSE otherwise
*/

void
Acceptor::run(void)
{
    while (this->wasTerminated() == PR_FALSE)
    {
        // Loop accepting connections

        PRNetAddr remoteAddress;
        PRFileDesc *socketAccept;

        // XXX the following code is duplicated in DaemonSession::GetConnection

        for (;;)
        {
            socketAccept = _listenSocket->accept(remoteAddress);

            if (socketAccept != NULL)
                break;        /* from for loop */

            if (this->wasTerminated())
                return;

            PRInt32 err = PR_GetError();

            // Load balancer "pings" will trigger this
            if (err == PR_CONNECT_ABORTED_ERROR)
                continue;

            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_DaemonSessionErrorAcceptConn),
                    system_errmsg());

            //
            // If the process is out of file descriptors (ulimit reached or
            // denial-of-service attack, then sleep for a second and try to
            // recover
            //
            if (err == PR_PROC_DESC_TABLE_FULL_ERROR)
                systhread_sleep(1000);

        }    /* end for */

        Connection *conn = _connQ->GetUnused();
        if (conn == NULL)
        {
            PR_Close(socketAccept);
            continue;
        }

        if (conn->create(socketAccept, &remoteAddress, _listenSocket) != PR_SUCCESS)
        {
            PR_Close(socketAccept);
            _connQ->AddUnused(conn);
            continue;
        }

        if (_connQ->AddReady(conn) != PR_SUCCESS) {
            conn->abort();
            conn->destroy();
        }
    } /* end while (!_terminating) */

    return;
}
