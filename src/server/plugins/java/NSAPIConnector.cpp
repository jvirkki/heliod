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

/**
 * NSAPI servlets implementation
 */

#include "netsite.h"
#include "frame/redirect.h"
#include "frame/http_ext.h"
#include "frame/httpfilter.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/httprequest.h"
#include "safs/clauth.h"
#include "support/stringvalue.h"

#include "JVMControl.h"
#include "NSJavaUtil.h"
#include "NSJavaI18N.h"
#include "j2eefilter.h"

#include "com_sun_webserver_connector_nsapi_NSAPIConnector.h"
#include "com_sun_webserver_logging_NSAPIServerHandler.h"

#include "NSAPIConnector.h"
#include "JavaStatsManager.h"

#define NSAPICONNECTOR  "com/sun/webserver/connector/nsapi/NSAPIConnector"
#define NSAPIPROCESSOR  "com/sun/webserver/connector/nsapi/NSAPIProcessor"
#define SERVLETEXCEPTION "javax/servlet/ServletException"
#define ILLEGALSTATEEXCEPTION "java/lang/IllegalStateException"

#define MAGNUS_INTERNAL "magnus-internal"
#define MAGNUS_INTERNAL_LEN (sizeof(MAGNUS_INTERNAL) - 1)
#define TEXT_HTML "text/html"
#define TEXT_HTML_LEN (sizeof(TEXT_HTML) - 1)

static const int MAX_NETBUF_SIZE = 64 * 1024;

// The following numbers must be changed in sync with the
// values in NSAPIRequest.java
// Headers values passed as UTF-8 strings
static const size_t NUM_FIXED_STRING_HEADERS = 14;
// Header values passed as bytes
static const size_t NUM_FIXED_BYTE_HEADERS = 6;
// Header values passed as integers
static const size_t NUM_INT_HEADERS = 7;

ReadWriteLock* NSAPIConnector::_requestLock = NULL;
static int _activeSlot = -1;
static int _idleSlot = -1;
static int _poolKey = -1;
// Number of header names within "_specialReqHeaders" array.
static int _numSpecialHeaders = 0;

SimpleIntHash *NSAPIConnector::_reqHeaderNames = NULL;
jstring NSAPIConnector::_methodGET = NULL;
jstring NSAPIConnector::_methodPOST = NULL;
jclass NSAPIConnector::_connectorClass = 0;
jclass NSAPIConnector::_processorClass = 0;
jmethodID NSAPIConnector::_processorService;
jmethodID NSAPIConnector::_connectorCreateNSAPIProcessor;
jclass NSAPIConnector::_servletexceptionClass = 0;
jclass NSAPIConnector::_illegalstateexceptionClass = 0;

// Used for debugging and perf runs
PRBool NSAPIConnector::_alwaysAccelerateIncludes = PR_FALSE; 

int NSAPIConnector::_maxDispatchDepth = 10;
const Filter* NSAPIConnector::_j2eefilter = 0;

// pblock to store those request header names which are 
// added prior to processHeaders() call 
static pblock *_specialReqHeadersPb = NULL;

// This array contains those request header names which are added 
// prior to processHeaders() call
static const pb_key *_specialReqHeaders[] = {
    pb_key_authorization,
    pb_key_content_length,
    pb_key_content_type,
    pb_key_cookie,
    pb_key_proxy_jroute
};

// Common request header names
static const char *_httpReqHeaders[] = { 
    "accept",
    "accept-charset",
    "accept-encoding",
    "accept-language",
    "connection",
    "content-encoding",
    "content-language",
    "content-location",
    "content-md5",
    "content-range",
    "date",
    "expect",
    "expires",
    "from",
    "host",
    "if-match",
    "if-modified-since",
    "if-none-match",
    "if-range",
    "if-unmodified-since",
    "last-modified",
    "max-forwards",
    "pragma",
    "proxy-authorization",
    "range",
    "referer",
    "transfer-encoding",
    "user-agent",
    "via",
    "warning"
};    

// Utility class that initializes the NSAPI environment for the current
// thread in its constructor and resets the thread local storage for the memory
// pool in its destructor
class NSAPIEnv {
    public:
        inline NSAPIEnv(Session* sn, Request* rq, PRThread* parent);
        inline ~NSAPIEnv();
        inline PRBool isValid();
    
    private:
        PRBool _valid;
        PRBool _reset;
};


/**
 * Initialize the NSAPI environment on a thread:
 *
 * If the current thread is the same as the parent (DaemonSession) thread, then
 * the environment is already set up, so do nothing
 *
 * If the current thread is different from the parent thread and if it is not
 * another DaemonSession thread (i.e. it doesn't have a pool) then setup the
 * NSAPI environment
 *
 * If the current thread is different from the parent thread and it is a thread
 * whose NSAPI environment is already setup then do not overwrite that and 
 * instead report an error.
 */
inline
NSAPIEnv::NSAPIEnv(Session* sn, Request* rq, PRThread* parent)
{
    PRThread* current = PR_GetCurrentThread();
    _reset = PR_FALSE;
    _valid = PR_TRUE;
    if (current != parent) {
        pool_handle_t* pool = (pool_handle_t*) systhread_getdata(_poolKey);
        if (pool != NULL) {
            // Abort processing if the thread already has a pool
            _valid = PR_FALSE;
            return;
        }
        // This is a non-DaemonSession thread, initialize its NSAPI environment
        prepare_nsapi_thread(rq, sn);
        _reset = PR_TRUE;
    }
}

inline
NSAPIEnv::~NSAPIEnv()
{
    // If the thread's environment was set in the constructor, then reset it
    // so that the next time around this thread isn't mistaken for a 
    // DaemonSession thread
    if (_reset)
        systhread_setdata(_poolKey, NULL);
}

inline PRBool
NSAPIEnv::isValid(void)
{
    return _valid;
}

NSPR_BEGIN_EXTERN_C
void deleteProcessorSlot(void* ptr)
{
    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env != NULL) {
        if (ptr != NULL)
            env->DeleteGlobalRef((jobject) ptr);
    }
}
NSPR_END_EXTERN_C

void
NSAPIConnector::setData(pblock* pb, Session* session, Request* rq)
{
    _pb = pb;
    _rq = rq;
    // always point to the original request
    _rq_orig = (rq != NULL ? rq->orig_rq : NULL);
    _sn = session;
    _accel = NULL;
    _inputStreamEOF = PR_FALSE; // indication that IS has end-of-filed
    _callStatus = REQ_ABORTED;
    _outputStreamSize = 0;
    _parentThread = PR_GetCurrentThread();
}

NSAPIConnector::NSAPIConnector()
{
    _rqStrsLength = 0;
    _rqBytesLength = 0;
    _rqStrs = NULL;
    _ioBlock.buffer = NULL;
    _ioBlock.bufferLength = 0;

    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();
    if (env == NULL) {
        _rqBytes = NULL;
        _rqInts = NULL;
        _processor = NULL;
        _isAttached = PR_FALSE;
        return;
    } else {
        _isAttached = PR_TRUE;
    }

    // multibyte headers
    _rqBytes = NSJavaUtil::createStringBytesArray(env, NUM_FIXED_BYTE_HEADERS);
    if (_rqBytes != NULL) {
        jobject jLocal = _rqBytes;
        _rqBytes = (jobjectArray) env->NewGlobalRef(_rqBytes);
        env->DeleteLocalRef(jLocal);
        _rqBytesLength = NUM_FIXED_BYTE_HEADERS;
    }

    _rqInts = NSJavaUtil::createIntArray(env, NUM_INT_HEADERS);
    if (_rqInts != NULL) {
        jobject jLocal = _rqInts;
        _rqInts = (jintArray) env->NewGlobalRef(_rqInts);
        env->DeleteLocalRef(jLocal);
    }

    _processor = env->CallStaticObjectMethod(_connectorClass,
                _connectorCreateNSAPIProcessor);
    // Release the local ref after creating a global ref
    if (_processor != NULL) {
        jobject jLocal = _processor;
        _processor = env->NewGlobalRef(jLocal);
        env->DeleteLocalRef(jLocal);
    } 
}

