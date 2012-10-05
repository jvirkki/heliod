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
 * File:    Cgistub.c
 *
 * Description:
 *
 *      This program is the child stub process which fork/exec's
 *      child processes upon request of an application.
 *      This was designed for use at Netscape by the Enterprise
 *      Server application to implement a fast CGI engine.
 *
 * Contact:  mike bennett (mbennett@netcom.com)     08Mar98
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <ctype.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#if defined(SOLARIS)
#include <dlfcn.h> /* dlopen */
#endif
#if !defined(LINUX)
#include <sys/stropts.h>
#endif
#if !defined(USE_CONNLD)
#include <sys/un.h>
#endif
#include <sys/uio.h>
#include <fcntl.h>
#if defined(HPUX) || defined(AIX)
#include <sys/wait.h>
#else
#include <wait.h>
#endif
#ifndef HPUX
#include <sys/select.h>
#endif
#include <pwd.h>
#include <grp.h>
#include "Cgistub.h"

void killcgis(void)
{
    kill(-getpid(), SIGTERM);
}

static void SetupCgiSignalDisposition();
static void SetupCgiStubSignalDisposition();


#define VERSION     "0.6 18Jul01"

#define SA          struct sockaddr

#define LCL_VARARGSZ   30 /* for default variable sizing */

#ifndef LINUX
#define MIN_USER_ID   100
#else
#define MIN_USER_ID    99 /* RedHat Linux uses uid 99 / gid 99 for nobody / nobody. */
#endif

/* Convert literal to a string via preprocessor */
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#undef MIN
#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#undef MAX
#define MAX(a, b)   ((a) > (b) ? (a) : (b))

const char * fileName    = NULL;
int          listen_fd   = 0;
const char * myName;
int          min_user_uid = MIN_USER_ID;    /* minimum non-system uid */
int          trusted_uid = -1;              /* trust no one */
#ifdef _BE_VERBOSE
int          verbose = 1;                   /* be noisy */
#else
int          verbose = 0;                   /* be noisy */
#endif

typedef struct {
    char *   user_name;             /* user name */
    char *   group_name;            /* group name */
    char *   ch_dir;                /* chdir path */
    char *   ch_root;               /* chroot path */
    char *   nice;                  /* nice value */
    char *   rlimit_as;             /* RLIMIT_AS values */
    char *   rlimit_core;           /* RLIMIT_CORE values */
    char *   rlimit_cpu;            /* RLIMIT_CPU values */
    char *   rlimit_nofile;         /* RLIMIT_NOFILE values */
    char *   path;                  /* program path */
    char *   prog;                  /* executable (argv0) */
    char **  argv;                  /* argv array */
    char **  envp;                  /* environment variables array */
} child_parms_t;

/*
 * This parameter block is written to the binary's .data section and parsed at
 * runtime, allowing Cgistub behaviour to be modified without recompilation
 */
char parameters[512] = "Cgistub parameter block\0"
                       "min_user_uid "STRINGIFY(MIN_USER_ID)"       \n"
                       "trusted_uid -1        \n";

/*
 * Return a pointer into a parameter block
 */
static void get_parameter(char *block, char *name, char **buffer, int *size)
{
    char *param = block + strlen(block) + 1;
    int namelen = strlen(name);

    while (*param) {
        char *space = strchr(param, ' ');
        char *newline = strchr(space, '\n');
        int paramlen = space - param;

        if (paramlen == namelen && !memcmp(param, name, namelen)) {
            *buffer = space + 1;
            *size = newline - space - 1;
            return;
        }

        param = newline + 1;
    }

    *buffer = NULL;
    *size = 0;
}

/*
 * Strip trailing slashes from a path
 */
static void strip_trailing_slashes(char *filename)
{
    int len = strlen(filename);
    if (len > 0) {
        while (filename[len - 1] == '/') {
            len--;
        }
    }
    filename[len] = '\0';
}

/*
 * Get the directory name from a path that contains a filename
 */
static char* get_dir(const char *filename)
{
    char *dir = strdup(filename);
    char *t;

    strip_trailing_slashes(dir);

    /* strip filename component */
    if (filename[0]) {
        t = strrchr(dir, '/');
        if (t) {
            t[1] = '\0';
        } else {
            strcpy(dir, ".");
        }
    }

    return dir;
}

/*
 * Look for string in file fp
 */
static long fstr(FILE *fp, char *string, int len)
{
    int matched = 0;
    long pos = 0;

    for (;;) {
        /* get one character from the file */
        int c = fgetc(fp);
        if (c == EOF) break;
        pos++;

        /* compare one character from string */
        if (c == string[matched]) {
            matched++;
            if (matched == len) {
                return pos - len;
            }
        } else {
            matched = 0;
        }
    }

    return -1;
}

/*
 * Work around the fact HP-UX won't let us unlink a currently excuting image.
 * Make sure you trust the user before calling this as setuid root, or
 * something horrific could happen.
 */
static int hp_create_backup(const char *path)
{
    char *filename = strdup(path);
    int size;
    char *buffer;

    strip_trailing_slashes(filename);

    /* allocate a buffer large enough for our biggest shell command */
    size = strlen("cp -f ") + strlen(filename) + 1 + strlen(filename) + strlen(".tmp4294967296") + 1;
    buffer = malloc(size);

    /* make a temporary copy of the file */
    sprintf(buffer, "cp -f %s %s.tmp.%u", filename, filename, getpid());
    if (system(buffer)) {
        return -1;
    }

    /* backup the image */
    sprintf(buffer, "%s.backup", filename);
    if (rename(filename, buffer)) {
        return -1;
    }

    /* rename the temporary copy; this is the file we will edit */
    sprintf(buffer, "%s.tmp.%u", filename, getpid());
    if (rename(buffer, filename)) {
        return -1;
    }

    free(filename);
    free(buffer);

    return 0;
}

/*
 * Replace a file with a duplicate of itself.  Make sure you trust the user
 * before calling this as setuid root, or something horrific could happen.
 */
