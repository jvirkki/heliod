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

#ifndef LIBSERVERXML_SERVERXMLEXCEPTIONCONTEXT_H
#define LIBSERVERXML_SERVERXMLEXCEPTIONCONTEXT_H

#if defined(XP_WIN32) && defined(BUILD_LIBSERVERXML_DLL)
#define LIBSERVERXML_EXPORT __declspec(dllexport)
#else
#define LIBSERVERXML_EXPORT
#endif

#include <stdarg.h>
#include "xercesc/util/XercesDefs.hpp"
#include "xercesc/dom/DOMNode.hpp"
#include "generated/ServerXMLSchema/Server.h"
#include "support/EreportableException.h"

//-----------------------------------------------------------------------------
// ServerXMLExceptionContext
//-----------------------------------------------------------------------------

/**
 * ServerXMLExceptionContext is a helper class for constructing
 * EreportableExceptions that contain server.xml context.
 */
class LIBSERVERXML_EXPORT ServerXMLExceptionContext {
public:
    /**
     * Construct a ServerXMLExceptionContext from a URI.
     */
    ServerXMLExceptionContext(const XMLCh *systemID);

    /**
     * Construct a ServerXMLExceptionContext from a URI.
     */
    ServerXMLExceptionContext(const char *systemID);

    /**
     * Construct a ServerXMLExceptionContext from an Element.
     */
    ServerXMLExceptionContext(const LIBXSD2CPP_NAMESPACE::Element& element);

    ~ServerXMLExceptionContext();

    /**
     * Set additional context to be prepended to the error description.
     */
    void setContextLineNumber(int lineNumber);

    /**
     * Set additional context to be prepended to the error description.
     */
    void setContextNode(const XERCES_CPP_NAMESPACE::DOMNode *node);

    /**
     * Return a formatted, localized context prefix suitable for logging.
     */
    NSString getContextPrefix() const;

private:
    /**
     * Copy constructor is undefined.
     */
    ServerXMLExceptionContext(const ServerXMLExceptionContext&);

    /**
     * Assignment operator is undefined.
     */
    ServerXMLExceptionContext& operator=(const ServerXMLExceptionContext&);

    /**
     * Set the line number(s) to be prepended to the error description.
     */
    void setContextNodeLineNumbers(const XERCES_CPP_NAMESPACE::DOMNode *node);

    char *localSystemID;
    char *transcodedSystemID;
    int firstLineNumber;
    int lastLineNumber;
};

#endif // LIBSERVERXML_SERVERXMLEXCEPTIONCONTEXT_H