NSAPIConnector::~NSAPIConnector()
{
    if (_ioBlock.buffer)
        PERM_FREE(_ioBlock.buffer);
    if (!isAttached())
        return;

    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (_rqInts != NULL) 
        env->DeleteGlobalRef(_rqInts);
    if (_rqBytes != NULL) 
        env->DeleteGlobalRef(_rqBytes);
    if (_rqStrs != NULL) 
        env->DeleteGlobalRef(_rqStrs);
    if (_processor != NULL)
        env->DeleteGlobalRef(_processor);
}

/**
 * This routine is responsible for allocating a buffer
 * based on the required length given as input parameter.
 * The allocated buffer will be always 8 byte aligned.
 * @return address of the allocated buffer or NULL 
 */
char*
NSAPIConnector::getAllocatedIOBuffer(PRInt32 len)
{
    // IO buffer for NSAPIConnector will be allocated and aligned
    // based on the size mentioned below.
    static const int MAX_IOBUF_SIZE = 8 * 1024;

    // Allocate buffer on a MAX_IOBUF_SIZE(8K) aligned.
    PRInt32 newLength = ((PRInt32)(len + (MAX_IOBUF_SIZE - 1)) & ~(PRInt32)(MAX_IOBUF_SIZE - 1));
    if (_ioBlock.buffer == NULL) {
        _ioBlock.buffer = (char*)PERM_MALLOC(newLength);
        if (_ioBlock.buffer)
            _ioBlock.bufferLength = newLength;
    } else if (len > _ioBlock.bufferLength) {
        _ioBlock.buffer = (char*)PERM_REALLOC(_ioBlock.buffer, newLength);
        if (_ioBlock.buffer)
            _ioBlock.bufferLength = newLength;
    }

    return _ioBlock.buffer;
}

PRStatus
NSAPIConnector::init(pblock* pb)
{
    _requestLock = new ReadWriteLock();    // a lock for serialized requests
    
    if (_requestLock == NULL)
        return PR_FAILURE;
    
    JvmCall jvm_call;
    JNIEnv* env = jvm_call.getEnv();

    if (env == NULL)
        return PR_FAILURE;    // error is already logged at this point

    _alwaysAccelerateIncludes = conf_getboolean("AlwaysAccelerateIncludes", PR_FALSE);

    const char* maxDispatchDepth = pblock_findval("MaxDispatchDepth", pb);
    if (maxDispatchDepth)
        _maxDispatchDepth = atoi(maxDispatchDepth);

    if (maxDispatchDepth)
        _maxDispatchDepth = atoi(maxDispatchDepth);

    _connectorClass = NSJavaUtil::findClassGlobal(env, NSAPICONNECTOR);
    if (_connectorClass == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.ERR_CLASS_LOOKUP");
        ereport(LOG_FAILURE, logMsg, NSAPICONNECTOR);
        FREE(logMsg);
        return PR_FAILURE;
    }
    _processorClass = NSJavaUtil::findClassGlobal(env, NSAPIPROCESSOR);
    if (_processorClass == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.ERR_CLASS_LOOKUP");
        ereport(LOG_FAILURE, logMsg, NSAPIPROCESSOR);
        FREE(logMsg);
        return PR_FAILURE;
    }

    static const char* JNISIG = "(JLorg/apache/catalina/core/StandardHost;[Ljava/lang/String;[[B[ILorg/apache/catalina/core/StandardContext;Ljava/lang/String;Lorg/apache/catalina/core/StandardWrapper;)I";
    _processorService = env->GetMethodID(_processorClass, "service", JNISIG);
    if (_processorService == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.ERR_METHODS_LOOKUP");
        ereport(LOG_FAILURE, logMsg);
        FREE(logMsg);
        return PR_FAILURE;
    }

    _connectorCreateNSAPIProcessor = env->GetStaticMethodID(
        _connectorClass, "createNSAPIProcessor",
        "()Lcom/sun/webserver/connector/nsapi/NSAPIProcessor;");
    if (_connectorCreateNSAPIProcessor == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.ERR_METHODS_LOOKUP");
        ereport(LOG_FAILURE, logMsg);
        FREE(logMsg);
        return PR_FAILURE;
    }

    _servletexceptionClass = NSJavaUtil::findClassGlobal(env,
                                                         SERVLETEXCEPTION);
    if (_servletexceptionClass == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.ERR_CLASS_LOOKUP");
        ereport(LOG_FAILURE, logMsg, SERVLETEXCEPTION);
        FREE(logMsg);
        return PR_FAILURE;
    }

    _illegalstateexceptionClass =
        NSJavaUtil::findClassGlobal(env, ILLEGALSTATEEXCEPTION);
    if (_illegalstateexceptionClass == NULL) {
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.ERR_CLASS_LOOKUP");
        ereport(LOG_FAILURE, logMsg, ILLEGALSTATEEXCEPTION);
        FREE(logMsg);
        return PR_FAILURE;
    }

    // Create the thread local storage that will point to an active
    // NSAPIConnector
    _activeSlot = session_alloc_thread_slot(NULL);

    // Create the thread local storage that will point to an idle
    // NSAPIConnector
    _idleSlot = session_alloc_thread_slot(NULL);

    // Get the handle to the thread local storage for the memory pool
    _poolKey = getThreadMallocKey();

    // initialize the J2EE-NSAPI filter subsystem
    PRStatus rv = j2eefilter_init(env);
    if (rv != PR_SUCCESS)
        return PR_FAILURE;

    _j2eefilter = j2eefilter_get_filter();

    if ( StatsManager::isInitialized() == PR_TRUE )
    {
        JavaStatsManager* javaStatsManger = new JavaStatsManager();
        StatsManager::setJavaStatsManager( javaStatsManger );
    }

    int numReqHdrs = sizeof(_httpReqHeaders)/sizeof(const char *);
    _numSpecialHeaders = sizeof(_specialReqHeaders)/sizeof(const pb_key *);
    _reqHeaderNames = new SimpleIntHash(numReqHdrs);
    createHeaderJavaStrings(env, numReqHdrs);
    
    return PR_SUCCESS;
}

void
NSAPIConnector::createHeaderJavaStrings(JNIEnv *env, int numReqHdrs)
{
   int i;

   // For every common request header name, create a "jstring" 
   // object with that name and insert it into the Hashtable
   for (i = 0; i < numReqHdrs; i++) { 
       const pb_key *hKey = pblock_key(_httpReqHeaders[i]);
       if (hKey) {
           jstring buf = NSJavaUtil::createString(env, _httpReqHeaders[i]);
           jstring hObj = (jstring) env->NewGlobalRef(buf);
           env->DeleteLocalRef(buf);
           _reqHeaderNames->insert((void *)hKey, (void *)hObj);    
       }
   }

   // Create "jstring" objects for the common request method 
   // types - "GET" and "PUT"
   jstring mt = NSJavaUtil::createString(env, "GET");
   _methodGET = (jstring) env->NewGlobalRef(mt);
   env->DeleteLocalRef(mt);
   mt = NSJavaUtil::createString(env, "POST");
   _methodPOST = (jstring) env->NewGlobalRef(mt);
   env->DeleteLocalRef(mt);

   // Initialize the pblock "_specialReqHeadersPb" with the 
   // keys of "_specialReqHeaders" array.
   _specialReqHeadersPb = pblock_create(_numSpecialHeaders);
   for (i = 0; i < _numSpecialHeaders; i++) {
       pblock_kvinsert(_specialReqHeaders[i], NULL, 0, 
                       _specialReqHeadersPb);
   }
}

