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

/*
 * SchemaUtil.java
 *
 */

package com.sun.webserver.xsd2cpp;

import java.util.List;
import java.util.Vector;
import java.io.StringWriter;
import org.w3c.dom.Element;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.stream.StreamResult;

public class SchemaUtil {
    
    /** Creates a new instance of SchemaUtil */
    public SchemaUtil() {
    }
    
    public static Element getLocalSimpleTypeDefinition(Element contextElement) {
        return getLocalDefinition(contextElement, "simpleType");
    }
    
    public static Element getLocalComplexTypeDefinition(Element contextElement) {
        return getLocalDefinition(contextElement, "complexType");
    }
    
    public static Element getLocalDefinition(Element contextElement, String definitionTagName) {
        List<Element> childElements = getChildElements(contextElement, definitionTagName);
        if (childElements.size() == 1)
            return childElements.get(0);
        return null;
    }
    
    public static List<Element> getChildElements(Element parentElement, String childElementTagName) {
        List<Element> list = new Vector<Element>();
        NodeList childNodes = parentElement.getChildNodes();
        for (int i = 0; i < childNodes.getLength(); i++) {
            Node node = childNodes.item(i);
            if (node.getNodeType() == Node.ELEMENT_NODE) {
                Element element = (Element) node;
                if (element.getLocalName().equals(childElementTagName))
                    list.add(element);
            }
        }
        return list;
    }
    
    public static String serialize(Node node) {
        StringWriter writer = new StringWriter();
        
        try {
            TransformerFactory transformerFactory = TransformerFactory.newInstance();
            Transformer transformer = transformerFactory.newTransformer();
            transformer.transform(new DOMSource(node), new StreamResult(writer));
        } catch (javax.xml.transform.TransformerException caught) {
            throw new Error(caught);
        }
        
        return writer.toString();
    }
    
    public static Element getGlobalElementDefinition(Document document, String name) {
        return getGlobalDefinition(document, "element", name);
    }

    public static Element getGlobalSimpleTypeDefinition(Document document, String name) {
        return getGlobalDefinition(document, "simpleType", name);
    }

    public static Element getGlobalComplexTypeDefinition(Document document, String name) {
        return getGlobalDefinition(document, "complexType", name);
    }

    private static Element getGlobalDefinition(Document document, String definitionTagName, String definedName) {
        List<Element> childElements = getChildElements(document.getDocumentElement(), definitionTagName);
        for (int i = 0; i < childElements.size(); i++) {
            Element element = childElements.get(i);
            if (definedName.equals(element.getAttribute("name")))
                return element;
        }
        return null;
    }
    
    
    
}
