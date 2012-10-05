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

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 2000 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

*/

#include <sechash.h>    /* MD5 routines */
#include <base64.h>	/* for MIME encoding */
#include <stdio.h>
#include <string.h>
#include <netsite.h>
#include <base/plist.h>
#include <base/util.h>
#include <frame/conf.h> /* for conf_getglobals() */
#include <libaccess/aclerror.h>
#include <ldaputil/LdapSessionPool.h>
#include <ldaputil/dbconf.h>
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <ldaputil/errors.h>
#include "libaccess/digest.h"
#include "libaccess/FileRealm.h"

#include <base/vs.h>

#include "pk11func.h"

/*    digest.cpp
 *    This file contains Digest Authentication code.
 */

/* Generate a private key */
void GenerateDigestPrivatekey(void)
{
    int keylen = PRIVATE_KEY_LEN;
    
    PK11_GenerateRandom((unsigned char*)DigestPrivatekey, keylen);
}

/* Retrieve the private key */
char * GetDigestPrivatekey(void)
{
    return DigestPrivatekey;
}

void CvtHex(DigestHash Bin, DigestHashHex Hex)
{
    unsigned short i;
    unsigned char j;

    for (i = 0; i < HASHLEN; i++) {
        j = (Bin[i] >> 4) & 0xf;
        if (j <= 9)
            Hex[i*2] = (j + '0');
         else
            Hex[i*2] = (j + 'a' - 10);
        j = Bin[i] & 0xf;
        if (j <= 9)
            Hex[i*2+1] = (j + '0');
         else
            Hex[i*2+1] = (j + 'a' - 10);
    };
    Hex[HASHHEXLEN] = '\0';
}

/* The nonce we are using is the base-64 encoding of:
 *
 *    time-stamp H(time-stamp ":" uri ":" private-key)
 *
 * Caller needs to PORT_Free() nonce.
 *
 */
int
Digest_create_nonce(const char *privatekey, char **nonce, char *uri)
{
    time_t current_time;
    unsigned int digestLen;
    DigestHash tmpbuf;
    DigestHashHex tmpnonce;
    DigestHash H;
    PK11Context * Md5Ctx = PK11_CreateDigestContext(SEC_OID_MD5);
    unsigned char buf[4096];
    unsigned char * base;
    unsigned int lenp;
    char * result;

    if (!privatekey) {
        return REQ_ABORTED;
    }

    time(&current_time);
    util_itoa(current_time,tmpbuf);

    strcpy((char *)buf, tmpbuf);
    PK11_DigestBegin(Md5Ctx);
    PK11_DigestOp(Md5Ctx, (const unsigned char *)tmpbuf, strlen(tmpbuf));
    PK11_DigestOp(Md5Ctx, (const unsigned char *)":", 1);
    PK11_DigestOp(Md5Ctx, (const unsigned char *)uri, strlen(uri));
    PK11_DigestOp(Md5Ctx, (const unsigned char *)":", 1);
    PK11_DigestOp(Md5Ctx, (const unsigned char *)privatekey, strlen(privatekey));
    PK11_DigestFinal(Md5Ctx, (unsigned char *)H, &digestLen, sizeof(H));
    CvtHex(H, tmpnonce);
    strcat((char *)buf, " ");
    strcat((char *)buf, tmpnonce);
    PK11_DestroyContext(Md5Ctx, 1);

    /* Convert the whole string to base-64 encoding */
    lenp = strlen((const char *)buf);
    *nonce = BTOA_DataToAscii((const unsigned char *)buf, lenp);

    return REQ_PROCEED;
}

/* Given a base-64 encoded nonce, strip off the timestamp and verify that it 
 * was generated with our private key
 *
 * returns PR_TRUE if ok, PR_FALSE if it is stale
 */