static int duplicate_file(const char *path)
{
    char *filename = strdup(path);
    int size;
    char *buffer;

    strip_trailing_slashes(filename);

    /* allocate a buffer large enough for the cp shell command */
    size = strlen("cp -f ") + strlen(filename) + 1 + strlen(filename) + strlen(".tmp4294967296") + 1;
    buffer = malloc(size);

    /* copy the original file to a temporary file; I'm lazy, so I use cp */
    sprintf(buffer, "cp -f %s %s.tmp%u", filename, filename, getpid());
    if (system(buffer)) {
        return -1;
    }

    /* rename the temporary file back to the original name */
    sprintf(buffer, "%s.tmp%u", filename, getpid());
    if (rename(buffer, filename)) {
        unlink(buffer);
        return -1;
    }

    free(filename);
    free(buffer);

    return 0;
}

/*
 * Write a value into the parameter block
 */
static void set_parameter(const char *filename, char *arg)
{
    char *value;
    struct stat finfo;
    char *dir;
    FILE *fp;
    long pos;
    char block[sizeof(parameters)];
    char *buffer = NULL;
    int size;

    /* separate the name and value within arg */
    value = strchr(arg, ' ');
    if (!value) return; /* invalid syntax */
    *value = '\0';
    value++;

    /* only the owner of the binary may change its parameters */
    if (stat(filename, &finfo)) {
        fprintf(stderr, "error: could not access %s\n", filename);
        exit(1);
    }
    if ((geteuid() != getuid()) || (getuid() != finfo.st_uid)) {
        struct passwd *pw;
        pw = getpwuid(finfo.st_uid);
        if (pw) {
            fprintf(stderr, "error: only user %s may set parameters\n", pw->pw_name);
        } else {
            fprintf(stderr, "error: only user %d may set parameters\n", finfo.st_uid);
        }
        exit(1);
    }

    /* make sure we have write access to the directory, too */
    dir = get_dir(filename);
    if (access(dir, W_OK)) {
        fprintf(stderr, "error: directory %s is not writeable\n", dir);
        exit(1);
    }
    free(dir);

    /*
     * there are races between the above checks and the stuff we do below, but
     * we don't care about that
     */

    /* 
     * replace the binary with a duplicate of itself as some OSs won't let us
     * edit an active program's image
     */
    if (duplicate_file(filename) && errno == ETXTBSY) {
        /* this smells like HP-UX; try to work around the problem */
        if (hp_create_backup(filename) && errno == ETXTBSY) {
            fprintf(stderr, "error: %s is currently running\n", filename);
            exit(1);
        }
    }

    /* make sure we have read and write access to the binary */
    if (access(filename, R_OK|W_OK)) {
        fprintf(stderr, "error: file %s must be readable and writeable\n", filename);
        exit(1);
    }

    /* open the binary */
    fp = fopen(filename, "rb+");
    if (!fp) {
        fprintf(stderr, "error: could not open %s\n", filename);
        exit(1);
    }

    /* search for the parameter block in the binary */
    fseek(fp, 0, SEEK_SET);
    pos = fstr(fp, parameters, strlen(parameters));
    if (pos == -1) {
        fprintf(stderr, "error: could not find parameter block in %s\n", filename);
        exit(1);
    }

    /* get the parameter block */
    fseek(fp, pos, SEEK_SET);
    if (fread(block, sizeof(block), 1, fp) != 1) {
        fprintf(stderr, "error: could not read parameter block from %s\n", filename);
        exit(1);
    }

    /* get the parameter's position in the parameter block */
    get_parameter(block, arg, &buffer, &size);
    if (!buffer) {
        fprintf(stderr, "error: unrecognized parameter %s\n", arg);
        exit(1);
    }

    /* check the new value */
    if (strlen(value) > size || strchr(value, '\n')) {
        fprintf(stderr, "error: invalid value\n");
        exit(1);
    }

    /* update the parameter block with the new value */
    memset(buffer, ' ', size);
    memcpy(buffer, value, strlen(value));

    /* write the updated parameter block to disk */
    fseek(fp, pos, SEEK_SET);
    if (fwrite(block, sizeof(block), 1, fp) != 1) {
        fprintf(stderr, "error: error writing to %s\n", filename);
        exit(1);
    }
    fclose(fp);

    fprintf(stderr, "%s %s\n", arg, value);

    exit(0); /* success */
}

/*
 * Get the integral value of a parameter from the parameter block
 */
static int get_int_parameter(char *name)
{
    char *buffer = NULL;
    int size;
    get_parameter(parameters, name, &buffer, &size);
    return buffer ? atoi(buffer) : -1;
}

/*
 * Parse the runtime configuration from the parameter block
 */
static void parse_parameters(void)
{
    min_user_uid = get_int_parameter("min_user_uid");
    trusted_uid = get_int_parameter("trusted_uid");
}

/*
 *  parse string and call setrlimit with the result
 *  returns the errno value on error, 0 otherwise
 */
int setrlimit_wrapper(int resource, const char* string)
{
    struct rlimit rl;
    long limit1;
    long limit2;
    char* end;
    int rc;

    if (!string || !*string) return 0; /* success, no params to set */

    /* 
     * XXX we don't do the string-to-rlim_t conversion properly when rlim_t is
     * long long and the numeric values from string won't fit in a long
     */

    /* get the first number */
    limit1 = strtol(string, &end, 0);
    if (end == string) return EINVAL; /* invalid parameter */

    /* skip past any white space and punctuation */
    string = end;
    while (*string && (isspace(*string) || ispunct(*string))) string++;

    /* get the second number */
    limit2 = strtol(string, &end, 0);
    if (end == string) limit2 = limit1; /* no second number given */

    /* rlim_cur is the lesser of limit1 and limit2, rlim_max the greater */
    memset(&rl, 0, sizeof(rl));
    rl.rlim_cur = MIN(limit1, limit2);
    rl.rlim_max = MAX(limit1, limit2);

    /* set params */
    if (setrlimit(resource, &rl)) return errno; /* failure */

    return 0; /* success */
}

