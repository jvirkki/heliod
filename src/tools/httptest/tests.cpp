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

#define TESTS_IMPLEMENTATION
#include "tests.h"
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <support/NSString.h>
#include "utils.h"
#include "log.h"
#include <sslerr.h>
#include "sslproto.h"
#include "nss.h"

PRInt32 SunTest :: timeouts = 0;

PRIntervalTime SunTestSuite :: slowthreshold = PR_SecondsToInterval(2);


// warning: -Q help list kept separately in main.cpp, need manual sync

int ssl2CipherSuites[] = {
    SSL_EN_RC4_128_WITH_MD5,                    /* A */
    SSL_EN_RC4_128_EXPORT40_WITH_MD5,           /* B */
    SSL_EN_RC2_128_CBC_WITH_MD5,                /* C */
    SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5,       /* D */
    SSL_EN_DES_64_CBC_WITH_MD5,                 /* E */
    SSL_EN_DES_192_EDE3_CBC_WITH_MD5,           /* F */
    0
};

int ssl3CipherSuites[] = {
    SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA,     /* a */
    SSL_FORTEZZA_DMS_WITH_RC4_128_SHA,          /* b */
    SSL_RSA_WITH_RC4_128_MD5,                   /* c */
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,              /* d */
    SSL_RSA_WITH_DES_CBC_SHA,                   /* e */
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,             /* f */
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,         /* g */
    SSL_FORTEZZA_DMS_WITH_NULL_SHA,             /* h */
    SSL_RSA_WITH_NULL_MD5,                      /* i */
    SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA,         /* j */
    SSL_RSA_FIPS_WITH_DES_CBC_SHA,              /* k */
    TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA,        /* l */
    TLS_RSA_EXPORT1024_WITH_RC4_56_SHA,         /* m */

    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,         /* n */
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,             /* o */
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,        /* p */
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,         /* q */

    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,        /* r */
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,            /* s */
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,       /* t */
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,        /* u */

    0
};


PRLock* listLock = NULL;
PtrList<TestInstance>* instanceList;
PRBool keypressed = PR_FALSE;

extern PRBool globalCert;

Test :: Test(const char* desc, long prots) : protocols(prots), instnum(0)
{
    if (desc)
        textdescription=strdup(desc);
    else
        textdescription=NULL;
    ready = PR_FALSE;
    instance = NULL;
    tid = 0;
};

Test :: ~Test()
{
    if (textdescription)
        free((void*)textdescription);
};

NetscapeTest :: NetscapeTest(const char* desc, long prots) : Test(desc, prots)
{
    request = NULL;
    response = NULL;
};

NetscapeTest :: ~NetscapeTest()
{
    if (request)
        delete request;

    if (response)
        delete response;
};

const char* Test :: description() const
{
    return textdescription;
};

const long Test :: supportedProtocols() const
{
    return protocols;
};

TestInstance :: TestInstance (TestItem& atest, HttpServer& aserver, HttpProtocol aprotocol, PRBool perf, PRInt32 conc) : 
        test(*(atest.newTest())), server(aserver), protocol(aprotocol), concurrency(conc)
{
    performance = perf;
    uri = NULL;
    thread = NULL;
    setStatus(PR_FAILURE); // defaults to failure
    instance_success = instance_fail = instance_attempts = attempts = passes = successes = failures = 0;
    instance_elapsed = 0;
    tid = 0;
};

PRBool TestInstance :: operator==(const TestInstance& rhs) const
{
    return PR_FALSE; // all instances are unique, therefore they can never be equal
};

TestInstance :: ~TestInstance()
{
    if (uri)
        free((void*)uri);
    if (&test)
        delete &test;
};

const HttpServer& TestInstance :: myServer() const
{
    return server;
};

const Test& TestInstance :: myTest() const
{
    return test;
};

const HttpProtocol TestInstance :: myProtocol() const
{
    return protocol;
};

void TestInstance :: execute_test()
{
    test.setInstance(this);

    if (PR_TRUE != test.isReady())
            test.setup();

    PRIntervalTime elapsed = 0;

    PRInt32 i;
    test.setThreadId(tid);
    for (i=0;(i<passes) || (0==passes);i++)
    {
        setStatus(PR_FAILURE);
        test.setInstnum(1+i);
        if (passes>0)
            Logger::logError(LOGTRACE, "Running %s thread %d, pass %d",  myTest().description(), getThreadId(), 1+i);

        PRIntervalTime start = PR_IntervalNow();
        test.run();
        PRIntervalTime stop = PR_IntervalNow();

        PR_AtomicIncrement(&attempts);
        if (PR_SUCCESS == getStatus())
            PR_AtomicIncrement(&successes);
        else
            PR_AtomicIncrement(&failures);

        PRIntervalTime t = elapsed + (stop - start);
        if (t > elapsed)
            elapsed = t;
    };

    if (-1 == passes)
    {
        // will only get here if the test was running in a loop which was aborted by the user
        instance_attempts += attempts;
        instance_success += successes;
        instance_fail += failures;
    };

    instance_elapsed = elapsed;

    PR_ASSERT(instance_attempts == (instance_success + instance_fail) );

    if ( (instance_attempts>0) &&
         (instance_success>0) &&
         (0 == instance_fail)
         )
    {
        setStatus(PR_SUCCESS);
    }
    else
    {
        setStatus(PR_FAILURE);
    };
};

void TestInstance :: setRepeat(PRInt32 runs)
{
    passes = runs;
};

PRStatus TestInstance :: getStatus()
{
    return status;
};

PRInt32 TestInstance :: getPasses() const
{
    return passes;
};

void TestInstance :: setStatus(PRStatus stat)
{
    status = stat;
};

void TestInstance :: setMode (execmode md)
{
    mode = md;
};

TestInstance::execmode TestInstance :: getMode ()
{
    return mode;
};

PRBool Test :: setup()
{
    ready = PR_TRUE;
    return ready;
};

void Test :: setInstance(TestInstance* inst)
{
    instance = inst;
};

const PRBool Test :: isReady() const
{
    return ready;
};

void Test :: setDescription(const char* desc)
{
    if (desc)
    {
        if (textdescription)
            free((void*)textdescription);
        textdescription = strdup(desc);
    };
};

