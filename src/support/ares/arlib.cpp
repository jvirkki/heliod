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

extern "C"
{
#include <memory.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <errno.h>
#ifdef Linux
#include <dlfcn.h>
typedef int (res_ninit_t)(res_state);
#endif
}

#include "arapi.h"
#include "arlib.h"

#ifdef HPUX
#ifndef IN6ADDRSZ
#define IN6ADDRSZ 16
#endif
extern "C"
{
int res_init(void);
int res_mkquery(int, const char *, int, int, const char *, int, struct rrec *, unsigned char *, int);
int dn_skipname(const unsigned char *, const unsigned char *);
int dn_expand(const unsigned char*, const unsigned char*, const unsigned char*, char*, int);
}
#endif

static char hex_digits[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

#ifdef Linux
static inline void res_init_tls()
{
    /*
     * XXX glibc-2.2.4 maintains a separate _res per thread, but res_init
     * only seems to work for the first thread's _res.
     */
    if (!(_res.options & RES_INIT))
    {
        static res_ninit_t *pres_ninit = (res_ninit_t *)-1;
        if (pres_ninit == (res_ninit_t *)-1)
        {
            res_ninit_t *fn = (res_ninit_t *)dlsym(NULL, "res_ninit");
            if (fn == NULL)
                fn = (res_ninit_t *)dlsym(NULL, "__res_ninit");
            pres_ninit = fn;
        }

        /* If there is a res_ninit, use it to initialize this thread's _res */
        if (pres_ninit != NULL)
            (*pres_ninit)(&_res);

        PR_ASSERT(_res.options & RES_INIT);
    }
}
#endif

// constructor for forward DNS lookup (name resolution)
DNSSession :: DNSSession(const char* name, PRUint16 family, PRIntervalTime tm,
                   char* buf, PRIntn bufsize, PRHostEnt* hentp)
{
    common(tm);
    ipfamily = family;
    hostname = name;
    ip = NULL;
    buffer = buf;
    buffersize = bufsize ;
    hostentp = hentp;
    qtype = FORWARD_LOOKUP;
}

// constructor for reverse DNS lookup (IP resolution)
DNSSession :: DNSSession(const PRNetAddr* addrp, PRIntervalTime tm,
                   char* buf, PRIntn bufsize, PRHostEnt* hentp)
{
    common(tm);
    ipfamily = 0;
    hostname = NULL;
    ip = addrp;
    buffer = buf;
    buffersize = bufsize ;
    hostentp = hentp;
    qtype = REVERSE_LOOKUP;
}

char DNSSession :: decrRetries()
{
    --re_retries;
    return re_retries;
}

void DNSSession :: setRetries(char r)
{
    re_retries = r;
}

datapacket* DNSSession :: getData()
{
    return re_data;
}

hostent* DNSSession :: getArhost()
{
    return &ar_host;
}

hent& DNSSession :: getHent()
{
    return re_he;
}

in6_addr* DNSSession :: getReaddr()
{
    return &re_addr;
}

char DNSSession :: getReaddrtype()
{
    return re_addrtype;
}

void DNSSession :: setReaddrtype(char t)
{
    re_addrtype = t;
}


void DNSSession :: incrSrch()
{
    re_srch ++;
}

char DNSSession :: getSrch() const
{
    return re_srch;
}

void DNSSession :: setType(char t)
{
    re_type = t;
}

char DNSSession :: getType() const
{
    return re_type;
}

void DNSSession :: common(PRIntervalTime tm)
{
    starttime = PR_IntervalNow();
    timeout = tm;
    timedout = PR_FALSE;
    sendfailed = PR_FALSE;
    id = 0;
    lock = PR_NewLock();
    PR_ASSERT(lock);
    cvar = PR_NewCondVar(lock);
    PR_ASSERT(cvar);
    status = DNS_INIT;
    awake = PR_FALSE;
    
    // query stuff

    re_retries = _res.retry;
    re_sends = 1;
    re_resend  = 1;
    re_he.h_addr_list[0] = 0;
    re_data = NULL;
    re_sent = 0;
    re_srch = 0;
    re_type = 0;
    memset(re_name, 0, sizeof(re_name));
    memset((void*)&re_he, 0, sizeof(re_he));
    memset((void*)&ar_host, 0, sizeof(ar_host));
}

void DNSSession :: setRename(const char* inname)
{
    strncpy(re_name, inname, sizeof(re_name)-1);
}

const char* DNSSession :: getRename() const
{
    return re_name;
}

DNSSession :: ~ DNSSession()
{
    //PR_ASSERT(awake);
    PR_DestroyCondVar(cvar);
    PR_DestroyLock(lock);
    hostname = "grmblll";
    free_packet_list();

    PRInt32 host_cnt = 0;
    while (re_he.h_addr_list[host_cnt])
    {
        free(re_he.h_addr_list[host_cnt++]);
    }

    if (re_he.h_name)
    {
        free(re_he.h_name);
    }
    
    ar_free_hostent(&ar_host, 0);
}

int DNSSession :: getID() const
{
    return id;
}

char DNSSession :: getSends() const
{
    return re_sends;
}
 
void DNSSession :: setSends(char c)
{
    re_sends = c;
}

void DNSSession :: incrSends()
{
    re_sends++;
}

char DNSSession :: getResends() const
{
    return re_resend;
}

void DNSSession :: setResends(char c)
{
    re_resend = c;
}

PRUint16 DNSSession :: getFamily() const
{
    return ipfamily;
}

querytype DNSSession :: getQType() const
{
    return qtype;
}

const PRNetAddr* DNSSession :: getIP() const
{
    return ip;
}

PRHostEnt* DNSSession :: getHentp() const
{
    return hostentp;
}

char* DNSSession :: getBuffer() const
{
    return buffer;
}

PRIntn DNSSession :: getBufferSize() const
{
    return buffersize;
}

const char* DNSSession :: getName() const
{
    return hostname;
}

void DNSSession :: setID(int newid)
{
    id = newid;
}

void DNSSession :: addSent(int sent)
{
    re_sent += sent;
}

void DNSSession :: update ( PRIntervalTime now )
{
    if (now - starttime > timeout)
    {
        timedout = PR_TRUE;
    }
}

PRBool DNSSession :: hasTimedOut()
{
    return timedout;
}

void DNSSession :: setSendFailed()
{
    sendfailed = PR_TRUE;
}

PRBool DNSSession :: hasSendFailed()
{
    return sendfailed;
}

void DNSSession :: wait()
{
    PR_Lock(lock);
    while ( PR_FALSE == awake ) 
    {
        PR_WaitCondVar(cvar, PR_INTERVAL_NO_TIMEOUT);
    }
    awake = PR_FALSE;
    PR_Unlock(lock);
}

void DNSSession :: error()
{
    // set status
    PR_Lock(lock);
    status = DNS_ERROR;
    awake = PR_TRUE;
    PR_NotifyCondVar(cvar);
    PR_Unlock(lock);
}

void DNSSession :: expire()
{
    // set status
    PR_Lock(lock);
    status = DNS_TIMED_OUT;
    awake = PR_TRUE;
    PR_NotifyCondVar(cvar);
    PR_Unlock(lock);
}

void DNSSession :: complete()
{
    // set status
    PR_Lock(lock);
    status = DNS_COMPLETED;
    awake = PR_TRUE;
    PR_NotifyCondVar(cvar);
    PR_Unlock(lock);
}

void DNSSession ::  setStatus(DNSStatus val)
{
    status = val;
}

DNSStatus DNSSession :: getStatus() const
{
    return status;
}

DNSSessionListElement :: DNSSessionListElement(DNSSessionListElement* p,
                              DNSSessionListElement* n, DNSSession* s)
{
    session = s;
    prev = p;
    next = n;
}

DNSSessionListElement :: ~DNSSessionListElement()
{
}

DNSSessionListElement* DNSSessionListElement :: getNext() const
{
    return next;
}

DNSSessionListElement* DNSSessionListElement :: getPrev() const
{
    return prev;
}

DNSSession* DNSSessionListElement :: getSession() const
{
    return session;
}

void DNSSessionListElement :: setNext(DNSSessionListElement* n)
{
    next = n;
}

void DNSSessionListElement :: setPrev(DNSSessionListElement* p)
{
    prev = p;
}

DNSSessionList :: DNSSessionList() 
{
    total = 0;
    head = NULL;
}

void DNSSessionList :: add(DNSSession* s)
{
    if (0 == total)
    {
        PR_ASSERT(head == NULL);
        head = new DNSSessionListElement(NULL, NULL, s);
        total ++;
    }
    else
    {
        PR_ASSERT(head);
        DNSSessionListElement* newelement =
            new DNSSessionListElement(NULL, head, s);
        head->setPrev(newelement);
        head = newelement;
        total++;
    }
}

PRBool DNSSessionList :: remove(DNSSession* s)
{
    PR_ASSERT(s);
    PR_ASSERT(head);
    PR_ASSERT(total);
    PRBool removed = PR_FALSE;
    DNSSessionListElement* element = head;
    while (element)
    {
        if (s == element->getSession())
        {
            if (element->getPrev())
            {
                DNSSessionListElement* prev = element->getPrev();
                DNSSessionListElement* next = element->getNext();
                prev->setNext(next);
                if (next)
                {
                    next->setPrev(prev);
                }
                delete(element);
            }
            else
            {
                // first element
                PR_ASSERT(element == head);
                head = element->getNext();
                if (head)
                {
                    head->setPrev(NULL);
                }
                delete(element);                
            }
            removed = PR_TRUE;
            break;
        }
        element = element->getNext();
    }
    if (removed)
    {
        total--;
        PR_ASSERT(total>=0);
        if (0 == total)
        {
            PR_ASSERT(NULL == head);
        }
    }
    return removed;
}

PRBool DNSSessionList :: remove(int id) // deletes this session from the list
{
    PRBool removed = PR_FALSE;
    DNSSession* session = lookup(id);
    if (session)
    {
        removed = remove(session);
    }
    return removed;
}

DNSSession* DNSSessionList :: lookup(int id)
{
    DNSSessionListElement* element = head;
    while (element)
    {
        DNSSession* session = element->getSession();
        if (id == session->getID())
        {
            return session;
        }
        element = element->getNext();
    }
    return NULL;
}

PRInt32 DNSSessionList :: numberOfElements() const
{
    return total;
}

DNSSessionHash :: DNSSessionHash()
{
    lock = PR_NewLock();
    total = 0;
    bucket = 0;
    nextitem = NULL;
    lastsession = NULL;

}

// check hash table consistency

void DNSSessionHash :: check()
{
    PR_Lock(lock);
    PRInt32 temptotal = 0;
    PRInt32 i;
    for (i=0;i<HASH_SIZE;i++)
    {
        temptotal += buckets[i].numberOfElements();
    }
    PR_ASSERT(total == temptotal);
    PR_Unlock(lock);
}

DNSSessionHash :: ~ DNSSessionHash()
{
    PR_DestroyLock(lock);
}

void DNSSessionHash :: add(DNSSession* session)
{
    PR_Lock(lock);
    int newbucket = session->getID() % HASH_SIZE;
    buckets[newbucket].add(session);
    total ++ ;
    PR_Unlock(lock);
}

void DNSSessionHash :: remove(DNSSession* session)
{
    PR_Lock(lock);
    int newbucket = session->getID() % HASH_SIZE;
    buckets[newbucket].remove(session);
    total --;
    PR_Unlock(lock);
}

DNSSession* DNSSessionHash :: lookup(int id)
{
    PR_Lock(lock);
    int newbucket = id  % HASH_SIZE;
    DNSSession* session = buckets[newbucket].lookup(id);
    PR_Unlock(lock);
    return session;
}

void DNSSessionHash :: start_enum()
{
    PR_Lock(lock);
    bucket = 0;
    nextitem = buckets[bucket].getHead();
    lastsession = NULL;
}

DNSSession* DNSSessionHash :: getNextSession()
{
    DNSSession* session = NULL;
    while (bucket < HASH_SIZE)
    {
        if (nextitem)
        {
            // found the session
            session = nextitem->getSession();
            // move to the next item for the next call to getNextSession
            nextitem = nextitem->getNext();
            break;
        }
        else
        {
            // try the next bucket
            bucket++;
            if (bucket <HASH_SIZE)
            {
                nextitem = buckets[bucket].getHead();
            }
            else
            {
                // done with all buckets, break out
                break;
            }
        }
    }
    lastsession = session;
    return session;
}

void DNSSessionHash :: end_enum()
{
    PR_Unlock(lock);
    bucket = 0;
    nextitem = NULL;
    lastsession = NULL;
}

void DNSSessionHash :: removeCurrent()
{
    if (lastsession)
    {
        buckets[bucket].remove(lastsession);
        total --;
        lastsession = NULL;
    }
}

DNSSessionListElement* DNSSessionList :: getHead()
{
    return head;
}

Resolver :: Resolver(PRIntervalTime timeout)
{
    curlookups = namelookups = addrlookups = 0;
    ar_vc = 0;

    ok = PR_FALSE;

    ar_default_workerthread_timeout = timeout;

    memset(&ar_reinfo, 0, sizeof(ar_reinfo));

    dnslock = PR_NewLock();

    initcvar = PR_NewCondVar(dnslock);

    awake = PR_FALSE;

    afd = NULL;

    // then create a DNS manager background thread

    PRThread* ar_worker = PR_CreateThread(PR_SYSTEM_THREAD,
        run,
        this,
        PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD,
        PR_JOINABLE_THREAD,
        0);
    
    if (ar_worker)
    {
        wait();

        if (!ok)
        {
            PR_JoinThread(ar_worker);
        }
    }
}

void Resolver :: wait()
{
    PR_Lock(dnslock);
    while (PR_FALSE == awake) 
    {
        PR_WaitCondVar(initcvar, PR_INTERVAL_NO_TIMEOUT);
    }
    awake = PR_FALSE;
    PR_Unlock(dnslock);
}

void Resolver :: started()
{
    PR_Lock(dnslock);
    awake = PR_TRUE;
    ok = PR_TRUE; // successful start, async DNS is now enabled
    PR_NotifyCondVar(initcvar);
    PR_Unlock(dnslock);
}

void Resolver :: error()
{
    PR_Lock(dnslock);
    awake = PR_TRUE;
    ok = PR_FALSE; // couldn't initialize resolver
    PR_NotifyCondVar(initcvar);
    PR_Unlock(dnslock);
}

PRBool Resolver :: hasStarted() const
{
    return ok;
}

Resolver :: ~Resolver()
{
}

/*
 * ar_open
 *
 * Open a socket to talk to a name server with.
 * Check _res.options to see if we use a TCP or UDP socket.
 */

PRFileDesc* Resolver :: ar_open()
{
    if (NULL == afd && _res.nscount > 0)
    {
        if (_res.options & RES_USEVC)
        {
            struct    sockaddr_in *sip;
            int    i = 0;

            sip = _res.nsaddr_list;
            ar_vc = 1;
            afd = PR_NewTCPSocket();

            /*
                         * Try each name server listed in sequence until we
                         * succeed or run out.
                         */
            while (PR_Connect(afd, (PRNetAddr *)sip++,
                PR_INTERVAL_NO_TIMEOUT))
            {
                PR_Close(afd);
                afd = NULL;
                if (i++ >= _res.nscount)
                    break;
                afd = PR_NewTCPSocket();
            }
        }
        else
        {
            afd = PR_NewUDPSocket();
        }
    }
    return afd;
}

/*
 * ar_close
 *
 * Closes and flags the ARLIB socket as closed.
 */

void Resolver :: ar_close()
{
    PR_Close(afd);
    afd = NULL;
    return;
}

PRIntervalTime Resolver :: getDefaultTimeout()
{
    return ar_default_workerthread_timeout;
}
        
DNSSessionHash& Resolver :: getHTable()
{
    return DNShash;
}

PRLock* Resolver :: getDnsLock()
{
    return dnslock;
}

void Resolver :: run(void* arg)
{
    Resolver* ptr = (Resolver*) arg;
    ptr->DNS_manager();
}

void Resolver :: DNS_manager()
{
    // setup a connection to the DNS (UDP or TCP)

    PRFileDesc* fd = ar_init(ARES_CALLINIT|ARES_INITSOCK);

    if (!fd)
    {
        error();
        return;
    }

    started();

    PRIntervalTime delay = PR_SecondsToInterval(1);
    PRPollDesc pfd;

    pfd.fd = afd;
    pfd.in_flags = PR_POLL_READ;

    // wake up periodically to expire clients

    while(PR_TRUE)
    {        
        switch (PR_Poll(&pfd, 1, delay))
        {
            case -1: // fail
            case 0:  // timeout
                break;

            default:
                // if there's data, let ar_receive figure out where to route it
                // read response from the DNS server
                if (pfd.out_flags & PR_POLL_READ)
                    ar_receive();
                break;
        }
        
        PRIntervalTime now = PR_IntervalNow();

        DNShash.start_enum();
        DNSSession* session;

        // enumerate session hash table 
        while (session = DNShash.getNextSession())
        {
            if (PR_TRUE == session->hasSendFailed() )
            {
                DNShash.removeCurrent(); // remove this session from the hash table
                session->error(); // set status and wake up client
                continue;
            }

            session->update(now); // update remaining time for each session
            if (PR_TRUE == session->hasTimedOut() )
            {
                DNShash.removeCurrent(); // remove this session from the hash table
                session->expire(); // set status and wake up client
                continue;
            }
        }
        DNShash.end_enum();
        DNShash.check();
    }
}



int         ar_default_retry;
const char* AR_DOT = ".";
const PRInt32 DEFAULT_RETRY = 20; /* # of seconds to wait for resolver to return data */
PRFileDesc *afd = NULL;

// legacy code

/*
 * ar_init
 *
 * Initializes the various ARLIB internal variables and related DNS
 * options for res_init().
 *
 * Returns NULL or the pointer to PRFileDesc which contain a socket 
 * opened for use with talking to name servers if 0 is passed or 
 * ARES_INITSOCK is set.
 */

PRFileDesc* Resolver :: ar_init(int op)
{
    PRFileDesc* ret = NULL;

    if (op & ARES_CALLINIT)
    {
        PR_Lock(dnslock);
        res_init();
#ifdef Linux
        res_init_tls();
#endif
        PR_Unlock(dnslock);
        strcpy(ar_domainname, AR_DOT);
        strncat(ar_domainname, _res.defdname, sizeof(ar_domainname)-2);
    }

    if (op & ARES_INITSOCK)
        ret = afd = ar_open();

    if (op & ARES_INITDEBG)
        _res.options |= RES_DEBUG;

    if (op == 0)
        ret = afd;

    return ret;
}

resstats_t* Resolver :: GetInfo()
{
    return &ar_reinfo;
}

PRBool Resolver :: process(DNSSession* session)
{    
    PR_AtomicIncrement(&curlookups);
    PRBool locstatus = PR_FALSE;

    char host[MAXDNAME];
    resinfo_t resi;
    resinfo_t* rp = &resi;
    struct hostent* hp = NULL;
    PRInt32 rv = 0;
    int querytype = 0;

    if (session)
    {
        session->setStatus(DNS_IN_PROCESS);

        if (FORWARD_LOOKUP == session->getQType ())
        {   
            PR_AtomicIncrement(&namelookups);
    
            memset((char *)rp, 0, sizeof(resi));
            ar_reinfo.re_na_look++;
            strncpy(host, session->getName(), 64);
            host[64] = '\0';
    
            locstatus = PR_TRUE;
            switch(session->getFamily())
            {
                case PR_AF_INET:
                    querytype = T_A;
                    break;
                case PR_AF_INET6:
                    querytype = T_AAAA;
                    break;
                default:
                    locstatus = PR_FALSE;
            }
    
            // Makes and sends the query
            // the session gets automatically added to the hash table
            if (PR_TRUE == locstatus)
            {
                if (do_query_name(rp, host, session, querytype) == -1)
                {
                    PR_SetError(PR_BAD_ADDRESS_ERROR, 0);
                    locstatus = PR_FALSE;
                }
            }
        }

        else

        if (REVERSE_LOOKUP == session->getQType ())
        {
            PR_AtomicIncrement(&addrlookups);

            memset((char *)rp, 0, sizeof(resi));
            ar_reinfo.re_na_look++;
            const PRNetAddr* addrp = session->getIP();

            locstatus = PR_TRUE;
            switch(addrp->raw.family)
            {
                case PR_AF_INET: 
                    rv = do_query_number(rp, (char *) &addrp->inet.ip, session, PR_AF_INET);
                    break;
                case PR_AF_INET6: 
                    rv = do_query_number(rp, (char *) &addrp->ipv6.ip, session, PR_AF_INET6);
                    break;
                default:
                    locstatus = PR_FALSE;
                    PR_SetError(PR_BAD_ADDRESS_ERROR, 0);
                    break;
            }
            if (-1 == rv)
            {
                locstatus = PR_FALSE;
            }
        }
    }

    if (PR_TRUE == locstatus)
    {
        locstatus = PR_FALSE;
        int resend;

        do {
            resend = 0;

            session->wait();
        
            if (DNS_COMPLETED == session->getStatus() )
            {
                // Process answer and return meaningful data to caller.
    
                if (FORWARD_LOOKUP == session->getQType())
                {
                    if ((hp = ar_answer(session, &resend, NULL, 0)) != NULL)
                    {
                        PRInt32 rv = CopyHostent(session->getHentp(), hp,
                                                 session->getBuffer(),
                                                 session->getBufferSize());
                        if (rv != PR_AR_OK)
                            PR_SetError(rv, 0);
                        else
                            locstatus = PR_TRUE;
                    }
                }
                else
                if (REVERSE_LOOKUP == session->getQType())
                {
                    if (hp = ar_answer(session, &resend, NULL, 0))
                    {
                        PRInt32 rv = CopyHostent(session->getHentp(), hp,
                                                 session->getBuffer(),
                                                 session->getBufferSize());
                        if (rv != PR_AR_OK)
                            PR_SetError(rv, 0);
                        else
                            locstatus = PR_TRUE;
                    }
                }
            }
            else
            if (DNS_TIMED_OUT == session->getStatus())
            {
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);                
            }
            else
            {
                PR_SetError(PR_BAD_ADDRESS_ERROR, 0);
            }
        } while (resend);
    }

    PR_AtomicDecrement(&curlookups);
    return locstatus;
}

