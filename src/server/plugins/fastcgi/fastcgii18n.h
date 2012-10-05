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

#include "i18n.h"

#undef BEGIN_STR
#undef ResDef
#undef END_STR

#ifdef RESOURCE_STR
// Setup ResDef, etc. so that including a dbt*.h header will define the default
// strings and pointers that will be used to cache localized strings
#define BEGIN_STR(argLibraryName)
#define ResDef(argToken, argID, argString)                                \
     const char *fastcgi_admin_str_ ## argToken = NULL;               \
     const char *fastcgi_default_str_ ## argToken = argString;
#define END_STR(argLibraryName)
#else
// Setup ResDef, etc. so that including a dbt*.h header will declare extern
// pointers to the default strings and to cached localized strings
#define BEGIN_STR(argLibraryName)
#define ResDef(argToken, argID, argString)                                \
     static const struct TokenID *argToken = (struct TokenID *) argID;    \
     extern const char *fastcgi_admin_str_ ## argToken;               \
     extern const char *fastcgi_default_str_ ## argToken;
#define END_STR(argLibraryName)
#endif

// XP_GetAdminStr wrapper
#define GetString(DBTTokenName) \
        (fastcgi_admin_str_ ## DBTTokenName != NULL && *fastcgi_admin_str_ ## DBTTokenName != '\0') ?                                \
        (fastcgi_admin_str_ ## DBTTokenName) :                                        \
        (((fastcgi_admin_str_ ## DBTTokenName = XP_GetAdminStr((long)DBTTokenName)) != NULL && *fastcgi_admin_str_ ## DBTTokenName != '\0') ?  \
         (fastcgi_admin_str_ ## DBTTokenName) :                               \
         (fastcgi_default_str_ ## DBTTokenName))

#include "dbtfastcgi.h"
