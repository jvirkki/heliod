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

#include "LdapServerSet.h"
#include <iostream.h>
#include <string.h>
#include <malloc.h>
#include <base/util.h>

LdapServerSetEntry::LdapServerSetEntry(void) :
        port(0), hostname(0), bindName(0), password(0), dcsuffix(0)
{
}

LdapServerSetEntry::LdapServerSetEntry(LDAPURLDesc *ludp,
                                        const char *_bindName, const char *_password,
                                        const char *_dcsuffix, int _digestauthstate)
{
  bindName = _bindName ? strdup(_bindName) : 0;
  password = _password ? strdup(_password) : 0;
  dcsuffix = _dcsuffix ? strdup(_dcsuffix) : 0;

  hostname = ludp->lud_host ? strdup(ludp->lud_host) : 0;
  useSSL = ludp->lud_options & LDAP_URL_OPT_SECURE;
  useDigestAuth = _digestauthstate;
  port = ludp->lud_port ? ludp->lud_port : useSSL ? 636 : 389;
  baseDN = ludp->lud_dn ? strdup(ludp->lud_dn) : 0;
}

// copy constructor
LdapServerSetEntry::LdapServerSetEntry(const LdapServerSetEntry &copy_me) {
  hostname = bindName = password = 0;
  port = copy_me.port;
  if (copy_me.hostname) hostname = strdup(copy_me.hostname);
  if (copy_me.bindName) bindName = strdup(copy_me.bindName);
  if (copy_me.password) password = strdup(copy_me.password);
  if (copy_me.baseDN) dcsuffix = strdup(copy_me.baseDN);
  if (copy_me.dcsuffix) dcsuffix = strdup(copy_me.dcsuffix);
}

LdapServerSetEntry::~LdapServerSetEntry(void) {
  if (hostname) free(hostname);
  if (bindName) free(bindName);
  if (password) free(password);
  if (baseDN) free(baseDN);
  if (dcsuffix) free(dcsuffix);
}

static int strmatch(const char *s1, const char *s2)
{
  if (s1 == s2)
    return 1;
  if (!s1)
    return 0;
  if (!s2)
    return 0;
  return !strcmp(s1, s2);
}

int LdapServerSetEntry::isCompatible(LdapServerSetEntry *entry)
{
  if (!strmatch(baseDN, entry->baseDN))
    return 0;
  if (!strmatch(dcsuffix, entry->dcsuffix))
    return 0;

  if (useSSL != entry->useSSL)
    return 0;
  if (useDigestAuth != entry->useDigestAuth)
    return 0;

  return 1;
}

void LdapServerSetEntry::readValues (const char *&_hostname, unsigned short &_port,
                                     const char *&_bindName, const char *&_password)
{
  _hostname = (const char *) hostname;
  _port = port;
  _bindName = (const char *) bindName;
  _password = (const char *) password;
  return;
}

const int LdapServerSet::maxEntries = 64;

LdapServerSet::LdapServerSet()
{
  entries = new LdapServerSetEntry_ptr[maxEntries];
  currentEntries = 0;
  currentEpoch = 0;
  currentIndex = -1;
}

LdapServerSet::~LdapServerSet(void)
{
  for (int i=0; i < currentEntries; i++)
    delete entries[i];
  delete [] entries;
}

int LdapServerSet::addServer(const char *url, const char *bindName, const char *password, const char *dcsuffix, int digestauthstate)
{
  // call ldapsdk's parse function
  LDAPURLDesc *ludp = 0;
  int rv = ldap_url_parse((char *)url, &ludp);
  if (rv != LDAP_SUCCESS) {
    if (ludp)
      ldap_free_urldesc(ludp);
    return rv;
  }

  LdapServerSetEntry *newEntry = new LdapServerSetEntry(ludp, bindName, password, dcsuffix, digestauthstate);

  ldap_free_urldesc(ludp);

  // this boundary check is not thread safe.
  if (currentEntries >= maxEntries) {
    delete newEntry;
    return LDAPU_FAILED;
  }

  // this increment is not thread safe.
  entries[currentEntries++] = newEntry;
  if (currentEntries == 1)
    currentIndex = 0;

  return LDAPU_SUCCESS;
}

void LdapServerSet::getServer
(int offset, const char *&hostname, unsigned short &port,
 const char *&bindName, const char *&password, unsigned long &epoch) {
  offset = offset % currentEntries;
  entries[offset]->readValues(hostname, port, bindName, password);
  epoch = currentEpoch;
  return;
}

void LdapServerSet::getCurrentServer
(const char *&hostname, unsigned short &port,
 const char *&bindName, const char *&password, unsigned long &epoch) {
  //assert (currentIndex != -1);
  entries[currentIndex]->readValues(hostname, port, bindName, password);
  epoch = currentEpoch;
  bumpCurrentServerIndex();
  return;
}

int LdapServerSet::areServersCompatible() {
  for (int i = 1; i < currentEntries; i++) {
    if (!entries[0]->isCompatible(entries[i]))
      return 0;
  }
  return 1;
}

int LdapServerSet::isEpochCurrent(const unsigned long myEpoch) {
  if (currentEpoch == myEpoch)
    return 1;
  return 0;
}

unsigned long LdapServerSet::bumpEpoch(const unsigned long myEpoch) {
  if (currentEpoch == myEpoch) 
    currentEpoch++;
  return currentEpoch;
}

void LdapServerSet::bumpCurrentServerIndex() {
  int index = currentIndex + 1;
  if (index == currentEntries)
    index = 0;
  currentIndex = index;
}

int LdapServerSet::count(void) {
  return currentEntries;
}

