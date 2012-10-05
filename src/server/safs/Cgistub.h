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
 * File:    cgistub.h
 *
 * Description:
 *
 *      This header file defines the internal message layouts
 *      used between the child stub processes and the child
 *      stub API library.  
 *
 * NOTE: This header file is not intended for use by upper level 
 *       applications; please use cstubapi.h.
 *
 * Contact:  mike bennett (mbennett@netcom.com)     08Mar98
 */

#ifndef __CGISTUB_H__  
#define __CGISTUB_H__  

#include <stddef.h>
#include <sys/types.h>

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * A Note on the child stub processing subsystem
 *
 * the child stub process subsystem consists of a single executable
 * that runs in three modes:
 *
 * There is, initially and until it's stdin is closed/hung up on, a
 * 'listener' process that manages an IPC socket (AF_UNIX domain) 
 * awaiting connections from other processes.  The listener gets
 * started, binds a AF_UNIX domain socket to the default name or 
 * the name passed to it in it's initial args, and sends back 
 * an OKMSG to STDIN when it's ready to accept connections.  Note
 * that as soon as stdin to the listener is closed, the listener goes
 * away; it's intended that the listener pipe be kept open.
 *
 * Secondly, there are a series of concurrent processes that get started
 * following a 'connect' to the 'listen' socket.  Each of these processes 
 * await a request, as described in the cstub_start_req_t layout below, to 
 * start a subprocess.  Each of these processes read the connected AF_UNIX
 * socket (the request pipe), awaiting a request, processing the 
 * request (by fork()ing itself) and responding with a response message.  
 * Note that requests will be serially processed; a start_response
 * will be generated upon complete processing of a start_request.
 * There is no support in the application protocol for interleaving
 * requests and responses to a single child; concurrency should be
 * managed by having multiple request pipes (ie, multiple 'connect()s'
 * being done to the bound AF_UNIX socket).
 * One more important note: the start RESPONSE message coming back 
 * also comes back with the stdin and stdout FDs of (stream) socketpairs;
 * in order to obtain these 2 fds, the caller MUST use the recvmsg()
 * call.  Use of normal read/recv on the request pipes will cause 
 * loss of fds and very odd errors to occur.
 *
 * Thirdly, while processing a request, the second-stage process will
 * create socket pairs for the child's stdin & out, fork(), do some 
 * minimal processing involving moving around stdin and stdout/err, and 
 * then exec...() the real child process.  Any errors during this will
 * be reported back to the second-stage process, which will send it
 * back to the requestor via the request pipe.
 *
 * to summarize, to get the subsystem created:
 *
 *   create a pipe
 *   start the Cgistub process with the pipe as stdin
 *     (optionally, passing in a unique socket name via the -f arg)
 *   wait for the OK message on the pipe
 *   save the pipe FD somewhere (as, for example, listener_pipe_fd)
 *   
 * for as many concurrent requests as you want to support: 
 *     rqpipe_fd = socket(AF_UNIX, SOCK_STREAM... ); 
 *     connect(rqpipe_fd, listeners_afunix_socket_name )
 *
 * then, when you want to start programs
 *     get a request pipe fd that isn't being used (up to you to manage)
 *     write()/send() a cstub_start_req_t message
 *     recvmsg() to get cstub_start_rsp_t and two fds
 *  
 * if you want to kill an individual second-stage process, just close
 * the listener_pipe_fd
 *
 * if you want to close the listener process, close the listener_pipe_fd
 *
 * this was designed such that all children go away when the 
 * original process using this subsystem goes away.
 *
 * All this is encoded in the ChildExec and CExecReqPipe classes 
 */

/* the default visible name the child stub listener binds */
#ifndef CSTUB_DEFSOCKNAME
#define CSTUB_DEFSOCKNAME       "/tmp/.cgistub"     
#endif /* CSTUB_DEFSOCKNAME */

/* the default name of the child stub process' executable */
#ifndef CSTUB_DEFLISTENPROG
#define CSTUB_DEFLISTENPROG     "./Cgistub"     
#endif /* CSTUB_DEFLISTENPROG */

/*
 *  internal use protocol definitions
 */
typedef enum {
        CRQ_START       = 1 
} csmsg_type_e;

