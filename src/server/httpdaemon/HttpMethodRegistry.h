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

#ifndef _HTTP_METHOD_REGISTRY_H_
#define _HTTP_METHOD_REGISTRY_H_

#include "base/systems.h"  // added rcrit
#include "prtypes.h"
#include "support/NSString.h"
#include "base/keyword.h"

class NSAPI_PUBLIC HttpMethodRegistry {
    public :
        static const HttpMethodRegistry* Init();
        static HttpMethodRegistry& GetRegistry();
        PRBool  IsValid() const;
        PRInt32 GetNumMethods() const;
        const NSKWSpace* GetMethodsTable() const;
        PRBool IsKnownHttpMethod(const char* method) const;
        PRInt32 RegisterMethod(const char* method);
        const char* GetAllMethods();
        const char* GetMethod(PRInt32 index);
        PRInt32 GetMethodIndex(const char* method) const;

    private :
        HttpMethodRegistry();
        ~HttpMethodRegistry();
        NSKWSpace*        _httpMethodsTable;
        PRInt32           _numRegisteredMethod;
        NSString          _allMethodsStr;
        PRBool            _valid;
        static HttpMethodRegistry* _gHttpRegistryPtr;
};

#endif /* _HTTP_METHOD_REGISTRY_H_ */
