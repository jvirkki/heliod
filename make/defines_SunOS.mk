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

#PHP plugin available for this platform
FEAT_PHP=1

# Support for SMF Integration
FEAT_SMF=1

# SMF files needed for the distribution
LIBSCF_SUFFIX=1
SCF_LIB=scf_jni
SMF_EXPORTS= $(addprefix $(LIBPREFIX), \
             $(addsuffix .$(DYNAMIC_LIB_SUFFIX).$(LIBSCF_SUFFIX), \
             $(SCF_LIB)))





LOCAL_COPY=0


# Tool locations
C++C		=$(PRE_CC) $(SUNWSPRO_DIR)/bin/CC
CC		=$(PRE_CC) $(SUNWSPRO_DIR)/bin/CC
C               =$(PRE_C)  $(SUNWSPRO_DIR)/bin/cc
AS              =$(SUNWSPRO_DIR)/bin/CC
AR              =/usr/ccs/bin/ar
RANLIB          =/usr/ccs/bin/ranlib
YACC            =/usr/ccs/bin/yacc
LD		=$(PRE_LD) /usr/ccs/bin/ld
PROFILER	=$(SUNWSPRO_DIR)/bin/profile
FTP		=/usr/bin/ftp
LINT		=$(SUNWSPRO_DIR)/bin/lint
STRIP		=/usr/ccs/bin/strip -x
SPEC2MAP	=/usr/lib/abi/spec2map -a i386
ZIP		=/usr/bin/zip
UNZIP		=/usr/bin/unzip

ifdef DEBUG_BUILD
CC_DEBUG	= -g -xs
C_DEBUG         = -g
else
CC_DEBUG        = -xO4
C_DEBUG         = -xO4
endif

BASEFLAGS = 

JNI_MD_NAME	= solaris

LD_DYNAMIC	= -G
LD_SYMBOLIC	= -Bsymbolic
ARFLAGS		= -r
PLATFORM_DEF	= -DSVR4 -DSYSV -DSOLARIS -D_REENTRANT 
PLATFORM_LIB	= $(PRE_PLATFORM_LIB) pthread socket nsl dl posix4 kstat Crun Cstd
PLATFORM_CC_OPTS = -mt $(BASEFLAGS)
PLATFORM_C_OPTS  = -mt $(BASEFLAGS)
PLATFORM_AS_OPTS = -mt -D_ASM $(BASEFLAGS)
PLATFORM_LD_OPTS = -mt -norunpath
RPATH_PREFIX = -R 
RPATH_ORIGIN = \$$ORIGIN
ifndef NO_KPIC
PLATFORM_CC_OPTS += -xcode=pic32
PLATFORM_C_OPTS  += -xcode=pic32
PLATFORM_AS_OPTS += -xcode=pic32
endif
SYSTEM_LIBDIRS += $(SUNWSPRO_DIR)/lib

ifdef BUILD64
PLATFORM_CC_OPTS += -m64
PLATFORM_C_OPTS  += -m64
PLATFORM_AS_OPTS += -m64
PLATFORM_LD_OPTS += -m64
else
PLATFORM_CC_OPTS += -m32
PLATFORM_C_OPTS  += -m32
PLATFORM_AS_OPTS +=- m32
PLATFORM_LD_OPTS += -m32
endif

# Mapfile generation.  Set USE_MAPFILE=1 to require a mapfile for each so.
#USE_MAPFILE=1
MAPFILE_SUFFIX=mapfile
SPECFILE_SUFFIX=spec
VERSIONSFILE_SUFFIX=versions

# template database location.
ifndef NO_STD_DATABASE_DEFINE
TEMPLATE_DATABASE_DIR=$(OBJDIR)
endif

ifdef TEMPLATE_DATABASE_DIR
# CC_FLAGS += -ptr$(TEMPLATE_DATABASE_DIR)
# LD_FLAGS += -ptr$(TEMPLATE_DATABASE_DIR)
endif

# libxml2 (used by NSAPI filter tests) is part of OS, external not needed
NO_LIBXML2=1

IDS_NSPR_DIR    =$(NSPR_DIR)
IDS_LDAPSDK_DIR =$(LDAPSDK_DIR)
SETUPSDK_JNIDIR = Unix/Solaris/X86

# Enables OS specific stats collection
FEAT_PLATFORM_STATS=1

# Enables creation of solaris packages
FEAT_OS_NATIVE_PKG=1
# Enables creation of solaris patch packages
FEAT_OS_PATCH_PKG=1

# Add platform specific NSS modules
SECURITY_MODULE_LIBS += freebl3

# Add platform specific JES patch dependencies
REQUIRE_LIST = ""

SASL_LIBS  = sasl

LDAPSDK_VERSION=6_0_5
XERCES_VERSION=2_7_0
XALAN_VERSION=1_10_0
PCRE_VERSION=8.02

NSPR_INC=-I/usr/include/mps
NSS_INC=-I/usr/include/mps

ifdef BUILD64
NSPR_LIBDIR=/usr/lib/mps/64
NSS_LIBDIR=/usr/lib/mps/64
else
NSPR_LIBDIR=/usr/lib/mps
NSS_LIBDIR=/usr/lib/mps
endif

LDAPSDK_INC=-I$(SBC)/ldapsdk/$(LDAPSDK_VERSION)/$(OBJDIR)/include
LDAPSDK_LIBDIR=$(SBC)/ldapsdk/$(LDAPSDK_VERSION)/$(OBJDIR)/lib
ZLIB_INC=
ZLIB_LIBDIR=
XERCESC_INC=-I$(SBC)/xerces-c/$(XERCES_VERSION)/$(OBJDIR)/include
XERCESC_LIBDIR=$(SBC)/xerces-c/$(XERCES_VERSION)/$(OBJDIR)/lib
XALANC_INC=-I$(SBC)/xalan-c/$(XALAN_VERSION)/$(OBJDIR)/include
XALANC_LIBDIR=$(SBC)/xalan-c/$(XALAN_VERSION)/$(OBJDIR)/lib
PCRE_INC=-I$(SBC)/pcre/$(PCRE_VERSION)/$(OBJDIR)/include
PCRE_LIBDIR=$(SBC)/pcre/$(PCRE_VERSION)/$(OBJDIR)/lib
ICU_INC=
ICU_LIBDIR=
SASL_INC=SASL_INCLUDE_NOT_SET_YET
SASL_LIBDIR=
PLATFORM_LIBPATH=$(NSS_LIBDIR):$(LDAPSDK_LIBDIR):$(XERCESC_LIBDIR):$(XALANC_LIBDIR):$(PCRE_LIBDIR)

RUNTIME_LIBDIR=$(NSS_LIBDIR)
