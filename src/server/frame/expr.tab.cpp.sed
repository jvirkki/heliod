#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2008 Sun Microsystems, Inc. All rights reserved.
#
# THE BSD LICENSE
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer. 
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
#
# Neither the name of the  nor the names of its contributors may be
# used to endorse or promote products derived from this software without 
# specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Remove nonportable #includes
/#include <inttypes.h>/d
/#include <values.h>/d

# Delete the YYSTYPE typedef and token #defines that are duplicated in expr.tab.h
/^typedef union/,/^$/d

# Rename yyparse, yylex, and yyerror, and provide each with a pointer to
# an instance-specific parsing context
s/int yyparse(void)/int expr_yy_parse(YYDATA *yydata)/
s/int yylex(.*/#define yylex() expr_yy_lex(yydata)/
s/void yyerror(.*/#define yyerror(msg) expr_yy_error(yydata, msg)/

# Remove global variable definitions
/^int yy/d
/^static int yy/d
/^int \*yy/d
/^static int \*yy/d
/^YYSTYPE /d
/^static YYSTYPE /d
/^extern /d

# Rewrite global variable references to use an instance-specific parsing context
s/\([^_a-zA-Z]\)\(yymaxdepth\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yyerrflag\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yystate\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yynerrs\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yychar\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yylval\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yyval\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yytmp\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yypv\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yyps\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yyv\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g
s/\([^_a-zA-Z]\)\(yys\)\([^_a-zA-Z0-9]\)/\1(yydata->\2)\3/g

# These #line directives make things harder to debug, not easier
/^# line/d

