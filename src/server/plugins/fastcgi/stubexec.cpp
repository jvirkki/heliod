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

// Author : Seema

#ifdef XP_UNIX
  #include <errno.h>
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <signal.h>
#else
  #include <Windows.h>
#endif // XP_UNIX

#include <nspr.h>
#include <plstr.h>
#include "private/pprio.h"
#include "base/sem.h"
#include "frame/conf.h"
#include "frame/log.h"
#include "base/util.h"
#include "support/objectlist.h"
#include "util.h"
#include "serverconfig.h"
#include "fastcgistub.h"
#include "fastcgii18n.h"
#include "errortypes.h"
#include "stubexec.h"

extern char LOG_FUNCTION_NAME[];
extern SEMAPHORE stubSem;
extern PRBool platform64;

// Constructor
StubExec::StubExec(const FcgiServerConfig *config, Session *sn, Request *rq)
    : fcgiConfig(config), requestMsg(NULL), stubFd(NULL), _sn(sn), _rq(rq),
      args(NULL)
{
    lastError = NO_FCGI_ERROR;
    int nameLen = strlen(FCGI_STUB_NAME);
    int pluginPathLen = strlen(fcgiConfig->fcgiPluginDir);
    int tmpDirLen = strlen(fcgiConfig->fcgiTmpDir);

    char *serverId = conf_getglobals()->Vserver_id;
    int len = strlen(STUB_SEMAPHORE) + strlen(serverId) + 2;

    // stubexec path : <pluginDir>/Fastcgistub
    stubExecPath.append(fcgiConfig->fcgiPluginDir);

#ifdef XP_UNIX
    char parentId[24];
    util_itoa(getppid(), parentId);
    len = tmpDirLen + nameLen + strlen(parentId) + 3; // 3 = file path sep + '_' + '\0'
    stubBindPath = (char *)MALLOC(len);
    // stub bind path : <tmp dir>/"fastcgistub_"<parent proc id>"
    PR_snprintf(stubBindPath, len, "%s%c%s_%s", fcgiConfig->fcgiTmpDir, FILE_PATHSEP, FCGI_STUB_NAME, parentId);
    if (getNetAddr(stubBindPath, &addr) != PR_SUCCESS) {
        log_error(LOG_VERBOSE, LOG_FUNCTION_NAME, _sn, _rq, getError());
        lastError = STUB_CONNECT_FAILURE;
    }
#else
    stubBindPath = (char *) MALLOC(MAX_PIPE_BUF_SIZE);
    PR_snprintf(stubBindPath, MAX_PIPE_BUF_SIZE, "\\\\.\\pipe\\%s-%s", FCGI_STUB_NAME, conf_getglobals()->Vserver_id);
#endif // XP_UNIX

    stubFd = NULL;
    stubPidFileName = createStubPidFileName();
    log_error(LOG_VERBOSE, LOG_FUNCTION_NAME , NULL, NULL, "Fastcgistub pid file name = %s", stubPidFileName.data());
    formArgs();
}

StubExec::~StubExec() {
    if (stubBindPath) {
        FREE(stubBindPath);
        stubBindPath = NULL;
     }
    if (args) {
#ifdef XP_UNIX
        fcgi_util_env_free(args);
#else
        FREE(*args);
        FREE(args);
#endif
        args = NULL;
     }
     if (stubFd) {
#ifdef XP_UNIX
         PR_Close(stubFd);
#else
         CloseHandle(stubFd);
#endif // XP_UNIX
        stubFd = NULL;
     }
}