NSAPIConnector*
NSAPIConnector::getConnector(pblock* pb, Session *sn, Request *rq)
{
    NSAPIConnector *connector;
 
    // Pop an NSAPIConnector from the idle stack or create a new one
    connector = (NSAPIConnector*) session_get_thread_data(sn, _idleSlot);
    if (connector) {
        session_set_thread_data(sn, _idleSlot, connector->_nextIdle);
        connector->_nextIdle = NULL;
    } else {
        connector = new NSAPIConnector;
        if (!connector->isAttached()) {
            delete connector;
            return NULL;
        }
    }

    // Setup the NSAPIConnector for the current request
    connector->setData(pb, sn, rq);

    // Push this NSAPIConnector onto the active stack
    connector->_nextActive = (NSAPIConnector*) session_get_thread_data(sn, _activeSlot);
    session_set_thread_data(sn, _activeSlot, connector);
 
    return connector;
}

void 
NSAPIConnector::release()
{
    PR_ASSERT(this == (NSAPIConnector*) session_get_thread_data(_sn, _activeSlot));

    // Pop this NSAPIConnector from the active stack
    session_set_thread_data(_sn, _activeSlot, _nextActive);
    _nextActive = NULL;

    // Push this NSAPIConnector onto the idle stack
    _nextIdle = (NSAPIConnector*) session_get_thread_data(_sn, _idleSlot);
    session_set_thread_data(_sn, _idleSlot, this);
}

void 
NSAPIConnector::setCallStatus(PRInt32 status)
{
    _callStatus = status;
}

PRInt32
NSAPIConnector::getCallStatus(void)
{
    return _callStatus;
}

/**
 * service  Actual service function for servlets. It's being called by 
 * NSAPI service function
 *
 * @return  PR_TRUE if succeded
 */
int
NSAPIConnector::service(NSAPIVirtualServer* j2eeVS)
{
    NSAPIRequest* nrq = (NSAPIRequest*)_rq;
    int rval = REQ_ABORTED;
    int attach = 1;

    setCallStatus(REQ_ABORTED);

    JvmCall jvm_call;

    JNIEnv *env = jvm_call.getEnv();

    if (env == NULL || !isWebAppRequest(_rq))
        return REQ_ABORTED;

    // Default Servlet content-type is text/html
    pb_param *type = pblock_findkey(pb_key_content_type, _rq->srvhdrs);
    if (type) {
        if (!strncmp(type->value, MAGNUS_INTERNAL, MAGNUS_INTERNAL_LEN)) {
            FREE(type->value);
            type->value = (char*) MALLOC(TEXT_HTML_LEN + 1);
            strcpy(type->value, TEXT_HTML);
        }
    } else {
        pblock_kvinsert(pb_key_content_type, TEXT_HTML, TEXT_HTML_LEN,
                        _rq->srvhdrs);
    }

    PRBool serializeRequestLocked = PR_FALSE;

    if (JVMControl::isSerializeFirstRequest() &&  
        !JVM_REQUEST_LOCKED(_rq_orig)) {
    
        if (nrq->webModule == NULL)
            _requestLock->acquireWrite();
        else
            _requestLock->acquireRead ();

        JVM_REQUEST_LOCKED(_rq_orig) = 1;
        serializeRequestLocked = PR_TRUE;
    }

    // The string array needs one element for each "known" slot +
    // one element for the name and one for the value of each header
    WebModule* wm = (WebModule*) nrq->webModule;
    size_t rqStrsNewLength = 0;
    size_t rqBytesNewLength = 0;
    if (wm->useRequestEncforHeaders()) {
        rqStrsNewLength = NUM_FIXED_STRING_HEADERS + 1 + (getNumParams() * 2);
        rqBytesNewLength =  NUM_FIXED_BYTE_HEADERS + (getNumHeaders() * 2) + 1;
    } else {
        rqStrsNewLength = NUM_FIXED_STRING_HEADERS + (getNumHeaders() * 2) + 1 +
                       (getNumParams() * 2) + 1;
        rqBytesNewLength = NUM_FIXED_BYTE_HEADERS;
    }
 
    if (rqStrsNewLength > _rqStrsLength) {
        if (_rqStrs != NULL)
            env->DeleteGlobalRef(_rqStrs);
        _rqStrs = NSJavaUtil::createStringArray(env, rqStrsNewLength);
        if (_rqStrs != NULL) {
            jobject jLocal = _rqStrs;
            _rqStrs = (jobjectArray) env->NewGlobalRef(_rqStrs);
            env->DeleteLocalRef(jLocal);
            _rqStrsLength = rqStrsNewLength;
        }
    } 

    if (rqBytesNewLength > _rqBytesLength) {
        if (_rqBytes != NULL)
            env->DeleteGlobalRef(_rqBytes);
        _rqBytes = NSJavaUtil::createStringBytesArray(env, rqBytesNewLength);
        if (_rqBytes != NULL) {
            jobject jLocal = _rqBytes;
            _rqBytes = (jobjectArray) env->NewGlobalRef(_rqBytes);
            env->DeleteLocalRef(jLocal);
            _rqBytesLength = rqBytesNewLength;
        }
    }

    if ((_rqStrs != NULL) && (_rqInts != NULL) && (_rqBytes != NULL)) {
        getRequestInfo(env);
        long jVirtualServer = j2eeVS->getJavaObject();

        WebModule* wm = (WebModule*) nrq->webModule;
        const char* contextPath = (const char*) wm->getContextPath();
        jstring jcontextPath = NSJavaUtil::createString(env, contextPath);

        long jContext = wm->getJavaContextObject();
        ServletResource* servletResource = (ServletResource*)nrq->servletResource;
        jobject jServletWrapper = NULL;
        if (servletResource) {
            jServletWrapper = (jobject) servletResource->getServletWrapper();
        }

        if (!env->ExceptionOccurred())
            rval = env->CallIntMethod(_processor, _processorService,
                                      (jlong)this, (jobject)jVirtualServer,
                                      _rqStrs, _rqBytes, _rqInts, 
                                      (jobject)jContext, jcontextPath, 
                                      jServletWrapper);

        if (jcontextPath != NULL)
            env->DeleteLocalRef(jcontextPath);

    } else {
        protocol_status (_sn, _rq, PROTOCOL_SERVER_ERROR, NULL);
        rval = REQ_ABORTED;
    }

    if (serializeRequestLocked == PR_TRUE) {
        _requestLock->release();
        JVM_REQUEST_LOCKED(_rq_orig) = 0;
    }

    if (NSJavaUtil::reportException(env) == PR_TRUE) {
        protocol_status (_sn, _rq, PROTOCOL_SERVER_ERROR, NULL);
        rval = REQ_ABORTED;
    }

    if (rval == REQ_PROCEED) {
        if (!_rq->status_num)
            protocol_status(_sn, _rq, PROTOCOL_OK, NULL);
    }

    setCallStatus(rval);
    return rval;
}

void
NSAPIConnector::enableOutputBuffering(void)
{
    httpfilter_buffer_output(_sn, _rq, PR_TRUE);

    _outputStreamSize = httpfilter_get_output_buffer_size(_sn, _rq);
}

size_t
NSAPIConnector::getNumHeaders(void)
{
    int i;
    int len = 0;
    const size_t MAX_HEADERS = 1024;
    // HTTP request headers should always come from the original request
    for (i = 0; i < _rq->headers->hsize; i++) {
        pb_entry *p = _rq->headers->ht[i];
        for ( ; (p != NULL && len < MAX_HEADERS); p = p->next) {
            len++;
        }
    }
    return len;
}

