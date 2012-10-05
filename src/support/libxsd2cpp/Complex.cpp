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

#include "xercesc/dom/DOMDocument.hpp"
#include "xercesc/dom/DOMNode.hpp"
#include "xercesc/dom/DOMNodeList.hpp"
#include "xercesc/dom/DOMText.hpp"
#include "xercesc/dom/DOMUserDataHandler.hpp"
#include "xercesc/util/XMLString.hpp"
#include "xercesc/util/XMLUniDefs.hpp"
#include "xalanc/XalanDOM/XalanDOMString.hpp"
#include "xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp"
#include "xalanc/XercesParserLiaison/XercesDOMSupport.hpp"
#include "xalanc/XPath/XObject.hpp"
#include "xalanc/XPath/XPath.hpp"
#include "xalanc/XPath/XPathEvaluator.hpp"
#include "nspr.h"
#include "support/SimpleHash.h"
#include "libxsd2cpp/Complex.h"

using namespace XERCES_CPP_NAMESPACE;
using namespace XALAN_CPP_NAMESPACE;
using namespace LIBXSD2CPP_NAMESPACE;

//-----------------------------------------------------------------------------
// CachedDocumentWrapper
//-----------------------------------------------------------------------------

class LIBXSD2CPP_NAMESPACE::CachedDocumentWrapper : public DOMUserDataHandler {
public:
    static inline CachedDocumentWrapper& getCachedDocumentWrapper(DOMNode *node);
    operator XercesDocumentWrapper *() { return wrapper; }
    virtual void handle(DOMOperationType operation, const XMLCh *const key, void *data, const DOMNode *src, const DOMNode *dst);
    static PRBool isXPathResultCacheable(const char *xpath);
    const DOMNode * const *cacheXPathNodeSet(const char *xpath, const NodeRefListBase& nodeset);
    const DOMNode * const *getCachedXPathNodeSet(const char *xpath);
    const PRBool *cacheXPathBoolean(const char *xpath, PRBool b);
    const PRBool *getCachedXPathBoolean(const char *xpath);

private:
    CachedDocumentWrapper(DOMDocument *document);
    ~CachedDocumentWrapper();

    XercesDocumentWrapper *wrapper;
    DOMDocument *document;
    SimpleStringHash cachedXPathNodeSets;
    SimpleStringHash cachedXPathBooleans;
    PRInt32 refcount;
    static const XMLCh userDataKey[];
    static const PRBool trueBoolean;
    static const PRBool falseBoolean;
};

const XMLCh CachedDocumentWrapper::userDataKey[] = { chLatin_c, chLatin_a, chLatin_c, chLatin_h, chLatin_e, chLatin_d, chLatin_D, chLatin_o, chLatin_c, chLatin_u, chLatin_m, chLatin_e, chLatin_n, chLatin_t, chLatin_W, chLatin_r, chLatin_a, chLatin_p, chLatin_p, chLatin_e, chLatin_r, chNull };
const PRBool CachedDocumentWrapper::trueBoolean = PR_TRUE;
const PRBool CachedDocumentWrapper::falseBoolean = PR_FALSE;

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::getCachedDocumentWrapper
//-----------------------------------------------------------------------------

CachedDocumentWrapper& CachedDocumentWrapper::getCachedDocumentWrapper(DOMNode *node)
{
    DOMDocument *document = node->getOwnerDocument();

    CachedDocumentWrapper *instance = (CachedDocumentWrapper *) document->getUserData(userDataKey);
    if (instance == NULL)
        instance = new CachedDocumentWrapper(document);

    return *instance;
}

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::handle
//-----------------------------------------------------------------------------

void CachedDocumentWrapper::handle(DOMOperationType operation, const XMLCh *const key, void *data, const DOMNode *src, const DOMNode *dst)
{
    switch (operation) {
    case NODE_DELETED:
        PR_ASSERT(refcount == 1);
        if (PR_AtomicDecrement(&refcount) == 0)
            delete this;
        break;
    }
}

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::CachedDocumentWrapper
//-----------------------------------------------------------------------------

CachedDocumentWrapper::CachedDocumentWrapper(DOMDocument *documentArg)
: document(documentArg), cachedXPathNodeSets(32), cachedXPathBooleans(32), refcount(1)
{
#if _XALAN_VERSION >= 10900
    wrapper = new XercesDocumentWrapper(XalanMemMgrs::getDefaultXercesMemMgr(), document, false, false);
#else
    wrapper = new XercesDocumentWrapper(document, false, false);
#endif

    PR_ASSERT(document->getUserData(userDataKey) == NULL);
    document->setUserData(userDataKey, this, this);
}

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::~CachedDocumentWrapper
//-----------------------------------------------------------------------------

CachedDocumentWrapper::~CachedDocumentWrapper()
{
    PR_ASSERT(refcount == 0);

    SimpleHashUnlockedIterator iterator(&cachedXPathNodeSets);
    while (const DOMNode **nodeSet = (const DOMNode **) iterator.next())
        delete [] nodeSet;

    delete wrapper;
}

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::isXPathResultCacheable
//-----------------------------------------------------------------------------

