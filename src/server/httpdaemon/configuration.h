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

#ifndef HTTPDAEMON_CONFIGURATION_H
#define HTTPDAEMON_CONFIGURATION_H

#include "nspr.h"
#include "netsite.h"
#include "generated/ServerXMLSchema/Server.h"
#include "support/GenericList.h"
#include "support/GenericVector.h"
#include "support/EreportableException.h"
#include "base/pool.h"
#include "libserverxml/ServerXML.h"
#include "libaccess/acl.h"
#include "httpdaemon/throttling.h"
#include "httpdaemon/nvpairs.h"

class SimplePtrStringHash;
class ListenSocketConfig;
class VirtualServer;
class Configuration;
class ConfigurationListener;
class MimeFile;
class ACLCache;

//-----------------------------------------------------------------------------
// ConfigurationServerXMLException
//-----------------------------------------------------------------------------

/**
 * ConfigurationServerXMLException records exceptions encountered while
 * processing server.xml elements.
 */
class ConfigurationServerXMLException : public EreportableException {
public:
    /**
     * Construct a ConfigurationServerXMLException by adding server.xml context
     * to an existing EreportableException.  The caller must ensure that the
     * passed EreportableException is not a ConfigurationServerXMLException.
     */
    ConfigurationServerXMLException(ServerXMLSchema::Element& element,
                                    const EreportableException& e);

    /**
     * Construct a ConfigurationServerXMLException.  The passed localized
     * message string may be logged via ereport() and must begin with a message
     * ID prefix.
     */
    ConfigurationServerXMLException(ServerXMLSchema::Element& element,
                                    const char *message);

    /**
     * Construct a ConfigurationServerXMLException.  The passed localized
     * message string may be logged via ereport() and must begin with a message
     * ID prefix.
     */
    ConfigurationServerXMLException(ServerXMLSchema::Element& element,
                                    const char *message,
                                    NSErr_t *err);

protected:
    void setServerXMLContextDescription(ServerXMLSchema::Element& element);
};

//-----------------------------------------------------------------------------
// ConfigurationFileFormatException
//-----------------------------------------------------------------------------

/**
 * ConfigurationFileFormatException records exceptions caused by an improperly
 * formatted plain text configuration.
 */
class ConfigurationFileFormatException : public EreportableException {
public:
    /**
     * Construct a ConfigurationFileFormatException.  The passed localized
     * message string may be logged via ereport() and must begin with a message
     * ID prefix.
     */
    ConfigurationFileFormatException(const char *filename,
                                     int lineNumber,
                                     const char *message);

    /**
     * Construct a ConfigurationFileFormatException.  The passed localized
     * message string may be logged via ereport() and must begin with a message
     * ID prefix.
     */
    ConfigurationFileFormatException(const char *filename,
                                     int lineNumber,
                                     int colNumber,
                                     const char *message);

    /**
     * Construct a ConfigurationFileFormatException.  The passed localized
     * message string may be logged via ereport() and must begin with a message
     * ID prefix.
     */
    ConfigurationFileFormatException(const char *filename,
                                     int lineNumber,
                                     int colNumber,
                                     const char *token,
                                     const char *message);
};

//-----------------------------------------------------------------------------
// ConfigurationObject
//-----------------------------------------------------------------------------

/**
 * A ConfigurationObject is a member of a Configuration.  A ConfigurationObject
 * typically wraps a server.xml element (more precisely, a ConfigurationObject
 * typically wraps a ServerXMLSchema::Element) and introduces additional
 * members.
 *
 * Adding/removing a reference to a ConfigurationObject increases/decreases the
 * parent Configuration's reference count.
 *
 * Note that a ConfigurationObject is automatically destroyed when its parent
 * is destroyed.
 */
class ConfigurationObject {
public:
    /**
     * Increase the Configuration reference count.
     */
    const Configuration *ref() const;

    /**
     * Drecrease the Configuration reference count.  The caller must have
     * previously obtained a reference.  Returns (Configuration *)NULL.
     */
    Configuration *unref() const;

    /**
     * Return a pointer to the Configuration of which this ConfigurationObject
     * is a member.
     */
    const Configuration *getConfiguration() const { return configuration; }

    /**
     * Return a pointer to the memory pool for Configuration-scope allocations.
     */
    pool_handle_t *getPool();

protected:
    /**
     * Create a new ConfigurationObject as a child of the specific parent
     * ConfigurationObject.  The child will be automatically destroyed when
     * the parent is destroyed.
     */
    ConfigurationObject(ConfigurationObject *parent);

    /*
     * Destroy the ConfigurationObject and its children.  Generally called by a
     * parent ConfigurationObject's destructor.
     */
    virtual ~ConfigurationObject();

    /**
     * Helper function to obtain a PRNetAddr and family from server.xml
     * elements.  Throws ConfigurationServerXMLException on error.
     */
    static void getNetAddrAndFamily(ServerXMLSchema::String& ip,
                                    ServerXMLSchema::Integer& port,
                                    ServerXMLSchema::Family& family,
                                    PRNetAddr *prNetAddr,
                                    PRUint16 *prFamily);

private:
    /**
     * Create a new root ConfigurationObject.  Called by the Configuration
     * constructor.
     */
    ConfigurationObject(Configuration *configuration);

    /*
     * Destroy all descendent ConfigurationObjects.  Called by the
     * Configuration destructor.
     */
    void destroyChildren();

    /**
     * Copy constructor is undefined.
     */
    ConfigurationObject(const ConfigurationObject&);

    /**
     * Assignment operator is undefined.
     */
    ConfigurationObject& operator=(const ConfigurationObject&);

