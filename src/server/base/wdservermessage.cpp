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


#include <assert.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include "sys/socket.h"
#include "sys/un.h"
#include "base/unix_utils.h"
#include "base/wdservermessage.h"
#include <strings.h>

int lastmsgtyperecv=0;
int lastmsgtypesent=0;


char * wdmsgNames[wdmsgLast + 1] = {
	"First",
	"GetPWD",
	"GetPWDreply",
	"GetLS",
	"GetLSreply",
	"CloseLS",
	"CloseLSreply",
	"EndInit",
	"EndInitreply",
	"EndInitreplyAck",
	"SetPIDpath",
	"SetPIDpathreply",
	"Restart",
	"Restartreply",
	"Terminate",
	"Terminatereply",
	"Reconfigure",
	"Reconfigurereply",
	"GetReconfigStatus",
	"GetReconfigStatusreply",
	"ReconfigStatus",
	"ReconfigStatusreply",
	"ReconfigStatusDone",
	"ReconfigStatusDonereply",
	"EmptyRead",
	"Rotate",
	"Rotatereply",
	"PeerReconfigure",
	"PeerReconfigurereply",
	"PeerReopenLogs",
	"PeerReopenLogsreply",
	"IdentifyStatsChannel",
	"IdentifyStatsChannelAck",
	"StatsMessageBegin",
	"ReqStatsData",
	"RespStatsData",
	"RespStatsDataAck",
	"RespError",
	"StatsNotification",
	"StatsNotificationAck",
	"StatsMessageLast",
	"Last"
};

// Used by reply messages to verify presence of message string
// 	0 => do nothing (or ignore for non-reply messages)
//	1 => string must be empty
//	2 => string must NOT be empty
int wdmsgCheckLength[wdmsgLast + 1] = { 
    0,  // wdmsgFirst
    0,  // wdmsgGetPWD
    2,  // wdmsgGetPWDreply
    0,  // wdmsgGetLS
    0,  // wdmsgGetLSreply
    0,  // wdmsgCloseLS
    1,  // wdmsgCloseLSreply
    0,  // wdmsgEndInit
    1,  // wdmsgEndInitreply
    0,  // wdmsgEndInitreplyAck
    0,  // wdmsgSetPIDpath
    1,  // wdmsgSetPIDpathreply
    0,  // wdmsgRestart
    0,  // wdmsgRestartreply
    0,  // wdmsgTerminate
    1,  // wdmsgTerminatereply
    0,  // wdmsgReconfigure
    1,  // wdmsgReconfigurereply
    0,  // wdmsgGetReconfigStatus
    0,  // wdmsgGetReconfigStatusreply
    0,  // wdmsgReconfigStatus
    0,  // wdmsgReconfigStatusreply
    0,  // wdmsgReconfigStatusDone
    0,  // wdmsgReconfigStatusDonereply
    0,  // wdmsgEmptyRead
    0,  // wdmsgRotate
    1,  // wdmsgRotatereply
    1,  // wdmsgPeerReconfigure
    0,  // wdmsgPeerReconfigurereply
    0,  // wdmsgPeerReopenLogs
    0,  // wdmsgPeerReopenLogsreply
    0,  // wdmsgIdentifyStatsChannel
    0,  // wdmsgIdentifyStatsChannelAck
    0,  // wdmsgStatsMessageBegin
    0,  // wdmsgReqStatsData
    0,  // wdmsgRespStatsData
    0,  // wdmsgRespStatsDataAck
    0,  // wdmsgRespError
    0,  // wdmsgStatsNotification
    0,  // wdmsgStatsNotificationAck
    0,  // wdmsgStatsMessageLast
    0   // wdmsgLast
};

wdServerMessage::wdServerMessage(int osFd) :
		msgFd(osFd)
{
}

wdServerMessage::~wdServerMessage() 
{
    if (msgFd != -1)
        close(msgFd);
}

void
wdServerMessage::invalidate(void)
{
    msgFd = -1;
}

#ifdef AIX
extern "C" ssize_t recvmsg(int, struct msghdr *, int);
#endif

