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
 * The provides the functionality of reading NSS database and return
 * the results in PKCS12 for keys and PKCS7 format for certificates.
 */
#include "Pkcs12Util.h"
#include "nss.h"
#include "certdb.h"
#include "secmodt.h"
#include "secmod.h"
#include"jni.h"
#include "com_iplanet_ias_security_NssStore.h"
#ifdef XP_WIN32
#include <windows.h>
#include <process.h>
#else
#include <dlfcn.h>
#endif
#include "base/ereport.h"
#include "base/servnss.h"

void JNU_Throw(JNIEnv *env, const char *msg) {
    jclass cls = env->FindClass("java/lang/Exception");
    if (cls != NULL) {
        env->ThrowNew(cls, msg);
    }
    env->DeleteLocalRef(cls);
}

/*
 * Get the absolute path of the PKCS11 library
 */
jstring
getLibAbsolutePath(JNIEnv *env, const char* lib) {
    jstring rval = NULL;

    PRLibrary *libHandle = PR_LoadLibrary(lib);
    if (libHandle == NULL)
        return NULL;

    PRFuncPtr ptr = PR_FindFunctionSymbol(libHandle, "C_GetFunctionList");
    char *pathFull = PR_GetLibraryFilePathname(lib, ptr);
    if (pathFull) {
        rval = env->NewStringUTF(pathFull);
        ereport(LOG_VERBOSE, (char*)"Lib [%s] was loaded from [%s]", 
                lib, pathFull);
        PR_Free(pathFull);
    }

#if !defined(HPUX)
    PR_UnloadLibrary(libHandle);
#endif

    return rval;
}

// ----- JNI calls --------------

JNIEXPORT jobject JNICALL Java_com_iplanet_ias_security_NssStore_getTokenInfoListNative(
    JNIEnv *env, jobject jobj, jstring jsoftoknlib) {

    SECMODListLock *lock = NULL;
    SECMODModuleList *list;
    SECMODModule *module;
    PK11SlotInfo *slot;

    jstring jtokenName;
    jstring jlibname;

    jclass jarrayListClass;
    jobject jarrayListObj;
    jmethodID jconstructor;
    jmethodID jaddMethod;

    jobject jnssTokenInfoObj;
    jclass jnssTokenInfoClass;
    jmethodID jnssTokenInfoConstructor;


    jarrayListClass = env->FindClass("java/util/ArrayList");
    if (jarrayListClass == NULL) {
        errMsg = "Cannot load java.util.ArrayList";
        goto done;
    }

    jconstructor = env->GetMethodID(jarrayListClass, "<init>", "()V");
    if (jconstructor == NULL) {
        errMsg = "Cannot load constructor for ArrayList";
        goto done;
    }

    jaddMethod = env->GetMethodID(jarrayListClass,
                                     "add", "(Ljava/lang/Object;)Z");
    if (jaddMethod == NULL) {
        errMsg = "Cannot load add method for ArrayList";
        goto done;
    }

    jarrayListObj = env->NewObject(jarrayListClass, jconstructor);
    if (jarrayListObj == NULL) {
        errMsg = "Cannot construct an ArrayList";
        goto done;
    }

    jnssTokenInfoClass = env->FindClass("com/iplanet/ias/security/NssTokenInfo");
    if (jnssTokenInfoClass == NULL) {
        errMsg = "Cannot load com.iplanet.ias.security.NssTokenInfo";
        goto done;
    }

    jnssTokenInfoConstructor = env->GetMethodID(jnssTokenInfoClass,
            "<init>", "(Ljava/lang/String;Ljava/lang/String;I)V");
    if (jnssTokenInfoConstructor == NULL) {
        errMsg = "Cannot load constructor for NssTokenInfoConstructor";
        goto done;
    }

    lock = SECMOD_GetDefaultModuleListLock();
    if (!lock) {
        errMsg = "Can't lock module";
        goto done;
    }       
    SECMOD_GetReadLock(lock);
    list = SECMOD_GetDefaultModuleList();   
    if (!list) {
        errMsg = "Can't get default module";
        goto done;
    }

    for (; list != NULL; list = list->next) {
        module = list->module;
        if (module->internal == PR_FALSE) {
            int slotCount = module->loaded ? module->slotCount : 0;
            if (slotCount > 0) {
                jlibname = getLibAbsolutePath(env, module->dllName);
                int i;
                for (i = 0; i < slotCount; i++) {
                    slot = module->slots[i];
                    jtokenName = env->NewStringUTF(PK11_GetTokenName(slot));

                    jnssTokenInfoObj = env->NewObject(jnssTokenInfoClass,
                            jnssTokenInfoConstructor, jtokenName, jlibname, i);
                    if (jnssTokenInfoObj == NULL) {
                        errMsg = "Cannot construct an NssTokenInfo";
                        goto done;
                    }

                    env->CallObjectMethod(jarrayListObj, jaddMethod,
                            jnssTokenInfoObj);
                }
            }
        }
        else {
            const char* lib = NULL;
            if (jsoftoknlib != NULL)
                lib = env->GetStringUTFChars(jsoftoknlib, NULL);

            jlibname = getLibAbsolutePath(env, lib);

            if (jsoftoknlib != NULL)
                env->ReleaseStringUTFChars(jsoftoknlib, lib);

            jtokenName = env->NewStringUTF(INTERNAL_TOKEN);
            jnssTokenInfoObj = env->NewObject(jnssTokenInfoClass,
                                              jnssTokenInfoConstructor, 
                                              jtokenName, jlibname, -1);
            if (jnssTokenInfoObj == NULL) {
                errMsg = "Cannot construct an NssTokenInfo for softokn lib";
                goto done;
            }

            env->CallObjectMethod(jarrayListObj, jaddMethod,
                                  jnssTokenInfoObj);

        }
    }
    
done:
    if (lock != NULL) {
        SECMOD_ReleaseReadLock(lock);
    }

    if (errMsg != NULL) {
        JNU_Throw(env, errMsg);
        errMsg = NULL;
    }

    return jarrayListObj;
}

