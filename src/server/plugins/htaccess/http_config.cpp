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
 * http_config.c: auxillary functions for reading httpd's config file
 * and converting filenames into a namespace
 *
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 * Based on NCSA HTTPd 1.3 by Rob McCool 
 */

#include "frame/log.h"
#include "base/file.h"
#include "base/buffer.h"
#include "frame/http.h"
#include "frame/func.h"
#include "htaccess.h"

static int
die(int code, const char *str, htaccess_context_s *ctxt)
{
    log_error(LOG_FAILURE, "htaccess-find", ctxt->sn, ctxt->rq, "%s", str);
    return REQ_ABORTED;
}

static int 
access_syntax_error(int n, const char *err, char *file, 
                    htaccess_context_s *ctxt) 
{
    char e[MAX_STRING_LEN];

    util_snprintf(e,MAX_STRING_LEN,"syntax error or override violation in "
            "access control file %s, line %d, reason: %s",file,n+1,err);
    protocol_status(ctxt->sn, ctxt->rq, PROTOCOL_SERVER_ERROR, NULL);
    return die(PROTOCOL_SERVER_ERROR,e,ctxt);
}

filebuffer * htaccess_cfg_open(char *name)
{
    filebuffer *buf;
    SYS_FILE fd = system_fopenRO(name);
    if(fd == SYS_ERROR_FD)
        return NULL;
    if(!(buf = filebuf_open(fd, FILE_BUFFERSIZE))) {
        system_fclose(fd);
        return NULL;
    }
    return buf;
}

void htaccess_cfg_close(filebuffer *buf)
{
    filebuf_close(buf);
}

int htaccess_get_methods(int n, char * l, char *file, htaccess_context_s *ctxt) {

    char w[MAX_STRING_LEN];
    char w2[MAX_STRING_LEN];
    int methods = 0;
    int methnum = 0;
    HttpMethodRegistry& registry = HttpMethodRegistry::GetRegistry();

    htaccess_getword(w2,l,'>');
    while(w2[0]) {
        htaccess_cfg_getword(w,w2);

        if (!strcasecmp(w, "TRACE"))
            return access_syntax_error(n,"TRACE cannot be controlled by <Limit>",file,ctxt);

        /* treat HEAD as a GET */
        if (!strcasecmp(w, "HEAD"))
            methnum = registry.HttpMethodRegistry::GetMethodIndex("GET");
        else
            methnum = registry.HttpMethodRegistry::GetMethodIndex(w);

        if(methnum == -1)
            return access_syntax_error(n,"Unknown method",
                                        file,ctxt);

        if (methnum >= MAX_METHODS)
            return access_syntax_error(n,"Too many methods defined.",
                                        file,ctxt);
        methods |= (1 << methnum);
    }
    return methods;
}


/*
 * Grows *data and *data_methods by SECTABLE_ALLOC_UNIT. Used by
 * htaccess_parse_access_dir below whenever sec->allow, sec->auth or 
 * sec->deny need to grow.
 *
 * Returns 1 if REALLOC returns NULL, else 0.
 *
 */
static int grow_sectable(int * max_num, char *** data, int ** data_methods)
{
    int new_max = *max_num + SECTABLE_ALLOC_UNIT;

    *data = (char **)REALLOC(*data, sizeof(char *) * new_max);
    if (*data == NULL) { return 1; }

    *data_methods = (int *)REALLOC(*data_methods, sizeof(int) * new_max);
    if (*data_methods == NULL) { return 1; }

    *max_num = new_max;

    return 0;
}