int
Digest_check_nonce(const char *pkey, char *nonce, char *uri)
{
    unsigned int digestLen;
    unsigned char * tmpnonce;
    DigestHashHex testnonce;
    DigestHash H;
    char * ntime;
    char * tnonce;
    unsigned int lenp;
    int rv;
    time_t current_time;
    time_t nonce_time;

    if (!pkey || !nonce || !uri) {
        return PR_FALSE;
    }

    /* Undo the base-64 encoding */
    lenp = strlen((const char *)nonce);
    tmpnonce = ATOB_AsciiToData((const char *)nonce, &lenp);
    if (!tmpnonce) {
        return PR_FALSE;
    }

    unsigned char * loctmp = (unsigned char *)MALLOC(lenp + 1);
    memcpy(loctmp, tmpnonce, lenp);
    loctmp[lenp] = 0;
    PORT_Free(tmpnonce);

    char *lasts;
    ntime = util_strtok((char *)loctmp, " ", &lasts);
    tnonce = util_strtok(NULL, " ", &lasts);

    PK11Context * Md5Ctx = PK11_CreateDigestContext(SEC_OID_MD5);
    PK11_DigestBegin(Md5Ctx);
    PK11_DigestOp(Md5Ctx, (const unsigned char *)ntime, strlen(ntime));
    PK11_DigestOp(Md5Ctx, (const unsigned char *)":", 1);
    PK11_DigestOp(Md5Ctx, (const unsigned char *)uri, strlen(uri));
    PK11_DigestOp(Md5Ctx, (const unsigned char *)":", 1);
    PK11_DigestOp(Md5Ctx, (const unsigned char *)pkey, strlen(pkey));
    PK11_DigestFinal(Md5Ctx, (unsigned char *)H, &digestLen, sizeof(H));
    CvtHex(H, testnonce);
    PK11_DestroyContext(Md5Ctx, 1);

    /* check for a stale time stamp. If the time stamp is stale we
     * still check to see if the user sent the proper digest
     * password. The stale flag is only set if the nonce is expired
     * AND the credentials are OK, otherwise the get a 401, but that
     * happens elsewhere.
     */

    nonce_time = (time_t) strtol(ntime, (char **)NULL, 10);

    time(&current_time);
    if ((current_time - nonce_time) > conf_getglobals()->digest_stale_timeout)
        rv = PR_FALSE;
    else
        rv = PR_TRUE;

    if ((strcmp(tnonce, testnonce)!=0))
        rv = PR_FALSE;

    FREE(loctmp);
    return rv;
}



static void MD5Init(PK11Context** context)
{
    *context = PK11_CreateDigestContext(SEC_OID_MD5);
}



static void MD5Update(PK11Context* context, const unsigned char *inbuf, int len)
{
    PK11_DigestOp(context,inbuf,len);
}


static void MD5Final(unsigned char * result, PK11Context* context)
{
    unsigned int digestLen=0;
    PK11_DigestFinal(context,result,&digestLen,HASHLEN);
    PR_ASSERT(digestLen==HASHLEN);
}


void CvtHex001(
    IN HASH Bin,
    OUT HASHHEX Hex
    )
{
    unsigned short i;
    unsigned char j;

    for (i = 0; i < HASHLEN; i++) {
        j = (Bin[i] >> 4) & 0xf;
        if (j <= 9)
            Hex[i*2] = (j + '0');
         else
            Hex[i*2] = (j + 'a' - 10);
        j = Bin[i] & 0xf;
        if (j <= 9)
            Hex[i*2+1] = (j + '0');
         else
            Hex[i*2+1] = (j + 'a' - 10);
    }
    Hex[HASHHEXLEN] = '\0';
}

/* calculate H(A1) as per spec */
void DigestCalcHA1(
    IN char * pszAlg,
    IN char * pszUserName,
    IN char * pszRealm,
    IN char * pszPassword,
    IN char * pszNonce,
    IN char * pszCNonce,
    OUT HASHHEX SessionKey
    )
{
      PK11Context* context;
      HASH HA1;

      MD5Init(&context);
      MD5Update(context, (const unsigned char *)pszUserName, strlen(pszUserName));
      MD5Update(context, (const unsigned char *)":", 1);
      MD5Update(context, (const unsigned char *)pszRealm, strlen(pszRealm));
      MD5Update(context, (const unsigned char *)":", 1);
      MD5Update(context, (const unsigned char *)pszPassword, strlen(pszPassword));
      MD5Final((unsigned char *)HA1, context);
      PK11_DestroyContext(context, PR_TRUE);
      if (strcmp(pszAlg, "md5-sess") == 0) {
            MD5Init(&context);
            MD5Update(context, (const unsigned char *)HA1, HASHLEN);
            MD5Update(context, (const unsigned char *)":", 1);
            MD5Update(context, (const unsigned char *)pszNonce, strlen(pszNonce));
            MD5Update(context, (const unsigned char *)":", 1);
            MD5Update(context, (const unsigned char *)pszCNonce, strlen(pszCNonce));
            MD5Final((unsigned char *)HA1, context);
            PK11_DestroyContext(context, PR_TRUE);
      }
      CvtHex(HA1, SessionKey);
}

