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

// Utility functions for extracting integral values from strings.

#ifndef STRINGVALUE_H
#define STRINGVALUE_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <nspr.h>

#if defined(XP_WIN32)
#ifndef strncasecmp
#define strncasecmp(a,b,c) strnicmp(a,b,c)
#endif
#endif

class StringValue {
public:
    // Returns PR_TRUE if string contains only a single boolean (without any
    // white space), PR_FALSE otherwise
    static PRBool isBoolean(const char* string)
    {
        if (string) {
            return (tryBoolean(string, strlen(string)) != -1);
        }

        return -1;
    }

    // Returns PR_TRUE if string contains a boolean, PR_FALSE otherwise.  
    // Leading spaces and trailing punctuation/spaces are ignored.
    static PRBool hasBoolean(const char* string)
    {
        return (tryBoolean(string) != -1);
    }

    // Get a boolean from string, defaulting to PR_FALSE
    static PRBool getBoolean(const char* string)
    {
        return (tryBoolean(string) == PR_TRUE);
    }

    // Try to get a boolean from string.  Returns -1 if string does not 
    // represent a boolean.  Leading spaces and trailing punctuation/spaces are
    // ignored.
    static int tryBoolean(const char* string)
    {
        if (string) {
            // Skip past leading white space
            while (isspace(*string)) string++;

            // Skip to trailing white space or punctuation
            const char* end = string;
            while (*end && !isspace(*end) && !ispunct(*end)) end++;
            int length = end - string;

            return tryBoolean(string, length);
        }

        return -1;
    }

    // Returns PR_TRUE if string contains only a single integer (without any
    // white space), PR_FALSE otherwise
    static PRBool isInteger(const char* string)
    {
        if (string) {
            if (isdigit(*string)) {
                int value;
                int length = 0;
                if (sscanf(string, "%i%n", &value, &length) == 1) {
                    return !string[length];
                }
            }
            return isBoolean(string);
        }

        return PR_FALSE;
    }

    // Returns PR_TRUE if string contains an integer, PR_FALSE otherwise.  
    // Leading spaces and trailing punctuation/spaces are ignored.
    static PRBool hasInteger(const char* string)
    {
        if (string) {
            int value;
            int length = 0;
            if (sscanf(string, "%i%n", &value, &length) == 1) {
                return (!string[length] || isspace(string[length]) || ispunct(string[length]));
            }
            return hasBoolean(string);
        }

        return PR_FALSE;
    }

    // Get an integer from string, defaulting to 0.  Leading spaces and
    // trailing non-digit characters are ignored.
    static int getInteger(const char* string)
    {
        if (string) {
            int value;
            if (sscanf(string, "%i", &value) == 1) return value;

            value = tryBoolean(string);
            if (value != -1) return value;
        }

        return 0;
    }

    // Get a 64 bit integer from string, defaulting to 0.  Leading spaces and
    // trailing non-digit characters are ignored.
    static PRInt64 getInteger64(const char* string)
    {
        if (string) {
            PRInt64 value;
            if (PR_sscanf(string, "%lli", &value) == 1) return value;

            int boolean;
            boolean = tryBoolean(string);
            if (boolean != -1) {
                LL_I2L(value, boolean);
                return value;
            }
        }

        return 0;
    }

private:
    // Try to get a boolean from the first length characters of string.
    // Returns -1 if the first length characters do not represent a boolean.
    static int tryBoolean(const char* string, int length)
    {
        switch (length) {
        case 1:
            if (*string == '1') return 1;
            if (*string == '0') return 0;
            if (tolower(*string) == 'y') return 1;
            if (tolower(*string) == 'n') return 0;
            break;

        case 2:
            if (!strncasecmp(string, "on", length)) return 1;
            if (!strncasecmp(string, "no", length)) return 0;
            break;

        case 3:
            if (!strncasecmp(string, "yes", length)) return 1;
            if (!strncasecmp(string, "off", length)) return 0;
            break;

        case 4:
            if (!strncasecmp(string, "true", length)) return 1;
            break;

        case 5:
            if (!strncasecmp(string, "false", length)) return 0;
            break;

        case 6:
            if (!strncasecmp(string, "enable", length)) return 1;
            break;

        case 7:
            if (!strncasecmp(string, "enabled", length)) return 1;
            if (!strncasecmp(string, "disable", length)) return 0;
            break;

        case 8:
            if (!strncasecmp(string, "disabled", length)) return 0;
            break;
        }

        return -1;
    }
};

#endif // STRINGVALUE_H
