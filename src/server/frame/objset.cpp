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
 * objset.c: Parses a config. file into an object set
 * 
 * See objset.h for details.
 * 
 * Rob McCool
 */

#include <string.h>
#include <ctype.h>

#include "netsite.h"
#include "support/NSString.h"
#include "support/SimpleHash.h"
#include "support/EreportableException.h"
#include "httpdaemon/nvpairs.h"
#include "httpdaemon/configuration.h"
#include "base/util.h"
#include "base/shexp.h"
#include "base/vs.h"
#include "frame/conf.h"
#include "frame/expr.h"
#include "frame/object.h"
#include "frame/httpdir.h"
#include "frame/dbtframe.h"
#include "frame/objset.h"
#include "expr_parse.h"


/* 
 * The default number of objects to leave room for in an object set,
 * and the number of new entries by which to increase the size when that 
 * room is filled.
 */
#define OBJSET_INCSIZE 8

/*
 * ObjsetException is thrown when an error is encountered while parsing
 * obj.conf.
 */
class ObjsetException : public EreportableException {
public:
    ObjsetException(const TokenPosition& position, const char *message)
    : EreportableException(LOG_MISCONFIG, message),
      line(position.line),
      col(position.col)
    { }

    ObjsetException(int lineArg, int colArg, const char *message)
    : EreportableException(LOG_MISCONFIG, message),
      line(lineArg),
      col(colArg)
    { }

    int line;
    int col;
};

/*
 * ObjsetTag enumerates the various case-insensitive obj.conf tag names.
 */
enum ObjsetTag {
    OBJSET_INVALID = -1,
    OBJSET_OBJECT,
    OBJSET_CLIENT,
    OBJSET_IF,
    OBJSET_ELSEIF,
    OBJSET_ELSE
};

#define OBJECT_OPEN_TAG "<Object>"
#define OBJECT_CLOSE_TAG "</Object>"
#define CLIENT_CLOSE_TAG "</Client>"

static const TokenContent OBJSET_LT(TOKEN_OPERATOR, "<");
static const TokenContent OBJSET_GT(TOKEN_OPERATOR, ">");
static const TokenContent OBJSET_LTSLASH(TOKEN_OPERATOR, "</");
static const TokenContent OBJSET_EQUALS(TOKEN_OPERATOR, "=");
static const TokenContent OBJSET_LEFTBRACE(TOKEN_OPERATOR, "{");
static const TokenContent OBJSET_RIGHTBRACE(TOKEN_OPERATOR, "}");


/* ---------------------------- objset_create ----------------------------- */

NSAPI_PUBLIC httpd_objset *objset_create(void)
{
    return objset_create_pool(system_pool());
}


/* -------------------------- objset_create_pool -------------------------- */

httpd_objset *objset_create_pool(pool_handle_t *pool)
{
    httpd_objset *os = (httpd_objset *) pool_malloc(pool, sizeof(httpd_objset));

    os->pos = 0;
    os->obj = (httpd_object **) pool_malloc(pool, OBJSET_INCSIZE*sizeof(httpd_object *));
    os->obj[0] = NULL;

    os->initfns = NULL;

    return os;
}


/* ------------------------- objset_free_setonly -------------------------- */

NSAPI_PUBLIC void objset_free_setonly(httpd_objset *os) 
{
    FREE(os->obj);
    FREE(os);
}


/* ----------------------------- objset_free ------------------------------ */

NSAPI_PUBLIC void objset_free(httpd_objset *os) 
{
    register int x;

    for(x=0; x < os->pos; x++)
        object_free(os->obj[x]);
    if(os->initfns) {
        for(x = 0; os->initfns[x]; ++x)
            pblock_free(os->initfns[x]);
        FREE(os->initfns);
    }
    objset_free_setonly(os);
}


/* -------------------------- objset_add_object --------------------------- */

