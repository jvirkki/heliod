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
 * channel.cpp: Outbound connection pooling
 * 
 * Chris Elving
 */

#include <stdlib.h>
#include <limits.h>
#include "netsite.h"
#include "private/pprio.h"
#include "ssl.h"
#include "cert.h"
#include "key.h"
#include "pk11func.h"
#include "base/pblock.h"
#include "base/util.h"
#include "base/systhr.h"
#include "frame/log.h"
#include "frame/conf.h"
#include "frame/http.h"
#include "frame/httpact.h"
#include "NsprWrap/NsprError.h"
#include "time/nstime.h"
#include "libproxy/dbtlibproxy.h"
#include "libproxy/route.h"
#ifdef FEAT_SOCKS
#include "libproxy/sockslayer.h"
#endif
#include "libproxy/channel.h"
#include "libproxy/util.h"


#define TEXT_PLAIN "text/plain"
#define TEXT_PLAIN_LEN (sizeof(TEXT_PLAIN) - 1)

#ifdef FEAT_SOCKS
/* Internal variables used when routing requests via a SOCKS server */
#define MAGNUS_INTERNAL_SOCKS_DEST_HOST "magnus-internal/socks-dest-host"
#define MAGNUS_INTERNAL_SOCKS_DEST_PORT "magnus-internal/socks-dest-port"
#endif


/* Default SOCKS port */
#define DEFAULT_SOCKS_PORT 1080

/*
 * Daemon encapsulates the configuration of a single daemon and tracks the
 * connections to it
 */
struct Daemon {
    struct Daemon *ht_next; // _daemon_ht hash chain
    unsigned hash; // _daemon_ht hash value

    struct Daemon *reap_next; // _daemon_reap_list linked list

    char *host; // host (no trailing :port)
    int port; // port

    PRBool secure;
    char *client_cert_nickname; // NULL and "" are considered equivalent

    CERTCertificate *client_cert;
    SECKEYPrivateKey *client_key;

    PRBool socks; // set to ensure SOCKS channels aren't kept open
    struct {
        PRLock *lock; // lock that protects the idle connection linked list
        struct DaemonChannel *head; // head of idle connection linked list
        struct DaemonChannel *tail; // tail of idle connection linked list
        PRBool reap; // set if idle connections should be destroyed
    } idle;

    PRInt32 num_channels; // number of connections

    PRIntervalTime timestamp; // last time a connection was used

    int inactivity; // larger values indicate less activity

    PRInt32 ref; // number of references (_daemon_ht, Channel, etc.)
};

/*
 * DaemonChannel is the private implementation behind the public Channel *.
 * DaemonChannel extends Channel, adding private members.
 */
struct DaemonChannel : public Channel {
    struct DaemonChannel *next; // Daemon::idle linked list
    struct DaemonChannel *prev; // Daemon::idle linked list
    PRIntervalTime timestamp; // time the connection was added to the idle list
    PRIntervalTime keep_alive_timeout; // max time for connection to sit idle
    Daemon *daemon; // Daemon this connection is to
};

/*
 * Size of the _daemon_ht hash table.  Should probably be prime and greater
 * than the number of CPUs and daemons.
 */
#define DAEMON_HT_SIZE (127)

/*
 * The _daemon_ht hash table tracks the daemons the proxy is aware of (i.e.
 * all the daemons the proxy has been communicating with recently)
 */
static struct {
    PRLock *lock;
    Daemon *head;
} _daemon_ht[DAEMON_HT_SIZE];

/*
 * Number of daemons in memory (may be more than the number in the _daemon_ht
 * hash table)
 */
static PRInt32 _num_daemons;

/*
 * Linked list of daemons sorted in order of increasing popularity
 */
static struct {
    PRLock *lock; // lock that protects the reap list
    Daemon *head; // head of the reap list
    int num; // number of daemons in the reap list
} _daemon_reap_list;

/*
 * Number of idle connections
 */
static PRInt32 _num_idle_channels;

/*
 * Maximum number of idle connections
 */
static int _max_idle_channels;

/*
 * Maximum number of connections for a single daemon
 */
static int _max_channels_per_daemon;

/*
 * Maximum number of daemons
 */
static int _max_daemons;

/*
 * Default SSL client cert nickname
 */
static char *_default_client_cert_nickname = NULL;

/*
 * Whether we validate SSL server certs by default
 */
static PRBool _default_validate_server_cert = PR_TRUE;

/*
 * Slot for a per-Request SSL client cert nickname char *
 */
static int _client_cert_nickname_request_slot;

/*
 * Slot for a per-Request SSL server cert validation PRBool
 */
static int _validate_server_cert_request_slot;

/*
 * Address from magnus.conf.  If there was no Address directive in magnus.conf,
 * _local_addr.raw.family = 0.
 */
static PRNetAddr _local_addr;

/*
 * Always PR_TRUE
 */
static const PRBool _true = PR_TRUE;

/*
 * Always PR_FALSE
 */
static const PRBool _false = PR_FALSE;

/*
 * _init_status is PR_SUCCESS if channel_init() completed successfully
 */
static PRStatus _init_status = PR_FAILURE;

PR_BEGIN_EXTERN_C
static void daemon_cleanup_callback(void *context);
PR_END_EXTERN_C

static inline void free_idle_channels(Daemon *daemon);


/* ------------- client_cert_nickname_request_slot_destructor ------------- */

PR_BEGIN_EXTERN_C
static void client_cert_nickname_request_slot_destructor(void *data)
{
    FREE(data);
}
PR_END_EXTERN_C


/* ----------------------- get_client_cert_nickname ----------------------- */

static inline const char * get_client_cert_nickname(Request *rq)
{
    const char *nickname = NULL;

    if (rq)
        nickname = (char *)
            request_get_data(rq, _client_cert_nickname_request_slot);
    if (!nickname)
        nickname = _default_client_cert_nickname;

    return nickname;
}


/* ----------------------- get_validate_server_cert ----------------------- */

static inline PRBool get_validate_server_cert(Request *rq)
{
    PRBool validate = _default_validate_server_cert;

    if (rq) {
        const PRBool *p = (const PRBool *)
            request_get_data(rq, _validate_server_cert_request_slot);
        if (p)
            validate = *p;
    }

    return validate;
}


/* ----------------------------- channel_init ----------------------------- */

