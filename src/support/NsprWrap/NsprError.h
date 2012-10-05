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

#ifndef NSPRERROR_H
#define NSPRERROR_H

#include <errno.h>
#include "nspr.h"

#ifdef XP_PC
#ifdef BUILD_NSPRWRAP_DLL
#define NSPRERROR_DLL_API _declspec(dllexport)
#else
#define NSPRERROR_DLL_API _declspec(dllimport)
#endif
#else
#define NSPRERROR_DLL_API
#endif

//-----------------------------------------------------------------------------
// NsprError
//-----------------------------------------------------------------------------

/**
 * Wrappers around NSPR error handling functions.
 */
class NSPRERROR_DLL_API NsprError {
public:
    /**
     * Create an NsprError object.  NsprError::save() can be called to store a
     * snapshot of the current thread's error state in the NsprError object,
     * and NsprError::restore() can later be used to restore that state.
     */
    NsprError();

    /**
     * Create a copy of the specified NsprError object.
     */
    NsprError(const NsprError& right);

    /**
     * Destroy the NsprError object.
     */
    ~NsprError();

    /**
     * Make the NsprError object a copy of the specified NsprError object.
     */
    NsprError& operator=(const NsprError& right);

    /**
     * Save the current thread's error state in the NsprError object.
     */
    void save();

    /**
     * Restore error state from the NsprError object.
     */
    void restore() const;

    /**
     * Set the NSPR error code along with a human-readable description.
     */
    static void setError(PRErrorCode prError, const char *text);

    /**
     * Set the NSPR and OS error codes along with a human-readable description.
     */
    static void setError(PRErrorCode prError, PRInt32 err, const char *text);

    /**
     * Set the NSPR error code along with a formatted human-readable
     * description.
     */
    static void setErrorf(PRErrorCode prError, const char *fmt, ...);

    /**
     * Set the NSPR and OS error codes along with a formatted human-readable
     * description.
     */
    static void setErrorf(PRErrorCode prError, PRInt32 err, const char *fmt, ...);

    /**
     * Map the errno value as a result of a failed Unix system or library call
     * to an NSPR error.
     */
    static void mapUnixErrno();

    /**
     * Map the errno or WSAGetLastError() value set as a result of a failed
     * sockets call to an NSPR error.
     */
    static void mapSocketError();

#ifdef XP_WIN32

    /**
     * Map the GetLastError() value set as a result of a failed Win32 system
     * call to an NSPR error.
     */
    static void mapWin32Error();

    /**
     * Map the WSAGetLastError() value set as a result of a failed Winsock2
     * call to an NSPR error.
     */
    static void mapWinsock2Error();

#endif /* XP_WIN32 */

private:
    PRErrorCode _prError;
    PRInt32 _err;
    char *_text;
    int _size;
    int _len;
};

//-----------------------------------------------------------------------------
// NsprError::mapSocketError
//-----------------------------------------------------------------------------

inline void NsprError::mapSocketError()
{
#ifdef XP_WIN32
    mapWinsock2Error();
#else
    mapUnixErrno();
#endif
}

#endif // NSPRERROR_H
