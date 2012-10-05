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
 * WatchdogClient class implementation.
 *
 * @author  $Author: bk139067 $
 * @version $Revision: 1.1.2.21.8.1.46.5 $ $Date: 2006/02/26 19:22:50 $
 * @since   iWS5.0
 */

#ifdef DEBUG
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
#include <iomanip>
using namespace std;
#else
#include <iostream.h>
#include <iomanip.h>                         // setw()
#endif
#endif
#include <stdlib.h>                          // getenv()
#include <string.h>                          // memset(), strcpy()
#include <unistd.h>                          // getppid()
#include <limits.h>                          // PATH_MAX
#include <sys/socket.h>                      // socket()
#include <sys/un.h>                          // sockaddr_un

#include "base/ereport.h"                    // ereport()
#include "base/util.h"                       // util_sprintf
#include "base/unix_utils.h"                 // setFDNonInheritable
#include "frame/conf.h"                      // conf_getglobals()
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/WatchdogClient.h"       // WatchdogClient class

PRBool WatchdogClient::bIsWDRunning_ = PR_FALSE;
wdServerMessage* WatchdogClient::wdMsg_ = NULL;

PRInt32 WatchdogClient::wdPID_ = -1;

PRStatus
WatchdogClient::init(void)
{
    PRStatus status = PR_SUCCESS;

    if (getenv("WD_STARTED") != NULL) {
        WatchdogClient::bIsWDRunning_ = PR_TRUE;
        WatchdogClient::wdPID_ = getppid();
    }

    if (WatchdogClient::isWDRunning())
    {
        // Save the existing connection
        wdServerMessage* oldConnection = WatchdogClient::wdMsg_;

        // Open a new connection
        status = WatchdogClient::connectToWD(WatchdogClient::wdPID_);

        // Close the old one
        if (oldConnection)
        {
            ereport(LOG_WARN,
                    XP_GetAdminStr(DBT_WatchDog_Reopen));
            WatchdogClient::closeWDConnection(oldConnection);
        }
    }
    return status;
}


PRStatus
WatchdogClient::close(void)
{
    PRStatus status = WatchdogClient::closeWDConnection(WatchdogClient::wdMsg_);
    WatchdogClient::wdMsg_ = NULL;
    return status;
}


PRStatus
WatchdogClient::reconnect(void)
{
    // Save the existing connection
    wdServerMessage* oldConnection = WatchdogClient::wdMsg_;

    // Create a new connection
    // we might be in a child worker process, so reuse the PID
    PRStatus status = WatchdogClient::connectToWD(WatchdogClient::wdPID_);

    // Close the old connection
    if (WatchdogClient::closeWDConnection(oldConnection) != PR_SUCCESS)
        return PR_FAILURE;

    return status;
}


PRStatus
WatchdogClient::closeWDConnection(wdServerMessage* wdConnection)
{
    if (wdConnection)
        delete wdConnection;
    return PR_SUCCESS;
}


PRStatus
WatchdogClient::connectToWD(const PRInt32 pid)
{
    PRStatus status = PR_SUCCESS;
    struct sockaddr_un address;

    // construct the name of the Unix domain socket by appending the 
    // process ID of the watchdog (i.e. this process's parent)
    char udsName[PATH_MAX];
    sprintf(udsName, "%s/"WDSOCKETNAME, system_get_temp_dir());
    if (strlen(udsName) >= sizeof(address.sun_path)) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_WatchDog_connect),
                udsName, "Temporary directory path too long");
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd >= 0)
    {
        memset((char *)&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, udsName);

        if (connect(fd, (struct sockaddr*)&address, sizeof(address)) >= 0)
        {
            // Prevent the Unix domain socket descriptor from being inherited by
            // forks such as CGIs
            if (setFDNonInheritable(fd) == 0)
            {
                // the wdServerMessage class closes the fd in its destructor
                WatchdogClient::wdMsg_ = new wdServerMessage(fd);
                if (WatchdogClient::wdMsg_ == NULL)
                {
                    ::close(fd);
                    status = PR_FAILURE;
                    ereport(LOG_FAILURE,
                            XP_GetAdminStr(DBT_WatchDog_malloc));
                }
            }
        }
        else
        {
            status = PR_FAILURE;
            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_WatchDog_connect),
                    udsName, system_errmsg());
        }
    }
    else
    {
        status = PR_FAILURE;
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_socket));
    }

    return status;
}

