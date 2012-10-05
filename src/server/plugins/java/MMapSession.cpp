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

#include "com_iplanet_server_http_session_MMapSession.h" 
#include "NSJavaUtil.h"
#include "MemMapSessionManager.h" 

extern jfieldID id_fid;
extern jfieldID lastAccessedTime_fid;
extern jfieldID prevReqEndTime_fid;
extern jfieldID maxInactiveInterval_fid;
extern jfieldID creationTime_fid;
extern jfieldID currentAccessTime_fid;
extern jfieldID isValid_fid;

/*
 * Class:     com_iplanet_server_http_session_MMapSession
 * Method:    _loadSessionData
 * Signature: (J[BJ)[B
 */
JNIEXPORT jbyteArray JNICALL Java_com_iplanet_server_http_session_MMapSession__1loadSessionData
  (JNIEnv *env, jobject obj, jlong nativeMgr, jbyteArray jid, jlong location)
{
//	PRBool  attach = NSJavaUtil::cb_enter (); 
	
	char id[IWS_MAX_SESSIONID_LEN];
    int  idlen = (PRUintn)env->GetArrayLength (jid);
    env->GetByteArrayRegion (jid, 0, idlen, (jbyte *)id);

    NS_JAVA_ASSERT(idlen < IWS_MAX_SESSIONID_LEN);

    id[idlen] = '\0';

	PRUintn size = 0;
	
    MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;

	char *buf = NULL;
    jbyteArray arr = NULL;

    time_t lastAccessedTime, creationTime, prevReqEndTime, currentAccessTime;
    long maxInactiveInterval;

    PRBool isValid = PR_TRUE;
        
    native_mgr->lockSession(id, location);
	PRBool res = native_mgr->getSessionDataAtLocation ("httpsession", 
                                                           buf, 
                                                           size, 
                                                           location,
                                                           lastAccessedTime,
                                                           prevReqEndTime,
                                                           maxInactiveInterval,
                                                           creationTime,
                                                           currentAccessTime,
                                                           isValid);
	if (res == PR_FALSE) 
	{ 
        native_mgr->unlockSession(id, location);
        env -> SetLongField(obj, isValid_fid, isValid);
		size = 0; 
#ifdef PRINT_DEBUG_INFO
			char* logMsg = get_message(j2eeResourceBundle,
                                                   "j2ee.MMapSession.ERR_LOAD_SESSION_DATA_FAILED");
			NSJavaUtil::log (LOG_WARN, logMsg, name);
			FREE(logMsg);
#endif
	}
    else {
        // set these fields on the java MMapSession object
        env -> SetLongField(obj, lastAccessedTime_fid, (jlong) lastAccessedTime);
        env -> SetLongField(obj, prevReqEndTime_fid, (jlong) prevReqEndTime);
        env -> SetLongField(obj, maxInactiveInterval_fid, (jlong) maxInactiveInterval);
        env -> SetLongField(obj, creationTime_fid, (jlong) creationTime);
        env -> SetLongField(obj, currentAccessTime_fid, (jlong) currentAccessTime);

        if (size > 0) {
	        arr = env->NewByteArray ((jint) size); 
            env -> SetByteArrayRegion (arr, 0, size, (jbyte *) buf);
        }

#ifdef PRINT_DEBUG_INFO
        char* logMsg = get_message(j2eeResourceBundle,
                                   "j2ee.MMapSession.INFO_LOAD_SESSION_DATA_SUCCESS");
        NSJavaUtil::log (LOG_WARN, logMsg, name);
        FREE(logMsg);
#endif
    }
	
//	NSJavaUtil::cb_leave (attach); 
	
	return arr; 
}

/*
 * Class:     com_iplanet_server_http_session_MMapSession
 * Method:    _saveSessionData
 * Signature: (J[B[BJJ)Z
 */
