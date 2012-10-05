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


#ifndef WIN32
#include <unistd.h>
#endif

#include <stdio.h>
#include <nspr.h>
#include <plgetopt.h>
#include "http.h"
#include "engine.h"
#include "testthread.h"
#include "log.h"
#include "nstime/nstime.h"

// --- define the tests
extern test_t dynamic1A;
extern test_t dynamic1B;
extern test_t dynamic1C;
extern test_t dynamic1D;
extern test_t dynamic1E;


test_t *testList[] = { 
                     &dynamic1A,
                     &dynamic1B,
                     &dynamic1C,
                     &dynamic1D,
                     &dynamic1E,
                     0 };
#define NUMTESTS ((sizeof(testList)/sizeof(test_t *))-1)


void 
threadMain(void *_arg)
{
    char myName[128];
    TestArg *arg = (TestArg *)_arg;
    sprintf(myName, "%s: %s", HttpProtocolToString(arg->proto), arg->test->name);
    Logger::setThreadName(myName);
    arg->test->func(arg);
}

int
runSerialTests(HttpServer *server, char *uri)
{
    int index;
    int proto, base;
    

    for (index=0; index < NUMTESTS; index++) {
        for (base=1,proto=testList[index]->testProtocols; proto; proto >>= 1, base*=2) {
            if (proto & 0x1) {
                PRThread *testThread;
                TestArg arg;
                arg.test = testList[index];
                arg.server = server;
                arg.proto = (HttpProtocol_e)(base);
                arg.uri = uri;

                testThread = PR_CreateThread(PR_USER_THREAD,
                                             threadMain,
                                             &arg,
                                             PR_PRIORITY_NORMAL,
                                             PR_GLOBAL_THREAD,
                                             PR_JOINABLE_THREAD,
                                             0);

                PR_JoinThread(testThread);
                if (arg.returnVal != 0) {
                    Logger::logError(LOGFAILURE, "test %s: %s failed", HttpProtocolToString(arg.proto), arg.test->name);
                } else {
                    Logger::logError(LOGPASS, "test %s: %s passed", HttpProtocolToString(arg.proto), arg.test->name);
                }
            }
        }
    }

    return 0;
}

int
runParallelTests(HttpServer *server, char *uri)
{
    TestArg arg[NUMTESTS];
    PRThread *testThread[NUMTESTS];
    int index;

    for (index=0; index<NUMTESTS; index++) {
        arg[index].test = testList[index];
        arg[index].server = server;
        arg[index].uri = uri;
        testThread[index] = PR_CreateThread(PR_USER_THREAD,
                                     threadMain,
                                     &(arg[index]),
                                     PR_PRIORITY_NORMAL,
                                     PR_GLOBAL_THREAD,
                                     PR_JOINABLE_THREAD,
                                     0);
    }

    for (index=0; index<NUMTESTS; index++) {
        PR_JoinThread(testThread[index]);
        if (arg[index].returnVal != 0) {
            Logger::logError(LOGFAILURE, "test %s failed", arg[index].test->name);
        } else {
            Logger::logError(LOGPASS, "test %s passed", arg[index].test->name);
        }
    }

    return 0;
}


void
usage(char *argv0)
{
    fprintf(stdout, "%s -h <host:port> [-s] [-l <level>] -u URI\n", argv0);
    fprintf(stdout, "\t -h specify the host:port of the server\n");
    fprintf(stdout, "\t -p run the tests in parallel\n");
    fprintf(stdout, "\t -l log level\n");
    fprintf(stdout, "\t URI is the URI for the dynamic content\n");
}

int
main(int argc, char **argv)
{
    char *addr = NULL;
    char *uri = NULL;
    PRBool serialize = PR_TRUE;
    LogLevel logLevel = LOGINFO;
    HttpServer *server;
    PLOptState *options;
    int rv;

    options = PL_CreateOptState(argc, argv, "h:pl:u:");
    while ( PL_GetNextOpt(options) == PL_OPT_OK) {
        switch(options->option) {
           case 'h':
              addr = strdup(options->value);
              break;
           case 'p':
              serialize = PR_FALSE;
              break;
           case 'l':
              logLevel = (LogLevel)atoi(options->value);
              break;
           case 'u':
              uri = strdup(options->value);
              break;
        }
    }
    PL_DestroyOptState(options);

    Logger::logInitialize(logLevel);
    nstime_init();

    if (!addr || !uri) {
        usage(argv[0]);
        return -1;
    }

    server = new HttpServer(addr);

    if (serialize)
        rv = runSerialTests(server, uri);
    else
        rv = runParallelTests(server, uri);

    delete server;

    Logger::logDump();

    return 0;
}
