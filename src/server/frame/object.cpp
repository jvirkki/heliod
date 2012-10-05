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
 * object.c: Handles individual HTTP objects, or documents
 * 
 * See object.h for documentation
 * 
 * Rob McCool
 * 
 */

#include "netsite.h"
#include "support/SimpleHash.h"
#include "base/pool.h"
#include "frame/object.h"
#include "frame/httpdir.h"
#include "frame/func.h"
#include "frame/log.h"
#include "frame/expr.h"
#include "frame/dbtframe.h"
#include "httpdaemon/statsmanager.h"


/* 
 * Allocation quantum for various arrays.
 */
#define OBJECT_ARRAY_INCSIZE 8

/*
 * ConditionResult records the result of evaluating a Condition.
 */
typedef struct ConditionResult ConditionResult;
struct ConditionResult {
    const Condition *cond;
    int rv;
    RequestBackrefData backrefs;
};

/*
 * RequestConditionData contains a ConditionResult for each Condition that has
 * been evaluated during a particular Request.
 */
typedef struct RequestConditionData RequestConditionData;
struct RequestConditionData {
    int nr;
    ConditionResult *result;
};

/*
 * object_request_cond_slot is a request_get_data/request_set_data slot that
 * holds a RequestConditionData *.
 */
static int object_request_cond_slot = request_alloc_slot(NULL);


/* --------------------------------- grow --------------------------------- */

template <class T>
inline T *grow(pool_handle_t *pool, T*& p, int& n)
{
    if ((n % OBJECT_ARRAY_INCSIZE) == 0) {
        T *np = (T *) pool_realloc(pool, p, (n + OBJECT_ARRAY_INCSIZE) * sizeof(T));
        if (!np)
            return NULL;
        p = np;
    }

    T *r = p + n;

    n++;

    return r;
}

static inline directive *grow_inst(dtable *dt)
{
    return grow<directive>(system_pool(), dt->inst, dt->ni);
}

static inline ConditionResult *grow_result(pool_handle_t *pool, RequestConditionData *rc)
{
    return grow<ConditionResult>(pool, rc->result, rc->nr);
}


/* -------------------------------- append -------------------------------- */

template <class T>
inline PRStatus append(pool_handle_t *pool, T*& p, int& n, const T& e)
{
    T *pe = grow<T>(pool, p, n);
    if (!pe)
        return PR_FAILURE;

    *pe = e;

    return PR_SUCCESS;
}

static inline PRStatus append_pb(httpd_object *obj, pblock *pb)
{
    return append<pblock *>(system_pool(), obj->pb, obj->np, pb);
}

static inline PRStatus append_model(httpd_object *obj, ModelPblock *model)
{
    return append<ModelPblock *>(system_pool(), obj->model, obj->nm, model);
}

static inline PRStatus append_cond(httpd_object *obj, Condition *cond)
{
    return append<Condition *>(system_pool(), obj->cond, obj->nc, cond);
}


/* ------------------------------- contains ------------------------------- */

template <class T>
inline PRBool contains(T **p, int n, const T *e)
{
    for (int i = 0; i < n; i++) {
        if (p[i] == e)
            return PR_TRUE;
    }

    return PR_FALSE;
}

static inline PRBool contains_pb(httpd_object *obj, pblock *pb)
{
    return contains<pblock>(obj->pb, obj->np, pb);
}

static inline PRBool contains_model(httpd_object *obj, const ModelPblock *model)
{
    return contains<ModelPblock>(obj->model, obj->nm, model);
}


/* ---------------------------- dtable_create ----------------------------- */

static dtable *dtable_create(int nd) 
{
    dtable *ndt = (dtable *) MALLOC(nd * sizeof(dtable));
    if (ndt) {
        for (int dc = 0; dc < nd; dc++) {
            ndt[dc].ni = 0;
            ndt[dc].inst = NULL;
        }
    }

    return ndt;
}


/* ----------------------------- dtable_free ------------------------------ */

