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

#include <netsite.h>
#include <nspr.h>

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

NSPR_BEGIN_EXTERN_C

int 
INTobjset_get_number_objects(const httpd_objset* objset);
const httpd_object*
INTobjset_get_object(const httpd_objset* objset, int pos);
const pblock* const* 
INTobjset_get_initfns(const httpd_objset* objset);
const pblock* 
INTobject_get_name(const httpd_object* object);
int     
INTobject_get_num_directives(const httpd_object* object);
const dtable*
INTobject_get_directive_table(const httpd_object* object, int pos);
int     
INTdirective_table_get_num_directives(const dtable* dir_table);
const directive* 
INTdirective_table_get_directive(const dtable* dir_table, int pos);
const pblock* 
INTdirective_get_pblock(const directive* dir);
const FuncStruct* 
INTdirective_get_funcstruct(const directive* dir);
const pblock*
INTdirective_get_client_pblock(const directive* dir);

NSPR_END_EXTERN_C

#ifdef INTNSAPI

#define objset_get_number_objects INTobjset_get_number_objects
#define objset_get_object INTobjset_get_object
#define objset_get_initfns INTobjset_get_initfns
#define object_get_name INTobject_get_name
#define object_get_num_directives INTobject_get_num_directives
#define object_get_directive_table INTobject_get_directive_table
#define directive_table_get_num_directives INTdirective_table_get_num_directives
#define directive_table_get_directive INTdirective_table_get_directive
#define directive_get_pblock INTdirective_get_pblock
#define directive_get_funcstruct INTdirective_get_funcstruct
#define directive_get_client_pblock INTdirective_get_client_pblock

#endif /* INTNSAPI */
