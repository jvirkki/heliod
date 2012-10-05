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

#define CCEOF 22

static int fcomp(sed_commands_t *commands, PRFileDesc *fin);
static char *compsub(sed_commands_t *commands,
                     sed_comp_args *compargs, char *rhsbuf);
static int rline(sed_commands_t *commands, PRFileDesc *fin,
                 char *lbuf, char *lbend);
static char *address(sed_commands_t *commands, char *expbuf,
                     PRStatus* status);
static char *text(sed_commands_t *commands, char *textbuf, char *endbuf);
static sed_label_t *search(sed_commands_t *commands);
static char *ycomp(sed_commands_t *commands, char *expbuf);
static char *comple(sed_commands_t *commands, sed_comp_args *compargs,
                    char *x1, char *ep, char *x3, char x4);
static sed_reptr_t *alloc_reptr(sed_commands_t *commands);

void command_errf(sed_commands_t *commands, const char *fmt, ...)
{
    if (commands->errfn && commands->pool) {
        va_list args;
        va_start(args, fmt);
        commands->errfn(commands->data, fmt, args);
        va_end(args);
    }
}

/*
 * sed_init_commands
 */
PRStatus sed_init_commands(sed_commands_t *commands, sed_err_fn_t *errfn, void *data,
                               pool_handle_t *p)
{
    memset(commands, 0, sizeof(*commands));

    commands->errfn = errfn;
    commands->data = data;

    commands->labtab = commands->ltab;
    commands->lab = commands->labtab + 1;
    commands->pool = p;

    commands->respace = (char *)pool_calloc(p, (size_t)1, (size_t)RESIZE);
    if (commands->respace == NULL) {
        command_errf(commands, XP_GetAdminStr(DBT_OOMMES));
        return PR_FAILURE;
    }

    commands->rep = alloc_reptr(commands);
    if (commands->rep == NULL)
        return PR_FAILURE;

    commands->rep->ad1 = commands->respace;
    commands->reend = &commands->respace[RESIZE - 1];
    commands->labend = &commands->labtab[SED_LABSIZE];

    return PR_SUCCESS;
}
 
/*
 * sed_destroy_commands
 */
void sed_destroy_commands(sed_commands_t *commands)
{
    sed_reptr_t *rep;
/* TBD
    int i;
    for (i = 0; i < commands->nfiles; i++)
        PR_Close(commands->fcode[i]);
*/
    pool_free(commands->pool, commands->linebuf);

    rep = commands->ptrspace;
    while (rep) {
        sed_reptr_t *next;

        next = rep->next;
        pool_free(commands->pool, rep);
        rep = next;
    }

    pool_free(commands->pool, commands->respace);
}

/*
 * sed_compile_string
 */
PRStatus sed_compile_string(sed_commands_t *commands, const char *s)
{
    PRStatus rv;

    commands->earg = s;
    commands->eflag = 1;

    rv = fcomp(commands, NULL);

    commands->eflag = 0;

    return rv;
}

/*
 * sed_compile_file
 */
PRStatus sed_compile_file(sed_commands_t *commands, PRFileDesc *fin)
{
    return fcomp(commands, fin);
}

/*
 * sed_finalize_commands
 */
PRStatus sed_finalize_commands(sed_commands_t *commands)
{
    sed_label_t *lab;

    if (commands->depth) {
        command_errf(commands, XP_GetAdminStr(DBT_TMOMES));
        return PR_FAILURE;
    }

    for (lab = commands->labtab; lab < commands->lab; lab++) {
        if (lab->address == 0) {
            command_errf(commands, XP_GetAdminStr(DBT_ULMES), lab->asc);
            return PR_FAILURE;
        }

        if (lab->chain) {
            sed_reptr_t *rep;

            rep = lab->chain;
            while (rep->lb1) {
                sed_reptr_t *next;

                next = rep->lb1;
                rep->lb1 = lab->address;
                rep = next;
            }

            rep->lb1 = lab->address;
        }
    }

    return PR_SUCCESS;
}

