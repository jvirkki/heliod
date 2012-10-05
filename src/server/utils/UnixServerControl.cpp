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

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include "prtypes.h"
#include "prthread.h"
#include "prinrval.h"
#include "base/ereport.h"
#include "definesEnterprise.h"
#include "UnixServerControl.h"
#include "base/net.h"
#include "base/util.h"
// needs to be after UnixServerControl.h, since UnixServerControl.h
// includes base/buffer.h which inturn includes netsite.h and
// safs/child.h needs netsite.h to be included before it.
#include "safs/child.h"

#define PID_FILE "pid"

#define PIPEBUF_SZ    1025
#define SIZEOF_PIPE() (PIPEBUF_SZ - 1)

/*5 minutes in seconds*/
#define TIMEOUT_SECS 300

#define DUMMY_NEWLINES "\n\n\n"

extern char **environ;

UnixServerControl::UnixServerControl(const char *installRoot,
                                     const char *instanceRoot,
                                     const char *instanceName,
                                     const char *tempDir) :
                 ServerControl(installRoot, instanceRoot, instanceName, tempDir) {
    pidPathStr.append(serverTempDir).append("/").append(PID_FILE);
    pidPath = pidPathStr.c_str();
    pid = UNIX_NO_PID;
}

UnixServerControl::~UnixServerControl() {
}

wdServerMessage *UnixServerControl::connectToWatchDog() {
    string socketstr;
    socketstr.append(serverTempDir).append("/"WDSOCKETNAME);
    int newfd = ConnectToWDMessaging((char*)socketstr.c_str());
    if (newfd >= 0) {
        wdServerMessage * wdmsg = new wdServerMessage(newfd);
        if (wdmsg)
            return wdmsg;
    } else {
        bufferOutput("failure: failed to connect to watchdog\n");
    }

    return NULL;
}

bool UnixServerControl::sendMessageToWatchdog(
                                    wdServerMessage **msg,
                                    WDMessages sendMsgType,
                                    const char *sendMsgTypeStr,
                                    const char *sendMsg,
                                    WDMessages recvMsgType,
                                    char **recvMsg) {
    wdServerMessage *wdmsg = connectToWatchDog();
    if (wdmsg == NULL) {
        bufferOutput("failure: failed to connect to server watchdog\n");
        return false;
    }
    int rc = wdmsg->SendToWD(sendMsgType, sendMsg);
    if (rc != 1) {
        bufferOutput("failure: failed to send watchdog %s message\n", sendMsgTypeStr);
        delete wdmsg;
        return false;
    }
    *recvMsg = wdmsg->RecvFromWD();
    if (wdmsg->getLastMsgType() != recvMsgType) {
        bufferOutput("failure: failed to get watchdog %s message reply\n", sendMsgTypeStr);
        delete wdmsg;
        return false;
    }

    *msg = wdmsg;

    return true;
}

bool UnixServerControl::getPid() {
    if (pid != UNIX_NO_PID)
        return true;

    struct stat finfo;
    bool ret = false;
    if (stat(pidPath, &finfo) == 0)  {
        FILE *p = fopen(pidPath, "r");
        if(p) {
            int z;
            if ((fscanf(p, "%d\n", &z)) != -1) {
                pid = (pid_t) z;
                ret = true;
            }
            fclose(p);
        }
    }
    return ret;
}

bool UnixServerControl::isRunning() {
    if (!getPid())
        return false;
    if((kill(pid, 0) != -1) || (errno == EPERM)) {
        /* success! it must be running. */
        return true;
    }
    pid = UNIX_NO_PID;
    return false;
}

bool UnixServerControl::reconfigServer() {
    if (!isRunning())
        return false;

    wdServerMessage *wdmsg;
    char *msgReply;
    if (!sendMessageToWatchdog(&wdmsg, wdmsgReconfigure, "reconfigure",
                NULL, wdmsgReconfigurereply, &msgReply)) {
        return false;
    }
    bool retV = true;
    string errMsg;
    while (1) {
        msgReply = wdmsg->RecvFromWD();
        if (wdmsg->getLastMsgType() != wdmsgGetReconfigStatusreply) {
            bufferOutput("failure: failed to get watchdog reconfigure message reply\n");
            delete wdmsg;
            return false;
        }
        if (msgReply) {		// print status from server
            if (strlen(msgReply)) {
                if (strstr(msgReply, RECONFIG_FAILED) != NULL) {
                    retV = false;
                    break;
                } else if (strstr(msgReply, CHANGES_INCOMPATIBLE) != NULL) {
                    char *msg = strstr(msgReply, CHANGES_INCOMPATIBLE) + strlen(CHANGES_INCOMPATIBLE);
                    errMsg.append(msg);
                } else if (strstr(msgReply, CHANGES_IGNORED) != NULL) {
                    char *msg = strstr(msgReply, CHANGES_IGNORED) + strlen(CHANGES_IGNORED);
                    errMsg.append(msg);
                }
                char* eol = strrchr(msgReply, '\n');
                if (eol && !eol[1]) {
                    *eol = '\0';
                }
                bufferOutput("%s\n", msgReply);
            } else {
                break;		// empty message
            }
        } else {
            break;
        }
    } /* end while */
    delete wdmsg;
    if (errMsg.size() > 0)
        throw RestartRequired(errMsg);
    return retV;
}
