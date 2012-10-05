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


#include "log.h"
#include <stdio.h>
#include <stdarg.h>

static LogLevel _logLevel;
static PRLock *_logLock;
static PRUintn _threadNameKey;

static char *logLevelStr[] = { "fail", "error", "warning", "pass", "info",  "trace" };

long Logger::_pass = 0;
long Logger::_fail = 0;
long Logger::_warning = 0;

void Logger::logInitialize(LogLevel level)
{
    PRStatus rv;

    _logLock = PR_NewLock();
    rv = PR_NewThreadPrivateIndex(&_threadNameKey, NULL);
    PR_ASSERT(rv == PR_SUCCESS);
    _logLevel = level;
};

void Logger::setThreadName(char *name)
{
    PR_SetThreadPrivate(_threadNameKey, name);
};

void Logger::logError(LogLevel level, char *fmt, ...)
{
    if (level == LOGPASS)
        _pass++;
    if (level == LOGFAILURE)
        _fail++;
    if (level == LOGWARNING)
        _warning++;
        
    if (level <= _logLevel)
	{
        char buf[1024];
        va_list args;

        va_start(args, fmt);
        vsprintf(buf, fmt, args);
        va_end(args);

        char *module = logLevelStr[level];
        char *threadName = (char *)PR_GetThreadPrivate(_threadNameKey);

        PR_Lock(_logLock);
        if (threadName)
            fprintf(stdout, "%s: (%s) %s\n", module, threadName, buf);
        else
            fprintf(stdout, "%s: %s\n", module, buf);
        PR_Unlock(_logLock);
    };
};


