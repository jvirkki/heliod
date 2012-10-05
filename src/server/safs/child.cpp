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
 * child.cpp: Child process control
 *
 * Chris Elving
 */

#ifdef XP_WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <signal.h>
#include <poll.h>
#include "ChildExec.h"
#endif

#include "netsite.h"
#include "support/NSString.h"
#include "NsprWrap/NsprDescriptor.h"
#include "NsprWrap/NsprError.h"
#include "NsprWrap/Thread.h"
#include "time/nstime.h"
#include "base/systhr.h"
#include "base/util.h"
#include "frame/log.h"
#include "frame/conf.h"
#include "safs/child.h"
#include "safs/dbtsafs.h"
#if defined(HPUX) || defined(AIX)
#include "xp/xpatomic.h"
#endif


/*
 * Simplified Child state transition diagram:
 *
 *       child_create() != NULL
 *                 |
 *                 V
 *         +----------------+
 *         | CHILD_INACTIVE |
 *         +----------------+
 *                 |
 *         child_exec() == PR_SUCCESS
 *                 |
 *                 V
 *           +------------+    child_term()    +------------+
 *           | CHILD_EXEC |-------  or  ------>| CHILD_TERM |
 *           +------------+      timeout       +------------+
 *                 |                                 |
 *         out.eof && err.eof                        |
 *                 |                         kill(pid, 0) == -1
 *                 V                                 |
 *         +----------------+                        |
 *         | CHILD_FINISHED |<-----------------------+
 *         +----------------+
 *
 * Major Child refcount events:
 *
 *     1. 1 reference for caller when child_create() != NULL
 *     2. +1 reference for child_running_ht[] when child_exec() == PR_SUCCESS
 *     3. -1 reference for child_running_ht[] when CHILD_FINISHED
 *     4. -1 reference for caller after child_term()/child_wait()/child_done()
 */

/*
 * OSProcessID is the operating system's native pid type.
 */
#ifdef XP_WIN32
typedef HANDLE OSProcessID;
#else
typedef pid_t OSProcessID;
#endif

/*
 * OS_INVALID_PID is an invalid OSProcessID value.
 */
#ifdef XP_WIN32
static const OSProcessID OS_INVALID_PID = INVALID_HANDLE_VALUE;
#else
static const OSProcessID OS_INVALID_PID = -1;
#endif

/*
 * OSFileDesc is the operating system's native file descriptor type.
 */
#ifdef XP_WIN32
typedef HANDLE OSFileDesc;
#else
typedef int OSFileDesc;
#endif

/*
 * OS_INVALID_FD is an invalid OSFileDesc value.
 */
#ifdef XP_WIN32
static const OSFileDesc OS_INVALID_FD = INVALID_HANDLE_VALUE;
#else
static const OSFileDesc OS_INVALID_FD = -1;
#endif

/*
 * PIPE_READ_INDEX is the index of the read end of a pipe in an array of 2
 * file descriptors.
 */
static const int PIPE_READ_INDEX = 0;

/*
 * PIPE_WRITE_INDEX is the index of the write end of a pipe in an array of 2
 * file descriptors.
 */
static const int PIPE_WRITE_INDEX = 1;

/*
 * Timer contains an epoch from which an interval is measured and a timeout
 * that the interval should not exceed.
 */
struct Timer {
    PRIntervalTime epoch;
    PRIntervalTime timeout;
};

/*
 * ChildDescriptor is an NSPR descriptor corresponding to one of a child
 * process's stdio descriptors.
 */
class ChildDescriptor : public NsprDescriptor {
public:
    ChildDescriptor();
    ~ChildDescriptor();
    inline void setTimeout(PRIntervalTime timeout);
    inline void setFileDesc(OSFileDesc osfd);
    inline void detach();
    inline PRBool expired(PRIntervalTime now);
    PRStatus close();

protected:
    inline void block();
    inline void unblock();
    inline void closePipe(OSFileDesc osfd);

    volatile OSFileDesc osfd;
    PRBool blocked;
    Timer blocking;
#if defined(HPUX) || defined(AIX)
    PRLock *lock;
#endif
};

/*
 * ChildWriter is a writable NSPR descriptor corresponding to a child process's
 * stdin descriptor.
 */
class ChildWriter : public ChildDescriptor {
public:
    PRInt32 write(const void *buf, PRInt32 amount);
    PRInt32 send(const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
};

/*
 * ChildReader is a readable NSPR descriptor corresponding to a child process's
 * stdout/stderr descriptor.
 */
class ChildReader : public ChildDescriptor {
public:
    ChildReader();
    PRInt32 read(void *buf, PRInt32 amount);
    PRInt32 recv(void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
    inline PRBool ready();
    inline PRBool ateof() { return eof; }

private:
    PRBool eof;
};

/*
 * ChildState describes the state of a Child.
 */
enum ChildState {
    CHILD_INACTIVE, // child_exec() not yet called
    CHILD_EXEC,     // child_exec() succeeded
    CHILD_TERM,     // timed out or child_term() called
    CHILD_FINISHED  // child exited or detached
};

/*
 * ChildBucketNode stores a pointer for a child_running_ht[] linked list.
 */
struct ChildBucketNode {
    Child *next;
};

/*
 * Child represents a child process.
 */
class Child : public ChildBucketNode {
public:
    Child(Session *sn, Request *rq, const char *program);
    ~Child();
    inline const char *getProgram() { return program; }
    inline PRFileDesc *getPipe(PRSpecialFD sfd, PRIntervalTime timeout);
    inline PRStatus exec(const char *path, const char * const *argv, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout);
    inline void wait(int hi);
    inline void term();
    inline PRBool done();
    inline void drain();
    inline void log();
    inline void check(PRIntervalTime now);
    inline PRBool inactive() { return (state == CHILD_INACTIVE); }
    inline PRBool finished() { return (state == CHILD_FINISHED); }
    inline void ref();
    inline void unref();
    inline void abandon();

private:
    PRBool log(PRSpecialFD sfd);
    inline void kill();
    inline void finish();
    inline PRBool exited();

