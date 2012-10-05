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

#include <string.h>
#include "nspr.h"
#include "xercesc/dom/DOMDocument.hpp"
#include "xercesc/dom/DOMElement.hpp"
#include "xercesc/dom/DOMNode.hpp"
#include "support/SimpleHash.h"
#include "libxsd2cpp/CString.h"
#include "libserverxml/ServerXMLExceptionContext.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/ListenSocketConfig.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/ListenSockets.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "support/prime.h"
#include "base/net.h"
#include "base/util.h"
#include "base/daemon.h"
#include "base/ereport.h"
#include "frame/conf.h"
#include "libaccess/aclproto.h" // ACL function prototypes
#include "libaccess/aclerror.h" // aclErrorFmt prototype
#include "libaccess/aclcache.h" // ACLCache object definition

using XERCES_CPP_NAMESPACE::DOMDocument;
using XERCES_CPP_NAMESPACE::DOMElement;
using XERCES_CPP_NAMESPACE::DOMNode;
using LIBXSD2CPP_NAMESPACE::CString;

//-----------------------------------------------------------------------------
// Definitions of static member variables
//-----------------------------------------------------------------------------

PRInt32 Configuration::ids = 0;
PRInt32 Configuration::idInvalid = -1;

//-----------------------------------------------------------------------------
// Configuration::parse
//-----------------------------------------------------------------------------

Configuration *Configuration::parse()
{
    // Parse server.xml and create a ServerXML object.  This may throw
    // EreportableException.
    ServerXML *serverXML = ServerXML::parse(conf_get_true_globals()->vs_config_file);

    PR_ASSERT(serverXML != NULL);

    // Create a new Configuration based on the ServerXML object.  The
    // Configuration will assume ownership of the ServerXML object.
    return new Configuration(serverXML, PR_TRUE);
}

//-----------------------------------------------------------------------------
// Configuration::create
//-----------------------------------------------------------------------------

Configuration *Configuration::create(ServerXML *serverXML, PRBool deleteServerXML)
{
    // Create a new Configuration based on the ServerXML object.  The
    // Configuration does not assume ownership of the ServerXML object.
    return new Configuration(serverXML, deleteServerXML);
}

//-----------------------------------------------------------------------------
// Configuration::Configuration
//-----------------------------------------------------------------------------

