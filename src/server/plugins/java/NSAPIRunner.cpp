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

#include "frame/conf_api.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/WebServer.h"

#include "JVMControl.h"
#include "NSAPIRunner.h"

#include "com_sun_webserver_connector_nsapi_NSAPIRunner.h"

//This is the class to be dynamically loaded for iWS Java Web Core.
//For AppServer or any hosting server, we will need to modify
//here to point to the appropriate class name.
#define J2EE_RUNNER_CLASSNAME "com/sun/webserver/init/J2EERunner"

JavaVM* NSAPIRunner::_jvm = NULL;

jclass NSAPIRunner::_runnerClass = 0;
jobject NSAPIRunner::_runnerObject = 0;

jmethodID NSAPIRunner::_runner_init = 0;
jmethodID NSAPIRunner::_runner_shutdown = 0;
jmethodID NSAPIRunner::_runner_createVS = 0;
jmethodID NSAPIRunner::_runner_destroyVS = 0;

jmethodID NSAPIRunner::_runner_confPreInit = 0;
jmethodID NSAPIRunner::_runner_confPostInit = 0;

jmethodID NSAPIRunner::_runner_confPreDestroy = 0;
jmethodID NSAPIRunner::_runner_confPostDestroy = 0;

jmethodID NSAPIRunner::_runner_onReady = 0;

NSAPIRunner::NSAPIRunner(void)
{
}

/**
 * init     initializes JVM ; must be only called once
 * 
 * @return  PR_SUCCESS if successful
 */

PRStatus
NSAPIRunner::init(void)
{
    // non-java init for utility classes
    if (!NSJavaUtil::init()) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.NSAPIRunner.ERR_UTILITY_CLASSES");
        ereport(LOG_FAILURE, logMsg);
        FREE(logMsg);
        return PR_FAILURE;
    }

    _jvm = JVMControl::acquire();
    if (_jvm == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.NSAPIRunner.ERR_JVM_LOAD");
        ereport(LOG_FAILURE, logMsg);
        FREE(logMsg);
        return PR_FAILURE;
    }

    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env == NULL)
        return PR_FAILURE;    // error is already logged at this point

    if (!NSJavaUtil::java_init(env)) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.NSAPIRunner.ERR_UTILITY_CLASSES");
        ereport(LOG_FAILURE, logMsg);
        FREE(logMsg);
        return PR_FAILURE;
    }

    _runnerClass = NSJavaUtil::findClassGlobal(env, J2EE_RUNNER_CLASSNAME);
    if (_runnerClass == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.ERR_CLASS_LOOKUP");
        ereport(LOG_FAILURE, logMsg, J2EE_RUNNER_CLASSNAME);
        FREE(logMsg);
        return PR_FAILURE;
    }

    _runner_init = env->GetMethodID(_runnerClass , "<init>", "()V");

    _runner_shutdown = env->GetMethodID(_runnerClass, "shutdown",  "()V");

    _runner_createVS = env->GetMethodID(_runnerClass, "createVS" , "(IJLjava/lang/String;Ljava/lang/String;)Lorg/apache/catalina/core/StandardHost;");


    _runner_destroyVS = env->GetMethodID(_runnerClass, "destroyVS", "(IJ)I");

    _runner_confPreInit = env->GetMethodID(_runnerClass, "confPreInit",
                                           "(II)I");
    _runner_confPostInit = env->GetMethodID(_runnerClass, "confPostInit",
                                            "(II)I");

    _runner_confPreDestroy = env->GetMethodID(_runnerClass, "confPreDestroy",
                                              "(I)V");
    _runner_confPostDestroy = env->GetMethodID(_runnerClass, "confPostDestroy",
                                               "(I)V");
    _runner_onReady = env->GetMethodID(_runnerClass, "onReady", "()V");

    if (_runner_init == NULL || _runner_shutdown == NULL  || 
        _runner_createVS == NULL || _runner_destroyVS == NULL ||
        _runner_confPreInit == NULL || _runner_confPostInit == NULL ||
        _runner_confPreDestroy == NULL || _runner_confPostDestroy == NULL ||
        _runner_onReady == NULL)
    {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.ERR_METHODS_LOOKUP");
        ereport(LOG_FAILURE, logMsg);
        FREE(logMsg);
        return PR_FAILURE;
    }
    

    // do actual initialization ...

    _runnerObject = env->NewObject(_runnerClass, _runner_init);
    if (_runnerObject)
        _runnerObject = env->NewGlobalRef(_runnerObject);

    if (_runnerObject == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.NSAPIRunner.ERR_OBJECT_CREATE");
        ereport(LOG_FAILURE, logMsg, J2EE_RUNNER_CLASSNAME);
        FREE(logMsg);
        NSJavaUtil::reportException(env);
        return PR_FAILURE;
    }
    
    return PR_SUCCESS;
}

