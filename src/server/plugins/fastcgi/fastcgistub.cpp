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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <vector>
using namespace std;

#ifdef XP_WIN32
  #include <Windows.h>
  #include "wingetopt.h"
#else
  #include <sys/resource.h>
  #include <unistd.h>
  #include <sys/stat.h>

  #if !defined(LINUX)
    #include <sys/stropts.h>
  #endif

  #if !defined(USE_CONNLD)
    #include <sys/un.h>
  #endif

  #include <sys/uio.h>
  #include <sys/uio.h>
  #include <fcntl.h>

  #if defined(HPUX) || defined(AIX)
    #include <sys/wait.h>
  #else
    #include <wait.h>
  #endif

  #ifndef HPUX
    #include <sys/select.h>
  #endif

  #include <pwd.h>
  #include <grp.h>
  #include <dirent.h>
#endif //XP_WIN32

#include <ctype.h>
#include <sys/types.h>

#include <nspr.h>
#include <plstr.h>
#include "private/pprio.h"
#include "nsapi.h"
#include "fastcgi.h"
#include "constants.h"
#include "util.h"
#include "serverconfig.h"
#include "fastcgistub.h"

//global vars
static GenericList processList; //list of processes
static PRLock *lockProcessList = PR_NewLock();
static PRLock *lockDeadChildList = PR_NewLock();
FileDescPtr stubFd = NULL;
PRNetAddr stubAddr;
char *stubBindPath  = NULL;
PluginError lastError;
char *stubName = NULL;
PRBool verbose = PR_FALSE;
FILE *stubLogFd = NULL;
char *stubLogFileName = NULL;
PRBool stubStopSignal = PR_FALSE;
#ifdef XP_WIN32
HANDLE stubStopEventHandle = NULL;
#endif //XP_WIN32

#ifdef XP_UNIX
PidType primordialPid = -1;
#endif // XP_UNIX

//static PRIntervalTime ticksPingInterval;
static PRLock *condVarLock = NULL;
static PRCondVar *condVar = NULL;
static PRThread *monThread = NULL;
static PRThread *childHandlerThread = NULL;
static PRBool terminateThread = PR_FALSE;

//functions
NSPR_BEGIN_EXTERN_C
void FastCGISetSignals();
void monitorThread(void *arg);
void childExitHandlerThread(void *arg);
void stopStubThread();
void startStubThread();
void cleanStub(int s);
NSPR_END_EXTERN_C

static void printMsg(const char *msg) {
    if(verbose && stubLogFd) {
        if(msg)
            fprintf(stubLogFd, "%s\n", msg);
        else
            (stubLogFd, "NULL\n");
        fflush(stubLogFd);
    }
}

void sendResponse() {
#ifdef XP_UNIX
    write(STDIN_FILENO, (char *)&lastError, sizeof(PluginError));
#else
    DWORD bytesSent;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin != INVALID_HANDLE_VALUE) {
        WriteFile(hStdin, (char *)&lastError, sizeof(PluginError), &bytesSent, NULL);
                FlushFileBuffers(hStdin);
    }
#endif //XP_UNIX
}

void sendResponseToStubRequest(FileDescPtr fd, PluginError reqLastError) {
#ifdef XP_UNIX
    PR_Send(fd, (char *)&reqLastError, sizeof(PluginError), 0, PR_INTERVAL_NO_TIMEOUT);
#else
    PRUint32 bytesWritten = -1;
   // Write the reply to the pipe.
    BOOL fSuccess = WriteFile(fd,            // handle to pipe
                              (char *)&reqLastError,   // buffer to write from
                              sizeof(PluginError), // number of bytes to write
                              (unsigned long *)&bytesWritten,   // number of bytes written
                              NULL);        // not overlapped I/O
        FlushFileBuffers(fd);
#endif //XP_UNIX
}

NSPR_BEGIN_EXTERN_C
void handleExit() {
    PR_Lock(lockProcessList);
    int len = processList.length();

    FcgiProcess *proc;
    for(int i=0; i<processList.length(); i++) {
        proc = (FcgiProcess *)processList[i];
        delete proc;
        proc = NULL;
    }
    processList.flush();
    PR_Unlock(lockProcessList);

    PR_DestroyLock(lockProcessList);
    lockProcessList = NULL;

    if(stubFd) {
#ifdef XP_UNIX
        PR_Close(stubFd);
#else
        CloseHandle(stubFd);
#endif //XP_UNIX
        stubFd = NULL;
    }

    if(stubBindPath) {
        PR_Delete(stubBindPath);
        PL_strfree(stubBindPath);
        stubBindPath = NULL;
    }

    if(stubLogFd) {
        fclose(stubLogFd);
        stubLogFd = NULL;
        verbose = PR_FALSE;
    }
}
NSPR_END_EXTERN_C

/*
 *----------------------------------------------------------------------
 *
 *
 *    start the FastCGI process managers which is responsible for:
 *      - starting all the FastCGI proceses.
 *      - restart should any of these die
 *        - looping and call waitpid() on each child
 *      - clean up these processes when server restart
 *
 * Results:
 *
 * Side effects:
 *  Registers a new AppClass handler for FastCGI.
 *
 *----------------------------------------------------------------------
 */
PRStatus startProcesses(FcgiProcess *proc, int numProcs, PRBool lifecycleCall = PR_FALSE) {
    int i;

    int numChilds = proc->getNumChildren();
    numProcs = ((proc->minProcs > numChilds) ? (proc->minProcs - numChilds) : 1);

    if(lifecycleCall) {
        if(proc->maxProcs - numChilds <= 0)
            numProcs = 0;
    }

    if(numProcs > 0) {
        for(i = 0; i < numProcs; i++) {
            if(proc->execFcgiProgram(lifecycleCall) != NO_FCGI_ERROR) {
                if(verbose) {
                    fprintf(stubLogFd, "failed to exec application %s\n",
                        proc->procInfo.execPath);
                    fflush(stubLogFd);
                }
                return PR_FAILURE;
            }
        } //for
    }

    PR_Lock(condVarLock);
    PR_NotifyAllCondVar(condVar);
    PR_Unlock(condVarLock);

    return PR_SUCCESS;
}