// Receive the length of the message. The length is sent as a integer vaule so
// sizeof the length message itself is sizeof(int).
int wdServerMessage::recvMessageLength(int* dataLength)
{
    struct msghdr msg;
    struct iovec iov[1];
    int length;

    memset( (char*)&msg, 0, sizeof(struct msghdr));
    memset( (char*)iov, 0, sizeof(struct iovec));

    msg.msg_name    = NULL;
    msg.msg_namelen = 0;
    iov[0].iov_base = (caddr_t) &length;
    iov[0].iov_len  = sizeof(length);
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 1;
    int bytesRead = recvmsg( msgFd, &msg, 0 );
    if (bytesRead > 0 ) {
        assert(bytesRead == sizeof(length));
        // assign the length received.
        *dataLength = length;
    }
    return bytesRead;
}

//  read a chunk of data from a child; return an indication of whether it worked
int wdServerMessage::recvMsg( int fdArray[], int fdArraySz)
{

    msgLastError	= 0;
    msgbytesRead	= 0;
    msgNfds		= 0;

    int bytesRead	= 0;
    int dataLength	= 0;
    bytesRead = recvMessageLength(&dataLength);
    if (bytesRead == 0)
        return 0;
    assert(dataLength >= 0);
    assert(dataLength <= sizeof(msgbuff));

#if defined(USE_POSIXFDPASSING)
    // make a CHDR for fdArraySz * sizeof(int)
    size_t clen = sizeof(struct cmsghdr) + fdArraySz * sizeof(int);
    struct cmsghdr *chdr = (struct cmsghdr *)malloc(clen);
#endif
    //  build the recvmsg struct
    struct msghdr	msg;
    struct iovec	iov[1];

    memset( (char*)&msg, 0, sizeof(struct msghdr));
    memset( (char*)iov, 0, sizeof(struct iovec));

    msg.msg_name	= NULL;
    msg.msg_namelen	= 0;
    iov[0].iov_base	= msgbuff;
    iov[0].iov_len	= dataLength;
    msg.msg_iov		= iov;
    msg.msg_iovlen	= 1;
#if defined(USE_POSIXFDPASSING)
    msg.msg_control	= chdr;
    msg.msg_controllen	= clen;
#ifdef swift_DEBUG
    fprintf(stderr, 
	"BEFORE: msg.msg_control = 0x%08x, msg.msg_controllen = %d\n", 
		 msg.msg_control,	   msg.msg_controllen);
#endif
#else /* !POSIX_FD_PASSING */
    msg.msg_accrights    = (caddr_t) fdArray;
    msg.msg_accrightslen = sizeof(int) * fdArraySz;
#endif

    //  receive a response; note that as we expect an iovec WITH
    //  fds coming back (yes, this is NOT a general purpose 
    //  model), getting partial data won't happen (FD passing 
    //  being the weird thing that it is)
    msgbytesRead = recvmsg( msgFd, &msg, 0 );
    if (msgbytesRead <= 0 ) {
        msgLastError = errno;
#ifdef swift_DEBUG
        if ( msgLastError == 0 ) {
            cerr << "wdServerMessage::recvMsg -- child process [fd " 
                 << msgFd << " failed" << endl;
        } else {
            cerr << "wdServerMessage::recvMsg - system error " 
                 << msgLastError << " on I/O to child process fd "
                 << msgFd << " (receiving fds)" << endl;
        }
#endif // swift_DEBUG
#if defined(USE_POSIXFDPASSING)
	free(chdr);
#endif
        return 0;
    }

    assert(msgbytesRead == dataLength);
    assert(iov[0].iov_len == dataLength);

#if defined(USE_POSIXFDPASSING)
#ifdef swift_DEBUG
    fprintf(stderr, 
	"AFTER: msg.msg_control = 0x%08x, msg.msg_controllen = %d\n", 
		msg.msg_control,	  msg.msg_controllen);
    char *p = (char *)msg.msg_control;
    for (int i=0; i<msg.msg_controllen; i++) {
	fprintf(stderr, "%02x%c", p[i], ((i+1)%8 == 0)? '\n': ' ');
    }
    fprintf(stderr, "\n");
    fprintf(stderr, 
	"chdr->cmsg_level = %d, chdr->cmsg_len = %d, chdr->cmsg_type = %d\n",
	 chdr->cmsg_level,	chdr->cmsg_len,	     chdr->cmsg_type);
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
    msgNfds = msg.msg_controllen / sizeof(int);
#else
    if (msg.msg_controllen >= sizeof(struct cmsghdr)) {
        if (chdr->cmsg_level != SOL_SOCKET || chdr->cmsg_type != SCM_RIGHTS) {
    	    // No File Descriptors, just return
	    free(chdr);
	    return 1;
        }
        msgNfds = (chdr->cmsg_len - sizeof(struct cmsghdr)) / sizeof(int);
    }

    if (msgNfds > 0) {
	memcpy(fdArray, CMSG_DATA(chdr), msgNfds * sizeof(int));
    }
#endif

#ifdef swift_DEBUG
    cerr << "wdServerMessage::recvMsg -- everything hunkydory" << endl;
#endif
    free(chdr);
#else /* !POSIX_FD_PASSING */
    msgNfds = msg.msg_accrightslen / sizeof(int);
#endif
    return 1;
}

