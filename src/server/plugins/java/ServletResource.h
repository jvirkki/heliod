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


#ifndef _ServletResource_h_
#define _ServletResource_h_

#include <stdio.h>
#include <ctype.h>

class ServletResource {

    public:
        ServletResource(const char* res, long wrapperObj,
                        PRBool content);
        ~ServletResource();
        PRBool hasContent();
        const char* getResourceName();
        int getResourceNameLen();
        long getServletWrapper();
        PRBool isSpecial();
        void setSpecial(PRBool flag);

    private:
        PRBool content;
        char*  resourceName; 
        int    resourceNameLen;
        long   jServletWrapper; // Java servlet wrapper Object
        PRBool special;
} ;

inline PRBool ServletResource::hasContent() {
    return content;
}

inline const char* ServletResource::getResourceName() {
    return resourceName;
}

inline int ServletResource::getResourceNameLen() {
    return resourceNameLen;
}

inline long ServletResource::getServletWrapper() {
    return jServletWrapper;
}

inline PRBool ServletResource::isSpecial() {
    return special;
}

#endif /* _ServletResource_h_ */
