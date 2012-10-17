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


# These sources cannot be built currently.

BUILD_ROOT=../../../..
USE_J2EE=1
USE_NSPR=1
USE_XERCESC=1
USE_LIBICU=1
USE_CLIENTLIBS=1
USE_MAPFILE=1

# disable position independent code to improve execution speed
NO_KPIC=1

MODULE=iasprototype
include $(BUILD_ROOT)/make/base.mk

J2EE_PROPERTIES_FILE=j2eeplugin

LOCAL_DEF += -DBUILD_J2EE_PLUGIN_DLL
ifeq ($(OS_ARCH), Linux)
LOCAL_DEF += -D_GNU_SOURCE
endif
LOCAL_DEF += -DJ2EE_PROPERTIES_FILE=\"$(J2EE_PROPERTIES_FILE)\"
# for jni
LOCAL_INC += $(JNI_INCLUDES)
# other includes
LOCAL_INC += -I../include
LOCAL_INC += -I$(JAVA_INCLUDE_DIR)

LOCAL_LIBDIRS += $(OBJDIR) $(JVM_LIBDIR) ../httpd/src/$(OBJDIR)

ifneq ($(OS_ARCH), WINNT)
ifneq ($(OS_ARCH), AIX)
# build a stub libjvm.so to avoid tying libj2eeplugin.so to a specific JVM
DLL1_OBJS=jvm
DLL1_TARGET=jvm
DLL_ADDITIONAL_DEPENDENCIES+=$(OBJDIR)/$(LIBPREFIX)$(DLL1_TARGET).$(DYNAMIC_LIB_SUFFIX)
endif
endif

DLL_OBJS+=NSJavaUtil
DLL_OBJS+=JVMControl NSAPIVirtualServer j2ee WebModule HashUtil
DLL_OBJS+=j2eefilter
DLL_OBJS+=Pkcs12Util NssStore
DLL_OBJS+=NSAPIConnector NSAPIRunner JavaStatsManager
DLL_OBJS+=MemMapFile MemMapSessionManager MMapSessionManager MMapSession
DLL_OBJS+=LockManager ServletResource

DLL_LIBS+=nstime support nsprwrap libsi18n xsd2cpp ns-httpd40 $(JNI_MD_LIBNAME) 
DLL_LIBS+=$(CLIENTLIBS)
DLL_TARGET=j2eeplugin
SHIP_PRIVATE_DYNAMIC_LIBRARIES=$(DLL_TARGET)

SHIP_ICU_RESOURCES=$(J2EE_PROPERTIES_FILE)_root

include $(BUILD_ROOT)/make/rules.mk
