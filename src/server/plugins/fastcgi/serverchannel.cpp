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

#include <ssl.h>
#include "frame/http.h"
#include "frame/log.h"
#include "base/ereport.h"
#include "constants.h"
#include "serverchannel.h"

//-----------------------------------------------------------------------------
// FcgiServerChannel::FcgiServerChannel
//-----------------------------------------------------------------------------
FcgiServerChannel::FcgiServerChannel(const FcgiServerConfig *conf)
: to(sizeBufferFromClient),
  from(sizeBufferToClient),
  countTransactions(0),
#ifdef XP_WIN32
   EndPoint(conf->procInfo.addr.inet.family, &from, &to, PR_FALSE, conf->udsName),
#else
  EndPoint(conf->procInfo.addr.inet.family, &from, &to),
#endif // XP_WIN32
  config(conf)
{
  reset();
}

//-----------------------------------------------------------------------------
// FcgiServerChannel::~FcgiServerChannel
//-----------------------------------------------------------------------------
FcgiServerChannel::~FcgiServerChannel()
{
    if (fd) {
#ifdef XP_WIN32
        if (config->udsName) {
            HANDLE hd = (HANDLE)PR_FileDesc2NativeHandle(fd);
            CloseHandle(hd);
        } else {
#endif // XP_WIN32
        PR_Close(fd);
#ifdef XP_WIN32
        }
#endif // XP_WIN32
        fd = NULL;
    }
}

//-----------------------------------------------------------------------------
// FcgiServerChannel::connect
//-----------------------------------------------------------------------------
PRStatus FcgiServerChannel::connect(PRIntervalTime timeoutVal)
{
    // Try to connect
#ifdef XP_WIN32
    if (config->udsName) {
        PRBool pipeBusy = PR_TRUE;
        while(pipeBusy) {
            HANDLE newFd = CreateFile(config->procInfo.bindPath,   // pipe name
                               GENERIC_READ | GENERIC_WRITE, // read and write access
                               FILE_SHARE_WRITE | FILE_SHARE_READ,              // sharing
                               NULL,           // default security attributes
                               OPEN_ALWAYS,  // opens existing pipe or creates a new one
                               FILE_FLAG_OVERLAPPED,              // default attributes
                               NULL);          // no template file


            // Break if the pipe handle is valid.
            if (newFd != INVALID_HANDLE_VALUE) {
                fd = PR_ImportFile((int)newFd);
                return PR_SUCCESS;
            }

            if (!WaitNamedPipe(config->procInfo.bindPath, PR_IntervalToMilliseconds(timeoutVal))) {
                 return PR_FAILURE;
            }

            if (GetLastError() != ERROR_PIPE_BUSY) {
                pipeBusy = PR_FALSE;
            }
        }

        return PR_FAILURE;

    } else {
#endif // XP_WIN32
    if (!fd)
        return PR_FAILURE;

    PRStatus rv = PR_Connect(fd, &(config->procInfo.addr), timeoutVal);
    return rv;

#ifdef XP_WIN32
    }
#endif // XP_WIN32

}


//-----------------------------------------------------------------------------
// FcgiServerChannel::reset
//-----------------------------------------------------------------------------
void FcgiServerChannel::reset()
{
    // Prepare for the next transaction
    to.reset();
    from.reset();

    flagHead = PR_FALSE;
    flagPersistent = PR_FALSE;
    flagServerError = PR_FALSE;
    flagClientError = PR_FALSE;
    flagReadable = PR_TRUE;
    flagWritable = PR_TRUE;
    countBytesSent = 0;
}

//-----------------------------------------------------------------------------
// FcgiServerChannel::setPersistent
//-----------------------------------------------------------------------------
void FcgiServerChannel::setPersistent() {
    flagPersistent = PR_TRUE;
    // enable socket keepalive
    if (fd) {
#ifdef XP_WIN32
        if (!config->udsName) {
#endif // XP_WIN32
        PRSocketOptionData data;
        data.option = PR_SockOpt_Keepalive;
        data.value.keep_alive = flagPersistent;
        PRStatus rv = PR_SetSocketOption(fd, &data);
#ifdef XP_WIN32
        }
#endif // XP_WIN32

    }
}