PRStatus channel_init(void)
{
    PR_ASSERT(_init_status == PR_FAILURE);

    if (_init_status != PR_SUCCESS) {
        _init_status = PR_SUCCESS;

        // Get the magnus.conf Address value
        if (const char *v = conf_getstring("Address", NULL)) {
            PRNetAddr a = _local_addr;
            if (PR_StringToNetAddr(v, &a) == PR_SUCCESS && net_has_ip(&a))
                _local_addr = a;
        }

        // Initialize slots for per-Request configuration overrides
        _validate_server_cert_request_slot = request_alloc_slot(NULL);
        _client_cert_nickname_request_slot = request_alloc_slot(
            client_cert_nickname_request_slot_destructor);

        // Initialize the _daemon_ht hash table
        for (int h = 0; h < DAEMON_HT_SIZE; h++) {
            _daemon_ht[h].lock = PR_NewLock();
            if (!_daemon_ht[h].lock)
                _init_status = PR_FAILURE;
        }

        // Initialize the _daemon_reap_list linked list
        _daemon_reap_list.lock = PR_NewLock();
        if (!_daemon_reap_list.lock)
            _init_status = PR_FAILURE;

        if (_max_idle_channels == 0) {
            _max_idle_channels =
                conf_getboundedinteger("MaxIdleServerConnections",
                                       0, 65536, pool_maxthreads * 2);
        }

        if (_max_channels_per_daemon == 0) {
            _max_channels_per_daemon =
                conf_getboundedinteger("MaxConnectionsPerServer",
                                       1, 65536, pool_maxthreads);
        }

        if (_max_daemons == 0) {
            _max_daemons = conf_getboundedinteger("MaxServers",
                                                  1, 65536, pool_maxthreads);
        }

        // Register our once-a-second cleanup callback function
        if (ft_register_cb(daemon_cleanup_callback, NULL) == -1)
            _init_status = PR_FAILURE;
    }

    return _init_status;
}


/* ------------------------- channel_set_max_idle ------------------------- */

void channel_set_max_idle(int n)
{
    _max_idle_channels = n;
}


/* ---------------------- channel_set_max_per_server ---------------------- */

void channel_set_max_per_server(int n)
{
    _max_channels_per_daemon = n;
}


/* ----------------------- channel_set_max_servers ------------------------ */

void channel_set_max_servers(int n)
{
    _max_daemons = n;
}


/* ----------------------------- hash_daemon ------------------------------ */

static inline unsigned hash_daemon(const char *host,
                                   int port,
                                   PRBool secure,
                                   const char *client_cert_nickname)
{
    unsigned hash = 0;

    while (*host) {
        hash = (hash << 5) ^ hash ^ ((unsigned char) *host);
        host++;
    }

    hash += port;
    hash += secure;
    hash += (client_cert_nickname && *client_cert_nickname);

    return hash;
}


/* ----------------------------- match_daemon ----------------------------- */

static inline PRBool match_daemon(const char *host,
                                  int port,
                                  PRBool secure,
                                  const char *client_cert_nickname,
                                  Daemon *daemon)
{
    if (strcmp(host, daemon->host))
        return PR_FALSE;

    if (port != daemon->port)
        return PR_FALSE;

    if (secure != daemon->secure)
        return PR_FALSE;

    // N.B. client_cert_nickname == NULL and *client_cert_nickname == '\0' are
    // considered equivalent
    const char *nickname1 = client_cert_nickname;
    if (!nickname1)
        nickname1 = "";
    const char *nickname2 = daemon->client_cert_nickname;
    if (!nickname2)
        nickname2 = "";
    if (strcmp(nickname1, nickname2))
        return PR_FALSE;

    return PR_TRUE;
}


/* ----------------------------- free_daemon ------------------------------ */

static void free_daemon(Daemon *daemon)
{
    log_error(LOG_VERBOSE, NULL, NULL, NULL,
              "no longer managing connections to server %s:%d",
              daemon->host, daemon->port);

    PR_ASSERT(daemon->idle.head == NULL);
    PR_ASSERT(daemon->idle.tail == NULL);
    PR_ASSERT(daemon->num_channels == 0);
    PR_ASSERT(daemon->ref == 0);

    PERM_FREE(daemon->host);
    PERM_FREE(daemon->client_cert_nickname);

    if (daemon->client_cert)
        CERT_DestroyCertificate(daemon->client_cert);
    if (daemon->client_key)
        SECKEY_DestroyPrivateKey(daemon->client_key);

    if (daemon->idle.lock)
        PR_DestroyLock(daemon->idle.lock);

    PERM_FREE(daemon);

    PR_AtomicDecrement(&_num_daemons);
    PR_ASSERT(_num_daemons >= 0);
}


/* ----------------------------- unref_daemon ----------------------------- */

static inline PRBool unref_daemon(Daemon *daemon)
{
    PR_ASSERT(daemon->ref > 0);
    if (PR_AtomicDecrement(&daemon->ref) == 0) {
        free_daemon(daemon);
        return PR_TRUE;
    }
    return PR_FALSE;
}


/* ---------------------------- remove_daemon ----------------------------- */

static void remove_daemon(Daemon *daemon)
{
    // The caller should have a reference
    PR_ASSERT(daemon->ref > 0);

    int h = daemon->hash % DAEMON_HT_SIZE;

    // Look for the specified daemon in the _daemon_ht hash table
    PR_Lock(_daemon_ht[h].lock);
    Daemon **pdaemon = &_daemon_ht[h].head;
    while (*pdaemon) {
        if (*pdaemon == daemon) {
            // Both the caller and _daemon_ht should have a reference
             PR_ASSERT(daemon->ref > 1);

            // Remove the daemon from the _daemon_ht hash table
            (*pdaemon) = daemon->ht_next;
            daemon->ht_next = NULL;

            // Remove _daemon_ht's daemon reference
            unref_daemon(daemon);

            break;
        }
        pdaemon = &(*pdaemon)->ht_next;
    }
    PR_Unlock(_daemon_ht[h].lock);
}


/* ----------------------------- reap_daemon ------------------------------ */

