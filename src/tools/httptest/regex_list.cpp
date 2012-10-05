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
 * regex_list.cpp
 *
 * Maintain a very basic list of RegexEntry objects.
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
#include "regex_list.h"

RegexList::RegexList()
{
		list = new RegexEntry*[INITIAL_LIST_SIZE];
		list_next = 0;
		list_max = INITIAL_LIST_SIZE;
}

RegexList::~RegexList()
{
	for (int i=0; i<list_next; i++)
		delete (list[i]);
	delete list;
}

void RegexList::add(const char *regex, const char *replace)
{
	if (list_next == list_max) {
		RegexEntry **newlist = new RegexEntry*[list_max * 2];
		memcpy (newlist, list, list_max * sizeof(RegexEntry *));
		list_max *= 2;
		delete list;
		list = newlist;
	}
	list[list_next] = new RegexEntry(regex, replace);
	if (list[list_next])
		list_next++;
	else
		delete (list[list_next]);
} 

RegexEntry* RegexList::get(int index) const
{
	if (index >= list_next)
		return (0);
	else
		return (list[index]); 
}

int RegexList::length() const
{
	return (list_next);
}

void RegexList::dump()
{
	for (int i = 0; i < list_next; i++) {
		list[i]->dump();
	}
}
