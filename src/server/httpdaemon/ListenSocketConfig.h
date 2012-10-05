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

#ifndef _ListenSocketConfig_h_
#define _ListenSocketConfig_h_

#include "nspr.h"                                   // NSPR declarations
#include "generated/ServerXMLSchema/HttpListener.h" // HttpListener class
#include "support/SimpleHash.h"                     // SimpleStringHash class
#include "support/LinkedList.hh"                    // CList class
#include "base/shexp.h"                             // shexp_casecmp
#include "base/sslconf.h"                           // SSLSocketConfiguration
#include "httpdaemon/servername.h"                  // ServerName class
#include "httpdaemon/ListenSocketConfigHash.h"      // ListenSocketConfigHash class
#include "httpdaemon/configuration.h"               // ConfigurationObject class

class VirtualServer;
class VirtualServerHostPattern;

/**
 * Represents the configuration/parameters that are used to create/initialize
 * server/listen sockets.
 *
 * An internet server listens for incoming connection requests at a specific 
 * port number on the machine and responds to requests that are sent over
 * these connections. This class defines the attributes of such a socket, 
 * such as the IP address, port number etc.
 *
 * @since   iWS5.0
 */

class ListenSocketConfig : public ServerXMLSchema::HttpListenerWrapper, public ConfigurationObject
{
    public:

        /**
         * Constructor that uses the elements specified in a server.xml file
         * to construct an object of this class.
         *
         * @param config    The listen socket elements from server.xml.
         */
         ListenSocketConfig(ServerXMLSchema::HttpListener& config, ServerXMLSchema::Pkcs11& pkcs11, ConfigurationObject* parent);

        /**
         * Destructor.
         */
        ~ListenSocketConfig(void);

        /**
         * Return the server name the outside world uses in URLs that refer to
         * this listen socket.
         */
        const ServerName& getExternalServerName() const;

        /**
         * Returns the scheme (e.g. <code>http</code>) that the socket uses to
         * to listen for requests.
         */
        const char* getListenScheme(void) const;
        
        /**
         * Return a string representation of the IP address the socket listens
         * for requests on.
         */
        const char* getListenIPAddress() const;

        /**
         * Returns the network address information stored in this object.
         */
        PRNetAddr getAddress(void) const;

        /**
         * Obtains a string representation of the IP address suitable for use
         * in an URL. This mirrors <code>getIPAddress</code>, except that IPv6
         * addresses are bracketed by <code>'['</code> and <code>']'</code>.
         */
        void getIPAddressForURL(char *buffer, int size) const;

        /**
         * Returns the address family of the socket. This will differ from the
         * family in the PRNetAddr when a nonstandard family (i.e. AF_NCA) is in
         * use.
         */
        PRUint16 getFamily(void) const;

        /**
         * Returns the number of "acceptor" threads that accept
         * connections on the listen socket.
         */
        int getConcurrency(void) const;

        /**
         * Returns <code>PR_TRUE</code> if we're listening on something other
         * than INADDR_ANY.
         */
        PRBool hasExplicitIP(void) const;

        /**
         * Returns <code>PR_TRUE</code> if the socket's family supports
         * <code>PR_SockOpt_NoDelay</code>/<code>TCP_NODELAY</code>,
         * <code>PR_FALSE</code> otherwise.
         */
        PRBool isNoDelaySupported(void) const;

        /**
         * Adds an entry in the hash table associating a given host header
         * with a virtual server.
         *
         * @returns <code>PR_SUCCESS</code> if the entry was successfully 
         *          added to the hash table. <code>PR_FAILURE</code>
         *          indicates that either the specified host already exists
         *          in the table or that there was an error in adding the
         *          entry to the table.
         */
        PRStatus addVS(VirtualServer* vs, const char* host = NULL);

        /**
         * Finds a virtual server that corresponds to the given host header.
         */
        VirtualServer* findVS(const char* host) const;

        /**
         * Sets the default virtual server for requests received by this listen
         * socket.
         */
        void setDefaultVS(VirtualServer* vs);

        /**
         * Gets the default virtual server for requests received by this listen
         * socket.
         */
        VirtualServer* getDefaultVS();

        /**
         * Associate an IP-specific configuration with this listen socket.
         * This is only used when the OS doesn't let us listen on INADDR_ANY
         * and a specific IP simultaneously.
         */
        PRStatus addIPSpecificConfig(ListenSocketConfig* lsc);

        /**
         * Retrieve the configuration for the specified IP address.
         */
        ListenSocketConfig* getIPSpecificConfig(PRNetAddr& localAddress);

        /**
         * Return the number of IP-specific configurations associated with
         * this listen socket.
         */
        int getIPSpecificConfigCount(void);

        /**
         * Return the <code>i</code>th IP-specific configuration.
         */
        ListenSocketConfig* getIPSpecificConfig(int i);

        /**
         * Indicate whether the pass IP-specific configuration is associated
         * with this configuration.
         */
        PRBool hasIPSpecificConfig(ListenSocketConfig* lsc);

        /**
         * Gets the SSL configuration parameters used by this listen socket.
         */
        SSLSocketConfiguration* getSSLParams();
        const SSLSocketConfiguration* getSSLParams() const;

        /**
         * Equality operator.
         *
         * Two objects of this class are considered equal if the network
         * addresses are equal
         *
         * @param lsc The object to be compared against
         * @returns   1 if the objects are equal and 0 if they are not.
         */
        int operator==(const ListenSocketConfig& lsc) const;

