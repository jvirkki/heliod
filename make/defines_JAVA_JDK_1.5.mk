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

# defines for Java build
ifndef JAVAC
JAVAC       = $(JDK_DIR)/bin/javac
endif
ifndef JAVAH
JAVAH       = $(JDK_DIR)/bin/javah
endif
ifndef JAR
JAR         = $(JDK_DIR)/bin/jar
endif
ifndef JAVADOC
JAVADOC     = $(JDK_DIR)/bin/javadoc
endif

ifdef USE_J2EE
SYSCLASSPATH:=$(SYSCLASSPATH)$(CLASSPATH_SEP)$(JAVA_CLASSES_DIR)$(CLASSPATH_SEP)$(BUILD_ROOT)/external/pwc/$(OBJDIR)/pwc.jar
endif

JAVACLASSPATH   = $(JAVA_OBJDIR)$(CLASSPATH_SEP)$(LOCAL_CLASS_PATH)$(CLASSPATH_SEP)$(SYSCLASSPATH)

ifeq ($(OS_ARCH),WINNT)
_JAVA_CLASSPATH=$(subst :,;,$(JAVACLASSPATH))
else
_JAVA_CLASSPATH=$(JAVACLASSPATH)
endif

ifdef DEBUG_BUILD
JAVAC_FLAGS     = -g -classpath "$(_JAVA_CLASSPATH)"
ifdef CHECK_DEPRECATION
JAVAC_FLAGS    += -deprecation
endif
else
JAVAC_FLAGS     = -O -classpath "$(_JAVA_CLASSPATH)"
endif

JAVAH_FLAGS = -jni -classpath "$(_JAVA_CLASSPATH)"


ifneq ($(OS_ARCH),WINNT)
CUR_DIR=$(shell pwd)
else
# PWD on NT appends CRLF which we must strip:
CUR_DIR=$(shell pwd | $(TR) -d "\r\n")
endif

#
# defs for ant
#

ANT_JAVA_HOME=$(CUR_DIR)/$(JDK_DIR)
ANT_HOME=$(CUR_DIR)/$(EXTERNAL_BASE)/ant/$(OBJDIR)

ifeq ($(OS_ARCH),WINNT)
ANT_BIN=ant.bat
else
ANT_BIN=ant
endif


STD_ANT_OPTIONS  = -buildfile $(ANT_BUILD_FILE)
STD_ANT_OPTIONS += -Dobj.dir=$(OBJDIR)

ifeq ($(BUILD_VARIANT), OPTIMIZED)
STD_ANT_OPTIONS += -Doptimize=on
else
STD_ANT_OPTIONS += -Doptimize=off
endif

_ANT_OPTIONS=$(STD_ANT_OPTIONS) $(ANT_OPTIONS) $(ANT_DEFS)

ifndef ANT_TARGETS
ANT_TARGETS=all
endif

# Variable containing environment value for running ant
ANT_ENV=ANT_OPTS=-Xmx256m

# Ant command line
# (ANT_ARGS are optional and are specified by the user on the command line)
ANT=$(ANT_BIN) $(_ANT_OPTIONS) $(ANT_TARGETS)
