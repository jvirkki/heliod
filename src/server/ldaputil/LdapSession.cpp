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

#include "LdapSession.h"
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include "libaccess/LdapRealm.h"
#include "LinkedList.hh"
#include "base/util.h"
#include <ldap.h>
#include <ldappr.h>
#include <ldaputil/certmap.h>
#include <ldaputil/errors.h>
#include <ldaputili.h>
#include <plstr.h>
#ifdef USE_LDAP_SSL
#include "ldap_ssl.h"
#include "base/util.h" // util_*
#endif
#if defined(AIX)
#include <strings.h>
#endif
#include "definesEnterprise.h"
#include "base/ereport.h"
#include "libaccess/LdapRealm.h"
#include <frame/log.h>
#include "ldaputil/dbtldaputil.h"

const int LdapSession::maxRetries = 10;

#ifdef LDAP_OPT_DNS_FN_PTRS
static LDAPHostEnt *
ldapu_copyPRHostEnt2LDAPHostEnt( LDAPHostEnt *ldhp, PRHostEnt *prhp )
{
	ldhp->ldaphe_name = prhp->h_name;
	ldhp->ldaphe_aliases = prhp->h_aliases;
	ldhp->ldaphe_addrtype = prhp->h_addrtype;
	ldhp->ldaphe_length =  prhp->h_length;
	ldhp->ldaphe_addr_list =  prhp->h_addr_list;
	return( ldhp );
}

static LDAPHostEnt *
ldapu_gethostbyname( const char *name, LDAPHostEnt *result,
	char *buffer, int buflen, int *statusp, void *extradata )
{
	PRHostEnt	prhent;

#if defined(LDAPSDK_KNOWS_IPV6) // not yet, guys...
	if( !statusp || 
	    ( *statusp = (int)PR_GetIPNodeByName( name, 
			PR_AF_INET6,PR_AI_DEFAULT,
			buffer, buflen, &prhent )) == PR_FAILURE ) {
		return( NULL );
	}
#else
	if( !statusp || 
	    ( *statusp = (int)PR_GetIPNodeByName( name, 
			PR_AF_INET,PR_AI_DEFAULT,
			buffer, buflen, &prhent )) == PR_FAILURE ) {
		return( NULL );
	}
#endif

	return( ldapu_copyPRHostEnt2LDAPHostEnt( result, &prhent ));
}

static LDAPHostEnt *
ldapu_gethostbyaddr( const char *addr, int length, int type,
	LDAPHostEnt *result, char *buffer, int buflen, int *statusp,
	void *extradata )
{
	PRHostEnt	prhent;
        PRNetAddr       naddr;

	if (util_init_PRNetAddr(&naddr, (char *)addr, length, type)) {
		return NULL;
	}

        if ( !statusp ||
             ( *statusp = (int)PR_GetHostByAddr( &naddr, buffer, buflen,
                                                 &prhent ) ) == PR_FAILURE ) {
                return( NULL );
        }

	return( ldapu_copyPRHostEnt2LDAPHostEnt( result, &prhent ));
}
#endif /* LDAP_OPT_DNS_FN_PTRS */

/*
LdapSession::LdapSession(LdapServerSet *_servers, dyngroupmode_t _mode)
{
    // just initialize the infrastructure, do not connect
    // XXX choose one of the servers in the set.
    // XXX no need to reconnect if connected already...
    session = NULL;
    servers = _servers;
    dyngroupmode = _mode;
    (void)init();
}
*/

LdapSession::LdapSession(LdapRealm *realm)
{
    ldapRealm = realm;
    session = NULL;
    //servers = NULL;
    //dyngroupmode = _mode;
    (void)init();
}


LdapSession::~LdapSession(void)
{
    if (session) {
        ldap_unbind_s(session);
        session = NULL;
    }
}

void LdapSession::_duplicate(void)
{
    refcount++;
}

void LdapSession::_release(void)
{
    refcount--;
    if (!refcount)
        delete this;
}