size_t
NSAPIConnector::getNumParams(void)
{
    const pblock *pb = ((NSAPIRequest*)_rq)->param;
    if (!pb)
        return 0;

    int i;
    int len = 0;
    const size_t MAX_PARAMS = 1024;
    for (i = 0; i < pb->hsize; i++) {
        pb_entry *p = pb->ht[i];
        for ( ; (p != NULL && len < MAX_PARAMS); p = p->next) {
            if (p->param->value)
                len++;
        }
    }
    return len;
}


size_t
NSAPIConnector::processHeaders(JNIEnv* env, jobjectArray jbuf, size_t index)
{
    const size_t MAX_HEADERS = 1024;
    size_t len = 0;
    int i;
    const pb_key *rHdrKey = NULL;
    jstring hdrName = NULL;

    // HTTP request headers should always come from the original request
    for (i = 0; i < _rq->headers->hsize; i++) {
        pb_entry *p = _rq->headers->ht[i];
        for ( ; (p != NULL && len < MAX_HEADERS); p = p->next, len++) {
            rHdrKey = param_key(p->param); 

            // Do not add the headers that have already been added to 
            // the array 
            if (rHdrKey && 
                (pblock_findkey(rHdrKey, _specialReqHeadersPb) != NULL)) {
                continue;
            }

            // Lookup the existing header name object
            if (rHdrKey) {
                hdrName = (jstring) _reqHeaderNames->lookup((void *)rHdrKey);
            } else {
                hdrName = NULL;
            }

            // If existing header name is not found, create one 
            if (hdrName == NULL) {
                NSJavaUtil::setStringElement(env, jbuf, index++, 
                                             p->param->name);
            } else { 
                env->SetObjectArrayElement(jbuf, index++, hdrName);
            }
            NSJavaUtil::setStringElement(env, jbuf, index++, p->param->value);
        }
    }
    return index;
}

size_t
NSAPIConnector::processHeadersBytes(JNIEnv* env, jobjectArray jbuf, size_t index)
{
    const size_t MAX_HEADERS = 1024;
    size_t len = 0;
    int i;

    // HTTP request headers should always come from the original request
    for (i = 0; i < _rq->headers->hsize; i++) {
        pb_entry *p = _rq->headers->ht[i];
        for ( ; (p != NULL && len < MAX_HEADERS); p = p->next) {

            NSJavaUtil::setByteArrayElement(env, jbuf, index++, p->param->name);
            NSJavaUtil::setByteArrayElement(env, jbuf, index++, p->param->value);
            len++;
        }
    }
    return index;
}


size_t
NSAPIConnector::processParams(JNIEnv* env, jobjectArray jbuf, size_t index)
{
    const pblock *pb = ((NSAPIRequest*)_rq)->param;
    if (pb) {
        const size_t MAX_PARAMS = 1024;
        size_t len = 0;
        int i;

        for (i = 0; i < pb->hsize; i++) {
            pb_entry *p = pb->ht[i];
            for ( ; (p != NULL && len < MAX_PARAMS); p = p->next) {
                if (p->param->value) {
                    NSJavaUtil::setStringElement(env, jbuf, index++,
                                                 p->param->name);
                    NSJavaUtil::setStringElement(env, jbuf, index++,
                                                 p->param->value);
                    len++;
                }
            }
        }
    }

    return index;
}


PRBool 
NSAPIConnector::isWebAppRequest(Request *rq)
{
    return ((NSAPIRequest*)rq)->webModule ? PR_TRUE:PR_FALSE;
}


PRBool
NSAPIConnector::isInternalRequest()
{
    return INTERNAL_REQUEST(_rq);
}

char *
NSAPIConnector::cvt2lower(const char* src, char* dst, size_t size)
{
    int i = 0;
    for ( ; src[i] && (size == 0 || i < size - 1); i++)
        dst[i] = tolower(src[i]);

    dst[i] = 0;
    return dst;
}

char *
NSAPIConnector::convertNSAPIHeader(const char* src, char* dst, size_t size)
{
    int i = 0;

    // Kludge for Bug 4650194
    // The app sets a header called status which overwrites
    // the NSAPI status header
    // if first char is lower s, change it to upper S, so that
    // it does not overwrite the status entry in the pblock
    if (src[i] && src[i] == 's' && (size == 0 || i < size - 1)) {
        dst[i] = 'S';
        i++;
    }

    for ( ; src[i] && (size == 0 || i < size - 1); i++)
        dst[i] = tolower(src[i]);
        
    dst[i] = 0;
    return dst;
}

const char* 
NSAPIConnector::getAuthType(void)
{
    const char *val = pblock_findkeyval(pb_key_auth_type, _rq->vars);
    if (val == NULL)
        val = pblock_findkeyval(pb_key_auth_type, _rq_orig->vars);
    
    return val;  
}

const char*
NSAPIConnector::getClientCert(void)
{
    return pblock_findkeyval(pb_key_auth_cert, _rq_orig->vars);
}

const char*
NSAPIConnector::getHttpsCipher(void)
{
    return pblock_findval("cipher", _sn->client);
}

const char*
NSAPIConnector::getErrorRequestURI(void)
{
    return pblock_findkeyval(pb_key_uri, _rq_orig->reqpb);
}

int
NSAPIConnector::getErrorStatusCode(void)
{
    return _rq_orig->status_num;
}

int
NSAPIConnector::getHttpsKeySize(void)
{
    // 'secret-keysize' contains the effective key size bits, this is the
    // value applications might be interested in
    const char* value = pblock_findval("secret-keysize", _sn->client);
    int keySize = 0;
    if (value != NULL)
        keySize = atoi(value);
    return keySize;
}

const char*
NSAPIConnector::getServerName(void)
{
    if (_rq && _rq->hostname)
        return _rq->hostname;
    
    return conf_getglobals()->Vserver_hostname;
}

const char*
NSAPIConnector::getRequestURI(void)
{
    return pblock_findkeyval(pb_key_uri, _rq->reqpb);
}

// Get the original request URI (with encoding), if null
// return getRequestURI()
const char*
NSAPIConnector::getEncodedRequestURI(void)
{
    // Extract the encoded URI from the request line if possible
    char *clf_request = pblock_findkeyval(pb_key_clf_request, _rq->reqpb);
    if (clf_request) {
        // Find the beginning of the method
        char *method = clf_request;
        while (isspace(*method))
            method++;

        // Find the beginning of the URI
        char *uri = method;
        while (*uri && !isspace(*uri))
            uri++;
        while (isspace(*uri))
            uri++;

        // Find the end of the URI
        char *protocol = uri;
        while (*protocol && !isspace(*protocol) && (*protocol != '?'))
            protocol++;

        // Create a copy of the unencoded URI
        char c = *protocol;
        *protocol = '\0';

        // Make a copy of the uri
        int len = strlen(uri);
        char* encodedUri = (char*) pool_malloc(_sn->pool, len+1);
        memcpy(encodedUri, uri, len);
        encodedUri[len] = '\0';
        *protocol = c;
        return encodedUri;
    }

    // No request line. (Internal request?) Fall back to the decoded URI.
    return pblock_findkeyval(pb_key_uri, _rq->reqpb);
}

const char*
NSAPIConnector::getQueryString(void)
{
    return pblock_findkeyval(pb_key_query, _rq->reqpb);
}

const char*
NSAPIConnector::getProtocol(void)
{
    return pblock_findkeyval(pb_key_protocol, _rq->reqpb);
}

const char*
NSAPIConnector::getMethod(void)
{
    return pblock_findkeyval(pb_key_method, _rq->reqpb);
}

const char*
NSAPIConnector::getContentType(void)
{
    return pblock_findkeyval(pb_key_content_type, _rq->headers);
}

const char*
NSAPIConnector::getContentLength(void)
{
    return pblock_findkeyval(pb_key_content_length, _rq->headers);
}

const char*
NSAPIConnector::getCookie(void)
{
    return pblock_findkeyval(pb_key_cookie, _rq->headers);
}

const char*
NSAPIConnector::getAuthorization(void)
{
    return pblock_findkeyval(pb_key_authorization, _rq->headers);
}

