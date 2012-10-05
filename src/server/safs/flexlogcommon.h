/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2009 Sun Microsystems, Inc. All rights reserved.
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
#ifndef FLEX_LOG_COMMON_H
#define FLEX_LOG_COMMON_H 1

#include "netsite.h"
#include "support/NSString.h"
#include "time/nstime.h"
#include "base/util.h"
#include "base/date.h"
#include "frame/log.h"
#include "frame/object.h"
#include "safs/flexlog.h"

#define BIN_LOG_VERSION "1.0"
#define BIN_LOG_VERSION_MAX_SIZE 10
#define BIN_LOG_VERSION_PREFIX "binlog-version="
#define BIN_LOG_VERSION_PREFIX_LEN (sizeof(BIN_LOG_VERSION_PREFIX) -1)
#define BIN_LOG_VERSION_LINE_LEN BIN_LOG_VERSION_PREFIX_LEN + BIN_LOG_VERSION_MAX_SIZE + ENDLINE_LEN

/*
 * FORMAT_PREFIX is the prefix for the format line in a log file.  The format
 * line records the log format in use in that log file.
 */
#define FORMAT_PREFIX "format="

/*
 * FORMAT_PREFIX_LEN is the number of characters in FORMAT_PREFIX (not
 * including any trailing '\0).
 */
#define FORMAT_PREFIX_LEN (sizeof(FORMAT_PREFIX) - 1)

/*
 * FORMAT_LEN is the maximum length of the format string in a log file's
 * format line, not including the "format=" prefix or trailing EOL.
 */
#define FORMAT_LEN (FORMAT_LINE_LEN - FORMAT_PREFIX_LEN - ENDLINE_LEN)

/*
 * FORMAT_LINE_LEN is the maximum length of the format line in a log file,
 * including the "format=" prefix and trailing EOL.
 */
#define FORMAT_LINE_LEN 2048

/*
 * TIME_PREFIX is the prefix for the optional time line in a log file.  The
 * time line records the base time from which %RELATIVETIME% values are
 * computed.
 */
#define TIME_PREFIX "time="

/*
 * TIME_PREFIX_LEN is the number of characters in TIME_PREFIX (not including
 * any trailing '\0).
 */
#define TIME_PREFIX_LEN (sizeof(TIME_PREFIX) - 1)

/*
 * TIME_LINE_LEN is the maximum length of the time line in a log file,
 * including the "time=" prefix and trailing EOL.
 */
#define TIME_LINE_LEN (TIME_PREFIX_LEN + UTIL_I64TOA_SIZE + ENDLINE_LEN)

/*
 * ENDLINE_LEN is the number of characters in an EOL (not including any
 * trailing '\0').
 */
#define ENDLINE_LEN (sizeof(ENDLINE) - 1)

/*
 * flex_scan_percent_token groks the following tokens.
 */
#define SYSDATE                                "SYSDATE"
#define LOCALEDATE                             "LOCALEDATE"
#define TIME                                   "TIME"
#define RELATIVETIME                           "RELATIVETIME"
#define SES_CLIENT_IP                          "Ses->client.ip"
#define REQ_METHOD_NUM                         "Req->method_num"
#define REQ_PROTV_NUM                          "Req->protv_num"
#define REQ_REQPB_CLF_REQUEST                  "Req->reqpb.clf-request"
#define REQ_REQPB_CLF_REQUEST_METHOD           "Req->reqpb.clf-request.method"
#define REQ_REQPB_CLF_REQUEST_URI              "Req->reqpb.clf-request.uri"
#define REQ_REQPB_CLF_REQUEST_URI_ABS_PATH     "Req->reqpb.clf-request.uri.abs_path"
#define REQ_REQPB_CLF_REQUEST_URI_QUERY        "Req->reqpb.clf-request.uri.query"
#define REQ_REQPB_CLF_REQUEST_PROTOCOL         "Req->reqpb.clf-request.protocol"
#define REQ_REQPB_CLF_REQUEST_PROTOCOL_NAME    "Req->reqpb.clf-request.protocol.name"
#define REQ_REQPB_CLF_REQUEST_PROTOCOL_VERSION "Req->reqpb.clf-request.protocol.version"
#define REQ_REQPB_METHOD                       "Req->reqpb.method"
#define REQ_SRVHDRS_CLF_STATUS                 "Req->srvhdrs.clf-status"
#define REQ_SRVHDRS_STATUS_CODE                "Req->srvhdrs.status.code"
#define REQ_SRVHDRS_STATUS_REASON              "Req->srvhdrs.status.reason"
#define REQ_SRVHDRS_CONTENT_LENGTH             "Req->srvhdrs.content-length"
#define REQ_HEADERS_REFERER                    "Req->headers.referer"
#define REQ_HEADERS_USER_AGENT                 "Req->headers.user-agent"
#define REQ_VARS_AUTH_USER                     "Req->vars.auth-user"
#define VSID                                   "vsid"
#define DURATION                               "duration"
#define SUBSYSTEM                              "subsystem"
#define REQ_HEADERS_COOKIE_PREFIX              "Req->headers.cookie."
#define REQ_HEADERS_COOKIE_PREFIX_LEN          (sizeof(REQ_HEADERS_COOKIE_PREFIX) - 1)
#define RECORD_SIZE                            "RECORD_SIZE"

