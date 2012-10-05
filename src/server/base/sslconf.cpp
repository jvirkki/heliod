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
 * sslconf.cpp: Configure SSL on server sockets
 * Including certs, keys, ciphers support
 *
 * This is new in iWS 5.0 to support the XML-based configuration
 *
 * As needed stolen code originally written by Rob McCool
 * then stolen again from Robin Maxwell
 *
 * author : Julien Pierre
 */

#include "nspr.h"
extern "C"
{
#include "ssl.h"
}

#include "base/servnss.h"
#include "base/sslconf.h"

#include "frame/conf.h"
#include "frame/conf_api.h"
#include "libaccess/nsauth.h"
#include "ereport.h"
#include "frame/log.h"
#include "base/shexp.h"
#include "base/dbtbase.h"
#include "certdb.h"
#include <stdarg.h>
#include "util.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/ListenSocketConfig.h"

#include <httpdaemon/configuration.h>
#include <httpdaemon/configurationmanager.h>

#ifdef XP_WIN32
#include "ereport.h"
#include <windows.h>
#endif

/* ----------------------- Multiple cipher support ------------------------ */

#ifndef STRINGIFY
#define STRINGIFY(literal) #literal
#endif

#define SET_SSL2_CIPHER(cipher) \
 set_cipher(SSL_EN_##cipher, "SSL_"STRINGIFY(cipher), ssl2Ciphers.ssl_##cipher)

#define SET_SSL3_CIPHER(cipher) \
 set_cipher(SSL_##cipher, "SSL_"STRINGIFY(cipher), ssl3TlsCiphers.ssl_##cipher)

#define SET_TLS_CIPHER(cipher) \
 set_cipher(TLS_##cipher, "TLS_"STRINGIFY(cipher), ssl3TlsCiphers.tls_##cipher)


SSLSocketConfiguration :: SSLSocketConfiguration(ServerXMLSchema::Ssl& config, 
ServerXMLSchema::Pkcs11& pkcs11, ConfigurationObject* parent)
: ServerXMLSchema::SslWrapper(config),
  ConfigurationObject(parent),
  model(NULL),
  rsa_ciphers(0),
  ecc_ciphers(0),
  export_ciphers(0),
  rsa_certs(0),
  ecc_certs(0),
  unknown_ciphers(0),
  ssl2_ciphers(0),
  ssl3tls_ciphers(0),
  servercertCount(0)
{
    for (int i = 0; i < MAX_SERVER_CERTS; i++) 
    {
        servercertNickname[i] = NULL;
        servercert[i] = NULL;
        serverkey[i] = NULL;
    }

    try
    {
        validate(config, pkcs11);
    }
    catch (...)
    {
        cleanup();
        throw;
    }
}

SSLSocketConfiguration :: ~SSLSocketConfiguration()
{
    cleanup();
}

void SSLSocketConfiguration :: cleanup()
{    
    if (model)
    {
        PR_Close(model);
    }

    for (int i = 0; i < servercertCount; i++) 
    {
        if (servercertNickname[i])
        {
            PERM_FREE((void *)servercertNickname[i]);
        }

        if (servercert[i])
        {
            CERT_DestroyCertificate(servercert[i]);
        }
          
        if (serverkey[i])
        {
            SECKEY_DestroyPrivateKey(serverkey[i]);
        }
    }
}

/* 
 * This function loops through all the server cert nicknames listed in the
 * given config and attempts to locate each one along with associated info
 * (private key). 
 *
 * The cert/key info is stored in servercert[] and related arrays (see
 * sslconf.h). These are currently of a fixed size (MAX_SERVER_CERTS).
 * Upon succesful return, servercertCount will contain the number of
 * certs processed.
 *
 * Temporary note: In NSS 3.11 (WS 7.0) it will only be possible to
 * really use 1 ECC cert (+ 1 RSA cert). When ECC curve negotiation is 
 * introduced in a future release it will be possible to have several ECC
 * certs.  Then when SNI support is introduced it will also be possible to
 * have many RSA certs. At that time the servercert[] list should be made
 * dynamic to account for the possibility of a large number of virtual server
 * certs.
 *
 */
void SSLSocketConfiguration :: set_cert_and_key(ServerXMLSchema::Ssl& config)
{
    // Check how many certs configured. If max is exceeded only install
    // as many as we can. See comments above.

    servercertCount = config.getServerCertNicknameCount();
    ereport(LOG_VERBOSE, "Configuration has %d server certs", servercertCount);
    if (servercertCount > MAX_SERVER_CERTS) 
    {
        ereport(LOG_MISCONFIG, 
                XP_GetAdminStr(DBT_tooManyCerts), MAX_SERVER_CERTS);
        servercertCount = MAX_SERVER_CERTS;
    }

    // If no nicknames listed, the default is to look for one named by
    // the value of DEFAULT_CERT_NICKNAME. Otherwise, retrieve the nicknames
    // as listed in config.

    if (servercertCount == 0) 
    {
        servercertNickname[0] = PERM_STRDUP(DEFAULT_CERT_NICKNAME);
        servercertCount = 1;
        ereport(LOG_VERBOSE,
                "No server cert nicknames listed, using default " 
                DEFAULT_CERT_NICKNAME);
    }
    else 
    {
        for (int i = 0; i < servercertCount; i++) 
        {
            servercertNickname[i] = 
                PERM_STRDUP(getServerCertNickname(i)->getStringValue());
        }
    }

    // Then process each nickname

    for (int i = 0; i < servercertCount; i++) 
    {
        ereport(LOG_VERBOSE, "Processing server cert nicknamed [%s]", 
                servercertNickname[i]);

        servercert[i] = servnss_get_cert_from_nickname(servercertNickname[i]);

        if (NULL == servercert[i])
        {
            NSString error;
            error.setGrowthSize(NSString::MEDIUM_STRING);
            error.printf(XP_GetAdminStr(DBT_certnotfound), 
                         servercertNickname[i]);
            throw EreportableException(LOG_MISCONFIG, error.data());
        }
    
        PK11SlotInfo* slot = NULL;
        if (strchr(servercertNickname[i], ':'))
        {
            char* token = STRDUP(servercertNickname[i]);
            char* colon = strchr(token, ':');
            if (colon)
            {
                *colon = 0;
                slot = PK11_FindSlotByName(token);
                if (!slot)
                {
                    // Slot not found . Should never happen, because 
                    // we already found the cert.
                    NSString error;
                    error.setGrowthSize(NSString::MEDIUM_STRING);
                    error.printf(XP_GetAdminStr(DBT_slotnotfound),
                                 token, servercertNickname[i]);
                    FREE(token);
                    throw EreportableException(LOG_CATASTROPHE, error.data());
                }
            }
            FREE(token);
        }
        else
        {
            slot = PK11_GetInternalKeySlot();
        }

        // Private key for our server cert must be available, find it or fail

        serverkey[i] = PK11_FindPrivateKeyFromCert(slot, servercert[i], NULL);
    
        if (serverkey[i] == NULL)
        { 
            NSString error;
            error.setGrowthSize(NSString::MEDIUM_STRING);
            error.printf(XP_GetAdminStr(DBT_keynotfound), 
                         servercertNickname[i]);
            throw EreportableException(LOG_MISCONFIG, error.data());
        }
    
        // Retrieve the key exchange type of this cert

        serverKEAType[i] = NSS_FindCertKEAType(servercert[i]);


        // Check for certs that are expired or not yet valid and WARN
        // about it, no need to refuse working - the client gets a
        // warning, but can work with the server we could also verify
        // if the certificate is made out for the correct hostname but
        // that would require a reverse DNS lookup for every virtual
        // server - too expensive?

        SECCertTimeValidity certtimestatus;
    
        certtimestatus = 
            CERT_CheckCertValidTimes(servercert[i], PR_Now(), PR_FALSE);

        switch (certtimestatus)
        {
        case secCertTimeValid:
            // ok
            break;
        case secCertTimeExpired:
            ereport(LOG_WARN, XP_GetAdminStr(DBT_servNSSExpiredCert), 
                    servercertNickname[i]);
            break;
        case secCertTimeNotValidYet:
            ereport(LOG_WARN, XP_GetAdminStr(DBT_servNSSCertNotValidYet), 
                    servercertNickname[i]);
            break;
        }

        // Show some status

        switch (serverKEAType[i])
        {
        case ssl_kea_rsa:
            ereport(LOG_VERBOSE, "Loaded server cert nicknamed [%s] (RSA)",
                    servercertNickname[i]);
            rsa_certs++;
            break;
        case ssl_kea_ecdh:
            ereport(LOG_VERBOSE, "Loaded server cert nicknamed [%s] (ECDH)",
                    servercertNickname[i]);
            ecc_certs++;
            break;
        case ssl_kea_dh:
            ereport(LOG_VERBOSE, "Loaded server cert nicknamed [%s] (DH)",
                    servercertNickname[i]);
            break;
        default:
            ereport(LOG_WARN, 
                    XP_GetAdminStr(DBT_unknownKEAType), serverKEAType[i]);
        }
    }
}


/*
 * This function checks all the certs previously loaded (in
 * set_cert_and_key()) against the given VirtualServer and/or
 * ListenSocketConfig. If there are any mismatches, warning messages
 * are printed and PR_FALSE is returned, otherwise returns PR_TRUE.
 *
 */
PRBool SSLSocketConfiguration :: CheckCertHosts(VirtualServer* vs, ListenSocketConfig* ls) const
{
    PRBool status = PR_TRUE;

    for (int cert = 0; cert < servercertCount; cert++) 
    {
        if (vs)
        {
            // check that the cert matches all urlhosts in the VS urlhosts list
            for (int i = 0; i < vs->getHostCount(); i++) {
                const char* hn = *vs->getHost(i);
                if (shexp_valid(hn) == NON_SXP) 
                {
                    SECStatus matches = 
                        CERT_VerifyCertName(servercert[cert], hn);
                    if (SECSuccess != matches)
                    {
                        char *cn = 
                            CERT_GetCommonName(&(servercert[cert]->subject));
                        ereport(LOG_WARN, 
                              XP_GetAdminStr(DBT_servNSSVirtualNoUrlhostSubjectMatch),
                              vs->name.getStringValue(), hn, 
                              cn,
                              servercertNickname[cert]);
                        if (cn)
                            PORT_Free(cn);
                        status = PR_FALSE;
                    }

                }
            }
        }

        if (ls)
        {
            // check that the cert matches the servername of the LS it
            // belongs to
            const char* hn = ls->getExternalServerName().getHostname();
            if (hn && strlen(hn))
            {
                SECStatus matches = CERT_VerifyCertName(servercert[cert], hn);
                if (SECSuccess != matches)
                {
                    char *cn = 
                        CERT_GetCommonName(&(servercert[cert]->subject));
                    ereport(LOG_WARN, 
                            XP_GetAdminStr(DBT_servNSSGroupNoServernameSubjectMatch),
                            ls->name.getStringValue(), hn, 
                            cn,
                            servercertNickname[cert]);
                    if (cn)
                        PORT_Free(cn);
                    status = PR_FALSE;
                }
            }
        }
    }

    return status;
}


/*
 * If the given cipher is valid and enabled in config, add it to enabledList;
 * if the cipher is disabled in config, add it to disabledList.
 * 
 * See enableSSL() where the contents of these lists are fed to NSS.
 *
 */
void SSLSocketConfiguration :: set_cipher(PRInt32 cipher, 
                                          const char *cipherName, 
                                          const ServerXMLSchema::Bool& on)
{
    SSLCipherSuiteInfo info;
    SECStatus status;
    char line[200];

    status = SSL_GetCipherSuiteInfo(cipher, &info, sizeof(info));

    if (status != SECSuccess) {
        if (on) {
            ereport(LOG_WARN, XP_GetAdminStr(DBT_badCiphersuite), 
                    cipherName, system_errmsg());
        } else {
            // If the cipher is not supported and we don't want it, no harm
            // done, but log a verbose warning to help weed them out over time
            ereport(LOG_VERBOSE, 
                    "Disabled cipher suite %s is not supported", cipherName);
        }
        
        return;
    }

    // Figure out whether this cipher needs a RSA or ECC-based server
    // cert. Not good to hardcode this logic here. NSS mozilla bug
    // 134122 proposes a validator function which could take care of
    // this and other config sanity testing. Once that gets added,
    // this and other sanity testing code can all go away, replaced by
    // a validator call at the end of configuration. Until then, slog
    // through by hand.

    int auth = 0;
    int kea = 0;
    const char * certType = NULL;

    if (!strcmp(info.authAlgorithmName, "RSA"))   { auth = AUTH_RSA; }
    if (!strcmp(info.authAlgorithmName, "ECDSA")) { auth = AUTH_ECDSA; }
    if (!strcmp(info.keaTypeName, "RSA"))   {kea = KEA_RSA; }
    if (!strcmp(info.keaTypeName, "ECDHE")) {kea = KEA_ECDHE; }
    if (!strcmp(info.keaTypeName, "ECDH"))  {kea = KEA_ECDH; }

    if (auth == AUTH_RSA   && kea == KEA_ECDHE) { certType = "RSA"; }
    if (auth == AUTH_RSA   && kea == KEA_RSA)   { certType = "RSA"; }
    if (auth == AUTH_ECDSA && kea == KEA_ECDHE) { certType = "ECC"; }
    if (auth == AUTH_ECDSA && kea == KEA_ECDH)  { certType = "ECC"; }
    if (auth == AUTH_RSA   && kea == KEA_ECDH)  { certType = "ECC"; }

    if (!certType) {
        // ugly: getting an unknown type will be bad. this should never happen
        // unless new ciphers are added which don't match the rules above.
        // since we don't know which key type it needs, don't prevent
        // server from starting because it just might be a valid
        // config. this nonsense can go away once NSS has a built-in
        // validator
        ereport(LOG_WARN, 
                XP_GetAdminStr(DBT_unknownCertType), info.cipherSuiteName);
        certType = "???";
        unknown_ciphers++;
    }
    
    // Format some verbose output
    sprintf(line,"cipher (cert: %3s, auth: %5s, kea: %5s, enc: %5s, mac: %5s, "
            "key bits: %4d): %s", certType,
            info.authAlgorithmName, info.keaTypeName, info.symCipherName,
            info.macAlgorithmName, info.effectiveKeyBits, 
            info.cipherSuiteName);

    // Add it to the correct list, enabled or disabled
    if (on) {
        enabledList.append((void *)cipher);
        ereport(LOG_VERBOSE, "enabling  %s", line);
        if (SSL_IS_SSL2_CIPHER(cipher)) {
            ssl2_ciphers++; 
        } else { 
            ssl3tls_ciphers++; 
        }

    } else {
        disabledList.append((void *)cipher);
        ereport(LOG_VERBOSE, "disabling %s", line);

        return;
    }

    // Below here only matters if the cipher was enabled

    // Warn about enabling really weak ciphers
    if (info.effectiveKeyBits <= 56) {
        ereport(LOG_WARN, 
                XP_GetAdminStr(DBT_suchWeakCipher), info.cipherSuiteName);
    }

    // Track how many RSA/ECC suites we have enabled
    if (!strcmp(certType, "RSA")) { rsa_ciphers++; }
    else if (!strcmp(certType, "ECC")) { ecc_ciphers++; }

    // Track whether any export ciphers are enabled
    if (info.isExportable) {
        export_ciphers++;
    }

}


void SSLSocketConfiguration :: set_ciphers()
{
    char err[MAGNUS_ERROR_LEN];
    int x;

    if ( (clientAuth.CLIENTAUTH_FALSE != clientAuth) && 
         (PR_FALSE == ssl3) &&
         (PR_FALSE == tls) )
    {
        throw ConfigurationServerXMLException(*this, 
                              XP_GetAdminStr(DBT_servNSSClientAuthMisconfig));
    }

    /* To inform the user, call getglobals again to make sure at least
       one version is active */
    if ( (PR_FALSE == ssl2) && (PR_FALSE == ssl3) && (PR_FALSE == tls) )
    {
        throw ConfigurationServerXMLException(*this, 
                                        XP_GetAdminStr(DBT_servNSSNoCiphers));
    }

    // Call set_ssl2_cipher() for all known SSL2 ciphers
    if (ssl2)
    {
        SET_SSL2_CIPHER(RC4_128_WITH_MD5);
        SET_SSL2_CIPHER(RC4_128_EXPORT40_WITH_MD5);
        SET_SSL2_CIPHER(RC2_128_CBC_WITH_MD5);
        SET_SSL2_CIPHER(RC2_128_CBC_EXPORT40_WITH_MD5);
        SET_SSL2_CIPHER(DES_64_CBC_WITH_MD5);
        SET_SSL2_CIPHER(DES_192_EDE3_CBC_WITH_MD5);

        if (!ssl2_ciphers) {
            throw ConfigurationServerXMLException(*this, 
                     XP_GetAdminStr(DBT_servNSSSSLv2OnWithoutCiphers));
        }

        ereport(LOG_VERBOSE,
                "SSLv2 is enabled and %d SSLv2 ciphers are enabled", 
                ssl2_ciphers);
    }

    // Call set_ssl3_tls_cipher() for all known SSL3/TLS ciphers
    if (ssl3 || tls)
    {
        SET_SSL3_CIPHER(RSA_WITH_RC4_128_MD5);
        SET_SSL3_CIPHER(RSA_WITH_RC4_128_SHA);
        SET_SSL3_CIPHER(RSA_WITH_3DES_EDE_CBC_SHA);
        SET_SSL3_CIPHER(RSA_WITH_DES_CBC_SHA);
        SET_SSL3_CIPHER(RSA_EXPORT_WITH_RC4_40_MD5);
        SET_SSL3_CIPHER(RSA_EXPORT_WITH_RC2_CBC_40_MD5);
        SET_SSL3_CIPHER(RSA_WITH_NULL_MD5);
        SET_SSL3_CIPHER(RSA_WITH_NULL_SHA);
        SET_SSL3_CIPHER(RSA_FIPS_WITH_3DES_EDE_CBC_SHA);
        SET_SSL3_CIPHER(RSA_FIPS_WITH_DES_CBC_SHA);

        SET_TLS_CIPHER(RSA_EXPORT1024_WITH_DES_CBC_SHA);
        SET_TLS_CIPHER(RSA_EXPORT1024_WITH_RC4_56_SHA);
        SET_TLS_CIPHER(ECDHE_RSA_WITH_AES_128_CBC_SHA);
        SET_TLS_CIPHER(ECDH_RSA_WITH_AES_128_CBC_SHA);
        SET_TLS_CIPHER(ECDH_RSA_WITH_RC4_128_SHA);
        SET_TLS_CIPHER(ECDH_RSA_WITH_3DES_EDE_CBC_SHA);
        SET_TLS_CIPHER(ECDH_RSA_WITH_AES_256_CBC_SHA);
        SET_TLS_CIPHER(ECDH_ECDSA_WITH_AES_128_CBC_SHA);
        SET_TLS_CIPHER(ECDH_ECDSA_WITH_RC4_128_SHA);
        SET_TLS_CIPHER(ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA);
        SET_TLS_CIPHER(ECDH_ECDSA_WITH_AES_256_CBC_SHA);
        SET_TLS_CIPHER(ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
        SET_TLS_CIPHER(ECDHE_ECDSA_WITH_NULL_SHA);
        SET_TLS_CIPHER(ECDHE_ECDSA_WITH_RC4_128_SHA);
        SET_TLS_CIPHER(ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA);
        SET_TLS_CIPHER(ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
        SET_TLS_CIPHER(ECDHE_RSA_WITH_NULL_SHA);
        SET_TLS_CIPHER(ECDHE_RSA_WITH_RC4_128_SHA);
        SET_TLS_CIPHER(ECDHE_RSA_WITH_3DES_EDE_CBC_SHA);
        SET_TLS_CIPHER(ECDHE_RSA_WITH_AES_256_CBC_SHA);
        SET_TLS_CIPHER(RSA_WITH_AES_128_CBC_SHA);
        SET_TLS_CIPHER(RSA_WITH_AES_256_CBC_SHA);

        if (ssl3 && !ssl3tls_ciphers) {
            throw ConfigurationServerXMLException(*this,
                     XP_GetAdminStr(DBT_servNSSSSLv3OnWithoutCiphers));
        }

        if (tls && !ssl3tls_ciphers) {
            throw ConfigurationServerXMLException(*this, 
                     XP_GetAdminStr(DBT_servNSSTLSOnWithoutCiphers));
        }

        ereport(LOG_VERBOSE,
                "SSLv3/TLS is enabled and %d SSLv3/TLS ciphers are enabled", 
                ssl3tls_ciphers);
    }
}


/*
 * Main entry point for processing the SSL configuration. 
 * Called from constructor.
 *
 */
void SSLSocketConfiguration :: validate(ServerXMLSchema::Ssl& config, 
                                        ServerXMLSchema::Pkcs11& pkcs11)
{
    // first, check if the global security is enabled
    if (!servssl_pkcs11_enabled())
    {
        throw ConfigurationServerXMLException(*this, XP_GetAdminStr(DBT_securityoff));
    }

    // retrieve the server certificate & key and cache them
    set_cert_and_key(config);
    
    // check that the cipher specified are valid and cache the list
    set_ciphers();

    // ciphers are not set globally, only per socket
    // we may need to do it globally some day though, for LDAP SSL cipherset

    model = PR_NewTCPSocket();

    if (model)
    {
        model = SSL_ImportFD(NULL, model);
    }

    if (model)
    {
        enableSSL(model, pkcs11);
    }
}

void SSLSocketConfiguration :: enableSSL(PRFileDesc* sock, 
                                         ServerXMLSchema::Pkcs11& pkcs11) const
{
    conf_global_vars_s *globals = conf_get_true_globals();

    // Set up SSL on a socket
    //
    // this is always  done on an accepted socket that's already got the
    // SSL layer pushed
    //
    // (note that we cannot use SSL_EnableDefault in InitNSS because
    //  we might (and will) use different settings in ldapsdk for LDAPSSL.
    //  Particularly, ldapsdk sets SSL_HANDSHAKE_AS_CLIENT, which will
    //  fail if there is a default of SSL_HANDSHAKE_AS_SERVER.)

    SECStatus stat = SSL_OptionSet(sock, SSL_SECURITY, PR_TRUE);

    if (stat == SECSuccess)
        stat = SSL_OptionSet(sock, SSL_HANDSHAKE_AS_CLIENT, PR_FALSE);

    if (stat == SECSuccess)
        stat = SSL_OptionSet(sock, SSL_HANDSHAKE_AS_SERVER, PR_TRUE);

    if (stat == SECSuccess)
        stat = SSL_OptionSet(sock, SSL_REQUIRE_CERTIFICATE, PR_FALSE);

    if (clientAuth.CLIENTAUTH_FALSE != clientAuth && (stat == SECSuccess) )
        stat = SSL_OptionSet(sock, SSL_REQUEST_CERTIFICATE, PR_TRUE);
    else
        stat = SSL_OptionSet(sock, SSL_REQUEST_CERTIFICATE, PR_FALSE);

    if ( (stat == SECSuccess) && (PR_TRUE == ssl2) )
        stat = SSL_OptionSet(sock, SSL_ENABLE_SSL2, PR_TRUE);
    else
        stat = SSL_OptionSet(sock, SSL_ENABLE_SSL2, PR_FALSE);

    if ( (stat == SECSuccess) && (PR_TRUE == ssl3) )
        stat = SSL_OptionSet(sock, SSL_ENABLE_SSL3, PR_TRUE);
    else
        stat = SSL_OptionSet(sock, SSL_ENABLE_SSL3, PR_FALSE);

    if ( (stat == SECSuccess) && (PR_TRUE == tls) )
        stat = SSL_OptionSet(sock, SSL_ENABLE_TLS, PR_TRUE);
    else
        stat = SSL_OptionSet(sock, SSL_ENABLE_TLS, PR_FALSE);

    if ( (stat == SECSuccess) && (PR_TRUE == tlsRollbackDetection) )
        stat = SSL_OptionSet(sock, SSL_ROLLBACK_DETECTION, PR_TRUE);
    else
        stat = SSL_OptionSet(sock, SSL_ROLLBACK_DETECTION, PR_FALSE);

    ereport(LOG_VERBOSE, "%d export ciphers enabled", export_ciphers);
    if (export_ciphers == 0) {
        // By default an RSA step-down key is generated for every SSL
        // socket but this is only needed if export ciphers are
        // enabled, which is very rarely going to be true. Generating
        // this step-down key is very expensive, so let's not do it
        // unless truly needed. Setting this flag will make the
        // SSL_ConfigSecureServer() call below about 10x faster.
        stat = SSL_OptionSet(sock, SSL_NO_STEP_DOWN, PR_TRUE);
    }

    // In servssl_init_early() (or servssl_init_late()) the SSL cache
    // was initialized if Vssl_cache_entries > 0. But if cache was
    // disabled (entries == 0) then no cache initialization was
    // done. In that case, we must set SSL_NO_CACHE option here, or
    // NSS will be unhappy about the uninitialized cache.

    if ( (stat == SECSuccess) && (globals->Vssl_cache_entries == 0) ) {
        ereport(LOG_VERBOSE, "SSL/TLS session cache is disabled");
        stat = SSL_OptionSet(sock, SSL_NO_CACHE, PR_TRUE);
    }

    // If the configuration wants bypass AND the keypairs live in
    // tokens which can allow bypass, do it, otherwise don't.

    if (pkcs11.allowBypass) {
        if (check_bypass() == PR_TRUE) {
            ereport(LOG_VERBOSE, "PKCS#11 bypass is enabled");
            stat = SSL_OptionSet(sock, SSL_BYPASS_PKCS11, PR_TRUE);
        } else {
            ereport(LOG_SECURITY, XP_GetAdminStr(DBT_cannotBypass));
            stat = SSL_OptionSet(sock, SSL_BYPASS_PKCS11, PR_FALSE);
        }
    } else {
        stat = SSL_OptionSet(sock, SSL_BYPASS_PKCS11, PR_FALSE);
        ereport(LOG_VERBOSE, "PKCS#11 bypass is disabled");
    }

    int i;

    // jpierre : disable all NSS supported cipher suites
    // this is to prevent any new NSS cipher suites from getting automatically
    // and unintentionally enabled as a result of the NSS_SetDomesticPolicy()
    // call . This way, only the ciphers explicitly specified in the server
    // configuration can ever be enabled
    //
    // for (i = 0; i < SSL_NumImplementedCiphers; i++)
    //     SSL_CipherPrefSet(sock, SSL_ImplementedCiphers[i], PR_FALSE);
    //
    // XXX elving: No, if a customer is using an NSS version other than the one
    // we shipped, they're very likely doing so in order to use new cipher
    // suites.  Don't explicitly disable cipher suites we don't know about.
    // This change also cuts down the number of libssl calls we make.

    for (i = 0; i < disabledList.length(); i++) {
        SSL_CipherPrefSet(sock, (PRInt32)(size_t) disabledList[i], PR_FALSE);
    }

    for (i = 0; i < enabledList.length(); i++) {
        if (SSL_CipherPrefSet(sock, (PRInt32)(size_t) enabledList[i], 
                              PR_TRUE) != SECSuccess) 
            stat = SECFailure;
    }


    // This sets the arg to the password function which can be used for context info.
    if (SSL_SetPKCS11PinArg(sock, NULL) != SECSuccess)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_servNSSPKCS11PinError), system_errmsg());
        stat = SECFailure;
    }

    // Check that the certs we have make sense given the ciphers enabled
    ereport(LOG_VERBOSE, 
            "%d RSA certificate(s) present, %d suitable cipher(s) enabled",
            rsa_certs, rsa_ciphers);
    ereport(LOG_VERBOSE, 
            "%d ECC certificate(s) present, %d suitable cipher(s) enabled",
            ecc_certs, ecc_ciphers);

    if (rsa_certs == 0 && ecc_certs > 0 && 
        ecc_ciphers == 0 && unknown_ciphers == 0) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_wantECCnoECC));
        stat = SECFailure;
    }
    if (ecc_certs == 0 && rsa_certs > 0 && 
        rsa_ciphers == 0 && unknown_ciphers == 0) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_wantRSAnoRSA));
        stat = SECFailure;
    }


    // Install all the server certs previously loaded
    for (int cert = 0; cert < servercertCount; cert++) 
    {
        if (SSL_ConfigSecureServer(sock, servercert[cert], serverkey[cert], 
                                   serverKEAType[cert]) != SECSuccess)
        {
            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_servNSSErrorSecureServerConfig), 
                    system_errmsg());
            stat = SECFailure;
        } 
        else 
        {
            ereport(LOG_VERBOSE, 
                    "Installed server cert [%s]", servercertNickname[cert]);
        }
    }

    if (SSL_HandshakeCallback(sock, servssl_handshake_callback, NULL) != SECSuccess)
    {
        ereport(LOG_FAILURE, 
                XP_GetAdminStr(DBT_servNSSHandshakeCallbackConfigureError),
                system_errmsg());
        stat = SECFailure;
    }

    if (stat != SECSuccess)
    {
        throw EreportableException(LOG_FAILURE, XP_GetAdminStr(DBT_badsslparams));
    }
}

