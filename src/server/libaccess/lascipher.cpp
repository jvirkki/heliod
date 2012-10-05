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
 * lascipher.cpp: LAS Program to evaluate cipher acl's
 * As needed stolen code from lasuser.cpp
 * Written by Pramod Khincha
 */


#include <netsite.h>
#include <base/shexp.h>
#include <base/util.h>
#include <base/ereport.h>
#include <libaccess/acl.h>
#include <libaccess/aclerror.h>
#include <libaccess/aclproto.h>
#include "aclpriv.h"
#include <libaccess/dbtlibaccess.h>
#include "ssl.h"
#include "secitem.h"
#include "base64.h"


/*
 *  LASCipherEval
 *    INPUT
 *	attr_name	The string "cipher" - in lower case.
 *	comparator	CMP_OP_EQ or CMP_OP_NE only
 *	attr_pattern	A comma-separated list of ciphers
 *	*cachable	Always set to ACL_NOT_CACHABLE.
 *      subject		Subject property list
 *      resource        Resource property list
 *      auth_info	Authentication info, if any (usually NULL)
 *    RETURNS
 *	retcode	        The usual LAS return codes.
 */
int LASCipherEval(NSErr_t *errp, char *attr_name, CmpOp_t comparator, 
		char *attr_pattern, ACLCachable_t *cachable,
                void **LAS_cookie, PList_t subject, PList_t resource,
                PList_t auth_info, PList_t global_auth)
{
    char	    *cipher = NULL;
    char	    *ciphers;
    char	    *algo;
    char	    *comma;
    int		    retcode;
    int		    matched;
    int		    rv;

    Session         *sn = NULL;
    PRInt32 secon = -1;
    PRInt32 keySize, secretKeySize;
    char *issuer_dn;
    char *user_dn;
    SECItem *iditem;


    *cachable = ACL_NOT_CACHABLE;
    *LAS_cookie = (void *)0;

    if (strcmp(attr_name, ACL_ATTR_CIPHER) != 0) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR5700, ACL_Program, 2, XP_GetAdminStr(DBT_lasUserEvalReceivedRequestForAtt_), attr_name);
	return LAS_EVAL_INVALID;
    }

    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR5710, ACL_Program, 2, XP_GetAdminStr(DBT_lasuserevalIllegalComparatorDN_), comparator_string(comparator));
	return LAS_EVAL_INVALID;
    }

    if (!strcmp(attr_pattern, "anyone")) {
        *cachable = ACL_INDEF_CACHABLE;
        ereport(LOG_VERBOSE, "acl cipher: match on cipher %s (anyone)",
                (comparator == CMP_OP_EQ) ? "=" : "!=");
	return comparator == CMP_OP_EQ ? LAS_EVAL_TRUE : LAS_EVAL_FALSE;
    }

    /* get the cipher string */

    rv = PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL);

#if 0
    if (sn) {
       if (!SSL_SecurityStatus((int)sn->csd, &secon, &cipher, &keySize, &secretKeySize, &issuer_dn, &user_dn)) {
	  if (secon < 1)
	     return LAS_EVAL_TRUE;
       }
    }
    else
	return LAS_EVAL_TRUE;
#endif

    if (sn) {
	char *temp = NULL;
	temp = pblock_findval("cipher", sn->client);
	if (temp)
           cipher = STRDUP(temp);
	if (!cipher) {
            ereport(LOG_VERBOSE, "acl cipher: match on cipher (no cipher)");
            return LAS_EVAL_TRUE;
	}
    }
    else {
        ereport(LOG_VERBOSE, "acl cipher: match on cipher (no cipher)");
	return LAS_EVAL_TRUE;
    }

    if (!strcmp(attr_pattern, "all")) {
        ereport(LOG_VERBOSE, "acl cipher: match on cipher %s (all)",
                (comparator == CMP_OP_EQ) ? "=" : "!=");
	return comparator == CMP_OP_EQ ? LAS_EVAL_TRUE : LAS_EVAL_FALSE;
    }

    ciphers = STRDUP(attr_pattern);

    if (!ciphers) {
	nserrGenerate(errp, ACLERRNOMEM, ACLERR5720, ACL_Program, 1,
	XP_GetAdminStr(DBT_lasuserevalRanOutOfMemoryN_));
	return LAS_EVAL_FAIL;
    }

    algo = ciphers;
    matched = 0;

    /* check if the cipher is amongst the list of ciphers */ 
    while(algo != 0 && *algo != 0 && !matched) {
	if ((comma = strchr(algo, ',')) != NULL) {
	    *comma++ = 0;
	}

	/* ignore leading whitespace */
	while(*algo == ' ' || *algo == '\t') algo++;

	if (*algo) {
	    /* ignore trailing whitespace */
	    int len = strlen(algo);
	    char *ptr = algo+len-1;

	    while(*ptr == ' ' || *ptr == '\t') *ptr-- = 0;
	}

	if (!WILDPAT_CASECMP(cipher, algo)) {
	    /* cipher is one of the ciphers */
	    matched = 1;
	}
	else {
	    /* continue checking for next algo */
	    algo = comma;
	}
    }

    if (comparator == CMP_OP_EQ) {
        ereport(LOG_VERBOSE, "acl cipher: %s on cipher = (%s)",
                matched ? "match" : "no match", attr_pattern);
	retcode = (matched ? LAS_EVAL_TRUE : LAS_EVAL_FALSE);
    }
    else {
        ereport(LOG_VERBOSE, "acl cipher: %s on cipher != (%s)",
                matched ? "no match" : "match", attr_pattern);
	retcode = (matched ? LAS_EVAL_FALSE : LAS_EVAL_TRUE);
    }

    FREE(ciphers);
    return retcode;
}


/*	LASCipherFlush
 *	Deallocates any memory previously allocated by the LAS
 */

void
LASCipherFlush(void **las_cookie)
{
    return;
}
