#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2012 Jyri J. Virkki. All rights reserved.
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

# This is the base include file for all makefiles.
#
# All it now does is include the base include file of the current platform.
# That file is responsible for determining what the platform needs.


# First get the details of current platform

PINFO=$(BUILD_ROOT)/make/pinfo
BASE_OS:=$(shell $(PINFO) -o)
BASE_VARIANT:=$(shell $(PINFO) -v)
BASE_PLATFORM:=$(shell $(PINFO) -p)
BASE_OSVERSION:=$(shell $(PINFO) -r)

ifdef BUILD64
BASE_BITS=64
else
BASE_BITS=32
endif

ifdef DEBUG
BASE_TYPE:=DBG
else
BASE_TYPE:=OPT
endif

BASE_STRING=$(BASE_OS)_$(BASE_VARIANT)_$(BASE_OSVERSION)_$(BASE_PLATFORM)_$(BASE_BITS)_$(BASE_TYPE)

# These calls find the common denominator include file for defines and
# rules to use.  We might have defines and rules files specific to
# BASE_STRING but this allows using a more common one if possible.

BASE_DEFINES_NAME:=$(shell $(PINFO) -bd $(BASE_BITS) $(BASE_TYPE) $(BUILD_ROOT))
BASE_RULES_NAME:=$(shell $(PINFO) -br $(BASE_BITS) $(BASE_TYPE) $(BUILD_ROOT))

# Compatibility defines go here.. These should be refactored out for
# cleanliness.

OS_ARCH=$(BASE_OS)
OBJDIR=$(BASE_STRING)
SHIPDIR=$(BASE_STRING)
JAVA_OBJDIR=$(BASE_STRING)

include ${BUILD_ROOT}/make/defines_COMMON.mk
include ${BUILD_ROOT}/make/defines_UNIX.mk
include ${BUILD_ROOT}/make/defines_WebServer.mk
include $(BASE_DEFINES_NAME)

ifdef BUILD_JAVA
include ${BUILD_ROOT}/make/defines_JAVA.mk
endif