#ifdef XP_UNIX
//Setup signal dispostion for the Cgistub process
static void setupStubSignalDisposition() {
    struct  sigaction sa;

    /*
     * chances are good that we were forked from a process which
     * has all signals (like SIGCHLD, SIGTERM and SIGHUP) blocked.
     * ensure they are unblocked.
     */
    memset(&sa, 0, sizeof(sa));
    sigemptyset( &sa.sa_mask );
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGCHLD);
    if ( sigprocmask( SIG_UNBLOCK, &sa.sa_mask, NULL ) == -1 ) {
        ;
    }
}
#endif //XP_UNIX

/*
 *  clean up our environment; close all fds we might have potentially
 *  inherited
 */
#ifdef XP_UNIX
void cleanupEnvironment(void) {
    int     fd;
    int     fdlimit;
    struct  sigaction sa;

    /*
     *  first, close all our potentially nasty inherited fds
     *  leave stdin, stdout & ..err
     */
    fdlimit = sysconf( _SC_OPEN_MAX );
    for ( fd = STDERR_FILENO+1; fd < fdlimit; fd++ ) {
    if(!stubLogFd || (fd != fileno(stubLogFd)))
            close( fd );
    }

    setupStubSignalDisposition();
}
#endif //XP_UNIX

/*
 * FcgiProgramExit --
 *
 *  This routine gets called when the child process for the
 *  program exits.  The exit status is recorded in the request log.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Status recorded in log, pid set to -1.
 *
 *----------------------------------------------------------------------
 */
#ifdef XP_UNIX
static void processExit(FcgiProcess *proc, int status, pid_t pid) {

    /*
     * Only log the state of the process' exit if it's expected to stay
     * alive.
     */
     if(WIFEXITED(status) && (WEXITSTATUS(status) != 0)) {
         if(verbose) {
             fprintf(stubLogFd, "FastCGI process %s exited with the status %d\n",
                     proc->procInfo.execPath, WEXITSTATUS(status));
             fflush(stubLogFd);
         }
         proc->currentNumFails++;
     } else if(WIFSIGNALED(status)) {
         if(verbose) {
            fprintf(stubLogFd, "FastCGI process %s terminated due to signal %d\n", 
                    proc->procInfo.execPath, WTERMSIG(status));
            fflush(stubLogFd);
         }
     }
}
#endif //XP_UNIX

/*
 *  build a varargs from a linear buffer
 *  initial_pad is for leaving space for argv[0]
 */
char **unbuildEnvArray( char ** argp, char * lb_start, int lb_len,
                       int initial_pad, int argp_sz ) {
    char *  lb     = lb_start;
    char *  lb_end = &lb[lb_len];
    int     ndx;
    char ** new_argp_area;
    char ** new_argp;


    for ( ndx = initial_pad; (lb < lb_end) &&
         (ndx < argp_sz - 1); ndx++ ) {
        argp[ndx] = PL_strdup(lb);
        lb += (strlen( lb ) + 1);
    }
    if ( lb >= lb_end ) {
        argp[ndx] = NULL;
        return argp;
    }

    /* overflow of argp; try it again, this time with a 2x bigger area */
    argp_sz *= 2+2;
    new_argp_area = (char **) PR_Calloc( argp_sz, sizeof (char *) );
    if ( new_argp_area == NULL ) {
        if(verbose) {
            fprintf(stubLogFd, "%s: calloc failure resizing for %d args\n", stubName, argp_sz );
            fflush(stubLogFd);
        }
        return NULL;
    }
    new_argp = unbuildEnvArray( new_argp_area, lb_start, lb_len,
                                      initial_pad, argp_sz );
    /*
     * if the recursed routine had to realloc itself, then free our
     * temp area
     */
    if ( new_argp && new_argp != new_argp_area ) {
        free( new_argp_area );
    }

    return new_argp;
}


#ifdef XP_WIN32
PRStatus MakePipeInstance(char *pipeName, HANDLE *hPipe) {
   // Create a new instance of the named pipe. This command will
   // fail if two instances already exist

   *hPipe = CreateNamedPipe( pipeName,               // name
                            PIPE_ACCESS_DUPLEX, // duplex mode
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                            PIPE_UNLIMITED_INSTANCES,   // max instances
                            4096,               // out buffer size
                            4096,               // in buffer size
                            5*1000,             // time-out value - 10 second
                            NULL );             // security attributes


   if (*hPipe == INVALID_HANDLE_VALUE) {
      printMsg("Unable to create stub socket");
      return PR_FAILURE;
   }


LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");
        HANDLE hPipe1 = CreateNamedPipe(
          lpszPipename,             // pipe name
          PIPE_ACCESS_DUPLEX,       // read/write access
          PIPE_TYPE_MESSAGE |       // message type pipe
          PIPE_READMODE_MESSAGE |   // message-read mode
          PIPE_WAIT,                // blocking mode
          PIPE_UNLIMITED_INSTANCES, // max. instances
          4096,                  // output buffer size
          4096,                  // input buffer size
          NMPWAIT_USE_DEFAULT_WAIT, // client time-out
          NULL);                    // default security attribute

      if (hPipe1 == INVALID_HANDLE_VALUE)
      {
          printf("CreatePipe failed");
          return PR_FAILURE;
      }

   return PR_SUCCESS;
}
#endif //XP_WIN32




/*
 *  parse a start request and extract the parameters sent
 *  return true if successful
 */
