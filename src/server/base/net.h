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

#ifndef BASE_NET_H
#define BASE_NET_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/* GLOBAL_FUNCTIONS:
 * DESCRIPTION:
 * system specific networking definitions
 * 
 * Rob McCool
 */

#ifndef NETSITE_H
#include "netsite.h"
#endif /* !NETSITE_H */

#ifndef BASE_FILE_H
#include "base/file.h"       /* for client file descriptors */
#endif /* !BASE_FILE_H */

#ifndef BASE_PBLOCK_H
#include "base/pblock.h"     /* for client data block */
#endif /* !BASE_PBLOCK_H */

#define NET_DEFAULT_BUFFERSIZE		8192

#define NET_ADDR_STRING_SIZE sizeof("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")

#ifndef AF_NCA
#define AF_NCA 28
#endif

extern unsigned int NET_BUFFERSIZE;
extern int net_enabledns;
extern int net_listenqsize;

/* --- Begin function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

/* --- Internal use only ----------------------------------------------- */
 /* XXXMB - these should be moved to netpriv.h */
NSAPI_PUBLIC int INTnet_init(int security);
NSAPI_PUBLIC int INTnet_shutdown(SYS_NETFD s, int how);

NSAPI_PUBLIC SYS_NETFD INTnet_socket(int domain, int type, int protocol);

NSAPI_PUBLIC SYS_NETFD INTnet_socket_alt(int domain, int type, int protocol);

NSAPI_PUBLIC SYS_NETFD INTnet_create_listener_alt(const char *ipstr, int port, PRBool internal);

#ifdef XP_UNIX
NSAPI_PUBLIC int INTnet_native_handle(SYS_NETFD s);
#else
NSAPI_PUBLIC HANDLE INTnet_native_handle(SYS_NETFD s);
#endif /* XP_UNIX */

NSAPI_PUBLIC int INTnet_listen(SYS_NETFD s, int backlog);

NSAPI_PUBLIC SYS_NETFD INTnet_create_listener(const char *ipaddr, int port);

NSAPI_PUBLIC
int INTnet_connect(SYS_NETFD s, const void *sockaddr, int namelen);

NSAPI_PUBLIC
int INTnet_getpeername(SYS_NETFD s, struct sockaddr *name, int *namelen);

NSAPI_PUBLIC int INTnet_close(SYS_NETFD s);

NSAPI_PUBLIC
int INTnet_bind(SYS_NETFD s, const struct sockaddr *name, int namelen);

NSAPI_PUBLIC
SYS_NETFD INTnet_accept(SYS_NETFD s, struct sockaddr *addr, int *addrlen);

NSAPI_PUBLIC int INTnet_read(SYS_NETFD sd, void *buf, int sz, int timeout);

NSAPI_PUBLIC int INTnet_write(SYS_NETFD sd, const void *buf, int sz);

NSAPI_PUBLIC int INTnet_writev(SYS_NETFD sd, const NSAPIIOVec *iov, int iovlen);

NSAPI_PUBLIC int INTnet_isalive(SYS_NETFD sd);

NSAPI_PUBLIC char *INTnet_ip2host(const char *ip, int verify);

NSAPI_PUBLIC void INTnet_inet_ntoa(struct in_addr ipaddr, char * strIp );

NSAPI_PUBLIC int INTnet_has_ip(const PRNetAddr *addr);

/* --- OBSOLETE ----------------------------------------------------------
 * The following macros/functions are obsolete and are only maintained for
 * compatibility.  Do not use them.
 * -----------------------------------------------------------------------
 */

NSAPI_PUBLIC int INTnet_getsockopt(SYS_NETFD s, int level, int optname,
                                   void *optval, int *optlen);

NSAPI_PUBLIC int INTnet_setsockopt(SYS_NETFD s, int level, int optname,
                                   const void *optval, int optlen);

NSAPI_PUBLIC int INTnet_select(int nfds, fd_set *r, fd_set *w, fd_set *e, 
                               struct timeval *timeout);

NSAPI_PUBLIC int INTnet_ioctl(SYS_NETFD s, int tag, void *result);

NSAPI_PUBLIC int INTnet_socketpair(SYS_NETFD *pair);

#ifdef XP_UNIX

NSAPI_PUBLIC SYS_NETFD INTnet_dup2(SYS_NETFD prfd, int osfd);

NSAPI_PUBLIC int INTnet_is_STDOUT(SYS_NETFD prfd);

NSAPI_PUBLIC int INTnet_is_STDIN(SYS_NETFD prfd);

#endif

PRBool dns_enabled();
NSAPI_PUBLIC void INTnet_cancelIO(PRFileDesc* fd);
NSAPI_PUBLIC int INTnet_flush(SYS_NETFD sd);
NSAPI_PUBLIC int INTnet_sendfile(SYS_NETFD sd, sendfiledata *sfd);
NSAPI_PUBLIC int INTnet_addr_to_string(const PRNetAddr *addr, char *buf, int sz);
NSAPI_PUBLIC int INTnet_addr_cmp(const PRNetAddr *addr1, const PRNetAddr *addr2);
NSAPI_PUBLIC void INTnet_addr_copy(PRNetAddr *addr1, const PRNetAddr *addr2);
NSAPI_PUBLIC int INTnet_is_timeout_safe(void);
NSAPI_PUBLIC PRIntervalTime INTnet_nsapi_timeout_to_nspr_interval(int timeout);
NSAPI_PUBLIC int INTnet_nspr_interval_to_nsapi_timeout(PRIntervalTime interval);
NSAPI_PUBLIC int INTnet_peek(SYS_NETFD sd, void *buf, int sz, int timeout);
NSAPI_PUBLIC int INTnet_buffer_input(SYS_NETFD sd, int sz);

NSPR_END_EXTERN_C


/* --- End function prototypes --- */

#define net_init INTnet_init
#define net_shutdown INTnet_shutdown

#define net_socket INTnet_socket
#define net_listen INTnet_listen
#define net_create_listener INTnet_create_listener
#define net_connect INTnet_connect
#define net_getpeername INTnet_getpeername
#define net_close INTnet_close
#define net_bind INTnet_bind
#define net_accept INTnet_accept
#define net_read INTnet_read
#define net_write INTnet_write
#define net_writev INTnet_writev
#define net_isalive INTnet_isalive
#define net_ip2host INTnet_ip2host
#define net_inet_ntoa INTnet_inet_ntoa
#define net_has_ip INTnet_has_ip
#define net_flush INTnet_flush
#define net_sendfile INTnet_sendfile
#define net_addr_to_string INTnet_addr_to_string
#define net_addr_cmp INTnet_addr_cmp
#define net_addr_copy INTnet_addr_copy
#define net_is_timeout_safe INTnet_is_timeout_safe
#define net_nsapi_timeout_to_nspr_interval INTnet_nsapi_timeout_to_nspr_interval
#define net_nspr_interval_to_nsapi_timeout INTnet_nspr_interval_to_nsapi_timeout
#define net_peek INTnet_peek
#define net_buffer_input INTnet_buffer_input

/* Obsolete */

#define net_getsockopt INTnet_getsockopt
#define net_setsockopt INTnet_setsockopt
#define net_select INTnet_select
#define net_ioctl INTnet_ioctl
#define net_socketpair INTnet_socketpair
#define net_dup2 INTnet_dup2
#define net_is_STDOUT INTnet_is_STDOUT
#define net_is_STDIN INTnet_is_STDIN

#endif /* INTNSAPI */

#endif /* !BASE_NET_H */
