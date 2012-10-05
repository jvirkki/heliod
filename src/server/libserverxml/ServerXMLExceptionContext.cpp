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
#include "nspr.h"
#include "xercesc/dom/DOMNode.hpp"
#include "xercesc/util/XMLString.hpp"
#include "base/ereport.h"
#include "libxsd2cpp/CString.h"
#include "libserverxml/ServerXML.h"
#include "libserverxml/ServerXMLExceptionContext.h"
#include "libserverxml/dbtlibserverxml.h"
#include "ServerXMLParser.h"

using namespace XERCES_CPP_NAMESPACE;
using namespace LIBXSD2CPP_NAMESPACE;

//-----------------------------------------------------------------------------
// ServerXMLExceptionContext::ServerXMLExceptionContext
//-----------------------------------------------------------------------------

ServerXMLExceptionContext::ServerXMLExceptionContext(const XMLCh *systemID)
: localSystemID(NULL),
  firstLineNumber(0),
  lastLineNumber(0)
{
    transcodedSystemID = XMLString::transcode(systemID);
}

ServerXMLExceptionContext::ServerXMLExceptionContext(const char *systemID)
: transcodedSystemID(NULL),
  firstLineNumber(0),
  lastLineNumber(0)
{
    localSystemID = strdup(systemID);
}

ServerXMLExceptionContext::ServerXMLExceptionContext(const LIBXSD2CPP_NAMESPACE::Element& element)
: localSystemID(NULL),
  firstLineNumber(0),
  lastLineNumber(0)
{
    const DOMNode *node = element.getDOMElement();

    const XMLCh *uri = node->getOwnerDocument()->getDocumentURI();
    transcodedSystemID = XMLString::transcode(uri);

    setContextNode(node);
}

//-----------------------------------------------------------------------------
// ServerXMLExceptionContext::~ServerXMLExceptionContext
//-----------------------------------------------------------------------------

ServerXMLExceptionContext::~ServerXMLExceptionContext()
{
    free(localSystemID);
    XMLString::release(&transcodedSystemID);
}

//-----------------------------------------------------------------------------
// ServerXMLExceptionContext::setContextLineNumber
//-----------------------------------------------------------------------------

void ServerXMLExceptionContext::setContextLineNumber(int lineNumber)
{
    if (lineNumber != 0) {
        if (firstLineNumber == 0 || lineNumber < firstLineNumber)
            firstLineNumber = lineNumber;
        if (lastLineNumber == 0 || lineNumber > lastLineNumber)
            lastLineNumber = lineNumber;
    }
}

//-----------------------------------------------------------------------------
// ServerXMLExceptionContext::setContextNode
//-----------------------------------------------------------------------------

void ServerXMLExceptionContext::setContextNode(const DOMNode *node)
{
    if (node != NULL) {
        // Find the first ancestor-or-self element that wasn't implicitly
        // instantiated
        for (;;) {
            int lineNumber = ServerXMLParser::getElementFirstLineNumber(node);
            if (lineNumber != 0)
                break;

            const DOMNode *parentNode = node->getParentNode();
            if (parentNode == NULL || parentNode->getNodeType() != DOMNode::ELEMENT_NODE)
                break;

            node = parentNode;
        }

        setContextNodeLineNumbers(node);
    }
}

//-----------------------------------------------------------------------------
// ServerXMLExceptionContext::setContextNodeLineNumbers
//-----------------------------------------------------------------------------

void ServerXMLExceptionContext::setContextNodeLineNumbers(const DOMNode *node)
{
    setContextLineNumber(ServerXMLParser::getElementFirstLineNumber(node));
    setContextLineNumber(ServerXMLParser::getElementLastLineNumber(node));

    // Recurse to include line numbers for all descendant nodes (an element may
    // be invalid because its contents are invalid)
    node = node->getFirstChild();
    while (node) {
        setContextNodeLineNumbers(node);
        node = node->getNextSibling();
    }
}

//-----------------------------------------------------------------------------
// ServerXMLExceptionContext::getContextPrefix
//-----------------------------------------------------------------------------

NSString ServerXMLExceptionContext::getContextPrefix() const
{
    const char *systemID = localSystemID ? localSystemID : transcodedSystemID;
    if (!strncmp(systemID, "file://", 7))
        systemID += 7;

    PR_ASSERT(strlen(systemID) > 0);

    NSString contextPrefix;

    if (firstLineNumber != lastLineNumber) {
        contextPrefix.printf(XP_GetAdminStr(DBT_filename_X_lines_Y_Z_prefix),
                             systemID,
                             firstLineNumber,
                             lastLineNumber);
    } else if (firstLineNumber != 0) {
        contextPrefix.printf(XP_GetAdminStr(DBT_filename_X_line_Y_prefix),
                             systemID,
                             firstLineNumber);
    } else {
        contextPrefix.printf(XP_GetAdminStr(DBT_filename_X_prefix),
                             systemID);
    }

    return contextPrefix;
}
