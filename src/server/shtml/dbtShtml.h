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

/**************************************************************************/
/* CONFIDENTIAL AND PROPRIETARY SOURCE CODE                               */
/* OF NETSCAPE COMMUNICATIONS CORPORATION                                 */
/*                                                                        */
/* Copyright © 1996,1997 Netscape Communications Corporation.  All Rights */
/* Reserved.  Use of this Source Code is subject to the terms of the      */
/* applicable license agreement from Netscape Communications Corporation. */
/*                                                                        */
/* The copyright notice(s) in this Source Code does not indicate actual   */
/* or intended publication of this Source Code.                           */
/**************************************************************************/

#define LIBRARY_NAME "shtml"

static char dbtShtmlId[] = "$DBT: shtml referenced v1 $";

#include "i18n.h"

/* Message IDs reserved for this file: HTTP6000-HTTP6099 */
BEGIN_STR(shtml)
	ResDef( DBT_LibraryID_, -1, dbtShtmlId )
	ResDef( DBT_execHandlerError1, 1, "HTTP6001: Exec Tag not allowed by configuration")
	ResDef( DBT_execHandlerError3, 3, "HTTP6003: no way to service request for %s")
	ResDef( DBT_execHandlerError6, 6, "HTTP6006: error sending script output (%s)")
	ResDef( DBT_execHandlerError7, 7, "HTTP6007: cannot execute command %s (%s)")
	ResDef( DBT_shtmlsafError1, 8, "HTTP6008: shtml init failed")
	ResDef( DBT_shtmlsafError2, 9, "HTTP6009: %d nested SSI requests - maximum is %d.")
	ResDef( DBT_shtmlsafError3, 10, "HTTP6010: you can't compress parsed HTML files")
	ResDef( DBT_shtmlsafError4, 11, "HTTP6011: Did not find Cached Entry")
	ResDef( DBT_shtmlsafError5, 12, "HTTP6012: Unable to create ShtmlPage object")
	ResDef( DBT_shtmlsafError6, 13, "HTTP6013: Shtml Page Execution failed")
	ResDef( DBT_fileInfoHandlerError1, 14, "HTTP6014: can't stat %s (%s)")
END_STR(shtml)