static void dtable_free(int nd, dtable *dt)
{
    for (int dc = 0; dc < nd; dc++) {
        for (int di = 0; di < dt[dc].ni; di++) {
            pblock_free(dt[dc].inst[di].param.pb);
            model_pb_free(dt[dc].inst[di].param.model);
        }
        FREE(dt[dc].inst);
    }
    FREE(dt);
}


/* ----------------------- dtable_append_directive ------------------------ */

static directive *dtable_append_directive(dtable *dt,
                                          pblock *param,
                                          pblock *client,
                                          const Condition *cond)
{
    directive *inst = grow_inst(dt);
    if (!inst)
        return NULL;

    FuncStruct *f = NULL;
    if (const char *fn = pblock_findkeyval(pb_key_fn, param))
        f = func_find_str(fn);

    inst->param.pb = param; // directive now owns param
    inst->param.model = NULL;
    inst->client.pb = client; // client is owned by object
    inst->client.model = NULL;
    inst->f = f;
    inst->bucket = STATS_PROFILE_DEFAULT;
    inst->cond = cond;

    return inst;
}


/* ---------------------------- object_create ----------------------------- */

NSAPI_PUBLIC httpd_object *object_create(int nd, pblock *name) 
{
    httpd_object *no = (httpd_object *) MALLOC(sizeof(httpd_object));
    if (no) {
        no->name = name; // We assume ownership of name
        no->nd = nd;
        no->dt = dtable_create(nd);
        no->np = 0; 
        no->pb = NULL;
        no->nm = 0; 
        no->model = NULL;
        no->nc = 0; 
        no->cond = NULL;
    }

    return no;
}


/* ----------------------------- object_free ------------------------------ */

NSAPI_PUBLIC void object_free(httpd_object *obj) 
{
    pblock_free(obj->name);

    dtable_free(obj->nd, obj->dt);

    for (int pi = 0; pi < obj->np; pi++)
        pblock_free(obj->pb[pi]);
    FREE(obj->pb);

    for (int mi = 0; mi < obj->nm; mi++)
        model_pb_free(obj->model[mi]);
    FREE(obj->model);

    for (int ci = 0; ci < obj->nc; ci++) {
        expr_free(obj->cond[ci]->expr);
        FREE(obj->cond[ci]);
    }
    FREE(obj->cond);

    FREE(obj);
}


/* ------------------------- object_add_directive ------------------------- */

NSAPI_PUBLIC void object_add_directive(int dc,
                                       pblock *p,
                                       pblock *c,
                                       httpd_object *obj)
{
    PR_ASSERT(p);
    PR_ASSERT(dc >= 0 && dc < obj->nd);

    if (c && !contains_pb(obj, c))
        append_pb(obj, c);

    dtable_append_directive(&obj->dt[dc], p, c, NULL);
}


/* ----------------------- object_append_directive ------------------------ */

NSAPI_PUBLIC PRStatus object_append_directive(int dc,
                                              pblock *param,
                                              pblock *client,
                                              Condition *cond,
                                              httpd_object *obj)
{
    PR_ASSERT(param);
    PR_ASSERT(!client || contains_pb(obj, client));
    PR_ASSERT(!cond || obj->cond[cond->i] == cond);
    PR_ASSERT(dc >= 0 && dc < obj->nd);

    directive *inst = dtable_append_directive(&obj->dt[dc], param, client, cond);
    if (!inst)
        return PR_FAILURE;

    return PR_SUCCESS;
}


/* ------------------------- object_add_condition ------------------------- */

NSAPI_PUBLIC Condition *object_add_condition(Expression *expr,
                                             const Condition *inside,
                                             const Condition *follows,
                                             httpd_object *obj)
{
    Condition *cond = (Condition *) MALLOC(sizeof(Condition));
    if (!cond)
        return NULL;

    append_cond(obj, cond);

    cond->i = obj->nc - 1;
    cond->expr = expr;
    cond->inside = inside;
    cond->follows = follows;

    return cond;
}


/* -------------------------- object_add_client --------------------------- */

PRStatus object_add_client(pblock *client, httpd_object *obj)
{
    PR_ASSERT(!contains_pb(obj, client));
    return append_pb(obj, client);
}


/* ------------------------------ ObjectMap ------------------------------- */

