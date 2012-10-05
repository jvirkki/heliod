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

#ifndef BASE_EREPORT_H
#define BASE_EREPORT_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * ereport.h: Records transactions, reports errors to administrators, etc.
 * 
 * Rob McCool
 */

#ifndef BASE_SESSION_H
#include "session.h"
#endif /* !BASE_SESSION_H */

/* Pseudo-filename to enable logging to syslog */
#define EREPORT_SYSLOG "SYSLOG"

/* NSAPI degrees used by Java but not exposed in nsapi.h */
#define LOG_FINER  7
#define LOG_FINEST 8

/* --- Begin function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

/*
 * INTereport logs an error of the given degree and formats the arguments with 
 * the printf() style fmt. Returns whether the log was successful. Records 
 * the current date.
 */

NSAPI_PUBLIC int INTereport(int degree, const char *fmt, ...);
NSAPI_PUBLIC int INTereport_v(int degree, const char *fmt, va_list args);

/*
 * INTereport_init initializes the error logging subsystem and opens the static
 * file descriptors. It returns NULL upon success and an error string upon
 * error. If a userpw is given, the logs will be chowned to that user.
 */

NSAPI_PUBLIC
char *INTereport_init(const char *err_fn, const char *email, struct passwd *pwuser, const char *version, int restarted);

NSAPI_PUBLIC void INTereport_terminate(void);

/* For restarts */
NSAPI_PUBLIC SYS_FILE INTereport_getfd(void);

NSPR_END_EXTERN_C

#ifdef __cplusplus
class EreportableException;
NSAPI_PUBLIC int INTereport_exception(const EreportableException& e);
#endif

/* --- End function prototypes --- */

#define ereport INTereport
#define ereport_v INTereport_v
#define ereport_init INTereport_init
#define ereport_terminate INTereport_terminate
#define ereport_getfd INTereport_getfd
#define ereport_exception INTereport_exception

typedef int (EreportFunc)(const VirtualServer* vs, int degree, const char *formatted, int formattedlen, const char *raw, int rawlen, void *data);

void ereport_set_servername(const char* name);
void ereport_set_logvsid(PRBool b);
void ereport_set_logall(PRBool b);
void ereport_set_alwaysreopen(PRBool b);
void ereport_set_timefmt(const char* timeFmt);
void ereport_set_degree(int degree);
int ereport_level2degree(const char *level, int defdegree);
PRBool ereport_can_log(int degree);
int ereport_request(Request* rq, int degree, const char *fmt, ...);
char *ereport_abs_filename(const char *filename);
void ereport_rotate(const char* ext);
void ereport_set_rotate_callback(void (*fn)(const char* filenameNew, const char* filenameOld));
void ereport_reopen(void);
void ereport_outofmemory(void);
void ereport_disaster(int degree, const char *fmt, ...);
NSAPI_PUBLIC int ereport_register_cb(EreportFunc* ereport_func, void* data);
NSAPI_PUBLIC int ereport_register_thread_cb(EreportFunc* ereport_func, void* data);

#endif /* INTNSAPI */

#endif /* !BASE_EREPORT_H */
