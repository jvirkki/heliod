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

# Maintain list of objects separately for utility.

DAEMONOBJS=
ifneq ($(OS_ARCH), WINNT)
DAEMONOBJS+=uxpolladapter
DAEMONOBJS+=WatchdogClient
endif

DAEMONOBJS+=daemonsession
DAEMONOBJS+=httprequest
DAEMONOBJS+=httpheader
DAEMONOBJS+=HttpMethodRegistry
DAEMONOBJS+=pollarray
DAEMONOBJS+=kapollthr
DAEMONOBJS+=pollmanager
DAEMONOBJS+=connqueue
DAEMONOBJS+=acceptconn
DAEMONOBJS+=configuration
DAEMONOBJS+=configurationmanager
DAEMONOBJS+=vsmanager
DAEMONOBJS+=nvpairs
DAEMONOBJS+=ListenSocketConfig
DAEMONOBJS+=ListenSocket
DAEMONOBJS+=ListenSockets
DAEMONOBJS+=ListenSocketConfigHash
DAEMONOBJS+=vsconf
DAEMONOBJS+=servername
DAEMONOBJS+=mime
DAEMONOBJS+=AuthDb
DAEMONOBJS+=statsutil
DAEMONOBJS+=statsnodes
DAEMONOBJS+=statsmessage
DAEMONOBJS+=StatsMsgHandler
DAEMONOBJS+=statsavg
DAEMONOBJS+=statsmanager
DAEMONOBJS+=StatsMsgPreparer
DAEMONOBJS+=statssession
DAEMONOBJS+=statsbkupmgr
DAEMONOBJS+=StatsClient
DAEMONOBJS+=throttling
DAEMONOBJS+=logmanager
DAEMONOBJS+=stdhandles
DAEMONOBJS+=lognsprdescriptor
DAEMONOBJS+=JavaConfig
ifneq ($(OS_ARCH), WINNT)
DAEMONOBJS+=UnixSignals
DAEMONOBJS+=ParentAdmin
DAEMONOBJS+=ChildAdminThread
DAEMONOBJS+=ParentStats
else
DAEMONOBJS+=NTStatsServer
endif
ifdef USE_SMARTHEAP
DAEMONOBJS+=smartheapinit
endif
DAEMONOBJS+=WebServer
ifeq ($(OS_ARCH), SunOS)
DAEMONOBJS+=solariskernelstats
DAEMONOBJS+=solarisprocessstats
endif
ifeq ($(OS_ARCH), Linux)
DAEMONOBJS+=linuxkernelstats
DAEMONOBJS+=linuxprocessstats
DAEMONOBJS+=fileutility
endif
ifeq ($(OS_ARCH), HP-UX)
DAEMONOBJS+=hpuxkernelstats
DAEMONOBJS+=hpuxprocessstats
endif
DAEMONOBJS+=scheduler
DAEMONOBJS+=updatecrl
