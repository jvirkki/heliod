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
 * Copyright (c) 2005, 2008 Sun Microsystems, Inc. All Rights Reserved.
 * Use is subject to license terms.
 *
 *	Copyright (c) 1984 AT&T
 *	  All Rights Reserved  	
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0. 
 * 
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express 
 * or implied. 
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */

#include "libsed.h"
#include "sed.h"
#include "regexp.h"

char    *trans[040]  = {
    "\\01",
    "\\02",
    "\\03",
    "\\04",
    "\\05",
    "\\06",
    "\\07",
    "-<",
    "->",
    "\n",
    "\\13",
    "\\14",
    "\\15",
    "\\16",
    "\\17",
    "\\20",
    "\\21",
    "\\22",
    "\\23",
    "\\24",
    "\\25",
    "\\26",
    "\\27",
    "\\30",
    "\\31",
    "\\32",
    "\\33",
    "\\34",
    "\\35",
    "\\36",
    "\\37"
};
char rub[] = {"\\177"};

extern int sed_step(char *p1, char *p2, int circf, step_vars_storage *vars);
static int substitute(sed_eval_t *eval, sed_reptr_t *ipc,
                      step_vars_storage *step_vars);
static PRStatus execute(sed_eval_t *eval);
static int match(sed_eval_t *eval, char *expbuf, int gf,
                 step_vars_storage *step_vars);
static PRStatus dosub(sed_eval_t *eval, char *rhsbuf, int n,
                          step_vars_storage *step_vars);
static char *place(sed_eval_t *eval, char *asp, char *al1, char *al2);
static PRStatus command(sed_eval_t *eval, sed_reptr_t *ipc,
                            step_vars_storage *step_vars);
static void wline(sed_eval_t *eval, char *buf, int sz);
static void arout(sed_eval_t *eval);

static void eval_errf(sed_eval_t *eval, const char *fmt, ...)
{
    if (eval->errfn && eval->pool) {
        va_list args;
        va_start(args, fmt);
        eval->errfn(eval->data, fmt, args);
        va_end(args);
    }
}

#define INIT_BUF_SIZE 1024

/*
 * grow_buffer
 */
static void grow_buffer(pool_handle_t *pool, char **buffer,
                        char **spend, unsigned int *cursize,
                        unsigned int newsize)
{
    char* newbuffer = NULL;
    int spendsize = 0;
    if (*cursize >= newsize)
        return;
    /* Align it to 4 KB boundary */
    newsize = (newsize  + ((1 << 12) - 1)) & ~((1 << 12) -1);
    newbuffer = (char *)pool_calloc(pool, (size_t)1, (size_t)newsize);
    if (*spend && *buffer && (*cursize > 0)) {
        spendsize = *spend - *buffer;
    }
    if ((*cursize > 0) && *buffer) {
        memcpy(newbuffer, *buffer, *cursize);
    }
    *buffer = newbuffer;
    *cursize = newsize;
    if (spend != buffer) {
        *spend = *buffer + spendsize;
    }
}

/*
 * grow_line_buffer
 */
static void grow_line_buffer(sed_eval_t *eval, int newsize)
{
    grow_buffer(eval->pool, &eval->linebuf, &eval->lspend,
                &eval->lsize, newsize);
}

/*
 * grow_hold_buffer
 */
static void grow_hold_buffer(sed_eval_t *eval, int newsize)
{
    grow_buffer(eval->pool, &eval->holdbuf, &eval->hspend,
                &eval->hsize, newsize);
}

/*
 * grow_gen_buffer
 */
static void grow_gen_buffer(sed_eval_t *eval, int newsize,
                            char **gspend)
{
    if (gspend == NULL) {
        gspend = &eval->genbuf;
    }
    grow_buffer(eval->pool, &eval->genbuf, gspend,
                &eval->gsize, newsize);
    eval->lcomend = &eval->genbuf[71];
}

/*
 * appendmem_to_linebuf
 */