Configuration::Configuration(ServerXML *serverXML, PRBool deleteServerXML)
: ServerXMLSchema::ServerWrapper(serverXML->server),
  ConfigurationObject(this),
  serverXML(serverXML),
  deleteServerXML(deleteServerXML),
  refcount(1),
  id(PR_AtomicIncrement(&ids)),
  pool(pool_create()),
  lscHash(0),
  vsHash(0),
  listener(0),
  aclcache(0)
{
    // Post process the server.xml configuration
    try {
        int count;
        int i;

        // Hash variable names
        count = getVariableCount();
        for (i = 0; i < count; i++)
            serverVars.addPair(getVariable(i)->name, getVariable(i)->value);

        // Instantiate a ListenSocketConfig for each ServerXMLSchema::HttpListener
        ServerXMLSchema::Pkcs11& pkcs11 = getPKCS11();
        count = getHttpListenerCount();
        for (i = 0; i < count; i++) {
            lscVector.append(new ListenSocketConfig(*getHttpListener(i), 
                                                    pkcs11, this));
        }

        // Instantiate a VirtualServer for each ServerXMLSchema::VirtualServer
        count = getVirtualServerCount();
        for (i = 0; i < count; i++) {
            vsVector.append(new VirtualServer(*getVirtualServer(i), this, this));
        }

        // Check for ListenSocketConfigs that have the same IP:port
        count = getLscCount();
        for (i = 0; i < count; i++) {
            ListenSocketConfig *lsc1 = getLsc(i);
            if (lsc1->enabled) {
                int j;
                for (j = i + 1; j < count; j++) {
                    ListenSocketConfig* lsc2 = getLsc(j);
                    if (lsc2->enabled && *lsc2 == *lsc1) {
                        ereport(LOG_WARN,
                                XP_GetAdminStr(DBT_Configuration_SameAddress),
                                lsc1->name.getStringValue(),
                                lsc2->name.getStringValue());
                    }
                }
            }
        }

        // Create and populate the table that will hash ListenSocketConfig 
        // IDs to pointers
        count = getLscCount();
        lscHash = new SimplePtrStringHash(findPrime(count));
        for (i = 0; i < count; i++) {
            // Add this ListenSocketConfig ID-to-pointer mapping
            ListenSocketConfig* lsc = getLsc(i);
            if (!lsc->enabled) {
                ereport(LOG_INFORM,
                        XP_GetAdminStr(DBT_Configuration_DisabledLs),
                        lsc->name.getStringValue());
            }
            if (lscHash->lookup((void*)lsc->name.getStringValue())) {
                throw ConfigurationServerXMLException(lsc->name, XP_GetAdminStr(DBT_Configuration_MultiplyDefined));
            }
            lscHash->insert((void*)lsc->name.getStringValue(), (void*)lsc);
        }

        // If we can't bind to INADDR_ANY and a specific IP simultaneously...
        if (!ListenSockets::canBindAnyAndSpecific()) {
            // Give INADDR_ANY ListenSocketConfigs a pointer to non-INADDR_ANY
            // ListenSocketConfigs on the same port
            count = getLscCount();
            for (int i = 0; i < count; i++) {
                ListenSocketConfig* lsc = getLsc(i);
                if (lsc->enabled && !lsc->hasExplicitIP()) {
                    for (int j = 0; j < count; j++) {
                        if (j != i) {
                            lsc->addIPSpecificConfig(getLsc(j));
                        }
                    }
                }
            }
        }

        // Create and populate the table that will hash VirtualServer IDs to
        // pointers
        vsHash = new SimplePtrStringHash(findPrime(getVSCount()));
        count = getVSCount();
        for (i = 0; i < count; i++) {
            VirtualServer* vs = getVS(i);
            if (!vs->enabled) {
                ereport(LOG_INFORM, XP_GetAdminStr(DBT_Configuration_DisabledVs), vs->name.getStringValue());
            }
            if (vsHash->lookup((void*)vs->name.getStringValue())) {
                throw ConfigurationServerXMLException(vs->name, XP_GetAdminStr(DBT_Configuration_MultiplyDefined));
            }
            vsHash->insert((void*)vs->name.getStringValue(), (void*)vs);
        }

        // Give ListenSocketConfigs a pointer to their default VirtualServers
        count = getLscCount();
        for (i = 0; i < count; i++) {
            ListenSocketConfig* lsc = getLsc(i);
            if (!lsc->enabled)
                continue;

            // Find default VS for ListenSocketConfig
            VirtualServer* vs = getVS(lsc->defaultVirtualServerName);
            if (!vs) {
                throw ConfigurationServerXMLException(lsc->defaultVirtualServerName, XP_GetAdminStr(DBT_Configuration_UndefinedVs));
            }
            if (!vs->enabled) {
                throw ConfigurationServerXMLException(lsc->defaultVirtualServerName, XP_GetAdminStr(DBT_Configuration_DisabledDefaultVs));
            }
            vs->bind(); // mark the VS as bound to a ListenSocketConfig

            // Give ListenSocketConfig a pointer to its default VS
            lsc->setDefaultVS(vs);

            // Check SSL properties
            SSLSocketConfiguration* sslc = lsc->getSSLParams();
            if (sslc) {
                sslc->CheckCertHosts(NULL, lsc);
            }
        }

        // Add each VirtualServer to the listen sockets it's attached to
        count = getVSCount();
        for (i = 0; i < count; i++) {
            VirtualServer* vs = getVS(i);
            if (vs->enabled && vs->isUnbound()) {
                ereport(LOG_WARN, XP_GetAdminStr(DBT_UnboundVS), vs->name.getStringValue());
            } else {
                addVs(vs);
            }
        }

        // Give VirtualServers a pointer to their MIME files
        SimplePtrStringHash mimeFileHash(getMimeFileCount() + 1);
        count = getVSCount();
        for (i = 0; i < count; i++) {
            int j;

            VirtualServer* vs = getVS(i);
            if (!vs->enabled)
                continue;

            // Add VS-specific MIME files
            for (j = 0; j < vs->getMimeFileCount(); j++) {
                vs->getMime().addMimeFile(parseMIMEFile(*vs->getMimeFile(j),
                                                        mimeFileHash));
            }

            // Add server-wide MIME files
            for (j = 0; j < getMimeFileCount(); j++) {
                vs->getMime().addMimeFile(parseMIMEFile(*getMimeFile(j),
                                                        mimeFileHash));
            }
        }

        // Set the name of the default ACL database.  This must be done after
        // we construct the configuration's AuthDbs (the AuthDb constructor
        // calls ACL_VirtualDbRegister) and before we parse its ACL files.
        ACL_DatabaseSetDefault(NULL, defaultAuthDbName);

        // construct ACLLists for the virtual servers
        SimplePtrStringHash globalAclFileHash(251);
        count = getVSCount();
        for (i = 0; i < count; i++) {
            int j;

            VirtualServer* vs = getVS(i);
            if (!vs->enabled)
                continue;

            // Build a list of all the ACLLists associated with this virtual server
            GenericVector vsAclListVector;
            SimplePtrStringHash vsAclFileHash(3);
            for (j = 0; j < vs->getAclFileCount(); j++) {
                // Check for VS-specific ACL files
                const char *filename = *vs->getAclFile(j);
                if (!vsAclFileHash.lookup((void*)filename)) {
                    ACLListHandle_t *acllist = parseACLFile(*vs->getAclFile(j), globalAclFileHash);
                    vsAclListVector.append(acllist);
                    vsAclFileHash.insert((void*)filename, (void*)acllist);
                }
            }
            for (j = 0; j < getAclFileCount(); j++) {
                // Check for server-wide ACL files
                const char *filename = *getAclFile(j);
                if (!vsAclFileHash.lookup((void*)filename)) {
                    ACLListHandle_t *acllist = parseACLFile(*getAclFile(j), globalAclFileHash);
                    vsAclListVector.append(acllist);
                    vsAclFileHash.insert((void*)filename, (void*)acllist);
                }
            }

            if (vsAclListVector.length() == 0)
                // no ACLList for this VS
                continue;

            ACLListHandle_t *aclroot = NULL;

            if (vsAclListVector.length() == 1) {
                // VS has just one ACLList so it can use the ACL file's ACLList
                // as is
                aclroot = (ACLListHandle_t *)vsAclListVector[0];
                ACL_ListIncrement(0, aclroot);
            } else {
                // VS has multiple ACLLists
                aclroot = ACL_ListNew(0);
                for (j = 0; j < vsAclListVector.length(); j++) {
                    ACLListHandle_t *acllist = (ACLListHandle_t *)vsAclListVector[j];
                    if (ACL_ListConcat(0, aclroot, acllist, 0) < 0) {
                        // XXX this error message could be more intellegible...
                        ACL_ListDecrement(0, aclroot);
                        throw EreportableException(LOG_FAILURE, XP_GetAdminStr(DBT_Configuration_CannotConstructAclLists));
                    }
                }
            }

            // copy the pointers over to the vs
            vs->setACLList(aclroot);
        }

        // create an ACL cache for this configuration
        // if "acl-cache" is enabled, then only create ACLCache object
        if (aclCache.enabled) {
            aclcache = new ACLCache();
        } else 
            aclcache = NULL;
    }
    catch (const EreportableException& e) {
        cleanup();
        throw;
    }
}

