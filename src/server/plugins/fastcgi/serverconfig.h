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

#ifndef FCGISERVERCONFIG_H
#define FCGISERVERCONFIG_H

#include <nspr.h>
#include "constants.h"

#define DEFAULT_MIN_PROC "1"
#define DEFAULT_MAX_PROC "1"
#define DEFAULT_LISTEN_QUEUE "256"
#define DEFAULT_RESTART_INTERVAL "0"  // no restart
#define DEFAULT_REQUEST_RETRIES 0
#define DEFAULT_ENV_VAR_SIZE 10
#define DEFAULT_FAILURE_NUM "3"


//-----------------------------------------------------------------------------
// FcgiServerConfig
//-----------------------------------------------------------------------------
typedef struct {
    char *execPath; // executable path
    char *bindPath; //bind path for the server app
    PRNetAddr addr;
    struct sockaddr *servAddr;
    char *argv0;    // first element of argv[0]
    char **env;     // environment vars passed to process during startup
    char **args;     // environment vars passed to process during startup
    char *userName; // process user
    char *groupName;    // process group
    char *chDir;    // working dir of the process
    char *chRoot;   // root dir of the process
    char *maxProcesses;               // max allowed processes of this class
    char *minProcesses;               // min allowed procesess of this class
    char *priority;    // process priority
    char *rlimit_as;  // rlimit settings
    char *rlimit_core;
    char *rlimit_cpu;
    char *rlimit_nofile;
    char *listenQueueDepth;           // size of listen queue for IPC
    char *restartOnExit;              // = TRUE = restart. else terminate/free
    char *numFailures;
    PRIntervalTime restartDelayTime; // in minutes - time interval to wait before restarting the process group
    char *restartDelay;
    int procPriority;
} ProcessInfo;

class FcgiServerConfig {
public:
    FcgiServerConfig(pblock *pb, const char *vsid, const char *tmpDir, const char *pluginDir, const char *stubLogDir);
    FcgiServerConfig(const FcgiServerConfig& config);
    ~FcgiServerConfig();
    FcgiServerConfig& operator=(const FcgiServerConfig& config);

    int operator==(const FcgiServerConfig& right) const;
    int operator==(pblock *pb) const;
    int operator!=(const FcgiServerConfig& right) const;
    int operator!=(pblock *pb) const;
    PluginError getLastError() { return lastError; }

    void printProcInfo();

    int retries;                       // Number of times to retry failed requests
    PRIntervalTime poll_timeout;       // How long to wait for data
    PRIntervalTime connect_timeout;    // How long to wait for a connection
    PRIntervalTime connect_interval;   // Interval between connect() calls
    PRIntervalTime keep_alive_timeout; // Maximum time to let connections idle
    PRBool always_persistent;          // Set to use keep-alive for POSTs
    PRBool keepAliveConnection;
    PRBool remoteServer;
    PRBool udsName;
    char *fcgiTmpDir;
    char *fcgiPluginDir;
    char *fcgiStubLogDir;
    ProcessInfo procInfo;
    char *vsId;

private:
    void init(pblock *pb);
    void init(const FcgiServerConfig& config);
    void destroy();
    static inline PRBool matches(const char *provided, const char *expected);
    void getEnvVars(pblock *pb);
    void getArgs(pblock *pb);
    void formatBindPath();
    PluginError lastError;
};

/*
//-----------------------------------------------------------------------------
// FcgiServerConfig::isStickyParam
//-----------------------------------------------------------------------------

PRBool FcgiServerConfig::isStickyParam(const char *param) const
{
    return isSticky(sticky_params, param);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::isStickyCookie
//-----------------------------------------------------------------------------

PRBool FcgiServerConfig::isStickyCookie(const char *cookie) const
{
    return isSticky(sticky_cookies, cookie);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::isRouteCookie
//-----------------------------------------------------------------------------

PRBool FcgiServerConfig::isRouteCookie(const char *cookie) const
{
    return matches(cookie, route_cookie);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::isSticky
//-----------------------------------------------------------------------------

PRBool FcgiServerConfig::isSticky(const CList<char>& list, const char *name)
{
    CListConstIterator<char> iterator(&list);
    while (const char *p = (++iterator)) {
        if (matches(name, p))
            return PR_TRUE;
    }
    return PR_FALSE;
}
*/

//-----------------------------------------------------------------------------
// FcgiServerConfig::matches
//-----------------------------------------------------------------------------

PRBool FcgiServerConfig::matches(const char *provided, const char *expected)
{
    while (*provided && *provided == *expected) {
        provided++;
        expected++;
    }
    if (*expected)
        return PR_FALSE;
    if (*provided && *provided != '=')
        return PR_FALSE;
    return PR_TRUE;
}

#endif // FCGISERVERCONFIG_H
