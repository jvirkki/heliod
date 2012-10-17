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
USE_CLIENTLIBS=1
USE_PCRE=1
MODULE=httptests
include $(BUILD_ROOT)/make/base.mk

all:: compile

LOCAL_INC+=-I../../support -I$(OBJDIR)
LOCAL_LIBDIRS+=../../support/time/$(OBJDIR)
LOCAL_LIBDIRS+=../../support/support/$(OBJDIR)
LOCAL_LIBDIRS+=../../support/NsprWrap/$(OBJDIR)
LOCAL_LIBDIRS+=../../support/libdbm/$(OBJDIR)

EXE_OBJS=main tests regex_scrubber regex_string regex_list regex_entry engine http log nscperror request response utils basic http11 negative
EXE_LIBS=support nsprwrap nstime $(CLIENTLIBS) libdbm
EXE_TARGET=httptest

SHIP_PRIVATE_BINARIES=$(EXE_TARGET)

$(OBJDIR)/arch.h: 
	$(MKDIR_DASH_P) $(OBJDIR) || echo $(OBJDIR) already exists
	echo '#define ARCH "'$(OS_ARCH)'"' > $(OBJDIR)/arch.h

compile:: $(OBJDIR)/arch.h

include $(BUILD_ROOT)/make/rules.mk
