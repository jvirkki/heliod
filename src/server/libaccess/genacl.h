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


#ifndef ACL_GENACL_H
#define ACL_GENACL_H

#include <base/plist.h>
#include <base/vs.h>
#include <libaccess/acl.h>
#include <libaccess/nserror.h>

NSPR_BEGIN_EXTERN_C

//
// generic ACL helper functions
//

NSAPI_PUBLIC extern int
get_is_owner_default (NSErr_t *errp, PList_t subject, PList_t resource, PList_t auth_info,
                        PList_t global_auth, void *unused);

#if 0
NSAPI_PUBLIC extern int
acl_user_exists (const char *user, const char *userdn, const char *dbname, const int logerr);
#endif

NSAPI_PUBLIC extern int
acl_groupcheck (const char *user, const void *cert, const char *group, const char *dbname,
                         const char *method, const int logerr);

NSAPI_PUBLIC int
ACL_Authenticate(const char *user, const char *password, const VirtualServer *vs, const char *dbname, const int logerr);

NSAPI_PUBLIC int
ACL_IsUserInRole(const char *user, int matchWith, const char *grouporrole, const VirtualServer *vs, const char *dbname, const int logerr);

NSPR_END_EXTERN_C

#endif /* ACL_GENACL_H */
