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
 * ChildExec.cpp: implementation of the child exec subsystem API library
 *
 *  This implements the cgi engine interface library, as defined in
 *  ChildExec.h.  
 *
 *  Mike Bennett  (mbennett@netcom.com)      13Mar98
 */

/*
 * TODO:
 * - some clever way to remove a potential security hole wherein
 *   someone armed with source code could construct an app which
 *   talks directly to the child stub process and get it to run
 *   processes under the euid of the server.  I think this basically
 *   is taken care of by the CGI subsystem itself as the daemon, I
 *   believe, executes under a configured (nobody?) userid to get
 *   around potentially nasty CGI scripts
 */

/* VB:
 * Linux expects the thread that forked a child to be the same thread that 
 * reaps it.
 * This necessitates us to have a separate thread to do the fork and waitpid
 * for the listener cgistub.
 * Note that if the listener dies, on linux, it will show up as a defunct 
 * process until we try to talk to it and then the connect will fail
 * causing us to reap the defunct process thru a waitpid and create a 
 * new listener all over again
 */


//  default tuning variables; specified this way to allow
//  overriding in the makefile
#ifndef DEFAULTMINCHILDREN
#define DEFAULTMINCHILDREN 2
#endif
#ifndef DEFAULTMAXCHILDREN
#define DEFAULTMAXCHILDREN 10
#endif 

//  time after which a reaper will awake and look around
#ifndef DEFAULTREAPINTERVAL
#define DEFAULTREAPINTERVAL PR_SecondsToInterval(30)
#endif 

//  # of children (max) the reaper thread will get rid of each check
//  this is to minimize the time spent holding the reaper's lock
#ifndef REAPLIMITPERCHECK
#define REAPLIMITPERCHECK   2
#endif 

#ifndef CGISTUB_PREFIX
#define CGISTUB_PREFIX "/.cgistub_"
#endif

#include <sys/types.h>
#if defined(USE_CONNLD)
#include <sys/stat.h>
#include <stropts.h>
#else
#include <sys/un.h>
#endif
#include <assert.h>
#include <signal.h>

#include "ChildExec.h"
#include "CExecReqPipe.h"

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

#ifdef SERVER_BUILD
#include "base/systems.h"
#include "base/pblock.h"
#include "base/session.h"
#include "frame/conf_api.h"
#include "frame/req.h"
#include "frame/log.h"
#include "frame/httpact.h"  /* for servact_translate_uri */

#include "safs/dbtsafs.h"
#include "base/util.h"      /* environment functions, can_exec */
#include "base/net.h"       /* SYS_NETFD */
#include "base/buffer.h"    /* netbuf */
#include "frame/protocol.h" /* protocol_status */
#include "frame/http.h"     /* http_hdrs2env */
#include "frame/conf.h"     /* server action globals */
#include "ssl.h"
#include "libaccess/nsauth.h"
#include "ldaputil/certmap.h"
#include "Cgistub.h"
#else // SERVER_BUILD
#include <nspr.h>
#include <nsapi.h>
#include <Cgistub.h>
#endif // SERVER_BUILD


static int cs_add_tlv_hdr( void * ptlv_area, creq_tlv_type_e type, int len, 
                           const char * vector );
static char * cs_build_varargs_array( int nargs,
                                      const char * const * pparg_save,
                                      int * parray_len );

PRBool ChildExec::_terminating = PR_FALSE;
//
//  static method for error handling
//
const char *
ChildExec::csErrorMsg( CHILDEXEC_ERR cserr, int ioerr,
                       char * workBuf, size_t workBufsz )
{
    int          perr = 0;
    const char * strMsg  = NULL;
    const char * pErrMsg = NULL;
    char         temp[30];

    switch( cserr ) {
        case CERR_OK            : strMsg = "No Error";  perr++; break;
        case CERR_BADPARAM      : strMsg = "Bad Parameter"; break;
        case CERR_MEMFAILURE    : strMsg = "Memory Allocation Failure"; break;
        case CERR_UNINIT        : strMsg = "Subsys Not Initialized"; break;
        case CERR_ALREADY       : strMsg = "Subsys Already Initialized"; break;
        case CERR_BADREQPIPE    : strMsg = "Bad Request Pipe"; break;
        case CERR_BADPATH       : strMsg = "Bad Child Path"; break;
        case CERR_BADARGV       : strMsg = "Invalid argv array"; break;
        case CERR_BADENV        : strMsg = "Invalid env array"; break;
        case CERR_REQFAILURE    : strMsg = "Request pipe I/O failed"; perr++; break;
        case CERR_NOLISTENER    : strMsg = "No listener process"; break;
        case CERR_LISTENERBUSY  : strMsg = "Listener process is busy"; break;
        case CERR_CHDIRFAILURE  : strMsg = "chdir() failure"; perr++; break;
        case CERR_CHROOTFAILURE : strMsg = "chroot() failure"; perr++; break;
        case CERR_FORKFAILURE   : strMsg = "fork() failure"; perr++; break; 
        case CERR_EXECFAILURE   : strMsg = "exec() failure"; perr++; break;
        case CERR_RESOURCEFAIL  : strMsg = "resource [pipe()] failure"; perr++; break;
        case CERR_USERFAILURE   : strMsg = "Unable to set user"; break;
        case CERR_GROUPFAILURE  : strMsg = "Unable to set group"; break;
        case CERR_STATFAILURE   : strMsg = "stat() failure"; perr++; break;
        case CERR_BADDIRPERMS   : strMsg = "Cgistub is marked as set user ID on execute and its directory is accessible by other users"; break;
        case CERR_BADEXECPERMS  : strMsg = "CGI program is writable by other users"; break;
        case CERR_BADEXECOWNER  : strMsg = "CGI program is not owned by this user"; break;
        case CERR_SETRLIMITFAILURE: strMsg = "setrlimit() failure"; perr++; break;
        case CERR_NICEFAILURE   : strMsg = "nice() failure"; perr++; break;
        case CERR_INTERNALERR   : strMsg = "Internal Error"; perr++; break;
        default:
            sprintf( temp, "Unknown CSERR(%d)", cserr );
            strMsg = temp;
            break;
    }
    if ( perr ) {
        pErrMsg = strerror( ioerr );
    }
    size_t needed_len = strlen( strMsg ) + 10; // allow some room
    if ( pErrMsg ) {
        needed_len += strlen( pErrMsg );
    }
    if ( needed_len < workBufsz ) {
        if ( pErrMsg ) {
            sprintf( workBuf, "%s [%s]", strMsg, pErrMsg );
        } else {
            sprintf( workBuf, "%s", strMsg );
        }
        return workBuf;
    } else {
        return "ChildExec::csErrorMsg -- ERROR: error message buffer too small";
    }
}