static void appendmem_to_linebuf(sed_eval_t *eval, const char* sz, int len)
{
    unsigned int reqsize = (eval->lspend - eval->linebuf) + len;
    if (eval->lsize < reqsize) {
        grow_line_buffer(eval, reqsize);
    }
    memcpy(eval->lspend, sz, len);
    eval->lspend += len;
}

/*
 * append_to_linebuf
 */
static void append_to_linebuf(sed_eval_t *eval, const char* sz)
{
    int len = strlen(sz);
    /* Copy string including null character */
    appendmem_to_linebuf(eval, sz, len + 1);
    --eval->lspend; /* lspend will now point to NULL character */
}

/*
 * copy_to_linebuf
 */
static void copy_to_linebuf(sed_eval_t *eval, const char* sz)
{
    eval->lspend = eval->linebuf;
    append_to_linebuf(eval, sz);
}

/*
 * append_to_holdbuf
 */
static void append_to_holdbuf(sed_eval_t *eval, const char* sz)
{
    int len = strlen(sz);
    unsigned int reqsize = (eval->hspend - eval->holdbuf) + len + 1;
    if (eval->hsize <= reqsize) {
        grow_hold_buffer(eval, reqsize);
    }
    strcpy(eval->hspend, sz);
    /* hspend will now point to NULL character */
    eval->hspend += len;
}

/*
 * copy_to_holdbuf
 */
static void copy_to_holdbuf(sed_eval_t *eval, const char* sz)
{
    eval->hspend = eval->holdbuf;
    append_to_holdbuf(eval, sz);
}

/*
 * append_to_genbuf
 */
static void append_to_genbuf(sed_eval_t *eval, const char* sz, char **gspend)
{
    int len = strlen(sz);
    unsigned int reqsize = (*gspend - eval->genbuf) + len + 1;
    if (eval->gsize < reqsize) {
        grow_gen_buffer(eval, reqsize, gspend);
    }
    strcpy(*gspend, sz);
    /* *gspend will now point to NULL character */
    *gspend += len;
}

/*
 * copy_to_genbuf
 */
static void copy_to_genbuf(sed_eval_t *eval, const char* sz)
{
    int len = strlen(sz);
    unsigned int reqsize = len + 1;
    if (eval->gsize < reqsize) {
        grow_gen_buffer(eval, reqsize, NULL);
    }
}

/*
 * sed_init_eval
 */
PRStatus sed_init_eval(sed_eval_t *eval, sed_commands_t *commands, sed_err_fn_t *errfn, void *data, pool_handle_t* p)
{
    memset(eval, 0, sizeof(*eval));
    eval->pool = p;
    return sed_reset_eval(eval, commands, errfn, data);
}

/*
 * sed_reset_eval
 */
PRStatus sed_reset_eval(sed_eval_t *eval, sed_commands_t *commands, sed_err_fn_t *errfn, void *data)
{
    int i;

    eval->errfn = errfn;
    eval->data = data;

    eval->commands = commands;

    eval->lnum = 0;
    eval->fout = NULL;

    if (eval->linebuf == NULL) {
        eval->lsize = INIT_BUF_SIZE;
        eval->linebuf = (char *)pool_calloc(eval->pool, 1, (size_t)eval->lsize);
    }
    if (eval->holdbuf == NULL) {
        eval->hsize = INIT_BUF_SIZE;
        eval->holdbuf = (char *)pool_calloc(eval->pool, 1, (size_t)eval->hsize);
    }
    if (eval->genbuf == NULL) {
        eval->gsize = INIT_BUF_SIZE;
        eval->genbuf = (char *)pool_calloc(eval->pool, 1, (size_t)eval->gsize);
    }
    eval->lspend = eval->linebuf;
    eval->hspend = eval->holdbuf;
    eval->lcomend = &eval->genbuf[71];

    for (i = 0; i < sizeof(eval->abuf) / sizeof(eval->abuf[0]); i++)
        eval->abuf[i] = NULL;
    eval->aptr = eval->abuf;
    eval->pending = NULL;
    eval->inar = (unsigned char *)pool_calloc(eval->pool, (size_t)commands->nrep, (size_t)sizeof(unsigned char));
    eval->nrep = commands->nrep;

    eval->dolflag = 0;
    eval->sflag = 0;
    eval->jflag = 0;
    eval->delflag = 0;
    eval->lreadyflag = 0;
    eval->quitflag = 0;
    eval->finalflag = 1; /* assume we're evaluating only one file/stream */
    eval->numpass = 0;
    eval->nullmatch = 0;
    eval->col = 0;

    for (i = 0; i < commands->nfiles; i++) {
        const char* filename = commands->fname[i];
        if ((eval->fcode[i] = PR_Open(filename, PR_WRONLY|PR_CREATE_FILE, 
                                       0644)) == NULL) {
            eval_errf(eval, XP_GetAdminStr(DBT_COMES), filename);
            return PR_FAILURE;
        }
    }

    return PR_SUCCESS;
}

