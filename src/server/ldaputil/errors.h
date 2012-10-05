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

#ifndef _LDAPU_ERRORS_H
#define _LDAPU_ERRORS_H

#ifndef NSAPI_PUBLIC
#ifdef XP_WIN32
#define NSAPI_PUBLIC __declspec(dllexport)
#else
#define NSAPI_PUBLIC 
#endif
#endif

#ifdef DBG_PRINT
#include <stdio.h>
#define DBG_PRINT1(x) fprintf(stderr, x)
#define DBG_PRINT2(x,y) fprintf(stderr, x, y)
#define DBG_PRINT3(x,y,z) fprintf(stderr, x, y, z)
#define DBG_PRINT4(x,y,z,a) fprintf(stderr, x, y, z, a)
#else
#define DBG_PRINT1(x) 
#define DBG_PRINT2(x,y) 
#define DBG_PRINT3(x,y,z) 
#define DBG_PRINT4(x,y,z,a) 
#endif

/* Common error codes */
#define LDAPU_ERR_NOT_IMPLEMENTED	     -1000
#define LDAPU_ERR_INTERNAL		     -1001
#define LDAPU_ERR_INVALID		     -1002

/* see also extcmap.h */
#define LDAPU_SUCCESS	                      0
#define LDAPU_FAILED                          -1

#define LDAPU_CERT_MAP_FUNCTION_FAILED        -2
#define LDAPU_CERT_SEARCH_FUNCTION_FAILED     -3
#define LDAPU_CERT_VERIFY_FUNCTION_FAILED     -4
#define LDAPU_CERT_MAP_INITFN_FAILED          -5

/* Error codes returned by ldapdb.c */
#define LDAPU_ERR_OUT_OF_MEMORY		     -110
#define LDAPU_ERR_URL_INVALID_PREFIX	     -112
#define LDAPU_ERR_URL_NO_BASEDN		     -113
#define LDAPU_ERR_URL_PARSE_FAILED	     -114
#define LDAPU_ERR_NO_SERVERNAME 	     -115
    
#define LDAPU_ERR_LDAP_INIT_FAILED	     -120
#define LDAPU_ERR_LCACHE_INIT_FAILED	     -121 
#define LDAPU_ERR_LDAP_SET_OPTION_FAILED     -122 
#define LDAPU_ERR_NO_DEFAULT_CERTDB          -123
#define LDAPU_ERR_BIND_FAILED                -124

/* Errors returned by dbconf.c */
#define LDAPU_ERR_CANNOT_OPEN_FILE	     -141
#define LDAPU_ERR_DBNAME_IS_MISSING	     -142
#define LDAPU_ERR_PROP_IS_MISSING	     -143
#define LDAPU_ERR_DIRECTIVE_IS_MISSING	     -145
#define LDAPU_ERR_NOT_PROPVAL		     -146
#define LDAPU_ATTR_NOT_FOUND		     -147

/* Error codes returned by certmap.c */
#define LDAPU_ERR_NO_ISSUERDN_IN_CERT	     -181
#define LDAPU_ERR_NO_ISSUERDN_IN_CONFIG_FILE -182
#define LDAPU_ERR_CERTMAP_INFO_MISSING	     -183
#define LDAPU_ERR_MALFORMED_SUBJECT_DN	     -184
#define LDAPU_ERR_MAPPED_ENTRY_NOT_FOUND     -185
#define LDAPU_ERR_UNABLE_TO_LOAD_PLUGIN	     -186
#define LDAPU_ERR_MISSING_INIT_FN_IN_LIB     -187
#define LDAPU_ERR_MISSING_INIT_FN_IN_CONFIG  -188
#define LDAPU_ERR_CERT_VERIFY_FAILED	     -189
#define LDAPU_ERR_CERT_VERIFY_NO_CERTS	     -190
#define LDAPU_ERR_MISSING_LIBNAME	     -191
#define LDAPU_ERR_MISSING_INIT_FN_NAME	     -192

#define LDAPU_ERR_EMPTY_LDAP_RESULT	     -193
#define LDAPU_ERR_MULTIPLE_MATCHES	     -194
#define LDAPU_ERR_MISSING_RES_ENTRY	     -195
#define LDAPU_ERR_MISSING_UID_ATTR	     -196
#define LDAPU_ERR_WRONG_ARGS		     -197
#define LDAPU_ERR_RENAME_FILE_FAILED	     -198

#define LDAPU_ERR_MISSING_VERIFYCERT_VAL     -199
#define LDAPU_ERR_CANAME_IS_MISSING	     -200
#define LDAPU_ERR_CAPROP_IS_MISSING	     -201
#define LDAPU_ERR_UNKNOWN_CERT_ATTR	     -202
#define LDAPU_ERR_INVALID_ARGUMENT	     -203
#define LDAPU_ERR_INVALID_SUFFIX	     -204

/* Error codes returned by cert.c */
#define LDAPU_ERR_EXTRACT_SUBJECTDN_FAILED   -300
#define LDAPU_ERR_EXTRACT_ISSUERDN_FAILED    -301
#define LDAPU_ERR_EXTRACT_DERCERT_FAILED     -302

/* Error codes returned by ldapauth.c */
#define LDAPU_ERR_CIRCULAR_GROUPS	     -400
#define LDAPU_ERR_INVALID_STRING	     -401
#define LDAPU_ERR_INVALID_STRING_INDEX	     -402
#define LDAPU_ERR_MISSING_ATTR_VAL	     -403
#define LDAPU_ERR_PASSWORD_EXPIRED           -404
#define LDAPU_ERR_PASSWORD_EXPIRING          -405

/* Error codes returned by LdapSession.cpp */
#define LDAPU_ERR_DOMAIN_NOT_ACTIVE          -500
#define LDAPU_ERR_USER_NOT_ACTIVE            -501

#ifdef __cplusplus
extern "C" {
#endif

    /* NSAPI_PUBLIC extern char *ldapu_err2string(int err); */

#ifdef __cplusplus
}
#endif

#endif /* LDAPUTIL_LDAPU_H */
