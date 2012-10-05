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

#include "frame/log.h"
#include "constants.h"
#include "fastcgii18n.h"
#include "serverconfig.h"
#include "fastcgistub.h"
#include "stubexec.h"
#include "servermanager.h"

//-----------------------------------------------------------------------------
// FcgiServerManager::FcgiServerManager
//-----------------------------------------------------------------------------
FcgiServerManager::FcgiServerManager()
: lockFcgiServers(PR_NewLock()) { 
}

//-----------------------------------------------------------------------------
// FcgiServerManager::~FcgiServerManager
//-----------------------------------------------------------------------------
FcgiServerManager::~FcgiServerManager() {
    // Blow away all the pools
    FcgiServer *server;
    PRBool firstTime = PR_TRUE;
    while (server = fcgiServers.removeFromTail()) {
        delete server;
    }

    PR_DestroyLock(lockFcgiServers);
}

//-----------------------------------------------------------------------------
// FcgiServerManager::destroy
//-----------------------------------------------------------------------------
void FcgiServerManager::destroy(FcgiServer *server) {
    PR_Lock(lockFcgiServers);
    fcgiServers.remove(server);
    PR_Unlock(lockFcgiServers);
}

//-----------------------------------------------------------------------------
// FcgiServerManager::getPool
//-----------------------------------------------------------------------------
FcgiServer* FcgiServerManager::getFcgiServer(pblock *pb, const char *vsid, const char *tmpDir, 
            const char *pluginDir, const char *stubLogDir) {
    FcgiServer *server = NULL;

    // Lock fcgi server config list
    PR_Lock(lockFcgiServers);

    // Look for an existing FcgiServerPool with an identical FcgiServerConfig
    FcgiServerIterator iter = fcgiServers.begin();
    //while ((server = *iter) && server->config != config)
    while ((server = *iter) && ((*(server->config)) != pb))
        ++iter;

    // Create the FcgiServer if it doesn't already exist
    if (!server) {
        ereport(LOG_VERBOSE, "did not find an existing FastCGI application configuration; will create one !");
        FcgiServerConfig *config = new FcgiServerConfig(pb, vsid, tmpDir, pluginDir, stubLogDir);
        if(config->getLastError() != NO_FCGI_ERROR) {
            delete config;
            PR_Unlock(lockFcgiServers);
            return NULL;
        }
        server = FcgiServer::createFcgiServer(config);
        if (server) {
            fcgiServers.addToTail(server);
        }
    } else {
        ereport(LOG_VERBOSE, "found an existing FastCGI application configuration !");
    }

    // It is an error to have a pool without any servers
    if (!fcgiServers.getCount()) {
        ereport(LOG_MISCONFIG, GetString(DBT_no_servers_defined));
    }

    // Unlock fcgi server config list
    PR_Unlock(lockFcgiServers);

    return server;
}