SunTestDefinition :: SunTestDefinition(const char* fname, const char* fpath,
                    SunTestSuite& testsuite, SunTestProperties* prop) :
                    BaseTestDefinition(fname, fpath, testsuite, prop),
                    cipherCount(0), nickName(NULL), mutexName(NULL),
                    noSSL(PR_FALSE), input(NULL), gold(NULL), inlen(0),
                    goldlen(0), ofs(0), del(0), properties(prop),
                    timeoutperiod(testsuite.getTimeout()), sslerr(0),
                    hsperiod(testsuite.getHsperiod()),
                    secprots(testsuite.getSecprotocols())
{
    Process();
    if (PR_FALSE == enabled)
        return; // nothing to do

    if (0 == timeoutperiod)
        timeoutperiod = 30*PR_TicksPerSecond(); // default 30 second timeout
    
    // now process the request with global regex
    if (testsuite.getDatCleaner() && input && inlen)
    {
        char* returned;
        int retlen;
        testsuite.getDatCleaner()->scrub_buffer((char*)input, inlen, returned, retlen);
        if (retlen && returned)
        {
            free(input);
            input = returned;
            inlen = retlen;
        };
    };
    
    // now process the dat with specific regex
    if (properties && input && inlen )
    {
        SunTestProperties* prop = properties;
    
        while (prop)
        {
            if (prop->dat_cleaner)
            {
                // process request
                char* temporary=NULL;
                int templen=0;
                if (input && inlen)
                    prop->dat_cleaner->scrub_buffer((char*)input, inlen, temporary, templen);
                if (temporary && templen)
                {
                    free(input);
                    input = temporary;
                    inlen = templen;
                };
            };
            prop = prop->parent;
        };
    };

    NSString outfname;
    if (fpath && 0!=strcmp(fpath, ""))
    {
        outfname.append(fpath);
        outfname.append("/");
    };
    outfname.append(file, strlen(file)-4);
    outfname.append(".res");
    PRFile goldfile(outfname, PR_RDONLY, 0);
    goldfile.read(gold, goldlen);
    
    // now process the res with global regex
    if (gold && goldlen && testsuite.getResCleaner())
    {
        char* returned;
        int retlen;
        testsuite.getResCleaner()->scrub_buffer((char*)gold, goldlen, returned, retlen);
        if (retlen && returned)
        {
            free(gold);
            gold = returned;
            goldlen = retlen;
        };
    };
    
    // now process the res with specific regex
    if (properties && gold && goldlen)
    {
        SunTestProperties* prop = properties;
    
        while (prop)
        {
            if (prop->res_cleaner)
            {
                // process reply
                char* temporary=NULL;
                int templen=0;
                if (input && inlen)
                    prop->res_cleaner->scrub_buffer((char*)gold, goldlen, temporary, templen);
                if (temporary && templen)
                {
                    free(gold);
                    gold = temporary;
                    goldlen = templen;
                };
            };
            prop = prop->parent;
        };
    };
};

SunTestDefinition :: ~SunTestDefinition()
{
    if (nickName)
        free(nickName);

    if (mutexName)
        free(mutexName);

    if (gold)
        free(gold);

    if (input)
        free(input);
};

SunTest :: SunTest() : Test("Undefined SWS Test", HTTP11)
{
    definition = NULL;
    request = NULL;
    response = NULL;
    failed = PR_FALSE;
};

SunTest :: ~ SunTest()
{
    if (request)
        delete request;

    if (response)
        delete response;
};

PRBool SunTest :: define(SunTestDefinition* def)
{
    definition = def;
    NSString desc;
    if (def && def->getFilename())
    {
        desc.append(def->getFilename() );
        desc.append(" ");
    };

    if (def && def->getDescription())
        desc.append(def->getDescription());

    setDescription(desc.data());

    return PR_TRUE;
};

PRBool SunTest :: run() // test execution
{
    if (!definition)
        return PR_FALSE;

    if (PR_TRUE == timeoutsExceeded())
    {
        return PR_FALSE;
    };

    if (!definition->isGood()) {
        printf("error: description or CR info missing - test cannot run.\n");
        return PR_FALSE;
    }

    NSString fname;
    fname.append(definition->getSuitepath());
    fname.append("/output/");
    fname.append(definition->getFilename());
    NSString basename = fname;
    if (tid)
    {
        fname.append(".t");
        char str[32];
        sprintf(str, "%d", tid);
        fname.append(str);
    };

    // code to execute Sun test here
    const HttpServer& server = instance->myServer();
    SunEngine engine;
    
    PRBool forcehs = PR_FALSE ; // the default is to use the session cache and not force a handshake

    request = new SunRequest(&server, definition->getInput(), definition->getInputLength(), 
                             definition->getTimeout(), definition->getCipherSet(), 
                             definition->getCipherSetCount(), definition->getNickName(),
                             definition->getMutexName(), definition->getNoSSL(),
                             forcehs, definition->getSecprotocols(),
                             definition->getFilename());

    if (!request)
        return PR_FALSE;

    PRInt32 ofs = 0, del =0;
    definition->getSplit(ofs, del);
    request->setSplit(ofs, del);

    if (PR_TRUE == instance->getPerformance())
        engine.setPerformance(PR_TRUE);

    response = engine.makeRequest(*request, server, (0==definition->sslerr)?PR_TRUE:PR_FALSE);

    PRInt32 ecode = PR_GetError();
    PRInt32 oscode = PR_GetOSError();

    if (!response)
    {
        // request failed
        char *buffer = NULL;
        PRInt32 len = PR_GetErrorTextLength();
        if (len)
            buffer = (char *) malloc(len+1);

        PRInt32 status = PR_FAILURE;
        if (buffer)
        {
            status = PR_GetErrorText(buffer);
            *(buffer+len) = '\0';
        };

        if (PR_SUCCESS != status)
        {
            if (buffer)
                free(buffer);
            buffer = (char*) nscperror_lookup(ecode);
        };

        if (ecode == definition->sslerr)
        {
            instance->setStatus(PR_SUCCESS);
            return PR_TRUE;
        }

        Logger::logError(LOGERROR, "%s thr. %d pass %d - NSPR rc = %d , OS rc = %d\n       %s", 
            definition->getFilename(), instance->getThreadId() + 1, instnum, ecode, oscode, buffer);

        if (buffer && PR_SUCCESS == status)
            free(buffer);
    }
    else
    if (PR_FALSE == instance->getPerformance())
    {
        // functional test
        PRInt32 length = 0;
        void* buf = NULL;
        response->getData(buf, length);

        // need to process the response with HTTP chunk decoder here
        char* unchunked=NULL;
        int unchunkedlen=0;
        if (buf && length && definition && definition->getSuite().getServerCleaner())
            HttpDecoder().decode((char*)buf, length, unchunked, unchunkedlen);

        // need to process the response with regular expressions here
        char* returned=NULL;
        int retlen=0;
        if (unchunked && unchunkedlen && definition && definition->getSuite().getServerCleaner())
            definition->getSuite().getServerCleaner()->scrub_buffer((char*)unchunked, unchunkedlen, returned, retlen);

        if (returned && retlen && definition->getProperties())
        {
            const SunTestProperties* prop = definition->getProperties();
            while (prop)
            {
                if (prop->server_cleaner)
                {
                    // process server output
                    char* temporary=NULL;
                    int templen=0;
                    if (returned && retlen)
                        prop->server_cleaner->scrub_buffer((char*)returned, retlen, temporary, templen);
                    if (temporary && templen)
                    {
                        free(returned);
                        returned = temporary;
                        retlen = templen;
                    };
                };
                prop = prop->parent;
            };
        };

        // now compare it with the gold
        if ( (definition->getGoldLength() == retlen) && (definition->getGold()) && (returned>=0) &&
                 (0 == memcmp(returned, definition->getGold(), retlen)) )
        {
            // got the expected response
            if (instance)
                instance->setStatus(PR_SUCCESS);
        }
        else
        {
            if (definition && PR_TRUE == definition->getSuite().getLog() )
            {
                NSString tmp;
                tmp = fname;
                tmp.append(".failure");

                char str[32];
                sprintf(str, "%d", instnum);
                tmp.append(str);

                PRFile newfile(tmp, PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE, 0xFFFFFFFF);
                if (buf && length)
                    newfile.write( buf, length);

                tmp.append(".processed");

                PRFile procfile(tmp, PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE, 0xFFFFFFFF);
                if (returned && retlen)
                {
                    procfile.write( returned, retlen);
                };

                tmp = fname;
                tmp.append(".failure");
                tmp.append(str);
                tmp.append(".unchunked");

                PRFile unchunkedfile(tmp, PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE, 0xFFFFFFFF);
                if (unchunked && unchunkedlen)
                {
                    unchunkedfile.write( unchunked, unchunkedlen);
                };
            };
        };

        if (returned)
            delete returned;

        if (unchunked)
            free(unchunked);

        delete(response);
        response = NULL;
    }
    else
    {
        // performance test
        instance->setStatus(PR_SUCCESS);        // defaults to success because we got a reply
        delete(response);
        response = NULL;
    };

    if (instance->getStatus() != PR_SUCCESS && definition && PR_TRUE == definition->getSuite().getLog() && PR_FALSE == failed)
    {
        failed = PR_TRUE;
        
        NSString req;
        req = basename;
        req.append(".request");

        PRTime creationtime;

        // check if the request file already exists and if it is from a previous run
        {
            PRFile rqx(req, PR_RDONLY, 0xFFFFFFFF);
            creationtime = rqx.GetFileInfo().creationTime;
        }

        if (creationtime<origin)
        {
            // the file does not exist yet or is old, overwrite it
            PRFile reqfile(req, PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE, 0xFFFFFFFF);
            if (definition->getInput() && definition->getInputLength())
                reqfile.write(definition->getInput(), definition->getInputLength());
        };

        NSString res;
        res = basename;
        res.append(".expected");

        // check if the expect file already exists and if it is from a previous run
        {
            PRFile expected(res, PR_RDONLY, 0xFFFFFFFF);
            creationtime = expected.GetFileInfo().creationTime;
        }

        if (creationtime<origin)
        {
            // the file does not exist yet or is old, overwrite it
            PRFile resfile(res, PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE, 0xFFFFFFFF);
            if ( (definition->getGoldLength()) && (definition->getGold()) )
                resfile.write(definition->getGold(), definition->getGoldLength());
        };
    };

    delete request;
    request = NULL;

    if ( (PR_FAILURE == instance->getStatus()) && 
         ( (PR_IO_TIMEOUT_ERROR == ecode) ||
           (PR_CONNECT_REFUSED_ERROR == ecode) ) )
    {
        // record a timeout if the test failed and there was an NSPR timeout or
        // connection refused error. After a certain number of these errors
        // is exceeded, the tests will abort
        recordTimeout();
    };

    return PR_TRUE;
};

