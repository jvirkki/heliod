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
 * shexp.c: shell-like wildcard match routines
 *
 *
 * See shexp.h for public documentation.
 *
 * Rob McCool
 * 
 */

#include <ctype.h>    /* isalpha, tolower */

#include "base/shexp.h"
#include "base/dbtbase.h"
#include "NsprWrap/NsprError.h"

/*
 * The observant engineer will notice 2 distinct sets of functions here.
 * All of the noicmp flavor of functions do case sensitive compares on all
 * platforms.  The other set (public set) does case insensitive compares on NT.
 */
int _shexp_match_noicmp(const char *str, const char *exp) ;


/* ----------------------------- shexp_valid ------------------------------ */


int valid_subexp(const char *exp, char stop) 
{
    register int x,y,t;
    int nsc,np,tld;

    x=0;nsc=0;tld=0;

    while(exp[x] && (exp[x] != stop)) {
        switch(exp[x]) {
          case '~':
            if(tld) return INVALID_SXP;
            else ++tld;
          case '*':
          case '?':
          case '^':
          case '$':
            ++nsc;
            break;
          case '[':
            ++nsc;
            if((!exp[++x]) || (exp[x] == ']'))
                return INVALID_SXP;
            for(++x;exp[x] && (exp[x] != ']');++x)
                if(exp[x] == '\\')
                    if(!exp[++x])
                        return INVALID_SXP;
            if(!exp[x])
                return INVALID_SXP;
            break;
          case '(':
            ++nsc;
            while(1) {
                if(exp[++x] == ')')
                    return INVALID_SXP;
                for(y=x;(exp[y]) && (exp[y] != '|') && (exp[y] != ')');++y)
                    if(exp[y] == '\\')
                        if(!exp[++y])
                            return INVALID_SXP;
                if(!exp[y])
                    return INVALID_SXP;
                t = valid_subexp(&exp[x],exp[y]);
                if(t == INVALID_SXP)
                    return INVALID_SXP;
                x+=t;
                if(exp[x] == ')') {
                    break;
                }
            }
            break;
          case ')':
          case ']':
            return INVALID_SXP;
          case '\\':
            if(!exp[++x])
                return INVALID_SXP;
          default:
            break;
        }
        ++x;
    }
    if((!stop) && (!nsc))
        return NON_SXP;
    return ((exp[x] == stop) ? x : INVALID_SXP);
}

NSAPI_PUBLIC int shexp_valid(const char *exp) {
    int x;

    x = valid_subexp(exp, '\0');
    if (x < 0) {
        if (x == INVALID_SXP) {
            NsprError::setError(PR_INVALID_ARGUMENT_ERROR,
                                XP_GetAdminStr(DBT_invalidshexp));
        }
        return x;
    }
    return VALID_SXP;
}


/* ----------------------------- shexp_match ----------------------------- */


#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

int _shexp_match(const char *str, const char *exp);

int handle_union(const char *str, const char *exp) 
{
    char *e2 = (char *) MALLOC(sizeof(char)*strlen(exp));
    register int t,p2,p1 = 1;
    int cp;

    while(1) {
        for(cp=1;exp[cp] != ')';cp++)
            if(exp[cp] == '\\')
                ++cp;
        for(p2 = 0;(exp[p1] != '|') && (p1 != cp);p1++,p2++) {
            if(exp[p1] == '\\')
                e2[p2++] = exp[p1++];
            e2[p2] = exp[p1];
        }
        for(t=cp+1;(e2[p2] = exp[t]);++t,++p2);
        if(_shexp_match(str,e2) == MATCH) {
            FREE(e2);
            return MATCH;
        }
        if(p1 == cp) {
            FREE(e2);
            return NOMATCH;
        }
        else ++p1;
    }
}

int handle_union_noicmp(const char *str, const char *exp) 
{
    char *e2 = (char *) MALLOC(sizeof(char)*strlen(exp));
    register int t,p2,p1 = 1;
    int cp;

    while(1) {
        for(cp=1;exp[cp] != ')';cp++)
            if(exp[cp] == '\\')
                ++cp;
        for(p2 = 0;(exp[p1] != '|') && (p1 != cp);p1++,p2++) {
            if(exp[p1] == '\\')
                e2[p2++] = exp[p1++];
            e2[p2] = exp[p1];
        }
        for(t=cp+1;(e2[p2] = exp[t]);++t,++p2);
        if(_shexp_match_noicmp(str,e2) == MATCH) {
            FREE(e2);
            return MATCH;
        }
        if(p1 == cp) {
            FREE(e2);
            return NOMATCH;
        }
        else ++p1;
    }
}

