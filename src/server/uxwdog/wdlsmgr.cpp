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
**
** Watchdog Listen Socket Manager
**
** This module manages the list of sockets on which the server
** listens for requests.
**
**/

#ifdef Linux
#define _XOPEN_SOURCE 500
#endif

#ifdef XP_UNIX
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif
#ifdef XP_WIN32
#include <io.h>                        // close()
#endif
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "base/wdservermessage.h"
#include "wdlsmgr.h"
#include "nspr.h"
#include "private/pprio.h"        // PR_FileDesc2NativeHandle()

#ifdef OSF1
#define SOCKOPTCAST const void *
#else
#define SOCKOPTCAST const char *
#endif

#ifndef AF_NCA
#define AF_NCA 28
#endif

wdLSmanager::wdLSmanager():
default_ipaddress(NULL),
default_port(80),
ls_count(0),
ls_table(NULL),
ls_table_size(INITIAL_LS_SIZE),
pa_count(0), pa_table(NULL), pa_table_size(INITIAL_PA_SIZE)
{
}

wdLSmanager::~wdLSmanager()
{
    /* Close all the open Listen Sockets */
    unbind_all();
    ls_count = 0;
    /* Free up data structures */
    if (pa_table)
        free(pa_table);
    if (ls_table)
        free(ls_table);
}

void wdLSmanager::unbind_all(void)
{
    int i;
    if (ls_table != NULL) {
        for (i = 0; i < ls_count; i++) {
            if (ls_table[i].fd != -1) {
                close(ls_table[i].fd);
                ls_table[i].fd = -1;
            }
        }
    }
}

int wdLSmanager::get_table_size(void)
{
    return pa_table_size;
}

