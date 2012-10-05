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

/**
 * Simple schema-to-C++ code generator good enough for parsing Web Server's
 * server.xml.
 */
package com.sun.webserver.xsd2cpp;

import java.lang.Integer;
import java.io.File;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.Stack;
import java.util.StringTokenizer;
import java.util.Vector;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.XMLConstants;
import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.DocumentFragment;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

/**
 * XSD2Cpp
 */
public class XSD2Cpp {
    public static void main(String[] args) throws Exception {
        if (args.length < 4) {
            System.err.println("syntax: java XSD2Cpp <filename> <root> <namespace> <generated_dir>");
            System.err.println("                     [ <xstype>=<cpptype> ...]");
            System.err.println("where <filename> is the input XML Schema filename");
            System.err.println("      <root> is the root element");
            System.err.println("      <namespace> is the C++ namespace for the generated code");
            System.err.println("      <generated_dir> is the directory in which to write the generated code");
            System.err.println("      <xstype> is the name of an XML Schema simpleType");
            System.err.println("      <cpptype> is PRIntervalTime or PRNetAddr");
            return;
        }

        HashMap<String, String> typeMap = new HashMap<String, String>();
        for (int i = 4; i < args.length; i++) {
            String xstype = args[i].substring(0, args[i].indexOf('='));
            String cpptype = args[i].substring(args[i].indexOf('=') + 1);
            typeMap.put(xstype, cpptype);
        }

        DocumentBuilderFactory documentBuilderFactory = DocumentBuilderFactory.newInstance();
        documentBuilderFactory.setNamespaceAware(true);
        DocumentBuilder documentBuilder = documentBuilderFactory.newDocumentBuilder();

        Document document = documentBuilder.parse(args[0]);

        CodeGenerator generator = new CodeGenerator(args[1], args[2], args[3], typeMap, document);
    }
}

/**
 * CodeGenerator
 */
class CodeGenerator {
    //-------------------------------------------------------------------------
    // Public constructor
    //-------------------------------------------------------------------------

    CodeGenerator(String rootElementNameArg, String namespaceArg, String generatedDirArg, HashMap<String, String> typeMapArg, Document documentArg) throws Exception {
        rootElementName = rootElementNameArg;
        namespace = namespaceArg;
        includePrefix = generatedDirArg + "/";
        includeGuardMacroPrefix = CodeUtil.getMacroName(includePrefix);
        generatedDir = generatedDirArg;
        typeMap = typeMapArg;
        document = documentArg;

        File generatedDirAbstractPathname = new File(generatedDir);
        generatedDirAbstractPathname.mkdirs();

        // Find the root element definition
        Element rootElementDefinition = SchemaUtil.getGlobalElementDefinition(document,rootElementName);
        if (rootElementDefinition == null)
            throw new Exception("Unknown element: " + rootElementName);

        generateElementCode(rootElementDefinition);
    }

    //-------------------------------------------------------------------------
    // Private member functions
    //-------------------------------------------------------------------------

    private boolean generateElementCode(Element elementDefinition) throws Exception {
        elementStack.push(elementDefinition);
        boolean generatedComplexClass = generatePushedElementCode(elementDefinition);
        elementStack.pop();
        return generatedComplexClass;
    }

    private boolean generatePushedElementCode(Element elementDefinition) throws Exception {
        String elementName = elementDefinition.getAttribute("name");
        String elementContextPath = getElementContextPath();

        // XXX Assume elements with the same name have the same definitions.
        // This keeps the class names short and pretty.
        // String elementClassName = CodeUtil.getMixedCaseName(elementContextPath);
        String elementClassName = CodeUtil.getMixedCaseName(elementName);

        String typeName = null;
        if (elementDefinition.hasAttribute("type"))
            typeName = elementDefinition.getAttribute("type");

        // Check for unique, key, and keyref definitions
        UniqueKeyKeyrefs uniqueKeyKeyrefs = new UniqueKeyKeyrefs(elementDefinition);

        // If we don't need to do any unique, key, or keyref checks...
        if (uniqueKeyKeyrefs.isEmpty()) {
            // Does this element specify a type?
            if (typeName != null) {
                // Yep.  This is a leaf element.  We'll just generate a class
                // for the type, not the element.
                String className = getClassNameForType(typeName);
                elementClassMap.put(elementContextPath, className);
                return false;
            }

            // Does the element have a local simpleType?
            Element simpleTypeDefinition = SchemaUtil.getLocalSimpleTypeDefinition(elementDefinition);
            if (simpleTypeDefinition != null) {
                String className = processSimpleType(simpleTypeDefinition, elementClassName);
                elementClassMap.put(elementContextPath, className);
                return false;
            }
        }

        Element complexTypeDefinition = null;
        if (typeName != null) {
            // Element specified a type.  If we get here, the element also had
            // unique, key, or keyref definitions, so it must be a complexType.
            complexTypeDefinition = SchemaUtil.getGlobalComplexTypeDefinition(document, typeName);
        } else {
            // Element has no global type and no local simpleType.  It should have
            // a local complexType.
            complexTypeDefinition = SchemaUtil.getLocalComplexTypeDefinition(elementDefinition);
        }
        if (complexTypeDefinition == null)
            throw new Exception("No content model for element " + elementName);

        boolean generatedComplexClass = false;

        // Generate code for this element's children
        ContentModel contentModel = new ContentModel(complexTypeDefinition, elementName);
        for (ContentModelElement child : contentModel.children) {
            if (generateElementCode(child.elementDefinition))
                generatedComplexClass = true;
        }

        // Generate a specific class for this element
        if (generateContentModelClass(elementClassName, "libxsd2cpp_1_0::Complex", contentModel, uniqueKeyKeyrefs))
            generatedComplexClass = true;
        elementClassMap.put(elementContextPath, elementClassName);

        return generatedComplexClass;
    }

    private boolean generateComplexTypeCode(Element complexTypeDefinition) throws Exception {
        String typeName = complexTypeDefinition.getAttribute("name");

        boolean generatedComplexClass = false;

        // Generate code for this complexType's children
        ContentModel contentModel = new ContentModel(complexTypeDefinition, typeName);
        for (ContentModelElement child : contentModel.children) {
            if (generateElementCode(child.elementDefinition))
                generatedComplexClass = true;
        }

        // Generate a specific class for this complexType
        String className = CodeUtil.getMixedCaseName(stripTypeSuffix(typeName));
        if (generateContentModelClass(className, "libxsd2cpp_1_0::Complex", contentModel, null))
            generatedComplexClass = true;
        typeMap.put(typeName, className);

        return generatedComplexClass;
    }