int 
htaccess_parse_access_dir(filebuffer *f, int line, char _or, char *dir, 
                          char *file, htaccess_context_s *ctxt) 
{
    char l[MAX_STRING_LEN];
    char w[MAX_STRING_LEN];
    char w2[MAX_STRING_LEN];
    char w3[MAX_STRING_LEN];
    int n=line;
    register int x,i,q;
    int methods = 0;
    int methnum = 0;
    int inlimit=0;
    int inlimitexcept=0;
    struct t_command * item;
    security_data * sec = htaccess_newsec();

    log_error(LOG_VERBOSE, "htaccess_parse_access_dir", ctxt->sn, ctxt->rq, 
              "Processing [%s]", file);

    x = ctxt->num_sec;

    if(!(sec->d = (char *)MALLOC((sizeof(char)) * (strlen(dir) + 2))))
        return die(NO_MEMORY,"parse_access_dir",ctxt);
    if(htaccess_is_matchexp(dir))
        strcpy(sec->d,dir);
    else
        htaccess_strcpy_dir(sec->d,dir);

    sec->auth_type = NULL;
    sec->auth_name = NULL;
    sec->auth_pwfile = NULL;
    sec->auth_grpfile = NULL;
#ifdef AUTHNSDBFILE
    sec->auth_nsdb = 0;
#endif /* AUTHNSDBFILE */
    for(i=0;i<MAX_METHODS;i++)
        sec->order[i] = DENY_THEN_ALLOW;
    sec->num_allow=0;
    sec->num_deny=0;
    sec->num_auth = 0;

    while(!(htaccess_cfg_getline(l,MAX_STRING_LEN,f))) {
        ++n;
        if((l[0] == '#') || (!l[0])) continue;
        htaccess_cfg_getword(w,l);

        if(!strcasecmp(w,"AuthName")) {
            if (inlimit || inlimitexcept)
                return access_syntax_error(n, "AuthName not allowed here.",
                                           file,ctxt);
            if(sec->auth_name) 
                FREE(sec->auth_name);
            if(!(sec->auth_name = STRDUP(l)))
                return die(NO_MEMORY,"parse_access_dir",ctxt);
        }
        else if(!strcasecmp(w,"AuthType")) {
            if (inlimit || inlimitexcept)
                return access_syntax_error(n, "AuthType not allowed here.",
                                           file,ctxt);
            htaccess_cfg_getword(w,l);
            if(sec->auth_type) 
                FREE(sec->auth_type);
            if(!(sec->auth_type = STRDUP(w)))
                return die(NO_MEMORY,"parse_access_dir",ctxt);
        }
        else if(!strcasecmp(w,"AuthUserFile")) {
            if (inlimit || inlimitexcept)
                return access_syntax_error(n, "AuthUserFile not allowed here.",
                                           file,ctxt);
            htaccess_cfg_getword(w,l);
            if(sec->auth_pwfile) 
                FREE(sec->auth_pwfile);
            if(!(sec->auth_pwfile = STRDUP(w)))
                return die(NO_MEMORY,"parse_access_dir",ctxt);
#ifdef AUTHNSDBFILE
            sec->auth_nsdb = 0;
#endif /* AUTHNSDBFILE */
        }
        else if(!strcasecmp(w,"AuthGroupFile")) {
            if (inlimit || inlimitexcept)
                return access_syntax_error(n, "AuthGroupFile not allowed here.",
                                           file,ctxt);
            htaccess_cfg_getword(w,l);
            if(sec->auth_grpfile) 
                FREE(sec->auth_grpfile);
            if(!(sec->auth_grpfile = STRDUP(w)))
                return die(NO_MEMORY,"parse_access_dir",ctxt);
#ifdef AUTHNSDBFILE
            sec->auth_nsdb = 0;
#endif /* AUTHNSDBFILE */
        }
#ifdef AUTHNSDBFILE
        else if (!strcasecmp(w,"AuthNSDBFile")) {
            htaccess_cfg_getword(w,l);
            if(sec->auth_pwfile) 
                FREE(sec->auth_pwfile);
            if(sec->auth_grpfile) 
                FREE(sec->auth_grpfile);
            if(!(sec->auth_pwfile = STRDUP(w)))
                return die(NO_MEMORY,"parse_access_dir",ctxt);
            if(!(sec->auth_grpfile = STRDUP(w)))
                return die(NO_MEMORY,"parse_access_dir",ctxt);
            sec->auth_nsdb = 1;
        }
#endif /* AUTHNSDBFILE */
#if 0 /* XXXNSAPI */
        else if(!strcasecmp(w,"AddType")) {
            htaccess_cfg_getword(w,l);
            htaccess_cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0')) {
                return access_syntax_error(n,
"AddType must be followed by a type, one space, then a file or extension.",
                                           file,ctxt);
            }
            add_type(w2,w,NULL);  /* XXXNSAPI out */
        }
        else if(!strcasecmp(w,"AddEncoding")) {
            htaccess_cfg_getword(w,l);
            htaccess_cfg_getword(w2,l);
            if((w[0] == '\0') || (w2[0] == '\0')) {
                return access_syntax_error(n,
"AddEncoding must be followed by a type, one space, then a file or extension.",
                                    file,ctxt);
            }
            add_encoding(w2,w,NULL); /* XXXNSAPI out */
        }
#endif
        else if(!strcasecmp(w,"<Limit")) {
            if (inlimit)
                return access_syntax_error(n, "<Limit> tags cannot be nested.",
                                           file,ctxt);
            inlimit = 1;

            methods = htaccess_get_methods(n,l,file,ctxt);
        }
        else if(!strcasecmp(w,"<LimitExcept")) {
            if (inlimitexcept)
                return access_syntax_error(n, "<LimitExcept> tags cannot be "
                                           "nested.", file,ctxt);
            inlimitexcept = 1;

            methods =~ htaccess_get_methods(n,l,file,ctxt);
        }
        else if(!strcasecmp(w,"</Limit>"))
            inlimit = 0;
        else if(!strcasecmp(w,"</LimitExcept>"))
            inlimitexcept = 0;
        else if(!strcasecmp(w,"order")) {
            int order;

            if(!strcasecmp(l,"allow,deny")) {
                order = ALLOW_THEN_DENY;
            }
            else if(!strcasecmp(l,"deny,allow")) {
                order = DENY_THEN_ALLOW;
            }
            else if(!strcasecmp(l,"mutual-failure")) {
                order = MUTUAL_FAILURE;
            }
            else
                return access_syntax_error(n,"Unknown order.",
                                           file,ctxt);
            for(i=0;i<MAX_METHODS;i++)
                if (htaccess_check_method(methods, i))
                    sec->order[i] = order;
        } 
        else if((!strcasecmp(w,"allow"))) {
            htaccess_cfg_getword(w,l);
            if(strcmp(w,"from"))
                return access_syntax_error(n,
                                    "allow must be followed by from.",
                                    file,ctxt);
            while(1) {
                const char * error;

                htaccess_cfg_getword(w,l);
                if(!w[0]) break;

                q = sec->num_allow++;

                // grow table if necessary
                if (sec->max_num_allow == q) {
                    if (grow_sectable(&(sec->max_num_allow), 
                                      &(sec->allow), 
                                      &(sec->allow_methods))) {
                        return die(NO_MEMORY, "parse_access_dir", ctxt);
                    }
                }
                if(!(sec->allow[q] = STRDUP(w)))
                    return die(NO_MEMORY,"parse_access_dir",ctxt);
                if((error = htaccess_check_allow(w)))
                    return access_syntax_error(n,error,file,ctxt);
                sec->allow_methods[q] = methods;
            }
        }
        else if(!strcasecmp(w,"require")) {
            
            q=sec->num_auth++;

            // grow table if necessary
            if (sec->max_num_auth == q) {
                if (grow_sectable(&(sec->max_num_auth), 
                                  &(sec->auth), 
                                  &(sec->auth_methods))) {
                    return die(NO_MEMORY, "parse_access_dir", ctxt);
                }
            }
            
            if(!(sec->auth[q] = STRDUP(l)))
                return die(NO_MEMORY,"parse_access_dir",ctxt);
            sec->auth_methods[q] = methods;
        }
        else if((!strcasecmp(w,"deny"))) {
            htaccess_cfg_getword(w,l);
            if(strcmp(w,"from"))
                return access_syntax_error(n,
                                    "deny must be followed by from.",
                                    file,ctxt);
            while(1) {
                htaccess_cfg_getword(w,l);
                if(!w[0]) break;

                q=sec->num_deny++;
                
                // grow table if necessary
                if (sec->max_num_deny == q) {
                    if (grow_sectable(&(sec->max_num_deny), 
                                      &(sec->deny), 
                                      &(sec->deny_methods))) {
                        return die(NO_MEMORY, "parse_access_dir", ctxt);
                    }
                }

                if(!(sec->deny[q] = STRDUP(w)))
                    return die(NO_MEMORY,"parse_access_dir", ctxt);
                sec->deny_methods[q] = methods;
            }
        } else {
        /* Now check all registered directives */
        item = root;
        while (item) {
            if(!strcasecmp(w,item->directive)) {
                if (inlimit || inlimitexcept)
                    return access_syntax_error(n, "directive not allowed here.",
                                               file,ctxt);
                switch(item->argtype) {
                    case BOOL:
                        htaccess_cfg_getword(w2,l);
                        if (*w2 == '\0' || (strcasecmp(w2, "on") && strcasecmp(w2, "off")))
                            return access_syntax_error(n, "must be On or Off.",
                                                       file,ctxt);

                        htaccess_call_external_function(item->directive, 
                            item->function, 1, w2, NULL, ctxt->pb, ctxt->sn, 
                            ctxt->rq);

                        switch(item->authtype) {
                            case USER_AUTH:
                                ctxt->user_check_fn = STRDUP(item->check_fn);
                                break;
                            case GROUP_AUTH:
                                if (!(ctxt->group_check_fn = STRDUP(item->check_fn)))
                                return die(NO_MEMORY,"parse_access_dir",ctxt);
                                break;
                            default:
                                return access_syntax_error(n, "Invalid authentication type registered for function.", file,ctxt);
                                break;
                        }
                        break;

                    case PASS1:
                        htaccess_cfg_getword(w2,l);
                        if (*w2 == '\0')
                            return access_syntax_error(n, "argument missing.",
                                                       file,ctxt);

                        htaccess_call_external_function(item->directive, 
                            item->function, 1, w2, NULL, ctxt->pb, ctxt->sn, 
                            ctxt->rq);

                        switch(item->authtype) {
                            case USER_AUTH:
                                if (!ctxt->user_check_fn)
                                   ctxt->user_check_fn = STRDUP(item->check_fn);
                                break;
                            case GROUP_AUTH:
                                if (!ctxt->group_check_fn)
                                    if (!(ctxt->group_check_fn = 
                                        STRDUP(item->check_fn)))
                                        return die(NO_MEMORY,"parse_access_dir",
                                            ctxt);
                                break;
                            default:
                                    return access_syntax_error(n, "Invalid authentication type registered for function.", file,ctxt);
                                    break;
                        }

                        break;

                    case PASS2:
                        htaccess_cfg_getword(w2,l);
                        htaccess_cfg_getword(w3,l);
                        if ((*w2 == '\0') || (*w3 == '\0'))
                            return access_syntax_error(n, "argument missing.",
                                                       file,ctxt);

                        htaccess_call_external_function(item->directive, 
                            item->function, 1, w2, w3, ctxt->pb, ctxt->sn, 
                            ctxt->rq);

                        switch(item->authtype) {
                            case USER_AUTH:
                                if (!ctxt->user_check_fn)
                                   ctxt->user_check_fn = STRDUP(item->check_fn);
                                break;
                            case GROUP_AUTH:
                                if (!ctxt->group_check_fn)
                                    if (!(ctxt->group_check_fn = 
                                        STRDUP(item->check_fn)))
                                        return die(NO_MEMORY,"parse_access_dir",
                                            ctxt);
                                break;
                            default:
                                    return access_syntax_error(n, "Invalid authentication type registered for function.", file,ctxt);
                                    break;
                        }
                        break;
                    default:
                        return access_syntax_error(n, "Unknown argument type "
                                                   "registered.", file,ctxt);
                }
            break;
            }

            item = item->next;
        }
        }
       /* Ignore directives we don't support */
#if 0
                else
                    return access_syntax_error(n,
                                           "Unknown keyword in Limit region.",
                                           file,ctxt);
#endif
    }
    /* EOF w/o </Limit> */
    if(inlimit)
        return access_syntax_error(n,"<Limit> missing </Limit>", file,ctxt);
    /* EOF w/o </LimitExcept> */
    if(inlimitexcept)
        return access_syntax_error(n,"<LimitExcept> missing </LimitExcept>", file,ctxt);
    ++ctxt->num_sec;
    ctxt->sec = htaccess_addsec(ctxt->sec, sec);
    return REQ_PROCEED;
}


