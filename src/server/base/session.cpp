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

/*
 * session.c: Deal with virtual sessions
 * 
 * Rob McCool
 */


#include "base/session.h"
#include <base/pool.h>
#include <base/systhr.h>
#include <base/plist.h>
#include <frame/conf.h>
#include "ssl.h"
#include "secitem.h"
#include "base64.h"

#ifdef XP_UNIX
#include <arpa/inet.h>  /* inet_ntoa */
#include <netdb.h>      /* hostent stuff */
#include <netinet/in.h> /* ntohl */
#endif /* XP_UNIX */

#include "libaccess/nsauth.h"
#include "netio.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/daemonsession.h"
#include "base/sslconf.h"
#include "httpdaemon/vsconf.h"
#include "frame/filter.h"
#include "base/session.h"


static PRCallOnceType _session_thread_once;
static PRLock *_session_thread_lock;
static PRInt32 _session_thread_slots;
static PRUintn _session_thread_key;
static SlotDestructorFuncPtr* _session_thread_destructors;


/* ---------------------------- session_create ---------------------------- */

Session *session_alloc(SYS_NETFD csd, struct sockaddr_in *sac)
{
    NSAPISession *nsn = (NSAPISession *)PERM_MALLOC(sizeof(NSAPISession));
    nsn->thread_data = NULL;
    nsn->httpfilter = NULL;
    nsn->input_done = PR_FALSE;
    nsn->input_os_pos = 0;
    nsn->exec_rq = NULL;
    nsn->filter_rq = NULL;
    nsn->session_clone = PR_FALSE;
    nsn->received = 0;
    nsn->transmitted = 0;

    Session *ns = &nsn->sn;
    ns->csd = csd;
    ns->iaddr = sac->sin_addr;
    ns->csd_open = 1;
    ns->fill = 0;
    ns->ssl = 0; // mark SSL off by default
    ns->clientauth = 0; // mark clientauth off by default

    ns->subject = NULL;

    ns->pr_local_addr = NULL;
    ns->pr_client_addr = NULL;

    return ns;
}

static void session_fill_cla(pool_handle_t *pool, Session *sn)
{
    /* Initialize ACL authentication information */
    ClAuth_t *cla = (ClAuth_t *) pool_malloc(pool, sizeof(ClAuth_t));
    if (cla) {
        cla->cla_realm = 0; /* v2 ACL stuff, not used anymore */
        cla->cla_dns = 0;   /* v2 ACL stuff, not used anymore */
        cla->cla_uoptr = 0; /* v2 ACL stuff, not used anymore */
        cla->cla_goptr = 0; /* v2 ACL stuff, not used anymore */
        cla->cla_cert = 0;
        cla->cla_ipaddr = 0; /* v2 ACL stuff not supported anymore */
        sn->clauth = cla;
    }
}

NSAPI_PUBLIC Session *session_fill(Session *ns) 
{
    pool_handle_t *pool = pool_create();

    ns->pool = pool;
    systhread_setdata(getThreadMallocKey(), ns->pool);

    ns->client = pblock_create(SESSION_HASHSIZE);
    ns->inbuf = netbuf_open(ns->csd, NET_BUFFERSIZE);
    ns->fill = 1;

    char  session_ipaddr[sizeof "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255"];

    if (ns->pr_client_addr) {
        if (ns->pr_client_addr->raw.family == PR_AF_INET || ns->pr_client_addr->raw.family == PR_AF_INET6) {
            net_addr_to_string(ns->pr_client_addr, session_ipaddr, sizeof(session_ipaddr));
            pblock_kvinsert(pb_key_ip, session_ipaddr, strlen(session_ipaddr), ns->client);
        }
    } else {
        net_inet_ntoa(ns->iaddr, session_ipaddr);
        pblock_kvinsert(pb_key_ip, session_ipaddr, strlen(session_ipaddr), ns->client);
    }

    session_fill_cla(pool, ns);

    NSAPISession *nsn = (NSAPISession *)ns;
    nsn->thread_data = NULL;
    nsn->httpfilter = NULL;
    nsn->input_done = PR_FALSE;
    nsn->input_os_pos = 0;
    nsn->exec_rq = NULL;
    nsn->filter_rq = NULL;
    nsn->session_clone = PR_FALSE;
    nsn->received = 0;
    nsn->transmitted = 0;

    INTsession_fill_ssl(ns);
    return ns;
}

NSAPI_PUBLIC void INTsession_empty_ssl(Session *sn)
{
    pblock_removekey(pb_key_cipher, sn->client);
    pblock_removekey(pb_key_keysize, sn->client);
    pblock_removekey(pb_key_secret_keysize, sn->client);
    pblock_removekey(pb_key_issuer_dn, sn->client);
    pblock_removekey(pb_key_user_dn, sn->client);
    pblock_removekey(pb_key_ssl_id, sn->client);
}

