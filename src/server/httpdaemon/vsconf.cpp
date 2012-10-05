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
#include <ctype.h>
#include "netsite.h"
#include "base/util.h"
#include "frame/conf.h"
#include "libaccess/aclproto.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/vsconf.h"
#include "frame/log.h"

// NVPairTree is a helper class to lookup variables from 2 levels
NVPairTree::NVPairTree(const NVPairs& first,
                       const NVPairs& second)
: vars1(first), vars2(second)
{ }

// special 2-level lookup function
const char* NVPairTree :: findValue(const char* name) const
{
    const char *value = vars1.findValue(name);
    if (!value)
        value = vars2.findValue(name);
    return value;
}

// the actual VirtualServer class
// this is the representation of a VS for runtime

VirtualServer :: VirtualServer(ServerXMLSchema::VirtualServer& config,
                               ConfigurationObject *parent,
                               Configuration *server)
: ServerXMLSchema::VirtualServerWrapper(config),
  ConfigurationObject(parent),
  objset(NULL),
  vartree(vars, server->getVariables()),
  objname(defaultObjectName),
  defaultVirtualDb(NULL),
  aclroot(NULL),
  mime(this),
  stats(server->qos.enabled, server->qos.interval.getSecondsValue()),
  canonicalServerName(NULL)
{
    int i;

    vars.addPair("id", name);

    // Hash variable names
    for (i = 0; i < getVariableCount(); i++)
        vars.addPair(getVariable(i)->name, getVariable(i)->value);

    for (i = 0; i < METHOD_MAX; i++)
        acl_method_shortcuts[i] = ACL_SHORTCUT_UNKNOWN;

    unbound = (getHttpListenerNameCount() == 0);

    if (!enabled)
        return;

    // Normalize <document-root>
    docroot.setGrowthSize(strlen(documentRoot) + 1);
    char *t = file_canonicalize_path(documentRoot);
    if (!t)
        t = STRDUP(documentRoot);
    util_uri_normalize_slashes(t);
    if (t[0] == '/' && t[1] == '\0')
        t[0] = '\0'; // remove trailing slash
    docroot.append(t);
    FREE(t);

    // Normalize any canonical server name
    if (getCanonicalServerName())
        canonicalServerName = new ServerName(*getCanonicalServerName());

#ifdef FEAT_SECRULE
    secRec = NULL;
#endif
    try {
        // Register our own <auth-db>s
        for (i = 0; i < getAuthDbCount(); i++) {
            AuthDb *authDb = new VirtualServerAuthDb(*getAuthDb(i), this);
            authDbList.Append(authDb);
            if (!defaultVirtualDb && !strcasecmp(authDb->name, server->defaultAuthDbName))
                defaultVirtualDb = authDb->getVirtualDb();
        }

        // Connect to the <server>'s <auth-db>s, too
        for (i = 0; i < server->getAuthDbCount(); i++) {
            AuthDb *authDb = new ServerAuthDb(*server->getAuthDb(i), this);
            authDbList.Append(authDb);
            if (!defaultVirtualDb && !strcasecmp(authDb->name, server->defaultAuthDbName))
                defaultVirtualDb = authDb->getVirtualDb();
        }

        // Parse the objset template
        // XXX when multiple VSs share a template, we should parse it only once
        objset = objset_load(objectFile, NULL);
        if (!objset) {
            // failed to parse
            NSString error;
            error.printf(XP_GetAdminStr(DBT_Configuration_ErrorProcessingFile), objectFile.getStringValue());
            throw ConfigurationServerXMLException(objectFile, error);
        }

        // Substitute VS-specific $variables in object names
        if (objset_substitute_vs_vars(this, objset) != PR_SUCCESS) {
            NSString error;
            error.printf(XP_GetAdminStr(DBT_Configuration_ErrorSubstitutingFileReason), objectFile.getStringValue(), system_errmsg());
            throw ConfigurationServerXMLException(objectFile, error);
        }

        objset_interpolative(objset);

        // read config-file
        PtrVector<const char > configFileVector;
        // Check for VS-specific config-file elements
        for (int j = 0; j < getConfigFileCount(); j++) {
            const char *filename = *getConfigFile(j);
            configFileVector.append(filename);
        }
        // Check for server-wide config-file elements
        for (int k = 0; k < server->getConfigFileCount(); k++) {
            const char *filename = *server->getConfigFile(k);
            configFileVector.append(filename);
        }
        if (configFileVector.length() > 0) {
            log_error(LOG_WARN, NULL, NULL, NULL, 
                      XP_GetAdminStr(DBT_Configuration_UnsupportedFeatureBeingUsed), 
                      "config-file");
#ifdef FEAT_SECRULE
            secRec = new SecRec(configFileVector);
            if (secRec->errorFlag && secRec->filename) {
                libxsd2cpp_1_0::String *cfgfile = NULL;
                PRBool found = 0;
                for (int j = 0; j < getConfigFileCount() && !found; j++) {
                    cfgfile = getConfigFile(j);
                    const char *filename = cfgfile->getStringValue();
                    found = !strcmp(secRec->filename, filename);
                }
                for (int k = 0; k < server->getConfigFileCount() && !found; k++) {
                    cfgfile = server->getConfigFile(k);
                    const char *filename = cfgfile->getStringValue();
                    found = !strcmp(secRec->filename, filename);
                }
                throw ConfigurationServerXMLException(*cfgfile,
                    XP_GetAdminStr(DBT_Configuration_CannotParseConfigFile));
            }
#endif
        }
    }
    catch (...) {
        cleanup();
        throw;
    }
}

VirtualServer :: ~VirtualServer()
{
    cleanup();
}

void VirtualServer :: cleanup()
{
    if (objset)
        objset_free(objset);
    if (aclroot)
        ACL_ListDecrement(0, aclroot);
    if (canonicalServerName)
        delete canonicalServerName;
#ifdef FEAT_SECRULE
    if (secRec)
        delete secRec;
#endif
}

ACLVirtualDb_t *
VirtualServer::getVirtualDb(const char *virtdbname) const
{
    if (!virtdbname)
        return defaultVirtualDb;

    CListConstIterator<AuthDb> authDbIterator(&authDbList);

    // find userDB by ID in list of userdbs
    // do a linear search since we typically have only one
    // or two dbs per VS - a hash table might be triple
    // overkill here. Plus, 99.8% of the ACLs are going
    // to use the default database anyway where we do not
    // have to do any searching.
    while (const AuthDb *authDb = ++authDbIterator) {
        if (!strcasecmp(authDb->name, virtdbname))
            return authDb->getVirtualDb();
    }

    return NULL;
}

httpd_objset* VirtualServer :: getObjset() const
{
    return objset;
}

void VirtualServer :: setUserData(int slot, void* data) const
{
    // userVector should be mutable, but mutable is not widely supported
    ((VirtualServer*)this)->userVector[slot] = data;
}

GenericVector& VirtualServer :: getUserVector()
{
    return userVector;
}

const GenericVector& VirtualServer :: getUserVector() const
{
    return userVector;
}

void
VirtualServer::setACLList(ACLListHandle_t *acllist)
{
    if (aclroot)
        ACL_ListDecrement(0, aclroot);

    aclroot = acllist;
}

const ServerName * VirtualServer::getNormalizedCanonicalServerName() const
{
    return canonicalServerName;
}

PRBool VirtualServer :: isUnbound() const
{
    return unbound;
}

void VirtualServer :: bind()
{
    unbound = PR_FALSE;
}
