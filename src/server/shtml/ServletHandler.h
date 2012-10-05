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

#ifndef  _SERVLET_HANDLER_H_ 
#define  _SERVLET_HANDLER_H_

#include "shtml_public.h"
#include "support/NSString.h"

#include "NSBaseTagHandler.h"

#include <base/crit.h>

class ServletHandler : public NSBaseTagHandler {
  public : 
    ServletHandler(pblock* pb, const char* textStart, size_t len);
    ~ServletHandler();
    
    int Execute(pblock* pb, Session* sn, Request* rq, PageStateHandler& pageState);

  private:
    PRBool ParseParamTags(const char* buf);
    PRBool ParseAParamTag(const char* buf);

    PRBool IsSpace(char c);
    const char* IgnoreSpaces(const char* buf);
    const char* IgnoreNonSpaces(const char* buf);
    const char* GetEndingDoubleQuote(const char* buf);

    char* GetValue(
	    const char*	 buf,
	    const char*  tuple_name,
	    int		 tuple_name_len,
	    const char** tuple_end);

    static PRBool is_servlet_SSI_keyword(char *word);

    static void get_num_keywords_enumerator(char *name, const void *value, void *addr);
    int  get_num_keywords(pblock *pb);

    static void get_num_nonkeywords_enumerator(char *name, const void *value, void *addr);
    int  get_num_nonkeywords(pblock *pb);

    static void add_keyword_to_pblock(char *name, const void *value, void *addr);
    pblock *extract_keywords(pblock *pb);

    static void add_nonkeyword_to_pblock(char *name, const void *value, void *addr);
    pblock *extract_nonkeywords(pblock *pb);

    void strip_suffix_from_classfile(char *classname);
    void set_default_codebase(Request *rq);

    pblock *perm_pblock_copy(pblock *pb);
    void   perm_pblock_free(pblock *pb);
    void   perm_pblock_nvinsert(char *name, const char *value, pblock *pb);
    
    PRBool convert_pblocks_to_perm_pblocks();

    void set_error_message(char *msg);
    
    pblock* _params;		// for run  parameters of the servlet
    pblock* _init_params;	// for init parameters of the servlet
    pblock* _key_params;	// for key  parameters of the servlet (name, code, codebase)

    char* _classfile;		// the pointer to the value of code
    int   _len_classfile;	// len of the classfile name

    char*  _codebase;		// cached pointer to the codebase
				     
    PRBool _is_valid;		// is everything you need to execute servlet there?
    PRBool _is_perm_pblock;	// does it use perm pblocks already?

    char*  _err_msg;		// what's wrong with this page?
    
    /*
     * so that two threads don't try to set _codebase simultaneously
     */
    CRITICAL thrlock;
};

#endif /* _SERVLET_HANDLER_H_ */