//
//  Construction
//
ChildExec::ChildExec(const char * stubExecProgram)
: cbRpIdle(NULL), cbRpInuse(NULL), cbListenFd(-1), cbListenPid(0),
  cbNbrPipesIdle(0), cbNbrPipes(0),
  cbPipesCreating(0),
  cbMinChildren( DEFAULTMINCHILDREN ),
  cbMaxChildren( DEFAULTMAXCHILDREN ),
  cbIdleReapInt( DEFAULTREAPINTERVAL ),
  cbReaperThread( NULL ),
  cbReaperRunning( PR_FALSE ),
  _listenerLock(NULL),
#if defined(LINUX)
  cbListenerThread(NULL),
  _listenerCvar(NULL), _revListenerCvar(NULL),
#endif
  _ioerr(0)
{
    CHILDEXEC_ERR cserr = CERR_OK;

    cbLock = PR_NewLock(); 
    assert( cbLock != NULL );

    cbMoreChildLock = PR_NewLock(); 
    assert( cbMoreChildLock != NULL );
    cbMoreChildCV = PR_NewCondVar( cbMoreChildLock ); 
    assert( cbMoreChildCV != NULL );

    cbReaperLock = PR_NewLock(); 
    assert( cbReaperLock != NULL );
    cbReaperCV = PR_NewCondVar( cbReaperLock ); 
    assert( cbReaperCV != NULL );

    cbExecPath = new char [strlen (stubExecProgram) + 1 ];
    strcpy( cbExecPath, stubExecProgram );

    //  construct a unique name using my pid 
    const char* path = system_get_temp_dir();
    cbSockPath = new char [strlen(path) + sizeof(CGISTUB_PREFIX) + sizeof("4294967296")];
    sprintf( cbSockPath, "%s"CGISTUB_PREFIX"%d", path, getpid() );

    _listenerLock = PR_NewLock();
#if defined(LINUX)
    _listenerCvar = PR_NewCondVar(_listenerLock);
    _revListenerCvar = PR_NewCondVar(_listenerLock);
    cbListenerThread = 0; 
#endif
}

// destructor
ChildExec::~ChildExec(void)
{
    Terminate();

    delete [] cbExecPath;
    delete [] cbSockPath;
    
    Unlock();

#ifdef CLEAN_SHUTDOWN
    PR_DestroyCondVar( cbReaperCV );
    PR_DestroyLock( cbReaperLock );
    PR_DestroyCondVar( cbMoreChildCV );
    PR_DestroyLock( cbMoreChildLock );
    PR_DestroyLock( cbLock );
#endif
}

void
ChildExec::Terminate()
{
    unlink(cbSockPath);

    stopReaper();

    Lock();
    
    CExecReqPipe * rqp;

    while ( (rqp = cbRpInuse )) {
        rqp->setState_CBLocked( RPST_DEAD );
        delete rqp;
    }
    if ( cbListenFd > 0 ) {
        int ioerr = 0;
        CHILDEXEC_ERR res = ShutdownListener(ioerr);
    }
}

//
//  lock and release locks on the control block
//
void ChildExec::Lock()
{
    PR_Lock( cbLock );
}

void ChildExec::Unlock()
{
    PR_Unlock( cbLock );
}

//
//  public accessors
//
void ChildExec::setMinChildren( int nbr )
{
    int  bump_max = 0;

    Lock();
    cbMinChildren = nbr;
    // adjust the max to be no less than the min
    if ( cbMinChildren > cbMaxChildren )
        bump_max = 1;
    Unlock();
    if ( bump_max ) 
        setMaxChildren( nbr );
}

void ChildExec::setMaxChildren( int nbr )
{
    Lock();
    int prevMax = cbMaxChildren;
    cbMaxChildren = nbr;
    // adjust the min to be no more than the max
    // do it directly to reduce the chance of some pathological case
    if ( cbMaxChildren < cbMinChildren ) {
         cbMinChildren = cbMaxChildren;
    }
    Unlock();
    
    //
    //  if we increased the count, whack the waiters so they can 
    //  create new kids
    //
    if ( nbr > prevMax ) {
        int     diff = nbr - prevMax;
        //  note the semantics are that there are more children's
        //  slots available; the wait loop in getIdlePipe will
        //  create more kids; we just have to wake up enough
        //  waiters
        //
        while ( diff-- ) {
            notifyMoreChildren();
        }
    }
}

void ChildExec::setIdleChildReapInterval( PRIntervalTime interval )
{
    Lock();
    cbIdleReapInt = interval;
    Unlock();
    wakeupReaper();
}

//
//  reaper thread handling; manage a background child
//  reaper that periodically kills idle kids
//
void ChildExec::stopReaper()
{
    PR_Lock( cbReaperLock );
    if ( PR_TRUE == cbReaperRunning ) {
        cbReaperRunning = PR_FALSE;
        PR_NotifyCondVar( cbReaperCV );
        PR_Unlock( cbReaperLock );
        PR_JoinThread( cbReaperThread );
        cbReaperThread = NULL;
    } else {
        PR_Unlock( cbReaperLock );
    }
}