enableSunTest :: enableSunTest(SunTestDefinition* def)
{
    if (def && (PR_TRUE == def->isEnabled() ) )
        definition = def;
    else
    {
        definition = NULL;
        if (def)
            delete def;
    };
};

Test* enableSunTest :: newTest()
{   
    SunTest* anewtest = NULL;
    if (definition)
    {
        anewtest = new SunTest();
        if (anewtest)
        {
            anewtest->define(definition);
            // lock to access numinst
            anewtest->setInstnum(numinst++); // set instance number into the new test instance and increment it
            // release locks
        };
    };
    return anewtest;
};

/* enableSunCompositeTest :: enableSunCompositeTest(SunCompositeTestDefinition* def)
{
    if (def && (PR_TRUE == def->isEnabled() ) )
        definition = def;
    else
    {
        definition = NULL;
        if (def)
            delete def;
    };
};

Test* enableSunCompositeTest :: newTest()
{   
    SunCompositeTest* anewtest = NULL;
    if (definition)
    {
        anewtest = new SunCompositeTest();
        if (anewtest)
        {
            anewtest->define(definition);
            // lock to access numinst
            anewtest->setInstnum(numinst++); // set instance number into the new test instance and increment it
            // release locks
        };
    };
    return anewtest;
}; */

void Test :: setInstnum(PRInt32 in)
{
    instnum = in;
};

void SunTestSuite :: addScrubber(NSString apath, char* name, RegexScrubber*& scrubber)
{
    apath.append(name);
    PRFile cleaner(apath.data(), PR_RDONLY, 0);
    void* data;
    char* returned = NULL;
    int len;
    int retlen;
    cleaner.read(data, len);
    platform_cleaner.scrub_buffer((char*)data, len, returned, retlen);

    if (retlen && returned)
    {
        scrubber = new RegexScrubber(returned, retlen);
        delete[] returned;
        returned = NULL;
        retlen = 0;
    };
};

void SunTestSuite :: recurse(PtrList<char>& groups, const char* path, RegexList& list, RegexList& xlist, const char* base, SunTestProperties* prop)
{
    if (NULL == path)
        return;

    PRFileInfo info;
    PRStatus rc = PR_GetFileInfo(path, &info);

    if (PR_SUCCESS != rc)
        return;

    switch (info.type)
    {
        case PR_FILE_DIRECTORY:    
        {
            PRDir* directory = PR_OpenDir(path);

            if (directory)
            {
                NSString loc;
                loc.append(path);
                SunTestProperties* newprop = new SunTestProperties(*this, loc, prop);

                PRDirEntry* entry = NULL;

                do
                {
                    entry = PR_ReadDir(directory, PRDirFlags(PR_SKIP_BOTH | PR_SKIP_HIDDEN));

                    if (entry)
                    {
                        NSString location;
                        location.append(path);
                        location.append("/");
                        location.append(entry->name);

                        recurse(groups, location.data(), list, xlist, base, newprop );
                    };

                } while (NULL!=entry);

                PR_CloseDir(directory);
            };
            break;
        };

        case PR_FILE_FILE:
        {
            char* name = strdup(path);
            char* pos = NULL;
	        PRBool composite = PR_FALSE;
            if (name)
                pos = strstr(name, ".dat");

            if (name && (!pos) )
    	    {
                pos = strstr(name, ".cts");
                if (pos)
                    composite = PR_TRUE;
    	    }

            if (pos)
            {
                PRBool disabled = PR_FALSE;

                char* truename=strstr(name, base);
                if (truename==name && (strlen(truename)>strlen(base)) )
                    truename = name + strlen(base)+1;

                * pos = 0; // clear extension

                // check exclusion list
                int counter;
                int regexs = xlist.length();
                for (counter=0;counter<regexs;counter++)
                {
                    RegexEntry* regex = xlist.get(counter);

                    // to do the regex matching here
                    if (regex && regex->match(truename))
                    {
                        disabled = PR_TRUE;
                        break;
                    };
                };

                // check inclusion list
                regexs = list.length();
                if (PR_FALSE == disabled)
                {
                    for (counter=0;counter<regexs;counter++)
                    {
                        RegexEntry* regex = list.get(counter);

                        // to do the regex matching here
                        if (regex && regex->match(truename))
                        {
                            * pos = '.'; // put it back
            			    if (PR_FALSE == composite)
            			    {
            				    SunTestDefinition* testdef = new SunTestDefinition(truename, base, *this, prop); // define a Sun test
            				    if (testdef && PR_TRUE == testdef->isEnabled())
            				        enableSunTest* asuntest = new enableSunTest(testdef); // add it to the test list
            				    else
            				        if (testdef)
                                        delete testdef;
            				    break;
            			    }
            			    else
            			    {
            				    // composite test suite
            				    /* SunCompositeTestDefinition* testdef = new SunCompositeTestDefinition(truename, base, groups, *this, prop, timeout, ofs, del); // define a Sun composite test
            				    if (testdef && PR_TRUE == testdef->isEnabled())
            				        enableSunCompositeTest* asuntest = new enableSunCompositeTest(testdef); // add it to the test list
            				    else
            				        if (testdef)
                                        delete testdef; */
            				    break;
            			    };
                        };
                    };
                };
            }
	    
            if (name)
                free(name);
        };
    };
};

