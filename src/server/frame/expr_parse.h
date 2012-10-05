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

#ifndef FRAME_EXPR_PARSE_H
#define FRAME_EXPR_PARSE_H

/*
 * expr_parse.h: NSAPI expression parsing
 * 
 * Chris Elving
 */

#include "tokenizer.h"

/*
 * expr_parse parses an expression from the passed set of tokens.  Returns
 * NULL on error; system_errmsg can be used to retrieve a localized description
 * of the error.
 */
Expression *expr_parse(const Token * const *tokens, int ntokens);

/*
 * expr_scan_inclusive parses an expression from the named source, up to and
 * including the first unquoted, unbracketed appearance of the passed end
 * string.  (The passed end string must either be NULL, indicating parsing
 * should continue until EOF is reached, or a valid token value such as an
 * identifier or operator.)  Returns NULL on error; system_errmsg can be used
 * to retrieve a localized description of the error.
 */
Expression *expr_scan_inclusive(TokenizerCharSource& source, const char *end = NULL);

/*
 * expr_scan_exclusive parses an expression from the named source, up to but
 * not including the first unquoted, unbracketed appearance of the passed end
 * string.  (The passed end string must either be NULL, indicating parsing
 * should continue until EOF is reached, or a valid token value such as an
 * identifier or operator.)  Returns NULL on error; system_errmsg can be used
 * to retrieve a localized description of the error.
 */
Expression *expr_scan_exclusive(TokenizerCharSource& source, const char *end = NULL);

/*
 * expr_is_operator returns PR_TRUE if the passed string s of length len is an
 * operator.
 */
PRBool expr_is_operator(const char *s, int len);

/*
 * expr_could_be_operator returns PR_TRUE if the first len bytes of the passed
 * string s match the first len bytes of one or more operators.
 */
PRBool expr_could_be_operator(const char *s, int len);

/*
 * expr_is_identifier returns PR_TRUE if the passed string s of length len
 * would be valid as an identifier.
 */
PRBool expr_is_identifier(const char *s, int len);

/*
 * expr_leading_identifier_char returns PR_TRUE if the passed character is
 * valid as the initial character of an identifier.
 */
PRBool expr_leading_identifier_char(char c);

/*
 * expr_nonleading_identifier_char returns PR_TRUE if the passed character is
 * valid as a non-initial character of an identifier.
 */
PRBool expr_nonleading_identifier_char(char c);

#endif /* FRAME_EXPR_PARSE_H */
