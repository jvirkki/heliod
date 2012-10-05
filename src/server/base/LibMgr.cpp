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
// Library manager class
//
// Copyright (c) 1996 Netscape Communications Corporation
// Robin J. Maxwell 08-08-96

#include <assert.h>
#include "base/LibMgr.hpp"
#include "base/ereport.h"
#include "base/net.h"

#include "base/dbtbase.h"

unsigned
SymbolEntryBase::hashFun(const SymbolEntryBase& rhs)
{
    return rhs.hash();
}

SymbolEntryBase::SymbolEntryBase(const RWCString& name, AnyFunction func):
RWCString(name),
_func(func)
{
}

SymbolEntryBase::SymbolEntryBase(const char *name, AnyFunction func):
_func(func)
{
    assert(name);

    *this = name;
}

unsigned
LibraryEntry::hashFun(const LibraryEntry& rhs)
{
    return rhs._name.hash();
}

LibraryEntry::LibraryEntry(LibraryManager& lm, const RWCString& name):
_name(name),
_refcnt(0),
_handle(0),
_symbols(SymbolEntryBase::hashFun),
_libmgr(lm)
{
    if ((_handle = dll_open(name)) == 0)
        ereport(LOG_WARN, XP_GetAdminStr(DBT_dlopenOfSFailedS_), name, dll_error(0));
}

LibraryEntry::~LibraryEntry()
{
    _symbols.clearAndDestroy();
    if (_handle)
        dll_close(_handle);
}

BOOLEAN
LibraryEntry::Open()
{
    if ((_handle = dll_open(_name.data())) == 0){
        ereport(LOG_WARN, XP_GetAdminStr(DBT_dlopenOfSFailedS_1), _name.data(), dll_error(0));
        return FALSE;
    }
    AddRef();
    return TRUE;
}

BOOLEAN
LibraryEntry::Close()
{
    if (RemoveRef() == 0){
        dll_close(_handle);
        return TRUE;
    }
    return FALSE;
}

unsigned
LibraryEntry::RemoveRef()
{
    if (_refcnt)
        _refcnt--;
    return _refcnt;
}

SymbolEntryBase *
LibraryEntry::LoadSymbol(const RWCString& name)
{
    SymbolEntryBase *thisSym = _libmgr.CreateSymbolEntry(name);
    SymbolEntryBase *oldSym =0;
    AnyFunction thisFunc = 0;

    assert(thisSym);

    if (thisSym){
        if ((oldSym = _symbols.find(thisSym))){  // See if it already exists
            delete thisSym;
            thisSym = oldSym;
        }else{
            thisFunc = (AnyFunction)dll_findsym(_handle, name.data());
            if (thisFunc){
                thisSym->SetFunction(thisFunc);// Save func pointer
                _symbols.insert(thisSym);  // Jam it into our set
            }else{
                delete thisSym;  // Load failed.
                thisSym = 0;
            }
        }
    }
    return thisSym;
}

SymbolEntryBase *
LibraryEntry::LoadSymbol(const char *name, AnyFunction func)
{
    assert(name);
    assert(func);

    SymbolEntryBase *thisSym = _libmgr.CreateSymbolEntry(RWCString(name), func);
    SymbolEntryBase *oldSym =0;

    assert(thisSym);

    if (thisSym){
        if ((oldSym = _symbols.find(thisSym))){  // See if it already exists
            delete thisSym;
            thisSym = oldSym;
        }else{
            _symbols.insert(thisSym);  // Jam it into our set
        }
    }
    return thisSym;
}

LibraryManager::LibraryManager():
_libs(LibraryEntry::hashFun)
{
}

LibraryManager::~LibraryManager()
{
    _libs.clearAndDestroy();
}

BOOLEAN
LibraryManager::OpenLibrary(const char *libname)
{
    SafeLock lock(_crtsec);
    
    assert(libname);

    return _Open(RWCString(libname)) ? TRUE:FALSE;
}

LibraryEntry *
LibraryManager::_Open(const RWCString& libname)
{
    LibraryEntry *thisLib = new LibraryEntry(*this, libname);
    LibraryEntry *oldLib;
    if ((oldLib = _libs.find(thisLib))){  // See if it already exists
        delete thisLib;
        oldLib->AddRef();
        thisLib = oldLib;
    }else{
        if (thisLib->Open())
            _libs.insert(thisLib);
        else{
            delete thisLib;
            return 0;  // Error opening lib.
        }
    }
    return thisLib;
}


BOOLEAN
LibraryManager::CloseLibrary(const char *libname)
{
    SafeLock lock(_crtsec);

    LibraryEntry *thisLib = new LibraryEntry(*this, libname);
    LibraryEntry *oldLib;
    if ((oldLib = _libs.find(thisLib))){  // See if it already exists
        if (oldLib->Close()){
            delete oldLib;
        }
    }
    return FALSE;  // No such library
}

SymbolEntryBase * 
LibraryManager::LoadSymbol(const char *libname, const char *symname)
{
    SafeLock lock(_crtsec);
    SymbolEntryBase *thisSym = 0;

    assert(libname);
    assert(symname);
    
    LibraryEntry *thisLib = 0;
    if ((thisLib = _Open(RWCString(libname)))){
        thisSym = thisLib->LoadSymbol(RWCString(symname));
    }
    return thisSym;
}

SymbolEntryBase *
LibraryManager::CreateSymbolEntry(const RWCString& name, AnyFunction)
{
    return new SymbolEntryBase(name);
}
