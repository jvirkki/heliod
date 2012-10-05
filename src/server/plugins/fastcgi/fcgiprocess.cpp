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

#ifdef XP_UNIX
  #include <pwd.h>
  #include <grp.h>
  #include <ctype.h>
  #include <sys/types.h>
  #include <dirent.h>
  #include <sys/types.h>
  #include <signal.h>
  #include <sys/wait.h>
  #include <sys/resource.h>
#else
  #include <windows.h>
#endif // XP_UNIX

#include <nspr.h>
#include <private/pprio.h>
#include <plstr.h>
#include "nsapi.h"
#include "fastcgi.h"
#include "constants.h"
#include "util.h"
#include "serverconfig.h"
#include "fcgiprocess.h"

FcgiProcess::FcgiProcess(FileDescPtr connFd, FILE *fd, PRBool verbose)
: clientFd(connFd), logFd(fd), verboseSet(verbose),
  currentNumFails(0), totalNumFails(0), serverFd(-1)
{
    memset((char *)&procInfo, 0, sizeof(procInfo));
    children = new GenericList();
}

FcgiProcess::~FcgiProcess() {
    PRBool serverFdClosed = PR_FALSE;
    terminateChilds();

    if(serverFd > 0) {
#ifdef XP_UNIX
        close(serverFd);
#else
        CloseHandle((HANDLE)serverFd);
#endif //XP_UNIX
        serverFd = -1;
        serverFdClosed = PR_TRUE;
    }
    
    //clear the memory from the ProcessInfo structure.
    if(&procInfo) {
        if(procInfo.execPath) {
            PL_strfree(procInfo.execPath);
            procInfo.execPath = NULL;
        }
        if(procInfo.bindPath) {
#ifdef XP_UNIX
            struct stat st;
            if(serverFdClosed && (!stat(procInfo.bindPath, &st))) {
                PR_Delete(procInfo.bindPath);
            } 
#endif //XP_UNIX
            PL_strfree(procInfo.bindPath);
            procInfo.bindPath = NULL;
        }
        if(procInfo.servAddr) {
            delete (procInfo.servAddr);
            procInfo.servAddr = NULL;
        }
        //memory will be freed when procInfo.args are freed
        if(procInfo.argv0) {
            procInfo.argv0 = NULL;
        }
        if(procInfo.env) {
            fcgi_util_env_free(procInfo.env);
            procInfo.env = NULL;
        }
        if(procInfo.args) {
            fcgi_util_env_free(procInfo.args);
            procInfo.args = NULL;
        }
        if(procInfo.userName) {
            PL_strfree(procInfo.userName);
            procInfo.userName = NULL;
        }
        if(procInfo.groupName) {
            PL_strfree(procInfo.groupName);
            procInfo.groupName = NULL;
        }
        if(procInfo.chDir) {
            PL_strfree(procInfo.chDir);
            procInfo.chDir = NULL;
        }
        if(procInfo.chRoot) {
            PL_strfree(procInfo.chRoot);
            procInfo.chRoot = NULL;
        }
        if(procInfo.maxProcesses) {
            PL_strfree(procInfo.maxProcesses);
            procInfo.maxProcesses = NULL;
        }
        if(procInfo.minProcesses) {
            PL_strfree(procInfo.minProcesses);
            procInfo.minProcesses = NULL;
        }
        if(procInfo.priority) {
            PL_strfree(procInfo.priority);
            procInfo.priority = NULL;
        }
        if(procInfo.rlimit_as) {
            PL_strfree(procInfo.rlimit_as);
            procInfo.rlimit_as = NULL;
        }
        if(procInfo.rlimit_core) {
            PL_strfree(procInfo.rlimit_core);
            procInfo.rlimit_core = NULL;
        }
        if(procInfo.rlimit_cpu) {
            PL_strfree(procInfo.rlimit_cpu);
            procInfo.rlimit_cpu = NULL;
        }
        if(procInfo.rlimit_nofile) {
            PL_strfree(procInfo.rlimit_nofile);
            procInfo.rlimit_nofile = NULL;
        }
        if(procInfo.listenQueueDepth) {
            PL_strfree(procInfo.listenQueueDepth);
            procInfo.listenQueueDepth = NULL;
        }
        if(procInfo.restartOnExit) {
            PL_strfree(procInfo.restartOnExit);
            procInfo.restartOnExit = NULL;
        }
        if(procInfo.numFailures) {
            PL_strfree(procInfo.numFailures);
            procInfo.numFailures = NULL;
        }
        if(procInfo.restartDelay) {
            PL_strfree(procInfo.restartDelay);
            procInfo.restartDelay = NULL;
        }
    }    
   
    // clear the vsIds in the vsIdList
    for (int i=0; i < vsIdList.length(); i++) {
       delete vsIdList[i]; 
    }
}

