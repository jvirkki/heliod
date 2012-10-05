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
 * net.c: sockets abstraction and DNS related things
 * 
 * Note: sockets created with net_socket are placed in non-blocking mode,
 *       however this API simulates that the calls are blocking.
 *
 * Rob McCool
 */


#include "netsite.h"
#ifdef Linux
#include <sys/ioctl.h>
#endif

#include <frame/conf.h>
#ifdef NS_OLDES3X
#include "sslio/sslio.h"
#else
#include "prnetdb.h"
#include "obsolete/probslet.h"
#endif /* NS_OLDES3X */

#include "net.h"
#include "util.h"
#include "daemon.h"  /* child_exit */
#include "ereport.h" /* error reporting */
#include <string.h>
#ifdef XP_UNIX
#include <arpa/inet.h>  /* inet_ntoa */
#include <netdb.h>      /* hostent stuff */
#include "base/servnss.h" // for SSL_Enable
#ifdef NEED_GHN_PROTO
extern "C" int gethostname (char *name, size_t namelen);
#endif
#endif /* XP_UNIX */

#if defined(OSF1)
#include <stropts.h>
#endif

#include "NsprWrap/NsprError.h"
#include "base/dbtbase.h"

#if defined(OSF1)
#include <stropts.h>
#endif

#ifdef IRIX
#include <bstring.h>   /* fd_zero uses bzero */
#endif
#include "netio.h"

net_io_t net_io_functions;

#ifdef NS_OLDES3X
#include "xp_error.h"
extern "C" {
#include <libares/arapi.h>      /* For Asynchronous DNS lookup */
}
#else
#include "private/pprio.h"
#include "ares/arapi.h"
#include "ssl.h"
#endif /* NS_OLDES3X */

int net_enabledns = 1;
int net_listenqsize = DAEMON_LISTEN_SIZE;
unsigned int NET_BUFFERSIZE = NET_DEFAULT_BUFFERSIZE;

#include "prio.h"


/* ------------------------------ net_socket ------------------------------ */
NSAPI_PUBLIC SYS_NETFD net_socket(int domain, int type, int protocol)
{
    SYS_NETFD sock;

    /*if (security_active)
    {
        sock = PR_NewTCPSocket();
        PRFileDesc *newDesc = SSL_ImportFD(NULL, sock);
	    sock = newDesc;
	    // need to do this because of NSS library bug
	    // SSL_ImportFD does not set up the socket correctly
	    SSL_Enable(sock, SSL_SECURITY, PR_TRUE);
    } else */
        sock = PR_Socket(domain, type, protocol);

    if (sock == NULL)
        return SYS_NET_ERRORFD;
    return sock;
}

/* ------------------------------ net_socket_alt -------------------------- */
/* XXXhep 12/18/97 The public NSAPI net_socket() now calls this function */
NSAPI_PUBLIC SYS_NETFD INTnet_socket_alt(int domain, int type, int protocol)
{
    SYS_NETFD sock;

    /* Get a normal socket, even if SSL is enabled */
    sock = PR_Socket(domain, type, protocol);

    return (sock) ? sock : SYS_NET_ERRORFD;
}

/* ------------------------------ net_native_handle ----------------------- */
#ifdef XP_UNIX
NSAPI_PUBLIC int INTnet_native_handle(SYS_NETFD s)
{
    return (int)PR_FileDesc2NativeHandle(s);
}
#else
NSAPI_PUBLIC HANDLE INTnet_native_handle(SYS_NETFD s)
{
    return (HANDLE)PR_FileDesc2NativeHandle(s);
}
#endif /* XP_UNIX */

/* ---------------------------- net_getsockopt ---------------------------- */
NSAPI_PUBLIC int net_getsockopt(SYS_NETFD s, int level, int optname,
                                void *optval, int *optlen)
{
    int rv;

    rv = getsockopt(PR_FileDesc2NativeHandle(s), level, optname,
                    (char *)optval, (TCPLEN_T*)optlen);
    if (rv == -1)
        NsprError::mapSocketError();

    return rv;
}


