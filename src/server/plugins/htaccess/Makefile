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

BUILD_ROOT=../../../..
USE_MAPFILE=1
USE_NSPR=1
MODULE=htaccess

include $(BUILD_ROOT)/make/base.mk

LOCAL_DEF+= -DBUILD_DLL
LOCAL_INC+= -I../.. -I../../../support
LOCAL_LIBDIRS+=../../libcrypt/$(OBJDIR)
LOCAL_LIBDIRS+= ../../webservd/$(OBJDIR) 
LOCAL_LIBDIRS+= ../../../support/support/$(OBJDIR) 

DLL_TARGET=htaccess
DLL_OBJS=http_access http_auth http_config http_register util main list
DLL_LIBS+=$(DAEMON_DLL)
DLL_LIBS+=support

EXE_OBJS=htpasswd
EXE_LIBS+=libcrypt
EXE_TARGET=htpasswd

# set libprefix to null since we dont want the shared lib to have a prefix
LIBPREFIX=
SHIP_PLUGIN_DYNAMIC_LIBRARIES=$(DLL_TARGET)
#SHIP_PLUGIN_HELPER_FILES=htconvert
SHIP_PUBLIC_BINARIES=$(EXE_TARGET)

include $(BUILD_ROOT)/make/rules.mk