static PRBool reap_daemon(void)
{
    /*
     * Note that we need to ensure that at least one daemon is actually freed
     * (not just unreferenced)
     */

    // First, check the _daemon_reap_list linked list
    for (;;) {
        // Remove a daemon from _daemon_reap_list
        Daemon *daemon = NULL;
        PR_Lock(_daemon_reap_list.lock);
        daemon = _daemon_reap_list.head;
        if (daemon) {
            _daemon_reap_list.head = daemon->reap_next;
            _daemon_reap_list.num--;
        }
        PR_Unlock(_daemon_reap_list.lock);

        if (!daemon)
            break;

        // Remove the daemon from the _daemon_ht hash table
        remove_daemon(daemon);

        // Tell channel_release() not to add any more idle channels to the idle
        // list
        PR_Lock(daemon->idle.lock);
        daemon->idle.reap = PR_TRUE;
        PR_Unlock(daemon->idle.lock);

        // Close all the daemon's idle channels
        free_idle_channels(daemon);

        // Remove _daemon_reap_list's daemon reference.  If that was the last
        // reference (i.e. the daemon was freed), we're done.
        if (unref_daemon(daemon))
            return PR_TRUE;
    }

    // Next, check _daemon_ht, starting at a random hash index
    int offset = PR_IntervalNow() % DAEMON_HT_SIZE;
    for (int i = 0; i < DAEMON_HT_SIZE; i++) {
        int h = offset + i;
        if (h >= DAEMON_HT_SIZE)
            h -= DAEMON_HT_SIZE;

        for (;;) {
            // Remove a daemon at this hash index
            Daemon *daemon;
            PR_Lock(_daemon_ht[h].lock);
            daemon = _daemon_ht[h].head;
            if (daemon)
                _daemon_ht[h].head = daemon->ht_next;
            PR_Unlock(_daemon_ht[h].lock);

            if (!daemon)
                break;

            daemon->ht_next = NULL;

            // Tell channel_release() not to add any more idle channels to the
            // idle list
            PR_Lock(daemon->idle.lock);
            daemon->idle.reap = PR_TRUE;
            PR_Unlock(daemon->idle.lock);

            // Close all the daemon's channels
            free_idle_channels(daemon);

            // Remove _daemon_ht's daemon reference.  If that was the last
            // reference (i.e. the daemon was freed), we're done.
            if (unref_daemon(daemon))
                return PR_TRUE;
        }
    }

    // Since we failed to remove a daemon, that must mean there are more worker
    // threads than allowed daemons, i.e. RqThrottle > _max_daemons
    PR_ASSERT(pool_maxthreads > _max_daemons);

    return PR_FALSE;
}


/* ----------------------- get_client_cert_and_key ------------------------ */

static PRStatus get_client_cert_and_key(const char *nickname,
                                        CERTCertificate **pcert,
                                        SECKEYPrivateKey **pkey)
{
    CERTCertificate *cert = NULL;
    SECKEYPrivateKey *key = NULL;

    char nickname_copy[256];
    util_strlcpy(nickname_copy, nickname, sizeof(nickname_copy));

    cert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), nickname_copy);
    if (!cert) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR,
                             XP_GetAdminStr(DBT_unknown_cert_X),
                             nickname_copy);
        goto get_client_cert_and_key_error;
    }

    if (CERT_VerifyCertNow(CERT_GetDefaultCertDB(),
                           cert,
                           PR_FALSE,
                           certUsageSSLClient,
                           NULL) != SECSuccess)
    {
        ereport(LOG_WARN,
                XP_GetAdminStr(DBT_cert_X_not_valid_client_cert_because_Y),
                nickname_copy,
                system_errmsg());
    }

    key = PK11_FindKeyByAnyCert(cert, NULL);
    if (!key) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR,
                             XP_GetAdminStr(DBT_no_key_for_X_because_Y),
                             nickname_copy,
                             system_errmsg());
        goto get_client_cert_and_key_error;
    }

    *pcert = cert;
    *pkey = key;

    return PR_SUCCESS;

get_client_cert_and_key_error:
    if (cert)
        CERT_DestroyCertificate(cert);
    if (key)
        SECKEY_DestroyPrivateKey(key);

    return PR_FAILURE;
}


/* ------------------------------ get_daemon ------------------------------ */

static Daemon * get_daemon(Session *sn,
                           Request *rq,
                           const char *host,
                           int port,
                           PRBool secure)
{
    PR_ASSERT(_init_status == PR_SUCCESS);

    // Get the client certificate nickname
    const char *client_cert_nickname = NULL;
    if (secure)
        client_cert_nickname = get_client_cert_nickname(rq);

    // Compute the hash value for a daemon with the requested parameters
    unsigned hash = hash_daemon(host, port, secure, client_cert_nickname);
    int h = hash % DAEMON_HT_SIZE;

    // Check the _daemon_ht hash table for an existing entry for this daemon
    PR_Lock(_daemon_ht[h].lock);
    Daemon *daemon = _daemon_ht[h].head;
    while (daemon) {
        if (daemon->hash == hash) {
            if (match_daemon(host,
                             port,
                             secure,
                             client_cert_nickname,
                             daemon))
            {
                // Give caller a reference to this daemon
                PR_AtomicIncrement(&daemon->ref);
                PR_Unlock(_daemon_ht[h].lock);
                return daemon;
            }
        }
        daemon = daemon->ht_next;
    }
    PR_Unlock(_daemon_ht[h].lock);

    // Make sure we never have more than _max_daemons daemons
    if (PR_AtomicIncrement(&_num_daemons) > _max_daemons) {
        if (!reap_daemon()) {
            // There must be more worker threads than allowed daemons, i.e.
            // RqThrottle > _max_daemons
            PR_ASSERT(pool_maxthreads > _max_daemons);
            PR_AtomicDecrement(&_num_daemons);
            NsprError::setErrorf(PR_NETWORK_UNREACHABLE_ERROR,
                                 XP_GetAdminStr(DBT_reached_max_X_servers),
                                 _max_daemons);
            return NULL;
        }
    }

    // Instantiate a new daemon
    daemon = (Daemon *)PERM_MALLOC(sizeof(Daemon));
    if (!daemon) {
        PR_AtomicDecrement(&_num_daemons);
        return NULL;
    }

    PRStatus rv = PR_SUCCESS;

    // Initialize the daemon_t's members
    daemon->ht_next = NULL;
    daemon->hash = hash;
    daemon->reap_next = NULL;
    daemon->host = PERM_STRDUP(host);
    daemon->port = port;
    daemon->secure = secure;
    if (client_cert_nickname) {
        daemon->client_cert_nickname = PERM_STRDUP(client_cert_nickname);
    } else {
        daemon->client_cert_nickname = NULL;
    }
    daemon->socks = PR_FALSE;
    daemon->client_cert = NULL;
    daemon->client_key = NULL;
    daemon->idle.lock = PR_NewLock();
    daemon->idle.head = NULL;
    daemon->idle.tail = NULL;
    daemon->idle.reap = PR_FALSE;
    daemon->num_channels = 0;
    daemon->timestamp = ft_timeIntervalNow();
    daemon->inactivity = 0;
    daemon->ref = 1; // our reference (will be passed to caller)

    // Lookup client certificate and key
    if (daemon->client_cert_nickname && *daemon->client_cert_nickname) {
        if (get_client_cert_and_key(daemon->client_cert_nickname,
                                    &daemon->client_cert,
                                    &daemon->client_key) != PR_SUCCESS)
        {
            rv = PR_FAILURE;
        }
    }

    if (!daemon->host)
        rv = PR_FAILURE;
    if (client_cert_nickname && !daemon->client_cert_nickname)
        rv = PR_FAILURE;
    if (!daemon->idle.lock)
        rv = PR_FAILURE;

    // Did everything initialize ok?
    if (rv == PR_SUCCESS) {
        // Add the daemon to the _daemon_ht hash table
        // XXX It's possible for multiple equivalent daemons to end up in
        // _daemon_ht, particularly on SMP systems
        daemon->ref++; // _daemon_ht's reference
        PR_Lock(_daemon_ht[h].lock);
        daemon->ht_next = _daemon_ht[h].head;
        _daemon_ht[h].head = daemon;
        PR_Unlock(_daemon_ht[h].lock);

        log_error(LOG_VERBOSE, NULL, sn, rq,
                  "now managing connections to server %s:%d",
                  daemon->host, daemon->port);
    } else {
        // Initialization error, throw this daemon away
        unref_daemon(daemon);
        daemon = NULL;
    }

    return daemon;
}