/*
 *  exec the new process (this runs as a child)
 *  this returns an error indication and sets the
 *  rsp_info to be the detail error info.
 *  note that this NEVER returns upon success, as
 *  we exec a new process.
 *  also, be aware we use some functions (e.g. getpwnam, getgrnam, initgroups)
 *  that manipulate global variables, opening the door for potential problems
 *  if we use vfork (i.e. DON'T do passwd/group stuff from both the parent and
 *  vfork'd child)
 */
crsp_type_e 
child_code( child_parms_t * parms, int * rsp_info )
{
    uid_t caller_uid;
    uid_t child_uid;
    struct passwd* pw = NULL;
    char* path = parms->path;
    char* dir = parms->ch_dir;
    struct sigaction sa;
    int rc;

    /* remember the uid of whoever executed us */
    caller_uid = getuid();
    child_uid = caller_uid;

    /* setrlimit */
    if ((rc = setrlimit_wrapper(RLIMIT_AS, parms->rlimit_as)) ||
        (rc = setrlimit_wrapper(RLIMIT_CORE, parms->rlimit_core)) ||
        (rc = setrlimit_wrapper(RLIMIT_CPU, parms->rlimit_cpu)) ||
        (rc = setrlimit_wrapper(RLIMIT_NOFILE, parms->rlimit_nofile)))
    {
        /* parse or setrlimit error */
        *rsp_info = rc;
        return CRSP_SETRLIMITFAIL;
    }

    /* nice */
    if (parms->nice && *parms->nice) {
        char* end;
        int incr;

        incr = strtol(parms->nice, &end, 0);
        if (end == parms->nice) {
            *rsp_info = EINVAL;
            return CRSP_NICEFAIL;
        }

        /* under Solaris, nice can return -1 on success */
        errno = 0;
        if ((nice(incr) == -1) && errno) {
            *rsp_info = errno;
            return CRSP_NICEFAIL;
        }
    }

    /* if a specific user was requested... */
    if (parms->user_name) {
        /* lookup the user */
        pw = getpwnam(parms->user_name);
        if (!pw) {
            *rsp_info = errno;
            return CRSP_USERFAIL;
        }
    }

    /*
     * set group
     * do this before we set the uid, as our current euid should be root 
     * (so we can set any gid) if Cgistub is supposed to allow group changes
     */
    if (parms->group_name) {
        /* lookup the group */
        struct group* gr;
        gr = getgrnam(parms->group_name);
        if (!gr) {
            *rsp_info = errno;
            return CRSP_GROUPFAIL;
        }

        /* change to the group */
        if (setgid(gr->gr_gid)) {
            *rsp_info = errno;
            return CRSP_GROUPFAIL;
        }
    } else if (pw) {
        /* 
         * no specific group was requested, but we're setting the user, so
         * setgid to the user's base gid
         */
        if (setgid(pw->pw_gid)) {
            *rsp_info = errno;
            return CRSP_USERFAIL;
        }
    }

    /* if we've been asked to set the user... */
    if (pw) {
        /* set the supplementary group access list for this user */
        initgroups(pw->pw_name, pw->pw_gid);
    }

    /*
     * chroot, if requested
     * Some important notes:
     * 1. this is done after we do all the user/group lookup stuff (therefore
     *    we're using the /etc/passwd, etc. relative to the server's root, not
     *    our chroot'd-to root)
     * 2. this is done before we do a setuid (as we need to be root to chroot)
     */
    if (parms->ch_root) {
        int len_ch_root;

        /* do the chroot */
        if (chroot(parms->ch_root)) {
            *rsp_info = errno;
            return CRSP_CHROOTFAIL;
        }

        /* find out how long ch_root is, not including any trailing '/' */
        len_ch_root = strlen(parms->ch_root);
        if (len_ch_root && parms->ch_root[len_ch_root-1] == '/') len_ch_root--;

        /* if the front of path is the same as ch_root... */
        if ((strlen(path) > len_ch_root) &&
            (path[len_ch_root] == '/') &&
            (!memcmp(path, parms->ch_root, len_ch_root)))
        {
            /* 
             * path is the path as seen by the server; adjust it so it's 
             * relative to our newly chroot'd-to root
             */
            path += len_ch_root;
        }

        /* if the front of dir is the same as ch_root... */
        if (dir && (strlen(dir) >= len_ch_root) &&
            (dir[len_ch_root] == '/' || dir[len_ch_root] == '\0' ) &&
            (!memcmp(dir, parms->ch_root, len_ch_root)))
        {
            /* 
             * dir is the path as seen by the server; adjust it so it's 
             * relative to our newly chroot'd-to root
             */
            dir += len_ch_root;
        }
    }

    /* if we've been asked to set the user... */
    if (pw) {
        /* set the user */
        if (setuid(pw->pw_uid)) {
            *rsp_info = errno;
            return CRSP_USERFAIL;
        }
        child_uid = pw->pw_uid;
    } else if (geteuid() != caller_uid) {
        /* caller didn't say who she wanted to be, but she's not herself */
        /* we're probably effectively root, try to change to the server user */
        if (setuid(caller_uid)) {
            *rsp_info = EPERM;
            return CRSP_USERFAIL;
        }
        child_uid = caller_uid;
    }

    /* set the cwd */
    if (dir && *dir) {
        /* chdir to path from request */
        if (chdir(dir)) {
            *rsp_info = errno;
            return CRSP_CHDIRFAIL;
        }
    }

    /*
     * security check
     * is caller attempting to change users?
     */
    if (child_uid != caller_uid) {
        struct stat st;

        /* 
         * caller is attempting to change users, be very picky about what is 
         * and is not allowed
         */

        /* are we attempting to change to a system user? */
        if (child_uid < min_user_uid) {
            /* only root can run a CGI as a system user */
            if (caller_uid != 0) {
                *rsp_info = EPERM;
                return CRSP_USERFAIL;
            }
        }

        /* get permissions on the program we're trying to execute */
        if (stat(path, &st)) {
            *rsp_info = errno;
            return CRSP_STATFAIL;
        }

        /* are we trying to execute something we don't own? */
        if (st.st_uid != child_uid) {
            /* if it's not owned by the trusted user... */
            if (trusted_uid < 0 || st.st_uid != trusted_uid) {
                *rsp_info = EACCES;
                return CRSP_EXECOWNER;
            }
        }

        /* trying to exec something we're not allowed to is handled by exec */

        /* are we trying to execute something someone else can write to? */
        if (st.st_mode & (S_IWGRP | S_IWOTH)) {
            *rsp_info = EACCES;
            return CRSP_EXECPERMS;
        }
    }

    /* cleanup for the kid */
    /* setsid(); */

    /* disable SIGPIPE  */
    sa.sa_flags = 0;
    sigemptyset( &sa.sa_mask );
    sa.sa_handler = SIG_DFL;
    sigaction( SIGPIPE, &sa, NULL );

    /* now, exec the child */
    if ( parms->envp ) {
        execve( path, parms->argv, parms->envp );
    } else {
        execv( path, parms->argv );
    }

    /* oops! ..... we're still here */
    *rsp_info = errno;
    return CRSP_EXECFAIL;
}

