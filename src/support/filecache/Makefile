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

#
#           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
#              NETSCAPE COMMUNICATIONS CORPORATION
# Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
# Reserved.  Use of this Source Code is subject to the terms of the
# applicable license agreement from Netscape Communications Corporation.
# The copyright notice(s) in this Source Code does not indicate actual or
# intended publication of this Source Code.
#

BUILD_ROOT=../../..
USE_NSPR=1
USE_MAPFILE=1
HP_NO_CORBA_PCH=1

# disable position independent code to improve execution speed
NO_KPIC=1

include $(BUILD_ROOT)/make/defines.mk
MODULE=support

all::

LOCAL_LIBDIRS+=../version/$(OBJDIR)
LOCAL_LIBDIRS+=../time/$(OBJDIR)
LOCAL_INC +=-I. -I..
LOCAL_INC +=-I../version
LOCAL_DEF += -DBUILD_NSFC_DLL

#find md objects
ifeq ($(OS_ARCH),WINNT)
  MDSRC = md_win32.cpp
  DLL_LIBS+=kernel32
else
  MDSRC = md_unix.cpp
endif

CPPSRCS =	alloc.cpp \
		filecopy.cpp \
		fileio.cpp \
		filename.cpp \
		init.cpp \
		instance.cpp \
		$(MDSRC)

SHIP_PRIVATE_DYNAMIC_LIBRARIES=nsfc

DLL_TARGET=nsfc
DLL_OBJS=$(CPPSRCS:.cpp=)
DLL_LIBS+=nstime
DLL_DESC="File cache module"
DLL_VER=1.0
DLL_COPYRIGHT="Copyright (c) 1998-1999 Netscape Communications Corporation"

include $(BUILD_ROOT)/make/rules.mk
