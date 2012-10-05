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
 * Simple program for managing the .htpasswd file for .htaccess
 * 
 * based on original htpasswd.c by Rob McCool
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#if !defined (XP_WIN32)
#include <crypt.h>
#include <unistd.h>
#else
#include <conio.h>
#endif

#include <time.h>

#define LF 10
#define CR 13

#define MAX_STRING_LEN 256

/* forward declares */
static void copy_file(FILE * source, FILE * dest);
int INTgetline(char *s, int n, FILE * f);
void putline(FILE * f, char *l);
int to64(register char *s, register long v, int n);
char *getpassword(const char *prompt);
void set_password(char *user, char *password, FILE * f);

#if defined (XP_WIN32)
char *getpass(const char *prompt);
char *crypt(const char *, const char *);
#endif

/* global variables */
char *tfile; /* temporary file used when modifying the password file */

/* Copy from one file to  another.  */
static void copy_file(FILE * source, FILE * dest)
{
    static char line[MAX_STRING_LEN];

    while (fgets(line, sizeof(line), source) != NULL)
        fputs(line, dest);
}

/* Grab one line from a file */
int INTgetline(char *s, int n, FILE * f)
{
    register int i = 0;

    while (1) {
        s[i] = (char) fgetc(f);

        if (s[i] == CR)
            s[i] = fgetc(f);

        if ((s[i] == 0x4) || (s[i] == LF) || (i == (n - 1))) {
            s[i] = '\0';
            return (feof(f) ? 1 : 0);
        }
        ++i;
    }
}

void putline(FILE * f, char *l)
{
    int x;

    for (x = 0; l[x]; x++)
        fputc(l[x], f);
    fputc('\n', f);
}

/* From local_passwd.c (C) Regents of Univ. of California blah blah */
static unsigned char itoa64[] =    /* 0 ... 63 => ascii - 64 */
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int to64(register char *s, register long v, int n)
{
    while (--n >= 0) {
        *s++ = itoa64[v & 0x3f];
        v >>= 6;
    }
    return (0);
}

/* Show a prompt and securely get the new password from the user */
char *getpassword(const char *prompt)
{
    char *passwd = NULL;
    char *tmp = NULL;

    while (!tmp && prompt) {
#ifdef SOLARIS
        tmp = getpassphrase((char *) prompt);
#else
        tmp = getpass((char *) prompt);
#endif
    }
    passwd = (char *) strdup(tmp);
    memset(tmp, 0, strlen(tmp));

    return passwd;
}

/* Generate the salt and write the new password */
void set_password(char *user, char *password, FILE * f)
{
    char *pw = NULL;
    char *cpw, salt[3];

    if (strcmp(password, "") == 0) {
        pw = getpassword("New password:");
        if (strcmp(pw, getpassword("Re-type new password:"))) {
            fprintf(stderr, "They don't match, sorry.\n");
            if (tfile)
                unlink(tfile);
            exit(1);
        }
    } else {
        pw = strdup(password);
    }
    (void) srand((int) time((time_t *) NULL));
    to64(&salt[0], rand(), 2);
    cpw = crypt(pw, salt);
    free(pw);
    fprintf(f, "%s:%s\n", user, cpw);
}

void usage()
{
    fprintf(stderr,
            "Usage: htpasswd [-c] passwordfile username [password]\n");
    fprintf(stderr, "    -c    Create a new password file.\n");

    exit(1);
}

void interrupted()
{
    fprintf(stderr, "Interrupted.\n");
    if (tfile)
        unlink(tfile);
    exit(1);
}

int main(int argc, char *argv[])
{
    FILE *tfp, *f;
    char user[MAX_STRING_LEN];
    char filename[MAX_STRING_LEN];
    char password[MAX_STRING_LEN] = "";
    char line[MAX_STRING_LEN];
    char l[MAX_STRING_LEN];
    char *colon, *arg;
    int found, i;
    int createfile = 0;

    tfile = NULL;

    signal(SIGINT, (void (*)(int)) interrupted);

    /* Parse the arguments. This is a bit much to handle a single argument
     * but it provides a nice starting point in case this gets enhanced 
     * later */
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        if (*arg != '-')
            break;
        while (*++arg != '\0') {
            if (*arg == 'c')
                createfile = 1;
            else
                usage();
        }
    }

    /* There may be a password on the command-line too, handle that */
    if ((argc - i) < 2)
        usage();

    strncpy(filename, argv[i], MAX_STRING_LEN);
    strncpy(user, argv[i + 1], MAX_STRING_LEN);

    if ((argc - i) == 3)
        strncpy(password, argv[i + 2], MAX_STRING_LEN);

    if (createfile) {
        if (!(tfp = fopen(filename, "w"))) {
            fprintf(stderr, "Could not open password file %s.\n",
                    filename);
            fprintf(stderr, "Use -c option to create new one.\n");
            perror("fopen");
            exit(1);
        }

        printf("Adding password for %s.\n", user);
        set_password(user, password, tfp);
        fclose(tfp);
        exit(0);
    }

    tfile = tmpnam(NULL);
    if (!(tfp = fopen(tfile, "w"))) {
        fprintf(stderr, "Unable to create temporary file %s.\n", tfile);
        exit(1);
    }

    if (!(f = fopen(filename, "r"))) {
        fprintf(stderr,
                "Could not open password file %s for reading.\n",
                filename);
        fprintf(stderr, "Use -c option to create new one.\n");
        exit(1);
    }

    found = 0;
    while (!(INTgetline(line, MAX_STRING_LEN, f))) {
        if (found || (line[0] == '#') || (!line[0])) {
            putline(tfp, line);
            continue;
        }

        /* See if this is the user from the command-line */
        strncpy(l, line, MAX_STRING_LEN);
        colon = strchr(l, ':');
        if (colon != NULL)
            *colon = '\0';

        if (strcmp(user, l)) {
            putline(tfp, line);
            continue;
        } else {
            printf("Changing password for user %s.\n", user);
            set_password(user, password, tfp);
            found = 1;
        }
    }

    if (!found) {
        printf("Adding user %s.\n", user);
        set_password(user, password, tfp);
    }

    fclose(f);
    fclose(tfp);

    /* now re-open the files and copy them */
    if (!(f = fopen(filename, "w"))) {
        fprintf(stderr,
                "Could not open password file %s for writing.\n",
                filename);
        exit(1);
    }
    if (!(tfp = fopen(tfile, "r"))) {
        fprintf(stderr,
                "Could not re-open temp file %s for reading.\n", tfile);
        exit(1);
    }
    copy_file(tfp, f);
    fclose(f);
    fclose(tfp);
    unlink(tfile);
    return (0);
}

#if defined(XP_WIN32)
/*
 * Windows lacks getpass().
 */

static char *getpass(const char *prompt)
{
    static char password[MAX_STRING_LEN];
    int n = 0;

    fputs(prompt, stderr);
   
    while ((password[n] = _getch()) != '\r') {
        if (password[n] >= ' ' && password[n] <= '~') {
            n++;
            printf("*");
        }
        else {
            printf("\n");
            fputs(prompt, stderr);
            n = 0;
        }
    }
    password[n] = '\0';
    printf("\n");

    if (n > (MAX_STRING_LEN - 1)) {
        password[MAX_STRING_LEN - 1] = '\0';
    }

    return (char *) &password;
}
#endif
