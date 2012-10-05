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

# Modified 2011 jyri@virkki.com

# This is the first file included by all makefiles.  Usage should be as follows:
# BUILD_ROOT=../../../../ (point to /ns/server)
# MODULE=mymodulename
# include $(BUILD_ROOT)/make/defines.mk
# // the rest of your makefile
# include $(BUILD_ROOT)/make/rules.mk


# This file determines the hardware and OS architecture we're running and the
# build variant that we want, then it includes the other files.

# First we collect some information from UNAME.  To allow speedier makefile
# execution, these can be set in the environment.

ifndef UNAME_REPORTS
UNAME_REPORTS:=$(shell uname)
endif # UNAME_REPORTS

ifndef UNAME_OS_RELEASE
UNAME_OS_RELEASE := $(shell uname -r)
endif #UNAME_OS_RELEASE

ifndef UNAME_OS_ARCH
UNAME_OS_ARCH:=$(subst /,_,$(shell uname -s))
endif #UNAME_OS_ARCH

OS_ARCH=$(UNAME_OS_ARCH)
OS_RELEASE=$(UNAME_OS_RELEASE)
OS_CPU=

# This is your chance to lie to the makefile.  Change OS_ARCH and OS_RELEASE
# as needed for various exceptional cases here.

####################
#### WINDOWS NT ####
####################

# There are many uname's for NT.  We need to get a single OS_CONFIG from them:
# We're going to use "WINNT" as the NT OS_ARCH string...  so if you find
# another, put in the aliasing here.  i.e.
ifeq ($(findstring CYGWIN32_NT, $(OS_ARCH)), CYGWIN32_NT)
OS_ARCH = WINNT
endif
ifeq ($(OS_ARCH),Windows_NT)
OS_ARCH = WINNT
endif

# Force OS release to 4.0 for now so it finds the components for win2k
ifeq ($(OS_ARCH),WINNT)
OS_RELEASE = 4.0
endif

ifeq ($(OS_ARCH),WINNT)
BUILD_ARCH=X86
BUILD_OS=NT
BUILD_VER=4.0
endif

#############
#### AIX ####
#############

ifeq ($(OS_ARCH),AIX)
UNAME_OS_RELEASE := $(shell uname -v).$(shell uname -r)
OS_RELEASE=5.3

BUILD_ARCH=POWER
BUILD_OS=$(OS_ARCH)
BUILD_VER=$(OS_RELEASE)
endif

##############
#### IRIX ####
#########*####

# Force the IRIX64 machines to use IRIX.
ifeq ($(OS_ARCH),IRIX64)
OS_ARCH = IRIX
endif

ifeq ($(OS_ARCH), IRIX)
BUILD_ARCH=MIPS
BUILD_OS=IRIX
BUILD_VER=6.5
endif

###############
#### SunOS ####
##########*####

ifeq ($(OS_ARCH),SunOS)
BUILD_OS=SOLARIS
BUILD_VER=$(OS_RELEASE)

# Solaris x86
ifeq ($(shell uname -m),i86pc)
ifdef BUILD64
BUILD_ARCH=AMD64
OS_CPU=i86pc_64
else
BUILD_ARCH=i86pc
OS_CPU=i86pc
endif
endif

# Solaris SPARC
ifneq ($(shell uname -m),i86pc)
ifdef BUILD64
BUILD_ARCH=SPARCV9
BUILD_VER=2.8
OS_RELEASE=5.8
OS_CPU=64
else
BUILD_ARCH=SPARC
BUILD_VER=2.8
OS_RELEASE=5.8
endif
OS_SHIP=SunOS5.8
endif

endif

##############
#### OSF1 ####
#########*####

ifeq ($(OS_ARCH),OSF1)
BUILD_ARCH=ALPHA
BUILD_OS=$(OS_ARCH)
BUILD_VER=$(OS_RELEASE)
endif

###############
#### Linux ####
##########*####

