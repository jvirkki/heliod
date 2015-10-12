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

#
# NOTE: This file is used by Enterprise, Messaging AND Collabra servers.
#       Please do NOT change indiscriminately
#

LOCAL_COPY=0

CC		=$(PRE_CC) /usr/bin/g++
C               =$(PRE_C)  /usr/bin/gcc
C++C		=$(CC)
AR              =/usr/bin/ar
TAR             =/bin/tar
RANLIB          =/usr/bin/ranlib
LD		=$(PRE_LD) /usr/bin/g++
ZIP		=/usr/bin/zip
UNZIP		=/usr/bin/unzip
GZIP		=/bin/gzip
GUNZIP		=/bin/gunzip
SHELL		=/bin/sh
DATE		=/bin/date
MKDIR		=/bin/mkdir
TOUCH		=/bin/touch
CHMOD		=/bin/chmod
CP		=/bin/cp
MV		=/bin/mv
ECHO		=/bin/echo -e
SED		=/bin/sed
MKDIR		=/bin/mkdir
LN		=/bin/ln -f
NMAKE		=/usr/bin/make -f
STRIP		=/usr/bin/strip -x
RM		=/bin/rm

# -L flag dereferences symbolic links, else symlinks are copied as is
CP_R            =/bin/cp -rL

ifndef GCC_VERSION
GCC_VERSION = $(shell $(C) -dumpversion)
endif

BASEFLAGS = -Wall

ifdef DEBUG_BUILD
# easy on the warnings for now
CC_DEBUG = -g $(BASEFLAGS) -Wno-unknown-pragmas -Wno-non-virtual-dtor -Wno-unused
C_DEBUG  = -g $(BASEFLAGS)
LD_DEBUG =
else 
# optimized settings here
CC_DEBUG  = -O3 $(BASEFLAGS) -Wno-unknown-pragmas -Wno-non-virtual-dtor -Wno-unused
C_DEBUG   = -O3 $(BASEFLAGS)
LD_DEBUG  = -s
endif

RPATH_PREFIX	= -Wl,-rpath,
RPATH_ORIGIN	= \$$ORIGIN

PLATFORM_DEF	= -DLinux -DLINUX -D_REENTRANT -D_LARGEFILE64_SOURCE

# -verbose for printing all informational messags
# -w0 for stricter than ANSI-C prototype warnings
# -fPIC is needed for any code that ends up in a shared library
ifndef NO_KPIC
PLATFORM_CC_OPTS = -fPIC
PLATFORM_C_OPTS  = -fPIC
endif
PLATFORM_LD_OPTS =
LD_DYNAMIC	= $(PLATFORM_LD_OPTS) -shared
LD_SYMBOLIC	= -Wl,-Bsymbolic

ifdef BUILD64
PLATFORM_CC_OPTS+=-m64
else
PLATFORM_CC_OPTS+=-m32
endif

# These libraries are platform-specific, not system-specific
# WARNING Don't use the -thread option, use -pthread option
PLATFORM_LIB	 += $(PRE_PLATFORM_LIB) pthread dl crypt resolv

JNI_MD_NAME	= linux
JNI_MD_SYSNAME	= i386
JNI_MD_SYSNAME64 = amd64
SETUPSDK_JNIDIR = Unix/Linux/X86

# Enables OS specific stats collection
FEAT_PLATFORM_STATS=1

# force native threads to be used at build runtime
export THREADS_FLAG=native

# Linux needs to be taken by the hand and told each and every directory
# where it might find libaries that other shared libs have dependencies to
LD_FLAGS += $(addprefix -Xlinker -rpath-link -Xlinker ,$(LIBDIRS))
LD_FLAGS += $(addprefix -Xlinker -rpath-link -Xlinker ,$(LOCAL_LIBDIRS))
LD_FLAGS += $(addprefix -Xlinker -rpath-link -Xlinker ,$(SYSTEM_LIBDIRS))

# No support for mapfiles
USE_MAPFILE=

# Enables creation of Linux RPM packages
FEAT_OS_NATIVE_PKG=1
# Enables creation of Linux patches
FEAT_OS_PATCH_PKG=1

# Add platform specific NSS modules
SECURITY_MODULE_LIBS += freebl3

# Setup platform specific shared component home
PLATFORM_JES_MFWK_HOME = "/opt/sun/mfwk"

# Setup platform specific binary path.
PLATFORM_JES_BINPATH="/opt/sun/private/bin"

# Setup platform specific library path.
PLATFORM_JES_LIBPATH="/opt/sun/private/lib:/opt/sun/private/share/lib:/opt/sun/lib"

# Setup platform specific class path
PLATFORM_JES_ANT_CP="/opt/sun/share/lib/ant.jar"
PLATFORM_JES_SERVER_CP="/opt/sun/share/lib/ant.jar:/opt/sun/private/share/lib/ktsearch.jar:/opt/sun/share/lib/jaxws-api.jar:/opt/sun/share/lib/jaxws-rt.jar:/opt/sun/share/lib/jaxws-tools.jar:/opt/sun/share/lib/jsr181-api.jar:/opt/sun/private/share/lib/jsr250-api.jar:/opt/sun/share/jaxb/lib/jaxb-api.jar:/opt/sun/share/jaxb/lib/jaxb-impl.jar:/opt/sun/share/jaxb/lib/jaxb-xjc.jar:/opt/sun/share/lib/sjsxp.jar:/opt/sun/share/lib/jsr173_api.jar:/opt/sun/share/lib/saaj-api.jar:/opt/sun/share/lib/saaj-impl.jar:/opt/sun/share/lib/xmldsig.jar:/opt/sun/share/lib/xmlsec.jar:/opt/sun/share/lib/xws-security.jar:/opt/sun/share/lib/xws-security_jaxrpc.jar:/opt/sun/share/lib/wss-provider-update.jar:/opt/sun/share/lib/security-plugin.jar:/opt/sun/share/lib/FastInfoset.jar:/opt/sun/private/share/lib/relaxngDatatype.jar:/opt/sun/share/lib/resolver.jar:/opt/sun/share/lib/mail.jar:/opt/sun/share/lib/activation.jar"

PLATFORM_JES_ADMIN_CP="/opt/sun/share/lib/ant.jar:/opt/sun/private/share/lib/ktsearch.jar:/opt/sun/private/share/lib/jss4.jar:/opt/sun/share/lib/ldapjdk.jar:/opt/sun/jdmk/5.1/lib/jmxremote_optional.jar:/opt/sun/jdmk/5.1/lib/jdmkrt.jar:/opt/sun/mfwk/share/lib/mfwk_instrum_tk.jar"

PLATFORM_JES_CLI_CP="/opt/sun/jdmk/5.1/lib/jmxremote_optional.jar"

#PHP plugin available for this platform
FEAT_PHP=1

# Add platform specific JES patch dependencies
REQUIRE_LIST = "121656-16"

# Construct platform specific LD_LIBRARY path component
PLATFORM_LIBPATH=$(NSS_LIBDIR):$(LDAPSDK_LIBDIR)
