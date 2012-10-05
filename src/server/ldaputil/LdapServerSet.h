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

#ifndef __LDAPSERVERSET_H__
#define __LDAPSERVERSET_H__

#include <ldaputil/ldaputil.h>
#include "LdapSessionCommon.h"

class LDAPSESSION_PUBLIC LdapServerSetEntry {
public:
  LdapServerSetEntry(void);
  LdapServerSetEntry(LDAPURLDesc *ludp, const char *bindName, 
		     const char *password, const char *dcsuffix, 
		     int digestauthstate); 
  LdapServerSetEntry(const LdapServerSetEntry &copy_me);
  virtual ~LdapServerSetEntry(void);
  int isCompatible(LdapServerSetEntry *entry);
  void readValues(const char *&hostname, unsigned short &port,
		  const char *&bindName, const char *&password);
  const char *getHostname(void);
  unsigned short getHostPort(void);
  const char *getBindName(void);
  const char *getPassword(void);
  const char *getBaseDN(void);
  const char *getDCSuffix(void);
  int getAllowDigestAuth(void) { return useDigestAuth; }
  int getUseSSL(void) { return useSSL; }

private:
  char *hostname;
  unsigned short port;
  char *bindName;
  char *password;
  char *baseDN;
  char *dcsuffix;
  int useDigestAuth;
  int useSSL;
  void operator=(const LdapServerSetEntry &);
};

typedef LdapServerSetEntry *LdapServerSetEntry_ptr;

// this class is not protected for thread safety.  Protect it externally.
// if all addServers occur before use, we should be okay without locks.
class LDAPSESSION_PUBLIC LdapServerSet {
public:
  LdapServerSet(void);
  virtual ~LdapServerSet(void);
  int addServer(const char *url, const char *bindName, const char *password, const char *dcsuffix, int digestauthstate);
  void getServer(int offset, const char * &hostname, unsigned short &port,
		 const char * &bindName, const char * &password,
		 unsigned long &epoch);
  void getCurrentServer(const char * &hostname, unsigned short &port,
			const char * &bindName, const char * &password,
			unsigned long &epoch);
  int areServersCompatible();
  int needSSL() { return currentIndex == -1 ? 0 : entries[currentIndex]->getUseSSL(); }
  int allowDigestAuth() { return currentIndex == -1 ? 0 : entries[currentIndex]->getAllowDigestAuth(); }
  const char *getBaseDN() { return currentIndex == -1 ? 0 : entries[currentIndex]->getBaseDN(); }
  const char *getDCSuffix() { return currentIndex == -1 ? 0 : entries[currentIndex]->getDCSuffix(); }
  int isEpochCurrent(const unsigned long myEpoch);
  unsigned long bumpEpoch(const unsigned long myEpoch);
  void bumpCurrentServerIndex();
  int count(void);
private:
  unsigned long currentEpoch;
  static const int maxEntries;
  volatile int currentIndex;
  int currentEntries;
  LdapServerSetEntry_ptr *entries;
  
  // not implemented:
  LdapServerSet(const LdapServerSet &copy_me);
  void operator=(const LdapServerSet &assignMe);
};


inline const char *
LdapServerSetEntry::getHostname()
{
  return hostname;
}

inline const char *
LdapServerSetEntry::getBindName()
{
  return bindName;
}

inline const char *
LdapServerSetEntry::getPassword()
{
  return password;
}

inline const char *
LdapServerSetEntry::getBaseDN()
{
  return baseDN;
}

inline const char *
LdapServerSetEntry::getDCSuffix()
{
  return dcsuffix;
}

inline unsigned short
LdapServerSetEntry::getHostPort()
{
  return port;
}

#endif // __LDAPSERVERSET_H__