int htaccess_check_group(char *group, int glen, char *grplist)
{
    httoken_t grouptok;

    /* Scan comma-separated list of user's group names */
    HTTOKEN_INIT(&grouptok, grplist);
    while (HTTOKEN_CHPEEK(&grouptok)) {

        /* Allow spaces between tokens */
        if (HTTOKEN_CHPEEK(&grouptok) == ' ') {
            HTTOKEN_CHSKIP(&grouptok);
            continue;
        }

        HTTOKEN_NEXT(&grouptok, ',');

        if (!HTTOKEN_STRNCMP(&grouptok, group, glen)) {

            return 1;
        }
    }

    return 0;
}

int htaccess_add_group(char *group, htaccess_context_s *ctxt)
{
    char *cp;
    int glen;
    int grpllen;
    int newlen;

    if (group && *group) {

        glen = strlen(group);

        if (!ctxt->auth_grplist) {
            ctxt->auth_grplist = (char *)MALLOC(128);
            ctxt->auth_grpllen = 128;
            *ctxt->auth_grplist = 0;
        }
        else if (htaccess_check_group(group, glen, ctxt->auth_grplist)) {

            /* The group is already on the list */
            return 0;
        }

        grpllen = strlen(ctxt->auth_grplist);

        newlen = grpllen + glen + 1;

        /* Grow the list buffer if necessary */
        if (newlen >= ctxt->auth_grpllen) {
            while (newlen >= ctxt->auth_grpllen) {
                ctxt->auth_grpllen += 128;
            }
            ctxt->auth_grplist = (char *)REALLOC(ctxt->auth_grplist,
                                                 ctxt->auth_grpllen);
        }

        cp = ctxt->auth_grplist;

        /* If this is not the first group, append a comma first */
        if (*cp) {
            cp[grpllen++] = ',';
            cp[grpllen] = 0;
        }

        /* Copy the group to the end of the list */
        strcpy(&cp[grpllen], group);

        return 1;
    }

    return 0;
}