/* calculate request-digest/response-digest as per HTTP Digest spec */
void DigestCalcResponse(
    IN HASHHEX HA1,           /* H(A1) */
    IN char * pszNonce,       /* nonce from server */
    IN char * pszNonceCount,  /* 8 hex digits */
    IN char * pszCNonce,      /* client nonce */
    IN char * pszQop,         /* qop-value: "", "auth", "auth-int" */
    IN char * pszMethod,      /* method from the request */
    IN char * pszDigestUri,   /* requested URL */
    IN HASHHEX HEntity,       /* H(entity body) if qop="auth-int" */
    OUT HASHHEX Response      /* request-digest or response-digest */
    )
{
      PK11Context* context;
      HASH HA2;
      HASH RespHash;
      HASHHEX HA2Hex;

      /* calculate H(A2) */
      MD5Init(&context);
      MD5Update(context, (const unsigned char *)pszMethod, strlen(pszMethod));
      MD5Update(context, (const unsigned char *)":", 1);
      MD5Update(context, (const unsigned char *)pszDigestUri, strlen(pszDigestUri));
      if ((pszQop) && (strcmp(pszQop, "auth-int") == 0)) {
            MD5Update(context, (const unsigned char *)":", 1);
            MD5Update(context, (const unsigned char *)HEntity, HASHHEXLEN);
      }
      MD5Final((unsigned char *)HA2, context);
      PK11_DestroyContext(context, PR_TRUE);
      CvtHex(HA2, HA2Hex);

      /* calculate response */
      MD5Init(&context);
      MD5Update(context, (const unsigned char *)HA1, HASHHEXLEN);
      MD5Update(context, (const unsigned char *)":", 1);
      MD5Update(context, (const unsigned char *)pszNonce, strlen(pszNonce));
      MD5Update(context, (const unsigned char *)":", 1);
      if (pszQop) {
          MD5Update(context, (const unsigned char *)pszNonceCount, strlen(pszNonceCount));
          MD5Update(context, (const unsigned char *)":", 1);
          MD5Update(context, (const unsigned char *)pszCNonce, strlen(pszCNonce));
          MD5Update(context, (const unsigned char *)":", 1);
          MD5Update(context, (const unsigned char *)pszQop, strlen(pszQop));
          MD5Update(context, (const unsigned char *)":", 1);
      }
      MD5Update(context, (const unsigned char *)HA2Hex, HASHHEXLEN);
      MD5Final((unsigned char *)RespHash, context);
      PK11_DestroyContext(context, PR_TRUE);
      CvtHex(RespHash, Response);
}

/*
 * Take a look at the database entry in dbswitch.conf and see if
 * digest authentication is allowed.
 */
int 
ACL_AuthDBAllowsDigestAuth(NSErr_t *errp, PList_t resource, PList_t auth_info)
{
    int rv;
    char *dbname;
    ACLMethod_t method;
    LdapSessionPool *sp = 0;
    ACLDbType_t dbtype;
    void        *pAnyDB=NULL;

    // get hold of the authentication DB name
    if ((rv = ACL_AuthInfoGetDbname(auth_info, &dbname)) < 0) {
        char rv_str[16];
        sprintf(rv_str, "%d", rv);
        nserrGenerate(errp, ACLERRFAIL, ACLERR5830, ACL_Program, 2,
        XP_GetAdminStr(DBT_ldapaclUnableToGetDatabaseName), rv_str);
        return LAS_EVAL_FAIL;
    }

    // get the VS's idea of the user database
    rv = ACL_DatabaseFind(errp, dbname, &dbtype, &pAnyDB);
    if (rv != LAS_EVAL_TRUE || ACL_DbTypeLdap == ACL_DBTYPE_INVALID) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR5840, ACL_Program, 2,
                XP_GetAdminStr(DBT_ldapaclUnableToGetParsedDatabaseName), dbname);
        return LAS_EVAL_FAIL;
    }
    if (dbtype ==  ACL_DbTypeFile) {
        FileRealm *pFileRealm = (FileRealm *)pAnyDB;
        return (pFileRealm->supportDigest()? LAS_EVAL_TRUE:LAS_EVAL_FALSE);
    }

    if (dbtype != ACL_DbTypeLdap)
        return LAS_EVAL_FALSE;
    LdapRealm *ldapRealm= (LdapRealm*)pAnyDB;
    return (ldapRealm->supportDigest()?LAS_EVAL_TRUE : LAS_EVAL_FALSE);
}
