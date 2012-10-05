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
 * ntwincgi.c: Handle WinCGI scripts
 *
 * WinCGI is an interface definition for external programs to the server which
 * can be executed to produce documents on the fly.  It differs from CGI in
 * that it uses temporary files to pass data to the subprogram instead of 
 * stdin/stdout.  This is because some programming environments (namely visual
 * basic) do not have stdin and stdout.
 * 
 * WinCGI thinks the best way to handle the work is for the server to do all
 * the parsing.  Why didn't they have a library do this within the wincgi 
 * program? WinCGI just clutters the server; and it is extremely inefficient.  
 * What a complete piece of junk.  Use of wincgi should be strongly discouraged.
 * 
 */

#ifndef MCC_ADMSERV

#include "safs/cgi.h"
#include "safs/dbtsafs.h"
#include "base/pblock.h"
#include "base/lexer.h"         /* lex_token_xxx stuff */
#include "base/session.h"
#include "frame/req.h"
#include "frame/log.h"


#include "base/util.h"      /* environment functions, can_exec */
#include "base/net.h"       /* SYS_NETFD */
#include "base/daemon.h"    /* child_fork */
#include "frame/protocol.h" /* protocol_status */
#include "frame/http.h"     /* http_hdrs2env */
#include "frame/conf.h"     /* server action globals */
#include "frame/conf_api.h"

#include "frame/httpact.h"
#include <frame/objset.h>

#include <base/buffer.h>
#include <base/daemon.h>
#include <sys/types.h>
#include <sys/timeb.h>

#ifdef NET_SSL
#include "ssl.h"
#endif


/* wincgi_request_t
 * Each wincgi request will create a wincgi_request_t and use it to store 
 * request-specific information
 */
#define TMPDIR_LEN	MAX_PATH
typedef struct wincgi_request_t {
	unsigned int	request_id;			/* unique id for this request */
	int				debug;
	int				post;				/* 1 if method=POST, 0 otherwise */
	int				content_length;		/* length of post data, if present */
	char			tmp_dir[TMPDIR_LEN];

	char	*		content_file;
	char	*		data_file;
	char	*		output_file;

    /* Handles for token objects */
	void	*		form_literal;
	void	*		form_external;
	void	*		form_huge;

	PRFileDesc		*content_handle;
	PRFileDesc		*data_handle;
	PRFileDesc		*output_handle;

} wincgi_request_t;

static PRFileDesc *make_temp_file(char *dir, char *suffix, long unique_id, char **filename);
static void _wincgi_cleanup_request(wincgi_request_t *wincgi_req);
extern char *_build_cwd(char *);

#define port conf_getglobals()->Vport

#define PIPE_BUFFERSIZE 8192
#define WIN_CGI_VERSION "CGI/1.3a (Win)"
#define REQ_DONT_PARSE -4

static pblock *wincgi_initenv = NULL;
static LONG GlobalCgiNumber = 0;

static DWORD GlobalTimeout = 0;
static CRITICAL_SECTION cgiCriticalSection;
static BOOLEAN InitCriticalSection = TRUE;

#define ENTER_CGI_CRITICAL_SECTION\
    if (InitCriticalSection) {                    \
        InitializeCriticalSection(&cgiCriticalSection);\
        InitCriticalSection = FALSE;                     \
    }                                                   \
    EnterCriticalSection(&cgiCriticalSection);

#define LEAVE_CGI_CRITICAL_SECTION LeaveCriticalSection(&cgiCriticalSection)

#define GetConfigParm(name, default) \
    (conf_findGlobal(name)?atoi(conf_findGlobal(name)):default)

LONG
GetUniqueCgiNumber()
{
    LONG UniqueNumber;

    ENTER_CGI_CRITICAL_SECTION;
    UniqueNumber = ++GlobalCgiNumber;
    if (GlobalCgiNumber > 100000)
        GlobalCgiNumber = 0;
    LEAVE_CGI_CRITICAL_SECTION;
    return UniqueNumber;
}

/* _get_executable_path()
 * Given the path and path_info fields for this request, Modify the path
 * to truncate out the path_info portion.
 */
char *_get_executable_path(char *path, char *path_info)
{

    char *t = &path[strlen(path)];
    /* find the last occurrence of path_info in path */
    while(1) {
        for(  ; t != path; --t)
            if(*t == FILE_PATHSEP)
                break;

        if(t == path) {
            /* Could not find path_info.This
             * should not happen */ 
            goto RETURN;
        } else if (!strcmp(t, path_info)) {
            *t = '\0';
            goto RETURN;
        } else {
            --t;
        }
    }

RETURN:
    return path;
}

/* --------------------------- wincgi_scan_headers --------------------------- */

#define BAD_CGI(s1, s2) \
    util_sprintf(err, s1, s2); \
    log_error(LOG_FAILURE, "cgi-parse-output", sn, rq, \
              XP_GetAdminStr(DBT_ntwincgiError1), \
              pblock_findval("path", rq->vars), err); \
    return REQ_ABORTED;