//  write a chunk of data to a child; return an indication of whether it worked
int wdServerMessage::writeMsg( const void * buf, int writeLen, int * sentLen)
{
    //  build the sendmsg struct
    struct msghdr msg;
    struct iovec iov[2];

    memset( (char*)&msg, 0, sizeof(struct msghdr));
    memset( (char*)iov, 0,  2 * sizeof(iovec));

    msg.msg_name	= NULL;
    msg.msg_namelen	= 0;
    iov[0].iov_base	= (char*)&writeLen;
    iov[0].iov_len	= sizeof(writeLen);
    iov[1].iov_base	= (char*)buf;
    iov[1].iov_len	= writeLen;
    msg.msg_iov		= iov;
    msg.msg_iovlen	= 2;
#if defined(USE_POSIXFDPASSING)
    msg.msg_control	= NULL;
    msg.msg_controllen	= 0;
#else /* !POSIX_FD_PASSING */
    msg.msg_accrights    = NULL;
    msg.msg_accrightslen = 0;
#endif

    int n = sendmsg( msgFd, &msg, 0 );
    if ( n <= 0 )
        return 0;
    assert(n > writeLen);
    if (n < writeLen )
    {
        return 0;
    }
    else
    {
        if (sentLen) 
            *sentLen = n;
        return 1;
    }
}


// Send the length of the message. Message will be send after this message.
// Length message itself is send as sizeof(int) size.
int wdServerMessage::sendMessageLength(int dataLength)
{
    struct msghdr msg;
    struct iovec  iov[1];

    memset( (char*)&msg, 0, sizeof(struct msghdr));
    memset( (char*)iov, 0, sizeof(struct iovec));

    msg.msg_name    = NULL;
    msg.msg_namelen = 0;
    iov[0].iov_base = (caddr_t) &dataLength;
    iov[0].iov_len  = sizeof(dataLength);
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 1;
    int bytesSent = sendmsg( msgFd, &msg, 0 );
    if (bytesSent > 0 ) {
        assert(bytesSent == sizeof(dataLength));
    }
    return bytesSent;
}

int wdServerMessage::send_LS_to_Server( int connfd, WDMessages msg_type, int ls_fd )
{
    int			i;
    struct msghdr       msg;
    struct iovec        iov[1];

    memset( (char*)&msg, 0, sizeof(struct msghdr));
    memset( (char*)iov, 0, sizeof(struct iovec));

#if defined(USE_POSIXFDPASSING)
    int			clen;
    struct cmsghdr *	chdr;
#endif
    ssize_t             nsent;
    struct msgstruct {
	int		rsp;
	char		msgstr[10];
    } msgstruct;
    struct msgstruct	amsg;
    int                 rsp_desc[2];
    int                 ndesc = 0;

    amsg.rsp		= msg_type;
    if (ls_fd < 0 ) {
	// Put error value into the message string
	memcpy(amsg.msgstr, &ls_fd, sizeof(ls_fd));
    } else {
	ndesc		= 1;
	rsp_desc[0]	= ls_fd;
	memset(amsg.msgstr, 0, sizeof(ls_fd));
    }

    msg.msg_name	= NULL;
    msg.msg_namelen	= 0;

    int dataLength	= sizeof(amsg);
    iov[0].iov_base	= (char *) &amsg;
    iov[0].iov_len	= dataLength;
    msg.msg_iov		= iov;
    msg.msg_iovlen	= 1;

#if defined(USE_POSIXFDPASSING)
    clen = sizeof(struct cmsghdr) + ndesc * sizeof(int);
    chdr = (struct cmsghdr *)malloc(clen);

    chdr->cmsg_len	= clen;
    chdr->cmsg_level	= SOL_SOCKET;
    chdr->cmsg_type	= SCM_RIGHTS;
#if 0
    fprintf(stderr, 
"STUB: chdr->cmsg_level = %d, chdr->cmsg_len = %d, chdr->cmsg_type = %d\n",
	chdr->cmsg_level, chdr->cmsg_len, chdr->cmsg_type);
#endif
    memcpy(CMSG_DATA(chdr), rsp_desc, sizeof(int) * ndesc);

    msg.msg_control	= chdr;
    msg.msg_controllen	= clen;
#else
    msg.msg_accrights    = (caddr_t)     rsp_desc;
    msg.msg_accrightslen = sizeof(int) * ndesc;
#endif

    // Write the length of the data that is being sent in the iov
    // buffer of the sendmsg call
    int nBytes = sendMessageLength(dataLength);

    if (nBytes > 0)
    {
        nsent = sendmsg( connfd, &msg, 0 );
        if ( nsent < 0 ) {
        fprintf(stderr, "failure: error %d passing listen socket to server\n", errno);
        }
    }

#if defined(USE_POSIXFDPASSING)
    free(chdr);
#endif
    return 1;
}

