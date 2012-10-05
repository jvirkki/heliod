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
USE_XERCESC=1
USE_XALANC=1

# disable position independent code to improve execution speed
NO_KPIC=1

include ${BUILD_ROOT}/make/defines.mk

MODULE=serverxml

LOCAL_INC+=-I.. -I../../support
LOCAL_INC+=-I$(OBJDIR)
LOCAL_LIBDIRS+=$(OBJDIR)

MAKE_XSD2CPP=$(MAKE) -f makefile.XSD2Cpp

#
# libServerXMLSchema.a - generated from heliod-web-server_1_0.xsd
#

SERVER_XSD2CPP_OPTIONS= \
	NAMESPACE=ServerXMLSchema \
	XSD=$(BUILD_ROOT)/schema/heliod-web-server_1_0.xsd \
	ROOT_ELEMENT=server \
	TYPE_MAPPINGS="intervalType=libxsd2cpp_1_0::IntervalTime \
	               largeIntervalType=libxsd2cpp_1_0::IntervalTime \
	               eventIntervalType=libxsd2cpp_1_0::IntervalTime \
	               pollIntervalType=libxsd2cpp_1_0::IntervalTime \
	               cacheUpdateIntervalType=libxsd2cpp_1_0::IntervalTime \
	               dynamicReloadIntervalType=libxsd2cpp_1_0::IntervalTime \
	               timeoutType=libxsd2cpp_1_0::IntervalTime \
	               nonZeroTimeoutType=libxsd2cpp_1_0::IntervalTime \
                       largeTimeoutType=libxsd2cpp_1_0::IntervalTime \
	               minLockTimeoutType=libxsd2cpp_1_0::IntervalTime"

all::

headers::
	$(MAKE_XSD2CPP) $(SERVER_XSD2CPP_OPTIONS) headers

compile::
	$(MAKE_XSD2CPP) $(SERVER_XSD2CPP_OPTIONS) libraries
	$(MAKE_XSD2CPP) $(SERVER_XSD2CPP_OPTIONS) publish

#
# libserverxml.a - server.xml parser
# 

DEFINES+=-DBUILD_LIBSERVERXML_DLL

AR_TARGET=$(MODULE)
AR_OBJS+=ServerXML
AR_OBJS+=ServerXMLParser
AR_OBJS+=ServerXMLExceptionContext
AR_OBJS+=NSPRInputSource
EXPORT_LIBRARIES=$(AR_TARGET)

# this should always be last!
include ${BUILD_ROOT}/make/rules.mk
