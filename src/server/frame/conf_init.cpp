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

#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"
#include "frame/conf_init.h"

//-----------------------------------------------------------------------------
// NSAPIConfigurationListener (ConfigurationListener interface)
//-----------------------------------------------------------------------------

class NSAPIConfigurationListener : public ConfigurationListener {
public:
    NSAPIConfigurationListener(ConfInitFunc* conf_init_func, ConfDestroyFunc* conf_destroy_func)
    : fnInit(conf_init_func), fnDestroy(conf_destroy_func)
    {
    }

    PRStatus setConfiguration(Configuration* incoming, const Configuration* current)
    { 
        int rv = REQ_PROCEED;
        if (fnInit) rv = (*fnInit)(incoming, current);
        return (rv == REQ_ABORTED) ? PR_FAILURE : PR_SUCCESS;
    }

    void releaseConfiguration(Configuration* outgoing)
    {
        if (fnDestroy) (*fnDestroy)(outgoing);
    }

private:
    ConfInitFunc* fnInit;
    ConfDestroyFunc* fnDestroy;
};

//-----------------------------------------------------------------------------
// conf_register_cb
//
// Server subsystems can use this function to register callbacks which will
// be called during server reconfiguration, allowing the subsystem to react
// to the new incoming config (i.e. to support dynamic reconfig).
//
// conf_init_func: called early in the reconfig process. The function
// should return PR_SUCCESS if there are no problems. It can also
// return PR_FAILURE to refuse the incoming configuration if it is invalid.
// Note that even after your conf_init_func returns PR_SUCCESS, some other
// subsystem's conf_init_func may later return PR_FAILURE in which case the
// incoming config will not be installed, so conf_init_func cannot assume the
// incoming config will necessarily become active. See conf_init.h for
// ConfInitFunc definition.
//
// conf_destroy_func: called after the incoming Configuration has been
// installed already. See conf_init.h for ConfDestroyFunc definition.
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int conf_register_cb(ConfInitFunc *conf_init_func, ConfDestroyFunc *conf_destroy_func)
{
    if (!conf_init_func && !conf_destroy_func) return REQ_ABORTED;

    // XXX The NSAPIConfigurationListener* is never delete'd
    ConfigurationManager::addListener(new NSAPIConfigurationListener(conf_init_func, conf_destroy_func));
    return REQ_PROCEED;
}
