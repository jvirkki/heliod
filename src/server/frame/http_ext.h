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

#ifndef _HTTP_EXTENDED_H_
#define _HTTP_EXTENDED_H_

/*************************************************************************
    File : http_ext.h
    The intent if this file is to act as a place holder for c++ based functions.
    Some .c files include http.h and hence those will not compile if we
    put these functions in http.h
    Hence a new file to put these functions.
    The corresponding .cpp is still http.cpp
***************************************************************************/

#include <support/NSString.h>
#include <base/sslconf.h>

/* 
   Function: FormatHttpStatusCodeAndMessage
   Allows us to share the formatting of the string b/w backend NSAPI and
   front-end accelerator especially in cases of Use-Local_Copy etc 
*/
NSAPI_PUBLIC void
FormatHttpStatusCodeAndMessage(int code,
                               const char* msg, NSString& res);

NSAPI_PUBLIC HttpRequest* GetHrq(const Request* rq);

NSAPI_PUBLIC void
GetServerHostnameAndPort(const Request& rq, const Session& sn,
                         NSString& srvName, NSString& port);

NSAPI_PUBLIC int GetServerPort(const Request* rq);
NSAPI_PUBLIC const char* GetServerHostname(const Request* rq);

NSAPI_PUBLIC PRBool GetSecurity(const Session* sn);
NSAPI_PUBLIC const SSLSocketConfiguration* GetSecurityParams(const Request* rq);

NSAPI_PUBLIC void GetUrlComponents(const Request* rq, const char** scheme, const char** hostname, PRUint16* port);

#endif /* _HTTP_EXTENDED_H_ */

