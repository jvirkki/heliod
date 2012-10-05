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

#ifndef FRAME_OBJECT_H
#define FRAME_OBJECT_H

/*
 * object.h: Handle httpd objects
 * 
 * Manages information about a document from config. files. Called mainly
 * by objset.c.
 * 
 * This module does not assume anything about the directives being parsed.
 * That is handled by objset.c.
 * 
 * This module requires the pblock module from the base library.
 * 
 * Rob McCool
 * 
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

#ifndef BASE_PBLOCK_H
#include "../base/pblock.h"
#endif /* !BASE_PBLOCK_H */

#ifndef FRAME_EXPR_H
#include "../frame/expr.h"
#endif /* !FRAME_EXPR_H */

#ifndef FRAME_MODEL_H
#include "../frame/model.h"
#endif /* !FRAME_MODEL_H */

/* ---------------------- NSAPI Function Prototypes ----------------------- */

PR_BEGIN_EXTERN_C

/*
 * INTobject_create will create a new object and return a pointer to it.
 * It will allocate space for nd directive types and set name accordingly.
 */
NSAPI_PUBLIC httpd_object *INTobject_create(int nd, pblock *name);

/*
 * INTobject_free will free an object and any data associated with it.
 */
NSAPI_PUBLIC void INTobject_free(httpd_object *obj);

/*
 * INTobject_add_directive will add a new directive to the dtable for the
 * directive class at position dc.
 *
 * The object assumes ownership of the passed pblocks; the pblocks will be
 * freed when the object is freed.
 */
NSAPI_PUBLIC void INTobject_add_directive(int dc, pblock *p, pblock *c, 
                                          httpd_object *obj);

/*
 * Executes the directive specified by inst within the context of the
 * given session and request structures. Returns what the executed function
 * returned (one of the REQ_* codes defined in req.h).
 */
NSAPI_PUBLIC int INTobject_execute(directive *inst, Session *sn, Request *rq);

PR_END_EXTERN_C

#define object_create INTobject_create
#define object_free INTobject_free
#define object_add_directive INTobject_add_directive
#define object_execute INTobject_execute

/* ---------------------------- Internal Stuff ---------------------------- */

/*
 * Hierarchy of httpd_object
 *
 * An object contains dtables. 
 * 
 * Each dtable is a table of directives that were entered of a certain type.
 * There is one dtable for each unique type of directive.
 *
 * Each dtable contains an array of directives, each of which is equivalent
 * to one directive that occurred in a config. file.
 *
 * It is up to the caller to determine how many dtables will be allocated
 * and to keep track of which of their directive types maps to which dtable
 * number.
 */

/*
 * Condition describes a condition (e.g. <If>/<ElseIf>/<Else> expression and
 * any associated preconditions) that must evaluate to REQ_PROCEED before
 * certain directive(s) are executed.
 */
typedef struct Condition Condition;
struct Condition {
    int i; /* index into httpd_object's cond[] array */
    Expression *expr; /* root of expression tree */
    const Condition *inside; /* parent <If>/<ElseIf>/Else> condition or NULL */
    const Condition *follows; /* preceding <If>/<ElseIf> condition or NULL */
};

/*
 * directive is a structure containing the protection and parameters to an
 * instance of a directive within an httpd_object.
 *
 * param is the parameters, client and cond are the protection.
 *
 * f is a pointer to the FuncStruct for the actual function identified by
 * name in the "fn" variable of the param pblock.
 */
struct directive {
    struct {
        pblock *pb; /* owned by directive */
        ModelPblock *model; /* owned by directive */
    } param;

    struct {
        pblock *pb; /* owned by httpd_object */
        const ModelPblock *model; /* owned by httpd_object */
    } client;

    const Condition *cond; /* owned by httpd_object */

    FuncStruct *f;

    PRInt32 bucket; /* perf stats bucket index (0 catches unbucketed) */
};

/*
 * dtable is a structure for creating tables of directives
 */
struct dtable {
    int ni;
    directive *inst;
};

/*
 * The httpd_object structure.
 *
 * The name pblock contains the names for this object, such as its physical
 * location or identifier.
 *
 * dt is an array of directive tables.
 *
 * The pblocks array contains all the pblocks owned by this object but not
 * owned by a particular directive (i.e. client tag pblocks). The models array
 * contains all the ModelPblocks owned by this object but not owned by a
 * particular directive (i.e. client tag pblock models). The conds array
 * contains all the conditions (and thus all the Expressions) owned by this
 * object.
 */
struct httpd_object {
    pblock *name;

    int nd;
    dtable *dt;

    int np;
    pblock **pb;

    int nm;
    ModelPblock **model;

    int nc;
    Condition **cond;
};

/*
 * object_dup creates a copy of an existing object.
 */
httpd_object *object_dup(const httpd_object *src);

/*
 * object_add_client adds a client tag pblock to an object. The client tag
 * pblock may subsequently be passed to object_append_directive when adding
 * directives to the object.
 *
 * The object assumes ownership of the passed client tag pblock; the client tag
 * pblock will be freed when the object is freed.
  */
PRStatus object_add_client(pblock *client, httpd_object *obj);

/*
 * object_add_condition adds a condition to an object. The condition may
 * subsequently be specified when adding directives to the object. The object
 * assumes ownership of the passed Expression; the expression will be freed
 * when the object is freed.
 */
NSAPI_PUBLIC Condition *object_add_condition(Expression *expr,
                                             const Condition *inside,
                                             const Condition *follows,
                                             httpd_object *obj);

/*
 * object_append_directive appends a new directive to the dtable for the
 * directive class at position dc. object_append_directive mirrors the NSAPI
 * object_add_directive function but adds support for Conditions (i.e. <If>/
 * <ElseIf>/<Else> tags).
 *
 * The object assumes ownership of the passed param pblock; the param pblock
 * will be freed when the object is freed. If a client tag pblock is passed, it
 * must have been previously added to the object using object_add_client. If a
 * condition is passed, it must have been previously added to the object using
 * object_add_condition.
 */
NSAPI_PUBLIC PRStatus object_append_directive(int dc,
                                              pblock *param,
                                              pblock *client,
                                              Condition *condition,
                                              httpd_object *obj);

/*
 * object_interpolative marks an object as interpolative, constructing pblock
 * models for any pblocks that contain $fragments.
 */
PRStatus object_interpolative(httpd_object *obj);

/*
 * object_check evalutes any client tags and expressions associated with a
 * directive. Returns REQ_PROCEED if the directive should be executed.
 */
int object_check(directive *d, Session *sn, Request *rq);

/*
 * object_interpolate interpolates a pblock model associated with a directive.
 * Returns NULL and logs an error on error.
 */
pblock *object_interpolate(const ModelPblock *model, directive *inst, Session *sn, Request *rq);

#endif /* FRAME_OBJECT_H */