int wincgi_scan_headers(Session *sn, Request *rq, filebuf_t *nb)
{
    register int x,y;
    register char c;
    int nh, i;
	filebuf_t *buf = nb;
    char t[REQ_MAX_LINE], err[REQ_MAX_LINE];
	CHAR *DirectReturnHeader = "HTTP/1.0";
	BOOLEAN DirectReturn = TRUE;
	int HeaderTracker = 0;
	int HeaderLength = strlen(DirectReturnHeader);

    nh = 0;
    x = 0; y = -1;

    while(1) {
        if(x > REQ_MAX_LINE) {
            BAD_CGI("CGI header line too long (max %d)", REQ_MAX_LINE);
        }
        if(nh > HTTP_MAX_HEADERS) {
            BAD_CGI("too many headers from CGI script (max %d)",
                        HTTP_MAX_HEADERS);
        }
		i = filebuf_getc(buf);

        if(i == IO_ERROR) {
            BAD_CGI("read failed, error is %s", system_errmsg());
        }
        else if(i == IO_EOF) {
            BAD_CGI("program terminated without a valid CGI header"
            	" (check for core dump or other abnormal termination", "");
        }

        c = (char) i;
        switch(c) {
          case LF:
            if((!x) || ((x == 1) && (t[0] == CR)))
                return REQ_PROCEED;

            if(t[x-1] == CR)
                --x;
            t[x] = '\0';
            if(y == -1) {
                BAD_CGI("name without value: got line \"%s\"", t);
            }else{
				while(t[y] && isspace(t[y])) ++y;

				pblock_nvinsert(t, &t[y], rq->srvhdrs);
			}

            x = 0; y = -1;
            ++nh;
            break;
          case ':':
            if(y == -1) {
                y = x+1;
                c = '\0';
            }
          default:
		  	if (DirectReturn) {
			    if (c != DirectReturnHeader[HeaderTracker]) {
					DirectReturn = FALSE;
				} else {
					if (++HeaderTracker == HeaderLength) {
						/* reset the filebuf length to the original length so that
						 * the cgi program's output will be sent back unaltered.
						 */
						buf->pos = 0;
						return REQ_DONT_PARSE;
					}
				}
			}
           t[x++] = ((y == -1) && isupper(c) ? tolower(c) : c);
        }
    }
}

/* _get_tmp_dir()
 * Figure out which directory to put temp files in 
 */
static int
_get_tmp_dir(char *tmp_dir, LONG unique_id)
{
	DWORD result;
	CHAR dir[TMPDIR_LEN];

	result = GetEnvironmentVariable("TEMP", dir, TMPDIR_LEN);
	if ( (result == 0) || (result > TMPDIR_LEN) ) {
		CreateDirectory("\\temp", NULL);
		strcpy(dir, "\\temp");
	} else {
		struct stat finfo;

		if ( system_stat(dir, &finfo) < 0) {
			CreateDirectory(dir, NULL);
		}
	}


	util_snprintf(tmp_dir, TMPDIR_LEN, "%s\\%d", dir, unique_id);

	if (!CreateDirectory(tmp_dir, NULL)) {
		result = GetLastError();
		if (result != ERROR_ALREADY_EXISTS)
			return -1;
	}
	return 0;
}

/* MakeTempFile()
 * Combines the path, the unique_id, and the suffix to create a new temp file.
 * Opens the new file and returns its PRFileDesc.  Also returns the name of the
 * new file in "filename".
 */
static PRFileDesc *
make_temp_file(char *dir, char *suffix, long unique_id, char **filename)
{

	/* Allocate a little more than necessary */
 	*filename = (char *)MALLOC(strlen(dir)+10 /* max length of unique_id */+
								sizeof(suffix)+5);

	sprintf(*filename, "%s\\%d.%s\0", dir, unique_id, suffix);

	return PR_Open(*filename, PR_RDWR|PR_CREATE_FILE, 0);
}

/* -------------------------- wincgi_start_exec ---------------------------- */

/*
 * Takes a path and request structures, and a pid_t to store the child pid
 * in. Returns -1 on error, and a file descriptor on success.
 * 
 * Note that NPH is no longer taken care of here. Data from the CGI
 * program is still buffered in case SSL is active.
 */

int wincgi_start_exec(Session *sn, Request *rq, 
	wincgi_request_t *wincgi_req, pid_t *pid, char *path)
{
	char *argv0 = NULL;
	char *query_string = NULL;
    struct stat *finfo;
	STARTUPINFO siStartInfo;
	PROCESS_INFORMATION piProcInfo;
	char *ChildCmdLine = NULL;
	SECURITY_ATTRIBUTES saAttr;
	char *dir = NULL;

    if(!(finfo = request_stat_path(path, rq))) {
        log_error(LOG_WARN, "send-wincgi", sn, rq, 
                  XP_GetAdminStr(DBT_ntwincgiError2),
                  path, rq->staterr);
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
    	return -1;
    } 
    else if(!S_ISREG(finfo->st_mode)) {
        log_error(LOG_WARN, "send-wincgi", sn, rq, 
                  XP_GetAdminStr(DBT_ntwincgiError3),
                  path);
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        return -1;
    }

    if((argv0 = strrchr(path, FILE_PATHSEP)) != NULL) {
        argv0++;
    } else {
    	argv0 = path;
	}
	ChildCmdLine = (char *)MALLOC(strlen(argv0) + 
			strlen(wincgi_req->content_file) + 
			strlen(wincgi_req->data_file) + 
			strlen(wincgi_req->output_file) + 5);
	sprintf(ChildCmdLine, "%s %s %s %s", argv0, wincgi_req->data_file, 
		wincgi_req->content_file, wincgi_req->output_file);

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	if (wincgi_req->debug) {
		siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;
		siStartInfo.wShowWindow = SW_SHOW;
	}

	ZeroMemory(&saAttr, sizeof(SECURITY_ATTRIBUTES));
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    /* Temporary fix for CreateProcess to work under Win95 -XXX aruna.
     * CreateProcess returns a FILE_NOT_FOUND error under Win95 if
     * forward slashes are used in the pathname */

    CONVERT_TO_NATIVE_FS(path);
	dir = _build_cwd(path);

	if (!(CreateProcess(path, ChildCmdLine, NULL, NULL, FALSE,
						0, (LPVOID)NULL, dir, &siStartInfo,
						&piProcInfo))) {
		log_error(LOG_FAILURE, "send-wincgi", sn, rq, 
              XP_GetAdminStr(DBT_ntwincgiError4), system_errmsg());
        goto REQ_ABORT;
	}

	*pid = piProcInfo.hProcess;

	FREE(ChildCmdLine);	ChildCmdLine = NULL;
    if (dir) FREE(dir);
	CloseHandle(piProcInfo.hThread);

    return TRUE;

REQ_ABORT:
	if (ChildCmdLine) FREE (ChildCmdLine);
    if (dir) FREE(dir);
	return REQ_ABORTED;
}


