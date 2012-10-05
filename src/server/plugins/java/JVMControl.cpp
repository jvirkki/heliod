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
#include <stdlib.h>
#include <ctype.h>

#include "JVMControl.h"
#include "NSJavaUtil.h"


#include "httpdaemon/JavaConfig.h"
#include "base/ereport.h"
#include "base/util.h"
#include "frame/conf.h"
#include "frame/conf_init.h"
#include "pratom.h"

#define TEMP_BUF_SIZE           1024
#define PATH_BUF_SIZE           4096

JavaVM * JVMControl::_jvm = NULL;

int JVMControl::_attachRefCountKey  = -1;
PRBool  JVMControl::_catchSignals   = PR_FALSE;
int  *JVMControl::_signum = NULL;
int   JVMControl::_siglen = 0;

JavaVMOption JVMControl::_options[MAX_JVM_PROPERTIES];


int JVMControl::_requestSerialize       = 0;    // serialize first request

static  int _allowJvmExit= 0;
static const PRBool DEFAULT_STICKY_ATTACH = PR_TRUE;

PRBool JVMControl::_stickyAttach = DEFAULT_STICKY_ATTACH; // never detach flag
static PRBool _jvmInitPhase = PR_FALSE;

#ifndef AIX
#define XML_DOCUMENT_BUILDER_FACTORY "com.sun.org.apache.xerces.internal.jaxp.DocumentBuilderFactoryImpl"
#define XML_SAX_PARSER_FACTORY "com.sun.org.apache.xerces.internal.jaxp.SAXParserFactoryImpl"
#else
#define XML_DOCUMENT_BUILDER_FACTORY "org.apache.xerces.jaxp.DocumentBuilderFactoryImpl"
#define XML_SAX_PARSER_FACTORY "org.apache.xerces.jaxp.SAXParserFactoryImpl"
#endif

// Since the server allows languages to be specified only as per 
// http://www.ietf.org/rfc/rfc3066.txt, we need to tokenize the
// language string on - rather than _ which we did earlier.
#define LANG_SEP "-"

/**
 * thread destruction callback to detach from the JVM upon thread destruction
 */
static void
PR_CALLBACK JVMThreadDTOR (void *priv)
{
    JVMControl::detach ((int *)priv, 1);
}

void JNICALL jvm_exit(jint code)
{
    if (_allowJvmExit) {
        NSJavaUtil::log (LOG_INFORM, "Exiting JVM" );
        _exit (code);
    }
}

extern "C" void jvm_atexit_handler(void)
{
    if (_jvmInitPhase)
    {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.JVMControl.ERR_JVM_CREATE");
        NSJavaUtil::log(LOG_CATASTROPHE, logMsg);
        FREE(logMsg);
    }
}

jint JNICALL jvm_vfprintf(FILE *fp, const char *format, va_list args)
{
    int ret = 0;
    char buf [LOG_BUF_SIZE];

    util_vsnprintf (buf,LOG_BUF_SIZE, format,args);
    ret = fputs (buf, stderr);
    return ret;
}

void
JVMControl::release ()
{
    if (_jvm) {
        _jvm->DestroyJavaVM();
        _jvm = NULL;
    }
}

JavaVM *
JVMControl::acquire ()
{

    if (_jvm == NULL) {
        // Install an exit handler to log a FATAL error message when the
        // JVM initialization fails and the JVM calls exit() 
        atexit(jvm_atexit_handler);

        jint numVMs = 0;

#if !defined (XP_PC)
        JVMStoreSignals signals (_catchSignals, _signum, _siglen);
#endif

        if (JNI_GetCreatedJavaVMs (&_jvm, 1, &numVMs) < 0 || numVMs == 0) {
            if (! startJVM())
                return NULL;
        }
        else {
            char* logMsg = get_message(j2eeResourceBundle,
                                       "j2ee.JVMControl.ERR_JVM_EXISTS");
            NSJavaUtil::log (LOG_INFORM, logMsg, numVMs);
            FREE(logMsg);
        }

        //
        // Create a thread-private data slot for threads that attach to the
        // VM. The thread private data is used for 2 purposes:
        //   - to refcount attaches (when stickyAttach is not enabled)
        //   - to ensure that a thread that is attached to the JVM is detached
        //     (regardless of whether stickyAttach is enabled or not) *before*
        //     the thread exits
        //
        if (_attachRefCountKey == -1) {
            if (PR_NewThreadPrivateIndex((unsigned int *)&_attachRefCountKey,
                                          JVMThreadDTOR) != PR_SUCCESS) {
                return NULL;
            }
        }
    }
    return _jvm;
}

