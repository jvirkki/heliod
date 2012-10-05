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

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <security/pam_appl.h>
#include <netsite.h>
#include <definesEnterprise.h>
#include <libaccess/pamauth.h>
#include <libaccess/aclerror.h>
#include <base/util.h>
#include <jni.h>
#include "com_iplanet_ias_security_auth_realm_solaris_SolarisRealm.h"


static int digits(int i);
static int converse(int num_msg, struct pam_message **msgm,
                    struct pam_response **response, void *appdata_ptr);

/* http://sac.sfbay.sun.com/PSARC/2002/053/contract-01 */
int _getgroupsbymember(const char *username, gid_t gid_array[],
                       int maxgids, int numgids);



/*-----------------------------------------------------------------------------
 * Do the actual authentication for the given user,password
 *
 * RETURN:
 *   0 = success
 *
 */
int pamauth_authenticate(const char *user, const char *password)
{
    struct pam_conv pam_conv;
    pam_handle_t *pamh = NULL;
    int ret;

    pam_conv.conv = converse;
    pam_conv.appdata_ptr = NULL;

    if ((ret = pam_start(PRODUCT_DAEMON_BIN, user, &pam_conv, &pamh))
        != PAM_SUCCESS) {
        pam_end(pamh, ret);
        return (ret);
    }

    /*
     * This doesn't work correctly in Solaris 8 due to bug 4393399, which
     * was fixed in Solaris 9.
     */
    if ((ret = pam_set_item(pamh, PAM_AUTHTOK, password)) != PAM_SUCCESS) {
        pam_end(pamh, ret);
        return (ret);
    }

    if ((ret = pam_authenticate(pamh, 0)) != PAM_SUCCESS) {
        pam_end(pamh, ret);
        return (ret);
    }

    if ((ret = pam_acct_mgmt(pamh, 0)) != PAM_SUCCESS) {
        pam_end(pamh, ret);
        return (ret);
    }

    /* If we get here, we're all ok */
    pam_end(pamh, ret);

    return (0);
}


/*-----------------------------------------------------------------------------
 * Obtain the groups for the given user.
 *
 * user: Name of user to check.
 * groups: Addr of ptr to group list. Will be allocated here and will
 *    contain a group name or a comma-separated list of group names.
 *
 * RETURNS:
 *  -1: on failure
 *
 */
int pamauth_get_groups_for_user(const char *user, char **groups)
{
    struct group *gr;
    struct passwd *pw = NULL;
    gid_t *gids = NULL;
    int ngroups, ngroups_max, i;
    const int sizeq = 128;
    int bufsize = sizeq * 8;
    int pambufsize;
    int rv = -1;
    char numbuf[sizeq];
    char* tmp = NULL;
    char* pambuff = NULL;
    
    ngroups_max = sysconf(_SC_NGROUPS_MAX);
    if (ngroups_max <= 0) {
        /* zero is actually a perfectly good value for _SC_NGROUPS_MAX */
        return (-1);
    }
    
    /* get pw info */
    pambufsize = (int)sysconf(_SC_GETPW_R_SIZE_MAX);
    if ((pambuff = (char*)MALLOC(pambufsize)) == NULL) {
        return(-1);
    }
    
    if ((pw = (struct passwd *)MALLOC(sizeof(struct passwd))) == NULL) {
        goto error;
    }
    
    util_getpwnam(user, pw, pambuff, pambufsize);
    
    /* temp space for _getgroupsbymember() call below */
    if ((gids = CALLOC((uint_t)ngroups_max * sizeof (gid_t))) == NULL) {
        goto error;
    }
    
    gids[0] = pw->pw_gid;
    ngroups = _getgroupsbymember(user, gids, ngroups_max, 1);
    
    /* print each group (and its gid) to comma-separated list into *groups */
    if ((*groups = (char *)MALLOC(bufsize)) == NULL) {
        goto error;
    }
    (*groups)[0] = 0;

    for (i = 0; i < ngroups; i++) {
        if ((gr = getgrgid(gids[i]))) {
            /* check there is still enough space in *groups */
            if (strlen(*groups) + strlen(gr->gr_name) + sizeq > bufsize) {
                bufsize += strlen(gr->gr_name) + sizeq;
                if ((tmp = (char *)REALLOC(*groups, bufsize)) == NULL) {
                    tmp = *groups;
                    goto error;
                }
                *groups = tmp;
                tmp = NULL;
            }
            
            strcat(*groups, gr->gr_name);
            strcat(*groups, ",");
        }

        sprintf(numbuf, "%d,", (int)gids[i]);
        strcat(*groups, numbuf);
    }

    /* remove last , */
    i = strlen(*groups);
    if (i) {
        (*groups)[i-1] = 0;
    }
    
    /* return # of groups found */
    rv = ngroups * 2;
    
error:

    if (pambuff) FREE(pambuff);
    if (pw) FREE(pw);
    if (gids) FREE(gids);
    if (tmp) FREE(tmp);

    return(rv);
}


