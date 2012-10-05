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

#include "netsite.h"
#include "base/SemPool.h"
#include "base/ereport.h"
#include "base/language.h"
#include "base/vs.h"
#include "frame/conf_api.h"
#include "frame/conf_init.h"
#include "frame/req.h"
#include "frame/httpdir.h"
#include "support/stringvalue.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/WebServer.h"
#include "base/daemon.h"
#include "NSJavaUtil.h"
#include "JVMControl.h"
#include "NSAPIRunner.h"
#include "NSAPIConnector.h"

#define NSAPIWEBMODULE  "com/sun/webserver/connector/nsapi/WebModule"

#ifdef XP_WIN32
#include <process.h>
#define getpid() _getpid()
#endif

// --------------------------------------------------------------
// XXX <cleanup>
// --------------------------------------------------------------

#if !defined (XP_PC)

#include <signal.h>
#include <ctype.h>

static  int def_signals [] = { SIGABRT };
static  struct  {
            const char* signam;
            int signum;
        }
        sig_table[] = {
            { "SIGABRT", SIGABRT },
            { "SIGBUS" , SIGBUS  },
            { "SIGSEGV", SIGSEGV },
            { "SIGUSR1", SIGUSR1 },
            { "SIGUSR2", SIGUSR2 },
            { "SIGQUIT", SIGQUIT },
            { "SIGFPE" , SIGFPE  },
            { "SIGTERM", SIGTERM },
            { "SIGILL" , SIGILL  },
            { NULL     , 0  }
        };

#define sig_delim "()|:,"
#define MAX_SIGLIST 50

#endif

static const char MAGNUS_INTERNAL_J2EE[] = "magnus-internal/j2ee";
static const int MAGNUS_INTERNAL_J2EE_LEN = sizeof(MAGNUS_INTERNAL_J2EE) - 1;
static const char* const JSESSIONID = ";jsessionid=";

// i18n resource bundle defination
UResourceBundle* j2eeResourceBundle = NULL;

// slot_number is used in vs_set_data and vs_get_data
// to store and retrieve per-VS J2EEVirtualServer objects
static int slot_number = -1;

// Maintains the state of whether to start the thread that simulates
// a NSAPI onReady callback
static PRBool isready_thread_started = PR_FALSE;

static jclass _webmoduleClass = 0;
static jmethodID _webModuleIsWARDir;

#define _J2EE_FUNC_ extern "C" _J2EE_PLUGIN_DLL_

_J2EE_FUNC_ int preinit_j2ee(pblock* pb, Session* sn, Request* rq);
_J2EE_FUNC_ int init_j2ee(pblock* pb, Session* sn, Request* rq);
_J2EE_FUNC_ int faster_j2ee(pblock* pb, Session* sn, Request* rq);
_J2EE_FUNC_ int ntrans_j2ee(pblock* pb, Session* sn, Request* rq);
_J2EE_FUNC_ int type_j2ee(pblock* pb, Session* sn, Request* rq);
_J2EE_FUNC_ int find_index_j2ee(pblock* pb, Session* sn, Request* rq);
_J2EE_FUNC_ int service_j2ee(pblock* pb, Session* sn, Request* rq);
_J2EE_FUNC_ int error_j2ee(pblock* pb, Session* sn, Request* rq);

_J2EE_FUNC_ int j2ee_vs_create(VirtualServer* incoming,
                               const VirtualServer* outgoing);
_J2EE_FUNC_ void j2ee_vs_destroy(VirtualServer* outgoing);

_J2EE_FUNC_ int j2ee_conf_preinit(Configuration* incoming,
                                  const Configuration* outgoing);
_J2EE_FUNC_ int j2ee_conf_postinit(Configuration* incoming,
                                   const Configuration* outgoing);

_J2EE_FUNC_ void j2ee_conf_predestroy(Configuration* outgoing);
_J2EE_FUNC_ void j2ee_conf_postdestroy(Configuration* outgoing);
_J2EE_FUNC_ void j2ee_onready(void);

_J2EE_FUNC_ void shutdown_j2ee(void* data);
_J2EE_FUNC_ void isready_thread(void* data);

static void stripPathSegmentParams(char* param);
static char* stripJSessionId(char* path);

/**
 * NSAPI plugin for the J2EE engine
 *
 *
 * magnus.conf:
 *
 *    Init fn="load-modules" shlib=/path/j2ee.so
 *
 * obj.conf:
 *
 *    <Object name="default">
 *    NameTrans fn="ntrans-j2ee" name="j2ee"
 *    ObjectType fn="type-j2ee"
 *    </Object>
 *
 *    <Object name="j2ee">
 *    Service fn="service-j2ee" method="*"
 *    </Object>
 */