PRInt32
WatchdogClient::getLS(const char* lsName, const char* ip, const PRUint16 port,
                      const PRUint16 family, const PRUint32 qsize,
                      const PRUint32 sendbuffersize,
                      const PRUint32 recvbuffersize)
{
    PRInt32 fd = -EIO;
    if (WatchdogClient::isWDRunning())
    {
        if (WatchdogClient::wdMsg_)
        {
            char params[1024];
            if (ip && strcmp(ip, "0.0.0.0"))
            {
                sprintf(params, "%s,%s,%d,%d,%d,%d,%d", lsName, ip, port,
                        family, qsize, sendbuffersize, recvbuffersize);
            }
            else
                sprintf(params, "%s,,%d,%d,%d,%d,%d", lsName, port, family,
                        qsize, sendbuffersize, recvbuffersize);

            if (WatchdogClient::wdMsg_->SendToWD(wdmsgGetLS, params) == 1)
            {
                fd = (PRInt32)(size_t)(WatchdogClient::wdMsg_->RecvFromWD());
                WDMessages msgType = WatchdogClient::wdMsg_->getLastMsgType();
                if (msgType != wdmsgGetLSreply)
                {
                    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorReceiving));
                    fd = -EIO;
                }
            }
            else
            {
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorSending));
            }
        }
        else
        {
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_WatchDog_initfail));
        }
    }
    return fd;
}

PRStatus
WatchdogClient::closeLS(const char* lsName, const char* ip, const PRUint16 port,
                        const PRUint16 family, const PRUint32 qsize,
                        const PRUint32 sendbuffersize,
                        const PRUint32 recvbuffersize)
{
    PRStatus status = PR_SUCCESS;
    if (WatchdogClient::isWDRunning())
    {
        if (WatchdogClient::wdMsg_)
        {
            char params[1024];
            if (ip && strcmp(ip, "0.0.0.0"))
            {
                sprintf(params, "%s,%s,%d,%d,%d,%d,%d", lsName, ip, port, 
                        family, qsize, sendbuffersize, recvbuffersize);
            }
            else
                sprintf(params, "%s,,%d,%d,%d,%d,%d", lsName, port, family, 
                        qsize, sendbuffersize, recvbuffersize);

            if (WatchdogClient::wdMsg_->SendToWD(wdmsgCloseLS, params) == 1)
            {
                WatchdogClient::wdMsg_->RecvFromWD();
                WDMessages msgType = WatchdogClient::wdMsg_->getLastMsgType();
                if (msgType != wdmsgCloseLSreply)
                {
                    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorReceiving));
                    status = PR_FAILURE;
                }
            }
            else
            {
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorSending));
                status = PR_FAILURE;
            }
        }
        else
        {
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_WatchDog_initfail));
            status = PR_FAILURE;
        }
    }
    return status;
}

PRStatus
WatchdogClient::sendEndInit(PRInt32 numprocs)
{
    PRStatus status = PR_SUCCESS;
    if (WatchdogClient::isWDRunning())
    {
        if (WatchdogClient::wdMsg_)
        {
            char msgstr[42];
            sprintf(msgstr,"%d",numprocs);
            if (WatchdogClient::wdMsg_->SendToWD(wdmsgEndInit, msgstr) == 1)
            {
                char* msg = WatchdogClient::wdMsg_->RecvFromWD();
                WDMessages msgType = WatchdogClient::wdMsg_->getLastMsgType();
                if (msgType != wdmsgEndInitreply)
                {
                    status = PR_FAILURE;
                    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorReceiving));
                }
            }
            else
            {
                status = PR_FAILURE;
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorSending));
            }
        }
        else
        {
            status = PR_FAILURE;
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_WatchDog_initfail));
        }
    }
    return status;
}

PRStatus
WatchdogClient::getPassword(const char *prompt, const PRInt32 serial,
                            char **password)
{
    PRStatus status = PR_FAILURE;
    char *msg;
    WDMessages msgType;
    char buf[1024]; // ugh

    *password = NULL;
    if (!WatchdogClient::isWDRunning()) {
        status = PR_SUCCESS;
        goto cleanup;
    }

    if (!WatchdogClient::wdMsg_) {
        ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_WatchDog_initfail));
        goto cleanup;
    }

    util_sprintf(buf, "%d,%s", serial, prompt ? prompt : "Password: ");
    if (WatchdogClient::wdMsg_->SendToWD(wdmsgGetPWD, buf) < 0) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorSending));
        goto cleanup;
    }

    msg = WatchdogClient::wdMsg_->RecvFromWD();
    msgType = WatchdogClient::wdMsg_->getLastMsgType();
    if (msgType != wdmsgGetPWDreply || !msg) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorReceiving));
        goto cleanup;
    }
    *password = STRDUP(msg);

    status = PR_SUCCESS;

