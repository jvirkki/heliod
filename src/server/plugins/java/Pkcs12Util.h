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

#ifndef __PKCS12UTIL_H__
#define __PKCS12UTIL_H__

#include <stdio.h>
#include "nspr.h"
#include "pk11func.h"


#define INTERNAL_TOKEN "internal"
#define PKCS12_PASSWD "changeit"

const int defaultBufferSize = 3000;

extern const char *errMsg;

typedef struct p12uContextStr {
    unsigned char*    buffer;
    int32             size;
    int32             bufSize;
} p12uContext;

enum PWSourceType {PW_NONE = 0, PW_FROMFILE = 1, PW_PLAINTEXT = 2, PW_EXTERNAL = 3};

typedef struct {
    PWSourceType source;
    char *data;
} secuPWData;

#ifdef NSSDEBUG
extern char tempBuf[];
void debug(char* msg);
#endif

static p12uContext * allocateP12uContext(PRIntn bufSize);
void freeP12uContext(p12uContext *p12cxt);
char * SECU_GetModulePassword(PK11SlotInfo *slot, PRBool retry, void *arg);
PK11SlotInfo * getSlot(char* tokenName);
static SECStatus p12u_SwapUnicodeBytes(SECItem *uniItem);
static PRBool p12u_ucs2_ascii_conversion_function(PRBool toUnicode,
                                                  unsigned char *inBuf,
                                                  unsigned int inBufLen,
                                                  unsigned char *outBuf,
                                                  unsigned int maxOutBufLen,
                                                  unsigned int *outBufLen,
                                                  PRBool swapBytes);
SECStatus P12U_UnicodeConversion(PRArenaPool *arena, SECItem *dest, SECItem *src,
                                 PRBool toUnicode, PRBool swapBytes);
SECItem * P12U_GetP12FilePassword(secuPWData *p12FilePw);
static void p12u_WriteToExportArray(void *arg, const char *buf, unsigned long len);
p12uContext* P12U_ExportPKCS12Object(char *nn, PK11SlotInfo *inSlot,
                                     secuPWData *slotPw, secuPWData *p12FilePw);
static void p12u_EnableAllCiphers();

#endif //__PKCS12UTIL_H__