/*
 *  build a varargs from a linear buffer 
 *  initial_pad is for leaving space for argv[0]
 */
char **
unbuild_varargs_array( char ** argp, char * lb_start, int lb_len, 
                       int initial_pad, int argp_sz )
{
    char *  lb     = lb_start;
    char *  lb_end = &lb[lb_len];
    int     ndx;
    char ** new_argp_area;
    char ** new_argp;

    for ( ndx = initial_pad; (lb < lb_end) && 
         (ndx < argp_sz - 1); ndx++ ) {
        argp[ndx] = lb;
        lb += (strlen( lb ) + 1);
    }
    if ( lb >= lb_end ) {
        argp[ndx] = NULL;
        return argp;
    }

    /* overflow of argp; try it again, this time with a 2x bigger area */
    argp_sz *= 2+2;
    new_argp_area = (char **) calloc( argp_sz, sizeof (char *) );
    if ( new_argp_area == NULL ) {
        if ( verbose ) 
            fprintf(stderr, "%s: calloc failure resizing for %d args\n", 
                    myName, argp_sz );
        return NULL;
    }
    new_argp = unbuild_varargs_array( new_argp_area, lb_start, lb_len,
                                      initial_pad, argp_sz );
    /*
     * if the recursed routine had to realloc itself, then free our
     * temp area 
     */
    if ( new_argp && new_argp != new_argp_area ) {
        free( new_argp_area );
    }
    return new_argp;
}

void
print_varargs( char ** va, const char * txt )
{
    int ndx = 0;

    for ( ndx = 0; va[ndx]; ndx++ ) {
        fprintf(stderr, "%s:    %s[%d]: %s\n", myName, txt, ndx, va[ndx] );
    }
}

/*
 *  parse a start request and extract the parameters sent
 *  return true if successful
 */
int
parse_start_req( cstub_start_req_t * rq, size_t rq_len, child_parms_t * parms,
                 int lcl_argsz )
{
    char * cptlv = (char *) &rq->cs_tlv;
    char * eov   = (char *) rq + rq_len;

    while ( cptlv < eov ) {
        creq_tlv_t * ptlv;
        /* LINTED */
        ptlv = (creq_tlv_t *) cptlv;
        switch( ptlv->type ) {
        case CRQT_USERNAME:      parms->user_name = &ptlv->vector; break;
        case CRQT_GROUPNAME:     parms->group_name = &ptlv->vector; break;
        case CRQT_CHDIRPATH:     parms->ch_dir = &ptlv->vector; break;
        case CRQT_CHROOTPATH:    parms->ch_root = &ptlv->vector; break;
        case CRQT_NICE:          parms->nice = &ptlv->vector; break;
        case CRQT_RLIMIT_AS:     parms->rlimit_as = &ptlv->vector; break;
        case CRQT_RLIMIT_CORE:   parms->rlimit_core = &ptlv->vector; break;
        case CRQT_RLIMIT_CPU:    parms->rlimit_cpu = &ptlv->vector; break;
        case CRQT_RLIMIT_NOFILE: parms->rlimit_nofile = &ptlv->vector; break;
        case CRQT_PATH:          parms->path = &ptlv->vector; break;
        case CRQT_PROG:          parms->prog = &ptlv->vector; break;
        case CRQT_ENVP:  
            parms->envp = 
                unbuild_varargs_array( parms->envp, &ptlv->vector,
                                       ptlv->len - TLV_VECOFF, 0,
                                       lcl_argsz );
            break;
        case CRQT_ARGV:  
            /* offset by 1 in argv for room for argv0 */
            parms->argv = 
                unbuild_varargs_array( parms->argv, &ptlv->vector, 
                                       ptlv->len - TLV_VECOFF, 1,
                                       lcl_argsz );
            break;
        case CRQT_END:
            /* ignore remainder */
            cptlv = eov; /* yeah, it'll go way past when round_up is done*/
            break;
        default:
            fprintf(stderr, "%s: WARNING - skipping unknown TLV value [%d]\n",
                    myName, (int) ptlv->type );
        }
        if ( ptlv->len < TLV_VECOFF ) {
            fprintf(stderr, "%s: WARNING : corrupt TLV length [%d]\n",
                    myName, (int) ptlv->len );
            return 0;
        }
        cptlv += ROUND_UP( ptlv->len, TLV_ALIGN );
    }

    if ( verbose ) {
        fprintf(stderr, "%s: TLVParser : ", myName );
        fprintf(stderr, "chdir [%s], ", (parms->ch_dir) ? parms->ch_dir : "NULL" );
        fprintf(stderr, "chroot [%s], ", (parms->ch_root) ? parms->ch_root : "NULL" );
        fprintf(stderr, "path [%s], ", (parms->path) ? parms->path : "NULL" );
        fprintf(stderr, "prog [%s], ", (parms->prog) ? parms->prog : "NULL" );
        fprintf(stderr, "envp [%s], ", (parms->envp) ? "(below)" : "NULL" );
        fprintf(stderr, "argv [%s]\n", (parms->argv) ? "(below)" : "NULL" );

        if ( parms->envp ) {
            print_varargs( parms->envp, "envp" );
        }
        if ( parms->argv ) {
            print_varargs( parms->argv, "argv" );
        }
    }

    /*  have to at LEAST have a program name and path */
    if ( ! parms->prog || ! parms->path ) {
        return 0;
    }
    /*  insert program name as argv[0] */
    parms->argv[0] = parms->prog;

    return 1;
}

