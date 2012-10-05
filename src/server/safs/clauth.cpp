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
 * Implementation for client authentication in a secure server.
 * 
 */

#include <netsite.h>

#ifndef XP_CPLUSPLUS
#define XP_CPLUSPLUS
#endif
#include "cert.h"
#include "ssl.h"
#include "sslerr.h"
#include "secerr.h"
#include "base64.h"

#include <prio.h>

#include "safs/clauth.h"
#include "safs/dbtsafs.h"
#include "safs/init.h"
#include "frame/req.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/conf.h"
#include "frame/conf_api.h"
#include "base/pblock.h"
#include "base/session.h"
#include "base/util.h"
#include "base/shexp.h"
#include <ldaputil/certmap.h>
#include <ldaputil/errors.h>

#include <libaccess/nsauth.h>
#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include <libaccess/ldapacl.h>
#include <libaccess/nullacl.h>

#include "base/sslconf.h"
#include "frame/http_ext.h"

/* Want enum string ids to allow a later array initializer to work */
#define WANT_ENUM_STRING_IDS

#include <secerr.h>
#include <sslerr.h>

#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <httpdaemon/httprequest.h>
#include <httpdaemon/daemonsession.h>
#include "frame/filter.h"

#define DEFAULT_SSL_HANDSHAKE_TIMEOUT_INTERVAL  PR_SecondsToInterval(30)

/* private prototypes implemented here */
static int ca_GetClientCert(Session* sn, Request* rq);
static void ca_HandshakeDone(PRFileDesc *fd, void *doneflag);

/*
 * Getting a Cert
 * --------------
 * Get the current SSL peer certificate, putting it onto
 * sn->clauth->cla_cert. If it is not present, return failure
 * immediately.
 *
 * Return value:
 *    REQ_ABORTED on failure
 *    REQ_PROCEED on success with a CERTCertificate* on sn->clauth->cla_cert.
 */
NSAPI_PUBLIC int
CA_GetClientCert(pblock *pb, Session *sn, Request *rq)
{
    ClAuth_t *cla = (ClAuth_t*)sn->clauth;
    CERTCertificate *cert = SSL_PeerCertificate(sn->csd);
    if (cla->cla_cert != NULL)
    	CERT_DestroyCertificate(cla->cla_cert);
    cla->cla_cert = cert;
    return (cert == NULL) ? REQ_ABORTED : REQ_PROCEED; 
}

/*
 * Requesting a Cert
 * ------------------
 *
 * Request the cert, redo the handshake (even if a cert is
 * already present).
 *
 * Return value:
 *    REQ_ABORTED on failure with error code obtainable by PR_GetError()
 *    REQ_PROCEED on success with a CERTCertificate* on sn->clauth->cla_cert.
 */
NSAPI_PUBLIC int
CA_RequestClientCert(pblock *pb, Session *sn, Request *rq)
{
    int rv = ca_GetClientCert(sn, rq);
    if (rv == REQ_PROCEED) {
	rv = CA_GetClientCert(pb, sn, rq);
    }
    return rv;
}

/***********************************************************/

/*
 * The following section is support for redoing the SSL
 * Handshake in order to acquire a client certificate.
 * 
 * The particularly messy part is reading the client data
 * to get it out of the way so SSL can use the transport to
 * run its protocol. That data is read onto the end of the
 * sn->inbuf socket buffer, expanding the size of the buffer
 * as needed.  It is more important to be able to finish the
 * handshake than to successfully capture all the client data.
 * 
 * If the handshake completes, but for some reason the user
 * data cannot be replayed, the operation can be aborted and
 * retried. The next time around, the certificate will already
 * be on board and we won't have to redo the handshake. Sure,
 * that's far from optimal, but it is progress.
 */

#define MIN_SSL_RECORD_READ_SIZE (34 * 1024)
static const int CA_ALLOCATE = MIN_SSL_RECORD_READ_SIZE; /* allocation quantum */

static void
ca_HandshakeDone(PRFileDesc *fd, void *doneflag)
{
    /*
     * This routine is called by SSL when the handshake is
     * complete. It sets local state so that the loop doing
     * reads from the socket can exit.
     */
    *((int *)doneflag) = 1;
}

