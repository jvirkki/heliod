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
#include "xercesc/util/PlatformUtils.hpp"
#include "xalanc/XPath/XPathEvaluator.hpp"
#include "support/NSString.h"
#include "support/EreportableException.h"
#include "generated/ServerXMLSchema/Server.h"
#include "generated/ServerXMLSchema/Platform.h"
#include "libserverxml/ServerXML.h"
#include "base/dll.h"
#include "base/ereport.h"
#include "base/platform.h"
#include "httpdaemon/JavaConfig.h"

using namespace XERCES_CPP_NAMESPACE;
using namespace XALAN_CPP_NAMESPACE;

static void usage()
{
    fprintf(stderr, "Usage: parsexml path -g option\n");
    fprintf(stderr, "       parsexml path\n");
}

static void invalidOption(const char *option)
{
    fprintf(stderr, "%s: invalid option\n", option);
    fprintf(stderr, "Options: PID_FILE\n");
    fprintf(stderr, "         TEMP_DIR\n");
    fprintf(stderr, "         USER\n");
    fprintf(stderr, "         JVM_LIBPATH\n");
    fprintf(stderr, "         PLATFORM_SUBDIR\n");
}

static const char *getJvmLibpath(const ServerXMLSchema::Server& server)
{
    const char *path = jvm_get_libpath(server);
    if (path)
        return path;
    return "";
}

static void set64bitPlatform(const char *platform)
{
    if (!platform || !strcmp(platform, "32"))
        return;

    // Construct the path to the platform/true binary
    NSString path;
    const char *library = dll_library();
    const char *slash = strrchr(library, '/');
#ifdef XP_WIN32
    const char *backslash = strrchr(library, '\\');
    if (backslash > slash)
        slash = backslash;
#endif
    if (slash)
        path.append(library, slash - library + 1);
    path.append(platform);
    path.append("/true");

    // Suppress errors caused by running a 64-bit binary on a 32-bit OS
    int oldStdErrFd = dup(fileno(stderr));
    int nullFd = open(FILE_DEV_NULL, O_WRONLY);
    dup2(nullFd, fileno(stderr));
    close(nullFd);

    // Try to execute platform/true
    int rv = system(path);

    // Restore stderr
    dup2(oldStdErrFd, fileno(stderr));
    close(oldStdErrFd);

    // Use 64-bit platform only if the OS was able to execute platform/true
    if (rv == 0)
        platform_set(platform, 64);
}

static void putNameValue(PRBool echoName, const char *name, const char *value)
{
    NSString line;

    if (echoName) {
        line.append(name);
        line.append('=');
    }

    int len = value ? strlen(value) : 0;
    for (int i = 0; i < len; i++) {
        switch (value[i]) {
        case '\r':
        case '\n':
        case '\t':
        case '\\':
        case '\"':
        case '\'':
        case ';':
        case ' ':
            line.append('\\');
            break;
        }
        line.append(value[i]);
    }

    puts(line);
}

int main(int argc, const char *argv[])
{
    PRBool echoName = PR_FALSE;
    PRBool needPidFile = PR_FALSE;
    PRBool needTempDir = PR_FALSE;
    PRBool needUser = PR_FALSE;
    PRBool needJvmLibPath = PR_FALSE;
    PRBool needPlatformSubDir = PR_FALSE;

    if (argc == 2) {
        echoName = PR_TRUE;
        needPidFile = PR_TRUE;
        needTempDir = PR_TRUE;
        needUser = PR_TRUE;
        needJvmLibPath = PR_TRUE;
        needPlatformSubDir = PR_TRUE;
    } else if (argc == 4 && !strcmp(argv[2], "-g")) {
        if (!strcmp(argv[3], "PID_FILE")) {
            needPidFile = PR_TRUE;
        } else if (!strcmp(argv[3], "TEMP_DIR")) {
            needTempDir = PR_TRUE;
        } else if (!strcmp(argv[3], "USER")) {
            needUser = PR_TRUE;
        } else if (!strcmp(argv[3], "JVM_LIBPATH")) {
            needJvmLibPath = PR_TRUE;
        } else if (!strcmp(argv[3], "PLATFORM_SUBDIR")) {
            needPlatformSubDir = PR_TRUE;
        } else {
            invalidOption(argv[3]);
            return 1;
        }
    } else {
        usage();
        return 1;
    }

    XMLPlatformUtils::Initialize();

    XPathEvaluator::initialize();

    NSString filename;
    filename.append(argv[1]);
    filename.append("/");
    filename.append(SERVERXML_FILENAME);

    ServerXML *serverXML = NULL;
    ServerXMLSummary *serverXMLSummary = NULL;
    try {
        if (needJvmLibPath) {
            // Caller wants the JVM library path, so we need a full ServerXML
            serverXML = ServerXML::parse(filename.data());
            serverXMLSummary = serverXML;
        } else {
            // Caller wants basic information, so we can use a ServerXMLSummary
            serverXMLSummary = ServerXMLSummary::parse(filename.data());
        }
    }
    catch (const EreportableException& e) {
        ereport_exception(e);
        return 1;
    }

    set64bitPlatform(serverXMLSummary->getPlatform());

    if (needPidFile)
        putNameValue(echoName, "SERVER_PID_FILE", serverXMLSummary->getPIDFile());
    if (needTempDir)
        putNameValue(echoName, "SERVER_TEMP_DIR", serverXMLSummary->getTempPath());
    if (needUser)
        putNameValue(echoName, "SERVER_USER", serverXMLSummary->getUser());
    if (needJvmLibPath)
        putNameValue(echoName, "SERVER_JVM_LIBPATH", getJvmLibpath(serverXML->server));
    if (needPlatformSubDir)
        putNameValue(echoName, "SERVER_PLATFORM_SUBDIR", platform_get_subdir());

    return 0;
}