/*
 * dechain
 */
static void dechain(sed_label_t *lpt, sed_reptr_t *address)
{
    sed_reptr_t *rep;
    if ((lpt == NULL) || (lpt->chain == NULL) || (address == NULL))
        return;
    rep = lpt->chain;
    while (rep->lb1) {
        sed_reptr_t *next;

        next = rep->lb1;
        rep->lb1 = address;
        rep = next;
    }
    rep->lb1 = address;
    lpt->chain = NULL;
}

/*
 * fcomp
 */
static int fcomp(sed_commands_t *commands, PRFileDesc *fin)
{
    char *p, *op, *tp;
    sed_reptr_t *pt, *pt1;
    int i, ii;
    sed_label_t *lpt;
    char fnamebuf[PATH_MAX];
    PRStatus status;
    sed_comp_args compargs;

    op = commands->lastre;
    if (!commands->linebuf) {
        commands->linebuf = (char *)pool_calloc(commands->pool, LBSIZE + 1, 1);
    }

    if (rline(commands, fin, commands->linebuf,
              (commands->linebuf + LBSIZE + 1)) < 0)
        return 0;
    if (*commands->linebuf == '#') {
        if (commands->linebuf[1] == 'n')
            commands->nflag = 1;
    }
    else {
        commands->cp = commands->linebuf;
        goto comploop;
    }

    for (;;) {
        if (rline(commands, fin, commands->linebuf,
                  (commands->linebuf + LBSIZE + 1)) < 0)
            break;

        commands->cp = commands->linebuf;

comploop:
        while (*commands->cp == ' ' || *commands->cp == '\t')
            commands->cp++;
        if (*commands->cp == '\0' || *commands->cp == '#')
            continue;
        if (*commands->cp == ';') {
            commands->cp++;
            goto comploop;
        }

        p = address(commands, commands->rep->ad1, &status);
        if (status != PR_SUCCESS) {
            command_errf(commands, XP_GetAdminStr(DBT_CGMES), commands->linebuf);
            return -1;
        }

        if (p == commands->rep->ad1) {
            if (op)
                commands->rep->ad1 = op;
            else {
                command_errf(commands, XP_GetAdminStr(DBT_NRMES));
                return -1;
            }
        } else if (p == 0) {
            p = commands->rep->ad1;
            commands->rep->ad1 = 0;
        } else {
            op = commands->rep->ad1;
            if (*commands->cp == ',' || *commands->cp == ';') {
                commands->cp++;
                commands->rep->ad2 = p;
                p = address(commands, commands->rep->ad2, &status);
                if ((status != PR_SUCCESS) || (p == 0)) {
                    command_errf(commands, XP_GetAdminStr(DBT_CGMES), commands->linebuf);
                    return -1;
                }
                if (p == commands->rep->ad2)
                    commands->rep->ad2 = op;
                else
                    op = commands->rep->ad2;
            } else
                commands->rep->ad2 = 0;
        }

        if(p > &commands->respace[RESIZE-1]) {
            command_errf(commands, XP_GetAdminStr(DBT_TMMES));
            return -1;
        }

        while (*commands->cp == ' ' || *commands->cp == '\t')
            commands->cp++;

swit:
        switch(*commands->cp++) {
        default:
            command_errf(commands, XP_GetAdminStr(DBT_UCMES), commands->linebuf);
            return -1;

        case '!':
            commands->rep->negfl = 1;
            goto swit;

        case '{':
            commands->rep->command = BCOM;
            commands->rep->negfl = !(commands->rep->negfl);
            commands->cmpend[commands->depth++] = &commands->rep->lb1;
            commands->rep = alloc_reptr(commands);
            commands->rep->ad1 = p;
            if (*commands->cp == '\0')
                continue;
            goto comploop;

        case '}':
            if (commands->rep->ad1) {
                command_errf(commands, XP_GetAdminStr(DBT_AD0MES), commands->linebuf);
                return -1;
            }

            if (--commands->depth < 0) {
                command_errf(commands, XP_GetAdminStr(DBT_TMCMES));
                return -1;
            }
            *commands->cmpend[commands->depth] = commands->rep;

            commands->rep->ad1 = p;
            continue;

        case '=':
            commands->rep->command = EQCOM;
            if (commands->rep->ad2) {
                command_errf(commands, XP_GetAdminStr(DBT_AD1MES), commands->linebuf);
                return -1;
            }
            break;

        case ':':
            if (commands->rep->ad1) {
                command_errf(commands, XP_GetAdminStr(DBT_AD0MES), commands->linebuf);
                return -1;
            }

            while (*commands->cp++ == ' ');
            commands->cp--;

            tp = commands->lab->asc;
            while ((*tp++ = *commands->cp++)) {
                if (tp >= &(commands->lab->asc[8])) {
                    command_errf(commands, XP_GetAdminStr(DBT_LTLMES), commands->linebuf);
                    return -1;
                }
            }
            *--tp = '\0';

            if ((lpt = search(commands)) != NULL) {
                if (lpt->address) {
                    command_errf(commands, XP_GetAdminStr(DBT_DLMES), commands->linebuf);
                    return -1;
                }
                dechain(lpt, commands->rep);
            } else {
                commands->lab->chain = 0;
                lpt = commands->lab;
                if (++commands->lab >= commands->labend) {
                    command_errf(commands, XP_GetAdminStr(DBT_TMLMES), commands->linebuf);
                    return -1;
                }
            }
            lpt->address = commands->rep;
            commands->rep->ad1 = p;

            continue;

        case 'a':
            commands->rep->command = ACOM;
            if (commands->rep->ad2) {
                command_errf(commands, XP_GetAdminStr(DBT_AD1MES), commands->linebuf);
                return -1;
            }
            if (*commands->cp == '\\')
                commands->cp++;
            if (*commands->cp++ != '\n') {
                command_errf(commands, XP_GetAdminStr(DBT_CGMES), commands->linebuf);
                return -1;
            }
            commands->rep->re1 = p;
            p = text(commands, commands->rep->re1, commands->reend);
            if (p == NULL)
                return -1;
            break;

        case 'c':
            commands->rep->command = CCOM;
            if (*commands->cp == '\\') commands->cp++;
            if (*commands->cp++ != ('\n')) {
                command_errf(commands, XP_GetAdminStr(DBT_CGMES), commands->linebuf);
                return -1;
            }
            commands->rep->re1 = p;
            p = text(commands, commands->rep->re1, commands->reend);
            if (p == NULL)
                return -1;
            break;

        case 'i':
            commands->rep->command = ICOM;
            if (commands->rep->ad2) {
                command_errf(commands, XP_GetAdminStr(DBT_AD1MES), commands->linebuf);
                return -1;
            }
            if (*commands->cp == '\\') commands->cp++;
            if (*commands->cp++ != ('\n')) {
                command_errf(commands, XP_GetAdminStr(DBT_CGMES), commands->linebuf);
                return -1;
            }
            commands->rep->re1 = p;
            p = text(commands, commands->rep->re1, commands->reend);
            if (p == NULL)
                return -1;
            break;

        case 'g':
            commands->rep->command = GCOM;
            break;

        case 'G':
            commands->rep->command = CGCOM;
            break;

        case 'h':
            commands->rep->command = HCOM;
            break;

        case 'H':
            commands->rep->command = CHCOM;
            break;

        case 't':
            commands->rep->command = TCOM;
            goto jtcommon;

        case 'b':
            commands->rep->command = BCOM;
jtcommon:
            while (*commands->cp++ == ' ');
            commands->cp--;

            if (*commands->cp == '\0') {
                if ((pt = commands->labtab->chain) != NULL) {
                    while ((pt1 = pt->lb1) != NULL)
                        pt = pt1;
                    pt->lb1 = commands->rep;
                } else
                    commands->labtab->chain = commands->rep;
                break;
            }
            tp = commands->lab->asc;
            while ((*tp++ = *commands->cp++))
                if (tp >= &(commands->lab->asc[8])) {
                    command_errf(commands, XP_GetAdminStr(DBT_LTLMES), commands->linebuf);
                    return -1;
                }
            commands->cp--;
            *--tp = '\0';

            if ((lpt = search(commands)) != NULL) {
                if (lpt->address) {
                    commands->rep->lb1 = lpt->address;
                } else {
                    pt = lpt->chain;
                    while ((pt1 = pt->lb1) != NULL)
                        pt = pt1;
                    pt->lb1 = commands->rep;
                }
            } else {
                commands->lab->chain = commands->rep;
                commands->lab->address = 0;
                if (++commands->lab >= commands->labend) {
                    command_errf(commands, XP_GetAdminStr(DBT_TMLMES), commands->linebuf);
                    return -1;
                }
            }
            break;

        case 'n':
            commands->rep->command = NCOM;
            break;

        case 'N':
            commands->rep->command = CNCOM;
            break;

        case 'p':
            commands->rep->command = PCOM;
            break;

        case 'P':
            commands->rep->command = CPCOM;
            break;

        case 'r':
            commands->rep->command = RCOM;
            if (commands->rep->ad2) {
                command_errf(commands, XP_GetAdminStr(DBT_AD1MES), commands->linebuf);
                return -1;
            }
            if (*commands->cp++ != ' ') {
                command_errf(commands, XP_GetAdminStr(DBT_CGMES), commands->linebuf);
                return -1;
            }
            commands->rep->re1 = p;
            p = text(commands, commands->rep->re1, commands->reend);
            if (p == NULL)
                return -1;
            break;

        case 'd':
            commands->rep->command = DCOM;
            break;

        case 'D':
            commands->rep->command = CDCOM;
            commands->rep->lb1 = commands->ptrspace;
            break;

        case 'q':
            commands->rep->command = QCOM;
            if (commands->rep->ad2) {
                command_errf(commands, XP_GetAdminStr(DBT_AD1MES), commands->linebuf);
                return -1;
            }
            break;

        case 'l':
            commands->rep->command = LCOM;
            break;

        case 's':
            commands->rep->command = SCOM;
            commands->sseof = *commands->cp++;
            commands->rep->re1 = p;
            p = comple(commands, &compargs, (char *) 0, commands->rep->re1,
                       commands->reend, commands->sseof);
            if (p == NULL)
                return -1;
            if (p == commands->rep->re1) {
                if (op)
                    commands->rep->re1 = op;
                else {
                    command_errf(commands, XP_GetAdminStr(DBT_NRMES));
                    return -1;
                }
            } else 
                op = commands->rep->re1;
            commands->rep->rhs = p;

            p = compsub(commands, &compargs, commands->rep->rhs);
            if ((p) == NULL)
                return -1;

            if (*commands->cp == 'g') {
                commands->cp++;
                commands->rep->gfl = 999;
            } else if (commands->gflag)
                commands->rep->gfl = 999;

            if (*commands->cp >= '1' && *commands->cp <= '9') {
                i = *commands->cp - '0';
                commands->cp++;
                while (1) {
                    ii = *commands->cp;
                    if (ii < '0' || ii > '9')
                        break;
                    i = i*10 + ii - '0';
                    if (i > 512) {
                        command_errf(commands, XP_GetAdminStr(DBT_TOOBIG), commands->linebuf);
                        return -1;
                    }
                    commands->cp++;
                }
                commands->rep->gfl = i;
            }

            if (*commands->cp == 'p') {
                commands->cp++;
                commands->rep->pfl = 1;
            }

            if (*commands->cp == 'P') {
                commands->cp++;
                commands->rep->pfl = 2;
            }

            if (*commands->cp == 'w') {
                commands->cp++;
                if (*commands->cp++ !=  ' ') {
                    command_errf(commands, XP_GetAdminStr(DBT_CGMES), commands->linebuf);
                    return -1;
                }
                if (text(commands, fnamebuf, &fnamebuf[PATH_MAX]) == NULL) {
                    command_errf(commands, XP_GetAdminStr(DBT_FNTL), commands->linebuf);
                    return -1;
                }
                for (i = commands->nfiles - 1; i >= 0; i--)
                    if (strcmp(fnamebuf,commands->fname[i]) == 0) {
                        commands->rep->findex = i;
                        goto done;
                    }
                if (commands->nfiles >= NWFILES) {
                    command_errf(commands, XP_GetAdminStr(DBT_TMWFMES));
                    return -1;
                }
                commands->fname[commands->nfiles] = (char *)
                            pool_strdup(commands->pool, (const char *)fnamebuf);
                if (commands->fname[commands->nfiles] == NULL) {
                    command_errf(commands, XP_GetAdminStr(DBT_OOMMES));
                    return -1;
                }
                commands->rep->findex = commands->nfiles++;
            }
            break;

        case 'w':
            commands->rep->command = WCOM;
            if (*commands->cp++ != ' ') {
                command_errf(commands, XP_GetAdminStr(DBT_SMMES), commands->linebuf);
                return -1;
            }
            if (text(commands, fnamebuf, &fnamebuf[PATH_MAX]) == NULL) {
                command_errf(commands, XP_GetAdminStr(DBT_FNTL), commands->linebuf);
                return -1;
            }
            for (i = commands->nfiles - 1; i >= 0; i--)
                if (strcmp(fnamebuf, commands->fname[i]) == 0) {
                    commands->rep->findex = i;
                    goto done;
                }
            if (commands->nfiles >= NWFILES) {
                command_errf(commands, XP_GetAdminStr(DBT_TMWFMES));
                return -1;
            }
            if ((commands->fname[commands->nfiles] =
                        (char *)pool_strdup(commands->pool, fnamebuf)) == NULL) {
                command_errf(commands, XP_GetAdminStr(DBT_OOMMES));
                return -1;
            }
            commands->rep->findex = commands->nfiles++;
            break;

        case 'x':
            commands->rep->command = XCOM;
            break;

        case 'y':
            commands->rep->command = YCOM;
            commands->sseof = *commands->cp++;
            commands->rep->re1 = p;
            p = ycomp(commands, commands->rep->re1);
            if (p == NULL)
                return -1;
            break;
        }
done:
        commands->rep = alloc_reptr(commands);

        commands->rep->ad1 = p;

        if (*commands->cp++ != '\0') {
            if (commands->cp[-1] == ';')
                goto comploop;
            command_errf(commands, XP_GetAdminStr(DBT_CGMES), commands->linebuf);
            return -1;
        }
    }
    commands->rep->command = 0;
    commands->lastre = op;

    return 0;
}

