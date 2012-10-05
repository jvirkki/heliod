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

#ifdef XP_UNIX
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#else
#include <process.h>
#endif //XP_UNIX
#include "base/vs.h"
#include "base/daemon.h"
#include "base/sem.h"
#include "frame/conf.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/func.h"
#include "httpdaemon/configuration.h"
#include "fastcgi.h"
#include "fastcgii18n.h"
#include "serverchannel.h"
#include "fcgiparser.h"
#include "server.h"
#include "fcgirole.h"
#include "servermanager.h"
#include "stubexec.h"
#include "constants.h"
#include "nsapifastcgi.h"

//-----------------------------------------------------------------------------
//Constants
//-----------------------------------------------------------------------------
#define PLUGIN_FULL_NAME "FastCGI NSAPI Plugin"

//-----------------------------------------------------------------------------
// SAF names
//-----------------------------------------------------------------------------
#define INIT_FASTCGI "init-fastcgi"
#define AUTH_FASTCGI "auth-fastcgi"
#define RESPONDER_FASTCGI "responder-fastcgi"
#define FILTER_FASTCGI "filter-fastcgi"
#define ERROR_FASTCGI "error-fastcgi"

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
extern "C" {
    Func NSAPI_PUBLIC nsapi_module_init;
    Func NSAPI_PUBLIC auth_fastcgi;
    Func NSAPI_PUBLIC responder_fastcgi;
    Func NSAPI_PUBLIC filter_fastcgi;
    Func NSAPI_PUBLIC error_fastcgi;
    VSInitFunc vsinit_fastcgi;
    VSDestroyFunc vsdestroy_fastcgi;
    VSDirectiveInitFunc directiveinit_fastcgi;
}

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
static int slotServerManager = -1;
static char *pluginDir = NULL;
static const char *systemTmpDir = NULL;
static char *stubLogDir = NULL;
PRBool isLateInit = PR_FALSE;

//-----------------------------------------------------------------------------
//utility function to remove the param from pblock
//-----------------------------------------------------------------------------
void removeParamIfExists(const char *paramName, pblock *pb) {
    char *p = pblock_findval(paramName, pb);
    if(p) {
        pb_param *pparm = pblock_remove(paramName, pb);
        param_free(pparm);
    }
}

//-----------------------------------------------------------------------------
// vsdestroy_fastcgi
//-----------------------------------------------------------------------------
extern "C"
void vsdestroy_fastcgi(VirtualServer *outgoing) {
    // Destroy this VS's server manager
    FcgiServerManager *mgr = (FcgiServerManager *)vs_get_data(outgoing, slotServerManager);
    if(mgr)
        delete mgr;
}

//-----------------------------------------------------------------------------
// vsinit_fastcgi
//-----------------------------------------------------------------------------
extern "C"
int vsinit_fastcgi(VirtualServer *incoming, const VirtualServer *current) {
    // Create a ConnectionPool for this VS
    FcgiServerManager *mgr = new FcgiServerManager();
    vs_set_data(incoming, &slotServerManager, mgr);

    const Configuration *conf = vs_get_conf(incoming);
    if((conf->platform) == conf->platform.PLATFORM_64) {
        platform64 = PR_TRUE; 
    } else {
        platform64 = PR_FALSE; 
    }

    return REQ_PROCEED;
}

NSString createStubPidFileName() {
    NSString pidFileName;
    int len = 0;
    char *serverId = conf_getglobals()->Vserver_id;
    const char *tmpDir = system_get_temp_dir();
#ifdef XP_UNIX
    pidFileName.append(tmpDir);
    pidFileName.append("/"STUB_PID_FILE_NAME);
#else
    if(!tmpDir || (strlen(tmpDir) < 1)) {
        tmpDir = "C:\\TEMP";
    }
    pidFileName.append(tmpDir);
    pidFileName.append("\\");
    pidFileName.append(serverId);
    pidFileName.append("-"STUB_PID_FILE_NAME);
#endif //XP_UNIX
    return pidFileName;
}