CERTCertificate *getCertificateFromSlot(PK11SlotInfo *slot, const char *nickname) {
    const char *nicknameHasToken = strchr(nickname, ':');
    char *tmpNick = NULL;
    if (!nicknameHasToken && !PK11_IsInternal(slot)) {
        const char *tokenName = PK11_GetTokenName(slot);
        tmpNick = (char *)malloc(strlen(tokenName)+strlen(nickname)+2);
        sprintf(tmpNick, "%s:%s", tokenName, nickname);
    } else {
        tmpNick = strdup(nickname);
    }
    CERTCertificate *cert = servnss_get_cert_from_nickname(tmpNick);
    if (tmpNick) {
        free(tmpNick);
    }
    return cert;
}

JNIEXPORT jbyteArray JNICALL Java_com_iplanet_ias_security_NssStore_getKeyAndCertificateNative(
    JNIEnv *env, jobject jobj, jstring jtokenName, jstring jnickName, jstring jpasswd) {

    const char *passwd = NULL;
    char* tokenName = NULL;
    char* nickName = NULL;

    jbyteArray bs=NULL;

    CERTCertificate *cert = NULL;
    secuPWData slotPw = { PW_NONE, NULL };
    secuPWData p12FilePw = { PW_NONE, NULL };
    PK11SlotInfo *slot;
    p12uContext* p12cxt = NULL;

    if (jtokenName != NULL) {
        tokenName = (char *)env->GetStringUTFChars(jtokenName, NULL);
        if (tokenName == NULL) {
            return NULL;
        }
    }
    
    if (jnickName != NULL) {
        nickName = (char *)env->GetStringUTFChars(jnickName, NULL);
        if (nickName == NULL) {
            if (jtokenName != NULL) {
                env->ReleaseStringUTFChars(jtokenName, tokenName);
            }
            return NULL;
        }
    } else {
        if (jtokenName != NULL) {
            env->ReleaseStringUTFChars(jtokenName, tokenName);
        }
        return NULL;
    }

    if (jpasswd != NULL) {
        passwd = env->GetStringUTFChars(jpasswd, NULL);
        if (passwd == NULL) {
            if (jtokenName != NULL) {
                env->ReleaseStringUTFChars(jtokenName, tokenName);
            }
            if (jnickName != NULL) {
                env->ReleaseStringUTFChars(jnickName, nickName);
            }
            return NULL;
        }
    }

    p12FilePw.source = PW_PLAINTEXT;
    p12FilePw.data = strdup(PKCS12_PASSWD);
    if (passwd != NULL) {
        slotPw.source = PW_PLAINTEXT;
        slotPw.data = strdup(passwd);
    }

    if (tokenName == NULL || strcmp(tokenName, INTERNAL_TOKEN) == 0)
        slot = getSlot(NULL);
    else
        slot = getSlot(tokenName);
    if (!slot) {
        errMsg = "Invalid slot.";
        goto done;
    }

    // keys and certs for given slot
#ifdef NSSDEBUG
    debug("Getting slot cert\n");
#endif
    cert = getCertificateFromSlot(slot, nickName);
#ifdef NSSDEBUG
    debug("Getting slot cert done\n");
#endif
    if (cert) {
        if (PK11_FindKeyByDERCert(slot, cert, NULL) != NULL) {
#ifdef NSSDEBUG
            sprintf(tempBuf, "CALLING ExportPCKCS12Object with %s\n", cert->nickname);
            debug(tempBuf);
#endif
            p12cxt= P12U_ExportPKCS12Object(cert->nickname, slot,
                    &slotPw, &p12FilePw);

            if (errMsg != NULL || p12cxt == NULL) {
                goto done;
            }
            bs = env->NewByteArray(p12cxt->size);
#ifdef NSSDEBUG
            sprintf(tempBuf, "Creating %s key bytes with length %d\n",
                    cert->nickname, p12cxt->size);
            debug(tempBuf);
#endif
            env->SetByteArrayRegion(bs, 0, p12cxt->size, (signed char *)p12cxt->buffer);
            freeP12uContext(p12cxt);
            p12cxt = NULL;
        } else {
#ifdef NSSDEBUG
            sprintf(tempBuf, "Creating %s cert bytes with length %d\n",
                    cert->nickname, cert->derCert.len);
            debug(tempBuf);
#endif
            bs = env->NewByteArray(cert->derCert.len);
            env->SetByteArrayRegion(bs, 0, cert->derCert.len, (signed char *)cert->derCert.data);
        }
    }

done:
#ifdef NSSDEBUG
    debug("begin of done\n");
#endif
    
    if (slot) {
#ifdef NSSDEBUG
        debug("Clean slot\n");
#endif
        PK11_FreeSlot(slot);
    }

    if (p12cxt) {
        freeP12uContext(p12cxt);
    }

    if (tokenName != NULL) {
#ifdef NSSDEBUG
        debug("release tokenName\n");
#endif
        env->ReleaseStringUTFChars(jtokenName, tokenName);
    }

    if (nickName != NULL) {
#ifdef NSSDEBUG
        debug("release nickName\n");
#endif
        env->ReleaseStringUTFChars(jnickName, nickName);
    }

    if (passwd != NULL) {
#ifdef NSSDEBUG
        debug("release passwd\n");
#endif
        env->ReleaseStringUTFChars(jpasswd, passwd);
    }

    if (p12FilePw.data != NULL) {
#ifdef NSSDEBUG
        debug("free p12FilePw\n");
#endif
        PORT_Memset(p12FilePw.data, 0, PL_strlen(p12FilePw.data));
        free(p12FilePw.data);
        p12FilePw.data = NULL;
    }
    if (slotPw.data != NULL) {
#ifdef NSSDEBUG
        debug("free slotPw\n");
#endif
        PORT_Memset(slotPw.data, 0, PL_strlen(slotPw.data));
        free(slotPw.data);
        slotPw.data = NULL;
    }

    if (errMsg != NULL) {
        JNU_Throw(env, errMsg);
        errMsg = NULL;
    }

#ifdef NSSDEBUG
    debug("return from JNI getKeyAndCert\n");
#endif
    return bs;
}

