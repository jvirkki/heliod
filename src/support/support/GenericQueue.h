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

#ifndef _GenericQueue_h
#define _GenericQueue_h

#ifdef DEBUG
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
#include <iomanip>
using namespace std;
#else
#include <iostream.h>                        // cout
#include <iomanip.h>                         // setw()
#endif
#endif
#include "nspr.h"                            // NSPR declarations
#include "support_common.h"                  // SUPPORT_EXPORT

/**
 * A template-based first-in first out (FIFO) queue that can be configured
 * to be either a fixed-size queue or a dynamic one.
 *
 * Resizable queues grow (by doubling their size) dynamically when a 
 * <code>add</code> is attempted on a full queue. Non-resizable queues
 * return an error when a <code>add</code> is attempted on a full queue.
 *
 * Objects of type T must have well-defined copy-constructor and
 * operator= semantics.
 *
 * @author  $Author: elving $
 * @version $Revision: 1.1.2.1 $
 * @since   iWS5.0
 */

template<class T>
class SUPPORT_EXPORT GenericQueue
{
    public:

        /**
         * Constructor specifying the initial size of the queue.
         *
         * @param size       The initial size of the queue.
         * @param bResizable Set to <tt>PR_TRUE</tt> to make enable 
         *                   dynamic resizing of the queue object.
         */
        GenericQueue(const PRSize size, const PRBool bResizable = PR_FALSE);


        /**
         * Destructor
         *
         * Frees memory that was allocated for the queue.
         */
        ~GenericQueue(void);


        /**
         * Returns (and removes) the top/first item in the queue.
         */
        PRStatus remove(T& item);


        /**
         * Inserts the specified item at the bottom/tail of the queue.
         *
         * @param item The element to be added to the queue.
         * @returns    <tt>PR_SUCCESS</tt> if the item was successfully added 
         *              to the queue, <tt>PR_FAILURE</tt> if an error occurred 
         *              while adding it to the queue.
         */
        PRStatus add(T item);


        /**
         * Returns the number of items currently in the queue.
         */
        PRSize length(void) const;


        /**
         * Returns the maximum number of items that the queue can hold
         * (without expanding).
         */
        PRSize capacity(void) const;


        /**
         * Returns whether or not this queue object grows dynamically.
         */
        PRBool isResizable(void) const;

#ifdef DEBUG
        /**
         * Prints information about the object and the values of its
         * data elements to standard output.
         *
         * This method is useful for testing/debugging.
         */ 
        void printInfo(void) const;
#endif


    private:

        /**
         * Memory for the items in the queue.
         */
         T* items_;
        

        /**
         * The number of items currently in the queue.
         */
        PRSize nItems_;


        /**
         * The maximum number of items that can be stored in the queue.
         * (i.e. the size of the <code>items_</code> array.)
         */
        PRSize size_;


        /**
         * The index of the first (top) item in the queue.
         */
        PRUint32 head_;


        /** 
         * The index of the last (bottom) item in the queue.
         */
        PRUint32 tail_;


        /**
         * Indicates whether queue resizing is allowed.
         */
        PRBool bResizable_;

        /**
         * Expands the queue (copying existing elements) to the
         * specified size
         */
        void resize(const PRSize newSize);

        /**
         * Copy constructor
         *
         * Don't allow users to copy-construct objects of this class
         */
        GenericQueue(const GenericQueue<T>& queue);

        /**
         * operator=
         *
         * Don't allow users to clone objects of this class
         */
        GenericQueue<T>& operator=(const GenericQueue<T>& queue);
};


//
// Constructs a circular queue of the specified size
//

template<class T>
GenericQueue<T>::GenericQueue(const PRSize size, const PRBool bResizable)
{
    this->head_ = 0;
    this->tail_ = 0;
    this->size_ = 0;
    this->items_ = NULL;
    this->nItems_ = 0;
    this->bResizable_ = bResizable;
    this->resize(size);
}

#ifdef SOLARIS
#pragma disable_warn
#endif

template<class T>
GenericQueue<T>::GenericQueue(const GenericQueue<T>& queue)
{
    // private method inaccessible to users
}

