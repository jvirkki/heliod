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

#include "NSJavaUtil.h"
#include "com_sun_webserver_connector_nsapi_NSAPIConnector.h"
#include "NSAPIConnector.h"

#include "netsite.h"
#include "base/util.h"
#include "frame/conf.h"
#include "frame/log.h"
#include "base/util.h"

#include <prinrval.h>

#include <support/SimpleHash.h>

#include <nstime/nstime.h>

#define JEXCEPTION_CLASSNAME    "com/sun/webserver/util/JException"
#define GET_STACK_TRACE_METHOD   JEXCEPTION_CLASSNAME "/getStackTrace"

jclass  NSJavaUtil::_stringClass = NULL;
jclass  NSJavaUtil::_byteClass   = NULL;

void*
NSJavaUtil::getArgs(JNIEnv* env, jobject obj, const char* name)
{
    jfieldID fid = env->GetFieldID(env->GetObjectClass(obj), name, NS_ARG_TYPE);
    jlong jval = env -> GetLongField (obj, fid);

    return (void *)jval;
}

void *
NSJavaUtil::getArgs (JNIEnv *env, jobject obj, jfieldID fid)
{
    jlong jval = env -> GetLongField (obj, fid);
    return (void *)jval;
}

jclass
NSJavaUtil::loadClass (JNIEnv *env, const char *path, const char *name)
{
    jclass  clazz;
    char *ep = NULL, *sp = (char *)name;

    while ((sp = strchr (sp, '.')) != NULL) ep = ++sp;  // find the last '.' in the name

    if (ep != NULL && !strcmp (ep, "class"))
    {
        PRFileInfo  finfo;
        void    *classbuf;
        if (PR_GetFileInfo (path, &finfo) != PR_SUCCESS)
            return NULL;
        if (finfo.size > SERVLET_CLASS_MAXFILESIZE)
            return NULL;    // XXX log a message
        
        PRFileDesc *prf;
        if ((prf = PR_Open (path, PR_RDONLY, 0)) == NULL)
            return NULL;

        if ((classbuf = malloc (finfo.size)) == NULL)
        {
            PR_Close (prf);
            return NULL;
        }

        if (PR_Read (prf, classbuf, finfo.size) != finfo.size)
        {
            PR_Close (prf);
            free (classbuf);
            classbuf = NULL;
            return NULL;
        }
        PR_Close (prf);
        *ep = 0;
        clazz = env -> DefineClass (name, NULL, (jbyte *)classbuf, finfo.size);
        *ep = '.';
        if (clazz == NULL)
        {
            free (classbuf);
            classbuf = NULL;
            return NULL;
        }
    }
    else    
        clazz = env -> FindClass (name);

    return  clazz != NULL ? (jclass)env -> NewGlobalRef (clazz) : NULL;
}

jclass
NSJavaUtil::findClassGlobal (JNIEnv *env, const char *name)
{
    jclass clazz = env -> FindClass (name);

    return  clazz != NULL ? (jclass)env -> NewGlobalRef (clazz) : NULL;
}

jclass
NSJavaUtil::findStringClassGlobal (JNIEnv *env)
{
    if (_stringClass == NULL)
    {
        jclass clazz = env -> FindClass ("java/lang/String");
        if (clazz != NULL)
            _stringClass = (jclass)env -> NewGlobalRef (clazz);
    }
    return _stringClass;
}

jclass
NSJavaUtil::findByteClassGlobal (JNIEnv *env)
{
    if (_byteClass == NULL)
    {
        jbyteArray bao = env -> NewByteArray (1);

        if (bao != NULL)
        {
            jclass clazz = env -> GetObjectClass (bao);
            if (clazz != NULL)
                _byteClass = (jclass)env -> NewGlobalRef (clazz);
        }
    }
    return _byteClass;
}