//-----------------------------------------------------------------------------
// Configuration::~Configuration
//-----------------------------------------------------------------------------

Configuration::~Configuration()
{
    cleanup();
}

//-----------------------------------------------------------------------------
// Configuration::cleanup
//-----------------------------------------------------------------------------

void Configuration::cleanup()
{
    int i;

    // Destroy the ACLLists we created
    for (i = 0; i < aclListVector.length(); i++)
        ACL_ListDecrement(0, (ACLListHandle_t *)aclListVector[i]);

    // Delete the name-to-pointer hash tables
    if (lscHash)
        delete lscHash;
    if (vsHash)
        delete vsHash;
    if (aclcache)
        delete aclcache;

    // We need to destroy all descendent ConfigurationObjects before we destroy
    // the pool as some may have allocated memory from the pool
    destroyChildren();

    if (pool)
        pool_destroy(pool);

    if (deleteServerXML)
        delete serverXML;
}

//-----------------------------------------------------------------------------
// Configuration::addVs
//-----------------------------------------------------------------------------

void Configuration::addVs(VirtualServer* vs)
{
    int i;

    // Call addVs for every listen socket associated with this VS
    for (i = 0; i < vs->getHttpListenerNameCount(); i++) {
        addVs(*vs->getHttpListenerName(i), vs);
    }
}