NSAPI_PUBLIC void objset_add_object(httpd_object *obj, httpd_objset *os) 
{
    char *t, *ppath = pblock_findkeyval(pb_key_ppath, obj->name);
    register httpd_object **objs;
    register int x, y, l;

    if((os->pos) && (!((os->pos+1) % OBJSET_INCSIZE))) {
        os->obj = (httpd_object **) 
            REALLOC(os->obj, (os->pos+1+OBJSET_INCSIZE) * sizeof(httpd_object*));
	}

    objs = os->obj;
    x=os->pos++;
    if(ppath) {
        l = strlen(ppath);
        for(x = 0; x < os->pos-1; x++) {
            if( (t = pblock_findkeyval(pb_key_ppath, objs[x]->name)) &&
                (strlen(t) > l))
                break;
        }
	for(y = os->pos; y != x; --y)
            objs[y] = objs[y-1];
    } else {
	objs[x+1] = objs[x];
    }
    objs[x] = obj;
}


/* -------------------------- objset_new_object --------------------------- */

NSAPI_PUBLIC httpd_object *objset_new_object(pblock *name, httpd_objset *os) 
{
    httpd_object *obj = object_create(NUM_DIRECTIVES, name);

    objset_add_object(obj, os);
    return obj;
}


/* --------------------------- objset_add_init ---------------------------- */

NSAPI_PUBLIC void objset_add_init(pblock *initfn, httpd_objset *os)
{
    // moved to conf_add_init
    // as Init directives are now back in magnus.conf

#if 0
    register int x;

    if(!os->initfns) {
        os->initfns = (pblock **) MALLOC(2*sizeof(pblock *));
        x = 0;
    }
    else {
        for(x = 0; os->initfns[x]; ++x); /* no action */
        os->initfns = (pblock **) 
            REALLOC(os->initfns, (x+2) * sizeof(pblock *));
    }
    os->initfns[x] = initfn;
    os->initfns[x+1] = NULL;
#endif
}


/* --------------------------- throw_unexpected --------------------------- */

static void throw_unexpected(const Token& token)
{
    NSString error;
    if (token.type == TOKEN_EOF) {
        error.append(XP_GetAdminStr(DBT_confUnexpectedEOF));
    } else {
        error.printf(XP_GetAdminStr(DBT_confUnexpectedValueX), token.value);
    }
    throw ObjsetException(token, error);
}


/* ---------------------------- throw_expected ---------------------------- */

static void throw_expected(const TokenPosition& position, const char *value)
{
    NSString error;
    error.printf(XP_GetAdminStr(DBT_confExpectedX), value);
    throw ObjsetException(position, error);
}


/* ------------------------------- adjacent ------------------------------- */

static inline PRBool adjacent(const Token& p1, const TokenPosition& p2)
{
    return (p2.line == p1.line && p2.col == p1.col + strlen(p1.value));
}


/* ---------------------------- require_token ----------------------------- */

static const Token& require_token(Tokenizer& tokenizer, const TokenContent& content)
{
    const Token& token = tokenizer.getToken();
    if (token != content)
        throw_expected(token, content.value);
    return token;
}


/* ------------------------------- get_tag -------------------------------- */

static ObjsetTag get_tag(const TokenContent& name)
{
    if (!strcasecmp(name.value, "Object")) {
        return OBJSET_OBJECT;
    } else if (!strcasecmp(name.value, "Client")) {
        return OBJSET_CLIENT;
    } else if (!strcasecmp(name.value, "If")) {
        return OBJSET_IF;
    } else if (!strcasecmp(name.value, "ElseIf")) {
        return OBJSET_ELSEIF;
    } else if (!strcasecmp(name.value, "Else")) {
        return OBJSET_ELSE;
    }

    return OBJSET_INVALID;
}


/* -------------------------- require_close_tag --------------------------- */

static void require_close_tag(Tokenizer& tokenizer, const TokenContent& name)
{
    const Token& p1 = tokenizer.getToken();
    if (p1 == OBJSET_LTSLASH) {
        const Token& p2 = tokenizer.getToken();
        if (get_tag(p2) == get_tag(name) && adjacent(p1, p2)) {
            const Token& p3 = tokenizer.getToken();
            if (p3 == OBJSET_GT)
                return;
        }
    }

    NSString tag;
    tag.append("</");
    tag.append(name.value);
    tag.append(">");
    throw_expected(p1, tag);
}


/* ---------------------------- check_open_tag ---------------------------- */

