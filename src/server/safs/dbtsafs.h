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

#define LIBRARY_NAME "safs"

static char dbtsafsid[] = "$DBT: safs referenced v1 $";

#include "i18n.h"

/* Message IDs reserved for this file: HTTP4000-HTTP4999 */
BEGIN_STR(safs)
    ResDef(DBT_aclNameNotDef, 1, "HTTP4001: ACL name %s not defined")
    ResDef(DBT_aclpcheckACLStateErr1, 2, "HTTP4002: 3.0 error, ACL %s directive %d")
    ResDef(DBT_addlogError2, 9, "HTTP4009: Cannot get security info for session")
    ResDef(DBT_authError1, 10, "HTTP4010: missing parameter (need type, userdb, and userfn)")
    ResDef(DBT_authError4, 13, "HTTP4013: cannot open group file %s")
    ResDef(DBT_authError5, 14, "HTTP4014: cannot open buffer to group file %s")
    ResDef(DBT_authError6, 15, "HTTP4015: can't open basic password file %s (%s)")
    ResDef(DBT_authError7, 16, "HTTP4016: can't open buffer from password file %s (%s)")
    ResDef(DBT_authError8, 17, "HTTP4017: user %s password did not match pwfile %s")
    ResDef(DBT_authError9, 18, "HTTP4018: user %s does not exist in pwfile %s")
    ResDef(DBT_clauthError1, 19, "HTTP4019: No mapping for certificate to a user")
    ResDef(DBT_clauthError6, 24, "HTTP4024: SSL v3 must be enabled to use client authentication.")
    ResDef(DBT_clauthError7, 25, "HTTP4025: Unable to get certificate. Client is not using SSL v3.")
    ResDef(DBT_clauthError8, 26, "HTTP4026: SSL operation failed (%s)")
    ResDef(DBT_clauthError9, 27, "HTTP4027: Unexpected end of file. Client may have closed connection")
    ResDef(DBT_clauthError10, 28, "HTTP4028: Error completing handshake (%s)")
    ResDef(DBT_clauthError11, 29, "HTTP4029: Connection closed by client.")
    ResDef(DBT_clauthError12, 30, "HTTP4030: Timeout while waiting for client certificate.")
    ResDef(DBT_clauthError13, 31, "HTTP4031: Unexpected error receiving data (%s)")
    ResDef(DBT_clauthError14, 32, "HTTP4032: Cannot buffer client data")
    ResDef(DBT_clauthError15, 33, "HTTP4033: %s: invalid pattern in \"method\" argument.")
    ResDef(DBT_clauthError16, 34, "HTTP4034: get-client-cert requires that security and SSL3 be enabled.")
    ResDef(DBT_clauthError17, 35, "HTTP4035: Certificate conversion to base64 failed.")
    ResDef(DBT_flexLogError1, 37, "HTTP4037: cannot find log named %s")
    ResDef(DBT_cgiError1, 44, "HTTP4044: the CGI program %s did not produce a valid header (%s)")
    ResDef(DBT_cgiError2, 45, "HTTP4045: client sent bad content-length (%s)")
    ResDef(DBT_cgiError3, 46, "HTTP4046: error sending content to script (%s)")
    ResDef(DBT_cgiError4, 47, "HTTP4047: could not initialize CGI subsystem with Cgistub path %s (%s)")
    ResDef(DBT_cgiError6, 49, "HTTP4049: cannot find CGI program %s (%s)")
    ResDef(DBT_cgiError7, 50, "HTTP4050: cannot execute %s (is a directory)")
    ResDef(DBT_cgiError8, 51, "HTTP4051: rejecting request due to dangerous characters (&,|,<,>) requested for .bat script")
    ResDef(DBT_cgiError9, 52, "HTTP4052: could not create STDOUT pipe (%s)")
    ResDef(DBT_cgiError17, 60, "HTTP4060: ignoring CGI status line %s (no 3-digit code)")
    ResDef(DBT_cgiError19, 62, "HTTP4062: error sending script output (%s)")
    ResDef(DBT_cgiError21, 64, "HTTP4064: can't find %s for execution")
    ResDef(DBT_cgiError22, 65, "HTTP4065: can't find file association of %s for execution")
    ResDef(DBT_cgiError25, 68, "HTTP4068: cannot execute CGI script %s (%s)")
    ResDef(DBT_cgiError26, 69, "HTTP4069: cannot initialize CGI execution subsystem (%s)")
    ResDef(DBT_cgiError45, 88, "HTTP4088: failed to get associated file for %s (Error %d)")
    ResDef(DBT_preencError1, 89, "HTTP4089: %s%s not found")
    ResDef(DBT_preencError2, 90, "HTTP4090: can't find %s (%s)")
    ResDef(DBT_preencError3, 91, "HTTP4091: error opening %s (%s)")
    ResDef(DBT_preencError4, 92, "HTTP4092: error opening buffer from %s (%s)")
    ResDef(DBT_ntransError1, 108, "HTTP4108: can't open password file %s (%s)")
    ResDef(DBT_ntransError2, 109, "HTTP4109: missing parameter (need from)")
    ResDef(DBT_ntransError3, 110, "HTTP4110: could not find home directory for user %s")
    ResDef(DBT_ntransError4, 111, "HTTP4111: missing parameter (need from and dir)")
    ResDef(DBT_ntransError5, 112, "HTTP4112: missing parameter (need root)")
    ResDef(DBT_ntransError6, 113, "HTTP4113: missing parameter (need from and url)")
    ResDef(DBT_ntransError7, 114, "HTTP4114: missing parameter (need name)")
    ResDef(DBT_ntwincgiError1, 115, "HTTP4115: the CGI program %s did not produce a valid header (%s)")
    ResDef(DBT_ntwincgiError2, 116, "HTTP4116: cannot find CGI program %s (%s)")
    ResDef(DBT_ntwincgiError3, 117, "HTTP4117: cannot execute %s (is a directory)")
    ResDef(DBT_ntwincgiError4, 118, "HTTP4118: could not start new process (%s)")
    ResDef(DBT_ntwincgiError5, 119, "HTTP4119: ignoring CGI status line %s (no 3-digit code)")
    ResDef(DBT_ntwincgiError6, 120, "HTTP4120: unable to create filebuffer (%s)")
    ResDef(DBT_ntwincgiError7, 121, "HTTP4121: client sent bad content-length (%s)")
    ResDef(DBT_ntwincgiError8, 122, "HTTP4122: error sending content to script (%s)")
    ResDef(DBT_ntwincgiError9, 123, "HTTP4123: wait for CGI child failed (%s)")
    ResDef(DBT_ntwincgiError10, 124, "HTTP4124: wait for CGI child timed out (%s)")
    ResDef(DBT_ntwincgiError11, 125, "HTTP4125: Could not open pipe (%s)")
    ResDef(DBT_ntwincgiError12, 126, "HTTP4126: error sending script output (%s)")
    ResDef(DBT_ntwincgiError13, 127, "HTTP4127: Unknown error occurred while sending script output (%d)")
    ResDef(DBT_ntwincgiError14, 128, "HTTP4128: missing parameter (need exp)")
    ResDef(DBT_pcheckError1, 129, "HTTP4129: denying existence of %s")
    ResDef(DBT_pcheckError2, 130, "HTTP4130: missing parameter (need flist)")
    ResDef(DBT_pcheckError3, 131, "HTTP4131: not full path: %s")
    ResDef(DBT_pcheckError4, 132, "HTTP4132: missing parameter (need extension)")
    ResDef(DBT_pcheckError5, 133, "HTTP4133: missing parameter (need auth-type)")
    ResDef(DBT_pcheckError6, 134, "HTTP4134: missing parameter (need realm)")
    ResDef(DBT_pcheckError7, 135, "HTTP4135: missing parameter (need disable)")
    ResDef(DBT_pcheckError8, 136, "HTTP4136: while servicing %s, lstat of %s failed (%s)")
    ResDef(DBT_pcheckError9, 137, "HTTP4137: while servicing %s, stat of %s failed (%s)")
    ResDef(DBT_pcheckError10, 138, "HTTP4138: will not follow link %s")
    ResDef(DBT_pcheckError11, 139, "HTTP4139: missing parameter (need dest)")
    ResDef(DBT_pcheckError12, 140, "HTTP4140: missing parameter (need cache-control)")
    ResDef(DBT_serviceError1, 141, "HTTP4141: %s%s not found")
    ResDef(DBT_serviceError2, 142, "HTTP4142: can't find %s (%s)")
    ResDef(DBT_serviceError3, 143, "HTTP4143: error opening %s (%s)")
    ResDef(DBT_serviceError4, 144, "HTTP4144: error sending %s (%s)")
    ResDef(DBT_serviceError5, 145, "HTTP4145: missing parameter (need path)")
    ResDef(DBT_serviceError6, 146, "HTTP4146: error opening buffer from %s (%s)")
    ResDef(DBT_serviceError7, 147, "HTTP4147: missing parameter (need trailer)")
    ResDef(DBT_serviceError8, 148, "HTTP4148: can't fstat %s (%s)")
    ResDef(DBT_serviceError9, 149, "HTTP4149: cannot read imagemap file %s (%s)")
    ResDef(DBT_serviceError10, 150, "HTTP4150: server not configured to allow the MIME type %s from this resource")
    ResDef(DBT_serviceError11, 151, "HTTP4151: Server too busy to service function %s (out of native threads in pool)")
    ResDef(DBT_aclsafsEreport1, 152, "HTTP4152: safs init: server root is not set")
    ResDef(DBT_aclsafsEreport2, 153, "HTTP4153: error reading configuration file: %s")
    ResDef(DBT_aclsafsEreport3, 154, "HTTP4154: safs init: failed to parse %s. Reason: %s")
    ResDef(DBT_aclsafsEreport4, 155, "HTTP4155: safs init: default database should have been set by now")
    ResDef(DBT_addlogEreport1, 159, "HTTP4159: common-log: cannot find log named %s")
    ResDef(DBT_authEreport1, 160, "HTTP4160: get_auth_user_basic: unable to get request address %d")
    ResDef(DBT_authEreport2, 161, "HTTP4161: get_auth_user_basic: unable to get session address %d")
    ResDef(DBT_authEreport3, 162, "HTTP4162: get_user_login_basic: getter returned NULL authorization header")
    ResDef(DBT_authEreport5, 164, "HTTP4164: get_authorization_basic: unable to get request address: %d")
    ResDef(DBT_authEreport6, 165, "HTTP4165: No spaces are allowed in the username field: '%s'")
    ResDef(DBT_clauthEreport2, 178, "HTTP4178: get_user_cert_ssl: unable to get request address: %d")
    ResDef(DBT_clauthEreport3, 179, "HTTP4179: get_auth_user_ssl: unable to get request address: %d")
    ResDef(DBT_clauthEreport4, 180, "HTTP4180: get_auth_user_ssl: unable to get database name %d")
    ResDef(DBT_clauthEreport5, 181, "HTTP4181: get_auth_user_ssl: unable to get parsed database %s")
    ResDef(DBT_clauthEreport13, 189, "HTTP4189: get_auth_user_ssl: unable to map cert to LDAP entry. Reason: %s, Issuer: \"%s\", User: \"%s\"")
    ResDef(DBT_clauthEreport14, 190, "HTTP4190: get_auth_user_ssl: unable to get session address %d\n")
    ResDef(DBT_flexlogereport8, 198, "HTTP4198: access log entry truncated (log entry would be greater than %d characters)")
    ResDef(DBT_initereport1, 200, "HTTP4200: register_module: module init function is missing\n")
    ResDef(DBT_initereport2, 201, "HTTP4201: register_module: could find module init function\n")
    ResDef(DBT_initereport3, 202, "HTTP4202: register_module: Failed to register module \"%s\"")
    ResDef(DBT_nsfcsafEreport1, 203, "HTTP4203: %s: parameter %s has invalid value \"%s\"")
    ResDef(DBT_nsfcsafEreport2, 204, "HTTP4204: %s: parameter %s (units %s) has invalid units specification \"%s\"")
    ResDef(DBT_nsfcsafEreport3, 205, "HTTP4205: file cache module initialization failed (version %d, %d)")
    ResDef(DBT_nsfcsafEreport4, 206, "HTTP4206: file cache API version is %d (version %d expected)")
    ResDef(DBT_nsfcsafEreport5, 207, "HTTP4207: file cache module initialized (API versions %d through %d)")
    ResDef(DBT_nsfcsafEreport6, 208, "HTTP4208: error accessing configuration file %s (%s)")
    ResDef(DBT_nsfcsafEreport7, 209, "HTTP4209: %s: value missing for parameter name %s")
    ResDef(DBT_nsfcsafEreport8, 210, "HTTP4210: %s: missing '=' after parameter name %s (line skipped)")
    ResDef(DBT_nsfcsafEreport9, 211, "HTTP4211: %s: value missing for parameter name %s (line skipped)")
    ResDef(DBT_nsfcsafEreport10, 212, "HTTP4212: %s: extra text after %s parameter value ignored")
    ResDef(DBT_nsfcsafEreport11, 213, "HTTP4213: %s: unknown parameter %s ignored")
    ResDef(DBT_nsfcsafEreport15, 217, "HTTP4217: file cache stopped by command")
    ResDef(DBT_nsfcsafEreport16, 218, "HTTP4218: file cache started by command")
    ResDef(DBT_ntwincgiereport1, 219, "HTTP4219: wincgi: unable to create keylist pblock")
    ResDef(DBT_ntwincgiereport2, 220, "HTTP4220: wincgi: out of memory")
    ResDef(DBT_ntwincgiereport3, 221, "HTTP4221: wincgi: unable to create Form External tmp file (%s)")
    ResDef(DBT_ntwincgiereport4, 222, "HTTP4222: wincgi: error writing Form External tmp file (%s)")
    ResDef(DBT_ntwincgiereport5, 223, "HTTP4223: wincgi: field name is too long (%d)\n")
    ResDef(DBT_ntwincgiereport6, 224, "HTTP4224: wincgi: Unknown content-encoding type for POST data.")
    ResDef(DBT_ntwincgiereport7, 225, "HTTP4225: wincgi_fill_cgi failure in filling [CGI] section")
    ResDef(DBT_ntwincgiereport8, 226, "HTTP4226: wincgi_fill_accept failure in filling [Accept] section")
    ResDef(DBT_ntwincgiereport9, 227, "HTTP4227: wincgi_fill_system failure in filling [System] section")
    ResDef(DBT_ntwincgiereport10, 228, "HTTP4228: wincgi_fill_extra_headers failure in filling [Extra Headers] section")
    ResDef(DBT_ntwincgiereport11, 229, "HTTP4229: wincgi_fill_form failure")
    ResDef(DBT_ntwincgiereport12, 230, "HTTP4230: send-wincgi: %s is not a valid URL")
    ResDef(DBT_ntwincgiereport13, 231, "HTTP4231: cgi_send: wincgi_initialize_request failed")
    ResDef(DBT_ntwincgiereport14, 232, "HTTP4232: cgi_send: wincgi_start_exec %s failed")
    
    ResDef(DBT_pcheckError13, 238, "HTTP4238: missing parameter (need virtual-index)")
    ResDef(DBT_pcheckError14, 239, "HTTP4239: uri value not found")

    ResDef(DBT_uploaderror1, 240, "HTTP4240: can't find %s for removal")
    ResDef(DBT_uploaderror2, 241, "HTTP4241: can't remove %s because it isn't a file")
    ResDef(DBT_uploaderror3, 242, "HTTP4242: can't remove %s (%s)")
    ResDef(DBT_uploaderror4, 243, "HTTP4243: client asked to rename %s but did not provide a new name")
    ResDef(DBT_uploaderror5, 244, "HTTP4244: can't resolve URI %s into a filesystem path")
    ResDef(DBT_uploaderror6, 245, "HTTP4245: can't rename %s to %s (%s)")
    ResDef(DBT_uploaderror7, 246, "HTTP4246: can't create directory %s because it already exists")
    ResDef(DBT_uploaderror8, 247, "HTTP4247: can't create directory %s (%s)")
    ResDef(DBT_uploaderror9, 248, "HTTP4248: can't remove directory %s (%s)")
    ResDef(DBT_uploaderror10, 249, "HTTP4249: can't index non-directory %s")
    ResDef(DBT_uploaderror11, 250, "HTTP4250: cannot open directory %s (%s)")
    ResDef(DBT_nsconfigerror1, 251, "HTTP4251: read of %s%c%s failed (%s)")
    ResDef(DBT_nsconfigerror2, 252, "HTTP4252: syntax error in %s/%s line %d: files directives must surround other directives")
    ResDef(DBT_nsconfigerror3, 253, "HTTP4253: syntax error in %s/%s line %d: missing parameter (need type)")
    ResDef(DBT_nsconfigerror4, 254, "HTTP4254: syntax error in %s/%s line %d: missing parameter (need one of dbm, userfile, or realm)")
    ResDef(DBT_nsconfigerror5, 255, "HTTP4255: syntax error in %s/%s line %d: unknown directive %s")
    ResDef(DBT_nsconfigerror6, 256, "HTTP4256: denying access to %s")
    ResDef(DBT_nsconfigerror7, 257, "HTTP4257: no basedir parameter, and none found from name trans")
    ResDef(DBT_indexerror1, 265, "HTTP4265: cannot open directory %s (%s)")
    ResDef(DBT_indexerror2, 266, "HTTP4266: error sending index (%s)")
    ResDef(DBT_indexerror3, 267, "HTTP4267: format string length must be less than %d")
    ResDef(DBT_digestEreport3, 270, "HTTP4270: get_user_login_digest: getter returned NULL authorization header")
    ResDef(DBT_MinCGIStubsSetTo, 273, "HTTP4273: minimum number of Cgistub children is %d") /* extracted from ChildExec.cpp */
    ResDef(DBT_MaxCGIStubsSetTo, 274, "HTTP4274: maximum number of Cgistub children is %d") /* extracted from ChildExec.cpp */
    ResDef(DBT_CGIStubIdleTimeoutSetTo, 275, "HTTP4275: Cgistub reap interval is %d milliseconds") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecNOLISTENER, 276, "HTTP4276: Cgistub reaper: no listening Cgistub") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecLISTENERBUSY, 277, "HTTP4277: Cgistub reaper: Cgistub listener busy") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecStartListener, 278, "HTTP4278: starting Cgistub listener") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecExitStartListener, 279, "HTTP4279: returning from Cgistub listener init with res = %d ioerr = %d") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecShutdownListener, 280, "HTTP4280: shutting down Cgistub listeners") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecShutdownListenerAfterPerformListenerOp, 281, "HTTP4281: asked Cgistub listener to terminate") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecExitShutdownListener, 282, "HTTP4282: done shutting down Cgistub listeners") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecListenerThreadMainLoop, 283, "HTTP4283: Listener thread is %d") /* extracted from ChildExec.cpp */
    ResDef(DBT_ChildExecExitListenerThreadMainLoop, 284, "HTTP4284: Exiting listener thread") /* extracted from ChildExec.cpp */
    ResDef(DBT_addlogEreport2, 285, "HTTP4285: clf-init: Log file %s filename %s is invalid for virtual server %s") /* extracted from addlog.cpp */
    ResDef(DBT_authEreport18, 286, "HTTP4286: get_auth_user_basic: cannot find request.") /* extracted from auth.cpp */
    ResDef(DBT_clauthEreport15, 290, "HTTP4290: get_auth_user_ssl: client passed no certificate.") /* extracted from clauth.cpp */
    ResDef(DBT_flexLogError2, 291, "HTTP4291: flex-init: Log file %s filename %s is invalid for virtual server %s") /* extracted from flexlog.cpp */
    ResDef(DBT_flexLogError3, 292, "HTTP4292: flex-init: Log file %s should be removed before changing its format") /* extracted from flexlog.cpp */
    ResDef(DBT_logbufError1, 293, "HTTP4293: could not create new thread (%s)") /* extracted from logbuf.cpp */
    ResDef(DBT_nsfcsafEreport17, 300, "HTTP4300: file cache is disabled")
    ResDef(DBT_nsfcsafEreport18, 301, "HTTP4301: file cache initialization failed")
    ResDef(DBT_nsfcsafEreport19, 302, "HTTP4302: file cache has been initialized")
    ResDef(DBT_nsfcsafEreport20, 303, "HTTP4303: Failed to create file cache")
    ResDef(DBT_nsfcsafEreport21, 304, "HTTP4304: nsfc-cache-init: This version of the Web Server does not allow setting MaxFiles to a greater value than the default of %d.")
    ResDef(DBT_nsfcsafEreport22, 305, "HTTP4305: nsfc-cache-init: This version of the Web Server does not allow setting HashInitSize to a greater value than the default of %d.")
    ResDef(DBT_shtmlError1, 306, "HTTP4306: file parameters can't contain ../, ./, or //, and must not be absolute paths: %s")
    ResDef(DBT_shtmlError2, 307, "HTTP4307: no way to service request for %s")
    ResDef(DBT_shtmlError5, 310, "HTTP4310: cannot execute command %s (%s)")
    ResDef(DBT_shtmlError6, 311, "HTTP4311: while sending script output (%s)")
    ResDef(DBT_shtmlError7, 312, "HTTP4312: malformed parameters to tag: %s")
    ResDef(DBT_shtmlError8, 313, "HTTP4313: unknown size format %s")
    ResDef(DBT_shtmlError9, 314, "HTTP4314: parameter missing (tag echo needs parameter var)")
    ResDef(DBT_shtmlError10, 315, "HTTP4315: parameter missing (tag redirect needs parameter url)")
    ResDef(DBT_shtmlError11, 316, "HTTP4316: exec tag used but not allowed in %s")
    ResDef(DBT_shtmlError12, 317, "HTTP4317: can't stat %s (%s)")
    ResDef(DBT_shtmlError13, 318, "HTTP4318: unknown tag %s")
    ResDef(DBT_shtmlError14, 319, "HTTP4319: tags may only be %d bytes long")
    ResDef(DBT_shtmlError15, 320, "HTTP4320: read from %s failed (%s)")
    ResDef(DBT_shtmlError16, 321, "HTTP4321: you can't compress parsed HTML files")
    ResDef(DBT_shtmlError17, 322, "HTTP4322: error opening %s (%s)")
    ResDef(DBT_shtmlError18, 323, "HTTP4323: can't fstat %s (%s)")
    ResDef(DBT_shtmlError19, 324, "HTTP4324: error opening buffer from %s (%s)")
    ResDef(DBT_uploadError1, 325, "HTTP4325: handle_chunk_upload: Invalid chunk length.")
    ResDef(DBT_uploadError2, 326, "HTTP4326: handle_chunk_upload: Write chunk failure.")
    ResDef(DBT_uploadError3, 327, "HTTP4327: handle_chunk_upload: Partial write 'chunk' failure.")
    ResDef(DBT_uploadError4, 328, "HTTP4328: get_chunk_header: Failed to obtain chunk header.")
    ResDef(DBT_uploadError5, 329, "HTTP4329: get_chunk_header: Exceeded header max line %d.")
    ResDef(DBT_ntransWarning1, 330, "HTTP4330: removed trailing / from %s/ for virtual server %s")
    ResDef(DBT_QOS_VSBandwidthX, 331, "HTTP4331: Virtual server bandwidth limit of %d exceeded. Current VS bandwidth : %d")
    ResDef(DBT_QOS_VSConnX, 332, "HTTP4332: Virtual server connection limit of %d exceeded. Current VS connections : %d")
    ResDef(DBT_QOS_VSClassBandwidthX, 333, "HTTP4333: Virtual server class bandwidth limit of %d exceeded. Current VSCLASS bandwidth : %d")
    ResDef(DBT_QOS_VSClassConnX, 334, "HTTP4334: Virtual server class connection limit of %d exceeded. Current VSCLASS connections : %d")
    ResDef(DBT_QOS_GlobalBandwidthX, 335, "HTTP4335: Global bandwidth limit of %d exceeded. Current global bandwidth : %d")
    ResDef(DBT_QOS_GlobalConnX, 336, "HTTP4336: Global connection limit of %d exceeded. Current global connections : %d")
    ResDef(DBT_InvalidExpressionNameValue, 337, "HTTP4337: invalid expression: %s=\"%s\"")
    ResDef(DBT_NeedFilter, 338, "HTTP4338: missing parameter (need filter)")
    ResDef(DBT_CantFindFilterX, 339, "HTTP4339: cannot find filter %s")
    ResDef(DBT_ErrorInsertingFilterX, 340, "HTTP4340: error inserting filter %s")
    ResDef(DBT_NeedFilters, 341, "HTTP4341: missing parameter (need filters)")
    ResDef(DBT_CannotOrderNonexistentFilterX, 342, "HTTP4342: could not set filter %s order (Unknown filter)")
    ResDef(DBT_FiltersMissingClosingParen, 343, "HTTP4343: filters: missing closing ')'")
    ResDef(DBT_NeedUriOrFile, 344, "HTTP4344: missing parameter (need uri or file)")
    ResDef(DBT_InvalidOrderForX, 345, "HTTP4345: the requested filter order for filter %s may result in incorrect server operation")

    ResDef(DBT_invalidBooleanValue, 346, "HTTP4346: Invalid value specified for parameter %s")
    ResDef(DBT_invalidFragmentSize, 347, "HTTP4347: A minimum value of 1024 required for parameter fragment-size")
    ResDef(DBT_invalidCompressionLevel, 348, "HTTP4348: Invalid value for parameter compression-level. An integer value between 1 and 9, inclusive, is required.")
    ResDef(DBT_invalidWindowSize, 349, "HTTP4349: Invalid value for parameter window-size. An integer value between 9 and 15, inclusive, is required.")
    ResDef(DBT_invalidMemoryLevel, 350, "HTTP4350: Invalid value for parameter memory-level. An integer value between 1 and 9, inclusive, is required.")
    ResDef(DBT_zlibInitFailure, 351, "HTTP4351: zlib initialization failed. Return code from zlib is %d")
    ResDef(DBT_zlibInternalError, 352, "HTTP4352: zlib internal error. Return code from zlib is %d")
    ResDef(DBT_NeedSed, 353, "HTTP4353: missing parameter (need sed)")
    ResDef(DBT_FnError, 354, "%s error")
    ResDef(DBT_invalidMinSize, 355, "HTTP4355: Invalid value for parameter min-size. An integer value between %d and %d, inclusive, is required.")
    ResDef(DBT_invalidMaxSize, 356, "HTTP4356: Invalid value for parameter max-size. An integer value between %d and %d, inclusive, is required.")
    ResDef(DBT_tempfileFailure, 357, "HTTP4357: could not create temporary file for %s. (%s)")
    ResDef(DBT_zlibgzopenFailure, 358, "HTTP4358: could not open file %s using zlib's gzopen(). (%s)")
    ResDef(DBT_zlibgzsetparamsFailure, 359, "HTTP4359: zlib gzsetparams() failed. Return code from zlib is %d.")
    ResDef(DBT_zlibgzwriteFailure, 360, "HTTP4360: could not write to file %s using zlib's gzwrite(). (%s)")
    ResDef(DBT_systemfopenROFailure, 361, "HTTP4361: could not open file %s in read only mode. (%s)")
    ResDef(DBT_systemfreadFailure, 362, "HTTP4362: could not read file %s. (%s).")
    ResDef(DBT_renameFailure, 363, "HTTP4363: could not rename file %s to %s. (%s)")
    ResDef(DBT_dircreateallFailure, 364, "HTTP4364: could not create directory structure %s. (%s)")
    ResDef(DBT_reqlimitCantWork, 365, "HTTP4365: check-request-limits: Neither max-rps nor max-connections given, skipping limit enforcement")
    ResDef(DBT_reqlimitBadContinue, 366, "HTTP4366: check-request-limits: Invalid value for continue")
    ResDef(DBT_reqlimitNoTimeout, 367, "HTTP4367: check-request-limits-init: No valid timeout given")
    ResDef(DBT_noVarX, 368, "Reference to undefined variable \"%s\"")
    ResDef(DBT_noMapXSubscriptY, 369, "Reference to undefined variable \"%s{'%s'}\"")
    ResDef(DBT_httpNeedUrlOrUrlPrefix, 370, "HTTP4370: missing parameter (need url or url-prefix)")
    ResDef(DBT_httpNeedUri, 371, "HTTP4371: missing parameter (need uri)")
    ResDef(DBT_coreFileXLineYNameZNoValue, 372, "CORE4372: error parsing file %s, line %d: name without value: \"%s\"")
    ResDef(DBT_badLookupFileX, 373, "Unable to access %s")
    ResDef(DBT_coreProgXWriteErrorY, 374, "CORE4374: error sending to %s (%s)")
    ResDef(DBT_coreProgXReadErrorY, 375, "CORE4375: error receiving from %s (%s)")
    ResDef(DBT_coreProgXSentNul, 376, "CORE4376: received unexpected nul character from %s")
    ResDef(DBT_coreBadLineXProgY, 377, "CORE4377: received malformed line \"%s\" from %s")
    ResDef(DBT_coreXBytesProgYNoEol, 378, "CORE4378: received %d bytes from %s without an end of line")
    ResDef(DBT_notLocalUriX, 379, "%s is not a local URI")
    ResDef(DBT_coreExecXErrorY, 380, "CORE4380: cannot execute %s (%s)")
    ResDef(DBT_programXFailed, 381, "Error communicating with %s")
    ResDef(DBT_coreOpenFileXErrorY, 382, "CORE4382: error opening %s (%s)")
    ResDef(DBT_coreReadFileXErrorY, 383, "CORE4383: error reading %s (%s)")
    ResDef(DBT_childStdoutLenXStrY, 384, "CORE4384: stdout: %-.*s")
    ResDef(DBT_childStderrLenXStrY, 385, "CORE4385: stderr: %-.*s")
    ResDef(DBT_reqlimitAboveMaxRPS, 386, "HTTP4386: max-rps exceeded (%d > %d), rejecting requests matching bucket [%s] during next interval")
    ResDef(DBT_recursiveDashUUriX, 387, "-U '%s' would result in a infinite loop")
    ResDef(DBT_childBadExe, 388, "The specified program is not a Windows executable")
    ResDef(DBT_missingPathParameter, 389, "HTTP4389: missing parameter (need path)")
    ResDef(DBT_NeedShlib, 390, "CORE4390: missing parameter (need shlib)")
    ResDef(DBT_NeedMessage, 391, "CORE4391: missing parameter (need message)")
    ResDef(DBT_LogPrefix, 392, "CORE4392: log message: ")
    ResDef(DBT_MalformedPathX, 393, "CORE4393: malformed path: %s")
    ResDef(DBT_pcheckError15, 394, "HTTP4394: missing parameter (need bong-file)")
    ResDef(DBT_nsfcsafEreport23, 395, "HTTP4395: Failed to set the max open descriptor in file cache")
    ResDef(DBT_flexLogError4, 396, "HTTP4396: Log file %s should be removed before changing its format (ASCII <--> Binary)")
    ResDef(DBT_flexLogError5, 397, "HTTP4397: Binary log file version mismatch error. Please remove the log file and restart the server" )
    ResDef(DBT_digestTooMany, 398, "HTTP4398: Too many parameters in Digest Authorization header (possibly an attack). Dropping excess parameters. Authentication in progress will fail")
END_STR(safs)
