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

#ifndef _NSAPIConnector_h_
#define _NSAPIConnector_h_

#include "netsite.h"
#include "base/cinfo.h"
#include "base/net.h"
#include "base/params.h"
#include "base/pblock.h"
#include "base/plist.h"
#include "base/pool.h"
#include "base/systhr.h"
#include "base/util.h"
#include "frame/conf.h"
#include "frame/http.h"
#include "frame/httpact.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/filter.h"
#include "frame/accel.h"
#include "safs/auth.h"
#include "NsprWrap/ReadWriteLock.h"
#include "NSAPIVirtualServer.h"

#ifdef XP_WIN32
#ifdef BUILD_J2EE_PLUGIN_DLL
#define _J2EE_PLUGIN_DLL_ _declspec(dllexport)
#else
#define _J2EE_PLUGIN_DLL_ _declspec(dllimport)
#endif /* BUILD_J2EE_PLUGIN_DLL */
#else
#define _J2EE_PLUGIN_DLL_
#endif /* XP_WIN32 */



class _J2EE_PLUGIN_DLL_ NSAPIConnector {
    public:
 
        static PRStatus init(pblock* pb);
        static NSAPIConnector *getConnector(pblock* pb, Session *sn, Request *rq);
        void release();

        PRBool isInternalRequest();
        static PRBool isWebAppRequest(Request *rq);

        void setCallStatus(PRInt32 status);
        PRInt32 getCallStatus(void);

        void enableOutputBuffering(void);
        int service(NSAPIVirtualServer* j2eeVS);

        const char* getAuthType(void);
        const char* getServerName(void);
        const char* getProtocol(void);
        const char* getRequestURI(void);
        const char* getEncodedRequestURI(void);
        const char* getMethod(void);
        const char* getQueryString(void);
        const char* getContentType(void);
        const char* getContentLength(void);
        const char* getCookie(void);
        const char* getAuthorization(void);
        const char* getProxyJroute(void);
        const char* getHeader(const char* name);
        const char* getRemoteAddr(void);
        const char* getRemoteHost(const char* ip);
        const char* getClientCert(void);
        const char* getHttpsCipher(void);
        int getHttpsKeySize(void);
        const Request* getRequest(void);
        const char* getCoreUser();
        const char* getCertificate(int dorequest);
        const char* getScheme(void);
        const char* getErrorRequestURI(void);
        int getErrorStatusCode(void);
        char* getAllocatedIOBuffer(PRInt32 len);
        void setStatus(int status, const char* msg);
        void setErrorDesc(const char* errorDesc);
        size_t getNumHeaders();
        size_t getNumParams();
        //----- Servlet 2.4
        int getRemotePort();
        int getLocalPort();
        char* getLocalAddr();
        //----- Servlet 2.4
        size_t processHeaders(JNIEnv* env, jobjectArray jbuf, size_t index);
        size_t processHeadersBytes(JNIEnv* env, jobjectArray jbuf, size_t index);
        size_t processParams(JNIEnv* env, jobjectArray jbuf, size_t index);

        const char* getResponseHeader(const char* name);
        jobjectArray getResponseHeaderNames(JNIEnv* env);
        jobjectArray getResponseHeaderValues(JNIEnv* env, jstring jname);
        void setResponseHeader(const char* name, const char* value, PRBool doReplace);
        void resetResponseHeaders(void);

        void setContentType(const char* ctype);
        void setContentLength(int clength);
        void setAuthUser(const char* user);

        void setSize(unsigned bytes);
        int flushResponse(void);
        void finishResponse(void);
        size_t write(char *buf, size_t offset, size_t len);

        int read(char* bytes, int len);

        int include(JNIEnv* env, jobject jres, jobject jos, jstring juri, jstring jquery);
        int forward(JNIEnv* env, jobject jres, jobject jis, jobject jos, jstring juri, jstring jquery, jstring jallow);

        static void log(int level, const char* msg);
        static void throwIllegalStateException(JNIEnv* env);

        Request* _rq;
        Session* _sn;
        PRThread* _parentThread;

    private:

        jobject _processor;
        NSAPIConnector *_nextActive;
        NSAPIConnector *_nextIdle;

        jobjectArray _rqStrs;
        jobjectArray _rqBytes;
        jintArray _rqInts;
        size_t _rqStrsLength;
        size_t _rqBytesLength;

        pblock* _pb;
        Request* _rq_orig;
        AcceleratorHandle* _accel;
        PRBool _inputStreamEOF;
        PRInt32 _outputStreamSize;

        PRInt32 _callStatus;
        PRBool _isAttached;

        struct 
        {
            PRInt32 bufferLength;
            char* buffer;
        } _ioBlock;

        static jclass _connectorClass;
        static jclass _processorClass;
        static jmethodID _processorService;
        static jmethodID _connectorCreateNSAPIProcessor;
        static jclass _servletexceptionClass;
        static jclass _illegalstateexceptionClass;

        static PRBool _alwaysAccelerateIncludes;
        static PRInt32 _maxDispatchDepth;
        static const Filter* _j2eefilter;

        static ReadWriteLock* _requestLock;

        static PRBool _crossThreadRequests;
        static SimpleIntHash *_reqHeaderNames;
        static jstring _methodGET;
        static jstring _methodPOST;

        static char* cvt2lower(const char *src, char *dst, size_t size);
        static char* convertNSAPIHeader(const char *src, char *dst, size_t size);
        static void createHeaderJavaStrings(JNIEnv *env, int numReqHdrs); 

        NSAPIConnector();
        ~NSAPIConnector();

        void setData(pblock* pb, Session *sn, Request *rq);
        void getRequestInfo(JNIEnv* env);
        PRBool isAttached();

        int dispatch(JNIEnv* env, jobject jres, jobject jis, jobject jos,
                     jstring juri, jstring jquery, jstring jallow,
                     PRBool forward);
};

#endif /* _NSAPIConnector_h_ */


