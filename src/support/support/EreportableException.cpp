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

#include <ctype.h>
#include "nspr.h"
#include "support/EreportableException.h"

//-----------------------------------------------------------------------------
// ereport_prefixlen
//-----------------------------------------------------------------------------

static int ereport_prefixlen(const char *msg)
{
    const char *p = msg;

    // Parse subsystem ID
    const char *subsystemID = p;
    while (isalpha(*p))
        p++;
    int subsystemIDLength = p - subsystemID;

    // Parse message number
    const char *messageNumber = p;
    while (isdigit(*p))
        p++;
    int messageNumberLength = p - messageNumber;

    // Get length of message ID prefix, e.g. strlen("CONF1234: ")
    if (subsystemIDLength >= 2 &&
        subsystemIDLength <= 8 &&
        messageNumberLength == 4 &&
        p[0] == ':' &&
        p[1] == ' ')
    {
        PR_ASSERT(strlen(msg) > subsystemIDLength + messageNumberLength + 2);
        return subsystemIDLength + messageNumberLength + 2;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// EreportableException::EreportableException
//-----------------------------------------------------------------------------

EreportableException::EreportableException(int degreeArg,
                                           const char *messageArg)
: degree(degreeArg)
{
    int prefixLen = ereport_prefixlen(messageArg);
    if (prefixLen > 2)
        messageID.append(messageArg, prefixLen - 2);

    description.append(messageArg + prefixLen);
}

EreportableException::EreportableException(const EreportableException& e)
: degree(e.degree),
  messageID(e.messageID),
  description(e.description)
{ }

EreportableException::EreportableException(const char *prefix,
                                           const EreportableException& e)
: degree(e.degree),
  messageID(e.messageID)
{
    description.append(prefix);
    description.append(e.description);
}

//-----------------------------------------------------------------------------
// EreportableException::~EreportableException
//-----------------------------------------------------------------------------

EreportableException::~EreportableException()
{ }

//-----------------------------------------------------------------------------
// EreportableException::setDescription
//-----------------------------------------------------------------------------

void EreportableException::setDescription(const char *descriptionArg)
{
    setDescriptionf("%s", descriptionArg);
}

//-----------------------------------------------------------------------------
// EreportableException::setDescriptionf
//-----------------------------------------------------------------------------

void EreportableException::setDescriptionf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    setDescriptionv(fmt, args);
    va_end(args);
}

//-----------------------------------------------------------------------------
// EreportableException::setDescriptionv
//-----------------------------------------------------------------------------

void EreportableException::setDescriptionv(const char *fmt, va_list args)
{
    // Note that args may contain a pointer to the existing description
    NSString newDescription;
    newDescription.printv(fmt, args);
    description = newDescription;
}