int htaccess_init_group(char *grpfile, htaccess_context_s *ctxt)
{

#ifdef AUTHNSDBFILE
    int rv;

    if (ctxt->auth_nsdb) {
        void * authdb;

        /* Open the user database in preparation for checking groups */
        rv = nsadbOpen(0, grpfile, 0, &authdb);
        if (rv < 0) {
            return REQ_ABORTED;
        }

        ctxt->auth_authdb = authdb;
    }
    else
#endif /* AUTHNSDBFILE */
    {
        SYS_FILE fd;
        filebuffer *buf;
        struct stat finfo;

        if ((system_stat(grpfile, &finfo) < 0) || !S_ISREG(finfo.st_mode)) {
            log_error(LOG_MISCONFIG, "htaccess-find", ctxt->sn, ctxt->rq,
                      "invalid group database file %s", grpfile);
            return REQ_ABORTED;
        }

        /* Open group file */

        fd = system_fopenRO(grpfile);
        if (fd == SYS_ERROR_FD) {
            log_error(LOG_FAILURE, "htaccess-find", ctxt->sn, ctxt->rq, 
                      "can't open group file %s (%s)", grpfile, 
                      system_errmsg());
            return REQ_ABORTED;
        }

        buf = filebuf_open(fd, FILE_BUFFERSIZE);
        if(!buf) {
            log_error(LOG_FAILURE, "htaccess-find", ctxt->sn, ctxt->rq, 
                      "can't open buffer for group file %s (%s)",
                      grpfile, system_errmsg());
            system_fclose(fd);
            return REQ_ABORTED;
        }

        ctxt->auth_authdb = (void *)buf;

        /* Do we have a list of this user's groups? */
        if (ctxt->auth_grplist) {

            /* Is this the same group file used to construct the list? */
            if (strcmp(grpfile, ctxt->auth_grplfile)) {

                /* No, free the old list */
                FREE(ctxt->auth_grplist);
                FREE(ctxt->auth_grplfile);
                ctxt->auth_grplist = 0;
                ctxt->auth_grplfile = STRDUP(grpfile);
            }
        }
    }

    return REQ_PROCEED;
}

