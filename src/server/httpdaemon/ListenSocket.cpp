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
 * ListenSocket class implementation.
 *
 * @author  celving
 * @since   iWS5.0
 */

#ifdef DEBUG
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
#include <iomanip>
using namespace std;
#else
#include <iostream.h>                        // cout
#include <iomanip.h>                         // setw()
#endif
#endif

#include "ssl.h"                             // SSL_ImportFD
#include "NsprWrap/NsprError.h"              // NsprError
#include "base/ereport.h"                    // LOG_*
#include "base/systhr.h"                     // systhread_sleep
#include "base/servnss.h"                    // NSSEnable()
#include "frame/conf.h"                      // conf_issecurityactive()
#include "private/pprio.h"                   // PR_ImportTCPSocket()
#include "httpdaemon/dbthttpdaemon.h"        // DBT_LS* message strings
#include "httpdaemon/acceptconn.h"           // Acceptor class
#include "httpdaemon/daemonsession.h"        // GetConnQueue
#include "httpdaemon/connqueue.h"            // ConnectionQueue class
#include "httpdaemon/ListenSocket.h"         // ListenSocket class
#include "httpdaemon/WatchdogClient.h"       // WatchdogClient class
#include "httpdaemon/WebServer.h"            // WebServer class
//#include "base/sslconf.h"

#if defined(XP_WIN32) && !defined(SO_EXCLUSIVEADDRUSE)
#define SO_EXCLUSIVEADDRUSE (~SO_REUSEADDR)
#endif

int ListenSocket::reuseOptname_ = SO_REUSEADDR;
int ListenSocket::reuseOptval_ = PR_TRUE;

ListenSocket::ListenSocket(ListenSocketConfig* config) : fd_(NULL),
    acceptors_(NULL), nAcceptors_(0), bExplicitIP_(PR_FALSE),
    bStoppingAcceptors_(PR_FALSE)
{
    config_ = NULL;
    this->setConfig(config);
    memset(&wakeupAddress_, 0, sizeof(wakeupAddress_));
}

ListenSocket::~ListenSocket(void)
{
    if ((this->fd_ != NULL) || (this->acceptors_ != NULL))
        this->close();
    if (this->config_ != NULL)
        this->config_->unref();
    this->config_ = NULL;
}

PRStatus
ListenSocket::create(void)
{
    PRStatus status = ((this->config_ != NULL) ? PR_SUCCESS : PR_FAILURE);

    if (status == PR_SUCCESS)
    {
#if defined(XP_UNIX)
        if (WatchdogClient::isWDRunning())
        {
            const char* ip = this->config_->hasExplicitIP() ? this->config_->getListenIPAddress() : NULL;
            const char* id = this->config_->name;
            PRUint16 port = this->config_->port;
            PRUint16 family = this->config_->getFamily();
            PRUint32 qsize = this->config_->listenQueueSize;
            int sendbuffsize = 0;
            if (this->config_->getSendBufferSize())
                sendbuffsize = *this->config_->getSendBufferSize();
            int recvbuffsize = 0;
            if (this->config_->getReceiveBufferSize())
                recvbuffsize = *this->config_->getReceiveBufferSize();
            PRInt32 wdfd = WatchdogClient::getLS(id, ip, port, family, qsize,
                                                 sendbuffsize, recvbuffsize);
            if (wdfd >= 0)
            {
                this->fd_ = PR_ImportTCPSocket(wdfd);
                this->setProperties();
            }
            else
            {
                errno = -wdfd;
                NsprError::mapUnixErrno();
                this->logError(XP_GetAdminStr(DBT_ListenSocket_watchdogfd));
                status = PR_FAILURE;
            }
        } else {
#endif
            // create a new TCP socket
            status = this->createSocket();
            if (status == PR_SUCCESS)
            {
                // bind it to the address/port specified in the configuration
                PRNetAddr address = this->config_->getAddress();

                // Set SO_REUSEADDR to avoid the TIME_WAIT state that would
                // otherwise occur on a server restart
                net_setsockopt(this->fd_, SOL_SOCKET, reuseOptname_,
                               &reuseOptval_, sizeof(reuseOptval_));

// Do not need to set this on every accept - accepted FDs can inherit them
// See also DaemonSession::DisableTCPDelay, HttpRequest::StartSession
// Should the following be done on All platforms, including NT???
                PRSocketOptionData optdata;
                optdata.option = PR_SockOpt_NoDelay;
                optdata.value.no_delay = PR_TRUE;
                PR_SetSocketOption(this->fd_, &optdata);

                // OSF1 V5.0 1094 won't bind to anything other than INADDR_ANY
                // unless we zero this.  NSPR 4.1.1-beta's PR_StringToNetAddr()
                // writes junk in here.
                if (address.raw.family == PR_AF_INET) {
                    memset(address.inet.pad, 0, sizeof(address.inet.pad));
                }

                status = PR_Bind(this->fd_, &address);

                if (status == PR_SUCCESS)
                {
                    this->setProperties();
                    status = this->listen();
                }
                else
                {
                    this->logError(XP_GetAdminStr(DBT_ListenSocket_bind));
                }
            }
#if defined(XP_UNIX)
        }
#endif
    } else
    {
        this->logError(XP_GetAdminStr(DBT_ListenSocket_nullconfig));
    }
    if (status == PR_SUCCESS)
    {
        status = this->enableSSL();
    }

    this->enableBlocking();
    return status;
}