cleanup:
    return status;
}

PRStatus
WatchdogClient::sendTerminate(void)
{
    PRStatus status = PR_FAILURE;
    char *msg;
    WDMessages msgType;

    if (!WatchdogClient::isWDRunning()) {
        status = PR_SUCCESS;
        goto cleanup;
    }

    if (!WatchdogClient::wdMsg_) {
        ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_WatchDog_initfail));
        goto cleanup;
    }

    if (WatchdogClient::wdMsg_->SendToWD(wdmsgTerminate, NULL) < 0) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorSending));
        goto cleanup;
    }

    msg = WatchdogClient::wdMsg_->RecvFromWD();
    msgType = WatchdogClient::wdMsg_->getLastMsgType();
    if (msgType != wdmsgTerminatereply) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorReceiving));
        goto cleanup;
    }
    if (msg != NULL) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_nonempty_terminate));
        goto cleanup;
    }

    status = PR_SUCCESS;
cleanup:
    if (status == PR_FAILURE)
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_terminate_fail));
    return status;
}

PRStatus
WatchdogClient::sendReconfigureStatus(const char *statusmsg)
{
    PRStatus status = PR_SUCCESS;
    if (WatchdogClient::isWDRunning())
    {
        if (WatchdogClient::wdMsg_)
        {
            if (WatchdogClient::wdMsg_->SendToWD(wdmsgReconfigStatus, statusmsg) == 1)
            {
                char* msg = WatchdogClient::wdMsg_->RecvFromWD();
                WDMessages msgType = WatchdogClient::wdMsg_->getLastMsgType();
                if (msgType != wdmsgReconfigStatusreply)
                {
                    status = PR_FAILURE;
                    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorReceiving));
                }
            }
            else
            {
                status = PR_FAILURE;
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorSending));
            }
        }
        else
        {
            status = PR_FAILURE;
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_WatchDog_initfail));
        }
    }
    if (status == PR_FAILURE)
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_reconfig_fail));
    return status;
}

PRStatus
WatchdogClient::sendReconfigureStatusDone()
{
    PRStatus status = PR_SUCCESS;
    if (WatchdogClient::isWDRunning())
    {
        if (WatchdogClient::wdMsg_)
        {
            if (WatchdogClient::wdMsg_->SendToWD(wdmsgReconfigStatusDone, NULL) == 1)
            {
                char* msg = WatchdogClient::wdMsg_->RecvFromWD();
                WDMessages msgType = WatchdogClient::wdMsg_->getLastMsgType();
                if (msgType != wdmsgReconfigStatusDonereply)
                {
                    status = PR_FAILURE;
                    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorReceiving));
                }
            }
            else
            {
                status = PR_FAILURE;
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_ErrorSending));
            }
        }
        else
        {
            status = PR_FAILURE;
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_WatchDog_initfail));
        }
    }
    if (status == PR_FAILURE)
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WatchDog_reconfig_status_fail));
    return status;
}

WDMessages
WatchdogClient::getAdminMessage()
{
    WDMessages msgType = wdmsgEmptyRead;
    if (WatchdogClient::isWDRunning())
    {
        if (WatchdogClient::wdMsg_)
        {
            char* msg = WatchdogClient::wdMsg_->RecvFromWD();
            msgType = WatchdogClient::wdMsg_->getLastMsgType();
        }
        else
        {
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_WatchDog_initfail));
        }
    }
    return msgType;
}

int
WatchdogClient::getFD(void)
{
    int fd = -1;

    if (wdMsg_ != NULL)
        fd = wdMsg_->getFD();

    return fd;
}
#ifdef DEBUG
void
WatchdogClient::printInfo(void)
{
    cout << "<WatchdogClient>" << endl
         << "<IsWDRunning> " << WatchdogClient::isWDRunning()
         << "</IsWDRunning>" << endl;
    cout << "</WatchdogClient>" << endl;
}
#endif