    private String getClassNameForType(String typeName) throws Exception {
        // Check if we already mapped this type to a specific C++ class
        if (typeMap.containsKey(typeName))
            return typeMap.get(typeName);

        String suggestedClassName = CodeUtil.getMixedCaseName(stripTypeSuffix(typeName));

        // If typeName specifies a global complexType, use the generated class
        if (SchemaUtil.getGlobalComplexTypeDefinition(document,typeName) != null) {
            typeMap.put(typeName, suggestedClassName);
            generateComplexTypeCode(SchemaUtil.getGlobalComplexTypeDefinition(document,typeName));
            return suggestedClassName;
        }

        // If typeName specifies a global simpleType, figure out what class to use
        Element simpleTypeDefinition = SchemaUtil.getGlobalSimpleTypeDefinition(document, typeName);
        if (simpleTypeDefinition != null) {
            String className = processSimpleType(simpleTypeDefinition, suggestedClassName);
            typeMap.put(typeName, className);
            return className;
        }

        // Use a predefined class for certain types defined by XSD
        if (typeName.equals("xs:token")) {
            return "libxsd2cpp_1_0::String";
        } else if (typeName.equals("xs:string")) {
            return "libxsd2cpp_1_0::String";
        } else if (typeName.equals("xs:language")) {
            return "libxsd2cpp_1_0::String";
        } else if (typeName.equals("xs:anyType")) {
            return "libxsd2cpp_1_0::String";
        } else if (typeName.equals("xs:decimal")) {
            return "libxsd2cpp_1_0::Integer"; // XXX
        } else if (typeName.equals("xs:integer")) {
            return "libxsd2cpp_1_0::Integer";
        } else if (typeName.equals("xs:boolean")) {
            return "libxsd2cpp_1_0::Bool";
        }

        throw new Exception(getElementContextPath() + ": Unknown type: " + typeName);
    }

    private String processSimpleType(Element simpleTypeDefinition, String suggestedClassName) throws Exception {
        // Get all the base types
        LinkedHashSet<String> baseTypeNameSet = new LinkedHashSet<String>();
        LinkedHashSet<String> enumerationSet = new LinkedHashSet<String>();
        getBaseTypeNames(simpleTypeDefinition, baseTypeNameSet, enumerationSet);

        // Is this simpleType an enumeration?
        if (baseTypeNameSet.size() == 1 && enumerationSet.size() > 0) {
            // Generate code for the enumeration
            generateSimpleTypeEnumerationCode(suggestedClassName, enumerationSet);
            return suggestedClassName;
        }

        // Use a predefined class
        if (baseTypeNameSet.contains("xs:token")) {
            return "libxsd2cpp_1_0::String";
        } else if (baseTypeNameSet.contains("xs:string")) {
            return "libxsd2cpp_1_0::String";
        } else if (baseTypeNameSet.contains("xs:decimal")) {
            return "libxsd2cpp_1_0::Integer"; // XXX
        } else if (baseTypeNameSet.contains("xs:integer")) {
            return "libxsd2cpp_1_0::Integer";
        } else if (baseTypeNameSet.contains("xs:boolean")) {
            return "libxsd2cpp_1_0::Bool";
        }
        return "libxsd2cpp_1_0::Simple";
    }

    private void getBaseTypeNames(Element typeDefinition, Set<String> baseTypeNameSet, Set<String> enumerationSet) {
        List<Element> restrictionElements = SchemaUtil.getChildElements(typeDefinition, "restriction");
        for (int i = 0; i < restrictionElements.size(); i++) {
            String baseTypeName = restrictionElements.get(i).getAttribute("base");
            baseTypeNameSet.add(baseTypeName);
            Element baseTypeElement = SchemaUtil.getGlobalSimpleTypeDefinition(document, baseTypeName);
            if (baseTypeElement != null)
                getBaseTypeNames(baseTypeElement, baseTypeNameSet, enumerationSet);

            List<Element> enumerationDefinitions = SchemaUtil.getChildElements(restrictionElements.get(i), "enumeration");
            for (int j = 0; j < enumerationDefinitions.size(); j++)
                enumerationSet.add(enumerationDefinitions.get(j).getAttribute("value"));
        }

        List<Element> unionElements = SchemaUtil.getChildElements(typeDefinition, "union");
        for (int i = 0; i < unionElements.size(); i++) {
            String memberTypes = unionElements.get(i).getAttribute("memberTypes");
            StringTokenizer memberTypesTokenizer = new StringTokenizer(memberTypes);
            while (memberTypesTokenizer.hasMoreTokens()) {
                String baseTypeName = memberTypesTokenizer.nextToken();
                baseTypeNameSet.add(baseTypeName);
                Element baseTypeElement = SchemaUtil.getGlobalSimpleTypeDefinition(document, baseTypeName);
                if (baseTypeElement != null)
                    getBaseTypeNames(baseTypeElement, baseTypeNameSet, enumerationSet);
            }

            List<Element> typeDefinitions = SchemaUtil.getChildElements(unionElements.get(i), "simpleType");
            for (int j = 0; j < typeDefinitions.size(); j++)
                getBaseTypeNames(typeDefinitions.get(j), baseTypeNameSet, enumerationSet);
        }
    }

    private void generateSimpleTypeEnumerationCode(String className, Set<String> enumerationSet) throws Exception {
        String baseClassName = "libxsd2cpp_1_0::String";

        String headerFilename = generatedDir + File.separator + className + ".h";
        System.err.println("Generating " + headerFilename);
        out = new PrintWriter(headerFilename);
        emitTopCommentBlock();
        emitBeginIncludeGuardMacro(className);
        out.println("#include <string.h>");
        out.println();
        emitClassHeaderIncludeDirective(baseClassName);
        out.println();
        out.println("using namespace libxsd2cpp_1_0;");
        out.println();
        out.println("namespace " + namespace + " {");
        out.println();
        out.println("class " + className + " : public " + baseClassName + " {");
        out.println("public:");

        Iterator<String> iterator;

        out.println("    enum Enum {");
        iterator = enumerationSet.iterator();
        while (iterator.hasNext()) {
            String enumValue = iterator.next();
            String enumIdentifier = CodeUtil.getMacroName(className) + "_" + CodeUtil.getMacroName(enumValue);
            out.print("        " + enumIdentifier);
            if (iterator.hasNext()) {
                out.println(",");
            } else {
                out.println();
            }
        }
        out.println("    };");
        out.println();
        out.println("    static " + className + " *create" + className + "Instance(XERCES_CPP_NAMESPACE::DOMElement *element)");
        out.println("    {");
        out.println("        return new " + className + "(element);");
        out.println("    }");
        out.println();
        out.println("    Enum getEnumValue() const { return _enumValue; }");
        out.println();
        out.println("    operator Enum() const { return _enumValue; }");
        out.println();
        out.println("private:");
        out.println("    " + className + "(XERCES_CPP_NAMESPACE::DOMElement *elementArg)");
        out.println("    : " + baseClassName + "(elementArg)");
        out.println("    {");
        out.print("        ");
        iterator = enumerationSet.iterator();
        while (iterator.hasNext()) {
            String enumValue = iterator.next();
            String enumIdentifier = CodeUtil.getMacroName(className) + "_" + CodeUtil.getMacroName(enumValue);
            out.println("if (!strcmp(getStringValue(), " + CodeUtil.getQuotedLiteral(enumValue) + ")) {");
            out.println("            _enumValue = " + enumIdentifier + ";");
            out.print("        } else ");
        }
        out.println("{");
        out.println("            throw libxsd2cpp_1_0::InvalidValueException(elementArg);");
        out.println("        }");
        out.println("    }");
        out.println();
        out.println("    Enum _enumValue;");
        out.println("};");
        out.println();
        out.println("} // namespace " + namespace);
        emitEndIncludeGuardMacro(className);
        out.close();
        out = null;
    }