PRBool SSLSocketConfiguration :: enable(PRFileDesc*& socket) const
{
    socket = SSL_ImportFD(model, socket);
    if (socket)
    {
        SSL_ResetHandshake(socket, PR_TRUE);
        return PR_TRUE;
    }
    else
        return PR_FALSE;
}


/*
 * Given the current server certificates, check if we can use PKCS11 bypass.
 * Returns PR_TRUE if so, otherwise PR_FALSE.
 *
 */
PRBool SSLSocketConfiguration :: check_bypass() const
{
    PRUint32 protocolmask = 0;
    PRBool canbypass = PR_TRUE;
    void * pwArg = NULL;

    PRUint16 * enabledCiphers;
    int numCiphers;

    // prepare array of enabled ciphers

    numCiphers = enabledList.length();
    enabledCiphers = (PRUint16 *)calloc(numCiphers, sizeof(PRUint16));

    for (int i = 0; i < numCiphers; i++) {
        enabledCiphers[i] = (PRUint16)(size_t)enabledList[i];
    }

    // check every server key/cert for bypass compatibility

    if (ssl3) { protocolmask = SSL_CBP_SSL3; }
    if (tls) { protocolmask |= SSL_CBP_TLS1_0; }

    for (int n = 0; n < servercertCount; n++) {
        SSL_CanBypass(servercert[n], serverkey[n], protocolmask,
                      enabledCiphers, numCiphers,
                      &canbypass, pwArg);

        if (canbypass == PR_FALSE) {
            ereport(LOG_SECURITY, 
                    XP_GetAdminStr(DBT_nickCantBypass), servercertNickname[n]);
            free(enabledCiphers);
            return PR_FALSE;
        }
    }

    free(enabledCiphers);
    return PR_TRUE;
}

