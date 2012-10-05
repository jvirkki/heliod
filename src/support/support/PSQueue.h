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

#ifndef _PSQueue_h
#define _PSQueue_h

#ifdef DEBUG
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
#include <iomanip>
using namespace std;
#else
#include <iostream.h>                    // standard C++ header file
#include <iomanip.h>                     // IOstream manipulators - setw
#endif
#endif
#include "NsprWrap/CriticalSection.h"    // CriticalSection/SafeLock class
#include "NsprWrap/ConditionVar.h"       // ConditionVar class
#include "support/GenericQueue.h"        // GenericQueue<T> class
#include "support_common.h"              // SUPPORT_EXPORT


/**
 * A fixed-size queue of type T that can be used by
 * publisher-subscriber threads to exchange data.
 *
 * Implements a FIFO queue upon which subscriber threads block
 * awaiting data to be put onto the queue by publisher threads.
 *
 * @author  $Author: elving $
 * @version $Revision: 1.1.2.1.62.1 $
 * @since   iWS5.0
 */

template<class T>
class SUPPORT_EXPORT PSQueue
{
    public:

        /**
         * Constructor specifying the maximum size of the queue.
         */
        PSQueue(const PRSize maxItems);

        /**
         * Destructor
         *
         * Empties the queue.
         */
        ~PSQueue(void);

        /**
         * Appends the specified item to the queue
         *
         * @param item The object to be appended to the queue
         * @returns    <tt>PR_FAILURE</tt> if an error occurs while appending
         *             the data or if the queue is full.
         */
        PRStatus put(T item);

        /**
         * Removes an item from the queue and returns it via the 
         * <code>item</code> reference.
         *
         * If the queue is empty, this method blocks until an item becomes 
         * available or until the queue is invalidated.
         *
         * @param item Points to the item removed from the queue
         * @returns    <tt>PR_SUCCESS</tt> if an item was successfully dequeued.
         *              <tt>PR_FAILURE</tt> if the queue was invalidated
         */
        PRStatus get(T& item);

        /**
         * Returns the number of items in the queue
         */
        PRSize length(void);

        /**
         * Returns the maximum number of items that the queue can hold
         */
        PRSize capacity(void);

        /**
         * Invalidate the queue indicating that it is no longer to be
         * used.
         *
         * This wakes up all threads that are waiting to remove
         * items from the queue (<code>removeNext</code> will return
         * <tt>PR_FAILURE</tt>).
         */
        void invalidate(void);

        /**
         * Tester method to determine if the queue has been invalidated or
         * not
         *
         * @returns If the queue has been invalidated, this method
         *          returns <tt>PR_TRUE</tt>, else it returns <tt>PR_FALSE</tt>
         */
        PRBool isInvalid(void);

    private:

        /**
         * The queue of elements to which access is
         * serialized via mutexes and condition variables.
         */
        GenericQueue<T>* queue_;

        /**
         * The mutex object used to synchronize access to the queue
         */
        CriticalSection lock_;

        /**
         * The ConditionVar object that is used to notify threads
         * that are waiting to remove items from the queue.
         *
         * If a thread requests to remove an item from the queue and the queue 
         * is currently empty, then the thread waits on this variable
         */
        ConditionVar* notEmpty_;

        /**
         * Indicates whether the queue is valid or not
         */
        PRBool bValid_;

        /**
         * Copy constructor
         *
         * Don't allow users to copy-construct PSQueue objects
         */
        PSQueue(const PSQueue<T>& queue);

        /**
         * operator=
         *
         * Don't allow users to clone PSQueue objects
         */
        PSQueue<T>& operator=(const PSQueue<T>& queue);
};



/*
 * Void constructor
 */
template<class T>
PSQueue<T>::PSQueue(const PRSize maxItems) : bValid_(PR_TRUE)
{
    this->queue_ = new GenericQueue<T>(maxItems, PR_FALSE);
    PR_ASSERT(this->queue_ != NULL);

    this->notEmpty_ = new ConditionVar(this->lock_);
    PR_ASSERT(this->notEmpty_ != NULL);

}


/*
 * Destructor invalidates the queue so that any threads that are waiting
 * for items to be available will be woken up 
 */
template<class T>
PSQueue<T>::~PSQueue(void)
{
    if (this->queue_)
    {
        this->invalidate();
        SafeLock guard(this->lock_);
        if (this->queue_)
        {
            delete this->queue_;
            this->queue_ = NULL;
        }
    }
}


/*
 * Adds the specified item to the queue in a thread-safe manner
 * If the method returns PR_FAILURE, that means that the queue is full or has
 * been invalidated and is not to be used any more.
 */
template<class T>
PRStatus
PSQueue<T>::put(T item)
{
    SafeLock guard(this->lock_);

    if (this->bValid_ == PR_FALSE)
        return PR_FAILURE;

    if (this->queue_->add(item) == PR_FAILURE)
        return PR_FAILURE;

    this->notEmpty_->notify();

    return PR_SUCCESS;
}

/*
 * Waits until an item is available on the queue and then removes and 
 * returns it. If the method returns PR_FAILURE, that means that the queue has
 * been invalidated and is not to be used any more.
 */
template<class T>
PRStatus
PSQueue<T>::get(T& item)
{
    SafeLock guard(this->lock_);

    if (this->bValid_ == PR_FALSE)
        return PR_FAILURE;

    while (this->queue_->length() == 0)
    {
        this->notEmpty_->wait();

        if (this->bValid_ == PR_FALSE)
            return PR_FAILURE;
    }
    return this->queue_->remove(item);
}


/*
 * Returns the number of items in the queue
 */
template<class T>
PRSize
PSQueue<T>::length(void)
{
    SafeLock guard(this->lock_);
    return this->queue_->length();
}

/*
 * Returns the maximum number of items that the queue can hold
 */
template<class T>
PRSize
PSQueue<T>::capacity(void)
{
    SafeLock guard(this->lock_);
    return this->queue_->capacity();
}


/*
 * Sets the queue state to indicate that it has been invalidated
 * Wakes up all threads that are waiting to remove items from the queue
 */
template<class T>
void
PSQueue<T>::invalidate(void)
{
    SafeLock guard(this->lock_);

    this->bValid_ = PR_FALSE;

    this->notEmpty_->notifyAll();
}


/*
 * Returns PR_TRUE if the queue is invalid, PR_FALSE if it isn't
 */
template<class T>
PRBool
PSQueue<T>::isInvalid(void)
{
    SafeLock guard(this->lock_);

    if (this->bValid_ == PR_TRUE)
        return PR_FALSE;
    else
        return PR_TRUE;
}

#ifdef SOLARIS
#pragma disable_warn
#endif

/*
 * Copy constructor
 * This method is private so that users cannot copy-construct PSQueue
 * objects
 */
template<class T>
PSQueue<T>::PSQueue(const PSQueue<T>& queue)
{
    // PSQueue: copy construction not allowed
}


/*
 * operator=
 * This method is private so that users cannot clone PSQueue
 * objects
 */
template<class T>
PSQueue<T>&
PSQueue<T>::operator=(const PSQueue<T>& queue)
{
    // PSQueue: operator= not allowed"
    return *this;
}

#ifdef SOLARIS
#pragma enable_warn
#endif

#endif /* _PSQueue_h */
