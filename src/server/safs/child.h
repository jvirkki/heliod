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

#ifndef SAFS_CHILD_H
#define SAFS_CHILD_H

/*
 * child.h: child process control
 *
 * Chris Elving
 */

/*
 * Child represents a child process.
 */
typedef struct Child Child;

/*
 * ChildOptions contains optional child_exec parameters.
 */
typedef struct ChildOptions {
    const char *dir;           /* initial working directory */
    const char *root;          /* root directory */
    const char *user;          /* user name */
    const char *group;         /* group name */
    const char *nice;          /* nice() increment */
    const char *rlimit_as;     /* setrlimit(RLIMIT_AS) value */
    const char *rlimit_core;   /* setrlimit(RLIMIT_CORE) values */
    const char *rlimit_cpu;    /* setrlimit(RLIMIT_CPU) values */
    const char *rlimit_nofile; /* setrlimit(RLIMIT_NOFILE) values */
} ChildOptions;

/*
 * child_init initializes the child process control subsystem.
 */
PRStatus child_init();

/*
 * child_set_term_timeout sets the maximum amount of time to wait for a child
 * process to exit after it is sent a SIGTERM before it is sent a SIGKILL.
 */
NSAPI_PUBLIC void child_set_term_timeout(PRIntervalTime timeout);

/*
 * child_create returns a Child * that can be used to execute a child process.
 * The process is not started until child_exec is called.
 */
NSAPI_PUBLIC Child *child_create(Session *sn, Request *rq, const char *program);

/*
 * child_pipe returns one end of a pipe associated with one of a child
 * process's stdio descriptors.  If a PR_Read/PR_Write on the PRFileDesc *
 * exceeds the passed timeout, PR_IO_TIMEOUT_ERROR will be returned.  Because
 * the PRFileDesc * may wrap a Win32 pipe, it must only be used from a native
 * thread.  Because the PRFileDesc * is owned by the Child *, it does not need
 * to be passed to PR_Close.  It is an error to call child_pipe after
 * child_exec has been called or to use the returned PRFileDesc * until after
 * child_exec returns PR_SUCCESS.
 */
NSAPI_PUBLIC PRFileDesc *child_pipe(Child *child, PRSpecialFD sfd, PRIntervalTime timeout);

/*
 * child_exec executes a child process.  If child_exec returns PR_SUCCESS, the
 * Child * must be freed by a call to child_wait, child_term, or child_done.
 * If child_exec returns PR_FAILURE, the Child * is automatically freed.  If
 * argv is non-NULL, it is a NULL-terminated list of arguments and argv[0]
 * should be equivalent to program.  If envp is non-NULL, it is a NULL-
 * terminated list of environment strings.  If the passed timeout occurs before
 * the process exits or detaches, an attempt will be made to terminate the
 * process and any IO operations on the process's pipes will fail with
 * PR_IO_TIMEOUT_ERROR.
 */
NSAPI_PUBLIC PRStatus child_exec(Child *child, const char * const *argv, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout);

/*
 * child_shell is equivalent to child_exec but invokes the child process using
 * the default OS shell or command interpreter.
 */
NSAPI_PUBLIC PRStatus child_shell(Child *child, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout);

/*
 * child_shell_argv is equivalent to child_shell but accepts an argv[]
 */
NSAPI_PUBLIC PRStatus child_shell_argv(Child *child, const char * const *argv, const char * const *envp, const ChildOptions *opts, PRIntervalTime timeout);

/*
 * child_wait waits for a child process to detach (that is, close its stdio
 * descriptors) or exit.  The passed Child * is freed.
 */
NSAPI_PUBLIC void child_wait(Child *child);

/*
 * child_term requests that a child process terminate.  The passed Child * is
 * freed.  The child process is sent a SIGTERM, and any pipes to/from the child
 * process's stdio descriptors are closed.
 */
NSAPI_PUBLIC void child_term(Child *child);

/*
 * child_done indicates that a child process has exited or detached (e.g. a
 * PR_Read() from the child process's stdout descriptor returned 0).  The
 * passed Child * is freed.  The child process is sent a SIGTERM, and any
 * remaining pipes to/from the child process's stdio descriptors are closed.
 */
NSAPI_PUBLIC void child_done(Child *child);

#ifndef XP_WIN32

/*
 * cgistub_init initializes the Cgistub execution subsystem.
 */
PRStatus cgistub_init();

/*
 * cgistub_set_path configures the path to the Cgistub binary.
 */
PRStatus cgistub_set_path(const char *path);

/*
 * cgistub_set_min_children configures the minimum number of Cgistub processes
 * to keep on hand.
 */
PRStatus cgistub_set_min_children(int n);

/*
 * cgistub_set_max_children configures the maximum number of Cgistub processes
 * to keep on hand.
 */
PRStatus cgistub_set_max_children(int n);

/*
 * cgistub_set_idle_timeout configures the interval at which idle Cgistub
 * processes are reaped.
 */
PRStatus cgistub_set_idle_timeout(PRIntervalTime timeout);

#endif

#endif /* SAFS_CHILD_H */
