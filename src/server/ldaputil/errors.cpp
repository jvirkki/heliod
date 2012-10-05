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


#include <ldaputil/errors.h>
#include <ldaputil/certmap.h>
#include "base/ereport.h"

NSAPI_PUBLIC char *ldapu_err2string(int err)
{
    const char *rv;

    switch(err) {

	/* Error codes defined in certmap.h */
    case LDAPU_SUCCESS:
	rv = "success";
	break;
    case LDAPU_FAILED:
	rv = "ldap search didn't find an ldap entry";
	break;
    case LDAPU_CERT_MAP_FUNCTION_FAILED:
	rv = "Cert mapping function failed";
	break;
    case LDAPU_CERT_SEARCH_FUNCTION_FAILED:
	rv = "Cert search function failed";
	break;
    case LDAPU_CERT_VERIFY_FUNCTION_FAILED:
	rv = "Cert verify function failed";
	break;
    case LDAPU_CERT_MAP_INITFN_FAILED:
	rv = "Certmap InitFn function failed";
	break;


	/* Error codes returned by ldapdb.c */
    case LDAPU_ERR_URL_INVALID_PREFIX:
	rv = "invalid local ldap database url prefix -- must be ldapdb://";
	break;
    case LDAPU_ERR_URL_NO_BASEDN:
	rv = "base dn is missing in ldapdb url";
	break;
    case LDAPU_ERR_OUT_OF_MEMORY:
	rv = "out of memory";
	break;
    case LDAPU_ERR_LDAP_INIT_FAILED:
	rv = "Couldn't initialize connection to the ldap directory server";
	break;
    case LDAPU_ERR_BIND_FAILED:
	rv = "Couldn't bind to the ldap directory server";
	break;
    case LDAPU_ERR_LCACHE_INIT_FAILED:
	rv = "Couldn't initialize connection to the local ldap directory";
	break;
    case LDAPU_ERR_LDAP_SET_OPTION_FAILED:
	rv = "ldap_set_option failed for local ldap database";
	break;
    case LDAPU_ERR_NO_DEFAULT_CERTDB:
	rv = "default cert database not initialized when using LDAP over SSL";
	break;


	/* Errors returned by ldapauth.c */
    case LDAPU_ERR_CIRCULAR_GROUPS:
	rv = "Circular groups were detected during group membership check";
	break;
    case LDAPU_ERR_URL_PARSE_FAILED:
	rv = "Invalid member URL";
	break;
    case LDAPU_ERR_INVALID_STRING:
	rv = "Invalid string";
	break;
    case LDAPU_ERR_INVALID_STRING_INDEX:
	rv = "Invalid string index";
	break;
    case LDAPU_ERR_MISSING_ATTR_VAL:
	rv = "Missing attribute value from the search result";
	break;


	/* Errors returned by dbconf.c */
    case LDAPU_ERR_CANNOT_OPEN_FILE:
	rv = "cannot open the config file";
	break;
    case LDAPU_ERR_DBNAME_IS_MISSING:
	rv = "database name is missing";
	break;
    case LDAPU_ERR_PROP_IS_MISSING:
	rv = "database property is missing";
	break;
    case LDAPU_ERR_DIRECTIVE_IS_MISSING:
	rv = "illegal directive in the config file";
	break;
    case LDAPU_ERR_NOT_PROPVAL:
	rv = "internal error - LDAPU_ERR_NOT_PROPVAL";
	break;


	/* Error codes returned by certmap.c */
    case LDAPU_ERR_NO_ISSUERDN_IN_CERT:
	rv = "cannot extract issuer DN from the cert";
	break;
    case LDAPU_ERR_NO_ISSUERDN_IN_CONFIG_FILE:
	rv = "issuer DN missing for non-default certmap";
	break;
    case LDAPU_ERR_CERTMAP_INFO_MISSING:
	rv = "cert to ldap entry mapping information is missing";
	break;
    case LDAPU_ERR_MALFORMED_SUBJECT_DN:
	rv = "Found malformed subject DN in the certificate";
	break;
    case LDAPU_ERR_MAPPED_ENTRY_NOT_FOUND:
	rv = "Certificate couldn't be mapped to an ldap entry";
	break;
    case LDAPU_ERR_UNABLE_TO_LOAD_PLUGIN:
	rv = "Unable to load certmap plugin library";
	break;
    case LDAPU_ERR_MISSING_INIT_FN_IN_CONFIG:
	rv = "InitFn must be provided when using certmap plugin library";
	break;
    case LDAPU_ERR_MISSING_INIT_FN_IN_LIB:
	rv = "Could not find InitFn in the certmap plugin library";
	break;
    case LDAPU_ERR_CERT_VERIFY_FAILED:
	rv = "Could not matching certificate in User's LDAP entry";
	break;
    case LDAPU_ERR_CERT_VERIFY_NO_CERTS:
	rv = "User's LDAP entry doesn't have any certificates to compare";
	break;
    case LDAPU_ERR_MISSING_LIBNAME:
	rv = "Library name is missing in the config file";
	break;
    case LDAPU_ERR_MISSING_INIT_FN_NAME:
	rv = "Init function name is missing in the config file";
	break;
    case LDAPU_ERR_WRONG_ARGS:
	rv = "ldaputil API function called with wrong arguments";
	break;
    case LDAPU_ERR_RENAME_FILE_FAILED:
	rv = "Renaming of file failed";
	break;
    case LDAPU_ERR_MISSING_VERIFYCERT_VAL:
	rv = "VerifyCert property value must be on or off";
	break;
    case LDAPU_ERR_CANAME_IS_MISSING:
	rv = "Cert issuer name is missing";
	break;
    case LDAPU_ERR_CAPROP_IS_MISSING:
	rv = "property name is missing";
	break;
    case LDAPU_ERR_UNKNOWN_CERT_ATTR:
	rv = "unknown cert attribute";
	break;


    case LDAPU_ERR_EMPTY_LDAP_RESULT:
	rv = "ldap search returned empty result";
	break;
    case LDAPU_ERR_MULTIPLE_MATCHES:
	rv = "ldap search returned multiple matches when one expected";
	break;
    case LDAPU_ERR_MISSING_RES_ENTRY:
	rv = "Could not extract entry from the ldap search result";
	break;
    case LDAPU_ERR_MISSING_UID_ATTR:
	rv = "ldap entry is missing the 'uid' attribute value";
	break;
    case LDAPU_ERR_INVALID_ARGUMENT:
	rv = "invalid argument passed to the certmap API function";
	break;
    case LDAPU_ERR_INVALID_SUFFIX:
	rv = "invalid LDAP directory suffix";
	break;


	/* Error codes returned by cert.c */
    case LDAPU_ERR_EXTRACT_SUBJECTDN_FAILED:
	rv = "Couldn't extract the subject DN from the certificate";
	break;
    case LDAPU_ERR_EXTRACT_ISSUERDN_FAILED:
	rv = "Couldn't extract the issuer DN from the certificate";
	break;
    case LDAPU_ERR_EXTRACT_DERCERT_FAILED:
	rv = "Couldn't extract the original DER encoding from the certificate";
	break;

	/* Error codes returned LdapSession.cpp */
    case LDAPU_ERR_DOMAIN_NOT_ACTIVE:
	rv = "The domain is not active";
	break;
    case LDAPU_ERR_USER_NOT_ACTIVE:
	rv = "The user is not active";
	break;

    case LDAPU_ERR_NOT_IMPLEMENTED:
	rv = "function not implemented yet";
	break;
    case LDAPU_ERR_INTERNAL:
	rv = "ldaputil internal error";
	break;

    default:
	if (err > 0) {
	    /* LDAP errors are +ve */
	    rv = ldap_err2string(err);
	}
	else {
	    rv = system_errmsg();
	}
	break;
    }

    return (char*)rv; // jpierre : may have to reinvestigate this cast
}