void FcgiProcess::initProcess() {
    maxProcs = getIntParameter(procInfo.maxProcesses);
    minProcs = getIntParameter(procInfo.minProcesses);
    if(!procInfo.restartDelay)
        procInfo.restartDelayTime = PR_SecondsToInterval(60 * 60);
    if(minProcs <= 0)
        minProcs = getIntParameter(DEFAULT_MIN_PROC);
    if(maxProcs < minProcs)
        maxProcs = minProcs;
    totalNumFails = getIntParameter(procInfo.numFailures);
    if(totalNumFails < 1) {
        //default it to 3
        totalNumFails = 3;
    }
}

int FcgiProcess::getIntParameter(const char *val) {
    int intVal = 0;
    char *temp = "";
    if(val == NULL)
        return 0;
    intVal = strtol(val, &temp, 10);
    if(temp != NULL && strlen(temp) > 0)
        return 0;
    return (intVal);
}

PRStatus FcgiProcess::addChild(PidType cPid) {
    if(children->length() > maxProcs)
        return PR_FAILURE;

    ChildProcessData *procData = PR_NEW(ChildProcessData);
    procData->pid = cPid;
    procData->epoch = PR_IntervalNow();
    children->append(procData);

    return PR_SUCCESS;
}

void FcgiProcess::addVsId(char *vsid) {
    if(!vsIdList.contains(vsid)) {
        vsIdList.append(vsid);
    }
}

void FcgiProcess::removeVsId(char *vsid) {
    if(vsIdList.contains(vsid)) {
        vsIdList.remove(vsid);
    }
}

PRBool FcgiProcess::containsVsId(char *vsid) {
    char *vId = NULL;
    int len = vsIdList.length();
    for(int i=0; i<len; i++) {
        vId = (char *)vsIdList[i];
        if(!PL_strcmp(vId, vsid))
            return PR_TRUE;
    }

    return PR_FALSE;
}

ChildProcessData *FcgiProcess::childExists(PidType p) {
    int len = children->length();
    ChildProcessData *child;
    for(int i=0; i<len; i++) {
        child = (ChildProcessData *)(*children)[i];
        if(child->pid ==p)
           return child;
    }

    return NULL;
}

FcgiProcess *FcgiProcess::lookupChildPid(GenericList procList, PidType p) {
    int len = procList.length();
    FcgiProcess *proc = NULL;

    for(int i=0; i<len; i++) {
        proc = (FcgiProcess *)procList[i];
        if(proc) {
            if(proc->childExists(p) != NULL)
                return proc;
        } //if
    } //for

    return NULL;
}

void FcgiProcess::removeChild(PidType p) {
    ChildProcessData *child = childExists(p);
    if(child) {
        children->remove(child);
    }
}

int FcgiProcess::operator==(const FcgiProcess& right) const
{
    return (strmatch(procInfo.execPath, right.procInfo.execPath) &&
           strmatch(procInfo.bindPath, right.procInfo.bindPath));
}

int FcgiProcess::operator!=(const FcgiProcess& right) const
{
    return !operator==(right);
}

void FcgiProcess::printMsg(const char *msg) {
    if(verboseSet && logFd) {
        if(msg)
            fprintf(logFd, "%s\n", msg);
        else
            fprintf(logFd, "NULL\n");
        fflush(logFd);
    }
}

void FcgiProcess::sendErrorStatus(PluginError errorType) {
#ifdef XP_UNIX
    PR_Send(clientFd, (char *)&errorType, sizeof(PluginError), 0, PR_INTERVAL_NO_TIMEOUT);
#else
    PRUint32 bytesWritten = -1;
   // Write the reply to the pipe.
    BOOL fSuccess = WriteFile(clientFd,            // handle to pipe
                              (char *)&errorType,   // buffer to write from
                              sizeof(PluginError), // number of bytes to write
                              (unsigned long *)&bytesWritten,   // number of bytes written
                              NULL);        // not overlapped I/O
#endif //XP_UNIX
}