PRBool CachedDocumentWrapper::isXPathResultCacheable(const char *xpath)
{
    // Is this a simple XPath whose result won't vary with the context node?
    return *xpath == '/' && !strchr(xpath, '|');
}

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::cacheXPathNodeSet
//-----------------------------------------------------------------------------

const DOMNode * const *CachedDocumentWrapper::cacheXPathNodeSet(const char *xpath, const NodeRefListBase& nodeList)
{
    PR_ASSERT(getCachedXPathNodeSet(xpath) == NULL);

    int n = nodeList.getLength();

    const DOMNode **nodeSet = new const DOMNode *[n + 1];
    for (int i = 0; i < n; i++)
        nodeSet[i] = wrapper->mapNode(nodeList.item(i));
    nodeSet[n] = NULL;

    cachedXPathNodeSets.insert((void *) xpath, (void *) nodeSet);

    return nodeSet;
}

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::getCachedXPathNodeSet
//-----------------------------------------------------------------------------

const DOMNode * const *CachedDocumentWrapper::getCachedXPathNodeSet(const char *xpath)
{
    if (!isXPathResultCacheable(xpath))
        return NULL;

    return (const DOMNode **) cachedXPathNodeSets.lookup((void *) xpath);
}

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::cacheXPathBoolean
//-----------------------------------------------------------------------------

const PRBool *CachedDocumentWrapper::cacheXPathBoolean(const char *xpath, const PRBool b)
{
    PR_ASSERT(getCachedXPathBoolean(xpath) == NULL);

    const PRBool *value = b ? &trueBoolean : &falseBoolean;

    if (isXPathResultCacheable(xpath))
        cachedXPathBooleans.insert((void *) xpath, (void *) value);

    return value;
}

//-----------------------------------------------------------------------------
// CachedDocumentWrapper::getCachedXPathBoolean
//-----------------------------------------------------------------------------

const PRBool *CachedDocumentWrapper::getCachedXPathBoolean(const char *xpath)
{
    return (const PRBool *) cachedXPathBooleans.lookup((void *) xpath);
}

//-----------------------------------------------------------------------------
// CachedXPath
//-----------------------------------------------------------------------------

struct CachedXPath {
    CachedXPath(const char *string, XPath *compiled)
    : string(string), compiled(compiled)
    { }

    const char * const string;
    XPath * const compiled;
};

//-----------------------------------------------------------------------------
// CachingXPathEvaluator
//-----------------------------------------------------------------------------

class CachingXPathEvaluator {
public:
    static inline const XObjectPtr evaluate(XercesDocumentWrapper *xercesDocumentWrapper, const DOMElement *contextElement, const char *xpath);

private:
    CachingXPathEvaluator();
    ~CachingXPathEvaluator();
    inline const XPath& getXPath(const char *xpath);
    inline const XObjectPtr evaluate(XalanNode *contextNode, XalanElement *namespaceNode, const char *xpath);

    XercesDOMSupport *xercesDOMSupport;
    XPathEvaluator *xpathEvaluator;
    SimpleStringHash xpaths;
    static PRUintn tpdIndex;
    static PRStatus tpdStatus;
};

PRUintn CachingXPathEvaluator::tpdIndex = (PRUintn) -1;
PRStatus CachingXPathEvaluator::tpdStatus = PR_NewThreadPrivateIndex(&tpdIndex, NULL);

//-----------------------------------------------------------------------------
// CachingXPathEvaluator::evaluate
//-----------------------------------------------------------------------------

const XObjectPtr CachingXPathEvaluator::evaluate(XercesDocumentWrapper *xercesDocumentWrapper, const DOMElement *contextElement, const char *xpath)
{
    XalanElement *namespaceNode = xercesDocumentWrapper->getDocumentElement();
    XalanNode *contextNode = xercesDocumentWrapper->mapNode(contextElement);

    PR_ASSERT(tpdIndex != (PRUintn) -1);
    PR_ASSERT(tpdStatus == PR_SUCCESS);

    CachingXPathEvaluator *instance = (CachingXPathEvaluator *) PR_GetThreadPrivate(tpdIndex);
    if (instance == NULL) {
        instance = new CachingXPathEvaluator();
        PR_SetThreadPrivate(tpdIndex, instance);
    }

    const XObjectPtr xpathResult(instance->evaluate(contextNode, namespaceNode, xpath));

    return xpathResult;
}

//-----------------------------------------------------------------------------
// CachingXPathEvaluator::CachingXPathEvaluator
//-----------------------------------------------------------------------------

CachingXPathEvaluator::CachingXPathEvaluator()
: xercesDOMSupport(new XercesDOMSupport()), xpathEvaluator(new XPathEvaluator), xpaths(32)
{ }

