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

#include "com_iplanet_server_http_session_MMapSessionManager.h"
#include "NSJavaUtil.h"
#include "MemMapSessionManager.h"

jfieldID id_fid = 0;
jfieldID lastAccessedTime_fid = 0;
jfieldID prevReqEndTime_fid = 0;
jfieldID maxInactiveInterval_fid = 0;
jfieldID creationTime_fid = 0;
jfieldID currentAccessTime_fid = 0;
jfieldID isValid_fid = 0;

/*
 * Class:     com_iplanet_server_http_session_MMapSessionManager
 * Method:    _cppInit
 * Signature: (Ljava/lang/String;IIIILjava/lang/String;I)J
 */
JNIEXPORT jlong JNICALL Java_com_iplanet_server_http_session_MMapSessionManager__1cppInit
  (JNIEnv *env, jobject obj, jstring dir, jint maxValueSize, jint maxSessions, jint maxVals,
   jint timeOut, jstring jsnContext, jint maxLocks)
{
//  PRBool  attach = NSJavaUtil:://cb_enter ();

  PRBool  success = PR_FALSE;

  const char* sessionDir = NULL;
  if (dir != NULL)
    sessionDir = env->GetStringUTFChars(dir, NULL);
  
  const char* snContext = NULL;
  if (jsnContext != NULL)
    snContext = env->GetStringUTFChars(jsnContext, NULL);
  
  MemMapSessionManager* native_mgr;

  // we store entire HttpSession attribute data in one chunk; so make the per
  // attribute maximum size to be the multiple of the max number of values and
  // value size.
  native_mgr = new MemMapSessionManager(sessionDir, (int)(maxValueSize * maxVals),
                                        (int) maxSessions, 1,
                                        (int) timeOut,
                                        snContext, (PRUint32) maxLocks);

  if (dir != NULL)
    env -> ReleaseStringUTFChars (dir, sessionDir);

  if (jsnContext != NULL)
    env -> ReleaseStringUTFChars (jsnContext, snContext);

//  NSJavaUtil::cb_leave (attach);
  return (jlong)native_mgr;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSessionManager
 * Method:    _cppDestroy
 * Signature: ()J
 */
JNIEXPORT void JNICALL
Java_com_iplanet_server_http_session_MMapSessionManager__1cppDestroy(
	JNIEnv	*env,
	jobject	obj,
    jlong nativeMgr)
{
//  PRBool  attach = NSJavaUtil::cb_enter ();

  MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
  if (native_mgr != NULL)
  {
    delete native_mgr;
  }

//  NSJavaUtil::cb_leave (attach);

  return;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSessionManager
 * Method:    _isSessionMangerUsable
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_iplanet_server_http_session_MMapSessionManager__1isSessionMangerUsable(
	JNIEnv	*env,
	jobject	obj,
    jlong nativeMgr)
{
//  PRBool  attach = NSJavaUtil::cb_enter ();

  jboolean rval = JNI_FALSE;
  MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
  if (native_mgr->isSessionManagerConfigured() == PR_TRUE)
    rval = JNI_TRUE;

//  NSJavaUtil::cb_leave (attach);

  return rval;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSessionManager
 * Method:    _isValidSession
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_com_iplanet_server_http_session_MMapSessionManager__1isValidSession
  (JNIEnv *env, jobject obj, jlong nativeMgr, jstring jname)
{
//  PRBool  attach = NSJavaUtil::cb_enter ();

  const char *name = NULL;
  if (jname != NULL)
    name = env->GetStringUTFChars(jname, NULL);

  jboolean rval = JNI_FALSE;
  MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
  if (native_mgr->isSessionValid (name) == PR_TRUE)
    rval = JNI_TRUE;

  if (jname != NULL)
    env -> ReleaseStringUTFChars (jname, name);

//  NSJavaUtil::cb_leave (attach);

  return rval;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSessionManager
 * Method:    _run_C_side_reaper
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_iplanet_server_http_session_MMapSessionManager__1run_1C_1side_1reaper
  (JNIEnv *env, jobject obj, jlong nativeMgr)
{
//  PRBool  attach = NSJavaUtil::cb_enter();
  MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
  int count = native_mgr->reaper();
//  NSJavaUtil::cb_leave(attach);

  return (jint)count;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSessionManager
 * Method:    _destroySession
 * Signature: (JLjava/lang/String;J)Z
 */
JNIEXPORT jboolean JNICALL Java_com_iplanet_server_http_session_MMapSessionManager__1destroySession
  (JNIEnv *env, jobject obj, jlong nativeMgr, jstring jid, jlong location)
{
//  PRBool  attach = NSJavaUtil::cb_enter ();

  const char *id = NULL;
  if (jid != NULL)
    id = env->GetStringUTFChars(jid, NULL);

  MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
  
  PRBool result = PR_TRUE;

  if (native_mgr->lockSession(id, location) == PR_TRUE) {
        result = native_mgr->deleteSessionAtLocation((PRUintn)location);
        native_mgr->unlockSession(id, location);
  }
  if (jid != NULL)
    env -> ReleaseStringUTFChars (jid, id);

//  NSJavaUtil::cb_leave (attach);
  
  return (jboolean)result;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSessionManager
 * Method:    _cacheSessionFieldReference
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_iplanet_server_http_session_MMapSessionManager__1cacheSessionFieldReference
  (JNIEnv *env, jobject obj)
{
//  PRBool  attach = NSJavaUtil::cb_enter();

  jclass clazz = env->FindClass("com/iplanet/server/http/session/MMapSession");
  id_fid = env->GetFieldID(clazz, "_id", "Ljava/lang/String;");

  lastAccessedTime_fid = env->GetFieldID(clazz, "_lastAccessedTime", "J");
  prevReqEndTime_fid = env->GetFieldID(clazz, "_prevReqEndTime", "J");
  maxInactiveInterval_fid = env->GetFieldID(clazz, "_maxInactiveInterval", "J");
  creationTime_fid = env->GetFieldID(clazz, "_creationTime", "J");
  currentAccessTime_fid = env->GetFieldID(clazz, "_currentAccessTime", "J");
  isValid_fid = env->GetFieldID(clazz, "_isValid", "Z");
  
//  NSJavaUtil::cb_leave (attach);
}

/*
 * Class:     com_iplanet_server_http_session_MMapSessionManager
 * Method:    _unlock
 * Signature: (JLjava/lang/String;J)Z
 */
JNIEXPORT jboolean JNICALL Java_com_iplanet_server_http_session_MMapSessionManager__1unlock
  (JNIEnv *env, jobject obj, jlong nativeMgr, jstring jid, jlong location)
{
//  PRBool  attach = NSJavaUtil::cb_enter();
    PRBool result = PR_TRUE;
    const char *id = NULL;
    if (jid != NULL)
        id = env->GetStringUTFChars(jid, NULL);

    MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
    result = native_mgr->unlockSession(id, location);
  
//  NSJavaUtil::cb_leave (attach);
    return (jboolean)result;
}