int htaccess_in_group(char *user, char *group, int glen, 
                      htaccess_context_s *ctxt)
{
    int rv;
    char groupbuf[128];

    /* Check for custom group authentication first */
    if (ctxt->group_check_fn) {
        pblock *npb;
        Request *rq = ctxt->rq;

        npb = pblock_create(10);
        pblock_copy(ctxt->pb, npb);
        param_free(pblock_remove("fn", npb)); /* don't want duplicate values */

        pblock_nvinsert("fn", ctxt->group_check_fn, npb);
        pblock_nvinsert("user", user, npb);
        pblock_nvinsert("group", group, npb);

        rv = func_exec(npb, ctxt->sn, rq);

        pblock_free(npb);

        if (rv != REQ_PROCEED)
            rv = 0;
        else
            rv = 1;
    } 
    else
#ifdef AUTHNSDBFILE
    /* Using Enterprise 2.0 database? */
    if (ctxt->auth_nsdb) {
        UserObj_t *uoptr = ctxt->auth_uoptr;
        GroupObj_t *goptr;

        if (glen >= sizeof(groupbuf)) {
            glen = sizeof(groupbuf) - 1;
        }
        strncpy(groupbuf, group, glen);

        /* If the user object doesn't match the desired user, free it */
        if (uoptr && strcmp(user, uoptr->uo_name)) {
            userFree(uoptr);
            ctxt->auth_uoptr = 0;
            uoptr = 0;
        }

        /* Look up the desired user if necessary */
        if (!uoptr) {
            rv = nsadbFindByName(0, ctxt->auth_authdb,
                                 user, AIF_USER, (void *)&uoptr);
            if (rv != AIF_USER) {
                return 0;
            }
            ctxt->auth_uoptr = uoptr;
        }

        /* Look up the desired group */
        rv = nsadbFindByName(0, ctxt->auth_authdb,
                             groupbuf, AIF_GROUP, (void *)&goptr);
        if (rv != AIF_GROUP) {
            return 0;
        }

        /* See if that group is in the user's list of groups */
        rv = usiPresent(&uoptr->uo_groups, goptr->go_gid);

        groupFree(goptr);
    }
    else
#endif /* AUTHNSDBFILE */
    {
        filebuffer *buf = (filebuffer *)(ctxt->auth_authdb);
        int ln;
        int eof;
        int c;
        int state;
        int len;
        int gmatch;
        int umatch;
        char *text, *groupname=NULL;
        char userbuf[128];

        /* Scan user file for desired group */
        state = 0;
        len = 0;
        text = groupbuf;
        umatch = 0;
        gmatch = 0;

        if (ctxt->auth_grplist &&
            htaccess_check_group(group, glen, ctxt->auth_grplist)) {
            /*
             * The desired group is in the list of groups of which
             * it has already been determined that the user is a member.
             */
            return 1;
        }

        /* Otherwise read from wherever we left off in the group file */
        for (eof = 0, ln = 1; !eof; ) {

            c = filebuf_getc(buf);
            switch (c) {
              case IO_EOF:
                  eof = 1;
                  /* FALL THROUGH TO '\n' */

              case '\n':

                  ++ln;
                  if (len > 0) {

                      /* Check for continuation - backslash preceding newline*/
                      if (text[len-1] == '\\') {
                          --len;
                          break;
                      }
                  }

                  /* Terminate text buffer */
                  text[len] = 0;

                  switch (state) {
                    case 0:		/* Accumulating group name */
                      
                        if (len > 0) {
                            /* Shouldn't be seeing a newline here */
                            log_error(LOG_WARN, "htaccess-find", ctxt->sn,
                                      ctxt->rq,
                                      "missing colon after group name %s",
                                      text);
                            gmatch = ((len == glen) &&
                                      !strncmp(group, text, len));
                        }
                        break;

                    case 1:		/* Skipping spaces before user */
                        break;

                    case 2:		/* Accumulating user name */

                        /* Newline ends user name */
                        umatch |= !strcmp(text, user);
                        break;
                  }

                  /* If the user matches any group name then cache it in
                   * case the 'require group' order isn't the same order
                   * found in the group file
                   */ 
                  if (umatch) {
                      /* Remember that the user is in this group */
                      htaccess_add_group(groupname, ctxt);
                  }

                  if (gmatch) {

                      /* Indicate whether the user was found */
                      return umatch;
                  }

                  /* Start accumulating next group name */
                  len = 0;
                  state = 0;
                  text = groupbuf;
                  umatch = 0;
                  break;

              case IO_ERROR:
                  log_error(LOG_WARN, "htaccess-find", ctxt->sn, ctxt->rq,
                            "error reading group file (%s)",
                            system_errmsg());
                  if (gmatch) {

                      if (umatch) {
                          htaccess_add_group(groupbuf, ctxt);
                          return 1;
                      }
                  }
                  return 0;

              case '\r':
                  /* Ignore CR */
                  break;

              default:
                  switch (state) {
                    case 0:
                        if (c == ':') {
                            /* Found end of group name */
                            text[len] = 0;
                            gmatch = ((glen == len) &&
                                      !strncmp(text, group, len));
                            groupname = text;
                            state = 1;
                            len = 0;
                            text = userbuf;
                            break;
                        }
                        break;

                    case 1:
                        if ((c != ' ') && (c != '\t')) {
                            state = 2;
                        }
                        break;

                    case 2:
                        if ((c == ' ') || (c == '\t')) {
                            text[len] = 0;
                            umatch |= !strcmp(text, user);
                            state = 1;
                            len = 0;
                            text = userbuf;
                        }
                        break;
                  }

                  if (state != 1) {
                      text[len++] = c;
                  }
                  break;
            }

            /* Protect from buffer overflow */
            if (len >= sizeof(userbuf)) {
                len = sizeof(userbuf) - 1;
                text[len] = 0;
            }
        }

        /* EOF and group not found */
        log_error(LOG_WARN, "htaccess-find", ctxt->sn, ctxt->rq,
                  "group %s is unknown", group);
        rv = 0;
    }

    return rv;
}

