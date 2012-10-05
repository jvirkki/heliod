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
 *
 */
/* 
 * Binary access log format
 *
 * The default format is the CLF (common log file) format which is
 * "%Ses->client.ip% - %Req->vars.auth-user% [%SYSDATE%] \"%Req->reqpb.clf-request%\" %Req->srvhdrs.clf-status% 
 * %Req->srvhdrs.content-length%"
 *
 * A binary format file consists of a new ASCII header "binlog-version=1.0".
 * In later releases, this version may get changed.
 *
 * The records have the following structure:
 * RECORD_LEN : PRInt32 4 bytes followed by the RECORD itself.
 * For CLF a typical binary record will be :
 * 
 * TOKEN                   DATA TYPE   WHAT IT CONTAINS
 * ----------------------------------------------------
 * TOKEN_RECORD_SIZE       PRInt32     record length
 * TOKEN_IP                String\0    Ses->client "ip"
 * TOKEN_AUTH_USER         String\0    rq->vars    "auth-user"
 * TOKEN_SYSDATE           PRInt64     time when log entry was created
 * TOKEN_REQUEST_LINE      String\0    rq->reqpb   "clf-request"
 * TOKEN_STATUS_CODE       short       rq->srvhdrs "clf-status"
 * TOKEN_CONTENT_LENGTH    PRInt64     rq->srvhdrs "content-length"
 * 
 * Cosmetic stuff in format (of type TEXT_TOKEN_OFFSET) like spaces, quotes,
 * brackets, endline, etc. will not be written in binary log. They will be 
 * added by the binlog reader.
 *
 * What other formats do
 *
 * TOKEN                               DATA TYPE   WHAT IT CONTAINS
 * -----------------------------------------------------------------
 * TOKEN_TEXT_OFFSET                   None        Nothing
 * TOKEN_METHOD_NUM                    short       rq->method_num
 * TOKEN_PROTV_NUM                     short       rq->protv_num
 * TOKEN_TEXT_POINTER                  String      ??
 * TOKEN_MODEL                         String\0    model
 * TOKEN_REFERER                       String\0    rq->headers "referer"
 * TOKEN_USER_AGENT                    String\0    rq->headers "user-agent"
 * TOKEN_VSID                          String\0    virtual server id
 * TOKEN_DNS                           String\0    DNS of the client
 * TOKEN_REQUEST_LINE_URI              String\0    Uri part from rq line
 *                                                 (includes query string)
 * TOKEN_REQUEST_LINE_URI_ABS_PATH     String\0    Path part from rq line
 *                                                 (excludes query string)
 * TOKEN_REQUEST_LINE_URI_QUERY        String\0    Query string from rq line
 * TOKEN_REQUEST_LINE_PROTOCOL         String\0    Rq line protocol 
 *                                                 For example "HTTP/1.0\0"
 * TOKEN_REQUEST_LINE_PROTOCOL_NAME    String\0    Rq line protocol name 
 *                                                 For example "HTTP\0"
 * TOKEN_REQUEST_LINE_PROTOCOL_VERSION String\0    Rq line protocol number
 *                                                 For example "1.0\0"
 * TOKEN_METHOD                        String\0    rq->reqpb "method"
 * TOKEN_STATUS_REASON                 String\0    Reason in rq->srvhdrs "status"
 * TOKEN_SUBSYSTEM                     String\0    "NSAPI\0"
 * TOKEN_COOKIE                        String\0    rq->headers "cookie"
 * TOKEN_PB_KEY                        String\0    pb key
 * TOKEN_PB_NAME                       String\0    pb value
 * TOKEN_SN_CLIENT_NAME                String\0    sn->client
 * TOKEN_SN_CLIENT_KEY                 String\0    sn->client key
 * TOKEN_RQ_VARS_KEY                   String\0    rq->vars for key
 * TOKEN_RQ_VARS_NAME                  String\0    rq->vars value
 * TOKEN_RQ_REQPB_KEY                  String\0    rq->reqpb key
 * TOKEN_RQ_REQPB_NAME                 String\0    rq->reqpb value
 * TOKEN_RQ_HEADERS_KEY                String\0    rq->headers key
 * TOKEN_RQ_HEADERS_NAME               String\0    rq->headers value
 * TOKEN_RQ_SRVHDRS_KEY                String\0    rq->srvhdrs key
 * TOKEN_RQ_SRVHDRS_NAME               String\0    rq->srvhdrs value
 * TOKEN_DURATION                      PRUint64    duration
 * TOKEN_LOCALEDATE                    PRInt64     current time 
 * TOKEN_TIME                          PRInt64     current time
 * TOKEN_RELATIVETIME                  PRInt64     current time minus time from
 *                                                 "time= ..." header line
 */
