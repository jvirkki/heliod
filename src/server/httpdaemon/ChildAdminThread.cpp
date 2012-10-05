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
 * ChildAdminThread class implementation.
 *
 * @author  $Author: bk139067 $
 * @version $Revision: 1.1.2.7.4.1.2.4.26.7 $
 * @since   iWS5.0
 */

#include <nspr.h>
#include <unistd.h>                          // getppid()
#include <limits.h>                          // PATH_MAX
#include <sys/socket.h>                      // socket()
#include <sys/un.h>                          // sockaddr_un
#include "private/pprio.h"                   // PR_ImportFile

#include "base/ereport.h"                    // ereport()
#include "base/util.h"                       // util_sprintf
#include "base/systhr.h"                     // systhread_sleep
#include "frame/conf.h"                      // conf_get_true_globals()
#include "httpdaemon/scheduler.h"            // scheduler_rotate()
#include "httpdaemon/WebServer.h"            // WebServer::Reconfigure
#include "httpdaemon/logmanager.h"           // LogManager class
#include "httpdaemon/ParentAdmin.h"          // ParentAdmin class
#include "httpdaemon/ChildAdminThread.h"     // ChildAdminThread class
#include "httpdaemon/dbthttpdaemon.h"        // resource error messages
#include "UnixSignals.h"
#include "httpdaemon/statsmessage.h"        // StatsServerMessage class
#include "httpdaemon/statsmanager.h"


ChildAdminThread::ChildAdminThread(void) :
    Thread("ChildAdminThread"),
    msgChannel_(NULL)
{
    wakeupFD_ = PR_NewPollableEvent();
}

ChildAdminThread::~ChildAdminThread(void)
{
    this->setTerminatingFlag();

    if (wakeupFD_ != NULL)
    {
        PR_SetPollableEvent(wakeupFD_);
    }

    int nWait = 0;
    while (this->isRunning() && (nWait < 300))
    {
        systhread_sleep(100);
        nWait++;
    }

    if (msgChannel_ != NULL)
    {
        delete msgChannel_;
        msgChannel_ = NULL;
    }

    if (wakeupFD_ != NULL)
    {
        PR_DestroyPollableEvent(wakeupFD_);
    }
}

PRStatus
ChildAdminThread::init(void)
{
    PRStatus status = PR_FAILURE;

    PR_ASSERT(!msgChannel_);
    if (msgChannel_)
    {
        delete msgChannel_;
        msgChannel_ = NULL;
    }

    msgChannel_ = ParentAdmin::connect();
    if (msgChannel_ != NULL)
        status = sendEndInit();
    if ( status == PR_SUCCESS )
        StatsManager::setChildInitialized();

    return status;
}


PRStatus
ChildAdminThread::sendEndInit(void)
{
    PRStatus status = PR_SUCCESS;
    if (msgChannel_)
    {
        char msgstr[20];
        util_sprintf(msgstr,"%d", getpid());
        if (msgChannel_->SendToServer(wdmsgEndInit, msgstr) == 1)
        {
            char* msg = msgChannel_->RecvFromServer();
            WDMessages msgType = msgChannel_->getLastMsgType();
            if (msgType != wdmsgEndInitreply)
                status = PR_FAILURE;
            msgChannel_->SendToServer(wdmsgEndInitreplyAck, msgstr);
        }
        else
        {
            status = PR_FAILURE;
        }
    }
    if (status == PR_FAILURE)
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ChildAdminThread_sendEndInit));
    return status;
}


static int
ereport_configuration_callback(const VirtualServer* vs, int degree, const char *formatted, int formattedlen, const char *raw, int rawlen, void *data)
{
    // We only write to the admin client if the admin channel is active
    PRBool fLogToAdminChannel = *(PRBool*)data;
    if (!fLogToAdminChannel)
        return 0;

    conf_global_vars_s* globals = conf_get_true_globals();
    if (globals->started_by_watchdog)
    {
        static int depth = 0;
        depth++;
        if (depth == 1)
            WatchdogClient::sendReconfigureStatus(raw);
        depth--;
    }

    // Log this message to the log file as well
    return 0;
}