//
// ca_GetClientCert - do the dirty work of actually getting a client cert
//
// We have to redo the SSL handshake because the original one did not
// get us the client's certificate.
// The problem is that we are requesting a new handshake, but it may not
// happen right away - especially when there is ongoing data transfer.
// SSL handshakes are completely out-of-band.
// Still, in case there's no data transfer going on, we need to kick
// off the handshake by calling PR_Recv on the SSL. We have to save
// the data we might get and put it back into the session's inbuffer
// 
// XXX The whole certificate getting stuff should be part and a special
// case of the net_read machinery. It should have callbacks that would call
// you once the certicate arrives.
//
static int
ca_GetClientCert(Session* sn, Request* rq)
{
    netbuf *sninbuf = sn->inbuf;  /* get closer */
    PRFileDesc *csd = sn->csd;
    int handshake_done = 0;
    int ran_out = 0;
    int rv = REQ_PROCEED;

    const SSLSocketConfiguration *sslc = GetSecurityParams(rq);
    if (!sslc) {
        log_error(LOG_MISCONFIG, CA_PROGRAM, sn, rq,
                  XP_GetAdminStr(DBT_clauthError6));
        return REQ_ABORTED;
    }

    PRIntervalTime timeout = sslc->clientAuthTimeout.getPRIntervalTimeValue();
    int bytelimit = sslc->maxClientAuthData;

    // Use IO Timeout for SSL_ReHandshakeWithTimeout and 
    // SSL_ForceHandshakeWithTimeout
    PRIntervalTime ioTimeout = DEFAULT_SSL_HANDSHAKE_TIMEOUT_INTERVAL;
    const HttpRequest* hrq = GetHrq(rq);
    if (hrq) {
        const DaemonSession &dsn = hrq->GetDaemonSession();
        ioTimeout = dsn.GetIOTimeout();
    }

    // Set up the machinery to redo the handshake. We set an handshake callback
    // that will notify us when the handshake has completed, set the options to
    // request, but not require a certificate, then tell SSL to do an handshake
    // on occasion.
    // Do NOT call SSL_ResetHandshake as this will tear down the existing
    // connection.
    if (SSL_HandshakeCallback(csd, ca_HandshakeDone, (void *)&handshake_done) ||
        SSL_OptionSet(csd, SSL_REQUEST_CERTIFICATE, PR_TRUE) ||
        SSL_OptionSet(csd, SSL_REQUIRE_CERTIFICATE, PR_FALSE) ||
        SSL_ReHandshakeWithTimeout(csd, PR_TRUE, ioTimeout))
    {
        // something went wrong
	int errCode = PR_GetError();
	if (errCode == SEC_ERROR_INVALID_ARGS) {
	    /* Treat this code specially - it means it's not an SSL3 socket */
	    if (!sslc->ssl3) {
		/* SSL3 is not enabled */
		log_error(LOG_MISCONFIG, CA_PROGRAM, sn, rq, 
		          XP_GetAdminStr(DBT_clauthError6));
	    } else {
		/* Client couldn't/wouldn't step up to SSL3 */
		log_error(LOG_FAILURE, CA_PROGRAM, sn, rq,
		          XP_GetAdminStr(DBT_clauthError7));
	    }
	} else {
	    log_error(LOG_FAILURE, CA_PROGRAM, sn, rq, 
		      XP_GetAdminStr(DBT_clauthError8),
		      system_errmsg());
	}
	return REQ_ABORTED;
    }

    // after the handshake has been requested, we need to read off and buffer
    // incoming data that may already be in the OS buffers in order to make the
    // handshake commence.
    // 
    int allocation_step = CA_ALLOCATE;
    while (!handshake_done) {
        int result;

        result = SSL_DataPending(csd);
        if (!result) {
            /* no data to read, so force handshake */
            result = SSL_ForceHandshakeWithTimeout(csd, ioTimeout);
            if (result > 0)
                result = 0;
        } else {
           /* data available to be read. */

            if (ran_out) {
                // reset position in inbuf (we're going to throw the data away anyway)
                sninbuf->cursize = 0;
            } else if (!sninbuf->inbuf) {
               if (sninbuf->maxsize < MIN_SSL_RECORD_READ_SIZE)
                     sninbuf->maxsize = MIN_SSL_RECORD_READ_SIZE;
                sninbuf->inbuf = (unsigned char *)MALLOC(sninbuf->maxsize);
                sninbuf->pos = 0;
                sninbuf->cursize = 0;
            } else if (sninbuf->cursize >= bytelimit) {
                // Ran out of space while buffering client data.
                // maximum data size exceeded. The request will be aborted.
                // just reuse this buffer over again until the request
                // is completely received.  Then toss it.

                ran_out = 1;
                sninbuf->pos = 0;
                sninbuf->cursize = 0;
            } else if ((sninbuf->maxsize - sninbuf->cursize) < CA_ALLOCATE) {
                // less than our allocation quantum is available in the buffer -
                // grow it, up to bytelimit.
                //
                // we need to be really careful not to call REALLOC too often
                // because it will NOT free the old chunk, and we'll run out
                // of swap space & memory real fast. therefore, we grow
                // the size of the buffer exponentially if data keeps coming.

                int newsize = sninbuf->maxsize + allocation_step;

                // limit the size to the max. amount we're willing to buffer
                if (newsize > bytelimit) {
                    // hit the ceiling.
                    // (next time we fill it we're running out)
                    newsize = bytelimit;
                } else {
                    // grow the step only if we're not hitting the ceiling
                    // to avoid integer overflow.
                    allocation_step *= 2;
                }

                // see if we're actually growing it or whether we're hitting the ceiling
                if (newsize > sninbuf->maxsize) {
                   unsigned char * newbuf =
                           (unsigned char *)REALLOC(sninbuf->inbuf, newsize);
                   if (newbuf) {
                       sninbuf->inbuf = newbuf;
                       sninbuf->maxsize = newsize;
                   } else {
                       /* realloc failed, treat as too long input */
                       ran_out = 1;
                       sninbuf->pos = 0;
                       sninbuf->cursize = 0;
                   }
                }
            }

            //
            // We're assuming that there is application data ready to
            // read, since the handshake is not done, but
            // SSL_ForceHandshake returned without an error.
            result = PR_Recv(csd, &sninbuf->inbuf[sninbuf->cursize],
                             sninbuf->maxsize - sninbuf->cursize, 0,
                             timeout);
            if (result == 0) {
                log_error(LOG_FAILURE, CA_PROGRAM, sn, rq,
                          XP_GetAdminStr(DBT_clauthError9));
                rv = REQ_ABORTED;  /* an EOF now isn't good */
                break;
            }
        }

        if (result < 0) {
            int errCode = PR_GetError();
            INTnet_cancelIO(csd);

            /* Can PR_WOULD_BLOCK_ERROR happen? */
            if (errCode == PR_WOULD_BLOCK_ERROR) {
                /* Put your breakpoint here to find out */
                log_error(LOG_FAILURE, CA_PROGRAM, sn, rq,
                          "Got PR_WOULD_BLOCK_ERROR");
                continue;
            }

            /* some other error */
            if (IS_SEC_ERROR(errCode) || IS_SSL_ERROR(errCode)) {
                /* SEC or SSL Error */
                log_error(LOG_FAILURE, CA_PROGRAM, sn, rq,
                          XP_GetAdminStr(DBT_clauthError10),
                          system_errmsg());
            } else if (errCode == PR_CONNECT_RESET_ERROR) {
                /* Connection reset by peer */
                log_error(LOG_FAILURE, CA_PROGRAM, sn, rq,
                          XP_GetAdminStr(DBT_clauthError11));
            } else if (errCode == PR_IO_TIMEOUT_ERROR) {
                log_error(LOG_FAILURE, CA_PROGRAM, sn, rq,
                          XP_GetAdminStr(DBT_clauthError12)); 
            } else {
                log_error(LOG_FAILURE, CA_PROGRAM, sn, rq, 
                          XP_GetAdminStr(DBT_clauthError13),
                          system_errmsg());
            }

            /* get out now */
            rv = REQ_ABORTED;
            break;
        }

        sninbuf->cursize += result;
    }

    // we either ran out of space or the handshake happened

    // Reset the ssl callback
    SSL_HandshakeCallback(csd, NULL, NULL);

    if (ran_out) {
        // we discarded data, so let's fail the request.
        log_error(LOG_FAILURE, CA_PROGRAM, sn, rq, 
                  XP_GetAdminStr(DBT_clauthError14));
        rv = REQ_ABORTED;
    }

    return rv;
}