    private boolean generateContentModelClass(String className, String baseClassName, ContentModel contentModel, UniqueKeyKeyrefs uniqueKeyKeyrefs) throws Exception {
        if (generatedClassContentModelMap.containsKey(className))
            return false;

        generatedClassContentModelMap.put(className, contentModel);

        String wrapperClassName = className + "Wrapper";
        String implClassName = className + "Impl";

        // Begin header
        String headerFilename = generatedDir + File.separator + className + ".h";
        System.err.println("Generating " + headerFilename);
        out = new PrintWriter(headerFilename);
        emitTopCommentBlock();
        emitBeginIncludeGuardMacro(className);
        emitContentModelIncludes(contentModel);
        emitClassHeaderIncludeDirective(baseClassName);
        out.println();
        out.println("namespace " + namespace + " {");
        out.println();
        out.println("using namespace libxsd2cpp_1_0;");
        out.println();

        // Abstract base class
        out.println("class " + className + " : public " + baseClassName + " {");
        out.println("public:");
        out.println("    static " + className + " *create" + className + "Instance(XERCES_CPP_NAMESPACE::DOMElement *element);");
        out.println();
        out.println("    virtual ~" + className + "() { };");
        out.println();
        emitContentModelRequiredChildren(contentModel);
        emitContentModelPureVirtualGetters(contentModel);
        out.println("protected:");
        out.println("    " + className + getContentModelBaseConstructorArgList(contentModel) + ";");
        out.println();
        out.println("    friend class " + implClassName + ";");
        out.println("    friend class " + wrapperClassName + ";");
        out.println("};");
        out.println();

        // Implementation class
        out.println("class " + implClassName + " : public " + className + " {");
        out.println("public:");
        out.println("    virtual ~" + implClassName + "();");
        out.println();
        emitContentModelImplGetters(contentModel);
        out.println("private:");
        out.println("    " + implClassName + getContentModelImplConstructorArgList(contentModel) + ";");
        out.println("    void destroy();");
        out.println();
        emitContentModelMembers(contentModel);
        out.println("    friend class " + className + ";");
        out.println("};");
        out.println();

        // Wrapper class
        out.println("class " + wrapperClassName + " : public " + className + " {");
        out.println("public:");
        out.println("    " + wrapperClassName + "(" + className + "& wrappedObjectArg);");
        out.println();
        emitContentModelWrapperGetters(contentModel);
        out.println("private:");
        out.println("    " + className + "& wrappedObject;");
        out.println("};");
        out.println();

        // End header
        out.println("} // namespace " + namespace);
        emitEndIncludeGuardMacro(className);
        out.close();
        out = null;

        // Begin cpp
        String cppFilename = generatedDir + File.separator + className + ".cpp";
        System.err.println("Generating " + cppFilename);
        out = new PrintWriter(cppFilename);
        emitTopCommentBlock();
        out.println("#include <stdlib.h>");
        out.println();
        out.println("#include \"xercesc/dom/DOMDocument.hpp\"");
        out.println("#include \"xercesc/dom/DOMNodeList.hpp\"");
        out.println("#include \"xercesc/util/XMLString.hpp\"");
        out.println("#include \"xercesc/util/XMLUniDefs.hpp\"");
        out.println("#include \"support/SimpleHash.h\"");
        out.println();
        out.println("XERCES_CPP_NAMESPACE_USE");
        out.println();
        emitClassHeaderIncludeDirective(className);
        out.println();
        out.println("using namespace " + namespace + ";");
        out.println();
        out.println("#ifndef STRINGIFY");
        out.println("#define STRINGIFY(literal) #literal");
        out.println("#endif // STRINGIFY");
        out.println();
        emitContentModelXMLChConstants(contentModel);
        out.println();

        // Factory
        out.println(className + " *" + className + "::create" + className + "Instance(XERCES_CPP_NAMESPACE::DOMElement *element)");
        out.println("{");
        emitContentModelCreateInstance(contentModel, className);
        out.println("}");
        out.println();

        // Abstract base class
        out.println(className + "::" + className + getContentModelBaseConstructorArgList(contentModel));
        out.print(": " + baseClassName + "(elementArg)");
        emitContentModelBaseInitializers(contentModel);
        out.println();
        out.println("{ }");
        out.println();

        // Implementation class
        out.println(implClassName + "::" + implClassName + getContentModelImplConstructorArgList(contentModel));
        out.print(": " + className + getContentModelImplBaseConstructorCall(contentModel));
        emitContentModelImplInitializers(contentModel);
        out.println();
        out.println("{");
        emitContentModelImplConstruction(className, contentModel, uniqueKeyKeyrefs);
        out.println("}");
        out.println();
        out.println(implClassName + "::~" + implClassName + "()");
        out.println("{");
        out.println("    destroy();");
        out.println("}");
        out.println();
        out.println("void " + implClassName + "::destroy()");
        out.println("{");
        emitContentModelImplDestruction(contentModel);
        out.println("}");
        out.println();

        // Wrapper class
        out.println(wrapperClassName + "::" + wrapperClassName + "(" + className + "& wrappedObjectArg)");
        out.println(": wrappedObject(wrappedObjectArg),");
        out.println("  " + className + getContentModelWrapperBaseConstructorCall(contentModel));
        out.println("{ }");
        out.println();

        // End cpp
        out.close();
        out = null;

        return true;
    }

    private String getElementContextPath() {
        String elementContextPath = "";
        Iterator<Element> elementStackIterator = elementStack.iterator();
        while (elementStackIterator.hasNext()) {
            elementContextPath = elementContextPath + "/" + elementStackIterator.next().getAttribute("name");
        }
        return elementContextPath;
    }

    private String getChildClassName(String childElementName) {
        return elementClassMap.get(getElementContextPath() + "/" + childElementName);
    }

    private ContentModel getGeneratedClassContentModel(String className) {
        return generatedClassContentModelMap.get(className);
    }

    private void emitTopCommentBlock() {
        out.println("/*");
        out.println(" * Automatically generated by " + getClass().getName());
        out.println(" * DO NOT HAND EDIT");
        out.println(" */");
        out.println();
    }

    private void emitBeginIncludeGuardMacro(String headerFilename) {
        out.println("#ifndef " + includeGuardMacroPrefix + CodeUtil.getMacroName(headerFilename));
        out.println("#define " + includeGuardMacroPrefix + CodeUtil.getMacroName(headerFilename));
        out.println();
    }

    private void emitEndIncludeGuardMacro(String headerFilename) {
        out.println();
        out.println("#endif // " + includeGuardMacroPrefix + CodeUtil.getMacroName(headerFilename));
    }

    private void emitContentModelIncludes(ContentModel contentModel) {
        LinkedHashSet<String> requiredClassNameSet = new LinkedHashSet<String>();

        for (ContentModelElement child : contentModel.children)
            requiredClassNameSet.add(getChildClassName(child.elementName));

        Iterator<String> iterator = requiredClassNameSet.iterator();
        while (iterator.hasNext()) {
            String className = iterator.next();
            emitClassHeaderIncludeDirective(className);
        }
    }

    private void emitClassHeaderIncludeDirective(String className) {
        if (className.startsWith("libxsd2cpp_1_0::")) {
            out.println("#include \"libxsd2cpp/" + className.substring(16) + ".h\"");
        } else {
            out.println("#include \"" + includePrefix + className + ".h\"");
        }
    }

