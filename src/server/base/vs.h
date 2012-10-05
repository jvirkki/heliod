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

#ifndef BASE_VS_H
#define BASE_VS_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

#include "netsite.h"
#include "nsapi.h"

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C
NSAPI_PUBLIC int INTvs_register_cb(VSInitFunc *vs_init_func, VSDestroyFunc *vs_destroy_func);
NSAPI_PUBLIC int INTvs_directive_register_cb(FuncPtr func, VSDirectiveInitFunc *vs_init_func, VSDirectiveDestroyFunc *vs_destroy_func);
NSAPI_PUBLIC const char *INTvs_get_id(const VirtualServer *vs);
NSAPI_PUBLIC void *INTvs_get_acllist(const VirtualServer *vs);
NSAPI_PUBLIC void INTvs_set_acllist(VirtualServer *vs, void *acllist);
NSAPI_PUBLIC const char *INTvs_get_servername(const VirtualServer *vs);
NSAPI_PUBLIC int INTvs_alloc_slot(void);
NSAPI_PUBLIC void *INTvs_set_data(const VirtualServer *vs, int *slot, void *data);
NSAPI_PUBLIC void *INTvs_get_data(const VirtualServer *vs, int slot);
NSAPI_PUBLIC httpd_objset *INTvs_get_httpd_objset(VirtualServer *vs);
NSAPI_PUBLIC httpd_object *INTvs_get_default_httpd_object(VirtualServer *vs);
NSAPI_PUBLIC char *INTvs_get_doc_root(const VirtualServer *vs);
NSAPI_PUBLIC char *INTvs_translate_uri(const VirtualServer *vs, const char *uri);
NSAPI_PUBLIC char *INTvs_get_mime_type(const VirtualServer *vs, const char *uri);
NSAPI_PUBLIC const char *INTvs_find_ext_type(const VirtualServer *vs, const char *ext);
NSAPI_PUBLIC int INTvs_is_default_vs(const VirtualServer *vs);
NSAPI_PUBLIC const char *INTvs_lookup_config_var(const VirtualServer *vs, const char *name);
NSAPI_PUBLIC char *INTvs_resolve_config_var(const VirtualServer *vs, const char *name);
NSAPI_PUBLIC char *INTvs_substitute_vars(const VirtualServer *vs, const char *string);
#if defined(__cplusplus) 
class Configuration;
NSAPI_PUBLIC const Configuration *vs_get_conf(const VirtualServer *vs);
#endif
NSPR_END_EXTERN_C

#define vs_register_cb INTvs_register_cb
#define vs_directive_register_cb INTvs_directive_register_cb
#define vs_get_id INTvs_get_id
#define vs_get_acllist INTvs_get_acllist
#define vs_set_acllist INTvs_set_acllist
#define vs_alloc_slot INTvs_alloc_slot
#define vs_set_data INTvs_set_data
#define vs_get_data INTvs_get_data
#define vs_get_httpd_objset INTvs_get_httpd_objset
#define vs_get_default_httpd_object INTvs_get_default_httpd_object
#define vs_get_doc_root INTvs_get_doc_root
#define vs_translate_uri INTvs_translate_uri
#define vs_get_mime_type INTvs_get_mime_type
#define vs_find_ext_type INTvs_find_ext_type
#define vs_is_default_vs INTvs_is_default_vs
#define vs_lookup_config_var INTvs_lookup_config_var
#define vs_resolve_config_var INTvs_resolve_config_var
#define vs_substitute_vars INTvs_substitute_vars

#endif /* INTNSAPI */

#endif /* !BASE_VS_H */
