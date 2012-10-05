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

#include <stdio.h>
#include <stdlib.h>
#ifdef LINUX
#include <getopt.h>
#elif XP_WIN32
#include "wingetopt.h"
#endif
#include "ServerControlHelper.h"

#define RECONFIG 1
#define RESTART  2

char *installRoot = NULL;
char *instanceRoot = NULL;
char *instanceName = NULL;
char *tempDir = NULL;

int reconfigServer(ServerControl *controller) {
    int ret = 0;
    try {
        if (!controller->reconfigServer()) {
            ret = 99;
        }
    } catch (RestartRequired& restartReq) {
        ret = 98;
    }
    return ret;
}

int main(int argc, char **argv) {
    installRoot = getenv("WS_INSTALLROOT");
    instanceRoot = getenv("WS_INSTANCEROOT");

    int o;
    int action = 0;
    while((o = getopt(argc, argv, "n:t:cs")) != -1)  {
        switch(o)  {
        case 'n':
            instanceName = optarg;
            break;
        case 't':
            tempDir = optarg;
            break;
        case 'c':
            action = RECONFIG;
            break;
        case 's':
            action = RESTART;
            break;
        }
    }
    if (action == 0 || !instanceName) {
        // XXX: printUsage();
        printf("Usage: svrctl [-c | -s] -n <instance_name> -t <temp_path_of_instance>\n");
        return 1;
    }

    if (!tempDir)
        tempDir = "";
    ServerControl *controller =
        ServerControlHelper::getServerControl(installRoot, instanceRoot, instanceName, tempDir);
    controller->setBuffering(false);
    int ret = 0;
    switch (action) {
    case RECONFIG:
        ret = reconfigServer(controller);
        break;
    case RESTART: /* Do nothing for now */
        break;
    }
    delete controller;
    return ret;
}
