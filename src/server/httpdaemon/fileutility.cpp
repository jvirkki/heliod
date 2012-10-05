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

#include "httpdaemon/fileutility.h"

//-----------------------------------------------------------------------------
// FileUtility::FileUtility  Default constructor
//-----------------------------------------------------------------------------

FileUtility::FileUtility()
{
	file 		= NULL;
	linenum 	= 0;
	tokenized 	= 0;
	invalidline	= 1;  // FIX for 627773: initializing invalidline to 1
	length		= 0;
	numtokens	= 0;
}

//-----------------------------------------------------------------------------
// FileUtility::FileUtility  Overloaded constructor open file 'filename'
//-----------------------------------------------------------------------------

FileUtility::FileUtility(char *filename)
{
	file 		= NULL;
	linenum 	= 0;
	tokenized 	= 0;
	invalidline	= 1;  // FIX for 627773: initializing invalidline to 1
	length		= 0;
	numtokens	= 0;
	
	OpenFile( filename );
}

//-----------------------------------------------------------------------------
// FileUtility::~FileUtility  Destructor closes opened file
//-----------------------------------------------------------------------------

FileUtility::~FileUtility()
{
	CloseFile();
}

//-----------------------------------------------------------------------------
// FileUtility::OpenFile  Opens file mentioned
//-----------------------------------------------------------------------------

int FileUtility::OpenFile(char *filename)
{
	int retval = 0;
	
	if ( file == NULL )
	{
		file = fopen( filename , "r" );
		if ( (file !=  NULL) && (!ferror(file)) )
		{
			retval = 1;
		}
	}
	return retval;
}
		
//-----------------------------------------------------------------------------
// FileUtility::ReadLine  reads a line from the file stream
//-----------------------------------------------------------------------------

int FileUtility::ReadLine()
{
	if ( (file !=  NULL) && (!ferror(file)) )
	{
		memset(ReadBuffer,'\0',READBUFSIZE);
		//printf("BEFORE READLINE: %s\n",ReadBuffer);
		fgets( ReadBuffer, READBUFSIZE, file );
		//
		// FIX for 627773: Changed || condition to && condition
		//
		// if ( !ferror(file) || !IsEof() ) 
		//
		if ( !ferror(file) && !IsEof() )
		{
			linenum++;
			tokenized = 0;
			numtokens = 0;
			invalidline = 0;
			//printf("READLINE: %s\n",ReadBuffer);
		}
		else
		{
			invalidline=1;
		}
	}
	////// START FIX 6277733
	else {
		invalidline=1;
	}
	////// END FIX 6277733
	return FUTIL_FALSE;
}

//-----------------------------------------------------------------------------
// FileUtility::CloseFile close the file stream
//-----------------------------------------------------------------------------

void FileUtility::CloseFile()
{
	if ( (file !=  NULL) )
	{ fclose (file); }
}

//-----------------------------------------------------------------------------
// FileUtility::IsEof returns the EOF of the file
//-----------------------------------------------------------------------------

int FileUtility::IsEof()
{
	if ( (file !=  NULL) && (!ferror(file)) )
	{ 
		return feof (file);
	}
	return FUTIL_TRUE;
}

//-----------------------------------------------------------------------------
// FileUtility::IsInvalid returns validity of current line
//-----------------------------------------------------------------------------

int FileUtility::IsInvalid()
{
	return invalidline;
}

//-----------------------------------------------------------------------------
// FileUtility::GetLineNum returns line-number of current line
//-----------------------------------------------------------------------------

int FileUtility::GetLineNum()
{
	return	linenum;
}

//-----------------------------------------------------------------------------
// FileUtility::GetToken returns the char * to token specified
// 		Strips off the whitespaces at the beginning of the token
//-----------------------------------------------------------------------------
		