/*
 * ObjectMap is an abstract base class useful for mapping pblock and condition
 * pointers from one httpd_object to another.
 */
template <class T>
class ObjectMap {
public:
    ObjectMap(httpd_object *obj, int n);
    T *lookup(const T *o);

protected:
    ObjectMap(const ObjectMap&);
    ObjectMap& operator=(const ObjectMap&);

    virtual T *dup(const T *o) = 0;

    httpd_object *obj;
    SimpleIntHash hash;
};

template <class T>
ObjectMap<T>::ObjectMap(httpd_object *obj, int n)
: obj(obj), hash(n > 1 ? n : 1)
{ }

template <class T>
T *ObjectMap<T>::lookup(const T *o)
{
    T *n = NULL;

    if (o) {
        n = (T *) hash.lookup((void *) o);
        if (!n) {
            n = dup(o);
            hash.insert((void *) o, n);
            hash.insert((void *) n, n);
        }
    }

    return n;
}


/* -------------------------------- PbMap --------------------------------- */

class PbMap : public ObjectMap<pblock> {
public:
    PbMap(httpd_object *obj) : ObjectMap<pblock>(obj, obj->np) { }

protected:
    pblock *dup(const pblock *op)
    {
        pblock *np = pblock_dup(op);
        if (np)
            append_pb(obj, np);
        return np;
    }
};


/* ------------------------------- CondMap -------------------------------- */

class CondMap : public ObjectMap<Condition> {
public:
    CondMap(httpd_object *obj) : ObjectMap<Condition>(obj, obj->nc) { }

protected:
    Condition *dup(const Condition *oc)
    {
        Expression *ne = expr_dup(oc->expr);
        const Condition *inside = lookup(oc->inside);
        const Condition *follows = lookup(oc->follows);

        Condition *nc = object_add_condition(ne, inside, follows, obj);
        if (!nc)
            expr_free(ne);

        return nc;
    }
};


/* ----------------------------- object_copy ------------------------------ */

static PRStatus object_copy(httpd_object *dst, const httpd_object *src)
{
    PR_ASSERT(dst->np == 0);
    PR_ASSERT(dst->nm == 0);
    PR_ASSERT(dst->nc == 0);

    PRBool interpolative = PR_FALSE;
    PbMap pbmap(dst);
    CondMap condmap(dst);

    // Copy the src object directive-by-directive
    for (int dc = 0; dc < src->nd; dc++) {
        for (int di = 0; di < src->dt[dc].ni; di++) {
            directive *od = &src->dt[dc].inst[di];

            if (od->param.model || od->client.model)
                interpolative = PR_TRUE;

            pblock *ncp = NULL;
            if (od->client.pb) {
                ncp = pbmap.lookup(od->client.pb);
                if (!ncp)
                    return PR_FAILURE;
            }

            Condition *nc = NULL;
            if (od->cond) {
                nc = condmap.lookup(od->cond);
                if (!nc)
                    return PR_FAILURE;
            }

            pblock *npp = pblock_dup(od->param.pb);
            if (!npp)
                return PR_FAILURE;

            directive *nd = dtable_append_directive(&dst->dt[dc], npp, ncp, nc);
            if (!nd) {
                pblock_free(npp);
                return PR_FAILURE;
            }
        }
    }

    if (interpolative) {
        if (object_interpolative(dst) != PR_SUCCESS)
            return PR_FAILURE;
    }

    PR_ASSERT(dst->np == src->np);
    PR_ASSERT(dst->nm == src->nm);
    PR_ASSERT(dst->nc == src->nc);

    return PR_SUCCESS;
}


/* ------------------------------ object_dup ------------------------------ */

httpd_object *object_dup(const httpd_object *src)
{
    httpd_object *no = object_create(src->nd, pblock_dup(src->name));
    if (no) {
        if (object_copy(no, src) != PR_SUCCESS) {
            object_free(no);
            no = NULL;
        }
    }

    return no;
}        


/* ------------------------- maybe_interpolative -------------------------- */

