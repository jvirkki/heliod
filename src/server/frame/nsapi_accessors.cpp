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

#include "frame/nsapi_accessors.h"
#include "frame/objset.h"
#include "frame/object.h"


int
INTobjset_get_number_objects(const httpd_objset* objset)
{
  PR_ASSERT(objset);
  
  return objset->pos;
}

const httpd_object*
INTobjset_get_object(const httpd_objset* objset, int pos)
{
  PR_ASSERT(objset);
  PR_ASSERT(pos >= 0);
  PR_ASSERT(pos < objset->pos);

  const httpd_object* res = NULL;
  if (objset && (pos >= 0) && (pos < objset->pos)) {
    res = objset->obj[pos];
  }

  return res;
}


const pblock* const*
INTobjset_get_initfns(const httpd_objset* objset)
{
  PR_ASSERT(objset);
 
  const pblock* const* res = NULL;
  if (objset) {
    res = objset->initfns;
  }

  return res;
}


const pblock* 
INTobject_get_name(const httpd_object* object)
{
  PR_ASSERT(object);

  const pblock* res = NULL;
  if (object) {
    res = object->name;
  }

  return res;
}

int     
INTobject_get_num_directives(const httpd_object* object)
{
  PR_ASSERT(object);

  int res = 0;
  if (object) {
    res = object->nd;
  }

  return res;
}

const dtable*
INTobject_get_directive_table(const httpd_object* object, int pos)
{
  PR_ASSERT(object);
  PR_ASSERT(pos >= 0);
  PR_ASSERT(pos < object->nd);

  const dtable* res = NULL;
  if (object && (pos >= 0) && (pos < object->nd)) {
    res = &object->dt[pos];
  }

  return res;
}


int     
INTdirective_table_get_num_directives(const dtable* dir_table)
{
  PR_ASSERT(dir_table);

  int res = 0;
  if (dir_table) {
    res = dir_table->ni;
  }

  return res;
}



const directive* 
INTdirective_table_get_directive(const dtable* dir_table, int pos)
{
  PR_ASSERT(dir_table);
  PR_ASSERT(pos >= 0);
  PR_ASSERT(pos < dir_table->ni);

  const directive* res = NULL;
  if (dir_table && (pos >= 0) && (pos < dir_table->ni)) {
    res = &dir_table->inst[pos];
  }

  return res;
}




const pblock* 
INTdirective_get_pblock(const directive* dir)
{
  PR_ASSERT(dir);

  const pblock* res = NULL;
  if (dir) {
    res = dir->param.pb;
  }

  return res;
}


const FuncStruct* 
INTdirective_get_funcstruct(const directive* dir)
{
  PR_ASSERT(dir);

  const FuncStruct* res = NULL;
  if (dir) {
    res = dir->f;
  }

  return res;
}


const pblock*
INTdirective_get_client_pblock(const directive* dir)
{
  PR_ASSERT(dir);

  const pblock* res = NULL;
  if (dir) {
    res = dir->client.pb;
  }

  return res;
}

