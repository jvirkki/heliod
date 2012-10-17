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

# The framework library contains the functions common to the netsite 
# servers, to implement configuration, server application functions,
# and speak network protocols. Depends on the base library for
# definitions.
BUILD_ROOT=../../..
USE_NSPR=1
USE_LDAPSDK=1
USE_CLIENTLIBS=1

include ${BUILD_ROOT}/make/base.mk

MODULE=LibLdapUtil

compile:: listldap

LOCAL_INC=-I. -I.. -I../../support
LOCAL_DEF+= -DUSE_LDAP_SSL
LOCAL_DEF+= -DPEER_SNMP
LOCAL_DEF+= -DBUILD_DLL
LOCAL_DEF+= -DBUILD_LDAPSESSION

#LOCAL_DEF+=-DLDAPDB_THREAD_SAFE

LOCAL_HEADERS=LdapEntry.h LdapSearchResult.h LdapSession.h

include ldaputilobjs.mk

CPPSRCS=$(LDAPUTILOBJS:=.cpp)

# We need everything that goes into to object directory
# especially on DEC which builds .o files for template stuff
# sorry - that won't work anymore - we need to add it to ldaputilobjs.mk
#AR_OBJS=$(patsubst $(OBJDIR)/%.$(OBJ),%,$(wildcard $(OBJDIR)/*.$(OBJ)))

AR_OBJS=$(LDAPUTILOBJS)
AR_TARGET=libldapu
EXPORT_LIBRARIES=$(AR_TARGET)

ifdef INCLUDE_UNIT_TEST
CPPSRCS += LdapSessionTest.cpp
EXE_TARGET = LdapSessionTest
EXE_OBJS   = LdapSessionTest $(LDAPUTILOBJS)
LOCAL_LIB  = $(SSLDAP_LIB) $(CLIENTLIBS) libdbm
LOCAL_BINARIES += LdapSessionTest
endif

listldap:
ifeq ($(OS_ARCH),OSF1)
	ls -lL $(BUILD_ROOT)/external/ldapsdk/$(OBJDIR)/include/*
endif

# this should always be last!
include ${BUILD_ROOT}/make/rules.mk
