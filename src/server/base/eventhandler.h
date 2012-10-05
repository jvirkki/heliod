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
 * eventhandler.h: Handle registration of event handlers
 *
 * This is a facility in the NT server to provide a way to register event
 * handling functions. Often there is a need to send a control signal of some
 * kind to the server. This could be a signal for the server to rotate its
 * logs, or a signal to collect and return statistical information of some kind
 * such as perfmon stats.
 * 
 * This file specifies the structures and functions necessary to set up this
 * kind of asynchronous special event handling.
 * 
 * Aruna Victor 2/21/96
 */

#ifndef EVENTHANDLER_H
#define EVENTHANDLER_H

#include "netsite.h"

/* ------------------------------ Structures ------------------------------ */

/* EVENT_HANDLER specifies
    1. The name of the event. This is the event that the event handler will
       create and wait on for a signal.
    2. The name of the function should be called to handle the event.
    3. The argument that should be passed to this function.
    4. The next EVENT_HANDLER on the list this structure is on. */

typedef struct event_handler {
	int event_number;
    char *event_name;
    void (*_event_handler)(void *);
    void *argument;
    struct event_handler *next;
} EVENT_HANDLER;

/* ------------------------------ Prototypes ------------------------------ */

NSPR_BEGIN_EXTERN_C

char *initialize_event_handler(char *serverid);

char *terminate_event_handler();

char *add_handler(char *event, void (*fn)(void *), void *arg);

char *delete_handler(char *event);

char *add_rotation_handler(char *event, void (*fn)(void *), void *arg);

NSPR_END_EXTERN_C

#endif /* !EVENTHANDLER	 */














