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


include ${BUILD_ROOT}/make/defines_Linux.mk

# Declare sbc components which need to be packaged for this platform:
SBC_PUBLISH_LDAPSDK=1
SBC_PUBLISH_XERCESC=1
SBC_PUBLISH_XALANC=1

LDAPSDK_VERSION=6_0_5
XERCES_VERSION=2_7_0
XALAN_VERSION=1_10_0


NSPR_INC=-I/usr/include/nspr4
NSPR_LIBDIR=
NSS_INC=-I/usr/include/nss3
NSS_LIBDIR=
LDAPSDK_INC=-I$(SBC)/ldapsdk/$(LDAPSDK_VERSION)/$(OBJDIR)/include
LDAPSDK_LIBDIR=$(SBC)/ldapsdk/$(LDAPSDK_VERSION)/$(OBJDIR)/lib
ZLIB_INC=
ZLIB_LIBDIR=
XERCESC_INC=-I$(SBC)/xerces-c/$(XERCES_VERSION)/$(OBJDIR)/include
XERCESC_LIBDIR=$(SBC)/xerces-c/$(XERCES_VERSION)/$(OBJDIR)/lib
XALANC_INC=-I$(SBC)/xalan-c/$(XALAN_VERSION)/$(OBJDIR)/include
XALANC_LIBDIR=$(SBC)/xalan-c/$(XALAN_VERSION)/$(OBJDIR)/lib
PCRE_INC=
PCRE_LIBDIR=
ICU_INC=
ICU_LIBDIR=
SASL_INC=
SASL_LIBDIR=

RUNTIME_LIBDIR=$(NSS_LIBDIR)
