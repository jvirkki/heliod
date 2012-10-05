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


#ifndef ACL_AUTH_H
#define ACL_AUTH_H

#include <ldap.h>
#include <base/plist.h>
#include <libaccess/nserror.h>

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC extern int parse_ldap_url (NSErr_t *errp, ACLDbType_t dbtype,
					const char *name, const char *url,
					PList_t plist, void **db);

extern int get_is_valid_password_ldap (NSErr_t *errp,
                                         PList_t subject,
                                         PList_t resource,
                                         PList_t auth_info,
                                         PList_t global_auth,
                                         void *arg);

extern int get_user_ismember_ldap (NSErr_t *errp,
				   PList_t subject,
				   PList_t resource,
				   PList_t auth_info,
				   PList_t global_auth,
				   void *arg);

extern int get_cert2group_ldap (NSErr_t *errp,
				PList_t subject,
				PList_t resource,
				PList_t auth_info,
				PList_t global_auth,
				void *arg);

extern int get_userdn_ldap (NSErr_t *errp,
			    PList_t subject,
			    PList_t resource,
			    PList_t auth_info,
			    PList_t global_auth,
			    void *arg);

extern int get_user_isinrole_ldap (NSErr_t *errp,
				   PList_t subject,
				   PList_t resource,
				   PList_t auth_info,
				   PList_t global_auth,
				   void *arg);

extern int ACL_NeedLDAPOverSSL();

extern int acl_map_cert_to_user_ldap (NSErr_t *errp, const char *dbname,
				 void *handle, void *cert,
				 PList_t resource, pool_handle_t *pool,
				 char **user, char **userdn, char *certmap);

extern int get_user_exists_ldap (NSErr_t *errp, PList_t subject,
				 PList_t resource, PList_t auth_info,
				 PList_t global_auth, void *unused);

NSAPI_PUBLIC extern int acl_get_default_ldap_db (NSErr_t *errp, void **db);

NSAPI_PUBLIC int ACL_LDAPDatabaseHandle (NSErr_t *errp,
                                         const char *dbname, LDAP **ld,
					 char **basedn);

NSAPI_PUBLIC int ACL_LDAPSessionAllocate(NSErr_t *errp,
                                         const char *dbname, LDAP **ld,
                                         const char **basedn);

NSAPI_PUBLIC void ACL_LDAPSessionFree(LDAP *ld);

NSPR_END_EXTERN_C

#endif /* ACL_AUTH_H */
