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

#include <string.h>

#include "base/ereport.h"
#include "stdhandles.h"
#include "lognsprdescriptor.h"

//-----------------------------------------------------------------------------
// LogNsprDescriptor::LogNsprDescriptor
//-----------------------------------------------------------------------------

LogNsprDescriptor::LogNsprDescriptor(PRFileDesc *input, PRFileDesc *output, int degree, const char *prefix)
: Thread("LogNsprDescriptor")
{
    init(input, output, degree, prefix);
}

LogNsprDescriptor::LogNsprDescriptor()
: Thread("LogNsprDescriptor")
{
}

//-----------------------------------------------------------------------------
// LogNsprDescriptor::~LogNsprDescriptor
//-----------------------------------------------------------------------------

LogNsprDescriptor::~LogNsprDescriptor()
{
    free(_prefix);
    _prefix = NULL;
}

//-----------------------------------------------------------------------------
// LogNsprDescriptor::init
//-----------------------------------------------------------------------------

void LogNsprDescriptor::init(PRFileDesc *input, PRFileDesc *output, int degree, const char *prefix)
{
    _input = input;
    _output = output;
    _degree = degree;
    _prefix = strdup(prefix ? prefix : "");
    _terminate = PR_FALSE;

    // Begin logging data read from the input PRFileDesc*.  Use a global thread
    // because pipe IO from a fibre on Win32 will block all NSPR user threads.
    if (_input)
        Thread::start(PR_GLOBAL_THREAD, PR_JOINABLE_THREAD);
}

//-----------------------------------------------------------------------------
// LogNsprDescriptor::terminate
//-----------------------------------------------------------------------------

void LogNsprDescriptor::terminate()
{
    _terminate = PR_TRUE;
}

//-----------------------------------------------------------------------------
// LogNsprDescriptor::run
//-----------------------------------------------------------------------------

void LogNsprDescriptor::run()
{
    char buffer[4096];
    int available = 0;

    for (;;) {
        PRInt32 rv;
        PRBool null = PR_FALSE;

        // Blocking pipe read
        rv = PR_Read(_input, &buffer[available], sizeof(buffer) - available);
        if (rv < 1) {
            // Error
            break;
        }

        // Write the data back out
        if (_output)
            PR_Write(_output, &buffer[available], rv);

        // We have some more data in our buffer
        available += rv;

        // While there's data left in the buffer...
        int consumed = 0;
        while (available - consumed > 0) {
            // Look for a LF/nul
            char *begin = &buffer[consumed];
            char *p = begin;
            char *end = &buffer[available];
            while (p < end && *p && *p != '\n' && *p != '\r')
                p++;
            if (!*p)
                null = PR_TRUE; // XXX Saw a null

            // Find length of line including any trailing LF/nul
            int rawlen = p - begin;
            if (p < end) {
                // Include trailing LF/nul
                rawlen++;
            } else if (available - consumed < sizeof(buffer)) {
                // We hit the end of the buffer without finding a LF/nul, and
                // there's room at the beginning of the buffer.  Wait for more
                // data to be ready before logging.
                break;
            }

            // Find length of line minus any trailing CR/LF/nul
            int len = rawlen;
            if (len > 0 && begin[len - 1] == '\0')
                len--;
            if (len > 0 && begin[len - 1] == '\n')
                len--;
            if (len > 0 && begin[len - 1] == '\r')
                len--;

            // Write line to the error log (don't include trailing CR/LF/nul)
            if (len > 0)
                ereport(_degree, "%s%-.*s", _prefix, len, begin);

            // We just consumed some data (including trailing CR/LF/nul)
            consumed += rawlen;
        }

        // If we consumed some data...
        if (consumed > 0) {
            if (available > consumed) {
                // Move data to the beginning of the buffer
                memmove(&buffer[0], &buffer[consumed], available - consumed);
                available -= consumed;
            } else {
                // No data left in buffer
                available = 0;
            }
        }

        // XXX Get out if we've been told to terminate
        if (null && _terminate)
            break;
    }

    // Log anything left in the buffer
    if (available)
        ereport(_degree, "%s%-.*s", _prefix, available, buffer);
}

//-----------------------------------------------------------------------------
// LogStdHandle::LogStdHandle
//-----------------------------------------------------------------------------

LogStdHandle::LogStdHandle(StdHandle &handle, int degree, const char *prefix)
: _handle(handle),
  _fdRead(NULL)
{
    // Create a new pipe
    PRFileDesc *fdWrite = NULL;
    if (PR_CreatePipe(&_fdRead, &fdWrite) == PR_SUCCESS) {
        // Install the write end of the pipe as stdout/stderr
        PR_SetFDInheritable(_fdRead, PR_FALSE);
        PR_SetFDInheritable(fdWrite, PR_TRUE);
        PRFileDesc *fdOutput = _handle.redirect(fdWrite);
        if (fdOutput)
            PR_Close(fdOutput);
    }

    LogNsprDescriptor::init(_fdRead, NULL, degree, prefix);
}

//-----------------------------------------------------------------------------
// LogStdHandle::~LogStdHandle
//-----------------------------------------------------------------------------

LogStdHandle::~LogStdHandle()
{
    // Tell the thread it should terminate
    terminate();

    // XXX Wake the thread by writing to the pipe
    PRFileDesc *fdWrite = _handle.null();
    if (fdWrite)
        PR_Write(fdWrite, "", 1);

    // Wait for the thread to terminate
    join();

    // Close the read end of the pipe
    if (_fdRead)
        PR_Close(_fdRead);
}
