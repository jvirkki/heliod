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

/* Netscape Enterprise Server 3.5 Virtual Server Plugin
 *
 * Adapted from ~mbelshe/hacks/virtual/virtual.c by Fred Cox
 */
#include "frame/domain.h"
#include "frame/conf_api.h"
#ifndef NS_OLDES3X
#include "private/pprio.h"
#endif /* !NS_OLDES3X */

typedef struct _domain {
    unsigned long  srvr;
    char           *dir;
    struct _domain *next;
} _domain;

/* The longest line we'll allow in an access control file */
#define MAX_D_LINE 256

#define HASH_SIZE   	512
#define HASH(x)			(x%HASH_SIZE)
static _domain *domains[HASH_SIZE];
static int num_domains = 0;

static void DOMAIN_free(void *unused);

/* DOMAIN_init -  
** Parse the data file and record the IP address/Document Root pairs.
** The file consists of lines of the form:
**
**       IP Address    directory
**
** The IP address is an address the machine is listening for. this
** relies on the OS handling and dealign with IP alias hacks.
**
** The Directory parameter is actually a full path name we insert into
** the string and pass back.  this can be both good and bad - good
** so you can have many document roots, bad in that you have to
** specifiy a path all the time.
*/
NSAPI_PUBLIC char *
DOMAIN_init( char *dataFile )
{
    int i, hash;
    FILE *fDataFile;
    char buf[MAX_D_LINE];
    char in_temp[MAX_D_LINE];
    char dir_temp[MAX_D_LINE];
    char err[MAGNUS_ERROR_LEN];

    if ( (fDataFile = fopen(dataFile, "r")) == NULL) {
        util_sprintf(err, "can't open file %s (%s)", dataFile, strerror(errno));
        return STRDUP(err);
    }

    /* Initialize domains */
    num_domains = 0;

    while(fgets(buf, MAX_D_LINE, fDataFile)) {
        /* skip over obvious comments */
        if (buf[0] == '#' || buf[0] == '\n' ||
            buf[0] == ' ' || buf[0] == '\t') continue;

        /* clear these out, just in case */
        memset (&in_temp[0], 0, MAX_D_LINE);
        memset (&dir_temp[0], 0, MAX_D_LINE);

        if (sscanf (buf, "%s %s\n", &in_temp, &dir_temp) > 0) {
            _domain *newDomain;
            newDomain = (_domain *) PERM_MALLOC (sizeof(_domain));
            if (!newDomain) {
                util_sprintf(err, "out of memory creating domain %d", 
                    num_domains);
                return STRDUP(err);
            }

#ifdef XP_WIN32
	    /* Convert backward slashes to forward slashes */
	    for(i = 0; dir_temp[i]; i++)
		if (dir_temp[i] == '\\')
		    dir_temp[i] = '/';
#endif /* XP_WIN32 */

            /* Strip trailing "/" from directory */
            i = strlen(dir_temp);
            if (dir_temp[i-1] == '/')  
                dir_temp[i-1] = '\0';

            newDomain->dir = (char *) PERM_STRDUP (dir_temp);
            newDomain->srvr = inet_addr (in_temp);

            /* Add to hash table */
            hash = HASH(newDomain->srvr);
            newDomain->next = domains[hash];
            domains[hash] = newDomain;
            num_domains++;
        }
    }
    fclose(fDataFile);

    if (num_domains > 0) {
	conf_setGlobal("VirtualServers", "on");
    }

    /* QQQQ At restart, free hosts array
    magnus_atrestart(DOMAIN_free, NULL); */

    return 0;
}

/* DOMAIN_find()
** Lookup a docroot based on an IP address.  If we find it, overwrite
** the docRoot pointer.
*/
NSAPI_PUBLIC void
DOMAIN_find( SYS_NETFD fd, char **docRoot )
{
  struct sockaddr_in from_s;
  TCPLEN_T size = (size_t)sizeof( from_s );
  _domain *d_ptr;
  int hash;

  if (!num_domains) 
    return;

  if ( getsockname( PR_FileDesc2NativeHandle( fd ),
		    (sockaddr *) &from_s, (TCPLEN_T*)&size ) < 0 ) {
    return;
  }
  
  hash = HASH(from_s.sin_addr.s_addr);
  for(d_ptr = domains[hash]; d_ptr != NULL; d_ptr = d_ptr->next) {
    if (from_s.sin_addr.s_addr == d_ptr->srvr) {
      *docRoot = d_ptr->dir;
      return;
    }
  }
}

/* DOMAIN_free()
** Cleanup the allocated memory for restarts
*/
static void 
DOMAIN_free(void *unused)
{
    int x;
    _domain *ptr, *deadPtr;

    for(x = 0; x < num_domains; ++x) {
        ptr = domains[x];
        while(ptr) {
	    deadPtr = ptr;
            ptr = ptr->next;
            PERM_FREE(deadPtr->dir);
            PERM_FREE(deadPtr);
        }
    }
    num_domains = 0;
}



NSAPI_PUBLIC PRBool
AreThereAnyVServersTheVirtualConfWay()
{
	return (num_domains > 0 ? PR_TRUE : PR_FALSE);
}