_J2EE_FUNC_
int nsapi_module_init(pblock* pb, Session* sn, Request* rq)
{
    if (conf_getboolean("MakeServerRunFaster", PR_FALSE)) {
        ereport(LOG_MISCONFIG, "Java is incompatible with MakeServerRunFaster.  Java support will be disabled.");
        func_insert("ntrans-j2ee", &faster_j2ee);
        func_insert("type-j2ee", &faster_j2ee);
        func_insert("find-index-j2ee", &faster_j2ee);
        func_insert("service-j2ee", &faster_j2ee);
        func_insert("error-j2ee", &faster_j2ee);
        return REQ_PROCEED;
    }

    // do EarlyInit stuff
    static PRBool nsapi_module_init_early = PR_FALSE;
    if (!nsapi_module_init_early) {
        int rv = preinit_j2ee(pb, sn, rq);
        if (rv != REQ_PROCEED)
            return rv;

        func_insert("ntrans-j2ee", &ntrans_j2ee);
        func_insert("type-j2ee", &type_j2ee);
        func_insert("find-index-j2ee", &find_index_j2ee);
        func_insert("service-j2ee", &service_j2ee);
        func_insert("error-j2ee", &error_j2ee);

        nsapi_module_init_early = PR_TRUE;
    }

    // we need to be called during LateInit
    if (!conf_is_late_init(pb)) {
        pblock_nvinsert("LateInit", "yes", pb);
        return REQ_PROCEED;
    }

    // do LateInit stuff
    return init_j2ee(pb, sn, rq);
}

_J2EE_FUNC_
int preinit_j2ee(pblock* pb, Session* sn, Request* rq)
{
    SemPool::init ();

    // initialize I18N resource bundle for localized log messages
    char* resourcefile;
    char *sroot = conf_getglobals()->Vnetsite_root;
    if (!sroot)
        log_error(LOG_WARN, (char*)"preinit-j2ee", sn, rq,
                  "Unable to find the server root to initialize ICU");

    resourcefile = get_resource_file(sroot, J2EE_PROPERTIES_FILE);
    j2eeResourceBundle = open_resource_bundle(resourcefile, XP_GetLanguage(XP_LANGUAGE_AUDIENCE_ADMIN));
    FREE(resourcefile);

    if (SemPool::add("SessionSem", getpid()) == SEM_ERROR)
    {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.j2ee.ERR_SEMCREATE");
        log_error(LOG_FAILURE, (char *)"preinit-j2ee", sn, rq, logMsg);
        FREE(logMsg);
        return REQ_ABORTED;
    }

    return REQ_PROCEED;
}

