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

#include "base/util.h"
#include "base/plist.h"
#include "base/ereport.h"
#include "libaccess/aclproto.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/AuthDb.h"

//-----------------------------------------------------------------------------
// AuthDb::AuthDb
//-----------------------------------------------------------------------------

AuthDb::AuthDb(ServerXMLSchema::AuthDb& authDb,
               ConfigurationObject *parent,
               const VirtualServer *vs)
: ServerXMLSchema::AuthDbWrapper(authDb),
  ConfigurationObject(parent),
  virtdb(NULL)
{
    NSErr_t err = NSERRINIT;
    int i;

    // Build a property list with the <auth-db>'s properties
    PList_t plist = PListNew(getPool());
    for (i = 0; i < authDb.getPropertyCount(); i++) {
        ServerXMLSchema::EncodableProperty& property = *authDb.getProperty(i);

        // Decode the property value if necessary
        const char *value;
        if (property.encoded) {
            char *decoded = util_uudecode(property.value);
            value = pool_strdup(getPool(), decoded);
            FREE(decoded);
        } else {
            value = property.value;
        }

        PListInitProp(plist, 0, property.name, value, 0);
    }

    // Attempt to register the database and retrieve the resulting dbname
    const char *dbname;
    int rv = ACL_VirtualDbRegister(&err, vs, authDb.name, authDb.url, plist, &dbname);

    // Destroy the property list (note that any decoded values are leaked until
    // the pool is cleaned up)
    PListDestroy(plist);

    if (rv < 0)
        throw ConfigurationServerXMLException(authDb, XP_GetAdminStr(DBT_Configuration_CannotInitAuthDb), &err);

    nserrDispose(&err);

    // Get a reference to the database
    ACL_VirtualDbRef(NULL, dbname, &virtdb);

    // Store any auth-expiring-url element for ACL_SetupEval()
    libxsd2cpp_1_0::String *eurl = authDb.getAuthExpiringUrl();
    if (eurl && *eurl) {
        ACL_VirtualDbWriteLock(virtdb);
        rv = ACL_VirtualDbSetAttr(&err, virtdb, "auth-expiring-url", *eurl);
        ACL_VirtualDbUnlock(virtdb);
        if (rv < 0)
            throw ConfigurationServerXMLException(authDb,
                XP_GetAdminStr(DBT_Configuration_CannotInitAuthDb), &err);
        nserrDispose(&err);
    }
}

//-----------------------------------------------------------------------------
// AuthDb::~AuthDb
//-----------------------------------------------------------------------------

AuthDb::~AuthDb()
{
    ACL_VirtualDbUnref(virtdb);
}

//-----------------------------------------------------------------------------
// ServerAuthDb::ServerAuthDb
//-----------------------------------------------------------------------------

ServerAuthDb::ServerAuthDb(ServerXMLSchema::AuthDb& authDb,
                           VirtualServer *vs)
: AuthDb(authDb, vs, NULL)
{ }


//-----------------------------------------------------------------------------
// VirtualServerAuthDb::VirtualServerAuthDb
//-----------------------------------------------------------------------------

VirtualServerAuthDb::VirtualServerAuthDb(ServerXMLSchema::AuthDb& authDb,
                                         VirtualServer *vs)
: AuthDb(authDb, vs, vs)
{ }
