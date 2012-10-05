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

include $(BUILD_ROOT)/make/defines.mk
MODULE=support

all::

SUPPORT_CPPSRCS = PseudoRandomGenerator.cpp \
        DynBuf.cpp \
        NSString.cpp \
        NSTokenizer.cpp \
        SimpleHash.cpp \
        nspool.cpp \
        nsmalloc.cpp \
        nsshexp.cpp \
        BaseCache.cpp \
        CacheConfig.cpp \
        GenericList.cpp \
        GenericVector.cpp \
        xmloutput.cpp \
        urimapper.cpp \
        EreportableException.cpp

CPPSRCS=$(SUPPORT_CPPSRCS)
#VerRead.cpp

EXPORT_LIBRARIES=support_stat
SHIP_PRIVATE_DYNAMIC_LIBRARIES=support

# needed for GAT
EXPORT_TESTS_DIR=httptests
EXPORT_TESTS_DYNAMIC_LIBRARIES=$(SHIP_PRIVATE_DYNAMIC_LIBRARIES)

LOCAL_INC+=-I. -I..
LOCAL_INC+=-I../version
LOCAL_LIBDIRS+=../NsprWrap/$(OBJDIR)
LOCAL_LIBDIRS+=../version/$(OBJDIR)
all::

DEFINES+=-DBUILD_SUPPORT_DLL

SimpleHash.cpp_CC_FLAGS+=-DBUILD_HASH_DLL
AR_TARGET=support_stat
AR_OBJS=$(CPPSRCS:.cpp=)
AR_LIBS=nsprwrap

DLL_TARGET=support
DLL_OBJS=$(CPPSRCS:.cpp=)
DLL_LIBS=nsprwrap
DLL_VER=1.0
DLL_DESC="Enterprise 4.0 runtime support library"
DLL_COPYRIGHT="Copyright (c) 1997-98 Netscape Communications Corporation"

#EXE_TARGET=VerRead
#EXE_OBJS=VerRead
#EXE_LIBS=
#EXE_DESC="Enterprise 4.0 version extraction tool"
#DLL_COPYRIGHT="Copyright (c) 1997-98 Netscape Communications Corporation"
#EXE_VER=1.0

include $(BUILD_ROOT)/make/rules.mk

