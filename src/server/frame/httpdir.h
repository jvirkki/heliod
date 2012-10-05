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

#ifndef FRAME_HTTPDIR_H
#define FRAME_HTTPDIR_H

/*
 * NSAPIPhase defines the indices of the dt array in an httpd_object.
 * Each dtable corresponds to a particular phase of NSAPI processing.
 */
enum NSAPIPhase {
    NSAPIAuthTrans = 0,
    NSAPINameTrans,
    NSAPIPathCheck,
    NSAPIObjectType,
    NSAPIService,
    NSAPIError,
    NSAPIAddLog,
    NSAPIRoute,
    NSAPIDNS,
    NSAPIConnect,
    NSAPIFilter,
    NSAPIInput,
    NSAPIOutput,
    NSAPIMaxPhase
};

typedef enum NSAPIPhase NSAPIPhase;

#define NUM_DIRECTIVES NSAPIMaxPhase

PR_BEGIN_EXTERN_C

/*
 * directive_name2num will return the position of the abbreviated directive
 * dir in the directive table.
 * 
 * If dir does not exist in the table, it will return -1.
 */
NSAPI_PUBLIC int directive_name2num(const char *dir);

/*
 * directive_num2name returns a string describing directive number num.
 */
NSAPI_PUBLIC const char *directive_num2name(int num);

PR_END_EXTERN_C

#endif /* FRAME_HTTPDIR_H */