int wdLSmanager::create_new_LS(char *UDS_Name, char *new_IP, int new_port,
                               int family, int ls_Qsize, int SendBSize, 
                               int RecvBSize)
{
    int one = 1; /* optval for setsockopt */
    int rv = 0;
    int new_fd;
    PRFileDesc *fd = NULL;

    /* Create and bind socket */
    if (family == AF_UNIX || family == AF_NCA) {
        /* Create an AF_UNIX or AF_NCA socket */
        new_fd = socket(family, SOCK_STREAM, 0);
        if (new_fd < 0) {
            return -errno;
        }

        /* Make the socket Non-Blocking */
        int flags = fcntl(new_fd, F_GETFL, 0);
        fcntl(new_fd, F_SETFL, (flags & ~O_NONBLOCK));

        setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, (SOCKOPTCAST)&one, sizeof(one));

        if (family == AF_UNIX) {
            /* Bind to the appropriate AF_UNIX sockaddr */
            struct sockaddr_un suna;
            if (strlen(UDS_Name) > sizeof(suna.sun_path))
                return -ENAMETOOLONG;
            memset(&suna, 0, sizeof(suna));
            suna.sun_family = AF_UNIX;
            strcpy(suna.sun_path, UDS_Name);
            rv = bind(new_fd, (struct sockaddr*)&suna, sizeof(suna));
        } else {
            setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY, (SOCKOPTCAST)&one, sizeof(one));

            /* AF_NCA's sockaddr looks just like like AF_INET's */
            struct sockaddr_in sina;
            memset(&sina, 0, sizeof(sina));
            sina.sin_family = AF_NCA;
            sina.sin_addr.s_addr = new_IP ? inet_addr(new_IP) : htonl(INADDR_ANY);
            sina.sin_port = htons(new_port);
            rv = bind(new_fd, (struct sockaddr*)&sina, sizeof(sina));
        }

        if (rv < 0) {
            /* Error binding */
            close(new_fd);
            return -errno;
        }

        /* Listen on socket */
        rv = listen(new_fd, ls_Qsize);
        if (rv < 0) {
            return -errno;
        }

    } else {
        /* PR_AF_INET or PR_AF_INET6 */
        PRNetAddr address;
        PRSocketOptionData optdata;

        PR_InitializeNetAddr(PR_IpAddrNull, new_port, &address);
        if (new_IP) {
            if (PR_StringToNetAddr(new_IP, &address) != PR_SUCCESS) {
                return -EINVAL;
            }
        } else {
            PR_SetNetAddr(PR_IpAddrAny, family, new_port, &address);
        }

        /* open a new socket using the address family of the IP address */
        fd = PR_OpenTCPSocket(address.raw.family);
        if (fd == NULL) {
            rv = PR_GetOSError();
            if (!rv)
                rv = EPROTONOSUPPORT;
            return -rv;
        }

        // Make the socket Non-Blocking
        optdata.option = PR_SockOpt_Nonblocking;
        optdata.value.non_blocking = PR_TRUE;
        PR_SetSocketOption(fd, &optdata);

        optdata.option = PR_SockOpt_Reuseaddr;
        optdata.value.reuse_addr = PR_TRUE;
        PR_SetSocketOption(fd, &optdata);

        optdata.option = PR_SockOpt_NoDelay;
        optdata.value.no_delay = PR_TRUE;
        PR_SetSocketOption(fd, &optdata);

        // OSF1 V5.0 1094 won't bind to anything other than INADDR_ANY unless
        // we zero this.  NSPR 4.1.1-beta's PR_StringToNetAddr() writes junk in
        // here.
        if (address.raw.family == PR_AF_INET) {
            memset(address.inet.pad, 0, sizeof(address.inet.pad));
        }

        /* Bind socket address */
        if (PR_Bind(fd, &address) != PR_SUCCESS) {
            PR_Close(fd);
            rv = PR_GetOSError();
            if (!rv)
                rv = EPROTONOSUPPORT;
            return -rv;
        }

        /* Listen on socket */
        if (PR_Listen(fd, ls_Qsize) != PR_SUCCESS) {
            PR_Close(fd);
            return -PR_GetOSError();
        }

        new_fd = PR_FileDesc2NativeHandle(fd);
    }

    /* Set optional buffers */
    if (SendBSize) {
        if (setsockopt(new_fd, SOL_SOCKET, SO_SNDBUF,
                       (SOCKOPTCAST) & SendBSize,
                       sizeof(SendBSize)) < 0) {
            return -errno;
        }
    }
    if (RecvBSize) {
        if (setsockopt(new_fd, SOL_SOCKET, SO_RCVBUF,
                       (SOCKOPTCAST) & RecvBSize,
                       sizeof(RecvBSize)) < 0) {
            return -errno;
        }
    }

    return new_fd;  /* Success */
}

void wdLSmanager::Initialize_new_ls_table(int table_start, int table_size)
{
    int i;
    for (i = table_start; i < table_size; i++) {
        ls_table[i].fd = -1;
        ls_table[i].port = -1;
        ls_table[i].family = -1;
        ls_table[i].ipaddress = NULL;
        ls_table[i].listen_queue_size = -1;
        ls_table[i].send_buff_size = -1;
        ls_table[i].recv_buff_size = -1;
    }
}

char *wdLSmanager::InitializeLSmanager(char *UDS_Name)
{
    int rc, i;
    char *retstr = NULL;

    /* allocate initial Listen Socket table */
    ls_table = (wdLS_entry *) malloc(sizeof(wdLS_entry) * ls_table_size);
    if (ls_table == NULL) {
        return "Allocate of Listen Socket table failed";
    }

    Initialize_new_ls_table(0, ls_table_size);

    /* open Listener on Unix domain socket */
    msg_listener_fd = create_new_LS(UDS_Name, NULL, 0, AF_UNIX, 64, 0, 0);
    if (msg_listener_fd < 0) {
        if (msg_listener_fd == -ENAMETOOLONG) {
            errno = ENAMETOOLONG;
            return "Temporary directory path too long";
        }
        return "Failed to create Listen Socket for messages to server";
    }

    /* Initialize Messages tables */
    msg_table = (msg_info *) malloc(pa_table_size * sizeof(msg_info));
                             // ^ - just make same size as Poll table
    if (msg_table == NULL) {
        return "Allocate of Messages table failed";
    }

    /* Initialize Poll table */
    pa_table =
        (struct pollfd *) malloc(pa_table_size * sizeof(struct pollfd));
    if (pa_table == NULL) {
        return "Allocate of Poll array failed";
    }

    /* Initialize table to mark fd as from Admin server */
    _heard_restart = (int *) malloc(pa_table_size * sizeof(int));
    if (_heard_restart == NULL) {
        return "Allocate of _heard_restart failed";
    }
    for (i = 0; i < pa_table_size; i++) {
        pa_table[i].fd = -1;
        pa_table[i].revents = 0;
        pa_table[i].events = 0;
        _heard_restart[i] = 0;
        msg_table[i].wdSM = NULL;
        msg_table[i]._waiting = 0;
    }
    if (Add_Poll_Entry(msg_listener_fd) == 0) {
        retstr = "Failed to add first entry to Poll table";
    }
    return retstr;
}