/**
 * attach - attach current thread to the JVM
 *
 * @return JNIEnv
 */
JNIEnv *
JVMControl::attach ()
{
    if (_jvm == NULL || _attachRefCountKey == -1)
        return NULL;

    void   *env = NULL;
    
    jint status = _jvm->GetEnv(&env, JNI_VERSION_1_4);
    // If the thread is not yet attached as in the case of the first
    // time for stickyAttach or each time for a non stickyAttach then
    // call AttachCurrentThread
    if (status == JNI_EDETACHED) {
        if (_stickyAttach) {
            // Just store some dummy data in order to ensure that the
            // thread private data destructor function (JVMThreadDTor) will
            // be called when this thread exits. JVMThreadDTor will in turn
            // detach this thread from the JVM *before* the thread exits
            PR_SetThreadPrivate(_attachRefCountKey, (void*) 1);
        }
        char threadName[TEMP_BUF_SIZE];
        static PRInt32 threadNum = 0;
        // Store the thread number in a local variable and use that when
        // creating the thread name, as multiple threads can simultaneously
        // attempt to attach to the JVM
        PRInt32 localThreadNum = PR_AtomicIncrement(&threadNum);
        PR_snprintf(threadName, TEMP_BUF_SIZE, "service-j2ee-%u",
                    localThreadNum);
        NSJavaUtil::log (LOG_VERBOSE, "Attaching thread %s to JVM", threadName);
        // Identify these threads as web container JNI threads
        JavaVMAttachArgs attachArgs;
        attachArgs.version = JNI_VERSION_1_4;
        attachArgs.name = threadName;
        attachArgs.group = NULL;
        if (_jvm->AttachCurrentThread(&env, &attachArgs) < 0) {
            char* logMsg = get_message(j2eeResourceBundle, 
                                    "j2ee.JVMControl.ERR_JVM_ATTACH");
            NSJavaUtil::log (LOG_FAILURE, logMsg, threadName);
            FREE(logMsg);
            env = NULL;
        }
    }

    // If stickyAttach is not enabled, keep a refCount to avoid premature
    // detach
    if (!_stickyAttach) {
        int *refCount = (int *)PR_GetThreadPrivate(_attachRefCountKey);
        // env is not NULL if status == JNI_OK or has been attached now
        if (env != NULL) {
            if (refCount == NULL) {
                // allocate the storage
                refCount  = new int;
                if (refCount == NULL)
                    return NULL;
                *refCount = 0;
                PR_SetThreadPrivate(_attachRefCountKey, (void *)refCount);
            }

            // Incr the refcount if newly attached or is already attached
            *refCount = *refCount + 1;
        }
        else {
            PR_SetThreadPrivate(_attachRefCountKey, NULL);
            delete refCount;
        }

    }

    return (JNIEnv *)env;
}

/**
 * detach - detach current thread from the JVM
 *
 * @param refCount  - attach refcount (0 by default)
 * @param isDestructor   - destructor call
 */
