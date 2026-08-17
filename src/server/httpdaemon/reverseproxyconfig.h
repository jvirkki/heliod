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
 */

/*
 * Joe McCabe
 *
 * reverseproxyconfig.h
 *
 * Global (magnus.conf) configuration controlling whether perfdump and the
 * errors log report the raw TCP peer IP or a reverse-proxy-forwarded client
 * IP. See also the per-virtual-server <reverse-proxy> server.xml element,
 * which can override any of these settings.
 *
 * magnus.conf directives:
 *
 *   ReverseProxyIPHeader <header-name>
 *       Name of the HTTP request header a trusted reverse proxy uses to
 *       convey the original client IP (e.g. "X-Forwarded-For"). If unset
 *       (the default), the feature is disabled and perfdump/errors log
 *       always show the actual TCP peer IP.
 *
 *   ReverseProxyTrustedIPs <ip-or-cidr> [<ip-or-cidr> ...]
 *       Whitespace/comma-separated list of IPv4/IPv6 addresses or CIDR
 *       network ranges (e.g. "10.0.0.0/8 192.168.1.5") that are trusted to
 *       supply ReverseProxyIPHeader. A request whose TCP peer isn't in this
 *       list never has its reported IP overridden, regardless of what
 *       headers it sends. Default is empty (no proxy is trusted).
 *
 *   ReverseProxyLogAttribution actual|proxied-ip|both
 *       Which IP(s) perfdump/errors log show. Default is "actual", so the
 *       feature has no effect unless explicitly configured.
 *         actual     - always show the real TCP peer IP (unchanged behavior).
 *         proxied-ip - show the resolved/reported IP (falls back to the actual
 *                      IP if it couldn't be resolved for a given request).
 *         both       - show both. Perfdump already carries the actual and
 *                      reported IPs in separate fields
 *                      (addressClient/addressClientReported, or the "Client"/
 *                      "Reported Client" columns), so "both" just means both
 *                      are populated. The errors log has only one %s slot for
 *                      the client IP, so HttpRequest::GetLoggedClientIP()
 *                      formats a single combined string:
 *                      "<actual-ip> (reported: <reported-ip>)" -- always with
 *                      both values present (even if reported == actual), so
 *                      the format is stable for log scrapers.
 *
 * Each VirtualServer builds its own effective ReverseProxyConfig by copying
 * the global config and then applying its <reverse-proxy> element's
 * overrides, if any (see VirtualServer::getReverseProxyConfig() in
 * vsconf.h).
 */

#ifndef REVERSEPROXYCONFIG_H
#define REVERSEPROXYCONFIG_H

#include <nspr.h>

#include "httpdaemon/trustedproxylist.h"

enum ReverseProxyLogAttribution {
    REVERSE_PROXY_LOG_ATTRIBUTION_ACTUAL = 0,
    REVERSE_PROXY_LOG_ATTRIBUTION_PROXIED_IP = 1,
    REVERSE_PROXY_LOG_ATTRIBUTION_BOTH = 2
};

class ReverseProxyConfig {
public:
    ReverseProxyConfig();
    ReverseProxyConfig(const ReverseProxyConfig& other);
    ReverseProxyConfig& operator=(const ReverseProxyConfig& other);
    ~ReverseProxyConfig();

    // Reads the ReverseProxy* directives from the already-parsed
    // magnus.conf globals (via conf_getstring/conf_getboolean) and
    // populates this object. Called once, during server startup.
    void initFromMagnusConf();

    PRBool isEnabled() const { return ipHeader_ && *ipHeader_; }
    const char *getIPHeader() const { return ipHeader_; }
    const TrustedProxyList& getTrustedIPs() const { return trustedIPs_; }
    ReverseProxyLogAttribution getLogAttribution() const { return logAttribution_; }

    // Overrides this config's IP header. Pass NULL or "" to leave the
    // current value unchanged. Used to apply a <reverse-proxy> ip-header
    // override on top of a copy of the global config.
    void overrideIPHeader(const char *header);

    // Overrides this config's log attribution.
    void overrideLogAttribution(ReverseProxyLogAttribution attribution);

    // Adds an additional trusted IP/CIDR entry (e.g. from a <reverse-proxy>
    // trusted-ip element) on top of whatever this config already has.
    // Returns PR_TRUE on success; see TrustedProxyList::add for errbuf usage.
    PRBool addTrustedIP(const char *ipOrCidr, char *errbuf = NULL, int errbuflen = 0);

    // Parses "actual"/"proxied-ip"/"both" (case-insensitive); returns the
    // default (actual) for anything else, including NULL/empty.
    static ReverseProxyLogAttribution parseLogAttribution(const char *value);

private:
    void copyFrom(const ReverseProxyConfig& other);

    char *ipHeader_;
    TrustedProxyList trustedIPs_;
    ReverseProxyLogAttribution logAttribution_;
};

// The single global (magnus.conf-derived) ReverseProxyConfig instance.
// Per-virtual-server <reverse-proxy> settings, when present, override this.
const ReverseProxyConfig& ReverseProxyConfig_GetGlobal();

// Mutable accessor used only by WebServer::Init() to populate the global
// config from magnus.conf during startup, before any request-processing
// threads exist.
ReverseProxyConfig& ReverseProxyConfig_GetGlobalForInit();

#endif // REVERSEPROXYCONFIG_H
