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

// The cinfo subsystem was overhauled with the addition of virtual server
// support for iWS 5.0.  Much of what used to live here was moved to 
// httpdaemon/mime.cpp

#include "base/cinfo.h"
#include "base/ereport.h"
#include "base/dbtbase.h"
#include "frame/conf.h"
#include "httpdaemon/vsconf.h"

//-----------------------------------------------------------------------------
// cinfo_find
//-----------------------------------------------------------------------------

NSAPI_PUBLIC cinfo* cinfo_find(char* uri)
{
    // Magic to find a VS
    const VirtualServer* vs = conf_get_vs();
    if (!vs)
        return NULL;
    return vs->getMime().getContentInfo(system_pool(), uri);
}

//-----------------------------------------------------------------------------
// cinfo_find_ext_type
//-----------------------------------------------------------------------------

NSAPI_PUBLIC const char* cinfo_find_ext_type(const char* ext)
{
    // Magic to find a VS
    const VirtualServer* vs = conf_get_vs();
    if (!vs)
        return NULL;
    const cinfo* cinfo = vs->getMime().findExt(ext);
    if (!cinfo)
        return NULL;
    return cinfo->type;
}

//-----------------------------------------------------------------------------
// Stubs for NSAPI functions we don't support any more
//-----------------------------------------------------------------------------

NSAPI_PUBLIC void cinfo_dump_database(FILE *dump)
{
    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_notSupportedInThisRelease), "cinfo_dump_database");
}

NSAPI_PUBLIC void cinfo_init(void)
{
    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_notSupportedInThisRelease), "cinfo_init");
}

NSAPI_PUBLIC void cinfo_terminate(void)
{
    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_notSupportedInThisRelease), "cinfo_terminate");
}

NSAPI_PUBLIC char *cinfo_merge(char *fn)
{
    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_notSupportedInThisRelease), "cinfo_merge");
    return NULL;
}

NSAPI_PUBLIC cinfo *cinfo_lookup(char *type)
{
    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_notSupportedInThisRelease), "cinfo_lookup");
    return NULL;
}
