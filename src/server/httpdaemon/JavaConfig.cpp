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

#include <stdlib.h>
#include <string.h>
#include "support/NSString.h"
#include "base/platform.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/JavaConfig.h"


// Platform-specific JRE lib paths
static const char *_jre_libpaths[] = {
#if defined(XP_WIN32)
    "\\jre\\bin\\"JNI_MD_LIBTYPE,
    "\\jre\\bin"
#elif defined(AIX)
    "/jre/bin/"JNI_MD_LIBTYPE, /* libjvm.so */
    "/jre/bin"
#else // all other UNIX
    "/jre/lib/"JNI_MD_SYSNAME"/"JNI_MD_LIBTYPE,
    "/jre/lib/"JNI_MD_SYSNAME,
    "/jre/lib/"JNI_MD_SYSNAME"/native_threads"
#endif
};

static const char *_jre_lib64paths[] = {
#if defined(XP_UNIX)
    "/jre/lib/"JNI_MD_SYSNAME64"/"JNI_MD_LIBTYPE,
    "/jre/lib/"JNI_MD_SYSNAME64,
    "/jre/lib/"JNI_MD_SYSNAME64"/native_threads"
#else //For windows, define below when we support 64 bit.
    ""
#endif
};



/* ---------------------------- jvm_get_config ---------------------------- */

NSAPI_PUBLIC const ServerXMLSchema::Jvm *jvm_get_config()
{
    const Configuration *configuration = ConfigurationManager::getConfiguration();
    if (!configuration)
        return NULL;

    const ServerXMLSchema::Jvm *jvm = configuration->getJvm();

    // We release our reference to the configuration.  As a result, the caller
    // must not rely on the returned ServerXMLSchema::Jvm * being valid outside
    // of Init.
    configuration->unref();

    return jvm;
}


/* --------------------------- jvm_get_libpath ---------------------------- */

 NSAPI_PUBLIC char *jvm_get_libpath(const ServerXMLSchema::Server& server)
{
    // Construct jvm object if configured or return NULL.
    const ServerXMLSchema::Jvm *jvm = server.getJvm();
    if (!jvm)
        return NULL;

#if !defined(HPUX) 
    // the libpath is returned as null if jvm is disabled except on HP-UX.
    // On HP-UX, the server binary is linked with the jvm at compile time, the
    // jvm is hence needed on the the library path even if it is disabled
    if (!jvm->enabled)
        return NULL;
#endif

    NSString path;
    int i;

    // Construct JRE paths within <java-home>
    if (platform_get_bitiness() == 64) {
        for (i = 0; i < sizeof(_jre_lib64paths) / sizeof(_jre_lib64paths[0]); i++) {
            path.append(jvm->javaHome);
            path.append(_jre_lib64paths[i]);
            path.append(LIBPATH_SEPARATOR);
        }
    } else {
        for (i = 0; i < sizeof(_jre_libpaths) / sizeof(_jre_libpaths[0]); i++) {
            path.append(jvm->javaHome);
            path.append(_jre_libpaths[i]);
            path.append(LIBPATH_SEPARATOR);
        }
    }

    // Append <native-library-path-prefix>
    if (strlen(jvm->nativeLibraryPathPrefix)) {
        path.append(jvm->nativeLibraryPathPrefix);
        path.append(LIBPATH_SEPARATOR);
    }

    // Append profiler lib path if any
    for (i = 0; i < jvm->getProfilerCount(); i++) {
        const ServerXMLSchema::Profiler& profiler = *jvm->getProfiler(i);
        if (profiler.enabled && strlen(profiler.nativeLibraryPath)) {
            path.append(profiler.nativeLibraryPath);
            path.append(LIBPATH_SEPARATOR);
        }
    }

    return path.length() ? STRDUP(path) : NULL;
}


/* --------------------------- jvm_set_libpath ---------------------------- */

NSAPI_PUBLIC void jvm_set_libpath(const ServerXMLSchema::Server& server)
{
    // On Windows, this function sets the PATH environment variable so that we
    // can find jvm.dll.  On Unix, ld.so.1 ignores any LD_LIBRARY_PATH changes
    // after process startup, so we rely on the start script to set up the
    // environment.

#if defined(XP_WIN32)
    // Set PATH based on server.xml's <jvm> element
    char *jvmPath = jvm_get_libpath(server);
    if (jvmPath) {
        NSString path;

        path.append("PATH=");

        path.append(jvmPath);

        const char *originalPath = getenv("PATH");
        if (originalPath)
            path.append(originalPath);

        putenv(PERM_STRDUP(path));

        FREE(jvmPath);
    }
#endif
}