/* ------------------------- add_channel_to_head -------------------------- */

static inline void add_channel_to_head(Daemon *daemon, DaemonChannel *channel)
{
    PR_ASSERT(!daemon->idle.reap);
    channel->next = daemon->idle.head;
    daemon->idle.head = channel;
    if (channel->next) {
        channel->next->prev = channel;
    } else {
        daemon->idle.tail = channel;
    }
}


/* ----------------------- remove_channel_from_head ----------------------- */

static inline DaemonChannel * remove_channel_from_head(Daemon *daemon)
{
    DaemonChannel *channel = daemon->idle.head;
    if (channel) {
        daemon->idle.head = channel->next;
        if (channel->next) {
            channel->next->prev = NULL;
        } else {
            daemon->idle.tail = NULL;
        }
    }
    return channel;
}


/* ----------------------- remove_channel_from_tail ----------------------- */

static inline DaemonChannel * remove_channel_from_tail(Daemon *daemon)
{
    DaemonChannel *channel = daemon->idle.tail;
    if (channel) {
        if (channel->prev) {
            channel->prev->next = NULL;
        } else {
            daemon->idle.head = NULL;
        }
        daemon->idle.tail = channel->prev;
    }
    return channel;
}


/* -------------------------------- stale --------------------------------- */

static inline PRBool stale(PRIntervalTime now, DaemonChannel *channel)
{
    if (channel && channel->keep_alive_timeout != PR_INTERVAL_NO_TIMEOUT) {
        PRIntervalTime elapsed = now - channel->timestamp;
        if (elapsed >= channel->keep_alive_timeout)
            return PR_TRUE;
    }
    return PR_FALSE;
}


/* ------------------------- free_active_channel -------------------------- */

static inline void free_active_channel(DaemonChannel *channel)
{
    PR_ASSERT(channel->daemon->idle.head != channel);
    PR_ASSERT(channel->daemon->idle.tail != channel);

    PR_AtomicDecrement(&channel->daemon->num_channels);
    unref_daemon(channel->daemon);

    PR_Close(channel->fd);
    PERM_FREE(channel);
}


/* -------------------------- free_idle_channel --------------------------- */

static inline void free_idle_channel(DaemonChannel *channel)
{
    free_active_channel(channel);
    PR_AtomicDecrement(&_num_idle_channels);
}


/* -------------------------- reap_idle_channel --------------------------- */

static PRBool reap_idle_channel(void)
{
    // Traverse _daemon_ht, starting at a random hash index
    int offset = PR_IntervalNow() % DAEMON_HT_SIZE;
    for (int i = 0; i < DAEMON_HT_SIZE; i++) {
        int h = offset + i;
        if (h >= DAEMON_HT_SIZE)
            h -= DAEMON_HT_SIZE;

        // Check for an idle channel at this hash index
        DaemonChannel *idle = NULL;
        PR_Lock(_daemon_ht[h].lock);
        Daemon *daemon = _daemon_ht[h].head;
        while (daemon) {
            if (daemon->idle.head) {
                PR_Lock(daemon->idle.lock);
                idle = remove_channel_from_tail(daemon);
                PR_Unlock(daemon->idle.lock);
                if (idle)
                    break;
            }
            daemon = daemon->ht_next;
        }
        PR_Unlock(_daemon_ht[h].lock);

        // If we found an idle channel, free it and we're done
        if (idle) {
            free_idle_channel(idle);
            return PR_TRUE;
        }
    }

    // No idle channels to free
    return PR_FALSE;
}


/* --------------------------- channel_release ---------------------------- */

