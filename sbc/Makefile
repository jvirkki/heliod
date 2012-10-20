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

BUILD_ROOT=..
include $(BUILD_ROOT)/make/base.mk

ifndef SBC
$(error Build needs SBC envvar defined to know where to go)
endif

ifndef SBC_SRC
$(error Build needs SBC_SRC envvar defined to know where to find sources)
endif

ifndef SBC_WORK_DIR
SBC_WORK_DIR=/tmp/sbc-work-dir
endif

default:
	@echo pick platform target to build

linux32:
	mkdir -p $(SBC_WORK_DIR)
	(cd ldapsdk && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		$(MAKE) -f Makefile.linux build32)
	(cd ldapsdk && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) DEBUG=1 \
		$(MAKE) -f Makefile.linux build32)

linux64:
	mkdir -p $(SBC_WORK_DIR)
	@echo not tested yet

solaris:
	mkdir -p $(SBC_WORK_DIR)
	(cd ldapsdk && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		$(MAKE) -f Makefile.solaris build32)
	(cd ldapsdk && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) DEBUG=1 \
		$(MAKE) -f Makefile.solaris build32)
	(cd ldapsdk && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		$(MAKE) -f Makefile.solaris build64)
	(cd ldapsdk && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) DEBUG=1 \
		$(MAKE) -f Makefile.solaris build64)
	(cd pcre && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		$(MAKE) -f Makefile.solaris build32)
	(cd pcre && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) DEBUG=1 \
		$(MAKE) -f Makefile.solaris build32)
	(cd pcre && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		BUILD64=1 \
		$(MAKE) -f Makefile.solaris build64)
	(cd pcre && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		DEBUG=1 BUILD64=1 \
		$(MAKE) -f Makefile.solaris build64)
	(cd xerces-c && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		$(MAKE) -f Makefile.solaris build64)
	(cd xerces-c && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) DEBUG=1 \
		$(MAKE) -f Makefile.solaris build64)
	(cd xerces-c && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		$(MAKE) -f Makefile.solaris build32)
	(cd xerces-c && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) DEBUG=1 \
		$(MAKE) -f Makefile.solaris build32)
	(cd xalan-c && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		$(MAKE) -f Makefile.solaris build64)
	(cd xalan-c && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) DEBUG=1 \
		$(MAKE) -f Makefile.solaris build64)
	(cd xalan-c && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) \
		$(MAKE) -f Makefile.solaris build32)
	(cd xalan-c && \
		SBC_WORK_DIR=$(SBC_WORK_DIR) DEBUG=1 \
		$(MAKE) -f Makefile.solaris build32)
