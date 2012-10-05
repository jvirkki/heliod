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

#include <nspr.h>
#include <plstr.h>
#include "frame/conf.h"
#include "frame/log.h"
#include "fastcgii18n.h"
#include "errortypes.h"

static const char *ERR_MISSING_PARAM = "Missing or Invalid Config Parameters";
static const char *ERR_STUB_START = "Stub Start Error";
static const char *ERR_STUB_CONNECTION = "Stub Connection Failure";
static const char *ERR_NO_PERMISSION = "No Permission";
static const char *ERR_STUB_REQUEST_HANDLER = "Stub Request Handling Error";
static const char *ERR_EXISTING_SERVER_PROCESS = "Server Process Exists";
static const char *ERR_SET_PARAM = "Set Parameter Failure";
static const char *ERR_INVALID_PARAM = "Invalid Parameters";
static const char *ERR_INVALID_USER_GROUP = "Invalid user and/or group";
static const char *ERR_CREATE_SERVER_PROCESS = "Server Process Creation Failure";
static const char *ERR_FASTCGI_PROTOCOL = "Fastcgi Protocol Error";
static const char *ERR_NOT_FOUND = "Not Found";
static const char *ERR_UNAUTHORIZED = "Unauthorized";
static const char *ERR_INTERNAL = "Internal Error";

const char *getErrorString(PluginError error) {
    const char *errorString = NULL;
    switch(error) {
        case NO_APP_BIND_PATH :
        case NO_BIND_PATH :
        case INVALID_BIND_PATH :
        case REQUEST_MISSING_OR_INVALID_PARAM :
            errorString = ERR_MISSING_PARAM;
            break;
        case SEMAPHORE_OPEN_ERROR :
        case STUB_PID_FILE_CREATE_FAILURE :
        case STUB_START_FAILURE :
        case STUB_NOT_RESPONDING :
        case STUB_BIND_ERROR :
        case STUB_LISTEN_ERROR :
        case STUB_ACCEPT_ERROR :
        case PIPE_CREATE_FAILURE :
        case STUB_FORK_ERROR :
        case STUB_EXEC_FAILURE :
            errorString = ERR_STUB_START;
            break;
        case STUB_SOCKET_CREATION_FAILURE :
        case STUB_CONNECT_FAILURE :
            errorString = ERR_STUB_CONNECTION;
            break;
        case STUB_STAT_FAILURE :
        case STUB_NO_PERM :
        case STAT_FAILURE:
        case NO_EXEC_PERMISSION:
        case WRITE_OTHER_PERMISSION :
            errorString = ERR_NO_PERMISSION;
            break;
        case REQUEST_SEND_FAILURE :
        case REQUEST_MEMORY_ALLOCATION_FAILURE :
        case BUILD_REQ_ERROR :
        case ERROR_RESPONSE :
        case UNKNOWN_STUB_REQ_TYPE :
        case STUB_POLL_ERROR :
        case REQUEST_THREAD_CREATE_FAILURE :
        case INVALID_REQUEST_TYPE :
            errorString = ERR_STUB_REQUEST_HANDLER;
            break;
        case PROC_EXISTS :
             errorString = ERR_EXISTING_SERVER_PROCESS;
             break;
        case SET_RLIMIT_FAILURE :
        case SET_NICE_FAILURE :
        case SET_GROUP_FAILURE :
        case SET_USER_FAILURE :
        case SET_CHDIR_FAILURE :
        case SET_CHROOT_FAILURE:
            errorString = ERR_SET_PARAM;
            break;
        case INVALID_PARAM_VALUE :
            errorString = ERR_INVALID_PARAM;
            break;
        case INVALID_USER :
        case INVALID_GROUP :
            errorString = ERR_INVALID_USER_GROUP;
            break;
        case CHILD_EXEC_FAILURE:
        case CHILD_FORK_FAILURE:
        case PROC_DOES_NOT_EXIST :
        case SERVER_SOCKET_CREATION_FAILURE :
        case SERVER_BIND_ERROR:
        case SERVER_LISTEN_ERROR:
            errorString = ERR_CREATE_SERVER_PROCESS;
            break;
        case REQUEST_INCOMPLETE_HEADER:
        case INVALID_VERSION:
        case INVALID_TYPE :
        case INVALID_RECORD:
        case INVALID_HTTP_HEADER:
        case UNKNOWN_ROLE :
        case FCGI_INVALID_SERVER:
        case FCGI_INVALID_RESPONSE:
            errorString = ERR_FASTCGI_PROTOCOL;
            break;
        case FCGI_FILTER_FILE_OPEN_ERROR:
            errorString = ERR_NOT_FOUND;
            break;
        case FCGI_NO_BUFFER_SPACE:
            errorString = ERR_INTERNAL;
            break;
        case FCGI_NO_AUTHORIZATION:
            errorString = ERR_UNAUTHORIZED;
            break;
        default :
            errorString = ERR_INTERNAL;
    }

    return errorString;
}

PRBool shouldRetry(PluginError error) {
    PRBool res;
    switch(error) {
        case NO_APP_BIND_PATH :
        case NO_BIND_PATH :
        case INVALID_BIND_PATH :
        case REQUEST_MISSING_OR_INVALID_PARAM :
        case STUB_EXEC_FAILURE :
        case STUB_STAT_FAILURE :
        case STUB_NO_PERM :
        case STAT_FAILURE:
        case NO_EXEC_PERMISSION:
        case WRITE_OTHER_PERMISSION :
        case SET_USER_FAILURE :
        case SET_CHDIR_FAILURE :
        case SET_CHROOT_FAILURE:
        case INVALID_USER :
        case INVALID_GROUP :
        case FCGI_FILTER_FILE_OPEN_ERROR:
            res = PR_FALSE;
            break;
        default :
            res = PR_TRUE;
    }

    return res;
}




