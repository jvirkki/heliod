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

#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

#include "nspr.h"
#include "plstr.h"
#include "netsite.h"

#include "base/net.h"
#include "base/util.h"
#include "base/plist.h"
#include "base/systhr.h"

#include "frame/log.h"
#include "frame/http.h"
#include "frame/httpact.h"

#include "ServletHandler.h"

ServletHandler::ServletHandler(pblock* pb, const char* textStart, size_t len)
  : _params(0),
    _init_params(0),
    _key_params(0),
    _is_valid(PR_TRUE),
    _is_perm_pblock(PR_FALSE),
    _classfile(0),
    _len_classfile(0),
    _codebase(0),
    _err_msg(0)
{
  thrlock = crit_init();
  
  if (! pb) {
    _is_valid = PR_FALSE;
    return;
  }
  
  /*
   * Store the contents of pb as the servlet url and some
   * key parameters for the url are all listed there
   */
  _key_params = extract_keywords(pb);
  if (_is_valid == PR_FALSE)
    return;

  if (! _key_params) {
    /*
     * To log the errors we need access to rq and sn object and we don't
     * have it yet, so just remember this message and log it when execute
     * method is called.
     */
    set_error_message("No servlet class name specified");
    return;
  }
  
  _init_params = extract_nonkeywords(pb);
  if (_is_valid == PR_FALSE)
    return;
  
  if (textStart && len) {
    char* buf = (char*) MALLOC(len + 1);
    if (! buf) {
      _is_valid = PR_FALSE;
      return;
    }
    
    PL_strncpy(buf, textStart, len);
    buf[len] = 0;

    const int SERVLET_PARAMSIZE = 5;
    _params = pblock_create(SERVLET_PARAMSIZE);
    if (! _params) {
      _is_valid = PR_FALSE;
      FREE(buf);
      
      return;
    }
    
    // Now parse the text and fill up _params pblock with
    // name-value pairs
    PRBool res = ParseParamTags(buf);
    if (res != PR_TRUE) {
      set_error_message("Parse error");
      
      FREE(buf);

      return;
    }
    
    FREE(buf);
  }

  /*
   * Now we need to make sure that either code or name keyword is
   * present in _key_params pblock, otherwise the information is
   * insufficient to execute a servlet.
   */
  if (_is_valid == PR_TRUE) {
    _is_valid = PR_FALSE;
    
    _classfile = pblock_findval("code", _key_params);
    if (_classfile) {
      _is_valid = PR_TRUE;
      
      strip_suffix_from_classfile(_classfile);
      
      _codebase = pblock_findval("codebase", _key_params);
      if ((! _codebase) && (! thrlock))
	_is_valid = PR_FALSE;	// very rare that this would happen
    } else {
      set_error_message("No servlet class name specified");
    }
  }

  /*
   * pblocks use pool memory and so are valid across only one thread,
   * since we want to use these pblocks across various requests we
   * need to PERM_MALLOC them
   */
  if (_is_valid == PR_TRUE)
    _is_valid = convert_pblocks_to_perm_pblocks();

  if (_is_valid == PR_TRUE) {
    _classfile = pblock_findval("code", _key_params);	// re-establish this pointer
    _len_classfile = PL_strlen(_classfile);

    _codebase = pblock_findval("codebase", _key_params);
  }
}

ServletHandler::~ServletHandler()
{
  if (_is_perm_pblock == PR_TRUE) {
    if (_params)
      perm_pblock_free(_params);

    if (_init_params)
      perm_pblock_free(_init_params);

    if (_key_params)
      perm_pblock_free(_key_params);
  } else {
    if (_params)
      pblock_free(_params);

    if (_init_params)
      pblock_free(_init_params);

    if (_key_params)
      pblock_free(_key_params);
  }

  _err_msg = 0;
  
  if (thrlock)
    crit_terminate(thrlock);
}

