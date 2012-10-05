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

#include "pkcs12.h"
#include "p12plcy.h"
#include "nss.h"
#include "certdb.h"
#include "secmodt.h"
#include "secmod.h"
#include "secerr.h"
#include "seccomon.h"
#include "Pkcs12Util.h"

PRBool pk12_debugging = PR_FALSE;
const char *errMsg = NULL;
char tempBuf[512]; //assuming all sprintf debug message can be fitted into tempBuf

void debug(char* msg)
{
    printf(msg);
    fprintf(stdout, msg);
    fflush(stdout);
}

static p12uContext *
allocateP12uContext(PRIntn bufSize)
{
    p12uContext *p12cxt;
    unsigned char *buf;

    p12cxt = (p12uContext *)PORT_ZAlloc(sizeof(p12uContext));
    if (!p12cxt) {
	PR_SetError(SEC_ERROR_NO_MEMORY, 0);
	return NULL;
    }

    buf = (unsigned char*)PORT_ZAlloc(bufSize);
    if(!buf) {
        PR_Free(p12cxt);
	PR_SetError(SEC_ERROR_NO_MEMORY, 0);
	return NULL;
    }

    p12cxt->buffer = buf;
    p12cxt->size = 0;
    p12cxt->bufSize = bufSize;

    return p12cxt;
}

void
freeP12uContext(p12uContext *p12cxt)
{
#ifdef NSSDEBUG
    debug("Free p12cxt\n");
#endif
    if (p12cxt != NULL) {
       if (p12cxt->buffer != NULL) {
           PR_Free(p12cxt->buffer);
           p12cxt->buffer = NULL;
       }
       PR_Free(p12cxt);
    }
}

char *
SECU_GetModulePassword(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    secuPWData *pwdata = (secuPWData *)arg;
    secuPWData pwnull = { PW_NONE, 0 };

    if (pwdata == NULL)
        pwdata = &pwnull;

    if (retry == PR_FALSE && pwdata->data != NULL) {
#ifdef NSSDEBUG
        char* tempVar = PK11_GetTokenName(slot);
        if (tempVar == NULL) {
            tempVar = "internal";
        }
        sprintf(tempBuf, "slot %s with pwd %s \n", tempVar, pwd);
        debug(tempBuf);
#endif
        return PL_strdup(pwdata->data);
    }

    return NULL;
}

PK11SlotInfo *
getSlot(char* tokenName)
{
    PK11SlotInfo *slot = NULL;
    if (tokenName != NULL) {
#ifdef NSSDEBUG
        sprintf(tempBuf, "Get slot %s\n", tokenName);
        debug(tempBuf);
#endif
        slot = PK11_FindSlotByName(tokenName);
    } else {
#ifdef NSSDEBUG
        debug("Get internal slot\n");
#endif
        slot = PK11_GetInternalKeySlot();
    }
    return slot;
}

static SECStatus
p12u_SwapUnicodeBytes(SECItem *uniItem)
{
    unsigned int i;
    unsigned char a;
    if((uniItem == NULL) || (uniItem->len % 2)) {
	return SECFailure;
    }
    for(i = 0; i < uniItem->len; i += 2) {
	a = uniItem->data[i];
	uniItem->data[i] = uniItem->data[i+1];
	uniItem->data[i+1] = a;
    }
    return SECSuccess;
}

static PRBool
p12u_ucs2_ascii_conversion_function(PRBool	   toUnicode,
				    unsigned char *inBuf,
				    unsigned int   inBufLen,
				    unsigned char *outBuf,
				    unsigned int   maxOutBufLen,
				    unsigned int  *outBufLen,
				    PRBool	   swapBytes)
{
    SECItem it = { siBuffer, NULL, 0 };
    SECItem *dup = NULL;
    PRBool ret;

#ifdef DEBUG_CONVERSION
    if (pk12_debugging) {
	int i;
	printf("Converted from:\n");
	for (i=0; i<inBufLen; i++) {
	    printf("%2x ", inBuf[i]);
	    /*if (i%60 == 0) printf("\n");*/
	}
	printf("\n");
    }
#endif
    it.data = inBuf;
    it.len = inBufLen;
    dup = SECITEM_DupItem(&it);
    /* If converting Unicode to ASCII, swap bytes before conversion
     * as neccessary.
     */
    if (!toUnicode && swapBytes) {
	if (p12u_SwapUnicodeBytes(dup) != SECSuccess) {
	    SECITEM_ZfreeItem(dup, PR_TRUE);
	    return PR_FALSE;
	}
    }
    /* Perform the conversion. */
    ret = PORT_UCS2_UTF8Conversion(toUnicode, dup->data, dup->len,
                                   outBuf, maxOutBufLen, outBufLen);
    if (dup)
	SECITEM_ZfreeItem(dup, PR_TRUE);

#ifdef DEBUG_CONVERSION
    if (pk12_debugging) {
	int i;
	printf("Converted to:\n");
	for (i=0; i<*outBufLen; i++) {
	    printf("%2x ", outBuf[i]);
	    /*if (i%60 == 0) printf("\n");*/
	}
	printf("\n");
    }
#endif
    return ret;
}

