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

#ifndef ARLIB_CLASSES
#define ARLIB_CLASSES

#include <nspr.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
//const PRInt32 MAXDNAME = 256;
//const PRInt32 HFIXEDSZ = 12;
#include <resolv.h>

static PRInt32 CopyHostent(PRHostEnt *to, hostent *from, char *buf, PRIntn bufsize);

class DNSSessionHash;

enum DNSStatus
{
    DNS_INIT, DNS_IN_PROCESS, DNS_SENT, DNS_RCVD, DNS_TIMED_OUT, DNS_DYING, DNS_ERROR, DNS_COMPLETED
};

enum querytype
{
    FORWARD_LOOKUP, REVERSE_LOOKUP
};

const PRInt32 RES_CHECKPTR = 0x0400;

typedef struct resinfo_t
{
    char  *ri_ptr;
    int    ri_size;
} resinfo_t;

void ar_free_hostent (struct hostent *hp, int flag);

#ifndef MAXALIASES
#define MAXALIASES      35
#endif

#ifndef MAXADDRS
#define MAXADDRS        35
#endif

struct hent
{
    char    *h_name;                  // official name of host
    char    *h_aliases[MAXALIASES];   // alias list
    int      h_addrtype;              // host address type
    int      h_length;                // length of address
    // list of addresses from name server
    char * h_addr_list[MAXADDRS+1];
#define    h_addr    h_addr_list[0]
};

const PRInt32 MAXPACKET = 1024;

struct datapacket
{
    // Each time a packet is received, we create a reslist_pkt_t structure
    // and queue it onto the reslist.
    char rcvbuf[HFIXEDSZ+MAXPACKET];
    int buflen;
    datapacket* next;
};

// DNSSession encapsulates the data members for a DNS request and response
// Objects of this class are allocated and deallocated by the upper-level
// PR_AR_Get... async DNS resolution functions

class DNSSession
{
    public:
        DNSSession(const char* name, PRUint16 family, PRIntervalTime timeout,
                   char* buf, PRIntn bufsize, PRHostEnt* hentp);    // constructor for forward DNS lookup (name resolution)
        DNSSession(const PRNetAddr* addrp, PRIntervalTime timeout,
                   char* buf, PRIntn bufsize, PRHostEnt* hentp);    // constructor for reverse DNS lookup (IP resolution)
        ~DNSSession();

        void common(PRIntervalTime tm);
        void wait();     // wait up to the timeout for a response from the
                    // DNS server


        void error();    // set the status to ERROR and wake up the client thread
        void expire();   // set the status to TIMEOUT and wake up the client thread
        void complete(); // set the status to DONE and wake up the client thread

        void update(PRIntervalTime tm); // update timeout
        PRBool hasTimedOut();    // returns whether the DNS request took 
        // too long to process. in this case, the worker thread will remove
        // it from the hash table and call expire() to wake up the client
        void setSendFailed();
        PRBool hasSendFailed();

        int getID() const;
        void setID(int);
        DNSStatus getStatus() const;
        void setStatus(DNSStatus val);

        char getSends() const;
        void setSends(char sends);
        void incrSends();

        char decrRetries();
        void setRetries(char r);

        char getResends() const;
        void setResends(char ins);

        void addSent(int sent);

        char getSrch() const;
        void incrSrch();

        char getType() const;
        void setType(char t);

        // accessors to the arguments of PR_AR_ functions
        const char* getName() const;
        PRUint16 getFamily() const;
        const PRNetAddr* getIP() const;
        PRHostEnt* getHentp() const;
        char* getBuffer() const;
        PRIntn getBufferSize() const;
        querytype getQType() const;

        const char* getRename() const;
        void setRename(const char* inname);

        void free_packet_list();

        hent& getHent();
        void reslist_add_pkt(datapacket *);
        in6_addr* getReaddr();

        char getReaddrtype();
        void setReaddrtype(char t);

        hostent* getArhost();

        datapacket* getData();
       
    protected:
        PRBool awake;
        PRUint16 ipfamily;
        const char* hostname;
        const PRNetAddr* ip;
        char* buffer;
        PRIntn buffersize;
        PRHostEnt* hostentp;
        querytype qtype;

        DNSStatus status;
        PRLock* lock;
        PRCondVar* cvar;
        PRIntervalTime timeout;
        PRIntervalTime starttime;
        PRBool timedout;
        PRBool sendfailed;
        int id; // id to match this request with the DNS response, as well
                // as for the hash table

