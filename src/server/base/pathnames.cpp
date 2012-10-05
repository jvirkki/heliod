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
 * pathnames.c
 *
 *
 * Intended to be a module for getting/setting/extracting path pieces.
 * Should help avoid some of the "/" nightmare we had with NT; and maybe also
 * make foreign ports much easier.
 *
 *
 */


#include <stdio.h>
#include "netsite.h"
#include "base/pathnames.h"
#include "util.h"
#include "nsassert.h"

char *pn_extract_filename(char *full_name)
{
#ifdef XP_WIN32
	char *ptr;
	char *last_delimiter = NULL;
	char *filename;
	int filename_len;

	NS_ASSERT(full_name);

	for (ptr = full_name; ptr && *ptr; ptr++)
		if (*ptr == '/')
			last_delimiter = ptr;

	if (last_delimiter)
		filename_len = (int)(ptr - last_delimiter);
	else
		filename_len = (int)(ptr - full_name)+1;

	if ( !(filename = (char *)MALLOC(filename_len+1-1)) ) 
		return NULL;

    if (last_delimiter)
		memmove(filename, last_delimiter+1, filename_len);
	else
		memmove(filename, full_name, filename_len);
	filename[filename_len-1] = '\0';

	return filename;
#endif
	return NULL;
}

char *pn_extract_directory(char *full_name)
{
#ifdef XP_WIN32
	char *pathname;
	int pathname_len;
	char *ptr;
	char *last_delimiter = NULL;

	for(ptr=full_name; ptr && *ptr; ptr++)
		if (*ptr == '/')
			last_delimiter = ptr;

	if (last_delimiter)
		pathname_len = (int)(last_delimiter - full_name);
	else
		return NULL;

	if ( !(pathname = (char *)MALLOC(pathname_len+1)) ) 
		return NULL;

	memmove(pathname, full_name, pathname_len);
	pathname[pathname_len] = '\0';

	return pathname;
#endif

	return NULL;
}

char *pn_concatenate(char *directory, int dirlen, char *filename, int filelen)
{
#ifdef XP_WIN32
	char *fullname;
	int fullname_len;

	NS_ASSERT(directory);
	NS_ASSERT(filename);

	if (dirlen < 0)
		dirlen = strlen(directory);
	if (filelen < 0)
		filelen = strlen(filename);
	
	fullname_len = dirlen + 1 + filelen;

	if ( !(fullname = (char *)MALLOC(fullname_len+1)) ) 
		return NULL;

	memmove(fullname, directory, dirlen);
	fullname[dirlen] = '/';
	memmove(&(fullname[dirlen+1]), filename, filelen);
	fullname[dirlen+filelen+1] = '\0';

	return fullname;
#endif

	return NULL;
}

int 
pn_directory_equal(char *d1, char *d2)
{
#ifdef XP_WIN32
	return strcasecmp(d1, d2);
#endif

	return -1;
}

int 
pn_pathname_equal(char *p1, char *p2)
{
#ifdef XP_WIN32
	return strcasecmp(p1, p2);
#endif

	return -1;
}
