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

#ifndef LIBXSD2CPP_CSTRING_H
#define LIBXSD2CPP_CSTRING_H

#include "xercesc/util/XercesDefs.hpp"
#include "xalanc/XalanDOM/XalanDOMString.hpp"
#include "libxsd2cpp.h"

namespace LIBXSD2CPP_NAMESPACE {

using XALAN_CPP_NAMESPACE::XalanDOMString;

//-----------------------------------------------------------------------------
// CString
//-----------------------------------------------------------------------------

/**
 * This utility class can be used to transcode a Xerces-C++ or Xalan-C++
 * Unicode string to a nul-terminated C string in the local codepage.
 */
class LIBXSD2CPP_EXPORT CString {
public:
    /**
     * Construct a CString from a nul-terminated Xerces-C++ XMLCh *.
     */
    CString(const XMLCh *string);

    /**
     * Construct a CString from a Xerces-C++ XMLCh *.
     */
    CString(const XMLCh *string, int len);

    /**
     * Construct a CString from a Xalan-C++ XalanDOMString.
     */
    CString(const XalanDOMString& string);

    CString(const CString& string);

    ~CString();

    CString& operator=(const CString& right);

    /**
     * Return the length of the C string.
     */
    int strlen() const;

    /**
     * Return the size of the C string, including the trailing nul.
     */
    int size() const;

    /**
     * Obtain a pointer to the underlying nul-terminated C string.
     */
    inline operator const char *() const
    {
        return cstring;
    }

    /**
     * Obtain a pointer to the underlying nul-terminated C string.
     */
    inline const char * getStringValue() const
    {
        return cstring;
    }

private:
    /**
     * Initialize a CString from a nul-terminated C string.
     */
    void init(const char *string);

    /**
     * The underlying nul-terminated C string.
     */
    char *cstring;
};

} // namespace LIBXSD2CPP_NAMESPACE

#endif // LIBXSD2CPP_CSTRING_H