/*
 * ar_send_res_msg
 *
 * When sending queries to nameservers listed in the resolv.conf file,
 * don't send a query to every one, but increase the number sent linearly
 * to match the number of resends. This increase only occurs if there are
 * multiple nameserver entries in the resolv.conf file.
 * The return value is the number of messages successfully sent to 
 * nameservers or -1 if no successful sends.
 */
int Resolver :: ar_send_res_msg(char *msg, int len, int rcount)
{
    int    i;
    int    sent = 0;

#ifdef HPUX
    /* VB: _res on HP is thread specific and needs to be initialized 
       on every thread */
    if (!(_res.options & RES_INIT)) {
        res_init();
    }
#endif
#ifdef Linux
    res_init_tls();
#endif

    if (!msg)
        return -1;

    if (_res.options & RES_PRIMARY)
        rcount = 1;
    rcount = (_res.nscount > rcount) ? rcount : _res.nscount;

    if (ar_vc)
    {
        ar_reinfo.re_sent++;
        sent++;
        if (PR_Send(afd, msg, len, 0, PR_INTERVAL_NO_TIMEOUT) == -1)
        {
            int errtmp = errno;
            PR_ASSERT(0);
            (void)PR_Close(afd);
            errno = errtmp;
            afd = NULL;
        }
    }
    else
        for (i = 0; i < rcount; i++)
        {
            struct sockaddr_in sin = _res.nsaddr_list[i];
            // XXX jpierre is this legal ???
            PRNetAddr *paddr = (PRNetAddr *)&sin;
#ifdef AIX
            PRNetAddr addr;
            /* For AIX struct sockaddr_in { uchar_t sin_len; 
             *                      sa_family_t sin_family;
             *                      in_port_t sin_port;
             * struct in_addr sin_addr; uchar_t sin_zero[8]; };
             * struct in_addr { in_addr_t s_addr; };
             * typedef uint32_t in_addr_t;
             *
             * In NSPR PRNetAddr is :
             *  union PRNetAddr { struct { PRUint16 family; 
             *                             char data[14]; } raw;
             *   struct { PRUint16 family; PRUint16 port; 
             *           PRUint32 ip; char pad[8]; } inet; ... }
             */
            PR_ASSERT(sin.sin_family == AF_INET);
            /* xxx strange when request is sent to IPV6 address 
             * then also address family is AF_INET 
             */
            memset(&addr, 0, sizeof(addr));
            addr.raw.family = sin.sin_family;
            addr.inet.family = sin.sin_family;
            addr.inet.port  = sin.sin_port;
            addr.inet.ip = sin.sin_addr.s_addr;
            memcpy(addr.inet.pad, sin.sin_zero, 8);
            paddr = &addr;
#endif
            if (PR_SendTo(afd, msg, len, 0,
                paddr,
                PR_INTERVAL_NO_TIMEOUT) == len) {
                ar_reinfo.re_sent++;
                sent++;
            }
            else
            {
                PR_ASSERT(0);
            }
        }
    return (sent) ? sent : -1;
}