/*****************************************************************/

/* NSAPI SAF Support 
 * 
 * The following section contains the entry points for the SAFs directly
 * configurable from obj.conf.
 *
 */


//
// checkmethod - check the request's method against a shell expression from the pblock
//
static int
checkmethod(pblock *pb, Session *sn, Request *rq)
{
    int rv = REQ_PROCEED;
    char *fn = 0;
    char *method = 0;
    char *methodexp = 0;

    methodexp = pblock_findval("method", pb);
    if (methodexp) {

	method = pblock_findval("method", rq->reqpb);
	if (method == 0) method = "";   /* method should always be there */

	switch (WILDPAT_CASECMP(method, methodexp)) {

	  case -1:		/* methodexp is invalid */
	    fn = pblock_findval("fn",pb);
	    if (fn == 0) fn = "unknown function";
	    log_error(LOG_MISCONFIG, CA_PROGRAM, sn, rq,
		      XP_GetAdminStr(DBT_clauthError15), fn);
	    rv = REQ_ABORTED;
	    break;

	  case 0:		/* method matches methodexp */
	    rv = REQ_PROCEED;
	    break;

	  case 1:		/* method doesn't match methodexp */
	    rv = REQ_NOACTION;
	    break;

	}
    }

    return rv;
}

//
// CA_getcert - implements the "get-client-cert" SAF
//
// Get the client cert.  If it is not already present on the session,
// and the value of "dorequest" is nonzero, then redo the handshake
// and get it.
//
// If a "method" parameter is present in the directive parameter block
// it should be a shell expression, and this operation is only done on
// requests where the method matches the shell expression specified in
// the directive.
//
// Success: If a cert is obtained successfully put the base64
// DER-encoded cert in "auth-cert" in the rq->vars pblock,
// return REQ_PROCEED.
//      
// Failure: If the value of "require" is present and is 0,
// in the pblock return REQ_NOACTION, otherwise it is REQ_ABORTED
// with protocol status set to FORBIDDEN.  (Note this means "require"
// is the default).  
//
// e.g.
// # This gets a cert, requesting it if it is not already present and
// # failing the request if it can't obtain an acceptable cert.
// # only on POST requests.
// PathCheck fn="get-client-cert" method="POST" dorequest="1" require="1"
//
NSAPI_PUBLIC int
CA_getcert(pblock *pb, Session *sn, Request *rq)
{
    int rv = REQ_PROCEED;
    CERTCertificate *cert = 0;
    char *certb64 = 0;

    rv = checkmethod(pb,sn,rq);
    if (rv != REQ_PROCEED) {
	return rv;
    }

    const SSLSocketConfiguration* sslc = GetSecurityParams(rq);
    if (!sslc || !sslc->ssl3)
    {
	log_error(LOG_MISCONFIG, CA_PROGRAM, sn, rq, 
	          XP_GetAdminStr(DBT_clauthError16)); 
	/* Tentatively fail, but actual value depends on "require" below */
	rv = REQ_ABORTED;
    } else {
	rv = CA_GetClientCert(pb, sn, rq);
	if (rv != REQ_PROCEED) {
	    /* don't already have a cert */
	    char *dorequest_val;
	    dorequest_val = pblock_findval("dorequest", pb);
	    if (dorequest_val && strcmp(dorequest_val,"0") != 0) {
		/* we are asked to request one from client */
		rv = CA_RequestClientCert(pb, sn, rq);
	    }
	}
    }
    
    if (rv != REQ_PROCEED) {
	/*
	 * Don't have the client's cert.  We return either REQ_NOACTION
	 * or REQ_ABORTED depending on whether a cert was supposed to
	 * be required to get past this directive or not.
	 */
	char *require_val;
	require_val = pblock_findval("require", pb);
	if (require_val && strcmp(require_val,"0") == 0) {
	    /* we DONT require that a cert be present at this point*/
	    return REQ_NOACTION;
        } else {
	    /* "require" is not present in pb or is 1, go no further */
	    protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
	    return REQ_ABORTED;
	}
    }
    
    /* if we get here we have a cert on sn->clauth cla_cert */
    /* base64 encode it and put it into "auth-cert" in rq->vars */
    cert = ((ClAuth_t*)sn->clauth)->cla_cert;
    certb64 = BTOA_DataToAscii(cert->derCert.data,
			      cert->derCert.len);
    if (certb64 == 0) {
	log_error(LOG_FAILURE, CA_PROGRAM, sn, rq, 
		        XP_GetAdminStr(DBT_clauthError17)); 
	return REQ_ABORTED;
    }
    param_free(pblock_remove("auth-cert", rq->vars));
    pblock_nvinsert("auth-cert", certb64, rq->vars);
    /* pblock_nvinsert will strdup() the value, we need to free ours */
    /* XXX ??? is this the right way to call free? */
    PORT_Free(certb64);
    return REQ_PROCEED;
    
}			/* CaGetCert */


