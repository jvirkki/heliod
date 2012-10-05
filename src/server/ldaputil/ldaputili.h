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

#ifndef _LDAPU_LDAPUTILI_H
#define _LDAPU_LDAPUTILI_H

#include <ldaputil/ldaputil.h>

#include <nspr.h>
#include <seccomon.h>
#include <ssl.h>

#define BIG_LINE 1024

extern const int SEC_OID_AVA_UNKNOWN; /* unknown OID */

#ifdef __cplusplus
extern "C" {
#endif

SECStatus CERT_RFC1485_EscapeAndQuote (char *dst, int dstlen, char *src, int srclen);

extern int ldapu_get_cert_ava_val (void *cert_in, int which_dn,
				   const char *attr, char ***val_out);
NSAPI_PUBLIC int ldapu_get_first_cert_extension (void *cert_in, void **handle,
				char **extension_type,char ** extension_value);
NSAPI_PUBLIC int ldapu_get_next_cert_extension (void *handle,
				char **extension_type,char ** extension_value);
NSAPI_PUBLIC int ldapu_cert_extension_done (void *handle);
NSAPI_PUBLIC int ldapu_get_cert_start_date (void *cert_in, char **val_out);
NSAPI_PUBLIC int ldapu_get_cert_end_date (void *cert_in, char **val_out);
NSAPI_PUBLIC int ldapu_get_cert_algorithm (void *cert_in, char **val_out);

extern int ldapu_member_certificate_match (void* cert, const char* desc);

/* Each of several LDAP API functions has a counterpart here.
 * They behave the same, but their implementation may be replaced
 * by calling ldapu_VTable_set(); as Directory Server does.
 */
#ifdef USE_LDAP_SSL
extern LDAP*  ldapu_ssl_init( const char *host, int port, int encrypted );
#else
extern LDAP*  ldapu_init    ( const char *host, int port );
#endif
extern int    ldapu_set_option( LDAP *ld, int opt, void *val );
extern int    ldapu_simple_bind_s( LDAP* ld, const char *username, const char *passwd );
extern int    ldapu_unbind( LDAP *ld );
extern int    ldapu_search_s( LDAP *ld, const char *base, int scope,
		              const char *filter, char **attrs, int attrsonly, LDAPMessage **res );
extern int    ldapu_count_entries( LDAP *ld, LDAPMessage *chain );
extern LDAPMessage* ldapu_first_entry( LDAP *ld, LDAPMessage *chain );
extern LDAPMessage* ldapu_next_entry( LDAP *ld, LDAPMessage *entry );
extern int    ldapu_msgfree( LDAP *ld, LDAPMessage *chain );
extern char*  ldapu_get_dn( LDAP *ld, LDAPMessage *entry );
extern void   ldapu_memfree( LDAP *ld, void *dn );
extern char*  ldapu_first_attribute( LDAP *ld, LDAPMessage *entry, BerElement **ber );
extern char*  ldapu_next_attribute( LDAP *ld, LDAPMessage *entry, BerElement *ber );
extern void   ldapu_ber_free( LDAP *ld, BerElement *ber, int freebuf );
extern char** ldapu_get_values( LDAP *ld, LDAPMessage *entry, const char *target );
extern struct berval** ldapu_get_values_len( LDAP *ld, LDAPMessage *entry, const char *target );
extern void   ldapu_value_free( LDAP *ld, char **vals );
extern void   ldapu_value_free_len( LDAP *ld, struct berval **vals );

/* str.cpp */
extern int    ldapu_str_append(LDAPUStr_t *lstr, const char *arg);
extern LDAPUStr_t *ldapu_str_alloc (const int size);
extern void   ldapu_str_free (LDAPUStr_t *lstr);

#ifdef __cplusplus
}
#endif
#endif
