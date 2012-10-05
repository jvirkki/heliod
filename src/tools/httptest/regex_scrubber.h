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
 * regex_scrubber.h
 *
 * Class used to cleanup ("scrub") HTTP responses so they can be compared 
 *  against a gold file.
 * This is now also used to scrub the dat and res files.
 */
#ifndef _REGEX_SCRUBBER_H
#define _REGEX_SCRUBBER_H

#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class RegexScrubber {
	private:
		RegexList list;         // List of regular expressions
		const char *filename;	// File holding expressions (for error msgs)
		int fail;               // Was this class created correctly

		// Utility functions for doing the scrubbing
		char *skipws (char *cp);
		void parse_line(char *line);
		char *parse_token(char *cp, char *&token);
		void scrub_line (char *start, char *end, RegexString *regstr) const;

	public:
		RegexScrubber();
		RegexScrubber(const char *filename);
        RegexScrubber(char *stringbuf, int stringsize);
		~RegexScrubber();

		operator void*();
	
		// input: 
		//    raw: request to be scrubbed
		// output:
		//    scrubbed: created string that has regular expressions replaced;
		//       the string is allocated through "new" and will need to be
		//       freed through "delete"

		void add_regex(void *regex, int regex_len, void *replace,
			int replace_len);

		void scrub_buffer (const char *raw, int rawlen, char *&scrubbed,
			int &scrubbedlen) const;
        protected:
                void initialize(istream& astream);
};

#endif
