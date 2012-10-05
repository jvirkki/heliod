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

#ifndef FCGISERVERCHANNEL_H
#define FCGISERVERCHANNEL_H

#include <nspr.h>
#include "support/objectlist.h"
#include "serverconfig.h"
#include "fcgirequest.h"
#include "endpoint.h"


//-----------------------------------------------------------------------------
// FcgiServerChannel
//-----------------------------------------------------------------------------

class FcgiServerChannel;
typedef ObjectList<FcgiServerChannel> FcgiServerChannelList;
typedef ObjectIterator<FcgiServerChannel> FcgiServerChannelIterator;

class FcgiServerChannel : public EndPoint, private ObjectLink {
public:
    FcgiServerChannel(const FcgiServerConfig *conf);
    ~FcgiServerChannel();

    PRStatus connect(PRIntervalTime timeoutVal = PR_INTERVAL_NO_TIMEOUT);
    PRStatus sendRequest(const FcgiRequest &request);
    int hasResponseData() { return from.hasData(); }
    CircularBuffer* getRequestBuffer() { return &to; }
    CircularBuffer* getResponseBuffer() { return &from; }
    void setNonPersistent() { flagPersistent = PR_FALSE; }
    void setPersistent(); 
    void setServerError() { flagServerError = PR_TRUE; flagReadable = PR_FALSE; flagWritable = PR_FALSE; }
    void setClientError() { flagClientError = PR_TRUE; }
    void reset();

    const FcgiServerConfig *config;

private:
    int countTransactions;
    CircularBuffer to;
    CircularBuffer from;
    PRInt64 cl;
    PRBool flagHead;
    PRBool flagPersistent;
    PRBool flagServerError;
    PRBool flagClientError;
    PRIntervalTime ticksLastActive;

    friend class ObjectList<FcgiServerChannel>;
    friend class ObjectIterator<FcgiServerChannel>;
    friend class FcgiServer;
};

#endif // FCGISERVERCHANNEL_H