#include <stdio.h>
#include "netsite.h"
#include "support/NSString.h"
#include "time/nstime.h"
#include "base/util.h"
#include "base/date.h"
#include "frame/log.h"
#include "frame/object.h"
#include "safs/flexlog.h"
#include "safs/flexlogcommon.h"
#include "base/pool.h"
#ifdef XP_WIN32
#include "wingetopt.h"
#endif

// T could be short, PRInt64 or PRUint64
template <class T>
inline PRInt32 convert(char *p, T& i)
{
    PRInt32 l =  sizeof(i);
    memcpy(&i, p, l);
    return l;
}

class BinLog
{
public :
    BinLog();
   ~BinLog();
   PRInt32 readLog(const char *filename);
private :
    SYS_FILE _fd;

    char *_recordBuf;
    PRInt32 _currRecordBufSize;

    NSString _line;

    char _headerBuf[FORMAT_LINE_LEN + 1];
    PRInt32 _headerBufSize;

    NSString _time;
    NSString _version;
    NSString _format;

    FlexFormat *_f;

    PRInt32 openFile(const char *filename);
    PRInt32 parseLogFileHeaders(void);
    PRInt32 parseRecord(void);
    PRInt32 readNextRecord(void);
    PRInt32 readTillENDLINE(const char *prefix, NSString &val);
};

BinLog::BinLog(void)
{
    _currRecordBufSize = 0;

    // use the same size working buffer for format, data, version
    // format line is the biggest line
    _headerBufSize = FORMAT_LINE_LEN + 1;
    PR_ASSERT(FORMAT_LINE_LEN > TIME_LINE_LEN);
    PR_ASSERT(FORMAT_LINE_LEN > BIN_LOG_VERSION_LINE_LEN);

    _recordBuf = NULL;
    _f = NULL;
    _fd = SYS_ERROR_FD;
}

BinLog::~BinLog(void)
{
    if (_recordBuf)
        PERM_FREE(_recordBuf);

    // Destroy the FlexFormat
    for (int ti = 0; ti < _f->ntokens; ti++) {
         if (_f->tokens[ti].p)
             PERM_FREE(_f->tokens[ti].p);
         model_str_free(_f->tokens[ti].model);
    }
    if (_f->tokens)
        PERM_FREE(_f->tokens);
    if (_f->format)
        PERM_FREE(_f->format);
    PERM_FREE(_f);
    _f = NULL;

    if (_fd)
        PR_Close(_fd);

}