int wdLSmanager::Wait_for_Message()
{
    int ready = poll(pa_table, pa_count, 10000 /* ten seconds */ );
    if (ready == 0)          /* Timeout - go back and loop       */
        return ready;
    else if (ready == -1) {  /* Error ?? */
        return ready;
    } else {                 /* > 0 means found events       */
        int i, count;
        count = ready;
        for (i = 0; i < pa_table_size; i++) {
            if (pa_table[i].fd > 0) {  // only look at valid entries
                if (pa_table[i].revents != 0) { // Found events
                    if (pa_table[i].revents & POLLHUP) { // Disconnect
                        msg_table[i]._waiting = -1;
                        // Delete msg_table object??
                    } else if ((i == 0) && (pa_table[i].revents & POLLIN)) {
                        // ready for Accept
                        int newfd = accept(pa_table[i].fd, NULL, 0);
                        if (newfd == -1) {
                            return -errno;
                        }
                        if (Add_Poll_Entry(newfd) == 0) {
                            return -3; // Error ??
                        }
                        ready--; // don't count this event as a message
                    } else if ((pa_table[i].revents & POLLRDBAND) ||
                               (pa_table[i].revents & POLLIN) ||
                               (pa_table[i].revents & POLLRDNORM) ||
                               (pa_table[i].revents & POLLPRI)) {
                        // ready for Read 
                        assert(msg_table[i]._waiting == 0);
                        // shouldn't be one already waiting
                        /* Create messaging to server */
                        if (msg_table[i].wdSM == NULL) {
                            wdServerMessage *wdSM = new
                                wdServerMessage(pa_table[i].fd);
                            if (wdSM == NULL) {
                                // retstr="Failed to start message listener";
                                return -4;
                            } else {
                                msg_table[i].wdSM = wdSM;
                            }
                        }
                        msg_table[i]._waiting = 1;
                    }
                    pa_table[i].revents = 0;
                    count--;
                    if (count == 0)
                        break;
                }
            }
        }
        return ready;
    }
}

int wdLSmanager::Reset_Poll_Entry(int index)
{
    pa_table[index].fd = -1;
    pa_table[index].events = 0;
    return 1;
}

int wdLSmanager::Add_Poll_Entry(int socket)
{
    int i;
    for (i = 0; i < pa_table_size; i++) {
        if (pa_table[i].fd == -1) {
            // Found empty element: set it there
            pa_table[i].fd = socket;
            pa_table[i].events =
                (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI);
            pa_table[i].revents = 0;
            _heard_restart[i] = 0;
            if ((i + 1) > pa_count)
                pa_count = i + 1;
            return 1;
        }
    }
    // Ran out of poll entries without finding any empty- resize??
    return 0;
}

