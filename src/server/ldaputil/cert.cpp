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
#include <malloc.h>
#include "base/systems.h" /* Pick up some defines so this stupid NSS headers will work */
#include "prmem.h"
#include "key.h"
#include "cert.h"
#include "secoid.h"
#include <ldaputil/certmap.h>
#include <ldaputil/errors.h>
#include "secder.h"
#include "ldaputili.h"

//
// ldapu_get_cert_subject_dn - get a certificate's subject DN
//
NSAPI_PUBLIC int
ldapu_get_cert_subject_dn (void *cert_in, char **subjectDN)
{
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    char *cert_subject = cert ? CERT_NameToAscii(&cert->subject) : 0;

    *subjectDN = cert_subject ? strdup(cert_subject) : 0;
    PR_Free(cert_subject);
    return *subjectDN ? LDAPU_SUCCESS : LDAPU_ERR_EXTRACT_SUBJECTDN_FAILED;
}

//
// ldapu_get_cert_issuer_dn - get a certificate's issuer DN
//
NSAPI_PUBLIC int
ldapu_get_cert_issuer_dn (void *cert_in, char **issuerDN)
{
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    char *cert_issuer = cert ? CERT_NameToAscii(&cert->issuer) : 0;

    *issuerDN = cert_issuer ? strdup(cert_issuer) : 0;
    PR_Free(cert_issuer);

    return *issuerDN ? LDAPU_SUCCESS : LDAPU_ERR_EXTRACT_ISSUERDN_FAILED;
}

//
// ldapu_get_cert_der - get a certificate's DER encoding
//
NSAPI_PUBLIC int
ldapu_get_cert_der (void *cert_in, unsigned char **der, unsigned int *len)
{
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    SECItem derCert = ((CERTCertificate*)cert)->derCert;
    unsigned char *data = derCert.data;

    *len = derCert.len;
    *der = (unsigned char *)malloc(*len);

    if (!*der) return LDAPU_ERR_OUT_OF_MEMORY;

    memcpy(*der, data, *len);

    return *len ? LDAPU_SUCCESS : LDAPU_ERR_EXTRACT_DERCERT_FAILED;
}


/*
 * Most of the functions in this list look at internal CERT parameters,
 * including this one. These will all have to change to accessor functions
 * in NSS 4.0 (if it will ever come - chrisk). rjr
 */

static char *
ldapu_mk_oid_string(SECItem *oid) {
    char *string, *cp;
    int i;

    string = (char *)malloc((oid->len*4) + 1);
    if (string == NULL) return NULL;
    cp = string;

    for (i=0; i < oid->len; i++) {
	unsigned char val = oid->data[i];
	*cp++ = '_';
	if (val >= 100) {
	   *cp++ = val/100 + '0';
	   val = val % 100;
	}
	if (val >= 10) {
	   *cp++ = val/10 + '0';
	   val = val % 10;
	}
	*cp++ = val + '0';
   }
   *cp = 0;

   return string;
}

#define hex(x) ((x) + ((x) >= 10) ? ('a' - 10) : '0')

static char *
ldapu_mk_ext_string(SECItem *value) {
    char *string, *cp;
    int i;

    string = (char *)malloc((value->len*3) + 1);
    if (string == NULL) return NULL;
    cp = string;

    for (i=0; i < value->len; i++) {
	unsigned char val = value->data[i];
	*cp++ = hex(val >> 8);
	*cp++ = hex(val & 0xff);
	*cp++ = ' ';
   }
   *cp = 0;

   return string;
}

    
typedef struct _ldapuExtHandle {
    CERTCertExtension **ext;
    int index;
} ldapuExtHandle;