/*
 * ar_query_name
 *
 * generate a query based on class, type and name.
 */
int Resolver :: ar_query_name(const char *name, int xclass, int type, DNSSession* session)
{
    u_char buf[MAXPACKET];
    int r,s;
    HEADER *hptr;

    memset(buf, 0, sizeof(buf));
    PR_Lock(dnslock);
    r = res_mkquery(QUERY, name, xclass, type, NULL, 0, NULL,
        (unsigned char*)buf, sizeof(buf));
    PR_Unlock(dnslock);

    if (r <= 0)
    {
#ifndef XP_OS2
        h_errno = NO_RECOVERY;
#endif
        return r;
    }
    hptr = (HEADER *)buf;

    /* Add to chains */
    session->setID(ntohs(hptr->id));
    DNShash.add(session);

    session->setStatus(DNS_SENT);
    s = ar_send_res_msg((char*)buf, r, session->getSends());

    if (s == -1)
    {
#ifndef XP_OS2
        h_errno = TRY_AGAIN;
#endif
// XXXcelving race
//      DNShash.remove(session);
//      return -1;
        /*
         * After a DNSSession has been added to the DNShash, only DNS_manager
         * can safely remove it. Flag the DNSSession so that DNS_manager
         * removes it ASAP.
         */
        session->setSendFailed();
    }
    else
    {
        session->addSent(s);
    }
    return 0;
}