const char*
NSAPIConnector::getProxyJroute(void)
{
    return pblock_findkeyval(pb_key_proxy_jroute, _rq->headers);
}

const char*
NSAPIConnector::getHeader(const char* name)
{
    return pblock_findval(name, _rq->headers);
}

const char*
NSAPIConnector::getRemoteAddr(void)
{
    return pblock_findkeyval(pb_key_ip, _sn->client);
}


const char*
NSAPIConnector::getRemoteHost(const char* ip)
{
    char* rhp = session_dns(_sn);
    if (rhp != NULL) {
        const char* val = STRDUP(rhp);
        return val;
    }
    return ip;      // default to value returned by getRemoteAddr()
}

int
NSAPIConnector::getRemotePort()
{
    return PR_NetAddrInetPort(_sn->pr_client_addr);
}

char*
NSAPIConnector::getLocalAddr(void)
{
    char buffer[PR_NETDB_BUF_SIZE];

    if (PR_NetAddrToString(_sn->pr_local_addr,buffer,sizeof(buffer))
        == PR_SUCCESS)
    {
        return STRDUP(buffer);
    }
    else {
        // What should we return on failure?
        return STRDUP("127.0.0.1");
    }
}

int
NSAPIConnector::getLocalPort()
{
    return PR_htons(PR_NetAddrInetPort(_sn->pr_local_addr));
}


const Request*
NSAPIConnector::getRequest()
{
   return _rq;
}

const char *
NSAPIConnector::getCoreUser()
{
    return pblock_findkeyval(pb_key_auth_user, _rq->vars);
}

void
NSAPIConnector::setAuthUser(const char* user)
{
    if (user != NULL) {
        const char* u = pblock_findkeyval(pb_key_auth_user, _rq->vars);
        if (u == NULL) {
            pblock_kvinsert(pb_key_auth_user, user, strlen(user)
                            , _rq->vars);
        }
    }
}

const char*
NSAPIConnector::getResponseHeader(const char* name)
{
    int i;

    // N.B. HttpServletResponse.getHeader() wants the value that was added
    // first, but pblock_findval() returns the most recently added value
    for (i = 0; i < _rq->srvhdrs->hsize; i++) {
        pb_entry *p;
        for (p = _rq->srvhdrs->ht[i]; p != NULL; p = p->next) {
            if (!strcasecmp(p->param->name, name))
                return p->param->value;
        }
    }

    return NULL;
}

jobjectArray
NSAPIConnector::getResponseHeaderNames(JNIEnv* env)
{
    int i;

    char** names = (char**) MALLOC(sizeof(char*) * _rq->srvhdrs->hsize);
    int n = 0;

    for (i = 0; i < _rq->srvhdrs->hsize; i++) {
        pb_entry *p;
        for (p = _rq->srvhdrs->ht[i]; p != NULL; p = p->next) {
            int j;
            for (j = 0; j < n; j++) {
                if (!strcasecmp(p->param->name, names[j]))
                    break;
            }
            if (j == n)
                names[n++] = p->param->name;
        }
    }

    jobjectArray jarray = NSJavaUtil::createStringArray(env, n);
    for (i = 0; i < n; i++)
        NSJavaUtil::setStringElement(env, jarray, i, names[i]);

    FREE(names);

    return jarray;
}

jobjectArray
NSAPIConnector::getResponseHeaderValues(JNIEnv* env, jstring jname)
{
    int i;

    const char* name = NULL;
    if (jname != NULL)
        name = env->GetStringUTFChars(jname, NULL);

    char** values = (char**) MALLOC(sizeof(char*) * _rq->srvhdrs->hsize);
    int n = 0;

    for (i = 0; i < _rq->srvhdrs->hsize; i++) {
        pb_entry *p;
        for (p = _rq->srvhdrs->ht[i]; p != NULL; p = p->next) {
            if (!strcasecmp(p->param->name, name))
                values[n++] = p->param->value;
        }
    }

    jobjectArray jarray = NSJavaUtil::createStringArray(env, n);
    for (i = 0; i < n; i++)
        NSJavaUtil::setStringElement(env, jarray, i, values[i]);

    FREE(values);

    if (jname != NULL)
        env->ReleaseStringUTFChars(jname, name);

    return jarray;
}

void
NSAPIConnector::setResponseHeader(const char* name, const char* value, PRBool doReplace)
{
    if (name == NULL)
        return;

    char buf[1024];   // FIXME

    convertNSAPIHeader(name, buf, sizeof(buf));

    if (doReplace) {
        pb_param *pp;
        while ((pp = pblock_remove(buf, _rq->srvhdrs)) != NULL)
            param_free(pp);
    }

    if (value != NULL)
        pblock_nvinsert(buf, value, _rq->srvhdrs);
}

void
NSAPIConnector::resetResponseHeaders()
{
    for(int x = 0; x < _rq->srvhdrs->hsize; x++) {
        pb_entry *p = _rq->srvhdrs->ht[x];
        while (p) {
            pb_param *pp;
            if ((pp = pblock_remove(p->param->name, _rq->srvhdrs)) != NULL)
                param_free(pp);
            p = p->next;
        }
    }
}

void
NSAPIConnector::setStatus(int status, const char* msg)
{
    char* value = pblock_findkeyval(pb_key_content_type, _rq->srvhdrs);
    if (value && !strncmp(value, "magnus-", 7))
        param_free(pblock_removekey(pb_key_content_type, _rq->srvhdrs));

    // set the status if one was explicitly specified or if no status
    // has been set before.
    if (status || !pblock_findkeyval(pb_key_status, _rq->srvhdrs)) {
        // if caller didn't explicitly specify a status, default to 200
        if (!status)
            status = PROTOCOL_OK;
        // if caller didn't specify a reason, use the server default
        if (msg && *msg == '\0')
            msg = NULL;
        protocol_status(_sn, _rq, status, (char*) msg);
    }
}

void
NSAPIConnector::setErrorDesc(const char* errorDesc)
{
    if (errorDesc) {
        pblock_kvreplace(pb_key_magnus_internal_webapp_errordesc,
                         errorDesc, strlen(errorDesc), _rq->vars);
    }
}

// The dorequest flag determines whether it should force the SSL handshake
// to reoccur to get a client cert if necessary. See lib/safs/clauth.cpp
const char* 
NSAPIConnector::getCertificate(int dorequest) {
    char* res = NULL;

    pblock* pb = pblock_create(4);
    if (dorequest) {
        pblock_nvinsert("dorequest", "1", pb);
    }
    // to get us a distiction between "no cert passed"
    // and some configuration error
    pblock_nvinsert("require", "0", pb);    

    // use get-client-cert SAF
    CA_getcert(pb, _sn, _rq);
    pblock_free(pb);
    res = pblock_findkeyval(pb_key_auth_cert, _rq->vars);
    return res;

}

const char* 
NSAPIConnector::getScheme(void)
{
    char* url = http_uri2url_dynamic("", "", _sn, _rq_orig);
    char* colon = strchr(url, ':');
    if (colon)
        *colon = '\0';
    return url;
}

void
NSAPIConnector::setSize(unsigned bytes)
{
    httpfilter_set_output_buffer_size(_sn, _rq, bytes);
}

int
NSAPIConnector::flushResponse(void)
{
    return net_flush(_sn->csd);
}

void
NSAPIConnector::finishResponse(void)
{
    http_finish_request(_sn, _rq);
}

size_t
NSAPIConnector::write(char *buf, size_t offset, size_t len)
{
    if (buf == NULL || len == 0)
        return 0;

    return net_write(_sn->csd, &buf[offset], len);
}