PRStatus parseRequest( RequestHeader * rq, size_t rq_len, ProcessInfo *params, int lcl_argsz, char **vsId ) {
    char * cptlv = (char *) &rq->dataHeader;
    char * eov   = (char *) rq + rq_len;

    while ( cptlv < eov ) {
        RequestDataHeader * ptlv;
        /* LINTED */
        ptlv = (RequestDataHeader *) cptlv;
        switch( ptlv->type ) {
            case RQT_USERNAME:
                params->userName = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_GROUPNAME:
                params->groupName = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_CHDIRPATH:
                params->chDir = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_CHROOTPATH:
                params->chRoot = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_NICE:
                params->priority = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_RLIMIT_AS:
                params->rlimit_as = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_RLIMIT_CORE:
                params->rlimit_core = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;

                break;
            case RQT_RLIMIT_CPU:
                params->rlimit_cpu = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_RLIMIT_NOFILE:
                params->rlimit_nofile = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_PATH:
                params->execPath = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_ARGV0:
                params->argv0 = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_BIND_PATH:
                params->bindPath = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_RESTART_INTERVAL:
                params->restartDelay = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                if(params->restartDelay) {
                    int delayTime = atoi(params->restartDelay);
                    if(delayTime > 0)
                        params->restartDelayTime = PR_SecondsToInterval(delayTime * 60);
                    else
                        params->restartDelayTime = 0;
                } else {
                    //defaults to 1 hr
                    params->restartDelayTime = PR_SecondsToInterval(60 * 60);
                }
                break;
            case RQT_MIN_PROCS:
                params->minProcesses = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_MAX_PROCS:
                params->maxProcesses = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_LISTENQ_SIZE:
                params->listenQueueDepth = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_RESTART_ON_EXIT:
                params->restartOnExit = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_NUM_FAILURE:
                params->numFailures = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_VS_ID:
                *vsId = (&ptlv->vector) ? PL_strdup(&ptlv->vector) : NULL;
                break;
            case RQT_ENVP:
                params->env = unbuildEnvArray( params->env, &ptlv->vector, ptlv->len - REQUEST_VECOFF, 0,
                                       lcl_argsz );
                break;
            case RQT_ARGV:
                /* offset by 1 in argv for room for argv0 */
                params->args = unbuildEnvArray( params->args, &ptlv->vector, ptlv->len - REQUEST_VECOFF, 1,
                                       lcl_argsz );
                break;
            case RQT_END:
                /* ignore remainder */
                cptlv = eov; /* it'll go way past when round_up is done*/
                break;
            default:
                if(verbose) {
                    fprintf(stubLogFd,
            "%s: WARNING - skipping unknown header value [%d]\n",
                        stubName, (int) ptlv->type );
                    fflush(stubLogFd);
                }
                break;
        }   //switch

        if ( ptlv->len < REQUEST_VECOFF ) {
            if(verbose) {
                fprintf(stubLogFd, "%s: WARNING : corrupt TLV length [%d]\n",
                            stubName, (int) ptlv->len );
                fflush(stubLogFd);
            }
            return PR_FAILURE;
        }

        cptlv += ROUND_UP( ptlv->len, REQUEST_ALIGN );
    }   //while

    /*  have to at LEAST have a program name,path and bindpath */
    if ( ! params->argv0 || ! params->execPath || !params->bindPath) {
        return PR_FAILURE;
    }

    /*  insert program name as argv[0] */
    params->args[0] = const_cast<char *>(params->argv0);

    return PR_SUCCESS;
}

PRStatus handleStartRequest(RequestHeader *rq, int pDatalen, FileDescPtr connfd, FcgiProcess *proc) {
    int one = 1; /* optval for setsockopt */
    int addrLen = 0;
    int isUds = 0;
    char *vsId = NULL;
    ProcessInfo procInfo;
    char **localArgs = (char **)PR_Malloc(sizeof(char *) * ENVSIZE);
    char **localEnv = (char **)PR_Malloc(sizeof(char *) * ENVSIZE);

    memset( &procInfo, 0, sizeof(ProcessInfo));
    memset( &localEnv[0], 0, ENVSIZE);
    memset( &localArgs[0], 0, ENVSIZE);

    proc->procInfo.env = localEnv;
    proc->procInfo.args = localArgs;

    if(parseRequest( rq, pDatalen, &(proc->procInfo), ENVSIZE, &vsId) == PR_FAILURE) {
        sendResponseToStubRequest(connfd, REQUEST_MISSING_OR_INVALID_PARAM);
        printMsg("missing param");
        return PR_FAILURE;
    }

    PR_Lock(lockProcessList);
    int len = processList.length();
    int i =0;
    for(; i<len; i++) {
        FcgiProcess *p = (FcgiProcess *)processList[i];
        if(*p == *proc) {
            if(verbose) {
                fprintf(stubLogFd, "%s bound to %s is already running\n", proc->procInfo.execPath, proc->procInfo.bindPath);
                fflush(stubLogFd);
            }
            PR_Unlock(lockProcessList);
            sendResponseToStubRequest(connfd, NO_FCGI_ERROR);
            free(vsId);
            delete proc;
            return PR_SUCCESS;
        }
    }

    proc->initProcess();
    PRStatus res = getSockAddr(proc->procInfo.bindPath, &proc->procInfo.servAddr, &isUds);
    procInfo = proc->procInfo;

#ifdef XP_WIN32
    if(res == PR_FAILURE && isUds) {
        //Create a named pipe and then connect to it
        res = createPipe(procInfo.bindPath, BIND_PATH_PREFIX,(HANDLE *)&proc->serverFd);
        if (res == PR_FAILURE) {
            sendResponseToStubRequest(connfd, SERVER_SOCKET_CREATION_FAILURE);
            printMsg("server socket creation failure (could be because of invalid bind-path)");
            goto failure;
        }
    } else {
#endif // XP_WIN32

    if(res == PR_SUCCESS) {
        //create socket
        proc->serverFd = socket(procInfo.servAddr->sa_family, SOCK_STREAM, 0);
    }

    if (res == PR_FAILURE || proc->serverFd < 0) {
        sendResponseToStubRequest(connfd, SERVER_SOCKET_CREATION_FAILURE);
        if(res == PR_FAILURE)
            printMsg(getError());
        else
            printMsg("server socket creation failure (could be because of invalid bind-path)");
        goto failure;
    }

    //set the reuse sockaddr option
    setsockopt(proc->serverFd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));

    // Try to bind
#ifdef XP_UNIX
    if(isUds)
        addrLen = sizeof(struct sockaddr_un);
    else
#endif //XP_UNIX
        addrLen = sizeof(struct sockaddr_in);

    if(bind(proc->serverFd, procInfo.servAddr, addrLen) == -1) {
        sendResponseToStubRequest(connfd, SERVER_BIND_ERROR);
        printMsg("server bind error");
        goto failure;
    }

    /*
     *  Since our uid may change part way through execution, allow connections
     *  from any user.  The socket should have been created in a protected
     *  directory, so this should be safe.
     */
#ifdef XP_UNIX
    if (isUds) {
        if(chmod(procInfo.bindPath, 0777)) {
            perror("chmod");
            PR_Unlock(lockProcessList);
            exit(3);
        }
    }
#endif //XP_UNIX

    //if(PR_Listen(proc->serverFd, proc->getIntParameter(procInfo.listenQueueDepth)) != PR_SUCCESS) {
    if(listen(proc->serverFd, proc->getIntParameter(procInfo.listenQueueDepth)) == -1) {
        sendResponseToStubRequest(connfd, SERVER_LISTEN_ERROR);
        printMsg("server listen error");
        goto failure;
    }

#ifdef XP_WIN32
    }
