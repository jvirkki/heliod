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

#define LIBRARY_NAME "frame"

static char dbtframeid[] = "$DBT: frame referenced v1 $";

#include "i18n.h"

/* Message IDs reserved for this file: CORE2000-CORE2999, HTTP2000-HTTP2999, CONF2000-CONF2999 */
BEGIN_STR(frame)
	ResDef( DBT_LibraryID_, -1, dbtframeid )/* extracted from dbtframe.h*/
	ResDef(DBT_CheckMsg_,1,"<HEAD><META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;charset=ISO-8859-1\"><TITLE>Not Found</TITLE></HEAD>\n<H1>Not Found</H1> The requested object does not exist on this server. The link you followed is either outdated, inaccurate, or the server has been instructed not to let you have it. ")/*extracted from error.cpp*/
	ResDef(DBT_RefererMsg_,2,"Please inform the site administrator of the <A HREF=\"%s\">referring page</A>.")/*extracted from error.cpp*/
	ResDef(DBT_ProtocolBadRequestMsg_2,10,"Your browser sent a query this server could not understand.")/*extracted from error.cpp*/
	ResDef(DBT_ProtocolUnauthorizedMsg_2,11,"Proper authorization is required for this area. Either your browser does not perform authorization, or your authorization has failed.")/*extracted from error.cpp*/
	ResDef(DBT_ProtocolForbiddenMsg_2,12,"Your client is not allowed to access the requested object.")/*extracted from error.cpp*/
	ResDef(DBT_ProtocolServerErrorMsg_2,13,"This server has encountered an internal error which prevents it from fulfilling your request. The most likely cause is a misconfiguration. Please ask the administrator to look for messages in the server's error log.")/*extracted from error.cpp*/
	ResDef(DBT_ProtocolNotImplementedMsg_2,14,"This server does not implement the requested method.")/*extracted from error.cpp*/
	ResDef(DBT_ProtocolDefaultErrorMsg_2,15,"An error has occurred.")/*extracted from error.cpp*/
	ResDef(DBT_ProtocolLengthRequiredMsg_2,17,"The server cannot accept the supplied request message body. Please retry the request with a Content-Length header.")
	ResDef(DBT_HtmlHeadTitleSTitleHeadNBodyH1SH_,18,"<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n<BODY><H1>%s</H1>\n%s\n</BODY></HTML>")/*extracted from error.cpp*/
	ResDef( DBT_cannotFindTemplateS_, 20, "HTTP2020: cannot find template %s" )/*extracted from httpact.cpp*/
	ResDef( DBT_noPartialPathAfterObjectProcessi_, 22, "HTTP2022: no partial path after object processing" )/*extracted from httpact.cpp*/
	ResDef( DBT_checkMethod_, 23, "check-method" )
	ResDef( DBT_invalidShexpS_, 24, "HTTP2024: invalid shexp %s" )/*extracted from httpact.cpp*/
	ResDef( DBT_checkType_, 25, "check-type" )
	ResDef( DBT_invalidShexpS_1, 26, "HTTP2026: invalid shexp %s" )/*extracted from httpact.cpp*/
	ResDef( DBT_handleProcessed_, 27, "handle-processed" )/*extracted from httpact.cpp*/
	ResDef( DBT_noWayToServiceRequestForS_, 28, "HTTP2028: no way to service request for %s" )/*extracted from httpact.cpp*/
	ResDef( DBT_noHandlerFunctionGivenForDirecti_, 120, "HTTP2120: no handler function given for directive" )/*extracted from func.cpp*/
	ResDef( DBT_cannotFindFunctionNamedS_, 122, "HTTP2122: cannot find function named %s" )/*extracted from func.cpp*/
	ResDef( DBT_startHttpResponse_, 131, "start-http-response" )/*extracted from http.cpp*/
	ResDef( DBT_writeFailedS_, 132, "HTTP2132: write failed (%s)" )/*extracted from http.cpp*/
	ResDef( DBT_httpStatus_, 135, "http-status" )/*extracted from http.cpp*/
	ResDef( DBT_DIsNotAValidHttpStatusCode_, 136, "HTTP2136: %d is not a valid HTTP status code" )/*extracted from http.cpp*/
    	ResDef(DBT_ProtocolUnauthorizedPWExpiredMsg_2,157,"Your password has expired. Please change it or contact your system administrator.")
	ResDef(DBT_ProtocolRequestTimeoutMsg_2,159,"The server timed out waiting for the client request.")/*extracted from error.cpp*/
	ResDef(DBT_ProtocolURITooLargeMsg_2,161,"The request URI is longer than the server can handle.")/*extracted from error.cpp*/
	ResDef(DBT_confExpectedString,172,"CORE2172: %s parameter ignored (expected string)")
	ResDef(DBT_confExpectedBoolean,173,"CORE2173: %s parameter ignored (expected boolean)")
	ResDef(DBT_confExpectedInteger,174,"CORE2174: %s parameter ignored (expected integer)")
	ResDef(DBT_confExpectedBoundedInteger,175,"CORE2175: %s parameter ignored (expected integer between %d and %d inclusive)")
	ResDef(DBT_confUnaccessed,176,"CORE2176: %s directive ignored")
	ResDef(DBT_confMultiplyDefined,177,"CORE2177: %s directive multiply defined")
	ResDef(DBT_confServerIDSet,178,"CORE2178: Set SERVER_ID to %s")
	ResDef(DBT_confTalkBackInitFailed,179,"CORE2179: TalkBack initializer failed")
	ResDef(DBT_confTalkBackInitSuccess,180,"CORE2180: TalkBack initializer succeeded")
	ResDef(DBT_confApiCallBeforeInit,181,"CORE2181: conf_findGlobal called before conf_api initialized")
	ResDef(DBT_confApiCannotBeUsedInMultPxMode,183, "CORE2183: The %s subsystem cannot be used with multiple daemon processes.  Initialization failed.")
	ResDef(DBT_funcThreadPoolNotFound,184, "CORE2184: Unable to find specified thread pool: %s")
	ResDef(DBT_funcThreadPoolAlreadyDecl,185, "CORE2185: Pool has already been declared: %s")
	ResDef(DBT_funcThreadMaxExceeded,186, "CORE2186: Maximum number of available thread pools has been exceeded: %s, %u")
	ResDef(DBT_funcThreadMinStackError,187, "CORE2187: Stack size can not be set to be less than %u bytes")
	ResDef(DBT_funcThreadPoolIndexError,188, "CORE2188: Unable to allocate thread index for the pool")
	ResDef(DBT_funcGlobalThreadPoolInitError,189, "CORE2189: Failure to initialize global thread pool")
	ResDef(DBT_funcPoolInitFailure,190, "CORE2190: Unable to initialize pool: %s")
	ResDef(DBT_nsapi30UnImplemented,191, "CORE2191: Invalid NSAPI function called.  This could be caused by an invalid version of nsapi.h being used to compile the NSAPI module or by using the wrong set of macro definitions (like defining XP_UNIX on Windows).  Regardless of the cause, the NSAPI module is attempting to call an NSAPI function that is not implemented in this version of the server.")
	ResDef(DBT_unknownMethod,204,"HTTP2204: The request method is unknown to the server.")/*extracted from httpact.cpp*/
	ResDef(DBT_methodNotAllowed,205,"HTTP2205: The request method is not applicable to the requested resource.")/*extracted from httpact.cpp*/
	ResDef(DBT_ProtocolEntityTooLargeMsg_2,206,"A request entity is longer than the server can handle.")/*extracted from error.cpp*/
    ResDef(DBT_ProtocolRequestedRangeNotSatisfiableMsg_2,207,"Requested range not satisfiable.") /*extracted from error.cpp*/
    ResDef(DBT_Objconf_Parsing, 217, "HTTP2217: internal error parsing objects")
    ResDef(DBT_Objconf_OutOfMemory, 220, "HTTP2220: Not enough memory")
	ResDef(DBT_HtmlNoContentTypeTitleBody, 222, "<HTML>\n<HEAD><TITLE>%s</TITLE></HEAD>\n<BODY>\n%s\n</BODY>\n</HTML>")/*extracted from error.cpp*/
    ResDef(DBT_ProtocolBadGatewayMsg_2, 223, "Processing of this request was delegated to a server that is not functioning properly.")
    ResDef(DBT_ProtocolGatewayTimeoutMsg_2, 224, "Processing of this request was delegated to a server that is not functioning properly.")
    ResDef(DBT_statNoPathGiven, 225, "no path given to stat")
    ResDef(DBT_checkQuery_, 226, "check-query")
    ResDef(DBT_ErrorReadingChunkedRequestBody, 227, "Error reading chunked request message body")
    ResDef(DBT_ResponseContentLengthMismatchXofY, 228, "HTTP2228: Response content length mismatch (%lld bytes with a content length of %lld)")
    ResDef(DBT_StageXError, 229, "Error processing %s directives")
    ResDef(DBT_StageXFnYError, 230, "HTTP2230: %s function %s returned an error")
    ResDef(DBT_FilterXDefinesYUnsupportedMethods, 231, "CORE2231: Filter %s defines %d filter method(s) not supported by this server release")
    ResDef(DBT_FilterNameXIsReserved, 232, "Filter name %s is reserved")
    ResDef(DBT_FilterXCannotBeOverriden, 233, "Filter %s cannot be overridden")
    ResDef(DBT_MissingSession, 234, "Missing Session")
    ResDef(DBT_FilterXRequiresNativeThreads, 235, "CORE2235: Filter %s requires native threads but is running on a non-native thread")
    ResDef(DBT_InvalidFilterStack, 236, "Invalid filter stack")
    ResDef(DBT_FilterXCannotBeRemoved, 237, "Filter %s cannot be removed")
    ResDef(DBT_ProtocolLockedMsg_2,238,"The specified resource is locked and the client either is not a lock owner or the lock type requires a lock token to be submitted and the client did not submit it.")
    ResDef(DBT_ProtocolFailedDependencyMsg_2,239,"The requested method could not be performed on the resource because the requested action depended on another action and that action failed.")
    ResDef(DBT_ProtocolInsufficientStorageMsg_2,240,"The requested method could not be performed on the resource because the server is unable to store the representation needed to successfully complete the request.")
    ResDef(DBT_logForHostX, 241, "for host %.*s")
    ResDef(DBT_logTryingToMethodXUriY, 242, "trying to %.*s %.*s")
    ResDef(DBT_logWhileTryingToMethodXUriY, 243, "while trying to %.*s %.*s")
    ResDef(DBT_logCommaSpace, 244, ", ")
    ResDef(DBT_logFunctionXReports, 245, "%s reports: ")
    ResDef(DBT_ProtocolUnsupportedMediaTypeMsg_2,246,"The server does not support the request type of the body.")
    ResDef(DBT_ResponseContentLengthExceeded, 247, "Response content length exceeded")
    ResDef(DBT_RequestUriMismatch, 248, "CORE2248: filename %s does not match requested URI")
    ResDef(DBT_confErrorOpeningFileXBecauseY, 249, "CONF2249: Error opening %s (%s)")
    ResDef(DBT_confErrorReadingFileXBecauseY, 250, "CONF2250: Error reading %s (%s)")
    ResDef(DBT_confFileXLineYNeedValue, 251, "CONF2251: Error parsing file %s, line %d: directive with no value")
    ResDef(DBT_confFileXLineYInvalidZValue, 252, "CONF2252: Error parsing file %s, line %d: invalid %s value")
    ResDef(DBT_confErrorRunningInitXErrorY, 253, "CORE2253: Error running Init function %s: %s")
    ResDef(DBT_confErrorRunningInitX, 254, "CORE2254: Error running Init function %s")
    ResDef(DBT_confUnexpectedEOF, 255, "CONF2255: Unexpected end of file")
    ResDef(DBT_confUnexpectedValueX, 256, "CONF2256: Unexpected value: %s")
    ResDef(DBT_confExpectedX, 257, "CONF2257: Expected \"%s\"")
    ResDef(DBT_confExpectedParameterName, 258, "CONF2258: Expected parameter name")
    ResDef(DBT_confExpectedParameterValue, 259, "CONF2259: Expected parameter value")
    ResDef(DBT_confContainerXExpressionBadErrorY, 260, "CONF2260: Unable to parse <%s> expression: %s")
    ResDef(DBT_confElseIfWithoutIfOrElseIf, 261, "CONF2261: <ElseIf> requires a preceding </If> or </ElseIf>")
    ResDef(DBT_confElseWithoutIfOrElseIf, 262, "CONF2262: <Else> requires a preceding </If> or </ElseIf>")
    ResDef(DBT_confXDirectiveRequireNewLine, 263, "CONF2263: %s directives must begin on a new line")
    ResDef(DBT_confDupFn, 264, "CONF2264: Duplicate fn parameter")
    ResDef(DBT_confNeedFn, 265, "CONF2265: Missing parameter (need fn)")
    ResDef(DBT_confNeedNamePpath, 266, "CONF2266: Missing parameter (need name or ppath)")
    ResDef(DBT_fileXLineYColZParseFailedPrefix, 267, "Error parsing file %s, line %d, column %d: ")
    ResDef(DBT_fileXLineYParseFailedPrefix, 268, "Error parsing file %s, line %d: ")
    ResDef(DBT_fileXParseFailedPrefix, 269, "Error parsing file %s: ")
    ResDef(DBT_confMissingClosingCharX, 270, "CONF2270: Missing closing \"%c\"")
    ResDef(DBT_confUnexpectedCharX, 271, "CONF2271: Unexpected \"%c\"")
    ResDef(DBT_confUnexpectedIntX, 272, "CONF2272: Unexpected \\x%02x")
    ResDef(DBT_missingClosingCharX, 273, "Missing closing \"%c\"")
    ResDef(DBT_unexpectedCharX, 274, "Unexpected \"%c\"")
    ResDef(DBT_unexpectedIntX, 275, "Unexpected \\x%02x")
    ResDef(DBT_unmatchedCharX, 276, "Unmatched \"%c\"")
    ResDef(DBT_expectedX, 277, "Expected \"%s\"")
    ResDef(DBT_unexpectedEndOfExpression, 278, "Unexpected end of expression")
    ResDef(DBT_syntaxError, 279, "Syntax error")
    ResDef(DBT_syntaxErrorNearX, 280, "Syntax error near \"%s\"")
    ResDef(DBT_noOpX, 281, "%s is not the name an operator")
    ResDef(DBT_noControlX, 282, "%s is not the name an operator")
    ResDef(DBT_noMapX, 283, "%s is not the name a map variable")
    ResDef(DBT_noVarX, 284, "Reference to undefined variable %s")
    ResDef(DBT_notEnoughArgs, 285, "Not enough arguments")
    ResDef(DBT_tooManyArgs, 286, "Too many arguments")
    ResDef(DBT_badRegex, 287, "Invalid regular expression")
    ResDef(DBT_badRegexBecauseX, 288, "Invalid regular expression: %s")
    ResDef(DBT_paramNameXErrorY, 289, "%s: %s")
    ResDef(DBT_confBadParameterXBecauseY, 290, "CONF2290: Malformed %s parameter (%s)")
    ResDef(DBT_badEscapeCharX, 291, "\\%c is an invalid escape sequence")
    ResDef(DBT_badEscapeIntX, 292, "\\\\x%02x is an invalid escape sequence")
    ResDef(DBT_confExprEvalErrorX, 293, "CONF2293: Error evaluating expression: %s")
    ResDef(DBT_noBackref, 294, "Backreference $& undefined")
    ResDef(DBT_noBackrefNumX, 295, "Backreference $%d undefined")
    ResDef(DBT_httpChildUriXExceedsDepthY, 296, "HTTP2296: Cannot create child request for %s (Exceeded maximum of %d internal requests)")
    ResDef(DBT_httpRestartUriXExceedsDepthY, 297, "HTTP2297: Cannot restart request for %s (Exceeded maximum of %d restarts)")
    ResDef(DBT_FnXInterpolationErrorY, 298, "HTTP2298: Error interpolating parameters for %s (%s)")
    ResDef(DBT_confFileXLineYDirectiveZDeprecated, 299, "CONF2299: File %s, line %d: the %s directive is deprecated")
    ResDef(DBT_badCookieName, 300, "Invalid cookie name")
    ResDef(DBT_badCookieValue, 301, "Invalid cookie value")
    ResDef(DBT_fnXErrorWithoutStatus, 302, "HTTP2302: Function %s aborted the request without setting the status code")
    ResDef(DBT_ProtocolServiceUnavailableMsg_2, 303, "The server is too busy to respond to your request. Please try again later.")
    ResDef(DBT_confFileXLineYDirectiveZJavaDeprecated, 304, "CONF2304: File %s, line %d: the %s directive is deprecated for Java Enabled Server")
     ResDef(DBT_ProtocolDefault4xxMsg_2, 305, "The server is unable to process your request.")
     ResDef(DBT_ProtocolDefault5xxMsg_2, 306, "The server encountered an error that prevents it from fulfilling your request.")
END_STR(frame)