int Resolver :: do_query_name(resinfo_t *resi, const char *name, DNSSession* session, int querytype)
{
    char    hname[MAXDNAME];
    int    len;

    len = strlen((char *)strncpy(hname, session->getName(), sizeof(hname)-1));

    /*
     * Store the name passed as the one to lookup and generate other host
     * names to pass onto the nameserver(s) for lookups.
     */
    session->setType(querytype);
    session->setRename(name);

    return (ar_query_name(hname, C_IN, querytype, session));
}

/*
 * do_query_number
 *
 * Use this to do reverse IP# lookups.
 */
int Resolver :: do_query_number(resinfo_t *resi, char *numb, DNSSession* session, int addrtype)
{
    unsigned char* cp = NULL;
    char ipbuf[256];
    int length = 0;

    switch (addrtype)
    {
        case PR_AF_INET:
            /*
             * Generate name in the "in-addr.arpa" domain.  No addings bits to 
             * this name to get more names to query!.
             */
            cp = (unsigned char *)numb;
            (void)sprintf(ipbuf,"%u.%u.%u.%u.in-addr.arpa.",
                (unsigned int)(cp[3]), (unsigned int)(cp[2]),
                (unsigned int)(cp[1]), (unsigned int)(cp[0]));
            length = INADDRSZ;
            break;
       case PR_AF_INET6:
            make_ipv6_addr(ipbuf, numb, 1);
            length = IN6ADDRSZ;
            break;
       default:
            return -1;
            break;
    }

    hent& re_he = session->getHent();

    session->setType(T_PTR);
    re_he.h_length = length;
    memcpy((char *)session->getReaddr(), numb, re_he.h_length);
    re_he.h_addr_list[0] = (char *)malloc(length);
    memcpy(re_he.h_addr_list[0], numb, length);
    re_he.h_name = NULL;
    session->setReaddrtype(addrtype);
    re_he.h_addrtype = addrtype;
    return (ar_query_name(ipbuf, C_IN, T_PTR, session));
}

