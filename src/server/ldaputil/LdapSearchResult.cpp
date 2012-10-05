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


#include "LdapSearchResult.h"

LdapSearchResult_var::LdapSearchResult_var(LdapSearchResult_ptr ownThis) {
  _p = ownThis;
}

LdapSearchResult_var::~LdapSearchResult_var(void) {
  delete _p;
}

LdapSearchResult_var::operator LdapSearchResult_ptr(void) {
  return _p;
}

LdapSearchResult_var& LdapSearchResult_var::operator=(LdapSearchResult* newPtr) {
  if (_p)
    delete _p;
  _p = newPtr;
  return (*this);
}

LdapSearchResult_ptr LdapSearchResult_var::operator->(void) {
  return _p;
}

LdapSearchResult::LdapSearchResult(LdapSession *session, LDAPMessage *message, int result)
    : _session(session), _message(message), _result(result), _iterator(NULL)
{
  session->_duplicate();
}

LdapSearchResult::~LdapSearchResult(void)
{
  // ignore session...  It's own destructor will handle it...
  if (_message != NULL) ldap_msgfree(_message);
  _message = NULL;
  _session->_release();
}

PRBool
LdapSearchResult::good(void)
{
    return (_result == LDAP_SUCCESS);
}

PRBool
LdapSearchResult::bad(void)
{
    return (_result != LDAP_SUCCESS);
}

int
LdapSearchResult::ldapresult(void)
{
    return _result;
}

const char *
LdapSearchResult::error_description(void)
{
    return (_result == LDAP_SUCCESS) ? NULL : ldapu_err2string(_result);
}

PRBool
LdapSearchResult::server_down(void)
{
  return ((_result == LDAP_CONNECT_ERROR) ||
	  (_result == LDAP_SERVER_DOWN) ||
	  (_result >= LDAP_OTHER));
}

unsigned long
LdapSearchResult::entries(void)
{
  return _session->count_entries(_message);
}

void
LdapSearchResult::sort(const char *attribute, int (*cmp)(const char *, const char *))
{
  _session->sort_entries(_message, (char *) attribute, cmp);
  return;
}

void
LdapSearchResult::reset(void)
{
  _iterator = NULL;
  return;
}

LdapEntry *
LdapSearchResult::next(void)
{
  _iterator = (_iterator == NULL) ?  _session->first_entry(_message) : _session->next_entry(_iterator);
  return (_iterator != NULL) ? new LdapEntry(_session, _iterator) : NULL;
}