    /**
     * This ConfigurationObject's parent or NULL if this ConfigurationObject
     * is a Configuration.
     */
    ConfigurationObject *parent;

    /**
     * The Configuration of which this ConfigurationObject is a member.
     */
    Configuration *configuration;

    /**
     * List of this ConfigurationObject's child ConfigurationObjects.
     */
    GenericList children;

    friend class Configuration;
};

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

/**
 * A Configuration encapsulates an entire server configuration.
 */
class Configuration : public ServerXMLSchema::ServerWrapper,
                      public ConfigurationObject {
public:
    /**
     * Parse a new Configuration.  Throws EreportableException on error.  Never
     * returns a NULL Configuration *.
     */
    static Configuration *parse();

    /**
     * Construct a new Configuration from a previously parsed ServerXML.  If
     * deleteServerXML is PR_TRUE, the Configuration assumes ownership of the
     * passed ServerXML *.  Otherwise, the caller is responsible for deleting
     * the passed ServerXML after the Configuration is destroyed.  Throws
     * EreportableException on error.  Never returns a NULL Configuration *.
     */
    static Configuration *create(ServerXML *serverXML, PRBool deleteServerXML);

    /**
     * Return this Configuration's unique ID.
     */
    PRInt32 getID() const { return id; }

    /**
     * Return a pointer to the ACLCache owned by this Configuration.
     */
    ACLCache *getACLCache() const { return aclcache; }

    /**
     * Return a reference to the pkcs11 element owned by this Configuration.
     */
    ServerXMLSchema::Pkcs11& getPKCS11() const { return this->pkcs11; }
    
    /**
     * Return a reference to the TrafficStats owned by this Configuration.
     */
    TrafficStats& getTrafficStats() const { return (TrafficStats&)trafficStats; }

    /**
     * Return the number of ListenSocketConfig objects owned by this
     * Configuration.
     */
    int getLscCount() const;

    /**
     * Return a pointer to the ith ListenSocketConfig object.  The returned
     * ListenSocketConfig is owned by the Configuration.
     */
    ListenSocketConfig *getLsc(int i);

    /**
     * Return a pointer to the ith ListenSocketConfig object.  The returned
     * ListenSocketConfig is owned by the Configuration.
     */
    const ListenSocketConfig *getLsc(int i) const;

    /**
     * Return a pointer to the named ListenSocketConfig object.  The returned
     * ListenSocketConfig is owned by the Configuration.
     */
    ListenSocketConfig *getLsc(const char *id);

    /**
     * Return a pointer to the named ListenSocketConfig object.  The returned
     * ListenSocketConfig is owned by the Configuration.
     */
    const ListenSocketConfig *getLsc(const char* name) const;

    /**
     * Return a pointer to the ith VirtualServer object.  The returned
     * VirtualServer is owned by the Configuration.
     */
    int getVSCount() const;

    /**
     * Return a pointer to the ith VirtualServer object owned by this
     * Configuration.
     */
    VirtualServer *getVS(int i);

    /**
     * Return a pointer to the ith VirtualServer object owned by this
     * Configuration.
     */
    const VirtualServer *getVS(int i) const;

    /**
     * Return a pointer to the named VirtualServer object.  The returned
     * VirtualServer is owned by the Configuration.
     */
    VirtualServer *getVS(const char* id);

    /**
     * Return a pointer to the named VirtualServer object.  The returned
     * VirtualServer is owned by the Configuration.
     */
    const VirtualServer *getVS(const char* id) const;

    /**
     * Return a reference to the server variables.
     */
    const NVPairs& getVariables() const { return serverVars; }

    /**
     * Return a pointer to the memory pool for Configuration-scope allocations.
     */
    pool_handle_t *getPool() { return pool; }

    /**
     * A value that will never be returned by getID().
     */
    static PRInt32 idInvalid;

private:
    /**
     * Allow ConfigurationManager to destroy the Configuration.  Others should
     * use unref().
     */
    ~Configuration();

    /**
     * Allow ConfigurationManager to register a ConfigurationListener for final
     * unref() notification.
     */
    void setListener(ConfigurationListener *listener);

    /**
     * Instantiate a Configuration.
     */
    Configuration(ServerXML *serverXML, PRBool deleteServerXML);

    /**
     * Copy constructor is undefined.
     */
    Configuration(const Configuration&);

    /**
     * Assignment operator is undefined.
     */
    Configuration& operator=(const Configuration&);

    VirtualServer *findDefaultVS(ListenSocketConfig *lsc);
    void addVs(VirtualServer *vs);
    void addVs(ServerXMLSchema::String& lsid, VirtualServer* vs);
    void cleanup();
    MimeFile *parseMIMEFile(ServerXMLSchema::String& mimeFile, SimplePtrStringHash& mimeFileHash);
    ACLListHandle_t *parseACLFile(ServerXMLSchema::String& aclFile, SimplePtrStringHash& aclFileHash);

    ServerXML *serverXML;
    PRBool deleteServerXML;
    PRInt32 refcount;
    PRInt32 id;
    pool_handle_t *pool;
    GenericVector lscVector;
    GenericVector vsVector;
    SimplePtrStringHash *lscHash;
    SimplePtrStringHash *vsHash;
    GenericVector aclListVector;
    ConfigurationListener *listener;
    NVPairs serverVars;
    ACLCache *aclcache;
    TrafficStats trafficStats;

    static PRInt32 ids;

friend class ConfigurationObject;
friend class ConfigurationManager;
};

#endif // HTTPDAEMON_CONFIGURATION_H