static ObjsetTag check_open_tag(const Token& lt, const Token& name)
{
    PR_ASSERT(lt == OBJSET_LT);

    if (!adjacent(lt, name)) {
        NSString tag;
        tag.append("<");
        tag.append(name.value);
        tag.append(" ...>");
        throw_expected(lt, tag);
    }

    if (name.type != TOKEN_IDENTIFIER)
        return OBJSET_INVALID;

    return get_tag(name);
}


/* ----------------------------- count_param ------------------------------ */

static int count_param(const pb_key *key, pblock *pb)
{
    int count = 0;

    for (int hi = 0; hi < pb->hsize; hi++) {
        for (pb_entry *p = pb->ht[hi]; p != NULL; p = p->next) {
            if (param_key(p->param) == key)
                count++;
        }
    }

    return count;
}


/* ----------------------------- insert_param ----------------------------- */

static PRStatus insert_param(const char *name, const char *value, pblock *param)
{
    PRStatus rv = PR_FAILURE;

    // Unescape \escapes and rewrite "\$" to "$$"
    char *unescaped = model_unescape_interpolative(value);
    if (unescaped) {
        // Check for $fragment syntax errors
        ModelString *model = model_str_create(unescaped);
        if (model) {
            // Add the name=value pair to the pblock
            pb_param *pp = pblock_nvinsert(name, unescaped, param);
            if (pp)
                rv = PR_SUCCESS;
            model_str_free(model);
        }
        FREE(unescaped);
    }

    return rv;
}


/* ----------------------------- parse_param ------------------------------ */

static pblock *parse_param(DirectiveTokenizer& tokenizer, PRBool directive)
{
    pblock *param = pblock_create(1);
    if (!param)
        throw EreportableException(LOG_CATASTROPHE, XP_GetAdminStr(DBT_Objconf_OutOfMemory));

    try {
        for (;;) {
            const Token& identifier = tokenizer.getToken();
            if (!directive && identifier == OBJSET_GT)
                break;
            if (identifier.type != TOKEN_IDENTIFIER) {
                if (directive && identifier.newline) {
                    tokenizer.pushToken(identifier);
                    break;
                }
                throw ObjsetException(identifier, XP_GetAdminStr(DBT_confExpectedParameterName));
            }

            NSString name(identifier.value);

            const Token *token = &tokenizer.getToken();
            if (*token == OBJSET_LEFTBRACE) {
                const Token& subscript = *token;
                if (!adjacent(identifier, subscript))
                    throw_expected(subscript, "=");

                name.append(subscript.value);

                int braces = 1;
                while (braces > 0) {
                    token = &tokenizer.getToken();
                    if (token->line != subscript.line)
                        throw_expected(*token, "}");

                    if (token->type == TOKEN_IDENTIFIER) {
                        name.append(token->value);
                    } else if (token->type == TOKEN_SINGLE_QUOTE_STRING) {
                        name.append("'");
                        for (const char *p = token->value; *p; p++) {
                            if (*p == '\'' || *p == '\\')
                                name.append('\\');
                            name.append(*p);
                        }
                        name.append("'");
                    } else if (*token == OBJSET_RIGHTBRACE) {
                        name.append(token->value);
                        braces--;
                    } else if (*token == OBJSET_LEFTBRACE) {
                        name.append(token->value);
                        braces++;
                    } else {
                        throw_expected(*token, "}");
                    }
                }

                token = &tokenizer.getToken();
            }

            if (*token != OBJSET_EQUALS) {
                if (directive && identifier.newline) {
                    tokenizer.pushToken(*token);
                    tokenizer.pushToken(identifier);
                    break;
                }
                throw_expected(*token, "=");
            }

            const Token& value = tokenizer.getToken();
            if (value.type != TOKEN_IDENTIFIER &&
                value.type != TOKEN_DOUBLE_QUOTE_STRING)
                throw ObjsetException(value, XP_GetAdminStr(DBT_confExpectedParameterValue));

            PRStatus rv = insert_param(name.data(), value.value, param);
            if (rv != PR_SUCCESS) {
                NSString error;
                error.printf(XP_GetAdminStr(DBT_confBadParameterXBecauseY), name.data(), system_errmsg());
                throw ObjsetException(value, error);
            }
        }
    }
    catch (...) {
        pblock_free(param);
        throw;
    }

    return param;
}