//
// LdapSession::init - create and initialize a new LDAP connection
//
int
LdapSession::init(void)
{
    const char *hostname;
    const char *username;
    const char *password;
    unsigned short port;
    int rv;

    // get rid of old session, if there
    if (session) {
        ldap_unbind_s(session);
        session = NULL;
    }

    // find out about our default bind parameters
    //servers->getCurrentServer(hostname, port, username, password, epoch);

    // create the new session
    refcount = 1;
#ifdef USE_LDAP_SSL
    if (ldapRealm->needSSL()) {
        /* get a handle to an LDAP connection */
        session = ldapssl_init(ldapRealm->prepLdaphosts(),ldapRealm->getDefport(),1);

        /* If we connect to a LDAP server via SSL and client auth is required
         * in LDAP server, we set property client-cert-nickname
         */
        const char* certnickname = ldapRealm->getClientCertNickName();
        if (certnickname) {
            rv = ldapssl_enable_clientauth(session,"", (char *)"",
                                           (char *)certnickname);
            if (rv < 0) {
                const char *err = ldap_err2string(rv);
                log_error(LOG_WARN, "ldapsession init", NULL, NULL,
                          XP_GetAdminStr(DBT_LdapSSLEnableClientAuth),
                          certnickname, err?err:"NULL", system_errmsg());
                return(LDAPU_ERR_LDAP_INIT_FAILED);
            }
       }
    } else 
#endif
        session = prldap_init(ldapRealm->prepLdaphosts(), ldapRealm->getDefport(), 1); //?????

    if (session == NULL)
        return LDAPU_ERR_LDAP_INIT_FAILED;

    boundto = NONE;

    // set LDAP v3
    const int desiredVersion = LDAP_VERSION3;
    rv = ldap_set_option(session, LDAP_OPT_PROTOCOL_VERSION, (void *)&desiredVersion);

    int value = LDAP_DEREF_NEVER;
    // we should evaluate the security implementations of this. XXXJBS
    rv |= ldap_set_option(session, LDAP_OPT_DEREF, &value);
    // automatic referrals handling
    rv |= ldap_set_option(session, LDAP_OPT_REFERRALS, LDAP_OPT_ON);
    // automatic reconnect using ldap_simple_bind_s
    rv |= ldap_set_option(session, LDAP_OPT_RECONNECT, LDAP_OPT_ON);

    // install DNS functions
    struct ldap_dns_fns	dnsfns;
    memset( &dnsfns, '\0', sizeof(struct ldap_dns_fns) );
    dnsfns.lddnsfn_bufsize = PR_NETDB_BUF_SIZE;
    dnsfns.lddnsfn_gethostbyname = ldapu_gethostbyname;
    dnsfns.lddnsfn_gethostbyaddr = ldapu_gethostbyaddr;
    rv |= ldap_set_option(session, LDAP_OPT_DNS_FN_PTRS, (void *)&dnsfns);

    if (rv)
        return LDAPU_ERR_LDAP_SET_OPTION_FAILED;

    return LDAPU_SUCCESS;
}

// Bind using default info for this server
// returns an LDAP error code
int
LdapSession::bindAsDefault(int retryCount)
{
    const char *hostname;
    const char *username;
    const char *password;
    unsigned short port;

    /*
    if (retryCount >= 0) {
        servers->getServer(retryCount, hostname, port, username, password,
                           epoch);
    } else {
        servers->getCurrentServer(hostname, port, username, password, epoch);
    }

    // update the server hostname and port
    int hlen = strlen(hostname) + 1 + strlen("65535") + 1;
    char *h = (char *)malloc(hlen);
    util_snprintf(h, hlen, "%s:%d", hostname, port);
    ldap_set_option(session, LDAP_OPT_HOST_NAME, h);
    free(h);
    */

    // bind (connect) to the server
    // Using async bind instead of sync bind see bug: 6295325
    int msg_id = ldap_simple_bind(session, ldapRealm->getBindName(), ldapRealm->getBindPwd());
    LDAPMessage *res;
    struct timeval time_out;
    time_out.tv_sec = (time_t)ldapRealm->getTimeOut();
    time_out.tv_usec = 0L;
    int ret = ldap_result( session, msg_id, 0, &time_out, &res);
    lastbindrv = ldap_result2error(session, res, 0);
    boundto = (lastbindrv == LDAP_SUCCESS) ? DEFAULT : NONE;
    if (res)
        ldap_msgfree(res);
    // we assume that the default bindDN will never expire
    return lastbindrv;
}