PRBool SunTest :: timeoutsExceeded() const
{
    PRBool exceeded = PR_FALSE;
    PRInt32 maxtimeouts = definition->getSuite().getMaxtimeouts();
    if ( (maxtimeouts>0) && (timeouts >= maxtimeouts) )
    {
        exceeded = PR_TRUE;
    };
    return exceeded;
};

void SunTest :: recordTimeout()
{
    PR_AtomicIncrement(&timeouts);
};

PRInt32 SunTestSuite :: getMaxtimeouts() const
{
    return maxtimeouts;
};

SunTestSuite :: SunTestSuite(PtrList<char>& groups, const char* dir, RegexList& list, RegexList& xlist,
                             char* platform, char* version, PRInt32 release, PRBool tolog, PRIntervalTime to,
                             PRInt32 splitofs, PRInt32 splitdel, PRInt32 hsp, SecurityProtocols sp, PRInt32 maxtm)
                            : groupSet(groups)
{
    maxtimeouts = maxtm;
    secprots = sp;
    hsperiod = hsp;
    currentplatform = platform;
    currentversion = version;
    currentrelease = release;
    ofs = splitofs;
    del = splitdel;
    timeoutperiod = to;
    log = tolog;
    suitepath = strdup(dir);
    server_cleaner = dat_cleaner = res_cleaner = NULL;
    NSString pathname;
    pathname.append(dir);
    pathname.append("/config/");

    NSString platform_regex;
    platform_regex.append("::");
    platform_regex.append(platform);
    platform_regex.append("_ONLY::");
    char* otherplatforms = "::.*_ONLY::";

    platform_cleaner.add_regex((void*) platform_regex.data(), platform_regex.length(), (void *)"", 0);

    int entries = groups.entries();
    for (int group = 0; group<entries; group++)
    {
        char* thisgroup = groups.at(group);
        NSString string;
        string.append("::");
        string.append(thisgroup);
        string.append("_ONLY");
        string.append("::");
        platform_cleaner.add_regex((void*) string.data(), string.length(), (void *)"", 0);
    };

    platform_cleaner.add_regex((void*) otherplatforms, strlen(otherplatforms), (void *)"# ", 2);
 
    addScrubber(pathname, "server_cleaner", server_cleaner);
    addScrubber(pathname, "dat_cleaner", dat_cleaner);
    addScrubber(pathname, "res_cleaner", res_cleaner);

    NSString outpath;
    outpath.append(dir);
    outpath.append("/output/");
    PR_MkDir(outpath.data() , 0xFFFFFFFF);

    if ( dir && (0 != list.length()) )
    {
        NSString location;
        location.append(dir);
        location.append("/data");

        recurse(groups, location.data(), list, xlist, location.data(), NULL);
    };
};

SunTestSuite :: ~SunTestSuite()
{
    if (suitepath)
        free((void*)suitepath);
    if (server_cleaner)
        delete server_cleaner;
    if (dat_cleaner)
        delete dat_cleaner;
    if (res_cleaner)
        delete res_cleaner;
};

void TestInstance :: getCurrentStats(PRInt32& attempted, PRInt32& success, PRInt32& fail)
{
    attempted = PR_AtomicSet(&attempts, 0);
    success = PR_AtomicSet(&successes, 0);
    fail = PR_AtomicSet(&failures, 0);

    // when tests have been stopped after running in a loop, the counters are incremented in execute_test,
    // so don't do it again here
    if (passes != -1)
    {
        instance_attempts += attempted;
        instance_success += success;
        instance_fail += fail;
    };
};

void TestInstance :: getTotalStats(PRInt32& attempted, PRInt32& success, PRInt32& fail, PRIntervalTime& elapse)
{
    attempted = instance_attempts;
    success = instance_success;
    fail = instance_fail;
    elapse = instance_elapsed;
};

void TestInstance :: setThreadId(PRInt32 intid)
{
    tid = intid;
};

PRInt32 TestInstance :: getThreadId()
{
    return tid;
};

void Test :: setThreadId(PRInt32 intid)
{
    tid = intid;
};

void SunTestDefinition :: setSplit(PRInt32 offset, PRInt32 sleeptime)
{
    ofs = offset;
    del = sleeptime;
};

void SunTestDefinition :: getSplit(PRInt32& offset, PRInt32& sleeptime)
{
    offset = ofs ;
    sleeptime = del;
};

inline void copy_and_grow(const char* source, char* & target, int& sourceofs, int& targetofs, int& targetlen, int amount)
{
  if ( (targetofs + amount) > targetlen )
    {
      targetlen += amount;
      target = (char*) realloc(target, targetlen);
    }
  memcpy(target + targetofs, source + sourceofs, amount);
  sourceofs += amount;
  targetofs += amount;
};

