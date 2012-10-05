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
 * libadmin.h - All functions contained in libadmin.a
 *
 * All blame goes to Mike McCool
 */

#ifndef	libadmin_h
#define	libadmin_h

#include "base/systems.h"

#ifdef HTTPS_ADMSERV
#undef HTTPS_ADMSERV
#endif
#define HTTPS_ADMSERV "admin-server"

#include <limits.h>

#ifdef XP_WIN32
#include <windows.h>
#include <direct.h>
#define PATH_MAX MAX_PATH
#endif // XP_WIN32

#include "cronutil.h"
/* Maintain what amounts to a handle to a list of strings */
/* strlist.c */
NSAPI_PUBLIC char **new_strlist(int size);
NSAPI_PUBLIC char **grow_strlist(char **strlist, int newsize);
NSAPI_PUBLIC void free_strlist(char **strlist);

NSAPI_PUBLIC char *new_str(int size);
NSAPI_PUBLIC void free_str(char *str);

/* List all of the installed servers at specified path.
 * admserv.c  */
NSAPI_PUBLIC char **listServersIn(char *path);

/* Compares the base64 decoding of dbpwd with the SHA1 hash or userpwd */
NSAPI_PUBLIC int https_sha1_pw_cmp( char *userpwd, char *dbpwd );

#endif	/* libadmin_h */
