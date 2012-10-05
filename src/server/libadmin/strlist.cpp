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
 * strlist.c:  Managing a handle to a list of strings
 *            
 * All blame to Mike McCool
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netsite.h"
#include "libadmin/libadmin.h"

char **new_strlist(int size) {
  char **new_list;
  register int x;

  new_list = (char **) MALLOC((size+1)*(sizeof(char *)));
  /* <= so we get the one right after the given size as well */
  for(x=0; x<= size; x++)  
    new_list[x] = NULL;

  return new_list;
}

char **grow_strlist(char **strlist, int newsize) {
  char **ans;

  int length;
  int x ;
  for (length=0; strlist[length]; length++);

  ans = (char **) REALLOC(strlist, (newsize+1)*sizeof(char *));

  for( ; length <= newsize; length++)
     ans[length] = NULL;

  return ans;
}

void free_strlist(char **strlist) {
  int x;

  for(x=0; (strlist[x]); x++) FREE(strlist[x]);
  FREE(strlist);
}


char *new_str(int size) {

  return (char*)MALLOC((size+1));
}


void free_str(char *str) {

  if (str) FREE(str);
}
