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

#ifndef SUPPORT_EREPORTABLEEXCEPTION_H
#define SUPPORT_EREPORTABLEEXCEPTION_H

#ifdef XP_WIN32
#ifdef BUILD_SUPPORT_DLL
#define EREPORTABLEXCEPTION_EXPORT __declspec(dllexport)
#else
#define EREPORTABLEXCEPTION_EXPORT __declspec(dllimport)
#endif
#else
#define EREPORTABLEXCEPTION_EXPORT
#endif

#include "support/NSString.h"

//-----------------------------------------------------------------------------
// EreportableException
//-----------------------------------------------------------------------------

/**
 * EreportableException is the base class for exceptions that can be logged via
 * ereport().  At a minimum, an EreportableException contains an NSAPI error
 * log degree, message ID (consisting of a subsystem name and 4 digit error
 * number), and a localized error description.
 *
 * It is not intended that EreportableExceptions be thrown during request
 * processing.  As a result, there is no builtin support for capturing
 * request-time context.
 */
class EREPORTABLEXCEPTION_EXPORT EreportableException {
public:
    /**
     * Construct an EreportableException from the given NSAPI log degree and
     * localized message.  The message string must begin with a message ID
     * prefix, e.g. "CORE1234: ".
     */
    EreportableException(int degree, const char *message);

    /**
     * Construct an EreportableException based on an existing exception.  A
     * derived class's constructor may then modify the description (e.g.,
     * prepend additional context) using setDescription(), etc.
     */
    EreportableException(const EreportableException& e);

    /**
     * Construct an EreportableException based on an existing exception,
     * prepending additional context.
     */
    EreportableException(const char *prefix, const EreportableException& e);

    virtual ~EreportableException();

    /**
     * Return the NSAPI error degree, e.g. LOG_FAILURE.
     */
    int getDegree() const { return degree; }

    /**
     * Return the message ID, e.g. "CORE1234".
     */
    const char *getMessageID() const { return messageID; }

    /**
     * Return the localized error description.  The error description does not
     * include the message ID prefix.
     */
    const char *getDescription() const { return description; }

protected:
    /**
     * Assignment operator is undefined.
     */
    EreportableException& operator=(const EreportableException& e);

    /**
     * Set the error description.
     */
    void setDescription(const char *description);

    /**
     * Set the error description.
     */
    void setDescriptionf(const char *fmt, ...);

    /**
     * Set the error description.
     */
    void setDescriptionv(const char *fmt, va_list args);

private:
    int degree;
    NSString messageID;
    NSString description;
};

#endif // SUPPORT_EREPORTABLEEXCEPTION_H
