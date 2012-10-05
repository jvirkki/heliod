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

#ifndef STDHANDLES_H
#define STDHANDLES_H

#include <nspr.h>

//-----------------------------------------------------------------------------
// StdHandle
//-----------------------------------------------------------------------------

class StdHandle {
public:
    // Connect this handle to the console
    PRStatus console();

    // Redirect this handle to /dev/null
    PRFileDesc* null();

    // Redirect this handle to an NSPR file descriptor
    PRFileDesc* redirect(PRFileDesc *fd);

private:
#ifdef XP_WIN32
    StdHandle(int posixfd, FILE *stdfp, DWORD nStdHandle, PRFileDesc *prfd);
#endif

#ifdef XP_UNIX
    StdHandle(int posixfd, FILE *stdfp);
#endif

    int _posixfd;
    FILE *_stdfp;
#ifdef XP_WIN32
    DWORD _nStdHandle;
    PRFileDesc *_prfd;
#endif
    PRFileDesc *_oldfd;

friend class StdHandles;
};

//-----------------------------------------------------------------------------
// StdHandles
//-----------------------------------------------------------------------------

class StdHandles {
public:
    // Initialize stdout and stderr to point to a console
    static PRStatus console();

    // Initialize stdout and stderr to point to /dev/null
    static PRStatus null();

    // StdHandle objects for stdout and stderr
    static StdHandle out;
    static StdHandle err;

private:
    StdHandles();
};

#endif // STDHANDLES_H
