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

#include "xmloutput.h"

//-----------------------------------------------------------------------------
// XMLOutput::XMLOutput
//-----------------------------------------------------------------------------

XMLOutput::XMLOutput(PRFileDesc* fd_, int sizeIndentation_, int sizeQueue_)
: fd(fd_),
  sizeIndentation(sizeIndentation_),
  sizeQueue(sizeQueue_),
  countQueueFree(sizeQueue),
  sizeMaxToQueue(sizeQueue/2),
  flagOpenTag(PR_FALSE),
  countOpenElements(0),
  list(0)
{
    if (sizeQueue) {
        queue = malloc(sizeQueue);
        if (!queue) sizeQueue = 0;
    }
}

//-----------------------------------------------------------------------------
// XMLOutput::~XMLOutput
//-----------------------------------------------------------------------------

XMLOutput::~XMLOutput()
{
    flush();
    if (sizeQueue) free(queue);
}

//-----------------------------------------------------------------------------
// XMLOutput::beginElement
//-----------------------------------------------------------------------------

void XMLOutput::beginElement(const char* tag)
{
    if (flagOpenTag) {
        endList();
        output(">\n", 2);
        indent(countOpenElements - 1);
    }

    flagOpenTag = PR_TRUE;
    if (countOpenElements) indent(1);
    output('<');
    output(tag);

    countOpenElements++;
}

//-----------------------------------------------------------------------------
// XMLOutput::attribute
//-----------------------------------------------------------------------------

void XMLOutput::attribute(const char* name, const char* value)
{
    PR_ASSERT(flagOpenTag);
    if (value) {
        endList();
        output(' ');
        output(name);
        output("=\"", 2);
        outputEscaped(value);
        output('"');
    }
}

void XMLOutput::attribute(const char* name, PRInt32 value)
{
    PR_ASSERT(flagOpenTag);
    endList();
    output(' ');
    output(name);
    char string[14];
    output(string, PR_snprintf(string, sizeof(string), "=\"%ld\"", value));
}

void XMLOutput::attribute(const char* name, PRUint32 value)
{
    PR_ASSERT(flagOpenTag);
    endList();
    output(' ');
    output(name);
    char string[14];
    output(string, PR_snprintf(string, sizeof(string), "=\"%lu\"", value));
}

void XMLOutput::attribute(const char* name, PRInt64 value)
{
    PR_ASSERT(flagOpenTag);
    endList();
    output(' ');
    output(name);
    char string[24];
    output(string, PR_snprintf(string, sizeof(string), "=\"%lld\"", value));
}

void XMLOutput::attribute(const char* name, PRUint64 value)
{
    PR_ASSERT(flagOpenTag);
    endList();
    output(' ');
    output(name);
    char string[24];
    output(string, PR_snprintf(string, sizeof(string), "=\"%llu\"", value));
}

void XMLOutput::attribute(const char* name, PRFloat64 value)
{
    PR_ASSERT(flagOpenTag);
    endList();
    output(' ');
    output(name);
    char string[24];
    output(string, PR_snprintf(string, sizeof(string), "=\"%f\"", value));
}

void XMLOutput::attribute(const char* name, const PRNetAddr& value)
{
    PR_ASSERT(flagOpenTag);
    endList();
    output(' ');
    output(name);
    output("=\"", 2);
    char string[46];
    PR_NetAddrToString(&value, string, sizeof(string));
    output(string);
    output('"');
}

void XMLOutput::attributef(const char* name, const char* format, ...)
{
    PR_ASSERT(flagOpenTag);

    endList();
    output(' ');
    output(name);
    output("=\"", 2);

    flush();

    va_list args;
    va_start(args, format);
    PR_vfprintf(fd, format, args);
    va_end(args);

    output('"');
}

void XMLOutput::attributeList(const char* name, const char* value)
{
    PR_ASSERT(flagOpenTag);

    if (name == list) {
        output(' ');
        outputEscaped(value);
    } else {
        endList();
        list = name;

        output(' ');
        output(name);
        output("=\"", 2);

        outputEscaped(value);
    }
}

//-----------------------------------------------------------------------------
// XMLOutput::characters
//-----------------------------------------------------------------------------

void XMLOutput::characters(const char* string)
{
    if (flagOpenTag) {
        endList();
        output('>');
        flagOpenTag = PR_FALSE;
    }

    outputEscaped(string);
}

//-----------------------------------------------------------------------------
// XMLOutput::endElement
//-----------------------------------------------------------------------------

void XMLOutput::endElement(const char* tag)
{
    countOpenElements--;

    if (flagOpenTag) {
        endList();
        output("/>\n", 3);
        flagOpenTag = PR_FALSE;
    } else {
        output("</", 2);
        output(tag);
        output(">\n", 2);
    }

    indent(countOpenElements - 1);
}

//-----------------------------------------------------------------------------
// XMLOutput::flush
//-----------------------------------------------------------------------------

void XMLOutput::flush()
{
    if (countQueueFree < sizeQueue) {
        PR_Write(fd, queue, sizeQueue - countQueueFree);
        countQueueFree = sizeQueue;
    }
}

//-----------------------------------------------------------------------------
// XMLOutput::endList
//-----------------------------------------------------------------------------

inline void XMLOutput::endList()
{
    if (list) {
        output('"');
        list = 0;
    }
}

//-----------------------------------------------------------------------------
// XMLOutput::indent
//-----------------------------------------------------------------------------

inline void XMLOutput::indent(int count)
{
    PR_ASSERT(sizeIndentation <= 8);
    int i;
    for (i = 0; i < count; i++) output("        ", sizeIndentation);
}

//-----------------------------------------------------------------------------
// XMLOutput::outputEscaped
//-----------------------------------------------------------------------------

inline void XMLOutput::outputEscaped(const char* string)
{
    if (!string) return;

    while (*string) {
        switch (*string) {
        case '\n':
            output('\n');
            indent(countOpenElements + 1);
            break;

        case '<':
        case '>':
        case '"':
        case '\'':
        case '&':
            outputEscaped(*string);
            break;

        default:
            output(*string);
            break;
        }       
        string++;
    }
}

inline void XMLOutput::outputEscaped(char c)
{
    char escaped[7];
    output(escaped, sprintf(escaped, "&#%d;", c & 0xff));
}

//-----------------------------------------------------------------------------
// XMLOutput::output
//-----------------------------------------------------------------------------

void XMLOutput::output(const char* string)
{
    output((void*)string, strlen(string));
}

inline void XMLOutput::output(char c)
{
    output(&c, 1);
}

inline void XMLOutput::output(const void* buffer, int size)
{
    if (size <= sizeMaxToQueue) {
        if (size > countQueueFree) flush();
        memcpy((char*)queue + sizeQueue - countQueueFree, buffer, size);
        countQueueFree -= size;
    } else {
        flush();
        PR_Write(fd, buffer, size);
    }
}
