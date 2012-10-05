#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2008 Sun Microsystems, Inc. All rights reserved.
#
# THE BSD LICENSE
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer. 
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
#
# Neither the name of the  nor the names of its contributors may be
# used to endorse or promote products derived from this software without 
# specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

ifeq ($(OS_ARCH),WINNT)
SAFSOBJS = cgi ntwincgi
else
# ChildExec and CExecReqPipe comprise the Cgistub interface
SAFSOBJS = cgi ChildExec CExecReqPipe
endif

SAFSOBJS+=aclsafs
SAFSOBJS+=init
SAFSOBJS+=addlog
SAFSOBJS+=flexlog
SAFSOBJS+=auth
SAFSOBJS+=digest
SAFSOBJS+=clauth
SAFSOBJS+=ntrans
SAFSOBJS+=otype
SAFSOBJS+=pcheck
SAFSOBJS+=service
SAFSOBJS+=dl
SAFSOBJS+=nsfcsafs
SAFSOBJS+=nstpsafs
SAFSOBJS+=nsapicachesaf
SAFSOBJS+=perf
SAFSOBJS+=poolsafs
SAFSOBJS+=preencrypted
SAFSOBJS+=ntconsafs
SAFSOBJS+=init_fn
SAFSOBJS+=index
SAFSOBJS+=upload
SAFSOBJS+=nsconfig
SAFSOBJS+=deprecated
SAFSOBJS+=qos
SAFSOBJS+=reconfig
SAFSOBJS+=favicon
SAFSOBJS+=debug
SAFSOBJS+=var
SAFSOBJS+=cond
SAFSOBJS+=filtersafs
SAFSOBJS+=headerfooter
SAFSOBJS+=trace
SAFSOBJS+=httpcompression
SAFSOBJS+=sed
SAFSOBJS+=reqlimit
SAFSOBJS+=control
SAFSOBJS+=child
SAFSOBJS+=dump
SAFSOBJS+=logsafs
SAFSOBJS+=errorbong
