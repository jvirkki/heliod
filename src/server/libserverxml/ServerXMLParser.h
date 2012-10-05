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

#ifndef LIBSERVERXML_SERVERXMLPARSER_H
#define LIBSERVERXML_SERVERXMLPARSER_H

#include "xercesc/dom/DOMDocument.hpp"
#include "xercesc/parsers/XercesDOMParser.hpp"

//-----------------------------------------------------------------------------
// ServerXMLParser
//-----------------------------------------------------------------------------

class ServerXMLParser : private XERCES_CPP_NAMESPACE::XercesDOMParser {
public:
    ServerXMLParser();
    void parse(const char *filename);
    XERCES_CPP_NAMESPACE::DOMDocument *getDocument();
    XERCES_CPP_NAMESPACE::DOMDocument *adoptDocument();
    static int getElementFirstLineNumber(const XERCES_CPP_NAMESPACE::DOMNode *node);
    static int getElementLastLineNumber(const XERCES_CPP_NAMESPACE::DOMNode *node);

protected:
    XERCES_CPP_NAMESPACE::InputSource *resolveEntity(XERCES_CPP_NAMESPACE::XMLResourceIdentifier *resourceIdentifier);
    XERCES_CPP_NAMESPACE::InputSource *resolveEntity(const XMLCh *const publicId,
                                                     const XMLCh *const systemId,
                                                     const XMLCh *const baseURI);
    void error(const unsigned int errCode,
               const XMLCh *const errDomain,
               const ErrTypes type,
               const XMLCh *const errorText,
               const XMLCh *const systemId,
               const XMLCh *const publicId,
               const XMLSSize_t lineNum,
               const XMLSSize_t colNum);
    void startElement(const XERCES_CPP_NAMESPACE::XMLElementDecl& elemDecl,
                      const unsigned int urlId,
                      const XMLCh *const elemPrefix,
                      const XERCES_CPP_NAMESPACE::RefVectorOf< XERCES_CPP_NAMESPACE::XMLAttr >& attrList,
                      const unsigned int attrCount,
                      const bool isEmpty,
                      const bool isRoot);
    void endElement(const XERCES_CPP_NAMESPACE::XMLElementDecl& elemDecl,
                    const unsigned int urlId,
                    const bool isRoot,
                    const XMLCh *const elemPrefix);
    void handleDatatypeError(const XMLCh *systemID,
                             int lineNumber,
                             const XERCES_CPP_NAMESPACE::DOMNode *contextNode);

private:
    XMLCh *filename;
};

#endif // LIBSERVERXML_SERVERXMLPARSER_H
