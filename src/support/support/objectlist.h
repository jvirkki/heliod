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

#ifndef OBJECTLIST_H
#define OBJECTLIST_H

// ObjectList<ObjectLink> is an efficient (that is, new/malloc()-less) doubly
// linked list of ObjectLinks.  This efficiency comes at a cost; the next and
// previous pointers are maintained within the ObjectLink itself, so a given
// ObjectLink cannot be a member of multiple ObjectLists at the same time
// (however, a given object may have multiple ObjectLinks via composition).

#include <stddef.h>
#include "nspr.h"

class ObjectLink;
template <class ObjectLink> class ObjectList;
template <class ObjectLink> class ObjectIterator;

//-----------------------------------------------------------------------------
// ObjectLink
//-----------------------------------------------------------------------------

class ObjectLink {
public:
    ObjectLink(ObjectLink *next = NULL, ObjectLink *prev = NULL)
    : _next(next), _prev(prev)
    { }

    ObjectLink *_next;
    ObjectLink *_prev;

friend class ObjectList<ObjectLink>;
friend class ObjectIterator<ObjectLink>;
};

//-----------------------------------------------------------------------------
// ObjectList
//-----------------------------------------------------------------------------

template <class ObjectLink>
class ObjectList {
public:
    ObjectList()
    : _count(0), _head(NULL), _tail(NULL)
    { }

    ObjectLink* getHead() { return _head; }
    ObjectLink* getTail() { return _tail; }
    ObjectLink* removeFromHead() { return _head ? remove(_head) : NULL; }
    ObjectLink* removeFromTail() { return _tail ? remove(_tail) : NULL; }
    void addToHead(ObjectLink *node) { addBefore(node, _head); }
    void addToTail(ObjectLink *node) { addAfter(node, _tail); }
    int getCount() { return _count; }
    PRBool isEmpty() { return (_count == 0); }
    inline void addToHead(ObjectList<ObjectLink> &list);
    inline void addToTail(ObjectList<ObjectLink> &list);
    inline ObjectList<ObjectLink> removeFromHead(int count);
    inline ObjectList<ObjectLink> removeFromTail(int count);
    inline ObjectIterator<ObjectLink> begin();
    inline ObjectIterator<ObjectLink> end();
    inline ObjectLink* remove(ObjectLink *node);

private:
    ObjectList(int count, ObjectLink *head, ObjectLink *tail)
    : _count(count), _head(head), _tail(tail)
    { }

    inline void addBefore(ObjectLink *node, ObjectLink *anchor);
    inline void addAfter(ObjectLink *node, ObjectLink *anchor);

    int _count;
    ObjectLink *_head;
    ObjectLink *_tail;

friend class ObjectIterator<ObjectLink>;
};

//-----------------------------------------------------------------------------
// ObjectIterator
//-----------------------------------------------------------------------------

template <class ObjectLink>
class ObjectIterator {
public:
    ObjectIterator(ObjectList<ObjectLink> &list)
    : _list(&list), _node(NULL), _next(NULL), _prev(NULL)
    { }

    ObjectLink* operator*() { return _node; }
    inline ObjectLink* getHead();
    inline ObjectLink* getTail();
    inline void addBefore(ObjectLink *node);
    inline void addAfter(ObjectLink *node);
    inline ObjectLink* remove();
    inline ObjectLink* operator++();
    inline ObjectLink* operator++(int);
    inline ObjectLink* operator--();
    inline ObjectLink* operator--(int);

private:
    ObjectList<ObjectLink> *_list;
    ObjectLink *_node;
    ObjectLink *_next;
    ObjectLink *_prev;
};

//-----------------------------------------------------------------------------
// ObjectList::addToHead
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline void ObjectList<ObjectLink>::addToHead(ObjectList<ObjectLink> &list)
{
    // Link from the added-from list's tail to the added-to list's head
    if (list._tail) {
        list._tail->_next = _head;
    }

    // Link from the added-to list's head to the added-from list's tail
    if (_head) {
        _head->_prev = list._tail;
    } else {
        _tail = list._tail;
    }

    // The added-from list's nodes are now in the added-to list
    _head = list._head;
    _count += list._count;

    // There's nothing left in the added-from list
    list._head = NULL;
    list._tail = NULL;
    list._count = 0;
}