/* --------------------------- wincgi_parse_output --------------------------- */


/*
 * Returns -1 on error, 0 for proceed normally, and 1 for dump output and 
 * restart request.
 */

int wincgi_parse_output(filebuf_t *buf, Session *sn, Request *rq)
{
    char *s, *l;
	int result;

    result = wincgi_scan_headers(sn, rq, buf);
    if (result == REQ_ABORTED) {
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return -1;
    }

    l = pblock_findval("location", rq->srvhdrs);

    if((s = pblock_findval("status", rq->srvhdrs))) {
        if((strlen(s) < 3) || (!isdigit(s[0]) || (!isdigit(s[1])) || 
                                                           (!isdigit(s[2]))))
        {
            log_error(LOG_WARN, "send-wincgi", sn, rq, 
                      XP_GetAdminStr(DBT_ntwincgiError5), s);
            s = NULL;
        }
        else {
           char ch = s[3];
           s[3] = '\0';
           int statusNum = atoi(s);
           s[3] = ch;

           rq->status_num = statusNum;
        }
    }
    //
    // Make a rather dubious judgement call on
    // whether this is an internal redirect or not.
    if(l && (!util_is_url(l)))
        return 1;

    if(!s) {
		if (l)
			pblock_nvinsert("url", l, rq->vars); 
        protocol_status(sn, rq, (l ? PROTOCOL_REDIRECT : PROTOCOL_OK), NULL);
	}
	if (result == REQ_DONT_PARSE)
		return -2;

    return 0;
}

/* _wincgi_initialize_request()
 * Initialize a wincgi structure
 * Returns 0 on success, -1 on failure.
 */
static int 
_wincgi_initialize_request(wincgi_request_t *wincgi_req, pblock *pb)
{
	char *tmp;
	
	memset(wincgi_req, 0, sizeof(wincgi_request_t));

	wincgi_req->request_id = GetUniqueCgiNumber();

	wincgi_req->post = 0;

	wincgi_req->tmp_dir[0] = '\0';
	if(_get_tmp_dir(wincgi_req->tmp_dir, wincgi_req->request_id) < 0) {
		return -1;
	}

	wincgi_req->debug = 0;
	if ( (tmp = pblock_findval("debug", pb)) ) {
		if (!strcasecmp(tmp, "yes")) 
			wincgi_req->debug = 1;
	} 
	
	if ( (wincgi_req->content_handle = 
			make_temp_file(
				wincgi_req->tmp_dir, 
				"con", 
				wincgi_req->request_id, 
				&(wincgi_req->content_file))) == 0) {
		goto failure;
	}
	if ( (wincgi_req->data_handle = 
			make_temp_file(
				wincgi_req->tmp_dir, 
				"dat", 
				wincgi_req->request_id, 
				&(wincgi_req->data_file))) == 0) {
		goto failure;
	}
	if ( (wincgi_req->output_handle = 
			make_temp_file(
				wincgi_req->tmp_dir, 
				"out", 
				wincgi_req->request_id, 
				&(wincgi_req->output_file))) == 0) {
		goto failure;
	}

	wincgi_req->form_literal = NULL;
	wincgi_req->form_external = NULL;
	wincgi_req->form_huge = NULL;

	return 0;

failure:
	_wincgi_cleanup_request(wincgi_req);
	return -1;
}

/* _wincgi_cleanup_request()
 * Cleans up a wincgi request structure.  If the structure was dynamically
 * allocated, the caller must actually free the structure itself.
 */
static void 
_wincgi_cleanup_request(wincgi_request_t *wincgi_req) 
{
	if (wincgi_req->content_file) {
		FREE(wincgi_req->content_file);
        wincgi_req->content_file = 0;
    }
	if (wincgi_req->data_file) {
		FREE(wincgi_req->data_file);
        wincgi_req->data_file = 0;
    }
	if (wincgi_req->output_file) {
		FREE(wincgi_req->output_file);
        wincgi_req->output_file = 0;
    }
	if (wincgi_req->content_handle) {
		PR_Close(wincgi_req->content_handle);
        wincgi_req->content_handle = 0;
    }
	if (wincgi_req->data_handle) {
		PR_Close(wincgi_req->data_handle);
        wincgi_req->data_handle = 0;
    }
	if (wincgi_req->output_handle) {
		PR_Close(wincgi_req->output_handle);
        wincgi_req->output_handle = 0;
    }

	if (!wincgi_req->debug) 
		if (wincgi_req->tmp_dir[0])
			util_delete_directory(wincgi_req->tmp_dir, TRUE);

	if (wincgi_req->form_literal) {
		lex_token_destroy(wincgi_req->form_literal);
        wincgi_req->form_literal = 0;
    }
	if (wincgi_req->form_external) {
		lex_token_destroy(wincgi_req->form_external);
        wincgi_req->form_external = 0;
    }
	if (wincgi_req->form_huge) {
		lex_token_destroy(wincgi_req->form_huge);
        wincgi_req->form_huge = 0;
    }
	return;
}

/* _wincgi_fill_cgi()
 * Fills all fields within the [CGI] section of the data file.
 */
