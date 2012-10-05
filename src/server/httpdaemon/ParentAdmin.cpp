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
#include <sys/types.h>                     // pid_t, mode_t
#include <sys/stat.h>                      // umask()
#include <signal.h>                        // kill()
#include <unistd.h>                        // getpid()
#include <sys/socket.h>                    // socket()
#include <sys/un.h>                        // sockaddr_un
#include "private/pprio.h"                 // PR_ImportFile

#include "NsprWrap/NsprError.h"
#include "base/ereport.h"                  // ereport()
#include "base/util.h"                     // util_sprintf()
#include "base/unix_utils.h"               // setFDNonInheritable
#include "base/wdservermessage.h"          // wdservermessage
#include "frame/conf.h"                    // conf_get_true_globals()
#include "httpdaemon/WatchdogClient.h"     // WatchdogClient class
#include "httpdaemon/WebServer.h"          // WebServer class
#include "httpdaemon/ParentAdmin.h"        // ParentAdmin class
#include "httpdaemon/dbthttpdaemon.h"
#include "UnixSignals.h"
#include "httpdaemon/statsmanager.h"

PRInt32 ParentAdmin::nChildren_ = 0;
PRInt32 ParentAdmin::nChildInitDone_ = 0;
char ParentAdmin::channelName_[PATH_MAX];

ParentAdmin::ParentAdmin(const PRInt32 nChildren)
{
    PR_ASSERT(nChildren_ == 0);
    nChildren_ = nChildren;

    admHandle_ = -1;
    admFD_ = NULL;
    wdFD_ = NULL;
    nPollItems_ = 0;
    maxPollItems_ = nChildren_ * 2 + 18;
    buildConnectionPath(system_get_temp_dir(), channelName_, 
                        sizeof(channelName_));
    pollArray_ = (PRPollDesc*)PERM_MALLOC(maxPollItems_* sizeof(PRPollDesc));
    if (pollArray_ == NULL)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_malloc_poll));
    }
    pollIndexWritable_ = (PRBool*)PERM_MALLOC(maxPollItems_* sizeof(PRBool));
    if (pollIndexWritable_ == NULL)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_malloc_poll));
    }

    pollIndexStatsChannel_ = (PRBool*)PERM_MALLOC(maxPollItems_* sizeof(PRBool));
    if (pollIndexStatsChannel_ == NULL)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_malloc_poll));
    }

    for (PRInt32 i = 0; i < maxPollItems_; i++)
    {
        pollArray_[i].fd = NULL;
        pollIndexWritable_[i] = PR_FALSE;
        pollIndexStatsChannel_[i] = PR_FALSE;
    }

    int wdHandle = WatchdogClient::getFD();
    if (wdHandle != -1)
        wdFD_ = PR_ImportFile(wdHandle);
    else
    {
        conf_global_vars_s* globals = conf_get_true_globals();
        if (globals->started_by_watchdog)
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_watchdog_inactive));
    }
}

ParentAdmin::~ParentAdmin()
{
    if (admFD_ != NULL)
    {
        PR_Close(admFD_);
        admFD_ = NULL;
    }
}

/*
   Checks for Admin commands from the watchdog or from one of the
   child processes.
   
   Currently, the only Admin commands that the primordial process reacts
   to (from the watchdog) are: Dynamic Reconfigure, Terminate.

   Once a child process sends wdmsgEndInit on a channel, that channel will
   be used exclusively for relaying messages to the child process from its
   siblings or the watchdog.
*/

void
ParentAdmin::poll(PRIntervalTime timeout)
{
    PRInt32 nReady = PR_Poll(pollArray_, nPollItems_, timeout);

    if (nReady > 0)
    {
        for (PRInt32 i = 0; i < nPollItems_; i++)
        {
            PRPollDesc desc = pollArray_[i];
            if (desc.out_flags & PR_POLL_READ)
            {
                processIncomingMessage(i);
                desc.out_flags = 0;
            }
            else if (desc.out_flags != 0)
            {
                processError(i);
                desc.out_flags = 0;
            }
        }
        removeInvalidItems();
    }
    else if (nReady < 0)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_exit),
                system_errmsg());
        PR_Sleep(timeout);
    }
}

PRStatus
ParentAdmin::init(void)
{
    if (pollArray_ == NULL)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_init_thread));
        return PR_FAILURE;
    }

    if (createAdminChannel() == PR_FAILURE)
        return PR_FAILURE;

    addPollItem(admFD_);

    return PR_SUCCESS;
}

void
ParentAdmin::unlinkAdminChannel(void)
{
    unlink(channelName_);
}

