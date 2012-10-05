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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This file provides native support functions for the NativeRealm java
 * realm. See NativeLoginModule and NativeRealm for more background.
 *
 * ACL functions are in netsite/lib/libaccess
 *
 */

#include "com_iplanet_ias_security_auth_realm_webcore_NativeRealm.h"
#include "frame/conf.h"
#include "libaccess/genacl.h"

/* Used as params to ACL_IsUserInRole and ACL_Authenticate, see genacl.cpp */
#define GROUP_CHECK 2
#define LOG_ERROR 1


/*
 * user and password must contain the respective information
 * authdb may contain an auth-db name or be NULL
 *
 * returns PR_TRUE|PR_FALSE to indicate auth status
 *
 */
int nativerealm_call_acl(const char * user, const char * password,
                         const char * authdb)
{
    return ACL_Authenticate(user, password, conf_get_vs(), authdb, LOG_ERROR);
}


/*
 * user and group must contain the respective information
 * authdb may contain an auth-db name or be NULL
 *
 * returns PR_TRUE|PR_FALSE to indicate success status
 *
 */
int nativerealm_check_group(const char * user, const char * group,
                            const char * authdb)
{
    // in order for this to work the module which services this authdb
    // must support 'user-exists' getter
    return ACL_IsUserInRole(user, GROUP_CHECK, group, 
                            conf_get_vs(), authdb, LOG_ERROR);
}




/*
 * Class:     com_iplanet_ias_security_auth_realm_webcore_NativeRealm
 * Method:    nativeAuth
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_iplanet_ias_security_auth_realm_webcore_NativeRealm_nativeAuth
(JNIEnv *env, jclass clazz,
 jstring juser, jstring jpwd, jstring jauthdb)
{
    const char * user;
    const char * password;
    const char * authdb = NULL;
    jboolean ret = JNI_FALSE;

    // anything to verify? (authdb can be null)
    if (juser == NULL || jpwd == NULL) {
        return ret;
    }
    
    user = env->GetStringUTFChars(juser, NULL);
    password = env->GetStringUTFChars(jpwd, NULL);

    if (jauthdb != NULL) {
        authdb = env->GetStringUTFChars(jauthdb, NULL);
    }

    ret = nativerealm_call_acl(user, password, authdb);

    env->ReleaseStringUTFChars(juser, user);
    env->ReleaseStringUTFChars(jpwd, password);
    if (jauthdb != NULL) {
        env->ReleaseStringUTFChars(jauthdb, authdb);
    }
    
    return ret;
}


/*
 * Class:     com_iplanet_ias_security_auth_realm_webcore_NativeRealm
 * Method:    nativeCheckGroup
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_iplanet_ias_security_auth_realm_webcore_NativeRealm_nativeCheckGroup
(JNIEnv * env, jclass clazz,
 jstring juser, jstring jgroup, jstring jauthdb)
{
  const char * user;
  const char * group;
  const char * authdb = NULL;
  jboolean ret = JNI_FALSE;

  // anything to check? (authdb can be null)
  if (juser == NULL || jgroup == NULL) {
      return ret;
  }
  
  user = env->GetStringUTFChars(juser, NULL);
  group = env->GetStringUTFChars(jgroup, NULL);
  if (jauthdb != NULL) {
      authdb = env->GetStringUTFChars(jauthdb, NULL);
  }

  ret = nativerealm_check_group(user, group, authdb);

  env->ReleaseStringUTFChars(juser, user);
  env->ReleaseStringUTFChars(jgroup, group);
  if (jauthdb != NULL) {
      env->ReleaseStringUTFChars(jauthdb, authdb);
  }

  return ret;
}