// right now it's a bit of a misnomer since this chunked-decoder will only work with the
// implementation in NES
PRBool HttpDecoder :: decode (const char* raw, int rawlen, char*& decoded, int& decodedlen)
{
    decodedlen = 2*rawlen+1024; // allocate extra room for CL headers to be added
    decoded = (char*)malloc(decodedlen);

    const char* header="";
    int writeofs=0;
    int readofs=0;

    while (readofs<rawlen && header)
    {
        header = strstr(raw+readofs, "\nTransfer-Encoding: chunked\r\n");
        if (!header)
            header = strstr(raw+readofs, "\nTransfer-encoding: chunked\r\n");
        if (!header)
        {
            // just copy all the data as is
	        copy_and_grow(raw, decoded, readofs, writeofs, decodedlen, rawlen - readofs );
        }
        else
        {
	        header+=1;
            // first copy all data prior to this header as is
	        copy_and_grow(raw, decoded, readofs, writeofs, decodedlen, header- (raw+readofs));

            // found chunked-encoding, now search for body
            const char* body = strstr(header, "\r\n\r\n");
            if (!body)
            {
                // skip chunked-decoding of this HTTP message since we can't find a body
                *(decoded+readofs) = 'T';
                readofs+=1;
                writeofs+=1;
                continue;
            }
            else
            {
                // we got processing to do
                readofs = body-raw+4;
                int chunklen = 0;
		        char lenstring[256];
		        int decodedbodyofs = writeofs; // save the offset for later

                do
                {
                    chunklen = 0;
                    sscanf(raw+readofs, "%x\n", &chunklen);
                    if (chunklen>=0)
                    {
                        const char* chunkdata = strchr(raw+readofs, '\n');
                        if (!chunkdata)
                            continue;
                        chunkdata+=1;
                        if (chunklen>0)
			            {
			                int unusedvar = 0;
			                copy_and_grow(chunkdata, decoded, unusedvar, writeofs, decodedlen, chunklen );
		                }
                        readofs = chunkdata-raw + chunklen +2 ; // skip CR LF after data
                    };
                } while (chunklen>0);

                // write a content-length header and compute its length
		        int datalen = writeofs - decodedbodyofs;
		        if (datalen)
		        {
		            // write the content-length header
                    int lenlen = sprintf(lenstring, "Content-length: %d\n\n", datalen);
		            int unusedvar = 0;
		            copy_and_grow(lenstring, decoded, unusedvar, writeofs, decodedlen, lenlen );

		            // and now we need to switch the data with the header
		            memmove(decoded+decodedbodyofs+lenlen, decoded+decodedbodyofs, datalen);
                    memcpy(decoded+decodedbodyofs, lenstring, lenlen);
		        };
            };
        };
    };
    decodedlen=writeofs;
    decoded=(char*)realloc((void*)decoded, writeofs+1);
    *(decoded+writeofs) = '\0'; // NULL termination
    return PR_TRUE;
};

SunTestProperties :: SunTestProperties(SunTestSuite& suite, NSString location, SunTestProperties* ancestor) : parent(ancestor)
{
    dat_cleaner = res_cleaner = server_cleaner = NULL;

    addScrubber(suite, location, "dat_cleaner", dat_cleaner);
    addScrubber(suite, location, "res_cleaner", res_cleaner);
    addScrubber(suite, location, "server_cleaner", server_cleaner);
};

void SunTestProperties :: addScrubber(SunTestSuite& suite, NSString apath, char* name, RegexScrubber*& scrubber)
{
    apath.append("/");
    apath.append(name);
    PRFile cleaner(apath.data(), PR_RDONLY, 0);
    void* data;
    char* returned;
    int len;
    int retlen;
    cleaner.read(data, len);
    suite.getPlatformCleaner().scrub_buffer((char*)data, len, returned, retlen);

    if (retlen && returned)
    {
        scrubber = new RegexScrubber(returned, retlen);
        delete [] returned;
        returned = NULL;
        retlen = 0;
    };
};

const char* BaseTestDefinition :: getFilename() const
{
    return (const char*) basename;
};

const char* BaseTestDefinition :: getDescription() const
{
    return (const char*) description;
};

const char* BaseTestDefinition :: getPath() const
{
    return (const char*) path;
};

const char* BaseTestDefinition :: getSuitepath() const
{
    return (const char*) suite.getPath();
};

const char* SunTestSuite :: getPath () const
{
    return suitepath;
};

const void* SunTestDefinition ::  getInput() const
{
    return input;
};

PRInt32 SunTestDefinition :: getInputLength() const
{
    return inlen;
};

const void* SunTestDefinition ::  getGold() const
{
    return gold;
};

PRInt32 SunTestDefinition :: getGoldLength() const
{
    return goldlen;
};

const PRInt32* SunTestDefinition :: getCipherSet() const
{
    return cipherSet;
};

const char* SunTestDefinition :: getNickName() const
{
   return nickName;
};

const char* SunTestDefinition :: getMutexName() const
{
   return mutexName;
};

PRBool SunTestDefinition :: getNoSSL() const
{
   return noSSL;
};

PRInt32 SunTestDefinition :: getCipherSetCount() const
{
    return cipherCount;
};

const SunTestProperties* SunTestDefinition :: getProperties() const
{
    return properties;
};

const SunTestSuite& SunTestDefinition :: getSuite() const
{
    return suite;
};

const PRBool BaseTestDefinition :: isEnabled() const
{
    return enabled;
};

const PRBool BaseTestDefinition :: isGood() const
{
    return (hasCR==PR_TRUE && hasDesc==PR_TRUE);
};

PRInt32 SunTestDefinition :: getTimeout() const
{
    return timeoutperiod;
};

RegexScrubber* SunTestSuite :: getDatCleaner() const
{
    return dat_cleaner;
};

RegexScrubber* SunTestSuite :: getResCleaner() const
{
    return res_cleaner;
};

RegexScrubber* SunTestSuite :: getServerCleaner() const
{
    return server_cleaner;
};

const RegexScrubber& SunTestSuite :: getPlatformCleaner() const
{
    return platform_cleaner;
};

PRBool SunTestSuite :: getLog() const
{
    return log;
};

const char* SunTestSuite :: getCurrentPlatform() const
{
    return currentplatform;
};

const char* SunTestSuite :: getCurrentVersion() const
{
    return currentversion;
};

PRInt32 SunTestSuite :: getCurrentRelease() const
{
    return currentrelease;
};

PRBool TestInstance :: getPerformance() const
{
    return performance;
};

void TestInstance :: setPerformance(PRBool perf)
{
    performance = perf;
};

const PtrList<char>& SunTestSuite :: getGroupSet() const
{
    return groupSet;
};

PRIntervalTime SunTestSuite :: getTimeout() const
{
    return timeoutperiod;
};

PRInt32 SunTestSuite :: getSplitOffset() const
{
    return ofs;
};

PRInt32 SunTestSuite :: getSplitDelay() const
{
    return del;
};

BaseTestDefinition :: BaseTestDefinition(const char* fname, const char* fpath,
            SunTestSuite& testsuite, SunTestProperties* prop) : suite(testsuite)   
{
    basename = NULL;
    if (fname)
    {
         basename = strdup(fname);
         char* pos = strrchr(basename, '.');
         if (pos)
            *pos = '\0'; // clear extension

         file.append(fname);

         if (fpath)
             path.append(fpath);
    };
    enabled = PR_FALSE;
    hasDesc = PR_FALSE;
    hasCR = PR_FALSE;
}
    
