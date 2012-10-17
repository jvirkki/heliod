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

MODULE=java
BUILD_ROOT=../..

BUILD_JAVA=1
USE_J2EE=

include $(BUILD_ROOT)/make/base.mk

#
# All default values for the variables defined in defines_JAVA_JDK_1.2.mk
# User definable variables are:
#    ANT_BUILD_FILE
#    ANT_OPTIONS
#    ANT_TARGETS
#

ANT_BUILD_FILE=build.xml

#
# The all rule simply invokes the antit target after setting up the environment
#
#ANT_OPTIONS += -verbose
ANT_OPTIONS += -Dwork.dir=$(WORK_ROOT) -Dinternal.dir=$(INTERNAL_ROOT) -Dplatformroot.dir=$(HOST_OBJDIR) -Dplatform.os=$(OS_ARCH)

define run-ant
	$(MAKE) MAKEFLAGS='$(MAKEFLAGS)' $(ANT_ENV) ANT_TARGETS="$@" antit
endef

all ant:
	$(MAKE) MAKEFLAGS='$(MAKEFLAGS)' $(ANT_ENV) antit

clean:
	$(run-ant)

clobber:
	$(run-ant)
   
compile:
	$(run-ant)

recompile:
	$(run-ant)

publish:
	$(run-ant)

compile-%: 
	$(run-ant)

publish-%: 
	$(run-ant)

help::
	$(MAKE) $(ANT_ENV) ANT_OPTIONS="-projecthelp" ANT_TARGETS="" antit

# This target should not be invoked directly from the command line
antit:
	$(ANT)