_J2EE_FUNC_
int init_j2ee(pblock* pb, Session* sn, Request* rq)
{
#if !defined (XP_PC)
    const char* catchSignals = pblock_findval("CatchSignals", pb);

    if (StringValue::getBoolean(catchSignals)) {
        char* arg = pblock_findval("Signals", pb);
        int siglen = 0;
        int sigarr[MAX_SIGLIST];

        if (arg != NULL) {

            char* cp = strtok(arg, sig_delim);

            if (cp != NULL) {
                do {
                    if (siglen == MAX_SIGLIST)
                        break;

                    if (isdigit(*cp))
                        sigarr[siglen++] = atoi(cp);
                    else {
                        for (int i = 0; sig_table[i].signam != NULL; i++) {
                            if (!PL_strcasecmp(cp, sig_table[i].signam)) {
                                sigarr[siglen++] = sig_table[i].signum;
                                break;
                            }
                        }
                    }
                }
                while ((cp = strtok(NULL, sig_delim)) != NULL);
            }
        }

        if (siglen > 0)
            JVMControl::setCatchSignals(PR_FALSE, sigarr, siglen);
        else
            JVMControl::setCatchSignals(PR_FALSE, def_signals, sizeof(def_signals) / sizeof(int));
    }
    else
        JVMControl::setCatchSignals(PR_TRUE , NULL, 0);
#endif

    NSAPIRunner runner;

    PRStatus status = NSAPIRunner::init();
    if (status == PR_SUCCESS)
        status = NSAPIConnector::init(pb);

    // try to run NameTrans and Error functions on a fiber
    func_set_native_thread_flag((char*)"ntrans-j2ee", 0);
    func_set_native_thread_flag((char*)"error-j2ee", 0);

    if (status == PR_SUCCESS) {
        // register callbacks that will be invoked at the start of a
        // conf init or at the end of a conf destroy
        conf_register_cb(j2ee_conf_preinit, j2ee_conf_postdestroy);

        // register our virtual server callbacks
        vs_register_cb(j2ee_vs_create, j2ee_vs_destroy);

        // register callbacks that will be invoked at the end of a
        // conf init or at the beginning of a conf destroy
        conf_register_cb(j2ee_conf_postinit, j2ee_conf_predestroy);

        // this allocates a new slot for storing a pointer to data specific
        // to a VirtualServer*. The returned slot number is valid for any
        // VirtualServer*.
        slot_number = vs_alloc_slot();

        // set up for graceful shutdown of servlets
        if (conf_getboolean("DestroyServletsOnShutdown", PR_TRUE)) {
            // run it by default!
            daemon_atrestart(shutdown_j2ee, NULL);
        }

        JvmCall jvm_call;
        JNIEnv* env = jvm_call.getEnv();
        if (env == NULL) {
            return REQ_ABORTED;
        }
    
        _webmoduleClass = NSJavaUtil::findClassGlobal(env, NSAPIWEBMODULE);
        if (_webmoduleClass == NULL) {
            char* logMsg = get_message(j2eeResourceBundle,
                                    "j2ee.ERR_CLASS_LOOKUP");
            ereport(LOG_FAILURE, logMsg, NSAPIWEBMODULE);
            FREE(logMsg);
            return REQ_ABORTED;
        }
    
        _webModuleIsWARDir = env->GetMethodID(_webmoduleClass, 
                                            "isWARDirectory",
                                            "(Ljava/lang/String;)Z");
        if (_webModuleIsWARDir == NULL) {
            char* logMsg = get_message(j2eeResourceBundle,
                                    "j2ee.ERR_METHODS_LOOKUP");
            ereport(LOG_FAILURE, logMsg);
            FREE(logMsg);
            return REQ_ABORTED;
        }
    
        return REQ_PROCEED;
    }
    return REQ_ABORTED;
}

/**
 *  Stub SAF called when MakeServerRunFaster is on
 */
_J2EE_FUNC_
int faster_j2ee(pblock* pb, Session* sn, Request* rq)
{
    return REQ_NOACTION;
}

/**
 *  NameTrans may be called a lot of times during the request, when people call
 *  something like url_translated
 */
