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

#if !defined (NS_COL_STRING_HXX)
#define NS_COL_STRING_HXX	1

/**
 * NSColString - a collectible string
 *	the purpose of this is really to minimize strdup/free. Due to NSAPI
 *	unfortunately we have to sometimes allocate memory for what it returns
 * 
 *	this allows us not to worry about freeing it
 *
 * @author Ruslan Belkin
 * @version 1.0
 */

#include <string.h>

class	NSColString	{
	char *data;
	int	allocated;
public:

	NSColString (const char *s = NULL)
	{
		allocated = 0;
		data = (char *)s;
	}

	NSColString (const NSColString & nss)
	{
		NSColString *p = (NSColString *)&nss;

		data = nss.data;
		allocated = nss.allocated;
		p -> allocated = 0;
	}

	~NSColString ()
	{
		release ();
	}

	NSColString & duplicate (const char *cp = NULL)
	{
		if (cp != NULL)
			release (cp);

		if (allocated == 0 && data != NULL)
		{
			data = strdup (data);
			allocated = 1;
		}
		return *this;
	}

	NSColString & append (const char *cp)
	{
		if (data != NULL && cp != NULL)
		{
			duplicate ();

			int len = strlen ( cp );
			int oln = strlen (data);
			data = (char *)realloc (data, oln + len + sizeof (char));
			
			if (data != NULL)
				strcpy (data + oln, cp);
		}
		return *this;
	}

	NSColString & append_slash (const char *cp)
	{
		if (data != NULL)
		{
			duplicate ();

			int   oln  = strlen (data);
			int  total = oln;
			char slash_c = 0;

			if (oln > 0 && ! is_any_slash (data[oln - 1]))
			{
				total += sizeof (char);
				slash_c = slash ();
			}

			if (cp != NULL)
				total += strlen (cp);

			data = (char *)realloc (data, total + sizeof (char));
			
			if (data != NULL)
			{
				if (slash_c)	{
					data[oln] = slash_c;
					if (cp != NULL)
						strcpy (data + oln + sizeof (char), cp);
				}
				else
					if (cp != NULL)
						strcpy (data + oln, cp);
			}
		}
		return *this;
	}

	int	isAllocated ()	{	return allocated;	}

	NSColString & operator= (const NSColString & nss)
	{
		NSColString *p = (NSColString *)&nss;

		release (nss.data);

		allocated = nss.allocated;
		p -> allocated = 0;
		return *this;
	}

	NSColString & operator= (const char * s)
	{
		release (s);
		return *this;
	}

	operator const char * () const	{	return str ();	}
	const char	* str ()	 const	{	return data;	}

	int is_any_slash (char c)
	{
		return c == '/' || c == '\\';
	}

	char slash ()
	{
#if defined (XP_PC)
		return '\\';
#else
		return '/';
#endif
	}

private:

	void release (const char *cp = NULL)
	{
		if (allocated)
		{
			if (data != NULL)
				free ((void *)data);

			allocated = 0;
		}
		data = (char *)cp;
	}
};

#endif