void Configuration::addVs(ServerXMLSchema::String& lsid, VirtualServer* vs)
{
    int i;

    // Find a ListenSocketConfig* from this name
    ListenSocketConfig* lsc = getLsc(lsid);
    if (!lsc)
        throw ConfigurationServerXMLException(lsid, XP_GetAdminStr(DBT_Configuration_UndefinedLs));

    // For every hostname in this VS...
    for (i = 0; i < vs->getHostCount(); i++) {
        // Check for an existing hostname->VirtualServer* association
        const char *host = *vs->getHost(i);
        VirtualServer* vsOld = lsc->findVS(host);
        if (vsOld) {
            // This hostname is already associated with a VirtualServer*.  If
            // there's another VirtualServer with the same hostname...
            if (vs != vsOld) {
                ereport(LOG_WARN,
                        XP_GetAdminStr(DBT_Configuration_DuplicateUrlhosts),
                        vsOld->name.getStringValue(),
                        vs->name.getStringValue(),
                        host);
            }
        } else {
            // Add this VirtualServer to the ListenSocketConfig
            lsc->addVS(vs, host);

            // Check SSL properties of the ListenSocketConfig against this SWVS
            SSLSocketConfiguration* sslc = lsc->getSSLParams();
            if (sslc)
                sslc->CheckCertHosts(vs, NULL);
        }
    }
}

//-----------------------------------------------------------------------------
// Configuration::setListener
//-----------------------------------------------------------------------------

void Configuration::setListener(ConfigurationListener* incoming)
{
    listener = incoming;
}

//-----------------------------------------------------------------------------
// Configuration::parseMIMEFile
//-----------------------------------------------------------------------------

MimeFile *Configuration::parseMIMEFile(ServerXMLSchema::String& mimeFile,
                                       SimplePtrStringHash& mimeFileHash)
{
    const char *filename = mimeFile;

    MimeFile *parsedMimeFile = (MimeFile *)mimeFileHash.lookup((void*)filename);
    if (parsedMimeFile == NULL) {
        parsedMimeFile = new MimeFile(mimeFile, this);
        mimeFileHash.insert((void*)filename, (void*)parsedMimeFile);
    }

    return parsedMimeFile;
}

//-----------------------------------------------------------------------------
// Configuration::parseACLFile
//-----------------------------------------------------------------------------

ACLListHandle_t *Configuration::parseACLFile(ServerXMLSchema::String& aclFile,
                                             SimplePtrStringHash& aclFileHash)
{
    const char *filename = aclFile;

    // If we haven't yet parsed this ACL file...
    ACLListHandle_t *acllist = (ACLListHandle_t *)aclFileHash.lookup((void*)filename);
    if (acllist == NULL) {
        NSErr_t err = NSERRINIT;

        // Parse the ACL file
        acllist = ACL_ParseFile(&err, (char *)filename);
        if (acllist == NULL)
            throw ConfigurationServerXMLException(aclFile, XP_GetAdminStr(DBT_Configuration_CannotParseAclFile), &err);

        // Put method indices into the ACL expression structures...
        if (ACL_ListPostParseForAuth(&err, acllist)) {
            ACL_ListDecrement(0, acllist);
            throw ConfigurationServerXMLException(aclFile, XP_GetAdminStr(DBT_Configuration_CannotPostParseAcls), &err);
        }

        nserrDispose(&err);

        // Remember the ACLList's ACLListHandle_t
        aclListVector.append(acllist);
        aclFileHash.insert((void*)filename, (void*)acllist);
    }

    return acllist;
}

//-----------------------------------------------------------------------------
// Configuration::getLscCount
//-----------------------------------------------------------------------------

int Configuration::getLscCount() const
{
    return lscVector.length();
}

//-----------------------------------------------------------------------------
// Configuration::getLsc
//-----------------------------------------------------------------------------

ListenSocketConfig* Configuration::getLsc(int i)
{
    return (ListenSocketConfig*)lscVector[i];
}

const ListenSocketConfig* Configuration::getLsc(int i) const
{
    return (const ListenSocketConfig*)lscVector[i];
}
  
ListenSocketConfig* Configuration::getLsc(const char* idLs)
{
    return (ListenSocketConfig*)lscHash->lookup((void*)idLs);
}
  
const ListenSocketConfig* Configuration::getLsc(const char* idLs) const
{
    return (const ListenSocketConfig*)lscHash->lookup((void*)idLs);
}

//-----------------------------------------------------------------------------
// Configuration::getVSCount
//-----------------------------------------------------------------------------

int Configuration::getVSCount() const
{
    return vsVector.length();
}

