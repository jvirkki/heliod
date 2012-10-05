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

#ifndef _LDAPU_CERTMAP_H
#define _LDAPU_CERTMAP_H

#ifndef INTLDAPU
#define INTLDAPU
#endif /* INTLDAPU */

#include "extcmap.h"

typedef struct {
    char *str;
    int size;
    int len;
} LDAPUStr_t;

#ifdef __cplusplus
extern "C" {
#endif

NSAPI_PUBLIC int ldapu_cert_to_ldap_entry (void *cert, LDAP *ld,
					   const char *basedn,
					   LDAPMessage **res);

NSAPI_PUBLIC int ldapu_cert_to_ldap_entry_with_certmap (void *cert, LDAP *ld,
					   const char *basedn,
					   LDAPMessage **res, char *certmap);

NSAPI_PUBLIC int ldapu_set_cert_mapfn (const char *issuerDN,
				       CertMapFn_t mapfn);


NSAPI_PUBLIC CertMapFn_t ldapu_get_cert_mapfn (const char *issuerDN);

NSAPI_PUBLIC int ldapu_set_cert_searchfn (const char *issuerDN,
					  CertSearchFn_t searchfn);


NSAPI_PUBLIC CertSearchFn_t ldapu_get_cert_searchfn (const char *issuerDN);

NSAPI_PUBLIC int ldapu_set_cert_verifyfn (const char *issuerDN,
					  CertVerifyFn_t verifyFn);

NSAPI_PUBLIC CertVerifyFn_t ldapu_get_cert_verifyfn (const char *issuerDN);


NSAPI_PUBLIC int ldapu_get_cert_subject_dn (void *cert, char **subjectDN);


NSAPI_PUBLIC int ldapu_get_cert_issuer_dn (void *cert, char **issuerDN);


NSAPI_PUBLIC int ldapu_get_cert_ava_val (void *cert, int which_dn,
					 const char *attr, char ***val);

NSAPI_PUBLIC int ldapu_get_first_cert_extension (void *cert_in, void **handle,
				char **extension_type,char ** extension_value);

NSAPI_PUBLIC int ldapu_get_next_cert_extension (void *handle,
				char **extension_type,char ** extension_value);

NSAPI_PUBLIC int ldapu_cert_extension_done (void *handle);

NSAPI_PUBLIC int ldapu_get_cert_start_date (void *cert_in, char **val_out);

NSAPI_PUBLIC int ldapu_get_cert_end_date (void *cert_in, char **val_out);

NSAPI_PUBLIC int ldapu_get_cert_algorithm (void *cert_in, char **val_out);


NSAPI_PUBLIC int ldapu_free_cert_ava_val (char **val);


NSAPI_PUBLIC int ldapu_get_cert_der (void *cert, unsigned char **derCert,
				     unsigned int *len);


NSAPI_PUBLIC int ldapu_issuer_certinfo (const char *issuerDN,
					void **certmap_info);

NSAPI_PUBLIC int ldapu_name_certinfo (const char *name,
					void **certmap_info);

NSAPI_PUBLIC int ldapu_certmap_info_attrval (void *certmap_info,
					     const char *attr, char **val);


NSAPI_PUBLIC char *ldapu_err2string (int err);

/* Keep the old fn for backward compatibility */
NSAPI_PUBLIC void ldapu_free_old (char *ptr);


NSAPI_PUBLIC void *ldapu_malloc (int size);


NSAPI_PUBLIC char *ldapu_strdup (const char *ptr);


NSAPI_PUBLIC void *ldapu_realloc (void *ptr, int size);


NSAPI_PUBLIC void ldapu_free (void *ptr);


NSAPI_PUBLIC int ldaputil_exit ();

#ifdef __cplusplus
}
#endif

#endif /* _LDAPU_CERTMAP_H */
