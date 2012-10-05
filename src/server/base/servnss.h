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
 *
 * Server NSS convenience functions
 *
 */
#ifndef SERVNSS_H
#define SERVNSS_H

#include "nss.h"
#include "ssl.h"
#include "generated/ServerXMLSchema/Pkcs11.h"
#include "generated/ServerXMLSchema/SslSessionCache.h"
#include "base/session.h"

#define DEFAULT_CERT_NICKNAME "Server-Cert"
#define CERTN_DB "cert8.db"
#define KEYN_DB "key3.db"
#define SECMOD_DB "secmod.db"

PRStatus servssl_init_early(const ServerXMLSchema::Pkcs11& pkcs11, const ServerXMLSchema::SslSessionCache& sslSessionCache);
PRStatus servssl_init_late(PRFileDesc *console, const ServerXMLSchema::Pkcs11& pkcs11);
PRBool servssl_pkcs11_enabled();
PRStatus servssl_check_session(Session *sn);
PRBool servssl_maybe_client_hello(void *buf, int sz);
void PR_CALLBACK servssl_handshake_callback(PRFileDesc *socket, void *arg);

NSAPI_PUBLIC CERTCertificate *servnss_get_cert_from_nickname(const char *nickname, PK11CertListType type=PK11CertListUser, CERTCertList* clist=NULL);

#endif /* SERVNSS_H */
