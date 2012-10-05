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

#ifndef BASE_FILE_H
#define BASE_FILE_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/* GLOBAL FUNCTIONS:
 * DESCRIPTION:
 * system-specific functions for reading/writing files
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */
#include <sys/types.h> /* stat */
#include <sys/stat.h> /* stat */

/* --- Begin function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC void INTsystem_errmsg_init(void);

NSAPI_PUBLIC SYS_FILE INTsystem_fopenRO(const char *path);
NSAPI_PUBLIC SYS_FILE INTsystem_fopenWA(const char *path);
NSAPI_PUBLIC SYS_FILE INTsystem_fopenRW(const char *path);
NSAPI_PUBLIC SYS_FILE INTsystem_fopenWT(const char *path);
NSAPI_PUBLIC int INTsystem_fread(SYS_FILE fd, void *buf, int sz);
NSAPI_PUBLIC int INTsystem_fwrite(SYS_FILE fd, const void *buf,int sz);
NSAPI_PUBLIC int INTsystem_fwrite_atomic(SYS_FILE fd, const void *buf, int sz);
NSAPI_PUBLIC int INTsystem_lseek(SYS_FILE fd, int off, int wh);
NSAPI_PUBLIC int INTsystem_fclose(SYS_FILE fd);
NSAPI_PUBLIC int INTsystem_stat(const char *name, struct stat *finfo);
NSAPI_PUBLIC int INTsystem_lstat(const char *name, struct stat *finfo);
#ifdef XP_WIN32
NSAPI_PUBLIC int INTsystem_stat64(const char *name, struct _stati64 *finfo);
NSAPI_PUBLIC int INTsystem_lstat64(const char *name, struct _stati64 *finfo);
#else
NSAPI_PUBLIC int INTsystem_stat64(const char *name, struct stat64 *finfo);
NSAPI_PUBLIC int INTsystem_lstat64(const char *name, struct stat64 *finfo);
#endif
NSAPI_PUBLIC int INTsystem_rename(const char *oldpath, const char *newpath);
NSAPI_PUBLIC int INTsystem_unlink(const char *path);
NSAPI_PUBLIC int INTsystem_tlock(SYS_FILE fd);
NSAPI_PUBLIC int INTsystem_flock(SYS_FILE fd);
NSAPI_PUBLIC int INTsystem_ulock(SYS_FILE fd);
NSAPI_PUBLIC int INTfile_is_path_abs(const char *path);
NSAPI_PUBLIC char *INTfile_canonicalize_path(const char *path);
NSAPI_PUBLIC char *INTfile_basename(const char *path);
NSAPI_PUBLIC int INTfile_are_files_distinct(SYS_FILE fd1, SYS_FILE fd2);

#ifdef XP_WIN32
NSAPI_PUBLIC SYS_DIR INTdir_open(const char *path);
NSAPI_PUBLIC SYS_DIRENT *INTdir_read(SYS_DIR ds);
NSAPI_PUBLIC void INTdir_close(SYS_DIR ds);
#endif /* XP_WIN32 */

NSAPI_PUBLIC int INTdir_create_mode(char *dir);
NSAPI_PUBLIC int INTdir_create_all(char *dir);

/* --- OBSOLETE ----------------------------------------------------------
 * The following macros/functions are obsolete and are only maintained for
 * compatibility.  Do not use them. 11-19-96
 * -----------------------------------------------------------------------
 */

#ifdef XP_WIN32
NSAPI_PUBLIC char *INTsystem_winsockerr(void);
NSAPI_PUBLIC char *INTsystem_winerr(void);
NSAPI_PUBLIC int INTsystem_pread(SYS_FILE fd, void *buf, int sz);
NSAPI_PUBLIC int INTsystem_pwrite(SYS_FILE fd, const void *buf, int sz);
NSAPI_PUBLIC void INTfile_unix2local(const char *path, char *p2);
#endif /* XP_WIN32 */

NSAPI_PUBLIC int INTsystem_nocoredumps(void);
NSAPI_PUBLIC int INTfile_setinherit(SYS_FILE fd, int value);
NSAPI_PUBLIC void INTfile_setdirectio(SYS_FILE fd, int value);
NSAPI_PUBLIC int INTfile_notfound(void);
NSAPI_PUBLIC char *INTsystem_errmsg(void);
NSAPI_PUBLIC int INTsystem_errmsg_fn(char **buff, size_t maxlen);

#ifdef XP_UNIX
NSAPI_PUBLIC void file_mode_init (mode_t mode);
#endif /* XP_UNIX */

NSPR_END_EXTERN_C

#define system_errmsg_init INTsystem_errmsg_init
#define system_fopenRO INTsystem_fopenRO
#define system_fopenWA INTsystem_fopenWA
#define system_fopenRW INTsystem_fopenRW
#define system_fopenWT INTsystem_fopenWT
#define system_fread INTsystem_fread
#define system_fwrite INTsystem_fwrite
#define system_fwrite_atomic INTsystem_fwrite_atomic
#define system_lseek INTsystem_lseek
#define system_fclose INTsystem_fclose
#define system_stat INTsystem_stat
#define system_lstat INTsystem_lstat
#define system_stat64 INTsystem_stat64
#define system_lstat64 INTsystem_lstat64
#define system_rename INTsystem_rename
#define system_unlink INTsystem_unlink
#define system_tlock INTsystem_tlock
#define system_flock INTsystem_flock
#define system_ulock INTsystem_ulock
#define file_is_path_abs INTfile_is_path_abs
#define file_canonicalize_path INTfile_canonicalize_path
#define file_basename INTfile_basename
#define file_are_files_distinct INTfile_are_files_distinct
#ifdef XP_WIN32
#define dir_open INTdir_open
#define dir_read INTdir_read
#define dir_close INTdir_close
#endif /* XP_WIN32 */
#define dir_create_all INTdir_create_all

/* Obsolete */
#ifdef XP_WIN32
#define system_winsockerr INTsystem_winsockerr
#define system_winerr INTsystem_winerr
#define system_pread INTsystem_pread
#define system_pwrite INTsystem_pwrite
#define file_unix2local INTfile_unix2local
#endif /* XP_WIN32 */

#define system_nocoredumps INTsystem_nocoredumps
#define file_setinherit INTfile_setinherit
#define file_setdirectio INTfile_setdirectio
#define file_notfound INTfile_notfound
#define rtfile_notfound INTfile_notfound
#define system_errmsg INTsystem_errmsg
#define system_errmsg_fn INTsystem_errmsg_fn
#define dir_create_mode INTdir_create_mode

#endif /* INTNSAPI */

#endif /* BASE_FILE_H */