void BaseTestDefinition :: Process()
{
    const PtrList<char>& groups = suite.getGroupSet();

    PRBool match = PR_FALSE;
    PtrList<char> grouplist;    // list of groups supported by this test
    PtrList<char> platformlist; // list of platforms supported by this  test
    enabled = PR_FALSE;

    if (file.length())
    {
        NSString infname;
        if (path.length())
        {
            infname.append(path);
            infname.append("/");
        };

        infname.append(file);
        PRFile infile(infname, PR_RDONLY, 0);
        void* input = NULL;
        PRInt32 len = 0;
        infile.read(input, len);
        
        char* versionString = NULL;
        PRInt32 testrelease = 0; // by default, test is for any version equal to 0 or greater

        if (len>1 && input )
        {
            PRBool foundbody = PR_FALSE;
            PRInt32 offset = 0; // offset of the request body

            while (!foundbody)
            {
                if ('#'!=*((char*)input+offset))
                {
                    foundbody = PR_TRUE;
                    continue;
                };

                // now search for end-of-line
                char* line = (char*)input+offset;
                char* lf = strchr(line, '\n'); // search for line feed
                if (!lf)
                {
                    // unable to find end of line - just treat it as body
                    foundbody = PR_TRUE;
                    continue;
                };

                *lf = '\0'; // terminate
                char* cr = strchr(line, '\r'); // search for carriage return
                if (cr)
                    *cr = '\0'; // terminate
                offset = lf-(char*)input+1; // offset after this line

                // now we have a comment line - parse it

                char* grp = strstr(line, "%group ");
                if (grp)
                {
                    char* group = NULL;
                    group = strdup(strchr(grp, ' ') + 1);
                    grouplist.insert(group);
                    continue;
                };

                char* platform = strstr(line, "%platform ");
                if (platform)
                {
                    char* os = NULL;
                    os = strdup(strchr(platform, ' ') + 1);
                    platformlist.insert(os);
                    continue;
                };

                char* version = strstr(line, "%version ");
                if (version)
                {                    
                    versionString = strdup(uppercase(strchr(version, ' ') + 1));
                    continue;
                };

                char* release = strstr(line, "%release ");
                if (release)
                {                    
                    testrelease = atoi(strchr(release, ' ') + 1);
                    continue;
                };

                char* CR = strstr(line, "%CR ");
                if (CR) {
                    hasCR = PR_TRUE;
                }

                char* desc = strstr(line, "%desc ");
                if (desc) {
                    if (!hasDesc) {
                        description.append("#");
                        description.append(strstr(desc," "));
                        description.append("; ");
                    }
                    hasDesc = PR_TRUE;
                }

                parse(line);
            };

            if (!grouplist.entries())
                grouplist.insert("COMMON"); // if the test does not have an associated group, use COMMON group by default

            // now check if this test belongs to one of the enabled groups
            for (unsigned int testgroup=0;testgroup<grouplist.entries() && PR_FALSE == enabled;testgroup++)
            {
                char* thistestgroup = grouplist.at(testgroup);
                for (unsigned int enabledgroup=0; enabledgroup<groups.entries() && PR_FALSE == enabled;enabledgroup++)
                {
                    char* thisenabledgroup = groups.at(enabledgroup);
                    if (0 == strcmp(thisenabledgroup, thistestgroup))
                    {
                        enabled = PR_TRUE;
                        break;
                    };
                };
            };

            if (PR_TRUE == enabled)
            {
                if (versionString && 0 != strcmp(versionString, suite.getCurrentVersion()))
                    enabled = PR_FALSE;
            };

            if (PR_TRUE == enabled)
            {
                if (suite.getCurrentRelease() < testrelease)
                    enabled = PR_FALSE;
            };

            // check if this test should run on the platform of the server selected
            if (PR_TRUE == enabled)
            {
                if (platformlist.entries()) // is the test platform-specific ?
                {
                    enabled = PR_FALSE;
                    const char* currentplatform = suite.getCurrentPlatform();
                    unsigned int testplatform;
                    PRBool Unix = PR_TRUE;
                    if (0 == strcmp(currentplatform, "WINNT"))
                        Unix = PR_FALSE;

                    for (testplatform=0;testplatform<platformlist.entries() && PR_FALSE == enabled;testplatform++)
                    {
                        char* thistestplatform = platformlist.at(testplatform);
                        if ( (0 == strcmp(currentplatform, thistestplatform)) ||
                            ( (PR_TRUE == Unix) && (0 == strcmp("UNIX", thistestplatform)) ) )
                        {
                            enabled = PR_TRUE;
                            break;
                        };
                    };

                    if (PR_TRUE == enabled)
                    {
                        match = PR_TRUE;
                        enabled = PR_FALSE;
                    };
                }
                else
                {
                    match = PR_TRUE;
                    enabled = PR_FALSE;
                };
            };

            if (offset > 0)
            {
                PRInt32 newlen = len-offset;
                if (newlen)
                {
                    if (input)
                    {
                        memmove(input, (void*) ( (char*)input + offset), newlen);
                        input = realloc(input, newlen);
                        len = newlen;
                    };
                }
                else
                {
                    free(input);
                    input = NULL;
                    len = 0;
                };
            };

            if (PR_FALSE == processBody(input, len) )
                free(input);
        };

        if (PR_TRUE == match)
            enabled = PR_TRUE;
    };
};

PRBool SunTestDefinition :: parse(const char* line)
{
    const char* tout = strstr(line, "%timeout ");
    if (tout)
    {
        PRInt32 tmout = 0;
        sscanf(strchr(tout, ' ') + 1, "%d", &tmout);

        if (tmout>0 && 0==suite.getTimeout()) // only set test timeout if a global one was not set
        {
            timeoutperiod = PR_TicksPerSecond()*tmout;
        };
        return PR_TRUE;
    };

    const char* err = strstr(line, "%sslerror ");
    if (err)
    {
        PRInt32 sslerrno;
        sscanf(strchr(err, ' ') + 1, "%d", &sslerrno);
        sslerr = (PRInt32) sslerrno;
        return PR_TRUE;
    };

    const char* prot= strstr(line, "%protocol ");

    if (prot)
    {
        const char* thisprot = strchr(prot, ' ') + 1;
        if (thisprot)
        {
            protlist.insert(strdup(thisprot));
            secprots = protlist;
        };
        return PR_TRUE;
    };

    const char* cipher= strstr(line, "%ciphers ");

    if (cipher)
    {
        const char *cipherString = NULL;
        cipherString = strchr(cipher, ' ') + 1;

        if (cipherString-1)
        {
            int ndx;

            while (0 != (ndx = *cipherString++))
            {
                int* cptr;
                int  cipher;

                if (! isalpha(ndx))
                    continue;
                cptr = islower(ndx) ? ssl3CipherSuites : ssl2CipherSuites;
                for (ndx &= 0x1f; (cipher = *cptr++) != 0 && --ndx > 0; )
                    /* do nothing */;
                if (cipher)
                {
                    cipherSet[cipherCount++] = cipher;
                };
            };
        };
        return PR_TRUE;
    };

    const char* certname = strstr(line, "%cert");
    if (certname)
    {
        const char* cert= strchr(certname, ' ');
        if (cert)
        {
           nickName = (char *)malloc(strlen(cert + 1) + 1);
           strcpy(nickName,  cert + 1);
        };
        return PR_TRUE;
    };

    const char* split = strstr(line, "%splitofs ");
    if (split)
    {
        PRInt32 spl = 0;
        sscanf(strchr(split, ' ') + 1, "%d", &spl);
        if (spl>0 && 0==suite.getSplitOffset()) // only set test splitofs if a global one was not set
        {
            ofs = spl;
        };
        return PR_TRUE;
    };

    const char* handshake = strstr(line, "handshake ");
    if (handshake)
    {
        PRInt32 hs  = 0;
        sscanf(strchr(handshake, ' ') + 1, "%d", &hs);
        if (hs>0 && 0==suite.getHsperiod())    // only set test SSL handshake period
                                               // if a global one was not set
        {
            hsperiod = hs;
        };
        return PR_TRUE;
    };

    split = strstr(line, "%splitdel ");
    if (split)
    {
        PRInt32 spl = 0;
        sscanf(strchr(split, ' ') + 1, "%d", &spl);
        if (spl>0 && 0==suite.getSplitDelay()) // only set test splitdel if a global one was not set
        {
            del = spl;
        };
        return PR_TRUE;
    };

    const char* mutex = strstr(line, "%mutex ");
    if (mutex)
    {
        const char* name = strchr(mutex, ' ');
        if (name)
           mutexName = strdup(name + 1);
        return PR_TRUE;
    };

    const char* nossl = strstr(line, "%nossl");
    if (nossl)
    {
        noSSL = PR_TRUE;
        return PR_TRUE;
    };

    return PR_FALSE;
};