void FcgiProcess::terminateChilds() {

    ChildProcessData *data;
    int len = getNumChildren();
    int i = 0;

    for(i=0; i<len; i++) {
        data = getChild(i);
        if(verboseSet) {
            fprintf(logFd, "terminating child - %d\n", data->pid);
            fflush(logFd);
        }
        if(data) {
#ifdef XP_UNIX
            kill(data->pid, SIGTERM);
#else
            HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, data->pid);
            if(hProc != NULL) { //process is still running
                TerminateProcess(hProc, 0);
            }
            delete data;
            data = NULL;
#endif //XP_UNIX
        }
    } //for

#ifdef XP_UNIX
    PR_Sleep(PR_SecondsToInterval(1));
    int status = -1;
    for(i=0; i<len; i++) {
        data = getChild(i);
        if(data) {
            if(waitpid(data->pid, &status, WNOHANG) <= 0) { //child is still active
                //send KILL signal
                if(verboseSet) {
                    fprintf(logFd, "killing child - %d\n", data->pid);
                    fflush(logFd);
                }
                kill(data->pid, SIGKILL);
            }

            delete data;
            data = NULL;
        } //if
    } //for
#endif //XP_UNIX

    if(len > 0) {
        if(&procInfo && procInfo.bindPath && isUds(procInfo.bindPath)) {
            PR_Delete(procInfo.bindPath);
        }
    }

    children->flush();
    delete children;
    children = NULL;
}

PluginError FcgiProcess::hasAccess() {
#ifdef XP_UNIX
    struct passwd* pw = NULL;
    struct group* gr;
    struct stat st;
    char* path = procInfo.execPath;

    // nice
    char *end = "";
    procInfo.procPriority = (procInfo.priority ? strtol(procInfo.priority, &end, 10) : 0);
    if(end != NULL && strlen(end) > 0  ) {
       //invlid value
       procInfo.procPriority = 0;
    }

    // if a specific user was requested...
    if (procInfo.userName) {
        // lookup the user
        pw = getpwnam(procInfo.userName);
        if (!pw) {
            return INVALID_USER;
        }
    }

    // check group
    // do this before we set the uid, as our current euid should be root
    // (so we can set any gid) if Cgistub is supposed to allow group changes
    if (procInfo.groupName) {
        // lookup the group
        struct group* gr;
        gr = getgrnam(procInfo.groupName);
        if (!gr) {
            return INVALID_GROUP;
        }
    }

#endif //XP_UNIX

    return NO_FCGI_ERROR;
}

/*
 *  parse string and call setrlimit with the result
 *  returns the errno value on error, 0 otherwise
 */
#ifdef XP_UNIX
static int setrlimitWrapper(int resource, char *data) {
    struct rlimit rl;
    long limit1;
    long limit2;
    char* end;
    int rc;

    if (!data) return 0; // success, no params to set

    //we don't do the string-to-rlim_t conversion properly when rlim_t is
    //long long and the numeric values from string won't fit in a long

    // get the first number
    limit1 = strtol(data, &end, 10);
    if (end == data) return EINVAL; // invalid parameter

    // skip past any white space and punctuation
    data = end;
    while (*data && (isspace(*data) || ispunct(*data))) data++;

    // get the second number
    limit2 = strtol(data, &end, 10);
    if (end == data) limit2 = limit1; // no second number given

    // rlim_cur is the lesser of limit1 and limit2, rlim_max the greater
    memset(&rl, 0, sizeof(rl));
    rl.rlim_cur = min(limit1, limit2);
    rl.rlim_max = max(limit1, limit2);

    // set params
    if (setrlimit(resource, &rl)) return errno; // failure

    return 0; // success
}
#endif //XP_UNIX