// CACert2user, CA_GetUserID used to be here, it's gone as of iWS5.0 because it was ES2NSDB stuff


//
// get_user_cert_ssl - attribute getter for ACL_ATTR_USER_CERT
//
int
get_user_cert_ssl (NSErr_t *errp, PList_t subject, PList_t resource,
		       PList_t auth_info, PList_t global_auth, void *unused)
{
    /* Get the user certificate */
    pblock *pb;
    Session *sn;
    Request *rq;
    CERTCertificate *cert;
    int rv;

    // get hold of session and request structures
    rv = PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL);
    if (rv < 0 || !sn) {
        // that's normal if we don't have a cert already
        return LAS_EVAL_FAIL;
    }
    rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, (void **)&rq, NULL);
    if (rv < 0 || !rq) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_clauthEreport2), rv);
        return LAS_EVAL_FAIL;
    }

    pb = pblock_create(4);
    pblock_nvinsert("dorequest", "1", pb);
    pblock_nvinsert("require", "0", pb);    // to get us a distiction between "no cert passed"
					    // and some configuration error
    // use get-client-cert SAF
    rv = CA_getcert(pb, sn, rq);
    pblock_free(pb);

    switch (rv) {
    case REQ_PROCEED:
	break;
    case REQ_ABORTED:
	return LAS_EVAL_FAIL;
    case REQ_NOACTION:
	// just no cert passed by client
	// we don't want to return a 500, but a 403
	return LAS_EVAL_FALSE;
    }

    // put it into the session
    cert = ((ClAuth_t*)sn->clauth)->cla_cert;

    // then do what we came here for - the the ACL_ATTR_USER_CERT attrribute
    PListInitProp(subject, ACL_ATTR_USER_CERT_INDEX, ACL_ATTR_USER_CERT, cert, 0);
    return LAS_EVAL_TRUE;
}

