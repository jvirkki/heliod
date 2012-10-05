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
 * netio.h: low-level IO routines
 * 
 * These are intended to be internal only; do not expose.
 * 
 */

#ifndef _LIB_BASE_NETIO_H_
#define _LIB_BASE_NETIO_H_

#include "netsite.h"
#include "base/net.h"
#ifdef NET_SSL
#include "nss.h"
#endif /* NET_SSL */

#ifdef NS_OLDES3X
typedef void (*_net_accept_hook_fn_t)(SSLAcceptFunc);
#endif /* NS_OLDES3X */

typedef int (*_net_accept_fn_t)(SYS_NETFD, struct sockaddr *, int *);
typedef int (*_net_real_accept_fn_t)(SYS_NETFD, struct sockaddr *, int *);
typedef int (*_net_bind_fn_t)(SYS_NETFD, const struct sockaddr *, int);
typedef int (*_net_close_fn_t)(SYS_NETFD);
typedef int (*_net_connect_fn_t)(SYS_NETFD, const void *, int);
typedef int (*_net_data_pending_hack_fn_t)(SYS_NETFD);
typedef int (*_net_force_handshake_fn_t)(SYS_NETFD);
typedef int (*_net_security_status_fn_t)(SYS_NETFD, int *, char **, int *, int *, char **, char **);
typedef int (*_net_getsockopt_fn_t)(SYS_NETFD, int, int, void *, int *);
typedef int (*_net_getpeername_fn_t)(SYS_NETFD, struct sockaddr *, int *);
typedef int (*_net_ioctl_fn_t)(SYS_NETFD, int, void *);
typedef int (*_net_import_fn_t)(SYS_NETFD);
typedef int (*_net_listen_fn_t)(SYS_NETFD, int);
typedef int (*_net_setsockopt_fn_t)(SYS_NETFD, int, int, const void *, int);
typedef int (*_net_select_fn_t)(int, fd_set *, fd_set *, fd_set *, struct timeval *);
typedef int (*_net_socket_fn_t)(int, int, int);
typedef int (*_net_read_fn_t)(SYS_NETFD, char *buf, int);
typedef int (*_net_write_fn_t)(SYS_NETFD, char *buf, int);
typedef int (*_net_writev_fn_t)(SYS_NETFD, NSAPIIOVec *, int);

#ifdef THREAD_NSPR_USER
extern int nspr_accept(SYS_NETFD, struct sockaddr *, int *);
#endif /* THREAD_NSPR_USER */
#ifdef NS_OLDES3X
#ifdef NET_SSL
extern void SSL_AcceptHook(SSLAcceptFunc);
#endif /* NET_SSL */
#else
#include "nss.h"
#endif /* NS_OLDES3X */
extern int _net_simulated_writev(SYS_NETFD, NSAPIIOVec *, int);

typedef struct net_io_t {
	_net_accept_fn_t 			real_accept;
	_net_bind_fn_t				bind;
	_net_close_fn_t				close;
	_net_connect_fn_t			connect;
	_net_getsockopt_fn_t		getsockopt;
	_net_getpeername_fn_t		getpeername;
	_net_ioctl_fn_t				ioctl;
	_net_import_fn_t			import;
	_net_import_fn_t			import_nonblocking;
	_net_listen_fn_t			listen;
	_net_setsockopt_fn_t		setsockopt;
	_net_select_fn_t			select;
	_net_socket_fn_t			socket;
	_net_read_fn_t				read;
	_net_write_fn_t				write;
	_net_writev_fn_t			writev;
		/* SSL specific functions */
#ifdef NS_OLDES3X
	_net_accept_hook_fn_t		accept_hook;
#endif /* NS_OLDES3X */
	_net_real_accept_fn_t		accept;
	_net_data_pending_hack_fn_t	data_pending_hack;
	_net_force_handshake_fn_t	force_handshake;
	_net_security_status_fn_t	security_status;
} net_io_t;

#ifdef __cplusplus
extern "C" net_io_t net_io_functions;
#else
extern net_io_t net_io_functions;
#endif

#endif /* _LIB_BASE_NETIO_H_ */
