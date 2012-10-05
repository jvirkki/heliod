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

#ifndef FRAME_MODEL_H
#define FRAME_MODEL_H

/*
 * model.h: String and pblock interpolation
 * 
 * Chris Elving
 */

/*
 * ModelString is a model from which a synthetic string may be constructed.
 */
typedef struct ModelString ModelString;

/*
 * ModelPblock is a model from which a synthetic pblock may be constructed.
 */
typedef struct ModelPblock ModelPblock;

PR_BEGIN_EXTERN_C

/*
 * model_unescape_interpolative constructs a string in which "\$" escape
 * sequences have been changed to "$$" and all other \escapes have been
 * unescaped.  The caller should free the returned string using FREE().
 * Returns NULL on error; system_errmsg can be used to retrieve a localized
 * description of the error.
 */
NSAPI_PUBLIC char *INTmodel_unescape_interpolative(const char *s);

/*
 * model_unescape_noninterpolative constructs a string in which all \escapes
 * have been unescaped.  The caller should free the returned string using
 * FREE().  Returns NULL on error; system_errmsg can be used to retrieve a
 * localized description of the error.
 */
NSAPI_PUBLIC char *INTmodel_unescape_noninterpolative(const char *s);

/*
 * model_fragment_scan returns the length of the of the non-\escaped
 * interpolative string fragment that begins at f.  A fragment is a span of
 * characters that consists entirely of either a) a single $fragment (e.g. a
 * single $variable reference), b) a single escaped '$' (that is, the "$$"
 * sequence), or c) nonescaped text.  Returns 0 if p points to an empty string.
 * Returns -1 on error; system_errmsg can be used to retrieve a localized
 * description of the error.
 */
NSAPI_PUBLIC int INTmodel_fragment_scan(const char *f);

/*
 * model_fragment_is_invariant indicates whether the fragment that begins at f
 * consists of either a single escaped '$' (that is, the "$$" sequence) or
 * nonescaped text.  If the it does, *ptext and *plen are set to point to the
 * text.
 */
NSAPI_PUBLIC PRBool INTmodel_fragment_is_invariant(const char *f, const char **ptext, int *plen);

/*
 * model_fragment_is_var_ref indicates whether the fragment that begins at f is
 * a reference to a subscriptless (that is, non-map) $variable.  If the
 * fragments is a subscriptless $variable reference, *pname and and *plen are
 * set to point to the variable name.
 */
NSAPI_PUBLIC PRBool INTmodel_fragment_is_var_ref(const char *f, const char **pname, int *plen);

/*
 * model_str_create constructs a string model from a non-\escaped interpolative
 * string.  Non-\escaped interpolative strings may contain $fragments but do
 * not contain \escapes.  Returns NULL on error; system_errmsg can be used to
 * retrieve a localized description of the error.
 */
NSAPI_PUBLIC ModelString *INTmodel_str_create(const char *s);

/*
 * model_str_dup creates a copy of a string model.
 */
NSAPI_PUBLIC ModelString *INTmodel_str_dup(const ModelString *model);

/*
 * model_str_free destroys a string model.
 */
NSAPI_PUBLIC void INTmodel_str_free(ModelString *model);

/*
 * model_str_interpolate constructs a synthetic string in which $fragments have
 * been interpolated and "$$" sequences have been unescaped.  On return, *pp
 * and *plen will point to a nul-terminated string or will be NULL and 0.
 * Returns 0 if no errors were encountered.  Returns -1 if any errors were
 * encountered; system_errmsg can be used to a retrieve a localized description
 * of the error.  Note that a partially interpolated string may be returned
 * through *pp and *plen even on error.
 */
NSAPI_PUBLIC int INTmodel_str_interpolate(const ModelString *model, Session *sn, Request *rq, const char **pp, int *plen);

/*
 * model_pb_create constructs a pblock model from a pblock whose values may
 * contain $fragments.
 */
NSAPI_PUBLIC ModelPblock *INTmodel_pb_create(const pblock *pb);

/*
 * model_pb_dup creates a copy of a pblock model.
 */
NSAPI_PUBLIC ModelPblock *INTmodel_pb_dup(const ModelPblock *model);

/*
 * model_pb_free destroys a pblock model.
 */
NSAPI_PUBLIC void INTmodel_pb_free(ModelPblock *model);

/*
 * model_pb_is_noninterpolative indicates whether a pblock model consists
 * entirely of nonescaped, noninterpolative text.  That is, whether
 * interpolation of the model will always result in a pblock identical to the
 * pblock from which the model was constructed.
 */
NSAPI_PUBLIC PRBool INTmodel_pb_is_noninterpolative(const ModelPblock *model);

/*
 * model_pb_interpolate constructs a synthetic pblock in which $fragments have
 * been interpolated and "$$" sequences have been unescaped..  Returns a pblock
 * allocated from rq's pool on success.  Returns NULL on error; system_errmsg
 * can be used to a retrieve a localized description of the error.
 */
NSAPI_PUBLIC pblock *INTmodel_pb_interpolate(const ModelPblock *model, Session *sn, Request *rq);

PR_END_EXTERN_C

#define model_unescape_interpolative INTmodel_unescape_interpolative
#define model_unescape_noninterpolative INTmodel_unescape_noninterpolative
#define model_fragment_scan INTmodel_fragment_scan
#define model_fragment_is_invariant INTmodel_fragment_is_invariant
#define model_fragment_is_var_ref INTmodel_fragment_is_var_ref
#define model_str_create INTmodel_str_create
#define model_str_dup INTmodel_str_dup
#define model_str_free INTmodel_str_free
#define model_str_interpolate INTmodel_str_interpolate
#define model_pb_create INTmodel_pb_create
#define model_pb_dup INTmodel_pb_dup
#define model_pb_free INTmodel_pb_free
#define model_pb_is_noninterpolative INTmodel_pb_is_noninterpolative
#define model_pb_interpolate INTmodel_pb_interpolate

#endif /* FRAME_MODEL_H */
