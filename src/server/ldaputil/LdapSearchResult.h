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

#ifndef __LDAPSEARCHRESULT_H__
#define __LDAPSEARCHRESULT_H__

#include <ldap.h>
#include <ldaputil/certmap.h>
#include <ldaputil/errors.h>
#include "LdapSessionCommon.h"
#include "LdapSession.h"
#include "LdapEntry.h"
#include <nspr.h>
#include <string.h>

class LdapSession;
class LdapEntry;
class LdapSearchResult;

typedef LdapSearchResult *LdapSearchResult_ptr;


// a container class
class LDAPSESSION_PUBLIC LdapSearchResult_var {
public:
  LdapSearchResult_var(LdapSearchResult_ptr ownThis);
  virtual ~LdapSearchResult_var(void);
  operator LdapSearchResult_ptr(void);
  LdapSearchResult_var& operator=(LdapSearchResult*);
  LdapSearchResult_ptr operator->(void);
private:
  LdapSearchResult_ptr _p;
};


class LDAPSESSION_PUBLIC LdapSearchResult {
public:
  virtual ~LdapSearchResult();
  PRBool bad(void);
  PRBool good(void);
  int ldapresult(void);
  PRBool server_down(void);

  const char *error_description(void);
  // function pointers will not CORBA-ize well:
  void sort(const char *attribute, int (*cmp)(const char *, const char *)
	    =strcmp);
  void reset(void);
  LdapEntry *next(void);
  unsigned long entries(void);
private:
  friend class LdapSession; // for access to ctor
  LdapSearchResult(LdapSession *session, LDAPMessage *message, int result);
  LdapSearchResult(const LdapSearchResult &copy_me);
  void operator=(const LdapSearchResult &assign_me);
  LdapSession *_session;
  LDAPMessage *_message;
  LDAPMessage *_iterator;
  int          _result;
};
#endif /* __LDAPSEARCHRESULT_H__ */

