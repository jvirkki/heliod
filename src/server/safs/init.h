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


#ifndef SAFS_INIT_H
#define SAFS_INIT_H

#include <base/pool.h>
#include <base/pblock.h>
#include <base/session.h>
#include <libaccess/acl.h>
#include <libaccess/nserror.h>
#include <frame/req.h>

#define ACL_ATTR_POSITION	    "position"

#define ACL_AUTHTYPE_BASIC	    "basic"
#define ACL_AUTHTYPE_SSL	    "ssl"
#define ACL_AUTHTYPE_DIGEST	    "digest"
#define ACL_AUTHTYPE_GSSAPI         "gssapi"

NSPR_BEGIN_EXTERN_C

extern int register_attribute_getter(pblock *pb, Session *sn, Request *rq);
extern int register_method(pblock *pb, Session *sn, Request *rq);
extern int register_module(pblock *pb, Session *sn, Request *rq);

extern int register_database_type(pblock *pb, Session *sn, Request *rq);
extern int register_database_name(pblock *pb, Session *sn, Request *rq);

extern int set_default_method (pblock *pb, Session *sn, Request *rq);
extern int set_default_database (pblock *pb, Session *sn, Request *rq);

extern int init_acl_modules(NSErr_t *errp);

extern ACLMethod_t ACL_MethodBasic;
extern ACLMethod_t ACL_MethodDigest;
extern ACLMethod_t ACL_MethodSSL;
extern ACLMethod_t ACL_MethodGSSAPI;

NSPR_END_EXTERN_C


#endif /* SAFS_INIT_H */

