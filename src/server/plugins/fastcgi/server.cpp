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
#include <nspr.h>
#include "frame/log.h"
#include "base/util.h"
#include "server.h"

extern char LOG_FUNCTION_NAME[];

//-----------------------------------------------------------------------------
// FcgiServer::FcgiServer
//-----------------------------------------------------------------------------
FcgiServer::FcgiServer(const FcgiServerConfig *conf)
: config(conf), 
  lockChannels(PR_NewLock()), 
  lockActiveFlag(PR_NewLock()), 
  countActive(0), 
  flagDown(PR_FALSE), 
  flagConnectFailed(PR_FALSE), 
  ref(0) 
{
  if (conf->remoteServer) 
      activeFlag = PR_TRUE;
  else 
      activeFlag = PR_FALSE;
}

//-----------------------------------------------------------------------------
// FcgiServer::~FcgiServer
//-----------------------------------------------------------------------------
FcgiServer::~FcgiServer() {
    PR_ASSERT(ref == 0);

    while (FcgiServerChannel *channel = channels.removeFromHead())
        delete channel;

    PR_DestroyLock(lockChannels);
    PR_DestroyLock(lockActiveFlag);
    delete config;
}

//-----------------------------------------------------------------------------
// FcgiServer::getChannel
//-----------------------------------------------------------------------------
FcgiServerChannel* FcgiServer::getChannel(PRIntervalTime timeout, PRBool flagReusePersistent) {
    // if server is not active, do not try to connect
    // as socket may have been created but the server may not be up and running
    if (!isActive()) 
        return NULL;

    // We need the current time if we're tracking keep-alive timeouts
    PRIntervalTime ticksNow;
    if (config->keep_alive_timeout)
        ticksNow = PR_IntervalNow();

    // Get an existing FcgiServerChannel connection
    PR_Lock(lockChannels);
    FcgiServerChannel *channel = channels.removeFromHead();
    countActive++;
    PR_Unlock(lockChannels);

    // If we found an existing connection...
    if (channel) {
        // Check to see if the connection is too old
        if (flagReusePersistent && config->keep_alive_timeout) {
            PRIntervalTime ticksIdle = ticksNow - channel->ticksLastActive;
            if (ticksIdle >= config->keep_alive_timeout)
                flagReusePersistent = PR_FALSE;
        }
        // Check to see if the connection is too old
        // If we want to reuse this connection...
        if (flagReusePersistent) {
            // Reuse this persistent FcgiServerChannel connection
            channel->reset();
            config->keepAliveConnection ? channel->setPersistent() : channel->setNonPersistent();
            if (config->keep_alive_timeout)
                channel->ticksLastActive = ticksNow;
            log_error(LOG_VERBOSE, LOG_FUNCTION_NAME, NULL, NULL, "reusing existing channel");
            return channel; // Success
        }

        // Throw away the connection as the caller doesn't want a persistent
        // connection.  This keeps unused persistent connections (i.e. file
        // descriptors) from piling up under error conditions.
        delete channel;
    }

    // Create a new FcgiServerChannel connection
    channel = new FcgiServerChannel(config);

    if (channel->connect(timeout) == PR_SUCCESS) {
        config->keepAliveConnection ? channel->setPersistent() : channel->setNonPersistent();
        flagConnectFailed = PR_FALSE;
        if (config->keep_alive_timeout)
            channel->ticksLastActive = ticksNow;
        return channel; // Success
    }

    // An unconnected FcgiServerChannel is useless
    delete channel;

    // Connect failed, mark FcgiServer down
    flagDown = PR_TRUE;
    flagConnectFailed = PR_TRUE;

    PR_Lock(lockChannels);
    countActive--;
    PR_Unlock(lockChannels);

    return NULL;
}

//-----------------------------------------------------------------------------
// FcgiServer::addChannel
//-----------------------------------------------------------------------------
void FcgiServer::addChannel(FcgiServerChannel *channel) {
    if (!channel->flagServerError)
        flagDown = PR_FALSE;

    // Mark FcgiServer down if this FcgiServerChannel encountered a
    // server error on its first transaction.
    // (We check countTransactions because an error reusing
    // a persistent connection doesn't necessarily indicate a down FcgiServer.)
    if (channel->flagServerError && !channel->countTransactions)
        flagDown = PR_TRUE;

    // Can we reuse this connection?
    PRBool flagReuse = !channel->flagServerError && !channel->flagClientError && config->keepAliveConnection;

    PR_Lock(lockChannels);
    if (flagReuse) {
        channel->countTransactions++;
        channels.addToHead(channel); // LIFO so we reuse hot connections
    }
    countActive--;
    PR_Unlock(lockChannels);

    if (!flagReuse)
        delete channel;
}

//-----------------------------------------------------------------------------
// FcgiServer::createFcgiServer
//-----------------------------------------------------------------------------
FcgiServer *FcgiServer::createFcgiServer(const FcgiServerConfig *config) {
    // Create a new FcgiServer
    return (new FcgiServer(config));
}

void FcgiServer::setActive(PRBool status) {
   PR_Lock(lockActiveFlag); 
   if (activeFlag != status) 
       activeFlag = status;  
   PR_Unlock(lockActiveFlag);
}