int
LdapSession::reconnect(int currentCount)
{
    // allow one extra retry to rebind to first server when only one server.
    /*
    if (currentCount >= servers->count())
        return LDAPU_FAILED; // we've already tried them all.
    servers->bumpEpoch(epoch);
    */
    // Call init() so that the server binds to the next available LDAP server
    // in case the current LDAP server is down.
    unsigned long tmprefcount = refcount;
    (void)init();
    refcount = tmprefcount;
    return bindAsDefault(currentCount);
}

PRBool
LdapSession::serverDown(int statusCode)
{
      return ((statusCode == LDAP_CONNECT_ERROR) ||
                (statusCode == LDAP_SERVER_DOWN) ||
                (statusCode >= LDAP_OTHER));
}

//
// base LDAP operations
//
// ::search is the only one we use...
//
LdapSearchResult *
LdapSession::search(const char *base, int scope, const char *filter, char **attrs, 
				      int attrsonly, int retryCount)
{
    int rv = LDAP_SUCCESS;
    LDAPMessage *search_results = NULL;

    if (retryCount > maxRetries)
        return new LdapSearchResult(this, NULL, LDAP_CONNECT_ERROR);
    if (boundto != DEFAULT)
        rv = bindAsDefault();
    // Using async search instead of sync search see bug: 6295325
    if (rv == LDAP_SUCCESS)
    {
        struct timeval time_out;
        time_out.tv_sec = (time_t)ldapRealm->getTimeOut();
        time_out.tv_usec = 0L;
        rv = lastoprv = ldap_search_ext_s(session, base, scope, filter, 
                                   attrs, attrsonly,
                                   NULL, NULL, &time_out, LDAP_NO_LIMIT, &search_results);
        if (rv == LDAP_TIMEOUT)
            ereport(LOG_VERBOSE, (char*)"Ldap Search Timeout");
    }
    if (!serverDown(rv))
        return new LdapSearchResult(this, search_results, rv);

    // retry after failure;
    while ( (rv != LDAP_SUCCESS) && (retryCount <= maxRetries) ) {
        rv = reconnect(retryCount++);
        if (rv == LDAP_SUCCESS)
            return search(base, scope, filter, attrs, attrsonly, retryCount);
    }

    return new LdapSearchResult(this, NULL, LDAP_CONNECT_ERROR);
}

int
LdapSession::compare(const char *dn, char *attr, char * value, int retryCount)
{
    int rv = LDAP_SUCCESS;

    if (retryCount > maxRetries)
        return LDAP_CONNECT_ERROR;
    if (boundto != DEFAULT)
        rv = bindAsDefault();
    if (rv == LDAP_SUCCESS)
        rv = lastoprv = ldap_compare_s(session, dn, attr, value);
    if (!serverDown(rv))
        return rv;

    // retry after failure;
    if (reconnect(retryCount++) == LDAP_SUCCESS)
        return compare(dn, attr, value, retryCount);

    return LDAP_CONNECT_ERROR;

}
#if 0
// this stuff is valid & probably working code, but we don't need it
// let's keep it here for reference
// but comment it out so that it does not take up space in the binary
int
LdapSession::add(const char *name, LDAPMod ** val, int retryCount)
{
    if (retryCount > maxRetries)
        return LDAP_CONNECT_ERROR;
    int rv = LDAP_SUCCESS;
    if (boundto != DEFAULT)
        rv = bindAsDefault();
    if (rv == LDAP_SUCCESS)
        rv = lastoprv = ldap_add_ext_s(session, name, val, NULL, NULL);
    if (!serverDown(rv))
        return rv;
    if (reconnect(retryCount++) == LDAP_SUCCESS)
        return add(name, val, retryCount);
    return rv;
}