PRBool
ServletHandler::Execute(pblock* pb, Session* sn, Request* rq, PageStateHandler& pageState)
{
  if (_is_valid == PR_FALSE) {
    
    if (_err_msg)
      log_error(LOG_FAILURE, "servlet-shtml", sn, rq, _err_msg);
          
    pageState.WriteErrMsg();

    return PR_FALSE;
  }
  
  /* Send the headers before dispatching the internal request */
  PRBool res = pageState.SendHeaders();

  /*
   * Let's the construct the uri for this embedded servlet
   */
  char *shtml_uri = pblock_findval("uri", rq->reqpb);
  int len_shtml_uri = PL_strlen(shtml_uri);

  char *query_str = pblock_findval("query", rq->reqpb);
  int len_query_str = PL_strlen(query_str);
 
  char *path_info = pblock_findval("path-info", rq->vars);
  int len_path_info = PL_strlen(path_info);
  if(len_path_info)
  {
      len_shtml_uri = len_shtml_uri - len_path_info;
      shtml_uri[len_shtml_uri] = 0;
  }
  int len_slash = 0;
  char *slash = PL_strrchr(shtml_uri, '/');
  if (slash)
    len_slash = PL_strlen(slash);

  int len = _len_classfile + len_shtml_uri + 1 - len_slash;
  if (len_query_str)
  {
    len = len + len_query_str + 1;
  }
  if (len_path_info)
  {
    len= len + len_path_info;
  }

  char *uri = (char *) MALLOC(len + 1);
  if (! uri) {
    pageState.WriteErrMsg();
    return PR_FALSE;
  }

  int end = 0;
  if (_classfile[0] != '/') {
      PL_strncpy(uri, shtml_uri, len_shtml_uri - len_slash);
      end = len_shtml_uri - len_slash;
      uri[end++] = '/';
  }
  PL_strcpy(&(uri[end]), _classfile);
  end = end + _len_classfile;
  if (len_path_info)
  {
    PL_strcpy(&(uri[end]), path_info);
    end = end + len_path_info;
  }	     
  if (len_query_str)
  {
    uri[end++] = '?';
    PL_strcpy(&(uri[end]), query_str);
  }
  // Set the codebase if needed
  if (! _codebase)
    set_default_codebase(rq);
  
  res = PR_FALSE;

  if (servact_include_virtual(sn, rq, uri, _params) == REQ_PROCEED) {
    res = PR_TRUE;
  } else {
    pageState.WriteErrMsg();
  }

  FREE(uri);
  
  return res;
}

PRBool 
ServletHandler::ParseParamTags(const char* buf)
{
  // We have one or more lines of <PARAM NAME=xxx value="yyy">
  PRBool res = PR_TRUE;
  char* cur = (char*) buf;

  while ((res == PR_TRUE) && *cur) {
    char* begin = PL_strchr(cur, '<');
    if (! begin)
      break;
    
    begin++; // skip over '<'
    char* end = PL_strchr(begin, '>');
    if (! end) {
      res = PR_FALSE;
    } else {
      char temp = *end;
	
      *end = '\0';
      res = ParseAParamTag(begin);  
      *end = temp;

      cur = ++end;
    }
  }
  
  return res;
}

PRBool
ServletHandler::ParseAParamTag(const char* buf)
{
  PRBool res = PR_FALSE;
  
  static const char* PARAM_TAG     = "PARAM";
  static const int   PARAM_TAG_LEN = 5;

  if (PL_strncasecmp(buf, PARAM_TAG, PARAM_TAG_LEN) == 0) {
    const char* ptr = buf + PARAM_TAG_LEN;

    static const char*	NAME_KEYWORD      = "name";
    static const int	NAME_KEYWORD_LEN  = 4;

    static const char*	VALUE_KEYWORD     = "value";
    static const int	VALUE_KEYWORD_LEN = 5;
    
    char *name = 0;
    char *value = 0;
    
    name = GetValue(ptr, NAME_KEYWORD, NAME_KEYWORD_LEN, &ptr);
    if (name)
      value = GetValue(ptr, VALUE_KEYWORD, VALUE_KEYWORD_LEN, &ptr);
      
    if (name && value) {
      pblock_nvinsert(name, value, _params);
      res = PR_TRUE;
    }

    FREE(name);
    FREE(value);
  }

  return (res);
}