/*
 * sed_destroy_eval
 */
void sed_destroy_eval(sed_eval_t *eval)
{
    int i;
    /* eval->linebuf, eval->holdbuf, eval->genbuf and eval->inar are allocated
     * on pool. It will be freed when pool will be freed */
    for (i = 0; i < eval->commands->nfiles; i++) {
        if (eval->fcode[i] != NULL) {
            PR_Close(eval->fcode[i]);
            eval->fcode[i] = NULL;
        }
    }
}

/*
 * sed_eval_file
 */
PRStatus sed_eval_file(sed_eval_t *eval, PRFileDesc *fin, void *fout)
{
    for (;;) {
        char buf[1024];
        PRInt32 read_bytes = 0;

        read_bytes = sizeof(buf);
        if (PR_Read(fin, (void *)buf, read_bytes) <= 0)
            break;

        if (sed_eval_buffer(eval, buf, read_bytes, fout) != PR_SUCCESS)
            return PR_FAILURE;

        if (eval->quitflag)
            return PR_SUCCESS;
    }

    return sed_finalize_eval(eval, fout);
}

/*
 * sed_eval_buffer
 */
PRStatus sed_eval_buffer(sed_eval_t *eval, const char *buf, int bufsz, void *fout)
{
    PRStatus rv;

    if (eval->quitflag)
        return PR_SUCCESS;

    eval->fout = fout;

    /* Process leftovers */
    if (bufsz && eval->lreadyflag) {
        eval->lreadyflag = 0;
        eval->lspend--;
        *eval->lspend = '\0';
        rv = execute(eval);
        if (rv != 0)
            return PR_FAILURE;
    }

    while (bufsz) {
        char *n;
        int llen;

        n = memchr(buf, '\n', bufsz);
        if (n == NULL)
            break;

        llen = n - buf;
        if (llen == bufsz - 1) {
            /* This might be the last line; delay its processing */
            eval->lreadyflag = 1;
            break;
        }
        
        appendmem_to_linebuf(eval, buf, llen + 1);
        --eval->lspend;
        /* replace new line character with NULL */
        *eval->lspend = '\0';
        buf += (llen + 1);
        bufsz -= (llen + 1);
        rv = execute(eval);
        if (rv != 0)
            return PR_FAILURE;
        if (eval->quitflag)
            break;
    }

    /* Save the leftovers for later */
    if (bufsz) {
        appendmem_to_linebuf(eval, buf, bufsz);
    }

    return PR_SUCCESS;
}

/*
 * sed_finalize_eval
 */