int
LdapSession::modify(const char *name, LDAPMod **val, int retryCount)
{
    if (retryCount > maxRetries)
        return LDAP_CONNECT_ERROR;
    int rv = LDAP_SUCCESS;
    if (boundto != DEFAULT)
        rv = bindAsDefault();
    if (rv == LDAP_SUCCESS)
        rv = lastoprv = ldap_modify_ext_s(session, name, val, NULL, NULL);
    if (!serverDown(rv))
        return rv;
    if (reconnect(retryCount++) == LDAP_SUCCESS)
        return modify(name, val, retryCount);
    return rv;
}

int
LdapSession::del(const char *name, int retryCount)
{
    if (retryCount > maxRetries)
        return LDAP_CONNECT_ERROR;
    int rv = LDAP_SUCCESS;
    if (boundto != DEFAULT)
        rv = bindAsDefault();
    if (rv == LDAP_SUCCESS)
        rv = lastoprv = ldap_delete_ext_s(session, name, NULL, NULL);
    if (!serverDown(rv))
        return rv;
    if (reconnect(retryCount++) == LDAP_SUCCESS)
        return del(name, retryCount);
    return rv;
}

int
LdapSession::modrdn(const char *source, const char *newrdn, PRBool delOld, int retryCount)
{
    if (retryCount > maxRetries)
        return LDAP_CONNECT_ERROR;
    int rv = LDAP_SUCCESS;
    if (boundto != DEFAULT)
        rv = bindAsDefault();
    if (rv == LDAP_SUCCESS)
        rv = lastoprv = ldap_rename_s(session, source, newrdn, NULL, delOld, NULL, NULL);
    if (!serverDown(rv))
        return rv;
    if (reconnect(retryCount++) == LDAP_SUCCESS)
        return modrdn(source, newrdn, delOld, retryCount);
    return rv;
}
#endif  // unused but valid code

unsigned long
LdapSession::count_entries(LDAPMessage *message)
{
  return ldap_count_entries(session, message);
}

LDAPMessage *
LdapSession::first_entry(LDAPMessage *message)
{
  return ldap_first_entry(session, message);
}

LDAPMessage *
LdapSession::next_entry(LDAPMessage *message)
{
  return ldap_next_entry(session, message);
}

char *
LdapSession::get_dn(LDAPMessage *message)
{
  return ldap_get_dn(session, message);
}

char *
LdapSession::first_attribute_name(LDAPMessage *message, BerElement **iterator)
{
  return ldap_first_attribute(session, message, iterator);
}

char *
LdapSession::next_attribute_name(LDAPMessage *message, BerElement **iterator)
{
  return ldap_next_attribute(session, message, *iterator);
}

struct berval **
LdapSession::get_values(LDAPMessage *message, const char *attr)
{
  return ldap_get_values_len(session, message, attr);
}

int
LdapSession::sort_entries(LDAPMessage *message, char *attr,
			      int (*cmp)(const char *, const char *))
{
  return ldap_sort_entries(session, &message, attr, cmp);
}

int
LdapSession::get_error_code(void)
{
    return ldap_get_lderrno(session, NULL, NULL);
}

const char *
LdapSession::get_error_message(void)
{
    char *rval;
    ldap_get_lderrno(session, NULL, &rval);
    return rval;
}

#if 0
// do we need that one?
static const char *
memchr(const char *s, char c, int n)
{
    if (n <= 0) return 0;
    while (n--) {
	if (*s == c)
	    return s;
	s++;
    }
    return 0;
}
#endif
