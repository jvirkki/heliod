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


#ifndef __HTTPTESTS__
#define __HTTPTESTS__

#include "http.h"
#include "engine.h"
#include <support/NSString.h>
#include "regex_string.h"
#include "regex_entry.h"
#include "regex_list.h"
#include "regex_scrubber.h"


class TestInstance;

// definition of a test
class Test
{
    public:
        Test(const char* desc, long prots = 0);        // constructs test and adds it to the list of tests
        virtual ~Test();
        void setDescription(const char* desc);
        const char* description() const;
        const long supportedProtocols() const;
        virtual PRBool setup();                // test initialization function
                // this creates the instance test data, uploads necessary file to the server
                // and if successful marks the test ready to run
                // the default is no setup is required
        virtual PRBool run() = 0;                        // the actual test - must be defined in derived class
        void setInstance(TestInstance* inst);
        const PRBool isReady() const;
        void setInstnum(PRInt32);
        void setThreadId(PRInt32 tid);

    protected:
        PRBool ready;
        TestInstance* instance;
        PRInt32 instnum; // instance number
        PRInt32 tid; // thread id

    private:
        const char* textdescription;                // text info about the test
        const long protocols;                       // which protocols are supported. Bitmapped, see http.h
};

class SunTestProperties;

class SunTestSuite
{
    public:
        SunTestSuite(PtrList<char>& groups, const char* dir, RegexList& list, RegexList& xlist, char* platform, char* version,
            PRInt32 release, PRBool tolog, PRIntervalTime to, PRInt32 splitofs, PRInt32 splitdel, PRInt32 hsp, SecurityProtocols sp, PRInt32 maxtm);
        ~SunTestSuite();

        void recurse(PtrList<char>& groups, const char* path, RegexList& list, RegexList& xlist, const char* base, SunTestProperties* prop);
        const char* getPath() const;

        RegexScrubber* getDatCleaner() const;
        RegexScrubber* getResCleaner() const;
        RegexScrubber* getServerCleaner() const;
        const RegexScrubber& getPlatformCleaner() const;

        PRBool getLog() const;

        const char* getCurrentPlatform() const;
        const char* getCurrentVersion() const;
        PRInt32 getCurrentRelease() const;
        const PtrList<char>& getGroupSet() const;
        PRIntervalTime getTimeout() const;
        PRInt32 getSplitOffset() const;
        PRInt32 getSplitDelay() const;
        PRInt32 getHsperiod() const;
        const SecurityProtocols& getSecprotocols() const;
        PRInt32 getMaxtimeouts() const;

        PRInt32 runTests(HttpServer& server, PRInt32 concurrent=0,
                         PRInt32 repeat=1, PRInt32 limit = 0,
                         PRBool performance = PR_FALSE,
                         PRBool loop = PR_FALSE,
                         PRInt32 displayperiod = 5);
        PRBool finishtest(TestInstance*& aninstance, PRInt32& totalops, PRInt32& fails, PRBool clean = PR_TRUE);

    protected:

        PRInt32 maxtimeouts;    // maximum number of test timeouts allowed before abortion
        SecurityProtocols secprots;
        PtrList<char>& groupSet;
        char* currentversion;
        PRInt32 currentrelease;
        char* currentplatform;
        PRInt32 ofs, del;
        PRBool log;
        const char* suitepath;
        RegexScrubber* server_cleaner;
        RegexScrubber* dat_cleaner;
        RegexScrubber* res_cleaner;
        RegexScrubber platform_cleaner;
        PRIntervalTime timeoutperiod;
        PRInt32 hsperiod;
        static PRIntervalTime slowthreshold;

        void addScrubber(NSString apath, char* name, RegexScrubber*& scrubber);
};

class SunTestProperties
{
    public:
        SunTestProperties(SunTestSuite& suite, NSString location, SunTestProperties* ancestor=NULL);

        RegexScrubber* server_cleaner;
        RegexScrubber* dat_cleaner;
        RegexScrubber* res_cleaner;
        SunTestProperties* parent;

    protected:
        void addScrubber(SunTestSuite& suite, NSString apath, char* name, RegexScrubber*& scrubber);
};