void htaccess_kill_group(htaccess_context_s *ctxt)
{
#ifdef AUTHNSDBFILE
    /* Free any saved user object */
    if (ctxt->auth_uoptr) {
        userFree(ctxt->auth_uoptr);
        ctxt->auth_uoptr = 0;
    }
#endif /* AUTHNSDBFILE */

    /* Close the user database if necessary */
    if (ctxt->auth_authdb) {

#ifdef AUTHNSDBFILE
        if (ctxt->auth_nsdb) {
            nsadbClose(ctxt->auth_authdb, 0);
        }
        else
#endif /* AUTHNSDBFILE */
        {
            filebuf_close((filebuffer *)(ctxt->auth_authdb));
        }
        ctxt->auth_authdb = 0;
    }
}

/* Do basic error checking on the IP address and fix up any matching so
 * in_ip is more efficient.
 */
const char * htaccess_check_allow(char * where) {
    char * s;

    if (!strcasecmp(where, "all"))
        return NULL;

    if ((s = strchr(where, '/'))) {
        unsigned long mask;

        *s++ = '\0';

        /* Make sure we have a valid IP address */
        if (!htaccess_is_ip(where)) {
            return "invalid IP address format";
        }

        /* Make sure the mask is ok */
        if (!htaccess_is_ip(s)) {
            return "invalid network mask";
        }

        /* mask is the form /a.b.c.d */
        if (strchr(s, '.')) {
            return NULL;
        } 

        /* assume mask is the form /nnn */
        mask = atoi(s);
        if (mask > 32 || mask <= 0) {
            return "invalid network mask";
        }
        return NULL;
    } else if (isdigit(*where) && htaccess_is_ip(where)) {
        /* handle a.b.c. ==> a.b.c.0/24 */

        char * t;
        int octet;
        int shift=24;

        s = where;
        while (*s) {
            t = s;
            if (!isdigit(*t))
                return "invalid IP address, 1 only digits and '.' allowed";

            while (isdigit(*t))
                ++t;

            if (*t == '.')
                *t++ = 0;
            else if (*t)
                return "invalid IP address, only digits and '.' allowed";

            if (shift < 0)
                return "invalid IP address, only 4 octets allowed";

            octet = atoi(s);
            if (octet < 0 || octet > 255)
                return "invalid IP address, octets must be in 0-255 range";

            s = t;
            shift -= 8; 
        }
        return NULL;
    }

    return NULL;
}