/*-----------------------------------------------------------------------------
 * Check if the given user exists.
 *
 * Returns:
 *   LAS_EVAL_FAIL: error
 *   LAS_EVAL_FALSE:: user does not exist
 *   LAS_EVAL_TRUE: user exists
 *
 */
int pamauth_userexists(const char* user)
{
    int bufsize, rv;
    struct passwd * pw;
    char* buffer;
    
    bufsize = (int)sysconf(_SC_GETPW_R_SIZE_MAX);
    if ((buffer = (char*)MALLOC(bufsize)) == NULL) {
        return(LAS_EVAL_FAIL);
    }
    
    if ((pw = (struct passwd *)MALLOC(sizeof(struct passwd))) == NULL) {
        return(LAS_EVAL_FAIL);
    }
    
    if (util_getpwnam(user, pw, buffer, bufsize)) {
        rv = LAS_EVAL_TRUE;
    } else {
        rv =LAS_EVAL_FALSE;
    }

    FREE(pw);
    FREE(buffer);

    return(rv);
}


/*-----------------------------------------------------------------------------
 * PAM conversation function
 *
 */
static int converse(int num_msg, struct pam_message **msgm,
                    struct pam_response **response, void *appdata_ptr)
{
    int i;
    struct pam_response *reply;

    if (num_msg <= 0)
        return (PAM_CONV_ERR);

    if ((reply = calloc(num_msg, sizeof (struct pam_response))) == NULL) {
        return (PAM_CONV_ERR);
    }

    for (i = 0; i < num_msg; i++) {
        switch (msgm[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
        case PAM_MSG_NOCONF:
        case PAM_CONV_INTERRUPT:
        default:
            break;
        }
        reply[i].resp_retcode = 0;
        reply[i].resp = "";
    }

    *response = reply;

    return (PAM_SUCCESS);
}


/*-----------------------------------------------------------------------------
 * Internal util, count digits.
 *
 */
static int digits(int i)
{
    int digits = 0;

    while (i /= 10)
        ++digits;

    return (digits + 1);
}


/*-----------------------------------------------------------------------------
 * JNI calls pam_get_groups_for_user through here. Sets up Java array with
 * return data.
 *
 */
static jobjectArray JNICALL java_GetGroups(JNIEnv *env, const char *user)
{
    char *groups;
    char *s;
    int i, ngroups, grouplen, groupnum;
    jstring js;
    jobjectArray jgroups;

    if ((ngroups = pamauth_get_groups_for_user(user, &groups)) < 1) {
        return (NULL);
    }

    if (!groups) {
        return (NULL);
    }
    
    jgroups = (*env)->NewObjectArray(env, ngroups,
                                     (*env)->FindClass(env,"java/lang/String"),
                                     (*env)->NewStringUTF(env, ""));
    s = groups;
    grouplen = strlen(groups);
    groupnum = 0;
    
    for (i = 0; i < grouplen; i++) {
        if (groups[i] == ',') {
            groups[i] = 0;
            js = (*env)->NewStringUTF(env, s);
            (*env)->SetObjectArrayElement(env, jgroups, groupnum++, js);
            s = groups + i + 1;
        }
    }
    js = (*env)->NewStringUTF(env, s);
    (*env)->SetObjectArrayElement(env, jgroups, groupnum++, js);
    
    FREE(groups);

    return (jgroups);
}


/*-----------------------------------------------------------------------------
 * JNI entry points below.
 *
 */


/*
 * Class:     Auth
 * Method:    nativeAuthenticate
 * Signature: (Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL
Java_com_iplanet_ias_security_auth_realm_solaris_SolarisRealm_nativeAuthenticate(JNIEnv *env, jclass class, jstring juser, jstring jpassword)
{
    const char *user, *password;
    jobjectArray jgroups;

    user = (*env)->GetStringUTFChars(env, juser, NULL);
    password = (*env)->GetStringUTFChars(env, jpassword, NULL);

    jgroups = NULL;
    /* Need to enforce that user name is not empty, or PAM will go
       into a loop calling converse() until we run out of memory. */
    if (user != NULL  && strlen(user) > 0) {
        if (!pamauth_authenticate(user, password)) {
            jgroups = java_GetGroups(env, user);
        }
    }

    (*env)->ReleaseStringUTFChars(env, juser, user);
    (*env)->ReleaseStringUTFChars(env, jpassword, password);

    return (jgroups);
}


/*
 * Class:     Auth
 * Method:    nativeGetGroups
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL
Java_com_iplanet_ias_security_auth_realm_solaris_SolarisRealm_nativeGetGroups(JNIEnv *env, jclass class, jstring juser)
{
    const char *user;
    jobjectArray jgroups;

    user = (*env)->GetStringUTFChars(env, juser, NULL);
    jgroups = java_GetGroups(env, user);

    (*env)->ReleaseStringUTFChars(env, juser, user);

    return (jgroups);
}