void StubExec::formArgs() {
    // Fastcgistub -b stubBindPath -t tmpDir -i serverId -l logDir
    char *serverId = conf_getglobals()->Vserver_id;

#ifdef XP_WIN32
    int len = strlen(FCGI_STUB_NAME) + 1 + strlen(BIND_PATH_PARAM) + 1 +
                strlen(stubBindPath) + 1 + strlen(TMP_DIR_PARAM) + 1 +
                strlen(fcgiConfig->fcgiTmpDir) + 1 +
                strlen(SERVER_ID_PARAM) + 1 + strlen(serverId) + 1 +
                strlen(LOG_DIR_PARAM) + 1 + strlen(fcgiConfig->fcgiStubLogDir) + 1;
    args = (char **)MALLOC(sizeof(char *));
    *args = (char *)MALLOC(sizeof(char) * len);
    PR_snprintf(*args, len, "%s %s %s %s %s %s %s %s %s", FCGI_STUB_NAME, BIND_PATH_PARAM, stubBindPath, TMP_DIR_PARAM, fcgiConfig->fcgiTmpDir, SERVER_ID_PARAM, serverId, LOG_DIR_PARAM, fcgiConfig->fcgiStubLogDir);
#else
    int index;
    args = fcgi_util_env_create(args, 10, &index);
    args[index++] = fcgi_util_arg_str((char *)FCGI_STUB_NAME);
    args[index++] = fcgi_util_arg_str((char *)BIND_PATH_PARAM);
    args[index++] = fcgi_util_arg_str(stubBindPath);
    args[index++] = fcgi_util_arg_str((char *)TMP_DIR_PARAM);
    args[index++] = fcgi_util_arg_str(fcgiConfig->fcgiTmpDir);
    args[index++] = fcgi_util_arg_str((char *)SERVER_ID_PARAM);
    args[index++] = fcgi_util_arg_str(serverId);
    args[index++] = fcgi_util_arg_str((char *)LOG_DIR_PARAM);
    args[index++] = fcgi_util_arg_str(fcgiConfig->fcgiStubLogDir);
    args[index] = NULL;
#endif  // XP_WIN32
}