int
JVMControl::detach (int *refCount, int isDestructor)
{
    int rval = 0;

    if (!_stickyAttach) {
        //
        // Decrement the count and detach if its the last one
        //
        if (!isDestructor) {   // can be called from the destructor
            refCount = (int *)PR_GetThreadPrivate(_attachRefCountKey);
        }

        if (refCount == NULL)
            return rval;    // was never attached
        if (*refCount == 1) {
            if (_jvm != NULL) {
                // destructing (refcount going to "0")
                rval = _jvm->DetachCurrentThread();
            }
            *refCount = *refCount - 1;
        }
        else {
            if (*refCount != 0) 
                *refCount = *refCount - 1;
        }
        if (isDestructor && *refCount == 0) {
            // this is a real destructor
            delete refCount;
        }
    } else if (isDestructor) {      // stickyAttach == PR_TRUE
        // When stickyAttach is enabled, we still need to detach
        // the thread from the JVM when the thread itself is exiting.
        if (_jvm != NULL) {
            void* env = NULL;
            jint status = _jvm->GetEnv(&env, JNI_VERSION_1_4);
            if (status != JNI_EDETACHED) {
                // this thread is attached to the VM, so detach it
                rval = _jvm->DetachCurrentThread();
            }
        }
    }

    return rval;
}

PRBool
JVMControl::startJVM ()
{
    JavaVMInitArgs  vm_args;
    JNIEnv * env;
    JavaVM * jvm;
    memset (&vm_args, 0, sizeof (vm_args));

    _loadConfigFromServerXml(vm_args);

    NSJavaUtil::log(LOG_VERBOSE, "jvm stickyAttach: %d", _stickyAttach);
    for (int mm = 0; mm < vm_args.nOptions; mm++) {
        NSJavaUtil::log (LOG_VERBOSE, "jvm option: %s", 
                         vm_args.options[mm].optionString);
    }

    _jvmInitPhase = PR_TRUE;
    if (JNI_CreateJavaVM (&jvm, (void **)&env, &vm_args) < 0) {
        _jvm = NULL;
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.JVMControl.ERR_JVM_CREATE");
        NSJavaUtil::log (LOG_CATASTROPHE, logMsg);
        FREE(logMsg);
        return (false);
    }
    else {
        _jvm = jvm;
    }
    _jvmInitPhase = PR_FALSE;

    if (_jvm != NULL)
        JException::init (env);
    
    if (env->ExceptionOccurred())
        env->ExceptionClear();

    return (true);
}