PRStatus sed_finalize_eval(sed_eval_t *eval, void *fout)
{
    if (eval->quitflag)
        return PR_SUCCESS;

    if (eval->finalflag)
        eval->dolflag = 1;

    eval->fout = fout;

    /* Process leftovers */
    if (eval->lspend > eval->linebuf) {
        PRStatus rv;

        if (eval->lreadyflag) {
            eval->lreadyflag = 0;
            eval->lspend--;
        } else {
            /* Code can probably reach here when last character in output
             * buffer is not a newline.
             */
            /* Assure space for NULL */
            append_to_linebuf(eval, "");
        }

        *eval->lspend = '\0';
        rv = execute(eval);
        if (rv != 0)
            return PR_FAILURE;
    }

    eval->quitflag = 1;

    return PR_SUCCESS;
}

/*
 * execute
 */
static PRStatus execute(sed_eval_t *eval)
{
    sed_reptr_t *ipc = eval->commands->ptrspace;
    step_vars_storage step_vars;

    eval->lnum++;

    eval->sflag = 0;

    if (eval->pending) {
        ipc = eval->pending;
        eval->pending = NULL;
    }

    memset(&step_vars, 0, sizeof(step_vars));

    while (ipc->command) {
        char *p1;
        char *p2;
        PRStatus rv;
        int c;

        p1 = ipc->ad1;
        p2 = ipc->ad2;

        if (p1) {

            if (eval->inar[ipc->nrep]) {
                if (*p2 == CEND) {
                    p1 = 0;
                } else if (*p2 == CLNUM) {
                    c = (unsigned char)p2[1];
                    if (eval->lnum > eval->commands->tlno[c]) {
                        eval->inar[ipc->nrep] = 0;
                        if (ipc->negfl)
                            goto yes;
                        ipc = ipc->next;
                        continue;
                    }
                    if (eval->lnum == eval->commands->tlno[c]) {
                        eval->inar[ipc->nrep] = 0;
                    }
                } else if (match(eval, p2, 0, &step_vars)) {
                    eval->inar[ipc->nrep] = 0;
                }
            } else if (*p1 == CEND) {
                if (!eval->dolflag) {
                    if (ipc->negfl)
                        goto yes;
                    ipc = ipc->next;
                    continue;
                }
            } else if (*p1 == CLNUM) {
                c = (unsigned char)p1[1];
                if (eval->lnum != eval->commands->tlno[c]) {
                    if (ipc->negfl)
                        goto yes;
                    ipc = ipc->next;
                    continue;
                }
                if (p2)
                    eval->inar[ipc->nrep] = 1;
            } else if (match(eval, p1, 0, &step_vars)) {
                if (p2)
                    eval->inar[ipc->nrep] = 1;
            } else {
                if (ipc->negfl)
                    goto yes;
                ipc = ipc->next;
                continue;
            }
        }

        if (ipc->negfl) {
            ipc = ipc->next;
            continue;
        }

yes:
        rv = command(eval, ipc, &step_vars);
        if (rv != PR_SUCCESS)
            return rv;

        if (eval->quitflag)
            return PR_SUCCESS;

        if (eval->pending)
            return PR_SUCCESS;

        if (eval->delflag)
            break;

        if (eval->jflag) {
            eval->jflag = 0;
            if ((ipc = ipc->lb1) == 0) {
                ipc = eval->commands->ptrspace;
                break;
            }
        } else
            ipc = ipc->next;
    }

    if (!eval->commands->nflag && !eval->delflag)
        wline(eval, eval->linebuf, eval->lspend - eval->linebuf);

    if (eval->aptr > eval->abuf)
        arout(eval);

    eval->delflag = 0;

    eval->lspend = eval->linebuf;

    return PR_SUCCESS;
}

/*
 * match
 */
static int match(sed_eval_t *eval, char *expbuf, int gf,
                 step_vars_storage *step_vars)
{
    char   *p1;
    int circf;

    if(gf) {
        if(*expbuf)    return(0);
        step_vars->locs = p1 = step_vars->loc2;
    } else {
        p1 = eval->linebuf;
        step_vars->locs = 0;
    }

    circf = *expbuf++;
    return(sed_step(p1, expbuf, circf, step_vars));
}