void channel_release(Channel *channel_handle,
                     PRIntervalTime keep_alive_timeout)
{
    DaemonChannel *channel = (DaemonChannel *)channel_handle;

    // If we're not supposed to keep the channel open...
    if (channel->daemon->socks || keep_alive_timeout == PR_INTERVAL_NO_WAIT) {
        // Free this channel
        free_active_channel(channel);
        return;
    }

    // We'll keep this channel around
    channel->timestamp = ft_timeIntervalNow();
    channel->keep_alive_timeout = keep_alive_timeout;

    Daemon *daemon = channel->daemon;

    // If there's room for another idle channel...
    if (PR_AtomicIncrement(&_num_idle_channels) <= _max_idle_channels) {
        // Add the channel to the front of the idle list
        PR_Lock(daemon->idle.lock);
        if (!daemon->idle.reap) {
            add_channel_to_head(daemon, channel);
            channel = NULL;
        }
        PR_Unlock(daemon->idle.lock);

        // Free the channel if we didn't add it to the idle list
        if (channel)
            free_idle_channel(channel);

        return;
    }

    // There are too many idle channels.  Add this channel to the head of the
    // idle list and simultaneously remove an older channel from the tail.
    DaemonChannel *idle;
    PR_Lock(daemon->idle.lock);
    if (daemon->idle.reap) {
        idle = channel;
    } else {
        idle = remove_channel_from_tail(daemon);
        add_channel_to_head(daemon, channel);
    }
    PR_Unlock(daemon->idle.lock);

    // N.B. we were borrowing channel_handle's daemon reference, which may
    // no longer valid
    channel = NULL;
    daemon = NULL;

    // If we've got a channel to free, free it and we're done
    if (idle) {
        free_idle_channel(idle);
        return;
    }

    // Try to free some arbitrary daemon's channel to make room for the channel
    // we just added to the idle list (in the worst case, we may end up freeing
    // the channel we just added)
    reap_idle_channel();
}


/* -------------------------- free_idle_channels -------------------------- */

static inline void free_idle_channels(Daemon *daemon)
{
    // Remove all idle channels from the idle channel list
    DaemonChannel *idle;
    PR_Lock(daemon->idle.lock);
    idle = daemon->idle.head;
    daemon->idle.head = NULL;
    daemon->idle.tail = NULL;
    PR_Unlock(daemon->idle.lock);

    // Discard the idle channels
    while (idle) {
        DaemonChannel *next = idle->next;
        free_idle_channel(idle);
        idle = next;
    }
}


/* ---------------------------- channel_purge ----------------------------- */

void channel_purge(Channel *channel_handle)
{
    DaemonChannel *channel = (DaemonChannel *)channel_handle;
    Daemon *daemon = channel->daemon;

    // Get rid of all the idle channels
    free_idle_channels(daemon);

    // Get rid of the caller's channel
    free_active_channel(channel);

    // N.B. we were borrowing channel_handle's daemon reference, which is no
    // longer valid
    daemon = NULL;

    // XXX When we're called because a Service SAF detected a hung server,
    // should we also PR_Shutdown() active connections?  Perhaps only those
    // active connections that were acquired with reuse_persistent?
}


/* ---------------- GetClientAuthData_client_cert_nickname ---------------- */

PR_BEGIN_EXTERN_C
static SECStatus GetClientAuthData_client_cert_nickname(
    void *arg,
    PRFileDesc *fd,
    CERTDistNames *caNames,
    CERTCertificate **pRetCert,
    SECKEYPrivateKey **pRetKey)
{
    const Daemon *daemon = (const Daemon *)arg;
    *pRetCert = CERT_DupCertificate(daemon->client_cert);
    *pRetKey = SECKEY_CopyPrivateKey(daemon->client_key);
    return SECSuccess;
}
PR_END_EXTERN_C


/* ------------------ AuthCertificateHook_always_success ------------------ */

PR_BEGIN_EXTERN_C
static SECStatus AuthCertificateHook_always_success(void *arg,
                                                    PRFileDesc *fd,
                                                    PRBool checksig,
                                                    PRBool isServer)
{
    return SECSuccess;
}
PR_END_EXTERN_C


/* ------------------------------ enable_ssl ------------------------------ */

