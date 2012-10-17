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

BUILD_ROOT=../../..

USE_NSPR=1
USE_MAPFILE=1
HP_NO_CORBA_PCH=1

# disable position independent code to improve execution speed
NO_KPIC=1

include $(BUILD_ROOT)/make/base.mk

all::

MODULE=NsprWrap

CPPSRCS=ReadWriteLock.cpp ThreadSafe.cpp LongLong.cpp countingsemaphore.cpp \
        IntervalTimer.cpp atomic.cpp Thread.cpp Sleeper.cpp ProcessLock.cpp \
        NsprDescriptor.cpp NsprSink.cpp NsprBuffer.cpp NsprError.cpp
ifeq ($(OS_ARCH),SunOS)
DEFINES+=-DHAVE_TIME_R -DHW_THREADS
else
DEFINES+=-DUSE_MONITOR
endif

ifeq ($(OS_ARCH), WINNT)
SHIP_PRIVATE_DYNAMIC_LIBRARIES=nsprwrap libnspr21 libplc21
else
SHIP_PRIVATE_DYNAMIC_LIBRARIES=nsprwrap
endif

# needed for GAT
EXPORT_TESTS_DIR=httptests
EXPORT_TESTS_DYNAMIC_LIBRARIES=$(SHIP_PRIVATE_DYNAMIC_LIBRARIES)

LOCAL_LIBDIRS+=../version/$(OBJDIR)
LOCAL_INC+=-I../version
DEFINES+=-DBUILD_NSPRWRAP_DLL

DLL_TARGET=nsprwrap
DLL_OBJS=$(CPPSRCS:.cpp=)
DLL_DESC="C++ Wrappers around the C Netscape Portable Runtime"
DLL_VER=1.0
DLL_COPYRIGHT="Copyright (c) 1997-1998 Netscape Communications Corporation"
ifeq ($(OS_ARCH), WINNT)
DLL1_TARGET=libnspr21
DLL1_RES=NsprThunk
DLL1_OBJS=NsprThunk
DLL1_DESC="Lib thunk layer for C Netscape Portable Runtime"
DLL1_VER=2.1
DLL_COPYRIGHT="Copyright (c) 1997-1998 Netscape Communications Corporation"
DLL2_TARGET=libplc21
DLL2_RES=PlcThunk
DLL2_OBJS=PlcThunk
DLL2_DESC="Lib thunk layer for C Netscape Portable Runtime"
DLL2_VER=2.1
DLL2_COPYRIGHT="Copyright (c) 1997-1998 Netscape Communications Corporation"
endif

# this should always be last!
include ${BUILD_ROOT}/make/rules.mk