#ifdef XP_UNIX
static PluginError childCode(ProcessInfo& param) {
    uid_t caller_uid;
    uid_t child_uid;
    gid_t child_gid;
    struct passwd* pw = NULL;
    char* path = param.execPath;
    char *dir = param.chDir;
    struct sigaction sa;
    struct stat st;
    int rc;

    // remember the uid of whoever executed us
    caller_uid = getuid();
    child_uid = caller_uid;
    child_gid = getgid();

    // setrlimit
    if ((rc = setrlimitWrapper(RLIMIT_AS, param.rlimit_as)) ||
        (rc = setrlimitWrapper(RLIMIT_CORE, param.rlimit_core)) ||
        (rc = setrlimitWrapper(RLIMIT_CPU, param.rlimit_cpu)) ||
        (rc = setrlimitWrapper(RLIMIT_NOFILE, param.rlimit_nofile))) {
        // parse or setrlimit error
        return SET_RLIMIT_FAILURE;
    }

    if(param.procPriority != 0) {
        errno = 0;
        if ((nice(param.procPriority) == -1) && errno) {
            return SET_NICE_FAILURE;
        }
    }

    // if a specific user was requested...
    if (param.userName) {
        // lookup the user
        pw = getpwnam(param.userName);
    }

    // set group
    // do this before we set the uid, as our current euid should be root
    // (so we can set any gid) if Cgistub is supposed to allow group changes
    if (param.groupName) {
        // lookup the group
        struct group* gr;
        gr = getgrnam(param.groupName);

        // change to the group
        if (setgid(gr->gr_gid)) {
            return SET_GROUP_FAILURE;
        }
        child_gid = gr->gr_gid;
    } else if (pw) {
        // no specific group was requested, but we're setting the user, so
        // setgid to the user's base gid
        if (setgid(pw->pw_gid)) {
            return SET_USER_FAILURE;
        }
        child_gid = pw->pw_gid;
    }

    // if we've been asked to set the user...
    if (pw) {
        // set the supplementary group access list for this user
        initgroups(pw->pw_name, pw->pw_gid);
    }

    // chroot, if requested
    // Some important notes:
    // 1. this is done after we do all the user/group lookup stuff (therefore
    //    we're using the /etc/passwd, etc. relative to the server's root, not
    //    our chroot'd-to root)
    // 2. this is done before we do a setuid (as we need to be root to chroot)
    if (param.chRoot) {
        int chrootLen;

        // do the chroot
        if (chroot(param.chRoot)) {
            return SET_CHROOT_FAILURE;
        }

        // find out how long ch_root is, not including any trailing '/'
        chrootLen = strlen(param.chRoot);
        if (chrootLen && (param.chRoot)[chrootLen - 1] == '/') chrootLen--;

        // if the front of path is the same as ch_root...
        if ((strlen(path) > chrootLen) && (path[chrootLen] == '/') &&
            (!memcmp(path, param.chRoot, chrootLen)))
        {
            // path is the path as seen by the server; adjust it so it's
            // relative to our newly chroot'd-to root
            path += chrootLen;
        }

        // if the front of dir is the same as ch_root...
        if (dir && *dir && (strlen(dir) >= chrootLen) &&
            (dir[chrootLen] == '/' || dir[chrootLen] == '\0' ) &&
            (!memcmp(dir, param.chRoot, chrootLen)))
        {
            // dir is the path as seen by the server; adjust it so it's
            // relative to our newly chroot'd-to root
            dir += chrootLen;
        }
    }

    // if we've been asked to set the user...
    if (pw) {
        // set the user
        if (setuid(pw->pw_uid)) {
            return SET_USER_FAILURE;
        }
        child_uid = pw->pw_uid;
    } else if (geteuid() != caller_uid) {
        // caller didn't say who she wanted to be, but she's not herself
        // we're probably effectively root, try to change to the server user
        if (setuid(caller_uid)) {
            return SET_USER_FAILURE;
        }
        child_uid = caller_uid;
    }

    // set the cwd
    if (dir && *dir) {
        // chdir to path from request
        if (chdir(dir)) {
            return SET_CHDIR_FAILURE;
        }
    } else {
        // no directory specified, use the one from the program path
        char* t = const_cast<char *>(strrchr(path, '/'));
        if (t > path) {
            *t = '\0';
            if (chdir(path)) {
                return SET_CHDIR_FAILURE;
            }
            *t = '/';
        } else {
            if (chdir("/")) {
                return SET_CHDIR_FAILURE;
            }
        }
    }

    
    // get permissions of the program we're trying to execute
    if (stat(path, &st)) {
        return STAT_FAILURE;
    }

    // are we trying to execute something we don't own then check the
    //exec permission. "exec" will handle other checks
    if (st.st_uid != child_uid) { //diffrent user
    if(st.st_gid != child_gid) { // does not belong to the program group
        if(! (st.st_mode & S_IXOTH)) { //no exec permission to others
            return NO_EXEC_PERMISSION;
        }
    } else if(! (st.st_mode & S_IXGRP)) { //no exec permission to the group
        return NO_EXEC_PERMISSION;
    }
    } else if(! (st.st_mode & S_IXUSR )) {  //no exec permission to the owner
        return NO_EXEC_PERMISSION;
    }

    // are we trying to execute something someone else can write to?
    if (st.st_mode & (S_IWGRP | S_IWOTH)) {
        return WRITE_OTHER_PERMISSION;
    }

    // disable SIGPIPE
    sa.sa_flags = 0;
    sigemptyset( &sa.sa_mask );
    sa.sa_handler = SIG_DFL;
    sigaction( SIGPIPE, &sa, NULL );

    // now, exec the child
    if ( param.env ) {
        execve( path, param.args, param.env );
    } else {
        execv( path, param.args );
    }

    // oops! ..... we're still here
    return CHILD_EXEC_FAILURE;
}