int _shexp_match(const char *str, const char *exp) 
{
    register int x,y;
    int ret,neg;

    ret = 0;
    for(x=0,y=0;exp[y];++y,++x) {
        if((!str[x]) && (exp[y] != '(') && (exp[y] != '$') && (exp[y] != '*'))
            ret = ABORTED;
        else {
            switch(exp[y]) {
              case '$':
                if( (str[x]) )
                    ret = NOMATCH;
                else
                    --x;             /* we don't want loop to increment x */
                break;
              case '*':
                while(exp[++y] == '*');
                if(!exp[y])
                    return MATCH;
                while(str[x]) {
                    switch(_shexp_match(&str[x++],&exp[y])) {
                    case NOMATCH:
                        continue;
                    case ABORTED:
                        ret = ABORTED;
                        break;
                    default:
                        return MATCH;
                    }
                    break;
                }
                if((exp[y] == '$') && (exp[y+1] == '\0') && (!str[x]))
                    return MATCH;
                else
                    ret = ABORTED;
                break;
              case '[':
                if((neg = ((exp[++y] == '^') && (exp[y+1] != ']'))))
                    ++y;
                
                if((isalnum(exp[y])) && (exp[y+1] == '-') && 
                   (isalnum(exp[y+2])) && (exp[y+3] == ']'))
                    {
                        int start = exp[y], end = exp[y+2];
                        
                        /* Droolproofing for pinheads not included */
                        if(neg ^ ((str[x] < start) || (str[x] > end))) {
                            ret = NOMATCH;
                            break;
                        }
                        y+=3;
                    }
                else {
                    int matched;
                    
                    for(matched=0;exp[y] != ']';y++)
                        matched |= (str[x] == exp[y]);
                    if(neg ^ (!matched))
                        ret = NOMATCH;
                }
                break;
              case '(':
                return handle_union(&str[x],&exp[y]);
                break;
              case '?':
                break;
              case '\\':
                ++y;
              default:
#ifdef XP_UNIX
                if(str[x] != exp[y])
#else /* XP_WIN32 */
                if(strnicmp(str + x, exp + y, 1))
#endif /* XP_WIN32 */
                    ret = NOMATCH;
                break;
            }
        }
        if(ret)
            break;
    }
    return (ret ? ret : (str[x] ? NOMATCH : MATCH));
}

int _shexp_match_noicmp(const char *str, const char *exp) 
{
    register int x,y;
    int ret,neg;

    ret = 0;
    for(x=0,y=0;exp[y];++y,++x) {
        if((!str[x]) && (exp[y] != '(') && (exp[y] != '$') && (exp[y] != '*'))
            ret = ABORTED;
        else {
            switch(exp[y]) {
              case '$':
                if( (str[x]) )
                    ret = NOMATCH;
                else
                    --x;             /* we don't want loop to increment x */
                break;
              case '*':
                while(exp[++y] == '*');
                if(!exp[y])
                    return MATCH;
                while(str[x]) {
                    switch(_shexp_match_noicmp(&str[x++],&exp[y])) {
                    case NOMATCH:
                        continue;
                    case ABORTED:
                        ret = ABORTED;
                        break;
                    default:
                        return MATCH;
                    }
                    break;
                }
                if((exp[y] == '$') && (exp[y+1] == '\0') && (!str[x]))
                    return MATCH;
                else
                    ret = ABORTED;
                break;
              case '[':
                if((neg = ((exp[++y] == '^') && (exp[y+1] != ']'))))
                    ++y;
                
                if((isalnum(exp[y])) && (exp[y+1] == '-') && 
                   (isalnum(exp[y+2])) && (exp[y+3] == ']'))
                    {
                        int start = exp[y], end = exp[y+2];
                        
                        /* Droolproofing for pinheads not included */
                        if(neg ^ ((str[x] < start) || (str[x] > end))) {
                            ret = NOMATCH;
                            break;
                        }
                        y+=3;
                    }
                else {
                    int matched;
                    
                    for(matched=0;exp[y] != ']';y++)
                        matched |= (str[x] == exp[y]);
                    if(neg ^ (!matched))
                        ret = NOMATCH;
                }
                break;
              case '(':
                return handle_union_noicmp(&str[x],&exp[y]);
                break;
              case '?':
                break;
              case '\\':
                ++y;
              default:
                if(str[x] != exp[y])
                    ret = NOMATCH;
                break;
            }
        }
        if(ret)
            break;
    }
    return (ret ? ret : (str[x] ? NOMATCH : MATCH));
}