typedef enum {
        CRSP_OK            = 0,  /* OK */
        CRSP_BADMSGTYPE    = 1,  /* unknown message type */
        CRSP_BADVERSION    = 2,  /* incorrect library version */
        CRSP_MISSINGINFO   = 3,  /* missing path or corrupt msg */
        CRSP_CHDIRFAIL     = 4,  /* chdir() failure; see errinfo */
        CRSP_CHROOTFAIL    = 5,  /* chroot() failure; see errinfo */
        CRSP_FORKFAIL      = 6,  /* fork() failure; see errinfo */
        CRSP_EXECFAIL      = 7,  /* exec..() failure; see errinfo */
        CRSP_RESOURCE      = 8,  /* pipe/socketpair create failure */
        CRSP_USERFAIL      = 9,  /* failed to change to requested user */
        CRSP_GROUPFAIL     = 10, /* failed to change to requested group */
        CRSP_STATFAIL      = 11, /* stat() failure; see errinfo */
        CRSP_EXECPERMS     = 12, /* CGI program is unsecured */
        CRSP_EXECOWNER     = 13, /* CGI program is not owned by this user */
        CRSP_SETRLIMITFAIL = 14, /* setrlimit() failure; see errinfo */
        CRSP_NICEFAIL      = 15, /* nice() failure; see errinfo */
        CRSP_INTERNALERR   = 16  /* catchall unexpected error */
} crsp_type_e;

#define PROGNAMESZ      256
#define ARGLEN          256
#define CSTUBVERSION      2     /* version of library */

/* ye-old type-len-vector (TLV) */
typedef enum {
        CRQT_PATH       = 1,    /* path vector */
        CRQT_PROG       = 2,    /* progname vector */
        CRQT_ARGV       = 3,    /* argv array vector */
        CRQT_ENVP       = 4,    /* env pointer vector */
        CRQT_CHDIRPATH  = 5,    /* chdir path */
        CRQT_CHROOTPATH = 6,    /* chroot path */
        CRQT_USERNAME   = 7,    /* user name */
        CRQT_GROUPNAME  = 8,    /* group name */
        CRQT_NICE       = 9,    /* nice increment */
        CRQT_RLIMIT_AS  = 10,   /* setrlimit(RLIMIT_AS) values */
        CRQT_RLIMIT_CORE   = 11,/* setrlimit(RLIMIT_CORE) values */
        CRQT_RLIMIT_CPU    = 12,/* setrlimit(RLIMIT_CPU) values */
        CRQT_RLIMIT_NOFILE = 13,/* setrlimit(RLIMIT_NOFILE) values */
        CRQT_END        = 14    /* no more (optional) vector */
} creq_tlv_type_e;

/* All TLVs start on a TLV_ALIGN offset in a linear buffer */
typedef struct {
    short   type;               /* type of this tlv argument */
    short   len;                /* len of this tlv (inclusive) */
    char    vector;             /* data associated with type/len */
} creq_tlv_t;

#define TLV_ALIGN  sizeof(unsigned)
#define TLV_VECOFF offsetof(creq_tlv_t, vector)
/*
 * sniped from <nss_dbdefs.h>
 * Power-of-two alignments only.
 */
#define ROUND_DOWN(n, align)    (((long)n) & ~((align) - 1))
#define ROUND_UP(n, align)  ROUND_DOWN(((long)n) + (align) - 1, (align))
/* end sniped code */

/* IMPORTANT NOTE :
 * 
 *  all REQUESTS on the request pipe are preceded by a single 32bit 
 *  length containing the length of the entire request (to allow the 
 *  request handling process to differentiate messages on STREAM sockets)
 *  this length is non-inclusive of itself.
 *
 *  RESPONSEs are not length-preceded, as it is expected (and serious
 *  problems would otherwise result) that recvmsg() will be called
 *  to receive (2) fds; the IPC message carrying fds will be atomic 
 *  in nature and MUST pull the fds out, otherwise subsequent reads of
 *  the pipe will error out in unpredictable ways.
 */

/* start request; most info is carried in tlvs */
typedef struct {
        int		cs_msgLength;
        csmsg_type_e    cs_msgtype;
        unsigned        cs_version;
        unsigned        cs_flags;       /* for future use */
        creq_tlv_t      cs_tlv;
} cstub_start_req_t;

/*
 * start response; this is received via recvmsg(), as two fds also
 * come back (as ancillary data) 
 */
typedef struct {
        csmsg_type_e    crsp_msgtype;
        unsigned        crsp_version;
        crsp_type_e     crsp_rspinfo;
        unsigned        crsp_errcode;
        unsigned        crsp_pid;
#if defined(USE_CONNLD)
        int             crsp_nfds;	/* we need to tell server how many fd's we send */
#endif
} cstub_start_rsp_t;

/*
 *  following an initial (listener process) fork, the listener
 *  sends back a message on it's standard IN to say that it did,
 *  in fact, bind the correct socket and is ready to continue.
 *  receiving this message is an indication that things are 
 *  ready -- this is for optional use 
 */
#define CSTUB_READY_MESSAGE "ok"

#ifdef  __cplusplus
}
#endif

#endif /* __CGISTUB_H__ */