//
// ldapu_get_first_cert_extension - return the first certificate extension.
//
NSAPI_PUBLIC int
ldapu_get_first_cert_extension(void *cert_in, void **handlePtr, char **extension, char **value)
{
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    CERTCertExtension **ext = cert->extensions;
    ldapuExtHandle *handle;

    *handlePtr = *extension = *value = NULL;
    if (ext == NULL) return LDAPU_FAILED;
    if (ext[0] == NULL) return LDAPU_FAILED;

    handle = (ldapuExtHandle *)malloc(sizeof(ldapuExtHandle));
    if (handle == NULL) return LDAPU_ERR_OUT_OF_MEMORY;
    handle->ext = ext;
    handle->index = 1;

    *extension = ldapu_mk_oid_string(&ext[0]->id);
    *value = ldapu_mk_ext_string(&ext[0]->value);
    *handlePtr = handle;

    if ((*extension == NULL) || (*value == NULL)) {
	ldapu_cert_extension_done(handle);
	if (*extension) free(*extension);
	if (*value) free(*value);
	*extension = *value = (char *)NULL;
	*handlePtr = NULL;
	return LDAPU_ERR_OUT_OF_MEMORY;
    }
    return LDAPU_SUCCESS;
}

//
// ldapu_get_next_cert_extension - return the next certificate extension.
//
NSAPI_PUBLIC int
ldapu_get_next_cert_extension(void *handlePtr, char **extension, char **value)
{
    ldapuExtHandle *handle = (ldapuExtHandle *)handlePtr;
    CERTCertExtension *ext;
    *extension = *value = NULL;

    ext = handle->ext[handle->index];
    if (ext == NULL) return LDAPU_FAILED; /* really should be 'done; */

    *extension = ldapu_mk_oid_string(&ext->id);
    *value = ldapu_mk_ext_string(&ext->value);
    if ((*extension == NULL) || (*value == NULL)) {
	if (*extension) free(*extension);
	if (*value) free(*value);
	*extension = *value = NULL;
	return LDAPU_ERR_OUT_OF_MEMORY;
    }
    handle->index ++;
    return LDAPU_SUCCESS;
}

//
// ldapu_cert_extension_done - free the iterator handle
//
NSAPI_PUBLIC int
ldapu_cert_extension_done(void *handlePtr)
{
    ldapuExtHandle *handle = (ldapuExtHandle *)handlePtr;

    free(handle);
    return LDAPU_SUCCESS;
}

/* get the starting validity date as a string */
NSAPI_PUBLIC int ldapu_get_cert_start_date(void *cert_in, char **date) {
    CERTCertificate *cert = (CERTCertificate *)cert_in;

     *date = DER_UTCTimeToAscii(&cert->validity.notBefore);
     return *date ? LDAPU_SUCCESS : LDAPU_FAILED;
}

/* get the ending validity date as a string */
NSAPI_PUBLIC int ldapu_get_cert_end_date(void *cert_in, char **date) {
    CERTCertificate *cert = (CERTCertificate *)cert_in;

     *date = DER_UTCTimeToAscii(&cert->validity.notAfter);
     return *date ? LDAPU_SUCCESS : LDAPU_FAILED;
}

/* get the Certificate type (based on the public key algorithm id) */
NSAPI_PUBLIC int ldapu_get_cert_algorithm(void *cert_in, char **alg) {
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    const char *string;
    SECOidTag tag;

    *alg = NULL;
    tag = SECOID_GetAlgorithmTag(&cert->subjectPublicKeyInfo.algorithm);
    string = SECOID_FindOIDTagDescription(tag);

    if (string == NULL) return LDAPU_FAILED;
    *alg = strdup(string);
     return *alg ? LDAPU_SUCCESS : LDAPU_FAILED;
}