SECStatus
P12U_UnicodeConversion(PRArenaPool *arena, SECItem *dest, SECItem *src,
		       PRBool toUnicode, PRBool swapBytes)
{
    unsigned int allocLen;
    if(!dest || !src) {
	return SECFailure;
    }
    allocLen = ((toUnicode) ? (src->len << 2) : src->len);
    if(arena) {
	dest->data = (unsigned char *)PORT_ArenaZAlloc(arena, allocLen);
    } else {
	dest->data = (unsigned char *)PORT_ZAlloc(allocLen);
    }
    if(PORT_UCS2_ASCIIConversion(toUnicode, src->data, src->len,
				 dest->data, allocLen, &dest->len,
				 swapBytes) == PR_FALSE) {
	if(!arena) {
	    PORT_Free(dest->data);
	}
	dest->data = NULL;
	return SECFailure;
    }
    return SECSuccess;
}

SECItem *
P12U_GetP12FilePassword(secuPWData *p12FilePw)
{
    SECItem *pwItem = NULL;

    if (p12FilePw != NULL && p12FilePw->source != PW_NONE) {
        /* Plaintext */
        pwItem = SECITEM_AllocItem(NULL, NULL, PL_strlen(p12FilePw->data) + 1);
        memcpy(pwItem->data, p12FilePw->data, pwItem->len);
    }

    return pwItem;
}

static void
p12u_WriteToExportArray(void *arg, const char *buf, unsigned long len)
{
    p12uContext *p12cxt = (p12uContext*)arg;
    int32 writeLen = (int32)len;
    int32 i = 0;
    unsigned char* totalBuf = p12cxt->buffer;
    int32 size = p12cxt->size;
    int32 bufSize = p12cxt->bufSize;
    int32 expandedSize = bufSize;

    //expand buffer if necessary
    //I try to put this loop inside if block below. It does not comply in
    //Solaris env. I think there is a bug in compiler.
    while (size + writeLen > expandedSize) {
        expandedSize += defaultBufferSize;
    }

#ifdef NSSDEBUG
    if (expandedSize > bufSize) {
        debug("growing p12cxt buffer\n");
    }
#endif
    if (expandedSize > bufSize) {
        unsigned char* newBuf = (unsigned char*)PORT_ZAlloc(expandedSize);
        if (newBuf == NULL) {
            errMsg = "Cannot allocate expanded buffer.";
            return;
        }
        //copy data of unsigned char*
        for (i = 0; i < size; i++) {
            newBuf[i] = totalBuf[i];
        }

        PR_Free(totalBuf);
        p12cxt->buffer = newBuf;
        p12cxt->bufSize = expandedSize;
        totalBuf = newBuf;
    }
    
    for (i = 0; i <  writeLen; i++) {
        totalBuf[i + size] = buf[i];
    }

    p12cxt->size = size + writeLen;
}

