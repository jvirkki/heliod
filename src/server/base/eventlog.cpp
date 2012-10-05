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
//       Copyright (C) 1995-1997 Netscape Communications Corporation        //
//--------------------------------------------------------------------------//
//                                                                          //
//  Name: EVENTLOG                                                          //
//	Platforms: WIN32                                                        //
//  ......................................................................  //
//  Revision History:                                                       //
//  01-12-95  Initial Version, Aruna Victor (aruna@netscape.com)            //
//  12-02-96  Code cleanup, Andy Hakim (ahakim@netscape.com)                //
//            - consolidated admin and http functions into one              //
//            - moved registry modification code to installer               //
//            - removed several unecessary functions                        //
//            - changed function parameters to existing functions           //
//                                                                          //
//--------------------------------------------------------------------------//

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "netsite.h"
#include "base/eventlog.h"
#include "frame/conf.h"
#include <nt/regparms.h>
#include <nt/messages.h>

HANDLE ghEventSource;

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC HANDLE InitializeLogging(const char *szEventLogName)
{
    ghEventSource = RegisterEventSource(NULL, szEventLogName);
    return ghEventSource;
}



NSAPI_PUBLIC BOOL TerminateLogging(HANDLE hEventSource)
{
    BOOL bReturn = FALSE;
    if(hEventSource == NULL)
        hEventSource = ghEventSource;
    if(hEventSource)
        bReturn = DeregisterEventSource(hEventSource);
    return(bReturn);
}



NSAPI_PUBLIC BOOL LogErrorEvent(HANDLE hEventSource, WORD fwEventType, WORD fwCategory, DWORD IDEvent, LPCTSTR chMsg, LPCTSTR lpszMsg)
{
    BOOL bReturn = FALSE;
    LPCTSTR lpszStrings[2];

	lpszStrings[0] = chMsg;
    lpszStrings[1] = lpszMsg;

    if(hEventSource == NULL)
        hEventSource = ghEventSource;

    if(hEventSource)
        bReturn = ReportEvent(hEventSource, fwEventType, fwCategory,
                        IDEvent, NULL, 2, 0, lpszStrings, NULL);
    return(bReturn);
}

NSPR_END_EXTERN_C
