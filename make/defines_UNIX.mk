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

# Modified 2011 jyri@virkki.com

TOOL_ROOT	=/usr
NSTOOL_ROOT     =/usr

CC              =$(ECHO) define CC in the platform definitions.
C               =$(ECHO) define C  in the platform definitions.
AR              =$(ECHO) define AR in the platform definitions.
LD              =$(ECHO) define LD in the platform definitions.

### for installer
CPPCMD		=$(C) -E

C++C            =$(CC)

### COMMON UNIX BINARIES
PERL		=$(TOOL_ROOT)/bin/perl
RM		=$(TOOL_ROOT)/bin/rm
LS		=$(TOOL_ROOT)/bin/ls
CP		=$(TOOL_ROOT)/bin/cp
CP_R		=$(CP) -r
LN		=$(TOOL_ROOT)/bin/ln -f
CMP		=$(TOOL_ROOT)/bin/cmp
MV		=$(TOOL_ROOT)/bin/mv
SED		=$(TOOL_ROOT)/bin/sed
ECHO		=$(TOOL_ROOT)/bin/echo
ENV		=$(TOOL_ROOT)/bin/env
DATE		=$(TOOL_ROOT)/bin/date
MKDIR           =$(TOOL_ROOT)/bin/mkdir
CHMOD           =$(TOOL_ROOT)/bin/chmod
CHMOD_DASH_R_755 =$(TOOL_ROOT)/bin/chmod -R 755
MKDIR_DASH_P	=$(MKDIR) -p
XARGS           =$(TOOL_ROOT)/bin/xargs
DIRNAME         =$(TOOL_ROOT)/bin/dirname
BASENAME        =$(TOOL_ROOT)/bin/basename
SHELL           =$(TOOL_ROOT)/bin/sh
SLEEP		=$(TOOL_ROOT)/bin/sleep
WC		=$(TOOL_ROOT)/bin/wc
GREP		=$(TOOL_ROOT)/bin/grep
EGREP		=$(TOOL_ROOT)/bin/egrep
FIND		=$(TOOL_ROOT)/bin/find
TOUCH		=$(TOOL_ROOT)/bin/touch
PRINTF		=$(TOOL_ROOT)/bin/printf
YACC		=$(TOOL_ROOT)/bin/yacc
TR              =$(TOOL_ROOT)/bin/tr
TAR		=$(TOOL_ROOT)/bin/tar
STRIP		=strip
RCP		=$(TOOL_ROOT)/bin/rcp
RCP_CMD		=$(RCP) -r $(RCPUSER)@$(RCPSERVER)

G++		=$(NSTOOL_ROOT)/bin/g++
GCC		=$(NSTOOL_ROOT)/bin/gcc
FLEX		=$(NSTOOL_ROOT)/bin/flex
BISON		=$(NSTOOL_ROOT)/bin/bison
CVS		=$(NSTOOL_ROOT)/bin/cvs
GZIP		=$(NSTOOL_ROOT)/bin/gzip
GUNZIP		=$(NSTOOL_ROOT)/bin/gunzip
ZIP		=$(NSTOOL_ROOT)/bin/zip
UNZIP		=$(NSTOOL_ROOT)/bin/unzip
NMAKE		=$(NSTOOL_ROOT)/bin/gmake -f
GENRB		=$(ENV) $(LD_LIB_VAR)=$(ICU_DIR)/lib $(ICU_DIR)/sbin/genrb

##############
## PREFIXES ##
##############

# Modify as needed in platform defines
OBJ =o
SBR =o
CPP =cpp
STATIC_LIB_SUFFIX=a
DYNAMIC_LIB_SUFFIX=so
LIBPREFIX=lib

COMMENT=\#
ifndef LD_LIB_VAR
LD_LIB_VAR=LD_LIBRARY_PATH
endif # !LD_LIB_VAR


###################
### COMMON LIBS ###
###################

NSPR_LIB   = plc4 plds4 nspr4
SEC_LIB+= ssl nss cert secmod key crypto hash secutil dbm

LDAP_LIBS  = ldap$(LDAP_LIB_VERSION) \
             prldap$(LDAP_LIB_VERSION)
