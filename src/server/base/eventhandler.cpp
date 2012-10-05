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

// eventhandler.c
//
//	This is an NT file that is currently used to handle log rotation events.
// 	This file can be extended to a generic event handling mechanism
//
//  Assumptions:
// 2/28/95 aruna
//

#include <windows.h>
#include <process.h>    /* beginthread, endthread */
#include <base/eventhandler.h>
#include <base/ereport.h>
#include <base/systhr.h>
#include <base/util.h>

#include "base/dbtbase.h"

#define ERR_MSG_LEN 500
#define EVENT_LEN 1024

static char resume[EVENT_LEN];
static char suspend[EVENT_LEN];

static SYS_THREAD handler_thread = 0;
static HANDLE *events = NULL;
static EVENT_HANDLER *handler_table = NULL;
static event_number = 0;

void (*event_handler)(void *) = NULL;
void *event_argument = NULL;

static HANDLE eventMutex = 0;
static HANDLE *resumeEvent = NULL;
static HANDLE suspendEvent = 0;
static BOOLEAN InitLock = TRUE;

#define ENTER_EVENT_LOCK \
    WaitForSingleObject(&eventMutex, 0); 

#define RELEASE_EVENT_LOCK ReleaseMutex(&eventMutex);

#define TERMINATE_HANDLER_THREAD()              \
	event_number = 0;                           \
	if (events) FREE(events);                   \
	if (handler_table) FREE(handler_table);     \
    if (handler_thread)                         \
 	    systhread_terminate(handler_thread);    \
	handler_thread = 0;                         

#define TERMINATE_HANDLER()                     \
	event_number = 0;                           \
	if (events) FREE(events);                   \
	if (handler_table) FREE(handler_table);     \
    if (handler_thread)                         \
 	    systhread_terminate(handler_thread);    \
	handler_thread = 0;                         \
    if (eventMutex) CloseHandle (eventMutex);   \
    eventMutex = 0;                             \
    if (suspendEvent) CloseHandle (suspendEvent);   \
    suspendEvent = 0;                           \
    if (resumeEvent) CloseHandle (*resumeEvent); \
    resumeEvent = NULL;

char *_create_handler_thread();

/* Key routine - handles events by invoking stored event handlers */

void _execute_event_handler(int event_number)
{
	EVENT_HANDLER *handler = handler_table;

	/* Serial search . This is okay for now. We are using this
	 * file only for log rotation */

	while(handler) {
		if(handler->event_number == event_number) {
			(*(handler->_event_handler))(handler->argument);
			return;	
		} else {
			handler = handler->next;
		}
	}
}

/*-----------------------------Event Routines-----------------------------*/

void _wait_for_events(void *argument)
{
	DWORD wait_result;
	
	while(TRUE) {
		if ((wait_result = WaitForMultipleObjects( event_number, events,
			FALSE, INFINITE)) == WAIT_FAILED) {
			ereport(LOG_WARN, XP_GetAdminStr(DBT_eventHandlerFailedToWaitOnEvents_),
				system_errmsg());
            ENTER_EVENT_LOCK;
			TERMINATE_HANDLER_THREAD();
            RELEASE_EVENT_LOCK;
		} else {
            ENTER_EVENT_LOCK;
			_execute_event_handler(wait_result - WAIT_OBJECT_0);
			ResetEvent(events[wait_result - WAIT_OBJECT_0]);
            RELEASE_EVENT_LOCK;
		}
	}
}

char *_add_table_event(EVENT_HANDLER *handler)
{
	HANDLE event;
	char *err;
	EVENT_HANDLER *tmp;

	if(!(event = CreateEvent(NULL, FALSE, FALSE, handler->event_name))) {
		err = (char *)MALLOC(ERR_MSG_LEN);
        util_snprintf(err, ERR_MSG_LEN, "could not open create event %s (%s)", 
            handler->event_name, system_errmsg());
		TERMINATE_HANDLER_THREAD();
        return err;
	}

	if (!events) {
		events = (HANDLE *)MALLOC(sizeof(HANDLE));
	} else {
		events = (HANDLE *)REALLOC(events, (sizeof(events) + sizeof(HANDLE)));
	}

	events[event_number -1] = event;
	if (!handler_table) {
		handler_table = handler;
	} else {
		/* Prevent duplicate handlers from being entered */
		for (tmp = handler_table; tmp; tmp = tmp->next) {
			if (!strcmp(tmp->event_name, handler->event_name)) {
				return NULL;
			}
		}
		handler->next = handler_table;
		handler_table = handler;
	}
    return NULL;
}

