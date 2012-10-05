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

#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#ifdef XP_WIN32
#include <windows.h>
#endif //XP_WIN32

/*
 * Constants.h :
 * This defines the constants used for FastCGI plugin
 */

#include "support/NSString.h"
#include "errortypes.h"

#define CGI_VERSION "CGI/1.1"
#define MAX_CGI_SSI_VARS         7
#define MAX_CGI_CLIENT_AUTH_VARS 64
#define MAX_EXTRA_FCGI_VARS 8

#define SERVER_PROTOCOL "HTTP/1.0"
#define IOBUFSIZE 10240
#define ENVSIZE 30
#define DEFAULT_BACKLOG_SIZE 1024
#define MAX_PIPE_BUF_SIZE 1024
#define AUTH_HEADER_PREFIX "Variable-"
#define DEFAULT_LINGER_TIMEOUT 1  //wait time(in seconds) before SIGKILL is sent to child server proc

#define FCGISTUB_INSTALL_SUFFIX PRODUCT_PLUGINS_SUBDIR"/fastcgi"

#ifdef XP_UNIX
#define DEFAULT_SLEEP_TIME 5 //seconds
#else
#define DEFAULT_SLEEP_TIME 10 //seconds
#endif //XP_UNIX
#define SLEEP_TIME_BEFORE_STUB_TERMINATE 15 //seconds
#define HTTP_PROTOCOL "http://"
#define HTTPS_PROTOCOL "https://"
#define STUB_SEMAPHORE_NAME "fastcgistubSem"
#define STUB_PID_FILE_NAME "stub.pid"
#if defined (XP_WIN32) || defined (LINUX)
  #define STUB_SEMAPHORE "fastcgistubsem"
  #define BIND_PATH_PREFIX "\\\\.\\pipe\\FastCGI\\"
#else
  #define STUB_SEMAPHORE "/fastcgistubsem"
#endif //XP_WIN32
#define DEFAULT_CHILD_EXEC_TIMEOUT 10 //milliseconds

//macros
#define MSB(x) ((x)/256)
#define LSB(x) ((x)%256)

#ifdef XP_UNIX
#ifndef min
#define min(x,y) ((x)<(y)?(x):(y))
#endif
#ifndef max
#define max(x,y) ((x)>(y)?(x):(y))
#endif
#endif

#ifdef XP_WIN32
typedef DWORD PidType;
typedef HANDLE FileDescPtr;
#else
typedef pid_t PidType;
typedef PRFileDesc * FileDescPtr;
#endif //XP_WIN32

static const int sizeBufferToClient = 16384;
static const int sizeBufferFromClient = 16384;

NSString createStubPidFileName();
#endif //_CONSTANTS_H