static inline PRBool maybe_interpolative(pblock *pb)
{
    for (int hi = 0; hi < pb->hsize; hi++) {
        for (pb_entry *p = pb->ht[hi]; p != NULL; p = p->next) {
            if (strchr(p->param->value, '$'))
                return PR_TRUE;
        }
    }

    return PR_FALSE;
}


/* ------------------------- object_interpolative ------------------------- */

PRStatus object_interpolative(httpd_object *obj)
{
    PRStatus rv = PR_SUCCESS;

    int dc;

    // Remove references to old client pblock models
    for (dc = 0; dc < obj->nd; dc++) {
        for (int di = 0; di < obj->dt[dc].ni; di++)
            obj->dt[dc].inst[di].client.model = NULL;
    }

    // Destroy existing pblock models for client pblocks
    for (int mi = 0; mi < obj->nm; mi++)
        model_pb_free(obj->model[mi]);
    FREE(obj->model);
    obj->nm = 0;
    obj->model = NULL;

    // Create new pblock models for client pblocks that contain $fragments
    SimpleIntHash clientmap(obj->np + 1);
    for (int pi = 0; pi < obj->np; pi++) {
        ModelPblock *mc = model_pb_create(obj->pb[pi]);
        if (mc) {
            if (model_pb_is_noninterpolative(mc)) {
                model_pb_free(mc);
            } else {
                clientmap.insert(obj->pb[pi], mc);
                append_model(obj, mc);
            }
        } else {
            rv = PR_FAILURE;
        }
    }

    // Assign pblock models to directives
    for (dc = 0; dc < obj->nd; dc++) {
        for (int di = 0; di < obj->dt[dc].ni; di++) {
            directive *inst = &obj->dt[dc].inst[di];

            // Lookup any previously created client pblock model
            const ModelPblock *mc = (const ModelPblock *) clientmap.lookup(inst->client.pb);
            inst->client.model = mc;

            // Create a param pblock model if param contains $fragments
            ModelPblock *mp = NULL;
            if (maybe_interpolative(inst->param.pb)) {
                mp = model_pb_create(inst->param.pb);
                if (mp) {
                    if (model_pb_is_noninterpolative(mp)) {
                        model_pb_free(mp);
                        mp = NULL;
                    }
                } else {
                    rv = PR_FAILURE;
                }
            }
            if (inst->param.model)
                model_pb_free(inst->param.model);
            inst->param.model = mp;
        }
    }

    return rv;
}


/* --------------------------- client_evaluate ---------------------------- */

static int client_evaluate(pblock *client, Session *sn, Request *rq)
{
    // If a condition SAF was explicitly specified...
    const char *fn = pblock_findkeyval(pb_key_fn, client);
    if (fn) {
        // Call the user's condition SAF
        return func_exec(client, sn, rq);
    }

    // Get a pointer to the builtin cond-match-variable SAF
    static FuncStruct *fs_cond_match_variable = NULL;
    if (!fs_cond_match_variable)
        fs_cond_match_variable = func_find_str("cond-match-variable");

    // Call the builtin cond-match-variable SAF
    return func_exec_str(fs_cond_match_variable, client, sn, rq);
}


/* ----------------------- cond_evaluate_recursive ------------------------ */