//-----------------------------------------------------------------------------
// Configuration::getVS
//-----------------------------------------------------------------------------

VirtualServer* Configuration::getVS(int i)
{
    return (VirtualServer*)vsVector[i];
}

const VirtualServer* Configuration::getVS(int i) const
{
    return (const VirtualServer*)vsVector[i];
}

VirtualServer* Configuration::getVS(const char* idVs)
{
    return (VirtualServer*)vsHash->lookup((void*)idVs);
}

const VirtualServer* Configuration::getVS(const char* idVs) const
{
    return (const VirtualServer*)vsHash->lookup((void*)idVs);
}

//-----------------------------------------------------------------------------
// getPRNetAddrFamily
//-----------------------------------------------------------------------------

static inline int getPRNetAddrFamily(PRUint16 prFamily)
{
    // AF_NCA addresses look like AF_INET, and NSPR doesn't like AF_NCA
    if (prFamily == AF_NCA)
        return AF_INET;
    return prFamily;
}

//-----------------------------------------------------------------------------
// ConfigurationObject::getNetAddrAndFamily
//-----------------------------------------------------------------------------

void ConfigurationObject::getNetAddrAndFamily(ServerXMLSchema::String& ip,
                                              ServerXMLSchema::Integer& port,
                                              ServerXMLSchema::Family& family,
                                              PRNetAddr *prNetAddr,
                                              PRUint16 *prFamily)
{
    memset(prNetAddr, 0, sizeof(*prNetAddr));

    *prFamily = 0;
    switch (family) {
    case ServerXMLSchema::Family::FAMILY_INET: *prFamily = PR_AF_INET; break;
    case ServerXMLSchema::Family::FAMILY_INET6: *prFamily = PR_AF_INET6; break;
    case ServerXMLSchema::Family::FAMILY_NCA: *prFamily = AF_NCA; break;
    }

    if (!strcmp(ip, "*")) {
        // An IP of "*" defaults to the inet family
        if (*prFamily == 0)
            *prFamily = PR_AF_INET;
        PR_SetNetAddr(PR_IpAddrAny, getPRNetAddrFamily(*prFamily), 0, prNetAddr);
    } else {
        // Format a PRNetAddr
        PRStatus rv = PR_StringToNetAddr(ip, prNetAddr);
        if (rv != PR_SUCCESS) {
            // That wasn't an IP; try to resolve the hostname
            ereport(LOG_VERBOSE, "Attempting to resolve %s", ip.getStringValue());
            PRHostEnt he;
            char buffer[PR_NETDB_BUF_SIZE];
            rv = PR_GetHostByName(ip, buffer, sizeof(buffer), &he);
            if (rv == PR_SUCCESS) {
                if (PR_EnumerateHostEnt(0, &he, 0, prNetAddr) < 0)
                    rv = PR_FAILURE;
            }
        }
        if (rv != PR_SUCCESS)
            throw ConfigurationServerXMLException(ip, XP_GetAdminStr(DBT_Configuration_ExpectedNetworkAddress));

        if (*prFamily == 0) {
            // No explicitly specified family, so use the IP address's family
            *prFamily = prNetAddr->raw.family;
        } else if (getPRNetAddrFamily(*prFamily) != prNetAddr->raw.family) {
            // The explicitly specified family didn't match the IP address
            throw ConfigurationServerXMLException(family, XP_GetAdminStr(DBT_Configuration_AddressFamilyMismatch));
        }
    }

    PR_NetAddrInetPort(prNetAddr) = PR_htons(port);
}

//-----------------------------------------------------------------------------
// ConfigurationObject::ref
//-----------------------------------------------------------------------------

const Configuration* ConfigurationObject::ref() const
{
    // One more reference to this configuration
    PR_AtomicIncrement(&configuration->refcount);
    return configuration;
}

//-----------------------------------------------------------------------------
// ConfigurationObject::unref
//-----------------------------------------------------------------------------

