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

#ifndef FCGISERVER_H
#define FCGISERVER_H

#include <string.h>
#include <nspr.h>
#include "support/objectlist.h"
#include "serverchannel.h"

//-----------------------------------------------------------------------------
// FcgiServer
//-----------------------------------------------------------------------------
class FcgiServer;
typedef ObjectList<FcgiServer> FcgiServerList;
typedef ObjectIterator<FcgiServer> FcgiServerIterator;

class FcgiServer : private ObjectLink {
public:
    ~FcgiServer();
    // Find/create a FcgiServer with configuration specified by FcgiServerConfig
    static FcgiServer* createFcgiServer(const FcgiServerConfig *config);

    // Access this FcgiServer's channels
    FcgiServerChannel *getChannel(PRIntervalTime timeoutConnect, 
                         PRBool flagReusePersistent = PR_TRUE);
    void addChannel(FcgiServerChannel *channel);

    // Statistics for this FcgiServer
    PRInt32 getCountActive() { return countActive; }
    PRInt32 getCountIdle() { return channels.getCount(); }
    PRBool isDown() { return flagDown; }
    void setActive(PRBool status);
    PRBool isActive() { return activeFlag; }

    // Configuration of this FcgiServer
    const FcgiServerConfig *config;

private:
    FcgiServer(const FcgiServerConfig *conf);

    // List of FcgiServerChannels associated with this FcgiServer
    PRLock *lockChannels;
    FcgiServerChannelList channels;
    PRInt32 countActive;

    // Status of this FcgiServer
    PRBool flagDown;
    PRBool flagConnectFailed;

    // Number of references to this FcgiServer
    int ref;

    PRLock *lockActiveFlag;
    PRBool activeFlag;

    friend class ObjectList<FcgiServer>;
    friend class ObjectIterator<FcgiServer>;
};

#endif // FCGISERVER_H
