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

#ifndef GENERICVECTOR_H
#define GENERICVECTOR_H

#include "support_common.h"

//-----------------------------------------------------------------------------
// GenericVector 
//
// Stores void*'s by value in a vector that allows efficient indexing and
// iterating.  The vector is grown as needed.
//-----------------------------------------------------------------------------

class SUPPORT_EXPORT GenericVector {
public:
    GenericVector(int sizeInitial = 64);
    GenericVector(const GenericVector& vector);
    GenericVector& operator=(const GenericVector& right);
    ~GenericVector();

    // Get the element stored at index i.  Caller must be careful to not exceed
    // the vector's length().
    void* operator[](int i) const { return storage[i]; }
    void*& operator[](int i) { return storage[i]; }

    // Returns 1 when the vector is empty, 0 otherwise
    int isEmpty() const { return countElements == 0; }

    // Returns the number of elements in the vector
    int length() const { return countElements; }

    // Adds an element to the end of the vector
    void* append(void* element);

    // Adds an element to the end of the vector
    void push(void* element) { append(element); }

    // Removes an from the end of the vector
    void* pop();

    // Returns the number of times element appears in the vector
    int contains(void* element);

private:
    int sizeStorage;
    int countElements;
    void** storage;
};

//-----------------------------------------------------------------------------
// PtrVector<T> 
//
// Stores pointers by value in a vector that allows efficient indexing and
// iterating.  The vector is grown as needed.
//-----------------------------------------------------------------------------

template <class T>
class SUPPORT_EXPORT PtrVector : private GenericVector {
public:
    PtrVector(int sizeInitial = 1) : GenericVector(sizeInitial) { }
    PtrVector(const PtrVector<T>& vector) : GenericVector(vector) { }
    PtrVector<T>& operator=(const PtrVector<T>& right) { GenericVector::operator=(right); return *this; }
    T* operator[](int i) const { return (T*)GenericVector::operator[](i); }
    T*& operator[](int i) { return *(T**)&GenericVector::operator[](i); }
    int isEmpty() const { return GenericVector::isEmpty(); }
    int length() const { return GenericVector::length(); }
    T* append(T* element) { return (T*)GenericVector::append((void *)element); }
    void push(T* element) { GenericVector::push((void *)element); }
    T* pop() { return (T*)GenericVector::pop(); }
    int contains(T* element) { return GenericVector::contains((void *)element); }
};

#endif // GENERICVECTOR_H
