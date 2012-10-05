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
USE_NSPR=1
USE_CLIENTLIBS=1
USE_MAPFILE=1
USE_XERCESC=1
MODULE=fastcgi

include $(BUILD_ROOT)/make/defines.mk

all::

LOCAL_DEF+=-DBUILD_DLL
LOCAL_DEF+=-DUSING_NSAPI

LOCAL_INC=-I../.. -I../../public -I../../../support
LOCAL_LIBDIRS+=../../webservd/$(OBJDIR)
LOCAL_LIBDIRS+=../../libserverxml/$(OBJDIR)
LOCAL_LIBDIRS+=../../libsi18n/$(OBJDIR)
LOCAL_LIBDIRS+=../../../support/support/$(OBJDIR)

DLL_TARGET=fastcgi
DLL_OBJS=errortypes util fastcgii18n serverconfig fcgirequest fcgiparser serverchannel 
DLL_OBJS+=server servermanager stubexec baserole fcgirole nsapifastcgi
DLL_LIBS=$(DAEMON_DLL) serverxml support libsi18n $(CLIENTLIBS)

ifeq ($(OS_ARCH),WINNT)
LOCAL_LIBDIRS+=../../../support/libxsd2cpp/$(OBJDIR)
DLL_LIBS+=xsd2cpp
endif #WINNT

DLL_PLUGIN=1
SHIP_PLUGIN_DYNAMIC_LIBRARIES+=$(DLL_TARGET)

SOMETHING_EXPORTED=1

MAKE_FCGISTUB=$(MAKE) -f Makefile.fastcgistub MAKEFLAGS='$(MAKEFLAGS) $(EXTRA_MAKEFLAGS)' $@ || exit 255

link::
	$(MAKE_FCGISTUB)

publish_copy::
	$(MAKE_FCGISTUB)

include $(BUILD_ROOT)/make/rules.mk