static char *compsub(sed_commands_t *commands,
                     sed_comp_args *compargs, char *rhsbuf)
{
    char   *p, *q;

    p = rhsbuf;
    q = commands->cp;
    for(;;) {
        if(p > &commands->respace[RESIZE-1]) {
            command_errf(commands, XP_GetAdminStr(DBT_TMMES), commands->linebuf);
            return NULL;
        }
        if((*p = *q++) == '\\') {
            p++;
            if(p > &commands->respace[RESIZE-1]) {
                command_errf(commands, XP_GetAdminStr(DBT_TMMES), commands->linebuf);
                return NULL;
            }
            *p = *q++;
            if(*p > compargs->nbra + '0' && *p <= '9') {
                command_errf(commands, XP_GetAdminStr(DBT_DOORNG), commands->linebuf);
                return NULL;
            }
            p++;
            continue;
        }
        if(*p == commands->sseof) {
            *p++ = '\0';
            commands->cp = q;
            return(p);
        }
          if(*p++ == '\0') {
            command_errf(commands, XP_GetAdminStr(DBT_EDMOSUB), commands->linebuf);
            return NULL;
        }
    }
}

/*
 * rline
 */
static int rline(sed_commands_t *commands, PRFileDesc *fin,
                 char *lbuf, char *lbend)
{
    char   *p;
    const char *q;
    int    t;
    PRInt32 bytes_read;

    p = lbuf;

    if(commands->eflag) {
        if(commands->eflag > 0) {
            commands->eflag = -1;
            q = commands->earg;
            while((t = *q++) != '\0') {
                if(t == '\n') {
                    commands->saveq = q;
                    goto out1;
                }
                if (p < lbend)
                    *p++ = t;
                if(t == '\\') {
                    if((t = *q++) == '\0') {
                        commands->saveq = NULL;
                        return(-1);
                    }
                    if (p < lbend)
                        *p++ = t;
                }
            }
            commands->saveq = NULL;

        out1:
            if (p == lbend) {
                command_errf(commands, XP_GetAdminStr(DBT_CLTL), commands->linebuf);
                return -1;
            }
            *p = '\0';
            return(1);
        }
        if((q = commands->saveq) == 0)    return(-1);

        while((t = *q++) != '\0') {
            if(t == '\n') {
                commands->saveq = q;
                goto out2;
            }
            if(p < lbend)
                *p++ = t;
            if(t == '\\') {
                if((t = *q++) == '\0') {
                    commands->saveq = NULL;
                    return(-1);
                }
                if (p < lbend)
                    *p++ = t;
            }
        }
        commands->saveq = NULL;

    out2:
        if (p == lbend) {
            command_errf(commands, XP_GetAdminStr(DBT_CLTL), commands->linebuf);
            return -1;
        }
        *p = '\0';
        return(1);
    }

    bytes_read = 1;
    /* XXX extremely inefficient 1 byte reads */
    while (PR_Read(fin, &t, bytes_read) == 1) {
        if(t == '\n') {
            if (p == lbend) {
                command_errf(commands, XP_GetAdminStr(DBT_CLTL), commands->linebuf);
                return -1;
            }
            *p = '\0';
            return(1);
        }
        if (p < lbend)
            *p++ = t;
        if(t == '\\') {
            bytes_read = 1;
            if (PR_Read(fin, &t, bytes_read) != 1) {
                return -1;
            }
            if(p < lbend)
                *p++ = t;
        }
        bytes_read = 1;
    }
    return(-1);
}