void 
JVMControl::_loadConfigFromServerXml(JavaVMInitArgs & vm_args)
{
    char *cfgClasspath = NULL;
    int nopts  = 0;

    const ServerXMLSchema::Jvm *jvm = jvm_get_config();

    if (jvm == NULL) {
        NSJavaUtil::log (LOG_WARN, "<jvm> element is server.xml is incorrect");
        return;
        
    }

    int num_jvm_options = 0;
    const char **jvm_options = (const char **)MALLOC(jvm->getJvmOptionsCount() * sizeof(char *));
    for (num_jvm_options = 0; num_jvm_options < jvm->getJvmOptionsCount(); num_jvm_options++)
        jvm_options[num_jvm_options] = *jvm->getJvmOptions(num_jvm_options);

    vm_args.version = JNI_VERSION_1_4;  // set JVM version (required)
    vm_args.ignoreUnrecognized = JNI_TRUE;


    char temp[TEMP_BUF_SIZE];
    char* java_home = strdup(jvm->javaHome);
    PR_snprintf(temp, TEMP_BUF_SIZE, "-DJAVA_HOME=%s",java_home);
    _options[nopts++].optionString = strdup(temp);

    PR_snprintf(temp, TEMP_BUF_SIZE,
    "-Djava.util.logging.manager=com.sun.webserver.logging.ServerLogManager");
    _options[nopts++].optionString = strdup(temp);

    PR_snprintf(temp, TEMP_BUF_SIZE,"-Djavax.xml.parsers.DocumentBuilderFactory=%s",XML_DOCUMENT_BUILDER_FACTORY);
    _options[nopts++].optionString = strdup(temp);

    PR_snprintf(temp, TEMP_BUF_SIZE,"-Djavax.xml.parsers.SAXParserFactory=%s",XML_SAX_PARSER_FACTORY);
    _options[nopts++].optionString = strdup(temp);

    PR_snprintf(temp, TEMP_BUF_SIZE, "-Dproduct.name=%s",conf_getServerString());
    _options[nopts++].optionString = strdup(temp);

    if (conf_getglobals()->Vnetsite_root) {
        PR_snprintf(temp, TEMP_BUF_SIZE, "-Dcom.sun.web.installRoot=%s",conf_getglobals()->Vnetsite_root);
        _options[nopts++].optionString = strdup(temp);
    }

    if (conf_getglobals()->Vserver_root) {
        PR_snprintf(temp, TEMP_BUF_SIZE, "-Dcom.sun.web.instanceRoot=%s",conf_getglobals()->Vserver_root);
        _options[nopts++].optionString = strdup(temp);
    }

    const char *defLang=XP_GetLanguage(XP_LANGUAGE_AUDIENCE_ADMIN);
    if (defLang && *defLang) {
        char *locale = STRDUP(defLang);
        char *token=strtok(locale, LANG_SEP);
        PR_snprintf(temp,TEMP_BUF_SIZE,"-Duser.language=%s",token);
        _options[nopts++].optionString = strdup(temp);
        token=strtok(NULL, LANG_SEP);
        if (token && *token) {
            PR_snprintf(temp,TEMP_BUF_SIZE, "-Duser.country=%s",token);
            _options[nopts++].optionString = strdup(temp);
        }
        token=strtok(NULL, LANG_SEP);
        if (token && *token) {
            PR_snprintf(temp,TEMP_BUF_SIZE,"-Duser.variant=%s",token);
            _options[nopts++].optionString = strdup(temp);
        }
        FREE(locale);
    }

    _options[nopts].optionString    = "exit";
    _options[nopts].extraInfo       = (void *)jvm_exit;
    nopts++;

    
    _options[nopts].optionString    = "vfprintf";
    _options[nopts].extraInfo       = (void *)jvm_vfprintf;
    nopts++;


    // MAX_JVM_PROPERTIES -1: save one for sending down classpath 
    handleJvmOptions(jvm_options, num_jvm_options, &nopts,
                     MAX_JVM_PROPERTIES -1);

    // Get the classpath prefix
    char* classpath_prefix = NULL;
    if (strlen(jvm->classPathPrefix))
        classpath_prefix = preProcessClasspath(jvm->classPathPrefix, java_home);

    // Get the server classpath
    char* server_classpath = NULL;
    if (strlen(jvm->serverClassPath))
        server_classpath = preProcessClasspath(jvm->serverClassPath, java_home);

    // Get the classpath suffix
    char* classpath_suffix = NULL;
    if (strlen(jvm->classPathSuffix))
        classpath_suffix = preProcessClasspath(jvm->classPathSuffix, java_home);

    // Get the env-classpath-ignored value
    PRBool ignore_classpath = jvm->envClassPathIgnored;

    // Handle debug options if present
    if (jvm->debug) {
        int num_debug_jvm_options = 0;
        const char **debug_jvm_options = (const char **)MALLOC(jvm->getDebugJvmOptionsCount() * sizeof(char *));
        for (num_debug_jvm_options = 0; num_debug_jvm_options < jvm->getDebugJvmOptionsCount(); num_debug_jvm_options++)
            debug_jvm_options[num_debug_jvm_options] = *jvm->getDebugJvmOptions(num_debug_jvm_options);

        if (num_debug_jvm_options) {
            // Parse the debug options if necessary
            // Save one for sending down the classpath
            handleJvmOptions(debug_jvm_options, num_debug_jvm_options,
                             &nopts, MAX_JVM_PROPERTIES -1);
        }

        FREE(debug_jvm_options);
    }

    // Handle profiler options and get the classpath if present
    char* profiler_classpath = handleProfiler(*jvm, &nopts, java_home);

    // Set the cumulative classpath
    setClassPath(classpath_prefix, server_classpath, classpath_suffix, 
		 profiler_classpath, ignore_classpath, &nopts);

    if (classpath_prefix)
        free(classpath_prefix);

    if (server_classpath)
        free(server_classpath);

    if (classpath_suffix)
        free(classpath_suffix);

    if (profiler_classpath)
        free(profiler_classpath);

    if (java_home)
        free(java_home);

#if defined (SOLARIS) && defined (_LP64)
    // Workaround for CR:6285325
    // Set the java library search path for 64bit web server.
    setLibPath(&nopts); 
#endif

    //fill in the properties
    if (nopts)
    {
        vm_args.options = _options;
        vm_args.nOptions = nopts;
    }

    // Free the jvm options
    FREE(jvm_options);

}

