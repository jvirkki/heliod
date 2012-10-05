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


#include "engine.h"
#include "log.h"
#include <nspr.h>
#include "sslproto.h"
#include "utils.h"

extern "C"
{
#include "ssl.h"
#include "nss.h"
#include "pk11func.h"
#include "cert.h"
#include "certt.h"
};

// globals ... Yuck
char* certName = NULL;
char* password = "httptest";
PRBool globalCiphers = PR_FALSE;
PRBool globalCert = PR_FALSE;
int ciphers[32];
int cipherCount = 0;

// see tests.cpp for cipher lists
extern int ssl2CipherSuites[];
extern int ssl3CipherSuites[];


PRIntervalTime Engine::globaltimeout = PR_TicksPerSecond()*30;

static struct {
    PRLock *lock;
    PLHashTable *ht;
} namedMutexes = { PR_NewLock(), PL_NewHashTable(0, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL) };

static void acquireNamedMutex(const char *name)
{
    if (name) {
        PRLock *lock;

        PR_Lock(namedMutexes.lock);
        lock = (PRLock *) PL_HashTableLookup(namedMutexes.ht, name);
        if (!lock) {
            lock = PR_NewLock();
            PL_HashTableAdd(namedMutexes.ht, name, (void *) lock);
        }
        PR_Unlock(namedMutexes.lock);

        PR_Lock(lock);
    }
}

static void releaseNamedMutex(const char *name)
{
    if (name) {
        PRLock *lock;

        PR_Lock(namedMutexes.lock);
        lock = (PRLock *) PL_HashTableLookup(namedMutexes.ht, name);
        PR_Unlock(namedMutexes.lock);

        PR_Unlock(lock);
    }
}

static char * ownPasswd( PK11SlotInfo *slot, PRBool retry, void *arg)
{
        if (!retry)
            return PL_strdup(password);
        else
            return NULL;
}

/*
 * Callback is called when incoming certificate is not valid.
 * Returns SECSuccess to accept the cert anyway, SECFailure to reject.
 */
static SECStatus 
ownBadCertHandler(void * arg, PRFileDesc * socket)
{
    PRErrorCode err = PR_GetError();
    /* can log invalid cert here */
    printf("Bad server certificate: %d, %s\n", err, nscperror_lookup(err));
    return SECSuccess;        /* override, say it's OK. */
}

PRBool __EXPORT InitSecurity(char* certDir, char* certname, char* certpassword)
{
    if (certpassword)
        password = strdup(certpassword);

    if (certname)
    {
        certName = strdup(certname);
	    globalCert = PR_TRUE;
    }

    //SECStatus stat = NSS_Init("/u/pkhincha/.netscape");
    SECStatus stat = NSS_Init(certDir);
    PK11_SetPasswordFunc(ownPasswd);

    if (SECSuccess != stat)
                return PR_FAILURE;

        CERTCertDBHandle* handle = CERT_GetDefaultCertDB();

        stat = NSS_SetDomesticPolicy();

        return PR_TRUE;
};


void disableAllCiphersOnSocket(PRFileDesc* sock)
{
    int i;
    int numsuites = SSL_NumImplementedCiphers;

    /* disable all the cipher suites for that socket */
    for (i = 0; i<numsuites; i++)
    {
        SSL_CipherPrefSet(sock, SSL_ImplementedCiphers[i], SSL_NOT_ALLOWED);
    };
};

PRBool __EXPORT EnableCipher(const char* cipherString)
{
     int ndx;
  
     if (!cipherString)
        return PR_FALSE;


     while (0 != (ndx = *cipherString++))
     {
        int* cptr;
        int cipher;

        if (! isalpha(ndx))
           continue;
        cptr = islower(ndx) ? ssl3CipherSuites : ssl2CipherSuites;
        for (ndx &= 0x1f; (cipher = *cptr++) != 0 && --ndx > 0; )
           /* do nothing */;
        ciphers[cipherCount++] = cipher;
     }

     globalCiphers = PR_TRUE;
     return PR_TRUE;
};

static SECStatus certcallback (
   void *arg,
   PRFileDesc *fd,
   PRBool checksig,
   PRBool isServer)
{
    return SECSuccess; // always succeed
};