/*
 * address
 */
static char *address(sed_commands_t *commands, char *expbuf,
                     PRStatus* status)
{
    char   *rcp;
    PRInt64 lno;
    sed_comp_args compargs;

    *status = PR_SUCCESS;
    if(*commands->cp == '$') {
        if (expbuf > &commands->respace[RESIZE-2]) {
            command_errf(commands, XP_GetAdminStr(DBT_TMMES), commands->linebuf);
            *status = PR_FAILURE;
            return NULL;
        }
        commands->cp++;
        *expbuf++ = CEND;
        *expbuf++ = CCEOF;
        return(expbuf);
    }
    if (*commands->cp == '/' || *commands->cp == '\\' ) {
        if ( *commands->cp == '\\' )
            commands->cp++;
        commands->sseof = *commands->cp++;
        return(comple(commands, &compargs, (char *) 0, expbuf, commands->reend,
                      commands->sseof));
    }

    rcp = commands->cp;
    lno = 0;

    while(*rcp >= '0' && *rcp <= '9')
        lno = lno*10 + *rcp++ - '0';

    if(rcp > commands->cp) {
        if (expbuf > &commands->respace[RESIZE-3]) {
            command_errf(commands, XP_GetAdminStr(DBT_TMMES), commands->linebuf);
            *status = PR_FAILURE;
            return NULL;
        }
        *expbuf++ = CLNUM;
        *expbuf++ = commands->nlno;
        commands->tlno[commands->nlno++] = lno;
        if(commands->nlno >= SED_NLINES) {
            command_errf(commands, XP_GetAdminStr(DBT_TMLNMES), commands->linebuf);
            *status = PR_FAILURE;
            return NULL;
        }
        *expbuf++ = CCEOF;
        commands->cp = rcp;
        return(expbuf);
    }
    return(NULL);
}

