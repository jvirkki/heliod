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

//
// CExecReqPipe.cpp: implementation of the child request pipe functions
//
//  This implements the child stub process' interface handling for
//  use by the ChildExec subsystem.
//  This is intended for use in conjunction with Netscape's 
//  Web Application Interface (WAI) programming model.
//
//  Mike Bennett  (mbennett@netcom.com)      13Mar98
//

#ifdef Linux
#include <string.h>
#endif
#include <assert.h>
#include <sys/uio.h>
#if defined(USE_CONNLD)
#include <sys/types.h>
#include <sys/stat.h>
#include <stropts.h>
#include "Cgistub.h"
#endif
#ifdef USING_NSAPI
#include <unistd.h>
#include <errno.h>
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#else
#include "server.h"
#endif // USING_NSAPI
#include "ChildExec.h"
#include "CExecReqPipe.h"

//
//  Construction
//
CExecReqPipe::CExecReqPipe(ChildExec& cb, int osFd )
: rpExecCB(cb), rpMyState(RPST_DEAD), 
  rpFd(osFd), rpLastError(0), rpNbrWrites(0), rpStateChangeTs(0) 
{
    rpIdle.Prev = NULL;
    rpIdle.Next = NULL;
    rpInuse.Prev = NULL;
    rpInuse.Next = NULL;
}

// destructor
CExecReqPipe::~CExecReqPipe(void)
{
    //  shouldn't delete this unless we're in the correct state
    assert( rpMyState == RPST_DEAD );

    if ( rpFd > -1 ) {
        close(rpFd);
    }
}

//
//  change the state of a particular pipe; this assumes
//  the control block's Lock is held
//
void CExecReqPipe::setState_CBLocked( CEXECRPSTATE newState ) 
{
    CExecReqPipe * rpWork = NULL;

    if ( rpMyState == newState ) {
        return;
    }
    rpStateChangeTs = PR_IntervalNow();
    
    // 
    //  remove the request pipe from whatever list it's on now 
    //
    switch( rpMyState ) {
        case RPST_DEAD:     
            assert( rpIdle.Next  == NULL );
            assert( rpInuse.Next == NULL );
            if ( rpExecCB.cbRpInuse ) {
                rpExecCB.cbRpInuse->rpInuse.Prev = this;
            }
            rpInuse.Prev  = NULL;
            rpInuse.Next  = rpExecCB.cbRpInuse;
            rpExecCB.cbRpInuse = this;
            rpExecCB.cbNbrPipes++; 
            break;
        case RPST_IDLE:     
            if ( this == rpExecCB.cbRpIdle ) {
                rpExecCB.cbRpIdle = rpIdle.Next;
                if ( rpIdle.Next ) {
                    rpIdle.Next->rpIdle.Prev = NULL;
                }
            } else {
                rpIdle.Prev->rpIdle.Next = rpIdle.Next;
                if ( rpIdle.Next ) {
                    rpIdle.Next->rpIdle.Prev = rpIdle.Prev;
                }
            }
            rpIdle.Next = NULL;
            rpIdle.Prev = NULL;
            rpExecCB.cbNbrPipesIdle--;
            break;
        case RPST_INUSE:     
            assert( rpIdle.Next  == NULL );
            break;
        default:
            assert(0);      //  big error
            break;
    }

    // 
    //  now, add it back to the correct list 
    //
    switch ( newState ) {
        case RPST_DEAD:     
            rpMyState = newState;
            if ( this == rpExecCB.cbRpInuse ) {
                rpExecCB.cbRpInuse = rpInuse.Next;
                if ( rpInuse.Next ) {
                    rpInuse.Next->rpInuse.Prev = NULL;
                }
            } else {
                rpInuse.Prev->rpInuse.Next = rpInuse.Next;
                if ( rpInuse.Next ) {
                     rpInuse.Next->rpInuse.Prev = rpInuse.Prev;
                }
            }
            rpInuse.Next = NULL;
            rpInuse.Prev = NULL;
            // when it goes from idle/inuse to dead, it's no longer tracked
            rpExecCB.cbNbrPipes--; 
            break;
        case RPST_IDLE:     
            if ( rpExecCB.cbRpIdle ) {
                rpExecCB.cbRpIdle->rpIdle.Prev = this;
            }
            rpIdle.Prev       = NULL;
            rpIdle.Next       = rpExecCB.cbRpIdle;
            rpMyState         = newState;
            rpExecCB.cbRpIdle = this;
            rpExecCB.cbNbrPipesIdle++;
            rpExecCB.notifyMoreChildren();
            break;
        case RPST_INUSE:     
            rpMyState         = newState;
            break;
        default:
            assert( newState == RPST_INUSE ); /* known failure */
    }
}