/* ------------------------ parse_directive_param ------------------------- */

static pblock *parse_directive_param(DirectiveTokenizer& tokenizer)
{
    return parse_param(tokenizer, PR_TRUE);
}


/* ------------------------ parse_container_param ------------------------- */

static pblock *parse_container_param(DirectiveTokenizer& tokenizer)
{
    return parse_param(tokenizer, PR_FALSE);
}


/* ------------------------- parse_container_expr ------------------------- */

static Expression *parse_container_expr(const TokenContent& name, TokenizerCharSource& source)
{
    // Parse the container expression.  The container expression ends at the
    // first first unquoted, unbracketed '>'.
    int firstLine = source.getLine();
    Expression *expr = expr_scan_exclusive(source, ">");
    if (!expr) {
        NSString error;
        error.printf(XP_GetAdminStr(DBT_confContainerXExpressionBadErrorY),
                     name.value, system_errmsg());
        throw ObjsetException(firstLine, 0 /* don't include col */, error);
    }

    return expr;
}


/* --------------------------- parse_container ---------------------------- */

static void parse_container(DirectiveTokenizer& tokenizer, httpd_object *obj, pblock *parentclient, Condition *parentcond)
{
    PRBool done = PR_FALSE;
    Condition *prevcond = NULL; // Set following </If> or </ElseIf>
    NSString comment; // XXX TODO: directive comments
    NSString error;
    int dc;

    do {
        const Token& token = tokenizer.getToken();
        switch (token.type) {
        case TOKEN_COMMENT:
            comment.append(token.value);
            break;

        case TOKEN_OPERATOR:
            if (token == OBJSET_LT) {
                // New child container
                const Token& container = tokenizer.getToken();
                ObjsetTag tag = check_open_tag(token, container);
                if (tag == OBJSET_IF) {
                    // New <If> container
                    Expression *expr = parse_container_expr(container, tokenizer.getSource());
                    Condition *cond = object_add_condition(expr, parentcond, NULL, obj);
                    parse_container(tokenizer, obj, parentclient, cond);
                    require_close_tag(tokenizer, container);
                    prevcond = cond;
                } else if (tag == OBJSET_ELSEIF) {
                    // <ElseIf> requires preceding </If> or </ElseIf>
                    if (!prevcond)
                        throw ObjsetException(token, XP_GetAdminStr(DBT_confElseIfWithoutIfOrElseIf));
                    Expression *expr = parse_container_expr(container, tokenizer.getSource());
                    Condition *cond = object_add_condition(expr, parentcond, prevcond, obj);
                    parse_container(tokenizer, obj, parentclient, cond);
                    require_close_tag(tokenizer, container);
                    prevcond = cond;
                } else if (tag == OBJSET_ELSE) {
                    // <Else> requires preceding </If> or </ElseIf>
                    if (!prevcond)
                        throw ObjsetException(token, XP_GetAdminStr(DBT_confElseWithoutIfOrElseIf));
                    require_token(tokenizer, OBJSET_GT);
                    Condition *cond = object_add_condition(NULL, parentcond, prevcond, obj);
                    parse_container(tokenizer, obj, parentclient, cond);
                    require_close_tag(tokenizer, container);
                    prevcond = NULL;
                } else if (tag == OBJSET_CLIENT) {
                    // New <Client> tag
                    if (parentclient)
                        throw_expected(container, CLIENT_CLOSE_TAG);
                    pblock *client = parse_container_param(tokenizer);
                    object_add_client(client, obj);
                    parse_container(tokenizer, obj, client, parentcond);
                    require_close_tag(tokenizer, container);
                    prevcond = NULL;
                } else if (tag == OBJSET_OBJECT) {
                    throw_expected(container, OBJECT_CLOSE_TAG);
                } else {
                    throw_unexpected(container);
                }
            } else if (token == OBJSET_LTSLASH) {
                // End of the current container
                tokenizer.pushToken(token);
                done = PR_TRUE;
            } else {
                throw_unexpected(token);
            }
            break;

        case TOKEN_IDENTIFIER:
            dc = directive_name2num(token.value);
            if (dc != -1) {
                const char *directive = directive_num2name(dc);
                if (!token.newline) {
                    error.printf(XP_GetAdminStr(DBT_confXDirectiveRequireNewLine),
                                 directive);
                    throw ObjsetException(token, error);
                }
                pblock *param = parse_directive_param(tokenizer);
                if (count_param(pb_key_fn, param) < 1)
                    throw ObjsetException(token, XP_GetAdminStr(DBT_confNeedFn));
                if (count_param(pb_key_fn, param) > 1)
                    throw ObjsetException(token, XP_GetAdminStr(DBT_confDupFn));
                pblock_kvinsert(pb_key_Directive, directive, strlen(directive), param);
                if (object_append_directive(dc, param, parentclient, parentcond, obj) != PR_SUCCESS) {
                    pblock_free(param);
                    throw ObjsetException(token, system_errmsg());
                }
                prevcond = NULL;
            } else {
                throw_unexpected(token);
            }
            break;

        case TOKEN_EOF:
            done = PR_TRUE;
            break;

        default:
            throw_unexpected(token);
        }
    } while (!done);
}