PRStatus
ListenSocket::close(void)
{
    // set the terminating flag in each of the Acceptor threads
    PRStatus status = this->stopAcceptors();

#if defined(XP_UNIX)
    if (WatchdogClient::isWDRunning())
    {
        status = ((this->config_ != NULL) ? PR_SUCCESS : PR_FAILURE);

        if (status == PR_SUCCESS)
        {
            const char* ip = this->config_->hasExplicitIP() ? this->config_->getListenIPAddress() : NULL;
            const char* id = this->config_->name;
            PRUint16 port = this->config_->port;
            PRUint16 family = this->config_->getFamily();
            PRUint32 qsize = this->config_->listenQueueSize;
            int sendbuffsize = 0;
            if (this->config_->getSendBufferSize())
                sendbuffsize = *this->config_->getSendBufferSize();
            int recvbuffsize = 0;
            if (this->config_->getReceiveBufferSize())
                recvbuffsize = *this->config_->getReceiveBufferSize();
            status = WatchdogClient::closeLS(id, ip, port, family, qsize,
                                             sendbuffsize, recvbuffsize);
            if (status == PR_FAILURE)
            {
                this->logError(XP_GetAdminStr(DBT_ListenSocket_watchdogsocket));
            }
        }
        else
        {
            this->logError(XP_GetAdminStr(DBT_ListenSocket_nullconfig));
        }
    }
#endif

    status = this->deleteAcceptors();

    if (this->fd_ != NULL)
    {
        status = PR_Close(this->fd_);
        this->fd_ = NULL;
        if (status == PR_FAILURE)
            this->logError(XP_GetAdminStr(DBT_ListenSocket_close));
    }

    if (status == PR_SUCCESS)
    {
        // Log that the LS was closed
        this->logInfo(XP_GetAdminStr(DBT_LSClosed_));
    }

    return status;
}

PRFileDesc*
ListenSocket::accept(PRNetAddr& remoteAddress, const PRIntervalTime timeout)
{
    PRFileDesc* fd = PR_Accept(this->fd_, &remoteAddress, timeout);
    if (fd != NULL)
    {
        if (this->bStoppingAcceptors_)
        {
            PRNetAddr localAddress;
            if (PR_GetSockName(fd, &localAddress) == PR_SUCCESS)
            {
                if ((localAddress.raw.family == PR_AF_INET &&
                     localAddress.inet.ip == remoteAddress.inet.ip &&
                     remoteAddress.inet.port == this->wakeupAddress_.inet.port) ||
                    (localAddress.raw.family == PR_AF_INET6 &&
                     !memcmp(&localAddress.ipv6.ip, &remoteAddress.ipv6.ip, sizeof(localAddress.ipv6.ip)) &&
                     remoteAddress.ipv6.port == this->wakeupAddress_.ipv6.port))
                {
                    // Ignore this, it was an internally-generated wakeup
                    PR_Close(fd);
                    fd = NULL;
                }
            }
        }
    }
#ifdef XP_WIN32
    else
    {
        INTnet_cancelIO(this->fd_);
    }
#endif
    return fd;
}