PRBool SunTestDefinition :: processBody(void* data, PRInt32 datalen)
{
    // we want to save the request body
    input = data;
    inlen = datalen;
    return PR_TRUE;
};

SunCompositeTestDefinition :: SunCompositeTestDefinition(const char* fname, const char* fpath,
                    SunTestSuite& testsuite, SunTestProperties* prop) : 
                    BaseTestDefinition(fname, fpath, testsuite, prop)
// process the CTS file
{
    Process();
};

PRInt32 SunTestSuite :: getHsperiod() const
{
    return hsperiod;
};

PRInt32 SunTestDefinition :: getHsperiod () const
{
    return hsperiod;
};

void keyboard(void* arg)
{
    getc(stdin);
    keypressed = PR_TRUE;
};

void thread_entry(void* arg)
{
    PRInt32 remaining = 0;
    do
    {
        TestInstance* that = NULL;

        PR_Lock(listLock);
        remaining = instanceList->entries();
        if (remaining)
        {
            that = instanceList->at(0);
            if (that)
            {
                instanceList->removeAt(0);
                remaining = instanceList->entries();
            };
        };
        
        PR_Unlock(listLock);
        if (that)
        {
            if (0 == that->getPasses())
            {
                Logger::logError(LOGINFO, "Running %s thread %d in a loop", 
                                 that->myTest().description(), that->getThreadId()); 
            }
            else
            {
                Logger::logError(LOGINFO, "Running %s thread %d , repeats = %d", 
                                 that->myTest().description(), that->getThreadId(), that->getPasses());
            };
            that->execute_test();
        };

    } while (remaining);
};