char *_create_event(char *event, void (*fn)(void *), void *arg)
{
	EVENT_HANDLER *event_handler;
	
	/* The first event the thread will wait on is the event to add
	 * more event handlers */

	event_handler = (EVENT_HANDLER *)MALLOC(sizeof(EVENT_HANDLER));
	event_handler->event_number = event_number++;
	event_handler->event_name = STRDUP(event);
	event_handler->_event_handler = fn;
	event_handler->argument = arg;
	event_handler->next = NULL;

	return (_add_table_event(event_handler));
}

char *_delete_event(char *event)
{
	EVENT_HANDLER *tmp, *old;
	int number, i;
	
	for (tmp = handler_table, old = NULL; tmp; old = tmp, tmp = tmp->next) {
		if (!strcmp(tmp->event_name, event)) {
			if (old) {
				old->next = tmp->next;
			} else {
				handler_table = tmp->next;
			}
			number = tmp->event_number;
			CloseHandle(events[number]);
            FREE(tmp->event_name);
			FREE(tmp);
            break;
		}
	}

	for (i = number; i <= event_number; i++ ) {
		events[i] = events[i+1];
	}
	events = (HANDLE *)REALLOC(events, (sizeof (events) - sizeof(HANDLE)));
    return NULL;

}

/*--------------------------Event Handler Routines---------------------------*/

void _wait_for_resume(void *arg)
{
	DWORD wait_result;
	
	if ((wait_result = WaitForMultipleObjects( 1, resumeEvent,
		FALSE, INFINITE)) == WAIT_FAILED) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_couldNotWaitOnResumeEventEventS_), 
			system_errmsg());
        return;
    }
}

char *_suspend_handler()
{
	HANDLE suspend_event;
	char *err;
	
    suspend_event = OpenEvent(EVENT_ALL_ACCESS, FALSE, suspend);

    if (suspend_event == NULL) {
		err = (char *)MALLOC(ERR_MSG_LEN);
        util_snprintf(err, ERR_MSG_LEN, "could not open reset event  (%s)", 
            system_errmsg());
        return err;
    }
	
    if(!SetEvent(suspend_event)) {
		err = (char *)MALLOC(ERR_MSG_LEN);
        util_snprintf(err, ERR_MSG_LEN, "cannot set reset event (%s)", 
            system_errmsg());
        return err;
    }

	return NULL;
}

char *_resume_handler()
{
	HANDLE resume_event;
	char *err;
	
    resume_event = OpenEvent(EVENT_ALL_ACCESS, FALSE, resume);

    if (resume_event == NULL) {
		err = (char *)MALLOC(ERR_MSG_LEN);
        util_snprintf(err, ERR_MSG_LEN, "could not open resume event  (%s)", 
            system_errmsg());
        return err;
    }
	
    if(!SetEvent(resume_event)) {
 		err = (char *)MALLOC(ERR_MSG_LEN);
       util_snprintf(err, ERR_MSG_LEN, "cannot set reset event (%s)", 
            system_errmsg());
        return err;
    }
	return NULL;
}

char *add_handler(char *event, void (*fn)(void *), void *arg)
{
	char *err;
	
    ENTER_EVENT_LOCK;

	if (!handler_thread) {
		if (err = _create_handler_thread()) {
        	RELEASE_EVENT_LOCK;
            return err;
		}	
	} 
	
	/* suspend handler */
	if (err = _suspend_handler()) {
        RELEASE_EVENT_LOCK;
		return err;
	}
	 
	if (err = _create_event(event, fn, arg)) {
        RELEASE_EVENT_LOCK;
		return err;
	}
	/* reset the handler to wake up and listen on the new event */

	if (err = _resume_handler()) {
        RELEASE_EVENT_LOCK;
		return err;
	}
    RELEASE_EVENT_LOCK;
	return NULL;
}

