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

#ifndef LIBXSD2CPP_VALIDATIONEXCEPTION_H
#define LIBXSD2CPP_VALIDATIONEXCEPTION_H

#include "xercesc/dom/DOMElement.hpp"
#include "xercesc/dom/DOMNode.hpp"
#include "xercesc/util/XMLString.hpp"
#include "libxsd2cpp.h"

namespace LIBXSD2CPP_NAMESPACE {

using XERCES_CPP_NAMESPACE::DOMElement;
using XERCES_CPP_NAMESPACE::DOMNode;
using XERCES_CPP_NAMESPACE::XMLString;

//-----------------------------------------------------------------------------
// ValidationException
//-----------------------------------------------------------------------------

/**
 * Base class for exceptions related to document validation.
 */
class LIBXSD2CPP_EXPORT ValidationException {
public:
    /**
     * Returns the DOMNode that triggered the validation exception.
     */
    const DOMNode *getDOMNode() const
    {
        return node;
    }

protected:
    ValidationException(const DOMNode *nodeArg)
    : node(nodeArg)
    { }

private:
    /**
     * The DOMNode that triggered the validation exception.
     */
    const DOMNode *node;
};

//-----------------------------------------------------------------------------
// InvalidValueException
//-----------------------------------------------------------------------------

/**
 * Exception triggered by invalid character data.
 */
class InvalidValueException : public ValidationException {
public:
    InvalidValueException(const DOMNode *nodeArg)
    : ValidationException(nodeArg)
    { }
};

//-----------------------------------------------------------------------------
// DuplicateElementException
//-----------------------------------------------------------------------------

/**
 * Exception triggered by multiple definitions of an element that may appear at
 * most once.
 */
class DuplicateElementException : public ValidationException {
public:
    DuplicateElementException(const DOMElement *firstElementArg,
                              const DOMElement *secondElementArg)
    : ValidationException(secondElementArg),
      firstElement(firstElementArg),
      secondElement(secondElementArg)
    { }

    /**
     * Returns the first DOMElement for the multiply defined element.
     */
    const DOMElement *getFirstDOMElement() const
    {
        return firstElement;
    }

    /**
     * Returns the second DOMElement for the multiply defined element.
     */
    const DOMElement *getSecondDOMElement() const
    {
        return secondElement;
    }

private:
    /**
     * The first DOMElement for the multiply defined element.
     */
    const DOMElement *firstElement;

    /**
     * The second DOMElement for the multiply defined element.
     */
    const DOMElement *secondElement;
};

//-----------------------------------------------------------------------------
// MissingElementException
//-----------------------------------------------------------------------------

/**
 * Exception triggered by the absence of a required element.
 */
class MissingElementException : public ValidationException {
public:
    MissingElementException(const DOMElement *parentElementArg, const XMLCh *missingElementTagNameArg)
    : ValidationException(parentElementArg),
      parentElement(parentElementArg)
    {
        missingElementTagName = XMLString::replicate(missingElementTagNameArg);
    }

    MissingElementException(const MissingElementException& missingElementException)
    : ValidationException(missingElementException.parentElement),
      parentElement(missingElementException.parentElement)
    {
        missingElementTagName = XMLString::replicate(missingElementException.missingElementTagName);
    }

    ~MissingElementException()
    {
        XMLString::release(&missingElementTagName);
    }

    MissingElementException& operator=(const MissingElementException& right)
    {
        if (&right != this) {
            ValidationException::operator=(right);
            XMLString::release(&missingElementTagName);
            parentElement = right.parentElement;
            missingElementTagName = XMLString::replicate(right.missingElementTagName);
        }
        return *this;
    }

    /**
     * Returns the DOMElement that should have contained the missing element.
     */
    const DOMElement *getParentDOMElement() const
    {
        return parentElement;
    }

    /**
     * Returns the tag name of the missing element.  The returned string is
     * valid only while the exception is in scope.
     */
    const XMLCh *getMissingElementTagName() const
    {
        return missingElementTagName;
    }

private:
    /**
     * The DOMElement that should have contained the missing element.
     */
    const DOMElement *parentElement;

    /**
     * The dynamically allocated tag name of the missing element.
     */
    XMLCh *missingElementTagName;
};

//-----------------------------------------------------------------------------
// DuplicateUniqueValueException
//-----------------------------------------------------------------------------

/**
 * Exception triggered by multiple definitions of a given key or unique value.
 */
class DuplicateUniqueValueException : public ValidationException {
public:
    DuplicateUniqueValueException(const DOMElement *firstElementArg,
                                  const DOMElement *secondElementArg)
    : ValidationException(secondElementArg),
      firstElement(firstElementArg),
      secondElement(secondElementArg)
    { }

    /**
     * Returns the first DOMElement that defines the multiply defined key or
     * unique value.
     */
    const DOMElement *getFirstDOMElement() const
    {
        return firstElement;
    }

    /**
     * Returns the second DOMElement that defines the multiply defined key or
     * unique value.
     */
    const DOMElement *getSecondDOMElement() const
    {
        return secondElement;
    }

    /**
     * Returns the multiply defined key or unique value.
     */
    const XMLCh *getValue() const
    {
        return firstElement->getTextContent();
    }

private:
    /**
     * The first DOMElement that defines the multiply defined key or unique
     * value.
     */
    const DOMElement *firstElement;

    /**
     * The second DOMElement that defines the multiply defined key or unique
     * value.
     */
    const DOMElement *secondElement;
};

//-----------------------------------------------------------------------------
// UndefinedKeyException
//-----------------------------------------------------------------------------

/**
 * Exception triggered by a reference to an undefined key.
 */
class UndefinedKeyException : public ValidationException {
public:
    UndefinedKeyException(const DOMElement *keyrefElementArg,
                          const XMLCh *keySelectorTagNameArg,
                          const XMLCh *keyFieldTagNameArg)
    : ValidationException(keyrefElementArg),
      keySelectorTagName(keySelectorTagNameArg),
      keyFieldTagName(keyFieldTagNameArg)
    { }

    /**
     * Returns the undefined key value.
     */
    const XMLCh *getValue() const
    {
        return getDOMNode()->getTextContent();
    }

    /**
     * Returns the tag name of the undefined element.
     */
    const XMLCh *getKeySelectorTagName() const
    {
        return keySelectorTagName;
    }

    /**
     * Returns the tag name of the key element of the undefined element.
     */
    const XMLCh *getKeyFieldTagName() const
    {
        return keyFieldTagName;
    }

private:
    /**
     * Key selector (i.e. the tag name of the undefined element).
     */
    const XMLCh *keySelectorTagName;

    /**
     * Key field (i.e. the tag name of the key element of the undefined
     * element).
     */
    const XMLCh *keyFieldTagName;
};

} // namespace LIBXSD2CPP_NAMESPACE

#endif // LIBXSD2CPP_VALIDATIONEXCEPTION_H