/*
 *  handle a start request
 */
void
process_start_req( cstub_start_req_t * rq, size_t rq_len, 
                   cstub_start_rsp_t * rsp, int * rsp_desc,
                   int * ndesc, int connfd )
{
    int             pin[2];
    int             pout[2];
    int             perr[2];
    int             pip[2];
    int             flags;
    int             pid;
    int             count;
    child_parms_t   parms;
    char *          lcl_argv[LCL_VARARGSZ];
    char *          lcl_envp[LCL_VARARGSZ];
    int             childResponse[2];
#define CLEANUP() \
        if ( parms.argv && parms.argv != lcl_argv ) free( parms.argv ); \
        if ( parms.envp && parms.envp != lcl_envp ) free( parms.envp )

    /*
     *  parse the start request information
     */
    *ndesc = 0;

    memset( &parms, 0, sizeof( parms ));
    memset( &lcl_argv[0], 0, sizeof( lcl_argv ));
    memset( &lcl_envp[0], 0, sizeof( lcl_envp ));
    parms.argv = lcl_argv;
    parms.envp = lcl_envp;

    if ( ! parse_start_req( rq, rq_len, &parms, LCL_VARARGSZ )) {
        rsp->crsp_rspinfo = CRSP_MISSINGINFO;
        CLEANUP();
        return;
    }

    if ( pipe( pin ) == -1 ) {
        rsp->crsp_rspinfo = CRSP_RESOURCE;
        rsp->crsp_errcode = errno;
        CLEANUP();
        return;
    }
    if ( pipe( pout ) == -1 ) {
        rsp->crsp_rspinfo = CRSP_RESOURCE;
        rsp->crsp_errcode = errno;
        close( pin[0] );
        close( pin[1] );
        CLEANUP();
        return;
    }
    if ( pipe( perr ) == -1 ) {
        rsp->crsp_rspinfo = CRSP_RESOURCE;
        rsp->crsp_errcode = errno;
        close( pin[0] );
        close( pin[1] );
        close( pout[0] );
        close( pout[1] );
        CLEANUP();
        return;
    }
    if ( pipe( pip ) == -1 ) {
        rsp->crsp_rspinfo = CRSP_RESOURCE;
        rsp->crsp_errcode = errno;
        close( pin[0] );
        close( pin[1] );
        close( pout[0] );
        close( pout[1] );
        close( perr[0] );
        close( perr[1] );
        CLEANUP();
        return;
    }
    /* enable close-on-exec to monitor if the exec() worked */
    fcntl( pip[0], F_SETFD, FD_CLOEXEC );
    fcntl( pip[1], F_SETFD, FD_CLOEXEC );
    
    /* start the child */
    /*
     * porting note -- not all platforms support vfork(); vfork is much more 
     * efficient on Solaris -- at a price.  The downside is you must be very 
     * careful regarding what you do as a child as you do NOT get a separate 
     * address space and anything you muck around with in the child must not 
     * be anything that would affect the parent. 
     * SUCH as, returning from the vfork, free() or malloc() calls, touching
     * global variables, ....
     * vfork() is intended as a prelude to an exec..() call; as that's what
     * we're about to do, everybody's happy.
     */
#if defined(HAVE_VFORK)
    pid = vfork();
#else
    pid = fork();
#endif
    if ( pid == -1 ) {
        rsp->crsp_rspinfo = CRSP_FORKFAIL;
        rsp->crsp_errcode = errno;
        close( pin[0] );
        close( pin[1] );
        close( pout[0] );
        close( pout[1] );
        close( perr[0] );
        close( perr[1] );
        close( pip[0] );
        close( pip[1] );
        CLEANUP();
        return;
    } else if ( pid == 0 ) {
        close( pin[1] );
        close( pout[0] );
        close( perr[0] );
        close( pip[0] );
        close( connfd );
        
        if ( pin[0] != STDIN_FILENO ) {
            dup2( pin[0], STDIN_FILENO );
            close( pin[0] );
        }
        if ( pout[1] != STDOUT_FILENO ) {
            dup2( pout[1], STDOUT_FILENO );
            close( pout[1] );
        }
        if ( perr[1] != STDERR_FILENO ) {
            dup2( perr[1], STDERR_FILENO );
            close( perr[1] );
        }

        SetupCgiSignalDisposition();

        childResponse[0] = child_code( &parms, &childResponse[1] );
    
        /*
         * if we got here, this means we failed in child_code.
         * BUT, since we are now in the fork()ed, child, we
         * have to let our dad know why
         */
        write( pip[1], (char *)childResponse, sizeof( childResponse ) );
        _exit(1);
    }

    /* we have a running child process now */
    rsp->crsp_pid = pid;

    /* close unneeded fds (as parent) */
    close( pin[0] );
    close( pout[1] );
    close( perr[1] );
    close( pip[1] );

    if ( verbose ) {
        fprintf( stderr, "%s: started child pid %d\n", myName, pid );
    }

    /*
     *  wait for the close-on-exec pipe to close; if we received a byte,
     *  the exec failed (we got to the 'childResponse' code above)
     */
    count = read( pip[0], (char *)childResponse, sizeof( childResponse ) );
    close( pip[0] );

    if ( count > 0 ) {
        rsp->crsp_rspinfo = childResponse[0];
        rsp->crsp_errcode = childResponse[1];
        if ( verbose ) {
            fprintf( stderr, "%s: exec failure on pid %d, info %d, code %d\n", 
                    myName, pid, rsp->crsp_rspinfo, rsp->crsp_errcode );
        }
        waitpid( pid, NULL, 0 );
        close( pin[1] );
        close( pout[0] );
        close( perr[0] );
        CLEANUP();
        return;
    }

    /*  okay; now, return the fds back to the calling program */
    rsp_desc[0] = pin[1];
    rsp_desc[1] = pout[0];
    rsp_desc[2] = perr[0];
    * ndesc = 3;
    if ( verbose ) {
        fprintf( stderr, "%s: exec OK for pid %d\n",
                myName, pid );
    }

    CLEANUP();
    return;
}