/* Function: SECStatus ownGetClientAuthData()
 *
 * Purpose: This callback is used by SSL to pull client certificate 
 * information upon server request.
 */

static SECStatus ownGetClientAuthData(void *arg, PRFileDesc *socket,
				    CERTDistNames *caNames,
				    CERTCertificate **pRetCert,/*return */
				    SECKEYPrivateKey **pRetKey)
{
    CERTCertificate *               cert;
    SECKEYPrivateKey *              privKey;
    void *                          proto_win = NULL;
    SECStatus                       rv = SECFailure;
    char *			    localNickName = (char *)arg;

    proto_win = SSL_RevealPinArg(socket);
   
    if (localNickName) 
    {
        cert = PK11_FindCertFromNickname(localNickName, proto_win);
        if (cert)
        {
            privKey = PK11_FindKeyByAnyCert(cert, proto_win);
            if (privKey)
            {
                    rv = SECSuccess;
            } else
            {
                    CERT_DestroyCertificate(cert);
            };
        }

        if (rv == SECSuccess)
        {
                *pRetCert = cert;
                *pRetKey  = privKey;
        };

        //free(localNickName);
        return rv;
    };

    char* chosenNickName = certName ? (char *)strdup(certName) : NULL;
    if (chosenNickName)
    {
        cert = PK11_FindCertFromNickname(chosenNickName, proto_win);
        if (cert)
        {
            privKey = PK11_FindKeyByAnyCert(cert, proto_win);
            if (privKey)
            {
                    rv = SECSuccess;
            } else
            {
                    CERT_DestroyCertificate(cert);
            };
        }
    }
    else
    {
        /* no nickname given, automatically find the right cert */
        CERTCertNicknames *     names;
        int                     i;

        names = CERT_GetCertNicknames(  CERT_GetDefaultCertDB(), 
                                        SEC_CERT_NICKNAMES_USER,
                                        proto_win);

        if (names != NULL)
        {
            for( i=0; i < names->numnicknames; i++ )
            {
                cert = PK11_FindCertFromNickname(names->nicknames[i], proto_win);
                if (!cert)
                {
                    continue;
                };

                /* Only check unexpired certs */
                if (CERT_CheckCertValidTimes(cert, PR_Now(), PR_FALSE) != secCertTimeValid)
                {
                    CERT_DestroyCertificate(cert);
                    continue;
                };

                rv = NSS_CmpCertChainWCANames(cert, caNames);

                if (rv == SECSuccess)
                {
                    privKey = PK11_FindKeyByAnyCert(cert, proto_win);
                    if (privKey)
                    {
                        // got the key
                        break;
                    };

                    // cert database password was probably wrong
                    rv = SECFailure;
                    break;
                };
            }; /* for loop */
            CERT_FreeNicknames(names);
        }; // names
    }; // no nickname chosen

    if (rv == SECSuccess)
    {
            *pRetCert = cert;
            *pRetKey  = privKey;
    };

    if (chosenNickName)
        free(chosenNickName);

    return rv;
};

void nodelay(PRFileDesc* fd)
{
    PRSocketOptionData opt;
    PRStatus rv;

    opt.option = PR_SockOpt_NoDelay;
    opt.value.no_delay = PR_FALSE;

    rv = PR_GetSocketOption(fd, &opt);
    if (rv == PR_FAILURE)
    {
        return;
    };

    opt.option = PR_SockOpt_NoDelay;
    opt.value.no_delay = PR_TRUE;
    rv = PR_SetSocketOption(fd, &opt);
    if (rv == PR_FAILURE)
    {
        return;
    };

    return;
};

