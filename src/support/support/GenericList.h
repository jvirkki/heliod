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

#ifndef _GenericList_h_
#define _GenericList_h_

#include "nspr.h"                              // NSPR declarations

#ifdef WIN32
#ifdef BUILD_SUPPORT_DLL
#define GENERICLIST_EXPORT __declspec(dllexport)
#else
#define GENERICLIST_EXPORT __declspec(dllimport)
#endif
#else
#define GENERICLIST_EXPORT
#endif

/**
 * A simple doubly-linked list class that can be used to store 
 * <code>void</code> pointers to any kind of object.
 *
 * This list is <i>not</i> multi-thread safe. Datum equality is determined
 * using pointer comparison. Being <code>void*</code> the datums are not 
 * dereferenced.
 *
 * XXX <code>void*</code> used instead of template class.
 *
 * @author  $Author: elving $
 * @version $Revision: 1.1.2.4.62.1 $ $Date: 2005/01/13 20:15:41 $ 
 * @since   iWS5.0
 */

class GENERICLIST_EXPORT GenericList
{
    public:

        /**
         * Void constructor, that constructs an empty list.
         */
        GenericList(void);

        /**
         * Copy constructor
         *
         * @param list The object that will be cloned.
         */
        GenericList(const GenericList& list);

        /**
         * operator=.
         *
         * @param list The object that will be cloned.
         * @returns    A reference to itself (after the copy).
         */
        GenericList& operator=(const GenericList& list);

        /**
         * Destructor. Clears the list.
         *
         * The memory allocated for the nodes is released.
         */
        ~GenericList(void);


        /**
         * Clears the list, deallocating the memory allocated for every node.
         */
        void flush(void);

        /**
         * Equality operator.
         *
         * Lists are considered equal if they contain the same number
         * of nodes and if the datums contained in the nodes are equal.
         *
         * @param list The list to be compared against
         * @returns    1 if the lists are equal, 0 if they are not.
         */
        int operator==(const GenericList& list) const;


        /**
         * Inequality operator.
         *
         * Lists are considered equal if they contain the same number
         * of nodes and if the datums contained in the nodes are equal.
         *
         * @param list The list to be compared against
         * @returns    1 if the lists are not equal, 0 if they are.
         */
        int operator!=(const GenericList& list) const;

        /**
         * Indexing operator.
         *
         * Returns the datum contained in the node at the specified index.
         *
         * @param index The position of the node whose datum will be returned
         * @returns     <code>NULL</code> indicates that either the value 
         *              stored in the specified node was <code>NULL</code>, or
         *              that the index specified was invalid.
         */
        void* operator[](const PRUint16 index);

        /**
         * Indicates whether the list is empty or not.
         *
         * @returns <code>PR_TRUE</code> if there are no nodes in the list.
         *          <code>PR_FALSE</code> if the list contains items.
         */
        PRBool isEmpty(void) const;

        /**
         * Returns the number of nodes in the list 
         */
        PRUint16 length(void) const;

        /**
         * Appends the specified item to the list.
         *
         * @param item The pointer to be stored in the list
         * @returns    <code>PR_SUCCESS</code> if a node containing
         *             item was successfully added to the list.
         *             <code>PR_FAILURE</code> if there was an error in 
         *             allocating memory for the new item.
         */
        PRStatus append(void* item);

        /**
         * Deletes the first node that contains a datum that matches
         * <code>item</code>
         *
         * Datum equality is determined by simple pointer comparison.
         *
         * @param item The node containing this pointer will be removed 
         * @returns    <code>PR_SUCCESS</code> if a matching node was found
         *             and removed. <code>PR_FAILURE</code> if no matching
         *             node could be found.
         */
        PRStatus remove(const void* item);

        /**
         * Deletes the node at the specified index in the list.
         *
         * @param index The index of the node to delete
         * @returns    <code>PR_SUCCESS</code> if the node was removed.
         *             <code>PR_FAILURE</code> if <code>index</code> was
         *             invalid.
         */
        PRStatus removeAt(const PRUint16 index);

        /**
         * Returns whether or not, the list contains the specified datum.
         *
         * @param item The datum to look for in the list
         * @returns    <code>PR_TRUE</code> if a node in the list contains the 
         *             specified datum. <code>PR_FALSE</code> otherwise.
         */
        PRBool contains(const void* item) const;


        /**
         * Returns a count of the number of nodes in the list that contain
         * the specified datum.
         *
         * @param item The datum to look for in the list
         * @returns    The number of nodes in the list that contain
         *             <code>item</code>.
         */
        PRUint16  matches(const void* item) const;

        /**
         * Returns the datum at the head of the list.
         *
         * @returns     <code>NULL</code> indicates that either the value 
         *              stored in the head node was <code>NULL</code>, or that
         *              that the list is empty.
         */
        void* head();

        /**
         * Returns the datum at the tail of the list.
         *
         * @returns     <code>NULL</code> indicates that either the value 
         *              stored in the tail node was <code>NULL</code>, or that
         *              that the list is empty.
         */
        void* tail();

#ifdef DEBUG
        /**
         * Performs a sanity check on the list by walking up and down
         * the list.
         */
        PRBool isValid(void) const;
#endif

    private:

        /**
         * Linked list node structure 
         */
        typedef struct listNode
        {
            /**
             * Points to the user-supplied datum 
             */
            void* data;

            /**
             * Pointer to the previous node in the list.
             *
             * This will be <code>NULL</code> for the first node.
             */
            struct listNode *prev;

            /**
             * Pointer to the next node in the list.
             *
             * This will be <code>NULL</code> for the last node.
             */
            struct listNode *next;

        } listNode;


        /**
         * The number of nodes in the list
         */
        PRUint16 length_;

        /**
         * Points to the first node in the list.
         */
        listNode* head_;

        /**
         * Poinst to the last node in the list.
         */
        listNode* tail_;
};

inline
PRBool
GenericList::isEmpty(void) const
{
    return !(this->length_);
}

inline
PRUint16
GenericList::length(void) const
{
    return this->length_;
}

#endif /* _GenericList_h_ */
