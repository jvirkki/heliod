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

#include <signal.h>
#include "xercesc/dom/DOMElement.hpp"
#include "xercesc/dom/DOMImplementation.hpp"
#include "xercesc/dom/DOMText.hpp"
#include "xercesc/util/XMLEntityResolver.hpp"
#include "xercesc/framework/MemBufInputSource.hpp"
#include "xercesc/framework/XMLValidityCodes.hpp"
#include "xercesc/internal/XMLScanner.hpp"
#include "base/ereport.h"
#include "i18n.h"
#include "libxsd2cpp/CString.h"
#include "libserverxml/ServerXMLExceptionContext.h"
#include "libserverxml/dbtlibserverxml.h"
#include "generated/ServerXMLSchema/ServerXMLSchema_xsd.h"
#include "NSPRInputSource.h"
#include "ServerXMLException.h"
#include "ServerXMLParser.h"

using namespace XERCES_CPP_NAMESPACE;
using LIBXSD2CPP_NAMESPACE::CString;

static const XMLCh firstLineNumberKey[] = { chLatin_f, chLatin_i, chLatin_r, chLatin_s, chLatin_t, chLatin_L, chLatin_i, chLatin_n, chLatin_e, chLatin_N, chLatin_u, chLatin_m, chLatin_b, chLatin_e, chLatin_r, chNull };
static const XMLCh lastLineNumberKey[] = { chLatin_l, chLatin_a, chLatin_s, chLatin_t, chLatin_L, chLatin_i, chLatin_n, chLatin_e, chLatin_N, chLatin_u, chLatin_m, chLatin_b, chLatin_e, chLatin_r, chNull };
static const char serverXSD[] = "server.xsd";

//-----------------------------------------------------------------------------
// UnexpectedXercesException
//-----------------------------------------------------------------------------

/**
 * UnexpectedXercesException is thrown when a Xerces-C++ parser error could not
 * be mapped to a specific ServerXMLException.
 */
class UnexpectedXercesException : public EreportableException {
public:
    UnexpectedXercesException(const XMLCh *systemID,
                              int lineNumber,
                              DOMNode *node,
                              const XMLCh *xercesErrorMessage)
    : EreportableException(LOG_MISCONFIG, XP_GetAdminStr(DBT_CONF1103_xerces_error_prefix))
    {
        ServerXMLExceptionContext context(systemID);
        context.setContextLineNumber(lineNumber);

        // Don't use the context node as it's often misleading (e.g. it will
        // point to the <server> node if one mistypes "<virtual-server>")
        // XXX context.setContextNode(node);

        char *transcodedErrorMessage = XMLString::transcode(xercesErrorMessage);

        NSString error;
        error.append(context.getContextPrefix());
        error.append(getDescription());
        error.append(transcodedErrorMessage);

        setDescription(error);

        XMLString::release(&transcodedErrorMessage);
    }
};

//-----------------------------------------------------------------------------
// ServerXMLParser::ServerXMLParser
//-----------------------------------------------------------------------------

ServerXMLParser::ServerXMLParser()
{
    setDoValidation(true);
    setValidationScheme(XercesDOMParser::Val_Always);
    setDoNamespaces(true);
    setDoSchema(true);
    setLoadExternalDTD(false);
    setExternalNoNamespaceSchemaLocation(serverXSD);
    getScanner()->setEntityHandler(this);
    getScanner()->setErrorReporter(this);
    getScanner()->setPSVIHandler(this);

    // We fully validate the schema itself (the .xsd file) only in debug bits
#ifdef DEBUG
    setValidationSchemaFullChecking(true);
#else
    setValidationSchemaFullChecking(false);
#endif

    // Because Xerces-C++ provides so little context on key/keyref validation
    // errors, we delay most error reporting until ServerXMLSchema::Server
    // construction.
    setValidationConstraintFatal(false); 
}

//-----------------------------------------------------------------------------
// ServerXMLParser::parse
//-----------------------------------------------------------------------------

void ServerXMLParser::parse(const char *filename)
{
    NSPRFileInputSource source(filename);
    XercesDOMParser::parse(source);
}

//-----------------------------------------------------------------------------
// ServerXMLParser::getDocument
//-----------------------------------------------------------------------------

DOMDocument *ServerXMLParser::getDocument()
{
    // Note that we continue to own the returned DOMDocument
    return XercesDOMParser::getDocument();
}

//-----------------------------------------------------------------------------
// ServerXMLParser::adoptDocument
//-----------------------------------------------------------------------------

DOMDocument *ServerXMLParser::adoptDocument()
{
    // Caller now owns the returned DOMDocument
    return XercesDOMParser::adoptDocument();
}

