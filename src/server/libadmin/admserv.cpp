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
 * admserv.c: Routines for dealing with ns-admin.conf
 *
 * Based on the magconf.c by the stupid looking one
 * Based on the magconf.c by the Grand, Supreme Being
 * 
 * Now also contains code to choose a server or several servers
 * 
 * Rob McCool
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "netsite.h"
#include "base/file.h"
#include "libadmin/libadmin.h"

extern "C" int qsortcmp(const void * a, const void * b)
{
    return strcmp(*(const char **)a, *(const char **) b);
}

void sortstrings(char **stringarray)
{
    int mainindex = 0;

    while(stringarray[mainindex])
    {
        mainindex++;
    }
    qsort(stringarray, mainindex, sizeof(stringarray[0]), qsortcmp); 
}

#ifdef USE_ADMSERV
#define BUFFER_SIZE 10
#define FILE_ERROR 0
/*
 * Spawned off as idependent routine for used by both admserv and cluster
 * - 8/2/96 Adrian
 */
char** scan_server_instance(char *dirname, char **namelist)
{
    SYS_DIR ds;
    SYS_DIRENT *d;

    char **ans = new_strlist(BUFFER_SIZE);
    int totalsize=BUFFER_SIZE, curentry=0;

    if(!(ds = dir_open(dirname))) {
        return NULL;
    }
 
    if (namelist == NULL)
       return NULL;

    while( (d = dir_read(ds)) )  {
        char *entry;
        register int z;

        entry = d->d_name;

        for(z=0; namelist[z]; z++)  {
	    char *srv_prefix;
	    srv_prefix = (char *) MALLOC(strlen(namelist[z])+2);
	    sprintf(srv_prefix, "%s-", namelist[z]);
            if( (!strncmp(entry, srv_prefix, strlen(srv_prefix))) )  {
                if( !(curentry < totalsize) )  {
                    totalsize += BUFFER_SIZE;
                    ans=grow_strlist(ans, totalsize);
                }
                ans[curentry++] = STRDUP(entry);
                ans[curentry] = NULL;
            }
	    (void) FREE(srv_prefix);
        }
    }

    dir_close(ds);

    if(curentry)
    {    
        sortstrings(ans);
        return ans;
    }
    else
        return NULL;
}

/* Generates a list of all the servers at specified path. */
NSAPI_PUBLIC char **listServersIn(char *path) {
  static char *nameList[] = {"https", 0};

  return(scan_server_instance(path, nameList));
}
#endif

