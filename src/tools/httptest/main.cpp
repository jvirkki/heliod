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
#include "log.h"
#include <time/nstime.h>
#include "tests.h"
#include "regex_string.h"
#include "regex_entry.h"
#include "regex_list.h"
#include "regex_scrubber.h"
#include "prio.h"
#include "utils.h"

#ifdef XP_UNIX
#include <sys/resource.h>
#endif
RegexScrubber* global=NULL;

#include "arch.h"

char* arch = ARCH;

void usage(char *argv0)
{
    fprintf(stdout, "-h <host:port>     specify the host:port of the server\n");
    fprintf(stdout, "-p <n>             to run n parallel threads for each test\n");
    fprintf(stdout, "-g <group>         to include tests belonging to a given group. Default: COMMON\n");
    fprintf(stdout, "-v <version>       version of the server to test. Default: ENTERPRISE\n");
    fprintf(stdout, "-R <release>       release of the server to test. Default: 41\n");
    fprintf(stdout, "-C <n>             to cap the maximum number of concurrent threads to n\n");
    fprintf(stdout, "-s                 to enable security\n");
    fprintf(stdout, "-E <protocol>      to enable a specific security protocol : SSL2, SSL3 or TLS.\n");
    fprintf(stdout, "-P                 to test performance\n");
    fprintf(stdout, "-c <cipher>        cipher suite to enable. All are enabled by default\n");
    fprintf(stdout, "-Q                 Print cipher list\n");
    fprintf(stdout, "-n <certname>      name of the client certificate to use\n");
    fprintf(stdout, "-k <certpwd>       key database password\n");
    fprintf(stdout, "-H <hsperiod>      set SSL handshake period. Default: infinite\n");
    fprintf(stdout, "-l <loglevel>      log level from 0 to 5\n");
    fprintf(stdout, "-d <suite>         directory location of test suite to use. Default : ./suite1\n");
    fprintf(stdout, "-x <test>          test case to execute, using regular expressions. Default : \".*\"\n");
    fprintf(stdout, "-X <test>          test case to exclude, using regular expressions\n");
    fprintf(stdout, "-r <x>             to repeat all tests x times. Default : 1\n");
    fprintf(stdout, "-a <ARCH>          OS Architecture. Default : %s\n", arch);
    fprintf(stdout, "-o <offset>        optional offset to split packets when sending requests\n");
    fprintf(stdout, "-t <time>          time in milliseconds to sleep between each send\n");
    fprintf(stdout, "-w                 log output from all test failures to disk in <suite>/output.\n");
    fprintf(stdout, "-e <timeout>       expiration timeout for socket send/receive. Default: 30s.\n");
    fprintf(stdout, "-T <maxtimeouts>   Maximum number of tests allowed to timeout. Default: infinite\n");
    fprintf(stdout, "-4                 Perform tests using IPv4 (default)\n");
    fprintf(stdout, "-6                 Perform tests using IPv6 localhost address ::1\n");
    fprintf(stdout, "-L <x>             Run tests in infinite loop and report stats every x seconds\n");
};

void printCipherOptions(void)
{
    // warning: actual cipher list is kept in tests.cpp
    printf(
           "A SSL_EN_RC4_128_WITH_MD5\n"
           "B SSL_EN_RC4_128_EXPORT40_WITH_MD5\n"
           "C SSL_EN_RC2_128_CBC_WITH_MD5\n"
           "D SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5\n"
           "E SSL_EN_DES_64_CBC_WITH_MD5\n"
           "F SSL_EN_DES_192_EDE3_CBC_WITH_MD5\n"

           "\n"

           "a N/A\n"
           "b N/A\n"
           "c SSL_RSA_WITH_RC4_128_MD5\n"
           "d SSL_RSA_WITH_3DES_EDE_CBC_SHA\n"
           "e SSL_RSA_WITH_DES_CBC_SHA\n"
           "f SSL_RSA_EXPORT_WITH_RC4_40_MD5\n"
           "g SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5\n"
           "h N/A\n"
           "i SSL_RSA_WITH_NULL_MD5\n"
           "j SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA\n"
           "k SSL_RSA_FIPS_WITH_DES_CBC_SHA\n"
           "l TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA\n"
           "m TLS_RSA_EXPORT1024_WITH_RC4_56_SHA\n"
           "n TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA\n"
           "o TLS_ECDHE_RSA_WITH_RC4_128_SHA\n"
           "p TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA\n"
           "q TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA\n"
           "r TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA\n"
           "s TLS_ECDH_ECDSA_WITH_RC4_128_SHA\n"
           "t TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA\n"
           "u TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA\n"
           );
    exit(-1);
};

#ifdef XP_UNIX
void unlimit()
{
   rlimit lim;
   getrlimit(RLIMIT_NOFILE, &lim);
   lim.rlim_cur = lim.rlim_max;
   setrlimit(RLIMIT_NOFILE, &lim);
}
#endif

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
#ifdef XP_UNIX
    unlimit();