/* ---------------------------- net_setsockopt ---------------------------- */


NSAPI_PUBLIC int net_setsockopt(SYS_NETFD s, int level, int optname,
                                const void *optval, int optlen)
{
    int rv;

    rv = setsockopt(PR_FileDesc2NativeHandle(s), level, optname, 
                    (char *)optval, optlen);
    if (rv == -1)
        NsprError::mapSocketError();

    return rv;
}
/* ------------------------------ net_listen ------------------------------ */


NSAPI_PUBLIC int net_listen(SYS_NETFD s, int backlog)
{
    return PR_Listen(s, backlog)==PR_FAILURE?IO_ERROR:0;
}


/* ------------------------- net_create_listener -------------------------- */


NSAPI_PUBLIC SYS_NETFD INTnet_create_listener_alt(const char *ipstr, int port, PRBool internal)
{
    SYS_NETFD sd;
    struct sockaddr_in sa_server;

    if (PR_TRUE == internal)
    {
        // internal listen sockets are always created without SSL 
        sd = INTnet_socket_alt(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    }
    else
    {
        // otherwise the socket may be SSL
        sd = net_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    };

    if (sd == SYS_NET_ERRORFD)
    {
        return SYS_NET_ERRORFD;
    };

    ZERO((char *) &sa_server, sizeof(sa_server));
    sa_server.sin_family=AF_INET;
    sa_server.sin_addr.s_addr = (ipstr ? inet_addr(ipstr) : htonl(INADDR_ANY));
    sa_server.sin_port=htons(port);
    if(net_bind(sd, (struct sockaddr *) &sa_server,sizeof(sa_server)) < 0) {
        return SYS_NET_ERRORFD;
    }
    net_listen(sd, net_listenqsize);

    return sd;
}

/* ------------------------- net_create_listener -------------------------- */

NSAPI_PUBLIC SYS_NETFD net_create_listener(const char *ipstr, int port)
{
    return INTnet_create_listener_alt(ipstr, port, PR_FALSE);
}


//#ifdef NS_OLDES3X

/* ------------------------------ net_select ------------------------------ */


NSAPI_PUBLIC int net_select(int nfds, fd_set *r, fd_set *w, fd_set *e,
                            struct timeval *timeout)
{
    /* this is ugly */
    PR_fd_set rd, wr, ex;
    int index;
    int rv;

    if (nfds > (64*1024))
        return -1;

    PR_FD_ZERO(&rd);
    PR_FD_ZERO(&wr);
    PR_FD_ZERO(&ex);

    for (index=0; index<nfds; index++) {
        if (FD_ISSET(index, r)) 
            PR_FD_NSET(index, &rd);
        if (FD_ISSET(index, w)) 
            PR_FD_NSET(index, &wr);
        if (FD_ISSET(index, e)) 
            PR_FD_NSET(index, &ex);
    }

    rv = PR_Select(0, &rd, &wr, &ex, PR_SecondsToInterval(timeout->tv_sec));
    if (rv > 0) {
        FD_ZERO(r);
        FD_ZERO(w);
        FD_ZERO(e);
        for (index=0; index<nfds; index++) {
            if (PR_FD_NISSET(index, &rd)) 
                FD_SET(index, r);
            if (PR_FD_NISSET(index, &wr)) 
                FD_SET(index, w);
            if (PR_FD_NISSET(index, &ex)) 
                FD_SET(index, e);
        }
    }

    return rv;
}

//#endif /* NS_OLDES3X */

/* ----------------------------- net_isalive ------------------------------ */


NSAPI_PUBLIC int net_isalive(SYS_NETFD sd)
{
    // JRP fix 355991 - deprecate this function in NES 4.0

    /*
     * Reintroduced for Proxy 4.0.
     *
     * We can't reliably detect whether a peer has disconnected, but we can
     * detect one situation that occurs commonly in Proxy: if sn->csd polls
     * ready for reading but there's no data available to be read, the user
     * probably got tired of waiting for a slow origin server.
     *
     * There are numerous caveats.  Here are a few:
     *
     * 1. if the client did a shutdown(s, SHUT_WR), we'll mistakenly assume
     *    that it disconnected
     * 2. if the client sent a pipelined request before it disconnected, we'll
     *    never notice it's gone
     * 3. if the client's network connection died, TCP can take a long time (up
     *    to and including forever) to notice
     *
     * Note that the underlying fd may be O_NONBLOCK or ~O_NONBLOCK, but we
     * must always avoid blocking.
     */

    int fd = PR_FileDesc2NativeHandle(sd);
    if (fd != -1) {
        int rv;

        do {
#if defined(POLLIN) && defined(POLLHUP)
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLIN | POLLHUP;
            pfd.revents = 0;
            rv = poll(&pfd, 1, 0);
#else
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            rv = select(fd + 1, &fds, NULL, NULL, &timeout);
#endif
        } while (rv == -1 && errno == EINTR);
        
        if (rv == -1)
            return 0;

        if (rv == 1) {
            do {
                char c;
                rv = recv(fd, &c, 1, MSG_PEEK);
            } while (rv == -1 && errno == EINTR);

            if (rv == -1 &&
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
                errno != EWOULDBLOCK &&
#endif
                errno != EAGAIN)
            {
                return 0;
            }

            if (rv == 0)
                return 0;
        }
    }

    return 1;
}

/* ------------------------------ net_connect ------------------------------ */

NSAPI_PUBLIC int net_connect(SYS_NETFD s, const void *sockaddr, int namelen)
{
    int rv;

    rv = PR_Connect(s, (PRNetAddr *)sockaddr, PR_INTERVAL_NO_TIMEOUT);

    return rv==PR_FAILURE?IO_ERROR:0;
}


/* ------------------------------ net_ioctl ------------------------------ */


NSAPI_PUBLIC int net_ioctl(SYS_NETFD s, int tag, void *result)
{
    int rv;

#ifdef XP_WIN32
    rv = ioctlsocket(PR_FileDesc2NativeHandle(s),tag,(unsigned long *)result);
    if (rv == -1)
        NsprError::mapWinsock2Error();
#else
    rv = ioctl(PR_FileDesc2NativeHandle(s), tag, result);
    if (rv == -1)
        NsprError::mapUnixErrno();
#endif

    return rv;
}
/* --------------------------- net_getpeername ---------------------------- */


NSAPI_PUBLIC int net_getpeername(SYS_NETFD s, struct sockaddr *name,
                                 int *namelen)
{
    int rv;

    rv = getpeername(PR_FileDesc2NativeHandle(s), name, (TCPLEN_T *)namelen);
    if (rv == -1)
        NsprError::mapSocketError();

    return rv;
}


/* ------------------------------ net_close ------------------------------- */


NSAPI_PUBLIC int net_close(SYS_NETFD s)
{
    return PR_Close(s);
}

NSAPI_PUBLIC int net_shutdown(SYS_NETFD s, int how)
{
#ifdef NS_OLDES3X
    return PR_Shutdown(s, how);
#else
    return PR_Shutdown(s, (PRShutdownHow)how);
#endif /* NS_OLDES3X */
}



/* ------------------------------- net_bind ------------------------------- */

NSAPI_PUBLIC int net_bind(SYS_NETFD s, const struct sockaddr *name,
                          int namelen)
{
    return PR_Bind(s, (PRNetAddr *)name);
}


/* ------------------------------ net_accept ------------------------------ */


NSAPI_PUBLIC SYS_NETFD net_accept(SYS_NETFD  sd, struct sockaddr *addr,
    int *addrlen)
{
    SYS_NETFD sock = PR_Accept(sd, (PRNetAddr *)addr, PR_INTERVAL_NO_TIMEOUT);

    if (sock == NULL)
        return SYS_NET_ERRORFD;
    return sock;
}

/* ------------------------------- net_read ------------------------------- */

NSAPI_PUBLIC int net_read(SYS_NETFD fd, void *buf, int sz, int timeout)
{
    int rv;

    rv = PR_Recv(fd, buf, sz, 0, net_nsapi_timeout_to_nspr_interval(timeout));
#ifdef XP_WIN32
    if (rv < 0) {
        INTnet_cancelIO(fd);
    }
#endif

    return rv;
}


/* ------------------------------ net_writev ------------------------------ */

int net_writev(SYS_NETFD fd, const NSAPIIOVec *iov, int iov_size)
{
    int rv;

    rv  = PR_Writev(fd, (PRIOVec *)iov, iov_size, PR_INTERVAL_NO_TIMEOUT);
#ifdef XP_WIN32
    if (rv < 0) {
        INTnet_cancelIO(fd);
    }
#endif

    return rv;
}


/* ------------------------------ net_write ------------------------------- */

NSAPI_PUBLIC int net_write(SYS_NETFD fd, const void *buf, int sz)
{
    int rv;

    rv = PR_Send(fd, buf, sz, 0, PR_INTERVAL_NO_TIMEOUT);
    if(rv < 0) {
#ifdef XP_WIN32
        INTnet_cancelIO(fd);
#endif
        return IO_ERROR;
    }
    return rv;
}

NSAPI_PUBLIC int net_socketpair(SYS_NETFD *pair)
{
    return PR_NewTCPSocketPair(pair);
}

#ifdef XP_UNIX
NSAPI_PUBLIC SYS_NETFD net_dup2(SYS_NETFD prfd, int osfd)
{
    SYS_NETFD newfd = NULL;

    if (prfd && PR_FileDesc2NativeHandle(prfd) != osfd) {
        if (dup2(PR_FileDesc2NativeHandle(prfd), osfd) != -1) {
            newfd = PR_ImportFile(osfd);
            if (!newfd)
                close(osfd);
        } else {
            NsprError::mapUnixErrno();
        }
    }

    return newfd;
}

NSAPI_PUBLIC int net_is_STDOUT(SYS_NETFD prfd)
{
    if (PR_FileDesc2NativeHandle(prfd) == STDOUT_FILENO)
        return 1;
    return 0;
}

NSAPI_PUBLIC int net_is_STDIN(SYS_NETFD prfd)
{
    if (PR_FileDesc2NativeHandle(prfd) == STDIN_FILENO)
        return 1;
    return 0;
}
#endif /* XP_UNIX */


/* ----------------------------- dns_enabled ------------------------------ */
PRBool
dns_enabled()
{
  return (net_enabledns ? PR_TRUE : PR_FALSE);
}

/* ----------------------------- net_ip2host ------------------------------ */


char *dns_ip2host(const char *ip, int verify);

NSAPI_PUBLIC char *net_ip2host(const char *ip, int verify)
{
    if(!net_enabledns)
        return NULL;

    return dns_ip2host(ip, verify);
}


/* ----------------------------- net_inet_ntoa ---------------------------- */

NSAPI_PUBLIC void net_inet_ntoa(struct in_addr ipaddr, char * strIp )
{
    unsigned char *cp = (unsigned char *)&ipaddr;
    int            ndx;

    for (ndx = 0; ndx < 4; ndx++,cp ++) {
        register unsigned int octet = *cp;
        unsigned int need_leading_zero = 0;

        /* octets are 0-255 */
        if (octet >= 200) {
            *strIp++ = '2';
            octet -= 200;
            need_leading_zero = 1;
        } else if (octet >= 100) {
            *strIp++ = '1';
            octet -= 100;
            need_leading_zero = 1;
        }
        if (octet >= 10) {
            int tens;
            for (tens = 0; octet >= 10; tens++)
                octet -= 10;
            *strIp++ = tens + '0';
        } else if (need_leading_zero ) { /* handle 10x or 20x case */
            *strIp++ = '0';
        }
        *strIp++ = octet + '0';
        *strIp++ = '.';
    }
    /* remove final dot and terminate string */
    *--strIp = 0;
}


/* ---------------------------- util_hostname ----------------------------- */



#ifdef XP_UNIX
#include <sys/param.h>
#else /* WIN32 */
#define MAXHOSTNAMELEN 255
#endif /* XP_UNIX */

/* Defined in dns.c */
char *net_find_fqdn(PRHostEnt *p);

NSAPI_PUBLIC char *util_hostname(void)
{
    char str[MAXHOSTNAMELEN];
    PRHostEnt  *p;
    PRHostEnt   hent;
    char        buf[PR_AR_MAXHOSTENTBUF];
    PRInt32     err;

    gethostname(str, MAXHOSTNAMELEN);
#ifdef NS_OLDES3X
    p = PR_AR_GetHostByName(
                str,
                &hent,
                buf,
                PR_AR_MAXHOSTENTBUF,
                &err,
                PR_AR_DEFAULT_TIMEOUT,
                AF_INET);

    if (p == NULL) 
        return NULL;
    return net_find_fqdn(p);
#else
    err = PR_AR_GetHostByName(str, buf, PR_AR_MAXHOSTENTBUF,
                              &hent, PR_AR_DEFAULT_TIMEOUT, AF_INET);
    if (err != PR_AR_OK)
        return NULL;
    return net_find_fqdn(&hent);
#endif /* NS_OLDES3X */

}


/* ------------------------ net_get_PR_NT_CancelIo ------------------------ */

#ifdef XP_WIN32
typedef PRStatus (*PFN_PR_NT_CancelIo)(PRFileDesc *fd);

static PFN_PR_NT_CancelIo net_pfn_PR_NT_CancelIo = (PFN_PR_NT_CancelIo)-1;

static inline PFN_PR_NT_CancelIo net_get_PR_NT_CancelIo(void)
{
    if (net_pfn_PR_NT_CancelIo == (PFN_PR_NT_CancelIo)-1) {
        MEMORY_BASIC_INFORMATION mbi;

        // Attempt to load PR_NT_CancelIo from the DLL that contains PR_Send.
        // PR_NT_CancelIo is defined in WINNT NSPR (the NSPR flavour that uses
        // fibers and separate IO completion threads) but not in WIN95 NSPR.
        VirtualQuery(&PR_Send, &mbi, sizeof(mbi));
        net_pfn_PR_NT_CancelIo = (PFN_PR_NT_CancelIo)
            GetProcAddress((HINSTANCE)mbi.AllocationBase, "PR_NT_CancelIo");

        if (net_pfn_PR_NT_CancelIo == NULL) {
            ereport(LOG_VERBOSE, "Using WIN95 NSPR");
        } else {
            ereport(LOG_VERBOSE, "Using WINNT NSPR");
        }
    }

    return net_pfn_PR_NT_CancelIo;
}
#endif


/* ----------------------------- net_cancelIO ----------------------------- */

/* VB: INTnet_cancelIO(PRFileDesc* fd)
 *     This function calls PR_NT_CancelIO when an I/O timed out 
 *     or was interrupted.
 *     It first store away the PRError and PROSError onto stack variables.
 *     It then calls PR_CancleIO. In theory this should never fail. So we
 *     assert for it's failure.
 *     Then the original error codes are restored back so that the logic of
 *     any of te callers of the I/O does not need to be modified.
 *     Note: It is wrong to call this fucntion when your IO did not get an error
 */

NSAPI_PUBLIC void INTnet_cancelIO(PRFileDesc* fd)
{
#ifdef XP_WIN32
    // In the case of filters, only the highest layer in the stack (e.g.
    // original net_read call) will make the PR_NT_CancelIo call
    if (fd->higher)
        return;

    // If we're using WIN95 NSPR, we don't need to do anything
    if (net_pfn_PR_NT_CancelIo == NULL)
        return;

    NsprError error;

    error.save();

    // If this isn't a custom, non-socket PRFileDesc...
    if (PR_FileDesc2NativeHandle(fd) != -1) {
        PFN_PR_NT_CancelIo pfn = net_get_PR_NT_CancelIo();
        if (pfn != NULL) {
            // Found a PR_NT_CancelIo() function.  We're using WINNT NSPR.
            // Call PR_NT_CancelIo().
            (*pfn)(fd);
        }
    }

    error.restore();
#endif
}


/* ------------------------- net_is_timeout_safe -------------------------- */

NSAPI_PUBLIC int INTnet_is_timeout_safe(void)
{
#ifdef XP_WIN32
    // Under WINNT NSPR, an IO operation with a timeout can potentially cause
    // a loss of data.  For example, PR_Recv(..., PR_MillisecondsToInterval(1))
    // will typically consume data from the socket but return with an error.
    // This occurs because of a lack of synchronization between the NSPR user
    // thread (Win32 fiber) that initiated the IO request and the IO completion
    // thread that handles the result.  If the user thread's timeout occurs at
    // about the same time the IO completion thread sees the result, the result
    // will be lost.
    return (net_get_PR_NT_CancelIo() == NULL);
#else
    return PR_TRUE;
#endif
}


/* ------------------------------ net_has_ip ------------------------------ */

NSAPI_PUBLIC int INTnet_has_ip(const PRNetAddr *addr)
{
    if (addr) {
        PRNetAddr any;
        switch (addr->raw.family) {
        case PR_AF_INET:
            return (addr->inet.ip != PR_INADDR_ANY);
#ifdef PR_AF_INET6
        case PR_AF_INET6:
            PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, 0, &any);
            return memcmp(&addr->ipv6.ip, &any.ipv6.ip, sizeof(addr->ipv6.ip)) != 0;
#endif
        }
    }
    return PR_FALSE;
}