/*
 * ar_resent_query
 *
 * resends a query.
 */
int Resolver :: ar_resend_query(DNSSession* session)
{
    if (!session->getResends())
        return -1;

    switch(session->getType())
    {
    case T_PTR:
        ar_reinfo.re_resends++;
        /* XXXMB - I think this should be:  (char *)&(session->re_addr) */
        return do_query_number(NULL, (char *)(session->getReaddr()), session, session->getReaddrtype());
    case T_A:
        ar_reinfo.re_resends++;
        return do_query_name(NULL, session->getName(), session, T_A);
    case T_AAAA:
        ar_reinfo.re_resends++;
        return do_query_name(NULL, session->getName(), session, T_AAAA);
    default:
        break;
    }

    return -1;
}

/*
 * ar_procanswer
 *
 * process an answer received from a nameserver.
 */

int Resolver :: ar_procanswer(DNSSession* session, HEADER* hptr, u_char* buf, u_char* eob)
{
    u_char* cp = NULL;
    char** alias = NULL;
    int xclass, type = 0, dlen, len, ans = 0, n = 0;
    char** adr = NULL;
    char ar_hostbuf[MAXDNAME];   /* Must be on stack */
    unsigned int ancount = 0 , arcount = 0 , nscount = 0, qdcount = 0;
    int host_cnt = 0;

    /*
     * convert things to be in the right order.
     */
    ancount = ntohs(hptr->ancount);
    arcount = ntohs(hptr->arcount);
    nscount = ntohs(hptr->nscount);
    qdcount = ntohs(hptr->qdcount);

    cp = buf + HFIXEDSZ;
    hent& re_he = session->getHent();
    adr = re_he.h_addr_list;

#ifdef DNS_DEBUG
    Print_query(buf, eob, 1);
#endif

    while (*adr)
    {
        adr++;
        host_cnt++;
    }

    alias = re_he.h_aliases;
    while (*alias)
    {
        alias++;
    }

    /*
     * Skip over the original question.
     */
    while (qdcount > 0)
    {
        qdcount--;
        cp += dn_skipname((const unsigned char*)cp, (const unsigned char*)eob) + QFIXEDSZ;
    }
    /*
     * process each answer sent to us. blech.
     */
    while ((ancount > 0) && (cp < eob))
    {
        ancount--;
        PR_Lock(dnslock);
        n = dn_expand((const unsigned char*)buf, (const unsigned char*)eob, (const unsigned char*)cp, (char*)ar_hostbuf, sizeof(ar_hostbuf));
        PR_Unlock(dnslock);
        cp += n;
        if (n <= 0)
            return ans;

        ans++;
        /*
         * 'skip' past the general dns crap (ttl, xclass, etc) to get
         * the pointer to the right spot.  Some of thse are actually
         * useful so its not a good idea to skip past in one big jump.
         */
        type = (int)GetShort(cp);
        cp += sizeof(short);
        xclass = (int)GetShort(cp);
        cp += sizeof(short);
        /* ttl = */ (void)(PRUint32)GetLong(cp);
        cp += INT32SZ;
        dlen =  (int)GetShort(cp);
        cp += sizeof(short);
        session->setType(type);

        switch(type)
        {
            case T_A :
            {
                re_he.h_addrtype = PR_AF_INET;
                re_he.h_length = INADDRSZ;
                re_he.h_addr_list[host_cnt] = (char *)malloc(INADDRSZ);
                memcpy(re_he.h_addr_list[host_cnt], cp, dlen);
                re_he.h_addr_list[++host_cnt] = NULL;
                cp += dlen;
                len = strlen(ar_hostbuf);
                if (!re_he.h_name)
                {
                    re_he.h_name = (char *)malloc(len+1);
                    (void)strcpy(re_he.h_name, ar_hostbuf);
                }
                break;
            }

            case T_AAAA :
            {
    	        char buf[1024];
    
                re_he.h_addrtype = PR_AF_INET6;
                re_he.h_length = IN6ADDRSZ;
                re_he.h_addr_list[host_cnt] = (char *)malloc(16);
                memcpy(re_he.h_addr_list[host_cnt], cp, dlen);
                re_he.h_addr_list[++host_cnt] = NULL;
                cp += dlen;
                len = strlen(ar_hostbuf);
                if (!re_he.h_name)
                {
                    re_he.h_name = (char *)malloc(len+1);
                    (void)strcpy(re_he.h_name, ar_hostbuf);
                }
                break;
            }

            case T_PTR :
            {
                PR_Lock(dnslock);
                n = dn_expand((const unsigned char*)buf, (const unsigned char*)eob, (const unsigned char*)cp, (char*)ar_hostbuf, sizeof(ar_hostbuf));
                PR_Unlock(dnslock);
                if (n < 0)
                {
                    cp += n;
                    continue;
                }
                cp += n;
                len = strlen(ar_hostbuf)+1;
                /*
                 * copy the returned hostname into the host name
                 * or alias field if there is a known hostname
                 * already.
                 */
                if (!re_he.h_name)
                {
                    re_he.h_name = (char *)malloc(len);
                    (void)strcpy(re_he.h_name, ar_hostbuf);
                }
                else
                {
                    *alias = (char*)malloc(len);
                    if (!*alias)
                        return -1;
                    (void)strcpy(*alias++, ar_hostbuf);
                    *alias = NULL;
                }
                break;
            }

            case T_CNAME :
            {
                cp += dlen;
                if (alias >= &(re_he.h_aliases[MAXALIASES-1]))
                    continue;
                n = strlen(ar_hostbuf)+1;
                *alias = (char*)malloc(n);
                if (!*alias)
                    return -1;
                (void)strcpy(*alias++, ar_hostbuf);
                *alias = NULL;
                break;
            }

            default :
            {
                break;
            }
        }
    }

    return ans;
}

