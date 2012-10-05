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
 * main.c: Startup processing etc.
 * 
 * Rob McCool
 * 
 */

#include <stdlib.h>
#include <unistd.h>
#include "nspr.h"
#include "httpdaemon/WebServer.h"

static void server_exit(int code)
{
#ifdef IRIX
    /* IRIX doesn't go down unless you kill all the sprocs; exit() only takes
     * down this sproc.
     */
    kill(0, SIGKILL);
    exit(code);
#else
    exit(code);
#endif
}

#ifdef DEBUG
static volatile int attached = 0;
#endif

int main(int argc, char *argv[])
{
#ifdef DEBUG
    if (getenv("DEBUG")) {
        fprintf(stderr, "Process %d found DEBUG environment variable\n", getpid());
        fprintf(stderr, "Waiting for debugger to assign attached = 1 or set value at %p...\n", &attached);
        while (attached == 0)
            PR_Sleep(1);
    }
#endif

    PRStatus status = WebServer::Init(argc, argv);
    if (status == PR_SUCCESS)
        status = WebServer::Run();

    WebServer::Cleanup();

    if (status == PR_FAILURE)
        server_exit(1);
    else
        exit(0);
}