/*
 * substitute
 */
static int substitute(sed_eval_t *eval, sed_reptr_t *ipc,
                      step_vars_storage *step_vars)
{
    if(match(eval, ipc->re1, 0, step_vars) == 0)    return(0);

    eval->numpass = 0;
    eval->sflag = 0;        /* Flags if any substitution was made */
    if (dosub(eval, ipc->rhs, ipc->gfl, step_vars) != PR_SUCCESS)
        return -1;

    if(ipc->gfl) {
        while(*step_vars->loc2) {
            if(match(eval, ipc->re1, 1, step_vars) == 0) break;
            if (dosub(eval, ipc->rhs, ipc->gfl, step_vars) != PR_SUCCESS)
                return -1;
        }
    }
    return(eval->sflag);
}

/*
 * dosub
 */
static PRStatus dosub(sed_eval_t *eval, char *rhsbuf, int n,
                          step_vars_storage *step_vars)
{
    char *lp, *sp, *rp;
    int c;
    PRStatus rv = PR_SUCCESS;

    if(n > 0 && n < 999) {
        eval->numpass++;
        if(n != eval->numpass) return PR_SUCCESS;
    }
    eval->sflag = 1;
    lp = eval->linebuf;
    sp = eval->genbuf;
    rp = rhsbuf;
    sp = place(eval, sp, lp, step_vars->loc1);
    while ((c = *rp++) != 0) {
        if (c == '&') {
            sp = place(eval, sp, step_vars->loc1, step_vars->loc2);
            if (sp == NULL)
                return PR_FAILURE;
        }
        else if (c == '\\') {
            c = *rp++;
            if (c >= '1' && c < NBRA+'1') {
                sp = place(eval, sp, step_vars->braslist[c-'1'],
                           step_vars->braelist[c-'1']);
                if (sp == NULL)
                    return PR_FAILURE;
            }
            else
                *sp++ = c;
          } else
            *sp++ = c;
        if (sp >= eval->genbuf + eval->gsize) {
            /* expand genbuf and set the sp appropriately */
            grow_gen_buffer(eval, eval->gsize + 1024, &sp);
        }
    }
    lp = step_vars->loc2;
    step_vars->loc2 = sp - eval->genbuf + eval->linebuf;
    append_to_genbuf(eval, lp, &sp);
    copy_to_linebuf(eval, eval->genbuf);
    return rv;
}

/*
 * place
 */
static char *place(sed_eval_t *eval, char *asp, char *al1, char *al2)
{
    char *sp = asp;
    int n = al2 - al1;
    unsigned int reqsize = (sp - eval->genbuf) + n + 1;

    if (eval->gsize < reqsize) {
        grow_gen_buffer(eval, reqsize, &sp);
    }
    memcpy(sp, al1, n);
    return sp + n;
}

/*
 * command
 */