static int cond_evaluate_recursive(const Condition *cond, Session *sn, Request *rq, RequestConditionData *rc, PRBool terminal, int depth)
{
    int rv;

    if (!terminal) {
        // Recurse to evaluate preceding <If>/<ElseIf>
        if (cond->follows) {
            rv = cond_evaluate_recursive(cond->follows, sn, rq, rc, PR_FALSE, depth + 1);
            if (rv == REQ_PROCEED)
                return REQ_PROCEED;
        }
    }

    // Check for a cached evaluation result
    for (int ri = rc->nr - 1; ri >= 0; ri--) {
        if (rc->result[ri].cond == cond) {
            if (depth == 0)
                expr_set_backrefs(&rc->result[ri].backrefs, sn, rq);
            return rc->result[ri].rv;
        }
    }

    if (terminal) {
        // Recurse to evaluate containing condition
        if (cond->inside) {
            rv = cond_evaluate_recursive(cond->inside, sn, rq, rc, PR_TRUE, depth + 1);
            if (rv != REQ_PROCEED)
                return REQ_NOACTION;
        }

        // Recurse to evaluate preceding <If>/<ElseIf>
        if (cond->follows) {
            rv = cond_evaluate_recursive(cond->follows, sn, rq, rc, PR_FALSE, depth + 1);
            if (rv == REQ_PROCEED)
                return REQ_NOACTION;
        }
    }

    // Evaluate the expression
    rv = expr_evaluate(cond->expr, sn, rq);
    if (rv == REQ_ABORTED) {
        const char *func;
        if (cond->follows) {
            func = "<ElseIf>";
        } else {
            func = "<If>";
        }
        // XXX log obj.conf filename and line number
        log_error(LOG_MISCONFIG, func, sn, rq,
                  XP_GetAdminStr(DBT_confExprEvalErrorX),
                  system_errmsg());
    }

    // Cache the evaluation result
    ConditionResult *cr = grow_result(sn->pool, rc);
    if (cr) {
        cr->cond = cond;
        cr->rv = rv;
        expr_get_backrefs(&cr->backrefs, sn, rq);
    }
    
    return rv;
}


/* ---------------------------- cond_evaluate ----------------------------- */

static inline int cond_evaluate(const Condition *cond, Session *sn, Request *rq)
{
    RequestConditionData *rc;

    rc = (RequestConditionData *) request_get_data(rq, object_request_cond_slot);
    if (!rc) {
        rc = (RequestConditionData *) pool_malloc(sn->pool, sizeof(RequestConditionData));
        if (!rc)
            return REQ_ABORTED;

        rc->nr = 0;
        rc->result = NULL;

        request_set_data(rq, object_request_cond_slot, rc);
    }

    return cond_evaluate_recursive(cond, sn, rq, rc, PR_TRUE, 0);
}


/* -------------------------- object_interpolate -------------------------- */

pblock *object_interpolate(const ModelPblock *model, directive *inst, Session *sn, Request *rq)
{
    pblock *interpolated = model_pb_interpolate(model, sn, rq);
    if (!interpolated) {
        FuncStruct *f = func_resolve(inst->param.pb, sn, rq);
        if (f) {
            log_error(LOG_FAILURE, XP_GetAdminStr(DBT_handleProcessed_),
                      sn, rq, XP_GetAdminStr(DBT_FnXInterpolationErrorY),
                      f->name, system_errmsg());
        }
    }

    return interpolated;
}


/* ----------------------------- object_check ----------------------------- */

int object_check(directive *inst, Session *sn, Request *rq)
{
    // Evaluate any <If>/<ElseIf>/<Else> expressions
    if (inst->cond) {
        rq->request_is_cacheable = 0;

        int rv = cond_evaluate(inst->cond, sn, rq);
        if (rv != REQ_PROCEED)
            return rv;
    }

    // Evaluate any <Client> tag
    if (inst->client.pb) {
        rq->request_is_cacheable = 0;

        // Interpolate <Client> tag parameters as necessary
        pblock *client;
        if (inst->client.model) {
            client = object_interpolate(inst->client.model, inst, sn, rq);
            if (!client)
                return REQ_ABORTED;
        } else {
            client = inst->client.pb;
        }

        int rv = client_evaluate(client, sn, rq);
        if (rv != REQ_PROCEED)
            return rv;
    }

    return REQ_PROCEED;
}


/* ---------------------------- object_execute ---------------------------- */

NSAPI_PUBLIC int object_execute(directive *inst, Session *sn, Request *rq)
{
    // Evaluate conditions (i.e. <Client>/<If>/<ElseIf>/<Else> tags)
    int rv = object_check(inst, sn, rq);
    if (rv != REQ_PROCEED)
        return rv;

    // Interpolate parameters as necessary
    pblock *param;
    if (inst->param.model) {
        rq->request_is_cacheable = 0;

        param = object_interpolate(inst->param.model, inst, sn, rq);
        if (!param)
            return REQ_ABORTED;
    } else {
        param = inst->param.pb;
    }

    return func_exec_directive(inst, param, sn, rq);
}
