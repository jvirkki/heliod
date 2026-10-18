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

#include <string.h>
#include <base/crit.h>
#include <libaccess/cryptwrapper.h>
#if !defined (XP_WIN32)
#include <unistd.h>
#else
extern "C" char *crypt(const char *key, const char *salt);
#endif
#include <netsite.h>

static CRITICAL acl_crypt_crit = NULL;

/* ACL_CryptCritInit
 *
 * Initializes the critical section needed for crypt calls if needed.
 */
#if defined(XP_WIN32) || defined(LINUX) || defined(AIX)
#define ACL_CRYPT_USES_CRIT
#endif

NSAPI_PUBLIC
void ACL_CryptCritInit(void)
{
#if defined(ACL_CRYPT_USES_CRIT)
    acl_crypt_crit = crit_init();
#endif
}


/* ACL_Crypt
 *
 * This is a wrapper function for the crypt() function which is not
 * always thread-safe. All calls to crypt() must go through this
 * function or threads may corrupt each others data.
 *
 * WARNING : Callers of this function MUST call FREE() after they have
 * used this char * returned by this function.
 *
 * In/Out parameters : see crypt() man page.
 *
 * Platform notes:
 *
 * Solaris : crypt(3C) is labeled MT-Safe.
 * Linux : crypt(3) is not thread safe; serialize with a critical section.
 * Windows : uses crypt() implemented in
 *     netsite/lib/libcrypt/crypt.c which is not thread safe.
 * HP-UX : use crypt
 * AIX : crypt() is not MT-safe; serialize with a critical section.
 * Other platforms not investigated.
 */
NSAPI_PUBLIC
char *ACL_Crypt(const char *key, const char *salt)
{
    char *res=NULL;
    char *enc=NULL;
#if defined(ACL_CRYPT_USES_CRIT)
    crit_enter(acl_crypt_crit);
#endif
    enc = crypt(key, salt);
    if (enc)
        res = STRDUP(enc);
#if defined(ACL_CRYPT_USES_CRIT)
    crit_exit(acl_crypt_crit);
#endif
    return res;
}

/* ACL_CryptCompare
 *
 * This is a wrapper function for the crypt() function which is not
 * always thread-safe. All calls to crypt() must go through this
 * function or threads may corrupt each others data. This function
 * apart from calling crypt(), also compares the results of the
 * crypt() with the encrypted password supplied.
 *
 * In parameters : see crypt() man page. plus const char * encrypted
 * Out parameters : int
 *
 * Platform notes:
 *
 * Solaris : crypt(3C) is labeled MT-Safe.
 * Linux : crypt(3) is not thread safe; serialize with a critical section.
 * Windows : uses crypt() implemented in
 *     netsite/lib/libcrypt/crypt.c which is not thread safe.
 * HP-UX : use crypt
 * AIX : crypt() is not MT-safe; serialize with a critical section.
 * Other platforms not investigated.
 */
NSAPI_PUBLIC
int ACL_CryptCompare(const char *key, const char *salt, const char *encrypted)
{
    char *enpass = NULL;
    int rv = -1;
    if (!key || !salt || !encrypted)
        return -1;
#if defined(ACL_CRYPT_USES_CRIT)
    crit_enter(acl_crypt_crit);
#endif
    enpass = crypt(key, salt);
    if (enpass)
        rv = strcmp(encrypted, enpass);
#if defined(ACL_CRYPT_USES_CRIT)
    crit_exit(acl_crypt_crit);
#endif
    return rv;
}