// Make a JNI call to the servlet engine to create a VS
int
NSAPIRunner::createVS(const char* vsID, const Configuration* conf,
                     NSAPIVirtualServer* j2eeVS)
{
    int attach = 1;
    int rval = REQ_ABORTED;

    JvmCall jvm_call;
    JNIEnv *env = jvm_call.getEnv();

    if (env == NULL)
        return REQ_ABORTED;

    jlong jniVS = (jlong)j2eeVS;
    jstring jhostname = NSJavaUtil::createString(env, vsID);
    char* docroot = j2eeVS->getDocumentRoot();
    jstring jdocroot  = NSJavaUtil::createString(env, docroot);
    jobject newvs = NULL;
    
    if (!env -> ExceptionOccurred())
        newvs = env->CallObjectMethod(_runnerObject, _runner_createVS,
                                      (jint) conf_get_id(conf), jniVS,
                                      jhostname, jdocroot);

    if (newvs) {
        // Release the old object (if any)
        long oldvs = j2eeVS->getJavaObject();
        if (oldvs) {
            env->DeleteGlobalRef((jobject)oldvs);
        }
        jobject jref = env->NewGlobalRef(newvs);
        j2eeVS->setJavaObject((long)jref);
        env->DeleteLocalRef(newvs);
        rval = REQ_PROCEED;
    } else {
        // XXX log 'cannot create vs' error
    }

    FREE(docroot);

    if (NSJavaUtil::reportException(env) == PR_TRUE)
        return REQ_ABORTED;

    return rval;
}


// Make a JNI call to the servlet engine to close down a VS
int
NSAPIRunner::destroyVS(const char* vsID, const Configuration* conf,
                      NSAPIVirtualServer* j2eeVS)
{
    int attach = 1;
    int rval = REQ_ABORTED;

    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env == NULL)
        return REQ_ABORTED;

    jlong jniVS = (jlong)j2eeVS;

    rval = env->CallIntMethod(_runnerObject, _runner_destroyVS, 
                              (jint) conf_get_id(conf), jniVS);
    if (NSJavaUtil::reportException(env) == PR_TRUE)
        return REQ_ABORTED;

    long vs = j2eeVS->getJavaObject();
    if (vs) {
        env->DeleteGlobalRef((jobject)vs);
    }

    return rval;
}

int
NSAPIRunner::confPreInit(Configuration* incoming, const Configuration* outgoing)
{
    int rval = REQ_ABORTED;

    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env == NULL)
        return REQ_ABORTED;


    rval = env->CallIntMethod(_runnerObject, _runner_confPreInit,
                              (jint) conf_get_id(incoming),
                              (jint) conf_get_id(outgoing));

    if (NSJavaUtil::reportException(env) == PR_TRUE)
        return REQ_ABORTED;

    return rval;
}

int
NSAPIRunner::confPostInit(Configuration* incoming, const Configuration* outgoing)
{
    int rval = REQ_ABORTED;

    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env == NULL)
        return REQ_ABORTED;

    rval = env->CallIntMethod(_runnerObject, _runner_confPostInit,
                              (jint) conf_get_id(incoming),
                              (jint) conf_get_id(outgoing));

    if (NSJavaUtil::reportException(env) == PR_TRUE)
        return REQ_ABORTED;

    return rval;
}

void
NSAPIRunner::confPreDestroy(Configuration* outgoing)
{
    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env == NULL)
        return;

    env->CallVoidMethod(_runnerObject, _runner_confPreDestroy,
                        (jint) conf_get_id(outgoing));
}

void
NSAPIRunner::confPostDestroy(Configuration* outgoing)
{
    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env == NULL)
        return;

    env->CallVoidMethod(_runnerObject, _runner_confPostDestroy,
                        (jint) conf_get_id(outgoing));
}

void
NSAPIRunner::onReady(void)
{
    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env == NULL)
        return;

    env->CallVoidMethod(_runnerObject, _runner_onReady);
}

jclass
NSAPIRunner::getRunnerClass(void)
{
    return NSAPIRunner::_runnerClass;
}

jobject
NSAPIRunner::getRunnerObject(void)
{
    return NSAPIRunner::_runnerObject;
}

/**
 * shutdown() - shutdown the JVM
 */

void
NSAPIRunner::shutdown()
{

    JvmCall jvm_call;

    JNIEnv *env = jvm_call.getEnv();

    if (env == NULL)
        return;

    env->CallVoidMethod(_runnerObject, _runner_shutdown);
}

JNIEXPORT jstring JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIRunner_getServerId(JNIEnv *env, jclass cls)
{ 
    jstring jname = NSJavaUtil::createString(env, 
                                             conf_getglobals()->Vserver_id);
    return jname;
}

JNIEXPORT jstring JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIRunner_getInstanceRoot(JNIEnv *env, jclass cls)
{ 
    jstring jname = NSJavaUtil::createString(env, 
                                             conf_getglobals()->Vserver_root);
    return jname;
}

JNIEXPORT jstring JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIRunner_getInstallRoot(JNIEnv *env, jclass cls)
{ 
    jstring jname = NSJavaUtil::createString(env, 
                                             conf_getglobals()->Vnetsite_root);
    return jname;
}

JNIEXPORT void JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIRunner_requestReconfiguration(JNIEnv *env, jclass cls)
{ 
    WebServer::RequestReconfiguration();
}