PRInt32 BinLog::parseRecord(void)
{
    // note we already have read sizeof(PRInt32) record length
    char *p = _recordBuf;

    for (int ti = 0; ti < _f->ntokens; ti++) {
        const FlexToken *t = &_f->tokens[ti];

        PRInt32 len;
        short s;
        const char *arr = NULL;
        PRInt64 i;
        PRUint64 u;
        time_t tim;

        switch (t->type) {
        case TOKEN_TEXT_OFFSET:
                _line.append(_f->format + t->offset, t->len);
            break;
        case TOKEN_TEXT_POINTER:
                len = t->len;
                _line.append(p, len);
                p += len;
             break;
        case TOKEN_SYSDATE:
        case TOKEN_LOCALEDATE:
                p += convert(p, i);

                tim = (time_t)i;

                if (flex_format_time(NULL, tim, t->type, &arr) == -1) {
                     fprintf(stderr, "Error converting time %lld to string\n",
                             i);
                     return -1;
                }
                _line.append(arr);
                pool_free(NULL, (void *)arr);
            break;
        // time_t
        case TOKEN_TIME:
        case TOKEN_RELATIVETIME:
                p += convert(p, i);

                tim = (time_t)i;

                if (flex_format_64_wrapper(NULL,(PRInt64) tim, &arr) == -1) {
                    fprintf(stderr, "Error converting time %lld to string\n",
                            i);
                    return -1;
                }
                _line.append(arr);
                pool_free(NULL, (void *)arr);
            break;

        // strings
        case TOKEN_MODEL:
        case TOKEN_IP:
        case TOKEN_AUTH_USER:
        case TOKEN_DNS:
        case TOKEN_REQUEST_LINE:
        case TOKEN_REQUEST_LINE_URI:
        case TOKEN_REQUEST_LINE_URI_ABS_PATH:
        case TOKEN_REQUEST_LINE_URI_QUERY:
        case TOKEN_REQUEST_LINE_PROTOCOL:
        case TOKEN_REQUEST_LINE_PROTOCOL_NAME:
        case TOKEN_REQUEST_LINE_PROTOCOL_VERSION:
        case TOKEN_METHOD:
        case TOKEN_STATUS_REASON:
        case TOKEN_REFERER:
        case TOKEN_USER_AGENT:
        case TOKEN_VSID:
        case TOKEN_SUBSYSTEM:
        case TOKEN_COOKIE:
        case TOKEN_PB_KEY:
        case TOKEN_PB_NAME:
        case TOKEN_SN_CLIENT_KEY:
        case TOKEN_SN_CLIENT_NAME:
        case TOKEN_RQ_VARS_KEY:
        case TOKEN_RQ_VARS_NAME:
        case TOKEN_RQ_REQPB_KEY:
        case TOKEN_RQ_REQPB_NAME:
        case TOKEN_RQ_HEADERS_KEY:
        case TOKEN_RQ_HEADERS_NAME:
        case TOKEN_RQ_SRVHDRS_KEY:
        case TOKEN_RQ_SRVHDRS_NAME:
            if (p && p[0] != '\0') {
                _line.append(p);
                len = strlen(p) + 1;
                p += len;
            } else {
                _line.append("-");
                p++;
            }
            break;
        // short
        case TOKEN_METHOD_NUM:
        case TOKEN_PROTV_NUM:
        case TOKEN_STATUS_CODE:
                p += convert(p, s);
                _line.printf("%d", s);
            break;

        // PRInt64
        case TOKEN_CONTENT_LENGTH:
                p += convert(p, i);

                // convert PRInt64 to string
                if (flex_format_64_wrapper(NULL, i, &arr) == -1) {
                    fprintf(stderr, 
                            "Error converting content length %lld to string\n",
                            i);
                    return -1;
                }
                _line.append(arr);
                pool_free(NULL, (void *)arr);
            break;

        // PRUint64
        case TOKEN_DURATION:
                p += convert(p, u);

                // convert PRUint64 to string
                char arr1[30];
                memset(arr1, '\0', sizeof(arr1));
                if (PR_snprintf(arr1, sizeof(arr1), "%llu", u) != -1)
                    _line.append(arr1);
            break;

        case TOKEN_RECORD_SIZE:
            // Field to add buffer length do nothing as it is already read
            break;
        default:
            PR_ASSERT(0);
            break;
        }
    }
    return 0;
}

