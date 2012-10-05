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

#include "LdapSessionPool.h"
#include "libaccess/LdapRealm.h"

static PRLock *_poolListLock = NULL;

LdapSessionPool *LdapSessionPool::_poolList = NULL;

static PRStatus
PoolListCreate(void)
{
    _poolListLock = PR_NewLock();
    return _poolListLock ? PR_SUCCESS : PR_FAILURE;
}

LdapSessionPool::LdapSessionPool(LdapRealm *ldapRealm,int limit)
{
    _ldapRealm = ldapRealm;
    _maxSessions = (limit < 1 ? 1 : limit);
    _waiters = 0;
    _lock = PR_NewLock();
    assert(_lock);
    _cvar = PR_NewCondVar(_lock);
    assert(_cvar);

    // add new pool to list of all pools
    if (_poolListLock == NULL) {
        static PRCallOnceType once = { 0, 0, (PRStatus)0 };
        PR_CallOnce(&once, PoolListCreate);
    }
    PR_Lock(_poolListLock);
    _nextPool = _poolList;
    _poolList = this;
    PR_Unlock(_poolListLock);
}


LdapSessionPool::LdapSessionPool(LdapServerSet *servers, unsigned short limit,
                                    dyngroupmode_t dyngroupmode)
{
  authmethod=NULL;
  certNickName=NULL;

  _maxSessions = (limit < 1 ? 1 : limit);
  _dyngroupmode = dyngroupmode;
  //_servers = servers;
  _waiters = 0;
  _lock = PR_NewLock();
  assert(_lock);
  _cvar = PR_NewCondVar(_lock);
  assert(_cvar);

  // add new pool to list of all pools
  if (_poolListLock == NULL) {
      static PRCallOnceType once = { 0, 0, (PRStatus)0 };
      PR_CallOnce(&once, PoolListCreate);
  }
  PR_Lock(_poolListLock);
  _nextPool = _poolList;
  _poolList = this;
  PR_Unlock(_poolListLock);
}


LdapSessionPool::~LdapSessionPool(void) {
  assert(!_waiters);
  // Close all the sessions in the free list.
  LdapSession *session;
  CListIterator<LdapSession> free_iterator (&_free_list);
  while (session = ++free_iterator) {
    _free_list.Delete (session);
    session->_release ();
  }
  
  // if there's anybody on the active list, then this object 
  // cannot be destroyed, because the user of that session will come back
  // to return the session to us.
  int count = _active_list.NumEntries();
  assert (count < 2); // allow a thread for the invalidator.
  
  // Destroy the semaphore object.
  PR_DestroyCondVar (_cvar);
  PR_DestroyLock    (_lock);

  // Remove this pool from the list of all pools
  PR_Lock(_poolListLock);
  LdapSessionPool **pp;
  for (pp = &_poolList; *pp; pp = &(*pp)->_nextPool) {
      if (*pp == this) {
          *pp = _nextPool;
          break;
      }
  }
  PR_Unlock(_poolListLock);
}


void LdapSessionPool::init()
{
  // prespawn one session to see if is ok
  //LdapSession *session = new LdapSession(_servers, _dyngroupmode);
  LdapSession *session = new LdapSession(_ldapRealm);
  _free_list.Append (session);
}


// Get session.
//
LdapSession *LdapSessionPool::get_session(void)
{
  assert(this); // we've seen problems with this...
  LdapSession *session = 0;

  PR_Lock(_lock);
  while (1) {
    if (_free_list.NumEntries() > 0) {
      // get one of those free sessions
      session = _free_list.First();
      _free_list.Delete(session);
      _active_list.Append(session); 
      break;
    }
    // no free sessions - see if we may spawn a new one
    if (_active_list.NumEntries() < _maxSessions) {
      PR_Unlock(_lock);
      //session = new LdapSession(_servers, _dyngroupmode);
      session = new LdapSession(this->getLdapRealm());
      PR_Lock(_lock);
      _active_list.Append(session);
      break;
    }

    // we have to wait on the semaphore for a free connection.
    _waiters++;
    (void)PR_WaitCondVar (_cvar, PR_INTERVAL_NO_TIMEOUT);
    _waiters--;
    // we cannot be sure that we are the first to be woken up, so back to square one
  }
  PR_Unlock(_lock);
  return (session);
}

int
LdapSessionPool::needSSL()
{
  //return _servers->needSSL();
  return _ldapRealm->needSSL();
}

int
LdapSessionPool::allowDigestAuth()
{
  //return _servers->allowDigestAuth();
  return _ldapRealm->allowDigestAuth();
}

void
LdapSessionPool::free_session (LdapSession *session)
{
    PRBool signalNeeded = 0;

    PR_Lock(_lock);
    add_free_session(session);
    PR_Unlock(_lock);
}

void
LdapSessionPool::free_ldap_session(LDAP *ld)
{
    LdapSessionPool *pp;
    LdapSession *session;

    PR_Lock(_poolListLock);
    for (pp = _poolList; pp; pp = pp->_nextPool) {
        PR_Lock(pp->_lock);
        CListIterator<LdapSession> free_iterator (&pp->_active_list);
        while (session = ++free_iterator) {
            if (session->getSession() == ld) {
                PR_Unlock(_poolListLock);
                session->setUnbound();
                pp->add_free_session(session);
                PR_Unlock(pp->_lock);
                return;
            }
        }
        PR_Unlock(pp->_lock);
    }

    // this shouldn't happen
    assert(ld == NULL);
    PR_Unlock(_poolListLock);
}

const char* LdapSessionPool::getAuthMethod() {
  return authmethod;
}


void LdapSessionPool::setAuthMethod(const char *_authmethod) {
  authmethod =  _authmethod? strdup(_authmethod) : 0;
}


const char* LdapSessionPool::getCertNickName()
{
    return certNickName;
}

void LdapSessionPool::setCertNickName(const char* certName)
{
    certNickName = strdup(certName);
}

void
LdapSessionPool::add_free_session(LdapSession *session)
{
    _active_list.Delete (session);
    _free_list.Append (session);
    if (_waiters) {
      // tell the waiters that we might have something for them
      PR_NotifyCondVar (_cvar);
    }
}

