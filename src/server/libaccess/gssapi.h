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



#ifndef __GSSAPI_H__
#define __GSSAPI_H__

#ifdef FEAT_GSS

#include <netsite.h>
#include <libaccess/aclproto.h>

NSPR_BEGIN_EXTERN_C

#define KERBEROS_AUTHDB_URL "kerberos"

#define KERBEROS_PARAM_SERVICENAME "servicename"
#define KERBEROS_PARAM_SERVICENAME_DEFAULT "HTTP"

#define KERBEROS_PARAM_HOSTNAME "hostname"

typedef struct kerberos_params_t 
{
    const char * servicename;
    const char * hostname;
} kerberos_params;


int get_auth_user_gssapi(NSErr_t *errp, PList_t subject, PList_t resource,
                         PList_t auth_info, PList_t global_auth, void *unused);

int gssapi_authenticate_user(NSErr_t *errp, PList_t subject, PList_t resource,
                             PList_t auth_info, PList_t global_auth,void *arg);


int gssapi_get_authorization_info(NSErr_t *errp, PList_t subject, 
                                  PList_t resource, PList_t auth_info,
                                  PList_t global_auth, void *unused);

void gssapi_init();

int kerberos_parse_url(NSErr_t *errp, ACLDbType_t dbtype, const char *name,
                       const char *url, PList_t plist, void **db);

NSPR_END_EXTERN_C

#endif

#endif