PRBool
ServletHandler::IsSpace(char c)
{
  switch (c) {	    
  case ' ':
  case '\t':
  case '\r':
  case '\n':
    return (PR_TRUE);
    break;
    
  default:
    break;
  }
  
  return (PR_FALSE);
}

const char *
ServletHandler::IgnoreSpaces(const char* buf)
{
  while (buf && (*buf)) {
    if (IsSpace(*buf))
      ++buf;
    else
      return (buf);	    
  }

  return ((const char *) 0);
}

const char*
ServletHandler::IgnoreNonSpaces(const char* buf)
{
  while (buf && (*buf)) {
    if (IsSpace(*buf))
      return (buf);	    
    else
      ++buf;
  }

  return (buf);
}

/*
 * Suppose the string is "123456789asdfgh", buf is pointing to '1' at
 * this time, it returns a pointer to the ending '"' if it exists.
 */

const char*
ServletHandler::GetEndingDoubleQuote(const char* buf)
{
  int i = 0;

  while (buf[i]) {
    if (buf[i] == '\"') {

      if (i == 0)
	return (buf);

      // We need to take care of escaped backslashes, hence this code
      
      if (buf[i-1] != '\\')
        return (buf + i);
      
      /*
       * Now if there are odd number of backslashes this is the
       * end of the double quoted string. If not we need to find
       * another double quote I suppose.
       */
	      
      int n_slashes = 0;	      
      for (int j = i-1; j >= 0; j--) {
        if (buf[j] != '\\')
          break;
        else
          ++n_slashes;
      }

      if (n_slashes % 2 == 1)
        return (buf + i);
    }

    ++i;
  }

  return ((const char *) 0);
}

/*
 * Extracts values from strings of the following format
 *	<tuple_name>=<value>
 */

char *
ServletHandler::GetValue(
	const char*  buf,
	const char*  tuple_name,
	int	     tuple_name_len,
	const char** tuple_end)
{
  const char* ptr = IgnoreSpaces(buf);
  if ((! ptr) || (! *ptr))
    return (0);

  if (PL_strncasecmp(ptr, tuple_name, tuple_name_len))
    return (0);

  ptr = IgnoreSpaces(ptr + tuple_name_len);
  if ((! ptr) || (*ptr != '='))
    return (0);

  ptr = IgnoreSpaces(ptr + 1);	/* skip "=" */
  if ((! ptr) || (! *ptr))
    return (0);

  // Now ptr is pointing to the requisite value
  const char *val = ptr;

  if (*val != '"') {
    ptr = IgnoreNonSpaces(ptr);
  } else {
    ptr = GetEndingDoubleQuote(ptr + 1);
    if (! ptr)
      return (0);
  }

  // Now val is pointing to the beginning and ptr to the end
  char* tmp_ptr = (char *) ptr;

  char c = *tmp_ptr;
  *tmp_ptr = 0;

  char *value = 0;
  if (*val == '"')
    value = STRDUP(val + 1);
  else
    value = STRDUP(val);
    	  
  *tmp_ptr = c;

  /* Now advance the end pointers */
  if (*ptr == '"')
    ++ptr;

  if (tuple_end)
    *tuple_end = ptr;

  return (value);
}

/*-----------------------------------------------------------------------------*
 *
 * SHTML parser in NES doesn't handle case sensitivity. So we need to take care
 * of some cases (like code, CODE, Code, etc.) ourselves, hence all the
 * following code!
 *
 *-----------------------------------------------------------------------------*/

/**
 * The following routine is used to find if a name in name=value tuple in the
 * servlet tag line is a keyword (one of code, codebase or name)
 */

PRBool
ServletHandler::is_servlet_SSI_keyword(char *word)
{
  PRBool res = PR_FALSE;

  if (word) {
    if (! PL_strcasecmp(word, "code"))
      res = PR_TRUE;
    else if (! PL_strcasecmp(word, "name"))
      res = PR_TRUE;
    else if (! PL_strcasecmp(word, "codebase"))
      res = PR_TRUE;
  }

  return res;
}

