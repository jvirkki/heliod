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

/*
 * httpdir.c: Directives for HTTP and Proxy servers
 * 
 * Rob McCool
 */



#include "base/systems.h"
#include "base/util.h"

#include "frame/httpdir.h"


/* ------------------------------------------------------------------------ */
/* ------------------- Lowest level: Define directives -------------------- */
/* ------------------------------------------------------------------------ */


struct dstruct {
    const char *name;
    int len;
};

/* Order must match the NSAPIPhase enum */
static struct dstruct dname[] = {
    {"AuthTrans", 9},
    {"NameTrans", 9},
    {"PathCheck", 9},
    {"ObjectType", 10},
    {"Service", 7},
    {"Error", 5},
    {"AddLog", 6},
    {"Route", 5},
    {"DNS", 3},
    {"Connect", 7},
    {"Filter", 6},
    {"Input", 5},
    {"Output", 6}
};


/* -------------------------- directive_name2num -------------------------- */


NSAPI_PUBLIC int directive_name2num(const char *dir) 
{
    register int x;

    for(x = 0; x < NUM_DIRECTIVES; x++) {
        if(!strncasecmp(dir,dname[x].name,dname[x].len))
            return x;
    }
    return -1;
}

/* -------------------------- directive_num2name -------------------------- */


NSAPI_PUBLIC const char *directive_num2name(int num) 
{
    return dname[num].name;
}