/*
 * Handle classpath attribute. class_path may contain a token ${java.home}
 * and this method will expand the token in specified class_path
 */
char *
JVMControl::preProcessClasspath(const char *class_path, char *java_home)
{
    const char* JAVA_HOME_TOKEN = "${java.home}";
    const char *current;
    const char *found;
    char *result;
    int result_len = 0;
    int num_matches = 0;

    if (class_path == NULL) {
        return NULL;
    }
    if (java_home == NULL) {
        return strdup(class_path);
    }
    current = class_path;
    while ((found = strstr(current, JAVA_HOME_TOKEN)) != NULL) {
        current = found + strlen(JAVA_HOME_TOKEN);
        num_matches++;
    }
    result_len = strlen(class_path)
            + (num_matches * (strlen(java_home) - strlen(JAVA_HOME_TOKEN))) + 1;
    result = (char *)malloc(result_len);
    memset(result, '\0', result_len);
    current = class_path;
    while ((found = strstr(current, JAVA_HOME_TOKEN)) != NULL) {
        strncat(result, current, abs(found - current));
        strcat(result, java_home);
        current = found + strlen(JAVA_HOME_TOKEN);
    }
    strcat(result, current);
    return result;
}

/**
 * Handle profiler jvmoptions and properties
 */
char*
JVMControl::handleProfiler(const ServerXMLSchema::Jvm& jvm, int* nopts, char* java_home)
{
    char* classpath = NULL;

    char* str = NULL;
    for (int i = 0; i < jvm.getProfilerCount(); i++) {
        const ServerXMLSchema::Profiler& profiler = *jvm.getProfiler(i);

        // Determine if the profiler is enabled 
        if (profiler.enabled) {
            if(!NSJavaUtil::isSingleProcess ()) {
                char* logMsg = get_message(j2eeResourceBundle,
                                           "j2ee.JVMControl.ERR_JVM_SINGLEPROCESS");
                NSJavaUtil::log (LOG_WARN, logMsg);
                FREE(logMsg);
            }

            if (strlen(profiler.classPath))
                classpath = preProcessClasspath(profiler.classPath, java_home);

            // If there is a valid profiler, parse the jvm options 
            // associated with the profiler

            int num_jvm_options = 0;
            const char **jvm_options = (const char **)MALLOC(profiler.getJvmOptionsCount() * sizeof(char *));
            for (num_jvm_options = 0; num_jvm_options < profiler.getJvmOptionsCount(); num_jvm_options++)
                jvm_options[num_jvm_options] = *profiler.getJvmOptions(num_jvm_options);

            // Save one for sending down the classpath
            handleJvmOptions(jvm_options, num_jvm_options, nopts, 
                             MAX_JVM_PROPERTIES -1);

            FREE(jvm_options);

            break;
        }
    }
    return classpath;
}

