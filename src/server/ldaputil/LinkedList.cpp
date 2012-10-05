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

#include "ldaputil/LdapSession.h"
#include "ldaputil/LinkedList.hh"

//
// Constructor for TElmt:
//
template <class C> CListElmt<C>::CListElmt(C *refP, int id)
  : _refP(refP), _nextP(0), _prevP(0), _id(id)
{
  // Nothing really has to be done here
}

//
// Constructor for CList
//
template <class C> CList<C>::CList()
  :  _headP(0), _tailP(0), _nextId(0), _numElts(0)
{
  // Nothing really has to be done here
}

template <class C> CList<C>::~CList()
{
  while (_headP)
    Remove(_headP);
}

//
// CList::Append()
//
template <class C> int CList<C>::Append(C* refP)
{
  CListElmt<C> *newElmtP = new CListElmt<C>(refP, _nextId++);
  newElmtP->_prevP = _tailP;
  if (_tailP)
    _tailP->_nextP = newElmtP;
  if (_headP == 0)
    _headP = newElmtP;
  _tailP = newElmtP;
  _numElts++;
  return(newElmtP->_id);
}

//
// CList::Remove
//   
template <class C> C* CList<C>::Remove(CListElmt<C> *elmtP)
{
  C  *recP = NULL;
  if (elmtP) {
    if (elmtP->_nextP)
      elmtP->_nextP->_prevP = elmtP->_prevP;
    if (elmtP->_prevP)
      elmtP->_prevP->_nextP = elmtP->_nextP;
    if (elmtP == _tailP)
      _tailP = elmtP->_prevP;
    if (elmtP == _headP)
      _headP = _headP->_nextP;
    recP = elmtP->_refP;
    _numElts--;
    delete elmtP;
  }
  return(recP);
}

//
// CList::Lookup
//
template <class C> CListElmt<C>* CList<C>::Lookup(C* memberP)
{
  CListElmt<C> *curEltP = _headP;

  while (curEltP) {
    if (curEltP->_refP == memberP) 
      break;
    curEltP = curEltP->_nextP;
  }
  return(curEltP);
}

//
// CList::Find
//
template <class C> C* CList<C>::Find(C* memberP)
{
  CListElmt<C> *curEltP = _headP;

  while (curEltP) {
    if (curEltP->_refP == memberP) 
      break;
    curEltP = curEltP->_nextP;
  }
  return(curEltP ? curEltP->_refP : 0);
}

//
// CList::Member
//
template <class C> int CList<C>::Member(C *memberP)
{
  return(Lookup(memberP) != 0);
}

//
// CList::Delete()
//
template <class C> int CList<C>::Delete(C* memberP)
{
  CListElmt<C> *listElmtP = Lookup(memberP);
  if (listElmtP) {
    (void) Remove(listElmtP);
    return(1);
  }
  return(0);
}

/*
 *  Iterator for the list. Iterators may be used by some operations
 * that are long-duration and we can't lock up the list for that long. 
 * Since the list may change underneath us, we can't rely on keeping
 * pointers to the list. Due to this we keep _curId with the iterator
 * and the ++ operator gets the element with subsequent id. It works
 * since all elements are added to end of the list and ids are strictly
 * increasing numbers.
 */
//
// C'tor for CListIterator<C>
//
template <class C> CListIterator<C>::CListIterator(CList<C> *listP)
  : _listP(listP), _curId(-1)
{
  // Nothing more to be done
}

//
// D'tor for CListIterator<C>
//
template <class C> CListIterator<C>::~CListIterator()
{
  _listP = NULL;
}

//
// CListIterator<C>::operator()
//
template <class C> void CListIterator<C>::operator()()
{
  _curId = -1;
}

//
// CListIterator<C>::operator++()
//
template <class C> C* CListIterator<C>::operator++()
{
  C *valueP = NULL;
  CListElmt<C> *curEltP = _listP->_headP;

  while (curEltP) {
    if (curEltP->_id > _curId) {
      _curId = curEltP->_id;
      return(curEltP->_refP);
    }
    curEltP = curEltP->_nextP;
  }
  _curId = -1;
  return(NULL);
}


//
// Explicitly instantiate a template to make sure that it and all member
// functions will be part of a library.
//
#if defined(XP_PC) || defined(HPUX) || defined(LINUX)
// REVISIT: Added "class" before Clist for HPUX.
// This should be O.K for NT also but need to double check.
// mohideen@cup.hp.com
template class CListElmt<LdapSession>;
template class CList<LdapSession>;
template class CListIterator<LdapSession>;
template class CListElmt<LdapEntry>;
template class CList<LdapEntry>;
template class CListIterator<LdapEntry>;

#elif defined(IRIX) || defined (OSF1)

#pragma instantiate CListElmt<LdapSession>
#pragma instantiate CList<LdapSession>
#pragma instantiate CListIterator<LdapSession>
#pragma instantiate CListElmt<LdapEntry>
#pragma instantiate CList<LdapEntry>
#pragma instantiate CListIterator<LdapEntry>

#elif defined(AIX)

#pragma define(CListElmt<LdapSession>)
#pragma define(CList<LdapSession>)
#pragma define(CListIterator<LdapSession>)
#pragma define(CListElmt<LdapEntry>)
#pragma define(CList<LdapEntry>)
#pragma define(CListIterator<LdapEntry>)

#endif
