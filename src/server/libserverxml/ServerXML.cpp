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

#include "xercesc/sax2/Attributes.hpp"
#include "xercesc/sax2/DefaultHandler.hpp"
#include "xercesc/sax2/SAX2XMLReader.hpp"
#include "xercesc/sax2/XMLReaderFactory.hpp"
#include "xercesc/util/PlatformUtils.hpp"
#include "xalanc/XPath/XPathEvaluator.hpp"
#include "generated/ServerXMLSchema/Server.h"
#include "base/file.h"
#include "base/ereport.h"
#include "libxsd2cpp/CString.h"
#include "libserverxml/ServerXML.h"
#include "libserverxml/ServerXMLExceptionContext.h"
#include "libserverxml/dbtlibserverxml.h"
#include "NSPRInputSource.h"
#include "ServerXMLException.h"
#include "ServerXMLParser.h"

using namespace XERCES_CPP_NAMESPACE;
using namespace XALAN_CPP_NAMESPACE;
using LIBXSD2CPP_NAMESPACE::CString;

//-----------------------------------------------------------------------------
// ServerXMLSummarySAXHandler
//-----------------------------------------------------------------------------

class ServerXMLSummarySAXHandler : public DefaultHandler {
public:
    ServerXMLSummarySAXHandler();
    void startElement(const XMLCh *const uri,
                      const XMLCh *const localname,
                      const XMLCh *const qname,
                      const Attributes& attrs);
    void endElement(const XMLCh *const uri,
                    const XMLCh *const localname,
                    const XMLCh *const qname);
    void characters(const XMLCh *const s,
                    const unsigned len);
    void warning(const SAXParseException& e) { }
    void error(const SAXParseException& e) { }
    void fatalError(const SAXParseException& e) { }

    int depth;
    NSString *content;
    NSString user;
    NSString platform;
    NSString tempPath;
};

//-----------------------------------------------------------------------------
// ServerXMLSummarySAXHandler::ServerXMLSummarySAXHandler
//-----------------------------------------------------------------------------

ServerXMLSummarySAXHandler::ServerXMLSummarySAXHandler()
: depth(0),
  content(NULL)
{ }

//-----------------------------------------------------------------------------
// ServerXMLSummarySAXHandler::startElement
//-----------------------------------------------------------------------------

void ServerXMLSummarySAXHandler::startElement(const XMLCh *const uri,
                                              const XMLCh *const localname,
                                              const XMLCh *const qname,
                                              const Attributes& attrs)
{
    // Tell characters() where to store any character data
    CString name(localname);
    if (!strcmp(name, "user")) {
        content = &user;
    } else if (!strcmp(name, "platform")) {
        content = &platform;
    } else if (!strcmp(name, "temp-path")) {
        content = &tempPath;
    } else {
        content = NULL;
    }

    // If we've already seen a value for this element name...
    if (content != NULL && content->length() != 0) {
        // Is this element a direct child of <server>?
        if (depth != 1) {
            // Yes, tell characters() to treat this as the canonical value
            content->clear();
        } else {
            // No, tell characters() to ignore this value
            content = NULL;
        }
    }

    depth++;
}

//-----------------------------------------------------------------------------
// ServerXMLSummarySAXHandler::endElement
//-----------------------------------------------------------------------------

void ServerXMLSummarySAXHandler::endElement(const XMLCh *const uri,
                                            const XMLCh *const localname,
                                            const XMLCh *const qname)
{
    depth--;

    content = NULL;
}

//-----------------------------------------------------------------------------
// ServerXMLSummarySAXHandler::characters
//-----------------------------------------------------------------------------

void ServerXMLSummarySAXHandler::characters(const XMLCh *const s,
                                            const unsigned len)
{
    if (content != NULL)
        content->append(CString(s, len));
}

//-----------------------------------------------------------------------------
// ServerXMLSummary::ServerXMLSummary
//-----------------------------------------------------------------------------

ServerXMLSummary::ServerXMLSummary(const char *filename,
                                   const char *userArg,
                                   const char *platformArg,
                                   const char *tempPathArg)
{
    if (userArg && *userArg)
        user = userArg;

    if (platformArg && *platformArg)
        platform = platformArg;

    if (tempPathArg && *tempPathArg) {
        tempPath = tempPathArg;
    } else {
        // Attempt to extract a unique instance name from the server.xml path
        NSString instancePath;
        instancePath.append(filename);
        instancePath.append("/../..");
        char *canonicalInstancePath = file_canonicalize_path(instancePath);
        char *instanceName = file_basename(canonicalInstancePath);

        // Append the instance name to the OS temporary directory
#ifdef XP_WIN32
        const char *systemTempPath = getenv("TEMP");
        if (!systemTempPath)
            systemTempPath = getenv("TMP");
        if (!systemTempPath)
            systemTempPath = "C:\\TEMP";
#else
        const char *systemTempPath = "/tmp";
#endif
        tempPath = systemTempPath;
        if (*instanceName) {
            tempPath.append(FILE_PATHSEP);
            tempPath.append(instanceName);
        }

        FREE(instanceName);
        FREE(canonicalInstancePath);
    }

    // Canonicalize the temporary directory
    char *canonicalTempPath = file_canonicalize_path(tempPath);
    tempPath = canonicalTempPath;
    FREE(canonicalTempPath);

#ifdef XP_UNIX
    // pid file lives in the temporary directory
    pidFile.append(tempPath);
    pidFile.append("/pid");
#endif
}

//-----------------------------------------------------------------------------
// ServerXMLSummary::parse
//-----------------------------------------------------------------------------