NSAPI_PUBLIC int shexp_match(const char *str, const char *exp)
{
    register int x;
    char *expbase = NULL;

    for(x=strlen(exp)-1;x;--x) {
        if((exp[x] == '~') && (exp[x-1] != '\\')) {
            /* we're done if the negative subexp matches */
            if(_shexp_match(str,&exp[x+1]) == MATCH)
                return 1;
            /* we're done if the only thing in front of the subexp is '*' */
            if (x == 1 && exp[0] == '*')
                return 0;
            /* create a copy so we can strip off the subexp */
            expbase = STRDUP(exp);
            expbase[x] = '\0';
            exp = expbase;
            break;
        }
    }
    if(_shexp_match(str,exp) == MATCH) {
        if (expbase)
            FREE(expbase);
        return 0;
    }

    if (expbase)
        FREE(expbase);
    return 1;
}

NSAPI_PUBLIC int shexp_match_noicmp(const char *str, const char *exp)
{
    register int x;
    char *expbase = NULL;

    for(x=strlen(exp)-1;x;--x) {
        if((exp[x] == '~') && (exp[x-1] != '\\')) {
            /* we're done if the negative subexp matches */
            if(_shexp_match_noicmp(str,&exp[x+1]) == MATCH)
                return 1;
            /* we're done if the only thing in front of the subexp is '*' */
            if (x == 1 && exp[0] == '*')
                return 0;
            /* create a copy so we can strip off the subexp */
            expbase = STRDUP(exp);
            expbase[x] = '\0';
            exp = expbase;
            break;
        }
    }
    if(_shexp_match_noicmp(str,exp) == MATCH) {
        if (expbase)
            FREE(expbase);
        return 0;
    }

    if (expbase)
        FREE(expbase);
    return 1;
}

/* ------------------------------ shexp_cmp ------------------------------- */


NSAPI_PUBLIC int shexp_cmp(const char *str, const char *exp)
{
    switch(shexp_valid(exp)) {
      case INVALID_SXP:
        return -1;
      case NON_SXP:
#ifdef XP_UNIX
        return (strcmp(exp,str) ? 1 : 0);
#else  /* XP_WIN32 */
        return (stricmp(exp,str) ? 1 : 0);
#endif /* XP_WIN32 */
      default:
        return shexp_match(str, exp);
    }
}

/* ------------------------------ shexp_cmp ------------------------------- */

NSAPI_PUBLIC int shexp_noicmp(const char *str, const char *exp)
{
    switch(shexp_valid(exp)) {
      case INVALID_SXP:
        return -1;
      case NON_SXP:
        return (strcmp(exp,str) ? 1 : 0);
      default:
        return shexp_match_noicmp(str, exp);
    }
}

/* ---------------------------- shexp_casecmp ----------------------------- */


NSAPI_PUBLIC int shexp_casecmp(const char *str, const char *exp)
{
    char *lstr = STRDUP(str), *lexp = STRDUP(exp), *t;
    int ret;

    for(t = lstr; *t; t++)
        if(isalpha(*t)) *t = tolower(*t);
    for(t = lexp; *t; t++)
        if(isalpha(*t)) *t = tolower(*t);

    switch(shexp_valid(lexp)) {
      case INVALID_SXP:
        ret = -1;
        break;
      case NON_SXP:
        ret = (strcmp(lexp, lstr) ? 1 : 0);
        break;
      default:
        ret = shexp_match(lstr, lexp);
    }
    FREE(lstr);
    FREE(lexp);
    return ret;
}