/*
 * FlexTokenType identifies the type of token that was parsed from a flex-log
 * format string.
 */
enum FlexTokenType {
    TOKEN_TEXT_OFFSET,
    TOKEN_TEXT_POINTER,
    TOKEN_MODEL,
    TOKEN_SYSDATE,
    TOKEN_LOCALEDATE,
    TOKEN_TIME,
    TOKEN_RELATIVETIME,
    TOKEN_IP,
    TOKEN_DNS,
    TOKEN_METHOD_NUM,
    TOKEN_PROTV_NUM,
    TOKEN_REQUEST_LINE,
    TOKEN_REQUEST_LINE_URI,
    TOKEN_REQUEST_LINE_URI_ABS_PATH,
    TOKEN_REQUEST_LINE_URI_QUERY,
    TOKEN_REQUEST_LINE_PROTOCOL,
    TOKEN_REQUEST_LINE_PROTOCOL_NAME,
    TOKEN_REQUEST_LINE_PROTOCOL_VERSION,
    TOKEN_METHOD,
    TOKEN_STATUS_CODE,
    TOKEN_STATUS_REASON,
    TOKEN_CONTENT_LENGTH,
    TOKEN_REFERER,
    TOKEN_USER_AGENT,
    TOKEN_AUTH_USER,
    TOKEN_VSID,
    TOKEN_DURATION,
    TOKEN_SUBSYSTEM,
    TOKEN_COOKIE,
    TOKEN_PB_KEY,
    TOKEN_PB_NAME,
    TOKEN_SN_CLIENT_KEY,
    TOKEN_SN_CLIENT_NAME,
    TOKEN_RQ_VARS_KEY,
    TOKEN_RQ_VARS_NAME,
    TOKEN_RQ_REQPB_KEY,
    TOKEN_RQ_REQPB_NAME,
    TOKEN_RQ_HEADERS_KEY,
    TOKEN_RQ_HEADERS_NAME,
    TOKEN_RQ_SRVHDRS_KEY,
    TOKEN_RQ_SRVHDRS_NAME,
    TOKEN_RECORD_SIZE
};

/*
 * FlexToken describes a token that was parsed from a flex-log format string.
 */
struct FlexToken {
    FlexTokenType type;
    char *p;
    int offset;
    int len;
    ModelString *model;
    const pb_key *key;
};

/*
 * FlexFormat defines the format of a log file.
 */
struct FlexFormat {
    FlexFormat *next;  // next FlexFormat in flex_format_list
    char *format;      // flex-log format string
    FlexToken *tokens; // parsed tokens
    int ntokens;       // number of elements in tokens[]
    PRBool accel;      // set if tokens can be retrieved from the accelerator
    PRBool binary;     // set if binary logging is enabled
    int refcount;      // number of references (excluding flex_format_list's)
};

// external functions used
#ifdef __cplusplus
extern "C"
{
#endif

NSAPI_PUBLIC const char *flex_scan_percent_token(FlexFormat *f, const char *p);
NSAPI_PUBLIC const char *flex_scan_dollar_token(FlexFormat *f, const char *p);
NSAPI_PUBLIC const char *flex_scan_text_token(FlexFormat *f, const char *p);
NSAPI_PUBLIC int flex_format_64_wrapper(pool_handle_t *pool, PRInt64 v, const char **p);
NSAPI_PUBLIC int flex_format_time(pool_handle_t *pool, time_t tim, int type, const char **p);


#ifdef __cplusplus
}
#endif

#endif
