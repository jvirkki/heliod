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

#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

/*
 *   Provided a template implementation of a simple linked list. 
 * It is a reference based container (stores the pointers to the
 * objects contained) It doesn't check for duplicates etc and find
 * (delete) will find(delete) the first occurence of an element.
 *
 * Iterator class can be used for traversal using the "++" operator.
 */
template <class C> class CList; // forward declaration
template <class C> class CListIterator; // forward declaration
template <class C> class CListConstIterator; // forward declaration

template <class C> class CListElmt {
  friend class CList<C>;
  friend class CListIterator<C>;
  friend class CListConstIterator<C>;
  private:
    CListElmt(C* refP, int id) : _refP(refP), _nextP(0), _prevP(0), _id(id)
        {
                // Nothing really has to be done here
        };


  private:
    int       _id;    
    C         *_refP;
#if defined(AIX)
    CListElmt<C> *_nextP;
    CListElmt<C> *_prevP;
#else
    CListElmt *_nextP;
    CListElmt *_prevP;
#endif
};

//
// NOTE : If you need the find functionality, make sure that 
//  class C has "==" defined.
//
template <class C> class CList {
  friend class CListIterator<C>;
  friend class CListConstIterator<C>;
  private:
    CListElmt<C>* _headP;
    CListElmt<C>* _tailP;
    CListElmt<C>* Lookup(C * memberP)
        {
                CListElmt<C> *curEltP = _headP;
                
                while (curEltP)
                {
                        if (curEltP->_refP == memberP) 
                                break;
                        curEltP = curEltP->_nextP;
                };
                return(curEltP);
        };
    
    int _nextId;
    int _numElts;
    int autoDestroy;
    C* Remove(CListElmt<C> *elmtP)
        {
                C  *recP = NULL;
                if (elmtP)
                {
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
                };
                if ( (1==autoDestroy) && recP)
                        delete recP;
                
                return(recP);
        };
    

  public:
    CList() :  _headP(0), _tailP(0), _nextId(0), _numElts(0), autoDestroy(0)
        {
                // Nothing really has to be done here
        };
        
    void setAutoDestroy(int setting)
        {
                autoDestroy = setting;
        };

    virtual ~CList()
        {
                while (_headP)
                        Remove(_headP);
        };

    int NextId()
        {
                return _nextId;
        };

    int Append(C* refP)
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
        };

    int Member(C* memberP)
        {
                return(Lookup(memberP) != 0);
        };
    
    int Delete(C* memberP)
        {
                CListElmt<C> *listElmtP = Lookup(memberP);
                if (listElmtP)
                {
                        (void) Remove(listElmtP);
                        return(1);
                }
                return(0);
        };
    
    int NumEntries() const { return _numElts; }
    C* Find(C* memberP) // Lookup based on == operator for class C
        {
                CListElmt<C> *curEltP = _headP;
        
                while (curEltP)
                {
                        if (curEltP->_refP == memberP) 
                                break;
                        curEltP = curEltP->_nextP;
                };
                return(curEltP ? curEltP->_refP : 0);
        };
    
    C* First() { return(_headP ? _headP->_refP : 0); }
    C* Last() { return(_tailP ? _tailP->_refP : 0); }
};

template <class C> class CListIterator {
  private:
    CList<C>* _listP;
    int _curId;
  public:
    CListIterator(CList<C>* linkedListP) : _listP(linkedListP), _curId(-1)
        {
                // Nothing more to be done
        };
    
    virtual ~CListIterator()
        {
                _listP = NULL;
        };
    
    C* operator++() // Define ++ operator to move forward along the list.
        {
                C *valueP = NULL;
                CListElmt<C> *curEltP = _listP->_headP;

                while (curEltP)
                {
                        if (curEltP->_id > _curId)
                        {
                                _curId = curEltP->_id;
                                return(curEltP->_refP);
                        }
                        curEltP = curEltP->_nextP;
                }
                _curId = -1;
                return(NULL);
        };
    
    void operator()() // Overload the function operator to reset the iterator
        {
                _curId = -1;
        };
    
};

template <class C> class CListConstIterator {
  private:
    const CList<C>* _listP;
    int _curId;
  public:
    CListConstIterator(const CList<C>* linkedListP)
        : _listP(linkedListP), _curId(-1)
        {
                // Nothing more to be done
        };
    
    virtual ~CListConstIterator()
        {
                _listP = NULL;
        };
    
    const C* operator++() // Define ++ operator to move forward along the list.
        {
                const C *valueP = NULL;
                const CListElmt<C> *curEltP = _listP->_headP;

                while (curEltP)
                {
                        if (curEltP->_id > _curId)
                        {
                                _curId = curEltP->_id;
                                return(curEltP->_refP);
                        }
                        curEltP = curEltP->_nextP;
                }
                _curId = -1;
                return(NULL);
        };
    
    void operator()() // Overload the function operator to reset the iterator
        {
                _curId = -1;
        };
    
};

#endif // _LINKED_LIST_H_
