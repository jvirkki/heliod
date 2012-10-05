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

#ifndef BASE_PLATFORM_H
#define BASE_PLATFORM_H

#ifndef NETSITE_H
#include "netsite.h"
#endif

NSPR_BEGIN_EXTERN_C

/*
 * platform_set sets platform information for subsequent platform_get_subdir
 * and platform_get_bitiness calls.
 */
NSAPI_PUBLIC void INTplatform_set(const char *subdir, int bits);

/*
 * platform_get_subdir returns the name of the subdirectory that contains
 * platform-specific binaries.  If platform_set was called, returns the
 * subdirectory that contains that platform's binaries.  Otherwise, returns
 * the subdirectory that contains the currently executing binary.  Returns
 * NULL for the default platform (i.e. when there is no subdirectory).
 */
NSAPI_PUBLIC const char *INTplatform_get_subdir(void);

/*
 * platform_get_bitiness returns the number of bits in the platform's
 * pointers.  If platform_set was called, returns the bitiness of that
 * platform.  Otherwise, returns the bitiness of the currently executing
 * binary.
 */
NSAPI_PUBLIC int INTplatform_get_bitiness(void);

NSPR_END_EXTERN_C

#define platform_set INTplatform_set
#define platform_get_bitiness INTplatform_get_bitiness
#define platform_get_subdir INTplatform_get_subdir

#endif /* BASE_PLATFORM_H */