/**
 * The following routine is used as a enumerator function to count the number
 * of keywords using is_servlet_SSI_keyword routine.
 */

void
ServletHandler::get_num_keywords_enumerator(char *name, const void *value, void *addr)
{
  if (is_servlet_SSI_keyword(name) == PR_TRUE) {
    if (addr)
      *((int *) addr) += 1;
  }
}

/**
 * Find out how many keywords are present in a given pblock using the
 * enumerator get_num_keywords_enumerator
 */

int
ServletHandler::get_num_keywords(pblock *pb)
{
  int	n = 0;
  
  PListEnumerate((PList_t) pb, &ServletHandler::get_num_keywords_enumerator, (void *) &n);
  return n;
}

/**
 * The following routine is used as a enumerator function to count the number
 * of words which are not keywords using is_servlet_SSI_keyword routine.
 */

void
ServletHandler::get_num_nonkeywords_enumerator(char *name, const void *value, void *addr)
{
  if (is_servlet_SSI_keyword(name) == PR_FALSE) {
    if (addr)
      *((int *) addr) += 1;
  }
}

/**
 * Find out how many words which are not keywords are present in a given
 * pblock using the enumerator get_num_nonkeywords_enumerator
 */

int
ServletHandler::get_num_nonkeywords(pblock *pb)
{
  int	n = 0;
  
  PListEnumerate((PList_t) pb, &ServletHandler::get_num_nonkeywords_enumerator, (void *) &n);
  return n;
}

/**
 * The following routine is used as a enumerator function to add only keyword
 * parameters to a pblock after converting the keyword to lowercase.
 */

void
ServletHandler::add_keyword_to_pblock(char *name, const void *value, void *addr)
{
  pblock *pb = (pblock *) addr;
  if (! pb)
    return;

  if (is_servlet_SSI_keyword(name) == PR_TRUE) {

    char *lower_case_name = STRDUP(name);
    for (int i = 0; lower_case_name[i]; i++)
      lower_case_name[i] = (char) tolower((int) lower_case_name[i]);

    pblock_nvinsert(lower_case_name, (const char *) value, pb);
    FREE(lower_case_name);	
  }
}

/**
 * The following routine extracts out all the keywords and returns them in a
 * pblock.
 */

pblock *
ServletHandler::extract_keywords(pblock *pb)
{
  if (! pb)
    return NULL;

  if (! get_num_keywords(pb))
    return NULL;

  static const int NUM_SERVLET_SSI_KEYWORDS = 3;
  pblock *kpb = pblock_create(NUM_SERVLET_SSI_KEYWORDS);
  if (! kpb) {
    set_error_message("pblock creation failed.");
    return NULL;
  }

  PListEnumerate((PList_t) pb, &ServletHandler::add_keyword_to_pblock, (void *) kpb);

  return kpb;
}

/**
 * The following routine is used as a enumerator function to add only
 * non keyword parameters to a pblock.
 */

void
ServletHandler::add_nonkeyword_to_pblock(char *name, const void *value, void *addr)
{
  pblock *pb = (pblock *) addr;
  if (! pb)
    return;

  if (is_servlet_SSI_keyword(name) == PR_FALSE)
    pblock_nvinsert(name, (const char *) value, pb);
}

/**
 * The following routine extracts out all the nonkeywords and returns them in a
 * pblock.
 */

pblock *
ServletHandler::extract_nonkeywords(pblock *pb)
{
  if (! pb)
    return NULL;

  int n = get_num_nonkeywords(pb);
  if (! n)
    return NULL;

  pblock *nkpb = pblock_create(n);
  if (! nkpb) {
    set_error_message("pblock creation failed.");
    return NULL;
  }
  
  PListEnumerate((PList_t) pb, &ServletHandler::add_nonkeyword_to_pblock, (void *) nkpb);

  return nkpb;
}

/*
 * The following routine strips the ".class" suffix from the string specified
 * as an argument to CODE. This needs to be done because Servlet class loader
 * insists that there be no ".class" suffix.
 */