//-----------------------------------------------------------------------------
// ServerXMLParser::resolveEntity
//-----------------------------------------------------------------------------

InputSource *ServerXMLParser::resolveEntity(XMLResourceIdentifier *resourceIdentifier)
{
    // Return an in-memory representation of server.xsd
    CString systemID(resourceIdentifier->getSystemId());
    if (!strcmp(systemID, serverXSD))
        return new MemBufInputSource((const XMLByte *)_ServerXMLSchema_xsd,
                                     sizeof(_ServerXMLSchema_xsd) - 1,
                                     serverXSD);
    return NULL;
}

InputSource *ServerXMLParser::resolveEntity(const XMLCh *const publicId,
                                            const XMLCh *const systemId,
                                            const XMLCh *const baseURI)
{
    // Return an in-memory representation of server.xsd
    CString systemID(systemId);
    if (!strcmp(systemID, serverXSD))
        return new MemBufInputSource((const XMLByte *)_ServerXMLSchema_xsd,
                                     sizeof(_ServerXMLSchema_xsd) - 1,
                                     serverXSD);
    return NULL;
}

//-----------------------------------------------------------------------------
// ServerXMLParser::error
//-----------------------------------------------------------------------------

void ServerXMLParser::error(const unsigned int errCode,
                            const XMLCh *const errDomain,
                            const ErrTypes type,
                            const XMLCh *const errorText,
                            const XMLCh *const systemIdArg,
                            const XMLCh *const publicId,
                            const XMLSSize_t lineNum,
                            const XMLSSize_t colNum)
{
    DOMNode *contextNode = getCurrentNode();

    const XMLCh *systemId = systemIdArg;
    if (systemId == NULL || XMLString::equals(systemId, XMLUni::fgZeroLenString))
        systemId = filename;

    // Throw specific exceptions for errors we understand
    if (XMLString::equals(errDomain, XMLUni::fgValidityDomain)) {
        switch (errCode) {
        case XMLValid::DatatypeError:
            // XXX lookup xs:simpleType documentation for acceptable values?
            handleDatatypeError(systemId, lineNum, contextNode);
            break;

        case XMLValid::IC_AbsentKeyValue:
            // Ignore missing key fields.  We'll catch MissingElementException
            // later when we try to construct the ServerXMLSchema::Server.
            return;

        case XMLValid::IC_FieldMultipleMatch:
            // Ignore multiple occurrences of a key/unique element.  We'll
            // catch DuplicateElementException later when we try to construct
            // the ServerXMLSchema::Server.
            return;

        case XMLValid::IC_DuplicateUnique:
        case XMLValid::IC_DuplicateKey:
            // Ignore multiple occurrences of a key/unique element value.
            // We'll catch DuplicateKeyException later when we try to construct
            // the ServerXMLSchema::Server.
            return;

        case XMLValid::IC_KeyRefOutOfScope:
        case XMLValid::IC_KeyNotFound:
            // Ignore unresolved key reference.  Xerces-C++ is totally useless
            // here; it won't even tell us the undefined key value.  We'll
            // catch MissingKeyException later when we try to construct the
            // ServerXMLSchema::Server.
            return;

        case XMLValid::NotEnoughElemsForCM:
        case XMLValid::EmptyNotValidForContent:
            // Ignore missing element.  We'll catch MissingElementException
            // later when we try to construct the ServerXMLSchema::Server.
            return;

        case XMLValid::ElementNotDefined:
            if (contextNode == NULL)
                throw ServerXMLException(CString(systemId), lineNum, contextNode, XP_GetAdminStr(DBT_CONF1116_expected_server));
            throw ServerXMLException(CString(systemId), lineNum, NULL, XP_GetAdminStr(DBT_CONF1117_unknown_element));

        case XMLValid::ElementNotValidForContent:
            throw ServerXMLException(CString(systemId), lineNum, contextNode, XP_GetAdminStr(DBT_CONF1102_invalid_syntax));
        }
    } else if (XMLString::equals(errDomain, XMLUni::fgXMLErrDomain)) {
        switch (errCode) {
        case XMLErrs::InvalidDocumentStructure:
            throw ServerXMLException(CString(systemId), lineNum, contextNode, XP_GetAdminStr(DBT_CONF1116_expected_server));
        }
    }

    // Throw a generic exception with the default (and most likely
    // incomprehensible) Xerces-C++ error text
    throw UnexpectedXercesException(systemId, lineNum, contextNode, errorText);
}

//-----------------------------------------------------------------------------
// ServerXMLParser::startElement
//-----------------------------------------------------------------------------

