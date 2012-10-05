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

//    header[ 
// 
//  Record Structure 
//     
//    data[CONFIGURABLE]  
#ifndef __MemMapFile__ 
#define __MemMapFile__ 
 
#include "NSString.h" 
#include "prio.h" 
#include "NsprWrap/ReadWriteLock.h" 
 
#if !defined (_NS_SERVLET_EXPORT) 
#ifdef XP_PC 
#define _NS_SERVLET_EXPORT  _declspec(dllexport) 
#define _NS_SERVLET_IMPORT  _declspec(dllimport) 
#else 
#define _NS_SERVLET_EXPORT 
#define _NS_SERVLET_IMPORT 
#endif 
#endif

#if !defined (XP_PC)
#include <unistd.h>
#endif

class _NS_SERVLET_EXPORT MemMapFile  
{ 
private: 
   
  PRUintn _BlockSize; 
  PRUintn _MaxBlocks;
  NSString _filename; 
 
  PRUintn _headersizeasinteger; 
  PRUintn _filesize;
  
  void *_memory; 
 
  PRFileDesc* _filedesc; 
  PRFileMap *_mapfile; 
 
  PRBool _setblock (PRUintn location, const void *data, PRUintn size);  
  void _clearblock (PRUintn location); 
  PRUintn _findunusedblock (); 
  void _setusedblock (PRUintn location); 
  void _setunusedblock (PRUintn location); 
  PRBool _checkblock (PRUintn location); 
  PRBool _reserveblock(PRUintn location, void *&data, PRUintn &size);
  PRBool _locateblock(PRUintn location, void *&data, PRUintn &size); 
 
public: 
  MemMapFile (const char *name, PRUintn blocksize, PRUintn maxsize, PRBool &done); 
  ~MemMapFile (); 
 
public: 
  PRUintn getBlockSize () const { return _BlockSize; }; 
  PRUintn getMaxBlocks () const { return _MaxBlocks; }; 
  const char *getFileName () const { return _filename; }; 
 
  PRBool setEntry (PRUintn location, const void *data, PRUintn size); 
  PRUintn setEntry (const void *data, PRUintn size); 
  PRBool getEntry (PRUintn location, void *&data, PRUintn &size); 

  PRUintn reserveEntry(void *&data, PRUintn &size); 
  PRUintn reserveEntry(PRUintn location, void *&data, PRUintn &size); 
  PRBool locateEntry(PRUintn location, void *&data, PRUintn &size);
  void clearEntry (PRUintn location); 
}; 
 
#endif 
 
