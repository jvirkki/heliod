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

#ifndef FRAME_CONF_H
#define FRAME_CONF_H

/*
 * Copyright (c) 1994, 1995.  Netscape Communications Corporation.  All
 * rights reserved.
 * 
 * Use of this software is governed by the terms of the license agreement for
 * the Netscape FastTrack or Netscape Enterprise Server between the
 * parties.
 */

/*
 * conf.h: Legacy NSAPI globals, NSAPI Init, and magnus.conf processing
 * 
 * Rob McCool
 */

#ifndef NETSITE_H
#include "netsite.h"
#endif /* !NETSITE_H */

/* ---------------------- NSAPI Function Prototypes ----------------------- */

PR_BEGIN_EXTERN_C
NSAPI_PUBLIC const char *INTconf_getServerString(void);
NSAPI_PUBLIC conf_global_vars_s *INTconf_getglobals(void);
NSAPI_PUBLIC const char *INTconf_getfilename(void);
NSAPI_PUBLIC const char *INTconf_getstring(const char *name, const char *def);
NSAPI_PUBLIC int INTconf_getboolean(const char *name, int def);
NSAPI_PUBLIC int INTconf_getinteger(const char *name, int def);
NSAPI_PUBLIC int INTconf_getboundedinteger(const char *name, int min, int max, int def);
PR_END_EXTERN_C

#define conf_getServerString INTconf_getServerString
#define conf_getglobals INTconf_getglobals
#define conf_getfilename INTconf_getfilename
#define conf_getstring INTconf_getstring
#define conf_getboolean INTconf_getboolean
#define conf_getinteger INTconf_getinteger
#define conf_getboundedinteger INTconf_getboundedinteger

/* ---------------------------- Internal Stuff ---------------------------- */

#if defined(__cplusplus) 

#ifdef GENERATED_SERVERXMLSCHEMA_SERVER
NSAPI_PUBLIC void conf_init_true_globals(const ServerXMLSchema::Server& server);
#endif

NSAPI_PUBLIC conf_global_vars_s *conf_get_true_globals();
NSAPI_PUBLIC void conf_set_thread_globals(class HttpRequest *hrq);
NSAPI_PUBLIC void conf_set_thread_globals(class HttpRequest *hrq, const VirtualServer *vs);
NSAPI_PUBLIC void conf_set_thread_vs_globals(const VirtualServer *vs);
NSAPI_PUBLIC void conf_reset_thread_globals();
NSAPI_PUBLIC void conf_get_thread_globals(HttpRequest *&hrq, const VirtualServer*& vs);
NSAPI_PUBLIC const VirtualServer *conf_get_vs();
NSAPI_PUBLIC PRStatus conf_parse(const char *cfn, PRBool jvm_enabled);
NSAPI_PUBLIC void conf_add_init(pblock *initfn, conf_global_vars_s *cg);
NSAPI_PUBLIC PRStatus conf_run_early_init_functions();
NSAPI_PUBLIC PRStatus conf_run_late_init_functions(class Configuration *configuration);
NSAPI_PUBLIC int conf_get_id(const class Configuration *configuration);

PR_BEGIN_EXTERN_C
NSAPI_PUBLIC int conf_is_late_init(pblock *pb);
PR_END_EXTERN_C

/*
 * conf_str2pblock behaves like pblock_str2pblock but strips out reserved
 * parameter names.  The currently reserved names are "magnus-internal"
 * (reserved for runtime binary data), "refcount" (reserved for pblock
 * reference counts), and "Directive" (reserved for NSAPI directive names).
 */
NSAPI_PUBLIC int conf_str2pblock(const char *str, pblock *pb);

#endif /* __cplusplus */

#endif /* !FRAME_CONF_H */