PRStatus
ParentAdmin::createAdminChannel(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        NsprError::mapUnixErrno();
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_ParentAdmin_socket),
                system_errmsg());
        return PR_FAILURE;
    }

    setFDNonInheritable(fd);

    // Make the socket non-blocking
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, (flags | O_NONBLOCK));

    struct sockaddr_un address;
    if (strlen(channelName_) >= sizeof(address.sun_path)) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_ParentAdmin_path_too_long),
                channelName_);
        return PR_FAILURE;
    }
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, channelName_);

    // remove the file in case it exists already
    unlink(channelName_);

#ifdef Linux
    // Change the umask so that unix domain socket is created with
    // permissions for all. This enables servers that run as nobody
    // to connect to this admin channel (created by root)
    mode_t oldmask;
    if (geteuid() == 0)
        oldmask = umask(0);
#endif

    int status = bind(fd, (struct sockaddr *)&address, sizeof(address));
    if (status < 0)
    {
#ifdef Linux
        // Restore the original umask
        if (geteuid() == 0)
            umask(oldmask);
#endif
        NsprError::mapUnixErrno();
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_ParentAdmin_bind),
                address.sun_path,
                system_errmsg());
        return PR_FAILURE;
    }

#ifdef Linux
     // Restore the original umask
     if (geteuid() == 0)
        umask(oldmask);
#endif

    status = listen(fd, nChildren_ * 2 + 18);
    if (status < 0)
    {
        NsprError::mapUnixErrno();
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_ParentAdmin_listen),
                system_errmsg());
        return PR_FAILURE;
    }

    admHandle_ = fd;
    admFD_ = PR_ImportFile(fd);         // Create a PRFileDesc* for PR_Poll

    return PR_SUCCESS;
}

PRStatus 
ParentAdmin::addPollItem(PRFileDesc* fd)
{
    PRStatus status = PR_FAILURE;
    if (pollArray_ != NULL)
    {
        if (nPollItems_ < maxPollItems_)
        {
            // Add the item at the end of the poll array
            status = PR_SUCCESS;
            pollArray_[nPollItems_].fd = fd;
            pollArray_[nPollItems_].in_flags = (PR_POLL_READ|PR_POLL_EXCEPT);
            pollArray_[nPollItems_].out_flags = 0;
            pollIndexWritable_[nPollItems_] = PR_FALSE;
            pollIndexStatsChannel_[nPollItems_] = PR_FALSE;
            nPollItems_++;
        }
    }
    return status;
}


PRStatus 
ParentAdmin::invalidatePollItem(int i)
{
    PRStatus status = PR_FAILURE;
    if (pollArray_ != NULL)
    {
        status = PR_SUCCESS;
        pollArray_[i].fd = NULL;
        pollArray_[i].in_flags = 0;
        pollArray_[i].out_flags = 0;
        pollIndexWritable_[i] = PR_FALSE;
        pollIndexStatsChannel_[i] = PR_FALSE;
    }
    return status;
}

PRStatus
ParentAdmin::removeInvalidItems(void)
{
    if (pollArray_ != NULL)
    {
        PRInt32 nRemoved = 0;
        for (PRInt32 i = nPollItems_ - 1; i >= 0; i--)
        {
            if (pollArray_[i].fd == NULL)
            {
                // Move everything else to fill the space created by the 
                // one entry that was removed
                for (PRInt32 j = i+1; j < nPollItems_; j++)
                {
                   pollArray_[j-1].fd = pollArray_[j].fd; 
                   pollArray_[j-1].in_flags = pollArray_[j].in_flags; 
                   pollArray_[j-1].out_flags = pollArray_[j].out_flags; 
                   pollIndexWritable_[j-1] = pollIndexWritable_[j];
                   pollIndexStatsChannel_[j-1] =  pollIndexStatsChannel_[j];
                   
                   invalidatePollItem(j);
                }
                nRemoved++;
            }
        }
        nPollItems_ -= nRemoved;
    }
    return PR_SUCCESS;
}


void
ParentAdmin::processIncomingMessage(int i)
{
    if (pollArray_[i].fd == admFD_)
        acceptChildConnection();
    else if (pollArray_[i].fd == wdFD_)
        processWatchdogMessage(i);
    else if ( pollIndexStatsChannel_[i] == PR_TRUE )
        processStatsMessage(i);
    else
        processChildMessage(i);
}

void
ParentAdmin::acceptChildConnection(void)
{
    for (;;) 
    {
        int newFD = accept(admHandle_, NULL, 0);
        if (newFD == -1)
            break;

        // Make the socket blocking
        int flags = fcntl(newFD, F_GETFL, 0);
        fcntl(newFD, F_SETFL, (flags & ~O_NONBLOCK));

        setFDNonInheritable(newFD);
        PRFileDesc *fd = PR_ImportFile(newFD);
        if (addPollItem(fd) != PR_SUCCESS)
        {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_accept));
            PR_Close(fd);
        }
    }
}