static int certmap_name_to_secoid (const char *str)
{
    if (!ldapu_strcasecmp(str, "c")) return SEC_OID_AVA_COUNTRY_NAME;
    if (!ldapu_strcasecmp(str, "o")) return SEC_OID_AVA_ORGANIZATION_NAME;
    if (!ldapu_strcasecmp(str, "cn")) return SEC_OID_AVA_COMMON_NAME;
    if (!ldapu_strcasecmp(str, "l")) return SEC_OID_AVA_LOCALITY;
    if (!ldapu_strcasecmp(str, "st")) return SEC_OID_AVA_STATE_OR_PROVINCE;
    if (!ldapu_strcasecmp(str, "ou")) return SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME;
    if (!ldapu_strcasecmp(str, "dc")) return SEC_OID_AVA_DC;
    if (!ldapu_strcasecmp(str, "uid")) return SEC_OID_RFC1274_UID;
    if (!ldapu_strcasecmp(str, "e")) return SEC_OID_PKCS9_EMAIL_ADDRESS;
    if (!ldapu_strcasecmp(str, "mail")) return SEC_OID_RFC1274_MAIL;

    return SEC_OID_AVA_UNKNOWN;	/* return invalid OID */
}

NSAPI_PUBLIC int ldapu_get_cert_ava_val (void *cert_in, int which_dn,
					 const char *attr, char ***val_out)
{
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    CERTName *cert_dn;
    CERTRDN **rdns;
    CERTRDN **rdn;
    CERTAVA **avas;
    CERTAVA *ava;
    int attr_tag = certmap_name_to_secoid(attr);
    char **val;
    char **ptr;
    int rv;

    *val_out = 0;

    if (attr_tag == SEC_OID_AVA_UNKNOWN) {
	return LDAPU_ERR_INVALID_ARGUMENT;
    }

    if (which_dn == LDAPU_SUBJECT_DN)
	cert_dn = &cert->subject;
    else if (which_dn == LDAPU_ISSUER_DN)
	cert_dn = &cert->issuer;
    else
	return LDAPU_ERR_INVALID_ARGUMENT;

    val = (char **)malloc(32*sizeof(char *));

    if (!val) return LDAPU_ERR_OUT_OF_MEMORY;

    ptr = val;

    rdns = cert_dn->rdns;

    if (rdns) {
	for (rdn = rdns; *rdn; rdn++) {
	    avas = (*rdn)->avas;
	    while ((ava = *avas++) != NULL) {
		int tag = CERT_GetAVATag(ava);

		if (tag == attr_tag) {
		    char buf[BIG_LINE];
		    int lenLen;
		    int vallen;
		    /* Found it */

		    /* Copied from ns/lib/libsec ...
		     * XXX this code is incorrect in general
		     * -- should use a DER template.
		     */
		    lenLen = 2;
		    if (ava->value.len >= 128) lenLen = 3;
		    vallen = ava->value.len - lenLen;

		    rv = CERT_RFC1485_EscapeAndQuote(buf,
						    BIG_LINE,
						    (char*) ava->value.data + lenLen,
						    vallen);

		    if (rv == SECSuccess) {
			*ptr++ = strdup(buf);
		    }
		    break;
		}
	    }
	}
    }

    *ptr = 0;

    if (*val) {
	/* At least one value found */
	*val_out = val;
	rv = LDAPU_SUCCESS;
    }
    else {
	free(val);
	rv = LDAPU_FAILED;
    }

    return rv;
}

static void
_rdns_free (char*** rdns)
{
    auto char*** rdn;
    for (rdn = rdns; *rdn; ++rdn) {
	ldap_value_free (*rdn);
    }
    free (rdns);
}

static char***
_explode_dn (const char* dn)
{
    auto char*** exp = NULL;
    if (dn && *dn) {
	auto char** rdns = ldap_explode_dn (dn, 0);
	if (rdns) {
	    auto size_t expLen = 0;
	    auto char** rdn;
	    for (rdn = rdns; *rdn; ++rdn) {
		auto char** avas = ldap_explode_rdn (*rdn, 0);
		if (avas && *avas) {
		    exp = (char***) ldapu_realloc (exp, sizeof(char**) * (expLen + 2));
		    if (exp) {
			exp[expLen++] = avas;
		    } else {
			ldap_value_free (avas);
			break;
		    }
		} else { /* parse error */
		    if (avas) {
			ldap_value_free (avas);
		    }
		    if (exp) {
			exp[expLen] = NULL;
			_rdns_free (exp);
			exp = NULL;
		    }
		    break;
		}
	    }
	    if (exp) {
		exp[expLen] = NULL;
	    }
	    ldap_value_free (rdns);
	}
    }
    return exp;
}