#endif // XP_WIN32

    proc->addVsId(vsId);

#ifdef LINUX
    proc->createRequest = PR_TRUE;
    processList.append(proc);
    PR_Unlock(lockProcessList);
#else
    if(startProcesses(proc, proc->minProcs) != PR_SUCCESS) {
        goto failure;
    }

    processList.append(proc);
    PR_Unlock(lockProcessList);
    sendResponseToStubRequest(connfd, NO_FCGI_ERROR);
#endif

    return PR_SUCCESS;

failure:
    delete proc;
    proc = NULL;
    PR_Unlock(lockProcessList);
    return PR_FAILURE;
}

PRStatus handleOverloadRequest(RequestHeader *rq, int pDatalen, FileDescPtr connfd, FcgiProcess *proc) {
    char *vsId = NULL;

    if(parseRequest( rq, pDatalen, &(proc->procInfo), ENVSIZE, &vsId) == PR_FAILURE) {
        sendResponseToStubRequest(connfd, REQUEST_MISSING_OR_INVALID_PARAM);
        printMsg("missing param");
        delete proc;
        return PR_FAILURE;
    }

    PR_Lock(lockProcessList);
    int len = processList.length();
    int i =0;
    for(; i<len; i++) {
        FcgiProcess *p = (FcgiProcess *)processList[i];
        if(*p == *proc) {
        //if((*(processList[i])) == (*proc)) {
           delete proc;
           proc = (FcgiProcess *)processList[i];
           break;
        }
    }

    if(i == len) { // process not in the list
        sendResponseToStubRequest(connfd, PROC_DOES_NOT_EXIST);
        printMsg("process does not exist");
        delete proc;
        PR_Unlock(lockProcessList);
        return PR_FAILURE;
    }

    if(startProcesses(proc, 1) != PR_SUCCESS) {
        PR_Unlock(lockProcessList);
        return PR_FAILURE;
    }

    proc->addVsId(vsId);
    PR_Unlock(lockProcessList);
    sendResponseToStubRequest(connfd, NO_FCGI_ERROR);
    return PR_SUCCESS;
}

PRStatus handleVSRequest(RequestHeader *rq, int pDatalen, FileDescPtr connfd, FcgiProcess *proc) {
    char *vsId = NULL;
    if(parseRequest( rq, pDatalen, &(proc->procInfo), ENVSIZE, &vsId) == PR_FAILURE) {
        sendResponseToStubRequest(connfd, REQUEST_MISSING_OR_INVALID_PARAM);
        printMsg("missing param");
        delete proc;
        return PR_FAILURE;
    }

    FcgiProcess *p;
    int len = processList.length();

    for(int i=0; i<len; i++) {
        p = (FcgiProcess *)processList[i];
        if((p->containsVsId(vsId)) && (p->getNumVS() == 1)) {
            processList.removeAt(i);
            delete p;
            p = NULL;
        } //if
    } //for proclist

    sendResponseToStubRequest(connfd, NO_FCGI_ERROR);
    return PR_SUCCESS;
}

PRStatus processMessage( FileDescPtr connfd, void * pData, int pDatalen, FcgiProcess *proc ) {
    RequestHeader * rq;
    PRStatus rv = PR_SUCCESS;

    rq = (RequestHeader *) pData;
    if ( pDatalen < sizeof( *rq )) {
        sendResponseToStubRequest(connfd, REQUEST_INCOMPLETE_HEADER);
        if(verbose) {
            fprintf(stubLogFd,
                "WARNING - got short message from client %d bytes; aborting\n",
                pDatalen );
            fflush(stubLogFd);
        }
        delete proc;
        return PR_FAILURE;
    }

    if ( rq->reqVersion != STUBVERSION ) {
        if(verbose) {
            fprintf(stubLogFd, "%s: got an INVALID VERSION id [%d]\n", STUB_NAME, rq->reqVersion );
            fflush(stubLogFd);
        }
        sendResponseToStubRequest(connfd, INVALID_VERSION);
        delete proc;
        return PR_FAILURE;
    }

    switch (rq->reqMsgType ) {
        case RQ_START:
            rv = handleStartRequest( rq, pDatalen, connfd, proc );
            break;
        case RQ_OVERLOAD:
            rv = handleOverloadRequest( rq, pDatalen, connfd, proc );
            break;
        case RQ_VS_SHUTDOWN:
//            rv = handleVSRequest( rq, pDatalen, connfd, proc );
            break;
        default:
            if(verbose) {
                fprintf(stubLogFd, "%s: got an INVALID request id [%d]\n", STUB_NAME, rq->reqMsgType );
                fflush(stubLogFd);
            }
            sendResponseToStubRequest(connfd, INVALID_REQUEST_TYPE);
            delete proc;
            rv = PR_FAILURE;
    }

    return rv;
}

/*
 *  process requests from our clients
 *  this is the second-stage process' main loop;  this is
 *  entered when a new request pipe has been created (due
 *  to an accept() completing on the listen socket)
 *  when a close (hangup,eof) occurs on our side of the
 *  pipe, we go away
 */