#define MAX_OUTPUT_LEN (REQ_MAX_LINE*2)
static int 
_wincgi_fill_cgi(wincgi_request_t *wincgi_req, Session *sn, Request *rq)
{
	char outbuf[MAX_OUTPUT_LEN];
	unsigned int outbuf_len=0;
    unsigned long result;
	char *value, *dns_name, *physical_path = NULL;
	PRFileDesc *cgi_file;

    if (!wincgi_req)
		return -1;

    cgi_file = wincgi_req->data_handle;

    outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN, "[CGI]\r\n");
    if ( outbuf_len &&
		((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
        return FALSE;

	/* Request Protocol */
	if ( (value = pblock_findval("protocol", rq->reqpb)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Request Protocol=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* Request Method */
    if( (value = pblock_findval("method", rq->reqpb)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Request Method=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
			return -1;
	}

	/* Executable Path */
	if ( (value = pblock_findval("uri", rq->reqpb))) {
		char tmp_char;
		char *path_info;
		int index;

		path_info = pblock_findval("path-info", rq->vars);
		if (path_info) {
			index = strlen(value) - strlen(path_info);
			tmp_char = value[index];
			value[index] = '\0';
		}
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Executable Path=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
			return -1;
		if (path_info) 
			value[index] = tmp_char;
	}

	/* Document Root */
	if ( (value = pblock_findval("ntrans-base", rq->vars)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Document Root=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
			return -1;
	}
	
	/* Logical Path / Physical Path */
    if( (value = pblock_findval("path-info", rq->vars)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN, 
			"Logical Path=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;

		
        if ( (value = pblock_findval("uri", rq->reqpb)) &&
			 (physical_path = request_translate_uri(value, sn)) ) {
            outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
                "Physical Path=%s\r\n", physical_path);
            if (outbuf_len &&
				((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
                return -1;
        }
    }

	/* Query String */
    if ( (value = pblock_findval("query", rq->reqpb)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN, 
			"Query String=");
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
        if (strlen(value) &&
			((result = PR_Write(cgi_file, value, strlen(value)) < 0)))
            return -1;
		if ((result = PR_Write(cgi_file, "\r\n", 2)) < 0)
			return -1;
    }

	/* Request Range */
    if ( (value = pblock_findval("range", rq->headers)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Request Range=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* Referer */
    if ( (value = pblock_findval("referer", rq->headers)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Referer=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* From */
    if ( (value = pblock_findval("from", rq->headers)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "From=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* User Agent */
	if ( (value = pblock_findval("user-agent", rq->headers)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN, 
			"User Agent=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
	}

	/* Content Type */
    if ( (value = pblock_findval("content-type", rq->headers)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Content Type=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* Content Length */
    if ( (value = pblock_findval("content-length", rq->headers)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Content Length=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* Content File */
	outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
		"Content File=%s\r\n", wincgi_req->content_file);
	if ((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0)
		return FALSE;

	/* Server Software / Server Name / Server Port */
	outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
        "Server Software=%s\r\nServer Name=%s\r\nServer Port=%d\r\n",
        MAGNUS_VERSION_STRING, server_hostname, port);
    if (outbuf_len &&
		((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
        return -1;

	/* Server Admin */

	/* CGI Version */
    outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
        "CGI Version=%s\r\n", WIN_CGI_VERSION);
    if (outbuf_len &&
		((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
        return -1;

	/* Remote Host / Remote Address */
    if ( (value = pblock_findval("ip", sn->client)) ) {
        if ( (dns_name = session_dns(sn)) ) {
            outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
                "Remote Host=%s\r\nRemote Address=%s\r\n",
                dns_name, value);
		} else {
            outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
                "Remote Address=%s\r\n", value);
		}
	
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* Authentication Method */
    if( (value = pblock_findval("auth-type", rq->vars))) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Authentication Method=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* Authentication Realm */
	
	/* Authenticated Username */
    if( (value = pblock_findval("auth-user", rq->vars))) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Authenticated Username=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
    }

	/* Authenticated Password */
	if ( (value = pblock_findval("auth-password", rq->vars))) {
		char *path, *ptr;
		/* wincgi spec says if the filename starts with '$', export the 
		 * password.
		 */
		path = pblock_findval("path", rq->vars);
		if ( (path) && (ptr = strrchr(path, '/')) ) {
			if ( ptr[1] == '$' ) {
				outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
					"Authenticated Password=%s\r\n", value);
				if (outbuf_len &&
					((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
					return -1;
			}
		}
	}

	if ( (value = pblock_findval("auth-cert", rq->vars)) ) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "Client Certificate=%s\r\n", value);
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
	}

	if (security_active) {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "HTTPS=on\r\n");
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;

		if ( (value = pblock_findval("keysize", sn->client)) ) {
        	outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
	            "HTTPS Keysize=%s\r\n", value);
			if (outbuf_len &&
				((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
			return -1;
		}
		if ( (value = pblock_findval("secret-keysize", sn->client)) ) {
        	outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
	            "HTTPS Secret Keysize=%s\r\n", value);
			if (outbuf_len &&
				((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
			return -1;
		}
	} else {
        outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
            "HTTPS=off\r\n");
        if (outbuf_len &&
			((result = PR_Write(cgi_file, outbuf, outbuf_len)) < 0))
            return -1;
	}



	return 0;
}

/* _wincgi_fill_accept()
 * Fill the [Accept] portion of the output data file.
 */
static int 
_wincgi_fill_accept(wincgi_request_t *wincgi_req, Session *sn, Request *rq)
{
    char outbuf[MAX_OUTPUT_LEN];
    char *accept;
    char *accept_ptr, *buf_ptr = outbuf;
    int value=0;

    buf_ptr += util_sprintf(outbuf, "[Accept]\r\n");

    if ( (accept = pblock_findval("accept", rq->headers)) && *accept ) {

        accept_ptr = accept;

        // (MAX_OUTPUT_LEN - 7) provides enough room for "=yes\r\n\0"
        while ( (buf_ptr - outbuf) < (MAX_OUTPUT_LEN - 7) ) {
            if (*accept_ptr == ';') {
                value++;
                *buf_ptr = '=';
                accept_ptr++;
            } else if (*accept_ptr == ',') {
                if (!value) {
                    strcpy(buf_ptr, "=yes\r\n");
                    buf_ptr += 6;
                } else {
                    strcpy(buf_ptr, "\r\n");
                    buf_ptr += 2;
                }
                value=0;
                accept_ptr++;
            } else if (*accept_ptr == ' ') {
                accept_ptr++;
            } else if (*accept_ptr == '=') {
                value++;
                accept_ptr++;
            } else if (*accept_ptr == '\0') {
                if (!value) {
                    strcpy(buf_ptr, "=yes\r\n");
                    buf_ptr += 6;
                } else {
                    strcpy(buf_ptr, "\r\n");
                    buf_ptr += 2;
                }
                value=0;
                *buf_ptr = '\0';
                break;
            } else {
                *buf_ptr++ = *accept_ptr++;
            }
        }
    }
    if (PR_Write(wincgi_req->data_handle, outbuf, buf_ptr - outbuf) < 0)
        return -1;

    return 0;
}

/* _wincgi_fill_system()
 * Fill the [System] portion of the output data file.
 */
static int 
_wincgi_fill_system(wincgi_request_t *wincgi_req, Session *sn, Request *rq)
{
	char outbuf[MAX_OUTPUT_LEN];
	unsigned outbuf_len=0;
    unsigned long result;
	PRFileDesc *system_file = wincgi_req->data_handle;
	struct _timeb timeptr;

	outbuf_len = util_sprintf(outbuf, "[System]\r\n");
	if (outbuf_len &&
		((result = PR_Write(system_file, outbuf, outbuf_len)) < 0))
        return -1;

	/* GMT Offset */
	_ftime(&timeptr);
	outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN, 
		"GMT Offset=%d\r\n", (-1)*(timeptr.timezone * 60));
    if (outbuf_len &&
		((result = PR_Write(system_file, outbuf, outbuf_len)) < 0))
        return -1;

	/* Debug Mode */
    outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN, 
		"Debug Mode=%s\r\n", wincgi_req->debug?"Yes":"No");
	if (outbuf_len &&
		((result = PR_Write(system_file, outbuf, outbuf_len)) < 0))
        return -1;
	
	/* Output File / Content File */
    outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN,
		"Output File=%s\r\nContent File=%s\r\n",
        wincgi_req->output_file, wincgi_req->content_file);
    if (outbuf_len &&
		((result = PR_Write(system_file, outbuf, outbuf_len)) < 0))
        return -1;

	return 0;
}

/* _wincgi_fill_extra_headers()
 * Fill the [Extra Headers] portion of the output data file.
 */
static int 
_wincgi_fill_extra_headers(wincgi_request_t *wincgi_req, Session *sn, Request *rq)
{
	char outbuf[MAX_OUTPUT_LEN];
	unsigned int outbuf_len=0;
    unsigned long result;
	int count;
	struct pb_entry *param;
	PRFileDesc *extra_header_file = wincgi_req->data_handle;

	outbuf_len = util_sprintf(outbuf, "[Extra Headers]\r\n");
	if (outbuf_len &&
		((result = PR_Write(extra_header_file, outbuf, outbuf_len)) < 0))
        return -1;

	/* Go through rq->headers and find all other header entries which 
	 * have not yet been dumped to a file
	 */
	for (count=0; count<rq->headers->hsize; count++) {
		param = rq->headers->ht[count];
		while(param) {
			if ( strcasecmp(param->param->name, "accept") ) {
				outbuf_len = util_snprintf(outbuf, MAX_OUTPUT_LEN, "%s=%s\r\n", 
					param->param->name, param->param->value);
				if (outbuf_len &&
					((result = PR_Write(extra_header_file, outbuf, outbuf_len)) < 0))
                    return -1;
			}
			param = param->next;
		}
	}
	
	return 0;
}

#define READ_SIZE		(16 *1024)
#define LARGE_FIELD_SIZE	65536
#define STATE_NAME	1
#define STATE_VALUE	2
static int
_wincgi_urldecode(wincgi_request_t *wincgi_req, Session *sn, Request *rq) 
{
	PRFileDesc *fh = wincgi_req->content_handle;
	filebuf_t *buf = NULL;
	unsigned int bytes_to_read = wincgi_req->content_length;
	unsigned long bytes_read;
	int status = 0;
	char *buffer = 0;// [LARGE_FIELD_SIZE + 10];	 for =,\r,\n,\0 or _xxxxx to add 
	unsigned int token_len, escape_count, dquote_count;
    unsigned long bytes_written;
	int state;
    int namelen;
    char * namebuf;
	pblock *keylist = pblock_create(10);
	if (keylist == NULL) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport1));
		return -1;
	}

	/* Now parse the data from the content file */
#ifdef NS_OLDES3X
	if (PR_Seek(fh, 0, FILE_BEGIN) < 0)
#else
	if (PR_Seek(fh, 0, PR_SEEK_SET) < 0)
#endif
		return -1;

	buffer = (char *)MALLOC(LARGE_FIELD_SIZE + 10);	/* for =,\r,\n,\0 or _xxxxx to add */

	if ( !(buf = filebuf_open(fh, 0)) ) {
		log_error(LOG_FAILURE, "wincgi-decode", sn, rq,
			        XP_GetAdminStr(DBT_ntwincgiError6), system_errmsg());
		pblock_free(keylist);
		status = -1;
		goto done;
	}

	state = STATE_NAME;
	token_len = escape_count = dquote_count = 0;
	for ( bytes_read = 0; bytes_read <= bytes_to_read; bytes_read++) {
		char ch;
		int sm_count;	/* select multiple count */
		pb_param *sm_param;
		int field_len;

		if (bytes_read < bytes_to_read) 
			ch = filebuf_getc(buf);
		else 
			ch = '&';		/* sentinel to flush last field out */

		switch ( ch ) {
			case '=':
				if (state == STATE_VALUE) 
					buffer[token_len++] = ch;
				else {
					/* Write the key name out to the data file.  Because wincgi 
					 * FOOLISHLY decided to differentiate SELECT MULTIPLE keys,
					 * we have to save a copy of every f**king key to detect
					 * duplicates.  
					 */
					buffer[token_len] = '\0';
					if ( (sm_param = pblock_find(buffer, keylist)) ) {
						sm_count = atoi(sm_param->value) + 1;
						buffer[token_len++] = '_';
						field_len = util_itoa(sm_count, &(buffer[token_len]));
	
						FREE(sm_param->value);
						sm_param->value = STRDUP(&buffer[token_len]);
	
						token_len += field_len;

					} else {
						pblock_nninsert(buffer, 0, keylist); 
					}
					buffer[token_len++] = '=';

					if ( !(namebuf = (char *)MALLOC(token_len)) ) {
						ereport(LOG_WARN, XP_GetAdminStr(DBT_ntwincgiereport2));
						status = -1;
						goto done;
					}
                    namelen = token_len;
					memcpy(namebuf, buffer, token_len);

					token_len = escape_count = dquote_count = 0;
					state = STATE_VALUE;
				}
				break;
			case '&':

				if (state == STATE_NAME) {
					buffer[token_len++] = ch;
					break;
				}
					
				field_len = token_len - (2 * escape_count);

				if ( field_len < 255 && !escape_count && !dquote_count) {
					/* Form Literal */
					buffer[token_len] = '\0';
					util_uri_unescape(buffer);

					token_len = strlen(buffer);

					buffer[token_len++] = '\r';
					buffer[token_len++] = '\n';

                    if ((lex_token_append(wincgi_req->form_literal,
                                          namelen, namebuf) < 0) ||
                        (lex_token_append(wincgi_req->form_literal,
                                          token_len, buffer) < 0)) {
						ereport(LOG_WARN, XP_GetAdminStr(DBT_ntwincgiereport2));
						status = -1;
						goto done;
					}

				} else if ( field_len < LARGE_FIELD_SIZE ) {
					PRFileDesc *ext_fh;
					char *ext_file;

					/* Form External */
					util_uri_unescape(buffer);
					ext_fh = make_temp_file(wincgi_req->tmp_dir, "ext", 
						GetUniqueCgiNumber(), &ext_file);

					if ( ext_fh == 0 ) {
						ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport3),
							system_errmsg());
						FREE(ext_file);
						status = -1;
						goto done;
					}

					if ((bytes_written = PR_Write(ext_fh, buffer, field_len)) < 0) {
						ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport4),
						system_errmsg());
						PR_Close(ext_fh);
						FREE(ext_file);
						status = -1;
						goto done;
					}

					PR_Close(ext_fh);
					token_len = util_snprintf(buffer, LARGE_FIELD_SIZE, 
						"%s %d\r\n", ext_file, field_len);
					FREE(ext_file);

                    if ((lex_token_append(wincgi_req->form_external,
                                          namelen, namebuf) < 0) ||
                        (lex_token_append(wincgi_req->form_external,
                                          token_len, buffer) < 0)) {
						ereport(LOG_WARN, XP_GetAdminStr(DBT_ntwincgiereport2));
						status = -1;
						goto done;
					}
				} else {
					/* Form Huge */
					/* Shouldn't ever land here */
				}

				token_len = escape_count = dquote_count = 0;
				state = STATE_NAME;
				break;
			case '"':
				dquote_count++;
				buffer[token_len++] = ch;
				break;
			case '%':
				escape_count++;
				buffer[token_len++] = ch;
				break;
			case '+':
				if (state == STATE_VALUE)
					buffer[token_len++] = ' ';
				else
					buffer[token_len++] = ch;
				break;
			default:
				buffer[token_len++] = ch;
				break;
		}
		
		if ( token_len >= LARGE_FIELD_SIZE ) {
			if (state == STATE_NAME) {
				ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport5),
					token_len);
				status = -1;
				goto done;
			} else {
				char tmp_ch;
				/* Form Huge! */
				do {
					/* Gulp the data until we get to the next field */
					tmp_ch = filebuf_getc(buf);
					bytes_read++;
					token_len++;
				} while (bytes_read < bytes_to_read && tmp_ch != '&');

				/* Now write it to disk */
				token_len = util_snprintf(buffer, LARGE_FIELD_SIZE,
					"%d %d\r\n", bytes_read - (token_len-1), token_len-1);

                if ((lex_token_append(wincgi_req->form_huge,
                                      namelen, namebuf) < 0) ||
                    (lex_token_append(wincgi_req->form_huge,
                                      token_len, buffer) < 0)) {
					ereport(LOG_WARN, XP_GetAdminStr(DBT_ntwincgiereport2));
					status = -1;
					goto done;
				}

				token_len = escape_count = dquote_count = 0;
				state = STATE_NAME;
			}
		}
	}
		
done:
	if (buf){
		filebuf_close(buf);
        wincgi_req->content_handle = 0; // Must indicate this sucker is closed.
    }
	if (buffer)
		FREE(buffer);
	pblock_free(keylist);
	return status;
}

/* _wincgi_fill_form()
 * Read all posted data from client; classify as form_literal, form_external,
 * form_file, or form_huge and write to the appropriate section.
 */
static int 
_wincgi_fill_form(wincgi_request_t *wincgi_req, Session *sn, Request *rq)
{
	char *content_type = pblock_findval("content-type", rq->headers); 

    lex_token_new((pool_handle_t *)0, 0, 64, &wincgi_req->form_literal);
    lex_token_new((pool_handle_t *)0, 0, 64, &wincgi_req->form_external);
	lex_token_new((pool_handle_t *)0, 0, 64, &wincgi_req->form_huge);

	/* Just use url encoding for anything except form-data... */
	if ( content_type && !strcasecmp(content_type, "multipart/form-data") ) {
		ereport(LOG_INFORM, XP_GetAdminStr(DBT_ntwincgiereport6));
		return -1;
    } else {
		if (_wincgi_urldecode(wincgi_req, sn, rq) < 0) {
			return -1;
		}
	}
    return 0;
}


/* _wincgi_get_content()
 * Load all POST data from the client into the content data file.
 * Later this data will be parsed and loaded into the appropriate
 * data files.
 * Returns REQ_PROCEED if successful; REQ_ABORTED otherwise.
 */
static int
_wincgi_get_content(wincgi_request_t *wincgi_req, Session *sn, Request *rq)
{
	char *post = pblock_findval("method", rq->reqpb); 
	char *cls, *cts;
	unsigned cl, x;

	if (!strncmp(post, "POST", 4))
		wincgi_req->post = 1;
	else
		return REQ_PROCEED;

	if ((request_header("content-length", &cls, sn, rq) == REQ_ABORTED) ||
		(request_header("content-type", &cts, sn, rq) == REQ_ABORTED))
		return REQ_ABORTED;

	if ((!cls) || (!cts)) {
		wincgi_req->content_length =0;
		return REQ_PROCEED;
	}

	while(isspace(*cls)) ++cls;
	for(x = 0; cls[x] && (!isspace(cls[x])); ++x) {
		if (!isdigit(cls[x])) {
			log_error(LOG_WARN, "send-wincgi", sn, rq, 
				        XP_GetAdminStr(DBT_ntwincgiError7), cls);
			return REQ_ABORTED;
		}
	}

	cl = atoi(cls);
	if (cl < 0) {
		log_error(LOG_WARN, "send-wincgi", sn, rq,
			        XP_GetAdminStr(DBT_ntwincgiError7), cls);
		return REQ_ABORTED;
	}
	wincgi_req->content_length = cl;
	
	if (netbuf_buf2file(sn->inbuf, wincgi_req->content_handle, cl) == IO_ERROR) {
		if ( !(errno & ERROR_PIPE)) {
			log_error(LOG_FAILURE, "send-wincgi", sn, rq, 
			          XP_GetAdminStr(DBT_ntwincgiError8), 	
				sn->inbuf->errmsg);
			return REQ_ABORTED;
		}
	}
	
	return REQ_PROCEED;
}

static int
_wincgi_dump_form(PRFileDesc *ofile, char *name, void * token)
{
    char * tdatabuf;
    int tdatalen;
	unsigned long wrote;

    /* Write the section header */
    if ( (wrote = PR_Write(ofile, name, strlen(name))) < 0)
        return -1;

    /* Write the token contents */
    tdatabuf = lex_token_info(token, &tdatalen, 0);
    if (tdatabuf) {
        if ((wrote = PR_Write(ofile, tdatabuf, tdatalen)) < 0)
				return -1;
    }

	return 0;
}

/* _wincgi_fill_data()
 * Parse through the headers and create the necessary data files.
 */
static int
_wincgi_fill_data(wincgi_request_t *wincgi_req, Session *sn, Request *rq)
{
	if ( _wincgi_fill_cgi(wincgi_req, sn, rq) ) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport7));
		return -1;
	}

	if ( _wincgi_fill_accept(wincgi_req, sn, rq) ) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport8));
		return -1;
	}

	if ( _wincgi_fill_system(wincgi_req, sn, rq) ) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport9));
		return -1;
	}

	if ( _wincgi_fill_extra_headers(wincgi_req, sn, rq) ) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport10));
		return -1;
	}

	if ( wincgi_req->post ) {
		if ( _wincgi_fill_form(wincgi_req, sn, rq) ) {
			ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport11));
			return -1;
		}

		_wincgi_dump_form(wincgi_req->data_handle, "[Form Literal]\r\n", 
			wincgi_req->form_literal);
		_wincgi_dump_form(wincgi_req->data_handle, "[Form External]\r\n", 
			wincgi_req->form_external);
		_wincgi_dump_form(wincgi_req->data_handle, "[Form Huge]\r\n", 
			wincgi_req->form_huge);
	}

	return 0;
}