/* ------------------------------ net_flush ------------------------------- */

NSAPI_PUBLIC int INTnet_flush(SYS_NETFD sd)
{
    if (!sd->methods->fsync) {
        PR_SetError(PR_INVALID_METHOD_ERROR, 0);
        return -1;
    }

    return sd->methods->fsync(sd);
}


/* ----------------------------- net_sendfile ----------------------------- */

NSAPI_PUBLIC int INTnet_sendfile(SYS_NETFD sd, sendfiledata *sfd)
{
    if (!sd->methods->sendfile) {
        PR_SetError(PR_INVALID_METHOD_ERROR, 0);
        return -1;
    }

    int rv = sd->methods->sendfile(sd, (PRSendFileData *)sfd, PR_TRANSMITFILE_KEEP_OPEN, PR_INTERVAL_NO_TIMEOUT);
    if (rv < 0)
        INTnet_cancelIO(sd);

    return rv;
}


/* -------------------------- net_addr_to_string -------------------------- */

static const char octet_strings[256][4] = {
    "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", "10", "11", "12", "13", "14", "15",
    "16", "17", "18", "19", "20", "21", "22", "23",
    "24", "25", "26", "27", "28", "29", "30", "31",
    "32", "33", "34", "35", "36", "37", "38", "39",
    "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "50", "51", "52", "53", "54", "55",
    "56", "57", "58", "59", "60", "61", "62", "63",
    "64", "65", "66", "67", "68", "69", "70", "71",
    "72", "73", "74", "75", "76", "77", "78", "79",
    "80", "81", "82", "83", "84", "85", "86", "87",
    "88", "89", "90", "91", "92", "93", "94", "95",
    "96", "97", "98", "99", "100", "101", "102", "103",
    "104", "105", "106", "107", "108", "109", "110", "111",
    "112", "113", "114", "115", "116", "117", "118", "119",
    "120", "121", "122", "123", "124", "125", "126", "127",
    "128", "129", "130", "131", "132", "133", "134", "135",
    "136", "137", "138", "139", "140", "141", "142", "143",
    "144", "145", "146", "147", "148", "149", "150", "151",
    "152", "153", "154", "155", "156", "157", "158", "159",
    "160", "161", "162", "163", "164", "165", "166", "167",
    "168", "169", "170", "171", "172", "173", "174", "175",
    "176", "177", "178", "179", "180", "181", "182", "183",
    "184", "185", "186", "187", "188", "189", "190", "191",
    "192", "193", "194", "195", "196", "197", "198", "199",
    "200", "201", "202", "203", "204", "205", "206", "207",
    "208", "209", "210", "211", "212", "213", "214", "215",
    "216", "217", "218", "219", "220", "221", "222", "223",
    "224", "225", "226", "227", "228", "229", "230", "231",
    "232", "233", "234", "235", "236", "237", "238", "239",
    "240", "241", "242", "243", "244", "245", "246", "247",
    "248", "249", "250", "251", "252", "253", "254", "255"
};

