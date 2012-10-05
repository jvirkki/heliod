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


typedef union
#ifdef __cplusplus
	ACLSTYPE
#endif
 {
	char	*string;
	int	ival;
} ACLSTYPE;
extern ACLSTYPE acllval;
# define ACL_ABSOLUTE_TOK 257
# define ACL_ACL_TOK 258
# define ACL_ALLOW_TOK 259
# define ACL_ALWAYS_TOK 260
# define ACL_AND_TOK 261
# define ACL_AT_TOK 262
# define ACL_AUTHENTICATE_TOK 263
# define ACL_CONTENT_TOK 264
# define ACL_DEFAULT_TOK 265
# define ACL_DENY_TOK 266
# define ACL_GROUP_TOK 267
# define ACL_IN_TOK 268
# define ACL_INHERIT_TOK 269
# define ACL_NOT_TOK 270
# define ACL_NULL_TOK 271
# define ACL_OR_TOK 272
# define ACL_QSTRING_TOK 273
# define ACL_READ_TOK 274
# define ACL_TERMINAL_TOK 275
# define ACL_VARIABLE_TOK 276
# define ACL_VERSION_TOK 277
# define ACL_WRITE_TOK 278
# define ACL_WITH_TOK 279
# define ACL_EQ_TOK 280
# define ACL_GE_TOK 281
# define ACL_GT_TOK 282
# define ACL_LE_TOK 283
# define ACL_LT_TOK 284
# define ACL_NE_TOK 285