#define DEFAULT_TIMEOUT 60

/* wincgi_send()
 * Service function to execute a WinCGI script and return its output to 
 * the client. 
 */
int 
wincgi_send(pblock *pb, Session *sn, Request *rq)
{
	wincgi_request_t wincgi_req;

    char *t, *path = pblock_findval("path", rq->vars), num[16];
    SYS_NETFD tfd;
    HANDLE pid = 0;
    filebuf_t *buf = 0;
    int result, length;
    pb_param *pp;
    int restart, ns;
    int linger = 1;
    int status;
	int r = 0;
    DWORD waitrc = 0;

    if((pp = pblock_remove("content-type", rq->srvhdrs)))
        param_free(pp);

    //
    // There is a race here but that's OK since there is only one value
    // for timeout.  We're just trying to save some computation time down
    // the road.
    if (GlobalTimeout == 0){
        GlobalTimeout = 1000 * GetConfigParm("WincgiTimeout", DEFAULT_TIMEOUT);        
        if ((GlobalTimeout) == 0)
            GlobalTimeout = 1000 * DEFAULT_TIMEOUT;       
    }

	// If there is a terminating '/', then get rid of it...

	length = strlen(path);

	if (path[length -1] == '\\') {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport12), path);
		return REQ_ABORTED;
	}
	if ( _wincgi_initialize_request(&wincgi_req, pb) < 0)  {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport13));
		return REQ_ABORTED;
	}

	if ( _wincgi_get_content(&wincgi_req, sn, rq) < 0) {
		status = REQ_ABORTED;
		goto done;
	}

	if ( _wincgi_fill_data(&wincgi_req, sn, rq) < 0) {
		status = REQ_ABORTED;
		goto done;
	}

        if((result = wincgi_start_exec(sn, rq, &wincgi_req, &pid, path)) == -1) {
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ntwincgiereport14), path);
                if (pid != 0) {
                    TerminateProcess(pid, 1);
                    CloseHandle(pid);
                }
		status = REQ_ABORTED;
		goto done;
	}

	/* Wait for the process to terminate */
        waitrc = WaitForSingleObject(pid, GlobalTimeout);
        switch(waitrc){
        case WAIT_FAILED:
            log_error(LOG_FAILURE, "send-wincgi", sn, rq, 
                      XP_GetAdminStr(DBT_ntwincgiError9), system_errmsg());
            status = REQ_ABORTED;
        	CloseHandle(pid);
            goto done;
            break;
        case WAIT_TIMEOUT:
            log_error(LOG_FAILURE, "send-wincgi", sn, rq, 
                      XP_GetAdminStr(DBT_ntwincgiError10), system_errmsg());
            status = REQ_ABORTED;
           	CloseHandle(pid);
            goto done;
            break;
        default:
            break;
        }

	CloseHandle(pid);

    t = strrchr(path, FILE_PATHSEP);
    /* XXXrobm FILE_PATHSEP is a forward slash. But at this point, the path
       may have backslashes */
    if(!t)
        t = strrchr(path, '\\');
    if(t && (!strncmp(t+1, "nph-", 4))) {
        tfd = sn->csd;
        restart = 0;
    }
    else {
        if ( !(buf = filebuf_open(wincgi_req.output_handle, 0)) ) {
            log_error(LOG_FAILURE, "send-wincgi", sn, rq, 
                      XP_GetAdminStr(DBT_ntwincgiError11), system_errmsg());
            status = REQ_ABORTED;
            goto done;
        }
        switch(wincgi_parse_output(buf, sn, rq)) {
          case -1:  // Req aborted
            filebuf_close(buf);
            buf = 0;
            wincgi_req.output_handle = 0;
            status = REQ_ABORTED;
            goto done;
          case -2:  // Req don't parse...send raw
            tfd = sn->csd;
            restart = 0;
            break;
          case 0:   // Req OK
			r = protocol_start_response(sn, rq);
            if((r == REQ_EXIT) || (r == REQ_NOACTION)){
                tfd = SYS_NET_ERRORFD;
				filebuf_close(buf);
                buf = 0;
				wincgi_req.output_handle = 0;
				status = r;
				goto done;
            }else
                tfd = sn->csd;
            restart = 0;
            break;
          case 1:   //
            tfd = SYS_NET_ERRORFD;
            restart = 1;
        }
    }
    if ((tfd == SYS_NET_ERRORFD) || ((ns = filebuf_buf2sd(buf, tfd)) == IO_ERROR)) {
        BOOLEAN WinsockError = FALSE;
        DWORD Error = GetLastError();

        if (Error == 0) {
            Error = WSAGetLastError();
            if (Error) {
                WinsockError = TRUE;
            }
        }
        switch (Error) {
            case ERROR_BROKEN_PIPE:
            case ERROR_BAD_PIPE:
            case ERROR_PIPE_BUSY:
            case ERROR_PIPE_LISTENING:
            case ERROR_PIPE_NOT_CONNECTED:
            case ERROR_NO_DATA:
			case ERROR_NETNAME_DELETED:
                break;
            default:
				if (buf->errmsg)
					log_error(LOG_FAILURE, "send-wincgi", sn, rq, 
						        XP_GetAdminStr(DBT_ntwincgiError12), buf->errmsg);
				else
					log_error(LOG_FAILURE, "send-wincgi", sn, rq, 
						        XP_GetAdminStr(DBT_ntwincgiError13), Error);

                if (WinsockError) {
                    WSASetLastError(Error);
                }
				if (buf){
					filebuf_close(buf);
                    buf = 0;
					wincgi_req.output_handle = 0;
				}

				status = REQ_ABORTED;
                goto done;
        }
    }
    else {
        util_itoa(ns, num);
        pblock_nvinsert("content-length", num, rq->srvhdrs);
    }
    filebuf_close(buf);
    buf = 0;
    wincgi_req.output_handle = 0;
    if(restart) {
        pb_param *l = pblock_remove("location", rq->srvhdrs);
		_wincgi_cleanup_request(&wincgi_req);
        return request_restart_location(sn, rq, l->value);
    }

	status = REQ_PROCEED;