int
NSAPIConnector::read(char* bytes, int len)
{
    if (bytes == NULL || len <= 0)
        return  0;

    if (_inputStreamEOF)
        return -1;

    int bytesRead = 0;
    int rval;

    do {
        rval = netbuf_getbytes(_sn->inbuf, bytes, len);

        if (rval == NETBUF_EOF) {
            _inputStreamEOF = PR_TRUE;
            break;
        }

        if (rval == NETBUF_ERROR || rval < 0) {
            _inputStreamEOF = PR_TRUE;
            break;
        }

        bytesRead += rval;
        len   -= rval;
        bytes += rval;
    } while (rval > 0 && len > 0);
    
    return  bytesRead == 0 ? -1 : bytesRead;
}


// NSAPI

void
NSAPIConnector::setContentType(const char* ctype)
{
    pb_param* cp = pblock_removekey(pb_key_content_type, _rq->srvhdrs);
    if (cp != NULL)
        param_free(cp);

    if (ctype != NULL) {
        pblock_kvinsert(pb_key_content_type, ctype, strlen(ctype),
                        _rq->srvhdrs);
    }
}

void
NSAPIConnector::setContentLength(int clength)
{
    pb_param* cp = pblock_removekey(pb_key_content_length, _rq->srvhdrs);
    if (cp != NULL)
        param_free(cp);

    char buffer[256];
    PR_snprintf(buffer, sizeof(buffer), "%d", clength);
    pblock_kvinsert(pb_key_content_length, buffer, strlen(buffer),
                    _rq->srvhdrs);
}

int
NSAPIConnector::include(JNIEnv* env, jobject jres, jobject jos, jstring juri, jstring jquery)
{
    return dispatch(env, jres, NULL, jos, juri, jquery, NULL, PR_FALSE);
}

int
NSAPIConnector::forward(JNIEnv* env, jobject jres, jobject jis, jobject jos, jstring juri, jstring jquery, jstring jallow)
{
    return dispatch(env, jres, jis, jos, juri, jquery, jallow, PR_TRUE);
}

int
NSAPIConnector::dispatch(JNIEnv* env, jobject jres, jobject jis, jobject jos,
    jstring juri, jstring jquery, jstring jallow, PRBool forward)
{
    const char* uri = NULL;
    if (juri != NULL)
        uri = env->GetStringUTFChars(juri, NULL);

    // we can accelerate query-less includes provided the HttpServletResponse
    // has already been flushed
    // Java sets jres = null if we don't need to call 
    // HttpServletResponse.flushBuffer
    PRBool acceleratable = !jres && !jos && !jquery && !forward;
    if (_alwaysAccelerateIncludes)
        acceleratable = !forward; 
    if (acceleratable) {
        if (_accel == NULL) {
            const HttpRequest *hrq = HttpRequest::CurrentRequest();
            _accel = hrq != NULL ? hrq->GetAcceleratorHandle() : NULL;
        }
        if (accel_process_include(_accel, _sn->pool, _sn->csd,
                                  request_get_vs(_rq), uri)) {
            if (juri != NULL)
                env->ReleaseStringUTFChars(juri, uri);

            return PROTOCOL_OK;
        }
    }

    // throw an exception if we're in too deep
    IncrementRecursionDepth();
    int depth = GetCurrentRecursionDepth();
    if (depth > _maxDispatchDepth) {
        if (juri != NULL)
            env->ReleaseStringUTFChars(juri, uri);

        char* fmt = get_message(j2eeResourceBundle,
            "j2ee.NSAPIConnector.ERR_MAX_DISPATCH_DEPTH");
        char* msg = PR_smprintf(fmt, depth, _maxDispatchDepth);

        env->ThrowNew(_servletexceptionClass, msg);

        PR_Free(msg);
        FREE(fmt);

        DecrementRecursionDepth();

        return -1;
    }

    const char* query = NULL;
    if (jquery != NULL)
        query = env->GetStringUTFChars(jquery, NULL);

    const char* allow = NULL;
    if (jallow != NULL)
        allow = env->GetStringUTFChars(jallow, NULL);

    // preserve method for forwards
    const char *method = NULL;
    if (forward)
        method = pblock_findkeyval(pb_key_method, _rq->reqpb);

    // create a subrequest to execute the include
    Request *rq_sub = request_create_child(_sn, _rq, method, uri, query);

    // create a new session (with a new filter stack) for the subrequest
    Session *sn_sub = session_clone(_sn, rq_sub);

    // pass along existing headers for forwards
    if (forward) {
        pblock_free(rq_sub->srvhdrs);
        rq_sub->srvhdrs = pblock_dup(_rq->srvhdrs);
        pblock_removekey(pb_key_content_type, rq_sub->srvhdrs);
    }

    // we'd like to store the result in the accelerator cache
    if (acceleratable)
        accel_enable(sn_sub, rq_sub);

    // Tell ntrans-j2ee that the subrequest is the result of a J2EE-NSAPI
    // include/forward
    pblock_kvinsert(pb_key_magnus_internal_j2ee_nsapi, "1", 1, rq_sub->vars);

    // create a J2EE-NSAPI filter context
    J2EEFilterContext* context;
    context = j2eefilter_create_context(_sn, _rq, env, jres, jis, jos, forward);

    // insert a filter layer that will write to the OutputStream
    filter_insert(NULL, NULL, sn_sub, rq_sub, context, _j2eefilter);

    // process the subrequest
    // XXX case sensitivity security vulnerability CR 6324431
    int res = servact_handle_processed(sn_sub, rq_sub);

    if (jres != NULL) {
        // If nobody could handle the method, list the methods we do support
        if (res == REQ_NOACTION) {
            if (allow)
                pblock_nvinsert("allow", allow, rq_sub->srvhdrs);
            res = servact_nowaytohandle(sn_sub, rq_sub);
        }
    }

    // store the result in the accelerator cache if possible
    if (acceleratable)
        accel_store(sn_sub, rq_sub);

    // check the result of the subrequest
    int status_num = rq_sub->status_num;
    if (res != REQ_PROCEED && status_num == PROTOCOL_OK)
        status_num = PROTOCOL_SERVER_ERROR;
    if (status_num < 100 || status_num > 999)
        status_num = PROTOCOL_SERVER_ERROR;

    // destroy the subrequest, its session, and the associated filter stack
    request_free(rq_sub);
    session_free(sn_sub);

    if (juri != NULL)
        env->ReleaseStringUTFChars(juri, uri);

    if (jquery != NULL)
        env->ReleaseStringUTFChars(jquery, query);

    if (jallow != NULL)
        env->ReleaseStringUTFChars(jallow, allow);

    // if a Java exception occurred during the request, we want it thrown upon
    // our return to Java land
    jthrowable throwable = j2eefilter_retrieve_throwable(context);
    if (throwable)
        env->Throw(throwable);

    // destroy the J2EE-NSAPI filter context
    j2eefilter_destroy_context(context);

    DecrementRecursionDepth();

    return status_num;
}

void
NSAPIConnector::log(int level, const char* msg)
{
    NSAPIConnector* connector = NULL;
    if (_activeSlot != -1) {
        connector = (NSAPIConnector*)
            session_get_thread_data(NULL, _activeSlot);
    }

    const char* fn = NULL;
    Session* sn = NULL;
    Request* rq = NULL;
    if (connector) {
        fn = "service-j2ee";
        sn = connector->_sn;
        rq = connector->_rq;
    }

    log_error(level, fn, sn, rq, "%s", msg);
}


void
NSAPIConnector::throwIllegalStateException(JNIEnv* env)
{
    char* msg = get_message(j2eeResourceBundle,
                            "j2ee.NSAPIConnector.ERR_ILLEGAL_THREAD_SCOPE");
    env->ThrowNew(_illegalstateexceptionClass, msg);
    FREE(msg);
}

// ------- JNI functions

JNIEXPORT void JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniSetSize(
    JNIEnv* env,jobject obj, jlong jniConnector, jint size)
{
    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return;
    }

    connector->setSize(size);
}