PRStatus StubExec::connect() {
#ifdef XP_UNIX
    if (!stubFd) {
        stubFd = PR_Socket(addr.local.family, SOCK_STREAM, 0);
        if (!stubFd || stubFd == SYS_NET_ERRORFD) {
            stubFd = NULL;
            lastError = STUB_SOCKET_CREATION_FAILURE;
            return PR_FAILURE;
        }
    }

    if (PR_Connect(stubFd, &addr, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
        lastError = STUB_CONNECT_FAILURE;
        PR_Close(stubFd);
        stubFd = NULL;
        return PR_FAILURE;
    }
#else
    if (!stubFd) {
        if (WaitNamedPipe(stubBindPath, NMPWAIT_USE_DEFAULT_WAIT)) {
            stubFd = CreateFile(stubBindPath,   // pipe name
                               GENERIC_READ | GENERIC_WRITE, // read and write access
                               0,              // no sharing
                               NULL,           // default security attributes
                               OPEN_EXISTING,  // opens existing pipe
                               0,              // default attributes
                               NULL);          // no template file

            // Break if the pipe handle is valid.
            if (stubFd == INVALID_HANDLE_VALUE) {
                //for the time being, consider busy as an error
                lastError = STUB_CONNECT_FAILURE;
                return PR_FAILURE;
            }
        } else {
            DWORD k = GetLastError();
            lastError = STUB_CONNECT_FAILURE;
            return PR_FAILURE;
        }
    }
#endif // XP_UNIX
    return PR_SUCCESS;
}

PRStatus StubExec::checkAccess() {
    struct stat     st;
    int defaultLocLen = 0;

    if (system_stat(stubExecPath, &st) == -1) {
        // look in the default dir <webserver root>/plugins/fastcgi 
        // for 32 bit and 
        // in <webserver root>/plugins/fastcgi/<PRODUCT_PLATFORM_SUBDIR> 
        // for64 bit library
        stubExecPath.clear();
        stubExecPath.append(conf_get_true_globals()->Vnetsite_root);
        stubExecPath.append("/"FCGISTUB_INSTALL_SUFFIX);
        // if webserver is running in 64 bit mode
        if (platform64) {
             stubExecPath.append("/"PRODUCT_PLATFORM_SUBDIR);  
        }
        stubExecPath.append("/"FCGI_STUB_NAME);
        if (system_stat(stubExecPath, &st) == -1) {
            log_error(LOG_VERBOSE, LOG_FUNCTION_NAME, _sn, _rq, "Fastcgistub path : %s", stubExecPath.data());
            lastError = STUB_STAT_FAILURE;
            goto res_failure;
        }
    }

    log_error(LOG_VERBOSE, LOG_FUNCTION_NAME, _sn, _rq, "Fastcgistub path : %s", stubExecPath.data());

#ifdef XP_UNIX
    // make sure the FastcgiStub has been properly secured
    // Specifically, if it's suid, it must be in a directory owned by us and
    // inaccessible to everyone else
    if ( st.st_mode & S_ISUID ) {
        // dirCgistub is stubExecPAth with any trailing '/'s stripped
        char* dirFcgistub = STRDUP(stubExecPath);
        char* t;
        for (t = dirFcgistub + strlen(dirFcgistub) - 1; (t > dirFcgistub) && (*t == '/'); t--) {
            *t = '\0';
        }

        // remove the tail path component (i.e. "/Fastcgistub") from dirFcgistub
        t = strrchr(dirFcgistub, '/');
        if (t) *t = '\0';

        // check permissions on dirFcgistub, the parent directory of Fastcgistub
        if ( stat(dirFcgistub, &st) ) {
            lastError = STUB_STAT_FAILURE;
            goto res_failure;
        } else if ( st.st_uid != getuid() ) {
            // we don't own the Cgistub parent directory
            lastError = STUB_NO_PERM;
            goto res_failure;
        } else if ( st.st_mode & (S_IWGRP | S_IWOTH) ) {
            // someone other than us can access the Cgistub parent directory
            lastError = STUB_NO_PERM;
            goto res_failure;
        }

        FREE(dirFcgistub);
    }

    return PR_SUCCESS;

#else
    return PR_Access(stubExecPath, PR_ACCESS_EXISTS);
#endif // XP_UNIX

res_failure:
    logError();
    return PR_FAILURE;
}


PRStatus StubExec::startStub(PRBool retryOption) {
    PRInt32 bytesRead = -1;
    lastError = NO_FCGI_ERROR;
    char *msg = (char *)&lastError;
    PRFileDesc *readPipe = NULL;
    PRFileDesc *writePipe = NULL;

    PRBool retries = PR_TRUE; // retry stub creation just once if stubpid file is deleted in this function.
    if (!stubSem) {
        lastError = SEMAPHORE_OPEN_ERROR;
        logError();
        return PR_FAILURE;
    }

retryStubStart:
    Sem_grab(stubSem);
    PRFileDesc *stubPidFd = PR_Open(stubPidFileName, PR_WRONLY|PR_CREATE_FILE|PR_EXCL, 0600);
    if (stubPidFd == NULL) {
        lastError = STUB_PID_FILE_CREATE_FAILURE;
        if (retryOption == PR_FALSE)
            logError();
        Sem_release(stubSem);
        return PR_FAILURE;
    }

    if (checkAccess() == PR_FAILURE) {
        goto startStubFailure;
    }

    if (PR_CreatePipe(&readPipe, &writePipe) == PR_FAILURE) {
        lastError = PIPE_CREATE_FAILURE;
        logError();
        goto startStubFailure;
    }

    PidType stubPid;

#ifdef XP_WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION procInfo;
    PRBool bSuccess;

    memset((void *)&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof (STARTUPINFO);
    si.lpReserved = NULL;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;
    si.lpDesktop = NULL;

    si.dwFlags = STARTF_USESTDHANDLES;

    // Set the standard input to the Socket Handle
    si.hStdInput  = (HANDLE) PR_FileDesc2NativeHandle(writePipe);
    // StdInput should be inheriatble
    bSuccess = SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT,
                   TRUE);
    if (!bSuccess) {
        return PR_FAILURE;
    }

    bSuccess = CreateProcess(stubExecPath,
                                *args,
                                NULL,
                                NULL,
                                TRUE,       /* Inheritable Handes inherited. */
                                CREATE_NO_WINDOW,
                                NULL,
                                NULL,
                                &si,
                                &procInfo);
    if (!bSuccess) {
        lastError = STUB_EXEC_FAILURE;
        logError();
        goto startStubFailure;
    } else {
        stubPid = procInfo.dwProcessId;
    }
#else
    if ((stubPid = fork()) == -1) {
        lastError = STUB_FORK_ERROR;
        logError();
        goto startStubFailure;
    }

    if (stubPid == 0) {  // child process
        PR_Close(readPipe);
        close(STDIN_FILENO);
        dup2(PR_FileDesc2NativeHandle(writePipe), STDIN_FILENO);
        PR_Close(writePipe);
        execv(stubExecPath, args);
        // if we are here, then exec failed.
        PluginError errVal = STUB_EXEC_FAILURE;
        PR_Write(PR_STDIN, (char *)&errVal, sizeof(PluginError));
        _exit(-1);
    }
#endif  // XP_WIN32
    PR_Close(writePipe);
    bytesRead = PR_Read(readPipe, msg, sizeof(PluginError));
    if (bytesRead > 0) {
        if (lastError != NO_FCGI_ERROR) {
            logError();
            goto startStubFailure;
        }
    } else {
        lastError = STUB_EXEC_FAILURE;
        logError();
        goto startStubFailure;
    }

    PR_fprintf(stubPidFd, "%d", stubPid);
    PR_Close(stubPidFd);
    if (readPipe) 
        PR_Close(readPipe);
    Sem_release(stubSem);
    return (connect());

startStubFailure:
    if (stubPidFd)
        PR_Close(stubPidFd);
    if (readPipe) 
        PR_Close(readPipe);
    if (writePipe)
        PR_Close(writePipe);
    PR_Delete(stubPidFileName);
    Sem_release(stubSem);
    return PR_FAILURE;
}

PRStatus StubExec::buildServerRequest(RequestMessageType type) {
    requestMsgSize = 0;
    size_t totlen;
    RequestHeader * pds;
    char *pHeader;
    char *pipeBuf;
    int len_execpath = 0;
    int len_bind_path = 0;
    int len_vs_id = 0;

    totlen  = offsetof( RequestHeader, dataHeader );

    if ( fcgiConfig->procInfo.execPath ) {
        len_execpath = strlen( fcgiConfig->procInfo.execPath ) + 1;
        totlen += ROUND_UP( (len_execpath + REQUEST_VECOFF), REQUEST_ALIGN );
    }
    if ( fcgiConfig->procInfo.bindPath ) {
        len_bind_path = strlen( fcgiConfig->procInfo.bindPath ) + 1;
        totlen += ROUND_UP( (len_bind_path + REQUEST_VECOFF), REQUEST_ALIGN );
    }
    if ( fcgiConfig->vsId ) {
        len_vs_id = strlen( fcgiConfig->vsId ) + 1;
        totlen += ROUND_UP( (len_vs_id + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    pipeBuf = (char*)MALLOC(totlen);
    if ( ! pipeBuf ) {
        requestMsg = NULL;
        requestMsgSize = 0;
        lastError = REQUEST_MEMORY_ALLOCATION_FAILURE;
        logError();
        return PR_FAILURE;
    }

    pds = (RequestHeader *) &pipeBuf[0];

    pds->reqMsgType = type;
    pds->reqFlags   = 0;
    pds->reqVersion = STUBVERSION;

    pHeader = (char *) &pds->dataHeader;
    if ( fcgiConfig->procInfo.execPath ) {
        pHeader += addRequestHeader( pHeader, RQT_PATH,
                                len_execpath, fcgiConfig->procInfo.execPath );
    }
    if ( len_bind_path ) {
        pHeader += addRequestHeader( pHeader, RQT_BIND_PATH,
                                len_bind_path, fcgiConfig->procInfo.bindPath );
    }
    if ( len_vs_id ) {
        pHeader += addRequestHeader( pHeader, RQT_VS_ID,
                                len_vs_id, fcgiConfig->vsId );
    }
    if ((totlen != (pHeader - (char*)pds)) || (totlen != (pHeader - pipeBuf))) {
        lastError = BUILD_REQ_ERROR;
        logError();
        return PR_FAILURE;
    }

    //
    //  set the size of the buffer
    //
    pds->reqMsgLen = totlen;
    requestMsgSize = totlen;
    requestMsg = pipeBuf;

    return PR_SUCCESS;
}


PRStatus StubExec::buildServerStartRequest() {
    requestMsgSize = 0;
    size_t              totlen;
    RequestHeader * pds;
    char *              pHeader;
    char *              pipeBuf;
    char *              envp_array   = NULL;
    char *              argv_array   = NULL;
    int                 len_envp_ar  = 0;
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
    int                 len_restart_interval = 0;
    int                 len_restart_on_exit = 0;
    int                 len_backlog_size = 0;
    int                 len_max_procs = 0;
    int                 len_min_procs = 0;
    int                 len_bind_path = 0;
    int                 len_num_failures = 0;
    int                 len_vs_id = 0;

    totlen  = offsetof( RequestHeader, dataHeader );

    if ( fcgiConfig->procInfo.execPath ) {
        len_execpath = strlen( fcgiConfig->procInfo.execPath ) + 1;
        totlen += ROUND_UP( (len_execpath + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->vsId ) {
        len_vs_id = strlen( fcgiConfig->vsId ) + 1;
        totlen += ROUND_UP( (len_vs_id + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.argv0 ) {
        len_argv0 = strlen( fcgiConfig->procInfo.argv0 ) + 1;
        totlen += ROUND_UP( (len_argv0 + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.chDir ) {
        len_chdir = strlen( fcgiConfig->procInfo.chDir ) + 1;
        totlen += ROUND_UP( (len_chdir + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.chRoot ) {
        len_chroot = strlen( fcgiConfig->procInfo.chRoot ) + 1;
        totlen += ROUND_UP( (len_chroot + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.userName ) {
        len_user = strlen( fcgiConfig->procInfo.userName ) + 1;
        totlen += ROUND_UP( (len_user + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.groupName ) {
        len_group = strlen( fcgiConfig->procInfo.groupName ) + 1;
        totlen += ROUND_UP( (len_group + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.priority) {
        len_nice = strlen( fcgiConfig->procInfo.priority ) + 1;
        totlen += ROUND_UP( (len_nice + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.rlimit_as) {
        len_rlimit_as = strlen( fcgiConfig->procInfo.rlimit_as ) + 1;
        totlen += ROUND_UP( (len_rlimit_as + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.rlimit_core) {
        len_rlimit_core = strlen( fcgiConfig->procInfo.rlimit_core ) + 1;
        totlen += ROUND_UP( (len_rlimit_core + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.rlimit_cpu) {
        len_rlimit_cpu = strlen( fcgiConfig->procInfo.rlimit_cpu ) + 1;
        totlen += ROUND_UP( (len_rlimit_cpu + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.rlimit_nofile) {
        len_rlimit_nofile = strlen( fcgiConfig->procInfo.rlimit_nofile ) + 1;
        totlen += ROUND_UP( (len_rlimit_nofile + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.minProcesses) {
        len_min_procs = strlen( fcgiConfig->procInfo.minProcesses ) + 1;
        totlen += ROUND_UP( (len_min_procs + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.maxProcesses ) {
        len_max_procs = strlen( fcgiConfig->procInfo.maxProcesses ) + 1;
        totlen += ROUND_UP( (len_max_procs + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.listenQueueDepth ) {
        len_backlog_size = strlen( fcgiConfig->procInfo.listenQueueDepth ) + 1;
        totlen += ROUND_UP( (len_backlog_size + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.bindPath ) {
        len_bind_path = strlen( fcgiConfig->procInfo.bindPath ) + 1;
        totlen += ROUND_UP( (len_bind_path + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.restartDelay ) {
        len_restart_interval = strlen( fcgiConfig->procInfo.restartDelay ) + 1;
        totlen += ROUND_UP( (len_restart_interval + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if (fcgiConfig->procInfo.restartOnExit) {
        len_restart_on_exit = strlen( fcgiConfig->procInfo.restartOnExit ) + 1;
        totlen += ROUND_UP( (len_restart_on_exit + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if (fcgiConfig->procInfo.numFailures) {
        len_num_failures = strlen( fcgiConfig->procInfo.numFailures ) + 1;
        totlen += ROUND_UP( (len_num_failures + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    if ( fcgiConfig->procInfo.env ) {
        envp_array = buildEnvArray( -1, fcgiConfig->procInfo.env, &len_envp_ar);
        if (len_envp_ar)
          totlen += ROUND_UP( (len_envp_ar + REQUEST_VECOFF), REQUEST_ALIGN );
    }
    if ( fcgiConfig->procInfo.args ) {
        argv_array = buildEnvArray( -1, fcgiConfig->procInfo.args, &len_argv_ar);
        if (len_argv_ar)
          totlen += ROUND_UP( (len_argv_ar + REQUEST_VECOFF), REQUEST_ALIGN );
    }

    pipeBuf = (char*)MALLOC(totlen);
    if ( ! pipeBuf ) {
        requestMsg = NULL;
        requestMsgSize = 0;
        if (envp_array)
            FREE(envp_array);
        if (argv_array)
            FREE(argv_array);
        lastError = REQUEST_MEMORY_ALLOCATION_FAILURE;
        logError();
        return PR_FAILURE;
    }

    pds = (RequestHeader *) &pipeBuf[0];

    pds->reqMsgType = RQ_START;
    pds->reqFlags   = 0;
    pds->reqVersion = STUBVERSION;

    pHeader = (char *) &pds->dataHeader;
    if ( fcgiConfig->procInfo.execPath ) {
        pHeader += addRequestHeader( pHeader, RQT_PATH,
                                len_execpath, fcgiConfig->procInfo.execPath );
    }
    if ( len_vs_id ) {
        pHeader += addRequestHeader( pHeader, RQT_VS_ID,
                                len_vs_id, fcgiConfig->vsId );
    }
    if ( fcgiConfig->procInfo.argv0 ) {
        pHeader += addRequestHeader( pHeader, RQT_ARGV0,
                                len_argv0, fcgiConfig->procInfo.argv0 );
    }
    if ( len_chdir ) {
        pHeader += addRequestHeader( pHeader, RQT_CHDIRPATH,
                                len_chdir, fcgiConfig->procInfo.chDir );
    }
    if ( len_chroot ) {
        pHeader += addRequestHeader( pHeader, RQT_CHROOTPATH,
                                len_chroot, fcgiConfig->procInfo.chRoot );
    }
    if ( len_user ) {
        pHeader += addRequestHeader( pHeader, RQT_USERNAME,
                                len_user, fcgiConfig->procInfo.userName );
    }
    if ( len_group ) {
        pHeader += addRequestHeader( pHeader, RQT_GROUPNAME,
                                len_group, fcgiConfig->procInfo.groupName );
    }
    if ( len_nice ) {
        pHeader += addRequestHeader( pHeader, RQT_NICE,
                                len_nice, fcgiConfig->procInfo.priority );
    }
    if ( len_rlimit_as ) {
        pHeader += addRequestHeader( pHeader, RQT_RLIMIT_AS,
                                len_rlimit_as, fcgiConfig->procInfo.rlimit_as );
    }
    if ( len_rlimit_core ) {
        pHeader += addRequestHeader( pHeader, RQT_RLIMIT_CORE,
                                len_rlimit_core, fcgiConfig->procInfo.rlimit_core );
    }
    if ( len_rlimit_cpu ) {
        pHeader += addRequestHeader( pHeader, RQT_RLIMIT_CPU,
                                len_rlimit_cpu, fcgiConfig->procInfo.rlimit_cpu );
    }
    if ( len_rlimit_nofile ) {
        pHeader += addRequestHeader( pHeader, RQT_RLIMIT_NOFILE,
                                len_rlimit_nofile, fcgiConfig->procInfo.rlimit_nofile );
    }
    if ( len_min_procs ) {
        pHeader += addRequestHeader( pHeader, RQT_MIN_PROCS,
                                len_min_procs, fcgiConfig->procInfo.minProcesses );
    }
    if ( len_max_procs ) {
        pHeader += addRequestHeader( pHeader, RQT_MAX_PROCS,
                                len_max_procs, fcgiConfig->procInfo.maxProcesses );
    }
    if ( len_backlog_size ) {
        pHeader += addRequestHeader( pHeader, RQT_LISTENQ_SIZE,
                                len_backlog_size, fcgiConfig->procInfo.listenQueueDepth );
    }
    if ( len_bind_path ) {
        pHeader += addRequestHeader( pHeader, RQT_BIND_PATH,
                                len_bind_path, fcgiConfig->procInfo.bindPath );
    }
    if ( len_restart_interval ) {
        pHeader += addRequestHeader( pHeader, RQT_RESTART_INTERVAL,
                                len_restart_interval, fcgiConfig->procInfo.restartDelay );
    }
    if ( len_envp_ar ) {
        pHeader += addRequestHeader( pHeader, RQT_ENVP, len_envp_ar, envp_array );
        FREE(envp_array);
    }
    if ( len_argv_ar ) {
        pHeader += addRequestHeader( pHeader, RQT_ARGV, len_argv_ar, argv_array );
        FREE(argv_array);
    }
    if ( len_restart_on_exit ) {
        pHeader += addRequestHeader( pHeader, RQT_RESTART_ON_EXIT,
                                len_restart_on_exit, fcgiConfig->procInfo.restartOnExit );
    }
    if ( len_num_failures ) {
        pHeader += addRequestHeader( pHeader, RQT_NUM_FAILURE,
                                len_num_failures, fcgiConfig->procInfo.numFailures );
    }


    if ((totlen != (pHeader - (char*)pds)) || (totlen != (pHeader - pipeBuf))) {
        lastError = BUILD_REQ_ERROR;
        logError();
        return PR_FAILURE;
    }

    //
    //  set the size of the buffer
    //
    pds->reqMsgLen = totlen;
    requestMsgSize = totlen;
    requestMsg = pipeBuf;

    return PR_SUCCESS;
}

PRStatus StubExec::sendRequest(RequestMessageType msgType) {
    PRBool flagError = PR_FALSE;


    switch(msgType) {
        case RQ_START:
            if (buildServerStartRequest() == PR_FAILURE) {
                    flagError = PR_TRUE;
            }
            break;
        case RQ_OVERLOAD:
            if (buildServerRequest(RQ_OVERLOAD) == PR_FAILURE) {
                    flagError = PR_TRUE;
            }
            break;
        case RQ_VS_SHUTDOWN:
            if (buildServerRequest(RQ_VS_SHUTDOWN) == PR_FAILURE) {
                    flagError = PR_TRUE;
            }
            break;
        default:
            lastError = UNKNOWN_STUB_REQ_TYPE;
            logError();
            flagError = PR_TRUE;
    }

    if (flagError)
        return PR_FAILURE;

    if (!stubFd) {
        if (connect() == PR_FAILURE)
            return PR_FAILURE;
    }

    PRUint32 bytesSent = 0;
    PRUint16 bytesRead = 0;
#ifdef XP_UNIX
    bytesSent = PR_Send(stubFd, requestMsg, requestMsgSize, 0, PR_INTERVAL_NO_TIMEOUT);
#else
    if (WriteFile(stubFd, requestMsg, requestMsgSize, (unsigned long *)&bytesSent, NULL) == 0) {
        lastError = REQUEST_SEND_FAILURE;
        logError();
        return PR_FAILURE;
    }
#endif // XP_UNIX
    if (bytesSent < sizeof(requestMsg)) {
        lastError = REQUEST_SEND_FAILURE;
        logError();
        return PR_FAILURE;
    }

    // wait for the stub response
    PluginError errVal;
#ifdef XP_UNIX
    if (PR_Recv(stubFd, (char *)&errVal, sizeof(PluginError), 0, fcgiConfig->poll_timeout) < 1) {
#else
    if (ReadFile(stubFd, (char *)&errVal, sizeof(PluginError), (unsigned long *)&bytesRead, NULL) < 1) {
#endif // XP_UNIX
        lastError = ERROR_RESPONSE;
        logError();
        return PR_FAILURE;
    }

    lastError = errVal;
    if (lastError != NO_FCGI_ERROR) {
        logError();
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}


/*
 *  convert an array of char * (char **) to a linear buffer
 */
char * StubExec::buildEnvArray( int nargs, char ** pparg_save, int * parray_len ) {
    int     totsz = 0;
    int     ndx;
    char ** ppargs;
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
//  add header element; return the length added
//
int StubExec::addRequestHeader(void *headerPtr, RequestType type, int datalen, const char * vector) {
    register RequestDataHeader *header = (RequestDataHeader *)headerPtr;
    register int pad;
    register int totalLen = datalen + REQUEST_VECOFF;
    header->type = type;
    header->len  = totalLen;
    memcpy ( &header->vector, vector, datalen );


    /* pad if necessary, assuming power-of-two */
    pad = ROUND_UP( totalLen, REQUEST_ALIGN ) - totalLen;
    if ( pad ) {
        char * padit = &header->vector + datalen;
        totalLen += pad;
        while ( pad-- ) {
            *padit = '\0';
            padit++;
        }
    }

    return (totalLen);
}

void StubExec::logError() {
    switch(lastError) {
        case REQUEST_MISSING_OR_INVALID_PARAM :
            log_error(LOG_MISCONFIG, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_missing_params));
            break;
        case STUB_START_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_stub_start_error));
            break;
        case STUB_BIND_ERROR :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_bind_error));
            break;
        case STUB_LISTEN_ERROR :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_listen_error));
            break;
        case STUB_ACCEPT_ERROR :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_stub_accept_error));
            break;
        case PIPE_CREATE_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_pipe_create_error));
            break;
        case CHILD_FORK_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_fork_error));
            break;
        case STUB_EXEC_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_stub_exec_failure));
            break;
        case STUB_SOCKET_CREATION_FAILURE :
        case SERVER_SOCKET_CREATION_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_socket_create_error));
            break;
        case STAT_FAILURE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_stat_failure), fcgiConfig->procInfo.execPath);
            break;
        case NO_EXEC_PERMISSION:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_no_exec_permission));
            break;
        case WRITE_OTHER_PERMISSION :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_write_other));
            break;
        case INVALID_REQUEST_TYPE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_unknow_stub_request_type));
            break;
        case PROC_EXISTS :
            log_error(LOG_VERBOSE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_process_exists));
            break;
        case SET_RLIMIT_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_set_rlimit_failure));
            break;
        case SET_NICE_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_set_priority_failure));
            break;
        case SET_GROUP_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_set_group_failure));
            break;
        case SET_USER_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_set_user_failure));
            break;
        case SET_CHDIR_FAILURE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_chdir_failure));
            break;
        case SET_CHROOT_FAILURE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_chroot_failure));
            break;
        case INVALID_PARAM_VALUE :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_invalid_param_value));
            break;
        case INVALID_USER :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_invalid_user), fcgiConfig->procInfo.userName, fcgiConfig->procInfo.execPath);
            break;
        case INVALID_GROUP :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_invalid_group), fcgiConfig->procInfo.groupName, fcgiConfig->procInfo.execPath);
            break;
        case CHILD_EXEC_FAILURE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_application_exec_failure), fcgiConfig->procInfo.execPath);
            break;
        case PROC_DOES_NOT_EXIST :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_no_application_info), fcgiConfig->procInfo.execPath);
            break;
        case SERVER_BIND_ERROR:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_application_socket_bind_error), fcgiConfig->procInfo.execPath);
            break;
        case SERVER_LISTEN_ERROR:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_application_socket_listen_error), fcgiConfig->procInfo.execPath);
            break;
        case REQUEST_INCOMPLETE_HEADER:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_incomplete_request));
            break;
        case STUB_FORK_ERROR:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_fork_error));
            break;
        case REQUEST_MEMORY_ALLOCATION_FAILURE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_memory_allocation_failure));
            break;
        case STUB_STAT_FAILURE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_stub_stat_failure), stubExecPath.data());
            break;
        case STUB_NO_PERM:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_no_permission));
            break;
        case STUB_PID_FILE_CREATE_FAILURE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_stub_pid_create_error));
            break;
        case SEMAPHORE_OPEN_ERROR:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_semaphore_creation_failure));
            break;
        case STUB_NOT_RESPONDING:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_stub_not_responding));
            break;
        case BUILD_REQ_ERROR:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_request_creation_error));
            break;
        case UNKNOWN_STUB_REQ_TYPE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_unknow_stub_request_type));
            break;
        case REQUEST_SEND_FAILURE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_stub_request_send_error));
            break;
        case ERROR_RESPONSE:
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_invalid_stub_response));
            break;
        default :
            log_error(LOG_FAILURE, LOG_FUNCTION_NAME, _sn, _rq, GetString(DBT_internal_error));
            break;
    }
}

