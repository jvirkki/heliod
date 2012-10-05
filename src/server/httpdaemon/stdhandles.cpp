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

#ifdef XP_WIN32
#include <io.h>
#endif
#include <fcntl.h>

#include "private/pprio.h"
#include "base/util.h"
#include "frame/conf.h"
#include "stdhandles.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#ifdef XP_UNIX
#define DEV_TTY "/dev/tty"
#define DEV_NULL "/dev/null"
#endif

#ifdef XP_WIN32
#define DEV_NULL "nul"
#endif

//-----------------------------------------------------------------------------
// StdHandles static member initialization
//-----------------------------------------------------------------------------

#ifdef XP_WIN32
StdHandle StdHandles::out(1, stdout, STD_OUTPUT_HANDLE, PR_STDOUT);
StdHandle StdHandles::err(2, stderr, STD_ERROR_HANDLE, PR_STDERR);
#endif

#ifdef XP_UNIX
StdHandle StdHandles::out(1, stdout);
StdHandle StdHandles::err(2, stderr);
#endif

//-----------------------------------------------------------------------------
// dupStdHandle (Win32)
//-----------------------------------------------------------------------------

#ifdef XP_WIN32
HANDLE dupStdHandle(DWORD nStdHandle)
{
    HANDLE hDupHandle = INVALID_HANDLE_VALUE;
    DuplicateHandle(GetCurrentProcess(), GetStdHandle(nStdHandle), GetCurrentProcess(), &hDupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);
    return hDupHandle;
}
#endif

//-----------------------------------------------------------------------------
// setStdHandle (Win32)
//-----------------------------------------------------------------------------

#ifdef XP_WIN32
static PRStatus setStdHandle(HANDLE hStdHandle, DWORD nStdHandle, int posixfd, FILE *stdfp, PRFileDesc *prfd)
{
    // Take a Win32 HANDLE and use it for the specific GetStdHandle() index,
    // ANSI FILE*, and POSIX fd

    // Use hStdHandle as the standard handle
    SetStdHandle(nStdHandle, hStdHandle);

    // Get a POSIX fd from the Win32 handle
    int newfd = _open_osfhandle((long)hStdHandle, O_BINARY);
    if (newfd == -1)
        return PR_FAILURE;

    // Make sure the FILE* uses the right POSIX fd number
    if (newfd != posixfd) {
        dup2(newfd, posixfd);
        // XXX It seems that calling close(newfd) will mess up posixfd; perhaps
        // they end up sharing the same underlying HANDLE?
        // close(newfd);
    }
    _fileno(stdfp) = posixfd;

    // Reset FILE* buffering
    setvbuf(stdfp, NULL, _IONBF, 0);

    // Point the NSPR PR_STDOUT/PR_STDERR to the new HANDLE
    PR_ChangeFileDescNativeHandle(prfd, (PRInt32)hStdHandle);

    return PR_SUCCESS;
}
#endif

//-----------------------------------------------------------------------------
// setStdFd (Unix)
//-----------------------------------------------------------------------------

#ifdef XP_UNIX
static PRStatus setStdFd(int newfd, int posixfd, FILE *stdfp)
{
    if (newfd == -1)
        return PR_FAILURE;

    if (newfd != posixfd) {
        dup2(newfd, posixfd);
        close(newfd);
    }

    // We want blocking IO so user write()s don't fail with EAGAIN
    int flags = fcntl(posixfd, F_GETFL, 0);
    if (flags != -1)
        fcntl(posixfd, F_SETFL, flags & (~O_NONBLOCK));

    // Keep the Solaris C runtime happy
    setvbuf(stdfp, NULL, _IONBF, 0);

    return PR_SUCCESS;
}
#endif

//-----------------------------------------------------------------------------
// StdHandle::StdHandle
//-----------------------------------------------------------------------------

#ifdef XP_WIN32
StdHandle::StdHandle(int posixfd, FILE *stdfp, DWORD nStdHandle, PRFileDesc *prfd)
: _posixfd(posixfd), _stdfp(stdfp), _nStdHandle(nStdHandle), _prfd(prfd), _oldfd(NULL)
{
}
#endif