//-----------------------------------------------------------------------------
// CachingXPathEvaluator::getXPath
//-----------------------------------------------------------------------------

const XPath& CachingXPathEvaluator::getXPath(const char *xpath)
{
    // Look for an existing compiled XPath
    const CachedXPath *x = (CachedXPath *) xpaths.lookup((void *) xpath);
    if (x)
        return *x->compiled;

    // First time we've seen this XPath, so compile it
    XPath *compiled = xpathEvaluator->createXPath(XalanDOMString(xpath).c_str());

    // Remember the compiled XPath for next time
    xpaths.insert((void *) xpath, new CachedXPath(xpath, compiled));

    return *compiled;
}

//-----------------------------------------------------------------------------
// CachingXPathEvaluator::evaluate
//-----------------------------------------------------------------------------

const XObjectPtr CachingXPathEvaluator::evaluate(XalanNode *contextNode, XalanElement *namespaceNode, const char *xpath)
{
    return xpathEvaluator->evaluate(*xercesDOMSupport, contextNode, getXPath(xpath), namespaceNode);
}

//-----------------------------------------------------------------------------
// Complex::getChildElement
//-----------------------------------------------------------------------------

DOMElement *Complex::getChildElement(DOMElement *parentElement, const XMLCh *name)
{
    DOMElement *uniqueChildElement = NULL;

    for (DOMNode *childNode = parentElement->getFirstChild(); childNode != NULL; childNode = childNode->getNextSibling()) {
        if (childNode->getNodeType() == DOMNode::ELEMENT_NODE) {
            DOMElement *childElement = (DOMElement *)childNode;
            if (XMLString::equals(childElement->getNodeName(), name)) {
                if (uniqueChildElement != NULL)
                    throw DuplicateElementException(uniqueChildElement, childElement);
                uniqueChildElement = childElement;
            }
        }
    }

    return uniqueChildElement;
}

//-----------------------------------------------------------------------------
// Complex::appendChildElement
//-----------------------------------------------------------------------------

DOMElement *Complex::appendChildElement(DOMElement *parentElement, const XMLCh *name)
{
    DOMDocument *document = parentElement->getOwnerDocument();
    DOMElement *childElement = document->createElement(name);
    parentElement->appendChild(childElement);

    // Coax the Xerces-C++ 2.6.0 DOMWriter into formatting non-ugly XML
    DOMNode *previousSibling = childElement->getPreviousSibling();
    if (previousSibling != NULL && previousSibling->getNodeType() == DOMNode::TEXT_NODE)
        parentElement->appendChild(previousSibling->cloneNode(false));

    return childElement;
}

//-----------------------------------------------------------------------------
// Complex::appendChildText
//-----------------------------------------------------------------------------

void Complex::appendChildText(DOMElement *parentElement, const XMLCh *text)
{
    DOMDocument *document = parentElement->getOwnerDocument();
    DOMText *childTextNode = document->createTextNode(text);
    parentElement->appendChild(childTextNode);
}

//-----------------------------------------------------------------------------
// Complex::getCachedDocumentWrapper
//-----------------------------------------------------------------------------

CachedDocumentWrapper& Complex::getCachedDocumentWrapper(DOMNode *node)
{
    return CachedDocumentWrapper::getCachedDocumentWrapper(node);
}

//-----------------------------------------------------------------------------
// Complex::appendChildXPathNodeSet
//-----------------------------------------------------------------------------

void Complex::appendChildXPathNodeSet(CachedDocumentWrapper& documentWrapper, DOMElement *parentElement, DOMElement *contextElement, const char *xpath)
{
    const DOMNode * const *nodeSet = documentWrapper.getCachedXPathNodeSet(xpath);

    if (nodeSet == NULL) {
        const XObjectPtr xpathResult(CachingXPathEvaluator::evaluate(documentWrapper, contextElement, xpath));
        if (!xpathResult.null()) {
            try {
                const NodeRefListBase& resultNodeset = xpathResult->nodeset();
                nodeSet = documentWrapper.cacheXPathNodeSet(xpath, resultNodeset);
            }
            catch (const XalanXPathException&) {
                appendChildText(parentElement, xpathResult->str().c_str());
            }
        }
    }

    if (nodeSet) {
        const DOMNode *node;
        for (int i = 0; node = nodeSet[i]; i++)
            parentElement->appendChild(node->cloneNode(true));
    }
}

//-----------------------------------------------------------------------------
// Complex::evaluateXPathBoolean
//-----------------------------------------------------------------------------

PRBool Complex::evaluateXPathBoolean(CachedDocumentWrapper& documentWrapper, DOMElement *contextElement, const char *xpath)
{
    const PRBool *b = documentWrapper.getCachedXPathBoolean(xpath);
    if (b == NULL) {
        const XObjectPtr xpathResult(CachingXPathEvaluator::evaluate(documentWrapper, contextElement, xpath));
        b = documentWrapper.cacheXPathBoolean(xpath, xpathResult->boolean());
    }

    return *b;
}
