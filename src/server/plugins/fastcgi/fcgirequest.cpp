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

#include <plstr.h>
#include "base/util.h"
#include "frame/conf.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/httpact.h"
#include "safs/cgi.h"
#include "fastcgi.h"
#include "frame/http.h"
#include "serverconfig.h"
#include "util.h"
#include "fcgirequest.h"
#ifdef SOLARIS
#include <sys/time.h>
#endif


extern char LOG_FUNCTION_NAME[];

//constructor
FcgiRequest::FcgiRequest(const FcgiServerConfig *config, Session *sn, Request *rq, PRUint8 role)
: _config(config), _sn(sn), _rq(rq), fcgiRole(role), filterApp(NULL) {
    _headers = pblock_dup(_rq->headers);
    _hasContentLength = (pblock_find("content-length", _headers) != NULL);
    _hasBody = _hasContentLength || pblock_find("transfer-encoding", _headers);
    _isHead = ISMHEAD(_rq);
    if(_hasContentLength)
        contentLength = atoi(pblock_findval(const_cast<char *> ("content-length"), _headers));
    else
        contentLength = 0;
#ifdef SOLARIS
    reqId = ((gethrtime() & 0x000000000000ffff) | 0xf);
#else
    PseudoRandomGenerator r;
    do {
       reqId = r.getNumber();
    } while(reqId == 0);
#endif //SOLARIS
}

FcgiRequest::~FcgiRequest() {
    pblock_free(_headers);
    FREE(filterApp);
}

void FcgiRequest::log(int degree, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_error_v(degree, LOG_FUNCTION_NAME, _sn, _rq, fmt, args);
    va_end(args);
}


char **FcgiRequest::getEnvironment() {
    char ** env;
    char *args;
    char *auth;
    char *user;
    char *pw;

    // The query string is converted to command line arguments for some CGI
    // programs
    args = pblock_findval("query", _rq->reqpb);

    // Setup the environment for the FCGI program
    env = http_hdrs2env(_rq->headers);
    env = cgi_common_vars(_sn, _rq, env);
    env = cgi_specific_vars(_sn, _rq, args, env, (fcgiRole != FCGI_AUTHORIZER));
    env = cgi_ssi_vars(args, env);
    env = fcgiSpecificVars(env);

    //if remote app, send the application envs via FCGI_PARAMS records
    // else it will be passed to the process at ts creation time
    if(_config->remoteServer && _config->procInfo.env)
        env = getRemoteAppEnv(env);

    return env;
}

char **FcgiRequest::getRemoteAppEnv(char **env) {
    int x;
    
    //find out the total number of application env vars 
    int numEnv = 0;
    for(;_config->procInfo.env[numEnv] != NULL; numEnv++);

    //if no app envs specified, return the existing env
    if(numEnv <= 0)
        return env;

    //create additional space for adding application envs
    env = util_env_create(env, numEnv, &x);
    for(int i=0; i<numEnv; i++) {
        env[x++] = fcgi_util_arg_str(_config->procInfo.env[i]);
    }
    env[x] = NULL;
    return env;
}

char** FcgiRequest::cgi_ssi_vars(char *args, char** env) {
    pblock *param = ((NSAPIRequest *) _rq)->param;

    if(param) {
        int hi;

        int nparam = 0;
        for(hi = 0; hi < param->hsize; hi++) {
            for(pb_entry *pe = param->ht[hi]; pe; pe = pe->next)
                nparam++;
        }

        int x;
        env = util_env_create(env, nparam, &x);

        for(hi = 0; hi < param->hsize; hi++) {
            for(pb_entry *pe = param->ht[hi]; pe; pe = pe->next)
                env[x++] = util_env_str(pe->param->name, pe->param->value);
        }

        env[x] = NULL;
    }

    return env;
}

char **FcgiRequest::fcgiSpecificVars(char **env) {
    int x;
    char *path, *auth;

    env = util_env_create(env, MAX_EXTRA_FCGI_VARS, &x);

    if (fcgiRole != FCGI_AUTHORIZER) {
        // set PATH_TRANSLATED if not set by cgi_specific_vars
        if (!(path = pblock_findval("path-translated", _rq->vars)) &&
             (path = pblock_findval("path", _rq->vars))) {
            env[x++] = util_env_str(const_cast<char *> ("PATH_TRANSLATED"),
                                    path);
        }
        // set the CONTENT_LENGTH 
        // We are setting content-length here since it is not supposed to
        // be set in the cgi_specific_vars
        char *t = pblock_findval("content-length", _rq->srvhdrs);
        if (t) {
           int cl = atoi(t);
           if (cl > 0)
               env[x++] = util_env_str(const_cast<char *> ("CONTENT_LENGTH"), t);
        }
    }

    // set other fastcgi specific variables
    env[x++] = util_env_str(const_cast<char *>("SERVER_HOSTNAME"), conf_getglobals()->Vserver_hostname);
    env[x++] = util_env_str(const_cast<char *>("SERVER_ADDR"), conf_getglobals()->Vaddr);

    request_header((char *)"authorization", &auth, _sn, _rq);
    if(auth) {
        env[x++] = util_env_str((char *)"HTTP_AUTHORIZATION", auth);
    }

    if (fcgiRole == FCGI_FILTER) {
        filterApp = STRDUP(path);
        // set the filter file
        PRStatus status = PR_GetFileInfo(filterApp, &filterFileInfo);
        if(filterFileInfo.type != PR_FILE_FILE)
            log(LOG_FAILURE, "send-request", "Filter application is not a file (%s)",
                                    path);
        else {
            PRTime lastMod = filterFileInfo.modifyTime;
            PRUint32 dataSize = filterFileInfo.size;
            char buf[64];
            PR_snprintf(buf, 64, "%u", dataSize);
            env[x++] = util_env_str(const_cast<char *>("FCGI_DATA_LENGTH"), STRDUP(buf));

            //calculate last modification time
            //convert microseconds to seconds
            PRTime lastModTimeInSec = 0;
            LL_DIV(lastModTimeInSec, lastMod, PR_USEC_PER_SEC);
            ZERO(buf, 64);

            PR_snprintf(buf, 64, "%u", lastModTimeInSec);
            env[x++] = util_env_str(const_cast<char *>("FCGI_DATA_LAST_MOD"), buf);
        }
    }

    env[x] = NULL;

    return env;
}