/*
 * ar_receive
 *
 * Get the answer from the DNS server and  wakeup the corresponding
 * thread which made the request.
 */

void Resolver :: ar_receive()
{
    char ar_rcvbuf[HFIXEDSZ + MAXPACKET];
    HEADER* hptr = NULL;        /* Name Server query header */
    DNSSession* session = NULL;
    int rc = 0;
    datapacket* newpkt = NULL;
    unsigned int id = 0;

    if (ar_vc)
        rc = PR_Recv(afd, ar_rcvbuf, HFIXEDSZ+MAXPACKET, 0, 
            PR_INTERVAL_NO_TIMEOUT);
    else
        rc = PR_Read(afd, ar_rcvbuf,  HFIXEDSZ+MAXPACKET);

    ar_reinfo.re_replies++;

    /* Create a reslist_pkt_t to queue against the reslist */
    newpkt = new datapacket();
    PR_ASSERT(newpkt);

    newpkt->next = NULL;
    if (rc >0)
    {
        // packet received

	    /*
	     * Wakeup corresponding thread.
	     */
	    hptr = (HEADER *)ar_rcvbuf;
	    /*
	     * convert things to be in the right order.
	     */
	    id = ntohs(hptr->id);
	    /*
	     * response for an id which we have already received an answer for
	     * just ignore this response.
	     */
	    session = DNShash.lookup(id);
	    if (!session)
        {            
            delete(newpkt);
	        /* Request no longer in queue, so just return. */
	        return;
	    }

        newpkt->buflen = rc;
        memset(newpkt, 0, HFIXEDSZ + MAXPACKET);
        memcpy(newpkt, ar_rcvbuf, rc);
    }
    else
    {
        // no packet received
        newpkt->buflen = 0;
    }

    // Append new data packet to end of known data packets
    session->reslist_add_pkt(newpkt);

    session->setStatus(DNS_RCVD);

    // Wakeup corresponding thread.

    DNShash.remove(session); // remove this completed session from the hash table
    session->complete();
}