template<class T>
GenericQueue<T>&
GenericQueue<T>::operator=(const GenericQueue<T>& queue)
{
    // private method inaccessible to users
    return *this;
}

#ifdef SOLARIS
#pragma enable_warn
#endif

template<class T>
GenericQueue<T>::~GenericQueue(void)
{
    if (this->items_ != NULL)
    {
        delete [] this->items_;
        this->items_ = NULL;
    }
    this->head_ = 0;
    this->tail_ = 0;
    this->size_ = 0;
    this->nItems_ = 0;
    this->bResizable_ = PR_FALSE;
}


//
// Resizes the queue
//

template<class T>
void
GenericQueue<T>::resize(const PRSize newSize)
{
    PR_ASSERT(newSize != 0);

    T* newArray = new T[newSize];

    PR_ASSERT(newArray != NULL);

    if (this->items_ != NULL)
    {
        if (this->nItems_ > newSize)    // if the queue is shrinking, then
            this->nItems_ = newSize;    // copy only what can be accomodated

        PRUint32 i;
        PRUint32 index;

        for (i = 0, index = this->head_; i < this->nItems_; i++, index++)
        {
            if (index == this->size_)
                index = 0;
            newArray[i] = this->items_[index];
        }
        delete this->items_;
    }
    this->head_ = 0;
    this->tail_ = this->nItems_;
    this->items_ = newArray;
    this->size_ = newSize;
}


//
// Returns and removes the first item in the queue.
//

template<class T>
PRStatus
GenericQueue<T>::remove(T& item)
{
    if (this->nItems_ == 0)
    {
        // remove() invoked on empty queue
        return PR_FAILURE;
    }

    PRUint32 index = this->head_; // save the index of the current head
    this->head_++;                // move the head to the next item
    this->nItems_--;              // reduce the queue count
    if (this->head_ == this->size_)
        this->head_ = 0;

    item = this->items_[index];
    return PR_SUCCESS;
}


//
// Adds the item to the tail of the queue, growing the queue if necessary
//

template<class T>
PRStatus
GenericQueue<T>::add(T item)
{
    if (this->nItems_ == this->size_)           // queue is full
    {
        if (this->isResizable())
        {
            size_t newSize = this->size_ * 2;
            this->resize(newSize);
        }
        else
            return PR_FAILURE;                  // fixed-size queue
    }

    this->items_[this->tail_] = item;
    this->tail_++;                // move the tail past the last item
    if (this->tail_ == this->size_)
        this->tail_ = 0;
    this->nItems_++;              // increase the queue count

    return PR_SUCCESS;
}


//
// Returns the number of items in the queue.
//

template<class T>
PRSize
GenericQueue<T>::length(void) const
{
    return this->nItems_;
}


//
//
//
template<class T>
PRBool
GenericQueue<T>::isResizable(void) const
{
    return this->bResizable_;
}


//
// Returns the maximum number of items that the queue can hold without
// it needing to expand.
//

template<class T>
PRSize
GenericQueue<T>::capacity(void) const
{
    return this->size_;
}


#ifdef DEBUG
template<class T>
void
GenericQueue<T>::printInfo(void) const
{
    PRSize nItems = this->length();

    char* queueType = "Fixed-size queue";
    if (this->isResizable())
        queueType = "Resizable queue";

    cout << "<GENERICQUEUE>\n"
         << "<CAPACITY> " << this->size_ << " </CAPACITY>" << endl
         << "<TYPE> " << queueType << " </TYPE>" << endl
         << "<LENGTH> " << nItems << " </LENGTH>" << endl
         << "<HEAD> " << this->head_ << " </HEAD>" << endl
         << "<TAIL> " << this->tail_ << " </TAIL>" << endl
         << "<CONTENTS>\n";

    PRUint32 index;
    PRUint32 i;
    for (i = 0, index = this->head_; i < nItems; i++, index++)
    {
        if (index == this->size_)
            index = 0;
        cout << this->items_[index] << ' ';
    }
    cout << endl << "</CONTENTS>\n";
    cout << "</GENERICQUEUE>\n";
}
#endif

#endif /* _GenericQueue_h */
