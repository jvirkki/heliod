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

#ifndef __LDAPSESSION_H__
#define __LDAPSESSION_H__

#include <ldap.h>
#include "LdapSessionCommon.h"
#include "LdapSearchResult.h"
#include "LdapEntry.h"
#include "LdapDNList.h"
#include <nspr.h>
class LdapRealm;

#ifdef __cplusplus
extern "C" {
#endif
typedef int (*LDAPU_GroupCmpFn_t)(const void *groupids, const char *group, const int len);
#ifdef __cplusplus
}
#endif

class LdapSearchResult;

int parse_memberURL(char *url, char **filter, char **basedn, int *scope);

class LDAPSESSION_PUBLIC LdapSession {
public:
    LdapSession(LdapRealm *realm);
  //LdapSession(LdapServerSet *servers, dyngroupmode_t dyngrmode);
  LdapSearchResult *search(const char *base, int scope, const char *filter, 
		      char **attrs, int attrsonly, int retryCount = 0);
  int compare(const char *dn, char *attr, char * value, int retryCount = 0);


  int usercert_groupids (const char* userDN,
			 void* certificate,
			 void* groupids,
			 LDAPU_GroupCmpFn_t grpcmpfn,
			 const char* baseDN,
			 int recurse,
			 char **group_out);

  int user_roleids (const char* userDN,
                    const char *roles,
                    const char* baseDN,
                    char **role_out);

  int userdn_digest(const char *userdn,
                    const char *nonce,
                    const char *cnonce,
                    const char *user,
                    const char *realm,
                    const char *passattr,
                    const char *alg,
                    const char *noncecount,
                    const char *method,
                    const char *qop,
                    const char *uri,
                    const char *cresponse,
                    char ** response);

  int userdn_password(const char *userdn,
		      const char *password);

  int find_userdn (const char *uid, const char *base, char **dn);  

  int find (const char *base, int scope,
	    const char *filter, const char **attrs,
	    int attrsonly, LdapSearchResult *&res);

  int cert_to_user (void *cert, const char *base, char **user, char **dn, char *certmap, int retryCount);
  int find_vsbasedn(const char *vsname, char *vsbasedn, int len);

  // CORBA objref semantics:
  void _duplicate(void);
  void _release(void); 

  int bindAsDefault(int retryCount = -1); // returns LDAPU_SUCCESS on success
  inline LDAP *getSession(void) const;
  inline void setUnbound();
  int reconnect(int retryCount); // returns true on success
  static PRBool serverDown(int statusCode);

  // The rest are intended for use by other classes, not directly by outside
  // programs.
  unsigned long count_entries       (LDAPMessage *message);
  LDAPMessage *first_entry         (LDAPMessage *message);
  LDAPMessage *next_entry          (LDAPMessage *message);
  char        *get_dn              (LDAPMessage *message);
  char        *first_attribute_name(LDAPMessage *message, 
				    BerElement **iterator);
  char        *next_attribute_name (LDAPMessage *message, 
				    BerElement **iterator);
  struct berval **get_values       (LDAPMessage *message, const char *attr);
  int          sort_entries        (LDAPMessage *message, char *attr,
				    int (*cmp)(const char *,const char *));
#if 0
  int          add                 (const char *name, LDAPMod** val, 
				    int retryCount = 0);
  int          modify              (const char *name, LDAPMod** val,
				    int retryCount = 0);
  int          del                 (const char *name, int retryCount = 0);
  int          modrdn              (const char *source, const char *newrdn, 
				    PRBool del_old,int retryCount = 0);
#endif

  int          get_error_code(void);
  const char * get_error_message(void);

  friend int   operator==(const LdapSession& s1, const LdapSession& s2) { return 1; }

private:
  int init(void); // returns LDAPU_SUCCESS on success
  int search_dyngroup(LdapDNList &DNs, LdapEntry *dyngroup, const char *basedn);
  int group_is_unique(LdapEntry *entry, const char *basedn, const char *group);
  int match_groups(LdapEntry *entry, void *groupids, LDAPU_GroupCmpFn_t groupcmpfn, const char *baseDN, char **cn, int test_unique);

  LdapRealm *ldapRealm;
  static const int maxRetries;

  // This object is reference counted and must not be stack allocated:
  virtual ~LdapSession();

  unsigned long epoch;
  unsigned long refcount;
  //dyngroupmode_t dyngroupmode;
  LDAP *session;
  enum { NONE, DEFAULT, OTHER } boundto;
  int lastbindrv;
  int lastoprv;

  // not implemented:
  LdapSession(const LdapSession &copy_me);
  void operator=(const LdapSession &assign_me);
};

inline LDAP *LdapSession::getSession() const
{
    return session;
}

inline void LdapSession::setUnbound()
{
    boundto = NONE;
}

#endif /* __LDAPSESSION_H__ */