NSAPI_PUBLIC void INTsession_fill_ssl(Session *sn)
{
    PRInt32 secon = -1;
    PRInt32 keySize, secretKeySize;
    char *cipher;
    char *issuer_dn;
    char *user_dn;
    char *idstr;
    SECItem *iditem;

    // we'll call SSL_SecurityStatus both when we know that SSL is on
    // or when we don't know anything.
    // either way, we can do this only when we have a descriptor.
    // if we don't have one, we're in a VSInit.
    if (sn->ssl && sn->csd_open) {
        if (!SSL_SecurityStatus(sn->csd, &secon, &cipher, &keySize,
                                &secretKeySize, &issuer_dn, &user_dn)) {
            if(secon > 0) {
                sn->ssl = 1;

                int cipher_len = cipher ? strlen(cipher) : 0;
                int issuer_dn_len = issuer_dn ? strlen(issuer_dn) : 0;
                int user_dn_len = user_dn ? strlen(user_dn) : 0;
                pblock_kvinsert(pb_key_cipher, cipher, cipher_len, sn->client);
                pblock_kninsert(pb_key_keysize, keySize, sn->client);
                pblock_kninsert(pb_key_secret_keysize, secretKeySize, sn->client);
                pblock_kvinsert(pb_key_issuer_dn, issuer_dn, issuer_dn_len, sn->client);
                pblock_kvinsert(pb_key_user_dn, user_dn, user_dn_len, sn->client);

                iditem = SSL_GetSessionID(sn->csd);
                if (iditem) {
                    /* Convert to base64 ASCII encoding */
                    idstr = BTOA_DataToAscii(iditem->data, iditem->len);
                    if (idstr) {
                        /* Add encoding to client pblock */
                        pblock_kvinsert(pb_key_ssl_id, idstr, strlen(idstr), sn->client);
                    }

                    /* Free the encoding buffer (pblock_nvinsert dups it) */
                    SECITEM_FreeItem(iditem, PR_TRUE);
                    PR_Free(idstr);
                }
            }
            if (cipher) PORT_Free (cipher);
            if (issuer_dn) PORT_Free (issuer_dn);
            if (user_dn) PORT_Free (user_dn);
        }
    }
}

NSAPI_PUBLIC Session *session_create(SYS_NETFD csd, struct sockaddr_in *sac) 
{
    Session *sn = session_alloc(csd, sac);
    if (sn)
        session_fill(sn);
    return sn;
}


/* ---------------------------- session_clone ----------------------------- */

NSAPI_PUBLIC Session *session_clone(Session *orig_sn, Request *child_rq)
{
    NSAPISession *orig_nsn = (NSAPISession *) orig_sn;

    NSAPIRequest *child_nrq = (NSAPIRequest *) child_rq;
    PR_ASSERT(INTERNAL_REQUEST(child_rq));
    PR_ASSERT(child_rq->orig_rq && child_rq->orig_rq != child_rq);

    NSAPISession *nsn = (NSAPISession *) pool_malloc(orig_sn->pool, sizeof(NSAPISession));
    if (!nsn)
        return NULL;

    // Initialize the session clone except for the filter stack
    nsn->sn.client = pblock_dup(orig_sn->client);
    nsn->sn.csd = NULL;
    nsn->sn.inbuf = NULL;
    nsn->sn.csd_open = 1;
    nsn->sn.iaddr = orig_sn->iaddr;
    nsn->sn.pool = orig_sn->pool;
    session_fill_cla(orig_sn->pool, &nsn->sn);
    nsn->sn.next = NULL;
    nsn->sn.fill = 1;
    nsn->sn.local_addr = orig_sn->local_addr;
    nsn->sn.subject = NULL;
    nsn->sn.ssl = 0;
    nsn->sn.clientauth = 0;
    nsn->sn.pr_client_addr = orig_sn->pr_client_addr;
    nsn->sn.pr_local_addr = orig_sn->pr_local_addr;
    nsn->thread_data = orig_nsn->thread_data;
    nsn->httpfilter = NULL;
    nsn->input_done = PR_FALSE;
    nsn->input_os_pos = 0;
    nsn->exec_rq = NULL;
    nsn->filter_rq = NULL;
    nsn->session_clone = PR_TRUE;
    nsn->received = 0;
    nsn->transmitted = 0;

    // Indicate that the child request has its own session and filter stack
    PR_ASSERT(!child_nrq->session_clone);
    child_nrq->session_clone = PR_TRUE;

    // Give the session a new filter stack
    nsn->sn.csd = filter_create_stack(&nsn->sn);
    nsn->sn.inbuf = netbuf_open(nsn->sn.csd, NET_BUFFERSIZE);

    return &nsn->sn;
}


/* ----------------------------- session_free ----------------------------- */

NSAPI_PUBLIC void INTsession_cleanup(Session *sn)
{
    ClAuth_t *cla = (ClAuth_t *) sn->clauth;

    if (cla) {
        if (cla->cla_cert != 0)
            CERT_DestroyCertificate(cla->cla_cert);
    }

    sn->client = NULL;
    sn->subject = NULL;
    sn->clauth = NULL;
}