NSAPI_PUBLIC int INTnet_addr_to_string(const PRNetAddr *addr, char *buf, int sz)
{
    if (addr->raw.family != PR_AF_INET || sz < sizeof("255.255.255.255"))
        return PR_NetAddrToString(addr, buf, sz);

    unsigned char *octets = (unsigned char *)&addr->inet.ip;
    const char *string;

    string = octet_strings[octets[0]];
    while (*string)
       *buf++ = *string++;

    *buf++ = '.';

    string = octet_strings[octets[1]];
    while (*string)
       *buf++ = *string++;

    *buf++ = '.';

    string = octet_strings[octets[2]];
    while (*string)
       *buf++ = *string++;

    *buf++ = '.';

    string = octet_strings[octets[3]];
    while (*string)
       *buf++ = *string++;

    *buf = '\0';

    return PR_SUCCESS;
}


/* ----------------------------- net_addr_cmp ----------------------------- */

NSAPI_PUBLIC int INTnet_addr_cmp(const PRNetAddr *addr1, const PRNetAddr *addr2)
{
    if (int diff = addr1->raw.family - addr2->raw.family)
        return diff;

    switch (addr1->raw.family) {
    case PR_AF_INET:
        if (int diff = addr1->inet.ip - addr2->inet.ip)
            return diff;
        return addr1->inet.port - addr2->inet.port;

    case PR_AF_INET6:
        if (int diff = addr1->ipv6.port - addr2->ipv6.port)
            return diff;
        return memcmp(&addr1->ipv6.ip, &addr2->ipv6.ip, sizeof(addr1->ipv6.ip));

#ifdef XP_UNIX
    case PR_AF_LOCAL:
        return strcmp(addr1->local.path, addr2->local.path);
#endif

    default:
        return memcmp(addr1, addr2, sizeof(*addr1));
    }
}