p12uContext*
P12U_ExportPKCS12Object(char *nn, PK11SlotInfo *inSlot,
			secuPWData *slotPw, secuPWData *p12FilePw)
{
    SEC_PKCS12ExportContext *p12ecx = NULL;
    SEC_PKCS12SafeInfo *keySafe = NULL, *certSafe = NULL;
    SECItem *pwitem = NULL;
    p12uContext *p12cxt = NULL;
    CERTCertList* certlist = NULL;
    CERTCertListNode* node = NULL;
    PK11SlotInfo* slot = NULL;

#ifdef NSSDEBUG
    debug("Finding Cert\n");
#endif
    certlist = PK11_FindCertsFromNickname(nn, slotPw);
    if(!certlist) {
        errMsg = "find user certs from nickname failed";
        return NULL;
    }

    /* setup unicode callback functions */
    PORT_SetUCS2_ASCIIConversionFunction(p12u_ucs2_ascii_conversion_function);

    if ((SECSuccess != CERT_FilterCertListForUserCerts(certlist)) ||
        CERT_LIST_EMPTY(certlist)) {
        errMsg = "no user certs from given nickname";
        goto loser;
    }

    /*	Password to use for PKCS12 file.  */
    pwitem = P12U_GetP12FilePassword(p12FilePw);
    if(!pwitem) {
	goto loser;
    }

    p12cxt = allocateP12uContext(defaultBufferSize);
    if(!p12cxt) {
        errMsg = "Initialization failed.";
        goto loser;
    }

#ifdef NSSDEBUG
    debug("Processing certlist\n");
#endif
    if (certlist) {
        CERTCertificate* cert = NULL;
        node = CERT_LIST_HEAD(certlist);
        if (node) {
            cert = node->cert;
        }
        if (cert) {
            slot = cert->slot; /* use the slot from the first matching
                certificate to create the context . This is for keygen */
        }
    }
    if (!slot) {
        errMsg = "cert does not have a slot";
        goto loser;
    }

#ifdef NSSDEBUG
    debug("Create export context\n");
#endif
    p12ecx = SEC_PKCS12CreateExportContext(NULL, NULL, slot, slotPw);
    if(!p12ecx) {
        errMsg = "export context creation failed";
        goto loser;
    }

#ifdef NSSDEBUG
    debug("Add password integrity\n");
#endif
    if(SEC_PKCS12AddPasswordIntegrity(p12ecx, pwitem, SEC_OID_SHA1)
       != SECSuccess) {
        errMsg = "PKCS12 add password integrity failed";
        goto loser;
    }

#ifdef NSSDEBUG
    debug("Looping cert list\n");
#endif
    for (node = CERT_LIST_HEAD(certlist);!CERT_LIST_END(node, certlist);node=CERT_LIST_NEXT(node))
    {
        CERTCertificate* cert = node->cert;
        if (!cert->slot) {
            errMsg = "cert does not have a slot";
            goto loser;
        }
    
        keySafe = SEC_PKCS12CreateUnencryptedSafe(p12ecx);
        if(/*!SEC_PKCS12IsEncryptionAllowed() || */ PK11_IsFIPS()) {
            certSafe = keySafe;
        } else {
            certSafe = SEC_PKCS12CreatePasswordPrivSafe(p12ecx, pwitem,
                SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC);
        }
    
        if(!certSafe || !keySafe) {
            errMsg = "key or cert safe creation failed";
            goto loser;
        }
    
        if(SEC_PKCS12AddCertAndKey(p12ecx, certSafe, NULL, cert,
            CERT_GetDefaultCertDB(), keySafe, NULL, PR_TRUE, pwitem,
            SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC)
            != SECSuccess) {
                errMsg = "add cert and key failed";
                goto loser;
        }
    }

    CERT_DestroyCertList(certlist);
    certlist = NULL;

#ifdef NSSDEBUG
    debug("pkcs12 encode\n");
#endif
    if(SEC_PKCS12Encode(p12ecx, p12u_WriteToExportArray, p12cxt)
                        != SECSuccess || errMsg != NULL) {
        errMsg = "PKCS12 encode failed";
        goto loser;
    }

#ifdef NSSDEBUG
    debug("Free pwItem, export context\n");
#endif
    SECITEM_ZfreeItem(pwitem, PR_TRUE);
    SEC_PKCS12DestroyExportContext(p12ecx);

    return p12cxt;

loser:
    SEC_PKCS12DestroyExportContext(p12ecx);
    freeP12uContext(p12cxt);

    if (certlist) {
        CERT_DestroyCertList(certlist);
        certlist = NULL;
    }    

    if(pwitem) {
        SECITEM_ZfreeItem(pwitem, PR_TRUE);
    }
    return NULL;
}

static void
p12u_EnableAllCiphers()
{
    SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
    SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
    SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
    SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);
}