char *createSemName() {
    char *serverId = conf_getglobals()->Vserver_id;
    int len = strlen(STUB_SEMAPHORE) + strlen(serverId) + 2;
    char *semaName = (char *)MALLOC(sizeof(char) * len);
    if(semaName) {
        PR_snprintf(semaName, len, "%s-%s", STUB_SEMAPHORE, serverId);
    }
    return semaName;
}

void terminateStub() {
    PidType stubPid = 0;

    //read the stub pid file
    NSString stubPidFileName = createStubPidFileName();
    log_error(LOG_VERBOSE, "clean", NULL, NULL, "Fastcgistub pid file name = %s", stubPidFileName.data());
    PRFileDesc *stubPidFd = PR_Open(stubPidFileName, PR_RDONLY, 0644);

    if(stubPidFd) {
        char pidStr[32] = "";
        int size = PR_Read(stubPidFd, pidStr, sizeof(pidStr) - 1);
        if (size > 0) {
            pidStr[size] = '\0';
            stubPid = atoi(pidStr);
        }
        //close the file
        PR_Close(stubPidFd);
        stubPidFd = NULL;
    }
    if(stubPid > 0) {
        log_error(LOG_VERBOSE, "clean", NULL, NULL, "terminating Fastcgistub process %d", stubPid);
#ifdef XP_WIN32
        // send  stub stop event
        HANDLE stubEventHandle = OpenEvent(EVENT_ALL_ACCESS, FALSE, "StubStopEvent");
        if(stubEventHandle != NULL)
        SetEvent(stubEventHandle);
        // after sending the stop event wait for some time;
        PR_Sleep(PR_SecondsToInterval(1));
        // check the status of the stub process
        HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, stubPid);
        DWORD result = WaitForSingleObject(hProc, 0);
        if(result != WAIT_OBJECT_0) {
            // if the process has not responded to the event,
            // kill the process
            log_error(LOG_VERBOSE, "clean", NULL, NULL, "failure of graceful termination of Fastcgistub process %d - killing it", stubPid);
            TerminateProcess(hProc, 0);
        }
#else
        kill(stubPid, SIGTERM);
        // wait till the process goes away or timeout after 10 secs 
        int wait_count = 0;
        int stub_terminated = 0;
        pid_t t = -1;
        do {
            t = waitpid(stubPid, &stub_terminated, WNOHANG);
            if (t != stubPid) {
                PR_Sleep(PR_SecondsToInterval(1));
                wait_count++;
            }
        } while ((t != stubPid) && (wait_count < 10)); 
        log_error(LOG_VERBOSE, "clean", NULL, NULL, "waited for approximately %d secs for the Fastcgistub process to terminate", wait_count);
#endif //XP_WIN32
    }
    if(stubPidFileName) {
        PR_Delete(stubPidFileName);
    }
}

//-----------------------------------------------------------------------------
// clean_fastcgi
//-----------------------------------------------------------------------------
extern "C"
void clean_fastcgi(void *data) {

    //terminate Fastcgistub process
    terminateStub();

    //delete the semaphore used for locking the stub pid file
    if(stubSem) {
        Sem_terminate(stubSem);
    }

    if(pluginDir) {
        PR_Free(pluginDir);
        pluginDir = NULL;
    }
    if(stubLogDir) {
        free(stubLogDir);
        stubLogDir = NULL;
    }
    systemTmpDir = NULL;
}

