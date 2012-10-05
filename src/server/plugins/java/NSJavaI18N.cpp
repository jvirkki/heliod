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

#include "base/ereport.h"
#include "frame/conf.h"
#include "i18n.h"

#include "NSJavaI18N.h"
#include "NSJavaUtil.h"
#include "definesEnterprise.h"

#include "unicode/ures.h"
#include "unicode/umsg.h"
#include "unicode/ustring.h"
#include "unicode/uloc.h"

#define FMT_MSG_LEN 4096
#define NB_OF_PARAMS 5

static UResourceBundle* icuResourceBundle = 0;
static int icu_initialized = 0;
static int icu_available   = 0;

char* ucs2strToUTF8(const UChar*);
const UChar* getConstUCS2String(UResourceBundle*, const char*);

PR_IMPLEMENT(char *)
rb_getProp(const char *name)
{
    if (icu_initialized && icu_available) {
        UErrorCode errcode = U_ZERO_ERROR;

        int32_t src_info_len;
        const UChar* src_info = ures_getStringByKey(icuResourceBundle, name,
                                                    &src_info_len, &errcode);
        if (U_SUCCESS(errcode)) {

            // assume utf-8 to be upto 4 bytes each
            int32_t destCapacity = 4 * src_info_len + 1;
            char* dest_info = (char*) malloc(destCapacity);

            int32_t destLen;
            u_strToUTF8(dest_info, destCapacity, &destLen, 
                        src_info, src_info_len, &errcode);

            if (U_SUCCESS(errcode))
                return dest_info;

            free(dest_info);
        }
    }
    return strdup(name);
}

NSAPI_PUBLIC char*
get_and_format_message(int nbOfParams, const char* key, ...) {

    UChar fmtMessage[FMT_MSG_LEN];
    UErrorCode status = U_ZERO_ERROR;
    const UChar* icuMessage;
    UChar* varList[NB_OF_PARAMS];
    char* varStr;

    va_list pvar;
    int i;
    char* message;

    const char* lang = GetAdminLanguage();
    if(lang == NULL || strlen(lang) == 0)
        lang = uloc_getDefault();

    if(nbOfParams > NB_OF_PARAMS)
        return strdup("Error: Too many arguments to format");

    va_start(pvar, key);
    for(i = 0; i < nbOfParams; i++) {
        varStr = va_arg(pvar, char*);
        varList[i] = (UChar*)malloc(sizeof(UChar) * strlen(varStr) + 2);
        u_uastrcpy(varList[i], varStr);    
    }
    va_end(pvar);
    icuMessage = getConstUCS2String(icuResourceBundle, key);
    switch(nbOfParams) {
    case 1 :
        u_formatMessage(lang, icuMessage, u_strlen(icuMessage), fmtMessage,
                        FMT_MSG_LEN, &status, varList[0]);
        break;
    case 2 :
        u_formatMessage(lang, icuMessage, u_strlen(icuMessage), fmtMessage,
                        FMT_MSG_LEN, &status, varList[0], varList[1]);
        break;
    case 3 :
        u_formatMessage(lang, icuMessage, u_strlen(icuMessage), fmtMessage,
                        FMT_MSG_LEN, &status, varList[0], varList[1],
                        varList[2]);
        break;
    case 4 :
        u_formatMessage(lang, icuMessage, u_strlen(icuMessage), fmtMessage,
                        FMT_MSG_LEN, &status, varList[0], varList[1],
                        varList[2], varList[3]);
        break;
    case 5 :
        u_formatMessage(lang, icuMessage, u_strlen(icuMessage), fmtMessage,
                        FMT_MSG_LEN, &status, varList[0], varList[1],
                        varList[2], varList[3], varList[4]);
        break;
    default:
        return NULL;    
    }   
    for(i = 0; i < nbOfParams; i++)
        free(varList[i]);
    message = ucs2strToUTF8(fmtMessage);
    return message;
}

PR_IMPLEMENT(void)
rb_init()
{
    if (icu_initialized)
        return;

    char resfile[1024];
    char* sroot = netsite_root;

    if (sroot) {
#ifdef PRODUCT_RES_SUBDIR
        sprintf(resfile, "%s/" PRODUCT_RES_SUBDIR "/%s",
                sroot, JAVA_PROPERTIES_NAME);
#else
        sprintf(resfile, "%s/lib/messages/%s", sroot, JAVA_PROPERTIES_NAME);
#endif
    } else {
        ereport(LOG_WARN,
                (char*)"Unable to find the server root to initialize ICU");
        strcpy(resfile, JAVA_PROPERTIES_NAME);
    }

    const char* lang = GetAdminLanguage();
    if(lang == NULL || strlen(lang) == 0)
        lang = uloc_getDefault();
    UErrorCode errcode = U_ZERO_ERROR;
    icuResourceBundle = ures_open(resfile, lang, &errcode);
    if (U_SUCCESS(errcode))
        icu_available = 1;
    
    if (!icu_available) {
        ereport(LOG_WARN,
                (char*)"Unable to initialize ICU resource bundle (%s)",
                JAVA_PROPERTIES_NAME);
    }

    icu_initialized = 1;
}

PR_IMPLEMENT(void)
rb_close()
{
    if (icu_initialized && icu_available)
        ures_close(icuResourceBundle);
}

/*------------------------- Helper functions -------------------------------*/

char* ucs2strToUTF8(const UChar* ustr){

    UErrorCode errcode = U_ZERO_ERROR;
    char* destStr = NULL;
    int32_t destLen;
    int32_t ustr_len = u_strlen(ustr); 
    int32_t destCapacity = 4 * ustr_len + 1;

    destStr = (char*)malloc(destCapacity);

    u_strToUTF8(destStr, destCapacity, &destLen, ustr, ustr_len, &errcode);
    if(U_SUCCESS(errcode))
        return destStr;
    return NULL;
}

const UChar* 
getConstUCS2String(UResourceBundle* myBundle, const char* key) {

    int32_t src_info_len;
    UErrorCode errcode = U_ZERO_ERROR;

    const UChar* result =  ures_getStringByKey(myBundle, key, &src_info_len,
                                               &errcode);
    if(U_SUCCESS(errcode))
        return result;
    return NULL;
}