//-----------------------------------------------------------------------------
// ObjectList::addToTail
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline void ObjectList<ObjectLink>::addToTail(ObjectList<ObjectLink> &list)
{
    // Link from the added-from list's head to the added-to list's tail
    if (list._head) {
        list._head->_prev = _tail;
    }

    // Link from the added-to list's tail the added-from list's head
    if (_tail) {
        _tail->_next = list._head;
    } else {
        _head = list._head;
    }

    // The added-from list's nodes are now in the added-to list
    _tail = list._tail;
    _count += list._count;

    // There's nothing left in the added-from list
    list._head = NULL;
    list._tail = NULL;
    list._count = 0;
}

//-----------------------------------------------------------------------------
// ObjectList::begin
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectIterator<ObjectLink> ObjectList<ObjectLink>::begin()
{
    // Get an iterator that points to the head of the list
    ObjectIterator<ObjectLink> iter(*this);
    iter.getHead();
    return iter;
}

//-----------------------------------------------------------------------------
// ObjectList::end
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectIterator<ObjectLink> ObjectList<ObjectLink>::end()
{
    // Get an iterator that points to the tail of the list
    ObjectIterator<ObjectLink> iter(*this);
    iter.getTail();
    return iter;
}

//-----------------------------------------------------------------------------
// ObjectList::remove
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectLink* ObjectList<ObjectLink>::remove(ObjectLink *node)
{
    // Fix up the guy after node
    if (node->_next) {
        node->_next->_prev = node->_prev;
    } else {
        _tail = (ObjectLink*)node->_prev;
    }

    // Fix up the guy before node
    if (node->_prev) {
        node->_prev->_next = node->_next;
    } else {
        _head = (ObjectLink*)node->_next;
    }

    // Node is no longer in any list
    node->_next = NULL;
    node->_prev = NULL;

    _count--;

    return node;
}

//-----------------------------------------------------------------------------
// ObjectList::removeFromHead
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectList<ObjectLink> ObjectList<ObjectLink>::removeFromHead(int count)
{
    // We assume count is small and walk the list starting from its head.  Note
    // that count > _count is alright; the removed-to list will end up with
    // _count nodes instead of count.
    int removed = 0;
    ObjectLink *head = _head;
    while (head && removed < count) {
        removed++;
        head = (ObjectLink*)head->_next;
    }

    // Break the list
    if (head && head->_prev) {
        head->_prev->_next = NULL;
        head->_prev = NULL;
    }

    // Create the new (removed-to) list
    ObjectList list(removed, _head, head ? (ObjectLink*)head->_prev : _tail);

    // Fix up the existing (removed-from) list
    _head = head;
    if (!_head) {
        _tail = NULL;
    }
    _count -= removed;

    return list;
}

//-----------------------------------------------------------------------------
// ObjectList::removeFromTail
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectList<ObjectLink> ObjectList<ObjectLink>::removeFromTail(int count)
{
    // We assume count is small and walk the list starting from its tail.  Note
    // that count > _count is alright; the removed-to list will end up with
    // _count nodes instead of count.
    int removed = 0;
    ObjectLink *tail = _tail;
    while (tail && removed < count) {
        removed++;
        tail = (ObjectLink*)tail->_prev;
    }

    // Break the list
    if (tail && tail->_next) {
        tail->_next->_prev = NULL;
        tail->_next = NULL;
    }

    // Create the new (removed-to) list
    ObjectList list(removed, tail ? (ObjectLink*)tail->_next : _head, _tail);

    // Fix up the existing (removed-from) list
    _tail = tail;
    if (!_tail) {
        _head = NULL;
    }
    _count -= removed;

    return list;
}

//-----------------------------------------------------------------------------
// ObjectList::addBefore
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline void ObjectList<ObjectLink>::addBefore(ObjectLink *node, ObjectLink *anchor)
{
    PR_ASSERT(node->_next == NULL);
    PR_ASSERT(node->_prev == NULL);

    node->_next = anchor;

    if (anchor) {
        // Add node before position anchor
        node->_prev = anchor->_prev;
        if (node->_prev) {
            node->_prev->_next = node;
        } else {
            _head = node;
        }
        anchor->_prev = node;
    } else {
        // Add node to empty list
        PR_ASSERT(_head == NULL);
        PR_ASSERT(_tail == NULL);
        node->_prev = NULL;
        _head = node;
        _tail = node;
    }

    _count++;
}