void ar_free_hostent (struct hostent *hp, int flag)
{
    char **s = NULL;

    if (hp->h_name)
        (void)free(hp->h_name);

    if (s = hp->h_aliases) {
        /* The individual aliases were allocated in one chunk... */
        if (*s)
        {
            (void)free(*s++);
            s++;
        }
        (void)free(hp->h_aliases);
    }
    if (s = hp->h_addr_list)
    {
        /* The individual addresses were allocated in one chunk... */
        if (*s)
        {
            (void)free(*s);
            s++;
        }
        (void)free(hp->h_addr_list);
    }

    if (flag)
        free(hp);
}

/*
 * ar_answer
 *
 * Get an answer from a DNS server and process it.  If a query is found to
 * which no answer has been given to yet, copy its 'info' structure back
 * to where "reip" points and return a pointer to the hostent structure.
 */
struct hostent* Resolver :: ar_answer(DNSSession* session, int* resend, char* reip, int size)
{
    hent& re_he = session->getHent();
    HEADER* hptr = NULL;        /* Name Server query header */
    struct hostent* hp = NULL;
    char** s = NULL;
    unsigned long* adr = NULL;
    int i = 0 , n = 0 , a = 0;
    char** cp = NULL;

    if (session->getData() == NULL)
    {
        /* If re_data is NULL, we don't have a packet; must have timed out */
        if (session->decrRetries() <= 0)
        {
            ar_reinfo.re_timeouts++;
            *resend = 0;
        }
        else
        {
            session->incrSends();
            if (!ar_resend_query(session))
                *resend = 1;
        }
        return NULL;
    }

    *resend = 0;

    hptr = (HEADER *)session->getData()->rcvbuf;
    /*
     * response for an id which we have already received an answer for
     * just ignore this response.
     */
    if ((hptr->rcode != NOERROR) || (hptr->ancount == 0)) {
#ifndef XP_OS2
        switch (hptr->rcode) {
        case NXDOMAIN:
            h_errno = HOST_NOT_FOUND;
            break;
        case SERVFAIL:
            h_errno = TRY_AGAIN;
            break;
        case NOERROR:
            h_errno = NO_DATA;
            break;
        case FORMERR:
        case NOTIMP:
        case REFUSED:
        default:
            h_errno = NO_RECOVERY;
            break;
        }
#endif
        ar_reinfo.re_errors++;
        /*
        ** If a bad error was returned, we stop here and dont send
        ** send any more (no retries granted).
        */
        if (h_errno != TRY_AGAIN)
        {
            session->setResends(0);
            session->setRetries(0);
        }
        goto getres_err;
    }

    a = ar_procanswer(session, 
        hptr, 
        (u_char*)session->getData()->rcvbuf, 
        (u_char*)session->getData()->rcvbuf + session->getData()->buflen);
    if (a <= 0)
    {
        *resend=0;
        return NULL;
    }

    if ((session->getType() == T_PTR) && (_res.options & RES_CHECKPTR))
    {
        /*
         * For reverse lookups on IP#'s, lookup the name that is given
         * for the ip# and return with that as the official result.
         * -avalon
         */
        switch(session->getReaddrtype())
        {
            case PR_AF_INET:
                session->setType(T_A);
                break;
            case PR_AF_INET6:
                session->setType(T_AAAA);
                break;
            default:
                return NULL;
        }
        /*
         * Clean out the list of addresses already set, even though
         * there should only be one :)
         */
        adr = (unsigned long *)re_he.h_addr_list;
        while (*adr)
            *adr++ = 0L;
        /*
         * Lookup the name that we were given for the ip#
         */
        ar_reinfo.re_na_look++;
        session->setRename(re_he.h_name);
        re_he.h_name = NULL;
        re_he.h_addr_list[0] = 0;
        session->setRetries(_res.retry);
        session->setSends(1);
        session->setResends(1);
        ar_reinfo.re_na_look++;
        if (!ar_query_name(session->getName(), C_IN, session->getType(), session))
            *resend = 1;
        return NULL;
    }

    if (session && re_he.h_name == NULL)
        goto getres_err;

/*
    if (reip && session->re_rinfo.ri_ptr && size)
        memcpy(reip, session->re_rinfo.ri_ptr, 
            MIN(session->re_rinfo.ri_size, size));
*/
    /*
     * Clean up structure from previous usage.
     */
    hp = session->getArhost();
    memset((char *)hp, 0, sizeof(*hp));

    /*
     * Setup and copy details for the structure we return a pointer to.
     */
    hp->h_addrtype = re_he.h_addrtype;
    hp->h_length = re_he.h_length;
    hp->h_name = (char *)malloc(strlen(re_he.h_name)+1);
    (void)strcpy(hp->h_name, re_he.h_name);
    /*
     * Count IP#'s.
     */
    n = 0;
    for (cp = re_he.h_addr_list; *cp; cp++)
        n++;

    s = hp->h_addr_list = (char **)malloc((n + 1) * sizeof(char *));
    if (n)
    {
        char buf[512];
        *s = (char *)malloc(n * re_he.h_length);
        memcpy(*s, re_he.h_addr_list[0], 
            re_he.h_length);

        s++;
        for (i = 1; i < n; i++, s++) {
            *s = hp->h_addr + i * re_he.h_length;
            memcpy(*s, (char *)re_he.h_addr_list[i], 
                re_he.h_length);
        }
    }
    *s = NULL;
    /*
     * Count CNAMEs
     */
    for (i = 0, n = 0; i < MAXADDRS; i++, n++)
        if (!re_he.h_aliases[i])
            break;

    s = hp->h_aliases = (char **)malloc((n + 1) * sizeof(char *));
    for (i = 0; i < n; i++)
    {
        *s++ = re_he.h_aliases[i];
        re_he.h_aliases[i] = NULL;
    }
    *s = NULL;

    return hp;

getres_err:
    if (session)
    {
/*
        if (reip && session->re_rinfo.ri_ptr && size)
            memcpy(reip, session->re_rinfo.ri_ptr, 
                MIN(session->re_rinfo.ri_size, size));
*/
        if ((h_errno != TRY_AGAIN) &&
            (_res.options & (RES_DNSRCH|RES_DEFNAMES) ==
             (RES_DNSRCH|RES_DEFNAMES) )) {
            if (_res.dnsrch[session->getSrch()]) {
                session->setRetries(_res.retry);
                session->setSends(1);
                session->setResends(1);
                if (!ar_resend_query(session)) {
                    *resend = 1;
                    session->incrSrch();
                }
            }
        }
    }