/*
 *  handle a request 
 */
/* ARGSUSED */
ssize_t
process_message( int connfd, void * pData, int pDatalen )
{
#if defined(USE_CONNLD)
    int			i;
#else
    struct msghdr       msg;
    struct iovec        iov[1];
#if defined(USE_POSIXFDPASSING)
    int			clen;
    struct cmsghdr *	chdr;
#endif
#endif
    ssize_t             nsent;
    int                 ndx;
    cstub_start_req_t * rq;
    cstub_start_rsp_t   rsp;
    int                 rsp_desc[3];
    int                 ndesc = 0;

    if ( verbose ) {
        fprintf(stderr, "%s: got a message, %d bytes long\n",
                myName, pDatalen );
    }

    rq = (cstub_start_req_t *) pData;
    if ( pDatalen < sizeof( *rq )) {
        fprintf( stderr, "WARNING - got short message from client %d bytes; aborting\n",
                 pDatalen );
        _exit(1);
    }
    rsp.crsp_msgtype  = rq->cs_msgtype;
    rsp.crsp_version  = CSTUBVERSION;
    rsp.crsp_rspinfo  = CRSP_OK;
    rsp.crsp_errcode  = 0;
    rsp.crsp_pid      = 0;

    if ( rq->cs_version != CSTUBVERSION ) {
        fprintf(stderr, "%s: got an INVALID VERSION id [%d]\n",
                             myName, rq->cs_version );
        rsp.crsp_rspinfo = CRSP_BADVERSION;
    }

    if ( rsp.crsp_rspinfo == CRSP_OK ) 
    switch (rq->cs_msgtype ) {
        case CRQ_START: 
            if ( verbose ) 
                fprintf(stderr, "%s: got a START\n", myName );
            process_start_req( rq, pDatalen, &rsp, rsp_desc, &ndesc, connfd );
            break;
        default: 
            fprintf(stderr, "%s: got an INVALID request id [%d]\n",
                             myName, rq->cs_msgtype );
            rsp.crsp_rspinfo = CRSP_BADMSGTYPE;
    }

#if defined(USE_CONNLD)
    rsp.crsp_nfds = ndesc;
    nsent = write(connfd, &rsp, sizeof(rsp));
    if ( nsent < 0 ) {
        perror("write");
    }
    for (i=0; i<ndesc; i++) {
        if (ioctl(connfd, I_SENDFD, rsp_desc[i]) < 0) {
            perror("ioctl(I_SENDFD) failed");
        }
    }
#else
    msg.msg_name    = NULL;
    msg.msg_namelen = 0;

    iov[0].iov_base = (char *) &rsp;
    iov[0].iov_len  = sizeof( rsp );
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 1;

#if defined(USE_POSIXFDPASSING)
    clen = sizeof(struct cmsghdr) + ndesc * sizeof(int);
    chdr = (struct cmsghdr *)malloc(clen);

    chdr->cmsg_len = clen;
    chdr->cmsg_level = SOL_SOCKET;
    chdr->cmsg_type = SCM_RIGHTS;
#if 0
    fprintf(stderr, "STUB: chdr->cmsg_level = %d, chdr->cmsg_len = %d, chdr->cmsg_type = %d\n", chdr->cmsg_level, chdr->cmsg_len, chdr->cmsg_type);
#endif
    memcpy(CMSG_DATA(chdr), rsp_desc, sizeof(int) * ndesc);

    msg.msg_control    = chdr;
    msg.msg_controllen = clen;
#else
    msg.msg_accrights    = (caddr_t)     rsp_desc;
    msg.msg_accrightslen = sizeof(int) * ndesc;
#endif

    nsent = sendmsg( connfd, &msg, 0 );
    if ( nsent < 0 ) {
        perror("sendmsg");
    }
#endif

    for (  ndx = 0; ndx < ndesc; ndx++ ) {
        close( rsp_desc[ndx] );
    }

#if defined(USE_POSIXFDPASSING)
    free(chdr);
#endif

    return nsent;
}

/*
 *  create our bound listen socket
 */
int
create_bound_socket( const char * strName )
{
    int     fd;
#if defined(USE_CONNLD)
    int     pfeiferl[2]; /* local flavour */

    /*
     *  create the end point, only the user that executed us has access
     */
    unlink( strName );
    if ((fd = creat(strName, 0600)) < 0) {
        perror("creat endpoint");
        exit(1);
    }
    if (close(fd) < 0) {
        perror("close endpoint");
        exit(1);
    }

    if (pipe(pfeiferl) < 0) {
        perror("connld pipe");
        exit(2);
    }

    if (ioctl(pfeiferl[1], I_PUSH, "connld") < 0) {
        perror("I_PUSH connld");
        exit(3);
    }

    if (fattach(pfeiferl[1], strName) < 0) {
        perror("fattach");
        exit(4);
    }

    return (pfeiferl[0]);
#else
    struct  sockaddr_un servaddr;

    /*
     *  create the socket
     */
    fd = socket( AF_UNIX, SOCK_STREAM, 0 );
    if ( fd == -1 ) {
        perror("socket");
        exit(1);
    }

    /*
     *  make it visible
     */
    memset( (char *)&servaddr, 0, sizeof( servaddr ));
    servaddr.sun_family = AF_UNIX;
    strcpy( servaddr.sun_path, strName );
    unlink(strName);
    if ( bind( fd, (SA *)&servaddr, sizeof( servaddr )) < 0 ) {
        perror("bind");
        exit(2);
    }

    /*
     *  Since our uid may change part way through execution, allow connections
     *  from any user.  The socket should have been created in a protected
     *  directory, so this should be safe.
     */
    if (chmod(strName, 0777)) {
        perror("chmod");
        exit(3);
    }
    listen( fd, 1024 );

    return (fd);
#endif
}

