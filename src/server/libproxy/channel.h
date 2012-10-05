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

#ifndef LIBPROXY_CHANNEL_H
#define LIBPROXY_CHANNEL_H

/*
 * channel.h: Outbound connection pooling
 * 
 * Chris Elving
 */

#include "netsite.h"

NSPR_BEGIN_EXTERN_C

/*
 * SAFs
 */
Func channel_ssl_client_config;
Func channel_service_channel_dump;

/*
 * SAF names
 */
#define SSL_CLIENT_CONFIG ("ssl-client-config")

/*
 * Channel describes a connection to a server.
 */
typedef struct Channel Channel;
struct Channel {
    PRFileDesc *fd;
};

/*
 * channel_init initializes the outbound connection pooling subsystem.
 */
PRStatus channel_init(void);

/*
 * channel_set_max_idle sets the maximum number of idle channels to keep open
 * simultaneously.  Typically n would be greater than or equal to RqThrottle.
 */
void channel_set_max_idle(int n);

/*
 * channel_set_max_per_server sets the maximum number of channels to keep open
 * per server.  Typically n would be equal to RqThrottle.
 */
void channel_set_max_per_server(int n);

/*
 * channel_set_max_servers sets the maximum number of servers to which the
 * server will simultaneously maintain open connections.  Typically n would be
 * greater than or equal to RqThrottle.
 */
void channel_set_max_servers(int n);

/*
 * channel_acquire grants access to a connection, reusing an existing
 * connection if appropriate.
 */
Channel * channel_acquire(Session *sn, Request *rq, const char *host, int port, PRBool secure, PRBool reuse_persistent, PRIntervalTime timeout);

/*
 * channel_release releases access to a connection.  If keep_alive_timeout is
 * not PR_INTERVAL_NO_WAIT, the connection is not closed and may be returned by
 * a subsequent call to channel_acquire.
 */
void channel_release(Channel *channel, PRIntervalTime keep_alive_timeout);

/*
 * channel_purge closes the specified connection.  In addition, any idle
 * connections to the same daemon are also closed.
 */
void channel_purge(Channel *channel);

/*
 * channel_get_remote_host returns the name of the host at the other end of the
 * specified connection.
 */
const char * channel_get_remote_host(Channel *channel);

/*
 * channel_get_remote_port returns the port number at the other end of the
 * specified connection.
 */
int channel_get_remote_port(Channel *channel);

NSPR_END_EXTERN_C

#endif /* LIBPROXY_CHANNEL_H */