NSPR_BEGIN_EXTERN_C
void processRequest(void *proc) {
    char iobuf[IOBUFSIZE] = "";
//    char *iobuf = (char *)PR_Malloc(sizeof(char) * IOBUFSIZE);
    PRUint32         iobufLen = 0;
    PRUint32         tobeRead = 0;
    PRUint32         readlen = 0;
    int    pDataLen = 0;
#ifdef XP_WIN32
    BOOL fSuccess;
#endif //XP_WIN32

//    netbuf *buf;
    PRBool done = PR_FALSE;
    FcgiProcess *fcgiProcess = (FcgiProcess *)proc;
    FileDescPtr connfd = fcgiProcess->getClientFd();
 //   memset(iobuf, 0, (sizeof(char *) * IOBUFSIZE));
    RequestHeader * pRec = (RequestHeader *)iobuf;

    tobeRead = IOBUFSIZE;

#ifdef XP_UNIX
processAgain:
    PR_ClearInterrupt();
    while ( tobeRead > 0 && ((readlen = PR_Recv( connfd, &iobuf[iobufLen], tobeRead,
                            0, PR_INTERVAL_NO_TIMEOUT )) > 0)) {
#else
    while(tobeRead > 0) {
        PR_ClearInterrupt();
        // Read client requests from the pipe.
        fSuccess = ReadFile(connfd,            // handle to pipe
                           &iobuf[iobufLen],  // buffer to receive data
                           tobeRead,          // size of buffer
                           (unsigned long *)&readlen, // number of bytes read
                           NULL);        // not overlapped I/O

       if (! fSuccess || readlen <= 0)
           break;
#endif //XP_UNIX


         // the logic is really hokey here: if the request is longer
         // than IOBUFSIZE (which might happen with those REALLY LONG
         // query strings, after filling up the iobuf, you'll call
         // read(..,..,0) which will return 0 which will cause it to exit.
         // The quick fix is to increase the buffer size to be able to
         // hold the longest possible request.
         // Which is: 4k query string * 2 (env and argv) + some -> 10240
        // as we're using STREAM sockets, wait until we've a complete
        // message
        pDataLen = pRec->reqMsgLen;
        iobufLen += readlen;
        if(readlen >= pDataLen)
            tobeRead = 0;
        else
            tobeRead = pDataLen - readlen;
        if(tobeRead == 0) {
            // the request length includes the whole RequestHeader record
            if ( processMessage( connfd, &iobuf[0], pDataLen, fcgiProcess) != PR_SUCCESS ) {
//                 delete fcgiProcess;
//                 fcgiProcess = NULL;
                //sleep for sometime before to give sometime for the client to read the
                //bytes
                PR_Sleep(PR_MillisecondsToInterval(10));
#ifdef XP_UNIX
                PR_Close(connfd);
#else
                CloseHandle(connfd);
#endif //XP_UNIX
                connfd = NULL;
                return;
            }

            // now, move the remaining data to the front of the
            // io buffer
        } //if
    } //while

#ifdef XP_UNIX
    if(PR_GetError() == PR_PENDING_INTERRUPT_ERROR) {
        goto processAgain;     //repeat
    }
#endif //XP_UNIX

    if (connfd) {
#ifdef XP_UNIX
    // EOF/error occurred; the client closed us (or went away),
    // so we go away also
#ifdef LINUX
        if(!fcgiProcess->createRequest)  {
#endif // LINUX
        PR_Close(connfd);
        connfd = NULL;
#ifdef LINUX
        }
#endif // LINUX

#else
        DisconnectNamedPipe(connfd);
        CloseHandle(connfd);
        connfd = NULL;
#endif //XP_UNIX
    } 
}
NSPR_END_EXTERN_C


void serveRequests() {
#ifdef XP_UNIX
    PRNetAddr netAddr;
#endif //XP_UNIX
    FileDescPtr connFd = NULL;
    PRThread *fastcgiRequestProcessorThread;

#ifdef XP_WIN32
         if(MakePipeInstance(stubBindPath, &connFd) == PR_FAILURE) {
            lastError = PIPE_CREATE_FAILURE;
            printMsg("stub file descriptor creation failure");
            sendResponse();
            exit(-1);
        }
#endif //XP_WIN32

    // we're all set; now, to complete the application protocol, we
    // send back an 'ok' indication to the parent letting them know
    // that our exec/bind sequence worked
    {
        lastError = NO_FCGI_ERROR;
        sendResponse();
    }

#ifdef XP_UNIX
    sigset_t threadSignals;
    sigemptyset(&threadSignals);
    sigaddset(&threadSignals, SIGTERM);
    sigaddset(&threadSignals, SIGHUP);

    //Block the term/hup signal so that monitor and child exit handler
    //do not handle those signals
    pthread_sigmask(SIG_BLOCK, &threadSignals, NULL);
#endif //XP_UNIX

    //start monitorThread to monitor server processes.
    startStubThread();

#ifdef XP_UNIX
    //unblock the signals in the main thread
    pthread_sigmask(SIG_UNBLOCK, &threadSignals, NULL);
#endif //XP_UNIX

     for(;;) {
        PR_ClearInterrupt();
#ifdef XP_WIN32
        if(!connFd) {
            if(MakePipeInstance(stubBindPath, &connFd) == PR_FAILURE) {
                lastError = PIPE_CREATE_FAILURE;
                printMsg("stub file descriptor creation failure");
                sendResponse();
                exit(-1);
            }
        }
#endif

#ifdef XP_UNIX
again:
        FileDescPtr connFd = PR_Accept( stubFd, &netAddr, PR_INTERVAL_NO_TIMEOUT);
        //accept connections from plugin
        if ( connFd == NULL ) {
            if(PR_GetError() == PR_PENDING_INTERRUPT_ERROR) {
                printMsg("got interrupted while serving request");
                if(stubStopSignal) {
                    PR_Close(connFd);
                    return;
                } else
                    goto again;     //repeat
            }

            if(stubStopSignal) {
                _exit(0);
            }

            lastError = STUB_ACCEPT_ERROR;
            sendResponse();
            PR_Close(connFd);
            goto again;     //repeat
        }
#else
        // Wait for the client to connect; if it succeeds,
        // the function returns a nonzero value. If the function returns
        // zero, GetLastError returns ERROR_PIPE_CONNECTED.
        BOOL fConnected = ConnectNamedPipe(connFd, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!fConnected) {
          DisconnectNamedPipe(connFd);
          lastError = STUB_ACCEPT_ERROR;
          sendResponse();
          CloseHandle(connFd);
          continue;
        }
#endif //XP_UNIX
        // Create a thread to process the request
        FcgiProcess *proc = new FcgiProcess(connFd, stubLogFd, verbose);
        fastcgiRequestProcessorThread = PR_CreateThread(PR_SYSTEM_THREAD,
                                                processRequest, (void *)proc,
                                                PR_PRIORITY_NORMAL,
                                                PR_LOCAL_THREAD,
                                                PR_UNJOINABLE_THREAD, 0 );

        if(fastcgiRequestProcessorThread == NULL ) {
            printMsg("can't create a request processor thread");
            sendResponseToStubRequest(connFd, REQUEST_THREAD_CREATE_FAILURE);
#ifdef XP_UNIX
            PR_Close(connFd);
#else
            CloseHandle(connFd);
#endif //XP_UNIX
            delete proc;
            proc = NULL;
        }

#ifdef XP_WIN32
        connFd = NULL;
#endif
     } //for

}

void cleanStub(int s) {
    printMsg("clean stub");
    stubStopSignal = PR_TRUE;
    if(monThread || childHandlerThread) {
        stopStubThread();
        monThread = NULL;
        childHandlerThread = NULL;
    }
    return;
}

/*
 *  main - get args, crank it up
 */

int main( int argc, char * argv[] ) {
    extern  char *optarg;
    int     errflg = 0;
    int     c;
    char *fcgiTmpDir = NULL;
    char *fcgiStubLogDir = NULL;
    char *wsId = NULL;

//#undef DEBUG_LOG
#define DEBUG_LOG
#ifdef DEBUG_LOG
    verbose = PR_TRUE;
#endif
    //parse args
    stubName = argv[0];

    while ( (c = getopt(argc, argv, "b:t:i:l:v")) != -1) {
        switch (c) {
            case 'b':
                stubBindPath = PL_strdup( optarg );    //UDS name
                break;
            case 't':
                fcgiTmpDir = PL_strdup( optarg );  //Temporary directory name
                break;
           case 'i':
                wsId = PL_strdup( optarg );
                break;
           case 'l':
                fcgiStubLogDir = PL_strdup( optarg );
                break;
           case 'v':
                verbose = PR_TRUE;
                break;
            case '?':
                errflg++;
        }   //switch
    }   //while

    if(verbose) {
        int len = 0;
#ifdef XP_UNIX
        // 2 bytes extra for path-separator and '\0'
        len = strlen(fcgiStubLogDir) + strlen(STUB_LOG_FILE_NAME) + 2;
        stubLogFileName = (char *)PR_Malloc(len);
        PR_snprintf(stubLogFileName, len, "%s%c%s", fcgiStubLogDir, FILE_PATHSEP, STUB_LOG_FILE_NAME);
#else
        // 3 bytes extra for path-separator and '-' (for separating server
        // id and stub log file name) and '\0'
        len = strlen(fcgiStubLogDir) + strlen(STUB_LOG_FILE_NAME) +
                  strlen(wsId) + 3;
        stubLogFileName = (char *)PR_Malloc(len);
        PR_snprintf(stubLogFileName, len, "%s%c%s-%s", fcgiStubLogDir, FILE_PATHSEP, wsId, STUB_LOG_FILE_NAME);
#endif //XP_UNIX
        stubLogFd = fopen(stubLogFileName, "a+");
        if(!stubLogFd)
            verbose = PR_FALSE;
    }

    printMsg("<<<<<< ERROR LOG >>>>>>");

    if(stubLogFileName) {
        PL_strfree(stubLogFileName);
        stubLogFileName = NULL;
    }
    if(fcgiTmpDir) {
        PL_strfree(fcgiTmpDir);
        fcgiTmpDir = NULL;
    }
    if(wsId) {
        PL_strfree(wsId);
        wsId = NULL;
    }

    if ( errflg || !stubBindPath ) {
        if(verbose)
            fprintf(stubLogFd, "usage: %s -b <bind path> -t <tmp dir> -i <server id> -l <log dir> [-v] \n", argv[0]);
        fflush(stubLogFd);
        return (1);
    }

    // make sure the child processes that stay in the same process
    // group get killed when the Fastcgi stub ends
    atexit(handleExit);

    if ( verbose ) {
        fprintf(stubLogFd, "%s - version (%d)\n", argv[0], STUBVERSION );
        fflush(stubLogFd);
    }

#ifdef XP_UNIX
   cleanupEnvironment();
#else
    stubStopEventHandle = CreateEvent(NULL, FALSE, FALSE, "StubStopEvent");
    if(stubStopEventHandle == NULL) {
        lastError = STUB_START_FAILURE;
        sendResponse();
        exit(-1);
    }
#endif //XP_UNIX

   FastCGISetSignals();

#ifdef XP_UNIX
    //create stubFd
    PRStatus res = PR_SUCCESS;
    if(stubBindPath) {
        res = getNetAddr(stubBindPath, &stubAddr);
        if(res == PR_SUCCESS)
            stubFd = PR_Socket(stubAddr.local.family, SOCK_STREAM, 0);
        primordialPid = atoi(strrchr(stubBindPath, '_') + 1);
        if ( verbose ) {
            fprintf(stubLogFd, "Web Server Primordial pid - (%d)\n", primordialPid );
            fflush(stubLogFd);
        }
    }

    //bind to stubFd
    if (res == PR_FAILURE || !stubFd) {
        lastError = STUB_SOCKET_CREATION_FAILURE;
        sendResponse();

        if(res == PR_FAILURE)
            printMsg(getError());
        else
            printMsg("stub socket creation failure");

        exit(-1);
    }

#ifdef LINUX
    // Change the umask so that unix domain socket is created with
    // permissions for all. This enables servers that run as nobody
    // to connect to this Fastcgistub channel (created by root)
    mode_t old_mask;
    if (geteuid() == 0)
        old_mask = umask(0);
#endif
    // Try to connect
    if(PR_Bind(stubFd, &stubAddr) != PR_SUCCESS) {
#ifdef LINUX
    // Restore the original umask
    if (geteuid() == 0)
        umask(old_mask);
#endif
        lastError = STUB_BIND_ERROR;
        sendResponse();
        printMsg("stub bind error");
        exit(-1);
    }
#ifdef LINUX
    // Restore the original umask
    if (geteuid() == 0)
        umask(old_mask);
#endif

    if(PR_Listen(stubFd, DEFAULT_BACKLOG_SIZE) != PR_SUCCESS) {
        lastError = STUB_LISTEN_ERROR;
        sendResponse();
        printMsg("stub listen error");
        exit(-1);
    }
#endif //XP_UNIX

    //  wait forever...
    serveRequests();

    return 0;
}


void FastCGISetSignals()
{
    signal(SIGTERM, cleanStub);
#ifdef XP_UNIX
    signal(SIGCHLD, SIG_DFL);
#endif //XP_UNIX
}


//-----------------------------------------------------------------------------
// startStubThread
//-----------------------------------------------------------------------------
void startStubThread() {
    condVarLock = PR_NewLock();
    condVar = PR_NewCondVar(condVarLock);

    // Create a thread to monitor processes
    monThread = PR_CreateThread(PR_SYSTEM_THREAD,
                              monitorThread,
                              0,
                              PR_PRIORITY_NORMAL,
                              PR_GLOBAL_THREAD,
                              PR_JOINABLE_THREAD,
                              0);


    // create a thread to handle child restarts
    childHandlerThread = PR_CreateThread(PR_SYSTEM_THREAD,
                              childExitHandlerThread,
                              0,
                              PR_PRIORITY_NORMAL,
                              PR_GLOBAL_THREAD,
                              PR_JOINABLE_THREAD,
                              0);
}

//-----------------------------------------------------------------------------
// stopDaemonPingThread
//-----------------------------------------------------------------------------

void stopStubThread() {
    //notify threads about termination
    PR_Lock(condVarLock);
    terminateThread = PR_TRUE;
    PR_NotifyAllCondVar(condVar);
    PR_Unlock(condVarLock);

    PR_Interrupt(childHandlerThread);
    PR_Interrupt(monThread);

    //wait for the threads to exit
    PR_JoinThread(monThread);
    PR_JoinThread(childHandlerThread);

    handleExit();
    PR_DestroyCondVar(condVar);
    PR_DestroyLock(condVarLock);
}

void childExitHandlerThread(void *arg) {
    FcgiProcess *proc = NULL;
    //thread is executed every 100 milliseconds till a stop is issued.
    PRIntervalTime sleepInterval = PR_MillisecondsToInterval(100);

#ifdef XP_WIN32
    ChildProcessData *childData = NULL;
    DWORD result = -1;
#else
    PidType childPid = -1;
    int status;
    PRIntervalTime startupFailureInterval = PR_SecondsToInterval(2);
#endif //XP_WIN32

    while (!terminateThread) {
        PR_ClearInterrupt();
#ifdef XP_UNIX
#ifdef LINUX
    //Linux (which does not have native POSIX thread support, expects
    //the thread that spawns the process to wait on it for exit.Hence,
    //the change

        //first create the process if createRequest is set to true
        PR_Lock(lockProcessList);
        int len = processList.length();
        if(len > 0) {
            int i=0;
            for(; i<len; i++) {
                proc = (FcgiProcess *)processList[i];
                if(proc->createRequest) {
                    if(startProcesses(proc, proc->minProcs) != PR_SUCCESS) {
                        PR_Close(proc->getClientFd());
                        if(processList.removeAt(i) == PR_SUCCESS) {
                            delete proc;
                            proc = NULL;
                            i--;
                            len = processList.length();
                        }
                        continue;
                    }
                    proc->createRequest = PR_FALSE;
                    sendResponseToStubRequest(proc->getClientFd(), NO_FCGI_ERROR);
                    PR_Close(proc->getClientFd());
                }
            }
        }
        PR_Unlock(lockProcessList);
#endif

            //do wait to get the child process status if at all any child has died.
            while((childPid = waitpid(-1, &status, WNOHANG)) > 0) {
                PR_Lock(lockProcessList);
                proc = FcgiProcess::lookupChildPid(processList, childPid);
                if(!proc) {
                    if(verbose) {
                        fprintf(stubLogFd, "The child %d is not listed within the Servers list \n", childPid);
                        fflush(stubLogFd);
                    }
                    PR_Unlock(lockProcessList);
                    continue;
                }

                int restartOnExit = proc->getIntParameter(proc->procInfo.restartOnExit);
                //if restartOnExit option is not set, then don't restart !
                //else, check if child exited soon after exec and it has happened several times in short span
                if((restartOnExit != 1) || (proc->currentNumFails >= proc->totalNumFails)) {
                    if(verbose) {
                        fprintf(stubLogFd, "Even after trying %d time(s), %s process failed to start...no more retries\n", proc->currentNumFails,  proc->procInfo.execPath);
                        fflush(stubLogFd);
                    }

                    //Assumption: all the child processes of this type are assumed to have startup failures and
                    //hence the process information pertaining to the failed one is removed
                    processList.remove(proc);
                    delete proc;
                    proc = NULL;
                    PR_Unlock(lockProcessList);
                    continue;
                }

                ChildProcessData *childData = proc->childExists(childPid);
                if(childData != NULL) {
                    //check the time interval elapsed since start of the process
                    PRIntervalTime startInterval = PR_IntervalNow() - childData->epoch;

                    //if this interval is too small, then we can assume that there was a startup error
                    if((startInterval <= startupFailureInterval) && (startInterval < proc->procInfo.restartDelayTime)) {
                        if(verbose) {
                            fprintf(stubLogFd, "%d process startup failure, trying to restart \n", childPid);
                            fflush(stubLogFd);
                        }
                        proc->currentNumFails++;
                    }
                }

                proc->removeChild(childPid);

                //log the status
                processExit(proc, status, childPid);

                //restart the process only if current num of processes is less than maxProcs
                startProcesses(proc, 1, PR_TRUE);

                PR_Unlock(lockProcessList);
            } //while

#else
         result = WaitForSingleObject(stubStopEventHandle, 0);
         if(result == WAIT_OBJECT_0) {
            printMsg("Got stub stop notification");
            exit(0);
         }

        PR_Lock(lockProcessList);
        int len = processList.length();
        int numChilds = 0;
        DWORD exitStatus = -1;
        if(len > 0) {
            int i=0;
            for(; i<len; i++) {
                proc = (FcgiProcess *)processList[i];
                numChilds = proc->getNumChildren();
                if(numChilds <= 0) {
                    PR_Unlock(lockProcessList);
                    continue;
                }
                for(int j=0; j<numChilds; j++) {
                    childData = proc->getChild(j);
                    if(childData->pid <= 0)
                        continue;
                    HANDLE childProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, childData->pid);
                    if(childProc == NULL)
                        continue;
                    result = WaitForSingleObject(childProc, 0);
                    if(result != WAIT_OBJECT_0)
                        continue;
                    //child has died
                    //get the exit status
                    GetExitCodeProcess(childProc, &exitStatus);
                    if(exitStatus < 0) { //startup failure
                        //do not restart the process
                        if(verbose) {
                            printMsg("child startup failure...will not restart");
                        }
                        continue;
                    }
                    proc->removeChild(childData->pid);
                    int restartOnExit = proc->getIntParameter(proc->procInfo.restartOnExit);
                    if (restartOnExit != 1)
                        continue;
                    //restart the process only if current num of processes is less than maxProcs
                    startProcesses(proc, 1, PR_TRUE);
                } //for (numChilds)
            } // for(processList length)
        }
        PR_Unlock(lockProcessList);
#endif //XP_UNIX
        PR_Lock(condVarLock);
        PR_WaitCondVar(condVar, sleepInterval);
        PR_Unlock(condVarLock);

        if(PR_GetError() == PR_PENDING_INTERRUPT_ERROR) {
       //stop the thread if terminateThread is set to TRUE
        }
        if (terminateThread)
            break;

#ifdef XP_UNIX
        // Check if the WS primoridial process exists. 
        // Else, terminate Fastcgistub process. Web Server 
        // might have terminated but becuase of active sessions, 
        // it may not have called the plugin's restart callback 
        // function.
        if (primordialPid > 0) {
            int rv = kill(primordialPid, 0);
            if (rv < 0 && errno == ESRCH) {
                printMsg("primordial died");
                kill(getpid(), SIGTERM);
            }
        }
#endif // XP_UNIX

    }//while(!terminateThread)

    printMsg("stopping child exit handler");
    return; //stop execution
}