/* ----------------------------- parse_objset ----------------------------- */

static void parse_objset(TokenizerCharSource& source, httpd_objset *os)
{
    DirectiveTokenizer tokenizer(source);

    PRBool done = PR_FALSE;
    NSString comment; // XXX TODO: file comments
    NSString error;

    do {
        const Token& token = tokenizer.getToken();
        switch (token.type) {
        case TOKEN_COMMENT:
            comment.append(token.value);
            break;

        case TOKEN_OPERATOR:
            if (token == OBJSET_LT) {
                // New child container
                const Token& container = tokenizer.getToken();
                ObjsetTag tag = check_open_tag(token, container);
                if (tag == OBJSET_OBJECT) {
                    // New <Object> container
                    pblock *name = parse_container_param(tokenizer);
                    if (!name || (!pblock_findkey(pb_key_name, name) && !pblock_findkey(pb_key_ppath, name)))
                        throw ObjsetException(container, XP_GetAdminStr(DBT_confNeedNamePpath));
                    httpd_object *obj = objset_new_object(name, os);
                    parse_container(tokenizer, obj, NULL, NULL);
                    require_close_tag(tokenizer, container);
                } else {
                    throw_expected(container, OBJECT_OPEN_TAG);
                }
            } else if (token == OBJSET_LTSLASH) {
                throw_expected(token, OBJECT_OPEN_TAG);
            } else {
                throw_unexpected(token);
            }
            break;

        case TOKEN_IDENTIFIER:
            if (directive_name2num(token.value) != -1) {
                throw_expected(token, OBJECT_OPEN_TAG);
            } else {
                throw_unexpected(token);
            }
            break;

        case TOKEN_EOF:
            done = PR_TRUE;
            break;

        default:
            throw_unexpected(token);
        }
    } while (!done);
}


/* -------------------------------- parse --------------------------------- */

static void parse(const char *filename, TokenizerCharSource& source, httpd_objset *os)
{
    try {
        try {
            parse_objset(source, os);
        }
        catch (const TokenizerUnclosedException& e) {
            NSString error;
            error.printf(XP_GetAdminStr(DBT_confMissingClosingCharX), e.closing);
            throw ObjsetException(e.position.line, e.position.col, error);
        }
        catch (const TokenizerCharException& e) {
            NSString error;
            if (isprint(e.c)) {
                error.printf(XP_GetAdminStr(DBT_confUnexpectedCharX), e.c);
            } else {
                error.printf(XP_GetAdminStr(DBT_confUnexpectedIntX), e.c);
            }
            throw ObjsetException(e.position.line, e.position.col, error);
        }
    }
    catch (const TokenizerIOErrorException& e) {
        e.error.restore();
        NSString error;
        error.printf(XP_GetAdminStr(DBT_confErrorReadingFileXBecauseY),
                     filename,
                     system_errmsg());
        throw EreportableException(LOG_FAILURE, error);
    }
    catch (const ObjsetException& e) {
        NSString context;
        if (e.col > 0) {
            context.printf(XP_GetAdminStr(DBT_fileXLineYColZParseFailedPrefix),
                           filename, e.line, e.col);
        } else if (e.line > 0) {
            context.printf(XP_GetAdminStr(DBT_fileXLineYParseFailedPrefix),
                           filename, e.line);
        } else {
            context.printf(XP_GetAdminStr(DBT_fileXParseFailedPrefix),
                           filename);
        }
        throw EreportableException(context, e);
    }
}


