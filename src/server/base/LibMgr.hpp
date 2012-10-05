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

//
// LibraryManager.hpp
//
// Manages loadable libraries
//
// Copyright (c) 1996 Netscape Communications Corporation
// Robin J. Maxwell 08-08-96
//

#ifndef _LIBMGR_HPP
#define _LIBMGR_HPP

#include <sys/types.h>
#include <rw/cstring.h>
#include <rw/tphset.h>
#include "dll.h"
#include "CritSec.hpp"

typedef void (*AnyFunction)(void);

class SymbolEntryBase: public RWCString
{

public:
    SymbolEntryBase(const RWCString& name, AnyFunction func=0);
    SymbolEntryBase(const char *name, AnyFunction func=0);

    ~SymbolEntryBase(){}
    AnyFunction virtual GetFunction()const{return _func;}
    AnyFunction virtual SetFunction(AnyFunction func){return _func=func;}
    time_t GetAccessTime(){return _lastAccessTime;}
    void SetAccessTime();
    static unsigned hashFun(const SymbolEntryBase& rhs);

private:
    AnyFunction _func;
    time_t _lastAccessTime;
};

class LibraryEntry
{
    friend class LibraryManager;

private:
    LibraryEntry(LibraryManager& lm, const RWCString& name);
    ~LibraryEntry();
    BOOLEAN Open();
    BOOLEAN Close();
    SymbolEntryBase *LoadSymbol(const RWCString& name);
    // Adds an entry for symbol name 
    SymbolEntryBase *LoadSymbol(const char *name, AnyFunction func);
    SymbolEntryBase *LoadSymbol(SymbolEntryBase& symentry);
    unsigned AddRef(){return _refcnt++;}
    unsigned RemoveRef();
    static unsigned hashFun(const LibraryEntry& rhs);

private:
    LibraryManager& _libmgr;
    unsigned _refcnt;
    DLHANDLE _handle;
    RWCString _name;
    RWTPtrHashSet<SymbolEntryBase> _symbols;
};

class LibraryManager 
{
public:
    LibraryManager();
    ~LibraryManager();
    BOOLEAN OpenLibrary(const char *libname);
    BOOLEAN CloseLibrary(const char *libname);
    // Loads symname from library libname
    SymbolEntryBase *LoadSymbol(const char *libname, const char *symname);

    // Allocates symbol entries for the LibraryManager
    // Override to allocate specialized symbol entry classes
    virtual SymbolEntryBase *CreateSymbolEntry(const RWCString& name, AnyFunction func=0);

private:
    LibraryEntry *_Open(const RWCString& name);
private:
    RWTPtrHashSet<LibraryEntry> _libs;
    CriticalSection _crtsec;
};

#endif // _LIBMGR_HPP
