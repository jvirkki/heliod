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

#ifndef FRAME_LOG_H
#define FRAME_LOG_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * log.h: Records transactions, reports errors to administrators, etc.
 * 
 * Rob McCool
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

#ifndef BASE_SESSION_H
#include "../base/session.h"   /* Session structure */
#endif /* !BASE_SESSION_H */

#ifndef FRAME_REQ_H
#include "req.h"               /* Request struct */
#endif /* !FRAME_REQ_H */

#ifndef BASE_EREPORT_H
#include "../base/ereport.h"   /* Error reporting, degrees */
#endif /* !BASE_EREPORT_H */

#define ERROR_CUTOFF 128


/* ------------------------------ Prototypes ------------------------------ */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

/*
 * INTlog_error logs an error of the given degree from the function func
 * and formats the arguments with the printf() style fmt. Returns whether the
 * log was successful. Records the current date.
 *
 * sn and rq are optional parameters. If given, information about the client
 * will be reported.
 */

NSAPI_PUBLIC int INTlog_error_v(int degree, const char *func, Session *sn,
                                Request *rq, const char *fmt, va_list args);

NSAPI_PUBLIC int INTlog_error(int degree, const char *func, Session *sn,
                              Request *rq, const char *fmt, ...);

/*
 *  Internal use only 
 */
NSAPI_PUBLIC int INTlog_ereport_v(int degree, const char *fmt, va_list args);
NSAPI_PUBLIC int INTlog_ereport(int degree, const char *fmt, ...);

NSPR_END_EXTERN_C

/* --- End function prototypes --- */

#define log_error_v INTlog_error_v
#define log_error INTlog_error
#define log_ereport_v INTlog_ereport_v
#define log_ereport INTlog_ereport

#endif /* INTNSAPI */

#endif /* !FRAME_LOG_H */