        // query stuff
        char re_sends;
        int re_sent;
        char re_srch;
        char re_type;
        char re_name[65];
        hent re_he;
        in6_addr re_addr;
        char re_addrtype;
        char re_resend;    // send flag. 0 == dont resend
        datapacket* re_data;
        char re_retries;
        hostent ar_host;
};

// linked list of DNS sessions

class DNSSessionListElement
{
    public:
        DNSSessionListElement(DNSSessionListElement* p,
                              DNSSessionListElement* n, DNSSession* s);
        ~DNSSessionListElement();

        DNSSessionListElement* getNext() const; // returns next entry
        DNSSessionListElement* getPrev() const; // returns previous entry
        DNSSession* getSession() const; // returns DNS session

        void setNext(DNSSessionListElement* n); // sets next entry
        void setPrev(DNSSessionListElement* p); // sets previous entry

    protected:
        DNSSession* session;
        DNSSessionListElement* prev;
        DNSSessionListElement* next;
};

class DNSSessionList
{
    friend class DNSSessionHash;

    public:
        DNSSessionList();
        void add(DNSSession* s);
        PRBool remove(DNSSession* s); // deletes this session from the list
        PRBool remove(int id); // deletes this session from the list
        DNSSession* lookup(int id);
        PRInt32 numberOfElements() const;
        
    protected:
        DNSSessionListElement* head;
        DNSSessionListElement* getHead();
        PRInt32 total;
};

const unsigned long HASH_SIZE = 512;

class DNSSessionHash
{
    public:
        DNSSessionHash();
        ~DNSSessionHash();
        void add(DNSSession* session);
        void remove(DNSSession* session);
        DNSSession* lookup(int id);

        // enumeration functions
        void start_enum();
        DNSSession* getNextSession();
        void removeCurrent();
        void end_enum();
        void check();

    protected:
        PRInt32 total;
        DNSSessionList buckets[HASH_SIZE];
        PRLock* lock;

        // variables used for enumerating
        PRInt32 bucket;
        DNSSessionListElement* nextitem;
        DNSSession* lastsession;
};

// statistics structure.

typedef struct resstats_t
{
    int re_errors;
    int re_nu_look;
    int re_na_look;
    int re_replies;
    int re_requests;
    int re_resends;
    int re_sent;
    int re_timeouts;
} resstats_t;

void make_ipv6_addr(char * textname, char * address, int nibble);

class Resolver
{
    public:
        Resolver(PRIntervalTime timeout );
        ~Resolver();
        PRBool process(DNSSession* session);
        PRIntervalTime getDefaultTimeout();
        DNSSessionHash& getHTable();
        PRLock* getDnsLock();
        resstats_t* GetInfo();
        static void run(void* arg);
        void wait();
        void started();
        void error();
        void DNS_manager();
        PRBool hasStarted() const;
        PRFileDesc* ar_init(int op);
        PRFileDesc* ar_open();
        void ar_close();
        int ar_send_res_msg(char *msg, int len, int rcount);
        int ar_query_name(const char *name, int xclass, int type, DNSSession* session);
        int do_query_name(resinfo_t *resi, const char *name, DNSSession* session, int querytype);
        int do_query_number(resinfo_t *resi, char *numb, DNSSession* session, int addrtype);
        int ar_resend_query(DNSSession* session);
        int ar_procanswer(DNSSession *rptr, HEADER *hptr, u_char *buf, u_char *eob);
        void ar_receive();
        struct hostent* ar_answer(DNSSession* session, int *resend, char *reip, int size);
        PRInt32 getNameLookups() const;
        PRInt32 getAddrLookups() const;
        PRInt32 getCurrentLookups() const;

    protected:
        int ar_vc;
        PRBool ok;
        char ar_domainname[MAXDNAME];
        resstats_t ar_reinfo;
        DNSSessionHash DNShash; // hash table of DNS sessions
        PRIntervalTime ar_default_workerthread_timeout;
        PRLock* dnslock;
        PRCondVar* initcvar;
        PRBool awake;
        PRFileDesc* afd; // socket connection to the DNS
        PRInt32 namelookups;
        PRInt32 addrlookups;
        PRInt32 curlookups;
};

const PRInt32 ARES_CALLINIT = 2;
const PRInt32 ARES_INITSOCK = 4;
const PRInt32 ARES_INITDEBG = 8;
const PRInt32 ARES_INITCACH = 16;

short GetShort(unsigned char* in);
long GetLong(unsigned char* in);

#endif

