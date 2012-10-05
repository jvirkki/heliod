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
 * regex_scrubber.cpp
 *
 * This object will parse a file of regular expression/replacement strings
 *   and use these strings to cleanup a HTTP response.
 */

#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <fstream>
#include <iostream>
using namespace std;
#else
#include <fstream.h>
#include <iostream.h>
#endif
#ifdef XP_WIN32
#include <strstrea.h>
#else
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <strstream>
#else
#include <strstream.h>
#endif
#endif
#include <string.h>
#include <stdlib.h>
#include "nspr.h"
#include "regex_string.h"
#include "regex_entry.h"
#include "regex_list.h"
#include "regex_scrubber.h"
#include <stdio.h>

/*
 * Create an empty list and add to it manually.
 */
RegexScrubber::RegexScrubber()
{
	filename = NULL;
	fail = 0;
}


void RegexScrubber :: initialize(istream& astream)
{
    char line[512];
    memset(line, 0, sizeof(line));

    while (astream.getline(line, sizeof(line), '\n'))
    {
        int a = strlen(line);
	for (int i = 0; i<2; i++)
           if (a && ('\n' == line[a-1] || '\r' == line[a-1] ) )
   	   {
              line[a-1] = '\0'; // work around a bug in Microsoft iostream library ...
	      a--;
	   }
        parse_line(line);
    }
};

/* 
 * Create a list from the entries in a memory buffer
 */

RegexScrubber::RegexScrubber(char *stringbuf, int stringsize)
{
    filename = NULL;
    fail = 0;
#if (defined(__GNUC__) && (__GNUC__ > 2))
    using namespace std;
#endif
    istrstream buf (stringbuf);
    initialize(buf);
};

/* 
 * Create a list from the entries in a file.
 */
RegexScrubber::RegexScrubber(const char *file)
{
    	filename = NULL;
	fail = 0;

        ifstream fin (file);

	if (!fin)
        {
	    //cerr << "Unable to open regular expression file: " << file << "\n";
	    fail = 1;
	    return;
	};

	filename = strdup(file);

        initialize(fin);

	fin.close();
}

RegexScrubber::~RegexScrubber()
{
    if (filename)
        free((void*)filename);
}

char* RegexScrubber::skipws(char *cp)
{
	while (*cp == ' ' || *cp == '\t')
		cp++;
	return (cp);
}

char* RegexScrubber::parse_token(char *cp, char *&token)
{
	token = 0;

	cp = skipws(cp);
	if (*cp == '\0')
		return (cp);

	if (*cp == '"') {
		token = ++cp;
		while (*cp != '\0') {
			if (*cp == '"' && *(cp-1) != '\\') {
				*cp = '\0';
				return (cp+1);
			}
			cp++;
		}
		return (0);
	}
	else {
		token = cp;
		while (*cp != ' ' && *cp != '\t' && *cp != '\0') 
			cp++;
		if (*cp == '\0')
			return (cp);
		else {
			*cp = '\0';
			return (cp + 1);
		}
	}
}

void RegexScrubber::parse_line(char *string)
{
	char *regexp = 0;
	char *replace = 0;
	char *ret;

	// Ignore comments
	if (string[0] == '#')
		return;

	// First parse the regular expression
	ret = parse_token(string, regexp);
	// Parsing error
	if (ret == 0) {
		//cerr << "Error in file: \""<<filename <<"\"  token: "<<string<<"\n";
		return;
	}
	// Empty line
	else if (regexp == 0) {
		return; 
	}
	string = ret;

	// Next parse the replacement string
	ret = parse_token(ret, replace);
	if (ret == 0) {
		//cerr << "Error in file: \""<<filename<<"\"  token: "<<string <<"\n";
		return;
	}

	// Add the strings to the list (replace can be 0)
	list.add(regexp, replace);
};

// We are guaranteed here that end points to '\n'
void RegexScrubber::scrub_line(char *start, char *end, RegexString *regstr) const
{
	// Create a string for us to use
	char *holder = new char[end-start + 2];
	memcpy (holder, start, end-start+1);
	holder[end-start+1] = '\0';

        RegexString temp;

	// Here we check the list of regular expressions that we have.
	// Currently, only one regular expression per line is allowed
	for (int i = 0; i < list.length(); i++) {
		RegexEntry *entry = list.get(i);

		// See if we have a match -- only allow one per string
		if (entry->replace(holder, temp)) {
                        delete[] holder;
                        holder = temp.takestring();
		}
                
	}
	// If we have not had a match of any of the expressions
	regstr->add(holder);

	delete[] holder;
}

void RegexScrubber::scrub_buffer(const char *rawconst, int rawlen, char *&scrubbed, 
	int &scrubbedlen) const
{
	scrubbed = NULL;
        scrubbedlen = 0;

	if (fail || !rawconst || !rawlen)
            return;

        char* raw = (char*)malloc(rawlen+1);
        if (raw)
        {
            memcpy(raw, rawconst, rawlen);
            raw[rawlen] = '\0';
        }
        else
            return;

	char *start;
	char *end = raw;

	// The 200 is purely arbitrary -- just provides some breathing room
	RegexString regstr(strlen(raw) + 200);

	start = end;

	// Try to create a line from single characters
	while (1) {
		if (end - raw >= rawlen) {
                    if (start != end)
                    {
                        scrub_line (start, end-1, &regstr);
                        start = raw+rawlen;
                    }
                    break;
		}
		else if (*end == '\n') {
			scrub_line (start, end, &regstr);
			start = ++end;
			continue;
		}
		// We have binary data here...
		else if (*end == '\0')
                    *end++ = '.';
			//break;
		else
			end++;
	}
		
	// If we have binary data, just tack it on to the end (not the best
	//   way to do it -- but sufficient for the current test suite)
	int remaining = rawlen - (start - raw);
	if (remaining != 0) {
		regstr.add(start, remaining);
	}

	// Return the scrubbed string to the caller
	scrubbedlen = regstr.length();
	scrubbed = regstr.takestring();
        free(raw);
}

RegexScrubber::operator void*()
{
	return (fail ? 0 : this);
}

void RegexScrubber::add_regex(void *regex, int regex_len, void *replace,
	int replace_len)
{
	char *reg = new char[regex_len + 1];
	memcpy (reg, regex, regex_len);
	reg[regex_len] = '\0';

	char *rep = new char[replace_len + 1];
	memcpy (rep, replace, replace_len);
	rep[replace_len] = '\0';

	list.add(reg, rep);
	
	delete[] reg;
	delete[] rep;
}