/* Ths Cgistub process messed around with SIGCHLD.
 * This can cause fork/exec etc in the cgi to fail.
 * Worse is the case when the cgis are perl/shell scripts doing exec
 * We should reset signals to default.
 */
static void setupFcgiSignalDisposition() {
    sigset_t threadSignals;
    sigemptyset(&threadSignals);
    sigfillset(&threadSignals);

    //unblock the signals in the main thread
    pthread_sigmask(SIG_UNBLOCK, &threadSignals, NULL);
}
#endif //XP_UNIX

#ifdef XP_WIN32
char *FcgiProcess::formWinArgs() {
    NSString wArgs;
    int i = 0;
    for(; procInfo.args[i] != NULL; i++) {
        wArgs.append(procInfo.args[i]);
        wArgs.append(" ");
    }
    if(i > 0) {
        wArgs.append("\0");
    }
    return (wArgs.length() > 0 ? PL_strdup((char *)wArgs.data()) : NULL);
}
#endif //XP_WIN32

#ifdef XP_WIN32
char *FcgiProcess::getWindowsEnvironmentVars() {
    int totalLen = 0;
    int envLen = 0;
    char *newEnv;
    char *newEnvPtr;

    char* currentEnv = (char *)GetEnvironmentStrings();
    char *currentEnvPtr = currentEnv;
    int curEnvVarLen = 0;

    while ( *currentEnvPtr ) {
        curEnvVarLen = strlen(currentEnvPtr);
        totalLen += curEnvVarLen + 1;
        currentEnvPtr += curEnvVarLen + 1;
    }

    int currentEnvLen = totalLen;

    int ii = 0;
    for (; procInfo.env[ii]; ii++)
        totalLen += (strlen(procInfo.env[ii])+1);

    totalLen++;  // terminating NULL byte

    if ( (newEnv = (char *)PR_Malloc(sizeof(char) * totalLen)) == NULL)
        return NULL;

    newEnvPtr = newEnv;

    //Copy current environment to newEnv
    int index = 0;

    memcpy(newEnvPtr, currentEnv, currentEnvLen);
    newEnvPtr += currentEnvLen;

    for (ii = 0; procInfo.env[ii]; ii++) {
        envLen = strlen(procInfo.env[ii]) + 1;
        memcpy(newEnvPtr, procInfo.env[ii], envLen);
        newEnvPtr += envLen;
    }

    *newEnvPtr = '\0';

    //Free current environment string
    FreeEnvironmentStrings(currentEnv);

    return newEnv;
}
#endif //XP_WIN32


/*
 *----------------------------------------------------------------------
 *
 * execFcgiProgram --
 *
 *  This procedure is invoked to start a fast cgi program.  It
 *      calls an OS specific routine to start the process and sets a
 *      wait handler on the process.
 *
 *      NOTE: At the present time, the only error than can be returned is
 *            SERVER_UNAVAILABLE which means a fork() failed (WS_errno is set).
 *
 * Results:
 *      Returns 0 if successful or a valid server error if not.
 *
 * Side effects:
 *      Possibly many.
 *
 *----------------------------------------------------------------------
 */