/* ----------------------------- objset_load ------------------------------ */

NSAPI_PUBLIC httpd_objset *objset_load(const char *filename, httpd_objset *os)
{
    SYS_FILE fd = system_fopenRO(filename);
    if (fd == SYS_ERROR_FD) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_confErrorOpeningFileXBecauseY),
                filename, system_errmsg());
        return NULL;
    }

    filebuf_t *buf = filebuf_open(fd, FILE_BUFFERSIZE);
    if (!buf) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_confErrorReadingFileXBecauseY),
                filename, system_errmsg());
        return NULL;
    }

    httpd_objset *nos = os;
    if (!nos)
        nos = objset_create();

    try {
        TokenizerFilebufCharSource source(buf);
        parse(filename, source, nos);
    }
    catch (const EreportableException& e) {
        if (nos != os)
            objset_free(nos);
        nos = NULL;
        ereport_exception(e);
    }

    filebuf_close(buf);

    return nos;
}


/* -------------------------- objset_scan_buffer -------------------------- */

NSAPI_PUBLIC httpd_objset *objset_scan_buffer(filebuf_t *buf, char *errstr, httpd_objset *os)
{
    httpd_objset *nos = os;
    if (!nos)
        nos = objset_create();

    try {
        TokenizerFilebufCharSource source(buf);
        parse("obj.conf", source, nos);
    }
    catch (const EreportableException& e) {
        if (nos != os)
            objset_free(nos);
        nos = NULL;
        if (errstr)
            util_snprintf(errstr, 256, e.getDescription());
    }

    return nos;
}


/* ---------------------------- open_condition ---------------------------- */

static PRInt32 open_condition(PRFileDesc *f, const Condition *cond, const char *&condtag)
{
     PRInt32 rv = 0;

     if (!cond->expr) {
         condtag = "Else";
     } else if (cond->follows) {
         condtag = "ElseIf";
     } else {
         condtag = "If";
     }
     char *s = expr_format(cond->expr);
     if (strchr(s, '>')) {
         PR_ASSERT(strcmp(condtag, "Else"));
         rv |= PR_fprintf(f, "<%s (%s)>\n", condtag, s);
     } else if (*s) {
         PR_ASSERT(strcmp(condtag, "Else"));
         rv |= PR_fprintf(f, "<%s %s>\n", condtag, s);
     } else {
         PR_ASSERT(!strcmp(condtag, "Else"));
         rv |= PR_fprintf(f, "<%s>\n", condtag);
     }
     FREE(s);

     return rv;
}

/* --------------------------- close_condition ---------------------------- */

static PRInt32 close_condition(PRFileDesc *f, const char *condtag)
{
    return PR_fprintf(f, "</%s>\n", condtag);
}


/* --------------------------- write_directives --------------------------- */

