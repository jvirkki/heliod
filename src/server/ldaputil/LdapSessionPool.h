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

#ifndef __LDAP_SESSION_POOL_H__
#define __LDAP_SESSION_POOL_H__

#include <ldap.h>
#include "libaccess/WSRealm.h"
#include "libaccess/LdapRealm.h"
#include "LinkedList.hh"
#include "LdapSessionCommon.h"
#include "LdapSession.h"
#include "NsprWrap/CriticalSection.h"
#include "prcvar.h"

class LdapServerSet;
class LdapRealm;

class LDAPSESSION_PUBLIC LdapSessionPool
{
public:
  LdapSessionPool(LdapRealm *ldapRealm,int limit);
  LdapSessionPool(LdapServerSet *servers, unsigned short limit, dyngroupmode_t dyngroupmode);

  void init();
  LdapRealm *getLdapRealm() { return _ldapRealm; }

  // Get session.
  LdapSession *get_session ();
  void add_free_session (LdapSession *session);
  
  // Free the session.
  void free_session (LdapSession *session);

  int needSSL();
  int allowDigestAuth();

  PRBool supportDigest() { return allowDigestAuth() > 0; }
  // Another way to free a session
  static void             free_ldap_session(LDAP *ld);
  void setAuthMethod(const char* _authmethod);
  const char* getAuthMethod();
  void setCertNickName(const char* cerName);
  const char* getCertNickName();

private:
  char *authmethod;
  char *certNickName;
  virtual ~LdapSessionPool();

  LdapRealm          *_ldapRealm;  
  unsigned short      _maxSessions;
  dyngroupmode_t      _dyngroupmode;
  PRUint32            _waiters;
  LdapServerSet      *_servers;
  PRLock             *_lock; // protects all members of class.
  PRCondVar          *_cvar; // uses _lock.
  CList<LdapSession>  _active_list;
  CList<LdapSession>  _free_list;
  LdapSessionPool    *_nextPool;

  LdapSessionPool(const LdapSessionPool &copy_me);
  void operator=(const LdapSessionPool &assign_me);

  static LdapSessionPool *_poolList;
};

#if defined(IRIX)
#pragma do_not_instantiate CList<LdapSession>
#pragma do_not_instantiate CListIterator<LdapSession>
#endif

#endif /* __LDAP_SESSION_POOL_H__ */