void
ParentAdmin::processWatchdogMessage(int i)
{
    WDMessages msgType = WatchdogClient::getAdminMessage();
    if (msgType == wdmsgEmptyRead)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_channel_shutdown));
        processError(i);
    }
    else
    {
        // Notify each of the child processes to reconfigure/terminate/etc.
        sendMessageToChildren(msgType);
        if (msgType == wdmsgReconfigure)
        {
            // Watchdog started the reconfigure.
            processReconfigure();
        }
    }
}

void
ParentAdmin::processChildMessage(int i)
{
    wdServerMessage msgChannel(PR_FileDesc2NativeHandle(pollArray_[i].fd));

    char* msg = msgChannel.RecvFromServer();
    WDMessages msgType = msgChannel.getLastMsgType();
    if (msgType == wdmsgEndInit)
    {
        // Acknowledge the message from the child
        msgChannel.SendToServer(wdmsgEndInitreply, NULL);

        ParentAdmin::nChildInitDone_++;

        if (wdFD_ != NULL && nChildInitDone_ == nChildren_)
        {
            // Tell the watchdog to release the terminal
            WatchdogClient::sendEndInit(nChildren_);

            // Start looking for messages from the watchdog
            addPollItem(wdFD_);
        }

        // Read the message as all further communication with child are started
        // by Parent to Child. We have just sent wdmsgEndInitreply message so
        // if we send the next message before child get a chance to read it,
        // then both of the messages will be concatented so it is safe to
        // receive message from Child now. After this point, primordial will
        // write the message and child will respond.  This is to avoid
        // concatening of messages.
        msg = msgChannel.RecvFromServer();
        if (msgChannel.getLastMsgType() != wdmsgEndInitreplyAck)
        {
            ereport(LOG_VERBOSE, "Primordial did not receive EndInitreplyAck");
        }

        // From now on, this channel will be used to send messages to a child
        pollIndexWritable_[i] = PR_TRUE;

        // handle post child initialization in Parent.
        handleChildInit(pollArray_[i].fd); 
    }
    else if (msgType == wdmsgEmptyRead)
    {
        ereport(LOG_VERBOSE, "Child process closed admin channel");
        processError(i);
    }
    else if (msgType == wdmsgIdentifyStatsChannel)
    {
        // Send a reply (a string containing true/false).
        const char* responseMsg = "false";
        if (StatsManager::isEnabled() == PR_TRUE)
        {
            pollIndexStatsChannel_[i] = PR_TRUE;
            responseMsg = "true";
        }
        msgChannel.SendToServer((WDMessages)(msgType + 1), responseMsg);
    }
    else
    {
        // Our synchronous protocol requires channels be unidirectional
        PR_ASSERT(!pollIndexWritable_[i]);

        if (msgChannel.isStatsMessage())
        {
            // This is a error. Stats request should not come here.
            // Close the connection to notify the caller.
            processError(i);
        }
        else
        {
            // Child had a message for its peers
            sendMessageToChildren(msgType);

            // Send a reply
            msgChannel.SendToServer((WDMessages)(msgType + 1), NULL);
            if (msgType == wdmsgPeerReconfigure)
            {
                // Child started the reconfigure.
                processReconfigure();
            }
        }
    }

    msgChannel.invalidate();       // Don't close() the fd
}

void
ParentAdmin::processError(int i)
{
    PRFileDesc* fd = pollArray_[i].fd;
    invalidatePollItem(i);
    // Close any descriptor on which an error occurs EXCEPT the connection
    // to the Watchdog as the WatchdogClient class manages it
    if (fd != wdFD_)
    {
        if (fd == admFD_)
        {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ParentAdmin_primary_shutdown));
            admFD_ = NULL;
            admHandle_ = -1;
        }
        processCloseFd(fd);
        PR_Close(fd);
    }
}

void
ParentAdmin::sendMessageToChildren(WDMessages msgType)
{
    // Notify each of the child processes to reconfigure/terminate/etc.
    for (int i = 0; i < nPollItems_; i++)
    {
        if (pollIndexWritable_[i])
        {
            PRFileDesc* childFD = pollArray_[i].fd;
            PR_ASSERT((childFD != admFD_) && (childFD != wdFD_));
            wdServerMessage msgChannel(PR_FileDesc2NativeHandle(childFD));
            msgChannel.SendToServer(msgType, NULL);
            msgChannel.RecvFromServer();
            PR_ASSERT(msgChannel.getLastMsgType() == msgType + 1 || msgChannel.getLastMsgType() == wdmsgEmptyRead);
            msgChannel.invalidate();
        }
    }

    // Behave as though we received a SIGTERM when we're asked to terminate
    if (msgType == wdmsgTerminate)
        UnixSignals::Fake(SIGTERM);
}

