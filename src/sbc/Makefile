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

# Copyright 2011 jyri@virkki.com

BUILD_ROOT=../..
include $(BUILD_ROOT)/make/defines.mk

# 
# publish will copy into the staging lib directory those libraries which
# are needed by each platform at runtime which are not otherwise available.
# 

publish:
	@echo sbc publish copying libraries needed by runtime
	@echo BUILD_OS is $(BUILD_OS)
ifeq ($(BUILD_OS),Linux)
ifeq ($(LINUX_DISTRO),Debian)
	@echo Linux.Debian needs ldapsdk in $(WORK_ROOT)/lib
	(cd $(LDAPSDK_LIBDIR) && tar cf - *.so) | \
		(cd $(WORK_ROOT)/lib && tar xf -)
endif
endif

ifeq ($(BUILD_OS),SOLARIS)
	@echo Solaris needs ldapsdk, xercesc, xalanc, pcre $(WORK_ROOT)/lib
	(cd $(LDAPSDK_LIBDIR) && tar cf - *.so) | \
		(cd $(WORK_ROOT)/lib && tar xf -)
	(cd $(XERCESC_LIBDIR) && tar cf - *) | \
		(cd $(WORK_ROOT)/lib && tar xf -)
	(cd $(XALANC_LIBDIR) && tar cf - *) | \
		(cd $(WORK_ROOT)/lib && tar xf -)
	(cd $(PCRE_LIBDIR) && tar cf - *) | \
		(cd $(WORK_ROOT)/lib && tar xf -)
endif

clobber:
	@echo sbc clobber

all:
	@echo sbc all
