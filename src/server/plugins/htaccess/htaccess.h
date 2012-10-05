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


#include "nspr.h"
#include "base/systems.h"
#include "base/util.h"
#include "base/pblock.h"
#include "base/session.h"
#include "frame/req.h"
#include "frame/log.h"
#include "httpdaemon/HttpMethodRegistry.h"

#ifdef AUTHNSDBFILE
#include "libaccess/nsuser.h"
#include "libaccess/nsgroup.h"
#endif /* AUTHNSDBFILE */

#include "httpd.h"

typedef struct {
    pblock *pb;
    Session *sn;
    Request *rq;

    char *auth_type;
    char *auth_name;
    char *auth_pwfile;
    char *auth_grpfile;
    char *auth_line;

    char * user_check_fn;
    char * group_check_fn;

    void *auth_authdb;
    char *auth_grplist;
    char *auth_grplfile;
    int auth_grpllen;
#ifdef AUTHNSDBFILE
    UserObj_t *auth_uoptr;
    int auth_nsdb;
#endif /* AUTHNSDBFILE */

    int num_sec;
    security_data * sec;

    char *remote_host;
    char *remote_ip;
    char *remote_name;

    char *access_name;

    char user[MAX_STRING_LEN];
    char groupname[MAX_STRING_LEN];

} htaccess_context_s;


/* Token structure for scanning text */
typedef struct httoken_s httoken_t;
struct httoken_s {
    char *buf;                  /* pointer to text being scanned */
    int ts;                     /* token start index */
    int tlen;                   /* token length */
    int rt;                     /* residual text start index */
};

typedef struct t_command {
    char * directive;
    char * function;
    int argtype;
    int authtype;
    char * check_fn;
    struct t_command * next;

} command;

/* Argument types */
#define BOOL  0
#define PASS1 1
#define PASS2 2

/* Authentication types */
#define USER_AUTH  0
#define GROUP_AUTH 1

#define HTTOKEN_START(tp) ((tp)->buf + (tp)->ts)
#define HTTOKEN_LENGTH(tp) ((tp)->tlen)
#define HTTOKEN_INIT(tp, ptr) { (tp)->buf = (ptr); (tp)->tlen = 0; (tp)->rt = 0; }
#define HTTOKEN_CHPEEK(tp) (*((tp)->buf + (tp)->rt))
#define HTTOKEN_CHSKIP(tp) ++(tp)->rt
#define HTTOKEN_NEXT(tp, delim) { \
	for ((tp)->ts = (tp)->rt, (tp)->tlen = -1; \
	     HTTOKEN_CHPEEK(tp); HTTOKEN_CHSKIP(tp)) { \
	    if (HTTOKEN_CHPEEK(tp) == (delim)) { \
		(tp)->tlen = (tp)->rt++ - (tp)->ts; break; \
	    } \
	} \
	if ((tp)->tlen < 0) (tp)->tlen = (tp)->rt - (tp)->ts; \
	}
#define HTTOKEN_STRCMP(tp, str) (((tp)->tlen != strlen(str)) || strncmp((tp)->buf + (tp)->ts, (str), (tp)->tlen))
#define HTTOKEN_STRNCMP(tp, str, len) (((tp)->tlen != (len)) || strncmp((tp)->buf + (tp)->ts, (str), (tp)->tlen))
#define HTTOKEN_STRCASECMP(tp, str) (((tp)->tlen != strlen(str)) || strncasecmp((tp)->buf + (tp)->ts, (str), (tp)->tlen))
#define HTTOKEN_STRNCASECMP(tp, str, len) (((tp)->tlen != (len)) || strncasecmp((tp)->buf + (tp)->ts, (str), (tp)->tlen))

int htaccess_evaluate(pblock *pb, Session *sn, Request *rq);


/* util */
int htaccess_strcmp_match(char *str, char *exp);
int htaccess_is_matchexp(char *str);
void htaccess_no2slash(char *name);
int htaccess_count_dirs(char *path);
void htaccess_strcpy_dir(char *d, char *s);
void htaccess_lim_strcpy(char *d, char *s, int n);
void htaccess_getword(char *word, char *line, char stop);
void htaccess_cfg_getword(char *word, char *line);
int htaccess_cfg_getline (char* s, int n, filebuffer *fp);
void htaccess_make_dirstr(char *s, int n, char *d);
void htaccess_make_full_path(char *src1,char *src2,char *dst);
int htaccess_is_ip(const char *host);

/* http_access */
int htaccess_check_method(int method_mask, int method);
int htaccess_evaluate(htaccess_context_s *ctxt);
static htaccess_context_s *
    _htaccess_newctxt(pblock *pb, Session *sn, Request *rq);
static void _htaccess_freectxt(htaccess_context_s *ctxt);

/* http_auth */
int htaccess_check_auth(security_data *sec, int m, htaccess_context_s *ctxt);

/* http_config */
int htaccess_get_method_number(char * method);
int htaccess_check_group(char *group, int glen, char *grplist);
int htaccess_add_group(char *group, htaccess_context_s *ctxt);
int htaccess_init_group(char *grpfile, htaccess_context_s *ctxt);
int htaccess_in_group(char *user, char *group, int glen, htaccess_context_s *ctxt);
void htaccess_kill_group(htaccess_context_s *ctxt);
const char * htaccess_check_allow(char * where);
filebuffer * htaccess_cfg_open(char *name);
void htaccess_cfg_close(filebuffer *buf);
int htaccess_parse_access_dir(filebuffer *f, int line, char _or, char *dir,
                              char *file, htaccess_context_s *ctxt);


/* http_request */
#ifdef __cplusplus
extern "C"
#endif
NSAPI_PUBLIC int htaccess_register(pblock *pb, Session *sn, Request *rq);
int htaccess_call_external_function(char * directive, char * function, int args, char * arg1, char * arg2, pblock * pb, Session *sn, Request *rq);

extern struct t_command * root;

/* main */
extern int _ht_gwu;
extern char *_ht_fn_basic;
extern char *_ht_fn_user;
extern char *_ht_fn_usrgrp;

/* list */
security_data * htaccess_newsec(void);
security_data * htaccess_addsec(security_data * head, security_data * item);
security_data * htaccess_getsec(security_data * head, int x);
void htaccess_secflush(security_data * head);