int wdServerMessage::SendToWD (WDMessages messageType, const char * msgstring) {
        lastmsgtypesent = messageType;
	int rc		= 0;
	int sentlen	= 0;
	int strlength	= 0;
	int msglen	= sizeof(WDMessages);
	char * msg_name	= NULL;
	struct msgstruct {
		int	msgT;
		char	buff[WDMSGBUFFSIZE];
	} msgstruct;
	struct msgstruct amsg;
	if (msgstring) strlength = strlen(msgstring);
    if (strlength >= WDMSGBUFFSIZE)
        strlength = WDMSGBUFFSIZE - 1;

	amsg.msgT = messageType;
	if (strlength) {
		memcpy (amsg.buff, msgstring, strlength);
		amsg.buff[strlength]=0;
		msglen = msglen+strlength;
	}
	if ((messageType <= wdmsgFirst) || (messageType >= wdmsgLast)) {
		fprintf(stderr, "in SendToWD: Unimplemented message: %d\n",
			messageType);
		return 0;
	} else {
		msg_name = wdmsgNames[messageType];
		// if (strstr(msg_name, "reply")) {
			// got bad request replies don't belong here
		// }
		rc = writeMsg((const void *)&amsg, msglen, &sentlen);
	}
	if (rc!=1) {
		fprintf(stderr, "failure: error %d sending %s message to watchdog", rc, msg_name);
		return 0;
	}
	return 1;
}

int wdServerMessage::SendToServer (WDMessages messageType, const char * msgstring) {

        lastmsgtypesent = messageType;
	int rc		= 0;
	int sentlen	= 0;
	int strlength	= 0;
	int msglen	= sizeof(WDMessages);
	char * msg_name = NULL;
	struct msgstruct {
		int	msgT;
		char	buff[WDMSGBUFFSIZE];
	} msgstruct;
	struct msgstruct amsg;
	if (msgstring) strlength = strlen(msgstring);
    if (strlength >= WDMSGBUFFSIZE)
        strlength = WDMSGBUFFSIZE - 1;

	amsg.msgT = messageType;
	if ((messageType <= wdmsgFirst) || (messageType >= wdmsgLast)) {
		fprintf(stderr, "in SendToServer: Unimplemented message: %d\n",
			messageType);
		return 0;
	} else {
		int do_check = wdmsgCheckLength[messageType];
		msg_name = wdmsgNames[messageType];
		// if (!strstr(msg_name, "reply") {
			// got bad request - only replies belong here
		// }
		if (strlength>0) {
			memcpy (amsg.buff, msgstring, strlength);
			amsg.buff[strlength]=0;
			msglen = msglen + strlength;
		}
		if (do_check) {
			if (do_check==1) {
				assert(strlength==0);
			} else if (do_check==2) {
				assert(strlength>0);
			} else {
				assert(0); // should not get here
			}
		}
	}
	if (messageType == wdmsgGetLSreply) {
		/* Special case: not just a simple text message */
//	fprintf(stderr, " msgstring in send_LS_to_Server: %d",*(int *)msgstring );
		rc = send_LS_to_Server( msgFd, messageType, *(int *)msgstring );
	} else {
		rc = writeMsg((const void *)&amsg, msglen, &sentlen);
	} 
	if (rc!=1) {
		fprintf(stderr, "failure: error %d sending %s message to server", rc, msg_name);
		return 0;
	}
	return 1;
}