PRStatus
ListenSocket::wakeup(void)
{
    PR_ASSERT(this->bStoppingAcceptors_);

    // Try to wake up the acceptor thread(s) by connecting to the listen
    // socket. This won't work in all circumstances, but the acceptor threads
    // are guaranteed to wake up at most _PR_INTERRUPT_CHECK_INTERVAL_SECS = 5
    // seconds after we call Thread::interrupt() anyway.

    // Open a new client socket using the address family of the listen socket
    PRFileDesc* fd = PR_OpenTCPSocket(this->address_.raw.family);
    if (fd == NULL)
        return PR_FAILURE;

    // Record the client socket's address so we can check it in accept()
    PR_SetNetAddr(PR_IpAddrAny, this->address_.raw.family, 0, &this->wakeupAddress_);
    if (PR_Bind(fd, &this->wakeupAddress_) != PR_SUCCESS || 
        PR_GetSockName(fd, &this->wakeupAddress_) != PR_SUCCESS)
    {
        PR_Close(fd);
        return PR_FAILURE;
    }

    // Try to connect to the listen socket
    PR_Connect(fd, &this->address_, PR_INTERVAL_NO_WAIT);

    PR_Close(fd);

    return PR_SUCCESS;
}

PRStatus
ListenSocket::startAcceptors(ConnectionQueue* connQueue)
{
    PRStatus status = PR_SUCCESS;

    if (this->isValid() == PR_TRUE)
    {
        if (this->createAcceptors(connQueue) == PR_SUCCESS)
        {
            for (int i = 0; i < this->nAcceptors_; i++)
            {
                Thread* t = this->acceptors_[i];
                if (t->start(PR_GLOBAL_BOUND_THREAD, PR_JOINABLE_THREAD) == PR_FAILURE)
                {
                    this->logError(XP_GetAdminStr(DBT_ListenSocket_start_acceptor));
                    status = PR_FAILURE;
                }
            }
            if (status != PR_FAILURE)
                this->logInfo(XP_GetAdminStr(DBT_LSReady_));
        }
        else
        {
            this->logError(XP_GetAdminStr(DBT_ListenSocket_create_acceptor));
            status = PR_FAILURE;
        }
    }
    else
    {
        status = PR_FAILURE;
    }
    return status;
}

PRStatus
ListenSocket::stopAcceptors(void)
{
    PRStatus status = PR_SUCCESS;

    this->bStoppingAcceptors_ = PR_TRUE;

    if (this->acceptors_ != NULL)
    {
        Thread* acceptorThread = NULL;
        for (int i = 0; i < this->nAcceptors_; i++)
        {
            if (this->stopAcceptor(this->acceptors_[i]) == PR_FAILURE)
                status = PR_FAILURE;
        }
    }

    wakeup();

    return status;
}

PRStatus
ListenSocket::stopAcceptor(Thread* acceptorThread)
{
    PRStatus status = PR_SUCCESS;
    if (acceptorThread)
    {
        if (!acceptorThread->wasTerminated())
        {
            acceptorThread->setTerminatingFlag();
            if (acceptorThread->interrupt() == PR_FAILURE)
            {
                status = PR_FAILURE;
                this->logError(XP_GetAdminStr(DBT_ListenSocket_interrupt_acceptor));
            }
        }
    }
    else
    {
        status = PR_FAILURE;
        this->logError(XP_GetAdminStr(DBT_ListenSocket_null_entry_acceptor));
    }
    return status;
}

PRStatus
ListenSocket::createAcceptors(ConnectionQueue* connQueue)
{
    PRStatus status = PR_SUCCESS;
    if (this->acceptors_ == NULL)
    {
        this->nAcceptors_ = this->config_->getConcurrency();
        this->acceptors_ = new Thread*[this->nAcceptors_];
        if (this->acceptors_)
        {
            int i;
            for (i = 0; i < this->nAcceptors_; i++)
                this->acceptors_[i] = NULL;

            for (i = 0; i < this->nAcceptors_; i++)
            {
                if (connQueue)
                {
                    this->acceptors_[i] = new Acceptor(connQueue, this);
                }
                else
                {
                    this->acceptors_[i] = new DaemonSession(this);
                }
                if (this->acceptors_[i] == NULL)
                {
                    this->logError(XP_GetAdminStr(DBT_ListenSocket_malloc_acceptor));
                    status = PR_FAILURE;
                    break;
                }
            }
        }
        else
        {
            this->logError(XP_GetAdminStr(DBT_ListenSocket_malloc_acceptor_table));
            status = PR_FAILURE;
        }
    }
    else
    {
        this->logError(XP_GetAdminStr(DBT_ListenSocket_thread_created));
        status = PR_FAILURE;
    }
    return status;
}