static size_t
_rdns_count (char*** rdns)
{
    auto size_t count = 0;
    auto char*** rdn;
    for (rdn = rdns; *rdns; ++rdns) {
	auto char** ava;
	for (ava = *rdns; *ava; ++ava) {
	    ++count;
	}
    }
    return count;
}

static int
_replaceAVA (char* attr, char** avas)
{
    if (attr && avas) {
	for (; *avas; ++avas) {
	    if (!ldapu_strcasecmp (*avas, attr)) {
		*avas = attr;
		return 1;
	    }
	}
    }
    return 0;
}

struct _attr_getter_pair {
    char* (*getter) (CERTName* dn);
    const char* name1;
    const char* name2;
} _attr_getter_table[] =
{
    {NULL, "OU", "organizationalUnitName"},
    {CERT_GetOrgName, "O", "organizationName"},
    {CERT_GetCommonName, "CN", "commonName"},
    {CERT_GetCertEmailAddress, "E", NULL},
    {CERT_GetCertEmailAddress, "MAIL", "rfc822mailbox"},
    {CERT_GetCertUid, "uid", NULL},
    {CERT_GetCountryName, "C", "country"},
    {CERT_GetStateName, "ST", "state"},
    {CERT_GetLocalityName, "L", "localityName"},
    {CERT_GetDomainComponentName, "DC", "domainComponent"},
    {NULL, NULL, NULL}
};

static int
_is_OU (const char* attr)
{
    auto struct _attr_getter_pair* descAttr;
    for (descAttr = _attr_getter_table; descAttr->name1; ++descAttr) {
	if (descAttr->getter == NULL) { /* OU attribute */
	    if (!ldapu_strcasecmp (attr, descAttr->name1) || (descAttr->name2 &&
		!ldapu_strcasecmp (attr, descAttr->name2))) {
		return 1;
	    }
	    break;
	}
    }
    return 0;
}

static char**
_previous_OU (char** ava, char** avas)
{
    while (ava != avas) {
	--ava;
	if (_is_OU (*ava)) {
	    return ava;
	}
    }
    return NULL;
}

static char*
_value_normalize (char* value)
    /* Remove leading and trailing spaces, and
       change consecutive spaces to a single space.
    */
{
    auto char* t;
    auto char* f;
    t = f = value;
    while (*f == ' ') ++f; /* ignore leading spaces */
    for (; *f; ++f) {
	if (*f != ' ' || t[-1] != ' ') {
	    *t++ = *f; /* no consecutive spaces */
	}
    }
    if (t > value && t[-1] == ' ') {
	--t; /* ignore trailing space */
    }
    *t = '\0';
    return value;
}

static int
_explode_AVA (char* AVA)
    /* Change an attributeTypeAndValue a la <draft-ietf-asid-ldapv3-dn>,
       to the type name, followed immediately by the attribute value,
       both normalized.
     */
{
    auto char* value = strchr (AVA, '=');
    if (!value) return LDAPU_FAILED;
    *value++ = '\0';
    _value_normalize (AVA);
    _value_normalize (value);
    {
	auto char* typeEnd = AVA + strlen (AVA);
	if ((typeEnd + 1) != value) {
	    memmove (typeEnd+1, value, strlen(value)+1);
	}
    }
    return LDAPU_SUCCESS;
}

static char*
_AVA_value (char* AVA)
{
    return (AVA + strlen (AVA) + 1);
}

static int
_value_match (char* value, char* desc)
{
    auto const int result =
      !ldapu_strcasecmp (_value_normalize(value), desc);
    return result;
}

