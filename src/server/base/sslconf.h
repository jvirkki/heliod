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
#ifndef SSLCONF_H
#define SSLCONF_H

#include "base/systems.h" /* Pick up some defines so this stupid NSS headers will work */
#include "sechash.h"
#ifdef XP_WIN32
typedef long int32;	// hack - could not find a definition...
#endif
#include <pk11func.h>
#include <secitem.h>
#include <ssl.h>
#include <cert.h>
#include <certt.h>
#include <nss.h>
#include <secder.h>
#include <key.h>
#include <sslproto.h>
#include <sslerr.h>
#include <secerr.h>
#include <public/nsapi.h> /* for "Session" */

#include "httpdaemon/configuration.h"

// Maximum number of server certs in this object currently supported.
#define MAX_SERVER_CERTS 5

// tmp: see comments in set_cipher()
#define AUTH_RSA   1
#define AUTH_ECDSA 2
#define KEA_ECDHE  3
#define KEA_RSA    4
#define KEA_ECDH   5

#if defined(__cplusplus)

class ListenSocketConfig;

#include <support/NSString.h>

class SSLSocketConfiguration : public ServerXMLSchema::SslWrapper, public ConfigurationObject
{
    public:
        SSLSocketConfiguration(ServerXMLSchema::Ssl& config,
                               ServerXMLSchema::Pkcs11& pkcs11,
                               ConfigurationObject* parent);
        ~SSLSocketConfiguration();

        // enable() turns on all the proper SSL mechanisms and certs
        // on a socket since is used at runtime, it cannot throw
        // exceptions
        PRBool enable(PRFileDesc*& socket) const;
        void enableSSL(PRFileDesc* sock,
                       ServerXMLSchema::Pkcs11& pkcs11) const;
        PRBool operator==(const SSLSocketConfiguration& rhs) const;
        PRBool operator!=(const SSLSocketConfiguration& rhs) const;
        PRBool CheckCertHosts(VirtualServer* vs, ListenSocketConfig* ls) const;
        const char* GetCommonName() const;

    private:
        // assignment operator is undefined
        SSLSocketConfiguration& operator=(const SSLSocketConfiguration&);

        // for caching, so that we don't have to look up the ciphers,
        // cert & key again when SSL is enabled on a socket
        int servercertCount;
        const char * servercertNickname[MAX_SERVER_CERTS];
        CERTCertificate * servercert[MAX_SERVER_CERTS];
        SECKEYPrivateKey * serverkey[MAX_SERVER_CERTS];
        SSLKEAType serverKEAType[MAX_SERVER_CERTS];

        // this fd is used as a model to set SSL parameters on another socket
        PRFileDesc* model;

        // track how many ciphers and certs of each kind
        int rsa_ciphers;
        int ecc_ciphers;
        int export_ciphers;
        int unknown_ciphers;
        int rsa_certs;
        int ecc_certs;

        // track how many ciphers for SSLv2 vs. SSLv3/TLS
        int ssl2_ciphers;
        int ssl3tls_ciphers;

        // track ciphers explicitly enabled/disabled
        GenericVector enabledList;
        GenericVector disabledList;

        // internal functions used for parsing
        void cleanup();
        void validate(ServerXMLSchema::Ssl& config,
                      ServerXMLSchema::Pkcs11& pkcs11);
        void set_cert_and_key(ServerXMLSchema::Ssl& config);
        void set_cipher(PRInt32 cipher, const char *cipherName,
                                              const ServerXMLSchema::Bool& on);
        void set_ciphers();
        PRBool check_bypass() const;
};

#endif

#endif /* SSLCONF_H */