/*
 * text
 */
static char *text(sed_commands_t *commands, char *textbuf, char *tbend)
{
    char   *p, *q;

    p = textbuf;
    q = commands->cp;
#ifndef S5EMUL
    /*
     * Strip off indentation from text to be inserted.
     */
    while(*q == '\t' || *q == ' ')    q++;
#endif
    for(;;) {

        if(p > tbend)
            return(NULL);    /* overflowed the buffer */
        if((*p = *q++) == '\\')
            *p = *q++;
        if(*p == '\0') {
            commands->cp = --q;
            return(++p);
        }
#ifndef S5EMUL
        /*
         * Strip off indentation from text to be inserted.
         */
        if(*p == '\n') {
            while(*q == '\t' || *q == ' ')    q++;
        }
#endif
        p++;
    }
}


/*
 * search
 */
static sed_label_t *search(sed_commands_t *commands)
{
    sed_label_t *rp;
    sed_label_t *ptr;

    rp = commands->labtab;
    ptr = commands->lab;
    while (rp < ptr) {
        if (strcmp(rp->asc, ptr->asc) == 0)
            return rp;
        rp++;
    }

    return 0;
}

/*
 * ycomp
 */
static char *ycomp(sed_commands_t *commands, char *expbuf)
{
    char    c;
    int cint; /* integer value of char c */
    char *ep, *tsp;
    int i;
    char    *sp;

    ep = expbuf;
    if(ep + 0377 > &commands->respace[RESIZE-1]) {
        command_errf(commands, XP_GetAdminStr(DBT_TMMES), commands->linebuf);
        return NULL;
    }
    sp = commands->cp;
    for(tsp = commands->cp; (c = *tsp) != commands->sseof; tsp++) {
        if(c == '\\')
            tsp++;
        if(c == '\0' || c == '\n') {
            command_errf(commands, XP_GetAdminStr(DBT_EDMOSTR), commands->linebuf);
            return NULL;
        }
    }
    tsp++;
    memset(ep, 0, 0400);

    while((c = *sp++) != commands->sseof) {
        c &= 0377;
        if(c == '\\' && *sp == 'n') {
            sp++;
            c = '\n';
        }
        cint = (int) c;
        if((ep[cint] = *tsp++) == '\\' && *tsp == 'n') {
            ep[cint] = '\n';
            tsp++;
        }
        if(ep[cint] == commands->sseof || ep[cint] == '\0') {
            command_errf(commands, XP_GetAdminStr(DBT_TSNTSS), commands->linebuf);
        }
    }
    if(*tsp != commands->sseof) {
        if(*tsp == '\0') {
            command_errf(commands, XP_GetAdminStr(DBT_EDMOSTR), commands->linebuf);
        }
        else {
            command_errf(commands, XP_GetAdminStr(DBT_TSNTSS), commands->linebuf);
        }
        return NULL;
    }
    commands->cp = ++tsp;

    for(i = 0; i < 0400; i++)
        if(ep[i] == 0)
            ep[i] = i;

    return(ep + 0400);
}

/*
 * comple
 */
static char *comple(sed_commands_t *commands, sed_comp_args *compargs,
                    char *x1, char *ep, char *x3, char x4)
{
    char *p;

    p = sed_compile(commands, compargs, ep + 1, x3, x4);
    if(p == ep + 1)
        return(ep);
    *ep = compargs->circf;
    return(p);
}

/*
 * alloc_reptr
 */
static sed_reptr_t *alloc_reptr(sed_commands_t *commands)
{
    sed_reptr_t *var;

    var = (sed_reptr_t *)pool_calloc(commands->pool, 1, sizeof(sed_reptr_t));
    if (var == NULL) {
        command_errf(commands, XP_GetAdminStr(DBT_OOMMES));
        return 0;
    }

    var->nrep = commands->nrep;
    var->findex = -1;
    commands->nrep++;

    if (commands->ptrspace == NULL)
        commands->ptrspace = var;
    else
        commands->ptrend->next = var;

    commands->ptrend = var;
    commands->labtab->address = var;
    return var;
}