//-----------------------------------------------------------------------------
// init_fastcgi
//-----------------------------------------------------------------------------
extern "C"
int NSAPI_PUBLIC init_fastcgi(pblock *pb, Session *sn, Request *rq) {
    // If this isn't LateInit time...
    if (!conf_is_late_init(pb)) {
        //create the semaphore to lock the stub pid file
        char *semName = createSemName();
        stubSem = Sem_init(semName, 1);
        FREE(semName);

        // Terminate the stale Fastcgistub process
        terminateStub();

        // We want to run during LateInit (after the fork)
        pblock_nvinsert("LateInit", "yes", pb);
        return REQ_PROCEED;
    }

    const Configuration *conf = vs_get_conf(request_get_vs(rq));
    stubLogDir = strdup(conf->log.logFile.getStringValue());
    char *stubLogDirEnd = (char *)strrchr(stubLogDir, FILE_PATHSEP);
    *stubLogDirEnd = '\0';
    ereport(LOG_VERBOSE, "FastCGI plugin's log directory is %s", stubLogDir);

    isLateInit = PR_TRUE;
    systemTmpDir = system_get_temp_dir();
#ifdef XP_WIN32
    if(!systemTmpDir || (strlen(systemTmpDir) < 1)) {
        systemTmpDir = "C:\\TEMP";
    }
#endif

    /* register function to be called at shutdown/restart */
    daemon_atrestart(clean_fastcgi, NULL);

    // We can only be initialized once
    if (slotServerManager != -1) {
        ereport(LOG_MISCONFIG, GetString(DBT_init_fastcgi_ignored));
        return REQ_PROCEED;
    }

    slotServerManager = vs_alloc_slot();

    // Log build info
    ereport(LOG_INFORM, GetString(DBT_build_info), PRODUCT_ID" "PRODUCT_FULL_VERSION_ID" "PLUGIN_FULL_NAME" B"BUILD_NUM);

    if(systemTmpDir)
        ereport(LOG_VERBOSE, "FastCGI plugin's temp directory is %s", systemTmpDir);

    // Register our VS initialization/destruction callback functions
    vs_register_cb(&vsinit_fastcgi, &vsdestroy_fastcgi);

    return REQ_PROCEED;
}


//-----------------------------------------------------------------------------
// nsapi_module_init
//-----------------------------------------------------------------------------
extern "C"
int NSAPI_PUBLIC nsapi_module_init(pblock *pb, Session *sn, Request *rq) {
    int rv;

    //Get the user specified fastcgi stub path info
    char *p = pblock_findval("fastcgistub-path", pb);

    if(p) {
        pluginDir = (char *)PR_Malloc(strlen(p)); 
        strcpy(pluginDir, p); 
    } else {
        // if user has not specified any path,
        // deduce the stub path from fastcgi library path.
        p = pblock_findval("shlib", pb);
        if(p) {
            char *a = PL_strdup(p);
            char *b = PL_strrchr(a, FILE_PATHSEP);
            if(b) {
                *b = '\0';
                //stubexec path : <pluginDir>/Fastcgistub
                int len = strlen(a) + strlen(FCGI_STUB_NAME) + 2; //2 = file path sep + '\0'
                pluginDir = (char *)PR_Malloc(len);
                PR_snprintf(pluginDir, len, "%s%c%s", a, FILE_PATHSEP, FCGI_STUB_NAME);
            }
            PL_strfree(a); 
        }
    }

    rv = init_fastcgi(pb, sn, rq);
    if (rv == REQ_PROCEED) {
        //insert SAFs to function list
        func_insert((char *)AUTH_FASTCGI, auth_fastcgi);
        func_insert((char *)RESPONDER_FASTCGI, responder_fastcgi);
        func_insert((char *)FILTER_FASTCGI, filter_fastcgi);
        func_insert((char *)ERROR_FASTCGI, error_fastcgi);
    }

    return rv;
}


//-----------------------------------------------------------------------------
//auth_fastcgi
//-----------------------------------------------------------------------------
NSAPI_PUBLIC int auth_fastcgi(pblock *pb, Session *sn, Request *rq) {
    FcgiServer *server = NULL;
    strcpy(LOG_FUNCTION_NAME, AUTH_FASTCGI);

    // Get server manager which pools the fastcgi servers
    FcgiServerManager *manager = (FcgiServerManager *)vs_get_data(request_get_vs(rq), slotServerManager);
    if (!manager) {
        log_error(LOG_MISCONFIG, AUTH_FASTCGI, sn, rq, GetString(DBT_init_fastcgi_not_called));
        return REQ_NOACTION;
    }

    //get the vs id
    const char *vsId = vs_get_id(request_get_vs(rq));

    // get the fastcgi server information corresponding to current request
    // if server not found, it creats one
    server = manager->getFcgiServer(pb, vsId, systemTmpDir, pluginDir, stubLogDir);

    //server will be null if configuration is invalid
    if (!server) {
        const char *errorReason = getErrorString(REQUEST_MISSING_OR_INVALID_PARAM);
        log_error(LOG_MISCONFIG, AUTH_FASTCGI, sn, rq, errorReason);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, errorReason);
        return REQ_ABORTED;
    }


    // Inspect the incoming request and prepare to build the outgoing request
    FcgiRequest request(server->config, sn, rq, FCGI_AUTHORIZER);
    //Create authorizer object
    AuthRole *role = new AuthRole(server, request);

    //process the request
    if(role->BaseRole::process() != PR_SUCCESS) {
        //error in processing the request
        PluginError err = role->getLastError();
        const char *errorReason = NULL;
        if(err != NO_FCGI_ERROR) {
            errorReason = getErrorString(err);
        } 
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, errorReason);
        delete role;
        return REQ_ABORTED;
    } else if(role->getLastError() == FCGI_NO_AUTHORIZATION) {
        // authorization failed
        delete role;
        return REQ_ABORTED;
    }

    delete role;
    return REQ_PROCEED;
}