JNIEXPORT jboolean JNICALL Java_com_iplanet_server_http_session_MMapSession__1saveSessionData
  (JNIEnv *env, jobject obj, jlong nativeMgr, jbyteArray jid, jbyteArray val, jlong location, jlong maxInactiveInterval, jlong prevReqEndTime)
{
//	PRBool  attach = NSJavaUtil::cb_enter (); 
	PRBool  res = PR_FALSE;

    MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;

	char id[IWS_MAX_SESSIONID_LEN];
    int  idlen = (PRUintn)env->GetArrayLength (jid);

    NS_JAVA_ASSERT(idlen < IWS_MAX_SESSIONID_LEN);

    env->GetByteArrayRegion (jid, 0, idlen, (jbyte *)id);
    id[idlen] = '\0';

    PRUintn size = (PRUintn)env->GetArrayLength (val);
    int blockSize = native_mgr->getBlockSize();

	if ((size > 0) && (size <= blockSize)) // limit ??? 
	{
		char *buf = NULL;
        PRUintn prevSize = size;

		res = native_mgr->setSessionDataAtLocation( "httpsession", 
                                                    buf, 
                                                    prevSize, 
                                                    location,
                                                    (PRUintn)maxInactiveInterval,
                                                    (PRUintn)prevReqEndTime );

        // It doesn't matter what the previous size is.
        if (res)
		    env->GetByteArrayRegion (val, 0, size, (jbyte *)buf);
	}
	else
	{
	        char* logMsg = get_message(j2eeResourceBundle,
                                           "j2ee.MMapSession.ERR_SESSION_DATA_SIZE_EXCEEDED");
		NSJavaUtil::log (LOG_FAILURE, logMsg, size, blockSize);
		FREE(logMsg);
	}

    native_mgr->unlockSession(id, location);

//	NSJavaUtil::cb_leave (attach); 
    return (jboolean)res;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSession
 * Method:    _createSession
 * Signature: (J[BI)J
 */
JNIEXPORT jlong JNICALL Java_com_iplanet_server_http_session_MMapSession__1createSession
  (JNIEnv *env, jobject obj, jlong nativeMgr, jbyteArray jid, jint timeOut)
{
//	PRBool  attach = NSJavaUtil::cb_enter (); 
	PRBool  sessionCreated = PR_FALSE;

	char id[IWS_MAX_SESSIONID_LEN];
    int  idlen = (PRUintn)env->GetArrayLength (jid);

    NS_JAVA_ASSERT(idlen < IWS_MAX_SESSIONID_LEN);

    env->GetByteArrayRegion (jid, 0, idlen, (jbyte *)id);
    id[idlen] = '\0';

    time_t lastAccessedTime, creationTime;
    MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
    long ret = -1;

    PRUintn location;
    sessionCreated = native_mgr->createSession (id, (PRUintn) timeOut, 
                                                    location, lastAccessedTime, creationTime);
    if (sessionCreated) {
        ret = location;

        if (native_mgr->lockSession(id, location) == PR_TRUE) {
            env -> SetLongField(obj, lastAccessedTime_fid, (jlong) lastAccessedTime);
            env -> SetLongField(obj, creationTime_fid, (jlong) creationTime);
        }
    }

//	NSJavaUtil::cb_leave (attach); 
	
	return (jlong)ret;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSession
 * Method:    _getSession
 * Signature: (J[B)J
 */
JNIEXPORT jlong JNICALL Java_com_iplanet_server_http_session_MMapSession__1getSession
  (JNIEnv *env, jobject obj, jlong nativeMgr, jbyteArray jid)
{
//	PRBool  attach = NSJavaUtil::cb_enter (); 
    PRBool  sessionFound = PR_FALSE;

	char id[IWS_MAX_SESSIONID_LEN];
    int  idlen = (PRUintn)env->GetArrayLength (jid);

    NS_JAVA_ASSERT(idlen < IWS_MAX_SESSIONID_LEN);

    env->GetByteArrayRegion (jid, 0, idlen, (jbyte *)id);
    id[idlen] = '\0';
	
    PRUintn location;

	if (id)  
	{ 
        MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
		sessionFound = native_mgr->getSession (id, location);
	}
	
//	NSJavaUtil::cb_leave (attach); 
	
	return (sessionFound == PR_TRUE) ? (jlong) location : -1;
}

/*
 * Class:     com_iplanet_server_http_session_MMapSession
 * Method:    _invalidate
 * Signature: (J[B)J
 */
JNIEXPORT jboolean JNICALL Java_com_iplanet_server_http_session_MMapSession__1invalidate
  (JNIEnv *env, jobject obj, jlong nativeMgr, jbyteArray jid)
{
//	PRBool  attach = NSJavaUtil::cb_enter (); 
    PRBool  invalidated = PR_FALSE;

	char id[IWS_MAX_SESSIONID_LEN];
    int  idlen = (PRUintn)env->GetArrayLength (jid);

    NS_JAVA_ASSERT(idlen < IWS_MAX_SESSIONID_LEN);

    env->GetByteArrayRegion (jid, 0, idlen, (jbyte *)id);
    id[idlen] = '\0';
	
	if (id)  
	{ 
        MemMapSessionManager* native_mgr = (MemMapSessionManager*)nativeMgr;
		invalidated = native_mgr->invalidate (id);
	}
	
//	NSJavaUtil::cb_leave (attach); 
	
	return (jboolean)invalidated;
}
