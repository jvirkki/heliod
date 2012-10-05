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

#include "HashUtil.h"
#include "WebModule.h"
#include "netsite.h"

// WStringHash functions
WStringHash::WStringHash(unsigned long size) : SimpleStringHash(size)
{
}

WStringHash::~WStringHash()
{
}

void WStringHash::datadelete(void* obj)
{
    char* ptr = (char*)obj;
    PERM_FREE(ptr);
}

PRBool WStringHash::insert(const char* key, const char* data)
{
    return SimpleStringHash::insert((void*)key, (void*)data);
}

const char* WStringHash::lookup(const char* key)
{
    return (const char*)SimpleStringHash::lookup((void*)key);
}

// WIntHash functions
WIntHash::WIntHash(unsigned long size) : SimpleIntHash(size)
{
}

WIntHash::~WIntHash()
{
}

void WIntHash::datadelete(void* obj)
{
    char* ptr = (char*)obj;
    PERM_FREE(ptr);
}

PRBool WIntHash::insert(int key, const char* data)
{
    return SimpleIntHash::insert((void*)key, (void*)data);
}

const char* WIntHash::lookup(int key)
{
    return (const char*)SimpleIntHash::lookup((void*)key);
}

// WebModuleHash functions
WebModuleHash::WebModuleHash(unsigned long size) : SimpleStringHash(size)
{
}

WebModuleHash::~WebModuleHash()
{
}

void WebModuleHash::datadelete(void* obj)
{
    WebModule* wm = (WebModule*)obj;
    delete wm;
}

// WResourceHash functions
WResourceHash::WResourceHash(unsigned long size) : SimpleStringHash(size)
{
}

WResourceHash::~WResourceHash()
{
}

PRBool WResourceHash::insert(const char* key, ServletResource* data)
{
    return SimpleStringHash::insert((void*)key, (void*)data);
}

ServletResource* WResourceHash::lookup(const char* key)
{
    return (ServletResource*)SimpleStringHash::lookup((void*)key);
}