    private void emitContentModelRequiredChildren(ContentModel contentModel) {
        for (ContentModelElement child : contentModel.requiredChildren)
            out.println("    " + getChildClassName(child.elementName) + "& " + child.varName + ";");

        if (contentModel.requiredChildren.size() > 0)
            out.println();
    }

    private void emitContentModelPureVirtualGetters(ContentModel contentModel) {
        for (ContentModelElement child : contentModel.optionalChildren) {
            out.println("    virtual " + getChildClassName(child.elementName) + " *" + child.getterName + "() = 0;");
            out.println("    virtual const " + getChildClassName(child.elementName) + " *" + child.getterName + "() const = 0;");
        }

        for (ContentModelElement child : contentModel.multipleChildren) {
            out.println("    virtual int " + child.getterName + "Count() const = 0;");
            out.println("    virtual " + getChildClassName(child.elementName) + " *" + child.getterName + "(int i) = 0;");
            out.println("    virtual const " + getChildClassName(child.elementName) + " *" + child.getterName + "(int i) const = 0;");
        }

        if (contentModel.optionalChildren.size() > 0 || contentModel.multipleChildren.size() > 0)
            out.println();
    }

    private void emitContentModelWrapperGetters(ContentModel contentModel) {
        for (ContentModelElement child : contentModel.optionalChildren) {
            out.println("    inline " + getChildClassName(child.elementName) + " *" + child.getterName + "() { return wrappedObject." + child.getterName + "(); }");
            out.println("    inline const " + getChildClassName(child.elementName) + " *" + child.getterName + "() const { return wrappedObject." + child.getterName + "(); }");
        }

        for (ContentModelElement child : contentModel.multipleChildren) {
            out.println("    inline int " + child.getterName + "Count() const { return wrappedObject." + child.getterName + "Count(); }");
            out.println("    inline " + getChildClassName(child.elementName) + " *" + child.getterName + "(int i) { return wrappedObject." + child.getterName + "(i); }");
            out.println("    inline const " + getChildClassName(child.elementName) + " *" + child.getterName + "(int i) const { return wrappedObject." + child.getterName + "(i); }");
        }

        if (contentModel.optionalChildren.size() > 0 || contentModel.multipleChildren.size() > 0)
            out.println();
    }

    private void emitContentModelImplGetters(ContentModel contentModel) {
        for (ContentModelElement child : contentModel.optionalChildren) {
            out.println("    inline " + getChildClassName(child.elementName) + " *" + child.getterName + "() { return " + child.privateMemberName + "; }");
            out.println("    inline const " + getChildClassName(child.elementName) + " *" + child.getterName + "() const { return " + child.privateMemberName + "; }");
        }

        for (ContentModelElement child : contentModel.multipleChildren) {
            out.println("    inline int " + child.getterName + "Count() const { return " + child.privateMemberName + "Count; }");
            out.println("    inline " + getChildClassName(child.elementName) + " *" + child.getterName + "(int i) { return " + child.privateMemberName + "[i]; }");
            out.println("    inline const " + getChildClassName(child.elementName) + " *" + child.getterName + "(int i) const { return " + child.privateMemberName + "[i]; }");
        }

        if (contentModel.optionalChildren.size() > 0 || contentModel.multipleChildren.size() > 0)
            out.println();
    }

    private String getContentModelWrapperBaseConstructorCall(ContentModel contentModel) {
        StringBuffer sb = new StringBuffer();

        sb.append("(wrappedObjectArg.getDOMElement()");

        for (ContentModelElement child : contentModel.requiredChildren) {
            sb.append(", wrappedObjectArg.");
            sb.append(child.varName);
        }

        sb.append(")");

        return sb.toString();
    }

    private String getContentModelBaseConstructorArgList(ContentModel contentModel) {
        StringBuffer sb = new StringBuffer();

        sb.append("(const XERCES_CPP_NAMESPACE::DOMElement *elementArg");

        for (ContentModelElement child : contentModel.requiredChildren) {
            sb.append(", ");
            sb.append(getChildClassName(child.elementName));
            sb.append("& ");
            sb.append(child.varName);
            sb.append("Arg");
        }

        sb.append(")");

        return sb.toString();
    }

    private String getContentModelImplBaseConstructorCall(ContentModel contentModel) {
        StringBuffer sb = new StringBuffer();

        sb.append("(elementArg");

        for (ContentModelElement child : contentModel.requiredChildren) {
            sb.append(", *");
            sb.append(child.varName);
            sb.append("Arg");
        }

        sb.append(")");

        return sb.toString();
    }

    private String getContentModelImplConstructorArgList(ContentModel contentModel) {
        StringBuffer sb = new StringBuffer();

        sb.append("(XERCES_CPP_NAMESPACE::DOMElement *elementArg");

        for (ContentModelElement child : contentModel.requiredChildren) {
            sb.append(", ");
            sb.append(getChildClassName(child.elementName));
            sb.append(" *");
            sb.append(child.varName);
            sb.append("Arg");
        }

        sb.append(")");

        return sb.toString();
    }

    private void emitContentModelMembers(ContentModel contentModel) {
        for (ContentModelElement child : contentModel.optionalChildren)
            out.println("    " + getChildClassName(child.elementName) + " *" + child.privateMemberName + ";");

        for (ContentModelElement child : contentModel.multipleChildren) {
            out.println("    int " + child.privateMemberName + "Count;");
            out.println("    " + getChildClassName(child.elementName) + " **" + child.privateMemberName + ";");
        }

        if (contentModel.optionalChildren.size() > 0 || contentModel.multipleChildren.size() > 0)
            out.println();
    }

    private void emitContentModelXMLChConstants(ContentModel contentModel) {
        for (ContentModelElement child : contentModel.children) {
            out.println("static const XMLCh " + child.tagNameConstant + "[] = " + CodeUtil.getXMLChArray(child.elementName) + ";");
        }
    }

