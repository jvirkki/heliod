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

#ifndef PRIME_H
#define PRIME_H

#include <math.h>

//-----------------------------------------------------------------------------
// findPrime
//
// Returns the lowest prime greater than or equal to value by brute force.  3
// is the lowest value that will be returned.
//
// This works well on small numbers (e.g. the kind you'd find as hash table
// modulos).
//-----------------------------------------------------------------------------

inline int findPrime(int value)
{
    // 3 is the lowest prime we'll return
    if (value <= 3) return 3;

    // Only odd numbers are prime
    if ((value & 1) == 0) value++;

    for (;;) {
        // Find square root of value, erring on the safe (large) side
        int root = (int)floor(sqrt((double)value)) + 1;

        // Check to see if value is evenly divisible by any number smaller than
        // its square root
        int divisor;
        for (divisor = 3; divisor <= root; divisor++) {
            if ((value % divisor) == 0) break;
        }

        // If it wasn't divisible, it's prime
        if (divisor > root) return value;

        // Try the next odd number
        value += 2;
    }
}

#endif // PRIME_H