#ifdef XP_UNIX
StdHandle::StdHandle(int posixfd, FILE *stdfp)
: _posixfd(posixfd), _stdfp(stdfp), _oldfd(NULL)
{
}
#endif

//-----------------------------------------------------------------------------
// StdHandle::redirect
//-----------------------------------------------------------------------------

PRFileDesc* StdHandle::redirect(PRFileDesc *fd)
{
    if (!fd)
        return NULL;

    // Get a copy of the current stdout/stderr
#ifdef XP_WIN32
    PRInt32 osfd = (PRInt32)dupStdHandle(_nStdHandle);
#endif
#ifdef XP_UNIX
    PRInt32 osfd = dup(_posixfd);
#endif

    // Import the current stdout/stderr into a PRFileDesc* we can return
    PRFileDesc *fdPrevious = _oldfd;
    if (!fdPrevious) {
#ifdef XP_WIN32
        // Assume the initial stdout/stderr was a pipe
        fdPrevious = PR_ImportPipe(osfd);
#else
        // Assume the initial stdout/stderr was a file
        fdPrevious = PR_ImportFile(osfd);
#endif
    } else if (osfd != -1) {
        // Use the PRFileDesc* from the previous redirect()
        PR_ChangeFileDescNativeHandle(fdPrevious, osfd);
    }

    // Use the new fd for stdout/stderr
    _oldfd = fd;
#ifdef XP_WIN32
    setStdHandle((HANDLE)PR_FileDesc2NativeHandle(fd), _nStdHandle, _posixfd, _stdfp, _prfd);
#endif
#ifdef XP_UNIX
    setStdFd(PR_FileDesc2NativeHandle(fd), _posixfd, _stdfp);
#endif

    return fdPrevious;
}

//-----------------------------------------------------------------------------
// StdHandle::null
//-----------------------------------------------------------------------------

PRFileDesc* StdHandle::null()
{
    PRFileDesc *fd = PR_Open(DEV_NULL, PR_WRONLY, 0666);
    return redirect(fd);
}

//-----------------------------------------------------------------------------
// StdHandle::console
//-----------------------------------------------------------------------------

PRStatus StdHandle::console()
{
#ifdef XP_WIN32
    setStdHandle(GetStdHandle(_nStdHandle), _nStdHandle, _posixfd, _stdfp, _prfd);
    return PR_SUCCESS;
#endif

#ifdef XP_UNIX
    // Is our work already done?
    if (isatty(_posixfd) == 1)
        return PR_SUCCESS;

    // Redirect fd to /dev/tty
    PRFileDesc *fd = PR_Open(DEV_NULL, PR_WRONLY, 0666);
    fd = redirect(fd);
    if (!fd)
        return PR_FAILURE;

    PR_Close(fd);

    return PR_SUCCESS;
#endif
}

//-----------------------------------------------------------------------------
// StdHandles::console
//-----------------------------------------------------------------------------

PRStatus StdHandles::console()
{
    PRStatus rv = PR_SUCCESS;

#ifdef XP_WIN32
    // Try to create a console
    if (!AllocConsole())
        return PR_FAILURE;

    // Make sure stdout and stderr have unique handles
    if (GetStdHandle(STD_ERROR_HANDLE) == GetStdHandle(STD_OUTPUT_HANDLE))
        SetStdHandle(STD_ERROR_HANDLE, dupStdHandle(STD_OUTPUT_HANDLE));
#endif

    if (out.console() != PR_SUCCESS)
        rv = PR_FAILURE;

    if (err.console() != PR_SUCCESS)
        rv = PR_FAILURE;

    return rv;
}

//-----------------------------------------------------------------------------
// StdHandles::null
//-----------------------------------------------------------------------------

PRStatus StdHandles::null()
{
    PRStatus rv = PR_SUCCESS;
    PRFileDesc *fd;

    fd = out.null();
    if (fd)
        PR_Close(fd);
    else
        rv = PR_FAILURE;

    fd = err.null();
    if (fd)
        PR_Close(fd);
    else
        rv = PR_FAILURE;

    return rv;
}
