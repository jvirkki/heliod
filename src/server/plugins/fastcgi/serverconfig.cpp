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

#ifdef XP_UNIX
#include <libgen.h>
#endif //XP_UNIX

#include "base/util.h"
#include "frame/log.h"
#include "base64.h"
#include "util.h"
#include "fastcgii18n.h"
#include "serverconfig.h"

extern char LOG_FUNCTION_NAME[];

//-----------------------------------------------------------------------------
// getTimeout
//-----------------------------------------------------------------------------
static inline PRIntervalTime getTimeout(const char *p, int def) {
    int ms = def;
    if (p) {
        //convert to milliseconds
        ms = atoi(p) * 1000;
    }
    return PR_MillisecondsToInterval(ms);
}

PRBool validIntParameter(const char *s) {
    char *temp = "";
    if(!s)
        return PR_FALSE;
    strtol(s, &temp, 10);
    if(temp != NULL && strlen(temp) > 0)
        return PR_FALSE;
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::FcgiServerConfig
//-----------------------------------------------------------------------------
FcgiServerConfig::FcgiServerConfig(pblock *pb, const char *vsid,
                  const char *tmpDir, const char *pluginDir, const char *stubLogDir) {
    vsId = PL_strdup(vsid);
    fcgiTmpDir = PL_strdup((char *)tmpDir);
    fcgiPluginDir = PL_strdup((char *)pluginDir);
    fcgiStubLogDir = PL_strdup((char *)stubLogDir);
    lastError = NO_FCGI_ERROR;
    procInfo.execPath = NULL;
    procInfo.bindPath = NULL;
    procInfo.servAddr = NULL;
    procInfo.argv0 = NULL;
    procInfo.env = NULL;
    procInfo.args = NULL;
    procInfo.userName = NULL;
    procInfo.groupName = NULL;
    procInfo.chDir = NULL;
    procInfo.chRoot = NULL;
    procInfo.maxProcesses = NULL;
    procInfo.minProcesses = NULL;
    procInfo.priority = NULL;
    procInfo.rlimit_as = NULL;
    procInfo.rlimit_core = NULL;
    procInfo.rlimit_cpu = NULL;
    procInfo.rlimit_nofile = NULL;
    procInfo.listenQueueDepth = NULL;
    procInfo.restartOnExit = NULL;
    procInfo.numFailures = NULL;
    procInfo.restartDelay = NULL;
    init(pb);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::FcgiServerConfig
//-----------------------------------------------------------------------------
FcgiServerConfig::FcgiServerConfig(const FcgiServerConfig& config) {
    lastError = NO_FCGI_ERROR;
    //initialize
    init(config);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::~FcgiServerConfig
//-----------------------------------------------------------------------------
FcgiServerConfig::~FcgiServerConfig() {
    destroy();
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::operator=
//-----------------------------------------------------------------------------
FcgiServerConfig& FcgiServerConfig::operator=(const FcgiServerConfig& config) {
    if (&config != this) {
        //destroy the existing config
        destroy();
        //re initialize to the new one
        init(config);
    }
    return *this;
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::init
//-----------------------------------------------------------------------------
void FcgiServerConfig::init(pblock *pb) {
    const char *p;

    //read app-path, set to NULL if not specified
    if(p = pblock_findval("app-path", pb)) {
        procInfo.execPath = PL_strdup(p);
        procInfo.argv0 = PL_strdup(procInfo.execPath);
    } else {
        procInfo.execPath = NULL;
        procInfo.argv0 = NULL;
    }

    //read bind-path, set to NULL if not specified
    if(p = pblock_findval("bind-path", pb)) {
        procInfo.bindPath = PL_strdup(p);
    } else
        procInfo.bindPath = NULL;

    //check app-path and bind-path
    formatBindPath();
    log_ereport(LOG_VERBOSE, procInfo.bindPath);

    // check for any errors post format
    if(lastError != NO_FCGI_ERROR)
         return;

    if(procInfo.bindPath) {
        if(getNetAddr(procInfo.bindPath, &(procInfo.addr)) != PR_SUCCESS) {
            lastError = INVALID_BIND_PATH;
            log_ereport(LOG_MISCONFIG, getError());
            return;
        }
    }

    //read min-procs, defaults to 1
    if(p = pblock_findval("min-procs", pb)) {
        if(validIntParameter(p))
            procInfo.minProcesses = PL_strdup(p);
        else {
            log_ereport(LOG_WARN, GetString(DBT_invalid_min_parameter), p, DEFAULT_MIN_PROC);
            procInfo.minProcesses = PL_strdup(DEFAULT_MIN_PROC);
        }
    } else
        procInfo.minProcesses = PL_strdup(DEFAULT_MIN_PROC);

    //read max-procs, defaults to 1-
    if(p = pblock_findval("max-procs", pb)) {
        if(validIntParameter(p))
            procInfo.maxProcesses = PL_strdup(p);
        else {
            log_ereport(LOG_WARN, GetString(DBT_invalid_max_parameter), p, DEFAULT_MAX_PROC);
            procInfo.maxProcesses = PL_strdup(DEFAULT_MAX_PROC);
        }
    } else
        procInfo.maxProcesses = PL_strdup(DEFAULT_MAX_PROC);

    //read chroot
    if(p = pblock_findval("chroot", pb)) {
        procInfo.chRoot = PL_strdup(p);
    } else
        procInfo.chRoot = NULL;

    //read chdir
    if(p = pblock_findval("chdir", pb)) {
        procInfo.chDir = PL_strdup(p);
    } else
        procInfo.chDir = NULL;

    //read user
    if(p = pblock_findval("user", pb)) {
        procInfo.userName = PL_strdup(p);
    } else
        procInfo.userName = NULL;

    //read group
    if(p = pblock_findval("group", pb)) {
        procInfo.groupName = PL_strdup(p);
    } else
        procInfo.groupName = NULL;

    //read nice
    if(p = pblock_findval("nice", pb)) {
        if(validIntParameter(p))
            procInfo.priority = PL_strdup(p);
        else {
            log_ereport(LOG_WARN, GetString(DBT_invalid_nice_parameter), p, "0");
            procInfo.priority = NULL;
        }
    } else
        procInfo.priority = NULL;

    //read listen-queue
    if(p = pblock_findval("listen-queue", pb)) {
        if(validIntParameter(p))
            procInfo.listenQueueDepth = PL_strdup(p);
        else {
            log_ereport(LOG_WARN, GetString(DBT_invalid_listen_q_parameter), p, DEFAULT_LISTEN_QUEUE);
            procInfo.listenQueueDepth = PL_strdup(DEFAULT_LISTEN_QUEUE);
        }
    } else
        procInfo.listenQueueDepth = PL_strdup(DEFAULT_LISTEN_QUEUE);

    //read number_of_failures
    if(p = pblock_findval("number_of_failures", pb))
        procInfo.numFailures = PL_strdup(p);
    else
        procInfo.numFailures = PL_strdup(DEFAULT_FAILURE_NUM);

    //read restart_on_exit, defaults to true
    if(p = pblock_findval("restart_on_exit", pb))
        procInfo.restartOnExit = PL_strdup(p);
    else
        procInfo.restartOnExit = PL_strdup("1");

    //read reuse-connection, defaults to false
    keepAliveConnection = util_getboolean(pblock_findval("reuse-connection", pb), PR_FALSE);
#ifdef XP_WIN32
    if(udsName && keepAliveConnection)
        keepAliveConnection = PR_FALSE;
#endif // XP_WIN32

    //read req-retry
    if (p = pblock_findval("req-retry", pb)) {
        if(validIntParameter(p)) {
            retries = atoi(p);
        } else {
            log_ereport(LOG_WARN, GetString(DBT_invalid_req_retry_parameter), p, DEFAULT_REQUEST_RETRIES);
            retries = DEFAULT_REQUEST_RETRIES;
        }
    } else
        retries = DEFAULT_REQUEST_RETRIES;

    //read resp-timeout
    poll_timeout = getTimeout(pblock_findval("resp-timeout", pb),
                              5 * 60 * 1000);     // default 5 minutes

    //read connection-timeout
    connect_timeout = getTimeout(pblock_findval("connection-timeout", pb),
                                 5 * 1000); // default 5 seconds

    //read connect-interval
    connect_interval = getTimeout(pblock_findval("connect-interval", pb),
                                  500);      // default 500 ms

    keep_alive_timeout = connect_timeout;

    //read restart-interval
    if(p = pblock_findval("restart-interval", pb))
        procInfo.restartDelay = PL_strdup(p);
    else
        procInfo.restartDelay = NULL;


    //read rlimit_as
    if(p = pblock_findval("rlimit_as", pb)) {
        procInfo.rlimit_as = PL_strdup(p);
    } else
        procInfo.rlimit_as = NULL;

    //read rlimit_core
    if(p = pblock_findval("rlimit_core", pb)) {
        procInfo.rlimit_core = PL_strdup(p);
    } else
        procInfo.rlimit_core = NULL;

    //read rlimit_cpu
    if(p = pblock_findval("rlimit_cpu", pb)) {
        procInfo.rlimit_cpu = PL_strdup(p);
    } else
        procInfo.rlimit_cpu = NULL;

    //read rlimit_nofile
    if(p = pblock_findval("rlimit_nofile", pb))
        procInfo.rlimit_nofile = PL_strdup(p);
    else
        procInfo.rlimit_nofile = NULL;

    //form the environment strings
    getEnvVars(pb);
    //form the process arguments
    getArgs(pb);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::formatBindPath
//-----------------------------------------------------------------------------
void FcgiServerConfig::formatBindPath() {
    udsName = PR_FALSE;
    remoteServer = PR_FALSE;

    if(procInfo.execPath) {
        if(!procInfo.bindPath) {
            // app-path is specified but not bind-path
            // so, create plugin specific bindpath
            char *appName = baseName(procInfo.execPath);
            procInfo.bindPath = (char *)PR_Malloc(MAX_STRING_LEN);
            udsName = PR_TRUE;
#ifdef XP_UNIX
            PR_snprintf(procInfo.bindPath, MAX_STRING_LEN, "%s%c%s_%d", fcgiTmpDir, FILE_PATHSEP, appName, getppid());
#else
            //on windows, bindpath will be "BIND_PATH_PREFIX<appname>"
            PR_snprintf(procInfo.bindPath, MAX_STRING_LEN, "%s%s", BIND_PATH_PREFIX, appName);
#endif //XP_UNIX
            PL_strfree(appName);
        } else {
            //check if the bind-path format matches <host>:<port>
            //otherwise, assume the name to be a UDS name
            udsName = isUds(procInfo.bindPath);
            if(udsName) {
#ifdef XP_UNIX
                //if UDS name is specified, create the socket in
                //"<webserver tmpdir>/<UDS name>"
                int newLen = strlen(fcgiTmpDir) + strlen(procInfo.bindPath) + 2;
                char *newBindPath = (char *)PR_Malloc(newLen);
                PR_snprintf(newBindPath, newLen, "%s/%s", fcgiTmpDir, procInfo.bindPath);
                PR_Free(procInfo.bindPath);
                procInfo.bindPath = newBindPath;
#else
                char *newBindPath = (char *)PR_Malloc(MAX_STRING_LEN);
                PR_snprintf(newBindPath, MAX_STRING_LEN, "%s%s", BIND_PATH_PREFIX, procInfo.bindPath);
                PR_Free(procInfo.bindPath);
                procInfo.bindPath = newBindPath;
#endif //XP_UNIX
            }
        }
        log_error(LOG_VERBOSE, LOG_FUNCTION_NAME, NULL, NULL, "bind-path = %s", procInfo.bindPath);

    } else {
        if(!procInfo.bindPath) {
            //both app-path and bind-path are missing !
            //log the error msg
            lastError = NO_APP_BIND_PATH;
            log_ereport(LOG_MISCONFIG, GetString(DBT_no_app_bind_path));
        } else {
            //no app-path, so consider it as remote application
            remoteServer = PR_TRUE;
        }
    }
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::getEnvVars
//-----------------------------------------------------------------------------
void FcgiServerConfig::getEnvVars(pblock *pb) {
    int i;
    int index;
    char **env = NULL;

    //initialize env
    env = fcgi_util_env_create(env, DEFAULT_ENV_VAR_SIZE, &index);

    // Iterate over the pblock looking for app-env="name=value"
    for (i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        while (p) {
           const char *name = p->param->name;
           const char *value = p->param->value;
           if (*name == 'a' && (!strcmp(name, "app-env")) &&
               //if app-env specified, add it to env list
               value && strlen(value) > 0) {
               env[index++] = fcgi_util_arg_str(const_cast<char *>(value));
           }

           p = p->next;
        }
    }

    if(index > 0) {
        //if there 1 or more app-envs specified, add NULL at the end of env list
        env[index] = NULL;
        procInfo.env = env;
    } else {
        PR_Free(env);
        procInfo.env = NULL;
    }
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::getArgs
//-----------------------------------------------------------------------------
void FcgiServerConfig::getArgs(pblock *pb) {
    int i;
    int index = -1;
    char **args = NULL;

    //initialize env
    args = fcgi_util_env_create(args, DEFAULT_ENV_VAR_SIZE, &index);

    // Iterate over the pblock looking for app-args="value"
    for (i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        while (p) {
           const char *name = p->param->name;
           const char *value = p->param->value;
           if (*name == 'a' && (!strcmp(name, "app-args")) &&
                value && strlen(value) > 0) {
                //if app-args specified, add the value to the args list
                args[index++] = fcgi_util_arg_str(const_cast<char *>(value));
           }

           p = p->next;
        }
    }

    if(index > 0) {
        //add null at the end of the args list
        args[index] = NULL;
        procInfo.args = args;
    } else {
        PR_Free(args);
        procInfo.args = NULL;
    }
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::init
//-----------------------------------------------------------------------------
void FcgiServerConfig::init(const FcgiServerConfig& config) {
    procInfo.execPath = config.procInfo.execPath ? PL_strdup(config.procInfo.execPath) : NULL;
    procInfo.bindPath = config.procInfo.bindPath ? PL_strdup(config.procInfo.bindPath) : NULL;
    procInfo.addr = config.procInfo.addr;
    procInfo.argv0 = PL_strdup(config.procInfo.argv0);
    procInfo.minProcesses = PL_strdup(config.procInfo.minProcesses);
    procInfo.maxProcesses = PL_strdup(config.procInfo.maxProcesses);
    procInfo.chRoot = PL_strdup(config.procInfo.chRoot) ? config.procInfo.chRoot : NULL;
    procInfo.chDir = PL_strdup(config.procInfo.chDir) ? config.procInfo.chDir : NULL;
    procInfo.userName = config.procInfo.userName ? PL_strdup(config.procInfo.userName) : NULL;
    procInfo.groupName = config.procInfo.groupName ? PL_strdup(config.procInfo.groupName) : NULL;
    procInfo.priority = config.procInfo.priority ? PL_strdup(config.procInfo.priority) : NULL;
    procInfo.listenQueueDepth = PL_strdup(config.procInfo.listenQueueDepth);
    retries = config.retries;
    poll_timeout = config.poll_timeout;
    connect_timeout = config.connect_timeout;
    connect_interval = config.connect_interval;
    keep_alive_timeout = config.keep_alive_timeout;
    keepAliveConnection = config.keepAliveConnection;
    procInfo.restartDelay = config.procInfo.restartDelay ? PL_strdup(config.procInfo.restartDelay) : NULL;
    procInfo.restartOnExit = PL_strdup(config.procInfo.restartOnExit);
    procInfo.numFailures = PL_strdup(config.procInfo.numFailures);
    procInfo.rlimit_as = config.procInfo.rlimit_as ? PL_strdup(config.procInfo.rlimit_as) : NULL;
    procInfo.rlimit_core = config.procInfo.rlimit_core ? PL_strdup(config.procInfo.rlimit_core) : NULL;
    procInfo.rlimit_cpu = config.procInfo.rlimit_cpu ? PL_strdup(config.procInfo.rlimit_cpu) : NULL;
    procInfo.rlimit_nofile = config.procInfo.rlimit_nofile ? PL_strdup(config.procInfo.rlimit_nofile) : NULL;
    procInfo.env = NULL;
    procInfo.args = NULL;
    procInfo.env = fcgi_util_env_copy(config.procInfo.env, procInfo.env);
    procInfo.args = fcgi_util_env_copy(config.procInfo.args, procInfo.args);
    vsId = config.vsId ? PL_strdup(config.vsId) : NULL;
    remoteServer = config.remoteServer;
    udsName = config.udsName;
    fcgiTmpDir = PL_strdup(config.fcgiTmpDir);
    fcgiPluginDir = PL_strdup(config.fcgiPluginDir);
    fcgiStubLogDir = PL_strdup(config.fcgiStubLogDir);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::destroy
//-----------------------------------------------------------------------------
void FcgiServerConfig::destroy() {
    if(fcgiPluginDir) {
        PL_strfree(fcgiPluginDir);
        fcgiPluginDir = NULL;
    }
    if(fcgiStubLogDir) {
        PL_strfree(fcgiStubLogDir);
        fcgiStubLogDir = NULL;
    }
    if(fcgiTmpDir) {
        PL_strfree(fcgiTmpDir);
        fcgiTmpDir = NULL;
    }
    if(vsId) {
        PL_strfree(vsId);
        vsId = NULL;
    }
    if(&procInfo) {
        if(procInfo.execPath) {
            PL_strfree(procInfo.execPath);
            procInfo.execPath = NULL;
        }
        if(procInfo.bindPath) {
            PL_strfree(procInfo.bindPath);
            procInfo.bindPath = NULL;
        }
        if(procInfo.servAddr) {
            delete (procInfo.servAddr);
            procInfo.servAddr = NULL;
        }
        if(procInfo.argv0) {
            PL_strfree(procInfo.argv0);
            procInfo.argv0 = NULL;
        }
        if(procInfo.env) {
            fcgi_util_env_free(procInfo.env);
            procInfo.env = NULL;
        }
        if(procInfo.args) {
            fcgi_util_env_free(procInfo.args);
            procInfo.args = NULL;
        }
        if(procInfo.userName) {
            PL_strfree(procInfo.userName);
            procInfo.userName = NULL;
        }
        if(procInfo.groupName) {
            PL_strfree(procInfo.groupName);
            procInfo.groupName = NULL;
        }
        if(procInfo.chDir) {
            PL_strfree(procInfo.chDir);
            procInfo.chDir = NULL;
        }
        if(procInfo.chRoot) {
            PL_strfree(procInfo.chRoot);
            procInfo.chRoot = NULL;
        }
        if(procInfo.maxProcesses) {
            PL_strfree(procInfo.maxProcesses);
            procInfo.maxProcesses = NULL;
        }
        if(procInfo.minProcesses) {
            PL_strfree(procInfo.minProcesses);
            procInfo.minProcesses = NULL;
        }
        if(procInfo.priority) {
            PL_strfree(procInfo.priority);
            procInfo.priority = NULL;
        }
        if(procInfo.rlimit_as) {
            PL_strfree(procInfo.rlimit_as);
            procInfo.rlimit_as = NULL;
        }
        if(procInfo.rlimit_core) {
            PL_strfree(procInfo.rlimit_core);
            procInfo.rlimit_core = NULL;
        }
        if(procInfo.rlimit_cpu) {
            PL_strfree(procInfo.rlimit_cpu);
            procInfo.rlimit_cpu = NULL;
        }
        if(procInfo.rlimit_nofile) {
            PL_strfree(procInfo.rlimit_nofile);
            procInfo.rlimit_nofile = NULL;
        }
        if(procInfo.listenQueueDepth) {
            PL_strfree(procInfo.listenQueueDepth);
            procInfo.listenQueueDepth = NULL;
        }
        if(procInfo.restartOnExit) {
            PL_strfree(procInfo.restartOnExit);
            procInfo.restartOnExit = NULL;
        }
        if(procInfo.numFailures) {
            PL_strfree(procInfo.numFailures);
            procInfo.numFailures = NULL;
        }
        if(procInfo.restartDelay) {
            PL_strfree(procInfo.restartDelay);
            procInfo.restartDelay = NULL;
        }
    }
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::operator==
//-----------------------------------------------------------------------------
int FcgiServerConfig::operator==(const FcgiServerConfig& right) const {
    //currently, check is done only for app-path and bind-path params as
    //these are the params that are used to distinguish the fastcgi process
    return strmatch(procInfo.execPath, right.procInfo.execPath) &&
           strmatch(procInfo.bindPath, right.procInfo.bindPath);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::operator==
//-----------------------------------------------------------------------------
int FcgiServerConfig::operator==(pblock *pb) const {
    PRBool bindPathSpecified = PR_TRUE;
    int match = 0;

    //currently, check is done only for app-path and bind-path params as
    //these are the params that are used to distinguish the fastcgi process
    char *param1 = pblock_findval("app-path", pb);
    char *param2 = pblock_findval("bind-path", pb);

    //check if app path is specified but not the bind path
    if(param1 && !param2) {
        //form the internal bind path and compare this with the
        //existing the value
        char *appName = baseName(param1);
        param2 = (char *)MALLOC(MAX_STRING_LEN);
#ifdef XP_UNIX
        PR_snprintf(param2, MAX_STRING_LEN, "%s%c%s_%d", fcgiTmpDir, FILE_PATHSEP, appName, getppid());
#else
        //on windows, bindpath will be "BIND_PATH_PREFIX<appname>"
        PR_snprintf(param2, MAX_STRING_LEN, "%s%s", BIND_PATH_PREFIX, appName);
#endif //XP_UNIX
        PL_strfree(appName);
        bindPathSpecified = PR_FALSE;
    }

    match = strmatch(procInfo.execPath, param1);
    if (match == PR_TRUE) {
        if(udsName && bindPathSpecified) {
            match = strmatch((procInfo.bindPath + strlen(fcgiTmpDir) + 1), param2);
        } else {
           strmatch(procInfo.bindPath, param2);
        }
    }

    return match;
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::operator!=
//-----------------------------------------------------------------------------
int FcgiServerConfig::operator!=(const FcgiServerConfig& right) const {
    return !operator==(right);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::operator!=
//-----------------------------------------------------------------------------
int FcgiServerConfig::operator!=(pblock *pb) const {
    return !operator==(pb);
}

//-----------------------------------------------------------------------------
// FcgiServerConfig::printProcInfo
//-----------------------------------------------------------------------------
void FcgiServerConfig::printProcInfo() {
    //debug log msgs
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "app-path = %s", ((procInfo.execPath) ? procInfo.execPath : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "argv0 = %s", ((procInfo.argv0) ? procInfo.argv0 : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "bind-path = %s", ((procInfo.bindPath) ? procInfo.bindPath : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "minProcesses = %s", procInfo.minProcesses);
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "maxProcesses = %s", procInfo.maxProcesses);
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "chRoot = %s", ((procInfo.chRoot) ? procInfo.chRoot : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "chDir = %s", ((procInfo.chDir) ? procInfo.chDir : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "userName = %s", (procInfo.userName ? procInfo.userName : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "groupName = %s", (procInfo.groupName ? procInfo.groupName : "NULL" ));
    log_error(LOG_VERBOSE, "FcgiServerConfig::init(pb)", NULL, NULL, "restartDelay = %s", (procInfo.restartDelay ? procInfo.restartDelay : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "priority = %s", (procInfo.priority ? procInfo.priority : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "listenQueueDepth = %s", procInfo.listenQueueDepth);
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "numFailures = %s", (procInfo.numFailures ? procInfo.numFailures :"NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "rlimit_as = %s", (procInfo.rlimit_as ? procInfo.rlimit_as:"NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "restartOnExit = %s", (procInfo.restartOnExit ? procInfo.restartOnExit : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "rlimit_core  = %s", (procInfo.rlimit_core ? procInfo.rlimit_core : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "rlimit_as  = %s", (procInfo.rlimit_as ? procInfo.rlimit_as : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "rlimit_cpu  = %s", (procInfo.rlimit_cpu ? procInfo.rlimit_cpu : "NULL"));
    log_error(LOG_VERBOSE, "config::init(pb)", NULL, NULL, "rlimit_nofile  = %s", (procInfo.rlimit_nofile ? procInfo.rlimit_nofile : "NULL"));
}