int
ldapu_benign_error (const int rv)
{
    // treat all pure LDAP errors as serious except for LDAP_NO_SUCH_OBJECT
    // All ldaputil and certmap errors are benign
    return (rv < 0 || rv == LDAP_NO_SUCH_OBJECT);
}

//
// get_auth_user_ssl - attribute getter for authenticated user (ACL_ATTR_USER) with SSL client auth
//
int
get_auth_user_ssl (NSErr_t *errp, PList_t subject, 
		       PList_t resource, PList_t auth_info,
		       PList_t global_auth, void *unused)
{
    Session *sn = 0;
    Request *rq = 0;
    char *dbname = 0;
    ACLDbType_t dbtype;
    int failed;
    int rv;
    char *user = 0;
    void *dbhandle = 0;
    CERTCertificate *cert;
    char *userdn = 0;
    char *certmap = 0;
    char* cached_user = NULL, *authreq = NULL;
    pool_handle_t *subj_pool = PListGetPool(subject);

    // are we just checking whether we need to authenticate?
    rv = PListGetValue(resource, ACL_ATTR_AUTHREQCHECK_INDEX, (void **)&authreq, NULL);
    if ((rv >= 0) && authreq && *authreq) {
        authreq[0] = '*';       // check the fact that we needed an authenticated user & fail 
        return LAS_EVAL_FAIL;
    }

    // do we have a cached user for this session? If so, we'll use that one as authenticated user
    // (XXX will that work for virtual servers?? A session might have requests for multiple VS!)
    rv = PListGetValue(subject, ACL_ATTR_CACHED_USER_INDEX, (void **)&cached_user, NULL);
    if ((rv >= 0) && cached_user && *cached_user) {
        PListInitProp(subject, ACL_ATTR_USER_INDEX, ACL_ATTR_USER, cached_user, 0);
        return LAS_EVAL_TRUE;
    }

    // now get hold of the certificate
    rv = ACL_GetAttribute(errp, ACL_ATTR_USER_CERT, (void **)&cert, subject,
			  resource, auth_info, global_auth);
    switch (rv) {
    case LAS_EVAL_TRUE:
	break;	// got a cert
    case LAS_EVAL_FALSE:
	ereport(LOG_SECURITY, XP_GetAdminStr(DBT_clauthEreport15));
	return LAS_EVAL_FALSE;
    default:
        return rv;
    }

    // if another "authenticate" statement set up BasicAuth already,
    // but we DID get a cert, we don't want to do the BasicAuth stuff, too.
    // The certificate authentication may still fail, of course.
    PListDeleteProp(resource, ACL_ATTR_WWW_AUTH_PROMPT_INDEX, ACL_ATTR_WWW_AUTH_PROMPT);

    // get the request structure pointer
    rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, (void **)&rq, NULL);
    if (rv < 0 || !rq) {
        // we do not bother if we don't have one
        // (XXX what about accelerated requests??)
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_clauthEreport3), rv);
        return LAS_EVAL_FAIL;
    }

    // get hold of the authentication database
    rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
    if (rv < 0 || !dbname) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_clauthEreport4), rv);
        return LAS_EVAL_FAIL;
    }
    rv = ACL_DatabaseFind(errp, dbname, &dbtype, &dbhandle);
    if (rv != LAS_EVAL_TRUE) {
	ereport(LOG_SECURITY, XP_GetAdminStr(DBT_clauthEreport5), dbname);
	return rv;
    }

    // get hold of the certmap for this ACL
    if (PListGetValue(auth_info, ACL_ATTR_CERTMAP_INDEX, (void **)&certmap, NULL) < 0) {
	// This ACL has no certmap attribute
	certmap = 0;
    }

    /* Assume general failure of LDAP case */
    failed = 1;

    // now map the cert to a user
    // we only have code here for LDAP and NULL databases
    // XXX this should probably be pluggable!
    if (ACL_DbTypeIsEqual(NULL, dbtype, ACL_DbTypeLdap)) {
        // complicated complicated complicated
        // XXX this needs work
        rv = acl_map_cert_to_user_ldap(errp, dbname, dbhandle, (void *)cert,
			      resource, subj_pool, &user, &userdn, certmap);
        if (rv == LDAPU_SUCCESS && user) {
            failed = 0;
            rv = LAS_EVAL_TRUE;
        }
    } else if (ACL_DbTypeIsEqual(NULL, dbtype, ACL_DbTypeNull)) {
	// simple simple simple
	switch ((size_t)dbhandle) {
	case NULLDB_NONE:
	    // cert matches no user in the user db
	    failed = 4;
	    rv = LAS_EVAL_FALSE;
	    break;
	case NULLDB_ALL:
	    // map cert to user "null"
	    failed = 0; // no failure here
	    user = pool_strdup(subj_pool, "null");
	    rv = LAS_EVAL_TRUE;
	    break;
	}
    }

    if (failed) {
        //
        // we did not arrive at a uid
        // so print out a verbose error message
        //
	char *subjectDN = 0;
	char *issuerDN = 0;
	int  tmp_rv;

	ldapu_get_cert_subject_dn((void *)cert, &subjectDN);
	ldapu_get_cert_issuer_dn((void *)cert, &issuerDN);

        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_clauthEreport13),
                ldapu_err2string(rv), issuerDN ? issuerDN : "unknown",
                subjectDN ? subjectDN : "unknown");

	ldapu_free(subjectDN);
	ldapu_free(issuerDN);

        //
        // now, make the user select the certificate again, in
        // case they accidentally provided the wrong one.
        // Ideally we'd like to invalidate the session only if
        // the user hits "Cancel" on the authentication request,
        // but that doesn't seem to send anything to the server.
        // This makes the one-time login more awkward, but this
        // seems to be the only way to give the user a chance
        // to pick another certificate if they make a mistake.
        //
	tmp_rv = PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL);
	if (tmp_rv < 0 || !sn) {
	    ereport(LOG_SECURITY, XP_GetAdminStr(DBT_clauthEreport14), rv);
	    return LAS_EVAL_FAIL;
	}
	SSL_InvalidateSession(sn->csd);

        if (failed == 1) {
            // Return FAIL only if there was LDAP error.  By returning FALSE we
            // keep the option open for redirection (bong file)
            return ldapu_benign_error(rv) ? LAS_EVAL_FALSE : LAS_EVAL_FAIL;
        }

	return rv;
    }

    // we have an authenticated user id
    // insert all the information into rq->vars
    pblock_nvinsert(ACL_ATTR_AUTH_TYPE, ACL_AUTHTYPE_SSL, rq->vars);
    pblock_nvinsert(ACL_ATTR_AUTH_USER, user, rq->vars);
    pblock_nvinsert(ACL_ATTR_AUTH_DB, dbname, rq->vars);

    if (userdn) {
	pblock_nvinsert(ACL_ATTR_USERDN, userdn, rq->vars);
	PListInitProp(subject, ACL_ATTR_USERDN_INDEX, ACL_ATTR_USERDN, userdn, 0);
    }

    // finally, do what we came here for: set the authenticated user attribute
    PListInitProp(subject, ACL_ATTR_USER_INDEX, ACL_ATTR_USER, user, 0);
    return LAS_EVAL_TRUE;
}
