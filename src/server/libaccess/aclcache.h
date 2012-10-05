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

#ifndef _ACLCACHE_H
#define _ACLCACHE_H

#include <plhash.h>
#include <base/pool.h>
#include <libaccess/acl.h>

enum aclcachetype { ACL_URI_HASH, ACL_URI_GET_HASH };

class NSAPI_PUBLIC ACLCache {
public:
    ACLCache();
    ~ACLCache();

    int CheckH(aclcachetype which, const char *vsid, char *uri, ACLListHandle_t **acllistp);

    int Check(const char *vsid, char *uri, ACLListHandle_t **acllistp) {
        return CheckH(ACL_URI_HASH, vsid, uri, acllistp);
    }
    int CheckGet(const char *vsid, char *uri, ACLListHandle_t **acllistp) {
        return CheckH(ACL_URI_GET_HASH, vsid, uri, acllistp);
    }

    void EnterH(aclcachetype which, const char *vsid, char *uri, ACLListHandle_t **acllistp);

    void Enter(const char *vsid, char *uri, ACLListHandle_t **acllistp) {
        EnterH(ACL_URI_HASH, vsid, uri, acllistp);
    }
    void EnterGet(const char *vsid, char *uri, ACLListHandle_t **acllistp) {
        EnterH(ACL_URI_GET_HASH, vsid, uri, acllistp);
    }

private:

    PRHashTable *hash;
    PRHashTable *gethash;
    PRHashTable *listhash;
    pool_handle_t *pool;
};

#endif // _ACLCACHE_H