_J2EE_FUNC_
int ntrans_j2ee(pblock* pb, Session* sn, Request* rq)
{
    char* name = pblock_findkeyval(pb_key_name, pb);

    if (name == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.j2ee.ERR_NAMETRANS");
        log_error(LOG_FAILURE, (char*)"ntrans-j2ee", sn, rq, logMsg);
        FREE(logMsg);
        return REQ_ABORTED;
    }

    const VirtualServer* vs = request_get_vs(rq);
    NSAPIVirtualServer* nsapiVS =
                      (NSAPIVirtualServer*)vs_get_data(vs, slot_number);

    // work on ppath instead of uri in reqpb since we dont want to be
    // modifying the uri
    char* ppath = pblock_findkeyval(pb_key_ppath, rq->vars);

    if (nsapiVS != NULL && ppath != NULL) {
        // temporarily remove any uri params like jsessionid so that the
        // url matching will work correctly
        char* uri = ppath;
        if (const char* semicolon = strchr(uri, ';')) {
            char* uriCopy = pool_strdup(sn->pool, uri);
            stripPathSegmentParams(uriCopy + (semicolon - uri));
            uri = uriCopy;
        }

        WebModule* wm = nsapiVS->matchWebModule(uri);
        if (wm) {
            // set the webModule ptr in the request so we have access
            // to the web module during further nsapi processing
            NSAPIRequest* nrq = (NSAPIRequest*)rq;
            nrq->webModule = wm;

            const char* contextPath = wm->getContextPath();
            const char* location = wm->getLocation();
            int contextPathLen = wm->getContextPathLen();

            if (!wm->isAvailable()) {
                // log the fact that there were errors during startup
                // of this web application and abort the request
                char* logMsg = get_message(j2eeResourceBundle,
                                           "j2ee.j2ee.ERR_CONTEXT_UNAVAILABLE");
                log_error(LOG_FAILURE, "ntrans-j2ee", sn, rq,
                          logMsg, contextPath);
                FREE(logMsg);
                return REQ_ABORTED;
            }

            // advance the uri past the context path
            uri += contextPathLen;
           
            // If the request is for sourceuri in DAV, then allow request
            // to the INF directories
            char* dsrc = pblock_findkeyval(pb_key_magnus_internal_dav_src,
                                           rq->vars);
                
            // return NOT_FOUND if request is for /WEB_INF or /META_INF
            // and dav_src is not set
            if (!dsrc && (strncasecmp(uri, "/WEB-INF", 8) == 0 || 
                          strncasecmp(uri, "/META-INF", 9) == 0)) {
                protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
                return REQ_ABORTED;
            }

            // does it match with a  java resource as well? If so, setup
            // the rq-vars to send the request to service_j2ee, else set
            // up the vars so it can be handled by the core web server

            ServletResource* servletResource = NULL;
            char* servletPath = NULL;
            char* pathInfo = NULL;

            NSAPIRequest* nsapiReq = (NSAPIRequest*)rq;

            if (*uri || wm->isWarFile()) {
                servletResource = wm->matchUrlPattern(uri, 
                                                      servletPath, 
                                                      pathInfo);
                // Assign the servlet object so that it can be used during 
                // service and/or type_j2ee
                if (servletResource)
                    nsapiReq->servletResource = (void*)servletResource;
            }
            // is this request the result of a J2EE-NSAPI include/forward?
            pb_param *j2eeNSAPI;
            j2eeNSAPI = pblock_removekey(pb_key_magnus_internal_j2ee_nsapi,
                                         rq->vars);
            if (j2eeNSAPI) {
                param_free(j2eeNSAPI);
#ifdef XP_WIN32
                // XXX need to ensure that the case of the URI matches the case
                // of the requested path; consider a security-constraint or
                // filter on /foo/* and a request for /fOo/index.html.
                //
                // This sort of check used to be done following PathCheck by
                // internalRedirect, but that was totally broken on 7.0 anyway.
                // 
                // XXX See CR 6324431 for more variations on this problem.
#endif
            }

            // permanently remove any ;jsessionid param but leave other params
            // intact
            uri = ppath + contextPathLen;
            stripJSessionId(uri);
            int uriLen = strlen(uri);

            // should the request be sent to Java?
            if (servletResource && !j2eeNSAPI) {
                // the uri maps to a j2ee resource, set "name" in rq-vars
                // so that further processing will go to the j2ee object
                pblock_kvinsert(pb_key_name, name, strlen(name), rq->vars);

                log_error(LOG_VERBOSE, (char*)"ntrans-j2ee", sn, rq,
                   (char*)"mapped uri \"%s\" in context \"%s\" to resource \"%s\"",
                   uri, contextPath, servletResource->getResourceName());

                // set servletPath and pathInfo so that they can be used
                // during service stage
                if (servletPath) {
                    pblock_kvreplace(pb_key_script_name, (char*)servletPath,
                                     strlen(servletPath), rq->vars);
                } else {
                    char* resName = (char*)servletResource->getResourceName();
                    pblock_kvreplace(pb_key_script_name, resName,
                                     servletResource->getResourceNameLen(),
                                     rq->vars);
                }

                if (pathInfo) {
                    pblock_kvreplace(pb_key_path_info, (char*)pathInfo,
                                     strlen(pathInfo), rq->vars);
                }

                // Since nvreplace makes a copy, free servletPath and pathInfo
                FREE(servletPath);
                FREE(pathInfo);

                // if this is a request to a java resource in the default
                // context, let the core web server handle the translation
                if (wm->isSystemDefault()) {
                    rq->directive_is_cacheable = 1;

                    return REQ_NOACTION;
                }

                int locationLen = wm->getLocationLen();
                int uriLen = strlen(uri);

                // set ntrans-base and nostat in request vars
                // to avoid stat calls in the PathCheck functions
                // Note that request uri should equal ntrans-base + nostat
                pblock_kvreplace(pb_key_ntrans_base, (char*)location,
                                 locationLen, rq->vars);

                // don't set nostat if the servlet resource is the JspServlet
                // and it matched a wildcard pattern. This enables welcome file 
                // processing to occur for such requests
                if (servletResource->hasContent() && !servletResource->isSpecial())
                    pblock_kvreplace(pb_key_nostat, uri, uriLen, rq->vars);

                // Get the filesystem path to the resource by appending the
                // partial uri to the context location
                char* filePath = (char*)pool_malloc(sn->pool,
                                                    locationLen + uriLen + 1);
                memcpy(filePath, location, locationLen);
                if (uriLen)
                    memcpy(filePath + locationLen, uri, uriLen);
                filePath[locationLen + uriLen] = '\0';

                // request is to a filesystem resource, set the filepath in
                // rq->vars so that further nsapi processing will get the 
                // path
                pblock_kvreplace(pb_key_ppath, filePath, locationLen + uriLen,
                                 rq->vars);
                pool_free(sn->pool, filePath);

            } else {
                // non-java resource, can be handled by the core web server

                // if this is a request to a non-java resource in the default
                // context, let the core web server handle it appropriately
                if (wm->isSystemDefault()) {
                    rq->directive_is_cacheable = 1;

                    return REQ_NOACTION;
                }

                // If there are no mapping, the request is probably to a
                // filesystem resource within the context-root. Get the
                // filesystem path to the resource by appending the
                // partial uri to the context location
                int locationLen = wm->getLocationLen();
                int uriLen = strlen(uri);
                char* filePath = (char*)pool_malloc(sn->pool, 
                                                    locationLen + uriLen + 1);

                memcpy(filePath, location, locationLen);
                if (uriLen)
                    memcpy(filePath + locationLen, uri, uriLen);
                filePath[locationLen + uriLen] = '\0';

                // set ntrans-base which is the effective docroot
                pblock_kvreplace(pb_key_ntrans_base, (char*)location,
                                 locationLen, rq->vars);

                // request is to a filesystem resource, set the filepath in
                // rq->vars so that further nsapi processing will get the path
                pblock_kvreplace(pb_key_ppath, filePath, 
                                 locationLen + uriLen, rq->vars);
                pool_free(sn->pool, filePath);
            }

            rq->directive_is_cacheable = 1;

            return REQ_PROCEED;
        }
    }

    rq->directive_is_cacheable = 1;

    return REQ_NOACTION;
}