/* ---------------------------- net_addr_copy ----------------------------- */

NSAPI_PUBLIC void INTnet_addr_copy(PRNetAddr *addr1, const PRNetAddr *addr2)
{
    switch (addr2->raw.family) {
    case PR_AF_INET:
        addr1->inet = addr2->inet;
        break;

    case PR_AF_INET6:
        addr1->ipv6 = addr2->ipv6;
        break;

    default:
        *addr1 = *addr2;
        break;
    }
}


/* ------------------ net_nsapi_timeout_to_nspr_interval ------------------ */

NSAPI_PUBLIC PRIntervalTime INTnet_nsapi_timeout_to_nspr_interval(int timeout)
{
    PRIntervalTime interval;

    if (timeout == NET_INFINITE_TIMEOUT) {
        interval = PR_INTERVAL_NO_TIMEOUT; // -1
    } else if (timeout == NET_ZERO_TIMEOUT) {
        interval = PR_INTERVAL_NO_WAIT; // 0
    } else {
        // XXX elving handle PRIntervalTime overflow
        interval = PR_SecondsToInterval(timeout);
    }

    return interval;
}


/* ------------------ net_nspr_interval_to_nsapi_timeout ------------------ */

NSAPI_PUBLIC int INTnet_nspr_interval_to_nsapi_timeout(PRIntervalTime interval)
{
    int timeout;

    if (interval == PR_INTERVAL_NO_TIMEOUT) {
        timeout = NET_INFINITE_TIMEOUT; // 0
    } else if (interval == PR_INTERVAL_NO_WAIT) {
        timeout = NET_ZERO_TIMEOUT; // -1
    } else {
        timeout = PR_IntervalToSeconds(interval);
        if (timeout < 1)
            timeout = 1;
    }

    return timeout;
}