PRStatus
ListenSocket::deleteAcceptors(void)
{
    PRStatus status = PR_SUCCESS;

    // free the memory allocated for the table of acceptor threads
    if (this->acceptors_ != NULL)
    {
        Thread* acceptorThread = NULL;
        for (int i = 0; i < this->nAcceptors_; i++)
            this->deleteAcceptor(this->acceptors_[i]);
        
        delete [] this->acceptors_;
        this->acceptors_ = NULL;
        this->nAcceptors_ = 0;
    }

    this->bStoppingAcceptors_ = PR_FALSE;

    return status;
}

PRStatus
ListenSocket::deleteAcceptor(Thread* acceptorThread)
{
    PRStatus status = PR_SUCCESS;
    if (acceptorThread)
    {
        const PRInt32 LS_TIMEOUT = 30;
        PRInt32 nRetries = 0;

        // Wait for the Acceptor thread to stop running before
        // deleting it
        while (acceptorThread->isRunning() && (nRetries < LS_TIMEOUT))
        {
            wakeup();
            systhread_sleep(1000);
            nRetries++;
        }
        if (!acceptorThread->isRunning())
            delete acceptorThread;
        else
        {
            this->logError(XP_GetAdminStr(DBT_ListenSocket_terminate_acceptor));
            status = PR_FAILURE;
        }
    }
    else
    {
        status = PR_FAILURE;
        this->logError(XP_GetAdminStr(DBT_ListenSocket_null_entry_acceptor));
    }
    return status;
}

PRStatus
ListenSocket::reconfigureAcceptors(ListenSocketConfig& newConfig)
{
    PRStatus status = PR_SUCCESS;

    // If the acceptor threads were previously started...
    if ((this->nAcceptors_ > 0) && (this->acceptors_ != NULL))
    {
        // Add/remove acceptor threads
        int newAcceptorCount = newConfig.getConcurrency();
        if (newAcceptorCount > this->nAcceptors_)
            status = addAcceptors(newAcceptorCount - this->nAcceptors_);
        else if (newAcceptorCount < this->nAcceptors_)
            status = removeAcceptors(this->nAcceptors_ - newAcceptorCount);

        // Log a "ready" message for LSCs added in newConfig
        this->logIPSpecificConfigChange(&newConfig, this->config_,
                                        XP_GetAdminStr(DBT_LSReady_));

        // Log a "closed" message for LSCs removed in newConfig
        this->logIPSpecificConfigChange(this->config_, &newConfig,
                                        XP_GetAdminStr(DBT_LSClosed_));
    }

    return status;
}

PRStatus
ListenSocket::addAcceptors(int nMore)
{
    PRStatus status = PR_SUCCESS;

    ConnectionQueue *connQueue = DaemonSession::GetConnQueue();

    int newSize = this->nAcceptors_ + nMore;

    // Allocate a new array of Acceptors and copy the 
    // existing set of pointers into the new array and then create the
    // additional Acceptors at the end
    Thread** newArray = new Thread*[newSize];
    if (newArray == NULL)
    {
        this->logError(XP_GetAdminStr(DBT_ListenSocket_malloc_acceptor_table));
        status = PR_FAILURE;
    }
    else
    {
        int i;
        int j;
        for (i = 0, j = 0; i < this->nAcceptors_; i++, j++)
            newArray[j] = this->acceptors_[i];

        for (i = this->nAcceptors_; i < newSize; i++)
        {
            if (connQueue)
            {
                newArray[i] = new Acceptor(connQueue, this);
            }
            else
            {
                newArray[i] = new DaemonSession(this);
            }
            if (newArray[i] == NULL)
            {
                this->logError(XP_GetAdminStr(DBT_ListenSocket_malloc_acceptor_table));
                status = PR_FAILURE;
            }
            else
            {
                Thread* t = newArray[i];
                if (t->start(PR_GLOBAL_BOUND_THREAD, PR_JOINABLE_THREAD) == PR_FAILURE)
                {
                    this->logError(XP_GetAdminStr(DBT_ListenSocket_start_acceptor));
                    status = PR_FAILURE;
                }
            }
        }

        delete [] this->acceptors_;
        this->acceptors_ = newArray;
        this->nAcceptors_ = newSize;
    }
    return status;
}