jobjectArray
NSJavaUtil::createStringArray(JNIEnv *env, size_t arrsize)
{
    jclass clazz = findStringClassGlobal(env);

    if (clazz == NULL || env->ExceptionOccurred())
        return NULL;

    jobjectArray obj = env->NewObjectArray(arrsize, clazz, NULL);

    if (obj == NULL)
        return NULL;

    return obj;
}

jintArray
NSJavaUtil::createIntArray(JNIEnv *env, size_t arrsize)
{
    jintArray intarray = NULL;

    if (!env->ExceptionOccurred())
        intarray = env->NewIntArray(arrsize);

    return intarray;
}

jlongArray
NSJavaUtil::createLongArray(JNIEnv *env, size_t arrsize)
{
    jlongArray longarray = NULL;

    if (!env->ExceptionOccurred())
        longarray = env->NewLongArray(arrsize);

    return longarray;
}

jstring
NSJavaUtil::createString (JNIEnv *env, const char *buf)
{
    if (buf == NULL || env->ExceptionOccurred())
        return NULL;

    return env -> NewStringUTF (buf);
}

jbyteArray
NSJavaUtil::createByteArray(JNIEnv *env, const char *buf, size_t bufsize) {

    if (buf == NULL || bufsize == 0 || env->ExceptionOccurred())
        return NULL;

    jbyteArray obj = env -> NewByteArray (bufsize);

    if (obj != NULL)
        env -> SetByteArrayRegion (obj, 0, bufsize, (jbyte *)buf);

    return obj;
}


jobjectArray
NSJavaUtil::createStringBytesArray (JNIEnv *env, size_t arrsize)
{
    jclass clazz = findByteClassGlobal (env);

    if (clazz == NULL || env->ExceptionOccurred())
        return NULL;

    jobjectArray obj = env -> NewObjectArray (arrsize, clazz, NULL);
    
    return obj;
}

//
// Create a UTF string and set it as the index'th element in the object (string)
// array
//
void
NSJavaUtil::setStringElement(JNIEnv *env, jobjectArray jarr,
                             size_t index, const char* str)
{
    jstring utf_str = NULL;

    if (str != NULL) 
        utf_str = NSJavaUtil::createString(env, str);

    env->SetObjectArrayElement(jarr, index, utf_str);

    if (utf_str != NULL) 
        env->DeleteLocalRef(utf_str);
}

void
NSJavaUtil::setByteArrayElement(JNIEnv *env, jobjectArray jarr,
                               size_t index, const char* str)
{
    jbyteArray byte_arr = NULL;

    if (str != NULL) 
        byte_arr = NSJavaUtil::createByteArray(env, str, strlen(str));

    env->SetObjectArrayElement(jarr, index, byte_arr);

    if (byte_arr != NULL) 
        env->DeleteLocalRef(byte_arr);
}

jint*
NSJavaUtil::getJNIArrayElements(JNIEnv* env, jintArray jarr)
{
    if (jarr != NULL)
        return env->GetIntArrayElements(jarr, NULL);
    return NULL;
}

jlong*
NSJavaUtil::getJNIArrayElements(JNIEnv* env, jlongArray jarr)
{
    if (jarr != NULL)
        return env->GetLongArrayElements(jarr, NULL);
    return NULL;
}

jfloat*
NSJavaUtil::getJNIArrayElements(JNIEnv* env, jfloatArray jarr)
{
    if (jarr != NULL)
        return env->GetFloatArrayElements(jarr, NULL);
    return NULL;
}

void
NSJavaUtil::releaseJNIArrayElements(JNIEnv* env, jintArray jarr, jint* intArray)
{
    if (intArray != NULL)
        env->ReleaseIntArrayElements(jarr, intArray, 0);
}

void
NSJavaUtil::releaseJNIArrayElements(JNIEnv* env,
                                    jlongArray jarr, jlong* longArray)
{
    if (longArray != NULL)
        env->ReleaseLongArrayElements(jarr, longArray, 0);
}