        /**
         * Inequality operator.
         *
         * @param lsc The object to be compared against
         * @returns   0 if the objects are equal and 1 if they are not.
         */
        int operator!=(const ListenSocketConfig& lsc) const;

        void validate();

    private:

        /**
         * The server name the outside world uses in URLs that refer to this
         * listen socket.
         */
        ServerName serverName_;

        /**
         * The IP address and port the socket listens for requests on.
         */
        PRNetAddr address_;

        /**
         * String representation of the IP address the socket listens for
         * requests on.
         */
        char ipString_[NET_ADDR_STRING_SIZE];

        /**
         * Set if we're listening on something other than INADDR_ANY.
         */
        PRBool bExplicitIP_;

        /**
         * The address family of the socket.
         */
        PRUint16 family_;

        /**
         * Whether this socket's family supports PR_SockOpt_NoDelay.
         */
        PRBool bNoDelaySupported_;

        /**
         * Set if there are multiple virtual servers associated with this
         * listen socket (i.e. indicates whether we need to check vsHash_ and
         * vsHashPatternList_).
         */
        PRBool multipleVS_;

        /**
         * Hash table that maps host headers to virtual server pointers.
         */
        SimpleStringHash vsHash_;

        /**
         * Linked list that maps host wildcard patterns to virtual server
         * pointers.
         */
        CList<VirtualServerHostPattern> vsHostPatternList_;

        /**
         * Virtual server to use when there is no host header or the host
         * header doesn't map to a virtual server.
         */
        VirtualServer* defaultVS_;

        /**
         * Optional pointer to SSL parameters.
         */
        SSLSocketConfiguration* sslparams_;

        /**
         * Optional mapping of IPs to IP-specific configurations.
         */
        ListenSocketConfigHash *ipConfigHash_;

        /**
         * Frees dynamically allocated memory.  Called by the destructor and in
         * the event of an exception during construction.
         */
        void cleanup(void);

        /**
         * Copy constructor. Objects of this class cannot be copy constructed.
         */
        ListenSocketConfig(const ListenSocketConfig& source);

        /**
         * operator=. Objects of this class cannot be cloned.
         */
        ListenSocketConfig& operator=(const ListenSocketConfig& source);
};

/**
 * Represents a host wildcard pattern to virtual server mapping.
 *
 * @since   WS7.0
 */

class VirtualServerHostPattern
{
    public:
        /**
         * Construct a host wildcard pattern to virtual server mapping.
         */
        VirtualServerHostPattern(const char* exp, VirtualServer* vs);

        /**
         * Destructor.
         */
        ~VirtualServerHostPattern();

        /**
         * Check whether the specified host maps to this virtual server.
         */
       inline VirtualServer *map(const char* host) const;

    private:
        /**
         * Host wildcard pattern.
         */
        char* exp_;

        /**
         * Virtual server corresponding to hosts that match the exp_ host
         * wildcard pattern.
         */
        VirtualServer* vs_;
};

inline
const ServerName&
ListenSocketConfig::getExternalServerName(void) const
{
    return this->serverName_;
}

inline
const char*
ListenSocketConfig::getListenIPAddress() const
{
    return ipString_;
}

inline
PRNetAddr
ListenSocketConfig::getAddress(void) const
{
    return this->address_;
}

inline
PRUint16
ListenSocketConfig::getFamily(void) const
{
    return this->family_;
}

inline
PRBool
ListenSocketConfig::hasExplicitIP(void) const
{
    return this->bExplicitIP_;
}

inline
PRBool
ListenSocketConfig::isNoDelaySupported(void) const
{
    return this->bNoDelaySupported_;
}

inline
VirtualServer *
ListenSocketConfig::findVS(const char* host) const
{
    if (multipleVS_) {
        VirtualServer* vs;

        // XXX Sigh.  This is safe.  SimpleHash has its constness wrong, and
        // it's been templated that way.
        vs = (VirtualServer*)((ListenSocketConfig*)this)->vsHash_.lookup((void*)host);
        if (vs)
            return vs;

        // No exact (fast) match, check for wildcard pattern (slow) matches
        CListConstIterator<VirtualServerHostPattern> iter(&vsHostPatternList_);
        while (const VirtualServerHostPattern* pattern = ++iter) {
            vs = pattern->map(host);
            if (vs)
                return vs;
        }
    }

    return NULL;
}

inline
VirtualServer *
ListenSocketConfig::getDefaultVS(void)
{
    return this->defaultVS_;
}

inline
SSLSocketConfiguration *
ListenSocketConfig::getSSLParams()
{
    return this->sslparams_;
}

inline
const SSLSocketConfiguration *
ListenSocketConfig::getSSLParams() const
{
    return this->sslparams_;
}

inline
int
ListenSocketConfig::getIPSpecificConfigCount(void)
{
    if (ipConfigHash_)
        return ipConfigHash_->getConfigCount();

    return 0;
}

inline
ListenSocketConfig*
ListenSocketConfig::getIPSpecificConfig(int i)
{
    if (ipConfigHash_)
        return ipConfigHash_->getConfig(i);

    return NULL;
}

inline
ListenSocketConfig*
ListenSocketConfig::getIPSpecificConfig(PRNetAddr& localAddress)
{
    if (ipConfigHash_)
    {
        ListenSocketConfig* lsc = ipConfigHash_->getConfig(localAddress);
        if (lsc)
            return lsc;
    }

    return this;
}

inline
VirtualServer*
VirtualServerHostPattern::map(const char* host) const
{
    if (!shexp_casecmp(host, exp_))
        return vs_;

    return NULL;
}

#endif /* _ListenSocketConfig_h_ */