/**
 * Sets Content-type for resources in a web application. Only sets
 * Content-type if it hasn't already been set.
 *
 * NOTE: type-j2ee must be the first ObjectType SAF in the default objset so
 * that web.xml mime-mappings take precedence
 */
_J2EE_FUNC_
int type_j2ee(pblock* pb, Session* sn, Request* rq)
{
    NSAPIRequest* nrq = (NSAPIRequest*)rq;
    WebModule* wm = (WebModule*)nrq->webModule;

    rq->directive_is_cacheable = 1;

    const char* otype = pblock_findkeyval(pb_key_content_type, rq->srvhdrs);

    // Only set Content-type if if the URI maps to a resource that is in a 
    // web application and if Content-type has not been set
    if (wm && !otype) {

        ServletResource* sr = (ServletResource*)nrq->servletResource;

        if (sr && sr->hasContent()) {
            // The URI maps to Java content (i.e. a servlet or JSP
            // but not a filter or security-constraint). Set Content-type
            // so that send-file won't attempt to service the request
            pblock_kvinsert(pb_key_content_type, MAGNUS_INTERNAL_J2EE,
                            MAGNUS_INTERNAL_J2EE_LEN, rq->srvhdrs);
        } else {
            // The URI maps to a resource protected by a filter or a 
            // security-constraint or to a resource that will be served
            // directly by core. Apply web.xml mime-mappings (if any).
            char* path = pblock_findkeyval(pb_key_path, rq->vars);
            char* extension = strrchr(path, '.');
            if (extension) {
                const char* mimeType = wm->getMimeMapping(extension + 1);
                if (mimeType) {
                    pblock_kvinsert(pb_key_content_type, (char*)mimeType,
                                     strlen(mimeType), rq->srvhdrs);
                }
            }
        }
    }

    return REQ_PROCEED;

}


/**
 * If the requested URI is a directory within a web application and if a
 * welcome-file-list has been configured in web.xml, then serve the welcome-file
 * if it exists in the requested directory.
 *
 * NOTE: find-index-j2ee must occur before find-index in the default objset so
 * that web.xml welcome-file-list takes precedence
 */