JNIEXPORT void JNICALL Java_com_iplanet_ias_security_NssStore_getKeysAndCertificatesNative(
    JNIEnv *env, jobject jobj, jobject jkeyMap, jobject jcertMap, jstring jtokenName, jstring jpasswd) {

    const char *passwd = NULL;
    char* tokenName = NULL;

    jbyteArray bs;
    jclass jhashMapClass;
    jmethodID jputMethod;
    jstring jnickname;

    CERTCertList *certs;
    CERTCertListNode *node;
    CERTCertificate *cert = NULL;
    secuPWData slotPw = { PW_NONE, NULL };
    secuPWData p12FilePw = { PW_NONE, NULL };
    PK11SlotInfo *slot;
    p12uContext* p12cxt = NULL;

    if (jtokenName != NULL) {
        tokenName = (char *)env->GetStringUTFChars(jtokenName, NULL);
        if (tokenName == NULL) {
            return;
        }
    }
    
    if (jpasswd != NULL) {
        passwd = env->GetStringUTFChars(jpasswd, NULL);
        if (passwd == NULL) {
            return;
        }
    }

    p12FilePw.source = PW_PLAINTEXT;
    p12FilePw.data = strdup(PKCS12_PASSWD);
    if (passwd != NULL) {
        slotPw.source = PW_PLAINTEXT;
        slotPw.data = strdup(passwd);
    }

    jhashMapClass = env->GetObjectClass(jkeyMap);
    if (jhashMapClass == NULL) {
        errMsg = "Cannot load java.lang.HashMap";
        goto done;
    }

    jputMethod = env->GetMethodID(jhashMapClass,
            "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (jputMethod == NULL) {
        errMsg = "Cannot load java.lang.HashMap.put(Object, Object)";
        goto done;
    }

    if (strcmp(tokenName, INTERNAL_TOKEN) == 0)
        slot = getSlot(NULL);
    else
        slot = getSlot(tokenName);
    if (!slot) {
        errMsg = "Invalid slot.";
        goto done;
    }

    // keys and certs for given slot
#ifdef NSSDEBUG
    debug("Getting slot certs\n");
#endif
    certs = PK11_ListCertsInSlot(slot);
#ifdef NSSDEBUG
    debug("Getting slot certs done\n");
#endif
    if (certs) {
        for (node = CERT_LIST_HEAD(certs); !CERT_LIST_END(node, certs);
				node = CERT_LIST_NEXT(node)) {
            cert = node->cert;
            jnickname = env->NewStringUTF(cert->nickname);

            if (PK11_FindKeyByDERCert(slot, cert, NULL) != NULL) {
#ifdef NSSDEBUG
                sprintf(tempBuf, "CALLING ExportPCKCS12Object with %s\n", cert->nickname);
                debug(tempBuf);
#endif
                p12cxt= P12U_ExportPKCS12Object(cert->nickname, slot,
                        &slotPw, &p12FilePw);

                if (errMsg != NULL || p12cxt == NULL) {
                    goto done;
                }
                bs = env->NewByteArray(p12cxt->size);
#ifdef NSSDEBUG
                sprintf(tempBuf, "Creating %s key bytes with length %d\n",
                        cert->nickname, p12cxt->size);
                debug(tempBuf);
#endif
                env->SetByteArrayRegion(bs, 0, p12cxt->size, (signed char *)p12cxt->buffer);
                env->CallObjectMethod(jkeyMap, jputMethod, jnickname, bs);
                freeP12uContext(p12cxt);
                p12cxt = NULL;
            } else {
#ifdef NSSDEBUG
                sprintf(tempBuf, "Creating %s cert bytes with length %d\n",
                        cert->nickname, cert->derCert.len);
                debug(tempBuf);
#endif
                bs = env->NewByteArray(cert->derCert.len);
                env->SetByteArrayRegion(bs, 0, cert->derCert.len, (signed char *)cert->derCert.data);
                env->CallObjectMethod(jcertMap, jputMethod, jnickname, bs);
            }
        }
    }

done:
#ifdef NSSDEBUG
    debug("begin of done\n");
#endif
    
    if (slot) {
#ifdef NSSDEBUG
        debug("Clean slot\n");
#endif
        PK11_FreeSlot(slot);
    }

    if (p12cxt) {
        freeP12uContext(p12cxt);
    }

    if (passwd != NULL) {
#ifdef NSSDEBUG
        debug("release passwd\n");
#endif
        env->ReleaseStringUTFChars(jpasswd, passwd);
    }

    if (p12FilePw.data != NULL) {
#ifdef NSSDEBUG
        debug("free p12FilePw\n");
#endif
        PORT_Memset(p12FilePw.data, 0, PL_strlen(p12FilePw.data));
        free(p12FilePw.data);
        p12FilePw.data = NULL;
    }
    if (slotPw.data != NULL) {
#ifdef NSSDEBUG
        debug("free slotPw\n");
#endif
        PORT_Memset(slotPw.data, 0, PL_strlen(slotPw.data));
        free(slotPw.data);
        slotPw.data = NULL;
    }

    if (errMsg != NULL) {
        JNU_Throw(env, errMsg);
        errMsg = NULL;
    }

#ifdef NSSDEBUG
    debug("return from JNI getKeysAndCerts\n");
#endif
    return;
}