ServerXMLSummary *ServerXMLSummary::parse(const char *filename)
{
    SAX2XMLReader *parser = XMLReaderFactory::createXMLReader();
    parser->setExitOnFirstFatalError(false);
    parser->setFeature(XMLUni::fgXercesSchema, false);
    parser->setFeature(XMLUni::fgSAX2CoreNameSpaces, false);
    parser->setFeature(XMLUni::fgSAX2CoreValidation, false);

    ServerXMLSummarySAXHandler handler;
    parser->setContentHandler(&handler);
    parser->setErrorHandler(&handler);

    try {
        NSPRFileInputSource source(filename);
        parser->parse(source);
    }
    catch (const EreportableException&) {
        // We (e.g. NSPRBinInputStream) throw EreportableException on
        // unrecoverable errors
        throw;
    }
    catch (...) {
        // Ignore XML parser exceptions
    }

    return new ServerXMLSummary(filename, handler.user, handler.platform, handler.tempPath);
}

//-----------------------------------------------------------------------------
// ServerXML::parse
//-----------------------------------------------------------------------------

ServerXML *ServerXML::parse(const char *filename)
{
    // Parse the XML.  This may throw EreportableException.
    ServerXMLParser parser;
    parser.parse(filename);

    ServerXMLSchema::Server *server;
    try {
        // At this point, the parser owns the DOMDocument
        DOMDocument *parserDocument = parser.getDocument();
        parserDocument->normalizeDocument();

        // Instantiate ServerXMLSchema objects from the parser's DOMDocument.
        // This may throw ValidationException.
        server = ServerXMLSchema::Server::createServerInstance(parserDocument->getDocumentElement());
    }
    catch (const EreportableException&) {
        // Exception from ServerXMLParser
        throw;
    }
    catch (const LIBXSD2CPP_NAMESPACE::UndefinedKeyException& e) {
        // Reference to undefined key
        CString selector(e.getKeySelectorTagName());
        CString field(e.getKeyFieldTagName());
        CString value(e.getValue());
        NSString error;
        error.printf(XP_GetAdminStr(DBT_CONF1110_selector_X_field_Y_key_Z_undefined),
                     selector.getStringValue(),
                     field.getStringValue(),
                     value.getStringValue());
        throw ServerXMLException(filename, 0, e.getDOMNode(), error);
    }
    catch (const LIBXSD2CPP_NAMESPACE::DuplicateUniqueValueException& e) {
        // Multiple definitions of a key or unique value
        CString tagName(e.getFirstDOMElement()->getTagName());
        CString value(e.getValue());
        NSString error;
        error.printf(XP_GetAdminStr(DBT_CONF1109_tag_X_value_Y_duplicates_line_Z),
                     tagName.getStringValue(),
                     value.getStringValue(),
                     ServerXMLParser::getElementFirstLineNumber(e.getFirstDOMElement()));
        throw ServerXMLException(filename, 0, e.getSecondDOMElement(), error);
    }
    catch (const LIBXSD2CPP_NAMESPACE::DuplicateElementException& e) {
        // Multiple definitions of an element that can appear only once
        CString tagName(e.getFirstDOMElement()->getTagName());
        NSString error;
        error.printf(XP_GetAdminStr(DBT_CONF1108_tag_X_previously_defined_on_line_Y),
                     tagName.getStringValue(),
                     ServerXMLParser::getElementFirstLineNumber(e.getFirstDOMElement()));
        throw ServerXMLException(filename, 0, e.getSecondDOMElement(), error);
    }        
    catch (const LIBXSD2CPP_NAMESPACE::MissingElementException& e) {
        // Missing required element
        CString tagName(e.getMissingElementTagName());
        NSString error;
        error.printf(XP_GetAdminStr(DBT_CONF1107_tag_X_missing),
                     tagName.getStringValue());
        throw ServerXMLException(filename, 0, e.getParentDOMElement(), error);
    }
    catch (const LIBXSD2CPP_NAMESPACE::ValidationException& e) {
        // Unexpected ServerXML::Schema exception
        throw ServerXMLException(filename, 0, e.getDOMNode(), XP_GetAdminStr(DBT_CONF1102_invalid_syntax));
    }
    catch (...) {
        // Unexpected exception
        throw ServerXMLException(filename, 0, NULL, XP_GetAdminStr(DBT_CONF1101_unexpected_error));
    }

    // We successfully instantiated a complete set of ServerXMLSchema objects.
    // Relieve the parser of its DOMDocument and transfer ownership to a new
    // ServerXML instance.
    DOMDocument *adoptedDocument = parser.adoptDocument();
    return new ServerXML(filename, server, adoptedDocument);
}

//-----------------------------------------------------------------------------
// ServerXML::getDOMDocument
//-----------------------------------------------------------------------------

const DOMDocument *ServerXML::getDOMDocument()
{
    return document;
}

//-----------------------------------------------------------------------------
// ServerXML::ServerXML
//-----------------------------------------------------------------------------

ServerXML::ServerXML(const char *filename,
                     ServerXMLSchema::Server *serverArg,
                     DOMDocument *documentArg)
: ServerXMLSummary(filename,
                   serverArg->getUser() ? serverArg->getUser()->getStringValue() : NULL,
                   serverArg->platform,
                   serverArg->getTempPath() ? serverArg->getTempPath()->getStringValue() : NULL),
  server(*serverArg),
  document(documentArg)
{ }

//-----------------------------------------------------------------------------
// ServerXML::~ServerXML
//-----------------------------------------------------------------------------

ServerXML::~ServerXML()
{
    delete &server;
    document->release();
}
