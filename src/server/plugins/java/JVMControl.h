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

#ifndef _ISERVLET_JVM_CONTROL_H_
#define _ISERVLET_JVM_CONTROL_H_

#ifdef Linux
#include <bfd.h>
#endif
#ifdef SOLARIS
#include <dlfcn.h>
#endif
#include <prtypes.h>
#include <prthread.h>
#include <prlock.h>
#include <prcvar.h>
#include <plstr.h>
#include "generated/ServerXMLSchema/Jvm.h"
#include "frame/conf.h"
#include "base/pblock.h"

#include "jni.h"

#if defined(XP_PC)
#define _JVMCONTROL_INIT_API  _declspec(dllexport)
#else
#define _JVMCONTROL_INIT_API
#endif

#define MAX_JVM_PROPERTIES      1024 // maxmimum number of allowed JVM properties

// Size of the log buffer for JVM printf
#define LOG_BUF_SIZE            4096 

/*
 * CLASSPATH separator character used in #defines that are used in both
 * Unix and PC builds.
 */
  
#if !defined (SEP)
#if  defined (XP_PC)
#define	SEP	";"
#else
#define	SEP ":"
#endif
#endif


class _JVMCONTROL_INIT_API JVMControl 
{
public:
    //aquire an instance of JVM, create one if necessary
    static JavaVM *acquire ();
    static void release    ();

    /**
     * attach - attach current thread to the JVM
     *
     * @return JNIEnv
     */ 
    static JNIEnv *attach  ();
    /**
     * detach - detach current thread from the JVM
     * @param refCount  - attach refcount (0 by default)
     * @param special   - destructor call
     */
    static int   detach (int *refCount = NULL, int isDestructor = 0);
    static int  isSerializeFirstRequest   ();
    static void setCatchSignals (PRBool catchSignals, int *signum, int siglen);
    
private:
    static PRBool startJVM ();    
    static void _loadConfigFromServerXml (JavaVMInitArgs & vm_args);
    static char* getLibPath(char* jhome);
#if defined (SOLARIS) && defined (_LP64)
    static void setLibPath(int* nopts);
#endif
    static char* preProcessClasspath(const char* class_path, char* java_home);
    static char* handleProfiler(const ServerXMLSchema::Jvm& jvm, int* nopts, char* java_home);
    static void  handleJvmOptions(const char* const* jvm_options, int num_options, 
                                  int* nopts, int maxopts);
    static void handleProperties(pblock* pb, int* nopts, int maxopts);
    static void setClassPath(char* classpath_prefix, char* server_classpath,
                             char* classpath_suffix, char* profiler_classpath,
                             PRBool ignore_env_cp, int* nopts);


    static JavaVM *_jvm;
    static int  _requestSerialize;
    static PRBool _stickyAttach;
    static int  _attachRefCountKey;
    static JavaVMOption _options [];

    static  PRBool  _catchSignals;
    static  int     *_signum;
    static  int      _siglen;
};

// ruslan: helper class for the JVM attach/detach

class _JVMCONTROL_INIT_API JvmCall 
{
    JNIEnv *_env;

public:
    
    JvmCall ();
    ~JvmCall ();
    
    JNIEnv *getEnv () {return _env;}
};

#if !defined (XP_PC)

#include <signal.h>
#include <unistd.h>

typedef struct  SigStore_s
        {
            void (*sigval) (int sig);
            int signum;
        }   SigStore;

class _JVMCONTROL_INIT_API JVMStoreSignals
{
public:
    JVMStoreSignals (PRBool catchSignals, int * signum, int siglen);
    ~JVMStoreSignals ();

private:
    PRBool _catchSignals;
    SigStore  *  _sigarr;
    int _siglen;
};

#endif /* !XP_PC */

#endif