static PRStatus command(sed_eval_t *eval, sed_reptr_t *ipc,
                            step_vars_storage *step_vars)
{
    int    i;
    char   *p1, *p2, *p3;
    int length;
    char sz[32]; /* 32 bytes enough to store 64 bit integer in decimal */


    switch(ipc->command) {

        case ACOM:
            if(eval->aptr >= &eval->abuf[SED_ABUFSIZE]) {
                eval_errf(eval, XP_GetAdminStr(DBT_TMAMES), eval->lnum);
            } else {
                *eval->aptr++ = ipc;
                *eval->aptr = NULL;
            }
            break;

        case CCOM:
            eval->delflag = 1;
            if(!eval->inar[ipc->nrep] || eval->dolflag) {
                for (p1 = ipc->re1; *p1; p1++)
                    ;
                wline(eval, ipc->re1, p1 - ipc->re1);
            }
            break;
        case DCOM:
            eval->delflag++;
            break;
        case CDCOM:
            p1 = eval->linebuf;

            while(*p1 != '\n') {
                if(*p1++ == 0) {
                    eval->delflag++;
                    return PR_SUCCESS;
                }
            }

            p1++;
            copy_to_linebuf(eval, p1);
            eval->jflag++;
            break;

        case EQCOM:
            length = PR_snprintf(sz, sizeof(sz), "%d", (int) eval->lnum);
            wline(eval, sz, length);
            break;

        case GCOM:
            copy_to_linebuf(eval, eval->holdbuf);
            break;

        case CGCOM:
            append_to_linebuf(eval, "\n");
            append_to_linebuf(eval, eval->holdbuf);
            break;

        case HCOM:
            copy_to_holdbuf(eval, eval->linebuf);
            break;

        case CHCOM:
            append_to_holdbuf(eval, "\n");
            append_to_holdbuf(eval, eval->linebuf);
            break;

        case ICOM:
            for (p1 = ipc->re1; *p1; p1++);
            wline(eval, ipc->re1, p1 - ipc->re1);
            break;

        case BCOM:
            eval->jflag = 1;
            break;


        case LCOM:
            p1 = eval->linebuf;
            p2 = eval->genbuf;
            eval->genbuf[72] = 0;
            while(*p1)
                if((unsigned char)*p1 >= 040) {
                    if(*p1 == 0177) {
                        p3 = rub;
                        while ((*p2++ = *p3++) != 0)
                            if(p2 >= eval->lcomend) {
                                *p2 = '\\';
                                wline(eval, eval->genbuf,
                                      strlen(eval->genbuf));
                                p2 = eval->genbuf;
                            }
                        p2--;
                        p1++;
                        continue;
                    }
                    if(!isprint(*p1 & 0377)) {
                        *p2++ = '\\';
                        if(p2 >= eval->lcomend) {
                            *p2 = '\\';
                            wline(eval, eval->genbuf, strlen(eval->genbuf));
                            p2 = eval->genbuf;
                        }
                        *p2++ = (*p1 >> 6) + '0';
                        if(p2 >= eval->lcomend) {
                            *p2 = '\\';
                            wline(eval, eval->genbuf, strlen(eval->genbuf));
                            p2 = eval->genbuf;
                        }
                        *p2++ = ((*p1 >> 3) & 07) + '0';
                        if(p2 >= eval->lcomend) {
                            *p2 = '\\';
                            wline(eval, eval->genbuf, strlen(eval->genbuf));
                            p2 = eval->genbuf;
                        }
                        *p2++ = (*p1++ & 07) + '0';
                        if(p2 >= eval->lcomend) {
                            *p2 = '\\';
                            wline(eval, eval->genbuf, strlen(eval->genbuf));
                            p2 = eval->genbuf;
                        }
                    } else {
                        *p2++ = *p1++;
                        if(p2 >= eval->lcomend) {
                            *p2 = '\\';
                            wline(eval, eval->genbuf, strlen(eval->genbuf));
                            p2 = eval->genbuf;
                        }
                    }
                } else {
                    p3 = trans[(unsigned char)*p1-1];
                    while ((*p2++ = *p3++) != 0)
                        if(p2 >= eval->lcomend) {
                            *p2 = '\\';
                            wline(eval, eval->genbuf, strlen(eval->genbuf));
                            p2 = eval->genbuf;
                        }
                    p2--;
                    p1++;
                }
            *p2 = 0;
            wline(eval, eval->genbuf, strlen(eval->genbuf));
            break;

        case NCOM:
            if(!eval->commands->nflag) {
                wline(eval, eval->linebuf, eval->lspend - eval->linebuf);
            }

            if(eval->aptr > eval->abuf)
                arout(eval);
            eval->lspend = eval->linebuf;
            eval->pending = ipc->next;

            break;
        case CNCOM:
            if(eval->aptr > eval->abuf)
                arout(eval);
            append_to_linebuf(eval, "\n");
            eval->pending = ipc->next;
            break;

        case PCOM:
            wline(eval, eval->linebuf, eval->lspend - eval->linebuf);
            break;
        case CPCOM:
            for (p1 = eval->linebuf; *p1 != '\n' && *p1 != '\0'; p1++);
            wline(eval, eval->linebuf, p1 - eval->linebuf);
            break;

        case QCOM:
            if (!eval->commands->nflag)
                wline(eval, eval->linebuf, eval->lspend - eval->linebuf);

            if(eval->aptr > eval->abuf)
                arout(eval);

            eval->quitflag = 1;
            break;
        case RCOM:
            if(eval->aptr >= &eval->abuf[SED_ABUFSIZE]) {
                eval_errf(eval, XP_GetAdminStr(DBT_TMRMES), eval->lnum);
            } else {
                *eval->aptr++ = ipc;
                *eval->aptr = NULL;
            }
            break;

        case SCOM:
            i = substitute(eval, ipc, step_vars);
            if (i == -1) {
                return PR_FAILURE;
            }
            if(ipc->pfl && eval->commands->nflag && i) {
                if(ipc->pfl == 1) {
                    wline(eval, eval->linebuf, eval->lspend - eval->linebuf);
                } else {
                    for (p1 = eval->linebuf; *p1 != '\n' && *p1 != '\0'; p1++);
                    wline(eval, eval->linebuf, p1 - eval->linebuf);
                }
            }
            if (i && (ipc->findex >= 0) && eval->fcode[ipc->findex])
                PR_fprintf(eval->fcode[ipc->findex], "%s\n",
                                eval->linebuf);
            break;

        case TCOM:
            if(eval->sflag == 0)  break;
            eval->sflag = 0;
            eval->jflag = 1;
            break;

        case WCOM:
            if (ipc->findex >= 0)
                PR_fprintf(eval->fcode[ipc->findex], "%s\n",
                                eval->linebuf);
            break;
        case XCOM:
            copy_to_genbuf(eval, eval->linebuf);
            copy_to_linebuf(eval, eval->holdbuf);
            copy_to_holdbuf(eval, eval->genbuf);
            break;

        case YCOM: 
            p1 = eval->linebuf;
            p2 = ipc->re1;
            while((*p1 = p2[(unsigned char)*p1]) != 0)    p1++;
            break;
    }
    return PR_SUCCESS;
}

