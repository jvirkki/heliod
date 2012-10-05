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

#include <stdarg.h>
#include <string.h>
#ifdef XP_WIN32
#include <winsock2.h>
#endif
#include "NsprError.h"

static const int ERRBUFSIZ = 256;

//-----------------------------------------------------------------------------
// NsprError::NsprError
//-----------------------------------------------------------------------------

NsprError::NsprError()
: _prError(0),
  _err(0),
  _text(NULL),
  _size(0),
  _len(0)
{ }

NsprError::NsprError(const NsprError& right)
: _prError(0),
  _err(0),
  _text(NULL),
  _size(0),
  _len(0)
{
    operator=(right);
}

//-----------------------------------------------------------------------------
// NsprError::operator=
//-----------------------------------------------------------------------------

NsprError& NsprError::operator=(const NsprError& right)
{
    // Make sure our buffer is big enough for right's error text
    if (right._size > 0 && _size <= right._len) {
        // Our buffer is too small.  Make it the same size as right's.
        _size = right._size;
        _text = (char *)PR_Realloc(_text, _size);
        if (_text == NULL)
            _size = 0;
    }

    // Copy error text from right
    if (_size > right._len) {
        if (right._len > 0)
            strcpy(_text, right._text);
        _len = right._len;
    } else {
        _len = 0;
    }

    _prError = right._prError;
    _err = right._err;

    return *this;
}

//-----------------------------------------------------------------------------
// NsprError::~NsprError
//-----------------------------------------------------------------------------

NsprError::~NsprError()
{
    if (_size > 0)
        PR_Free(_text);
}

//-----------------------------------------------------------------------------
// NsprError::save
//-----------------------------------------------------------------------------

void NsprError::save()
{
    // If there's error text...
    _len = PR_GetErrorTextLength();
    if (_len > 0) {
        // If our buffer is too small for the error text...
        if (_len >= _size) {
            // Grow our buffer
            _size = PR_MAX(ERRBUFSIZ, _len + 1);
            _text = (char *)PR_Realloc(_text, _size);
            if (_text == NULL)
                _size = 0;
        }

        // If our buffer is big enough for the error text...
        if (_size > _len) {
            // Save the error text
            PR_GetErrorText(_text);
        } else {
            // No room for the error text
            _len = 0;
        }
    }

    _prError = PR_GetError();
    _err = PR_GetOSError();
}

//-----------------------------------------------------------------------------
// NsprError::restore
//-----------------------------------------------------------------------------

void NsprError::restore() const
{
    PR_SetError(_prError, _err);

    /*
     * PR_SetError() cleared the error text, so we only need to set the error
     * text if we saved some previously.
     */
    if (_len > 0)
        PR_SetErrorText(_len, _text);
}

//-----------------------------------------------------------------------------
// _setError
//-----------------------------------------------------------------------------

static inline void _setError(PRErrorCode prError, PRInt32 err, const char *text, int len)
{
    /*
     * We need to always set the NSPR error code, not just the error message,
     * as the error message cannot be used to make error recovery decisions
     * programmatically.
     */
    PR_ASSERT(prError != 0 || (err == 0 && len == 0));
    PR_ASSERT(prError <= 0);
    PR_ASSERT(err >= 0);

    PR_SetError(prError, err);
    if (len > 0)
        PR_SetErrorText(len, text);
}

//-----------------------------------------------------------------------------
// _setErrorv
//-----------------------------------------------------------------------------

static inline void _setErrorv(PRErrorCode prError, PRInt32 err, const char *fmt, va_list args)
{
    char buf[ERRBUFSIZ];
    int len;

    len = PR_vsnprintf(buf, sizeof(buf), fmt, args);
    if (len < sizeof(buf) - 1) {
        // Formatted string fit on the stack
        _setError(prError, err, buf, len);
    } else {
        // Formatted string was too big for the stack
        char *msg = PR_smprintf(fmt, args);
        _setError(prError, err, msg, msg ? strlen(msg) : 0);
        PR_Free(msg);
    }
}

//-----------------------------------------------------------------------------
// NsprError::setError
//-----------------------------------------------------------------------------