void ChildExec::wakeupReaper()
{
    PR_Lock( cbReaperLock );
    if ( PR_TRUE == cbReaperRunning ) {
        PR_NotifyCondVar( cbReaperCV );
    }
    PR_Unlock( cbReaperLock );
}

void ChildExec::startReaper()
{
    PR_Lock( cbReaperLock );
    if ( PR_TRUE == cbReaperRunning ) {
        PR_Unlock( cbReaperLock );
        return;
    }

    cbReaperThread = PR_CreateThread( PR_SYSTEM_THREAD, 
                                      reaperThreadEntry, (void *)this,
                                      PR_PRIORITY_NORMAL,
                                      PR_LOCAL_THREAD, 
                                      PR_JOINABLE_THREAD, 0 );
    if ( cbReaperThread == NULL ) {
#ifdef CGISTUB_DEBUG
        cerr << "ChildExec::startReaper -- can't create a reaper thread"
             << endl;
#endif // CGISTUB_DEBUG
        cbReaperRunning = PR_FALSE;
    } else {
        cbReaperRunning = PR_TRUE;
    }

    // note that until we unlock this the new thread isn't reaping
    PR_Unlock( cbReaperLock );
}

//
//  main reaper thread starting point; the void * pointer contains a 
//  'this' pointer we resolve
//
void ChildExec::reaperThreadEntry( void * cbPtr )
{
    ChildExec * cb = (ChildExec *)cbPtr;
    cb->reaperMainLoop();
}

//
//  reaper thread's main loop; until we're told to go away
//  wait on our condition variable and, when we wake up
//  look for idle request pipes
//
void ChildExec::reaperMainLoop()
{
    int             childrenKilled = 0;
    PRIntervalTime  childReapEpoch;     // we reap all idle children that haven't
                                        // changed state since this time
    CExecReqPipe *  rqp;
    CHILDEXEC_ERR   cserr;
    int             ioerr;

    PR_Lock( cbReaperLock );

    while ( PR_TRUE == cbReaperRunning ) {
        PR_WaitCondVar( cbReaperCV, cbIdleReapInt );
        if ( cbReaperRunning == PR_FALSE ) {
            break;
        }
        Lock();

        //  if there aren't more than minchildren going, don't bother
        if ( ( cbNbrPipes + cbPipesCreating ) <= cbMinChildren ) {
            int     diff = cbMinChildren - ( cbNbrPipes + cbPipesCreating );
            //  keep minchildren going
            while ( diff-- > 0 ) {
                // unlock, as this call could wait awhile
                cbPipesCreating++;
                Unlock();
                cserr = createRequestPipe( rqp, ioerr );
                Lock();
                cbPipesCreating--;
                if ( cserr == CERR_OK ) {
                    rqp->setState_CBLocked( RPST_IDLE );
                } else if ( cserr == CERR_NOLISTENER ) {
                    log_ereport(LOG_WARN, XP_GetAdminStr(DBT_ChildExecNOLISTENER));
                    if (( cserr = StartListener( ioerr )) != CERR_OK ) {
#ifdef CGISTUB_DEBUG
                        cerr << "reaperMainLoop -- cserr "
                             << cserr << ", I/O error "
                             << ioerr << ", starting listener" << endl;
#endif //  CGISTUB_DEBUG
                        // TODO log an error ?
                    }
                } else if ( cserr == CERR_LISTENERBUSY ) {
                    log_ereport(LOG_WARN, XP_GetAdminStr(DBT_ChildExecLISTENERBUSY));
                    break;
                }
            } 
            Unlock();
            continue;
        }

        //
        //  right now, just look for children that have been in idle
        //  state since before our epoch
        //
        childReapEpoch = PR_IntervalNow() - cbIdleReapInt;
        childrenKilled = 0;
        PRBool checkedAll = PR_FALSE;
        while ( childrenKilled < REAPLIMITPERCHECK &&
                cbNbrPipes > cbMinChildren &&
                PR_FALSE == checkedAll ) {
            checkedAll = PR_TRUE; // assume we make it
            for ( rqp = cbRpIdle; rqp; rqp = rqp->getNextIdle() ) {
                if ( rqp->getStateChangeTime() < childReapEpoch ) {
                    rqp->setState_CBLocked( RPST_DEAD );
                    //  note; the for loop is broken now; just start again
                    delete rqp;
                    childrenKilled++;
                    checkedAll = PR_FALSE;
                    break;
                }
            }
        }
        Unlock();
    }
    PR_Unlock( cbReaperLock );
}


//
//  throttle based on # of idle children
//
void ChildExec::notifyMoreChildren()
{
    PRStatus stat;
    PR_Lock( cbMoreChildLock );
    stat = PR_NotifyCondVar( cbMoreChildCV );
    assert( stat == PR_SUCCESS );
    PR_Unlock( cbMoreChildLock );
}

void ChildExec::waitForMoreChildren()
{
    PRStatus stat = PR_FAILURE;
    PR_Lock( cbMoreChildLock );

    while ( stat != PR_SUCCESS ) {
        stat = PR_WaitCondVar( cbMoreChildCV, PR_INTERVAL_NO_TIMEOUT );
    }
    PR_Unlock( cbMoreChildLock );
}

//
//  public methods:
//      start a program and return the fds of stdin & stdout
//      
CHILDEXEC_ERR ChildExec::initialize( int& ioerr )
{
    CHILDEXEC_ERR  cserr;

    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_MinCGIStubsSetTo), 
                cbMinChildren);
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_MaxCGIStubsSetTo), 
                cbMaxChildren);
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_CGIStubIdleTimeoutSetTo),
                PR_IntervalToMilliseconds(cbIdleReapInt));

    Lock();
#if defined(LINUX)
    if (cbListenerThread == NULL) {
        PR_Lock(_listenerLock);
        if (cbListenerThread == NULL) {
            cbListenerThread = PR_CreateThread(PR_SYSTEM_THREAD, 
                                               ListenerOpThreadStart, 
                                               (void *)this,
                                               PR_PRIORITY_NORMAL,
                                               PR_LOCAL_THREAD, 
                                               PR_JOINABLE_THREAD, 0 );
            if (cbListenerThread) {
                PR_WaitCondVar(_revListenerCvar, PR_INTERVAL_NO_TIMEOUT);
            }
            else {
                cserr = CERR_NOLISTENER;
            }
        }
        PR_Unlock(_listenerLock);
    }
