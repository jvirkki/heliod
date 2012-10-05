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

#ifndef __CRONUTIL_H_
#define __CRONUTIL_H_

#include <limits.h>
#include <stdio.h>
 
/* Make NSAPI_PUBLIC available */
/* Copy from #include "base/systems.h"  */
#if defined (XP_WIN32)      /* Windows NT */

#include <wtypes.h>
#include <winbase.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#ifdef BUILD_DLL
#define NSAPI_PUBLIC __declspec(dllexport)
#else
#define NSAPI_PUBLIC
#endif /* BUILD_DLL */
#else
#define NSAPI_PUBLIC
#endif  /* Windows NT */

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                                                                          */
/* Migrate cron_conf related stuff to libadminutil                          */
/*                                                                          */
/****************************************************************************/

/* read and write to cron.conf, cron_conf.c */
/* Alex Feygin, 3/22/96                     */
typedef struct https_cron_conf_obj
{
  char *name;
  char *command;
  char *dir;
  char *user;
  char *start_time;
  char *days;
} 
https_cron_conf_obj;
 
typedef struct https_cron_conf_list
{
  char *name;
  https_cron_conf_obj *obj;
  struct https_cron_conf_list *next;
} 
https_cron_conf_list;
 
/* Reads cron.conf to a null terminated list of cron_conf_objects; returns
   0 if unable to do a read; 1 otherwise */
NSAPI_PUBLIC int https_cron_conf_read(void);
 
/* gets a cron object, NULL if it doesnt exist */
NSAPI_PUBLIC https_cron_conf_obj *https_cron_conf_get(char *name);
 
/* returns a NULL-terminated cron_conf_list of all the cron conf objects */
NSAPI_PUBLIC https_cron_conf_list *https_cron_conf_get_list();
 
/* Creates a cron conf object; all these args get STRDUP'd in the function
   so make sure to free up the space later if need be */
NSAPI_PUBLIC https_cron_conf_obj *https_cron_conf_create_obj(char *name, char *command,
						 char *dir,  char *user, 
						 char *start_time, char *days);
 
/* Puts a cron conf object into list or updates it if it already in there.
   Returns either the object passed or the object in there already;
   cco may be FREE'd during this operation so if you need the object
   back, call it like so:
   
   cco = https_cron_conf_set(cco->name, cco);  
 
   calling cron_conf_set with a NULL cco will cause the 'name' object
   to be deleted.
*/
NSAPI_PUBLIC https_cron_conf_obj *https_cron_conf_set(char *name, https_cron_conf_obj *cco);
 
/* write out current list of https_cron_conf_objects to cron.conf file */
NSAPI_PUBLIC void https_cron_conf_write(void);
 
/* delete current cron object based on "name" */
NSAPI_PUBLIC void https_cron_conf_delete(char *name, https_cron_conf_obj *cco);

/* free all cron conf data structures */
NSAPI_PUBLIC void https_cron_conf_free(void);

/****************************************************************************/
/*                                                                          */
/* End of cron_conf related stuff                                           */
/*                                                                          */
/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _CRONUTIL_H_ */