JNIEXPORT jint JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniFlushResponse(
    JNIEnv* env, jobject obj, jlong jniConnector)
{
    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return -1;
    }

    return connector->flushResponse();
}

JNIEXPORT void JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniFinishResponse(
    JNIEnv* env, jobject obj, jlong jniConnector)
{
    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return;
    }

    connector->finishResponse();
}

JNIEXPORT void JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniResetResponseHeaders(
    JNIEnv* env, jobject obj, jlong jniConnector)
{
    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return;
    }

    connector->resetResponseHeaders();
}

JNIEXPORT jint JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniWrite(
    JNIEnv* env,
    jobject obj, jlong jniConnector, jbyteArray jbuf, jint off, jint len)
{
    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return -1;
    }

    // Try to pre allocate buffer on a 8k boundary
    char* ioBuffer = connector->getAllocatedIOBuffer(len);
    if (ioBuffer != NULL) {
        // We have a valid IO buffer, then use this buffer
        jbyte* bp = (jbyte*)ioBuffer;
        env->GetByteArrayRegion(jbuf, off, len, bp);
        return connector->write((char*)bp, 0, len);
    } else {
        // Looks like PERM_ALLOC has failed for some reason
        // Use the conventional way to transfer bytes from Java land
        jbyte* bp = env->GetByteArrayElements(jbuf, NULL);
        jint rval = connector->write((char*)bp, off, len);
        env->ReleaseByteArrayElements(jbuf, bp, 0);
        return rval;
    }
}


// inputstream/read

JNIEXPORT jint JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniRead(
    JNIEnv* env,
    jobject obj, jlong jniConnector, jbyteArray jbuf, jint off, jint len)
{
    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return -1;
    }

    int bufsize = env->GetArrayLength(jbuf);
    jint rval = -1;
    if (off + len <= bufsize) {
        // Try to pre allocate buffer on a 8k boundary
        char* ioBuffer = connector->getAllocatedIOBuffer(len);
        if (ioBuffer != NULL) {
            // We have a valid IO buffer, this buffer.
            jbyte* bp = (jbyte*)ioBuffer;
            rval = connector->read(ioBuffer, len);
            if (rval > 0)
                env->SetByteArrayRegion(jbuf, off, rval, bp);
        } else {
            // Looks like PERM_ALLOC has failed for some reason
            // Use the conventional way to transfer bytes from Java land
            jbyte* bp = env->GetByteArrayElements(jbuf, NULL);
            rval = connector->read((char*)(&bp[off]), len);
            env->ReleaseByteArrayElements(jbuf, bp, 0);
        }
    }
    return rval;
}



JNIEXPORT void JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniSetAllResponseFields(
    JNIEnv* env, jobject obj, jlong jniConnector, jint jstatus,
    jstring jmessage, jstring jcontentType, jint content_length,
    jstring juser,
    jboolean jerror, jobjectArray jresponseStrs, jint jnumStrs)
{
    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return;
    }

    const char *str  = NULL;
    char name[1024];
    char value[1024];

    if (jmessage != NULL)
        str = env->GetStringUTFChars(jmessage , NULL);    

    // Some CTS tests expect the sendError string in the Reason field of the 
    // HTTP Status line 
    connector->setStatus(jstatus, str);

    if (jerror)
        connector->setErrorDesc(str);

    if (jmessage != NULL)
        env->ReleaseStringUTFChars(jmessage , str);
        
    if (jcontentType != NULL) {
        str = env->GetStringUTFChars(jcontentType , NULL);    
        connector->setContentType(str);
        env->ReleaseStringUTFChars(jcontentType , str);

    }

    if (juser != NULL) {
        str = env->GetStringUTFChars(juser, NULL);
        connector->setAuthUser(str);
        env->ReleaseStringUTFChars(juser , str);
    }

    if (content_length >= 0) {
        connector->setContentLength(content_length);
    }

    //
    // Process the data in the JNI buffers only if there is header
    // data to process
    //
    if ((jresponseStrs != NULL) && (jnumStrs > 0)) {
        jstring jname;
        jstring jvalue;
        const char* name;
        const char* value;
        for (jsize i = 0; i < jnumStrs; i+= 2) {
            // Initialize/reset the values
            jname = NULL;
            jvalue = NULL;
            name = NULL;
            value = NULL;

            jname = (jstring) env->GetObjectArrayElement(jresponseStrs, i);
            if (jname != NULL)
                name = env->GetStringUTFChars(jname, NULL);

            jvalue = (jstring) env->GetObjectArrayElement(jresponseStrs, i+1);
            if (jvalue != NULL)
                value = env->GetStringUTFChars(jvalue, NULL);

            if (name != NULL)
                connector->setResponseHeader(name, value, PR_FALSE);

            if (jvalue != NULL)
                env->ReleaseStringUTFChars(jvalue, value);

            if (jname != NULL)
                env->ReleaseStringUTFChars(jname, name);
        }
    }
}


void
NSAPIConnector::getRequestInfo(JNIEnv* env) 
{
    NSAPIRequest* nrq = (NSAPIRequest*)_rq;

    size_t index = 0;
    size_t byteindex = 0;

    // Request values passed as bytes
    NSJavaUtil::setByteArrayElement(env, _rqBytes, byteindex++, getRequestURI());
    const char* servletPath = NULL;
    const char* pathInfo = NULL;
    ServletResource* servletResource = (ServletResource*)nrq->servletResource;
    if (servletResource && servletResource->getServletWrapper()) {
        servletPath = pblock_findkeyval(pb_key_script_name, _rq->vars);
        pathInfo = pblock_findkeyval(pb_key_path_info, _rq->vars);
    }

    NSJavaUtil::setByteArrayElement(env, _rqBytes, byteindex++, servletPath);
    NSJavaUtil::setByteArrayElement(env, _rqBytes, byteindex++, pathInfo);
    NSJavaUtil::setByteArrayElement(env, _rqBytes, byteindex++, getQueryString());

    PRBool viaErrorSAF = (_rq_orig &&
                          pblock_findkeyval(pb_key_magnus_internal_error_j2ee,
                                            _rq_orig->vars));
    if (viaErrorSAF)
        NSJavaUtil::setByteArrayElement(env, _rqBytes, byteindex++, getErrorRequestURI());
    else
        NSJavaUtil::setByteArrayElement(env, _rqBytes, byteindex++, NULL);

    NSJavaUtil::setByteArrayElement(env, _rqBytes, byteindex++, getCookie());

    int isHttps = GetSecurity(_sn);
    index = 0;
    // Request values passed as UTF-8 strings
    const char* ip = getRemoteAddr();
    NSJavaUtil::setStringElement(env, _rqStrs, index++, ip);

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getProtocol());
    
    switch(_rq->method_num) {
        case METHOD_GET: 
            env->SetObjectArrayElement(_rqStrs, index++, _methodGET);
            break;
        case METHOD_POST: 
            env->SetObjectArrayElement(_rqStrs, index++, _methodPOST);
            break;
        default: 
            NSJavaUtil::setStringElement(env, _rqStrs, index++, getMethod());
    }


    NSJavaUtil::setStringElement(env, _rqStrs, index++, getServerName());

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getContentLength());

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getContentType());

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getAuthType());

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getClientCert());

    if (isHttps)
        NSJavaUtil::setStringElement(env, _rqStrs, index++, getHttpsCipher());
    else
        NSJavaUtil::setStringElement(env, _rqStrs, index++, NULL);

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getAuthorization());

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getProxyJroute());

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getRemoteHost(ip));

    NSJavaUtil::setStringElement(env, _rqStrs, index++, getCoreUser());

    //----- Servlet 2.4
    char *pLocalAddr = getLocalAddr();
    NSJavaUtil::setStringElement(env, _rqStrs, index++, pLocalAddr);
    if (pLocalAddr != NULL) {
        FREE(pLocalAddr);
        pLocalAddr = NULL;
    }
    //----- Servlet 2.4
    WebModule* wm = (WebModule*) nrq->webModule;
    if (wm->useRequestEncforHeaders()) {
        byteindex = processHeadersBytes(env, _rqBytes, byteindex);
    } else {
       index = processHeaders(env, _rqStrs, index); 
       NSJavaUtil::setStringElement(env, _rqStrs, index++, NULL);
    }

    index = processParams(env, _rqStrs, index); 

    NSJavaUtil::setStringElement(env, _rqStrs, index++, NULL);

    // Fill in port and isSecure
    jint intValues[NUM_INT_HEADERS];
    intValues[0] = GetServerPort(_rq);
    intValues[1] = isHttps;
    int httpsKeySize = 0;
    if (isHttps)
        httpsKeySize = getHttpsKeySize();
    intValues[2] = httpsKeySize;
    intValues[3] = _outputStreamSize;
    if (viaErrorSAF)
        intValues[4] = getErrorStatusCode();
    else
        intValues[4] = -1;

    //----- Servlet 2.4
    intValues[5] = getRemotePort();
    intValues[6] = getLocalPort();
    //----- Servlet 2.4

    if (!env->ExceptionOccurred())
        env->SetIntArrayRegion(_rqInts, 0, NUM_INT_HEADERS, intValues);
}

