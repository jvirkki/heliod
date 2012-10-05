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
 * regex_entry.cpp
 *
 * An entry in the list of regular expressions (RegexList).  It contains the
 *    regular expression and its replacement.
 */   
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <string.h>
#include "nspr.h"
#include "regex_string.h"
#include "regex_entry.h"

RegexEntry::RegexEntry(const char *exp, const char *rep)
{
	int status;
	fail = 0;
	expression = NULL;
	replacement = NULL;
	re = NULL;
	extra = NULL;

	if (exp != 0) {
		expression = new char[strlen(exp) + 1];
		strcpy (expression, exp);
                if (!simplify(expression)) {
                    fail = -1;
                    return;
                }

		const char *error;
		int erroroffset;
		re = pcre_compile(expression, PCRE_DOTALL, &error, &erroroffset, NULL);
		if (!re) {
			cerr << "Invalid regular expression: " << exp << " (" << error << ")\n";
			fail = 1;
			return;
		}

		extra = pcre_study(re, 0, &error);

		if (rep != 0) {
			replacement = new char[strlen(rep) + 1];
			strcpy(replacement, rep);
			if (!simplify(replacement)) {
				cerr << "Invalid string: " << rep << "\n";
				fail = 1;
				return;
			}
		}
		else
			replacement = 0;
	}
	else {
		cerr << "No regular expression specified\n";
		fail = 1;
	}
}

RegexEntry::~RegexEntry() 
{
	if (expression)
	   delete [] expression;
	if (replacement)
	   delete [] replacement;
	if (re)
	   pcre_free(re);
	if (extra)
	   pcre_free(extra);
}

int RegexEntry::simplify(char *cp) 
{
	char *curr = cp;

	while (*cp != '\0') {
		if (*cp == '\\') {
			if (*(cp+1) == 'n')
				*(curr)++ = '\n';
			else if (*(cp+1) == 't')
				*(curr)++ = '\t';
			else if (*(cp+1) == 'r')
				*(curr)++ = '\r';
			else if (*(cp+1) != '\0')
				*(curr)++ = *(cp+1);
			else 
				 return (0);
			cp += 2;
			continue;
		}
		*curr++ = *cp++;
	}
	*curr = *cp;
	return (1);
}

RegexEntry::operator void *()
{
	return (fail ? 0 : this);
}

PRBool RegexEntry::match(const char *subject)
{
	int rv = pcre_exec(re, extra, subject, strlen(subject), 0, 0, NULL, 0);
	return (rv >= 0);
}

PRBool RegexEntry::replace(const char *subject, RegexString& result)
{
	int len = strlen(subject);

	int ovector[30];
	int rv = pcre_exec(re, extra, subject, len, 0, 0, ovector, 30);
	if (rv < 1)
		return PR_FALSE;

	result.add(subject, ovector[0]);
	if (replacement)
		result.add(replacement);
	result.add(subject + ovector[1], len - ovector[1]);

	return PR_TRUE;
}

void RegexEntry::dump()
{
	if (expression != 0)
		cerr << expression;
	cerr << ":";
	if (replacement != 0)
		cerr << replacement;
	cerr << "\n";
}