PRInt32 BinLog::readTillENDLINE(const char *prefix, NSString &val)
{

    memset(_headerBuf, '\0', _headerBufSize);

    // read ...=.... (till endline)
    char c = 'a';
    char *p = _headerBuf;
    PRInt32 totalBytesRead = 0;
    PRInt32 bytes = 0;
    do {
        bytes = PR_Read(_fd, (void *)&c, sizeof(char));
        if (bytes == 0)
            break;
        if (bytes != sizeof(char)) {
            fprintf(stderr, "Error reading %s... line. %d bytes read.\n",
                    prefix, bytes);
            return -1;
        }
        *p = c;
        p++;
        totalBytesRead++;
    } while ((c != ENDLINE[0]) && (totalBytesRead < _headerBufSize));

    // If no bytes are read error
    if (totalBytesRead <= 0) {
        fprintf(stderr, "Error reading %s... line.\n", prefix);
        return -1;
    }

    // read ENDLINE
    // ENDLINE is '\r\n' (windows) and is '\n' on Unix
    if (c == ENDLINE[0] && ENDLINE_LEN > 1) {
        for (int j = 0; j < ENDLINE_LEN - 1; j++) {
            bytes = PR_Read(_fd, (void *)p, sizeof(char));
            if (bytes != sizeof(char)) {
                fprintf(stderr, "Error reading end of line.\n");
                return -1;
            }
            p++;
        }
    }

    // if it doesn't contain ENDLINE return error
    const char *eol = strstr(_headerBuf, ENDLINE);
    if (eol == NULL) {
        fprintf(stderr,
                "Error reading %s... line. It should end with ENDLINE\n",
                prefix);
        return -1;
    }

    // If _headerBuf doesn't start with ...= return error
    if (strncmp(_headerBuf, prefix, strlen(prefix)) != 0) {
        fprintf(stderr, "Error reading %s line.\n", prefix);
        return -1;
    }

    // value should contain the actual format minus format prefix
    const char *suffix = _headerBuf + strlen(prefix);
    PRInt32 suffix_len =  eol - suffix;
    val.append(suffix, suffix_len);

    printf("%s", _headerBuf);
    return 0;
}

PRInt32 BinLog::parseLogFileHeaders(void)
{

    // read and parse the format= line
    if (readTillENDLINE(FORMAT_PREFIX, _format) == -1)
        return -1;

    // The time line is needed only when %RELATIVETIME% is used
    if (strstr(_format.data(), RELATIVETIME) != NULL) {
        if (readTillENDLINE(TIME_PREFIX, _time) == -1)
            return -1;
    }

    // read and compare binlog-version= line
    if (readTillENDLINE(BIN_LOG_VERSION_PREFIX, _version) == -1)
        return -1;

    // compare version number
    if ((_version.length() < 1) ||
        (strcmp(_version, BIN_LOG_VERSION) != 0)) {
        fprintf(stderr,
                "Error log reader version mismatch. Log reader version is (%s) where as Access Log writer version is %s\n",
                BIN_LOG_VERSION, _version.data());
        return -1;
    }

    // Create a new FlexFormat for the requested format string
    _f = (FlexFormat *)PERM_MALLOC(sizeof(FlexFormat));
    if (_f == NULL)
        return -1;

    _f->format = PERM_STRDUP(_format.data());
    if (_f->format == NULL)
        return -1;
    _f->tokens = NULL;
    _f->ntokens = 0;

    // Parse the format string into tokens
    const char *p = _f->format;
    while (*p) {
        if (*p == '%') {
            p = flex_scan_percent_token(_f, p);
        } else if (*p == '$') {
            p = flex_scan_dollar_token(_f, p);
        } else {
            p = flex_scan_text_token(_f, p);
        }
    }

    return 0;
}