Configuration* ConfigurationObject::unref() const
{
    // One less reference to this configuration
    if (!PR_AtomicDecrement(&configuration->refcount)) {
        // Final reference removed.  The user (i.e. ConfigurationManager) is
        // responsible for managing any race.

        // If someone cared about our death, let them know the time is now
        if (configuration->listener) configuration->listener->releaseConfiguration(configuration);

        // The releaser thread in ConfigurationManager will delete the memory
        // associated with this object
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// ConfigurationObject::getPool
//-----------------------------------------------------------------------------

pool_handle_t* ConfigurationObject::getPool()
{
    return configuration->pool;
}

//-----------------------------------------------------------------------------
// ConfigurationObject::ConfigurationObject
//-----------------------------------------------------------------------------

ConfigurationObject::ConfigurationObject(ConfigurationObject *parentArg)
: parent(parentArg),
  configuration(parentArg->configuration)
{
    PR_ASSERT(parent != this);    
    parent->children.append(this);
}

ConfigurationObject::ConfigurationObject(Configuration *configurationArg)
: parent(NULL),
  configuration(configurationArg)
{
    PR_ASSERT(this == (ConfigurationObject *)configuration);
}

//-----------------------------------------------------------------------------
// ConfigurationObject::~ConfigurationObject
//-----------------------------------------------------------------------------

ConfigurationObject::~ConfigurationObject()
{
    destroyChildren();

    if (parent)
        parent->children.remove(this);
}

//-----------------------------------------------------------------------------
// ConfigurationObject::destroyChildren
//-----------------------------------------------------------------------------

void ConfigurationObject::destroyChildren()
{
    // delete all child ConfigurationObjects
    while (!children.isEmpty())
        delete (ConfigurationObject *)children.tail();
}

//-----------------------------------------------------------------------------
// ConfigurationServerXMLException::ConfigurationServerXMLException
//-----------------------------------------------------------------------------

ConfigurationServerXMLException::ConfigurationServerXMLException(ServerXMLSchema::Element& element,
                                                                 const EreportableException& e)
: EreportableException(e)
{
    setServerXMLContextDescription(element);
}

ConfigurationServerXMLException::ConfigurationServerXMLException(ServerXMLSchema::Element& element,
                                                                 const char *message)
: EreportableException(LOG_MISCONFIG, message)
{
    setServerXMLContextDescription(element);
}

ConfigurationServerXMLException::ConfigurationServerXMLException(ServerXMLSchema::Element& element,
                                                                 const char *message,
                                                                 NSErr_t *err)
: EreportableException(LOG_MISCONFIG, message)
{
    char aclErrorText[1024];
    aclErrorFmt(err, aclErrorText, sizeof(aclErrorText), 6);

    NSString error;
    error.append(getDescription());
    error.append(aclErrorText);

    setDescription(error);

    setServerXMLContextDescription(element);
}

//-----------------------------------------------------------------------------
// ConfigurationServerXMLException::setServerXMLContextDescription
//-----------------------------------------------------------------------------

void ConfigurationServerXMLException::setServerXMLContextDescription(ServerXMLSchema::Element& element)
{
    ServerXMLExceptionContext context(element);
    CString tagName(element.getTagName());

    NSString error;
    error.append(context.getContextPrefix());
    error.printf(XP_GetAdminStr(DBT_ErrorProcessingTagXPrefix),
                 tagName.getStringValue());
    error.append(getDescription());

    setDescription(error);
}

//-----------------------------------------------------------------------------
// ConfigurationFileFormatException::ConfigurationFileFormatException
//-----------------------------------------------------------------------------

ConfigurationFileFormatException::ConfigurationFileFormatException(const char *filename,
                                                                   int lineNumber,
                                                                   const char *message)
: EreportableException(LOG_MISCONFIG, message)
{
    setDescriptionf(XP_GetAdminStr(DBT_Configuration_ErrorParsingFileLineDesc),
                    filename,
                    lineNumber,
                    getDescription());
}

ConfigurationFileFormatException::ConfigurationFileFormatException(const char *filename,
                                                                   int lineNumber,
                                                                   int colNumber,
                                                                   const char *message)
: EreportableException(LOG_MISCONFIG, message)
{
    setDescriptionf(XP_GetAdminStr(DBT_Configuration_ErrorParsingFileLineColDesc),
                    filename,
                    lineNumber,
                    colNumber,
                    getDescription());
}

ConfigurationFileFormatException::ConfigurationFileFormatException(const char *filename,
                                                                   int lineNumber,
                                                                   int colNumber,
                                                                   const char *token,
                                                                   const char *message)
: EreportableException(LOG_MISCONFIG, message)
{
    setDescriptionf(XP_GetAdminStr(DBT_Configuration_ErrorParsingFileLineColTokenDesc),
                    filename,
                    lineNumber,
                    colNumber,
                    token,
                    getDescription());
}