static PRStatus write_directives(PRFileDesc *f, const char *dc, dtable *dt, int& i, Condition ** conds, int ci, int& pci){
    PRInt32 rv = 0;

    // Open the condition tag
    const char *condtag = NULL;
    const Condition *cond = NULL;

    if (ci != -1)
        cond = conds[ci];

    if (cond) {
        if (ci > (pci+1)) {
            for (int lv = pci + 1; lv < ci; lv++) {
                rv |= open_condition(f, conds[lv], condtag);
                rv |= close_condition(f, condtag);
            }
        }
        rv |= open_condition(f, cond, condtag);
        pci = cond->i;
    }

    const Condition *childcond = NULL;
    const pblock *client = NULL;

    while (i < dt->ni) {
        // Close any existing <Client> tag
        if (dt->inst[i].cond != cond || dt->inst[i].client.pb != client) {
            if (client)
                rv |= PR_fprintf(f, "</Client>\n");
            client = NULL;
        }

        if (dt->inst[i].cond != cond) {
            if (dt->inst[i].cond) {
                // Recurse for nested <ElseIf>/<Else>
                if (childcond && dt->inst[i].cond->follows == childcond) {
                    childcond = dt->inst[i].cond;
                    write_directives(f, dc, dt, i, conds, childcond->i, pci);
                    continue;
                }

                // Recurse for nested <If>
                childcond = dt->inst[i].cond;
                while (childcond) {
                    if (childcond->inside == cond)
                        break;
                    childcond = childcond->inside;
                }
                if (childcond) {
                    write_directives(f, dc, dt, i, conds, childcond->i, pci);
                    continue;
                }
            }

            // Break on end of condition
            break;
        }

        // Open any new <Client> tag
        if (dt->inst[i].client.pb != client) {
            client = dt->inst[i].client.pb;
            if (client) {
                char *s = pblock_pblock2str(client, NULL);
                rv |= PR_fprintf(f, "<Client %s>\n", s);
                FREE(s);
            }
        }

        // Format a directive
        pblock *pb = pblock_dup(dt->inst[i].param.pb);
        param_free(pblock_removekey(pb_key_Directive, pb));
        char *param = pblock_pblock2str(pb, NULL);
        rv |= PR_fprintf(f, "%s %s\n", dc, param);
        FREE(param);
        pblock_free(pb);

        // Onto the next directive...
        i++;
    }

    // Close any existing <Client> tag
    if (client)
        rv |= PR_fprintf(f, "</Client>\n");

    // Close the condition tag
    if (cond)
        rv |= close_condition(f, condtag);

    return (rv < 0) ? PR_FAILURE : PR_SUCCESS;
}


/* ----------------------------- objset_save ------------------------------ */

NSAPI_PUBLIC PRStatus objset_save(const char *filename, httpd_objset *os)
{
    PRInt32 rv = 0;

    SYS_FILE f = system_fopenWT(filename);
    if (f == SYS_ERROR_FD)
        return PR_FAILURE;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    int year = tm->tm_year + 1900;

    rv |= PR_fprintf(f, "#\n");
    rv |= PR_fprintf(f, "# Copyright %d Sun Microsystems, Inc.  All rights reserved.\n", year);
    rv |= PR_fprintf(f, "# Use is subject to license terms.\n");
    rv |= PR_fprintf(f, "#\n\n");

    rv |= PR_fprintf(f, "# You can edit this file, but comments and formatting changes\n", f);
    rv |= PR_fprintf(f, "# might be lost when you use the administration GUI or CLI.\n\n", f);

#if defined(XP_WIN32)
    rv |= PR_fprintf(f, "# Use only forward slashes in pathnames as backslashes can cause\n");
    rv |= PR_fprintf(f, "# problems.  Refer to the documentation for more information.\n\n");
#endif /* XP_WIN32 */

    // XXX TODO: preserve formatting
    // XXX TODO: write user comments, skip ones we wrote

    // Write out the Init directives first
    if (os->initfns)  {
        for (int x = 0; os->initfns[x]; x++)  {
            char *s = NULL;
            s = pblock_pblock2str(os->initfns[x], NULL);
            rv |= PR_fprintf(f, "Init %s\n", s);
            FREE(s);
        }
        rv |= PR_fprintf(f, "\n");
    }

    // For each <Object>...
    for (int x = 0; x < os->pos; x++) {
        char *name = pblock_pblock2str(os->obj[x]->name, NULL);
        rv |= PR_fprintf(f, "<Object %s>\n", name);
        FREE(name);

        // For each directive class...
        for (int di = 0; di < os->obj[x]->nd; di++) {
            const char *dc = directive_num2name(di);
            dtable *dt = &os->obj[x]->dt[di];
            for (int z = 0, pci = -1; z < dt->ni; rv |= write_directives(f, dc, dt, z, os->obj[x]->cond, -1, pci));
        }

        rv |= PR_fprintf(f, "</Object>\n\n");
    }

    system_fclose(f);

    return (rv < 0) ? PR_FAILURE : PR_SUCCESS;
}


/* -------------------------- objset_findbyname --------------------------- */

/* 
 * These should be read from hash tables build in the scan-buffer 
 * routine. 
 */