    private void emitContentModelCreateInstance(ContentModel contentModel, String className) {
        // Lookup the Xalan-C++ XercesDocumentWrapper if we need to evaluate XPaths
        boolean hasXPaths = false;
        for (ContentModelElement child : contentModel.children) {
            for (ImplicitValue implicitValue : child.implicitValues) {
                if (implicitValue.ifXPath != null || implicitValue.contentXPath != null)
                    hasXPaths = true;
            }
        }
        if (hasXPaths) {
            out.println("    CachedDocumentWrapper& documentWrapper = libxsd2cpp_1_0::Complex::getCachedDocumentWrapper(element);");
            out.println();
        }
        
        // Optional children with implicit values
        if (contentModel.optionalChildren.size() > 0) {
            boolean hasOptionalChildrenWithImplicitValues = false;
            for (ContentModelElement child : contentModel.optionalChildren) {
                if (child.implicitValues.size() > 0) {
                    if (!hasOptionalChildrenWithImplicitValues) {
                        out.println("    // Create implicit DOMNodes for optional Element children as appropriate");
                        out.println("    {");
                        out.println("        XERCES_CPP_NAMESPACE::DOMElement *childElement;");
                        hasOptionalChildrenWithImplicitValues = true;
                    }
                    out.println();
                    emitContentModelElementImplicitValues(child);
                }
            }
            if (hasOptionalChildrenWithImplicitValues) {
                out.println("    }");
                out.println();
            }
        }

        // Required children
        if (contentModel.requiredChildren.size() > 0) {
            out.println("    // Instantiate all required child Element objects, creating implicit");
            out.println("    // DOMNodes as necessary");
            for (ContentModelElement child : contentModel.requiredChildren) {
                String childClassName = getChildClassName(child.elementName);
                out.println("    " + childClassName + " *" + child.varName + " = NULL;");
            }
            out.println("    try {");
            out.println("        XERCES_CPP_NAMESPACE::DOMElement *childElement;");

            for (ContentModelElement child : contentModel.requiredChildren) {
                out.println();
                emitContentModelElementImplicitValues(child);
                String childClassName = getChildClassName(child.elementName);
                if (!child.hasDefaultImplicitValue()) {
                    out.println("        if (childElement == NULL)");
                    out.println("            throw libxsd2cpp_1_0::MissingElementException(element, " + child.tagNameConstant + ");");
                }
                out.println("        " + child.varName + " = " + childClassName + "::create" + stripNamespacePrefix(childClassName) + "Instance(childElement);");
            }
            out.println("    }");

            out.println("    catch (const libxsd2cpp_1_0::ValidationException&) {");
            for (ContentModelElement child : contentModel.requiredChildren)
                out.println("        delete " + child.varName + ";");
            out.println("        throw;");
            out.println("    }");
            out.println();
        }

        out.println("    // Instantiate the Element");
        String returnValueVarName = "new" + className + "Instance";
        out.print("    " + className + " *" + returnValueVarName + " = new " + className + "Impl(element");
        for (ContentModelElement child : contentModel.requiredChildren)
            out.print(", " + child.varName);
        out.println(");");

        out.println("    PR_ASSERT(" + returnValueVarName + " != NULL);");
        out.println();
        out.println("    return " + returnValueVarName + ";");
    }

    private void emitContentModelElementImplicitValues(ContentModelElement child) {
        out.println("        // " + child.elementName);
        out.println("        childElement = getChildElement(element, " + child.tagNameConstant + ");");

        for (ImplicitValue implicitValue : child.implicitValues) {
            String condition = "childElement == NULL";
            if (implicitValue.ifXPath != null)
                condition += " && evaluateXPathBoolean(documentWrapper, element, " + CodeUtil.getQuotedLiteral(implicitValue.ifXPath) + ")";
            if (implicitValue.ifPlatform != null)
                condition += " && !strcmp(BUILD_PLATFORM, " + CodeUtil.getQuotedLiteral(implicitValue.ifPlatform) + ")";
            if (implicitValue.ifNotPlatform != null)
                condition += " && strcmp(BUILD_PLATFORM, " + CodeUtil.getQuotedLiteral(implicitValue.ifNotPlatform) + ")";
            out.println("        if (" + condition + ") {");
            out.println("            childElement = appendChildElement(element, " + child.tagNameConstant + ");");
            if (implicitValue.contentFunction != null) {
                out.println("            // XXX appendChildText(childElement, " + implicitValue.contentFunction + ");");
            } else if (implicitValue.contentXPath != null) {
                out.println("            appendChildXPathNodeSet(documentWrapper, childElement, element, " + CodeUtil.getQuotedLiteral(implicitValue.contentXPath) + ");");
            } else if (implicitValue.domContent != null) {
                emitImplicitValueDOMContent(implicitValue.domContent);
            } else if (child.defaultValue != null) {
                out.println("            {");
                out.println("                const XMLCh defaultValue[] = " + CodeUtil.getXMLChArray(child.defaultValue) + ";");
                out.println("                appendChildText(childElement, defaultValue);");
                out.println("            }");
            }
            out.println("        }");
        }
    }

    private void emitImplicitValueDOMContent(Node contentNode) {
        Node contentChildNode = contentNode.getFirstChild();
        while (contentChildNode != null) {
            if (contentChildNode.getNodeType() == Node.TEXT_NODE) {
                String textContent = contentChildNode.getTextContent();
                if (textContent.trim().length() > 0) {
                    out.println("            {");
                    out.println("                const XMLCh childText[] = " + CodeUtil.getXMLChArray(textContent) + ";");
                    out.println("                appendChildText(childElement, childText);");
                    out.println("            }");
                }
            } else if (contentChildNode.getNodeType() == Node.ELEMENT_NODE) {
                // XXX we don't do attributes
                out.println("            {");
                out.println("                const XMLCh tagName[] = " + CodeUtil.getXMLChArray(contentChildNode.getNodeName()) + ";");
                if (contentChildNode.getFirstChild() != null) {
                    out.println("                childElement = appendChildElement(childElement, tagName);");
                    out.println("            }");
                    emitImplicitValueDOMContent(contentChildNode);
                    out.println("            childElement = (XERCES_CPP_NAMESPACE::DOMElement *)childElement->getParentNode();");
                } else {
                    out.println("                appendChildElement(childElement, tagName);");
                    out.println("            }");
                }
            }
            contentChildNode = contentChildNode.getNextSibling();
        }
    }

    private void emitContentModelBaseInitializers(ContentModel contentModel) {
        for (ContentModelElement child : contentModel.requiredChildren) {
            out.println(",");
            out.print("  " + child.varName + "(" + child.varName + "Arg)");
        }
    }

    private void emitContentModelImplInitializers(ContentModel contentModel) {
        for (ContentModelElement child : contentModel.optionalChildren) {
            out.println(",");
            out.print("  " + child.privateMemberName + "(NULL)");
        }

        for (ContentModelElement child : contentModel.multipleChildren) {
            out.println(",");
            out.println("  " + child.privateMemberName + "Count(0),");
            out.print("  " + child.privateMemberName + "(NULL)");
        }
    }