PRInt32 SunTestSuite :: runTests(HttpServer& server, PRInt32 concurrent,
                                 PRInt32 repeat, PRInt32 maxthreads,
                                 PRBool performance,
                                 PRBool loop, PRInt32 displayperiod)
{
    PRIntervalTime beginning = PR_IntervalNow();
    PRInt32 totalops = 0;
    PRInt32 totalfail = 0;

    if (0 == repeat)
    {
        // tests need to run at least once
        repeat = 1;
    };

    if (PR_TRUE == loop)
    {
        // a zero repeat rate tells the tests to run forever
        repeat = 0;
        maxtimeouts = 0;
    };

    if (concurrent == 0)
    {
        // multithread mode was not specified, therefore synchronous operation is requested
        // since all tests now run in threads, simply set the maxthreads to 1
        concurrent = 1;
        maxthreads = 1;
    };

    PRInt32 total=0;
    PRInt32 success=0;
    PRInt32 fail=0;
    
    if (!alltests)
        return 0;
    
    int i, j, k;
    int count = 0;
    
    // first count tests
    count = alltests->entries();
    
    if (!count)
        return 0;

    // then allocate enough instance pointers for all tests
    TestInstance** instarray[NUM_PROTOS];
    for (i=0;i<NUM_PROTOS;i++)
        instarray[i] = (TestInstance**)calloc(count*concurrent, sizeof(TestInstance*));
    
    instanceList = new PtrList<TestInstance>;
    listLock = PR_NewLock();
    
    // create all the test instances and put them in both the start list and the array
    for (i=0;i<count;i++)
    {
        Test* currenttest = alltests->at(i)->newTest(); // this test object is used only for checking supported protocols
        if (currenttest)
        {
            for (j=0;j<NUM_PROTOS;j++)
                for (k=0;k<concurrent; k++)
                {
                    if ( 1<<j & currenttest->supportedProtocols() )
                    {
                        TestInstance*& aninstance = instarray[j] [i*concurrent+k] =
                        new TestInstance(*alltests->at(i), server, (HttpProtocol_e) (1<<j), performance, concurrent);
                        if (!aninstance)
                            continue;

                        instanceList->insert(aninstance);
                        aninstance->setRepeat(repeat);
                        aninstance -> setThreadId(k+1);
                    };
                };
        };
        delete(currenttest); 
    };

    total = instanceList->entries(); // number of test instances available to run
    PRInt32 threads = total;
    if ( (PR_FALSE == loop) && (maxthreads) )
    {
        if (threads>maxthreads)
        {
            // there are more tests than we want to start threads, so limit them
            threads = maxthreads;
        };
    };

    Logger::logError(LOGINFO, "Running %d test%s .", count, (count>1)?"s":"");
    Logger::logError(LOGINFO, "Concurrency : %d .", concurrent);
    if (PR_TRUE == loop)
    {
        Logger::logError(LOGINFO, "Repeat rate : infinite .");
    }
    else
    {
        Logger::logError(LOGINFO, "Repeat rate : %d .", repeat);
    };
    Logger::logError(LOGINFO, "Total client threads : %d", threads);
    if (PR_TRUE == performance)
    {
        Logger::logError(LOGINFO, "Performance mode enabled.");
    }
    else
    {
        Logger::logError(LOGINFO, "Regex checking mode enabled.");
    };

    // now start this number of threads
    PRThread** allthreads = (PRThread**)malloc(sizeof(PRThread*)*threads);
    for (i=0;i<threads;i++)
    {
        allthreads[i] =
            PR_CreateThread(PR_USER_THREAD,
                                 thread_entry,
                                 (void*) NULL,
                                 PR_PRIORITY_NORMAL,
                                 PR_GLOBAL_THREAD,
                                 PR_JOINABLE_THREAD,
                                 0);
    };

    PRInt32 percent = 0;
    PRIntervalTime previous = beginning;

    if (PR_TRUE == loop)
    {
        PR_CreateThread(PR_USER_THREAD,
                     keyboard,
                     (void*) NULL,
                     PR_PRIORITY_NORMAL,
                     PR_GLOBAL_THREAD,
                     PR_JOINABLE_THREAD,
                     0);
    };

    PRInt32 iterations = 0;
    do
    {
        if ( (PR_TRUE==loop) && (PR_TRUE == keypressed) )
        {
            loop = PR_FALSE; // the user entered data, this is the signal to end the loop
            Logger::logError(LOGINFO, "User input - waiting for all tests to end");
            for (i=0;i<count;i++)
                for (j=0;j<NUM_PROTOS;j++)
                    for (k=0;k<concurrent; k++)
                        {
                            if ( instarray[j] [i*concurrent+k] )
                            {
                                TestInstance*& aninstance = instarray[j] [i*concurrent+k];
                                aninstance->setRepeat(-1); // the repeat rate of -1 aborts the test loop and will
                                // eventually cause all test threads to end, making them joinable
                            };
                        };
        };

        PRInt32 currentops = 0, currentfail = 0;
        if (PR_TRUE == loop)
        {
            // delay until next statistics are displayed
            PR_Sleep(PR_SecondsToInterval(displayperiod));
        }
        else
        {
            // wait for all test threads to end
            for (i=0;i<threads;i++)
            {
                PR_JoinThread(allthreads[i]);
            };
        };

        // get all test statistics
        for (i=0;i<count;i++)
        {
            PRBool passed = PR_TRUE;
            for (j=0;j<NUM_PROTOS;j++)
            {
                for (k=0;k<concurrent; k++)
                    if ( instarray[j] [i*concurrent+k] )
                    {
                        TestInstance*& aninstance = instarray[j] [i*concurrent+k];
                        if (!aninstance)
                        {
                            continue;
                        };

                        if (PR_TRUE != finishtest(aninstance, currentops, currentfail, (loop == PR_FALSE) ? PR_TRUE:PR_FALSE) )
                            passed = PR_FALSE;
                    };
            };
            if (PR_FALSE == loop)
            {
                if (PR_TRUE == passed)
                {
                    success ++;
                }
                else
                {
                    fail ++;
                };
            };
        };

        totalops += currentops;
        totalfail += currentfail;

        PRIntervalTime latest = PR_IntervalNow();
    
        PRIntervalTime totaltime = latest - beginning;
        PRIntervalTime period = latest - previous;
        previous = latest;
    
        PRFloat64 totalseconds = (PRFloat64) totaltime / ( PRFloat64) PR_SecondsToInterval(1);
        if (totalseconds<= 0 )
            totalseconds = 1;

        PRFloat64 currentseconds = (PRFloat64) period / ( PRFloat64 )PR_SecondsToInterval(1);
        if (currentseconds<= 0 )
            currentseconds = 1;
    
        PRFloat64 current_ops_per_sec = currentops / currentseconds;
        PRFloat64 total_ops_per_sec = totalops / totalseconds;
        
        if ( (PR_TRUE == loop) || (iterations>0) )
        {
            Logger::logError(LOGINFO, "%d operation%s and %d failure%s in the last %.2f second%s - %.2f op%s/second",
                             currentops, (currentops>1)?"s":"",
                             currentfail, (currentfail>1)?"s":"",
                             currentseconds, (currentseconds>1)?"s":"",
                             current_ops_per_sec, (current_ops_per_sec>1)?"s":"");
        };

        if (PR_FALSE == loop)
        {
            Logger::logError(LOGINFO, "%d operation%s and %d failure%s in %.2f second%s - %.2f ops/second",
                         totalops, (totalops>1)?"s":"", totalfail, (totalfail>1)?"s":"", totalseconds,
                         (totalseconds>1)?"s":"", total_ops_per_sec, (total_ops_per_sec>1)?"s":"");
        };
        iterations ++;
    } while ( PR_TRUE == loop );

    if ( (totalops + totalfail) )
        percent = (PRInt32) ( (PRFloat64) ((PRFloat64)100*totalops) / (PRFloat64)(totalops+totalfail) );

    Logger::logError(LOGINFO, "%d %% of all test operations were successful", percent);

    fprintf(stdout, "---------------------------------\n");
    fprintf(stdout, "Tests passed :    %d / %d\n", success, count);
    fprintf(stdout, "Tests failed :    %d / %d\n", fail,count);
    fprintf(stdout, "Ops passed   :    %d / %d\n", totalops, totalops+totalfail);
    fprintf(stdout, "Ops failed   :    %d / %d\n", totalfail, totalops+totalfail);
    fprintf(stdout, "---------------------------------\n");

    return percent;
};

PRBool SunTestSuite :: finishtest(TestInstance*& aninstance, PRInt32& totalops, PRInt32& totalfail, PRBool final)
{
    PRInt32 passes = 0;
    PRInt32 successes = 0;
    PRInt32 failures = 0;
    PRFloat64 rate = 0;

    aninstance->getCurrentStats(passes, successes, failures);

    if (passes>0)
       rate = (PRFloat64) ((PRFloat64) successes / (PRFloat64) passes);

    totalops += successes;
    totalfail += failures;

    if (PR_TRUE == final)
    {
        // get the final stats
        PRIntervalTime elapsed;
        aninstance->getTotalStats(passes, successes, failures, elapsed);
        if (passes>0)
        {
           rate = (PRFloat64) ((PRFloat64) successes / (PRFloat64) passes);

           // warn about tests that slow down GAT runs
           PRIntervalTime duration = (PRIntervalTime)((PRFloat64)elapsed / (PRFloat64)passes);
           if (duration > slowthreshold)
           {
               int ms = PR_IntervalToMilliseconds(duration);
               Logger::logError(LOGWARNING, "%s thread %d ran slowly - %d.%03d seconds/pass",
                                aninstance->myTest().description(), aninstance->getThreadId(), ms / 1000, ms % 1000);
           }
        }
        PR_ASSERT(passes == (successes + failures));
    };

    if (passes && passes == successes)
    {
        if (PR_TRUE == final)
        {
            // only print the success at the end
            Logger::logError(LOGPASS, "%s thread %d passed - %d out of %d pass%s",
                             aninstance->myTest().description(), aninstance->getThreadId(), successes, passes, (passes>1)?"es":"");
        };
    }
    else
    {
        if (passes>0)
            Logger::logError(LOGFAILURE, "%s thread %d failed - %d out of %d pass%s",
                             aninstance->myTest().description(),  aninstance->getThreadId(), successes, passes, (passes>1)?"es":"");
        else
            if (failures>0)
                Logger::logError(LOGFAILURE, "%s thread %d failed %d pass%s",
                                 aninstance->myTest().description(), aninstance->getThreadId(), failures, (failures>1)?"es":"");
    };

    if (PR_TRUE == final)
    {
        delete aninstance;
        aninstance = NULL;
    };

    return rate;
};

PRInt32 TestInstance :: getConcurrency() const
{
    return concurrency;
};

const SecurityProtocols& SunTestSuite :: getSecprotocols() const
{
    return secprots;
};

const SecurityProtocols& SunTestDefinition :: getSecprotocols() const
{
    return secprots;
};