void
ChildAdminThread::run(void)
{
    PR_ASSERT(msgChannel_ != NULL && wakeupFD_ != NULL);

    // Capture all ereport() calls on this thread
    PRBool fLogToAdminChannel = PR_FALSE;
    ereport_register_thread_cb(&ereport_configuration_callback, &fLogToAdminChannel);

    PRPollDesc pollArray[2];
    PRFileDesc* msgFD = PR_ImportFile(msgChannel_->getFD());

    pollArray[0].fd = msgFD;
    pollArray[0].in_flags = (PR_POLL_READ|PR_POLL_EXCEPT);
    pollArray[0].out_flags = 0;
    pollArray[1].fd = wakeupFD_;
    pollArray[1].in_flags = (PR_POLL_READ|PR_POLL_EXCEPT);
    pollArray[1].out_flags = 0;

    while (this->wasTerminated() == PR_FALSE)
    {
        PRInt32 nReady = PR_Poll(pollArray, 2, PR_INTERVAL_NO_TIMEOUT);
        if (nReady > 0)
        {
            if (pollArray[0].out_flags & PR_POLL_READ)
            {
                char* msg = msgChannel_->RecvFromServer();
                WDMessages msgType = msgChannel_->getLastMsgType();
                if (msgType == wdmsgReconfigure)
                {
                    msgChannel_->SendToServer(wdmsgReconfigurereply, NULL);

                    fLogToAdminChannel = PR_TRUE;
                    WebServer::Reconfigure();
                    fLogToAdminChannel = PR_FALSE;

                    conf_global_vars_s* globals = conf_get_true_globals();
                    if (globals->started_by_watchdog)
                    {
                        WatchdogClient::sendReconfigureStatusDone();
                    }
                    StatsManager::processReconfigure();
                }
                else if (msgType == wdmsgRotate)
                {
                    scheduler_rotate(NULL);
                    msgChannel_->SendToServer(wdmsgRotatereply, NULL);
                }
                if (msgType == wdmsgPeerReconfigure)
                {
                    WebServer::Reconfigure();
                    msgChannel_->SendToServer(wdmsgPeerReconfigurereply, NULL);
                    StatsManager::processReconfigure();
                }
                if (msgType == wdmsgPeerReopenLogs)
                {
                    WebServer::ReopenLogs();
                    msgChannel_->SendToServer(wdmsgPeerReopenLogsreply, NULL);
                }
                else if (msgType == wdmsgTerminate || msgType == wdmsgEmptyRead)
                {
                    // Behave as though we received a SIGTERM
                    UnixSignals::Fake(SIGTERM);
                    break;
                }
                else 
                {
                    if ( msgChannel_->isStatsMessage() )
                        processStatsMessage();
                }
            }
            else
                break;
        }
        else if (nReady < 0)
            break;
    }
    msgChannel_->invalidate();
    PR_Close(msgFD);
}

void
ChildAdminThread::reconfigure(void)
{
    wdServerMessage *msgChannel = ParentAdmin::connect();
    if (msgChannel != NULL) {
        if (msgChannel->SendToServer(wdmsgPeerReconfigure, NULL) == 1) {
            char* msg = msgChannel->RecvFromServer();
            WDMessages msgType = msgChannel->getLastMsgType();
            PR_ASSERT(msgType == wdmsgPeerReconfigurereply);
        }
        delete msgChannel;
    }
}

void
ChildAdminThread::reopenLogs(void)
{
    wdServerMessage *msgChannel = ParentAdmin::connect();
    if (msgChannel != NULL) {
        if (msgChannel->SendToServer(wdmsgPeerReopenLogs, NULL) == 1) {
            char* msg = msgChannel->RecvFromServer();
            WDMessages msgType = msgChannel->getLastMsgType();
            PR_ASSERT(msgType == wdmsgPeerReopenLogsreply);
        }
        delete msgChannel;
    }
}

void
ChildAdminThread::restart(void)
{
    wdServerMessage *msgChannel = ParentAdmin::connect();
    if (msgChannel != NULL) {
        if (msgChannel->SendToServer(wdmsgTerminate, NULL) == 1) {
            char* msg = msgChannel->RecvFromServer();
            WDMessages msgType = msgChannel->getLastMsgType();
            PR_ASSERT(msgType == wdmsgTerminatereply || msgType == wdmsgEmptyRead);
        }
        delete msgChannel;
    }
}

void
ChildAdminThread::processStatsMessage()
{
    if ( StatsManager::isEnabled() != PR_TRUE )
        return;
    // now msgChannel is truely a StatsServerMessage pointer.
    // dynamic_cast preferable ??
    // This must be called only if message is a stats message.
    StatsManager::processStatsMessage( static_cast<StatsServerMessage*>
                                       (msgChannel_));
}