    private void emitContentModelImplConstruction(String className, ContentModel contentModel, UniqueKeyKeyrefs uniqueKeyKeyrefs) {
        if (contentModel.optionalChildren.size() > 0 || contentModel.multipleChildren.size() > 0 || uniqueKeyKeyrefs != null && !uniqueKeyKeyrefs.isEmpty()) {
            out.println("    try {");
            out.println("        // Instantiate optional child Element objects");
            out.println("        for (XERCES_CPP_NAMESPACE::DOMNode *childNode = elementArg->getFirstChild(); childNode != NULL; childNode = childNode->getNextSibling()) {");
            out.println("            if (childNode->getNodeType() == XERCES_CPP_NAMESPACE::DOMNode::ELEMENT_NODE) {");
            out.println("                XERCES_CPP_NAMESPACE::DOMElement *childElement = (XERCES_CPP_NAMESPACE::DOMElement *)childNode;");

            for (ContentModelElement child : contentModel.optionalChildren) {
                String childClassName = getChildClassName(child.elementName);
                out.println();
                out.println("                // " + child.elementName);
                out.println("                if (XMLString::equals(childElement->getNodeName(), " + child.tagNameConstant + ")) {");
                out.println("                    if (" + child.privateMemberName + " != NULL)");
                out.println("                        throw libxsd2cpp_1_0::DuplicateElementException(" + child.privateMemberName + "->getDOMElement(), childElement);");
                out.println("                    " + child.privateMemberName + " = " + childClassName + "::create" + stripNamespacePrefix(childClassName) + "Instance(childElement);");
                out.println("                }");
            }

            for (ContentModelElement child : contentModel.multipleChildren) {
                String childClassName = getChildClassName(child.elementName);
                out.println();
                out.println("                // " + child.elementName);
                out.println("                if (XMLString::equals(childElement->getNodeName(), " + child.tagNameConstant + ")) {");
                out.println("                    void *copy = realloc(" + child.privateMemberName + ", (" + child.privateMemberName + "Count + 1) * sizeof(" + childClassName + " *));");
                out.println("                    if (copy != NULL) {");
                out.println("                        " + child.privateMemberName + " = (" + childClassName + " **)copy;");
                out.println("                        " + child.privateMemberName + "[" + child.privateMemberName + "Count] = " + childClassName + "::create" + stripNamespacePrefix(childClassName) + "Instance(childElement);");
                out.println("                        " + child.privateMemberName + "Count++;");
                out.println("                    }");
                out.println("                }");
            }

            out.println("            }");
            out.println("        }");

            if (uniqueKeyKeyrefs != null && !uniqueKeyKeyrefs.isEmpty()) {
                for (SelectorField unique : uniqueKeyKeyrefs.uniques) {
                    out.println();
                    out.println("        // Check for " + unique.selector + " " + unique.field + " uniqueness");
                    out.println("        {");

                    ContentModelElement selector = contentModel.getChildElement(unique.selector);
                    String fieldName = CodeUtil.getCppSafeIdentifier(CodeUtil.getCamelCaseName(unique.field));
                    String typeName = getChildClassName(selector.elementName);

                    out.println("            int i;");
                    out.println("            SimpleStringHash nameHash(" + selector.privateMemberName + "Count + 1);");
                    out.println("            for (i = 0; i < " + selector.privateMemberName + "Count; i++) {");
                    out.println("                " + typeName + " *newElement = " + selector.privateMemberName + "[i];");
                    out.println("                " + typeName + " *oldElement = (" + typeName + " *)nameHash.lookup((void *)newElement->" + fieldName + ".getStringValue());");
                    out.println("                if (oldElement)");
                    out.println("                    throw DuplicateUniqueValueException(oldElement->" + fieldName + ".getDOMElement(), newElement->" + fieldName + ".getDOMElement());");
                    out.println("                nameHash.insert((void *)newElement->" + fieldName + ".getStringValue(), newElement);");
                    out.println("            }");
                    out.println("        }");
                }

                for (SelectorField key : uniqueKeyKeyrefs.keys) {
                    out.println();
                    out.println("        // Check " + key.selector + " keys and keyrefs");
                    out.println("        {");
                    if (uniqueKeyKeyrefs.keyrefs.size() > 0) {
                        out.println("            static const XMLCh selectorTagName[] = " + CodeUtil.getXMLChArray(key.selector) + ";");
                        out.println("            static const XMLCh fieldTagName[] = " + CodeUtil.getXMLChArray(key.field) + ";");
                    }
                    out.println("            int i;");
                    out.println();

                    ContentModelElement keySelector = contentModel.getChildElement(key.selector);
                    String keySelectorTypeName = getChildClassName(keySelector.elementName);
                    String keyFieldName = CodeUtil.getCppSafeIdentifier(CodeUtil.getCamelCaseName(key.field));

                    out.println("            // Build a hash table of " + key.selector + " " + key.field + " values");
                    out.println("            SimpleStringHash nameHash(" + keySelector.privateMemberName + "Count + 1);");
                    out.println("            for (i = 0; i < " + keySelector.privateMemberName + "Count; i++) {");
                    out.println("                " + keySelectorTypeName + " *newElement = " + keySelector.privateMemberName + "[i];");
                    out.println("                " + keySelectorTypeName + " *oldElement = (" + keySelectorTypeName + " *)nameHash.lookup((void *)newElement->" + keyFieldName + ".getStringValue());");
                    out.println("                if (oldElement)");
                    out.println("                    throw DuplicateUniqueValueException(oldElement->" + keyFieldName + ".getDOMElement(), newElement->" + keyFieldName + ".getDOMElement());");
                    out.println("                nameHash.insert((void *)newElement->" + keyFieldName + ".getStringValue(), newElement);");
                    out.println("            }");

                    for (SelectorField keyref : uniqueKeyKeyrefs.keyrefs) {
                        if (keyref.refer.equals(key.name)) {
                            out.println();

                            ContentModelElement refField = null;

                            if (keyref.selector.equals(".")) {
                                refField = contentModel.getChildElement(keyref.field);

                                out.println("            // Resolve " + keyref.field + " key references");
                                out.println("            {");
                                out.println("                " + className + " *selector = this;");
                            } else {
                                ContentModelElement refSelector = contentModel.getChildElement(keyref.selector);
                                String refSelectorClassName = getChildClassName(refSelector.elementName);
                                ContentModel refSelectorContentModel = getGeneratedClassContentModel(refSelectorClassName);
                                refField = refSelectorContentModel.getChildElement(keyref.field);

                                out.println("            // Resolve " + keyref.selector + " " + keyref.field + " key references");
                                if (refSelector.isMultiple()) {
                                    out.println("            for (i = 0; i < " + refSelector.privateMemberName + "Count; i++) {");
                                    out.println("                " + refSelectorClassName + " *selector = " + refSelector.privateMemberName + "[i];");
                                } else if (refSelector.isOptional()) {
                                    out.println("            {");
                                    out.println("                " + refSelectorClassName + " *selector = " + refSelector.privateMemberName);
                                } else {
                                    out.println("            {");
                                    out.println("                " + refSelectorClassName + " *selector = &" + refSelector.privateMemberName);
                                }
                            }

                            if (refField.isMultiple()) {
                                out.println("                int j;");
                                out.println("                for (j = 0; j < selector->" + refField.getterName + "Count(); j++) {");
                                out.println("                    if (!nameHash.lookup((void *)selector->" + refField.getterName + "(j)->getStringValue()))");
                                out.println("                        throw UndefinedKeyException(selector->" + refField.getterName + "(j)->getDOMElement(), selectorTagName, fieldTagName);");
                                out.println("                }");
                            } else if (refField.isOptional()) {
                                out.println("                if (selector->" + refField.getterName + "()) {");
                                out.println("                    if (!nameHash.lookup((void *)selector->" + refField.getterName + "()->getStringValue()))");
                                out.println("                        throw UndefinedKeyException(selector->" + refField.getterName + "()->getDOMElement(), selectorTagName, fieldTagName);");
                                out.println("                }");
                            } else {
                                out.println("                if (!nameHash.lookup((void *)selector->" + refField.varName + ".getStringValue()))");
                                out.println("                    throw UndefinedKeyException(selector->" + refField.varName + ".getDOMElement(), selectorTagName, fieldTagName);");
                            }

                            out.println("            }");
                        }
                    }

                    out.println("        }");
                }
            }

            out.println("    }");
            out.println("    catch (const libxsd2cpp_1_0::ValidationException&) {");
            out.println("        destroy();");
            out.println("        throw;");
            out.println("    }");
        }
    }