//-----------------------------------------------------------------------------
//responder_fastcgi : Service SAF for responder role
//-----------------------------------------------------------------------------
extern "C"
int NSAPI_PUBLIC responder_fastcgi(pblock *pb, Session *sn, Request *rq) {
    FcgiServer *server = NULL;
    strcpy(LOG_FUNCTION_NAME, RESPONDER_FASTCGI);

    // Get server manager which pools the fastcgi servers
    FcgiServerManager *manager = (FcgiServerManager *)vs_get_data(request_get_vs(rq), slotServerManager);
    if (!manager) {
        log_error(LOG_MISCONFIG, RESPONDER_FASTCGI, sn, rq, GetString(DBT_init_fastcgi_not_called));
        return REQ_NOACTION;
    }

    // get the vs id for this request
    const char *vsId = vs_get_id(request_get_vs(rq));
    // get the fastcgi server information corresponding to current request
    // if server not found, it creats one
    server = manager->getFcgiServer(pb, vsId, systemTmpDir, pluginDir, stubLogDir);

    // server will be null if configuration is invalid
    if (!server) {
        const char *errorReason = getErrorString(REQUEST_MISSING_OR_INVALID_PARAM);
        log_error(LOG_MISCONFIG, RESPONDER_FASTCGI, sn, rq, errorReason);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, errorReason);
        return REQ_ABORTED;
    }

    // Inspect the incoming request and prepare to build the outgoing request
    FcgiRequest request(server->config, sn, rq, FCGI_RESPONDER);
    ResponderRole *role = new ResponderRole(server, request);

    // process the request
    if(role->BaseRole::process() != PR_SUCCESS) {
        // error in processing the request
        PluginError err = role->getLastError();
        const char *errorReason = NULL;
        if(err != NO_FCGI_ERROR) {
            errorReason = getErrorString(err);
        } 
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, errorReason);
        delete role;
        return REQ_ABORTED;
    }
    delete role;
    return REQ_PROCEED;
}


//-----------------------------------------------------------------------------
//filter_fastcgi : Service SAF for filter role
//-----------------------------------------------------------------------------
NSAPI_PUBLIC int filter_fastcgi(pblock *pb, Session *sn, Request *rq) {
    FcgiServer *server = NULL;
    strcpy(LOG_FUNCTION_NAME, FILTER_FASTCGI);

    // Get server manager which pools the fastcgi servers
    FcgiServerManager *manager = (FcgiServerManager *)vs_get_data(request_get_vs(rq), slotServerManager);
    if (!manager) {
        log_error(LOG_MISCONFIG, FILTER_FASTCGI, sn, rq, GetString(DBT_init_fastcgi_not_called));
        return REQ_NOACTION;
    }

    // get the vs id for this request
    const char *vsId = vs_get_id(request_get_vs(rq));

    // get the fastcgi server information corresponding to current request
    //if server not found, it creats one
    server = manager->getFcgiServer(pb, vsId, systemTmpDir, pluginDir, stubLogDir);

    // server will be null if configuration is invalid
    if (!server) {
        const char *errorReason = getErrorString(REQUEST_MISSING_OR_INVALID_PARAM);
        log_error(LOG_MISCONFIG, FILTER_FASTCGI, sn, rq, errorReason);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, errorReason);
        return REQ_ABORTED;
    }

    // Inspect the incoming request and prepare to build the outgoing request
    FcgiRequest request(server->config, sn, rq, FCGI_FILTER);
    FilterRole *role = new FilterRole(server, request);

    // process the request
    if(role->BaseRole::process() != PR_SUCCESS) {
        // error in processing the request
        PluginError err = role->getLastError();
        const char *errorReason = NULL;
        int errorCode = PROTOCOL_SERVER_ERROR;
        if(err != NO_FCGI_ERROR) {
            if(err == FCGI_FILTER_FILE_OPEN_ERROR)
                errorCode = PROTOCOL_NOT_FOUND;
            else {
                errorReason = getErrorString(err);
            } 
        } 
        protocol_status(sn, rq, errorCode, errorReason);
        delete role;
        return REQ_ABORTED;
    }
    delete role;
    return REQ_PROCEED;

}

