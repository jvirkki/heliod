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

#ifndef __lexer_h
#define __lexer_h

#ifndef _POOL_H_
#include "base/pool.h"
#endif /* _POOL_H_ */

/* Define error codes */
#define LEXERR_MALLOC	-1		/* insufficient dynamic memory */


typedef struct LEXStream_s LEXStream_t;
typedef int (*LEXStreamGet_t)(LEXStream_t *);
struct LEXStream_s {
    LEXStream_t * lst_next;		/* link for "include" parent stream */
    void * lst_strmid;			/* client stream identifier */
    LEXStreamGet_t lst_get;		/* pointer to stream "get" function */
    char * lst_buf;			/* stream buffer pointer */
    char * lst_cp;			/* current position in buffer */
    int lst_len;			/* remaining bytes in buffer */
    int lst_buflen;			/* buffer length */
    int lst_flags;			/* bit flags */
#define LST_FREEBUF	0x1		/* free lst_buf in stream destroy */
};
NSPR_BEGIN_EXTERN_C

/* Functions in lexer.c */
NSAPI_PUBLIC
int lex_class_check(void * chtab, char code, unsigned long cbits);

NSAPI_PUBLIC
int lex_class_create(int classc, char * classv[], void **pchtab);

NSAPI_PUBLIC void lex_class_destroy(void * chtab);

NSAPI_PUBLIC
LEXStream_t * lex_stream_create(LEXStreamGet_t strmget, void * strmid,
                                char * buf, int buflen);

NSAPI_PUBLIC void lex_stream_destroy(LEXStream_t * lst);

NSAPI_PUBLIC int
lex_token_new(pool_handle_t * pool, int initlen, int growlen, void **token);

NSAPI_PUBLIC int lex_token_start(void * token);

NSAPI_PUBLIC
char * lex_token_info(void * token, int * tdatalen, int * tbufflen);

NSAPI_PUBLIC char * lex_token(void * token);

NSAPI_PUBLIC void lex_token_destroy(void * token);

NSAPI_PUBLIC
char * lex_token_get(void * token, int * tdatalen, int * tbufflen);

NSAPI_PUBLIC char * lex_token_take(void * token);

NSAPI_PUBLIC
int lex_token_append(void * token, int nbytes, char * src);

NSAPI_PUBLIC
int lex_next_char(LEXStream_t * lst, void * chtab, unsigned long cbits);

NSAPI_PUBLIC
int lex_scan_over(LEXStream_t * lst, void * chtab, unsigned long cbits,
			 void * token);

NSAPI_PUBLIC
int lex_scan_string(LEXStream_t * lst, void * token, int flags);

NSAPI_PUBLIC
int lex_scan_to(LEXStream_t * lst, void * chtab, unsigned long cbits,
                void * token);

NSAPI_PUBLIC
int lex_skip_over(LEXStream_t * lst, void * chtab, unsigned long cbits);

NSAPI_PUBLIC
int lex_skip_to(LEXStream_t * lst, void * chtab, unsigned long cbits);

NSPR_END_EXTERN_C

#endif /* __lexer_h */