//
//  set the state, first obtaining the locks in the control block
//
void CExecReqPipe::setState_CBUnlocked( CEXECRPSTATE newState )
{
    rpExecCB.Lock();
    setState_CBLocked( newState );
    rpExecCB.Unlock();
}

//
//  write a chunk of data to a child; return an indication of whether
//  it worked
// 
PRBool CExecReqPipe::writeMsg( const void * buf, size_t writeLen, 
                               int& sentLen )
{
    int     n;

    assert( rpMyState == RPST_INUSE );
    rpLastError = 0;
    sentLen     = 0;

    //
    //  send all data; if we fail, return false
    //
    while ( sentLen < writeLen ) {
        n = write( rpFd, (char *)buf + sentLen, writeLen - sentLen );
        if ( n < 1 ) {
            rpLastError = errno;
#ifdef CGISTUB_DEBUG
            if ( rpLastError == 0 ) {
                cerr << "CExecReqPipe::writeMsg -- child process [fd " 
                     << rpFd << " failed" << endl;
            } else {
                cerr << "CExecReqPipe::writeMsg - system error " 
                     << rpLastError << " on I/O to child process fd "
                     << rpFd << endl;
            }
#endif // CGISTUB_DEBUG
            return PR_FALSE;
        }
        sentLen += n;
    }

    rpNbrWrites++;
    return PR_TRUE;
}

#if defined(USE_CONNLD)
//
//  read a chunk of data from a child; connld/SVR4 version
// 
PRBool CExecReqPipe::recvMsg( char * rspBuf, size_t rspBufSz, 
                              int& bytesRead, int fdArray[],
                              int fdArraySz, int& nbrFdsRcvd )
{
    assert( rpMyState == RPST_INUSE );
    rpLastError  = 0;
    bytesRead    = 0;
    nbrFdsRcvd   = 0;

    struct strrecvfd    recvfd;
    cstub_start_rsp_t *		rsp;
    int			i;

    //
    //  receive a response; note that as we expect an iovec WITH
    //  fds coming back (yes, this is NOT a general purpose 
    //  model), getting partial data won't happen (FD passing 
    //  being the weird thing that it is)
    //
    if ( (bytesRead = read( rpFd, rspBuf, rspBufSz )) <= 0 ) {
        rpLastError = errno;
#ifdef CGISTUB_DEBUG
        if ( rpLastError == 0 ) {
            cerr << "CExecReqPipe::recvMsg -- child process [fd " 
                 << rpFd << " failed" << endl;
        } else {
            cerr << "CExecReqPipe::recvMsg - system error " 
                 << rpLastError << " on I/O to child process fd "
                 << rpFd << " (receiving data)" << endl;
        }
#endif // CGISTUB_DEBUG
        return PR_FALSE;
    }
    if (bytesRead != rspBufSz) {
#ifdef CGISTUB_DEBUG
	cerr << "CExecReqPipe::recvMsg -- wrong size" << endl;
#endif
        return PR_FALSE;
    }
    rsp = (cstub_start_rsp_t *)rspBuf;
    nbrFdsRcvd = rsp->crsp_nfds;

    for (i=0; i < rsp->crsp_nfds; i++) {
	if (ioctl(rpFd, I_RECVFD, &recvfd) < 0) {
	    break;
	}
	fdArray[i] = recvfd.fd;
    }

#ifdef CGISTUB_DEBUG
    cerr << "CExecReqPipe::recvMsg -- everything hunkydory" << endl;
#endif
    return PR_TRUE;
}

#elif defined(USE_POSIXFDPASSING)

