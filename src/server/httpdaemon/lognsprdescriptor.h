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

#ifndef LOGNSPRDESCRIPTOR_H
#define LOGNSPRDESCRIPTOR_H

#include <nspr.h>

#include "NsprWrap/Thread.h"

//-----------------------------------------------------------------------------
// LogNsprDescriptor
//-----------------------------------------------------------------------------

class LogNsprDescriptor : protected Thread {
public:
    LogNsprDescriptor(PRFileDesc *input, PRFileDesc *output, int degree, const char *prefix = NULL);
    ~LogNsprDescriptor();

protected:
    LogNsprDescriptor();
    void init(PRFileDesc *input, PRFileDesc *output, int degree, const char *prefix);
    void terminate();

private:
    LogNsprDescriptor(const LogNsprDescriptor&);
    LogNsprDescriptor& operator=(const LogNsprDescriptor&);
    void run();

    PRFileDesc *_input;
    PRFileDesc *_output;
    int _degree;
    char *_prefix;
    PRBool _terminate;
};

//-----------------------------------------------------------------------------
// LogStdHandle
//-----------------------------------------------------------------------------

class LogStdHandle : public LogNsprDescriptor {
public:
    LogStdHandle(StdHandle &handle, int degree, const char *prefix = NULL);
    ~LogStdHandle();

private:
    StdHandle &_handle;
    PRFileDesc *_fdRead;
};

#endif // LOGNSPRDESCRIPTOR_H
