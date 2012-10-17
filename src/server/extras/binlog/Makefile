#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2009 Sun Microsystems, Inc. All rights reserved.
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

BUILD_ROOT=../../../..
USE_NSPR=1
USE_PCRE=1
USE_XERCESC=1
USE_XALANC=1

MODULE=binlog
include $(BUILD_ROOT)/make/base.mk

all::

# object list is here
LOCAL_SRC=binlog
CCSRCS=$(LOCAL_SRC:=.cpp)

LOCAL_INC=-I../../
LOCAL_INC+=-I../../../support

LOCAL_LIB+=ns-httpd40 support
LOCAL_LIB+= nsfc nsprwrap nstime nstp xp ares3 xsd2cpp libdbm libsi18n
LOCAL_LIB+= ssldap60 ldap60 prldap60
LOCAL_LIB+= softokn3
LOCAL_LIBDIRS+=../../webservd/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/support/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/filecache/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/NsprWrap/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/time/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/threadpool/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/xp/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/ares/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/libxsd2cpp/$(OBJDIR)/
LOCAL_LIBDIRS+=../../../support/libdbm/$(OBJDIR)/
LOCAL_LIBDIRS+=../../libsi18n/$(OBJDIR)/
LOCAL_LIBDIRS+=$(LDAPSDK_LIBDIR)
LOCAL_LIBDIRS+=$(NSS_LIBDIR)

EXE_TARGET=binlog
EXE_OBJS=binlog
EXE_LIBS+=ns-httpd40 support

SHIP_PRIVATE_BINARIES=binlog

# this should always be last!
include $(BUILD_ROOT)/make/rules.mk