JNIEXPORT void JNICALL Java_com_iplanet_ias_security_NssStore_getCACertificatesNative(
    JNIEnv *env, jobject jobj, jobject jcertMap) {

    const char *passwd = NULL;
    
    jbyteArray bs;
    jstring jnickname;
    jclass jhashMapClass;
    jmethodID jputMethod;

    CERTCertList *certs;
    CERTCertListNode *node;
    CERTCertificate *cert = NULL;

    jhashMapClass = env->GetObjectClass(jcertMap);
    if (jhashMapClass == NULL) {
        errMsg = "Cannot load java.lang.HashMap";
        goto done;
    }

    jputMethod = env->GetMethodID(jhashMapClass,
            "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (jputMethod == NULL) {
        errMsg = "Cannot load java.lang.HashMap.put(Object, Object)";
        goto done;
    }

    certs = PK11_ListCerts(PK11CertListCA, NULL);

    if (certs) {
        for (node = CERT_LIST_HEAD(certs); !CERT_LIST_END(node, certs);
				node = CERT_LIST_NEXT(node)) {
            cert = node->cert;
            jnickname = env->NewStringUTF(cert->nickname);
            bs = env->NewByteArray(cert->derCert.len);
            env->SetByteArrayRegion(bs, 0, cert->derCert.len, 
                                       (signed char*)cert->derCert.data);
            env->CallObjectMethod(jcertMap, jputMethod, jnickname, bs);
        }
    }

done:
    if (errMsg != NULL) {
        JNU_Throw(env, errMsg);
	errMsg = NULL;
    }
}
