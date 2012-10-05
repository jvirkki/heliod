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
// File:    CExecReqPipe.h
//
// Description:
//
//      This header file is used to manage the individual child
//      stub processes; this is an internal header file intended 
//      for use by the ChildExec subsystem only.
//
// Contact:  mike bennett (mbennett@netcom.com)     13Mar98
//

#ifndef __CEXECREQPIPE_H__ 
#define __CEXECREQPIPE_H__ 

//
//  this class encapsulates the access to the subprocesses which exec
//  new processes on our behalf.
//
//  There are a dynamic number of these processes, controlled by the
//  amount of times we initiate a 'connect' to the listening process.
//  Each time we send a request to the stub process, it forks and execs
//  a new cgi process for which we get the stdin and stdout pipe
//  fds back.  Meanwhile, the stub process process goes back to 
//  awaiting a new request.
//
//  As we use stream pipes to talk to the stub, we have to make sure
//  we don't interleave requests to an individual stub.  We control
//  this through the use of two lists used for tracking the states
//  of the stub processes; an idle and an in-use list.  This list
//  is kept in the control block; when our state is changed we 
//  manipulate the control block's lists to insert and remove ourself 
//  appropriately.
//

#include "ChildExec.h"
#include <nspr.h>

//
//  track the current state of a request pipe
//
typedef enum {
    RPST_DEAD   = 0,            // reqpipe isn't usable 
    RPST_IDLE   = 1,            // reqpipe is available for use; is on
                                // the cb's cbRpIdle list
    RPST_INUSE  = 2             // reqpipe has a request in progress; 
                                // is on the cb's cbRpInUse list
} CEXECRPSTATE;

class CExecReqPipe;

struct CRPList {
    CExecReqPipe*   Prev;
    CExecReqPipe*   Next;
};

//
//  ChildExec subsystem control block
//
class CExecReqPipe { 
public:
    //  constructor; requires a build control block and a valid fd
    //  as returned from a listener
    CExecReqPipe( ChildExec& cb, int osFd );

    //  destructor
    //  NOTE: if we're not in the RPST_DEAD state when being deleted,
    //  we assert (probably should throw an exception, but that could
    //  get nasty coming out of a destructor)
    ~CExecReqPipe(void);
private:
    //  these are not allowed currently
    CExecReqPipe(const CExecReqPipe& x);            // copy constructor
    CExecReqPipe& operator=(const CExecReqPipe& x); // '=' operator

public:
    //
    //  accessor methods:
    //      
    ChildExec&      getCB(void)          { return rpExecCB; }
    CEXECRPSTATE    getState()           { return rpMyState; }
    int             getLastIOError()     { return rpLastError; }
    int             getNbrWrites()       { return rpNbrWrites; }
    CExecReqPipe *  getNextIdle()        { return rpIdle.Next; }
    CExecReqPipe *  getNextInUse()       { return rpInuse.Next; }
    PRIntervalTime  getStateChangeTime() { return rpStateChangeTs; }

    //
    //  I/O operations
    //
    //      these return a boolean indication of whether the 
    //      I/O was successful; if not, the child can be
    //      considered dead and should be set 'dead' and deleted
    //
    PRBool  writeMsg( const void * buf, size_t writeLen, int& sentLen );
    PRBool  recvMsg( char * rspBuf, size_t rspBufSz, int& bytesRead, 
                     int fdArray[], int fdArraySz, int& nbrFdsRcvd );

    //
    //  these routines assist in scheduling of request
    //  pipe states; be VERY careful to call the correct one
    //  based upon whether the control block's Lock() method
    //  has been called.  Deadlocks or corruption could occur
    //  if the wrong method is called.  
    //
    //      change state; CB's lock is not currently held
    void  setState_CBUnlocked( CEXECRPSTATE newState ); 
    //
    //      change state, assuming the lock is already held
    void  setState_CBLocked( CEXECRPSTATE newState ); 

private:
    ChildExec&      rpExecCB;       //  ChildExec control block
    CEXECRPSTATE    rpMyState;      //  my current state
    int             rpFd;           //  os fd
    int             rpLastError;    //  last errno
    int             rpNbrWrites;    //  nbr WriteMsgs done 
    CRPList         rpIdle;         //  Idle list
    CRPList         rpInuse;        //  Inuse list
    PRIntervalTime  rpStateChangeTs;//  time of last state change
};

#endif // __CEXECREQPIPE_H__ 