char* FileUtility::GetToken( int tokennum )
{
	char *rettoken;
	int i;
	int currenttoken = 1;
			
	rettoken =  NULL;
	if ( linenum )
	{
		if ( !tokenized )
		{
			Tokeniser(); 
		}
		
		for ( i = 0 ; i < length ; i++)
		{
			if (!IsWhiteSpace(( ReadBuffer[i] )))
			{ 
				rettoken = &ReadBuffer[i];
				break;
			}
		}
		
		if ( tokennum <= numtokens )
		{
			for ( i = 0 ; (( i < length ) && ( tokennum != currenttoken )) ; i++)
			{
				if (( ReadBuffer[i] == '\0' ))
				{
					// FIX for 6277733:
					// Advance the lookup index by One -
					// it is important to advance this index
					// so that next for-loop works fine
					//
					i++;

					// FIX for 6277733:
					// Commenting out the useless for loop 
					// and corrected it with a right one.
					//
					// for ( ; i++ ; i<length )
					//
					for ( ; i < length ; i++)
					{	if (!IsWhiteSpace(ReadBuffer[i]))
						{ break; }
					}

					rettoken = &ReadBuffer[i];
					currenttoken++;
				}
			}
		}
		else
		{ rettoken = NULL; }
	}
	return rettoken;
}

//-----------------------------------------------------------------------------
// FileUtility::StartsWith compares if the line read starts with the string
//-----------------------------------------------------------------------------
		
int FileUtility::StartsWith(char *string)
{
	int len, len1;
	int retval = 0;
	 
	if (linenum)
	{
		len = strlen ( string );
		if ( strncmp ( string, ReadBuffer, len ) == 0)
		{ retval = 1; }
	}
	return retval;
}
		
//-----------------------------------------------------------------------------
// FileUtility::Close closes the current file
//-----------------------------------------------------------------------------
		
int FileUtility::Close()
{
	if ((file !=  NULL) && (!ferror(file)) )
	{ return fclose (file); }
	return FUTIL_TRUE;
}

//-----------------------------------------------------------------------------
// FileUtility::Tokeniser tokenises the current line read
// 	Rules for tokens: 
//		- tokens can start with 'whitespace' character
//		- tokens end when the next character encountered is a 'whitespace'
//-----------------------------------------------------------------------------

void FileUtility::Tokeniser()
{
	int i;

	////// START FIX 6277733

	// Reset numtokens to Zero - At this stage we don't know whether
	// the ReadBuffer has any tokens or not.
	//
	numtokens = 0;

	if ( linenum )
	{
		length = strlen (ReadBuffer);

		// Commented the below line as it appears to be useless to do
		// do this. Why we need to truncate last char in the ReadBuffer ???
		// Doesn't sound right to do this here, Also this will result in
		// a coredump or segment voilation if strlen is Zero.
		//
		// ReadBuffer[length-1] = '\0';
		//

		// If string is null then return from here. And at this
		// stage numtokens=0 and tokenized=1
		//
		if(length <= 0)
		{
			tokenized = 1;
			return;
		}

		// Set numtokens=1 - because at this stage ReadBuffer will have
		// at least one character or one token
		//
		numtokens = 1;
		
		for (i = 1 ; i < length ; i++)
		{
			if (IsWhiteSpace(ReadBuffer[i]))
			{
				if ((ReadBuffer[i-1] != '\0') && !IsWhiteSpace(ReadBuffer[i-1]))
				{
					ReadBuffer[i] = '\0';

		// Increment numtokens only if you not yet reached last char.
		// That is, if you reached the last char OR if you hit the
		// condition (i == (length - 1)), then don't increment the
		// numtokens because there are no charcters/tokens beyond that point
		// To understand this take "   a test string " as the example
		// to trace this function - this string has one trailing space char
		//
					if(i < (length - 1))
					{
						numtokens++;
					}
				}
			}
		}
	}

	////// END FIX 6277733

	tokenized = 1;
}

//-----------------------------------------------------------------------------
// FileUtility::IsWhiteSpace tests if current character matches any of the
//		unwanted characters mentioned
//-----------------------------------------------------------------------------

int FileUtility::IsWhiteSpace(char ch)
{
	if ((ch == ' ')||(ch == '\t' )||(ch == ':')||(ch == '|'))
	{ return FUTIL_TRUE; }
	return FUTIL_FALSE;
}