PluginError FcgiProcess::execFcgiProgram(PRBool lifecycleCall) {
    int err = PR_FALSE;
//    int listenFd;
    PidType childPid = -1;
    PluginError errVal;

    if((errVal = hasAccess()) != NO_FCGI_ERROR) {
        if(!lifecycleCall)
            sendErrorStatus(errVal);
        return errVal;
    }


#ifdef XP_UNIX
    int statusPipe[2];
    // The call to ExecFcgiProgram will only return once.  It will not
    // return as a result of the child's creation.
    if ( pipe(statusPipe) == -1 ) {
        return PIPE_CREATE_FAILURE;
    }

    /* enable close-on-exec to monitor if the exec() worked */
    fcntl(statusPipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(statusPipe[1], F_SETFD, FD_CLOEXEC);    // Fork the fcgi process.
    fcntl(fileno(logFd), F_SETFD, FD_CLOEXEC);    // On exec, close the log file FD.

    childPid = fork();
    if(childPid < 0) {
        return CHILD_FORK_FAILURE;
    }

    if(childPid == 0) {    //child process
        //PR_Close(readPipe);
        close(statusPipe[0]);
        close(FCGI_LISTENSOCK_FILENO);
        if(serverFd != FCGI_LISTENSOCK_FILENO) {
            dup2(serverFd, FCGI_LISTENSOCK_FILENO);
        }

        // Close any file descriptors we may have gotten from the parent
        // process.  The only FD left open is the FCGI listener socket.
        int     fdlimit;

        // first, close all our potentially nasty inherited fds
        // leave stdin, stdout & ..err

        fdlimit = sysconf( _SC_OPEN_MAX );

        for(int i=0; i < fdlimit ; i++) {
            if ((i != FCGI_LISTENSOCK_FILENO) && (i != statusPipe[1]) &&
                (i != fileno(logFd)))
               close(i);
        }

        setupFcgiSignalDisposition();
        PluginError errorType = NO_FCGI_ERROR;
        errorType = childCode(procInfo);

        if (verboseSet && logFd) {
            fprintf(logFd, "Process creation failed with the error %d: %s\n", errno, strerror(errno));
            fflush(logFd);
            fclose(logFd);
        }

        write(statusPipe[1], (char *)&errorType, sizeof(PluginError));
        _exit(-1);

    } else { //parent
        close(statusPipe[1]);

//again:
        PluginError result = NO_FCGI_ERROR;
        PRInt32 bytesRead = -1;
        do {
            bytesRead = read(statusPipe[0], (char *)&result, sizeof(PluginError));
        } while((bytesRead < 0) && (errno == EINTR));

        close(statusPipe[0]);
        if(!bytesRead) { // pipe was closed on successful exec
            addChild(childPid);
            if(verboseSet) {
                fprintf(logFd, "created child - %d\n", childPid);
                fflush(logFd);
            }
        }

        if(!lifecycleCall)
            sendErrorStatus(result);
        return result;
    }
#else
    STARTUPINFO si;
    PROCESS_INFORMATION pInfo;
    BOOL bSuccess;

    memset((void *)&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof (STARTUPINFO);
    si.lpReserved = NULL;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;
    si.lpDesktop = NULL;
    si.dwFlags = STARTF_USESTDHANDLES;

    // Set the standard input to the Socket Handle
    si.hStdInput  = (HANDLE) serverFd;
    si.hStdOutput = INVALID_HANDLE_VALUE;
    si.hStdError  = INVALID_HANDLE_VALUE;

    // StdInput should be inheriatble
    bSuccess = SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT,
                   TRUE);
    if(!bSuccess) {
        errVal = CHILD_EXEC_FAILURE;
        if(!lifecycleCall)
            sendErrorStatus(errVal);
        return errVal;
    }

    char *winEnv = getWindowsEnvironmentVars();
    char *winArgs = formWinArgs();

    bSuccess = CreateProcess(procInfo.execPath,
                             winArgs,
                             NULL,
                             NULL,
                             TRUE,       /* Inheritable Handes inherited. */
                             CREATE_NO_WINDOW,
                             winEnv,
                             procInfo.chDir,
                             &si,
                             &pInfo);
    PL_strfree(winArgs);
    PR_Free(winEnv);

    if(!bSuccess) {
        DWORD dw = GetLastError();

        if (verboseSet) {
            // Retrieve the system error message for the last-error code
            LPVOID lpMsgBuf;

            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dw,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,
                0, NULL );

            fprintf(logFd, "Process creation failed with the error %d: %s", dw, (LPCTSTR)lpMsgBuf);
            fflush(logFd);
            LocalFree(lpMsgBuf);
        }

        switch (dw) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND: errVal = STAT_FAILURE; break;
            case ERROR_ACCESS_DENIED: errVal = NO_EXEC_PERMISSION; break;
            case ERROR_DIRECTORY: errVal = SET_CHDIR_FAILURE; break;
            default: errVal = CHILD_EXEC_FAILURE;
        }

        if(!lifecycleCall)
            sendErrorStatus(errVal);
        return errVal;
    }

    if(verboseSet) {
        fprintf(logFd, "created child - %d\n", pInfo.dwProcessId);
        fflush(logFd);
    }

    addChild(pInfo.dwProcessId);
#endif //XP_UNIX

    return NO_FCGI_ERROR;
}
