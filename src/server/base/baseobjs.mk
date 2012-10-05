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

# include for base objects
# All libbase objects go here so httd dll and libbase build can pick them up.

ifeq ($(OS_ARCH), WINNT)
BASEOBJS=eventlog eventhandler ntpipe pathnames ntservermessage
else
BASEOBJS=wdservermessage
BASEOBJS+=unix_utils
endif

BASEOBJS +=daemon
BASEOBJS +=shexp
BASEOBJS +=regexp
BASEOBJS +=pblock
BASEOBJS +=plist
BASEOBJS +=buffer
BASEOBJS +=netbuf
BASEOBJS +=file
BASEOBJS +=net
BASEOBJS +=session
BASEOBJS +=cinfo
BASEOBJS +=util
BASEOBJS +=util_rng
BASEOBJS +=ereport
BASEOBJS +=sem
BASEOBJS +=systhr
BASEOBJS +=crit
BASEOBJS +=rwlock
BASEOBJS +=dns
BASEOBJS +=dnsdmain
BASEOBJS +=shmem
BASEOBJS +=dll
BASEOBJS +=lexer
BASEOBJS +=restart
BASEOBJS +=objndx
BASEOBJS +=cache
BASEOBJS +=pool
BASEOBJS +=date
BASEOBJS +=dns_cache
BASEOBJS +=system
BASEOBJS +=nscperror
BASEOBJS +=language
BASEOBJS +=servnss
BASEOBJS +=servssl
BASEOBJS +=sslconf
BASEOBJS +=keyword
BASEOBJS +=params
BASEOBJS +=SemPool
BASEOBJS +=vs
BASEOBJS +=uri
BASEOBJS +=strtok
BASEOBJS +=loadavg
BASEOBJS +=uuencode
BASEOBJS +=url64
BASEOBJS +=uuid
BASEOBJS +=qvalue
BASEOBJS +=netlayer
BASEOBJS +=arcfour
BASEOBJS +=platform
