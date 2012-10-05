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
 * util.c: string utility things, and other utilities
 * 
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 */


#include "httpd.h"
#include <base/util.h>


/* Match = 0, NoMatch = 1, Abort = -1 */
/* Based loosely on sections of wildmat.c by Rich Salz */
int htaccess_strcmp_match(char *str, char *exp) {
    int x,y;

    for(x=0,y=0;exp[y];++y,++x) {
        if((!str[x]) && (exp[y] != '*'))
            return -1;
        if(exp[y] == '*') {
            while(exp[++y] == '*');
            if(!exp[y])
                return 0;
            while(str[x]) {
                int ret;
                if((ret = htaccess_strcmp_match(&str[x++],&exp[y])) != 1)
                    return ret;
            }
            return -1;
        } else 
            if((exp[y] != '?') && (str[x] != exp[y]))
                return 1;
    }
    return (str[x] != '\0');
}

int htaccess_is_matchexp(char *str) {
    register int x;

    for(x=0;str[x];x++)
        if((str[x] == '*') || (str[x] == '?'))
            return 1;
    return 0;
}


void htaccess_no2slash(char *name) {
    register int x,y;

    for(x=0; name[x]; x++)
        if(x && (name[x-1] == '/') && (name[x] == '/'))
            for(y=x+1;name[y-1];y++)
                name[y-1] = name[y];
}

int htaccess_count_dirs(char *path) {
    register int x,n;

    for(x=0,n=0;path[x];x++)
        if(path[x] == '/') n++;
    return n;
}


void htaccess_strcpy_dir(char *d, char *s) {
   register int x;

   for(x=0;s[x];x++)
       d[x] = s[x];

   if(s[x-1] != '/') d[x++] = '/';
   d[x] = '\0';
}

/*
 * My version of strncpy. This will null terminate d if
 * s is n characters or longer. It also will only copy
 * n - 1 characters to d. SSG 4/13/95
 */
void htaccess_lim_strcpy(char *d, char *s, int n) 
{
    while (--n && (*d++ = *s++))
     ;

    if (!n) 
      *d = '\0';
}

void htaccess_getword(char *word, char *line, char stop) {
    int x = 0,y;

    for(x=0;((line[x]) && (line[x] != stop));x++)
        word[x] = line[x];

    word[x] = '\0';
    if(line[x]) ++x;
    y=0;

    while( (line[y++] = line[x++]) );
}

void htaccess_cfg_getword(char *word, char *line) {
    int x=0,y;
    
    for(x=0;line[x] && isspace(line[x]);x++);
    y=0;
    while(1) {
        if(!(word[y] = line[x]))
            break;
        if(isspace(line[x]))
            if((!x) || (line[x-1] != '\\'))
                break;
        if(line[x] != '\\') ++y;
        ++x;
    }
    word[y] = '\0';
    while(line[x] && isspace(line[x])) ++x;
    for(y=0; (line[y] = line[x]); ++x,++y);
}

int htaccess_eat_ws (FILE* fp)
{
    int ch;

    while ((ch = fgetc (fp)) != EOF) {
        if (ch != ' ' && ch != '\t')
            return ch;
    }
    return ch;
}


int htaccess_cfg_getline (char* s, int n, filebuffer *fp)
{
    int rv = util_getline(fp, 0, n, s);
    char *t;
    if(rv < 0)
        return rv;
    if (rv == 1) {
      /* EOF */
      if (*s == '\0')
        return 1;
    }
  
    /* Get rid of the whitespace */
    for(t = s; isspace(*t); ++t)
        /* nothing */;
    if(t != s) {
        while(*t)
            *s++ = *t++;
        *s = '\0';
    }
    return 0;
}

void htaccess_make_dirstr(char *s, int n, char *d) {
    register int x,f;

    for(x=0,f=0;s[x];x++) {
        if((d[x] = s[x]) == '/')
            if((++f) == n) {
                d[x] = '\0';
                return;
            }
    }
    d[x] = '\0';
}


void htaccess_make_full_path(char *src1,char *src2,char *dst) {
    register int x,y;

    for(x=0; (dst[x] = src1[x]); x++);

    if(!x) dst[x++] = '/';
    else if((dst[x-1] != '/'))
        dst[x++] = '/';

    for(y=0; (dst[x] = src2[y]); x++,y++);
    dst[x] = '\0';

}


/* aaaack but it's fast and const should make it shared text page. */
static const int pr2six[256]={
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,0,1,2,3,4,5,6,7,8,9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,64,26,27,
    28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64
};

void ncsa_uudecode(char *bufcoded, unsigned char *bufplain, int outbufsize) {
    int nbytesdecoded;
    register char *bufin = bufcoded;
    register unsigned char *bufout = bufplain;
    register int nprbytes;
    
    /* Strip leading whitespace. */
    
    while(*bufcoded==' ' || *bufcoded == '\t') bufcoded++;
    
    /* Figure out how many characters are in the input buffer.
     * If this would decode into more bytes than would fit into
     * the output buffer, adjust the number of input bytes downwards.
     */
    bufin = bufcoded;
    while(pr2six[*(bufin++)] <= 63);
    nprbytes = bufin - bufcoded - 1;
    nbytesdecoded = ((nprbytes+3)/4) * 3;
    if(nbytesdecoded > outbufsize) {
        nprbytes = (outbufsize*4)/3;
    }
    
    bufin = bufcoded;
    
    while (nprbytes > 0) {
        *(bufout++) = 
            (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        *(bufout++) = 
            (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        *(bufout++) = 
            (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }
    
    if(nprbytes & 03) {
        if(pr2six[bufin[-2]] > 63)
            nbytesdecoded -= 2;
        else
            nbytesdecoded -= 1;
    }
    bufplain[nbytesdecoded] = '\0';
}

/* sorta checks for the pattern [\d.]+
 * basically it just forces the string to consist of dots or digits */
int htaccess_is_ip(const char *host)
{
    while ((*host == '.') || isdigit(*host))
        host++;
    return (*host == '\0');
}