ifeq ($(OS_ARCH), Linux)
ARCH_REPORTS=$(shell uname -m)
BUILD_OS=Linux
OS_RELEASE=2.4

ifeq ($(ARCH_REPORTS),i686)
BUILD_ARCH=x86
else #!i686
ifeq ($(ARCH_REPORTS),x86_64)
BUILD_ARCH=x86_64
endif
ifeq ($(BUILD_ARCH),x86_64)
OS_CPU=64
OS_RELEASE=2.6
else
BUILD_ARCH=UNKNOWN
endif #!x86_64
endif #ARCH_REPORTS

BUILD_VER=$(OS_RELEASE)
endif #Linux

###############
#### HP-UX ####
##########*####

ifeq ($(OS_ARCH), HP-UX)
BUILD_ARCH=HPPA
BUILD_OS=HP-UX
BUILD_VER=B.11.11
endif

ifndef OSVERSION
OSVERSION = $(subst .,0,$(UNAME_OS_RELEASE))
endif #OSVERSION

###############
## BUILD64 ##
###############
# Detect for BUILD64 variable on incompatible 
# platform and report error
ifneq ($(OS_ARCH), SunOS)
ifdef BUILD64
$(error BUILD64 variable is not supported for this platform)
endif
endif

###############
## OS_CONFIG ##
###############
OS_CONFIG := $(OS_ARCH)$(OS_RELEASE)
ifdef OS_CPU
OS_CONFIG := $(OS_CONFIG)_$(OS_CPU)
endif # OS_CPU

PLATFORM=${BUILD_ARCH}_${BUILD_OS}_${BUILD_VER}

###################
## BUILD VARIANT ##
###################

ifndef BUILD_VARIANT
BUILD_VARIANT=DEBUG
endif # BUILD_VARIANT

ifeq ($(BUILD_VARIANT),DEBUG)
DEBUG=1
DEBUG_BUILD=1
endif

ifeq ($(BUILD_VARIANT),OPTIMIZED)
OPTIMIZED_BUILD=1
NPROBE=1
endif

ifeq ($(BUILD_VARIANT),RELEASE)
OPTIMIZED_BUILD=1
NPROBE=1
endif

# ADD NEW BUILD_VARIANT TAGS HERE

ifdef OPTIMIZED_BUILD
OBJDIR_TAG = _OPT
else # OPTIMIZED_BUILD
ifdef DEBUG_BUILD
OBJDIR_TAG = _DBG
endif # DEBUG_BUILD
endif # OPTIMIZED_BUILD

# OBJDIR contains bits specific to a single build variant and architecture
HOST_OBJDIR:=$(OS_CONFIG)$(OBJDIR_TAG).OBJ
OBJDIR:=$(HOST_OBJDIR)

# SHIPDIR contains bits in their ready-to-ship layout (on Solaris, we package
# both sparcv8 and sparcv9 bits in one tree and both i386 and amd64 bits in
# another)
ifdef OS_SHIP
SHIPDIR:=$(OS_SHIP)$(OBJDIR_TAG).OBJ
else
SHIPDIR:=$(OBJDIR)
endif

ifndef JAVA_VERSION
JAVA_VERSION = 5
endif

USE_JDK=1

include ${BUILD_ROOT}/make/defines_COMMON.mk
include ${BUILD_ROOT}/make/defines_${PLATFORM}.mk

# we assume PROJECT is WebServer if it's not already defined
ifndef PROJECT
PROJECT = WebServer
endif

ifdef PROJECT
include $(BUILD_ROOT)/make/defines_$(PROJECT).mk
endif

JAVA_OBJDIR=JDK1.$(JAVA_VERSION)$(OBJDIR_TAG).OBJ

ifdef BUILD_JAVA
OBJDIR:=$(JAVA_OBJDIR)
endif

ifdef BUILD_JAVA
include ${BUILD_ROOT}/make/defines_JAVA_JDK_1.$(JAVA_VERSION).mk
endif
