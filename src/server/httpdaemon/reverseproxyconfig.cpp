/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright (c) 2026, The Heliod Web Server Project. All rights reserved.
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

 * Joe McCabe
 * 
 * Determines whether perfdump and the errors log report the raw TCP peer IP
 * or a reverse-proxy-forwarded client IP. See also the per-virtual-server
 * <reverse-proxy> server.xml element,  which can override any of these settings.

 * This does NOT change what is logged in the access log. An administrator
 * must change the access log format to change IP addresses logged there.

 */

#include "httpdaemon/reverseproxyconfig.h"

#include <string.h>

#include "netsite.h"
#include "frame/conf.h"
#include "base/ereport.h"
#include "base/util.h"

//-----------------------------------------------------------------------------
// ReverseProxyConfig::ReverseProxyConfig
//-----------------------------------------------------------------------------

ReverseProxyConfig::ReverseProxyConfig()
: ipHeader_(NULL), logAttribution_(REVERSE_PROXY_LOG_ATTRIBUTION_ACTUAL)
{
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::ReverseProxyConfig (copy constructor)
//-----------------------------------------------------------------------------

ReverseProxyConfig::ReverseProxyConfig(const ReverseProxyConfig& other)
: ipHeader_(NULL), logAttribution_(REVERSE_PROXY_LOG_ATTRIBUTION_ACTUAL)
{
    copyFrom(other);
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::operator=
//-----------------------------------------------------------------------------

ReverseProxyConfig& ReverseProxyConfig::operator=(const ReverseProxyConfig& other)
{
    if (this != &other) {
        if (ipHeader_)
            PERM_FREE(ipHeader_);
        ipHeader_ = NULL;
        copyFrom(other);
    }
    return *this;
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::copyFrom
//-----------------------------------------------------------------------------

void ReverseProxyConfig::copyFrom(const ReverseProxyConfig& other)
{
    ipHeader_ = (other.ipHeader_ && *other.ipHeader_) ? PERM_STRDUP(other.ipHeader_) : NULL;
    trustedIPs_ = other.trustedIPs_;
    logAttribution_ = other.logAttribution_;
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::~ReverseProxyConfig
//-----------------------------------------------------------------------------

ReverseProxyConfig::~ReverseProxyConfig()
{
    if (ipHeader_)
        PERM_FREE(ipHeader_);
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::overrideIPHeader
//-----------------------------------------------------------------------------

void ReverseProxyConfig::overrideIPHeader(const char *header)
{
    if (!header || !*header)
        return;

    if (ipHeader_)
        PERM_FREE(ipHeader_);
    ipHeader_ = PERM_STRDUP(header);
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::overrideLogAttribution
//-----------------------------------------------------------------------------

void ReverseProxyConfig::overrideLogAttribution(ReverseProxyLogAttribution attribution)
{
    logAttribution_ = attribution;
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::addTrustedIP
//-----------------------------------------------------------------------------

PRBool ReverseProxyConfig::addTrustedIP(const char *ipOrCidr, char *errbuf, int errbuflen)
{
    return trustedIPs_.add(ipOrCidr, errbuf, errbuflen);
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::parseLogAttribution
//-----------------------------------------------------------------------------

ReverseProxyLogAttribution ReverseProxyConfig::parseLogAttribution(const char *value)
{
    if (value && !strcasecmp(value, "proxied-ip"))
        return REVERSE_PROXY_LOG_ATTRIBUTION_PROXIED_IP;

    if (value && !strcasecmp(value, "both"))
        return REVERSE_PROXY_LOG_ATTRIBUTION_BOTH;

    return REVERSE_PROXY_LOG_ATTRIBUTION_ACTUAL;
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig::initFromMagnusConf
//-----------------------------------------------------------------------------

void ReverseProxyConfig::initFromMagnusConf()
{
    const char *header = conf_getstring("ReverseProxyIPHeader", NULL);
    ipHeader_ = (header && *header) ? PERM_STRDUP(header) : NULL;

    const char *trustedSpec = conf_getstring("ReverseProxyTrustedIPs", NULL);
    if (trustedSpec && *trustedSpec) {
        char errbuf[256];
        if (!trustedIPs_.add(trustedSpec, errbuf, sizeof(errbuf))) {
            ereport(LOG_MISCONFIG,
                    "Invalid ReverseProxyTrustedIPs directive: %s",
                    errbuf);
        }
    }

    const char *attribution = conf_getstring("ReverseProxyLogAttribution", NULL);
    logAttribution_ = parseLogAttribution(attribution);

    if (ipHeader_ && trustedIPs_.isEmpty()) {
        ereport(LOG_WARN,
                "ReverseProxyIPHeader is set but ReverseProxyTrustedIPs is "
                "empty; perfdump and the errors log will continue to show "
                "the actual client IP for every request");
    }
}

//-----------------------------------------------------------------------------
// ReverseProxyConfig_GetGlobal
//-----------------------------------------------------------------------------

static ReverseProxyConfig _reverseProxyGlobalConfig;

const ReverseProxyConfig& ReverseProxyConfig_GetGlobal()
{
    return _reverseProxyGlobalConfig;
}

// Non-const accessor used only by WebServer::Init() during startup, before
// any request-processing threads exist.
ReverseProxyConfig& ReverseProxyConfig_GetGlobalForInit()
{
    return _reverseProxyGlobalConfig;
}
