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

#ifndef __LDAPREALM_H__
#define __LDAPREALM_H__

#include "support/NSString.h"
#include "libaccess/WSRealm.h"
#include "ldaputil/LdapSessionCommon.h"

class LdapSessionPool;
class LdapValues;
class LdapEntry;
 
//from ldapacl.h
#define LDAP_URL_PREFIX      "ldap:"
#define LDAP_URL_PREFIX_LEN  5
#define LDAPS_URL_PREFIX     "ldaps:"
#define LDAPS_URL_PREFIX_LEN 6

class LDAPSESSION_PUBLIC LdapServerEntry {
public:
  LdapServerEntry(const char *host, int port, int useSSL);
  virtual ~LdapServerEntry(void);

public:
  const char *getHost(void) { return hostname; }
  int         getPort(void) { return port; }

private:
  char *hostname;
  int  port;
};


typedef LdapServerEntry *LdapServerEntry_ptr;


class LDAPSESSION_PUBLIC LdapRealm : public WSRealm {
public:
  LdapRealm(const char* _rawUrl,
            const char* _bindName,
            const char* _bindPwd,
            const char* _dcSuffix,
            dyngroupmode_t  _dynGrpMode,
            int   _nsessions,
            int   _allowDigestAuth,
            int _timeout,
            const char* _authMethod,
            const char* _certName,
            const char* _userSearchFilter,
            const char* _groupSearchFilter,
            const char* _groupTargetAttr);
  virtual ~LdapRealm(void);

public:
  static void setNSSDBPwd(char* pwd)     { if (pwd) NSSDBPwd.append(pwd); }
  static void setNSSDBCertdir(char* dir) { if (dir) NSSDBCertdir.append(dir); }
  static const char* getNSSDBPwd()     { return NSSDBPwd.data(); }
  static const char* getNSSDBCertdir() { return NSSDBCertdir.data(); }
  static void clearNSSDBPwd()   { NSSDBPwd.clear(); }
  int         getTimeOut(void) { return timeout; }

public:
  int init(NSErr_t *errp);
  const char* prepLdaphosts();
  int getDefport() { return defport;};
  LdapSessionPool* getSessionPool() { return ldapSessionPool; }
  
  const char * getBindName() { return bindName; }
  const char * getBindPwd()  { return bindPwd; }
  const char * getBaseDN()   { return baseDN;   }
  const char * getDCSuffix() { return dcSuffix; }
  const char * getClientCertNickName() { return clientCertNickName; }
  const char * getAuthMethod() { return authMethod; }
  dyngroupmode_t getDyngroupmode()  { return dynGrpMode; }

  int needSSL() { return ldapOverSSL; }
  int supportDigest()  { return allowDigestAuth(); }
  int allowDigestAuth() { return  digestAuth; }
  
  const char *getUserSearchFilter() { return userSearchFilter; }
  const char *getGroupSearchFilter(){ return groupSearchFilter;}
  const char *getGroupTargetAttr()  { return groupTargetAttr; }

public:
    GenericVector* getDirectMemberUsersForGroup (char* groupDN);
    GenericVector* getDirectMemberGroupsForGroup(char* groupDN);
    
private:
  void setSessionPool(LdapSessionPool* pool) { ldapSessionPool = pool; }
  void addServer(const char *url);
  void addServer(LdapServerEntry *entry);
  int  isCompatible(const char *_baseDN,int _useSSL);
  int  areServersCompatible();
  int  count(void);

private:
  void addAttrValues(IDType t,GenericVector& vector,LdapValues& attrVals);
  void addEntry(IDType t,GenericVector& vector,LdapEntry& entry);

private:
  static NSString NSSDBPwd;
  static NSString NSSDBCertdir;

private:
  NSString ldaphosts;
  int      defport;

  char *rawUrl;
  char *bindName;
  char *bindPwd;
  char *baseDN;
  char *dcSuffix;
  int   digestAuth;
  int   ldapOverSSL;
  int   nsessions;
  int timeout;
  dyngroupmode_t dynGrpMode;
  char *authMethod;
  char *clientCertNickName;
  char *userSearchFilter;
  char *groupSearchFilter;
  char *groupTargetAttr;

private:
  LdapSessionPool *ldapSessionPool; //runtime ldap sessions
  LdapServerEntry_ptr *entries;
  static const int maxEntries;
  int currentEntries;
};


#endif // __LDAPREALM_H__




