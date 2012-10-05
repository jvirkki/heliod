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

//--------------------------------------------------------------------------//
//      Copyright (C) 1996, 1997 Netscape Communications Corporation        //
//--------------------------------------------------------------------------//
//                                                                          //
//  Name: Web Server Binary (httpd.exe)                                     //
//	Platforms: WIN32                                                        //
//  ......................................................................  //
//  httpd.exe always runs as an application.  It is called from the         //
//  WatchDog process ns-httpd.exe, which runs as a service.  (see           //
//  \ns\netsite\httpd\ntwdog\watchdog.c for information on ns-httpd.exe)    //
//  Command line syntax: httpd.exe <serviceid>                              //
//  Example: httpd.exe https-nickname                                       //
//                                                                          //
//  How to Debug                                                            //
//  Option 1: Command Line Startup                                          //
//  cd serverroot\bin\https                                                 //
//  msdev httpd.exe https-nickname                                          //
//  Open source files, set breakpoints, and Build|Debug...                  //
//                                                                          //
//  Option 2: Project Startup                                               //
//  Create empty project in VC++.  For the executable, specify              //
//  <sroot>\bin\https\httpd.exe.  For Parameters, use https-nickname.       //
//  Open source files, set breakpoints, and Build|Debug...                  //
//                                                                          //
//  Option 3: Hardcoded Breakpoint                                          //
//  Add a DebugBreak(); statement in the code where you wish to stop. Then  //
//  recompile and run as normal.  You should see a breakpoint exception     //
//  dialog when DebugBreak() is executed.  Press Cancel to debug.  You may  //
//  need to turn on the JustInTime Debugging option in VC++.                //
//                                                                          //
//  Option 4: Attach to Running Process                                     //
//  Under NT 3.51, use ps or pview to get the pid, then use msdev -p pid.   //
//  Under NT 4.0, activate Task Manager (right click on taskbar), select    //
//  httpd.exe from the Processes tab, then right click and select Debug.    //
//                                                                          //
//  ......................................................................  //
//  Revision History:                                                       //
//  01-12-94  Initial Version, Aruna Victor (aruna@netscape.com)            //
//  12-02-96  3.0 registry changes, cleanup, Andy Hakim (ahakim)            // 
//  03-12-97  service ID uses the form https-nickname, not https30-nickname //
//--------------------------------------------------------------------------//
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <tchar.h>
#include <prthread.h>

#include "wingetopt.h"
#include "uniquename.h"
#include "netsite.h"
#include "base/daemon.h"
#include "base/ereport.h"
#include "frame/req.h"
#include "frame/func.h"
#include "frame/protocol.h"
#include "base/util.h"
#include <base/eventlog.h>
#include <base/systhr.h>
#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include <frame/aclinit.h>
#include <safs/acl.h>

#include "frame/conf.h"
#include "base/cinfo.h"

#include <nt/regparms.h>
#include <nt/magnus.h>
#include <nt/messages.h>

#include "../lib/libsi18n/gshttpd.h"

#include "nstime/nstime.h"
#include "nss.h"

#include "dbthttpdsrc.h"
#include "getproperty/GetProperty.h"

#include "httpdaemon/WebServer.h"
extern "C" {
#include "preenc.h"
}

//--------------------------------------------------------------------------//
// global variables                                                         //
//--------------------------------------------------------------------------//
static const int NUM_WS_EVENTS = 2;
static const char *unique_name = NULL;
static HANDLE phEvents[NUM_WS_EVENTS];
enum WSEvents { WS_SHUTDOWN = 0, WS_RECONFIG = 1 };

PRThread* MainThread = NULL;

// forward declarations
BOOL StartHttpService(int argc, char *argv[]);


//--------------------------------------------------------------------------//
// send reconfiguration information back to admin client                    //
//--------------------------------------------------------------------------//