done:
	_wincgi_cleanup_request(&wincgi_req);
    return status;
}


/* ------------------------------ wincgi_query ------------------------------- */


int wincgi_query(pblock *pb, Session *sn, Request *rq)
{
    pb_param *rqpath = pblock_find("path", rq->vars);
    char *path = pblock_findval("path", pb);

    if (!path)
        return REQ_ABORTED;

    // VB: path might not exist if we got called on an error handler as in the case of a missing unix-home dir
    if (rqpath) {
        FREE(rqpath->value);
        rqpath->value = STRDUP(path);
    }
    else {
        pblock_nvinsert("path", path, rq->vars);
    }

    if(!(pblock_findval("path-info", rq->vars)))
        pblock_nvinsert("path-info", pblock_findval("uri", rq->reqpb), 
                        rq->vars);

    return wincgi_send(NULL, sn, rq);
}

void wincgi_terminate(void *unused)
{
    if(wincgi_initenv) {
        pblock_free(wincgi_initenv);
        wincgi_initenv = NULL;
    }
}

int wincgi_init(pblock *pb, Session *sn, Request *rq)
{
    if(!wincgi_initenv)
        wincgi_initenv = pblock_create(4);
    pblock_copy(pb, wincgi_initenv);
    param_free(pblock_remove("fn", wincgi_initenv));

    magnus_atrestart(wincgi_terminate, NULL);
	return TRUE;
}


#endif /* MCC_ADMSERV */
