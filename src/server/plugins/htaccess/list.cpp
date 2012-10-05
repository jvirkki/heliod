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

/* Simple single linked-list functions. This replaces an array of MAX_SECURITY
 * elements of security_data in the context, reducing memory usage from
 * 68,788 bytes/request to at around 1960 bytes/request */

#include "htaccess.h"


/* Allocate security_data entry. Initialize allow,auth and deny tables with
 * the minimum SECTABLE_ALLOC_UNIT entries.
 */
security_data * htaccess_newsec(void)
{
    security_data * item;

    item = (security_data *)MALLOC(sizeof(security_data));

    item->max_num_allow = SECTABLE_ALLOC_UNIT;
    item->allow = (char **)CALLOC(sizeof(char *) * SECTABLE_ALLOC_UNIT);
    item->allow_methods = (int *)CALLOC(sizeof(int) * SECTABLE_ALLOC_UNIT);

    item->max_num_auth = SECTABLE_ALLOC_UNIT;
    item->auth = (char **)CALLOC(sizeof(char *) * SECTABLE_ALLOC_UNIT);
    item->auth_methods = (int *)CALLOC(sizeof(int) * SECTABLE_ALLOC_UNIT);

    item->max_num_deny = SECTABLE_ALLOC_UNIT;
    item->deny = (char **)CALLOC(sizeof(char *) * SECTABLE_ALLOC_UNIT);
    item->deny_methods = (int *)CALLOC(sizeof(int) * SECTABLE_ALLOC_UNIT);

    item->num_allow = 0;
    item->num_auth = 0;
    item->num_deny = 0;

    item->next = NULL;

    return item;
}

/* Add a new entry, returning a pointer to the head. This first entry needs
 * to have already been allocated as a place holder. */
security_data * htaccess_addsec(security_data * head, security_data * item)
{
    security_data * start;

    start = head;

    while (head->next != NULL) {
        head = head->next;
    }

    head->next = item;

    return start;
}

/*
 * Get an element. The first entry is a place holder so give the x+1 entry
 * back.
 */
security_data * htaccess_getsec(security_data * head, int x)
{
    int i = 0;
    security_data * start;

    start = head;
    x++;

    while (start && (i < x)) {
        start = start->next;
        i++;
    }

    if (start)
        return start;
    else
        return NULL;
}

/* Free all elements of the list except the head */
void htaccess_secflush(security_data * head)
{
    security_data * start;
    security_data * item;
    int y;

    start = head->next;

    while(start) {
        item = start;
        FREE(item->d);

        for(y=0;y<item->num_allow;y++)
            FREE(item->allow[y]);
        FREE(item->allow);
        FREE(item->allow_methods);
        item->max_num_allow = 0;

        for(y=0;y<item->num_deny;y++)
            FREE(item->deny[y]);
        FREE(item->deny);
        FREE(item->deny_methods);
        item->max_num_deny = 0;

        for(y=0;y<item->num_auth;y++)
            FREE(item->auth[y]);
        FREE(item->auth);
        FREE(item->auth_methods);
        item->max_num_auth = 0;

        if(item->auth_type)
            FREE(item->auth_type);
        if(item->auth_name)
            FREE(item->auth_name);

        if(item->auth_pwfile)
            FREE(item->auth_pwfile);
        if(item->auth_grpfile)
            FREE(item->auth_grpfile);
        start = item->next;
        FREE(item);
    }
}