PRStatus
ParentAdmin::terminateChildren(void)
{
    PRStatus rv = PR_SUCCESS;

    // Notify each of the child processes to terminate
    for (int i = 0; i < nPollItems_; i++)
    {
        if (pollIndexWritable_[i])
        {
            PRFileDesc* childFD = pollArray_[i].fd;
            //PR_ASSERT((childFD != admFD_) && (childFD != wdFD_));
            wdServerMessage msgChannel(PR_FileDesc2NativeHandle(childFD));
            msgChannel.SendToServer(wdmsgTerminate, NULL);
            if (!msgChannel.RecvFromServer()) {
                if (msgChannel.getLastMsgType() != wdmsgEmptyRead)
                    rv = PR_FAILURE;
            }
            msgChannel.invalidate();
        }
    }

    return rv;
}

// Return -1 on failure
int
ParentAdmin::connectInternal(const char* channelName, int retries)
{
    if ( ! channelName )
        return -1;

    int fd = -1;
    for (; retries > 0; retries--)
    {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
        {
            NsprError::mapUnixErrno();
            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_ParentAdmin_socket),
                    system_errmsg());
            return -1;
        }

        setFDNonInheritable(fd);

        struct sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, channelName);

        int rv = ::connect(fd, (struct sockaddr*)&address, sizeof(address));
        if (rv != -1)
            break;
        NsprError::mapUnixErrno();

        close(fd);
        fd = -1;

        if (retries > 1)
            PR_Sleep(PR_SecondsToInterval(1));
    }

    if (fd == -1)
    {
        if (channelName == channelName_)
        {
            // Since the stats are also using the same channel. This function
            // could be called either by within process group (primordial,
            // child etc) or outside child processes. Log the message only if
            // function is called by this instance's child processes only. If
            // admin channel or snmp client calls this function then it should
            // simply return error.  In those cases logging depends on the
            // caller's wish.
            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_ParentAdmin_connect),
                    system_errmsg());
        }
    }

    return fd;
}

wdServerMessage*
ParentAdmin::connect()
{
    int fd = ParentAdmin::connectInternal(channelName_,
                                          PARENT_ADMIN_RETRY_COUNT);
    if ( fd == -1 )
        return NULL;
    if ( StatsManager::isEnabled() != PR_TRUE )
    {
        return new wdServerMessage(fd);
    }
    else
    {
        return new StatsServerMessage(fd);
    }
}

// if channelName is zero it will try to connect the same instance's Admin
// Channel
StatsServerMessage*
ParentAdmin::connectStatsChannel(const char* channelName,
                                 int retries)
{
    if ( channelName == 0 )
    {
        channelName = channelName_;
    }
    int fd = ParentAdmin::connectInternal(channelName, retries);
    if ( fd == -1 )
        return NULL;
    // This will happen only if outside process will try to connect.
    return new StatsServerMessage(fd);
}

int 
ParentAdmin::buildConnectionPath(const char* connectionPathDir,
                                 char* outputPath,
                                 int sizeOutputPath)
{
    return util_snprintf(outputPath, sizeOutputPath,
                         "%s/"PRODUCT_DAEMON_BIN".socket", 
                         connectionPathDir);
}

PRBool
ParentAdmin::isChildInitDone(void)
{
    return nChildInitDone_ >= nChildren_;
}

PRInt32 
ParentAdmin::findFileDescIndex(PRFileDesc* fd)
{
    for (PRInt32 i = 0; i < nPollItems_; i++)
    {
        if ( pollArray_[i].fd == fd )
        {
            return i;
        } 
    }
    return -1;
}

void 
ParentAdmin::handleChildInit(PRFileDesc* fd)
{
    // Do nothing
    // Derive classes e.g ParentStats will handle this method
}

void 
ParentAdmin::initLate(void)
{
    // Do nothing
    // Derive classes e.g ParentStats will handle this method
}

void 
ParentAdmin::processChildDeath(int pid)
{
    // Do nothing
    // Derive classes e.g ParentStats will handle this method
}

void 
ParentAdmin::processChildStart(int pid)
{
    // Do nothing
    // Derive classes e.g ParentStats will handle this method
}

void
ParentAdmin::processCloseFd(PRFileDesc* fd)
{
    // Do nothing
    // Derive classes e.g ParentStats will handle this method
}

void
ParentAdmin::processStatsMessage(int i)
{
    // It should not come here
    // Derive classes e.g ParentStats will handle this method
    // Close the connection to inform caller that error has happened.
    PR_ASSERT(0);
    processError(i);
}

void
ParentAdmin::processReconfigure()
{
    // Do nothing.
    // Derive classes e.g ParentStats will handle this method
}

