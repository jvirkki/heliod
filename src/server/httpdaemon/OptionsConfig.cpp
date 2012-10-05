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


#include <string.h>                          // strdup(), memcmp(), memset()
#include "base/ereport.h"                    // LOG_*
#include "base/net.h"
#include "frame/conf.h"
#include "httpdaemon/WebServer.h"            // WebServer::Log()
#include "httpdaemon/OptionsConfig.h"        
#include "httpdaemon/JvmOptions.h"           // JvmOptions class


OptionsConfig::OptionsConfig(ConfigurationObject* parent):ConfigurationObject(parent) {
    jvmOptions = new const char*[MAX_JVM_OPTIONS];
    numOptions = 0;

}

OptionsConfig::~OptionsConfig() {
    for (int i = 0; i < numOptions; i++) {
        if (jvmOptions[i])
            free ((void*)jvmOptions[i]);
    }
    delete[] jvmOptions;
}


void
OptionsConfig::addJvmOptions(const char* option) {
    if (numOptions < MAX_JVM_OPTIONS)
        jvmOptions[numOptions++] = strdup(option);
    
}

const char** 
OptionsConfig::getJvmOptions(int& options) {
    options =  numOptions;
    return jvmOptions;
}