// _doConnect is a private member that returns a file descriptor for I/O if the HTTP connection is successful
PRFileDesc * Engine::_doConnect(const PRNetAddr *addr, PRBool SSLOn, const PRInt32* cipherSuite, 
                                PRInt32 count, const char *nickName, PRBool handshake, const SecurityProtocols& secprots,
                                PRIntervalTime timeout,
                                const char* testName)
{
    PRFileDesc *sock;
    SECStatus status;

    sock = PR_OpenTCPSocket(addr->raw.family);

    if (!sock)
    {
        return NULL;
    };

    nodelay(sock);

    if (PR_TRUE == SSLOn)
    {
        sock=SSL_ImportFD(NULL, sock);
        int error = 0;
        PRBool rv = SSL_OptionSet(sock, SSL_SECURITY, 1);
        if (SECSuccess != rv )
            error = PORT_GetError();
        rv = SSL_OptionSet(sock, SSL_HANDSHAKE_AS_CLIENT, 1);
        if (SECSuccess != rv )
            error = PORT_GetError();

        rv =SSL_OptionSet(sock, SSL_ENABLE_SSL2, secprots.ssl2);
        rv =SSL_OptionSet(sock, SSL_ENABLE_SSL3, secprots.ssl3);
        rv =SSL_OptionSet(sock, SSL_ENABLE_TLS, secprots.tls);

        if (PR_TRUE == globalCert)
        {
            int st = SSL_GetClientAuthDataHook(
                sock,
                ownGetClientAuthData,
                NULL);
        }
        else
            if (nickName)
            {
                int st = SSL_GetClientAuthDataHook(
                            sock,
                            ownGetClientAuthData,
                            (void*)nickName);
            };

        if ((testName == NULL) || 
            (SSL_SetSockPeerID(sock, (char *)testName) != SECSuccess)) {
            printf("error: Unable to set SSL peer ID for SSL test\n");
        }

        SSL_AuthCertificateHook(sock, certcallback, CERT_GetDefaultCertDB());

        if (globalCiphers == PR_FALSE)
        {
            if (count > 0)
            {
                // set the ciphers specified via the %ciphers option in the dat file
                disableAllCiphersOnSocket(sock);

                for (int i=0; i < count; i++)
                {
                    status = SSL_CipherPrefSet(sock, cipherSuite[i], SSL_ALLOWED);
                    if (status != SECSuccess)
                        error = PORT_GetError();
                };
            }
            else
            {
                // don't change anything - all ciphers are enabled by default
            };
        }
        else
        {
   	        /* Set global ciphers on the sock based on command-line options */
            disableAllCiphersOnSocket(sock);

            for (int i=0; i < cipherCount; i++)
	            SSL_CipherPrefSet(sock, ciphers[i], SSL_ALLOWED);
        };
    };
    
    if ( PR_Connect(sock, addr, timeout) == PR_FAILURE)
    {
#ifdef DEBUG
        printerror("PR_Connect");
#endif
        PR_Close(sock);
	sock = NULL;
        return NULL;
    };

    return sock;
};

SunEngine :: SunEngine()
{
    performance = PR_FALSE;
};

void SunEngine :: setPerformance(PRBool perf)
{
    performance = perf;
};

// low-level request engine
SunResponse * SunEngine::makeRequest(Request &request, const HttpServer& server, PRBool ignoresenderror)
{
    PRFileDesc *sock;
    SunResponse *rv = NULL;

    acquireNamedMutex(request.mutexName);

    const PRNetAddr *addr = server.getAddr();
    sock = _doConnect(addr, request.isSSL(), request.cipherSet, request.cipherCount, request.nickName, request.handshake, request.secprots, request.getTimeout(), request.testName);

    if (sock)
    {
        PRBool status = request.send(sock);
        if (status || ignoresenderror)
        {
            rv = new SunResponse(sock, &request);
            if (PR_TRUE == performance)
                rv->setPerformance(PR_TRUE);
            rv->processResponse();
        }
        PR_Close(sock);
        sock = NULL;
    };

    releaseNamedMutex(request.mutexName);

    return rv;
};

// high-level request engine
HttpResponse * HttpEngine::makeRequest(HttpRequest &request, const HttpServer& server, PRBool ignoresenderror)
{
    PRFileDesc *sock = NULL;
    HttpResponse *rv = NULL;

    acquireNamedMutex(request.mutexName);

    const PRNetAddr *addr = server.getAddr();
    sock = _doConnect(addr, request.isSSL(), 0, 0, NULL);

    if (sock)
    {
        PRBool status = request.send(sock);
        if (status || ignoresenderror)
        {
            rv = new HttpResponse(sock, &request);
            rv->processResponse();
        }
        PR_Close(sock);
        sock = NULL;
    };

    releaseNamedMutex(request.mutexName);

    return rv;
};

