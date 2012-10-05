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

# if !defined (NS_JAVA_UTIL_HXX)
#define NS_JAVA_UTIL_HXX    1

#include <string.h>
#include <jni.h>

#include <prtypes.h>
#include <prtime.h>
#include <prio.h>
#include <private/pprthred.h>
#include <pratom.h>
#include "base/ereport.h"
#include "i18n.h"
#include "libadmin/resource.h"

#define SERVLET_CLASS_MAXFILESIZE   (1024 *256)

#if !defined (_NS_SERVLET_EXPORT)
#if  defined (XP_PC)
#define _NS_SERVLET_EXPORT  _declspec(dllexport)
#define _NS_SERVLET_IMPORT  _declspec(dllimport)
#else
#define _NS_SERVLET_EXPORT
#define _NS_SERVLET_IMPORT
#endif
#endif

#if ! defined (PATH_SEPARATOR)
#if defined (XP_PC)
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif
#endif

extern UResourceBundle* j2eeResourceBundle;
class NSString;

#define NS_ARG_TYPE "J"
#define NS_DYN_ARG  "_d_ptr"
#define NS_FIN_ARG  "_s_ptr"

#define NS_JAVA_ASSERT(_expr) \
    ((_expr)?((void)0):NSJavaUtil::logAssert(NULL, # _expr,__FILE__,__LINE__))
 

// the size of random generator buffer; this will be 64 number; increase it if you need
// stronger security

#define NS_RANDOM_SID_LENGTH    8

// extra 256 bytes are for any host-specific prefix used for sticky load balancing.
#define	IWS_MAX_SESSIONID_LEN	(256 + NS_RANDOM_SID_LENGTH + 120)

class	_NS_SERVLET_EXPORT	NSJavaUtil	{
public:

    NSJavaUtil (JNIEnv *env)
    {
        _env = env;
    }

    static  int java_init (JNIEnv *env);
    static  int init ();

    jstring createString (const char *buf)
    {
        return createString (_env, buf);
    }

    void *  getArgs   (jobject obj, const char *name = NS_DYN_ARG)
    {
        return getArgs (_env, obj, name);
    }

    void *  getArgs   (jobject obj, jfieldID fid)
    {
        return getArgs (_env, obj, fid);
    }

    jclass  loadClass (const char *path, const char *name)
    {
        return loadClass (_env, path, name);
    }

    jclass  findClassGlobal (const char *name)
    {
        return findClassGlobal (_env, name);
    }

    jclass  findStringClassGlobal ()
    {
        return findStringClassGlobal (_env);
    }

    static  void *  getArgs   (JNIEnv *env, jobject obj, const char *name = NS_DYN_ARG);
    static  void *  getArgs   (JNIEnv *env, jobject obj, jfieldID fid);
    static  jclass  loadClass (JNIEnv *env, const char *path, const char *name);

    static  jclass  findClassGlobal (JNIEnv *env, const char *name);
    static  jclass  findStringClassGlobal  (JNIEnv *env);
    static  jclass  findByteClassGlobal    (JNIEnv *env);
    static jobjectArray createStringArray(JNIEnv *env, size_t arrsize);
    static jintArray createIntArray(JNIEnv *env, size_t arrsize);
    static jlongArray createLongArray(JNIEnv *env, size_t arrsize);
    static  jstring createString  (JNIEnv *env, const char *buf);

    static  jbyteArray      createByteArray (JNIEnv *env, const char *buf, size_t bufsize);
    static  jobjectArray    createStringBytesArray (JNIEnv *env, size_t arrsize);   

    static void setStringElement(JNIEnv *env, jobjectArray jarr,
                                 size_t index, const char* str);
    static void setByteArrayElement(JNIEnv *env, jobjectArray jarr,
                                    size_t index, const char* str);
    static jint*   getJNIArrayElements(JNIEnv* env, jintArray jarr);
    static jlong*  getJNIArrayElements(JNIEnv* env, jlongArray jarr);
    static jfloat* getJNIArrayElements(JNIEnv* env, jfloatArray jarr);
    static void releaseJNIArrayElements(JNIEnv* env,
                                        jintArray jarr, jint* intArray);
    static void releaseJNIArrayElements(JNIEnv* env,
                                        jlongArray jarr, jlong* longArray);
    static void releaseJNIArrayElements(JNIEnv* env,
                                        jfloatArray jarr, jfloat* floatArray);

    static void getStringAtIndex(JNIEnv* env, jobjectArray jobjArray, int index,
                                 NSString& outString);

    static  void log (int log_level, const char *fmt, ...);

    static  PRBool  isSingleProcess ();
    static  PRBool  reportException (JNIEnv *env);

    static  PRBool  fixCacheVersion (const char *cache_dir, unsigned version);
    static  void    removeAllFiles  (const char *directory, const char *tagFile = NULL);

    static  void logAssert (const char *msg, const char *s, const char *file, PRIntn ln);


private:

    static  jclass  _stringClass;
    static  jclass  _byteClass;
    JNIEnv  *_env;
};


class   _NS_SERVLET_EXPORT  JException  {
public:
    JException (JNIEnv *env, jthrowable jobj = NULL);
    ~JException ();

    const char *getMessage ();
    const char *toString   ();
    const char *getStackTrace ();

    int isThrown ();

    static  void init (JNIEnv *env);

private:
    JNIEnv *    _env;
    jthrowable  _obj;
    jstring     _jgetMessage;
    jstring     _jtoString;
    jstring     _jgetStackTrace;

    const   char * _getMessage;
    const   char * _toString;
    const   char * _getStackTrace;

    int   _release_flag;

    static  jmethodID   _gmID;
    static  jmethodID   _tsID;
    static  jmethodID   _stID;  // stack trace
    static  jclass      _jexc_cls;
};

class   NSMonStruc;

#define NSMON_UPDATE_SELF       0
#define NSMON_UPDATE_PARENT     1
#define NSMON_UPDATE_CHILDREN   2

class   NSMonStruc  {
    char *_name;
    PRTime  _lastmodified;
    PRInt32 _is_modified;

    unsigned    _flag;  // A mode of operation: UPDATE_PARENT or UPDATE_CHILDREN
    NSMonStruc *_root;  // root dependant
    NSMonStruc *_next;  // next dependant
    NSMonStruc *_head;  // children

public:

    NSMonStruc (const char *filename, NSMonStruc *parent = NULL, unsigned flag = NSMON_UPDATE_SELF)
    {
        _name = strdup (filename);
        memset (&_lastmodified, 0, sizeof (_lastmodified));
        
        _flag = flag;

        _next = NULL;
        _root = NULL;
        _head = NULL;

        if (parent != NULL)
        {
            _root = parent;
            _next = _root -> _head;
            _root -> _head =  this;
        }

        _is_modified = 0;
        check ();
        _is_modified = 0;
    }

   ~NSMonStruc ()
    {
       if (_name != NULL)
           free (_name);
    }

    int isModified ()
    {
        int rval = PR_AtomicSet (&_is_modified, 0);
        return rval;
    }

    void check ()
    {
        if (_name != NULL && _is_modified == 0)
        {
            PRFileInfo fi;
            
            if (PR_GetFileInfo (_name, &fi) == PR_FAILURE)
                PR_AtomicSet (&_is_modified, 1);
            else 
                if (_lastmodified != fi.modifyTime)
            {
                update (fi.modifyTime);
            } /* PR_GetFileInfo */
        }
    } /* check */

    void update (PRTime newtime)
    {
        _lastmodified = newtime;
        PR_AtomicSet (&_is_modified, 1);

        if (_flag &  NSMON_UPDATE_PARENT )
        {
            if (_root != NULL)
                _root -> update (_lastmodified);
        }

        if (_flag & NSMON_UPDATE_CHILDREN)
        {
            for (NSMonStruc *p = _head; p != NULL; p = p -> _next)
                p -> update (_lastmodified);
        }
    } /* update */

    static  void remove (const char *name);
};


// NSJStringLocalRef simplifies the access and deletion of local 
// jstring references
class NSJStringLocalRef
{
    private:
        JNIEnv* penv_;
        jstring jobj_;
        const char* strptr_;

        // Prohibit assignment operator and copy constructor
        NSJStringLocalRef& operator=(const NSJStringLocalRef& );
        NSJStringLocalRef(const NSJStringLocalRef& );
    public:
        NSJStringLocalRef( JNIEnv* penv, jstring jobj);
        ~NSJStringLocalRef();
        operator const char*()
        {
            return strptr_;
        }

};

template <class JType, class JArrayType>
class NSJniArrayPtr
{
    private:
        JNIEnv* env_;
        JType*   nativeArray_;
        JArrayType jobjArray_;
    public:
        NSJniArrayPtr(JNIEnv* env, JArrayType array):
                        env_(env),
                        jobjArray_(array),
                        nativeArray_(NULL)
        {
            nativeArray_ = NSJavaUtil::getJNIArrayElements(env_, jobjArray_);
        }
        ~NSJniArrayPtr()
        {
            NSJavaUtil::releaseJNIArrayElements(env_, jobjArray_, nativeArray_);
        }
        operator JType*()
        {
            return nativeArray_;
        }
};

typedef NSJniArrayPtr<jint, jintArray> NSJIntArrayPtr;
typedef NSJniArrayPtr<jlong, jlongArray> NSJLongArrayPtr;
typedef NSJniArrayPtr<jfloat, jfloatArray> NSJFloatArrayPtr;

#endif