    return NULL;
}

void DNSSession :: free_packet_list()
{
    datapacket* ptr = re_data;

    while (ptr)
    {
        datapacket* next = ptr->next;
        delete(ptr);
        ptr = next;
    }
}

/* Caller must hold the reslist->re_lock
 * add a pkt to the reslist
 */
void DNSSession :: reslist_add_pkt(datapacket *pkt)
{
    datapacket* ptr = re_data;

    /* must add to tail of list */

    while (ptr && ptr->next)
        ptr = ptr->next;

    pkt->next = NULL;
    if (ptr)
    {
        // add packet to the end of the list
        ptr->next = pkt;
    }
    else
    {
        // first packet
        re_data = pkt;
    }
}

/*
 * Mostly stolen from bind 9.0.1 code from byaddr.c
 */
void make_ipv6_addr(char  * textname, char * address, int nibble) {
    unsigned char *bytes;
    char * cp;
    int i;

    bytes = (unsigned char *)(address);

    if (nibble) {
            cp = textname;
            for (i = 15; i >= 0; i--) {
                    *cp++ = hex_digits[bytes[i] & 0x0f];
                    *cp++ = '.';
                    *cp++ = hex_digits[(bytes[i] >> 4) & 0x0f];
                    *cp++ = '.';
            }
            strcpy(cp, "ip6.int.");
    } else {
            cp = textname;
            *cp++ = '\\';
            *cp++ = '[';
            *cp++ = 'x';
            for (i = 0; i < 16; i += 2) {
                    *cp++ = hex_digits[(bytes[i] >> 4) & 0x0f];
                    *cp++ = hex_digits[bytes[i] & 0x0f];
                    *cp++ = hex_digits[(bytes[i+1] >> 4) & 0x0f];
                    *cp++ = hex_digits[bytes[i+1] & 0x0f];
            }
            strcpy(cp, "].ip6.arpa.");
    }
}


/*
** Allocate space from the buffer, aligning it to "align" before doing
** the allocation. "align" must be a power of 2.
*/
char *Alloc(PRIntn amount, char **bufp, PRIntn *buflenp, PRIntn align)
{
    char *buf = *bufp;
    PRIntn buflen = *buflenp;

    if (align && ((long)buf & (align - 1)))
    {
        PRIntn skip = align - ((ptrdiff_t)buf & (align - 1));
        if (buflen < skip) {
            return 0;
        }
        buf += skip;
        buflen -= skip;
    }
    if (buflen < amount)
    {
        return 0;
    }
    *bufp = buf + amount;
    *buflenp = buflen - amount;
    return buf;
}

/*
** Copy a hostent, and all of the memory that it refers to into
** (hopefully) stacked buffers.
*/

PRInt32 CopyHostent(PRHostEnt *to, struct hostent *from, char *buf, PRIntn bufsize)
{
    PRIntn len = 0, na = 0;
    char** ap = NULL;

    /* Do the easy stuff */
    to->h_addrtype = from->h_addrtype;
    to->h_length = from->h_length;

    /* Copy the official name */
    if (!from->h_name)
        return PR_BAD_ADDRESS_ERROR;
    len = strlen(from->h_name) + 1;
    to->h_name = Alloc(len, &buf, &bufsize, 0);
    if (!to->h_name)
        return PR_BAD_ADDRESS_ERROR;
    memcpy(to->h_name, from->h_name, len);

    /* Count the aliases, then allocate storage for the pointers */
    for (na = 1, ap = from->h_aliases; *ap != 0; na++, ap++);
    to->h_aliases = (char**) Alloc(na * sizeof(char*), &buf, &bufsize,
        sizeof(char**));
    if (!to->h_aliases)
        return PR_BAD_ADDRESS_ERROR;

    /* Copy the aliases, one at a time */
    for (na = 0, ap = from->h_aliases; *ap != 0; na++, ap++)
    {
        if (*ap)
        {
            len = strlen(*ap) + 1;
            to->h_aliases[na] = Alloc(len, &buf, &bufsize, 0);
            if (!to->h_aliases[na]) return PR_BAD_ADDRESS_ERROR;
            memcpy(to->h_aliases[na], *ap, len);
        } else
        {
            to->h_aliases[na] = 0;
        }
    }
    to->h_aliases[na] = 0;

    /* Count the addresses, then allocate storage for the pointers */
    for (na = 1, ap = from->h_addr_list; *ap != 0; na++, ap++);
    to->h_addr_list = (char**) Alloc(na * sizeof(char*), &buf, &bufsize,
        sizeof(char**));
    if (!to->h_addr_list) return PR_BAD_ADDRESS_ERROR;

    /* Copy the addresses, one at a time */
    for (na = 0, ap = from->h_addr_list; *ap != 0; na++, ap++)
    {
        if (*ap)
        {
            to->h_addr_list[na] = Alloc(to->h_length, &buf, &bufsize, 0);
            if (!to->h_addr_list[na]) return PR_BAD_ADDRESS_ERROR;
            memcpy(to->h_addr_list[na], *ap, to->h_length);
        } else
        {
            to->h_addr_list[na] = 0;
        }
    }
    to->h_addr_list[na] = 0;
    return PR_AR_OK;
}

PRInt32 Resolver :: getNameLookups() const
{
    return namelookups;
}

PRInt32 Resolver :: getAddrLookups() const
{
    return addrlookups;
}

PRInt32  Resolver :: getCurrentLookups() const
{
    return curlookups;
}

short GetShort(unsigned char* in)
{
    char myarray[2];
    myarray[0] = in[0];
    myarray[1] = in[1];
    return ntohs(*(short*)myarray);
}

long GetLong(unsigned char* in)
{
    char myarray[4];
    myarray[0] = in[0];
    myarray[1] = in[1];
    myarray[2] = in[2];
    myarray[3] = in[3];
    return ntohl(*(long*)myarray);
}