int
ldapu_member_certificate_match (void* cert, const char* desc)
/*
 *	Return Values: (same as ldapu_find)
 *	    LDAPU_SUCCESS	cert matches desc
 *	    LDAPU_FAILED	cert doesn't match desc
 *	    <rv>		Something went wrong.
 */
{
    auto int err = LDAPU_FAILED;
    auto char*** descRDNs;
    if (!cert || !desc || desc[0] != '{') return LDAPU_FAILED;
    if (desc[1] == '\0') return LDAPU_SUCCESS; /* no AVAs */
    descRDNs = _explode_dn (desc+1);
    if (descRDNs) {
	auto char** descAVAs = (char**)ldapu_malloc(sizeof(char*) * (_rdns_count(descRDNs)+1));
	if (!descAVAs) {
	    err = LDAPU_ERR_OUT_OF_MEMORY;
	} else {
	    auto CERTName* subject = &(((CERTCertificate*)cert)->subject);
	    auto char** descAVA;

	    err = LDAPU_SUCCESS;
	    { /* extract all the AVAs, but not duplicate types, except OU */
		auto size_t descAVAsLen = 0;
		auto char*** descRDN;
		descAVAs[0] = NULL;
		for (descRDN = descRDNs; err == LDAPU_SUCCESS && *descRDN; ++descRDN) {
		    for (descAVA = *descRDN; err == LDAPU_SUCCESS && *descAVA; ++descAVA) {
			err = _explode_AVA (*descAVA);
			if (err == LDAPU_SUCCESS) {
			    if (_is_OU (*descAVA) ||
				!_replaceAVA (*descAVA, descAVAs)) {
				descAVAs[descAVAsLen++] = *descAVA;
				descAVAs[descAVAsLen] = NULL;
			    }
			}
		    }
		}
	    }

	    /* match all the attributes except OU */
	    for (descAVA = descAVAs; err == LDAPU_SUCCESS && *descAVA; ++descAVA) {
		auto struct _attr_getter_pair* descAttr;
		err = LDAPU_FAILED; /* if no match */
		for (descAttr = _attr_getter_table; descAttr->name1; ++descAttr) {
		    if (!ldapu_strcasecmp (*descAVA, descAttr->name1) || (descAttr->name2 &&
			!ldapu_strcasecmp (*descAVA, descAttr->name2))) {
			if (descAttr->getter == NULL) { /* OU attribute */
			    err = LDAPU_SUCCESS; /* for now */
			} else {
			    auto char* certVal = (*(descAttr->getter))(subject);
			    if (certVal && _value_match (certVal, _AVA_value (*descAVA))) {
				err = LDAPU_SUCCESS;
			    }
			    PR_Free (certVal);
			}
			break;
		    }
		}
	    }

	    /* match the OU attributes */
	    if (err == LDAPU_SUCCESS && descAVA != descAVAs) {
		/* Iterate over the OUs in the certificate subject */
		auto CERTRDN** certRDN = subject->rdns;
		descAVA = _previous_OU (descAVA, descAVAs);
		for (; descAVA && *certRDN; ++certRDN) {
		    auto CERTAVA** certAVA = (*certRDN)->avas;
		    for (; descAVA && *certAVA; ++certAVA) {
			auto const int tag = CERT_GetAVATag (*certAVA);
			if (tag == SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME) {
			    auto const size_t certValLen =(*certAVA)->value.len;
			    auto const size_t lenLen = (certValLen < 128) ? 2 : 3;
			    auto const size_t buflen = certValLen - lenLen;
			    auto char* buf = (char*)ldapu_malloc(buflen+1);
			    if (!buf) {
				err = LDAPU_ERR_OUT_OF_MEMORY;
				descAVA = NULL;
			    } else {
				memcpy (buf, (*certAVA)->value.data+lenLen, buflen);
				buf[buflen] = 0;
				if (_value_match (buf, _AVA_value (*descAVA))) {
				    descAVA = _previous_OU (descAVA, descAVAs);
				}
				free (buf);
			    }
			}
		    }
		}
		if (descAVA) {
		    err = LDAPU_FAILED; /* no match for descAVA in subject */
		}
	    }
	    free (descAVAs);
	}
	_rdns_free (descRDNs);
    }
    return err;
}