_J2EE_FUNC_
int find_index_j2ee(pblock* pb, Session* sn, Request* rq)
{
    rq->directive_is_cacheable = 1;

    NSAPIRequest* nrq = (NSAPIRequest*)rq;
    WebModule* wm = (WebModule*) nrq->webModule;
    if ((wm == NULL) || !wm->getNumWelcomeFiles())
        return REQ_NOACTION;   // welcome-file-list not configured

    PRBool isWar = wm->isWarFile();
    
    // Only do welcome-file processing for GET/HEAD/POST requests
    if (!ISMGET(rq) && !ISMPOST(rq) && !ISMHEAD(rq))
        return REQ_NOACTION;

    pb_param* pp = pblock_findkey(pb_key_path, rq->vars);
    char* path = pp->value;

    if (!path)
        return REQ_NOACTION;

    // do welcome-file-list processing only for directories
    NSFCFileInfo* finfo = NULL;
    if (!isWar &&
        (INTrequest_info_path(path, rq, &finfo) != PR_SUCCESS) ||
        (finfo && (finfo->pr.type != PR_FILE_DIRECTORY)))
        return REQ_NOACTION;

    rq->directive_is_cacheable = 0;
    
    char* uri = pblock_findkeyval(pb_key_uri, rq->reqpb);

    // temporarily remove any uri params
    char* uriParams = strchr(uri, ';');
    if (uriParams)
        *uriParams = '\0';

    long jContext = 0;
    jboolean warDirExists = JNI_FALSE;

    if (isWar) {
        JvmCall jvm_call;
        JNIEnv* env = jvm_call.getEnv();
        if (env == NULL) {
            return REQ_ABORTED;
        }

        char* dirPathInWarFile = uri + wm->getContextPathLen();
        jstring jwarResourceName = 
                        NSJavaUtil::createString(env, dirPathInWarFile);
        jContext = wm->getJavaContextObject();

        warDirExists = env->CallBooleanMethod((jobject)jContext,
                                                _webModuleIsWARDir,
                                                jwarResourceName);
        if (env->ExceptionOccurred()) {
            env->ExceptionClear();
        }
        if (jwarResourceName != NULL) {
            env->DeleteLocalRef(jwarResourceName);
        }

        // do welcome-file-list processing only for directories
        if (!warDirExists) {
            return REQ_NOACTION;
        }
    }

    int urilen = strlen(uri);

    if (uri[urilen - 1] != '/') {
        if (isWar) {
            // redirect URIs without a trailing slash
            pb_param *url = pblock_key_param_create(rq->vars, pb_key_url, NULL, 0);
            url->value = protocol_uri2url_dynamic(uri, "/", sn, rq);
            pblock_kpinsert(pb_key_url, url, rq->vars);
            protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
            return REQ_ABORTED;
        } else {
            // find-index redirects URIs without a trailing slash
            // restore ';' if it was removed earlier
            if (uriParams)
                *uriParams = ';';
            return REQ_NOACTION;
        }
    }

    // Allocate enough memory for the path + the name of the welcome file
    int lp = strlen(path);
    int size = lp + wm->getLongestWelcomeFileLen() + 1;
    char* file = (char*) pool_malloc(sn->pool, size);

    int newlen = urilen + wm->getLongestWelcomeFileLen() + 1;
    char* tempuri = (char *)pool_malloc(sn->pool, newlen);

    // Iterate thru the welcome-file-list. Redirect to the first existing file
    int return_status = REQ_NOACTION;
    int done = false;
    for (int i = 0; (i < wm->getNumWelcomeFiles() && !done); i++) {
        const char* welcome = wm->getWelcomeFile(i);
        char* langpath = NULL;
        if (!isWar) {
            memcpy(file, path, lp);
            file[lp] = '\0';
            strcat(file, welcome);
            langpath = file;
            /* additonal language lookup if accept-language feature is on */
            const VirtualServer *vs = request_get_vs(rq);
            if (vs->localization.negotiateClientLanguage) {
                char *newpath;
                if (newpath = lang_acceptlang_file(file))
                    langpath = newpath;
            }
            finfo = NULL;
        }

        if (!isWar && INTrequest_info_path(langpath, rq, &finfo) == PR_SUCCESS) {
            if (finfo && finfo->pr.type == PR_FILE_FILE) {
                pool_free(sn->pool, pp->value);
                pp->value = langpath;
                int pathLen = strlen(welcome);
                char *luri = (char *) pool_malloc(sn->pool, urilen + pathLen + 1);
                memcpy(luri, uri, urilen);
                memcpy(&luri[urilen], welcome, pathLen);
                luri[urilen + pathLen] = '\0';
                char *query = pblock_findkeyval(pb_key_query, rq->reqpb);

                // Don't free file if its used in path
                // It will be freed when the request goes away
                if (file == langpath)
                    file = NULL; 

                return_status =  request_restart(sn, rq, NULL, luri, query);
                pool_free(sn->pool, luri);
                done = true;
            }
        } else {
            // Per SRV.9.10, check if the (valid partial req + welcome file) 
            // maps to any dynamic/static resource in the web application

            memcpy(tempuri, uri, urilen + 1); // copy the null terminator also
            strcat(tempuri, welcome);

            ServletResource* servletResource = NULL;
            char* servletPath = NULL;
            char* pathInfo = NULL;

            NSAPIRequest* nsapiReq = (NSAPIRequest*)rq;

            char* uriWithoutContextPath = tempuri + wm->getContextPathLen();
            servletResource = wm->matchUrlPattern(uriWithoutContextPath,
                                                  servletPath,
                                                  pathInfo);
            if (servletResource) {
                char *query = pblock_findkeyval(pb_key_query, rq->reqpb);

                // create a subreq for this req uri 
                Request *rq_sub = 
                            request_create_child(sn, rq, NULL, tempuri, query);

                // create a new session for the subrequest
                Session *sn_sub = session_clone(sn, rq_sub);

                int res = servact_handle_processed(sn_sub, rq_sub);

                // No error should occur *and* status code should be 200.
                // In case of non-existing jsp as welcome file, req uri maps to
                // JspServlet. The above function performs the task
                // successfully but status code will be a 404.
                // In such case, we should not restart the request
                if (res == REQ_PROCEED && rq_sub->status_num == PROTOCOL_OK) {
                    return_status = request_restart(sn, rq, NULL, tempuri, query);
                    done = true;
                }
                // destroy the subrequest, its session, and the associated
                // filter stack
                request_free(rq_sub);
                session_free(sn_sub);
            }
        }
    }
    pool_free(sn->pool, tempuri);
    // restore ';' if it was removed earlier
    if (uriParams)
        *uriParams = ';';
    pool_free(sn->pool, file);
    return return_status;          // no welcome-file found
}