/*
 *  process requests from our clients
 *  this is the second-stage process' main loop;  this is
 *  entered when a new request pipe has been created (due
 *  to an accept() completing on the listen socket)
 *  when a close (hangup,eof) occurs on our side of the
 *  pipe, we go away
 */
void
wait_for_request( int connfd )
{
    int         buf_size = 10240; /* initial default */
    char *      iobuf = malloc( buf_size );
    int         iobuf_len = 0;
    int         readlen;
    /* LINTED */
    cstub_start_req_t * pRec = (cstub_start_req_t *)iobuf;
    
    if ( verbose ) {
        fprintf( stderr, "pid %d: waiting for request on fd %d\n",
                getpid(), connfd );
    }

    while ( (readlen = read( connfd, &iobuf[iobuf_len], buf_size - iobuf_len )) > 0 ) {
        int pDataLen = 0;
        if ( verbose ) {
            fprintf( stderr, "pid %d: got %d bytes in request from fd %d\n",
                     getpid(), readlen, connfd );
        }
                
        /*
         *  as we're using STREAM sockets, wait until we've a complete
         *  message
         */
        iobuf_len += readlen;
        pDataLen = pRec->cs_msgLength;
        while ( iobuf_len >= sizeof( cstub_start_req_t ) && 
                (pDataLen  <= iobuf_len ) ) {
            /*
             *  the request length includes the whole cstub_start_req_t record
             */
            if ( process_message( connfd, &iobuf[0], pDataLen) < 0 ) {
                _exit(1);
            }

            /*
             *  now, move the remaining data to the front of the
             *  io buffer
             */
            if ( iobuf_len > pDataLen ) {
                memmove( iobuf, &iobuf[pDataLen], iobuf_len - pDataLen );
                iobuf_len -= pDataLen;
                /* 
                 * Now assign the message length of new request if the
                 * remaining length is greater than header size.
                 */
                if ( iobuf_len >= sizeof( cstub_start_req_t ) )
                    pDataLen = pRec->cs_msgLength;
            } else {
                iobuf_len = 0;
            }
        }
        if ( pDataLen > buf_size ) {
            /* Align to 1024 bytes */
            buf_size = (pDataLen % 1024) ? (pDataLen + 1024 - (pDataLen % 1024))
                                         : pDataLen;
            /* reallocate the buffer. */
            iobuf = realloc( iobuf, buf_size );
            pRec = (cstub_start_req_t *)iobuf;
        }
    }
    free(iobuf);

    /*
     *  EOF/error occurred; the client closed us (or went away),
     *  so we go away also
     */
    if ( verbose ) {
        fprintf( stderr, "pid %d - disconnected (exiting)\n",
                getpid() );
    }

    /* Linux seems to be running into problems with the recycling of fds
     * Close the fd ourselves instead of expecting the system to close it
     * for us.
    */
    close(connfd);
}

/*
 *  do the accept loop
 */
int
do_server(void)
{
    int                 connfd;
    int                 clilen;
    int                 childpid;
#if defined(USE_CONNLD)
    struct pollfd	pfd[2];
    struct strrecvfd	recvfd;
#else
    struct  sockaddr_un cliaddr;
#endif
    fd_set              rfds;
    int                 nready;
    int                 max_fd = STDIN_FILENO;

    /*
     * set up our select args
     * NOTE: the purpose of using multiplexing I/O (poll or, now, select)
     * is to detect when the daemon goes away so we can die ourselves
     * and not leave a bunch of orphan stub processes hanging around.
     * Porting this is fine, but do not disable this code unless you
     * have an alternative means to achieve the same goal
     */
    if ( listen_fd > max_fd )
        max_fd = listen_fd;
#if defined(USE_CONNLD)
    memset(pfd, 0, 2 * sizeof(struct pollfd));
#else
    FD_ZERO( &rfds );
#endif

    /* we're all set; now, to complete the application protocol, we
     * send back an 'ok' indication to the parent letting them know
     * that our exec/bind sequence worked
     */
    { 
        char  okmsg[] = CSTUB_READY_MESSAGE;
        if ( write( STDIN_FILENO, okmsg, sizeof(CSTUB_READY_MESSAGE) ) == -1 ) {
            perror("write (okmsg)");
        }
    }
    
    /*
     *  the main accept() loop; this is where the listener waits,
     *  starting a new process at each connection indication received
     */
    for (;;) {
#if defined(USE_CONNLD)
        pfd[0].fd = STDIN_FILENO;
        pfd[0].events = POLLIN;
        pfd[1].fd = listen_fd;
        pfd[1].events = POLLIN;
        if (poll(pfd, 2, -1) < 0) {
            char    msg[200];
            if ( errno == EINTR ) {
                continue;
            }
            sprintf(msg, "Cgistub listener pid %d : poll error", getpid() );
            perror( msg );
            return -1;
        }
#else
        FD_SET( STDIN_FILENO, &rfds );
        FD_SET( listen_fd, &rfds );
        if ( (nready = select( max_fd + 1, &rfds, NULL, NULL, NULL ) < 0 )) {
            char    msg[200];
            if ( errno == EINTR ) {
                continue;
            }
            sprintf(msg, "Cgistub listener pid %d : select error", getpid() );
            perror( msg );
            return -1;
        }
#endif
#if defined(USE_CONNLD)
        if (pfd[0].revents & POLLIN)
#else
        if ( FD_ISSET( STDIN_FILENO, &rfds ))
#endif
	{
            if ( verbose ) {
                fprintf( stderr, "Cgistub pid %d: got input on stdin (bad news)\n",
                         getpid() );
                {
                    char buf[1000];
                    int n;
                    n = read(STDIN_FILENO, buf, 1000);
                    fprintf(stderr, "read %d bytes\n", n);
                    fprintf(stderr, "read \"%s\"\n", buf);
                }
            }
            /* bad news */
            return -1;
        }
        /* note that stdin-fd doesn't fall through so this is unexpected */
#if defined(USE_CONNLD)
        if ( !(pfd[1].revents & POLLIN) )
#else
        if ( ! FD_ISSET( listen_fd, &rfds ))
#endif
	{
            fprintf( stderr, "Cgistub pid %d: got invalid return from select\n",
                     getpid() );
            return -1;
        }

        /* 
         *  a new connect request; this indicates the daemon wants us
         *  to start a new childstub process
         */
#if defined(USE_CONNLD)
	if (ioctl(listen_fd, I_RECVFD, &recvfd) < 0) {
            if (errno == EINTR) 
                continue;
            perror("ioctl I_RECVFD accept");
            return -1;
	}
	connfd = recvfd.fd;
#else
        clilen = sizeof( cliaddr );
        if (( connfd = accept( listen_fd, (SA*)&cliaddr, &clilen)) < 0 ) {
            if ( errno == EINTR) 
                continue;
            perror("accept");
            return -1;
        }
#endif
        if ( (childpid = fork()) == 0 ) {
            close( listen_fd );
            close( STDIN_FILENO );
            wait_for_request( connfd );
            return 0;
        } else if ( childpid == -1 ) {
            perror("fork");
            return -1;
        }
        close( connfd );        
    }
}

