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

#ifndef _NSAPIunner_h_
#define _NSAPIRunner_h_

#include "netsite.h"
#include "base/cinfo.h"
#include "base/net.h"
#include "base/params.h"
#include "base/pblock.h"
#include "base/plist.h"
#include "base/pool.h"
#include "base/systhr.h"
#include "base/util.h"
#include "frame/conf.h"
#include "frame/http.h"
#include "frame/httpact.h"
#include "frame/log.h"
#include "frame/http.h"
#include "safs/auth.h"

#include "NSJavaUtil.h"
#include "NSAPIVirtualServer.h"

#ifdef XP_WIN32
#ifdef BUILD_J2EE_PLUGIN_DLL
#define _J2EE_PLUGIN_DLL_ _declspec(dllexport)
#else
#define _J2EE_PLUGIN_DLL_ _declspec(dllimport)
#endif /* BUILD_J2EE_PLUGIN_DLL */
#else
#define _J2EE_PLUGIN_DLL_
#endif /* XP_WIN32 */



class _J2EE_PLUGIN_DLL_ NSAPIRunner
{
    public:
    
        NSAPIRunner(void);
 
        static PRStatus init(void);

        static int createVS(const char* vsID, const Configuration* conf,
                            NSAPIVirtualServer* j2eeVS);
        static int destroyVS(const char* vsID, const Configuration* conf,
                             NSAPIVirtualServer* j2eeVS);

        static int confPreInit(Configuration* incoming,
                               const Configuration* outgoing);
        static int confPostInit(Configuration* incoming,
                                const Configuration* outgoing);
        static void confPreDestroy(Configuration* outgoing);
        static void confPostDestroy(Configuration* outgoing);

        static void onReady(void);

        static void shutdown(void);

        void prepare(void);

        // Temporary
        static jclass getRunnerClass(void);
        static jobject getRunnerObject(void);

    private:

        static JavaVM* _jvm;
        
        static jclass _runnerClass;
        static jobject _runnerObject;
        static jmethodID _runner_init;
        static jmethodID _runner_shutdown;
        static jmethodID _runner_createVS;
        static jmethodID _runner_destroyVS;
        static jmethodID _runner_confPreInit;
        static jmethodID _runner_confPostInit;
        static jmethodID _runner_confPreDestroy;
        static jmethodID _runner_confPostDestroy;
        static jmethodID _runner_onReady;

};

#endif /* _NSAPIRunner_h_ */