class BaseTestDefinition
{
    public:
        const char* getFilename() const;
        const char* getDescription() const;
        const char* getPath() const;
        const char* getSuitepath() const;
        const PRBool isEnabled() const;
        virtual PRBool parse(const char* line) = 0;
        virtual PRBool processBody(void* data, PRInt32 datalen) = 0;
        const PRBool isGood() const;
        
    protected:
        BaseTestDefinition(const char* fname, const char* fpath,
            SunTestSuite& testsuite, SunTestProperties* prop);
        void parseCommon();
        void Process();

        PRBool enabled;
        NSString file, path, description;
        SunTestSuite& suite;
        char* basename;
        PRBool hasDesc;
        PRBool hasCR;
};

class SunTestDefinition: public BaseTestDefinition
{
    public:
        SunTestDefinition(const char* fname, const char* fpath,
            SunTestSuite& testsuite, SunTestProperties* prop);
        ~SunTestDefinition();

        // loading functions
        virtual PRBool parse(const char* line);
        virtual PRBool processBody(void* data, PRInt32 datalen);

        // properties of a Sun test

        // input (request)
        const void* getInput() const;
        PRInt32 getInputLength() const;

        // expected response aka "gold"
        const void* getGold() const;
        PRInt32 getGoldLength() const;

        // cipher set
        const PRInt32* getCipherSet() const;
        PRInt32  getCipherSetCount() const;

        // request/response timeout
        PRInt32 getTimeout() const;

        // SSL handshake period
        PRInt32 getHsperiod() const;

        // security protocols
        const SecurityProtocols& getSecprotocols() const;

        // client cert properties
        const char* getNickName() const;

        // test mutual exclusion group
        const char* getMutexName() const;

        // explicitly disable SSL
        PRBool getNoSSL() const;

        // parent test suie
        const SunTestSuite& getSuite() const;
        const SunTestProperties* getProperties() const;

        // split delay and offset for slow client emulation
        void setSplit(PRInt32 offset, PRInt32 sleeptime);
        void getSplit(PRInt32& offset, PRInt32& sleeptime);
        PRInt32 sslerr;

    protected:
        PRInt32 ofs, del; // split offset and delay
        void* input;
        PRInt32 inlen;
        void* gold;
        PRInt32 goldlen;
        SunTestProperties* properties;
        PRIntervalTime timeoutperiod; // timeout for request/reply
        PRInt32 cipherSet[32];
        PRInt32 cipherCount;
        char* nickName;
        char* mutexName;
        PRBool noSSL;
        PRInt32 hsperiod;
        SecurityProtocols secprots;
        PtrList<char> protlist;
};

class SunCompositeTestDefinition: public BaseTestDefinition
// need to define a list of SunTestDefinition here
// use a list object and init in the constructor
// you need to read the .cts file to get the list of .dat / .res
// look in SunTestDefinition() constructor for parsing sample
{
    SunCompositeTestDefinition(const char* fname, const char* fpath,
        SunTestSuite& testsuite, SunTestProperties* prop);


   protected:
};

// this new class defines an HTTP composite test
// it embeds a list of Http requests and expected answers 

// there are three types of tests supported :
// 
// 1) regular 1.0 test : single HTTP request / single reply
// 2) keep-alive test : socket stays open, but requests are submitted only after the previous reply is sent
// 3) pipeline test : all requests are submitted first, then the replies are matched
//
// this class will also have some intelligence : it is able to deal with headers sent out of order and ignore
// extraneous headers, depending on the degree of error tolerance set

class HttpTestDefinition
{
    public:
        HttpTestDefinition(const char* fname, const char* fpath, SunTestSuite& testsuite, SunTestProperties* prop);
        ~HttpTestDefinition();
};

class NetscapeTest: public Test
{
    public:
        NetscapeTest(const char* desc, long prots);        // constructs test and adds it to the list of tests
        virtual ~NetscapeTest();

    protected:
        HttpRequest *request;
        HttpResponse *response;
};

class HttpDecoder
{
    public:
        PRBool decode(const char *raw, int rawlen, char *&decoded, int &decodedlen);
};