//-----------------------------------------------------------------------------
// monitorThread
//-----------------------------------------------------------------------------

void monitorThread(void *arg) {
    vector<PidType> terminatedProcessList;
    PRBool processTerminated = PR_FALSE;
    FcgiProcess *proc = NULL;
    ChildProcessData *data = NULL;
    PRIntervalTime restartIntervalTime = 0;
    PRIntervalTime lingerTimeInterval = PR_SecondsToInterval(DEFAULT_LINGER_TIMEOUT);
    PRIntervalTime minSleepInterval = 0;
    PRIntervalTime elapsedTime = 0;
    PRIntervalTime remainingTime = 0;

    while (!terminateThread) {
        PR_ClearInterrupt();
        processTerminated = PR_FALSE;
        minSleepInterval = 0;
        remainingTime = 0;
        PR_Lock(lockProcessList);
        int len = processList.length();
        if(len > 0) {
            int i =0;
            for(; i<len; i++) {
                proc = (FcgiProcess *)processList[i];
                restartIntervalTime = proc->procInfo.restartDelayTime;

                if(restartIntervalTime > 0) {
                    int childNum = proc->getNumChildren();
                    for(int j=0; j<childNum; j++) {
                        data = proc->getChild(j);
                        elapsedTime = (PRIntervalTime)(PR_IntervalNow() - data->epoch);
                        remainingTime = (PRIntervalTime)(restartIntervalTime - elapsedTime);

                        if((elapsedTime < restartIntervalTime) && ((minSleepInterval == 0) || (remainingTime < minSleepInterval)))
                            minSleepInterval = remainingTime;

                        if(elapsedTime >= restartIntervalTime) {
#ifdef XP_UNIX
                            // store the pid of the process that will
                            // be terminated in a list which will be used
                            // to send a kill signal if it did not terminate
                            terminatedProcessList.push_back(data->pid);

                            // time for restart
                            // send TERM signal
                            kill(data->pid, SIGTERM);
#else
                            HANDLE childProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, data->pid);
                            if(childProc)
                                TerminateProcess(childProc, 1);
#endif //XP_UNIX
                            processTerminated = PR_TRUE;
                        }
                    } //for
                }
            } //for
        } //if(len>0)
        PR_Unlock(lockProcessList);

        if(minSleepInterval == 0)
            minSleepInterval = lingerTimeInterval;

        // Pace ourselves
        PR_Lock(condVarLock);
        PR_WaitCondVar(condVar, minSleepInterval);
        PR_Unlock(condVarLock);

        if(PR_GetError() == PR_PENDING_INTERRUPT_ERROR) {
            //stop the thread if terminateThread is set to TRUE
        }

#ifdef XP_UNIX
        if(processTerminated) {
            int termLen = terminatedProcessList.size();
            for(int k=0; k<termLen; k++) {
                //check if the process is still running
                if(!kill(terminatedProcessList[k], 0)) {
                    //send KILL signal
                    kill(terminatedProcessList[k], SIGKILL);
                }
            }
            terminatedProcessList.clear();
        }
#endif // XP_UNIX

        if (terminateThread) {
            goto terminate;
        }
    } //while

terminate:
    printMsg("stopping monitoring");
    return;
}