_J2EE_FUNC_
int service_j2ee(pblock* pb, Session* sn, Request* rq)
{
    NSAPIConnector *connector = NSAPIConnector::getConnector(pb, sn, rq);
    if (connector == NULL) {
        return REQ_ABORTED;
    }
    connector->enableOutputBuffering();
    const VirtualServer* vs = request_get_vs((Request*)connector->getRequest());
    NSAPIVirtualServer* nsapiVS = (NSAPIVirtualServer *) vs_get_data(vs, slot_number); 
    connector->service(nsapiVS);
    PRInt32 callStatus = connector->getCallStatus();
    connector->release();

    return callStatus;
}

/**
 *  error_j2ee - error handler for webapps
 */
_J2EE_FUNC_
int error_j2ee(pblock* pb, Session* sn, Request* rq)
{
    int ret = REQ_NOACTION;

    // nrq->webModule is set to the web module in the nametrans
    NSAPIRequest* nrq = (NSAPIRequest*)rq;
    WebModule* wm = (WebModule*)nrq->webModule;
    if (wm) {
        int errorCode = rq->status_num;
        const char* errorPage = wm->getErrorPage(errorCode);

        // If an error page is specified for the error-code, the server
        // serves it through an internalRedirect
        if (errorPage != NULL) {
            // mark the request as coming from the error-j2ee SAF so
            // that javax.servlet.error.* attributes are populated with the
            // correct values
            pblock_kvinsert(pb_key_magnus_internal_error_j2ee, "1", 1,
                            rq->vars);

            ret = servact_include_virtual(sn, rq, errorPage, NULL);
        }
    }
    return ret;
}

/*
 * Shutdown the VM
 */
_J2EE_FUNC_
void shutdown_j2ee(void* data)
{
    NSAPIRunner::shutdown();
    close_resource_bundle(j2eeResourceBundle);
}

/*
 * j2ee_vs_create -- called upon when a virtual server is being initialized.
 */
_J2EE_FUNC_
int j2ee_vs_create(VirtualServer* incoming, const VirtualServer* outgoing)
{
    int ret = REQ_ABORTED;
    PR_ASSERT(incoming);

    const char* vsID = vs_get_id(incoming);
    PR_ASSERT(vsID);

    NSAPIVirtualServer* nsapiVS = new NSAPIVirtualServer(incoming);
    PR_ASSERT(nsapiVS);

    vs_set_data(incoming, &slot_number, (void*)nsapiVS);

    const Configuration* conf = vs_get_conf(incoming);

    ret = NSAPIRunner::createVS(vsID, conf, nsapiVS);

    if (ret != REQ_PROCEED)
    {
        vs_set_data(incoming, &slot_number, NULL);
        delete nsapiVS;

        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.j2ee.ERR_WEBAPP_VSINIT_FAILED");
        ereport(LOG_FAILURE, logMsg, vsID);
        FREE(logMsg);
        return REQ_ABORTED;
    }
    
    ereport(LOG_VERBOSE, (char*)"Successfully initialized web application environment for virtual server [%s]", vsID);
    return REQ_PROCEED;
}

