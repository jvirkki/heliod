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

BUILD_ROOT=..
USE_NSPR=1
USE_LDAPSDK=1
USE_CLIENTLIBS=1
USE_LIBICU=1
USE_XERCESC=1
USE_XALANC=1
USE_SMARTHEAP=1
USE_ZLIB=1
USE_PCRE=1

include $(BUILD_ROOT)/make/defines.mk

ifndef WS_INSTALL_ROOT
WS_INSTALL_ROOT=$(WORK_ROOT)
endif

ifndef WS_CONFIG_NAME
WS_CONFIG_NAME=test
endif

ifndef WS_INSTANCE_NAME
WS_INSTANCE_NAME=https-$(WS_CONFIG_NAME)
endif

ifndef WS_SERVER_USER
WS_SERVER_USER=$(shell logname)
endif

ifndef WS_HTTP_PORT
WS_HTTP_PORT=8080
endif

ifndef WS_SERVER_NAME
WS_SERVER_NAME=$(shell hostname)
endif

ifndef WS_INSTANCE_ROOT
WS_INSTANCE_ROOT=$(WS_INSTALL_ROOT)
endif

WS_SERVER_TEMPDIR=$(shell mktemp -u /tmp/$(WS_INSTANCE_NAME)-XXXXXX)

WS_VARS=WS_INSTALL_ROOT=$(WS_INSTALL_ROOT) WS_INSTANCE_ROOT=$(WS_INSTANCE_ROOT)
WS_VARS+=WS_CONFIG_NAME=$(WS_CONFIG_NAME) WS_INSTANCE_NAME=$(WS_INSTANCE_NAME)
WS_VARS+=WS_SERVER_USER=$(WS_SERVER_USER) WS_HTTP_PORT=$(WS_HTTP_PORT)
WS_VARS+=WS_SERVER_NAME=$(WS_SERVER_NAME) WS_SERVER_TEMPDIR=$(WS_SERVER_TEMPDIR)
WS_VARS+=WS_EXTRA_LIBPATH=$(PLATFORM_LIBPATH)

install:: bin lib-dtds lib-icons instance includes lib

bin::
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/bin
	$(PERL) replace_vars.pl $(WS_VARS) INPUT_DIR=$(BUILD_ROOT)/templates/install-bin/ OUTPUT_DIR=$(WS_INSTALL_ROOT)/bin/
	$(CHMOD) +x $(WS_INSTALL_ROOT)/bin/binlog
	#$(CP) TBD/flexanlg ./bin/flexanlg
	#$(CP) TBD ./bin/htpasswd

lib-dtds::
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/lib/dtds
	$(CP) $(BUILD_ROOT)/schema/heliod-web-server_1_0.xsd $(WS_INSTALL_ROOT)/lib/dtds/

lib-icons::
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/lib/icons
	$(CP) $(BUILD_ROOT)/templates/icons/* $(WS_INSTALL_ROOT)/lib/icons/

lib::
	cd $(BUILD_ROOT)/src; $(MAKE) BUILD_VARIANT=$(BUILD_VARIANT) SECURITY_POLICY=$(SECURITY_POLICY) publish
	$(PERL) replace_vars.pl $(WS_VARS) INPUT_DIR=$(BUILD_ROOT)/templates/lib/ OUTPUT_DIR=$(WS_INSTALL_ROOT)/lib/
	$(CHMOD) +x $(WS_INSTALL_ROOT)/lib/wsenv

includes::
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/include/
	cd $(BUILD_ROOT)/src/server/public; $(MAKE) BUILD_VARIANT=$(BUILD_VARIANT) SECURITY_POLICY=$(SECURITY_POLICY) all publish

instance:: logs instance-config instance-bin instance-docs instance-lib

logs::
	# Intially keep logs directory empty
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/logs/

instance-config::
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/config/
	$(PERL) replace_vars.pl $(WS_VARS) INPUT_DIR=$(BUILD_ROOT)/templates/config/ OUTPUT_DIR=$(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/config/
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/config.bak/
	$(CP) $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/config/* $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/config.bak/

instance-bin::
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/bin
	$(PERL) replace_vars.pl $(WS_VARS) INPUT_DIR=$(BUILD_ROOT)/templates/bin/ OUTPUT_DIR=$(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/bin/
	$(CHMOD) +x $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/bin/*

instance-docs::
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/docs
	$(CP) $(BUILD_ROOT)/templates/docs/*.html  $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/docs

instance-lib::
	$(MKDIR_DASH_P) $(WS_INSTALL_ROOT)/$(WS_INSTANCE_NAME)/lib