PRStatus
ListenSocket::removeAcceptors(int nLess)
{
    PRStatus status = PR_SUCCESS;

    this->bStoppingAcceptors_ = PR_TRUE;

    if (this->acceptors_ != NULL)
    {
        int i;
        for (i = 0; i < nLess && i < this->nAcceptors_; i++)
            this->stopAcceptor(this->acceptors_[i]);

        int nStopped = 0;
        for (i = 0; i < nLess && i < this->nAcceptors_; i++)
        {
            if (this->deleteAcceptor(this->acceptors_[i]) == PR_SUCCESS)
            {
                this->acceptors_[i] = NULL;
                nStopped++;
            }
        }
        this->bStoppingAcceptors_ = PR_FALSE;
        if (nStopped)
        {
            int newSize = this->nAcceptors_ - nStopped;

            // Allocate a new array of Acceptors and copy the shortened
            // existing set of pointers into the new array and delete the
            // existing array
            Thread** newArray = new Thread*[newSize];
            if (newArray == NULL)
            {
                this->logError(XP_GetAdminStr(DBT_ListenSocket_malloc_acceptor_table));
                status = PR_FAILURE;
            }
            else
            {
                int j;
                for (i = 0, j = 0; i < this->nAcceptors_ && j < newSize; i++)
                {
                    if (this->acceptors_[i] != NULL)
                    {
                        newArray[j] = this->acceptors_[i];
                        j++;
                    }
                }

                delete [] this->acceptors_;
                this->acceptors_ = newArray;
                this->nAcceptors_ = newSize;
            }
        }
        if (nStopped != nLess)
            this->logError(XP_GetAdminStr(DBT_ListenSocket_acceptor_not_terminated));

    }
    return status;
}

PRStatus
ListenSocket::createSocket(void)
{
    PRStatus status = PR_SUCCESS;
    if (this->fd_ == NULL)
    {
        // Create the socket
        PRNetAddr address = this->config_->getAddress();
        this->fd_ = PR_OpenTCPSocket(address.raw.family);
        if (this->fd_ == NULL)
        {
            status = PR_FAILURE;
            this->logError(XP_GetAdminStr(DBT_ListenSocket_create_socket));
        }
    }
    else
    {
        status = PR_FAILURE;
    }
    return status;
}

PRStatus
ListenSocket::enableSSL(void)
{
    PRStatus status = PR_SUCCESS;
    return status;
}

PRStatus
ListenSocket::enableBlocking(void)
{
    PRStatus status = PR_SUCCESS;
#ifdef XP_UNIX
    if (this->config_->blockingIo) {
        PRInt32 flags = fcntl(PR_FileDesc2NativeHandle(this->fd_), F_GETFL, 0);
        fcntl(PR_FileDesc2NativeHandle(this->fd_), F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
    return status;
}

PRBool
ListenSocket::isValid(void) const
{
    if (this->fd_ == NULL)
    {
        this->logError("ListenSocket descriptor is NULL");
        return PR_FALSE;
    }

    if (this->config_ == NULL)
    {
        this->logError(XP_GetAdminStr(DBT_ListenSocket_nullconfig));
        return PR_FALSE;
    }
    return PR_TRUE;
}

void
ListenSocket::logError(const char* msg) const
{
    this->logError(this->config_, msg);

    if (this->config_)
    {
        for (int i = 0; i < this->config_->getIPSpecificConfigCount(); i++)
            this->logError(this->config_->getIPSpecificConfig(i), msg);
    }
}

void
ListenSocket::logError(ListenSocketConfig *config, const char* msg)
{
    /*
     * Print out context information such as the name of the socket,
     * the IP address it was configured for, the port it was configured
     * to listen on and the error msg.
     */
    if (config != NULL)
    {
        const char* id = config->name;
        char ip[256] = "";
        getLogHostname(config, ip, sizeof(ip));
        PRUint16 port = config->port;
        const char* protocol = config->getListenScheme();
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ListenSocket_failure),
                id ? id : "?", protocol, ip, port, msg, system_errmsg());
    }
    else
    {

        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ListenSocket_unknownfailure), msg);
    }
}

void
ListenSocket::logInfo(const char* msg) const
{
    this->logInfo(this->config_, msg);
    
    if (this->config_)
    {
        for (int i = 0; i < this->config_->getIPSpecificConfigCount(); i++)
            this->logInfo(this->config_->getIPSpecificConfig(i), msg);
    }
}