PRBool NSAPIConnector::isAttached()
{
    return _isAttached;
}

JNIEXPORT jstring JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniGetCertificate(
    JNIEnv* env, jobject obj, jlong jniConnector, jboolean doRequire) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return NULL;
    }

    jstring rval = NULL;
    const char* res = NULL;
    res = connector->getCertificate((int)doRequire);
    if (res != NULL) {
        rval = env->NewStringUTF (res);
  
    }
    return rval;
}

JNIEXPORT jstring JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniGetScheme(
    JNIEnv* env, jobject obj, jlong jniConnector) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return NULL;
    }

    jstring rval = NULL;
    const char* res = NULL;
    res = connector->getScheme();
    if (res != NULL)
        rval = env->NewStringUTF(res);
    return rval;
}

JNIEXPORT jint JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniInclude(
    JNIEnv* env, jobject obj, jlong jniConnector, jobject jres, jobject jos,
    jstring juri, jstring jquery) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return -1;
    }

    return connector->include(env, jres, jos, juri, jquery);
}

JNIEXPORT jint JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniForward(
    JNIEnv* env, jobject obj, jlong jniConnector, jobject jres, jobject jis,
    jobject jos, jstring juri, jstring jquery, jstring jallow) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return -1;
    }

    return connector->forward(env, jres, jis, jos, juri, jquery, jallow);
}

JNIEXPORT jstring JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniGetResponseHeader(
    JNIEnv* env, jobject obj, jlong jniConnector, jstring jname) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return NULL;
    }

    const char* name = NULL;
    if (jname != NULL)
        name = env->GetStringUTFChars(jname, NULL);

    jstring jvalue = NULL;
    const char *value = connector->getResponseHeader(name);
    if (value != NULL)
        jvalue = env->NewStringUTF(value);

    if (jname != NULL)
        env->ReleaseStringUTFChars(jname, name);

    return jvalue;
}

JNIEXPORT jobjectArray JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniGetResponseHeaderNames(
    JNIEnv* env, jobject obj, jlong jniConnector) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return NULL;
    }

    return connector->getResponseHeaderNames(env);
}

JNIEXPORT jobjectArray JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniGetResponseHeaderValues(
    JNIEnv* env, jobject obj, jlong jniConnector, jstring jname) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return NULL;
    }

    return connector->getResponseHeaderValues(env, jname);
}

JNIEXPORT void JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniSetResponseHeader(
    JNIEnv* env, jobject obj, jlong jniConnector, jstring jname,
    jstring jvalue, jboolean doReplace) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return;
    }

    const char* name = NULL;
    if (jname != NULL)
        name = env->GetStringUTFChars(jname, NULL);

    const char* value = NULL;
    if (jvalue != NULL)
        value = env->GetStringUTFChars(jvalue, NULL);

    connector->setResponseHeader(name, value, doReplace);

    if (jname != NULL)
        env->ReleaseStringUTFChars(jname, name);

    if (jvalue != NULL)
        env->ReleaseStringUTFChars(jvalue, value);
}

JNIEXPORT void JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniSetResponseHeaderBytes(
    JNIEnv* env, jobject obj, jlong jniConnector, jstring jname,
    jbyteArray jvalue, jboolean doReplace) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return;
    }

    const char* name = NULL;
    if (jname != NULL)
        name = env->GetStringUTFChars(jname, NULL);

    // value can be non-UTF8. So to support non UTF8 chars use
    // GetByteArrayElements instead of GetStringUTFChars

    jbyte *tmpval = NULL;
    char *value = NULL;
    if (jvalue != NULL) {
        tmpval = env -> GetByteArrayElements(jvalue, 0);
        int len = env -> GetArrayLength(jvalue);
        value = (char*) MALLOC(len + 1);

        // byte array is not null terminated
        strncpy(value, (const char*)tmpval, len);
        value[len] = '\0';
    }

    connector->setResponseHeader(name, value, doReplace);

    if (jname != NULL)
        env->ReleaseStringUTFChars(jname, name);

    if (tmpval != NULL)
        env -> ReleaseByteArrayElements (jvalue, tmpval, 0);

    FREE(value);
}

JNIEXPORT jbyteArray JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniGetEncodedRequestURI(
    JNIEnv* env, jobject obj, jlong jniConnector) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return NULL;
    }

    jstring rval = NULL;
    const char* uri = connector->getEncodedRequestURI();
    jbyteArray uribytes = NULL;
    if (uri != NULL) 
        uribytes = NSJavaUtil::createByteArray(env, uri, strlen(uri));
    return uribytes;
}

JNIEXPORT jstring JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIConnector_jniGenerateSessionId(
    JNIEnv* env, jobject obj, jlong jniConnector) {

    NSAPIConnector* connector = (NSAPIConnector*) jniConnector;

    // Setup the NSAPI environment for this thread
    NSAPIEnv nsapi(connector->_sn, connector->_rq,
                   connector->_parentThread);
    if (!nsapi.isValid()) {
        NSAPIConnector::throwIllegalStateException(env);
        return NULL;
    }

    // Generate a pseudo random number
    const int RANDOM_BUF_SIZE = 16;
    char random[RANDOM_BUF_SIZE];
    session_random(connector->_sn, random, RANDOM_BUF_SIZE);

    // Convert the buffer to hexadecimal digits
    const int RANDOM_STRING_LEN = RANDOM_BUF_SIZE * 2;
    char res[RANDOM_STRING_LEN + 1];
    for (int i = 0; i < RANDOM_BUF_SIZE; i++) {
        int hi = ((random[i] & 0xf0) >> 4);
        int lo = (random[i] & 0x0f);
        res[i*2] = (hi < 10) ? ('0' + hi) : ('A' + (hi - 10));
        res[i*2 + 1] = (lo < 10) ? ('0' + lo) : ('A' + (lo - 10));
    }
    res[RANDOM_STRING_LEN] = '\0';

    return env->NewStringUTF(res);
}

JNIEXPORT void JNICALL
Java_com_sun_webserver_logging_NSAPIServerHandler_log_1error(
    JNIEnv *env, jclass clazz, jint level, jstring jmsg) {

    const char *msg = NULL;

    if (jmsg != NULL)
        msg = env->GetStringUTFChars(jmsg, NULL);

    // A negative level indicates the message should be logged even if that
    // level is suppressed by default (i.e. caller does its own log level
    // checks)
    NSAPIConnector::log(-level, msg);

    if (jmsg != NULL)
        env -> ReleaseStringUTFChars (jmsg, msg);
}
