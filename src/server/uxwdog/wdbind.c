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
 * wdbind.c - code to allow the watchdog to bind to server ports
 *
 *
 */

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "wdbind.h"
#include "wdlog.h"

int
watchdog_bind(server_socket_t *sock, int num_sock)
{
    int sfd;                    /* next contiguous file descriptor */
    int tfd;                    /* socket temporary file descriptor */
    int ns;                     /* number of sockets bound so far */
    int rv;                     /* result value */
    struct sockaddr saddr;      /* socket address for bind */
    struct sockaddr_in *sa;     /* pointer to TCP/IP socket address */
    char *ipstr;                /* pointer to IP address string */
    int port;                   /* TCP port number */
    int one = 1;                    /* optval for setsockopt */

    sa = (struct sockaddr_in *)&saddr;

    /* Loop on the number of sockets to be bound */
    for (ns = 0, sfd = DEFAULT_WDOG_BASE_FD;
         ns < num_sock; ++sfd) {

        ipstr = sock[ns].ip;
        port = sock[ns].port;

        tfd = socket(AF_INET, SOCK_STREAM, 0);
        if (tfd < 0) {
            /* Failed to get socket */
            rv = -1;
            break;
        }

        /* Dup the socket on the next contiguous file descriptor */
        if (tfd != sfd) {

            rv = dup2(tfd, sfd);
            close(tfd);

            if (rv < 0) {
                /* Failed to dup socket to contiguous fd */
                rv = -2;
                break;
            }
        }

        /* Count number of open contiguous sockets */
        ++ns;

	if ( setsockopt (sfd, SOL_SOCKET, SO_REUSEADDR, 
	    (const void *)&one, sizeof(one) ) < 0) {
            rv = -4;
            break;
	}

        /* Bind socket address */
        memset((void *)sa, 0, sizeof(struct sockaddr));
        sa->sin_family = AF_INET;
        sa->sin_addr.s_addr = (ipstr ? inet_addr(ipstr) : htonl(INADDR_ANY));
        sa->sin_port = htons(port);

        rv = bind(sfd, &saddr, sizeof(struct sockaddr_in));
        if (rv < 0) {
            /* Failed to bind socket to desired address */
            rv = -3;
            break;
        }
    }

    if (rv < 0) {
        char *oserr = strerror(errno);

        if (ipstr) {
            if (guse_stderr) {
                fprintf(stderr,
                "startup failure: could not bind to IP address %s port %d (%s)\n",
                     ipstr, port, oserr);
            }
	    watchdog_log(LOG_ERR,
			 "startup failure: could not bind to IP address %s port %d (%m)",
			 ipstr, port);

        }
        else {
            if (guse_stderr) {
                fprintf(stderr,
                "startup failure: could not bind to port %d (%s)\n",
                     port, oserr);
            }
            watchdog_log(LOG_ERR,
			 "startup failure: could not bind to port %d (%m)",
			 port);
        }

        /* Close any open sockets on error */
        watchdog_unbind(ns, DEFAULT_WDOG_BASE_FD);
    }

    return rv;
}

void
watchdog_unbind(int num_sock, int base_fd)
{
    int i;

    for (i = 0; i < num_sock; ++i, ++base_fd) {
        close(base_fd);
    }
}