//-----------------------------------------------------------------------------
// ObjectList::addAfter
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline void ObjectList<ObjectLink>::addAfter(ObjectLink *node, ObjectLink *anchor)
{
    PR_ASSERT(node->_next == NULL);
    PR_ASSERT(node->_prev == NULL);

    node->_prev = anchor;

    if (anchor) {
        // Add node after position anchor
        node->_next = anchor->_next;
        if (node->_next) {
            node->_next->_prev = node;
        } else {
            _tail = node;
        }
        anchor->_next = node;
    } else {
        // Add node to empty list
        PR_ASSERT(_head == NULL);
        PR_ASSERT(_tail == NULL);
        node->_next = NULL;
        _head = node;
        _tail = node;
    }

    _count++;
}

//-----------------------------------------------------------------------------
// ObjectIterator::getHead
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectLink* ObjectIterator<ObjectLink>::getHead()
{
    _node = _list->getHead();
    _next = NULL;
    _prev = NULL;
    return _node;
}

//-----------------------------------------------------------------------------
// ObjectIterator::getTail
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectLink* ObjectIterator<ObjectLink>::getTail()
{
    _node = _list->getTail();
    _next = NULL;
    _prev = NULL;
    return _node;
}

//-----------------------------------------------------------------------------
// ObjectIterator::addBefore
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline void ObjectIterator<ObjectLink>::addBefore(ObjectLink *node)
{
    if (_node) {
        _list->addBefore(node, _node);
    } else if (_prev) {
        _list->addAfter(node, _prev);
    } else {
        _list->addToHead(node);
    }
}

//-----------------------------------------------------------------------------
// ObjectIterator::addAfter
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline void ObjectIterator<ObjectLink>::addAfter(ObjectLink *node)
{
    if (_node) {
        _list->addAfter(node, _node);
    } else if (_next) {
        _list->addBefore(node, _next);
    } else {
        _list->addToTail(node);
    }
}

//-----------------------------------------------------------------------------
// ObjectIterator::remove
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectLink* ObjectIterator<ObjectLink>::remove()
{
    ObjectLink *node = _node;

    // Remember where we were in the list
    if (_node) {
        _next = (ObjectLink*)_node->_next;
        _prev = (ObjectLink*)_node->_prev;
    }
    _node = NULL;

    // Remove node from the list
    _list->remove(node);

    return node;
}

//-----------------------------------------------------------------------------
// ObjectIterator::operator++
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectLink* ObjectIterator<ObjectLink>::operator++()
{
    // Prefix increment
    if (_node) {
        _node = (ObjectLink*)_node->_next;
    } else {
        _node = _next;
        _next = NULL;
        _prev = NULL;
    }
    return _node;
}

template <class ObjectLink>
inline ObjectLink* ObjectIterator<ObjectLink>::operator++(int)
{
    // Postfix increment
    ObjectLink *node = _node;
    if (_node) {
        _node = (ObjectLink*)_node->_next;
    } else {
        _node = _next;
        _next = NULL;
        _prev = NULL;
    }
    return node;
}

//-----------------------------------------------------------------------------
// ObjectIterator::operator--
//-----------------------------------------------------------------------------

template <class ObjectLink>
inline ObjectLink* ObjectIterator<ObjectLink>::operator--()
{
    // Prefix decrement
    if (_node) {
        _node = (ObjectLink*)_node->_prev;
    } else {
        _node = _prev;
        _next = NULL;
        _prev = NULL;
    }
    return _node;
}

template <class ObjectLink>
inline ObjectLink* ObjectIterator<ObjectLink>::operator--(int)
{
    // Postfix decrement
    ObjectLink *node = _node;
    if (_node) {
        _node = (ObjectLink*)_node->_prev;
    } else {
        _node = _prev;
        _next = NULL;
        _prev = NULL;
    }
    return node;
}

#endif // OBJECTLIST_H
