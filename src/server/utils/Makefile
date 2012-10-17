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

# Modified 2011 jyri@virkki.com


BUILD_ROOT=../../..
MODULE=parsexml
USE_NSPR=1
USE_XERCESC=1
USE_XALANC=1
USE_LDAPSDK=1
USE_PCRE=1

include $(BUILD_ROOT)/make/base.mk

DIRS=true

ifneq ($(OS_ARCH),WINNT)
LOCAL_INC+=-I../../support
LOCAL_INC+=-I..
EXE_LIBS=$(DAEMON_DLL) serverxml xsd2cpp ServerXMLSchema libsi18n support

LOCAL_LIBDIRS+=../webservd/$(OBJDIR) ../libserverxml/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/libxsd2cpp/$(OBJDIR)
LOCAL_LIBDIRS+= ../libsi18n/$(OBJDIR) 
LOCAL_LIBDIRS+= ../../support/support/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/filecache/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/threadpool/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/time/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/NsprWrap/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/getproperty/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/ares/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/xp/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/libdbm/$(OBJDIR)
LOCAL_DEF+= -DJNI_MD_SYSNAME=\"$(JNI_MD_SYSNAME)\" -DJNI_MD_SYSNAME64=\"$(JNI_MD_SYSNAME64)\"

EXE_TARGET=parsexml
EXE_OBJS=parsexml

SHIP_PRIVATE_BINARIES=$(EXE_TARGET)
endif #!WINNT

LOCAL_LIBDIRS += $(OBJDIR)
LOCAL_LIBDIRS += $(LDAPSDK_LIBDIR)

ifeq ($(OS_ARCH), HP-UX)
SYSTEM_LIB+=Csup stream
endif

DLL1_LIBS+=support $(DAEMON_DLL)
DLL1_OBJS+=ServerControl
DLL1_OBJS+=UnixServerControl
DLL1_OBJS+=ServerControlHelper

DLL1_TARGET=wsserverctrl
SHIP_PRIVATE_DYNAMIC_LIBRARIES+=$(DLL1_TARGET)

EXE1_TARGET=svrctl
EXE1_OBJS=ServerControlCLI
EXE1_LIBS=wsserverctrl
ifeq ($(OS_ARCH),Linux)
LOCAL_LD_OPTS+=$(addprefix -Xlinker -rpath-link -Xlinker ,$(LOCAL_LIBDIRS))
endif
SHIP_PRIVATE_BINARIES+=$(EXE1_TARGET)

include $(BUILD_ROOT)/make/rules.mk
