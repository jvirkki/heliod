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

#ifndef LIBSERVERXML_SERVERXML_H
#define LIBSERVERXML_SERVERXML_H

#if defined(XP_WIN32) && defined(BUILD_LIBSERVERXML_DLL)
#define LIBSERVERXML_EXPORT __declspec(dllexport)
#else
#define LIBSERVERXML_EXPORT
#endif

#include "xercesc/dom/DOMElement.hpp"
#include "generated/ServerXMLSchema/Server.h"
#include "support/NSString.h"

/**
 * SERVERXML_FILENAME is the default name of the server.xml file.
 */
#define SERVERXML_FILENAME "server.xml"

//-----------------------------------------------------------------------------
// ServerXMLSummary
//-----------------------------------------------------------------------------

/**
 * ServerXMLSummary is an unvalidated summary of a server.xml file, useful for
 * obtaining indentifying information from an instance's server.xml file
 * without heavyweight schema validation.
 */
class LIBSERVERXML_EXPORT ServerXMLSummary {
public:
    /**
     * Parse a server.xml file into a ServerXMLSummary.  Throws
     * EreportableException on unrecoverable error.  Never returns a NULL
     * ServerXMLSummary *.
     *
     * Note that parse may return successfully even if the specified server.xml
     * file is malformed.
     */
    static ServerXMLSummary *parse(const char *filename);

    /**
     * Return the server user.  May return NULL.
     */
    const char *getUser() const { return user.length() ? user.data() : NULL; }

    /**
     * Return the name of the configured platform.  May return NULL.
     */
    const char *getPlatform() const { return platform.length() ? platform.data() : NULL; }

    /**
     * Return the path to the server's temporary files, including its Unix
     * domain sockets and pid file.  Never returns NULL.
     */
    const char *getTempPath() const { return tempPath; }

#ifdef XP_UNIX
    /**
     * Return the path of the server's pid file.  Never returns NULL.
     */
    const char *getPIDFile() const { return pidFile; }
#endif

protected:
    ServerXMLSummary(const char *filename,
                     const char *user,
                     const char *platform,
                     const char *tempPath);

private:
    NSString user;
    NSString platform;
    NSString tempPath;
#ifdef XP_UNIX
    NSString pidFile;
#endif
};

//-----------------------------------------------------------------------------
// ServerXML
//-----------------------------------------------------------------------------

/**
 * ServerXML is a parsed, validated, and normalized representation of a
 * server.xml file.
 */
class LIBSERVERXML_EXPORT ServerXML : public ServerXMLSummary {
public:
    /**
     * Parse a server.xml file into a ServerXML.  Throws EreportableException
     * on error.  Never returns a NULL or invalid ServerXML *.
     */
    static ServerXML *parse(const char *filename);

    /**
     * Destroy a ServerXML previously returned by parse().
     */
    ~ServerXML();

    /**
     * The topmost element of a server.xml document.
     */
    ServerXMLSchema::Server& server;

    /**
     * Return the DOMDocument owned by this ServerXML instance.
     */
    const XERCES_CPP_NAMESPACE::DOMDocument *getDOMDocument();

private:
    ServerXML(const char *filename,
              ServerXMLSchema::Server *server,
              XERCES_CPP_NAMESPACE::DOMDocument *document);

    /**
     * Copy constructor is undefined.
     */
    ServerXML(const ServerXML& serverXML);

    /**
     * Assignment operator is undefined.
     */
    ServerXML& operator=(const ServerXML& right);

    /**
     * The DOMDocument owned by this ServerXML instance.
     */
    XERCES_CPP_NAMESPACE::DOMDocument *document;
};

#endif // LIBSERVERXML_SERVERXML_H