void
NSJavaUtil::releaseJNIArrayElements(JNIEnv* env,
                                    jfloatArray jarr, jfloat* floatArray)
{
    if (floatArray != NULL)
        env->ReleaseFloatArrayElements(jarr, floatArray, 0);
}

void
NSJavaUtil::getStringAtIndex(JNIEnv* env, jobjectArray jobjArray, int index,
                             NSString& outString)
{
    jstring jStringVal = (jstring) env->GetObjectArrayElement(jobjArray,
                                                              index);
    if (jStringVal != NULL) {
        NSJStringLocalRef strVal(env, jStringVal);
        outString = (const char*) strVal;
    }
    else {
        outString = NULL;
    }
}

void
NSJavaUtil::log (int level, const char *fmt, ...)
{
    if (fmt !=  NULL)
    {
        va_list args;
        va_start  (args, fmt);

        ereport_v (level, fmt, args);
        va_end(args);
    }
}

PRBool
NSJavaUtil::isSingleProcess ()
{
    return conf_getglobals () -> Vpool_max <= 1;
}

/*
 * Class:     com_iplanet_server_http_util_isSingleProcess
 * Method:    isSingleProcess
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_webserver_connector_nsapi_NSAPIConnector_isSingleProcess
  (JNIEnv *env, jclass clazz)
{
    jboolean rval = NSJavaUtil::isSingleProcess ();

    return rval;
}

int
NSJavaUtil::init()
{
    return 1;
}

int
NSJavaUtil::java_init(JNIEnv* env)
{
    return 1;
}

jmethodID JException::_gmID = NULL;
jmethodID JException::_tsID = NULL;
jmethodID JException::_stID = NULL; // stack trace

jclass    JException::_jexc_cls = NULL;

JException::JException (JNIEnv *env, jthrowable obj)
{
    _env  =  env;

    _getMessage  = NULL;
    _jgetMessage = NULL;

    _toString    = NULL;
    _jtoString   = NULL;

    _getStackTrace  = NULL;
    _jgetStackTrace = NULL;

    if (obj == NULL)
    {
        _obj = _env -> ExceptionOccurred ();
        if (_obj != NULL)
        {
            _release_flag = 1;
            _env -> ExceptionClear ();
        }
    }
    else
    {
        _obj = obj;
        _release_flag = 0;
    }

    init (env);

    if (_obj != NULL)
    {
        if (_gmID != NULL)
        {
            _jgetMessage = (jstring) _env -> CallObjectMethod (_obj, _gmID);
            if (_jgetMessage != NULL)
                _getMessage = _env->GetStringUTFChars(_jgetMessage, NULL);
            _env -> ExceptionClear();
        }
        else
        {
            char* msg = get_message(j2eeResourceBundle,
                                    "j2ee.NSJavaUtil.ERR_FINDTHROWABLE");
            NSJavaUtil::log (LOG_FAILURE, msg, "java.lang.Throwable.getMessage()");
            FREE(msg);
        }

        if (_tsID != NULL)
        {
            _jtoString   = (jstring) _env -> CallObjectMethod (_obj, _tsID);
            if (_jtoString  != NULL)
                _toString = _env->GetStringUTFChars(_jtoString, NULL);
            _env -> ExceptionClear();
        }
        else
        {
            char* msg = get_message(j2eeResourceBundle,
                                    "j2ee.NSJavaUtil.ERR_FINDTHROWABLE");
            NSJavaUtil::log (LOG_FAILURE, msg, "java.lang.Throwable.toString()");
            FREE(msg);
        }

        if (_stID != NULL)
        {
            _jgetStackTrace  = (jstring) _env -> CallStaticObjectMethod (_jexc_cls, _stID, _obj);
            if (_jgetStackTrace != NULL)
                _getStackTrace = _env->GetStringUTFChars(_jgetStackTrace, NULL);
            _env -> ExceptionClear();
        }
        else
        {
            char* msg = get_message(j2eeResourceBundle,
                                    "j2ee.NSJavaUtil.ERR_FINDTHROWABLE");
            NSJavaUtil::log (LOG_FAILURE, msg, GET_STACK_TRACE_METHOD);
            FREE(msg);
        }

        if (_getMessage == NULL)
            _getMessage = "no description";

        if (_toString   == NULL)
            _toString   = "unknown exception";

        if (_getStackTrace == NULL)
            _getStackTrace = "no stack trace";
    }
}

void
JException::init (JNIEnv *env)
{
    if (_gmID == NULL)
    {
        jclass cls = env -> FindClass ("java/lang/Throwable");
        if (cls != NULL)
        {
            _gmID = env -> GetMethodID (cls, "getMessage", "()Ljava/lang/String;");
            _tsID = env -> GetMethodID (cls, "toString"  , "()Ljava/lang/String;");
            env -> DeleteLocalRef (cls);
        }
        else
        {
            char* msg = get_message(j2eeResourceBundle,
                                    "j2ee.NSJavaUtil.ERR_FINDTHROWABLE");
            NSJavaUtil::log (LOG_FAILURE, msg, "java/lang/Throwable class");
            FREE(msg);
        }

        if (_jexc_cls == NULL)
            _jexc_cls = NSJavaUtil::findClassGlobal (env, JEXCEPTION_CLASSNAME);

        if (_jexc_cls != NULL)
        {
            _stID = env -> GetStaticMethodID (_jexc_cls, "getStackTrace", "(Ljava/lang/Throwable;)Ljava/lang/String;");
        }
        else
        {
            char* msg = get_message(j2eeResourceBundle,
                                    "j2ee.NSJavaUtil.ERR_FINDTHROWABLE");
            NSJavaUtil::log (LOG_FAILURE, msg, JEXCEPTION_CLASSNAME);
            FREE(msg);
        }
    }
}

const char *
JException::getMessage ()
{
    return _getMessage;
}

const char *
JException::toString ()
{
    return _toString;
}

const char *
JException::getStackTrace ()
{
    return _getStackTrace;
}

int
JException::isThrown ()
{
    return _obj != NULL;
}

JException::~JException ()
{
    if (_obj != NULL)
    {
        if (_jgetMessage != NULL)
            _env -> ReleaseStringUTFChars (_jgetMessage, _getMessage);

        if (_jtoString != NULL)
            _env -> ReleaseStringUTFChars (_jtoString, _toString);

        if (_jgetStackTrace != NULL)
            _env -> ReleaseStringUTFChars (_jgetStackTrace, _getStackTrace);

        if (_release_flag)
        {
            _env -> DeleteLocalRef (_obj);
        }
    }
}

// ----------------------------------------------------------- ]

PRBool
NSJavaUtil::reportException (JNIEnv *env)
{
    JException jexc (env);

    if (jexc.isThrown ())
    {
        char* msg = get_message(j2eeResourceBundle,
                                "j2ee.NSJavaUtil.ERR_EXCEPTION");
        log (LOG_FAILURE, msg, jexc.toString (), jexc.getMessage (),
             jexc.getStackTrace ());
        FREE(msg);
        return PR_TRUE;
    }
    return PR_FALSE;
}


void
NSJavaUtil::logAssert (const char *msg, const char *s, const char *file, PRIntn ln)
{
    ereport (LOG_CATASTROPHE, (char*)"Assert condition (%s): %s, at %s:%d",
             msg != NULL ? msg : "", s, file, ln);
    _exit (1);
}

// ----------------------------------------------------------- ]

// NSJStringLocalRef members
NSJStringLocalRef::NSJStringLocalRef( JNIEnv* penv, 
                            jstring jobj) : 
              penv_(penv), jobj_(jobj)
{
    strptr_ = penv->GetStringUTFChars(jobj, NULL);
}

NSJStringLocalRef::~NSJStringLocalRef()
{
    if ( strptr_ )
        penv_->ReleaseStringUTFChars( jobj_, strptr_);
    if ( jobj_ )
        penv_->DeleteLocalRef ( jobj_ );
}