char *delete_handler(char *event)
{
	char *err;
	
    ENTER_EVENT_LOCK;
	if (!handler_thread) {
		err = (char *)MALLOC(ERR_MSG_LEN);
        util_snprintf(err, ERR_MSG_LEN, "handler thread is not present");
        RELEASE_EVENT_LOCK;
        return err;
	}

	/* suspend handler */
	if (err = _suspend_handler()) {
        RELEASE_EVENT_LOCK;
		return err;
	}
	 
	if (err = _delete_event(event)) {
        RELEASE_EVENT_LOCK;
		return err;
	}
	/* reset the handler to wake up and listen on the new event */

	if (err = _resume_handler()) {
        RELEASE_EVENT_LOCK;
		return err;
	}
    RELEASE_EVENT_LOCK;
	return NULL;
}

char *_create_handler_thread()
{
	char *err;
	
	/* The first event the thread will wait on is the event to suspend
	 * the event handler thread */

	if (err = _create_event(suspend, _wait_for_resume, NULL)) {
		return err;
	}

    handler_thread = systhread_start(15, 65536, _wait_for_events, 
                                         NULL);
	if (handler_thread == NULL) {	
		err = (char *)MALLOC(ERR_MSG_LEN);
		util_snprintf(err, ERR_MSG_LEN, "Failed to start event handler thread %s",
				system_errmsg());
		return err;
	}
	return NULL;
}

char *initialize_event_handler(char *serverid)
{
	char *err;
	char mutex_name[EVENT_LEN];
    HANDLE event;
	
	/* First create the global lock -acquire ownership immediately */

	util_snprintf(mutex_name, EVENT_LEN, "%s.event_handler", serverid);
	eventMutex = CreateMutex(NULL, TRUE, mutex_name); 
	if (eventMutex == NULL) {
		err = (char *)MALLOC(ERR_MSG_LEN);
		util_snprintf(err, ERR_MSG_LEN, "Failed to create mutex for event handler %s",
				system_errmsg());
		return err;
	}
	
    /* Create the resume and suspend event names */

	util_snprintf(resume, EVENT_LEN, "%s.resume", serverid);
	util_snprintf(suspend, EVENT_LEN, "%s.suspend", serverid);
	/* Create the global event to resume the event handler thread */
		
	if(!(event = CreateEvent(NULL, FALSE, FALSE, resume))) {
		err = (char *)MALLOC(ERR_MSG_LEN);
        util_snprintf(err, ERR_MSG_LEN, "could not open create event %s (%s)", 
            resume, system_errmsg());
    	RELEASE_EVENT_LOCK;
        return err;
	}
    resumeEvent = (HANDLE *)MALLOC(sizeof(HANDLE));
    resumeEvent[0] = event;

	if (!handler_thread) {
		if (err = _create_handler_thread()) {
        	RELEASE_EVENT_LOCK;
            return err;
		}	
	}
	 
	/* Release mutex */
	RELEASE_EVENT_LOCK;
    return NULL;

}

char *terminate_event_handler()
{
	
    TERMINATE_HANDLER();
    return NULL;

}

//--------log rotation routines -----------------------------------

char *_create_rotation_thread()
{
	char *err;
	
    handler_thread = systhread_start(15, 65536, _wait_for_events, 
                                         NULL);
	if (handler_thread == NULL) {	
		err = (char *)MALLOC(ERR_MSG_LEN);
		util_snprintf(err, ERR_MSG_LEN, "Failed to start event handler thread %s",
				system_errmsg());
		return err;
	}
	return NULL;
}

char *add_rotation_handler(char *event, void (*fn)(void *), void *arg)
{
	char *err;
	
	if (err = _create_event(event, fn, arg)) {
        return err;
	}

	if (!handler_thread) {
		if (err = _create_rotation_thread()) {
		    return err;
		}	
	} 
	
	return NULL;
}