    private void emitContentModelImplDestruction(ContentModel contentModel) {
        if (contentModel.multipleChildren.size() > 0) {
            out.println("    int i;");
        }

        if (contentModel.requiredChildren.size() > 0) {
            out.println();
            for (ContentModelElement child : contentModel.requiredChildren)
                out.println("    delete &" + child.varName + ";");
        }

        if (contentModel.optionalChildren.size() > 0) {
            out.println();
            for (ContentModelElement child : contentModel.optionalChildren)
                out.println("    delete " + child.privateMemberName + ";");
        }

        for (ContentModelElement child : contentModel.multipleChildren) {
            out.println();
            out.println("    for (i = 0; i < " + child.privateMemberName + "Count; i++)");
            out.println("        delete " + child.privateMemberName + "[i];");
            out.println("    if (" + child.privateMemberName + " != NULL)");
            out.println("        free(" + child.privateMemberName + ");");
        }
    }

    private static String stripNamespacePrefix(String className) {
        int i = className.indexOf("::");
        if (i != -1)
            return className.substring(i + 2);
        return className;
    }

    private static String stripTypeSuffix(String typeName) {
        int length = typeName.length();
        if (typeName.endsWith("Type"))
            length -= 4;
        return typeName.substring(0, length);
    }

    //-------------------------------------------------------------------------
    // Private member variables
    //-------------------------------------------------------------------------

    private String rootElementName;
    private String namespace;
    private String includePrefix;
    private String includeGuardMacroPrefix;
    private String generatedDir;
    private HashMap<String, String> typeMap;
    private Document document;
    private Stack<Element> elementStack = new Stack<Element>();
    private HashMap<String, String> elementClassMap = new HashMap<String, String>();
    private HashMap<String, ContentModel> generatedClassContentModelMap = new HashMap<String, ContentModel>();
    private PrintWriter out;
}

/**
 * CodeUtil
 */
class CodeUtil {
    static String getMacroName(String name) {
        StringBuffer sb = new StringBuffer();

        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            if (Character.isJavaIdentifierPart(c)) {
                sb.append(Character.toUpperCase(c));
            } else {
                sb.append('_');
            }
        }

        return sb.toString();
    }

    static String getPartialXMLChArray(String string) {
        StringBuffer sb = new StringBuffer();

        for (int j = 0; j < string.length(); j++) {
            char c = string.charAt(j);
            if (c == '-') {
                sb.append("chDash, ");
            } else if (c == '_') {
                sb.append("chUnderscore, ");
            } else if (c == ' ') {
                sb.append("chSpace, ");
            } else if (c == '*') {
                sb.append("chAsterisk, ");
            } else if (c == '/') {
                sb.append("chForwardSlash, ");
            } else if (c == ':') {
                sb.append("chColon, ");
            } else if (c == '.') {
                sb.append("chPeriod, ");
            } else if (c == ',') {
                sb.append("chComma, ");
            } else if (c == '%') {
                sb.append("chPercent, ");
            } else if (c == '[') {
                sb.append("chOpenSquare, ");
            } else if (c == ']') {
                sb.append("chCloseSquare, ");
            } else if (c == '<') {
                sb.append("chOpenAngle, ");
            } else if (c == '>') {
                sb.append("chCloseAngle, ");
            } else if (c == '(') {
                sb.append("chOpenParen, ");
            } else if (c == ')') {
                sb.append("chCloseParen, ");
            } else if (c == '\'') {
                sb.append("chSingleQuote, ");
            } else if (c == '"') {
                sb.append("chDoubleQuote, ");
            } else if (Character.isDigit(c)) {
                sb.append("chDigit_");
                sb.append(c);
                sb.append(", ");
            } else {
                sb.append("chLatin_");
                sb.append(c);
                sb.append(", ");
            }
        }

        return sb.toString();
    }

    static String getXMLChArray(String string) {
        StringBuffer sb = new StringBuffer();

        sb.append("{ ");
        sb.append(getPartialXMLChArray(string));
        sb.append("chNull }");

        return sb.toString();
    }

    static String getCppSafeIdentifier(String name) {
        if (name.equals("class"))
            return "className";
        return name;
    }

    static String getCamelCaseName(String name) {
        return getMixedCaseName(name, false);
    }

    static String getMixedCaseName(String name) {
        return getMixedCaseName(name, true);
    }

    static String getMixedCaseName(String name, boolean uppercaseFirstCharacter) {
        StringBuffer sb = new StringBuffer();

        boolean uppercase = uppercaseFirstCharacter;
        boolean lowercase = !uppercaseFirstCharacter;

        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            if (!Character.isJavaIdentifierPart(c)) {
                uppercase = true;
                lowercase = false;
            } else {
                if (uppercase) {
                    sb.append(Character.toUpperCase(c));
                } else if (lowercase) {
                    char l = Character.toLowerCase(c);
                    sb.append(l);
                    if (l == c)
                        lowercase = false;
                } else {
                    sb.append(c);
                }
                uppercase = false;
            }
        }

        return sb.toString();
    }

    static String getQuotedLiteral(String string) {
        String escaped = string.replace("\\", "\\\\");
        escaped = escaped.replace("\r", "\\r");
        escaped = escaped.replace("\n", "\\n");
        escaped = escaped.replace("\"", "\\\"");
        return "\"" + escaped + "\"";
    }
}

/**
 * SelectorField
 */
class SelectorField {
    SelectorField(Element element) {
        name = element.getAttribute("name");
        refer = element.getAttribute("refer");

        List<Element> selectors = SchemaUtil.getChildElements(element, "selector");
        selector = selectors.get(0).getAttribute("xpath");

        List<Element> fields = SchemaUtil.getChildElements(element, "field");
        field = fields.get(0).getAttribute("xpath");
    }

    String name;
    String refer;
    String selector;
    String field;
}

/**
 * UniqueKeyKeyrefs
 */
class UniqueKeyKeyrefs {
    UniqueKeyKeyrefs(Element elementDefinition) {
        uniques = getSelectorFields(elementDefinition, "unique");
        keys = getSelectorFields(elementDefinition, "key");
        keyrefs = getSelectorFields(elementDefinition, "keyref");
    }

    private List<SelectorField> getSelectorFields(Element elementDefinition, String name) {
        List<SelectorField> selectorFields = new Vector<SelectorField>();

        List<Element> elements = SchemaUtil.getChildElements(elementDefinition, name);
        for (Element element : elements)
            selectorFields.add(new SelectorField(element));

        return selectorFields;
    }

    boolean isEmpty() {
        return uniques.size() == 0 && keys.size() == 0 && keyrefs.size() == 0;
    }

    List<SelectorField> uniques;
    List<SelectorField> keys;
    List<SelectorField> keyrefs;
}

/**
 * ImplicitValue
 */
class ImplicitValue {
    ImplicitValue(Element implicitElementDefinition) throws Exception {
        NamedNodeMap attributes = implicitElementDefinition.getAttributes();
        for (int i = 0; i < attributes.getLength(); i++) {
            Attr attr = (Attr) attributes.item(i);
            String attrName = attr.getName();
            if (attrName.equals("if-xpath")) {
                ifXPath = attr.getValue();
            } else if (attrName.equals("if-platform")) {
                ifPlatform = attr.getValue();
            } else if (attrName.equals("if-not-platform")) {
                ifNotPlatform = attr.getValue();
            } else if (attrName.equals("content-xpath")) {
                contentXPath = attr.getValue();
            } else if (attrName.equals("content-function")) {
                contentFunction = attr.getValue();
            } else {
                throw new Exception("Unknown appinfo attribute: " + attrName);
            }
        }

        Node firstChild = implicitElementDefinition.getFirstChild();
        if (firstChild != null) {
            domContent = implicitElementDefinition.getOwnerDocument().createDocumentFragment();
            NodeList childNodes = implicitElementDefinition.getChildNodes();
            for (int i = 0; i < childNodes.getLength(); i++)
                domContent.appendChild(childNodes.item(i).cloneNode(true));
        }

        int contentCount = 0;
        if (contentXPath != null)
            contentCount++;
        if (contentFunction != null)
            contentCount++;
        if (domContent != null)
            contentCount++;
        if (contentCount > 1)
            throw new Exception("Ambiguous implicit value for " + implicitElementDefinition.getLocalName());
    }

