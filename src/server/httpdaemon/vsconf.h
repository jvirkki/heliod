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

#ifndef VSCONF_H
#define VSCONF_H

#include "httpdaemon/nvpairs.h"

#include "support/LinkedList.hh"
#include "frame/objset.h"
#include "support/NSString.h"
#include "support/GenericVector.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/servername.h"
#include "httpdaemon/mime.h"
#include "httpdaemon/AuthDb.h"
#include "base/sslconf.h"
#include "httpdaemon/throttling.h"
#include "libaccess/acl.h"              // basic ACL data structures
#ifdef FEAT_SECRULE
#include "libsecrule/SecRec.h"      
#endif

class AuthDb;

class NVPairTree : public NVPairs
{
    public:
        NVPairTree(const NVPairs& first, const NVPairs& second);

        // Find the value associated with name.  Do not free this storage.
        const char* findValue(const char* name) const;

    private:
        const NVPairs& vars1;
        const NVPairs& vars2;
};

typedef enum ACLShortcut {
    ACL_SHORTCUT_UNKNOWN = -1,    // have not checked for shortcut
    ACL_SHORTCUT_NONE = 0,        // no shortcut
    ACL_SHORTCUT_ALWAYS_ALLOW = 1 // ACL always allows
} ACLShortcut_t;

class VirtualServer : public ServerXMLSchema::VirtualServerWrapper, public ConfigurationObject
{
    public:
        VirtualServer(ServerXMLSchema::VirtualServer& config, ConfigurationObject *parent, Configuration *server);
        ~VirtualServer();
        const NVPairs& getVars() const { return vartree; }
        httpd_objset* getObjset() const;
        const NSString& getDefaultObjectName() const { return objname; }
        const NSString& getNormalizedDocumentRoot() const { return docroot; }
        void* getUserData(int slot) const { return userVector[slot]; }
#ifdef FEAT_SECRULE
        const SecRec *getSecRec() const { return secRec; }
#endif
        void setUserData(int slot, void* data) const;
        GenericVector& getUserVector();
        const GenericVector& getUserVector() const;
        inline ACLListHandle_t *getACLList() const;
        inline ACLShortcut_t getACLMethodShortcut(int method_num) const;
        inline void setACLMethodShortcut(int method_num, ACLShortcut_t shortcut) const;
        void setACLList(ACLListHandle_t *acllist);
        ACLVirtualDb_t *getVirtualDb(const char *virtdbname) const;
        inline Mime& getMime();
        inline const Mime& getMime() const;
        inline VSTrafficStats& getTrafficStats() const;
        const ServerName *getNormalizedCanonicalServerName() const;

        PRBool isUnbound() const;
        void bind();

    protected:
        void cleanup();

        httpd_objset* objset;
        NVPairs vars;
        NVPairTree vartree;
        NSString objname;
        NSString docroot;
        CList<AuthDb> authDbList;
        ACLVirtualDb_t *defaultVirtualDb;
        GenericVector userVector; // user-defined data associated with this VS
        ACLListHandle_t *aclroot;         // the parsed acl list from the ACL file
        ACLShortcut_t acl_method_shortcuts[METHOD_MAX]; // tracks if ACLs apply to a given HTTP method
        Mime mime;
        VSTrafficStats stats;
        ServerName *canonicalServerName;
#ifdef FEAT_SECRULE
        SecRec *secRec;
#endif
        PRBool unbound;
};

inline Mime& VirtualServer :: getMime()
{
    return mime;
}

inline const Mime& VirtualServer :: getMime() const
{
    return mime;
}

inline VSTrafficStats& VirtualServer :: getTrafficStats() const
{
    return (VSTrafficStats&)stats;
}

inline ACLListHandle_t * VirtualServer :: getACLList() const
{
    return aclroot;
}

inline ACLShortcut_t VirtualServer :: getACLMethodShortcut(int method_num) const
{
    if (method_num < 0 || method_num >= METHOD_MAX)
        return ACL_SHORTCUT_NONE;

    return acl_method_shortcuts[method_num];
}

inline void VirtualServer :: setACLMethodShortcut(int method_num, ACLShortcut_t shortcut) const
{
    PR_ASSERT(method_num >= 0 && method_num < METHOD_MAX);
    PR_ASSERT(acl_method_shortcuts[method_num] == ACL_SHORTCUT_UNKNOWN ||
              acl_method_shortcuts[method_num] == shortcut);

    // XXX treat this as mutable
    ((ACLShortcut_t *)acl_method_shortcuts)[method_num] = shortcut;
}

#endif // VSCONF_H