NSAPI_PUBLIC void session_free(Session *sn)
{
    NSAPISession *nsn = (NSAPISession *) sn;

    // If this session was cloned,
    // Remove any filters that were installed during this session
    if (nsn->session_clone && sn->csd && sn->csd_open)
        filter_finish_response(sn);

    INTsession_cleanup(sn);
    if (sn->inbuf)
      netbuf_close(sn->inbuf);

    if (nsn->session_clone) {
        if (sn->csd && sn->csd_open)
            PR_Close(sn->csd);
        sn->csd = NULL;
        sn->csd_open = 0;

        pool_free(sn->pool, sn);
    } else {
        systhread_setdata(getThreadMallocKey(), NULL);
        pool_destroy(sn->pool);

        PERM_FREE(sn);
    }
}

/* ----------------------------- session_dns ------------------------------ */


#include "net.h"


NSAPI_PUBLIC char *session_dns_lookup(Session *s, int verify)
{
    pb_param *dns = pblock_findkey(pb_key_dns, s->client);
    char *hn;

    if(!dns) {
        char *ip = pblock_findkeyval(pb_key_ip, s->client);
        if(!ip || !(hn = net_ip2host(ip, verify))) {
            pblock_kvinsert(pb_key_dns, "-none", 5, s->client);
            return NULL;
        }
        dns = pblock_kvinsert(pb_key_dns, hn, strlen(hn), s->client);
    }
    else if(!strcmp(dns->value, "-none"))
        return NULL;
    ((ClAuth_t *) s->clauth)->cla_dns = dns->value;
    return dns->value;
}


/* --------------------------- find_thread_data --------------------------- */

static inline SessionThreadData *find_thread_data(Session *sn)
{
    NSAPISession *nsn = (NSAPISession *)sn;

    if (nsn && nsn->thread_data)
        return nsn->thread_data;

    HttpRequest *hrq = HttpRequest::CurrentRequest();
    if (hrq) {
        DaemonSession &dsn = hrq->GetDaemonSession();
        if (nsn)
            nsn->thread_data = &dsn.thread_data;
        return &dsn.thread_data;
    }

    // We're being called from a non-DaemonSession thread
    void *data = PR_GetThreadPrivate(_session_thread_key);
    if (!data) {
        data = PERM_CALLOC(sizeof(SessionThreadData));
        PR_SetThreadPrivate(_session_thread_key, data);
    }

    return (SessionThreadData *)data;
}


/* ------------------------- session_thread_init -------------------------- */

static PRStatus session_thread_init(void)
{
    _session_thread_lock = PR_NewLock();
    PR_NewThreadPrivateIndex(&_session_thread_key, NULL);
    return PR_SUCCESS;
}


/* ---------------------- session_alloc_thread_slot ----------------------- */

NSAPI_PUBLIC int session_alloc_thread_slot(SlotDestructorFuncPtr destructor)
{
    PR_CallOnce(&_session_thread_once, &session_thread_init);

    PR_Lock(_session_thread_lock);

    int slot = _session_thread_slots;
    _session_thread_slots++;

    _session_thread_destructors = (SlotDestructorFuncPtr*)
        PERM_REALLOC(_session_thread_destructors,
                     sizeof(_session_thread_destructors[0]) *
                     _session_thread_slots);
    _session_thread_destructors[slot] = destructor;

    PR_Unlock(_session_thread_lock);

    return slot;
}


/* ------------------------ session_destroy_thread ------------------------ */

NSAPI_PUBLIC void session_destroy_thread(Session *sn)
{
    PR_CallOnce(&_session_thread_once, &session_thread_init);

    PR_Lock(_session_thread_lock);

    SessionThreadData *thread_data = find_thread_data(sn);
    if (thread_data) {
        // Call the destructors for all non-NULL slots
        for (int i = 0; i < thread_data->count && i < _session_thread_slots; i++) {
            if (thread_data->slots[i]) {
                if (_session_thread_destructors[i])
                    (_session_thread_destructors[i])(thread_data->slots[i]);
                thread_data->slots[i] = NULL;
            }
        }
    }

    PR_Unlock(_session_thread_lock);
}


/* ----------------------- session_get_thread_data ------------------------ */

NSAPI_PUBLIC void *INTsession_get_thread_data(Session *sn, int slot)
{
    SessionThreadData *thread_data = find_thread_data(sn);
    if (!thread_data)
        return NULL;

    if (slot >= thread_data->count)
        return NULL;

    return thread_data->slots[slot];
}


/* ----------------------- session_set_thread_data ------------------------ */

NSAPI_PUBLIC void *INTsession_set_thread_data(Session *sn, int slot, void *data)
{
    SessionThreadData *thread_data = find_thread_data(sn);
    if (!thread_data)
        return NULL;

    // Grow the slots[] array if necessary
    if (slot >= thread_data->count) {
        int count = slot + 1;
        void **slots = (void **)PERM_REALLOC(thread_data->slots,
            sizeof(thread_data->slots[0]) * count);
        if (!slots)
            return data;

        for (int i = thread_data->count; i < count; i++) {
            slots[i] = NULL;
        }

        thread_data->slots = slots;
        thread_data->count = count;
    }

    void *old = thread_data->slots[slot];

    thread_data->slots[slot] = data;

    return old;
}