//-----------------------------------------------------------------------------
//error_fastcgi
//-----------------------------------------------------------------------------
NSAPI_PUBLIC int error_fastcgi(pblock *pb, Session *sn, Request *rq) {
    char *errorUrl;
    // get the "error-reason" string
    char *reasonString = pblock_findval("error-reason", pb);
    //get the acutal reason for which the error was thrown
    char *actualReason = pblock_findval("status", rq->srvhdrs);

    //check if this SAF should handle the error
    //if reason string is specified, match with the actual reason string
    if(reasonString && actualReason && (strstr(actualReason, reasonString) == NULL)) {
        //error should not be handled by this SAF
        return REQ_NOACTION;
    }

    //copy the pblock content as it will be modified
    pblock *pbCopy = pblock_dup(pb);

    // remove the existing "reson" string and set it to "error-reason" string
    removeParamIfExists("reason", pbCopy);

    if(reasonString) {
        pblock_nvinsert("reason", reasonString, pbCopy);
        //now remove "error-reason" string
        removeParamIfExists("error-reason", pbCopy);
    }

    // check if "error-url" is specified
    // if not, it is an error !
    errorUrl = pblock_findval("error-url", pbCopy);
    if(!errorUrl) {
        log_error(LOG_MISCONFIG, ERROR_FASTCGI, sn, rq, GetString(DBT_errorurl_not_specified));
        pblock_free(pbCopy);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }

    if((errorUrl[0] == 'h' || errorUrl[0] == 'H') &&
       ((!PL_strncasecmp(errorUrl, HTTP_PROTOCOL, PL_strlen(HTTP_PROTOCOL))) ||
       (!PL_strncasecmp(errorUrl, HTTPS_PROTOCOL, PL_strlen(HTTPS_PROTOCOL))))) {
        // if error-url is absolute url, set is as Location header and
        // invoke "set-variable" SAF.
        removeParamIfExists("stop", pbCopy);
        pblock_nvinsert("stop", "", pbCopy);
        removeParamIfExists("error", pbCopy);
        pblock_nvinsert("error", "302", pbCopy);
        removeParamIfExists("fn", pbCopy);
        pblock_nvinsert("fn", "set-variable", pbCopy);
        removeParamIfExists("set-srvhdrs", pbCopy);
        int len = strlen(errorUrl) + strlen("Location: ") + 1;
        char *paramVal = (char *)MALLOC(len);
        PR_snprintf(paramVal, len, "Location: %s", errorUrl);
        pblock_nvinsert("set-srvhdrs", paramVal, pbCopy);
        FREE(paramVal);
    } else {
        // if "error-url" is a relative path, then
        // set it as "uri", "path" and invoke "send-error" SAF.
        removeParamIfExists("uri", pbCopy);
        pblock_nvinsert("uri", errorUrl, pbCopy);
        removeParamIfExists("path", pbCopy);
        pblock_nvinsert("path", errorUrl, pbCopy);
        pblock_nvinsert("fn", "send-error", pbCopy);
    }

    //remove "error-url" entry
    pb_param *urlParam = pblock_remove("error-url", pbCopy);
    param_free(urlParam);
    if(func_exec(pbCopy, sn, rq) != REQ_PROCEED) {
        pblock_free(pbCopy);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }
    pblock_free(pbCopy);
    return REQ_PROCEED;
}