    boolean isDefaultImplicitValue() {
        return ifXPath == null;
    }

    String ifXPath;
    String ifPlatform;
    String ifNotPlatform;
    String contentXPath;
    String contentFunction;
    DocumentFragment domContent;
}

/**
 * ContentModelType
 */
enum ContentModelType { ALL, CHOICE };

/**
 * ContentModelElementOccurs
 */
enum ContentModelElementOccurs { REQUIRED, OPTIONAL, MULTIPLE };

/**
 * ContentModelElement
 */
class ContentModelElement {
    ContentModelElement(Element elementDefinitionArg, int minOccurs, int maxOccurs) throws Exception {
        elementDefinition = elementDefinitionArg;
        elementName = elementDefinition.getAttribute("name");
        varName = CodeUtil.getCppSafeIdentifier(CodeUtil.getCamelCaseName(elementName));
        getterName = "get" + CodeUtil.getMixedCaseName(elementName);
        privateMemberName = "_" + CodeUtil.getCamelCaseName(elementName);
        if (maxOccurs != 1)
            privateMemberName += "s";
        tagNameConstant = privateMemberName + "TagName";

        if (elementDefinition.hasAttribute("default"))
            defaultValue = elementDefinition.getAttribute("default");

        NodeList xsAppinfoElements = elementDefinition.getElementsByTagNameNS(XMLConstants.W3C_XML_SCHEMA_NS_URI, "appinfo");
        for (int i = 0; i < xsAppinfoElements.getLength(); i++) {
            NodeList appinfoNodes = xsAppinfoElements.item(i).getChildNodes();
            for (int j = 0; j < appinfoNodes.getLength(); j++) {
                Node appinfoNode = appinfoNodes.item(j);
                if (appinfoNode.getNodeType() == Node.ELEMENT_NODE) {
                    Element appinfoElement = (Element) appinfoNode;
                    String appinfoElementName = appinfoElement.getLocalName();
                    if (appinfoElementName.equals("implicit")) {
                        implicitValues.add(new ImplicitValue(appinfoElement));
                    } else if (appinfoElementName.equals("validation")) {
                        System.err.println("Unhandled appinfo element: validation"); // XXX
                    } else {
                        throw new Exception("Unknown appinfo element: " + appinfoElement.getLocalName());
                    }
                }
            }
        }

        if (maxOccurs == 1 && (minOccurs == 1 || hasDefaultImplicitValue())) {
            occurs = ContentModelElementOccurs.REQUIRED;
        } else if (maxOccurs == 1 && minOccurs == 0) {
            occurs = ContentModelElementOccurs.OPTIONAL;
        } else if (maxOccurs == -1) {
            occurs = ContentModelElementOccurs.MULTIPLE;
        } else {
            throw new Exception("Unexpected minOccurs/maxOccurs combination for " + elementName);
        }

        if (implicitValues.size() > 0 && maxOccurs != 1)
            throw new Exception("Element " + elementName + " with maxOccurs != 1 must not have an implicit value");
    }

    boolean isRequired() {
        return occurs == ContentModelElementOccurs.REQUIRED;
    }

    boolean isOptional() {
        return occurs == ContentModelElementOccurs.OPTIONAL;
    }

    boolean isMultiple() {
        return occurs == ContentModelElementOccurs.MULTIPLE;
    }

    boolean hasDefaultImplicitValue() {
        for (int i = 0; i < implicitValues.size(); i++) {
            if (implicitValues.get(i).isDefaultImplicitValue())
                return true;
        }
        return false;
    }

    Element elementDefinition;
    String elementName;
    String elementContextPath;
    String varName;
    String getterName;
    String privateMemberName;
    String tagNameConstant;
    String defaultValue;
    List<ImplicitValue> implicitValues = new Vector<ImplicitValue>();
    ContentModelElementOccurs occurs;
}

/**  
 * ContentModel
 */
class ContentModel {
    ContentModel() {
    }

    ContentModel(Element complexTypeDefinition, String name) throws Exception {
        if (complexTypeDefinition == null)
            return;

        ContentModelType contentModelType;
        Element contentModelElement;
        Element allElement = SchemaUtil.getLocalDefinition(complexTypeDefinition, "all");
        Element choiceElement = SchemaUtil.getLocalDefinition(complexTypeDefinition, "choice");
        if (allElement != null) {
            contentModelType = ContentModelType.ALL;
            contentModelElement = allElement;
        } else if (choiceElement != null) {
            contentModelType = ContentModelType.CHOICE;
            contentModelElement = choiceElement;
        } else {
            throw new Exception("Unknown content model for " + name);
        }

        List<Element> childElements = SchemaUtil.getChildElements(contentModelElement, "element");
        for (int i = 0; i < childElements.size(); i++) {
            Element childElement = childElements.get(i);
            if (childElement.hasAttribute("name")) {
                String childElementName = childElement.getAttribute("name");

                int minOccurs;
                if (childElement.hasAttribute("minOccurs")) {
                    minOccurs = getCardinality(childElement.getAttribute("minOccurs"));
                } else if (contentModelType == ContentModelType.ALL) {
                    minOccurs = 1;
                } else {
                    throw new Exception("Expected maxOccurs attribute for " + childElementName);
                }

                int maxOccurs;
                if (childElement.hasAttribute("maxOccurs")) {
                    maxOccurs = getCardinality(childElement.getAttribute("maxOccurs"));
                } else if (contentModelType == ContentModelType.ALL) {
                    maxOccurs = 1;
                } else {
                    throw new Exception("Expected maxOccurs attribute for " + childElementName);
                }

                ContentModelElement child = new ContentModelElement(childElement, minOccurs, maxOccurs);
                children.add(child);
                if (child.isMultiple()) {
                    multipleChildren.add(child);
                } else if (child.isOptional()) {
                    optionalChildren.add(child);
                } else {
                    requiredChildren.add(child);
                }
            }
        }
    }

    boolean empty() {
        return children.size() == 0;
    }

    ContentModelElement getChildElement(String elementName) {
        for (ContentModelElement child : children) {
            if (child.elementName.equals(elementName))
                return child;
        }
        return null;
    }

    int getCardinality(String cardinality) {
        if (cardinality.equals("unbounded"))
            return -1;
        return Integer.parseInt(cardinality);
    }

    List<ContentModelElement> children = new Vector<ContentModelElement>();
    List<ContentModelElement> requiredChildren = new Vector<ContentModelElement>();
    List<ContentModelElement> optionalChildren = new Vector<ContentModelElement>();
    List<ContentModelElement> multipleChildren = new Vector<ContentModelElement>();
}
