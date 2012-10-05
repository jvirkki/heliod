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

#include <stdlib.h>
#include <string.h>

#include "GenericVector.h"

GenericVector::GenericVector(int sizeInitial)
: sizeStorage(sizeInitial),
  countElements(0),
  storage(0)
{
}

GenericVector::GenericVector(const GenericVector& vector)
: sizeStorage(vector.sizeStorage),
  countElements(vector.countElements)
{
    storage = (void**)malloc(sizeStorage * sizeof(storage[0]));
    memcpy(storage, vector.storage, countElements * sizeof(storage[0]));
}

GenericVector& GenericVector::operator=(const GenericVector& right)
{
    // Check for self-assignment
    if (&right == this) return *this;

    countElements = right.countElements;

    // Get rid of any existing buffer that's too small
    if (storage && sizeStorage < countElements) {
        free(storage);
        storage = 0;
    }

    // If we need a new buffer...
    if (!storage) {
        sizeStorage = right.sizeStorage;
        storage = (void**)malloc(sizeStorage * sizeof(storage[0]));
    }

    memcpy(storage, right.storage, countElements * sizeof(storage[0]));

    return *this;
}

GenericVector::~GenericVector()
{
    free(storage);
}

void* GenericVector::append(void* element)
{
    if (!storage) {
        // Initial allocation
        storage = (void**)malloc(sizeStorage * sizeof(storage[0]));
    } else if (countElements >= sizeStorage) {
        // Double storage size
        sizeStorage *= 2;
        storage = (void**)realloc(storage, sizeStorage * sizeof(storage[0]));
    }

    storage[countElements++] = element;

    return element;
}

void* GenericVector::pop(void)
{
    if (countElements < 1)
        return NULL;

    return storage[--countElements];
}

int GenericVector::contains(void* element)
{
    int count = 0;
    int i;
    for (i = 0; i < countElements; i++) {
        if (storage[i] == element) count++;
    }

    return count;
}