SASL_LIBS  = sasl2
SSLDAP_LIB = ssldap$(LDAP_LIB_VERSION)
ICU_LIBS   = icui18n icuuc icudata
Z_LIB = z

# files needed for the (re)distribution
ZLIB_EXPORTS= $(addprefix $(LIBPREFIX), \
              $(addsuffix .$(DYNAMIC_LIB_SUFFIX), \
              $(Z_LIB)))

ICU_EXPORTS= $(addprefix $(LIBPREFIX), \
             $(addsuffix .$(DYNAMIC_LIB_SUFFIX).$(LIBICU_SUFFIX), \
             $(ICU_LIBS)))

XERCESC_LIBS=xerces-c
XERCESC_LIB_SUFFIX=26
XERCESC_EXPORTS=$(addprefix $(LIBPREFIX), \
                $(addsuffix .$(DYNAMIC_LIB_SUFFIX).$(XERCESC_LIB_SUFFIX), \
                $(XERCESC_LIBS)))

XALANC_LIBS=xalan-c xalanMsg
XALANC_LIB_SUFFIX=19
XALANC_EXPORTS=$(addprefix $(LIBPREFIX), \
               $(addsuffix .$(DYNAMIC_LIB_SUFFIX).$(XALANC_LIB_SUFFIX), \
               $(XALANC_LIBS)))

PCRE_LIBS=pcre
PCRE_LIB_SUFFIX=0
PCRE_EXPORTS=$(addprefix $(LIBPREFIX), \
             $(addsuffix .$(DYNAMIC_LIB_SUFFIX).$(PCRE_LIB_SUFFIX), \
             $(PCRE_LIBS)))

PAM_LIBS=pam
GSS_LIBS=gss

###############################
### COMMON COMPILER OPTIONS ###
###############################

ifdef DEBUG_BUILD
CC_DEBUG = -g
C_DEBUG  = -g
else
# jsalter: remove -O4 option because not all compilers use
#          that option for high-level optimization
CC_DEBUG =
C_DEBUG  =
endif

ARFLAGS = -r

ZIPFLAGS = -ry

# Unix-generic defines
SYSTEM_DEF += -DXP_UNIX

# Library expansion code.

REAL_LIBS=$(addprefix -l,$(LIBS))
EXE_REAL_LIBS=$(addprefix -l,$(EXE_LIBS))
EXE1_REAL_LIBS=$(addprefix -l,$(EXE1_LIBS))
EXE2_REAL_LIBS=$(addprefix -l,$(EXE2_LIBS))
EXE3_REAL_LIBS=$(addprefix -l,$(EXE3_LIBS))
EXE4_REAL_LIBS=$(addprefix -l,$(EXE4_LIBS))
EXE5_REAL_LIBS=$(addprefix -l,$(EXE5_LIBS))

DLL_REAL_LIBS=$(addprefix -l,$(DLL_LIBS))
DLL1_REAL_LIBS=$(addprefix -l,$(DLL1_LIBS))
DLL2_REAL_LIBS=$(addprefix -l,$(DLL2_LIBS))
DLL3_REAL_LIBS=$(addprefix -l,$(DLL3_LIBS))
DLL4_REAL_LIBS=$(addprefix -l,$(DLL4_LIBS))
DLL5_REAL_LIBS=$(addprefix -l,$(DLL5_LIBS))

REAL_LIBDIRS=$(addprefix -L,$(LIBDIRS))
EXE_REAL_LIBDIRS=$(addprefix -L,$(EXE_LIBDIRS))
EXE1_REAL_LIBDIRS=$(addprefix -L,$(EXE1_LIBDIRS))
EXE2_REAL_LIBDIRS=$(addprefix -L,$(EXE2_LIBDIRS))
EXE3_REAL_LIBDIRS=$(addprefix -L,$(EXE3_LIBDIRS))
EXE4_REAL_LIBDIRS=$(addprefix -L,$(EXE4_LIBDIRS))
EXE5_REAL_LIBDIRS=$(addprefix -L,$(EXE5_LIBDIRS))
