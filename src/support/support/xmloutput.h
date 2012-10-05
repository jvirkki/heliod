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

#ifndef XMLOUTPUT_H
#define XMLOUTPUT_H

#include <nspr.h>

#include "support_common.h"

//-----------------------------------------------------------------------------
// XMLOutput
//
// Simplifies generating/outputting XML documents over a PRFileDesc.
//-----------------------------------------------------------------------------

class SUPPORT_EXPORT XMLOutput {
public:
    XMLOutput(PRFileDesc* fd_, int sizeIndentation_ = 4, int sizeQueue_ = 512);
    ~XMLOutput();
    void beginElement(const char* tag);
    void attribute(const char* id, const char* value);
    void attribute(const char* id, PRInt32 value);
    void attribute(const char* id, PRUint32 value);
    void attribute(const char* id, PRInt64 value);
    void attribute(const char* id, PRUint64 value);
    void attribute(const char* id, PRFloat64 value);
    void attribute(const char* id, const PRNetAddr& value);
    void attributef(const char* id, const char* format, ...);
    void attributeList(const char* id, const char* value);
    void characters(const char* string);
    void endElement(const char* tag);
    void output(const char* string);
    void flush();

private:
    inline void endList();
    inline void indent(int count);
    inline void outputEscaped(const char* string);
    inline void outputEscaped(char c);
    inline void output(char c);
    inline void output(const void* buffer, int size);

    PRFileDesc* fd;
    int sizeIndentation;
    int sizeQueue;
    int countQueueFree;
    int sizeMaxToQueue;
    void* queue;
    PRBool flagOpenTag;
    int countOpenElements;
    const char* list;
};

#endif // XMLOUTPUT_H
