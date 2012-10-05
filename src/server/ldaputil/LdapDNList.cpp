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

#include <stdlib.h>
#include <string.h>
#include <ldaputil/LdapDNList.h>
#include <ldaputil/errors.h>

LdapDNList::LdapDNList(void)
{
    memset(&list_, 0, sizeof(LDAPUList_t));
}

LdapDNList::~LdapDNList(void)
{
    clear();
}

static void*
DNList_free1 (void* info, void* arg)
{
    free (info);
    return 0;
}

void
LdapDNList::clear(void)
{
    ldapu_list_empty(&list_, DNList_free1, 0);
}

void
LdapDNList::append(LdapDNList &from)
{
    ldapu_list_move(&(from.list_), &list_);
}

int
LdapDNList::add(const char* dn)
{
    char* dnCopy = ldapu_strdup(dn);
    if (!dnCopy)
        return LDAPU_ERR_OUT_OF_MEMORY;
    return ldapu_list_add_info (&list_, dnCopy);
}

int
LdapDNList::is_empty(void)
{
    return (this == 0 || !(list_.head));
}

void *
LdapDNList::first(void)
{
    return this ? list_.head : NULL;
}

void *
LdapDNList::next(void *iter)
{
    return iter ? ((LDAPUListNode_t*)iter)->next : NULL;
}

const char *
LdapDNList::item(void *iter)
{
    return iter ? (const char*)(((LDAPUListNode_t*)iter)->info) : NULL;
}

