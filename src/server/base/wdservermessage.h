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
// File:    wdservermessage.h
//
// Description:
//      This header file is used to manage the message traffic
//	between the watchdog and server.
//

#ifndef __WDSERVERMESSAGE_H_ 
#define __WDSERVERMESSAGE_H_ 

//
//  this class encapsulates the access to messages send between the
//  watchdog and server processes.
//
// 	Server requests to watchdog

typedef enum {
	wdmsgFirst,			// unused
	wdmsgGetPWD,			// get Password from terminal
	wdmsgGetPWDreply,		// reply to Password request
	wdmsgGetLS,			// get Listen Socket, return fd
	wdmsgGetLSreply,		// reply to Listen Socket request
	wdmsgCloseLS,			// close Listen Socket
	wdmsgCloseLSreply,		// reply to close Listen Socket
	wdmsgEndInit,			// done with server initialization (can clean
					// 	up watchdog and terminal)
	wdmsgEndInitreply,		// reply to initialization done message
	wdmsgEndInitreplyAck,	// Ackknowlegment to wdmsgEndInitreply
	wdmsgSetPIDpath,		// unused
	wdmsgSetPIDpathreply,		// unused
	wdmsgRestart,			// Admin message to restart servers
	wdmsgRestartreply,		// reply to Admin message to restart servers
	wdmsgTerminate,			// clean shut down from server
	wdmsgTerminatereply,		// reply to server that Terminate received
	wdmsgReconfigure,		// Admin message to start a reconfiguration
	wdmsgReconfigurereply,		// reply to Admin reconfig message
	wdmsgGetReconfigStatus,		// get status from reconfiguration
	wdmsgGetReconfigStatusreply,	// reply to get status reconfig message
	wdmsgReconfigStatus,		// status from server from reconfiguration
	wdmsgReconfigStatusreply,	// reply to status from server
	wdmsgReconfigStatusDone,	// done sending status from reconfiguration
	wdmsgReconfigStatusDonereply,	// reply to done sending status
	wdmsgEmptyRead,			// Empty read receiving msg => closed socket
	wdmsgRotate,			// Admin message to rotate log files
	wdmsgRotatereply,		// reply to Admin rotate message
	wdmsgPeerReconfigure,		// Worker reconfiguration request
	wdmsgPeerReconfigurereply,	// Reply to worker reconfiguration request
	wdmsgPeerReopenLogs,		// Worker reopen request
	wdmsgPeerReopenLogsreply,	// Reply to worker reopen request
	wdmsgIdentifyStatsChannel, // Identify the channel as stats channel
	wdmsgIdentifyStatsChannelAck, // Acknowledgement to wdmsgIdentifyStatsChannel
                                  // return with string (true/false)
	wdmsgStatsMessageBegin,    // Place holder for first stats message (unused)
	wdmsgReqStatsData,     // Stats data request
	wdmsgRespStatsData,     // Stats data response
	wdmsgRespStatsDataAck,     // Stats data Ack
	wdmsgRespError,     // Stats response error
	wdmsgStatsNotification,  // Stats Notification
	wdmsgStatsNotificationAck,  // Stats Notification Acknowledgement
	wdmsgStatsMessageLast,   // unused.
	wdmsgLast			// unused
} WDMessages;

#define WDMSGBUFFSIZE	4096

#ifdef XP_UNIX
#define WDSOCKETNAME	PRODUCT_WATCHDOG_BIN".socket"

extern int ConnectToWDMessaging	(char * UDS_Name);

class wdServerMessage { 

  public:
    //  constructor; requires a valid fd as returned from a listener
    wdServerMessage(int osFd );
    virtual ~wdServerMessage(void);

    int		getLastIOError()	{ return msgLastError;	}
    int		getNbrWrites()		{ return msgNbrWrites;	}
    WDMessages	getLastMsgType()	{ return msgType;	}
    int getRecvBytes() { return msgbytesRead; }

    //	These functions are used by the server to talk to the watchdog
    //
    int		SendToWD	(WDMessages msgtype, const char * msgstring);
    char *	RecvFromWD	();
    //
    //	These functions are used by the watchdog to talk to the server
    //
    int		SendToServer	(WDMessages msgtype, const char * msgstring);
    virtual char *	RecvFromServer	();


    // Returns the underlying handle to the socket
    int getFD(void) const;

    // Sets msgFd to an invalid value so that the descriptor will NOT
    // be closed in the destructor
    void invalidate(void);

    // Return the message header size. Right now it is only type of message.
    int getMessageHeaderSize() { return sizeof(WDMessages); }

    // Return 1 if the message is a stats message.
    int isStatsMessage(void);

  private:
    // Let the msgbuff be the first member of the class.
    // This will make msgbuff pointer boundary aligned and 
    // we can read/write structures in msgbuff with proper
    // alignment. If msgbuff for some platform is not aligned on
    // pointer boundary then STATS_BUFFER_ALIGNMENT_OK must be
    // undefined in iwsstats.h for that platform.
    char	msgbuff[WDMSGBUFFSIZE];	//  recvmsg buffer
    int		msgFd;			//  os fd
    int		msgLastError;		//  last errno
    int		msgNbrWrites;		//  number of WriteMsgs done 
    int		msgbytesRead;		//  number of bytes read
    int		msgNfds;		//  number of FDs received

    WDMessages	msgType;		//  Type from last message received
    //  I/O operations
    //      these return a boolean indication of whether the 
    //      I/O was successful; if not, the child can be
    //      considered dead and should be set 'dead' and deleted
    //
    int send_LS_to_Server	( int connfd, WDMessages msg_type, int ls_fd );
  protected: 
    // Derive classes e.g StatsServerMessage need to access these functions
    int	recvMsg			( int fdArray[], int fdArraySz);
    int	writeMsg		( const void * buf, int writeLen, int * sentLen );
    int recvMessageLength( int* dataLength);
    int sendMessageLength( int dataLength);
    char* getMessageBuffer() { return msgbuff; }
};

// On windows wdServerMessage functionality is implemented by NTServerMessage
// This typedef is to use to make both classes similar to be used by derive
// classes.
typedef wdServerMessage StatsServerMessageBase;

// For Linux, do we need to need to define StatsFileHandle as void* ??
// It may have trouble in 64 bit Linux in future. xerces defines the
// handle as void* ??
typedef int StatsFileHandle;

#endif // XP_UNIX

#endif // __WDSERVERMESSAGE_H_ 