NSAPI_PUBLIC
httpd_object *objset_findbyname(const char *name, httpd_objset *ign,
                                httpd_objset *os) 
{
    register int x, y;
    pb_param *pp;

    for(x = 0; x < os->pos; x++) {
        if((pp = pblock_findkey(pb_key_name, os->obj[x]->name))) {
            if(!strcasecmp(name, pp->value)) {
                if(ign) {
                    for(y = 0; y < ign->pos; y++)
                        if(ign->obj[y] == os->obj[x])
                            break;
                    if(y != ign->pos)
                        continue;
                }
                return os->obj[x];
            }
        }
    }
    return NULL;
}


/* -------------------------- objset_findbyppath -------------------------- */

NSAPI_PUBLIC
httpd_object *objset_findbyppath(char *ppath, httpd_objset *ign, 
                                 httpd_objset *os) 
{
    register int x, y;
    pb_param *pp;
    char *tmp_path = NULL;

    for(x = 0; x < os->pos; x++) {
        if((pp = pblock_findkey(pb_key_ppath, os->obj[x]->name))) {
	    /* httpd uses shexp's, proxy regexp's */
            if(WILDPAT_CMP(ppath, pp->value)) {
	        /* If the path wasn't found try adding a '/' (anton) */
	        if (!tmp_path) {
		    /* 2 for '/' and '\0' */
#ifdef IRIX
		    int pplen = strlen(ppath);
		    tmp_path = (char *) MALLOC(pplen + 2);
		    memcpy(tmp_path, ppath, pplen);
		    tmp_path[pplen] = '/';
		    tmp_path[pplen + 1] = 0;
#else
		    tmp_path = (char *) MALLOC(strlen(ppath) + 2);
		    util_sprintf(tmp_path, "%s%c", ppath, '/');
#endif
		}

		if(WILDPAT_CMP(tmp_path, pp->value))
		    continue;
	    }

	    if(ign) {
	        for(y = 0; y < ign->pos; y++)
		    if(ign->obj[y] == os->obj[x])
		        break;
		if(y != ign->pos)
		    continue;
	    }
	    if (tmp_path) FREE(tmp_path);
	    return os->obj[x];
        }
    }
    if (tmp_path) FREE(tmp_path);
    return NULL;
}


/* ------------------------------ objset_dup ------------------------------ */

NSAPI_PUBLIC httpd_objset* objset_dup(const httpd_objset *src) 
{
    if (!src)
        return NULL;
    
    httpd_objset *nos = objset_create();
    if (!nos)
        return NULL;

    // copy the Init functions
    if (src->initfns) {
        int ninits;
        for (ninits = 0; src->initfns[ninits]; ninits++); // count the Inits
    
        nos->initfns = (pblock **) MALLOC((ninits + 1) * sizeof(pblock *));
        if (nos->initfns) {
            for (int i = 0; i < ninits; i++)
                nos->initfns[i] = pblock_dup(src->initfns[i]);
            nos->initfns[ninits] = NULL; // NULL termination required
        }
    }

    // make actual copies of all the objects
    for (int x = 0; x < src->pos; x++) {
        httpd_object *no = object_dup(src->obj[x]);
        if (!no) {
            objset_free(nos);
            return NULL;
        }
        objset_add_object(no, nos);
    }

    return nos;
}


/* ---------------------- objset_substitute_vs_vars ----------------------- */

PRStatus objset_substitute_vs_vars(const VirtualServer *vs, httpd_objset *os)
{
    // Substitute $variables in the object name pblock
    // XXX It'd be nice to do this at request time
    for (int x = 0; x < os->pos; x++) {
        httpd_object *obj = os->obj[x];
        for (int hi = 0; hi < obj->name->hsize; hi++) {
            for (pb_entry *p = obj->name->ht[hi]; p != NULL; p = p->next) {
                char *value = vs_substitute_vars(vs, p->param->value);
                if (!value)
                    return PR_FAILURE;
                FREE(p->param->value);
                p->param->value = value;
            }
        }
    }

    return PR_SUCCESS;
}


/* ------------------------- objset_interpolative ------------------------- */

PRStatus objset_interpolative(httpd_objset *os)
{
    for (int x = 0; x < os->pos; x++) {
        if (object_interpolative(os->obj[x]) != PR_SUCCESS)
            return PR_FAILURE;
    }

    return PR_SUCCESS;
}
