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

/*
 * ListenSocketConfig class implementation.
 *
 * @since   iWS5.0
 */

#include <string.h>                          // strdup(), memcmp(), memset()
#include "base/ereport.h"                    // LOG_*
#include "base/util.h"
#include "base/net.h"
#include "httpdaemon/WebServer.h"
#include "httpdaemon/ListenSocketConfig.h"   // ListenSocketConfig class
#include "httpdaemon/dbthttpdaemon.h"

ListenSocketConfig::ListenSocketConfig(ServerXMLSchema::HttpListener& config, ServerXMLSchema::Pkcs11& pkcs11, ConfigurationObject* parent)
  : ConfigurationObject(parent),
    ServerXMLSchema::HttpListenerWrapper(config),
    serverName_(config.serverName,
                config.ssl.enabled ? HTTPS_URL : HTTP_URL,
                config.port),
    defaultVS_(NULL),
    bNoDelaySupported_(PR_FALSE),
    multipleVS_(PR_FALSE),
    vsHash_(7),
    sslparams_(NULL),
    ipConfigHash_(NULL)
{
    vsHash_.setMixCase();
    vsHostPatternList_.setAutoDestroy(PR_TRUE);

    // Process ip, port, and family
    getNetAddrAndFamily(config.ip, config.port, config.family, &address_, &family_);
    net_addr_to_string(&address_, ipString_, sizeof(ipString_));

    // Are we listening to something other than INADDR_ANY?
    bExplicitIP_ = net_has_ip(&address_);

    // NCA doesn't support TCP_NODELAY
    bNoDelaySupported_ = (family_ != AF_NCA);

    // Configure SSL
    try
    {
        if (ssl.enabled)
            sslparams_ = new SSLSocketConfiguration(ssl, pkcs11, this);
    }
    catch (...)
    {
        cleanup();
        throw;
    }
}

ListenSocketConfig::~ListenSocketConfig(void)
{
    cleanup();
}

void
ListenSocketConfig::cleanup(void)
{
    if (ipConfigHash_)
    {
        delete ipConfigHash_;
        ipConfigHash_ = NULL;
    }
}

int
ListenSocketConfig::operator==(const ListenSocketConfig& lsc) const
{
    return (net_addr_cmp(&this->address_, &lsc.address_) == 0);
}

int
ListenSocketConfig::operator!=(const ListenSocketConfig& lsc) const
{
    return !(*this == lsc);
}

PRStatus
ListenSocketConfig::addVS(VirtualServer* vs, const char* host)
{
    PRStatus status = PR_FAILURE;

    PR_ASSERT(defaultVS_ != NULL);

    if (vs != defaultVS_)
        multipleVS_ = PR_TRUE;

    if (!vsHash_.lookup((void*)host))
    {
        if (vsHash_.insert((void*)host, (void*)vs) == PR_TRUE)
            status = PR_SUCCESS;
    }

    if (shexp_valid(host) == VALID_SXP)
    {
        vsHostPatternList_.Append(new VirtualServerHostPattern(host, vs));
        status = PR_SUCCESS;
    }

    return status;
}

void
ListenSocketConfig::setDefaultVS(VirtualServer* vs)
{
    defaultVS_ = vs;
}

PRStatus
ListenSocketConfig::addIPSpecificConfig(ListenSocketConfig* lsc)
{
    PRStatus status = PR_FAILURE;

    if (!hasExplicitIP() &&
        lsc->hasExplicitIP() &&
        getFamily() == lsc->getFamily() &&
        port == lsc->port)
    {
        if (!ipConfigHash_)
            ipConfigHash_ = new ListenSocketConfigHash;

        if (ipConfigHash_)
        {
            if (ipConfigHash_->addConfig(lsc) == PR_TRUE)
                status = PR_SUCCESS;
        }
    }

    return status;
}

PRBool
ListenSocketConfig::hasIPSpecificConfig(ListenSocketConfig* lsc)
{
    if (ipConfigHash_)
    {
        for (int i = 0; i < ipConfigHash_->getConfigCount(); i++)
        {
            if (ipConfigHash_->getConfig(i) == lsc)
                return PR_TRUE;
        }
    }

    return PR_FALSE;
}

int
ListenSocketConfig::getConcurrency(void) const
{
    int maxAcceptors = 0;

    if (maxAcceptors < WebServer::GetConcurrency(getAcceptorThreads()));
        maxAcceptors = WebServer::GetConcurrency(getAcceptorThreads());

    if (ipConfigHash_)
    {
        for (int i = 0; i < ipConfigHash_->getConfigCount(); i++)
        {
            ListenSocketConfig *lsc = ipConfigHash_->getConfig(i);
            if (maxAcceptors < lsc->getConcurrency())
                maxAcceptors = lsc->getConcurrency();
        }
    }

    return maxAcceptors;
}

const char*
ListenSocketConfig::getListenScheme(void) const
{
    return ssl.enabled ? HTTPS_URL : HTTP_URL;
}

void
ListenSocketConfig::getIPAddressForURL(char *buffer, int size) const
{
    if (family_ == PR_AF_INET6)
        util_snprintf(buffer, size, "[%s]", (const char *)ip);
    else
        util_strlcpy(buffer, ip, size);
}

VirtualServerHostPattern::VirtualServerHostPattern(const char* exp, VirtualServer* vs)
  : vs_(vs)
{
    exp_ = strdup(exp);
}

VirtualServerHostPattern::~VirtualServerHostPattern()
{
    free(exp_);
}