    ChildState state;
    Session *sn;
    Request *rq;
    NSString program;
    OSProcessID pid;
    PRBool log_out;
    PRBool log_err;
    Timer running;
    Timer terminating;
    ChildWriter in;
    ChildReader out;
    ChildReader err;
    PRCondVar *waiter;
    int refcount;
};

/*
 * child_running_ht contains Child pointers for running processes hashed by
 * pointer value.
 */
static const int child_running_hsize = 17; // prime, small, and > typical CPUs
static struct ChildBucket {
#ifdef DEBUG
    int held;
#endif
    PRLock *lock;
    Child *head;
} child_running_ht[child_running_hsize];

/*
 * CHILD_LOG_BUF_SIZE is the size of the buffer used when logging output from
 * a child process's stdout or stderr.
 */
static const int CHILD_LOG_BUF_SIZE = 1024;

/*
 * child_term_timeout is the maximum amount of time to wait for a process to
 * exit after a SIGTERM.
 */
static PRIntervalTime child_term_timeout = PR_SecondsToInterval(30);

/*
 * child_stdout_msg is the localized message format used when logging output
 * from a child process's stdout.
 */
static const char *child_stdout_msg;

/*
 * child_stderr_msg is the localized message format used when logging output
 * from a child process's stderr.
 */
static const char *child_stderr_msg;

/*
 * child_babysitter_lock serializes attempts to create the child_babysitter
 * thread and protects the child_babysitter_wakeup condition variable.
 */
static PRLock *child_babysitter_lock;

/*
 * child_babysitter_wakeup can be notified to rouse the child_babysitter
 * thread.
 */
static PRCondVar *child_babysitter_wakeup;

/*
 * child_babysitter_started is set after the child_babysitter thread has been
 * started.
 */
static PRBool child_babysitter_started;

/*
 * child_shell_bin_sh is the path to the default OS shell or command
 * interpreter.
 */
static const char *child_shell_bin_sh;

/*
 * CHILD_SHELL_DASH_C is the "-c" or "/C" parameter for the OS shell or command
 * interpreter.
 */
#ifdef XP_WIN32
static const char CHILD_SHELL_DASH_C[] = "/C";
#else
static const char CHILD_SHELL_DASH_C[] = "-c";
#endif

#ifdef XP_WIN32

/*
 * child_create_win32_process_lock makes the act of marking stdio pipes as
 * inheritable, creating a child process, and closing the child's end of those
 * pipes an atomic operation.  This helps keep pipe handles from leaking into
 * unrelated processes.
 */
static PRLock *child_create_win32_process_lock;

#endif

#ifndef XP_WIN32

/*
 * CGISTUB_SETUID_PATH is path relative to the config directory where we
 * look for a setuid Cgistub.
 */
static const char CGISTUB_SETUID_PATH[] = "../private/Cgistub";

/*
 * CGISTUB_INSTALL_SUFFIX is the path within the install root where the default
 * Cgistub is located.
 */
static const char CGISTUB_INSTALL_SUFFIX[] = "/lib/Cgistub";

/*
 * cgistub_path is the administrator-defined path to the Cgistub binary or NULL
 * if the default should be used.
 */
static char *cgistub_path;

/*
 * cgistub_min_children is the administrator-defined minimum number of Cgistub
 * processes to keep on hand or -1 if the default should be used.
 */
static int cgistub_min_children = -1;

/*
 * cgistub_max_children is the administrator-defined maximum number of Cgistub
 * processes to keep on hand or -1 if the default should be used.
 */
static int cgistub_max_children = -1;

/*
 * cgistub_idle_timeout is the administrator-defined interval at which idle
 * Cgistub processes are reaped or PR_INTERVAL_NO_WAIT if the default should be
 * used.
 */
static PRIntervalTime cgistub_idle_timeout = PR_INTERVAL_NO_WAIT;

/*
 * cgistub_child_exec_lock serializes calls to cgistub_child_exec_init.
 */
static PRLock *cgistub_child_exec_lock;

/*
 * cgistub_child_exec is the ChildExec * used to execute child processes.
 */
static ChildExec *cgistub_child_exec;

#endif


/* ---------------------------- timer_expired ----------------------------- */

static inline PRBool timer_expired(Timer *timer, PRIntervalTime now)
{
    if (timer->timeout != PR_INTERVAL_NO_TIMEOUT) {
        // Note that we treat elapsed as signed; now may actually be slightly
        // older than timer->epoch
        PRIntervalTime elapsed = now - timer->epoch;
        if ((PRInt32) elapsed > (PRInt32) timer->timeout)
            return PR_TRUE;
    }

    return PR_FALSE;
}


/* ---------------------------- create_cmdline ---------------------------- */

static inline char *create_cmdline(const char * const *argv)
{
    const char * const *a;

    int len = 0;
    for (a = argv; *a; a++)
        len += (2 + strlen(*a) + 1);

    char *cmdline = (char *) MALLOC(len + 1);
    if (cmdline) {
        char *p = cmdline;
        for (a = argv; *a; a++) {
            if (a != argv)
                *p++ = ' ';

#ifdef XP_WIN32
            // To keep CMD.EXE happy, we don't quote switches such as /C
            PRBool quotes = (**a != '/' || strpbrk(*a, " \t\r\n") != NULL);
            const char *value = *a;
            if (quotes)
                *p++ = '"';
#else
            char *tmp = util_sh_escape((char *)*a);
            const char *value = tmp;
#endif

            strcpy(p, value);
            p += strlen(value);

#ifdef XP_WIN32
            if (quotes)
                *p++ = '"';
#else
            FREE(tmp);
#endif
        }
        *p = '\0';
    }

    return cmdline;
}


/* ------------------------ create_win32_envblock ------------------------- */

#ifdef XP_WIN32

static inline void *create_win32_envblock(const char * const *envp)
{
    const char * const *e;

    // Construct a Win32 environment block, a sequence of nul-terminated
    // strings terminated by an additional nul character

    int size = 0;
    for (e = envp; *e; e++)
        size += (strlen(*e) + 1);
    if (size == 0) {
        size = 2;
    } else {
        size++;
    }

    char *envblock = (char *) MALLOC(size);
    if (envblock) {
        char *p = envblock;
        for (e = envp; *e; e++) {
            strcpy(p, *e);
            p += strlen(*e);
            *p++ = '\0';
        }
        if (p == envblock)
            *p++ = '\0';
        *p = '\0';
    }

    return envblock;
}

#endif


/* -------------------------- create_win32_pipe --------------------------- */

#ifdef XP_WIN32

static inline PRStatus create_win32_pipe(HANDLE pipe[], SECURITY_ATTRIBUTES *sa)
{
    int tries = 3;
    while (tries--) {
        if (CreatePipe(&pipe[PIPE_READ_INDEX], &pipe[PIPE_WRITE_INDEX], sa, 0))
            return PR_SUCCESS;
    }

    return PR_FAILURE;
}

#endif


/* ------------------------ set_win32_inheritable ------------------------- */

#ifdef XP_WIN32

static inline PRStatus set_win32_inheritable(HANDLE& handle, HANDLE process)
{
    // Replace handle with an equivalent but inheritable handle
    if (DuplicateHandle(process, handle, process, &handle, 0, TRUE,
                        DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS))
        return PR_SUCCESS;

    // DuplicateHandle failed, but not before it closed handle
    handle = INVALID_HANDLE_VALUE;

    return PR_FAILURE;
}

#endif


/* -------------------------- close_win32_handle -------------------------- */

#ifdef XP_WIN32

static inline void close_win32_handle(HANDLE& handle)
{
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

#endif


/* ------------------------- create_win32_process ------------------------- */

#ifdef XP_WIN32

static PRStatus create_win32_process(const char *path,
                                     char *cmdline,
                                     void *envblock,
                                     const char *dir,
                                     HANDLE *ppid,
                                     ChildWriter *pin,
                                     ChildReader *pout,
                                     ChildReader *perr)
{
    PRBool holding_create_win32_process_lock = PR_FALSE;
    SECURITY_ATTRIBUTES sa;
    OSFileDesc in[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
    OSFileDesc out[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
    OSFileDesc err[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
    HANDLE us;
    STARTUPINFO si;
    PROCESS_INFORMATION pi = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = FALSE;

    // Create pipes for each of the child's stdin, stdout, and stderr
    if (create_win32_pipe(in, &sa) == PR_FAILURE)
        goto create_win32_process_error;
    if (create_win32_pipe(out, &sa) == PR_FAILURE)
        goto create_win32_process_error;
    if (create_win32_pipe(err, &sa) == PR_FAILURE)
        goto create_win32_process_error;

    PR_Lock(child_create_win32_process_lock);
    holding_create_win32_process_lock = PR_TRUE;

    // Indicate which end of each pipe belongs to the child
    us = GetCurrentProcess();
    if (set_win32_inheritable(in[PIPE_READ_INDEX], us) == PR_FAILURE)
        goto create_win32_process_error;
    if (set_win32_inheritable(out[PIPE_WRITE_INDEX], us) == PR_FAILURE)
        goto create_win32_process_error;
    if (set_win32_inheritable(err[PIPE_WRITE_INDEX], us) == PR_FAILURE)
        goto create_win32_process_error;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = in[PIPE_READ_INDEX];
    si.hStdOutput = out[PIPE_WRITE_INDEX];
    si.hStdError = err[PIPE_WRITE_INDEX];
    si.wShowWindow = SW_HIDE;

    // Exec the child process
    if (!CreateProcess(path, cmdline,
                       NULL, NULL, TRUE, 0,
                       envblock, dir, &si, &pi))
        goto create_win32_process_error;

    // Close the ends of the pipes that belong to the child process
    close_win32_handle(in[PIPE_READ_INDEX]);
    close_win32_handle(out[PIPE_WRITE_INDEX]);
    close_win32_handle(err[PIPE_WRITE_INDEX]);

    PR_Unlock(child_create_win32_process_lock);
    holding_create_win32_process_lock = PR_FALSE;

    // Close our handle to the child process's initial thread
    close_win32_handle(pi.hThread);

    // Give caller our handles to the child process and its stdio descriptors
    *ppid = pi.hProcess;
    pin->setFileDesc(in[PIPE_WRITE_INDEX]);
    pout->setFileDesc(out[PIPE_READ_INDEX]);
    perr->setFileDesc(err[PIPE_READ_INDEX]);

    return PR_SUCCESS;

create_win32_process_error:
    // We use custom error response strings for those CreateProcess() errors
    // where the Win32 formatted message contains "%1" and isn't very specific
    switch (GetLastError()) {
    case ERROR_BAD_EXE_FORMAT:
        NsprError::setError(PR_UNKNOWN_ERROR, ERROR_BAD_EXE_FORMAT, XP_GetAdminStr(DBT_childBadExe));
        break;
#define ERROR_CASE(error) \
    case error: \
        NsprError::setError(PR_UNKNOWN_ERROR, error, #error); \
        break;
    ERROR_CASE(ERROR_INVALID_ORDINAL);
    ERROR_CASE(ERROR_CHILD_NOT_COMPLETE);
    ERROR_CASE(ERROR_INVALID_STARTING_CODESEG);
    ERROR_CASE(ERROR_INVALID_STACKSEG);
    ERROR_CASE(ERROR_INVALID_MODULETYPE);
    ERROR_CASE(ERROR_INVALID_EXE_SIGNATURE);
    ERROR_CASE(ERROR_EXE_MARKED_INVALID);
    ERROR_CASE(ERROR_ITERATED_DATA_EXCEEDS_64k);
    ERROR_CASE(ERROR_INVALID_MINALLOCSIZE);
    ERROR_CASE(ERROR_INVALID_SEGDPL);
    ERROR_CASE(ERROR_RELOC_CHAIN_XEEDS_SEGLIM);
    ERROR_CASE(ERROR_INFLOOP_IN_RELOC_CHAIN);
    ERROR_CASE(ERROR_EXE_MACHINE_TYPE_MISMATCH);
#undef ERROR_CASE
    default:
        // Use the default Win32 to NSPR error code mapping
        NsprError::mapWin32Error();
        break;
    }

    close_win32_handle(in[PIPE_READ_INDEX]);
    close_win32_handle(in[PIPE_WRITE_INDEX]);
    close_win32_handle(out[PIPE_READ_INDEX]);
    close_win32_handle(out[PIPE_WRITE_INDEX]);
    close_win32_handle(err[PIPE_READ_INDEX]);
    close_win32_handle(err[PIPE_WRITE_INDEX]);
    close_win32_handle(pi.hProcess);
    close_win32_handle(pi.hThread);

    if (holding_create_win32_process_lock)
        PR_Unlock(child_create_win32_process_lock);

    return PR_FAILURE;
}

#endif


/* -------------------------- child_running_hash -------------------------- */

static inline int child_running_hash(const Child *child)
{
    return ((unsigned long) child) % child_running_hsize;
}


/* -------------------------- child_running_lock -------------------------- */

static inline int child_running_lock(const Child *child)
{
    int hi = child_running_hash(child);

    PR_Lock(child_running_ht[hi].lock);

#ifdef DEBUG
    PR_ASSERT(!child_running_ht[hi].held);
    child_running_ht[hi].held = PR_TRUE;
#endif

    return hi;
}


/* ------------------------- child_running_unlock ------------------------- */

static inline void child_running_unlock(int hi)
{
#ifdef DEBUG
    PR_ASSERT(child_running_ht[hi].held);
    child_running_ht[hi].held = PR_FALSE;
#endif

    PR_Unlock(child_running_ht[hi].lock);
}


/* ------------------- ChildDescriptor::ChildDescriptor ------------------- */

ChildDescriptor::ChildDescriptor()
: osfd(OS_INVALID_FD),
  blocked(PR_FALSE)
{
    blocking.epoch = 0;
    blocking.timeout = PR_INTERVAL_NO_TIMEOUT;
#if defined(HPUX) || defined(AIX)
    lock = PR_NewLock();
    PR_ASSERT(lock);
#endif
}


/* ------------------ ChildDescriptor::~ChildDescriptor ------------------- */

ChildDescriptor::~ChildDescriptor()
{
    if (osfd != OS_INVALID_FD) {
        closePipe(osfd);
    }
#if defined(HPUX) || defined(AIX)
    if (lock) 
        PR_DestroyLock(lock);
#endif
}


/* --------------------- ChildDescriptor::setTimeout ---------------------- */

void ChildDescriptor::setTimeout(PRIntervalTime timeout)
{
    blocking.timeout = timeout;
}


/* --------------------- ChildDescriptor::setFileDesc --------------------- */

void ChildDescriptor::setFileDesc(OSFileDesc fd)
{
    PR_ASSERT(sizeof(PRInt32) == sizeof(OSFileDesc));
    fd = (OSFileDesc) PR_AtomicSet((PRInt32 *) &osfd, (PRInt32) fd);
    PR_ASSERT(fd == OS_INVALID_FD);
}


/* ----------------------- ChildDescriptor::detach ------------------------ */

void ChildDescriptor::detach()
{
    if (osfd != OS_INVALID_FD) {
        PR_ASSERT(sizeof(PRInt32) == sizeof(OSFileDesc));
        OSFileDesc fd = (OSFileDesc) PR_AtomicSet((PRInt32 *) &osfd, (PRInt32) OS_INVALID_FD);
        PR_ASSERT(fd != OS_INVALID_FD);
        closePipe(fd);
    }
}


/* ------------------------ ChildDescriptor::close ------------------------ */

PRStatus ChildDescriptor::close()
{
    // Close the underlying pipe
    detach();

    // Fail any PR_Close() calls to keep PR_FreeFileDesc() away from us
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return PR_FAILURE;
}


/* ------------------------ ChildDescriptor::block ------------------------ */

void ChildDescriptor::block()
{
    PR_ASSERT(!blocked);

    // XXX we require total store order; that is, we assume that the
    // child_babysitter thread will never see our update to blocked before it
    // sees our update to blocking.epoch
    blocking.epoch = ft_timeIntervalNow();
#if defined(HPUX) || defined(AIX)
    XP_ProducerMemoryBarrier();
#endif
    blocked = PR_TRUE;
}


/* ----------------------- ChildDescriptor::unblock ----------------------- */

void ChildDescriptor::unblock()
{
    PR_ASSERT(blocked);
    blocked = PR_FALSE;
}


/* ----------------------- ChildDescriptor::expired ----------------------- */

PRBool ChildDescriptor::expired(PRIntervalTime now)
{
    return blocked && timer_expired(&blocking, now);
}


/* ---------------------- ChildDescriptor::closePipe ---------------------- */

void ChildDescriptor::closePipe(OSFileDesc fd)
{
#ifdef XP_WIN32
    CloseHandle(fd);
#else
#if defined(HPUX) || defined(AIX)
    PR_Lock(lock);
#endif
    ::close(fd);
#if defined(HPUX) || defined(AIX)
    PR_Unlock(lock);
#endif
#endif
}


/* -------------------------- ChildWriter::write -------------------------- */

PRInt32 ChildWriter::write(const void *buf, PRInt32 amount)
{
    PRInt32 rv;

    block();

#ifdef XP_WIN32
    DWORD nwritten;
    if (WriteFile(osfd, buf, amount, &nwritten, NULL)) {
        rv = nwritten;
    } else {
        NsprError::mapWin32Error();
        rv = -1;
    }
#else
    rv = ::write(osfd, buf, amount);
    if (rv == -1)
        NsprError::mapUnixErrno();
#endif

    unblock();

    if (rv == amount)
        return rv;

    // If ChildDescriptor::detach() was called, report an IO timeout error
    if (osfd == OS_INVALID_FD) {
        PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
        rv = -1;
    }

    return rv;
}


/* -------------------------- ChildWriter::send --------------------------- */

PRInt32 ChildWriter::send(const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    return write(buf, amount);
}


/* ----------------------- ChildReader::ChildReader ----------------------- */

ChildReader::ChildReader()
: eof(PR_FALSE)
{ }


/* -------------------------- ChildReader::read --------------------------- */

PRInt32 ChildReader::read(void *buf, PRInt32 amount)
{
    PRInt32 rv = 0;

    block();

#ifdef XP_WIN32
    DWORD nread;
    if (ReadFile(osfd, buf, amount, &nread, NULL)) {
        rv = nread;
    } else if (GetLastError() == ERROR_BROKEN_PIPE) {
        rv = 0;
    } else {
        NsprError::mapWin32Error();
        rv = -1;
    }
#else
#if defined(HPUX) || defined(AIX)
    if (osfd != OS_INVALID_FD) {
        struct pollfd pfd;
        pfd.fd = osfd;
        pfd.events = POLLIN | POLLHUP;
        pfd.revents = 0;
        PRIntn msecs = -1;
        switch(blocking.timeout) {
            case (PR_INTERVAL_NO_TIMEOUT) :
              msecs = -1; break;
            case (PR_INTERVAL_NO_WAIT) :
              msecs = 0; break;
            default :
              msecs = PR_IntervalToMilliseconds(blocking.timeout);
              break;
        }
        if (::poll(&pfd, 1, msecs) == 1) { 
            if (pfd.revents & POLLIN) {
                PR_Lock(lock);
                rv = ::read(osfd, buf, amount);
                PR_Unlock(lock);
                if (rv == -1)
                    NsprError::mapUnixErrno();
            }
        }
    }
#else
    rv = ::read(osfd, buf, amount);
    if (rv == -1)
        NsprError::mapUnixErrno();
#endif
#endif

    unblock();

    if (rv > 0)
        return rv;

    eof = PR_TRUE;

    // If ChildDescriptor::detach() was called, report an IO timeout error
    if (osfd == OS_INVALID_FD) {
        PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
        rv = -1;
    }

    return rv;
}


/* -------------------------- ChildReader::recb --------------------------- */

PRInt32 ChildReader::recv(void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
    return read(buf, amount);
}


/* -------------------------- ChildReader::ready -------------------------- */

PRBool ChildReader::ready()
{
    // Return PR_TRUE if data is ready or the other end of the pipe was closed
#ifdef XP_WIN32
    DWORD available;
    if (PeekNamedPipe(osfd, NULL, 0, 0, &available, NULL))
        return (available > 0);
    return PR_TRUE;
#else
    struct pollfd pfd;
    pfd.fd = osfd;
    pfd.events = POLLIN | POLLHUP;
    pfd.revents = 0;
    return (::poll(&pfd, 1, 0) == 1);
#endif
}


/* ----------------------------- Child::Child ----------------------------- */

Child::Child(Session *sn, Request *rq, const char *programArg)
: state(CHILD_INACTIVE),
  sn(sn),
  rq(rq),
  program(programArg),
  pid(OS_INVALID_PID),
  log_out(PR_TRUE),
  log_err(PR_TRUE),
  waiter(NULL),
  refcount(1)
{ }


/* ---------------------------- Child::~Child ----------------------------- */

Child::~Child()
{
    PR_ASSERT(state == CHILD_INACTIVE || state == CHILD_FINISHED);
    PR_ASSERT(pid == OS_INVALID_PID);
    PR_ASSERT(refcount == 0);
    if (waiter)
        PR_DestroyCondVar(waiter);
}


/* ---------------------------- Child::getPipe ---------------------------- */

PRFileDesc *Child::getPipe(PRSpecialFD sfd, PRIntervalTime timeout)
{
    PRFileDesc *prfd;

    switch (sfd) {
    case PR_StandardInput:
        in.setTimeout(timeout);
        prfd = in;
        break;

    case PR_StandardOutput:
        log_out = PR_FALSE;
        out.setTimeout(timeout);
        prfd = out;
        break;

    case PR_StandardError:
        log_err = PR_FALSE;
        err.setTimeout(timeout);
        prfd = err;
        break;

    default:
        PR_ASSERT(0);
        prfd = NULL;
        break;
    }

    return prfd;
}


/* ----------------------------- Child::exec ------------------------------ */

PRStatus Child::exec(const char *path, const char * const *argv, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout)
{
    log_error(LOG_VERBOSE, NULL, sn, rq, "attempting to execute %s", program.data());

    const char *dir = NULL;
    if (opts && opts->dir) {
        // Use the directory specified by the caller
        dir = opts->dir;
    } else if (rq) {
        // Use the directory specified by "path" in rq->vars
        char *path = STRDUP(pblock_findkeyval(pb_key_path, rq->vars));
        if (path) {
            NSFCFileInfo *finfo = NULL;
            if (INTrequest_info_path(path, rq, &finfo) == PR_SUCCESS) {
                char *slash = strrchr(path, '/');
#ifdef XP_WIN32
                if (char *backslash = strrchr(slash ? slash : path, '\\'))
                    slash = backslash;
#endif
                if (slash > path)
                    *slash = '\0';
                if (file_is_path_abs(path))
                    dir = path;
            }
        }
    }

#ifdef XP_WIN32
    char *cmdline = NULL;
    void *envblock = NULL;
    PRStatus rv = PR_FAILURE;

    // Construct a Win32 command line string from the Unix-style argv[] array
    if (argv && argv[0]) {
        cmdline = create_cmdline(argv);
    } else {
        const char *t[] = { path, NULL };
        cmdline = create_cmdline(t);
    }
    if (!cmdline)
        goto exec_done;

    // Construct a Win32 environment block from the Unix-style envp[] array
    if (envp) {
        envblock = create_win32_envblock(envp);
        if (!envblock)
            goto exec_done;
    }

    // Create the child process using Win32
    rv = create_win32_process(path, cmdline, envblock, dir, &pid, &in, &out, &err);

exec_done:
    FREE(cmdline);
    FREE(envblock);

    if (rv == PR_FAILURE) {
        PR_ASSERT(pid == OS_INVALID_PID);
        log_error(LOG_VERBOSE, NULL, sn, rq, "error executing %s (%s)", program.data(), system_errmsg());
        return PR_FAILURE;
    }
#else
    // Prepare the Cgistub subsystem parameters
    cexec_args_t cs;
    if (opts) {
        cs.cs_opts = *opts;
    } else {
        memset(&cs.cs_opts, 0, sizeof(cs.cs_opts));
    }
    if (!cs.cs_opts.dir)
        cs.cs_opts.dir = dir;
    cs.cs_exec_path = path;
    if (argv && argv[0]) {
        cs.cs_args = &argv[1];
        cs.cs_argv0 = argv[0];
    } else {
        cs.cs_args = NULL;
        cs.cs_argv0 = path;
    }
    cs.cs_envargs = envp;

    // Create the child process using the Cgistub subsystem
    int ioerr;
    CHILDEXEC_ERR cserr = cgistub_child_exec->exec(cs, ioerr); 
    if (cserr != CERR_OK) {
        char buf[256];
        const char *p = ChildExec::csErrorMsg(cserr, ioerr, buf, sizeof(buf));
        NsprError::setError(PR_UNKNOWN_ERROR, ioerr, p);
        log_error(LOG_VERBOSE, NULL, sn, rq, "error executing %s (%s)", program.data(), system_errmsg());
        return PR_FAILURE;
    }

    pid = cs.cs_pid;
    in.setFileDesc(cs.cs_stdin);
    out.setFileDesc(cs.cs_stdout);
    err.setFileDesc(cs.cs_stderr);
#endif

    PR_ASSERT(pid != OS_INVALID_PID);    

    // Child process is up and running
    running.epoch = ft_timeIntervalNow();
    running.timeout = timeout;
    state = CHILD_EXEC;

    return PR_SUCCESS;
}


/* ----------------------------- Child::wait ------------------------------ */

void Child::wait(int hi)
{
    PR_ASSERT(hi == child_running_hash(this));
    PR_ASSERT(child_running_ht[hi].held);
    PR_ASSERT(refcount > 0);

    while (state == CHILD_EXEC) {
#ifdef DEBUG
        child_running_ht[hi].held--;
#endif

        if (!waiter)
            waiter = PR_NewCondVar(child_running_ht[hi].lock);
        PR_WaitCondVar(waiter, PR_INTERVAL_NO_TIMEOUT);

#ifdef DEBUG
        child_running_ht[hi].held++;
#endif
    }

    PR_ASSERT(refcount > 0);
}


/* ----------------------------- Child::term ------------------------------ */

void Child::term()
{
    PR_ASSERT(refcount == 1 || child_running_ht[child_running_hash(this)].held);
    PR_ASSERT(refcount > 0);

    if (state == CHILD_EXEC) {
        log_error(LOG_VERBOSE, NULL, sn, rq, "closing stdin, stdout, and stderr pipes to/from %s", program.data());

        // Close pipes to/from process
        err.detach();
        out.detach();
        in.detach();

        // Ask process to terminate nicely
#ifdef XP_WIN32
        // XXX no SIGTERM in Win32, so rely on closing the stdio descriptors
#else
        if (::kill(pid, SIGTERM)) {
            pid = OS_INVALID_PID;
        } else {
            log_error(LOG_VERBOSE, NULL, sn, rq, "sent SIGTERM to %s", program.data());
        }
#endif

        // Begin tracking how long process takes to exit
        terminating.epoch = ft_timeIntervalNow();
        terminating.timeout = child_term_timeout;

        state = CHILD_TERM;
    }
}


/* ----------------------------- Child::done ------------------------------ */

PRBool Child::done()
{
    PR_ASSERT(refcount == 1);

    // If child has exited...
    if (exited()) {
        finish();
        return PR_TRUE;
    }

    // Child detached from stdout but didn't exit?  Tell it to terminate.
    term();

    return PR_FALSE;
}


/* ---------------------------- Child::exited ----------------------------- */

PRBool Child::exited()
{
    if (pid == OS_INVALID_PID)
        return PR_TRUE;

#ifdef XP_WIN32
    if (WaitForSingleObject(pid, 0) == WAIT_OBJECT_0) {
        close_win32_handle(pid);
        PR_ASSERT(pid == OS_INVALID_PID);
        return PR_TRUE;
    }
#else
    // XXX watch for pid being recycled for another process
    if (::kill(pid, 0)) {
        pid = OS_INVALID_PID;
        return PR_TRUE;
    }
#endif

    return PR_FALSE; 
}


/* ----------------------------- Child::check ----------------------------- */

void Child::check(PRIntervalTime now)
{
    PR_ASSERT(child_running_ht[child_running_hash(this)].held);
    PR_ASSERT(state != CHILD_INACTIVE);
    PR_ASSERT(refcount > 0);

    // Check for data to log
    log();

    // Check for a detached/exited process.  This is the normal way for a
    // child process to finish.
    if (state == CHILD_EXEC) {
        if (out.ateof() && err.ateof()) {
            log_error(LOG_VERBOSE, NULL, sn, rq, "%s closed stdout and stderr", program.data());
            term();
        }
    }

    // If we asked the process to terminate, see if it complied
    if (state == CHILD_TERM) {
        if (exited()) {
            finish();
        } else if (timer_expired(&terminating, now)) {
            log_error(LOG_VERBOSE, NULL, sn, rq, "%s is taking too long to terminate", program.data());
            kill();
        }
    }

    // Check timeouts and ask the process to terminate if necessary
    if (state == CHILD_EXEC) {
        if (timer_expired(&running, now)) {
            // Process has been running too long
            log_error(LOG_VERBOSE, NULL, sn, rq, "%s has been running too long", program.data());
            term();
        } else if (in.expired(now)) {
            // Write timeout
            log_error(LOG_VERBOSE, NULL, sn, rq, "server has been blocked trying to send data to %s for too long", program.data());
            term();
        } else if (out.expired(now) || err.expired(now)) {
            // Read timeout
            log_error(LOG_VERBOSE, NULL, sn, rq, "server has been blocked waiting for data from %s for too long", program.data());
            term();
        }
    }
}


/* ----------------------------- Child::kill ------------------------------ */

void Child::kill()
{
    PR_ASSERT(refcount == 1 || child_running_ht[child_running_hash(this)].held);
    PR_ASSERT(refcount > 0);
    PR_ASSERT(pid != OS_INVALID_PID);

    if (pid != OS_INVALID_PID) {
        // Forcibly terminate the process
#ifdef XP_WIN32
        log_error(LOG_VERBOSE, NULL, sn, rq, "forcibly terminating %s", program.data());
        TerminateProcess(pid, 1);
        close_win32_handle(pid);
        PR_ASSERT(pid == OS_INVALID_PID);
#else
        // XXX Since we don't waitpid(), it's possible this pid has already
        // been assigned to a new process.  NES/iWS/S1WS has done things this
        // (broken) way for a while, though...
        log_error(LOG_VERBOSE, NULL, sn, rq, "sending SIGKILL to %s", program.data());
        ::kill(pid, SIGKILL);
        pid = OS_INVALID_PID;
#endif
    }

    finish();
}


/* ---------------------------- Child::finish ----------------------------- */

void Child::finish()
{
    PR_ASSERT(refcount == 1 || child_running_ht[child_running_hash(this)].held);
    PR_ASSERT(refcount > 0);
    PR_ASSERT(pid == OS_INVALID_PID);

    log_error(LOG_VERBOSE, NULL, sn, rq, "%s terminated", program.data());

    // Close pipes to/from process
    err.detach();
    out.detach();
    in.detach();

    state = CHILD_FINISHED;

    if (waiter)
        PR_NotifyCondVar(waiter);
}


/* ----------------------------- Child::drain ----------------------------- */

void Child::drain()
{
    // Drain stdout/stderr log messages
    while (log(PR_StandardOutput) || log(PR_StandardError));
}


/* ------------------------------ Child::log ------------------------------ */

void Child::log()
{
    // Log at most one buffer's worth of data from each of stdout/stderr
    log(PR_StandardOutput);
    log(PR_StandardError);
}

PRBool Child::log(PRSpecialFD sfd)
{
    ChildReader *reader = NULL;
    int degree;
    const char *fmt;

    switch (sfd) {
    case PR_StandardOutput:
        if (log_out) {
            reader = &out;
            degree = LOG_INFORM;
            fmt = child_stdout_msg;
        }
        break;

    case PR_StandardError:
        if (log_err) {
            reader = &err;
            degree = LOG_WARN;
            fmt = child_stderr_msg;
        }
        break;

    default:
        PR_ASSERT(0);
        break;
    }

    if (!reader || reader->ateof() || !reader->ready())
        return PR_FALSE;

    // Get some data from the child process's stdout/stderr.  So that a single
    // child process doesn't monopolize the child_babysitter thread, we make at
    // most one read() call per log() call.
    char buf[CHILD_LOG_BUF_SIZE];
    PRInt32 n = reader->read(buf, sizeof(buf));

    // If we read some data...
    if (n > 0) {
        // Log the data one line at a time
        char *begin = buf;
        char *p = buf;
        while (p <= buf + n) {
            if (p == buf + n || *p == '\n') {
                char *end = p;
                while (end > begin && (end[-1] == '\0' || isspace(end[-1])))
                    end--;
                while (begin < end && (*begin == '\0' || isspace(*begin)))
                    begin++;
                if (end > begin)
                    log_error(degree, program, sn, rq, fmt, end - begin, begin);
                begin = p + 1;
            }
            p++;
        }
    }

    return PR_TRUE;
}


/* ------------------------------ Child::ref ------------------------------ */

void Child::ref()
{
    PR_ASSERT(child_running_ht[child_running_hash(this)].held);
    PR_ASSERT(refcount > 0);

    refcount++;
}


/* ----------------------------- Child::unref ----------------------------- */

void Child::unref()
{
    PR_ASSERT(refcount == 1 || child_running_ht[child_running_hash(this)].held);
    PR_ASSERT(refcount > 0);

    refcount--;

    if (refcount == 0)
        delete this;
}


/* ---------------------------- Child::abandon ---------------------------- */

void Child::abandon()
{
    // We're called when a DaemonSession thread wants to abandon a Child.
    // child_running_ht[] might still have a reference, but child_babysitter
    // must no longer use the DaemonSession's sn and rq.
    sn = NULL;
    rq = NULL;

    unref();
}


/* --------------------------- child_babysitter --------------------------- */

PR_BEGIN_EXTERN_C

static void child_babysitter(void *arg)
{
    PRIntervalTime busy_interval = PR_MillisecondsToInterval(10);
    PRIntervalTime idle_interval = PR_SecondsToInterval(1);

    for (;;) {
        int children = 0;

        PRIntervalTime now = ft_timeIntervalNow();

        for (int hi = 0; hi < child_running_hsize; hi++) {
            if (child_running_ht[hi].head) {
                PR_Lock(child_running_ht[hi].lock);
#ifdef DEBUG
                child_running_ht[hi].held++;
#endif

                Child **pchild = &child_running_ht[hi].head;
                while (*pchild) {
                    Child *child = *pchild;

                    child->check(now);

                    if (child->finished()) {
                        *pchild = child->next; // remove from child_running_ht[]
                        child->unref();
                    } else {
                        pchild = &child->next;
                    }

                    children++;
                }

#ifdef DEBUG
                child_running_ht[hi].held--;
#endif
                PR_Unlock(child_running_ht[hi].lock);
            }
        }

        PRIntervalTime wait_interval;
        if (children) {
            wait_interval = busy_interval;
        } else {
            wait_interval = idle_interval;
        }

        PR_Lock(child_babysitter_lock);
        PR_WaitCondVar(child_babysitter_wakeup, wait_interval);
        PR_Unlock(child_babysitter_lock);
    }
}

PR_END_EXTERN_C


/* ------------------------ child_babysitter_start ------------------------ */

static inline PRStatus child_babysitter_start()
{
    if (!child_babysitter_started) {
        PR_Lock(child_babysitter_lock);

        if (!child_babysitter_started) {
            PRThread *thread = PR_CreateThread(PR_SYSTEM_THREAD,
                                               child_babysitter,
                                               0,
                                               PR_PRIORITY_NORMAL,
                                               PR_GLOBAL_THREAD,
                                               PR_UNJOINABLE_THREAD,
                                               0);
            if (thread)
                child_babysitter_started = PR_TRUE;
        }

        PR_Unlock(child_babysitter_lock);
    }

    return child_babysitter_started ? PR_SUCCESS : PR_FAILURE;
}


/* ----------------------------- child_create ----------------------------- */

NSAPI_PUBLIC Child *child_create(Session *sn, Request *rq, const char *program)
{
    Child *child;
    try {
        child = new Child(sn, rq, program);
    } catch (...) {
        child = NULL;
    }
    return child;
}
    

/* ------------------------------ child_pipe ------------------------------ */

NSAPI_PUBLIC PRFileDesc *child_pipe(Child *child, PRSpecialFD sfd, PRIntervalTime timeout)
{
    return child->getPipe(sfd, timeout);
}


/* ------------------------- child_exec_internal -------------------------- */

NSAPI_PUBLIC PRStatus child_exec_internal(Child *child, const char *path, const char * const *argv, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout)
{
    PR_ASSERT(child->inactive());

#ifndef XP_WIN32
    // Lazily initialize the Cgistub subsystem if necessary
    // XXX we require total store order; that is, we assume that no thread will
    // see the cgistub_child_exec pointer before *cgistub_child_exec is fully
    // initialized
    if (!cgistub_child_exec) {
        if (cgistub_init() == PR_FAILURE) {
            child->unref();
            return PR_FAILURE;
        }
    }
#endif

    if (child_babysitter_start() == PR_FAILURE)
        return PR_FAILURE;

    PRStatus rv = child->exec(path, argv, envp, opts, timeout);
    if (rv == PR_FAILURE) {
        child->unref();
        return PR_FAILURE;
    }

    // Add child to child_running_ht[]
    int hi = child_running_lock(child);
    child->ref();
    child->next = child_running_ht[hi].head;
    child_running_ht[hi].head = child;
    child_running_unlock(hi);

    return PR_SUCCESS;
}


/* ------------------------------ child_exec ------------------------------ */

NSAPI_PUBLIC PRStatus child_exec(Child *child, const char * const *argv, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout)
{
    return child_exec_internal(child, child->getProgram(), argv, envp, opts, timeout);
}


/* ----------------------------- init_bin_sh ------------------------------ */

static void init_bin_sh()
{
    // Lookup the default OS shell/command interpreter if we haven't already
    if (!child_shell_bin_sh) {
        const char *bin_sh;

#ifdef XP_WIN32
        bin_sh = getenv("ComSpec");
        if (!bin_sh || !strlen(bin_sh))
            bin_sh = "CMD.EXE";
#else
        bin_sh = getenv("SHELL");
        if (!bin_sh || !strlen(bin_sh))
            bin_sh = "/bin/sh";
#endif
        log_error(LOG_VERBOSE, NULL, NULL, NULL, "using %s %s as the OS shell", bin_sh, CHILD_SHELL_DASH_C);

        child_shell_bin_sh = bin_sh;
    }
}


/* ----------------------------- child_shell ------------------------------ */

NSAPI_PUBLIC PRStatus child_shell(Child *child, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout)
{
    PR_ASSERT(child->inactive());

    // Lookup the default OS shell/command interpreter if we haven't already
    init_bin_sh();

    const char *argv[4] = { child_shell_bin_sh,
                            CHILD_SHELL_DASH_C,
                            child->getProgram(),
                            NULL };

    return child_exec_internal(child, child_shell_bin_sh, argv, envp, opts, timeout);
}


/* --------------------------- child_shell_argv ---------------------------- */

NSAPI_PUBLIC PRStatus child_shell_argv(Child *child, const char * const *argv, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout)
{
    PR_ASSERT(child->inactive());

    // Lookup the default OS shell/command interpreter if we haven't already
    init_bin_sh();

    const char *cmdline = NULL;

    if (argv && argv[0]) {
        cmdline = (const char *)create_cmdline(argv);
    } else {
        cmdline = child->getProgram();
    }

    const char *command_argv[4] = { child_shell_bin_sh,
                                    CHILD_SHELL_DASH_C,
                                    cmdline,
                                    NULL };

    return child_exec_internal(child, child_shell_bin_sh, command_argv, envp, opts, timeout);
}


/* ------------------------------ child_wait ------------------------------ */

NSAPI_PUBLIC void child_wait(Child *child)
{
    // Prod the child_babysitter thread as we rely on it to notice that the
    // child process detached/exited
    PR_Lock(child_babysitter_lock);
    PR_NotifyCondVar(child_babysitter_wakeup);
    PR_Unlock(child_babysitter_lock);

    int hi = child_running_lock(child);

    // Wait for the child to exit/detach
    child->wait(hi);

    // Disassociate the Child from this thread
    child->abandon();

    child_running_unlock(hi);
}


/* ------------------------------ child_term ------------------------------ */

NSAPI_PUBLIC void child_term(Child *child)
{
    int hi = child_running_lock(child);

    // Check for a line of stdout/stderr output, but don't sit there draining
    // stdout/stderr forever
    child->log();

    // Send SIGTERM and close stdio descriptors
    child->term();

    // Disassociate the Child from this thread
    child->abandon();

    child_running_unlock(hi);
}


/* ------------------------------ child_done ------------------------------ */

NSAPI_PUBLIC void child_done(Child *child)
{
    int hi = child_running_lock(child);

    // Remove child from child_running_ht[]
    Child **pchild = &child_running_ht[hi].head;
    while (*pchild) {
        if (*pchild == child) {
            *pchild = child->next; // remove from child_running_ht[]
            child->unref();
            break;
        }
        pchild = &(*pchild)->next;
    }

    child_running_unlock(hi);

    // Drain stdout/stderr log messages
    child->drain();

    if (child->done()) {
        // Child has exited, remove the last reference
        child->unref();
    } else {
        // Oops, child hasn't actually exited yet
        int hi = child_running_lock(child);

        // Add the Child back into child_running_ht[]
        child->ref();
        child->next = child_running_ht[hi].head;
        child_running_ht[hi].head = child;

        // Disassociate the Child from this thread
        child->abandon();

        child_running_unlock(hi);
    }
}


/* ----------------------- cgistub_child_exec_init ------------------------ */

#ifndef XP_WIN32

static PRStatus cgistub_child_exec_init()
{
    NSString path;

    if (cgistub_path) {
        // The administrator specified the Cgistub path
        path = cgistub_path;
    } else {
        // Look for Cgistub in the default locations
        struct stat st;
        if (stat(CGISTUB_SETUID_PATH, &st) == 0) {
            path = CGISTUB_SETUID_PATH;
        } else {
            const char *install = conf_get_true_globals()->Vnetsite_root;
            if (!install)
                return PR_FAILURE;

            path.append(install);
            path.append(CGISTUB_INSTALL_SUFFIX);
        }
    }

    // Create the Cgistub subsystem
    ChildExec *ce = new ChildExec(path);
    if (!ce)
        return PR_FAILURE;

    // Configure the Cgistub pool
    if (cgistub_min_children != -1)
        ce->setMinChildren(cgistub_min_children);
    if (cgistub_max_children != -1)
        ce->setMaxChildren(cgistub_max_children);
    if (cgistub_idle_timeout != PR_INTERVAL_NO_WAIT)
        ce->setIdleChildReapInterval(cgistub_idle_timeout);

    // Start the Cgistub subsystem (start listener, etc.)
    int ioerr;
    CHILDEXEC_ERR cserr = ce->initialize(ioerr);
    if (cserr != CERR_OK) {
        char buf[256];
        const char *err = ChildExec::csErrorMsg(cserr, ioerr, buf, sizeof(buf));
        log_error(LOG_FAILURE, "cgi-init", NULL, NULL,
                  XP_GetAdminStr(DBT_cgiError4),
                  path.data(), err);

        delete ce;

        return PR_FAILURE;
    }

    // Cgistub subsystem is good to go
    PR_ASSERT(cgistub_child_exec == NULL);
    cgistub_child_exec = ce;

    return PR_SUCCESS;
}

#endif


/* ----------------------------- cgistub_init ----------------------------- */

#ifndef XP_WIN32

PRStatus cgistub_init()
{
    PRStatus rv = PR_SUCCESS;

    if (!cgistub_child_exec) {
        PR_Lock(cgistub_child_exec_lock);
        if (!cgistub_child_exec)
            rv = cgistub_child_exec_init();
        PR_Unlock(cgistub_child_exec_lock);
    }

    PR_ASSERT((rv == PR_SUCCESS) == (cgistub_child_exec != NULL));

    return rv;
}

#endif


/* --------------------------- cgistub_set_path --------------------------- */

#ifndef XP_WIN32

PRStatus cgistub_set_path(const char *path)
{
    PR_ASSERT(!cgistub_child_exec);
    PR_ASSERT(strlen(path) > 0);

    PERM_FREE(cgistub_path);

    cgistub_path = PERM_STRDUP(path);
    if (!cgistub_path)
        return PR_FAILURE;

    return PR_SUCCESS;
}

#endif


/* ----------------------- cgistub_set_min_children ----------------------- */

#ifndef XP_WIN32

PRStatus cgistub_set_min_children(int n)
{
    PR_ASSERT(!cgistub_child_exec);
    PR_ASSERT(n >= 0);

    cgistub_min_children = n;

    return PR_SUCCESS;
}

#endif


/* ----------------------- cgistub_set_max_children ----------------------- */

#ifndef XP_WIN32

PRStatus cgistub_set_max_children(int n)
{
    PR_ASSERT(!cgistub_child_exec);
    PR_ASSERT(n >= 1);

    cgistub_max_children = n;

    return PR_SUCCESS;
}

#endif


/* ----------------------- cgistub_set_idle_timeout ----------------------- */

#ifndef XP_WIN32

PRStatus cgistub_set_idle_timeout(PRIntervalTime timeout)
{
    PR_ASSERT(!cgistub_child_exec);
    PR_ASSERT(timeout != PR_INTERVAL_NO_WAIT && timeout != PR_INTERVAL_NO_TIMEOUT);

    cgistub_idle_timeout = timeout;

    return PR_SUCCESS;
}

#endif


/* ------------------------ child_set_term_timeout ------------------------ */

NSAPI_PUBLIC void child_set_term_timeout(PRIntervalTime timeout)
{
    child_term_timeout = timeout;
}


/* ------------------------------ child_init ------------------------------ */

PRStatus child_init()
{
    // child_init should only be called once
    PR_ASSERT(!child_stdout_msg);

    child_stdout_msg = XP_GetAdminStr(DBT_childStdoutLenXStrY);
    if (!child_stdout_msg)
        return PR_FAILURE;

    child_stderr_msg = XP_GetAdminStr(DBT_childStderrLenXStrY);
    if (!child_stderr_msg)
        return PR_FAILURE;

    for (int hi = 0; hi < child_running_hsize; hi++) {
        child_running_ht[hi].lock = PR_NewLock();
        if (!child_running_ht[hi].lock)
            return PR_FAILURE;
    }

    child_babysitter_lock = PR_NewLock();
    if (!child_babysitter_lock)
        return PR_FAILURE;

    child_babysitter_wakeup = PR_NewCondVar(child_babysitter_lock);
    if (!child_babysitter_wakeup)
        return PR_FAILURE;

#ifdef XP_WIN32
    child_create_win32_process_lock = PR_NewLock();
    if (!child_create_win32_process_lock)
        return PR_FAILURE;
#endif

#ifndef XP_WIN32
    // child_init should be called before cgistub_init
    PR_ASSERT(!cgistub_child_exec);

    cgistub_child_exec_lock = PR_NewLock();
    if (!cgistub_child_exec_lock)
        return PR_FAILURE;
#endif

    return PR_SUCCESS;
}
