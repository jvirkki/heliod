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

/*
 * GenericList class implementation
 *
 * @author  $Author: elving $
 * @version $Revision: 1.1.2.2.62.1 $ $Date: 2005/01/13 20:15:40 $
 * @since   iWS5.0
 */

#include "GenericList.h"


// Void constructor

GenericList::GenericList(void) : length_(0), head_(NULL), tail_(NULL)
{
}

// Copy construct the list

GenericList::GenericList(const GenericList& list) : length_(0), head_(NULL),
    tail_(NULL)
{
    if (list.length_ > 0)
    {
        listNode* node = list.head_;
        for (PRUint16 i = 0; i < list.length_; i++)
        {
            this->append(node->data);
            node = node->next;
        }
    }
}


// Assignment operator

GenericList&
GenericList::operator=(const GenericList& list)
{

    if (this == &list)
        return *this;

    // Empty the current contents of the list
    this->flush();

    if (list.length_ > 0)
    {
        listNode* node = list.head_;
        for (PRUint16 i = 0; i < list.length_; i++)
        {
            this->append(node->data);
            node = node->next;
        }
    }
    return *this;
}

// Deletes the list

GenericList::~GenericList(void)
{
    flush();
}


// Clears data from the list

void GenericList::flush()
{
    while (this->head_)
    {
        listNode* nodeToDelete = this->head_;
        this->head_ = this->head_->next;
        delete nodeToDelete;
    }
    this->length_ = 0;
    this->head_ = NULL;
    this->tail_ = NULL;
}



// Test for equality

int
GenericList::operator==(const GenericList& list) const
{
    if (this->length_ != list.length_)
        return 0;

    listNode* node1  = this->head_;
    listNode* node2 = list.head_;
    for (PRUint16 i = 0; i < this->length_; i++)
    {
        if (node1->data != node2->data)
            return 0;

        node1 = node1->next;
        node2 = node2->next;
    }
    return 1;
}


// test for inequality

int
GenericList::operator!=(const GenericList& list) const
{
    return !(*this == list);
}


// Returns PR_TRUE if item is in the list, else returns PR_FALSE

PRBool
GenericList::contains(const void* item) const
{

    listNode* node = this->head_;
    while (node)
    {
        if (item == node->data)
            return PR_TRUE;
        node = node->next;
    }
    return PR_FALSE;
}


// Returns number of matches of item in the list

PRUint16
GenericList::matches(const void* item) const
{
    PRUint16 count = 0;
    listNode* node = this->head_;
    while (node)
    {
        if (item == node->data)
            count++;

        node = node->next;
    }
    return count;
}



// Deletes first node with datum == item 
// Returns PR_SUCCESS if successfully deleted, PR_FAILURE if list empty or 
// node with matching data could not be found

PRStatus
GenericList::remove(const void* item)
{
    if (this->length_ > 0)
    {
        listNode* current = this->head_;

        while (current)
        {
            if (current->data == item)             // found a matching node
            {
                listNode* savenext = current->next;
                listNode* saveprev = current->prev;
                delete current;

                if (saveprev)                   // deleted node wasn't the head
                    saveprev->next = savenext;
                else                            // deleted node was the head
                    this->head_ = savenext;

                if (savenext)                   // deleted node wasn't the tail
                {
                    savenext->prev = saveprev;
                    current = savenext;
                }
                else                            // deleted node was the tail
                {
                    this->tail_ = saveprev;
                    current = saveprev;
                }
                
                this->length_--;
                return PR_SUCCESS;
            }
            current = current->next;
        }
    }
    return PR_FAILURE;
}


// Deletes list node at index

PRStatus
GenericList::removeAt(const PRUint16 index)
{

    if ((this->length_ > 0) && (index < this->length_))
    {
        PRUint16 i = 0;
        listNode* current = this->head_;

        while (current)
        {
            if (i == index)
            {
                listNode* savenext = current->next;
                listNode* saveprev = current->prev;
                delete current;

                if (saveprev)                  // deleted node wasn't the head
                    saveprev->next = savenext;
                else                           // deleted node was the head
                    this->head_ = savenext;

                if (savenext)                  // deleted node wasn't the tail
                {
                    current = savenext;
                    savenext->prev = saveprev;
                }
                else                           // deleted node was the tail
                {
                    current = saveprev;
                    this->tail_ = saveprev;
                }
                this->length_--;
                return PR_SUCCESS;
            }
            current = current->next;
            i++;
        }
    }
    return PR_FAILURE;
}



// Returns the datum at the specified index

void*
GenericList::operator[](const PRUint16 index)
{

    if (index < this->length_)
    {
        listNode* current = this->head_;
        PRUint16 i = 0;
        while (current)
        {
            if (i == index)
                return current->data;

            current = current->next;
            i++;
        }
    }
    return NULL;
}


// Appends a node with newdata at end of the list

PRStatus
GenericList::append(void* item)
{
    listNode* newNode = new listNode;

    PR_ASSERT(newNode != NULL);

    if (!newNode)
    {
        // XXX - report error ?
        return PR_FAILURE;
    }

    newNode->data = item;
    newNode->next = NULL;

    if (this->length_)
    {
        newNode->prev = this->tail_;
        this->tail_->next = newNode;
        this->tail_ = newNode;
        this->length_++;
    }
    else                    // empty list
    {
        newNode->prev = NULL;
        this->head_ = newNode;
        this->tail_ = newNode;
        this->length_++;
    }
    return PR_SUCCESS;
}


// Returns the datum at the head of the list

void*
GenericList::head()
{
    if (this->head_)
    {
        return this->head_->data;
    }
    return NULL;
}


// Returns the datum at the tail of the list

void*
GenericList::tail()
{
    if (this->tail_)
    {
        return this->tail_->data;
    }
    return NULL;
}


#ifdef DEBUG
//  Processing: returns 1 if the list ok, else aborts with message

PRBool
GenericList::isValid(void) const
{

    if (this->length_ == 0)
    {
        if (this->head_ || this->tail_)
        {
            // non-zero head_ or tail_ in null list
            return PR_FALSE;
        }
        return PR_TRUE;
    }
    else
    {
        if (this->head_->prev != NULL)
        {
            // head_->prev != NULL
            return PR_FALSE;
        }

        if (this->tail_->next != NULL)
        {
            // tail->next != NULL
            return PR_FALSE;
        }

        PRUint16 nNodes = 1;            // we know that there is at least 1

        // Walk down the list from the head and count the nodes.

        listNode* node = this->head_;
        while (node->next)
        {
            node = node->next;
            nNodes++;
        }
        // node now points to the last item in the list

        if (this->length_ != nNodes)
        {
            // length_ != number of Nodes
            return PR_FALSE;
        }
        if (node != this->tail_)
        {
            // tail_ doesn't point to end of the list
            return PR_FALSE;
        }

        // Now walk up the list starting at the tail
        node = this->tail_;
        while (node->prev)
        {
            node = node->prev;
            nNodes--;
        }

        if ((nNodes != 1) || (node != this->head_))
        {
            // walk up the list != walk down the list
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}
#endif