void NsprError::setError(PRErrorCode prError, const char *text)
{
    _setError(prError, 0, text, text ? strlen(text) : 0);
}

void NsprError::setError(PRErrorCode prError, PRInt32 err, const char *text)
{
    _setError(prError, err, text, text ? strlen(text) : 0);
}

//-----------------------------------------------------------------------------
// NsprError::setErrorf
//-----------------------------------------------------------------------------

void NsprError::setErrorf(PRErrorCode prError, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _setErrorv(prError, 0, fmt, args);
    va_end(args);
}

void NsprError::setErrorf(PRErrorCode prError, PRInt32 err, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _setErrorv(prError, err, fmt, args);
    va_end(args);
}

//-----------------------------------------------------------------------------
// NsprError::mapUnixErrno
//-----------------------------------------------------------------------------

void NsprError::mapUnixErrno()
{
    PRInt32 err;
    PRErrorCode prError;

    err = errno;

    /*
     * From NSPR's mozilla/nsprpub/pr/src/md/unix/unix_errors.c
     */
    switch (err) {
    case EACCES:
        prError = PR_NO_ACCESS_RIGHTS_ERROR;
        break;
#ifdef EADDRINUSE
    case EADDRINUSE:
        prError = PR_ADDRESS_IN_USE_ERROR;
        break;
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL:
        prError = PR_ADDRESS_NOT_AVAILABLE_ERROR;
        break;
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT:
        prError = PR_ADDRESS_NOT_SUPPORTED_ERROR;
        break;
#endif
    case EAGAIN:
        prError = PR_WOULD_BLOCK_ERROR;
        break;
    /*
     * On QNX and Neutrino, EALREADY is defined as EBUSY.
     */
#if defined(EALREADY) && (EALREADY != EBUSY)
    case EALREADY:
        prError = PR_ALREADY_INITIATED_ERROR;
        break;
#endif
    case EBADF:
        prError = PR_BAD_DESCRIPTOR_ERROR;
        break;
#ifdef EBADMSG
    case EBADMSG:
        prError = PR_IO_ERROR;
        break;
#endif
    case EBUSY:
        prError = PR_FILESYSTEM_MOUNTED_ERROR;
        break;
#ifdef ECONNABORTED
    case ECONNABORTED:
        prError = PR_CONNECT_ABORTED_ERROR;
        break;
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED:
        prError = PR_CONNECT_REFUSED_ERROR;
        break;
#endif
#ifdef ECONNRESET
    case ECONNRESET:
        prError = PR_CONNECT_RESET_ERROR;
        break;
#endif
    case EDEADLK:
        prError = PR_DEADLOCK_ERROR;
        break;
#ifdef EDIRCORRUPTED
    case EDIRCORRUPTED:
        prError = PR_DIRECTORY_CORRUPTED_ERROR;
        break;
#endif
#ifdef EDQUOT
    case EDQUOT:
        prError = PR_NO_DEVICE_SPACE_ERROR;
        break;
#endif
    case EEXIST:
        prError = PR_FILE_EXISTS_ERROR;
        break;
    case EFAULT:
        prError = PR_ACCESS_FAULT_ERROR;
        break;
    case EFBIG:
        prError = PR_FILE_TOO_BIG_ERROR;
        break;
#ifdef EINPROGRESS
    case EINPROGRESS:
        prError = PR_IN_PROGRESS_ERROR;
        break;
#endif
    case EINTR:
        prError = PR_PENDING_INTERRUPT_ERROR;
        break;
    case EINVAL:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
    case EIO:
        prError = PR_IO_ERROR;
        break;
#ifdef EISCONN
    case EISCONN:
        prError = PR_IS_CONNECTED_ERROR;
        break;
#endif
    case EISDIR:
        prError = PR_IS_DIRECTORY_ERROR;
        break;
#ifdef ELOOP
    case ELOOP:
        prError = PR_LOOP_ERROR;
        break;
#endif
    case EMFILE:
        prError = PR_PROC_DESC_TABLE_FULL_ERROR;
        break;
    case EMLINK:
        prError = PR_MAX_DIRECTORY_ENTRIES_ERROR;
        break;
#ifdef EMSGSIZE
    case EMSGSIZE:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
#endif
#ifdef EMULTIHOP
    case EMULTIHOP:
        prError = PR_REMOTE_FILE_ERROR;
        break;
#endif
    case ENAMETOOLONG:
        prError = PR_NAME_TOO_LONG_ERROR;
        break;
#ifdef ENETUNREACH
    case ENETUNREACH:
        prError = PR_NETWORK_UNREACHABLE_ERROR;
        break;
#endif
    case ENFILE:
        prError = PR_SYS_DESC_TABLE_FULL_ERROR;
        break;
    /*
     * On SCO OpenServer 5, ENOBUFS is defined as ENOSR.
     */
#if defined(ENOBUFS) && (ENOBUFS != ENOSR)
    case ENOBUFS:
        prError = PR_INSUFFICIENT_RESOURCES_ERROR;
        break;
#endif
    case ENODEV:
        prError = PR_FILE_NOT_FOUND_ERROR;
        break;
    case ENOENT:
        prError = PR_FILE_NOT_FOUND_ERROR;
        break;
    case ENOLCK:
        prError = PR_FILE_IS_LOCKED_ERROR;
        break;
#ifdef ENOLINK 
    case ENOLINK:
        prError = PR_REMOTE_FILE_ERROR;
        break;
#endif
    case ENOMEM:
        prError = PR_OUT_OF_MEMORY_ERROR;
        break;
#ifdef ENOPROTOOPT
    case ENOPROTOOPT:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
#endif
    case ENOSPC:
        prError = PR_NO_DEVICE_SPACE_ERROR;
        break;
#ifdef ENOSR
    case ENOSR:
        prError = PR_INSUFFICIENT_RESOURCES_ERROR;
        break;
#endif
#ifdef ENOTCONN
    case ENOTCONN:
        prError = PR_NOT_CONNECTED_ERROR;
        break;
#endif
    case ENOTDIR:
        prError = PR_NOT_DIRECTORY_ERROR;
        break;
#ifdef ENOTSOCK
    case ENOTSOCK:
        prError = PR_NOT_SOCKET_ERROR;
        break;
#endif
    case ENXIO:
        prError = PR_FILE_NOT_FOUND_ERROR;
        break;
#ifdef EOPNOTSUPP
    case EOPNOTSUPP:
        prError = PR_NOT_TCP_SOCKET_ERROR;
        break;
#endif
#ifdef EOVERFLOW
    case EOVERFLOW:
        prError = PR_BUFFER_OVERFLOW_ERROR;
        break;
#endif
    case EPERM:
        prError = PR_NO_ACCESS_RIGHTS_ERROR;
        break;
    case EPIPE:
        prError = PR_CONNECT_RESET_ERROR;
        break;
#ifdef EPROTO
    case EPROTO:
        prError = PR_IO_ERROR;
        break;
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT:
        prError = PR_PROTOCOL_NOT_SUPPORTED_ERROR;
        break;
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE:
        prError = PR_ADDRESS_NOT_SUPPORTED_ERROR;
        break;
#endif
    case ERANGE:
        prError = PR_INVALID_METHOD_ERROR;
        break;
    case EROFS:
        prError = PR_READ_ONLY_FILESYSTEM_ERROR;
        break;
    case ESPIPE:
        prError = PR_INVALID_METHOD_ERROR;
        break;
#ifdef ETIMEDOUT
    case ETIMEDOUT:
        prError = PR_IO_TIMEOUT_ERROR;
        break;
#endif
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
    case EWOULDBLOCK:
        prError = PR_WOULD_BLOCK_ERROR;
        break;
#endif
    case EXDEV:
        prError = PR_NOT_SAME_DEVICE_ERROR;
        break;
    default:
        prError = PR_UNKNOWN_ERROR;
        break;
    }

    PR_SetError(prError, err);
}