/*
 * arout
 */
static void arout(sed_eval_t *eval)
{
    eval->aptr = eval->abuf - 1;
    while (*++eval->aptr) {
        if ((*eval->aptr)->command == ACOM) {
            char *p1;

            for (p1 = (*eval->aptr)->re1; *p1; p1++);
            wline(eval, (*eval->aptr)->re1, p1 - (*eval->aptr)->re1);
        } else {
            PRFileDesc *fi = NULL;
            char buf[512];
            int n;

            if ((fi = PR_Open((*eval->aptr)->re1, PR_RDONLY, 0)) == NULL)
                continue;
            while ((n  = PR_Read(fi, buf, sizeof(buf))) > 0) {
                PR_Write(eval->fout, buf, n);
            }
            PR_Close(fi);
        }
    }
    eval->aptr = eval->abuf;
    *eval->aptr = NULL;
}

/*
 * wline
 */
static void wline(sed_eval_t *eval, char *buf, int sz)
{
    PRFileDesc *fd = eval->fout;
    PRIOVec iov[2];
    int ioc = 0;

    if (sz > 0) {
        iov[ioc].iov_base = buf;
        iov[ioc].iov_len = sz;
        ioc++;
    }

    iov[ioc].iov_base = "\n";
    iov[ioc].iov_len = 1;
    ioc++;

    if (fd->methods->file_type == PR_DESC_FILE) {
        int i;

        for (i = 0; i < ioc; i++)
            PR_Write(fd, iov[i].iov_base, iov[i].iov_len);
    } else {
        PR_Writev(fd, iov, ioc, PR_INTERVAL_NO_TIMEOUT);
    }
}