#endif

    ioerr = 0;

    //
    // start the listener if necessary
    //
    if ( cbListenPid == 0 ) {
        if (( cserr = StartListener( ioerr )) != CERR_OK ) {
            Unlock();
            return cserr;
        }
    }

    //
    //  attempt to crank up minchildren
    //
    if ( (cbNbrPipes + cbPipesCreating ) < cbMinChildren ) {
        int     ndx;
        for ( ndx = cbMinChildren - ( cbNbrPipes + cbPipesCreating ); 
              ndx > 0; ndx-- ) {
            CExecReqPipe * rqp;
            cbPipesCreating++;
            Unlock();
            cserr = createRequestPipe( rqp, ioerr );
            Lock();
            cbPipesCreating--;
            if ( cserr == CERR_OK ) {
                rqp->setState_CBLocked( RPST_IDLE );
            } else if ( cserr == CERR_LISTENERBUSY ) {
                continue;
            }
        }
    }

    //
    //  start the reaper thread if necessary
    //
    if ( PR_FALSE == cbReaperRunning ) {
        startReaper();
    }
    Unlock();
    return CERR_OK;
}

//
//  public methods:
//      start a program and return the fds of stdin & stdout
//      
CHILDEXEC_ERR ChildExec::exec( cexec_args_t& execargs, int& ioerr )
{
    CHILDEXEC_ERR       cserr;
    cstub_start_rsp_t   crsp;
    char *              cPtr = NULL;
    size_t              msgSize;
    CExecReqPipe *      rqp = NULL;

    //
    //  build the request, then find a pipe to use
    //
    if ( ! buildStartRequest( cPtr, msgSize, execargs )) {
        return CERR_BADPARAM;
    }
    if (( cserr = getIdlePipe( rqp, ioerr )) != CERR_OK ) {
        FREE(cPtr);
        return cserr;
    }

    //  
    //  send a message to the pipe
    //
    int  nSent;
    if ( rqp->writeMsg( cPtr, msgSize, nSent ) == PR_FALSE ) {
        ioerr = rqp->getLastIOError();
        rqp->setState_CBUnlocked( RPST_DEAD );
        delete rqp;
        FREE(cPtr);
        return CERR_REQFAILURE;
    }

    //  
    //  get the response
    //
    int    nRead      = 0;
    int    nbrFdsRcvd = 0;
    PRBool stubBad    = PR_FALSE;
    int    fdArray[3];
    cserr = CERR_OK;
    if ( rqp->recvMsg( (char *)&crsp, sizeof( crsp ), nRead, 
                       fdArray, 3, nbrFdsRcvd ) == PR_FALSE ) {
        ioerr = rqp->getLastIOError();
        if ( ioerr == EMFILE ) {
            //
            //  if we are at our max-descriptors and the recvmsg
            //  fails w/ EMFILE, that means we tried to exceed our
            //  max descriptors.  This means the child stub is OK,
            //  but the fd pass failed.  This (supposedly) is
            //  invisible to the stub, and the OS silently 
            //  closes the fds.
            //
            //  overwrite what it said with our interpretation
            //
            cserr = CERR_RESOURCEFAIL;
        } else {
            //  the stub is probably bad; kill it
            stubBad = PR_TRUE;
            cserr   = CERR_REQFAILURE;
        }
    }

    //  
    //  return the pipe to the idle list OR delete it if it's broken 
    //
    if ( PR_TRUE == stubBad ) {
        rqp->setState_CBUnlocked( RPST_DEAD );
        delete rqp;
    } else {
        rqp->setState_CBUnlocked( RPST_IDLE );
    }
    FREE(cPtr);
    if ( cserr != CERR_OK ) {
        return cserr;
    }
    assert( nRead == sizeof( crsp ));

    //
    //  validate the fds returned
    //
    if ( crsp.crsp_rspinfo == CRSP_OK && nbrFdsRcvd == 3 ) {
        execargs.cs_stdin  = fdArray[0];
        execargs.cs_stdout = fdArray[1];
        execargs.cs_stderr = fdArray[2];
        execargs.cs_pid    = crsp.crsp_pid;
        ioerr              = 0;
    } else {
        switch ( nbrFdsRcvd ) {
            case 3: close( fdArray[2] );
            case 2: close( fdArray[1] );
            case 1: close( fdArray[0] );
        }
        execargs.cs_pid = -1;
        CHILDEXEC_ERR rcode;
        switch( crsp.crsp_rspinfo ) {
            case CRSP_BADMSGTYPE:
            case CRSP_BADVERSION:
            case CRSP_MISSINGINFO:
                fprintf( stderr, "failure - incorrect rcode (%d)\n", 
                         crsp.crsp_rspinfo );
                rcode = CERR_INTERNALERR;
                break;
            case CRSP_CHDIRFAIL:
                rcode = CERR_CHDIRFAILURE;
                break;
            case CRSP_CHROOTFAIL:
                rcode = CERR_CHROOTFAILURE;
                break;
            case CRSP_FORKFAIL:
                rcode = CERR_FORKFAILURE;
                break;
            case CRSP_EXECFAIL:
                rcode = CERR_EXECFAILURE;
                break;
            case CRSP_RESOURCE:
                rcode = CERR_RESOURCEFAIL;
                break;
            case CRSP_USERFAIL:
                rcode = CERR_USERFAILURE;
                break;
            case CRSP_GROUPFAIL:
                rcode = CERR_GROUPFAILURE;
                break;
            case CRSP_STATFAIL:
                rcode = CERR_STATFAILURE;
                break;
            case CRSP_EXECPERMS:
                rcode = CERR_BADEXECPERMS;
                break;
            case CRSP_EXECOWNER:
                rcode = CERR_BADEXECOWNER;
                break;
            case CRSP_SETRLIMITFAIL:
                rcode = CERR_SETRLIMITFAILURE;
                break;
            case CRSP_NICEFAIL:
                rcode = CERR_NICEFAILURE;
                break;
            default:
                rcode = CERR_INTERNALERR;
                fprintf( stderr, "failure - bad rcode (%d)\n", rcode );
                break;
        }
        ioerr = crsp.crsp_errcode;
        return rcode;
    }

    return CERR_OK;
}

