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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "libadmin/resource.h"
#include "unicode/uloc.h"
#include "definesEnterprise.h"

NSAPI_PUBLIC const char* 
getServerLocale() {
    const char* lang = getenv("AdminLanguage"); 
    if (lang == NULL || strlen(lang) == 0)
        lang = uloc_getDefault();
    return lang;
}

/*
 * This method returns the resource bundle name including the complete
 * path
 */
NSAPI_PUBLIC char*
get_resource_file(const char* serverRoot, const char* propertiesFile) {
    char resfile[1024];
    char* resourcefile;

    if (serverRoot) {
#ifdef PRODUCT_RES_SUBDIR
        sprintf(resfile, "%s/" PRODUCT_RES_SUBDIR "/%s", serverRoot,
                                                         propertiesFile);
#else
        sprintf(resfile, "%s/lib/messages/%s", serverRoot, propertiesFile);
#endif
    } else {
        strcpy(resfile, propertiesFile);
    }
    resourcefile = STRDUP(resfile);
    return resourcefile;
}


NSAPI_PUBLIC UResourceBundle* 
open_resource_bundle(const char* resourceFile, const char* lang) {
    UResourceBundle* resource;
    UErrorCode errcode = U_ZERO_ERROR;

    resource = ures_open(resourceFile, lang, &errcode);
    if (!U_SUCCESS(errcode))
        resource = NULL;

    return resource;
}

NSAPI_PUBLIC void
close_resource_bundle(UResourceBundle* resBundle) {
    ures_close(resBundle);
}

static char*
UCharStrToUTF8(pool_handle_t* pool_handle, const UChar* ustr) {
    UErrorCode errcode = U_ZERO_ERROR;
    char* destStr = NULL;
    int32_t destLen;
    int32_t ustr_len = u_strlen(ustr); 
    int32_t destCapacity = 4 * ustr_len + 1;

    if (pool_handle == NULL)
        destStr = (char*)MALLOC(destCapacity);
    else
        destStr = (char*)pool_malloc(pool_handle, destCapacity);

    u_strToUTF8(destStr, destCapacity, &destLen, ustr, ustr_len, &errcode);
    if (U_SUCCESS(errcode))
        return destStr;

    return NULL;
}

NSAPI_PUBLIC char*
get_message(UResourceBundle* rsBundle, const char* key) {
    return pool_get_message(NULL, rsBundle, key);
}

NSAPI_PUBLIC char*
pool_get_message(pool_handle_t* pool_handle, UResourceBundle* rsBundle,
                 const char* key) {
    UErrorCode errcode = U_ZERO_ERROR;                               
    const UChar* resultMsg;
    int32_t resultMsgLen;
    char* resultMsgInUTF8 = NULL;

    if (rsBundle == NULL) {
        static const char* errMsg = "Internal Error: Null Resource Bundle";
        if (pool_handle == NULL)
            return STRDUP(errMsg);
        else
            return pool_strdup(pool_handle, errMsg);
    }

    resultMsg = ures_getStringByKey(rsBundle, key, &resultMsgLen, &errcode);
    if (U_SUCCESS(errcode))
        resultMsgInUTF8 = UCharStrToUTF8(pool_handle, resultMsg);
    else {
        if (pool_handle == NULL)
            resultMsgInUTF8 = STRDUP(key);
        else
            resultMsgInUTF8 = pool_strdup(pool_handle, key);
    }

    return resultMsgInUTF8;
}
