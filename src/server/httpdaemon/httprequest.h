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

/*
** HttpRequest.h
*/

#ifndef _HTTPREQUEST_H_
#define _HTTPREQUEST_H_

class ListenSocketConfig;

#include "httpdaemon/httpheader.h"
#include "base/session.h"
#include "frame/req.h"
#include "frame/accel.h"
#include "frame/httpact.h"
#include "public/iwsstats.h"
#include "support/NSString.h"

class DaemonSession;
class VirtualServer;

class HTTPDAEMON_DLL HttpRequest {

 public:
    HttpRequest (DaemonSession *);
    ~HttpRequest ();

    static PRBool    Initialize ();
    static void      SetStrictHttpHeaders(PRBool f);
    static void      SetDiscardMisquotedCookies(PRBool f);
    static PRBool    GetDiscardMisquotedCookies(void);
    PRBool           StartSession(netbuf *buf);
    void             EndSession();
    PRBool           HandleRequest(netbuf *buf, PRIntervalTime timeout);

    void IncrementRecursionDepth() { currRecursionDepth++; }
    void DecrementRecursionDepth() { currRecursionDepth--; }
    PRUint32 GetRecursionDepth() const { return currRecursionDepth; }

    void SetFunction(const char *name) { fn = name; }
    const char* GetFunction(void) { return fn; }

    const char* GetServerHostname() const { return serverHostname; }

    static PRIntn getTpdIndex () { return myTpdIndex; }

    static HttpRequest *CurrentRequest();
    static void SetCurrentRequest(HttpRequest *hrq);

    DaemonSession& GetDaemonSession() { return *pSession; }
    const DaemonSession& GetDaemonSession() const { return *pSession; }
    const NSAPIRequest *GetNSAPIRequest() const;
    const NSAPISession *GetNSAPISession() const;
    AcceleratorHandle *GetAcceleratorHandle() const;
    httpd_objset* getObjset();
    const VirtualServer* getVS() const { return vs; }

 private:
    void             UnacceleratedRespond();
    PRBool           AcceleratedRespond();
    PRStatus         RestoreInputBuffer(netbuf *buf, unsigned char *inbuf, int maxsize);
    PRStatus         MakeHeadersPblock();
    void             processQOS();

 private:
    pool_handle_t              *pool;
    DaemonSession              *pSession;
    HttpHeader                 *rqHdr;
    ListenSocketConfig         *lsc;
    AcceleratorHandle          *accel;

    /* Space for NSAPI data structures */
    NSAPISession                rqSn;
    NSAPIRequest                rqRq;

    PRInt32                     idConfiguration;
    PRBool                      fQOS;
    PRBool                      fLogFinest;

    PRBool                      fRedirectToSSL;
    PRBool                      fClientAuthRequired;
    PRBool                      fClientAuthRequested;
    PRBool                      fSSLEnabled;
    PRBool                      fKeepAliveRequested;
    PRBool                      fAcceleratable;
    PRBool                      fNSAPIActive;
    PRBool                      fPipelined;

    PRUint32                    iStatus;   // 200, 304, etc

    static PRBool               fTCPDelayInherited;
    static PRIntn               myTpdIndex;

    /**
     * Determines whether strict header error checking is on or off.
     */
    static PRBool               fStrictHttpHeaders;

    /**
     * Determines whether misquoted cookie error checking is on or off.
     */
    static PRBool               fDiscardMisquotedCookies;

    PRUint32                    currRecursionDepth;

    /* whether to Canonicalize URI paths */
    static PRBool               fCanonicalizeURI;

    /**
     * HTTP server filter.
     **/
    static const Filter        *httpfilter;

    /**
     * HTTP server filter context object, reused across requests.
     **/
    HttpFilterContext          *httpfilter_context;

    // Information about the virtual server associated with this request
    VirtualServer* vs;
    const char* serverHostname;

    // NSAPI SAF we're currently executing
    const char *fn;
};

#endif // _HttpRequest_h_