void
ListenSocket::logInfo(ListenSocketConfig *config, const char* msg)
{
    if (msg == NULL)
        return;

    char ip[256] = "";

    /*
     * Print out context information such as the name of the socket,
     * the IP address it was configured for, the port it was configured
     * to listen on.
     */
    if (config != NULL)
    {
        const char* fmt = XP_GetAdminStr(DBT_LS_inform);

        const char* id = config->name;
        getLogHostname(config, ip, sizeof(ip));
        PRUint16 port = config->port;
        const char* protocol = config->getListenScheme();
        if (id != NULL)
        {
            ereport(LOG_INFORM, fmt, id, protocol, ip, port, msg);
        }
        else
        {
            ereport(LOG_INFORM, fmt, "?", protocol, ip, port, msg);
        }
    }
}

void
ListenSocket::getLogHostname(ListenSocketConfig *config, char *buffer, int size)
{
    if (config->hasExplicitIP())
    {
        config->getIPAddressForURL(buffer, size);
    }
    else
    {
        util_strlcpy(buffer, config->getExternalServerName().getHostname(), size);
    }
}

void
ListenSocket::setProperties(void)
{
    if (this->fd_ != NULL)
    {
        PRStatus status = PR_SUCCESS;
        // Prevent the ListenSocket descriptor from being inherited by
        // forks such as CGIs
        if (file_setinherit(this->fd_, 0) != 0)
        {
            status = PR_FAILURE;
            this->logError(XP_GetAdminStr(DBT_ListenSocket_closeonexec));
        }
    }
}

PRStatus
ListenSocket::listen(void)
{
    PRInt32 value = -1;
    PRInt32 valueLen = sizeof(value);

    /* Check whether listen has already been invoked on the socket */

#ifdef Linux
    PRInt32 rc = -1;    // On Linux, cannot find out if listen already invoked 
#else
    PRInt32 rc = net_getsockopt(this->fd_, SOL_SOCKET, SO_ACCEPTCONN,
                                (void*)&value, &valueLen);
#endif

    PRStatus status = PR_SUCCESS;

    /*
     * NB: net_getsockopt() should return -1 on Solaris 2.5.1
     * and multiple listen() calls should work.  On Solaris
     * 2.6 net_getsockopt() should return successfully, with
     * value == 0 if no listen() has been done.
     */
    if ((rc < 0) || (value == 0))
    {
        status = PR_Listen(this->fd_, this->config_->listenQueueSize);
        if (status != PR_SUCCESS)
            this->logError(XP_GetAdminStr(DBT_ListenSocket_listen));
    }
    return status;
}

void
ListenSocket::logIPSpecificConfigChange(ListenSocketConfig* config1,
                                        ListenSocketConfig* config2,
                                        const char* msg)
{
    // Log msg for each IP in config1 that's not in config2
    for (int i = 0; config1 && i < config1->getIPSpecificConfigCount(); i++)
    {
        ListenSocketConfig* ip1 = config1->getIPSpecificConfig(i);
        PRBool found = PR_FALSE;

        for (int j = 0; config2 && j < config2->getIPSpecificConfigCount(); j++)
        {
            ListenSocketConfig* ip2 = config2->getIPSpecificConfig(j);
            if (*ip1 == *ip2)
            {
                found = PR_TRUE;
                break;
            }
        }

        if (!found)
            logInfo(ip1, msg);
    }
}

void
ListenSocket::setConfig(ListenSocketConfig* config)
{
    SafeLock guard(this->configLock_);

    bExplicitIP_ = config->hasExplicitIP();
    address_ = config->getAddress();

    if (this->config_)
    {
        this->reconfigureAcceptors(*config);
        this->config_->unref();     // release the old configuration
    }

    this->config_ = config;
    if (this->config_)
        this->config_->ref();       // mark the new one as in-use
}

ListenSocketConfig*
ListenSocket::getConfig(void)
{
    SafeLock guard(this->configLock_);
    if (this->config_)
        this->config_->ref();       // increase the refcount before returning
    return this->config_;           // a pointer to the socket configuration
}

PRBool
ListenSocket::hasExplicitIP(void) const
{
    return this->bExplicitIP_;
}

PRNetAddr
ListenSocket::getAddress(void) const
{
    return this->address_;
}

void
ListenSocket::setReuseAddr(const char *reuse)
{
#ifdef SO_EXCLUSIVEADDRUSE
    if (!strcasecmp(reuse, "exclusive"))
    {
        reuseOptname_ = SO_EXCLUSIVEADDRUSE;
    }
#endif

    reuseOptval_ = util_getboolean(reuse, PR_TRUE);
}