void 
JVMControl::setClassPath(char* classpath_prefix,
			 char* server_classpath, 
			 char* classpath_suffix,  
			 char* profiler_classpath, 
                         PRBool env_cp_ignored, int* nopts)
{
    // Send down the classpath based on classpath-suffix and any profiler
    // classpath
    int classpath_len = 0;
    if (classpath_prefix) {
        // add one for the separator
        classpath_len += strlen(classpath_prefix) + 1; 
    } 
    if (server_classpath) {
        // add one for the separator
        classpath_len += strlen(server_classpath) + 1; 
    } 
    if (classpath_suffix) {
        // add one for the separator
        classpath_len += strlen(classpath_suffix) + 1; 
    } 
    // Add the profiler classpath if any
    if (profiler_classpath) {
        classpath_len += strlen(profiler_classpath); 
    }
    
    char* env_classpath = NULL;
    if (!env_cp_ignored) {
        env_classpath =  getenv("CLASSPATH");
        // add one for the separator
        if (env_classpath != NULL)
            // Add one for Sep added before appending this cp
            classpath_len += strlen(env_classpath) + 1;
    }

    if (classpath_len > 0) {
        classpath_len += strlen("-Djava.class.path=");
        classpath_len++; // add one for the null termination
        char* new_classpath = new char[classpath_len];
        strcpy(new_classpath,"-Djava.class.path=");
        if (classpath_prefix) {
            strcat(new_classpath, classpath_prefix);
            strcat(new_classpath, SEP);
        }
        if (server_classpath) {
            strcat(new_classpath, server_classpath);
            strcat(new_classpath, SEP);
        }
        if (classpath_suffix) {
            strcat(new_classpath, classpath_suffix);
            strcat(new_classpath, SEP);
        }
        if (profiler_classpath) {
            strcat(new_classpath, profiler_classpath);
        }
        if (env_classpath) {
            strcat(new_classpath, SEP);
            strcat(new_classpath, env_classpath);
        }
        _options[*nopts].optionString = new_classpath;
        *nopts +=1;
    }
}

#if defined (SOLARIS) && defined (_LP64)

void 
JVMControl::setLibPath(int* nopts)
{
    Dl_serinfo info_size;
    Dl_serinfo* info_path = NULL;
    Dl_serpath* info_searchpath = NULL;

    // Determine search path count and required buffer size
    dlinfo(RTLD_SELF, RTLD_DI_SERINFOSIZE, (void *)&info_size);
    if (info_size.dls_size == 0) {
        return;
    }

    const char* JAVA_LIBPATH_TOKEN = "-Djava.library.path=";
    char search_libpath[PATH_BUF_SIZE];
    int libpath_len = 0;
    search_libpath[0] = 0;
    libpath_len += strlen(JAVA_LIBPATH_TOKEN);

    // Allocate new buffer and initialize
    info_path = (Dl_serinfo*)MALLOC(info_size.dls_size);
    PR_ASSERT(info_path != NULL);
    info_path->dls_size = info_size.dls_size;
    info_path->dls_cnt = info_size.dls_cnt;

    // Obtain search path information
    dlinfo(RTLD_SELF, RTLD_DI_SERINFO, (void *)info_path);
    info_searchpath = &info_path->dls_serpath[0];

    for (int count = 0; count < info_path->dls_cnt; count++) {
        if (info_searchpath && info_searchpath->dls_name) {
            char* path = info_searchpath->dls_name;
            // Add one for Sep before appending this libpath
            libpath_len += strlen(path) + 1;
            strcat(search_libpath, path);
            strcat(search_libpath, SEP);
        }
        info_searchpath++;
    }
    FREE(info_path);

    // Add one for the null termination
    libpath_len++; 

    // Set the java.library.path with the search library path
    // just obtained for this process
    if (search_libpath[0] != 0) {
        char* new_libpath = new char[libpath_len];
        strcpy(new_libpath,JAVA_LIBPATH_TOKEN);
        strcat(new_libpath, search_libpath);
        _options[*nopts].optionString = new_libpath;
        *nopts +=1;
    }
}

#endif

