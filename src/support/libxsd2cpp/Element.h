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

#ifndef LIBXSD2CPP_ELEMENT_H
#define LIBXSD2CPP_ELEMENT_H

#include "nspr.h"
#include "xercesc/dom/DOMElement.hpp"
#include "libxsd2cpp.h"
#include "ValidationException.h"

namespace LIBXSD2CPP_NAMESPACE {

using XERCES_CPP_NAMESPACE::DOMElement;

//-----------------------------------------------------------------------------
// Element
//-----------------------------------------------------------------------------

/**
 * Element is the base class for elements.
 */
class LIBXSD2CPP_EXPORT Element {
public:
    /**
     * Returns this Element's tag name.
     */
    const XMLCh *getTagName() const { return element->getTagName(); }

    /**
     * Returns the DOMElement this Element was instantiated from.
     */
    const DOMElement *getDOMElement() const { return element; }

    /**
     * Returns a boolean indicating whether two Elements are equal.
     */
    int operator==(const Element& right) const { return element->isEqualNode(right.element); }

    /**
     * Returns a boolean indicating whether two Elements are inequal.
     */
    int operator!=(const Element& right) const { return !element->isEqualNode(right.element); }

protected:
    /**
     * Instantiates an Element object based on an element from a DOM tree.
     */
    Element(const DOMElement *elementArg)
    : element(elementArg)
    { }

    virtual ~Element() { }

private:
    /**
     * Copy constructor is undefined.
     */
    Element(const Element& element);

    /**
     * Assignment operator is undefined.
     */
    Element& operator=(const Element& right);

    /**
     * The DOMElement this Element was instantiated from.
     */
    const DOMElement *element;
};

} // namespace LIBXSD2CPP_NAMESPACE

#endif // LIBXSD2CPP_ELEMENT_H
