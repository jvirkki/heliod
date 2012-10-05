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

#include "ServerControl.h"
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <stdlib.h>
#include "prtypes.h"
#include "base/util.h"

#define CONFIG_DIR_NAME "config"

ServerControl::ServerControl(const char *instRoot,
                             const char *instanceRoot,
                             const char *instName,
                             const char *tempDir) {
    installRoot = instRoot;
    instanceName = instName;
    serverTempDir = tempDir;
    this->instanceRoot = instanceRoot;

    noBuffering = false;

    // Allocate an array to hold the path to the instance's config directory,
    // 2 extra '/'es and a '\0'
    cfgDir = new char[strlen(instanceRoot)+strlen(instanceName)+
                      strlen(CONFIG_DIR_NAME)+3];
    sprintf(cfgDir, "%s/%s/"CONFIG_DIR_NAME, instanceRoot, instanceName);
    const char *temp = system_get_temp_dir();
    if (!temp)
        temp = "/tmp";
    adminTempDir = strdup(temp);
}

ServerControl::~ServerControl() {
    if (cfgDir != NULL)
        delete [] cfgDir;
    if (adminTempDir)
        free(adminTempDir);
}

void ServerControl::bufferOutput(const char *fmt, ...) {
    char line[MAX_BUF_SIZE];
    va_list vargs;

    va_start(vargs, fmt);
    util_vsnprintf(line, MAX_BUF_SIZE-1, fmt, vargs);
    va_end(vargs);

    if (noBuffering)
        cout << line;
    else
        strout << line;
}