//-----------------------------------------------------------------------------
// NsprError::mapWindowsError
//-----------------------------------------------------------------------------

#ifdef XP_WIN32

static inline void mapWindowsError(PRInt32 err)
{
    PRErrorCode prError;

    /*
     * From NSPR's mozilla/nsprpub/pr/src/md/windows/win32_errors.c
     */
    switch (err) {
    case ERROR_ACCESS_DENIED:
        prError = PR_NO_ACCESS_RIGHTS_ERROR;
        break;
    case ERROR_ALREADY_EXISTS:
        prError = PR_FILE_EXISTS_ERROR;
        break;
    case ERROR_BROKEN_PIPE:
        prError = PR_CONNECT_RESET_ERROR;
        break;
    case ERROR_DISK_CORRUPT:
        prError = PR_IO_ERROR; 
        break;
    case ERROR_DISK_FULL:
        prError = PR_NO_DEVICE_SPACE_ERROR;
        break;
    case ERROR_DISK_OPERATION_FAILED:
        prError = PR_IO_ERROR;
        break;
    case ERROR_DRIVE_LOCKED:
        prError = PR_FILE_IS_LOCKED_ERROR;
        break;
    case ERROR_FILENAME_EXCED_RANGE:
        prError = PR_NAME_TOO_LONG_ERROR;
        break;
    case ERROR_FILE_CORRUPT:
        prError = PR_IO_ERROR;
        break;
    case ERROR_FILE_EXISTS:
        prError = PR_FILE_EXISTS_ERROR;
        break;
    case ERROR_FILE_INVALID:
        prError = PR_BAD_DESCRIPTOR_ERROR;
        break;
    case ERROR_FILE_NOT_FOUND:
        prError = PR_FILE_NOT_FOUND_ERROR;
        break;
    case ERROR_HANDLE_DISK_FULL:
        prError = PR_NO_DEVICE_SPACE_ERROR;
        break;
    case ERROR_INVALID_ADDRESS:
        prError = PR_ACCESS_FAULT_ERROR;
        break;
    case ERROR_INVALID_HANDLE:
        prError = PR_BAD_DESCRIPTOR_ERROR;
        break;
    case ERROR_INVALID_NAME:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
    case ERROR_INVALID_PARAMETER:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
    case ERROR_INVALID_USER_BUFFER:
        prError = PR_INSUFFICIENT_RESOURCES_ERROR;
        break;
    case ERROR_LOCKED:
        prError = PR_FILE_IS_LOCKED_ERROR;
        break;
    case ERROR_NETNAME_DELETED:
        prError = PR_CONNECT_RESET_ERROR;
        break;
    case ERROR_NOACCESS:
        prError = PR_ACCESS_FAULT_ERROR;
        break;
    case ERROR_NOT_ENOUGH_MEMORY:
        prError = PR_INSUFFICIENT_RESOURCES_ERROR;
        break;
    case ERROR_NOT_ENOUGH_QUOTA:
        prError = PR_OUT_OF_MEMORY_ERROR;
        break;
    case ERROR_NOT_READY:
        prError = PR_IO_ERROR;
        break;
    case ERROR_NO_MORE_FILES:
        prError = PR_NO_MORE_FILES_ERROR;
        break;
    case ERROR_OPEN_FAILED:
        prError = PR_IO_ERROR;
        break;
    case ERROR_OPEN_FILES:
        prError = PR_IO_ERROR;
        break;
    case ERROR_OPERATION_ABORTED:
        prError = PR_OPERATION_ABORTED_ERROR;
        break;
    case ERROR_OUTOFMEMORY:
        prError = PR_INSUFFICIENT_RESOURCES_ERROR;
        break;
    case ERROR_PATH_BUSY:
        prError = PR_IO_ERROR;
        break;
    case ERROR_PATH_NOT_FOUND:
        prError = PR_FILE_NOT_FOUND_ERROR;
        break;
    case ERROR_SEEK_ON_DEVICE:
        prError = PR_IO_ERROR;
        break;
    case ERROR_SHARING_VIOLATION:
        prError = PR_FILE_IS_BUSY_ERROR;
        break;
    case ERROR_STACK_OVERFLOW:
        prError = PR_ACCESS_FAULT_ERROR;
        break;
    case ERROR_TOO_MANY_OPEN_FILES:
        prError = PR_SYS_DESC_TABLE_FULL_ERROR;
        break;
    case ERROR_WRITE_PROTECT:
        prError = PR_NO_ACCESS_RIGHTS_ERROR;
        break;
    case WSAEACCES:
        prError = PR_NO_ACCESS_RIGHTS_ERROR;
        break;
    case WSAEADDRINUSE:
        prError = PR_ADDRESS_IN_USE_ERROR;
        break;
    case WSAEADDRNOTAVAIL:
        prError = PR_ADDRESS_NOT_AVAILABLE_ERROR;
        break;
    case WSAEAFNOSUPPORT:
        prError = PR_ADDRESS_NOT_SUPPORTED_ERROR;
        break;
    case WSAEALREADY:
        prError = PR_ALREADY_INITIATED_ERROR;
        break;
    case WSAEBADF:
        prError = PR_BAD_DESCRIPTOR_ERROR;
        break;
    case WSAECONNABORTED:
        prError = PR_CONNECT_ABORTED_ERROR;
        break;
    case WSAECONNREFUSED:
        prError = PR_CONNECT_REFUSED_ERROR;
        break;
    case WSAECONNRESET:
        prError = PR_CONNECT_RESET_ERROR;
        break;
    case WSAEDESTADDRREQ:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
    case WSAEFAULT:
        prError = PR_ACCESS_FAULT_ERROR;
        break;
    case WSAEHOSTUNREACH:
        prError = PR_HOST_UNREACHABLE_ERROR;
        break;
    case WSAEINVAL:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
    case WSAEISCONN:
        prError = PR_IS_CONNECTED_ERROR;
        break;
    case WSAEMFILE:
        prError = PR_PROC_DESC_TABLE_FULL_ERROR;
        break;
    case WSAEMSGSIZE:
        prError = PR_BUFFER_OVERFLOW_ERROR;
        break;
    case WSAENETDOWN:
        prError = PR_NETWORK_DOWN_ERROR;
        break;
    case WSAENETRESET:
        prError = PR_CONNECT_ABORTED_ERROR;
        break;
    case WSAENETUNREACH:
        prError = PR_NETWORK_UNREACHABLE_ERROR;
        break;
    case WSAENOBUFS:
        prError = PR_INSUFFICIENT_RESOURCES_ERROR;
        break;
    case WSAENOPROTOOPT:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
    case WSAENOTCONN:
        prError = PR_NOT_CONNECTED_ERROR;
        break;
    case WSAENOTSOCK:
        prError = PR_NOT_SOCKET_ERROR;
        break;
    case WSAEOPNOTSUPP:
        prError = PR_OPERATION_NOT_SUPPORTED_ERROR;
        break;
    case WSAEPROTONOSUPPORT:
        prError = PR_PROTOCOL_NOT_SUPPORTED_ERROR;
        break;
    case WSAEPROTOTYPE:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
    case WSAESHUTDOWN:
        prError = PR_SOCKET_SHUTDOWN_ERROR;
        break;
    case WSAESOCKTNOSUPPORT:
        prError = PR_INVALID_ARGUMENT_ERROR;
        break;
    case WSAETIMEDOUT:
        prError = PR_CONNECT_ABORTED_ERROR;
        break;
    case WSAEWOULDBLOCK:
        prError = PR_WOULD_BLOCK_ERROR;
        break;
    default:
        prError = PR_UNKNOWN_ERROR;
        break;
    }

    PR_SetError(prError, err);
}

#endif // XP_WIN32

//-----------------------------------------------------------------------------
// NsprError::mapWin32Error
//-----------------------------------------------------------------------------

#ifdef XP_WIN32

void NsprError::mapWin32Error()
{
    PRInt32 err;

    err = GetLastError();
    if (err == 0)
        err = WSAGetLastError();

    mapWindowsError(err);
}

#endif // XP_WIN32

//-----------------------------------------------------------------------------
// NsprError::mapWinsock2Error
//-----------------------------------------------------------------------------

#ifdef XP_WIN32

void NsprError::mapWinsock2Error()
{
    PRInt32 err;

    err = WSAGetLastError();
    if (err == 0)
        err = GetLastError();

    mapWindowsError(err);
}

#endif // XP_WIN32