/*
 * Called when a virtual server is being destroyed
 */
_J2EE_FUNC_
void j2ee_vs_destroy(VirtualServer* outgoing)
{
    PR_ASSERT(outgoing);

    const char* vsID = vs_get_id(outgoing);

    NSAPIVirtualServer* nsapiVS;
    nsapiVS = (NSAPIVirtualServer *)vs_get_data(outgoing, slot_number);

    if (nsapiVS != NULL) {
        ereport(LOG_VERBOSE, "Closing web application environment for virtual server [%s]", vsID);

        const Configuration* conf = vs_get_conf(outgoing);

        NSAPIRunner::destroyVS(vsID, conf, nsapiVS);

        vs_set_data(outgoing, &slot_number, NULL);
        delete nsapiVS;
    }
}

/*
 * The first phase of installling a new Configuration (precedes the first
 * j2ee_vs_create call)
 */
_J2EE_FUNC_
int j2ee_conf_preinit(Configuration* incoming, const Configuration* outgoing)
{
    return NSAPIRunner::confPreInit(incoming, outgoing);
}

/*
 * The last phase of installing a new Configuration (follows the last
 * j2ee_vs_create call)
 */
_J2EE_FUNC_
int j2ee_conf_postinit(Configuration* incoming, const Configuration* outgoing)
{
    int status = NSAPIRunner::confPostInit(incoming, outgoing);

    /* TO DO: For 64 bit, we will need to some how use a 
     * different stacksize value than what is specified 
     * in the core.
     */
    unsigned long stackSize = 0;
    if (incoming)
        stackSize = incoming->threadPool.stackSize;

    if (!isready_thread_started && (status == REQ_PROCEED)) {
        // Temporary workaround to start a one-time thread that simulates a 
        // NSAPI callback signalling that the server is "ready" to 
        // process requests.
        isready_thread_started = PR_TRUE;
        PRThread* thread = PR_CreateThread(PR_SYSTEM_THREAD,
                                           isready_thread, NULL,
                                           PR_PRIORITY_NORMAL,
                                           PR_GLOBAL_THREAD,
                                           PR_UNJOINABLE_THREAD,
                                           stackSize);
    }
        
    return status;
}

/*
 * The first phase of destroying a Configuration (precedes the first
 * j2ee_vs_destroy call)
 */
_J2EE_FUNC_
void j2ee_conf_predestroy(Configuration* outgoing)
{
    NSAPIRunner::confPreDestroy(outgoing);
}

/*
 * The last phase of destroying a Configuration (follows the last
 * j2ee_vs_destroy call)
 */
_J2EE_FUNC_
void j2ee_conf_postdestroy(Configuration* outgoing)
{
    NSAPIRunner::confPostDestroy(outgoing);
}

/*
 * This thread loops until the server is "ready" and then simulates
 * an "onReady" callback.
 *
 * This temporary workaround may be replaced by an NSAPI onReady callback,
 * once that infrastructure has been implemented.
 */
_J2EE_FUNC_
void isready_thread(void* data)
{
    const PRIntervalTime timeout = PR_MillisecondsToInterval(200);
    ereport(LOG_VERBOSE, (char*)"Waiting until the server is ready");
    while (!WebServer::isReady()) {
        PR_Sleep(timeout);
    }
    ereport(LOG_VERBOSE, (char*)"The server is now ready to process requests");
    NSAPIRunner::onReady();
}

void stripPathSegmentParams(char* param)
{
    PR_ASSERT(*param == ';');

    // skip over all params in this path segment
    char *rest = param + 1;
    while (*rest && *rest != '/')
        rest++;

    // copy rest of the path in place of the params
    while (*param++ = *rest++);
}

char* stripJSessionId(char* path)
{
    char* id = strstr(path, JSESSIONID);
    if (id) {
       // start after the ';'
       char* rest = id + 1;

       // skip over the jsession id param till end or till the next ';' or '/'
       while (*rest && *rest != ';' && *rest != '/')
           rest++;

       // copy rest of the path in place of the jsessionid
       while (*id++ = *rest++);
    }
    return path;
}