int wdLSmanager::lookupLS(char *new_ls_name, char *new_IP, int new_port,
                          int new_family, int ls_Qsize, int SendBSize,
                          int RecvBSize)
{
    int i;
    char *pzServerIp;

    /* Lookup the entry */
    /* Only look at IP, Port and Socket Family */
    for (i = 0; i < ls_count; i++) {
        pzServerIp = ls_table[i].ipaddress;
        if ((new_port == ls_table[i].port) && (new_family == ls_table[i].family)) {
            if (new_IP == NULL) {
                if (pzServerIp == NULL) {
                    return i; /* found it */
                }
            } else {
                if (pzServerIp && strcmp(new_IP, pzServerIp) == 0) {
                    return i; /* found it */
                }
            }
        }
    }
    /* No match - return not found */
    return -1;
}

int wdLSmanager::getNewLS(char *new_ls_name, char *new_IP, int new_port,
                          int family, int ls_Qsize, int SendBSize, 
                          int RecvBSize)
{
    int new_fd; /* socket temporary file descriptor */

    int i = lookupLS(new_ls_name, new_IP, new_port, family,
                     ls_Qsize, SendBSize, RecvBSize);
    if (i >= 0) {
        /* found it: return LS */
        return ls_table[i].fd;
    }

    /* Listen Socket not found: must add a new one */

    new_fd = create_new_LS(NULL, new_IP, new_port, family,
                           ls_Qsize, SendBSize, RecvBSize);
    if (new_fd < 0) {
        char *oserr = strerror(-new_fd);
        if (!oserr)
            oserr = "Unknown error";
        if (new_IP) {
            fprintf(stderr, 
                    "startup failure: could not bind to %s:%d (%s)\n",
                    new_IP, new_port, oserr);
        } else {
            fprintf(stderr,
                    "startup failure: could not bind to port %d (%s)\n",
                    new_port, oserr);
        }
        new_fd = -errno;
    } else {
        // Success: add to table 
        int rv = addLS(new_ls_name, new_IP, new_port, family, new_fd,
                       ls_Qsize, SendBSize, RecvBSize);
        if (rv == 0) {
            fprintf(stderr, "Could not add to LS table\n");
            new_fd = -new_fd;
        }
    }
    return new_fd;
}

int wdLSmanager::addLS(char *ls_name, char *ip, int port, int family, 
                       int new_fd, int ls_Qsize, int SendBSize, int RecvBSize)
{
    /* Add one Listen Socket to table */

    int i, index;

    /* First search for an empty table entry */
    for (i = 0; i < ls_count; i++) {
        if (ls_table[i].port == -1)
            break;
    }
    if (i < ls_count) {
        index = i;
    } else {
        /* None found - is table full? */
        if (ls_count >= ls_table_size) { /* then make some more */
#ifdef        FEAT_NOLIMITS
            int new_size = ls_table_size * 2;
            ls_table = (wdLS_entry *) realloc(ls_table,
                                              sizeof(wdLS_entry) *
                                              new_size);
            if (ls_table == NULL) {
                return 0;
            }
            Initialize_new_ls_table(ls_table_size, new_size);
            ls_table_size = new_size;
#else
            /* Don't allow more than fixed limit */
            return 0;
#endif
        }
        index = ls_count++;
    }
    ls_table[index].ls_name = strdup(ls_name);
    if (ip)
        ls_table[index].ipaddress = strdup(ip);
    else
        ls_table[index].ipaddress = ip;
    ls_table[index].port = port;
    ls_table[index].family = family;
    ls_table[index].fd = new_fd;
    ls_table[index].listen_queue_size = ls_Qsize;
    ls_table[index].send_buff_size = SendBSize;
    ls_table[index].recv_buff_size = RecvBSize;
    return 1;
}

int wdLSmanager::removeLS(char *new_ls_name, char *new_IP, int new_port,
                          int family, int ls_Qsize, int SendBSize, 
                          int RecvBSize)
{
    int i = lookupLS(new_ls_name, new_IP, new_port, family,
                     ls_Qsize, SendBSize, RecvBSize);
    if (i >= 0) {
        /* found it: close fd */
        close(ls_table[i].fd);
        /* remove from table : port will never match  */
        ls_table[i].port = -1;
        return 1;
    }
    /* Not found - return error */
    return i;
}