//
//  getIdlePipe - get an idle pipe for use; if there's none
//  available, make one
//
CHILDEXEC_ERR ChildExec::getIdlePipe( CExecReqPipe *& rqp, int& ioerr )
{
    CHILDEXEC_ERR     cserr;

    //
    //  make sure the listener is running before manipulating
    //  pipes
    //  Note: this should only happen once per instance
    //
    if ( cbListenPid == 0 ) {
        initialize(ioerr);
    }

    // 
    //  note on change-state usage; we have to do it here rather than
    //  returning it back to the caller to do this, as there would
    //  be a race condition between taking the list head and then
    //  having the cb's lock taken.  So, the rqp will always be
    //  put into INUSE state before we return 
    //

    for (;;) {
        // 
        //  This will be the main code path; try to get a pipe 
        //  from the control block's idle list 
        //
        Lock();
        if ( (rqp = cbRpIdle) != NULL ) {
            rqp->setState_CBLocked( RPST_INUSE );
            Unlock();
            return CERR_OK;
        }
                
        //  
        //  if we are throttled by maxpipes (ie, we cannot start 
        //  creating another child), then wait for a new
        //  pipe to go idle, else create a brand new pipe
        //
        if ( ( cbNbrPipes + cbPipesCreating + 1 ) >= cbMaxChildren ) {
            Unlock();
            waitForMoreChildren();
        } else {
            //
            // unlock, as this call could wait awhile
            //
            cbPipesCreating++;
            Unlock();
            cserr = createRequestPipe( rqp, ioerr );
            Lock();
            cbPipesCreating--;
            if ( cserr != CERR_OK ) {
		if ( cserr == CERR_LISTENERBUSY ) {
			// Solaris 2.5.1 returns ECONNREFUSED even if listener process
			// is dead. So we send a kill to the listener process with SIG 0
			// to determine for sure if the listerer process is gone. If 
			// listener is dead map cserr to CERR_NOLISTERER and then follow
			// through.
      int status = 0;
      pid_t deadPid = waitpid(cbListenPid, &status, WNOHANG);
      if (deadPid == cbListenPid) {
        cserr = CERR_NOLISTENER;
      }
      else if (errno == ECHILD) {
        cserr = CERR_NOLISTENER;
      }
      else {
			  int ret = kill(cbListenPid, 0);
			  if (ret) 
			     cserr = CERR_NOLISTENER;
      }
		}
                // TODO - we either return an error, or wait for a
                //   pipe to go idle.  If any are in use, then wait
                //   for them here; 
                // NOTE - there's probably a weird race condition here
                // but I think the net effect would be that that we'd
                // have too many (or few) children, which the reaping
                // should take care of 
                if ( cserr == CERR_NOLISTENER ) {
                    if ( (cserr = StartListener( ioerr )) != CERR_OK ) {
                            // real problem here...
                        Unlock();
                        return cserr;
                    }
                    Unlock();
                    continue;
                } else {
                    if ( cbNbrPipes > 0 ) {
                        Unlock();
                        waitForMoreChildren();
                        continue; // back to for(;;) loop
                    }
                }
                Unlock();
                return cserr;
            }
            rqp->setState_CBLocked( RPST_INUSE );
            Unlock();
            return CERR_OK;
        }
    }
}


CHILDEXEC_ERR ChildExec::StartListener(int& ioerr)
{
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_ChildExecStartListener));
    CHILDEXEC_ERR res = PerformListenerOp(LISTENER_START, ioerr);
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_ChildExecExitStartListener),
                res, ioerr);
    return res;
}

CHILDEXEC_ERR ChildExec::ShutdownListener(int& ioerr)
{
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_ChildExecShutdownListener));
    CHILDEXEC_ERR res = PerformListenerOp(LISTENER_SHUTDOWN, ioerr);
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_ChildExecShutdownListenerAfterPerformListenerOp));
#if defined(LINUX)
    if (cbListenerThread) {
        PR_JoinThread(cbListenerThread);
    }
    cbListenerThread = 0;
#endif
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_ChildExecExitShutdownListener));
    return res;
}

CHILDEXEC_ERR ChildExec::PerformListenerOp(ListenerOp op, int& ioerr)
{
    CHILDEXEC_ERR res;
    PR_Lock(_listenerLock);
#if defined(LINUX)
    _listenerOp = op;
    PR_NotifyCondVar(_listenerCvar);
    PR_WaitCondVar(_revListenerCvar, PR_INTERVAL_NO_TIMEOUT);
    ioerr = _ioerr;
    res = _res;   
#else
    _listenerOp = op;
    _performListenerOp();
    res = _res;
    ioerr = _ioerr;
#endif
    PR_Unlock(_listenerLock);
    return res;
}

#if defined(LINUX)
void
ChildExec::ListenerOpThreadStart(void* arg)
{
    PR_ASSERT(arg);
    ChildExec* me = (ChildExec*)arg;

    me->ListenerThreadMainLoop();
}

void
ChildExec::ListenerThreadMainLoop()
{
    PRBool needToExit = PR_FALSE;
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_ChildExecListenerThreadMainLoop), getpid());
    PR_Lock(_listenerLock);
    // This notify is for the first time and should wake up the
    // corresponding wait in the ::initialize method
    PR_NotifyCondVar(_revListenerCvar);
    while (needToExit == PR_FALSE) {
        PR_WaitCondVar(_listenerCvar, PR_INTERVAL_NO_TIMEOUT);
        if (_listenerOp == LISTENER_SHUTDOWN) {
            needToExit = PR_TRUE;
        }
        _performListenerOp();
        PR_NotifyCondVar(_revListenerCvar);
    }
    PR_Unlock(_listenerLock);
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_ChildExecExitListenerThreadMainLoop));
}
#endif
    