//
//  read a chunk of data from a child; POSIX version
// 
PRBool CExecReqPipe::recvMsg( char * rspBuf, size_t rspBufSz, 
                              int& bytesRead, int fdArray[],
                              int fdArraySz, int& nbrFdsRcvd )
{
    assert( rpMyState == RPST_INUSE );
    rpLastError  = 0;
    bytesRead    = 0;
    nbrFdsRcvd   = 0;

    // make a CHDR for fdArraySz * sizeof(int)
    size_t clen = sizeof(struct cmsghdr) + fdArraySz * sizeof(int);
    struct cmsghdr *chdr = (struct cmsghdr *)malloc(clen);

    //
    //  build the recvmsg struct
    //  
    struct msghdr   msg;
    struct iovec    iov[1];

    msg.msg_name         = NULL;
    msg.msg_namelen      = 0;
    iov[0].iov_base      = rspBuf;
    iov[0].iov_len       = rspBufSz;
    msg.msg_iov          = iov;
    msg.msg_iovlen       = 1;
    msg.msg_control      = chdr;
    msg.msg_controllen   = clen;

#ifdef CGISTUB_DEBUG
    fprintf(stderr, "BEFORE: msg.msg_control = 0x%08x, msg.msg_controllen = %d\n", msg.msg_control, msg.msg_controllen);
#endif
    //
    //  receive a response; note that as we expect an iovec WITH
    //  fds coming back (yes, this is NOT a general purpose 
    //  model), getting partial data won't happen (FD passing 
    //  being the weird thing that it is)
    //
    if ( (bytesRead = recvmsg( rpFd, &msg, 0 )) <= 0 ) {
        rpLastError = errno;
#ifdef CGISTUB_DEBUG
        if ( rpLastError == 0 ) {
            cerr << "CExecReqPipe::recvMsg -- child process [fd " 
                 << rpFd << " failed" << endl;
        } else {
            cerr << "CExecReqPipe::recvMsg - system error " 
                 << rpLastError << " on I/O to child process fd "
                 << rpFd << " (receiving fds)" << endl;
        }
#endif // CGISTUB_DEBUG
	free(chdr);
        return PR_FALSE;
    }

#ifdef CGISTUB_DEBUG
    fprintf(stderr, "AFTER: msg.msg_control = 0x%08x, msg.msg_controllen = %d\n", msg.msg_control, msg.msg_controllen);
    char *p = (char *)msg.msg_control;
    for (int i=0; i<msg.msg_controllen; i++) {
	fprintf(stderr, "%02x%c", p[i], ((i+1)%8 == 0)? '\n': ' ');
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "chdr->cmsg_level = %d, chdr->cmsg_len = %d, chdr->cmsg_type = %d\n", chdr->cmsg_level, chdr->cmsg_len, chdr->cmsg_type);
    p = (char *)chdr;
    for (int i=0; i<chdr->cmsg_len; i++) {
	fprintf(stderr, "%02x%c", p[i], ((i+1)%8 == 0)? '\n': ' ');
    }
    fprintf(stderr, "\n");
#endif

#if defined(UnixWare)
    // for some reason, the fds arrive in the old format on UnixWare
    // hmm.... this might be a bug.
    memcpy(fdArray, msg.msg_control, msg.msg_controllen);
    nbrFdsRcvd = msg.msg_controllen / sizeof(int);
#else
    if (msg.msg_controllen >= sizeof(struct cmsghdr)) {
        if (chdr->cmsg_level != SOL_SOCKET || chdr->cmsg_type != SCM_RIGHTS) {
    	    free(chdr);
	    return PR_FALSE;
        }

        nbrFdsRcvd = (chdr->cmsg_len - sizeof(struct cmsghdr)) / sizeof(int);
    }

    if (nbrFdsRcvd > 0) {
	memcpy(fdArray, CMSG_DATA(chdr), nbrFdsRcvd * sizeof(int));
    }
#endif

#ifdef CGISTUB_DEBUG
    cerr << "CExecReqPipe::recvMsg -- everything hunkydory" << endl;
#endif
    free(chdr);
    return PR_TRUE;
}

#else /* !POSIX_FD_PASSING */

//
//  read a chunk of data from a child; return an indication of whether
//  it worked
// 
#ifdef AIX
extern "C" ssize_t recvmsg(int, struct msghdr *, int);
#endif
PRBool CExecReqPipe::recvMsg( char * rspBuf, size_t rspBufSz, 
                              int& bytesRead, int fdArray[],
                              int fdArraySz, int& nbrFdsRcvd )
{
    assert( rpMyState == RPST_INUSE );
    rpLastError  = 0;
    bytesRead    = 0;
    nbrFdsRcvd   = 0;

    //
    //  build the recvmsg struct
    //  
    struct msghdr   msg;
    struct iovec    iov[1];

    msg.msg_name         = NULL;
    msg.msg_namelen      = 0;
    iov[0].iov_base      = rspBuf;
    iov[0].iov_len       = rspBufSz;
    msg.msg_iov          = iov;
    msg.msg_iovlen       = 1;
    msg.msg_accrights    = (caddr_t) fdArray;
    msg.msg_accrightslen = sizeof(int) * fdArraySz;

    //
    //  receive a response; note that as we expect an iovec WITH
    //  fds coming back (yes, this is NOT a general purpose 
    //  model), getting partial data won't happen (FD passing 
    //  being the weird thing that it is)
    //
    bytesRead = recvmsg( rpFd, &msg, 0 );
    if (bytesRead <= 0 ) {
        rpLastError = errno;
#ifdef CGISTUB_DEBUG
        if ( rpLastError == 0 ) {
            cerr << "CExecReqPipe::recvMsg -- child process [fd " 
                 << rpFd << " failed" << endl;
        } else {
            cerr << "CExecReqPipe::recvMsg - system error " 
                 << rpLastError << " on I/O to child process fd "
                 << rpFd << " (receiving fds)" << endl;
        }
#endif // CGISTUB_DEBUG
        return PR_FALSE;
    }

    nbrFdsRcvd = msg.msg_accrightslen / sizeof(int);
    return PR_TRUE;
}
#endif

