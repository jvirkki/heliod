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

#ifndef ACL_HEADER
#define ACL_HEADER

#ifndef NOINTNSACL
#define INTNSACL
#endif /* NOINTNSACL */

#include <netsite.h>
#include <base/pool.h>
#include <base/pblock.h>
#include <base/plist.h>
#include <libaccess/nserror.h>

#ifndef FALSE
#define	FALSE			0
#endif
#ifndef TRUE
#define	TRUE			1
#endif

/* include the public stuff */

#ifndef PUBLIC_NSACL_ACLAPI_H
#include "public/nsacl/aclapi.h"
#endif /* !PUBLIC_NSACL_ACLAPI_H */

#ifdef INTNSACL

NSPR_BEGIN_EXTERN_C

#define ACL_GENERIC_RIGHT_READ "read"
#define ACL_GENERIC_RIGHT_READ_LEN (sizeof(ACL_GENERIC_RIGHT_READ) - 1)
#define ACL_GENERIC_RIGHT_WRITE "write"
#define ACL_GENERIC_RIGHT_WRITE_LEN (sizeof(ACL_GENERIC_RIGHT_WRITE) - 1)
#define ACL_GENERIC_RIGHT_EXECUTE "execute"
#define ACL_GENERIC_RIGHT_EXECUTE_LEN (sizeof(ACL_GENERIC_RIGHT_EXECUTE) - 1)
#define ACL_GENERIC_RIGHT_DELETE "delete"
#define ACL_GENERIC_RIGHT_DELETE_LEN (sizeof(ACL_GENERIC_RIGHT_DELETE) - 1)
#define ACL_GENERIC_RIGHT_INFO "info"
#define ACL_GENERIC_RIGHT_INFO_LEN (sizeof(ACL_GENERIC_RIGHT_INFO) - 1)
#define ACL_GENERIC_RIGHT_LIST "list"
#define ACL_GENERIC_RIGHT_LIST_LEN (sizeof(ACL_GENERIC_RIGHT_LIST) - 1)

extern	char	*generic_rights[];
extern	char	*http_generic[];

/* Define symbol type codes */
#define ACLSYMACL       0               /* ACL */

NSPR_END_EXTERN_C

#endif /* INTNSACL */

#endif