class SunTest: public Test
{
    public:
        SunTest();
        ~SunTest();
        PRBool define(SunTestDefinition* def);
        virtual PRBool run(); // test execution

        PRBool timeoutsExceeded() const;
        void recordTimeout();

    protected:
        static int timeouts;
        SunTestDefinition* definition;
        SunRequest *request;
        SunResponse *response;
        PRBool failed;
};

class SunCompositeTest: public Test
{
    public:
        SunCompositeTest();
        ~SunCompositeTest();
        PRBool define(SunCompositeTestDefinition* def);
        virtual PRBool run(); // test execution

    protected:
        SunCompositeTestDefinition* definition;
        PRBool failed;
};

class HttpTest: public SunTest
{
    public:
        HttpTest();
        ~HttpTest();
};

class TestItem
{
    public:
        virtual Test* newTest() = 0;
        virtual ~TestItem() {};

        int operator== (const TestItem& rhs) const
        {
            return (this == &rhs);
        };
};

// global singly-linked list of all tests
#ifndef TESTS_IMPLEMENTATION
extern PtrList<TestItem>* alltests;
#else
PtrList<TestItem>* alltests = NULL;
#endif

template <class T> class enableTest: public TestItem
{
    public:
        enableTest()                // constructor
        {
            numinst = 0;
            if (!alltests)
                alltests = new PtrList<TestItem>;
            if (alltests)
                alltests->insert(this);                                // adds this test into the list
        };

        virtual ~enableTest()
        {
            if (alltests)
                alltests->remove((TestItem*)this);                // adds this test into the list
        };

        virtual Test* newTest()        // creates a new instance of the test
        {
            Test* tmp = new T(); // use default constructor to create a new instance
            if (tmp)
            {
                // lock to access numinst
                tmp->setInstnum(numinst++); // set instance number into the new test instance and increment it
                // release locks
            };
            return tmp;
        };

    protected:
        PRInt32 numinst; // number of instances for tests of this type
};

class enableSunTest: public enableTest<SunTest>
{
   public:
      enableSunTest(SunTestDefinition* def);
      virtual Test* newTest();

   protected:
      SunTestDefinition* definition;
};

/* class enableSunCompositeTest: public enableTest<SunCompositeTest>
{
   public:
      enableSunCompositeTest(SunCompositeTestDefinition* def);
      virtual Test* newTest();

   protected:
      SunCompositeTestDefinition* definition;
}; */

class TestInstance // : public HttpEngine
{
    public:
        TestInstance(TestItem& atest, HttpServer& aserver, HttpProtocol aprotocol, PRBool perf, PRInt32 concurrency);
        virtual ~TestInstance();

        enum execmode { sync, async };

        const HttpServer& myServer() const;
        const Test& myTest() const;
        const HttpProtocol myProtocol() const;

        void setMode(execmode);
        execmode getMode();

        PRStatus getStatus();
        void setStatus(PRStatus stat);

        void setRepeat(PRInt32 runs);
        PRBool wait();
        void execute_test();

        void getCurrentStats(PRInt32& attempted, PRInt32& success, PRInt32& fail);
        void getTotalStats(PRInt32& attempted, PRInt32& success, PRInt32& fail, PRIntervalTime& elapse);

        void setThreadId(PRInt32 tid);
        PRInt32 getThreadId();

        void setPerformance(PRBool perf);
        PRBool getPerformance() const;

        void setConcurrency(PRInt32 val);
        PRInt32 getConcurrency() const;

        PRInt32 getPasses() const;
        PRBool operator==(const TestInstance& rhs) const;

    protected:
        PRInt32 concurrency;
        PRBool performance;
        Test& test;
        HttpServer& server;
        HttpProtocol protocol;
        PRStatus status;
        const char* uri;
        static void threadInstance(void*);
        execmode mode;
        PRThread* thread;
        PRInt32 passes;
        PRInt32 successes;
        PRInt32 failures;
        PRInt32 attempts;
        PRInt32 tid;

        PRInt32 instance_success;
        PRInt32 instance_fail;
        PRInt32 instance_attempts;
        PRIntervalTime instance_elapsed;
};

#endif