/*
 *  clean up our environment; close all fds we might have potentially
 *  inherited
 */
void
cleanup_environment(void)
{
    int closed = 0;

#if defined(SOLARIS)
    void *handle = dlopen("libc.so", RTLD_LAZY);
    void (*closefromptr)(int) = NULL;
    if (handle) {
        closefromptr = (void (*)(int))dlsym(handle,"closefrom");
        if (closefromptr) {
            (*closefromptr)(STDERR_FILENO+1);
            closed = 1;
        }
        dlclose(handle);
        handle= NULL;
    }
#endif

  if (!closed) {
    int     fd;
    int     fdlimit;

    /*
     *  first, close all our potentially nasty inherited fds
     *  leave stdin, stdout & ..err
     */
    fdlimit = sysconf( _SC_OPEN_MAX );
    for ( fd = STDERR_FILENO+1; fd < fdlimit; fd++ ) {
        close( fd );
    }
  
  }
    SetupCgiStubSignalDisposition();    
}

/*
 *  main - get args, crank it up
 */
int
main( int argc, char * argv[] )
{
    extern  char *optarg;
    int     errflg = 0;
    int     c;

#undef DEBUG_LOG
#ifdef DEBUG_LOG
    FILE *fp;
    close(2);
    fp = fopen("/tmp/cgistub.log", "w+");
    dup(fp->__fileL);
    *stderr = *fp;

    fprintf(stderr, "<<<<<< ERRROR LOG >>>>>>\n");
    verbose = 1;
#endif

    /* 
     *  parse args
     */
    myName = argv[0];

    while ( (c = getopt(argc, argv, "f:s:lv")) != EOF) 
    switch (c) {
    case 'f':
        fileName = strdup( optarg );
        break;
    case 's':
        set_parameter(argv[0], optarg);
        errflg++;
        break;
    case 'l':
        fprintf(stderr, "%s", parameters + strlen(parameters) + 1);
        exit(0);
    case 'v':
        verbose++;
        break;
    case '?':
        errflg++;
    }
    if ( errflg || !fileName ) {
        fprintf(stderr, "usage: %s -l\n"
                        "       %s -s \"parameter value\"\n"
                        "where: -l lists all parameters\n"
                        "       -s sets parameter value\n",
                        argv[0], argv[0]);
        return (1);
    }

    /* 
     * make the primordial CGI stub a process group leader 
     * this will allow us to later kill all child CGI stubs,
     * as well as all the CGI processes spawned by them that
     * remain in the same process group
     */
    setsid();

    /* make sure the CGIs that stay in the same process group get killed when the CGI stub ends */
    atexit(killcgis);

    if ( verbose ) {
        fprintf(stderr, "%s - version (%s)\n", argv[0], VERSION );
    }

    parse_parameters();

    cleanup_environment();

    /*
     *  do socket creation 
     */
    listen_fd = create_bound_socket( fileName );
    if ( verbose ) {
        fprintf(stderr, "created socket fd %d named %s\n",
                listen_fd, fileName );
    }

    /*
     *  wait for requests
     */
    if ( do_server() == -1 ) {
        /* parent exploded? */
        unlink(fileName);
        return -1;
    }

    return 0;
}

/* Setup signal dispostion for the Cgistub process */
static void 
SetupCgiStubSignalDisposition()
{
    struct  sigaction sa;
    /*
     *  disable zombies; we don't really care to collect exit
     *  statii on our children
     */
#ifdef AIX
    signal(SIGCHLD, SIG_IGN);
#else
#if defined(LINUX)
    sa.sa_flags   = SA_NOCLDSTOP|SA_RESTART; /* probably redundant */
    sa.sa_handler = SIG_IGN;
#else
    sa.sa_flags   = SA_NOCLDWAIT|SA_NOCLDSTOP|SA_RESTART;
    sa.sa_handler = SIG_DFL;
#endif
    sigemptyset( &sa.sa_mask );
    if ( sigaction( SIGCHLD, &sa, NULL ) == -1 ) {
        perror("sigaction [SIGCHLD]");
    }
#endif


    /*
     * chances are good that we were forked from a process which
     * has all signals (like SIGCHLD, SIGTERM and SIGHUP) blocked.
     * ensure they are unblocked.
     */
    memset(&sa, 0, sizeof(sa));
    sigemptyset( &sa.sa_mask );
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGCHLD);
    if ( sigprocmask( SIG_UNBLOCK, &sa.sa_mask, NULL ) == -1 ) {
        perror("sigprocmask");
    }
}

/* Ths Cgistub process messed around with SIGCHLD.
 * This can cause fork/exec etc in the cgi to fail.
 * Worse is the case when the cgis are perl/shell scripts doing exec
 * We should reset signals to default.
 */
static void
SetupCgiSignalDisposition()
{
    struct  sigaction sa;
#ifdef AIX
    signal(SIGCHLD, SIG_DFL);
#else 
    sa.sa_flags   = 0;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
#endif
}
