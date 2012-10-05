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

#ifndef NVPAIRS_H
#define NVPAIRS_H

#include "httpdaemon/libdaemon.h"

struct pblock;

class HTTPDAEMON_DLL NVPairs {
public:
    NVPairs(int sizePblock = 11);
    NVPairs(const NVPairs& nvpairs);
    ~NVPairs();
    NVPairs& operator=(const NVPairs& nvpairs);

    // Add a name+value pair.  The name and value are copied.
    void addPair(const char* name, const char* value);
    void addPair(const char* name, int value);

    // Parse a string, adding all name=value pairs.  The names and values are
    // copied.  Returns number of pairs added.
    int addPairs(const char* string);

    // Find a given name.  Do not free this storage.
    const char* findName(int i) const;

    // Find the value associated with name.  Do not free this storage.
    virtual const char* findValue(const char* name) const;

    // Remove the name+value pair associated with name.
    int removePair(const char* name);

    // Access to the underlying pblock
    pblock* getPblock() { return pb; }
    const pblock* getPblock() const { return pb; }
    pblock* dup() const;

private:
    pblock* pb;
};

#endif // NVPAIRS_H