CHILDEXEC_ERR ChildExec::_performListenerOp()
{
    CHILDEXEC_ERR res = CERR_OK;
    if (cbListenPid > 0) {
        close( cbListenFd );
        cbListenFd = -1;
        util_waitpid( cbListenPid, NULL, 0 );
        cbListenPid = 0;
    }
    if (_listenerOp == LISTENER_START) {
        if (_terminating != PR_TRUE) {
            res = _startListener();
        }
        else {
            res = CERR_UNINIT;
        }
    }
    else {
        //shutting down listener
        res = CERR_OK;
        _terminating = PR_TRUE;
    }

    return res;
}
 
    
        

//
//  start a listener process
//  This assumes the necessary locks are held (in the control block)
//
CHILDEXEC_ERR ChildExec::_startListener()
{
    struct stat     st;
    int             pid;
    int             pip[2];
    int             pipette[2];
    int             errcode;
    int             len;

    _res = CERR_OK;


    //
    //  make sure the Cgistub has been properly secured
    //  Specifically, if it's suid, it must be in a directory owned by us and
    //  inaccessible to everyone else
    //
    if ( stat(cbExecPath, &st) ) {
        _ioerr = errno;
        _res = CERR_STATFAILURE;
        return _res;
    }
    if ( st.st_mode & S_ISUID ) {
        // dirCgistub is cbExecPath with any trailing '/'s stripped
        char* dirCgistub = STRDUP(cbExecPath);
        char* t;
        for (t = dirCgistub + strlen(dirCgistub) - 1; (t > dirCgistub) && (*t == '/'); t--)
        {
            *t = '\0';
        }

        // remove the tail path component (i.e. "/Cgistub") from dirCgistub
        t = strrchr(dirCgistub, '/');
        if (t) *t = '\0';

        // check permissions on dirCgistub, the parent directory of Cgistub
        if ( stat(dirCgistub, &st) ) {
            _ioerr = errno;
            _res = CERR_STATFAILURE;
        } else if ( st.st_uid != getuid() ) {
            // we don't own the Cgistub parent directory
            _res = CERR_BADDIRPERMS;
        } else if ( st.st_mode & (S_IRWXG | S_IRWXO) ) {
            // someone other than us can access the Cgistub parent directory
            _res = CERR_BADDIRPERMS;
        }

        FREE(dirCgistub);

        if (_res) return _res;
    }

    //
    //  create an internal pipe to monitor the fork & exec; enable
    //  close-on-exec so the parent can monitor the child.  Also,
    //  create a pipe corresponding to stdin of the listener so
    //  it can tell when we want it to go away (by closing it's fd)
    //
    if ( pipe(pip) == -1 ||
         socketpair(AF_UNIX, SOCK_STREAM, 0, pipette) == -1 )
    {
        _ioerr = errno;
        fprintf( stderr, "failure - pipe creation ioerr (%d)\n", _ioerr );
        _res = CERR_INTERNALERR;
        return _res;
    }
    // pip is used to detect exec failure only 
    fcntl( pip[0], F_SETFD, FD_CLOEXEC );
    fcntl( pip[1], F_SETFD, FD_CLOEXEC );

    //
    //  fork/exec sequence; as child, send a status back to the parent
    //  if the exec failed
    //
#if defined(SOLARIS)
    if ( (pid = fork1() ) == -1 ) {
#else
    if ( (pid = fork() ) == -1 ) {
#endif
        _ioerr = errno;
#ifdef CGISTUB_DEBUG
#if defined(SOLARIS)
        cerr << "ChildExec::startListener - error on fork1(), error " 
#else
        cerr << "ChildExec::startListener - error on fork(), error " 
#endif
             << _ioerr << ", aborting request" << endl;
#endif //  CGISTUB_DEBUG
        close( pip[0] );
        close( pip[1] );
        close( pipette[0] );
        close( pipette[1] );
        _res = CERR_FORKFAILURE;
        return _res;
    }
    if ( pid == 0 ) {
        close( pip[0] );
        close( pipette[0] );
        if ( pipette[1] != STDIN_FILENO ) {
            dup2( pipette[1], STDIN_FILENO );
            close( pipette[1] );
        }
        execl( cbExecPath, cbExecPath, "-f", cbSockPath, NULL );
        //
        //  it failed if we're here
        //
        errcode = errno;
#ifdef CGISTUB_DEBUG
        cerr << "ChildExec::startListener - error on execl(), error " 
             << errcode << ", path was " 
             << cbExecPath << ", sockname "
             << cbSockPath << "." << endl;
#endif //  CGISTUB_DEBUG
        write( pip[1], (char *)&errcode, sizeof( errcode ) );
        _exit(1);
    }
        
    // 
    //  we're the parent; wait for the child to exec - we detect this 
    //  by the pipe's closing
    //
    close( pip[1] );
    close( pipette[1] );

    len = read( pip[0], (char *) &errcode, sizeof( errcode ));
    close( pip[0] );
    if ( len == sizeof( errcode ) ) {
        // exec failure; errcode contains the error 
        close( pipette[0] );
        _ioerr = errcode;
        _res = CERR_EXECFAILURE;
        return _res;
    }

    //
    //  now, we await a message from the listener on whether it was 
    //  successful in creating the socket and putting it into a listen state
    //
    char okmsg[sizeof(CSTUB_READY_MESSAGE)+1];

    len = read( pipette[0], okmsg, sizeof( okmsg ));
    if ( len != sizeof( CSTUB_READY_MESSAGE ) || 
         strcmp( okmsg, CSTUB_READY_MESSAGE ) != 0 ) {
        // the listener couldn't start 
#ifdef CGISTUB_DEBUG
        cerr << "ChildExec::startListener - "
                "incorrect response from the listener process" << endl;
#endif //  CGISTUB_DEBUG
        close( pipette[0] );
        _ioerr = 0;
        _res = CERR_EXECFAILURE;
        return _res;
    }

    //
    //  stub's happy, we're happy
    //
    cbListenPid = pid;
    cbListenFd  = pipette[0];
    _res = CERR_OK;
}

//
//  create a connected unix domain socket to the listener
//  process and wrap it in a request pipe
//  NOTE this is done without the cb locks taken, so 
//  do not try to manipulate the new request pipe's state here
//
CHILDEXEC_ERR ChildExec::createRequestPipe( CExecReqPipe*& rqp, int& ioerr )
{
    int     fd;

#if defined(USE_CONNLD)

    int     rv;

    //
    //  create the pipe
    //
    if ((fd = open(cbSockPath, 2)) < 0) {
        ioerr = errno;
#ifdef CGISTUB_DEBUG
        cerr << "ChildExec::createRequestPipe - error on open() pipe, error " 
             << ioerr << ", aborting request" << endl;
#endif //  CGISTUB_DEBUG
        return CERR_REQFAILURE;
    }

    //
    //  connect to the listener to create a new request pipe fd
    //

    rv = isastream(fd);
    if (rv == 0) {
	/* no one listening anymore - we got a plain file fd */
#ifdef CGISTUB_DEBUG
        cerr << "ChildExec::createRequestPipe - error on isastream, no listener, aborting request" << endl;
#endif //  CGISTUB_DEBUG
        close( fd );
	return CERR_NOLISTENER;
    } else if (rv < 0) {
#ifdef CGISTUB_DEBUG
        cerr << "ChildExec::createRequestPipe - error on isastream, error " 
             << errno << ", aborting request" << endl;
#endif //  CGISTUB_DEBUG
        close( fd );
        return CERR_REQFAILURE;
    }

#else /* USE_CONNLD */

    struct  sockaddr_un servaddr;

    //
    //  create the socket
    //
    fd = socket( AF_UNIX, SOCK_STREAM, 0 );
    if ( fd == -1 ) {
        ioerr = errno;
#ifdef CGISTUB_DEBUG
        cerr << "ChildExec::createRequestPipe - error on socket(), error " 
             << ioerr << ", aborting request" << endl;
#endif //  CGISTUB_DEBUG
        return CERR_REQFAILURE;
    }

    memset( &servaddr, 0, sizeof( servaddr ));
    servaddr.sun_family = AF_UNIX;
    strcpy( servaddr.sun_path, cbSockPath );

    //
    //  connect to the listener to create a new request pipe fd
    //
    if ( connect( fd, (struct sockaddr *)&servaddr, sizeof( servaddr )) < 0 ) {
        ioerr = errno;
#ifdef CGISTUB_DEBUG
        cerr << "ChildExec::createRequestPipe - error on connect, error " 
             << ioerr << ", aborting request" << endl;
#endif //  CGISTUB_DEBUG
        close( fd );
        if ( ioerr == ENOENT || ioerr == ECONNRESET )
            return CERR_NOLISTENER;
        if ( ioerr == ECONNREFUSED )
#if defined(LINUX)
            return CERR_NOLISTENER;
#else
            return CERR_LISTENERBUSY;
#endif
        return CERR_REQFAILURE;
    }

#endif /* USE_CONNLD */

    fcntl( fd, F_SETFD, FD_CLOEXEC );

    //
    //  now wrap the fd in a request pipe and make it available
    //
    rqp = new CExecReqPipe( *this, fd );
    if ( rqp == NULL ) {
        close( fd );
        return CERR_MEMFAILURE;
    }
    return CERR_OK;
}

//
// -- stub process request building routines --
//

//
//  build the start request arguments
//  return 0 if we failed
//
int ChildExec::buildStartRequest( char *& cPtr, size_t& msgSize, 
                                  const cexec_args_t& parms)
{
    size_t              totlen;
    cstub_start_req_t * pds;
    char *              ptlv;
    char *              pipeBuf;
    char *              envp_array   = NULL;
    int                 len_envp_ar  = 0;
    char *              argv_array   = NULL;
    int                 len_argv_ar  = 0;
    int                 len_execpath = 0;
    int                 len_argv0    = 0;
    int                 len_chdir    = 0;
    int                 len_chroot   = 0;
    int                 len_user     = 0;
    int                 len_group    = 0;
    int                 len_nice     = 0;
    int                 len_rlimit_as = 0;
    int                 len_rlimit_core = 0;
    int                 len_rlimit_cpu = 0;
    int                 len_rlimit_nofile = 0;

    cPtr    = NULL;
    msgSize = 0;
    totlen  = offsetof( cstub_start_req_t, cs_tlv );

    if ( parms.cs_exec_path ) {
        len_execpath = strlen( parms.cs_exec_path ) + 1;
        totlen += ROUND_UP( (len_execpath + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_argv0 ) {
        len_argv0 = strlen( parms.cs_argv0 ) + 1;
        totlen += ROUND_UP( (len_argv0 + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_args ) {
        argv_array = cs_build_varargs_array( -1, parms.cs_args, &len_argv_ar );
        if (len_argv_ar)
          totlen += ROUND_UP( (len_argv_ar + TLV_VECOFF), TLV_ALIGN );
    }
    //
    //  NOTE: this currently (31Mar98) is not being processed in the
    //  Cgistub process (#ifdef'd out)
    //
    if ( parms.cs_opts.dir ) {
        len_chdir = strlen( parms.cs_opts.dir ) + 1;
        totlen += ROUND_UP( (len_chdir + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_opts.root ) {
        len_chroot = strlen( parms.cs_opts.root ) + 1;
        totlen += ROUND_UP( (len_chroot + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_opts.user ) {
        len_user = strlen( parms.cs_opts.user ) + 1;
        totlen += ROUND_UP( (len_user + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_opts.group ) {
        len_group = strlen( parms.cs_opts.group ) + 1;
        totlen += ROUND_UP( (len_group + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_opts.nice ) {
        len_nice = strlen( parms.cs_opts.nice ) + 1;
        totlen += ROUND_UP( (len_nice + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_opts.rlimit_as ) {
        len_rlimit_as = strlen( parms.cs_opts.rlimit_as ) + 1;
        totlen += ROUND_UP( (len_rlimit_as + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_opts.rlimit_core ) {
        len_rlimit_core = strlen( parms.cs_opts.rlimit_core ) + 1;
        totlen += ROUND_UP( (len_rlimit_core + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_opts.rlimit_cpu ) {
        len_rlimit_cpu = strlen( parms.cs_opts.rlimit_cpu ) + 1;
        totlen += ROUND_UP( (len_rlimit_cpu + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_opts.rlimit_nofile ) {
        len_rlimit_nofile = strlen( parms.cs_opts.rlimit_nofile ) + 1;
        totlen += ROUND_UP( (len_rlimit_nofile + TLV_VECOFF), TLV_ALIGN );
    }
    if ( parms.cs_envargs ) {
        envp_array = cs_build_varargs_array( -1, parms.cs_envargs, &len_envp_ar );
        if (len_envp_ar)
          totlen += ROUND_UP( (len_envp_ar + TLV_VECOFF), TLV_ALIGN );
    }

    //
    //  The size of the message includes the initial cstub_start_req_t
    //
    pipeBuf = (char*)MALLOC(totlen);
    if ( ! pipeBuf ) {
        cPtr = NULL;
        msgSize = 0;
        FREE(argv_array);
        FREE(envp_array);
        return 0;
    }

    /* LINTED */
    pds = (cstub_start_req_t *) &pipeBuf[0];

    pds->cs_msgtype = CRQ_START;
    pds->cs_version = CSTUBVERSION;
        
    ptlv = (char *) &pds->cs_tlv;
    if ( parms.cs_exec_path ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_PATH, 
                                len_execpath, parms.cs_exec_path );
    }
    if ( parms.cs_argv0 ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_PROG, 
                                len_argv0, parms.cs_argv0 );
    }
    if ( len_argv_ar ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_ARGV, len_argv_ar, argv_array );
        FREE(argv_array);
    }

    if ( len_chdir ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_CHDIRPATH, 
                                len_chdir, parms.cs_opts.dir );
    }
    if ( len_chroot ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_CHROOTPATH, 
                                len_chroot, parms.cs_opts.root );
    }
    if ( len_user ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_USERNAME, 
                                len_user, parms.cs_opts.user );
    }
    if ( len_group ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_GROUPNAME, 
                                len_group, parms.cs_opts.group );
    }
    if ( len_nice ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_NICE, 
                                len_nice, parms.cs_opts.nice );
    }
    if ( len_rlimit_as ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_RLIMIT_AS, 
                                len_rlimit_as, parms.cs_opts.rlimit_as );
    }
    if ( len_rlimit_core ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_RLIMIT_CORE, 
                                len_rlimit_core, parms.cs_opts.rlimit_core );
    }
    if ( len_rlimit_cpu ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_RLIMIT_CPU, 
                                len_rlimit_cpu, parms.cs_opts.rlimit_cpu );
    }
    if ( len_rlimit_nofile ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_RLIMIT_NOFILE, 
                                len_rlimit_nofile, parms.cs_opts.rlimit_nofile );
    }
    if ( len_envp_ar ) {
        ptlv += cs_add_tlv_hdr( ptlv, CRQT_ENVP, len_envp_ar, envp_array );
        FREE(envp_array);
    }
    

    PR_ASSERT(totlen == (ptlv - (char*)pds));
    PR_ASSERT(totlen == (ptlv - pipeBuf));
    // 
    //  set the size of the buffer
    //
    /* LINTED */
    pds->cs_msgLength = totlen;

    msgSize = totlen;
    cPtr    = pipeBuf;

    return 1;
}

/*
 *  convert an array of char * (char **) to a linear buffer
 */
static char *
cs_build_varargs_array( int nargs, const char * const * pparg_save,
                        int * parray_len )
{
    int     totsz = 0;
    int     ndx;
    const char * const * ppargs;
    char *  rbuf = NULL;
    char *  bufend;

    *parray_len = 0;
    ppargs = pparg_save;
    if ( nargs == -1 ) {
        for ( nargs = 0; ppargs[nargs] != NULL ; nargs ++ ) {
             totsz += strlen( ppargs[nargs] ) + 1;
        }
    } else {
        for ( ndx = 0; ndx < nargs; ndx ++ ) {
             totsz += strlen( ppargs[ndx] ) + 1;
        }
    }

    if (totsz)
        rbuf = (char*)MALLOC(totsz);
    if ( rbuf == NULL ) {
        return NULL;
    }
    bufend = rbuf;

    for ( ndx = 0; ndx < nargs; ndx ++ ) {
        int arglen = strlen( ppargs[ndx] );
        memcpy( bufend, ppargs[ndx], arglen + 1 );
        bufend += arglen + 1;
    }

    *parray_len = (bufend - rbuf);
    return rbuf;
}

//
//  add a tlv element; return the length added
//
static int
cs_add_tlv_hdr( void * ptlv_area, creq_tlv_type_e type, int datalen, 
                const char * vector )
{
    register creq_tlv_t * ptlv = (creq_tlv_t *) ptlv_area;
    register int pad;
    register int tlvlen = datalen + TLV_VECOFF;

    ptlv->type = type;
    ptlv->len  = tlvlen;       
    memcpy ( &ptlv->vector, vector, datalen );

    /* pad if necessary, assuming power-of-two */
    pad = ROUND_UP( tlvlen, TLV_ALIGN ) - tlvlen;
    if ( pad ) {
        char * padit = &ptlv->vector + datalen;
        tlvlen += pad;
        while ( pad-- ) {
            *padit = '\0';
            padit++;
        }
    }
        
    return (tlvlen);
}