// Get the jvm-options
// Note that there could be more than one options, so parse
// each options line and send multiple options in the same
// line as a separate option
void
JVMControl::handleJvmOptions(const char* const* jvm_options, int num_options, int* nopts,
                             int max_options) 
{
    int i;
    const char* str = NULL;
    int quoted = 0;
    int count = 0;
    char* optionBuf = NULL;
    char* option = NULL;
    int len = 0;
    const char* startp = 0;

    if (!jvm_options)
        return;

    for (i = 0; (i < num_options && *nopts < max_options); i++) {
        str  = jvm_options[i];
        if (str && *str) {
            int size = strlen(str);
            // Allocate a buffer size as big as the jvm-option currently 
            // being processed
            if (size >= len) {
                if (optionBuf != NULL) 
                    FREE(optionBuf);
                optionBuf = (char*) MALLOC(size+1);
                len = size;
            }

            // Skip to the first nonspace character
            while (isspace(*str)) 
                str++;

            startp = str;

            // Parse to send out each option separately
            while (*str) {
                // if its eol or isspace and is not quoted, then 
                // it signifies end of token
                if (!quoted && (isspace(*str) || *str == '\n')) {
                    str++;
                    if (count) {
                        // add one for null termination
                        option = (char*) malloc(count +1);
                        strncpy(option, optionBuf, count);
                        option[count] = '\0';
                        _options[*nopts].optionString = option;
                        *nopts += 1;
                        count = 0;
                        if (*nopts == max_options)
                            break;
                    }
                    // Skip to the first nonspace character
                    while (isspace(*str)) 
                        str++;

                    startp = str;
                    continue;
                }
                // if its a escaped character, skip one and
                // copy the next character to the buffer
                if (quoted && *str == '\\') {
                    str++;
                    if (*str) {
                        optionBuf[count] = *str;
                        str++;
                        count++;
                    }
                    continue;
                }
                // If quoted, then set the flag and continue
                // if end quote, unset the flag and continue
                if (*str == '"') {
                    str++;
                    quoted = !quoted;
                }
                else {
                    optionBuf[count] = *str;
                    str++;
                    count++;
                }
            }
        }
        if (quoted) {
            // If the quote has not ended, then log an error
            char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.JVMControl.MISCONFIG_JVM_OPTION");
            NSJavaUtil::log(LOG_MISCONFIG, logMsg, startp);
            free(logMsg);
        }
        else if (count && *nopts < max_options) {
            // add one for null termination
            option = (char*) malloc(count +1);
            strncpy(option, optionBuf, count);
            option[count] = '\0';
            _options[*nopts].optionString = option;
            *nopts += 1;
        }
        count = 0;
        quoted = 0;
        startp = 0;
    }
    if (optionBuf != NULL) 
        FREE(optionBuf);
}

int
JVMControl::isSerializeFirstRequest ()
{
    return _requestSerialize;
}

void
JVMControl::setCatchSignals (PRBool catchSignals, int * signum, int siglen)
{
    _catchSignals = catchSignals;
    _signum = signum;
    _siglen = siglen;
}


// ---------------------------------------------------------------------- ]
// JvmCall helper class

JvmCall::JvmCall ()
{

    _env = JVMControl::attach ();
}

JvmCall::~JvmCall ()
{
    if (_env != NULL && JVMControl::detach () < 0)
    {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.JVMControl.ERR_JVM_DETACH");
        NSJavaUtil::log (LOG_FAILURE, logMsg);
        FREE(logMsg);
    }
}

// ---------------------------------------------------------------------- ]

#if !defined (XP_PC)

JVMStoreSignals::JVMStoreSignals (PRBool catchSignals, int * signum, int siglen)
{
    _sigarr = NULL;
    _catchSignals = catchSignals;

    if (_catchSignals)
        return;

    _siglen = siglen;

    if (signum != NULL)
    {
        _sigarr = new SigStore [_siglen];

        if (_sigarr != NULL)
        {
            for (int i = 0; i < _siglen; i++)
            {
                _sigarr[i].sigval = signal (signum[i], NULL);
                _sigarr[i].signum = signum[i];
            }
        }
    }
}

JVMStoreSignals::~JVMStoreSignals ()
{
    if (_catchSignals == PR_FALSE 
        && _sigarr != NULL)
    {
        for (int i = 0; i < _siglen; i++)
            signal (_sigarr[i].signum, _sigarr[i].sigval);
    }
    delete [] _sigarr;
}

#endif
