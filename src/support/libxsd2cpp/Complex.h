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

#ifndef LIBXSD2CPP_COMPLEX_H
#define LIBXSD2CPP_COMPLEX_H

#include <stddef.h>
#include "nspr.h"
#include "xercesc/dom/DOMNodeList.hpp"
#include "libxsd2cpp.h"
#include "Element.h"

namespace LIBXSD2CPP_NAMESPACE {

using XERCES_CPP_NAMESPACE::DOMNodeList;

//-----------------------------------------------------------------------------
// CachedDocumentWrapper
//-----------------------------------------------------------------------------

/**
 * CachedDocumentWrapper wraps a Xalan-C++ XercesDocumentWrapper that's cached
 * per Xerces-C++ document.
 */
class CachedDocumentWrapper;

//-----------------------------------------------------------------------------
// Complex
//-----------------------------------------------------------------------------

/**
 * Complex is the base class for Elements that contain child Elements.
 */
class LIBXSD2CPP_EXPORT Complex : public Element {
protected:
    /**
     * Instantiates a Complex object based on an element from a DOM tree.
     */
    Complex(const DOMElement *element)
    : Element(element)
    { }

    /**
     * Helper function that returns the DOMElement * of the named child element
     * or NULL if it does not exist.  Throws DuplicateElementException if there
     * are multiple instances of the named child element.
     */
    static DOMElement *getChildElement(DOMElement *parentElement, const XMLCh *name);

    /**
     * Helper function for adding an implicitly-defined element to a DOM
     * document.  Returns the newly added DOMElement * which may be used in a
     * subsequent appendChildXML() or appendChildText() call.  The caller
     * should instantiate a child Element object.
     */
    static DOMElement *appendChildElement(DOMElement *parentElement, const XMLCh *name);

    /**
     * Helper function used in conjunction with appendChildElement to add text
     * content to a DOM document.
     */
    static void appendChildText(DOMElement *parentElement, const XMLCh *text);

    /**
     * Return a cached XercesDocumentWrapper for use in XPath operations.
     */
    static CachedDocumentWrapper& getCachedDocumentWrapper(DOMNode *node);

    /**
     * Helper function used in conjunction with appendChildElement to add the
     * result of an XPath expression to a DOM document.
     */
    static void appendChildXPathNodeSet(CachedDocumentWrapper& documentWrapper, DOMElement *parentElement, DOMElement *contextElement, const char *xpath);

    /**
     * Helper function for evaluating an XPath expression as a boolean.
     */
    static PRBool evaluateXPathBoolean(CachedDocumentWrapper& documentWrapper, DOMElement *contextElement, const char *xpath);
};

} // namespace LIBXSD2CPP_NAMESPACE

#endif // LIBXSD2CPP_COMPLEX_H