void ServerXMLParser::startElement(const XMLElementDecl& elemDecl,
                                   const unsigned int urlId,
                                   const XMLCh *const elemPrefix,
                                   const RefVectorOf< XMLAttr >& attrList,
                                   const unsigned int attrCount,
                                   const bool isEmpty,
                                   const bool isRoot)
{
    XercesDOMParser::startElement(elemDecl, urlId, elemPrefix, attrList, attrCount, isEmpty, isRoot);

    // Track the line number in the DOMNode for later use
    int lineNumber = getScanner()->getLocator()->getLineNumber();
    DOMNode *node = getCurrentNode();
    if (node != NULL)
        node->setUserData(firstLineNumberKey, (void *)lineNumber, NULL);
}

//-----------------------------------------------------------------------------
// ServerXMLParser::endElement
//-----------------------------------------------------------------------------

void ServerXMLParser::endElement(const XMLElementDecl& elemDecl,
                                 const unsigned int urlId,
                                 const bool isRoot,
                                 const XMLCh *const elemPrefix)
{
    XercesDOMParser::endElement(elemDecl, urlId, isRoot, elemPrefix);

    // Track the line number in the DOMNode for later use
    int lineNumber = getScanner()->getLocator()->getLineNumber();
    DOMNode *node = getCurrentNode();
    if (node != NULL)
        node->setUserData(lastLineNumberKey, (void *)lineNumber, NULL);
}

//-----------------------------------------------------------------------------
// ServerXMLParser::getElementFirstLineNumber
//-----------------------------------------------------------------------------

int ServerXMLParser::getElementFirstLineNumber(const DOMNode *node)
{
    if (node != NULL) {
        if (node->getNodeType() == DOMNode::ELEMENT_NODE)
            return (int)(size_t)node->getUserData(firstLineNumberKey);

        // Recurse to get our parent element's first line number
        return getElementFirstLineNumber(node->getParentNode());
    }

    return 0;
}

//-----------------------------------------------------------------------------
// ServerXMLParser::getElementLastLineNumber
//-----------------------------------------------------------------------------

int ServerXMLParser::getElementLastLineNumber(const DOMNode *node)
{
    if (node != NULL) {
        if (node->getNodeType() == DOMNode::ELEMENT_NODE)
            return (int)(size_t)node->getUserData(lastLineNumberKey);

        // Recurse to get our parent element's last line number
        return getElementLastLineNumber(node->getParentNode());
    }

    return 0;
}

//-----------------------------------------------------------------------------
// ServerXMLParser::handleDatatypeError
//-----------------------------------------------------------------------------

void ServerXMLParser::handleDatatypeError(const XMLCh *systemID,
                                          int lineNumber,
                                          const DOMNode *node)
{
    DOMElement *element = NULL;
    if (node != NULL) {
        if (node->getNodeType() == DOMNode::ELEMENT_NODE) {
            element = (DOMElement *)node;
        } else {
            DOMNode *parentNode = node->getParentNode();
            if (parentNode != NULL && parentNode->getNodeType() == DOMNode::ELEMENT_NODE)
                element = (DOMElement *)parentNode;
        }
    }
    if (element == NULL)
        return; // We only know how to handle invalid element content

    CString tagName(element->getTagName());

    char *transcodedTextContent = NULL;
    const char *begin = NULL;
    const char *end = NULL;

    if (node->getNodeType() == DOMNode::TEXT_NODE) {
        const DOMText *textNode = (const DOMText *)node;

        transcodedTextContent = XMLString::transcode(textNode->getTextContent());

        if (transcodedTextContent) {
            begin = transcodedTextContent;
            while (isspace(*begin))
                begin++;
            end = begin;
            while (isprint(*end))
                end++;
            while (end > begin && isspace(*end))
                end--;
        }
    }

    NSString error;
    if (end > begin) {
        // Got a value with at least some printable characters
        error.printf(XP_GetAdminStr(DBT_CONF1104_invalid_tag_X_value_nY),
                     tagName.getStringValue(),
                     (int)(end - begin),
                     begin);
    } else if (end && *end) {
        // There was a value, but it didn't contain any printable characters
        error.printf(XP_GetAdminStr(DBT_CONF1105_invalid_tag_X_value),
                     tagName.getStringValue());
    } else {
        // Empty value.  This element must require a non-empty value.
        error.printf(XP_GetAdminStr(DBT_CONF1106_tag_X_must_not_be_empty),
                     tagName.getStringValue());
    }

    if (transcodedTextContent != NULL)
        XMLString::release(&transcodedTextContent);

    throw ServerXMLException(CString(systemID), lineNumber, node, error);
}
