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

#ifndef __clauth_h_
#define __clauth_h_

#include <frame/req.h>
#include <frame/func.h>
#include <base/pblock.h>
#include <base/session.h>
#include <base/plist.h>
#include <libaccess/nserror.h>

#define CA_PROGRAM	    "Client-Auth"
#define CA_USER_TAG	    "Auth_user"

/*
 * prototypes for client authentication
 */

NSPR_BEGIN_EXTERN_C

/*
 * Getting a Cert
 * --------------
 * Get the current SSL peer certificate, putting it onto
 * sn->clauth->cla_cert. If it is not present, return failure
 * immediately.
 *
 * Return value:
 *   REQ_ABORTED on failure
 *   REQ_PROCEED on success with a SECCertificate* on
 *		sn->clauth->cla_cert.
 */
NSAPI_PUBLIC int CA_GetClientCert(pblock *pb, Session *sn, Request *rq);

/*
 * Requesting a Cert
 * ------------------
 *
 * Request the cert, redo the handshake (even if a cert is
 * already present).
 *
 * Return value:
 *   REQ_ABORTED on failure with error code obtainable by XP_GetError()
 *   REQ_PROCEED on success with a SECCertificate* on sn->clauth->cla_cert.
 */
NSAPI_PUBLIC int CA_RequestClientCert(pblock *pb, Session *sn, Request *rq);

/*
 * Mapping a cert
 * --------------
 *
 * Map the cert on sn->clauth_t->cla_cert using the "userdb" in pb
 * and put the result onto the user object on sn->clauth.
 *
 * IF NO MAPPING EXISTS return an appropriate error code, don't do
 * basic auth automatically.
 *
 * Return values:
 *   REQ_ABORTED on failure with an error code obtainable by XP_GetError()
 *   REQ_PROCEED on success with a UserObj_t* on sn->clauth_t->cla_uoptr
 */
NSAPI_PUBLIC int CA_MapCertToUser(pblock *pb, Session *sn, Request *rq);

#if 0
/* XXX not implemented */
/*
 * Establishing a Mapping Using Basic Auth
 * ---------------------------------------
 *
 * Establish a mapping for the cert on sn->clauth->cla_cert
 * to a userobject and put the newly mapped user object on the
 * sn->clauth_t->cla_uoptr. Use the "userdb" in pb. This returns
 * an error if the mapping is already established.
 *
 * Return values:
 *    REQ_ABORTED on failure with an error code obtainable
 *		by XP_GetError()
 *    REQ_PROCEED on success with a UserObj_t* on
 *		sn->clauth_t->cla_uoptr
 */
NSAPI_PUBLIC int CA_EstCertMapFromBasicAuth(pblock *pb, Session *sn, Request *rq);
#endif

/*
 * Full featured function
 * ----------------------
 * The goal here is to find the "user name" of the client
 * involved in the current secure session. That is done by
 * matching his current certificate with out data base and
 * ferreting out the user name.
 */
NSAPI_PUBLIC int CA_GetUserID(pblock *param, Session *sn,  Request *rq);


/****************************SAFs********************************/

/*
 * getcert --
 *	Get the client cert.  If it is not already present on the session,
 *	and the value of "dorequest" is nonzero, then redo the handshake
 * 	and get it.
 *
 *	Success: If a cert is obtained successfully put the base64
 *	DER-encoded cert in "auth-cert" in the rq->vars pblock,
 *	return REQ_PROCEED.
 *	
 *	Failure: If the value of "require" is present and is 0,
 *	in the pblock return REQ_NOACTION, otherwise it is REQ_ABORTED
 *  with protocol status set to FORBIDDEN.
 *	(Note this means "require" is the default).  
 *
 *	e.g.
 *	# This gets a cert, requesting it if it is not already present and
 *	# failing the request if it can't obtain an acceptable cert.
 *	PathCheck fn="get-client-cert" dorequest="1" require="1"
 *
 */


NSAPI_PUBLIC int CA_getcert(pblock *pb, Session *sn, Request *rq);

/*
 * cert2user
 * ---------
 *	Maps cert on sn->clauth->cla_cert to a user name, using the
 *      db "userdb" and puts the result in "auth-user" in the rq->vars
 *      pblock. If the mapping doesn't exist and "makefrombasic" is
 *		present and nonzero, make it by the basic auth thing.
 *
 *	Success: REQ_PROCEED with "auth-user" and "auth-type" and
 *	"authdb" in rq->vars.
 *
 *  Failure: If "require" is not present or is present and is "1" then
 *  the return value on failure is REQ_ABORTED, otherwise it is
 *  REQ_NOACTION with protocol status set to FORBIDDEN.
 *	(Note this means "require" is the default).  
 *
 *	e.g.
 *		PathCheck fn="cert2user" makefrombasic="0" require="1"
 *
 */

NSAPI_PUBLIC int CA_cert2user(pblock *pb, Session *sn, Request *rq);

extern int get_auth_user_ssl (NSErr_t *errp, PList_t subject, 
			      PList_t resource, PList_t auth_info,
			      PList_t global_auth, void *arg);

extern int get_user_cert_ssl (NSErr_t *errp, PList_t subject,
			      PList_t resource, PList_t auth_info,
			      PList_t global_auth, void *arg);

NSPR_END_EXTERN_C

#endif /* __clauth_h_ */
