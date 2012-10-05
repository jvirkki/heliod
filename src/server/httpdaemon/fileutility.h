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

#ifndef _fileutility_
#define _fileutility_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define	 READBUFSIZE	1024
#define  FUTIL_TRUE	1
#define  FUTIL_FALSE	0

/**
 * This class represents the helper utility for reading from the proc filesystem
 * 
 * @author  $Author: wb123103 $
 * @version $Revision: 1.1.6.1 $ $Date: 2003/05/08 19:22:40 $
 * @since   S1AS Linux Port
 */

class FileUtility
{
	public:
	        /**
         	 * Default constructor.
         	 */
		FileUtility();
		
		/**
         	 * Overloaded constructor opens the file.
         	 */
		FileUtility(char *filename);
		
		/**
         	 * Destructor.
         	 */
		~FileUtility();
		
		/**
         	 * Open a specified file for reading.
		 *
		 * @returns	<code>1</code> if error
		 *		<code>0</code> if success
		 */
		int OpenFile(char *filename);
		
		/**
         	 * Read a single line from the file stream.
		 *
		 * @returns	<code>1</code> if error
		 *		<code>0</code> if success
		 */
		int ReadLine();
		
		/**
         	 * Close the file.
		 */
		void CloseFile();
		
		/**
         	 * Returns EOF of the file.
		 *
		 * @returns	<code>1</code> if EOF
		 *		<code>0</code> if not EOF
		 */
		int IsEof();
		
		/**
         	 * Returns if line read is invalid can happen before EOF has been reached.
		 * Invalid flag set by the ReadLine(...) method
		 *
		 * @returns	<code>1</code> if invalid line
		 *		<code>0</code> if valid line
		 */
		int IsInvalid();
		
		/**
         	 * Gets the current line number.
		 *
		 * @returns	line number of the file
		 *
		 */
		int GetLineNum();
		
		/**
         	 * Gets the Token to the token number specified.
		 *
		 * @returns	<code>char *</code> to the token specified
		 *		<code> NULL </code> if token number not found
		 */
		char* GetToken( int tokennum );
		
		/**
         	 * Compares if the first 'n' characters in string matches the line read.
		 *
		 * @returns	<code>1</code> if there is a match
		 *		<code>0</code> if no match
		 */
		int StartsWith(char *string);
		
		/**
         	 * Close the file.
		 *
		 * @returns	<code>1</code> if error closing
		 *		<code>0</code> 
		 */
		int Close();
		
	private:
		/**
         	 * Tokenises the line read.
		 * Tokens are seperate by 'whitespaces' 
		 */
		void	Tokeniser();
		
		/**
         	 * Tests current character if its a whitespace.
		 *
		 * @returns	<code>1</code> if its a whitespace character
		 *		<code>0</code> if it isn't a whitespace
		 */
		int	IsWhiteSpace(char ch);
		
		FILE*	file;
		char	ReadBuffer[READBUFSIZE];
		short	linenum;
		short	tokenized;
		short	numtokens;
		int	length;
		int	invalidline;
};

#endif /* _FileUtility_h_ */