static PRStatus enable_ssl(PRFileDesc *fd,
                           Daemon *daemon,
                           PRBool validate_server_cert)
{
    PR_ASSERT(daemon->secure);

    PRFileDesc *sslfd = SSL_ImportFD(NULL, fd);
    if (!sslfd)
        return PR_FAILURE;

    PR_ASSERT(sslfd == fd);

    if (SSL_OptionSet(fd, SSL_SECURITY, PR_TRUE) < 0)
        return PR_FAILURE;

    SSL_OptionSet(fd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
    SSL_OptionSet(fd, SSL_ENABLE_SSL2, PR_TRUE);
    SSL_OptionSet(fd, SSL_ENABLE_SSL3, PR_TRUE);
    SSL_OptionSet(fd, SSL_ENABLE_TLS, PR_TRUE);

    if (daemon->client_cert && daemon->client_key) {
        // We should present a specific client cert
        SSL_GetClientAuthDataHook(fd,
                                  GetClientAuthData_client_cert_nickname,
                                  (void *)daemon);
    } else {
        // Let NSS choose the client cert to present
        SSL_GetClientAuthDataHook(fd, NSS_GetClientAuthData, NULL);
    }

    if (validate_server_cert) {
        // Let NSS validate the server cert
        PR_ASSERT(!strchr(daemon->host, ':'));
        SSL_SetURL(fd, daemon->host);
    } else {
        // We'll trust any server cert
        SSL_AuthCertificateHook(fd,
                                AuthCertificateHook_always_success,
                                NULL);
    }

    return PR_SUCCESS;
}


/* -------------------------- connect_to_daemon --------------------------- */

static Channel * connect_to_daemon(Session *sn,
                                   Request *rq,
                                   Daemon *daemon,
                                   PRIntervalTime timeout)
{
    /*
     * N.B. we assume ownership of the caller's daemon reference, passing it
     * on to the channel or releasing it as appropriate
     */

    PRFileDesc *fd = NULL;
    PRHostEnt *he;
    int hei;
    char ipbuf[NET_ADDR_STRING_SIZE];
    const char *ip = NULL;
    DaemonChannel *channel;
    PRIntervalTime epoch;
    PRBool log_verbose;
    int retry_ms;
    int retry;
#ifdef FEAT_SOCKS
    PRNetAddr socks_dest_addr;
#endif
    int res;


    epoch = ft_timeIntervalNow();

    log_verbose = ereport_can_log(LOG_VERBOSE);

    retry_ms = 200; // XXX

    // Process Connect directives
    if (rq) {
        PR_SetError(0, 0);
        int res = servact_connect(daemon->host, daemon->port, sn, rq);
        switch (res) {
        case REQ_NOACTION:
            // No Connect directives
            break;

        case REQ_ABORTED:
        case REQ_EXIT:
        case REQ_RESTART:
            // Error running Connect directives
            NsprError::setErrorf(PR_INVALID_STATE_ERROR,
                                 XP_GetAdminStr(DBT_error_running_connect));
            goto connect_to_daemon_error;

        default:
            // Connect directive returned a connected socket
            fd = PR_ImportTCPSocket(res);
            if (fd == NULL) {
#ifdef XP_WIN32
                CloseHandle((HANDLE)res);
#else
                close(res);
#endif
                goto connect_to_daemon_error;
            }

            // Return the Channel * for this connected socket
            goto connect_to_daemon_success;
        }
    }

#ifdef FEAT_SOCKS
    // If the request is routed via a SOCKS server, then determine the actual
    // origin/destination server's network address
    if (daemon->socks) {
        const char *socks_dest_port = pblock_findval(MAGNUS_INTERNAL_SOCKS_DEST_PORT, rq->vars);
        if (!socks_dest_port)
            goto connect_to_daemon_error;
        int socks_port = atoi(socks_dest_port);

        const char *socks_dest_host = pblock_findval(MAGNUS_INTERNAL_SOCKS_DEST_HOST, rq->vars);
        if (!socks_dest_host)
            goto connect_to_daemon_error;

        PRHostEnt *sockshe = servact_gethostbyname(socks_dest_host, sn, rq);
        if (!sockshe)
            goto connect_to_daemon_error;

        int sockshei = PR_EnumerateHostEnt(0, sockshe, socks_port, &socks_dest_addr);
        if (sockshei < 1)
            goto connect_to_daemon_error;
    }
#endif

    pblock_nninsert("dns-start", (int)PR_IntervalToMilliseconds(PR_IntervalNow()), rq->vars);

    // Process DNS directives
    he = servact_gethostbyname(daemon->host, sn, rq);

    pblock_nninsert("dns-end", (int)PR_IntervalToMilliseconds(PR_IntervalNow()), rq->vars);

    if (!he)
        goto connect_to_daemon_error;

    // Connect, trying multiple times if necessary
    for (retry = 0; retry < 3 /* XXX */; retry++) {
        // Throttle back connect() attempts
        if (retry) {
            systhread_sleep(retry_ms);
            retry_ms *= 2;
        }

        // Connect, trying up to 3 A records if necessary
        hei = 0;
        pblock_nninsert("conn-start", (int)PR_IntervalToMilliseconds(PR_IntervalNow()), rq->vars);
        for (int record = 0; record < 3 /* XXX */; record++) {
            PRNetAddr addr;

            hei = PR_EnumerateHostEnt(hei, he, daemon->port, &addr);
            if (hei == 0)
                break; // no more A records
            if (hei < 1)
                goto connect_to_daemon_error;

            if (log_verbose) {
                if (net_addr_to_string(&addr, ipbuf, sizeof(ipbuf)) < 0) {
                    ip = daemon->host;
                } else {
                    ip = ipbuf;
                }

                log_error(LOG_VERBOSE, NULL, sn, rq,
                          "attempting to connect to %s:%d",
                          ip, daemon->port);
            }

            // Make sure we're not trying to talk to ourselves.  (Note that we
            // don't enumerate all the interfaces on the host, so an admin with
            // more time than sense can still create forwarding loops.  Don't
            // do that.)
            if (sn) {
                if (sn->pr_local_addr &&
                    !net_addr_cmp(&addr, sn->pr_local_addr))
                {
                    NsprError::setError(PR_BAD_ADDRESS_ERROR,
                                        XP_GetAdminStr(DBT_src_dst_match));
                    goto connect_to_daemon_error;
                }

                // XXX should we do more?
            }

            // XXX IPv6
            fd = PR_NewTCPSocket();
            if (!fd)
                goto connect_to_daemon_error;

            // Disable Nagle algorithm
            PRSocketOptionData opt;
            opt.option = PR_SockOpt_NoDelay;
            opt.value.no_delay = PR_TRUE;
            PR_SetSocketOption(fd, &opt);

            // Bind to the IP specified by the magnus.conf Address directive
            if (_local_addr.raw.family != 0)
                PR_Bind(fd, &_local_addr);

            // Enable SSL
            if (daemon->secure) {
                if (enable_ssl(fd, daemon, get_validate_server_cert(rq)) != PR_SUCCESS)
                    goto connect_to_daemon_error;
            }

#ifdef FEAT_SOCKS
            // Insert the layer that sets up the SOCKS tunnel to the real server
            if (daemon->socks) {
                sockslayer_insert(fd, socks_dest_addr.inet.ip, socks_dest_addr.inet.port);
            }
#endif

            // Attempt to connect
            
            res = PR_Connect(fd, &addr, timeout);

            if (res == PR_SUCCESS) {
                pblock_nninsert("conn-end", (int)PR_IntervalToMilliseconds(PR_IntervalNow()),
                                rq->vars);
                if (log_verbose) {
                    log_error(LOG_VERBOSE, NULL, sn, rq,
                              "connected to %s:%d",
                              ip, daemon->port);
                }
#ifdef FEAT_SOCKS
                // Remove any SOCKS layer that was inserted as its job is done
                // during PR_Connect
                if (daemon->socks) {
                    sockslayer_remove(fd);
                }
#endif
                goto connect_to_daemon_success;
            }

            if (log_verbose) {
                log_error(LOG_VERBOSE, NULL, sn, rq,
                          "error connecting to %s:%d (%s)",
                          ip, daemon->port, system_errmsg());
            }

#ifdef FEAT_SOCKS
            // Remove any SOCKS layer that was inserted
            if (daemon->socks) {
                sockslayer_remove(fd);
            }
#endif
            // We can't recycle the socket after PR_Connect() times out
            PR_Close(fd);
            fd = NULL;
        }

        if (timeout != PR_INTERVAL_NO_TIMEOUT) {
            if (PR_GetError() == PR_IO_TIMEOUT_ERROR)
                goto connect_to_daemon_error;
            if ((PRIntervalTime)(ft_timeIntervalNow() - epoch) >= timeout)
                goto connect_to_daemon_error;
        }
    }

connect_to_daemon_error:
    unref_daemon(daemon);
    if (fd)
        PR_Close(fd);

    return NULL; // failure

connect_to_daemon_success:
    // Return the connection to the caller
    channel = (DaemonChannel *)PERM_MALLOC(sizeof(DaemonChannel));
    if (!channel)
        goto connect_to_daemon_error;
    channel->next = NULL;
    channel->prev = NULL;
    channel->fd = fd;
    channel->daemon = daemon; // channel now owns caller's daemon reference

    return channel; // success
}


/* --------------------------- channel_acquire ---------------------------- */

Channel * channel_acquire(Session *sn,
                          Request *rq,
                          const char *host,
                          int port,
                          PRBool secure,
                          PRBool reuse_persistent,
                          PRIntervalTime timeout)
{
    Daemon *daemon = NULL;

#ifndef FEAT_SOCKS
    // Acquire a reference to the daemon
    daemon = get_daemon(sn, rq, host, port, secure);
#else
    // If route via a SOCKS server has been configured then acquire a 
    // channel to the SOCKS server rather than the origin server directly
    char* socks_addr = route_get_socks_addr(sn, rq);
    if (socks_addr) {
        const char *socks_host;
        char *dynamic_socks_host = NULL;
        int socks_port;
        char *port_suffix = util_host_port_suffix(socks_addr);
        if (port_suffix) {
            socks_port = atoi(port_suffix + 1);
            if (socks_port < 1 || socks_port > 65535)
                return NULL;
            int socks_host_len = port_suffix - socks_addr;
            dynamic_socks_host = (char *)pool_malloc(sn->pool,  socks_host_len + 1);
            if (!dynamic_socks_host)
                return NULL;

            memcpy(dynamic_socks_host, socks_addr, socks_host_len);
            dynamic_socks_host[socks_host_len] = '\0';
            socks_host = dynamic_socks_host;
            // Store the real destination server's host and port name in
            // internal variables for use when initializing the SOCKS I/O layer
            pblock_nvinsert(MAGNUS_INTERNAL_SOCKS_DEST_HOST, host, rq->vars);
            char portstr[256];
            PR_snprintf(portstr, sizeof(portstr), "%d", port);
            pblock_nvinsert(MAGNUS_INTERNAL_SOCKS_DEST_PORT, portstr, rq->vars);

        } else {
            socks_port = DEFAULT_SOCKS_PORT;
            socks_host = socks_addr;
        }
        // Acquire a reference to the daemon
        daemon = get_daemon(sn, rq, socks_host, socks_port, secure);
        if (dynamic_socks_host)
            pool_free(sn->pool, dynamic_socks_host);
        reuse_persistent = PR_FALSE;
        // Mark this as a SOCKS daemon so that its connections are closed when
        // channels are released
        if (daemon)
            daemon->socks = PR_TRUE;
    } else {
        // Acquire a reference to the daemon
        daemon = get_daemon(sn, rq, host, port, secure);
    }
#endif
    if (!daemon)
        return NULL;

    PRIntervalTime now = ft_timeIntervalNow();
    daemon->timestamp = now;

    // Try to reuse an existing persistent connection
    if (reuse_persistent && daemon->idle.head) {
        // Remove a channel from the idle list
        DaemonChannel *persistent;
        PR_Lock(daemon->idle.lock);
        persistent = remove_channel_from_head(daemon);
        PR_Unlock(daemon->idle.lock);

        if (persistent) {
            persistent->next = NULL;
            persistent->prev = NULL;

            // Check channel freshness
            if (stale(now, persistent)) {
                // The channel at the head of the idle list is stale.  That
                // means ALL the channels in the idle list are stale.
                free_idle_channels(daemon);
                free_idle_channel(persistent);
            } else {
                // The caller can reuse this previously idle channel
                PR_AtomicDecrement(&_num_idle_channels);

                log_error(LOG_VERBOSE, NULL, sn, rq,
                          "reusing existing persistent connection to %s:%d",
                          daemon->host, daemon->port);

                // Release our daemon reference and return the channel
                unref_daemon(daemon);
                return persistent;
            }
        }
    }

    // Make sure we never have more than _max_channels_per_daemon channels
    if (PR_AtomicIncrement(&daemon->num_channels) > _max_channels_per_daemon) {
        // Remove an old channel from the tail of the idle list
        DaemonChannel *idle;
        PR_Lock(daemon->idle.lock);
        idle = remove_channel_from_tail(daemon);
        PR_Unlock(daemon->idle.lock);

        // If there wasn't an old channel for us to remove...
        if (!idle) {
            // There must be more worker threads than allowed channels, i.e.
            // RqThrottle > _max_channels_per_daemon
            PR_ASSERT(pool_maxthreads > _max_channels_per_daemon);
            PR_AtomicDecrement(&daemon->num_channels);
            NsprError::setErrorf(PR_NETWORK_UNREACHABLE_ERROR,
                                 XP_GetAdminStr(DBT_reached_max_X_conn_to_Y_Z),
                                 _max_channels_per_daemon,
                                 daemon->host, daemon->port);

            // Release our daemon reference and fail the connection attempt
            unref_daemon(daemon);
            return NULL;
        }

        // Free the old channel
        free_idle_channel(idle);
    }

    // Connect to the daemon.  The resulting channel will assume ownership of
    // our daemon reference (or, if an error occurs, connect_to_daemon will
    // release our daemon reference).
    Channel *channel_handle = connect_to_daemon(sn, rq, daemon, timeout);
    if (!channel_handle)
        PR_AtomicDecrement(&daemon->num_channels);

    return channel_handle;
}


/* ----------------------- channel_get_remote_host ------------------------ */

const char * channel_get_remote_host(Channel *channel_handle)
{
    DaemonChannel *channel = (DaemonChannel *)channel_handle;
    return channel->daemon->host;
}


/* ----------------------- channel_get_remote_port ------------------------ */

int channel_get_remote_port(Channel *channel_handle)
{
    DaemonChannel *channel = (DaemonChannel *)channel_handle;
    return channel->daemon->port;
}


/* ---------------------- channel_ssl_client_config ----------------------- */

int channel_ssl_client_config(pblock *pb, Session *sn, Request *rq)
{
    const char *nickname = pblock_findkeyval(pb_key_client_cert_nickname, pb);
    if (nickname) {
        if (rq) {
            // Request time, allow the most-specific object to set a
            // per-Request override
            if (!request_get_data(rq, _client_cert_nickname_request_slot)) {
                request_set_data(rq, _client_cert_nickname_request_slot,
                                 STRDUP(nickname));
            }
        } else {
            // Init time, set the default
            _default_client_cert_nickname = PERM_STRDUP(nickname);
        }
    }

    const char *validate = pblock_findkeyval(pb_key_validate_server_cert, pb);
    if (validate) {
        PRBool b = util_getboolean(validate, PR_TRUE);

        if (rq) {
            // Request time, allow the most-specific object to set a
            // per-Request override
            if (!request_get_data(rq, _validate_server_cert_request_slot)) {
                const PRBool *p = b ? &_true : &_false;
                request_set_data(rq, _validate_server_cert_request_slot,
                                 (void *)p);
            }
        } else {
            // Init time, set the default
            _default_validate_server_cert = b;
        }
    }

    return REQ_NOACTION;
}


/* --------------------- channel_service_channel_dump --------------------- */

int channel_service_channel_dump(pblock *pb, Session *sn, Request *rq)
{
    param_free(pblock_removekey(pb_key_content_type, rq->srvhdrs));
    pblock_kvinsert(pb_key_content_type,
                    TEXT_PLAIN,
                    TEXT_PLAIN_LEN,
                    rq->srvhdrs);

    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    protocol_start_response(sn, rq);

    int num_daemons = 0;
    int num_channels = 0;

    for (int h = 0; h < DAEMON_HT_SIZE; h++) {
        PR_Lock(_daemon_ht[h].lock);
        Daemon *daemon = _daemon_ht[h].head;
        while (daemon) {
            num_daemons++;

            PR_Lock(daemon->idle.lock);
            int num_idle = 0;
            DaemonChannel *idle = daemon->idle.head;
            while (idle) {
                num_idle++;
                idle = idle->next;
            }
            PR_Unlock(daemon->idle.lock);

            int num_active = daemon->num_channels - num_idle;
            if (num_active < 0)
                num_active = 0;

            PR_fprintf(sn->csd,
                       "host = %s, "
                       "port = %d, "
                       "secure = %s, "
                       "client-cert-nickname = %s, "
                       "idle channels = %d, "
                       "active channels = %d\n",
                       daemon->host,
                       daemon->port,
                       daemon->secure ? "true" : "false",
                       daemon->client_cert_nickname,
                       num_idle,
                       num_active);

            num_channels += num_idle;
            num_channels += num_active;

            daemon = daemon->ht_next;
        }
        PR_Unlock(_daemon_ht[h].lock);
    }

    PR_fprintf(sn->csd, "%d daemon(s)\n", num_daemons);
    PR_fprintf(sn->csd, "%d channel(s)\n", num_channels);

    return REQ_PROCEED;
}


/* ---------------------- compare_daemon_inactivity ----------------------- */

PR_BEGIN_EXTERN_C
static int compare_daemon_inactivity(const void *p1, const void *p2)
{
    Daemon *daemon1 = *(Daemon **)p1;
    Daemon *daemon2 = *(Daemon **)p2;

    return daemon1->inactivity - daemon2->inactivity;
}
PR_END_EXTERN_C


/* ----------------------- daemon_cleanup_callback ------------------------ */

PR_BEGIN_EXTERN_C
static void daemon_cleanup_callback(void *context)
{
    int i;

    // Run if:
    // a) 30 seconds have elapsed
    // b) the maximum number of allowed daemons has been reached
    // c) the maximum number of idle channels has been reached
    // d) the reap list is less than 80% full
    static unsigned count = 0;
    count++;
    if ((count % 30 != 0) &&
        _num_daemons < _max_daemons &&
        _num_idle_channels < _max_idle_channels &&
        _daemon_reap_list.num >= (_num_daemons * 80 + 99) / 100)
    {
        return;
    }

    // Build a Daemon * array that contains a reference to each daemon
    int num_daemons = 0;
    Daemon **daemons = (Daemon **)PERM_MALLOC(_max_daemons * sizeof(Daemon *));
    if (daemons) {
        for (int h = 0; h < DAEMON_HT_SIZE; h++) {
            PR_Lock(_daemon_ht[h].lock);
            Daemon *daemon = _daemon_ht[h].head;
            while (daemon) {
                PR_AtomicIncrement(&daemon->ref);
                daemons[num_daemons++] = daemon;
                daemon = daemon->ht_next;
            }
            PR_Unlock(_daemon_ht[h].lock);
        }
    }

    PRIntervalTime now = ft_timeIntervalNow();

    // For each daemon...
    for (i = 0; i < num_daemons; i++) {
        Daemon *daemon = daemons[i];

        // Remove stale channels from the tail of the idle list
        DaemonChannel *idle = NULL;
        PR_Lock(daemon->idle.lock);
        while (daemon->idle.tail && stale(now, daemon->idle.tail)) {
            idle = daemon->idle.tail;
            daemon->idle.tail = idle->prev;
        }
        if (idle) {
            if (daemon->idle.tail) {
                daemon->idle.tail->next = NULL;
            } else {
                daemon->idle.head = NULL;
            }
        }
        PR_Unlock(daemon->idle.lock);

        // Discard the stale channels
        while (idle) {
            DaemonChannel *next = idle->next;
            free_idle_channel(idle);
            idle = next;
        }

        // Update the inactivity moving average
        PRIntervalTime elapsed = now - daemon->timestamp;
        if (daemon->inactivity != INT_MAX) {
            if (elapsed >= 0x3fffffff) {
                daemon->inactivity = INT_MAX;
            } else {
                daemon->inactivity = (elapsed + daemon->inactivity) / 2;
            }
        }
    }

    // Sort the Daemon * array by activity, most active first
    qsort(daemons,
          num_daemons,
          sizeof(daemons[0]),
          compare_daemon_inactivity);

    // Build a new _daemon_reap_list linked list with the least active daemon
    // at the head
    PR_Lock(_daemon_reap_list.lock);
    Daemon *old = _daemon_reap_list.head;
    while (old) {
        Daemon *next = old->reap_next;
        unref_daemon(old);
        old = next;
    }
    Daemon *reap_head = NULL;
    for (i = 0; i < num_daemons; i++) {
        daemons[i]->reap_next = reap_head;
        reap_head = daemons[i];
    }
    _daemon_reap_list.head = reap_head;
    _daemon_reap_list.num = num_daemons;
    PR_Unlock(_daemon_reap_list.lock);

    // Discard the Daemon * array.  The _daemon_reap_list linked list has
    // inherited the array's daemon references.
    PERM_FREE(daemons);
}
PR_END_EXTERN_C