static int ereport_configuration_callback(const VirtualServer* vs, int degree, const char *formatted, int formattedlen, const char *raw, int rawlen, void *data)
{
    // We only write to the admin client if the admin channel is active
    HANDLE hMsgPipe = *(HANDLE*)data;
    if (hMsgPipe != INVALID_HANDLE_VALUE) {
        // Log the message to the admin client
        DWORD nWritten;
        WriteFile(hMsgPipe, raw, rawlen, &nWritten, NULL);
    }

    // Log this message to the log file as well
    return 0;
}

//--------------------------------------------------------------------------//
// connect to the admin client                                              //
//--------------------------------------------------------------------------//

HANDLE admin_connect(void)
{
    HANDLE hMsgPipe = INVALID_HANDLE_VALUE;

    // Name of the pipe the admin client creates
    const int MAX_PIPE_BUF_SIZE = 1024;
    char pipeName[MAX_PIPE_BUF_SIZE];
    sprintf(pipeName, "\\\\.\\pipe\\iWSAdmin-%s", unique_name);

    for (;;) {
        // Attempt to connect to the admin client
        hMsgPipe = CreateFile(pipeName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                              0, NULL);
        if (hMsgPipe != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() != ERROR_PIPE_BUSY)
            break;

        // Wait 20 seconds to establish a connection
        if (!WaitNamedPipe(pipeName, 20000))
            break;
    }

    if (hMsgPipe != INVALID_HANDLE_VALUE) {
        DWORD dwMode = PIPE_READMODE_MESSAGE;
        BOOL fSuccess = SetNamedPipeHandleState(hMsgPipe, &dwMode, NULL, NULL);
        if (!fSuccess) {
            CloseHandle(hMsgPipe);
            hMsgPipe = INVALID_HANDLE_VALUE;
        }
    }

    return hMsgPipe;
}

//--------------------------------------------------------------------------//
// wait for reconfiguration or termination request                          //
//--------------------------------------------------------------------------//
void _WaitSignalThread(void *unused)
{
    // Capture all ereport() calls on this thread
    HANDLE hMsgPipe = INVALID_HANDLE_VALUE;
    ereport_register_thread_cb(&ereport_configuration_callback, &hMsgPipe);

    DWORD dwWait;
    while (TRUE)
    {
        dwWait = WaitForMultipleObjects(NUM_WS_EVENTS, phEvents, FALSE,
                                        INFINITE);
        if ((dwWait - WAIT_OBJECT_0) == WS_RECONFIG)
        {
            ResetEvent(phEvents[WS_RECONFIG]);

            hMsgPipe = admin_connect();

            PRStatus status = WebServer::Reconfigure();

            if (hMsgPipe != INVALID_HANDLE_VALUE) {
                FlushFileBuffers(hMsgPipe);
                CloseHandle(hMsgPipe);
                hMsgPipe = INVALID_HANDLE_VALUE;
            }
        }
        else
        {
            // either WS_SHUTDOWN or WaitForMultipleObjects error
            break;
        }
    }
}

//--------------------------------------------------------------------------//
//                                                                          //
//--------------------------------------------------------------------------//
int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpszLine, int nShow)
{
    WebServer::SetInstance(hInst);
    if (!lpszLine || !lpszLine[0])
    {
        LogErrorEvent(NULL, EVENTLOG_ERROR_TYPE, 0, MSG_BAD_STARTUP, 
      	      "Malformed Command Line", "");
	return TRUE;
    }

    StartHttpService(__argc, __argv);
    return TRUE;
}

//--------------------------------------------------------------------------//
//                                                                          //
//--------------------------------------------------------------------------//

void webserver_run(void* arg)
{
	*(PRStatus*)arg = WebServer::Run();
	SetEvent(phEvents[WS_SHUTDOWN]);
};

