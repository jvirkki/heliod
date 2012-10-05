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

#ifndef _LDAPU_LDAPUTIL_H
#define _LDAPU_LDAPUTIL_H

#include <ldaputil/dbconf.h>
#include <ldaputil/certmap.h>
#include <ldaputil/list.h>

typedef struct {
    char *prop;			/* property name */
    char *val;			/* value -- only char* supported for now */
} LDAPUPropVal_t;

typedef LDAPUList_t LDAPUPropValList_t;

enum {
    COMPS_COMMENTED_OUT,
    COMPS_EMPTY,
    COMPS_HAS_ATTRS
};

typedef struct {
    char *issuerName;		  /* issuer (symbolic/short) name */
    char *issuerDN;		  /* cert issuer's DN */
    LDAPUPropValList_t *propval;  /* pointer to the prop-val pairs list */
    CertMapFn_t mapfn;		  /* cert to ldapdn & filter mapping func */
    CertVerifyFn_t verifyfn;	  /* verify cert function */
    CertSearchFn_t searchfn;	  /* search ldap entry function */
    long dncomps;		  /* bitmask: components to form ldap dn */
    long filtercomps;		  /* components used to form ldap filter */
    int verifyCert;		  /* Verify the cert? */
    char *searchAttr;		  /* LDAP attr used by the search fn */
    int dncompsState;		  /* Empty, commented out, or attr names */
    int filtercompsState;	  /* Empty, commented out, or attr names */
} LDAPUCertMapInfo_t;

typedef LDAPUList_t LDAPUCertMapListInfo_t;

#ifdef __cplusplus
extern "C" {
#endif

extern int certmap_read_default_certinfo (const char *file);

extern int certmap_read_certconfig_file (const char *file);

extern void ldapu_certinfo_free (void *certmap_info);

extern void ldapu_certmap_listinfo_free (void *certmap_listinfo);

extern void ldapu_propval_list_free (void *propval_list);

NSAPI_PUBLIC extern int ldaputil_init (const char *config_file,
				       const char *dllname,
				       const char *serv_root,
				       const char *serv_type,
				       const char *serv_id);

NSAPI_PUBLIC extern int ldaputil_exit ();

NSAPI_PUBLIC extern int ldapu_cert_to_user (void *cert, LDAP *ld,
					    const char *basedn,
					    LDAPMessage **res,
					    char **user);

NSAPI_PUBLIC extern int ldapu_certmap_init (const char *config_file,
					    const char *libname,
					    LDAPUCertMapListInfo_t **certmap_list,
					    LDAPUCertMapInfo_t
					    **certmap_default);

NSAPI_PUBLIC extern void ldapu_certmap_exit ();

NSAPI_PUBLIC extern int ldapu_certinfo_modify (const char *issuerName,
					       const char *issuerDN,
					       const LDAPUPropValList_t *propval);

NSAPI_PUBLIC extern int ldapu_certinfo_delete (const char *issuerDN);

NSAPI_PUBLIC extern int ldapu_certinfo_save (const char *fname,
					     const char *old_fname,
					     const char *tmp_fname);

NSAPI_PUBLIC extern int ldapu_propval_alloc (const char *prop, const char *val,
					     LDAPUPropVal_t **propval);

/* LdapUtil.cpp */
NSAPI_PUBLIC extern int ldapdn_normalize( char *dn );
NSAPI_PUBLIC extern int ldapdn_issuffix(const char *dn, const char *suffix);

/* Keep the old fn for backward compatibility */
NSAPI_PUBLIC void ldapu_free_old (char *ptr);

NSAPI_PUBLIC void *ldapu_malloc (int size);

NSAPI_PUBLIC char *ldapu_strdup (const char *ptr);

NSAPI_PUBLIC void *ldapu_realloc (void *ptr, int size);

NSAPI_PUBLIC void ldapu_free (void *ptr);

extern int ldapu_find (LDAP *ld, const char *base, int scope,
		       const char *filter, const char **attrs,
		       int attrsonly, LDAPMessage **res);

#ifndef DONT_USE_LDAP_SSL
#ifndef USE_LDAP_SSL
#define USE_LDAP_SSL
#endif
#endif

typedef struct {
#ifdef USE_LDAP_SSL
    LDAP*       (LDAP_CALL LDAP_CALLBACK *ldapuV_ssl_init)         ( const char*, int, int );
#else
    LDAP*       (LDAP_CALL LDAP_CALLBACK *ldapuV_init)             ( const char*, int );
#endif
    int         (LDAP_CALL LDAP_CALLBACK *ldapuV_set_option)       ( LDAP*, int, const void* );
    int         (LDAP_CALL LDAP_CALLBACK *ldapuV_simple_bind_s)    ( LDAP*, const char*, const char* );
    int         (LDAP_CALL LDAP_CALLBACK *ldapuV_unbind)           ( LDAP* );
    int         (LDAP_CALL LDAP_CALLBACK *ldapuV_search_s)         ( LDAP*, const char*, int, const char*, char**, int, LDAPMessage** );
    int         (LDAP_CALL LDAP_CALLBACK *ldapuV_count_entries)    ( LDAP*, LDAPMessage* );
    LDAPMessage*(LDAP_CALL LDAP_CALLBACK *ldapuV_first_entry)      ( LDAP*, LDAPMessage* );
    LDAPMessage*(LDAP_CALL LDAP_CALLBACK *ldapuV_next_entry)       ( LDAP*, LDAPMessage* );
    int         (LDAP_CALL LDAP_CALLBACK *ldapuV_msgfree)          ( LDAP*, LDAPMessage* );
    char*       (LDAP_CALL LDAP_CALLBACK *ldapuV_get_dn)           ( LDAP*, LDAPMessage* );
    void        (LDAP_CALL LDAP_CALLBACK *ldapuV_memfree)          ( LDAP*, void* );
    char*       (LDAP_CALL LDAP_CALLBACK *ldapuV_first_attribute)  ( LDAP*, LDAPMessage*, BerElement** );
    char*       (LDAP_CALL LDAP_CALLBACK *ldapuV_next_attribute)   ( LDAP*, LDAPMessage*, BerElement* );
    void        (LDAP_CALL LDAP_CALLBACK *ldapuV_ber_free)         ( LDAP*, BerElement*, int );
    char**      (LDAP_CALL LDAP_CALLBACK *ldapuV_get_values)       ( LDAP*, LDAPMessage*, const char* );
    void        (LDAP_CALL LDAP_CALLBACK *ldapuV_value_free)       ( LDAP*, char** );
    struct berval**(LDAP_CALL LDAP_CALLBACK *ldapuV_get_values_len)( LDAP*, LDAPMessage*, const char* );
    void           (LDAP_CALL LDAP_CALLBACK *ldapuV_value_free_len)( LDAP*, struct berval** );
} LDAPUVTable_t;

NSAPI_PUBLIC extern void ldapu_VTable_set (LDAPUVTable_t*);

#ifdef __cplusplus
}
#endif

#endif /* _LDAPU_LDAPUTIL_H */