PRInt32 BinLog::readNextRecord(void)
{
    // read record size (number of bytes to read)
    PRInt32 len = 0;
    PRInt32 bytes =  PR_Read(_fd, (void*)&len, sizeof(PRInt32));
    if (bytes == 0)
        return 0;
    if (bytes != sizeof(PRInt32)) {
        fprintf(stderr, "Error reading record length (%d)\n", bytes);
        return bytes;
    }

    // len should not be greater than access log buffer size max limit
    if (len > 1048576) {
            fprintf(stderr, 
                    "Error access log line can not  greater than %d bytes. Record size %d bytes.\n",
                    1048576, len);
            return -1;
    }
 
    // check if buffer is big enuf else realloc
    if (_currRecordBufSize < (len + 1)) {
        _recordBuf = (char *)PERM_REALLOC(_recordBuf, len + 1);
        if (_recordBuf == NULL) {
            fprintf(stderr, "Error realloc %d bytes\n", bytes);
            return -1;
        }
        _currRecordBufSize = len + 1;
    }
    memset(_recordBuf, '\0', _currRecordBufSize);

    // read those many bytes
    bytes = PR_Read(_fd, (void *)_recordBuf, len - sizeof(PRInt32));
    if (bytes != (len - sizeof(PRInt32))) {
        fprintf(stderr, "Error reading record buffer (%d)\n", bytes);
        return bytes;
    }

    return len;
}

PRInt32 BinLog::openFile(const char *filename)
{

    if (filename == NULL) {
        _fd = PR_STDIN;
        return 0;
    }

    // If the log file exists and is a plain file...
    PRFileInfo64 finfo;
    if (PR_GetFileInfo64(filename, &finfo) != PR_SUCCESS) {
        fprintf(stderr, "Error file %s doesn't exist.\n", filename);
        return -1;
    }
    if (finfo.type != PR_FILE_FILE) {
        fprintf(stderr, "Error file %s is not a plain file.\n", filename);
        return -1;
    }

    // Open the log file
    _fd = system_fopenRO(filename);
    if (_fd == SYS_ERROR_FD) {
        fprintf(stderr, "Error opening file %s.\n", filename);
        return -1;
    }
    return 0;
}

PRInt32 BinLog::readLog(const char *filename)
{

    if (openFile(filename) == -1)
        return -1;

    if (parseLogFileHeaders() == -1)
        return -1;

    PRInt32 len = 0;
    _currRecordBufSize = 4096;
    _recordBuf = (char *)PERM_MALLOC(sizeof(char) * _currRecordBufSize);
    if (_recordBuf == NULL)
        return -1;

    do {
         _line.clear();
         len = readNextRecord();
         if (len <= 0)
             break;
         parseRecord();
         printf("%s%s", _line.data(), ENDLINE);
    } while (len > 0);

    return 0;
}

void printUsage(char *prog)
{
    printf("%s version %s\nUsage: %s [-i file] [-v]\n",
            prog, BIN_LOG_VERSION, prog);
    printf(" [-i file]: Input log file name  Default: stdin\n");
    printf(" [-v]: prints the version of %s\n", prog);
}

int main(int argc, char **argv)
{
    PRInt32 len = 0;

    char *filename = NULL;
    char *program = argv[0];

    if (argc > 1) {
        int o;

        while((o = getopt(argc, argv, "hvi:")) != -1) {
            switch(o) {
                case 'i':
                    if (optarg)
                        filename = PERM_STRDUP(optarg);
                    else {
                        fprintf(stderr, "Error please specify log filename\n");
                        exit(1);
                    }
                    break;
                case 'v':
                    printf("%s version %s\n", program, BIN_LOG_VERSION);
                    return 0;
                    break;
                case 'h':
                default:
                    printUsage(program);
                    exit(1);
                    break;
            }
        }
        // ./binlog ./access case
        if (filename == NULL) {
            printUsage(program);
            exit(1);
        }
    }

    BinLog *log = new BinLog();
    log->readLog(filename);
    delete log;

    if (filename)
        PERM_FREE(filename);

    return 0;
}

