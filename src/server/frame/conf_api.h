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

/* GLOBAL_FUNCTIONS:
 * DESCRIPTION:
 * Access primitives to server configuration parameters.  These values are
 * initially loaded from magnus.conf; but once the server is running, 
 * NSAPI routines may add/delete their own name/value pairs from this list.
 *
 * RESTRICTIONS:
 * This API is designed for global configuration parameters and should not
 * be used for saving state.
 * This API is not thread safe.
 */

#ifndef _CONF_API_H_
#define _CONF_API_H_

#include "prtypes.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* FUNCTION: conf_findGlobal
 * DESCRIPTION:
 *    Initialize globals; used internally.
 */
NSAPI_PUBLIC PRBool conf_initGlobal(void);

/* FUNCTION: conf_findGlobal
 * DESCRIPTION:
 *    Searches for <name> within the name-value pairs listed in
 *    the globals area.
 * INPUTS:
 *    name - the name of the name-value pair to search for
 * OUTPUTS:
 *    none
 * RETURNS:
 *    NULL if not found
 *    or the value associated with name
 */
NSAPI_PUBLIC char *conf_findGlobal(char *name);

/* FUNCTION: conf_setGlobal
 * DESCRIPTION:
 *    If the name does not already exist in the globals area, the name value 
 *    pair is added to the globals.
 *    If the name does already exist, then the value for the name/value pair
 *    is replaced with the new value.
 * INPUTS:
 *    name - the name to insert
 *    value - the value to insert
 * OUTPUTS:
 *    none
 * RETURNS:
 *    PR_TRUE on success
 *    PR_FALSE on failure
 */
NSAPI_PUBLIC PRBool conf_setGlobal(char *name, char *value);

/* FUNCTION: conf_deleteGlobal
 * DESCRIPTION:
 *    Removes a name-value pair from the globals area.
 * INPUTS:
 *    name - the name of the name/value pair to delete
 * OUTPUTS:
 * RETURNS:
 *    PR_TRUE- on success
 *    PR_FALSE- on failure (the name was not already in the globals area)
 */
NSAPI_PUBLIC PRBool conf_deleteGlobal(char *name);

/* Hostname of the Mail Transport Agent :
 * typically "localhost" for the Unix boxes,
 * or the hostname of a POP server otherwise.
 * Needed by the Agents subsystem.
 */
NSAPI_PUBLIC extern const char *GetMtaHost(void);
NSAPI_PUBLIC extern const char *SetMtaHost(const char *HostName);
NSAPI_PUBLIC extern const char *GetNntpHost(void);
NSAPI_PUBLIC extern const char *SetNntpHost(const char *HostName);

NSAPI_PUBLIC void conf_warnunaccessed(void);
NSAPI_PUBLIC void conf_warnduplicate(void);

#if defined(__cplusplus)
}
#endif

#endif /* _CONF_API_H_ */
