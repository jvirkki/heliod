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
* <RUSLAN>
*  I wrapped that into a class to prevent it from conflicting with NSAPI box one
* </RUSLAN>
*/

#include "nsshexp.h"

/* ----------------------------- shexp_valid ------------------------------ */


int
NSShellExp::valid_subexp (char *exp, char stop) 
{
    register int x,y,t;
    int nsc,np,tld;
    
    x=0;nsc=0;tld=0;
    
    while(exp[x] && (exp[x] != stop)) {
        switch(exp[x]) {
        case '~':
            if(tld) return 0;
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
                return 0;
            for(++x;exp[x] && (exp[x] != ']');++x)
                if(exp[x] == '\\')
                    if(!exp[++x])
                        return 0;
                    if(!exp[x])
                        return 0;
                    break;
        case '(':
            ++nsc;
            while(1) {
                if(exp[++x] == ')')
                    return 0;
                for(y=x;(exp[y]) && (exp[y] != '|') && (exp[y] != ')');++y)
                    if(exp[y] == '\\')
                        if(!exp[++y])
                            return 0;
                        if(!exp[y])
                            return 0;
                        t = valid_subexp(&exp[x],exp[y]);
                        if(t == 0)
                            return 0;
                        x+=t;
                        if(exp[x] == ')') {
                            break;
                        }
            }
            break;
        case ')':
        case ']':
            return 0;
        case '\\':
            if(!exp[++x])
                return 0;
        default:
            break;
        }
        ++x;
    }
    if((!stop) && (!nsc))
        return 2;
    
    return ((exp[x] == stop) ? x : 0);
}

int
NSShellExp::shexp_valid(char *exp) {
    int x;
    
    x = valid_subexp(exp, '\0');
    return (x <= 0 ? 0 : 1);
}


/* ----------------------------- shexp_match ----------------------------- */


#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

int
NSShellExp::handle_union(char *str, char *exp) 
{
    char *e2 = (char *) malloc (sizeof(char)*strlen(exp));
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
                free (e2);
                return MATCH;
            }
            if(p1 == cp) {
                free (e2);
                return NOMATCH;
            }
            else ++p1;
    }
}


int
NSShellExp::_shexp_match(char *str, char *exp) 
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

int
NSShellExp::shexp_match(char *str, char *xp)    {
    register int x;
    char *exp = strdup (xp);
    
    for(x=strlen(exp)-1;x;--x) {
        if((exp[x] == '~') && (exp[x-1] != '\\')) {
            exp[x] = '\0';
            if(_shexp_match(str,&exp[++x]) == MATCH)
                goto punt;
            break;
        }
    }
    if(_shexp_match(str,exp) == MATCH) {
        free (exp);
        return 0;
    }
    
punt:
    free (exp);
    return 1;
}


/* ------------------------------ shexp_cmp ------------------------------- */


int
NSShellExp::shexp_cmp(char *str, char *exp)
{
    switch(shexp_valid(exp)) {
    case 0:
        return -1;
    case 2:
#ifdef XP_UNIX
        return (strcmp(exp,str) ? 1 : 0);
#else  /* XP_WIN32 */
        return (stricmp(exp,str) ? 1 : 0);
#endif /* XP_WIN32 */
    default:
        return shexp_match(str, exp);
    }
}


/* ---------------------------- shexp_casecmp ----------------------------- */


int
NSShellExp::shexp_casecmp(char *str, char *exp)
{
    char *lstr = strdup (str), *lexp = strdup (exp), *t;
    int ret;
    
    for(t = lstr; *t; t++)
        if(isalpha(*t)) *t = tolower(*t);
        for(t = lexp; *t; t++)
            if(isalpha(*t)) *t = tolower(*t);
            
            switch(shexp_valid(lexp)) {
            case 0:
                ret = -1;
                break;
            case 2:
                ret = (strcmp(lexp, lstr) ? 1 : 0);
                break;
            default:
                ret = shexp_match(lstr, lexp);
            }
            free (lstr);
            free (lexp);
            return ret;
}