void
ServletHandler::strip_suffix_from_classfile(char *classfile)
{
  if (! classfile)
    return;

  static const char *CLASS_SUFFIX     = ".class";
  static const int   CLASS_SUFFIX_LEN = strlen(CLASS_SUFFIX);

  int len = strlen(classfile);
  if (len <= CLASS_SUFFIX_LEN)	// don't need to do anything!
    return;

  // let's see if it ends with ".class"
  if (! PL_strcasecmp(&(classfile[len - CLASS_SUFFIX_LEN]), CLASS_SUFFIX))
    classfile[len - CLASS_SUFFIX_LEN] = '\0';
}

/*
 * The following function sets the directory where this shtml page was located
 * as the default classpath if codebase is not set.
 */

void
ServletHandler::set_default_codebase(Request *rq)
{
  char *path = pblock_findval("path", rq->vars);
  if (! path)
    return;
  
  char *classpath  = STRDUP(path);

  // strip the last part from 
  char *slash = PL_strrchr(classpath, '/');
  if (slash)
    *slash = '\0';

  crit_enter(thrlock);
  if (! _codebase) {
    perm_pblock_nvinsert("codebase", (const char *) classpath, _key_params);
    _codebase = pblock_findval("codebase", _key_params);
  }
  crit_exit(thrlock);
  
  FREE(classpath);
}

/**
 * pblock uses pool memory and so cannot be used across requests, so the
 * following are the hacks for getting around this problem.
 */
pblock *
ServletHandler::perm_pblock_copy(pblock *pb)
{
  if (! pb)
    return NULL;
  
  // Set thread mallok key to NULL to ensure that pblocks get PERM_MALLOCED
  
  int malloc_key = getThreadMallocKey(); 
  void *oldpool = INTsysthread_getdata(malloc_key);
  INTsysthread_setdata(malloc_key, NULL);
  
  pblock *ppb = pblock_create(SHTML_PARAMSIZE);
  if (ppb)
    pblock_copy(pb, ppb);
  
  // Reset original thread malloc key
  INTsysthread_setdata(malloc_key, oldpool);
  
  return (ppb);
}

void
ServletHandler::perm_pblock_free(pblock *pb)
{
  if (! pb)
    return;
  
  // Set thread mallok key to NULL to ensure that pblocks get PERM_MALLOCED
  
  int malloc_key = getThreadMallocKey(); 
  void *oldpool = INTsysthread_getdata(malloc_key); 
  INTsysthread_setdata(malloc_key, NULL);
  
  pblock_free(pb);
  
  INTsysthread_setdata(malloc_key, oldpool); 
}

void
ServletHandler::perm_pblock_nvinsert(char *name, const char *value, pblock *pb)
{
  if (! pb)
    return;
  
  // Set thread mallok key to NULL to ensure that pblocks get PERM_MALLOCED
  
  int malloc_key = getThreadMallocKey(); 
  void *oldpool = INTsysthread_getdata(malloc_key); 
  INTsysthread_setdata(malloc_key, NULL);

  pblock_nvinsert(name, value, pb);
  
  INTsysthread_setdata(malloc_key, oldpool); 
}

PRBool
ServletHandler::convert_pblocks_to_perm_pblocks()
{
  pblock *tpbk = perm_pblock_copy(_key_params);
  pblock *tpbi = perm_pblock_copy(_init_params);
  pblock *tpbp = perm_pblock_copy(_params);

  PRBool res = PR_TRUE;

  if ( ((_key_params)  && (! tpbk)) ||
       ((_init_params) && (! tpbi)) ||
       ((_params)      && (! tpbp)) )
    res = PR_FALSE;

  if (_key_params) {
    pblock_free(_key_params);
    _key_params = tpbk;
  }
  
  if (_init_params) {
    pblock_free(_init_params);
    _init_params = tpbi;
  }
  
  if (_params) {
    pblock_free(_params);
    _params = tpbp;
  }

  _is_perm_pblock = PR_TRUE;
  return (res);
}

/*
 * Sets the reason as to what's wrong with this page?
 */
void
ServletHandler::set_error_message(char *msg)
{
  _is_valid = PR_FALSE;
  
  if (! _err_msg)
    _err_msg = msg;
}