//--------------------------------------------------------------------------//
//                                                                          //
//--------------------------------------------------------------------------//
BOOL StartHttpService(int argc, char *argv[])
{
    HANDLE hEventLog = NULL;
    BOOL bReturn = FALSE;
    char szReconfigEvent[MAX_PATH];
    char szDoneEvent[MAX_PATH];
    PRStatus webserver_run_status = PR_SUCCESS;

    // Process the argv and extract config_root into unique_name. 
    // Use this to name events.

    int c;
    char *serverconfig = NULL;
    while ((c = getopt(argc, argv, "s:n:d:r:f:cvi")) != -1) {
        switch(c) {
        case 'd':
            serverconfig = optarg;
            break;
        default:
            break;
        }
    }
    if (!serverconfig || !serverconfig[0])
        goto cleanup;

    // canonicalize the server config dir path in case it is a relative
    // path and use it to generate a unique id that is used for all server
    // to watchdog communication
    serverconfig = file_canonicalize_path(serverconfig);
    unique_name = set_uniquename(serverconfig);
    FREE(serverconfig);

    hEventLog = InitializeLogging(unique_name);

    wsprintf(szDoneEvent, "NS_%s", unique_name);

    phEvents[WS_SHUTDOWN] = OpenEvent(EVENT_ALL_ACCESS, FALSE, szDoneEvent);
    if (phEvents[WS_SHUTDOWN] != NULL)
    {
        // should not exist
        goto cleanup;
    }

    phEvents[WS_SHUTDOWN] = CreateEvent(NULL, TRUE, FALSE, szDoneEvent);
    if (phEvents[WS_SHUTDOWN] == NULL)
	goto cleanup;

    wsprintf(szReconfigEvent, "NR_%s", unique_name);

    phEvents[WS_RECONFIG] = OpenEvent(EVENT_ALL_ACCESS, FALSE, szReconfigEvent);
    if (phEvents[WS_RECONFIG] != NULL)
    {
        // should not exist
        goto cleanup;
    }

    phEvents[WS_RECONFIG] = CreateEvent(NULL, TRUE, FALSE, szReconfigEvent);
    if (phEvents[WS_RECONFIG] == NULL)
	goto cleanup;

    if (WebServer::Init(argc, argv) == PR_FAILURE)
        goto cleanup;

    MainThread = PR_CreateThread(PR_USER_THREAD,
				 webserver_run,
				 &webserver_run_status,
				 PR_PRIORITY_NORMAL,
				 PR_GLOBAL_THREAD,
				 PR_UNJOINABLE_THREAD,
				 65536);
    if (MainThread == NULL)
	goto cleanup;

    PRThread *thread;
			
    /* In NSPR20, the first thread becomes a user thread.
     * You can't go to sleep on a blocking call like WaitForSingleObject
     * or you'll kill the whole process.
     * So send it off to a native thread before Wait()ing.
     */
    thread = PR_CreateThread(PR_USER_THREAD,
				_WaitSignalThread,
				NULL,
				PR_PRIORITY_NORMAL,
				PR_GLOBAL_THREAD,
				PR_JOINABLE_THREAD,
				0);
			
    if (thread == NULL)
	goto cleanup;

    // the MainThread runs the server at this point, while we wait for
    // the WaitSignalThread to end

    PR_JoinThread(thread);

    if (webserver_run_status == PR_SUCCESS) {
        SuspendHttpdService();
        bReturn = TRUE;
    }
	
cleanup:
    if(!bReturn)
        LogErrorEvent(NULL, EVENTLOG_ERROR_TYPE, 0, MSG_BAD_STARTUP, "Initialization Failed", "");

    WebServer::Cleanup();

    TerminateLogging(hEventLog);

    if (phEvents[WS_SHUTDOWN])
        CLOSEHANDLE(phEvents[WS_SHUTDOWN]);

    if (phEvents[WS_RECONFIG])
        CLOSEHANDLE(phEvents[WS_RECONFIG]);

    return bReturn;
}

NSAPI_PUBLIC VOID ResumeHttpdService()
{
    return;
}

/* service cleanup */

NSAPI_PUBLIC VOID CleanupHttpdService()
{
    MainThread = NULL;
    ereport_terminate();
    return;
}

NSAPI_PUBLIC VOID SuspendHttpdService()
{
    ereport(LOG_INFORM, XP_GetAdminStr(DBT_ntmagnuserror3));

    /* daemon_terminate blocks until the server dies */
    WebServer::Terminate();   

    CleanupHttpdService();
    return;
}