char *	wdServerMessage::RecvFromWD	() {
	int fd;
	int fdarray[2];		/* for LS */
	int rc = recvMsg(fdarray,1);
	if (rc == 0) {
	    if (msgbytesRead==0) {
		// Found an empty read - treat as end of file/closed socket
		lastmsgtyperecv = msgType = wdmsgEmptyRead;
	    }
	    return NULL;
	}
	lastmsgtyperecv = msgType = *(WDMessages *)msgbuff;
	if ((msgType <= wdmsgFirst) || (msgType >= wdmsgLast)) {
		fprintf(stderr, "in RecvFromWD: Unimplemented message: %d\n",
			msgType);
		return NULL;
	} else {
	    char * msg_name = wdmsgNames[msgType];
		// if (!strstr(msg_name, "reply") {
			// got bad request - only replies belong here
		// }
	    if (msgType == wdmsgGetLSreply) {
		/* get reply: Listen Socket fd */
		// return code value is in msg string
		memcpy(&fd, msgbuff + sizeof(WDMessages), sizeof(fd));
		if (fd == 0) { // No error
			// Expected exactly 1 fd 
			fd = fdarray[0];
		}
		// Here we are casing from integer to char*. It should be fine
		// on 64 bit platform too, as the caller will cast it back to
		// integer.
		return (char *)(size_t) fd;
	    } else if (msgType == wdmsgGetPWDreply) {
		/* returned string is password */
    		msgbuff[msgbytesRead]=0; /* Terminate string in buffer */
		return (char *)(msgbuff+sizeof(WDMessages));
	    } else if (msgType == wdmsgRestartreply) {
		/* returned string is error message if any */
		if (msgbytesRead==sizeof(WDMessages)) return NULL;
    		msgbuff[msgbytesRead]=0; /* Terminate string in buffer */
		return (char *)(msgbuff+sizeof(WDMessages));
	    } else if (msgType == wdmsgGetReconfigStatusreply) {
    		msgbuff[msgbytesRead]=0; /* Terminate string in buffer */
		return (char *)(msgbuff+sizeof(WDMessages));
	    }
	/* OK, no action need for others: just needed for synching up */
	}
	return NULL;
}

char *	wdServerMessage::RecvFromServer	() {
	int fdarray[2];
	int rc = recvMsg(fdarray,1);
	if (rc == 0) {
	    if (msgbytesRead==0) {
		// Found an empty read - treat as end of file/closed socket
		lastmsgtyperecv = msgType = wdmsgEmptyRead;
	    }
	    return NULL;
	}
	lastmsgtyperecv = msgType = *(WDMessages*)msgbuff;
	if ((msgType <= wdmsgFirst) || (msgType >= wdmsgLast)) {
		fprintf(stderr, "in RecvFromServer: Unimplemented message: %d\n",
			msgType);
	} else {
		char * msg_name = wdmsgNames[msgType];
		// if (strstr(msg_name, "reply")) {
			// got bad request replies don't belong here
		// }
		if (msgbytesRead >= sizeof(WDMessages)) {
			msgbuff[msgbytesRead]=0; /* Terminate string in buffer */
		}
		return msgbuff+(sizeof(msgType));
	}
	return NULL;
}

int
wdServerMessage::getFD(void) const
{
    return msgFd;
}

int wdServerMessage::isStatsMessage(void)
{
    if ((getLastMsgType() >= wdmsgStatsMessageBegin) &&
        (getLastMsgType() <= wdmsgStatsMessageLast))
    {
        return 1;
    }
    return 0;
}


#ifdef XP_UNIX

#define SA struct sockaddr
int	ConnectToWDMessaging	(char * UDS_Name)
{
#include <sys/un.h>
	sockaddr_un servaddr;
	/* Connect to the Unix Domain socket */
	int msgFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( msgFd == -1 ) {
		return -1;	// Socket failure
	}
	memset( (char *)&servaddr, 0, sizeof( servaddr ));
	servaddr.sun_family = AF_UNIX;
	strcpy( servaddr.sun_path, UDS_Name );
	if ( connect(msgFd, (SA *)&servaddr, sizeof(servaddr)) < 0) {
		return -2;	// Connect failure
	}
	return msgFd;
}
#endif // XP_UNIX