#endif
    fprintf(stdout, "\n");

    unsigned long limit = 0; // no thread limit by default
    char *addr = NULL;
    PRBool log = PR_FALSE;
    LogLevel logLevel = LOGINFO;
    PLOptState *options;
    PRBool secure = PR_FALSE;
    PRBool NSTests = PR_FALSE; // don't execute Netscape tests by default
    PtrList<char> protlist; // list of security protocols
    PtrList<char> configlist; // list of configurations
    RegexList regexlist; // list of Sun tests/regex to load
    RegexList regexcludelist; // list of Sun tests/regex to exclude
    char* suitename = "suite1";
    PRInt32 concurrent = 0; // number of concurrent threads for each test. 0 means sequential, single-threaded
    PRInt32 delay = 0;
    PRInt32 split = 0;
    PRInt32 timeout = 0; // leave timeout unchanged by default
    char* cert=NULL;
    char* certpwd=NULL;
    char* cipherString = NULL;
    char* version = "ENTERPRISE";
    PRInt32 release = 41;
    PRInt32 hsp = 0; // SSL handshake period
    PRBool performance = PR_FALSE;
    PRInt32 maxtm = 0;
    PRUint16 af = PR_AF_INET;
    PRInt32 displayperiod=0;
    PRBool loop = PR_FALSE;

    Logger::logInitialize(logLevel);

    options = PL_CreateOptState(argc, argv, "X:C:h:H:l:c:d:n:w46r:sx:p:o:t:a:e:k:Ng:v:R:QPE:T:L:");
    long repeat = 1;
    while ( PL_GetNextOpt(options) == PL_OPT_OK)
    {
        switch(options->option)
        {
            case 'L':
                loop = PR_TRUE;
                if (options->value)
                    displayperiod  = (PRInt32) atoi(options->value);
                break;

            case 'E':
                if (options->value)
                    protlist.insert(strdup(options->value));
                break;

            case 'T':
                if (options->value)
                    maxtm  = (PRInt32) atoi(options->value);
                break;

            case 'H':
                if (options->value)
                    hsp  = (PRInt32) atoi(options->value);
                break;

            case 'v':
                if (options->value)
                    version = uppercase(strdup(options->value));
                break;

            case 'g':
                if (options->value)
                    configlist.insert(strdup(options->value));
                break;

            case 'x':
                if (options->value)
                    regexlist.add(options->value);
                break;

            case 'X':
                if (options->value)
                    regexcludelist.add(options->value);
                break;

            case 'w':
                log = PR_TRUE;
                break;

            case 'r':
                if (options->value)
                    repeat = atol(options->value);
                break;

            case 'e':
                if (options->value)
                    timeout = atol(options->value);
                break;

            case 'o':
                if (options->value)
                    split = atol(options->value);
                break;

            case 't':
                if (options->value)
                    delay = atol(options->value);
                break;

            case 'd':
                if (options->value)
                   suitename = strdup(options->value);
                break;

            case 'a':
                if (options->value)
                    arch = strdup(options->value);
                break;

            case 'N':
                NSTests = PR_TRUE;
                break;

            case 'h':
                if (options->value)
                    addr = strdup(options->value);
                break;

            case 'p':
                if (options->value)
                    concurrent = atol(options->value);
                else
                    concurrent = 1; // 1 thread per test
                break;

            case 'P':
                performance = PR_TRUE; // meaure performance only
                break;

            case 'l':
                if (options->value)
                    logLevel = (LogLevel)atoi(options->value);
                break;

            case 'R':
                if (options->value)
                    release = (PRInt32) atoi(options->value);
                break;

            case 's':
                secure = PR_TRUE;
                break;

            case 'n':
                if (options->value)
                    cert = strdup(options->value);
                break;

            case 'k':
                if (options->value)
                    certpwd = strdup(options->value);
                break;

            case 'c':
		if (options->value) {
                  cipherString = strdup(options->value);
                  if (PR_TRUE != EnableCipher(cipherString))
                  {
                         Logger::logError(LOGINFO, "Invalid cipher specified.\n");
                   };
                 }
 		 break;


            case 'C':
                if (options->value)
                    limit = atol(options->value);
                else
                    limit = 0; // no thread limit
                break;

	    case 'Q':
		printCipherOptions();
		break;

	    case '6':
                af = PR_AF_INET6;
		break;

	    case '4':
                af = PR_AF_INET;
		break;

        };
    };

    SecurityProtocols secprots;

    if (PR_TRUE == secure)
    {
        NSString str;
        str.append(suitename);
        str.append("/certs/client");
        secure = InitSecurity((char*)str.data(),
            cert,
            certpwd);
        if (PR_TRUE != secure)
            Logger::logError(LOGINFO, "Unable to initialize security.\n");

        if (protlist.entries())
        {
            secprots = protlist;
        };
    };

    PL_DestroyOptState(options);

    Logger::logInitialize(logLevel);
    nstime_init();

    if (!addr)
    {
        usage(argv[0]);
        return -1;
    };

    HttpServer server(addr, af);
    server.setSSL(secure);
    
    if (PR_FALSE == NSTests)
    {
        if (alltests)
            alltests->clear(); // cancel all the Netscape tests
        if (!regexlist.length())
            regexlist.add(".*");
    };

    if (!configlist.entries())
        configlist.insert("COMMON"); // if no config is specified, select default COMMON configuration

    Engine::globaltimeout = PR_TicksPerSecond()*timeout;

    SunTestSuite suite(configlist, suitename, regexlist, regexcludelist, arch, version, release, log, PR_TicksPerSecond()*timeout, split, delay, hsp, secprots, maxtm);
    PRInt32 percent = suite.runTests(server, concurrent, repeat, limit, performance, loop, displayperiod);

    return percent;
};

